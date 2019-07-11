#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "math_utils.h"
#include "os.h"
#include "return_codes.h"
#include "sim_plan_rand.h"
#include "sim_plan_txt.h"
#include "ansi_console.h"
#include "safe_alloc.h"

#define COLORS_ALL
#include "ansi_colors.h"

#include "config.h"

#define log_val_i(key, col, comment) (printf($white "  %-" #col "." #col "s " $web_lightsteelblue "%4d   " $green " %s" $cdef "\n", #key, key, comment))
#define log_val_f(key, col, comment) (printf($white "  %-" #col "." #col "s " $web_lightsteelblue "%7.2f" $green " %s" $cdef "\n", #key, key, comment))
#define log_error_i(err, key, col, comment) (printf($web_lightsalmon "  %-" #col "." #col "s " $red "%4d   " $web_red " %s" $cdef "\n", #key, key, comment), (err)++)
#define log_error_f(err, key, col, comment) (printf($web_lightsalmon "  %-" #col "." #col "s " $red "%7.2f" $web_red " %s" $cdef "\n", #key, key, comment), (err)++)

bool ispowerof2(unsigned int x)
{
  return x && !(x & (x - 1));
}

void print_constants()
{
  int err = 0;
  printf($web_orange "CPU and Memory managers simulator" $cdef "\n");
  printf("\n");
  printf($web_skyblue "Alunos:" $cdef "\n");
  printf($white "  Miguel Angelo " $gray "(" $web_lightsteelblue "116033119" $gray ")" $cdef "\n");
  printf($white "  Erick Rocha   " $gray "(" $web_lightsteelblue "111335299" $gray ")" $cdef "\n");
  printf("\n");
  printf($web_skyblue "Constants:" $cdef "\n");
  log_val_i(MAX_PROCESSES, 23, "maximum number of processes that will be created by the simulation");
  log_val_i(MAX_TIME_SLICE, 23, "maximum time-slice allowed, even if the simulation-plan asks for more");
  log_val_i(MAX_PRIORITY_LEVEL, 23, "");
  log_val_f(PROB_NEW_PROCESS, 23, "");
  log_val_f(AVG_PROC_DURATION, 23, "");
  log_val_f(PROB_NEW_PROC_CPU_BOUND, 23, "");
  log_val_i(MAX_NUMBER_OF_DEVICES, 23, "indicates the maximum number of devices that can be connected to the OS");
  if (!ispowerof2(MAX_SUPPORTED_FRAMES))
    log_error_i(err, MAX_SUPPORTED_FRAMES, 23, "must be power of 2");
  else
    log_val_i(MAX_SUPPORTED_FRAMES, 23, "maximum number of frames, indicates the maximum amount of RAM that can be installed");
  log_val_i(MAX_WORKING_SET, 23, "invariant maximum working set");
  log_val_i(MAX_PAGE_TABLE_SIZE, 23, "size of the page table for each process, it defines the virtual memory address space size");
  log_val_i(MAX_WAIT_FRAME_QUEUE_SIZE, 23, "maximum number of waiting processes in the wait frame queue");
  if (err > 0)
  {
    printf("  " $red "%d" $cdef " errors occured!", err);
    exit(1);
  }
}

void get_simulation_plan(simulation_plan *plan)
{
  if (0)
  {
    // initialized a random execution plan
    plan_rand_init(
        plan,
        922337231LL,
        6854775827LL,
        MAX_PROCESSES,
        PROB_NEW_PROCESS,
        AVG_PROC_DURATION,
        PROB_NEW_PROC_CPU_BOUND);
  }
  else
  {
    plan_txt_init(plan, "plans/mem_man_test_plan.txt", MAX_PROCESSES);
  }
}

void promote_waiting_processes(os *os, int time)
{
  for (int itq = 1; itq < os->scheduler->queue_count; itq++)
  {
    process_queue *queue = os->scheduler->queues + itq;
    process *proc = 0;
    if (pq_get(queue, 0, &proc) == OK)
    {
      int max_wait_time = 1 << clamp(itq, 0, 10);
      if (proc->ready_since + max_wait_time < time)
      {
        pq_dequeue(queue, &proc);
        pq_enqueue(os->scheduler->queues + itq - 1, proc);
      }
    }
  }
}

int incoming_processes(simulation_plan *plan, os *os, int time)
{
  int new_proc_count = (*plan->incoming_processes)(plan, time);
  for (int it = 0; it < new_proc_count; it++)
  {
    process_queue *target_queue = os->scheduler->queues + 0;
    process *new_proc = safe_malloc(sizeof(process), os);
    int pid = os->next_pid++;
    (*plan->create_process)(plan, time, pid);
    process_init(new_proc, pid, MAX_PAGE_TABLE_SIZE, os->max_working_set);
    pq_enqueue(target_queue, new_proc);
  }
  return new_proc_count;
}

bool device_next_finished(device *list, device **item, int count, int time)
{
  if (*item == NULL)
    *item = list;
  for (; *item < list + count; (*item)++)
    if ((*item)->is_connected && (*item)->current_job_end == time)
      return true;
  return false;
}

void handle_page_fault(os *os, process *proc, int page_number, int swap_device, int time)
{
  int free_ws_index = -1;
  for (int itW = 0; itW < os->max_working_set; itW++)
    if (proc->working_set[itW] < 0 && free_ws_index < 0)
    {
      free_ws_index = itW;
      break;
    }

  // we need to use a free working-set entry to fill with the needed page data
  if (free_ws_index < 0)
  {
    // Free working-set entry not found, we need to write
    // the LRU page to disk then load the requested page
    // into that entry

    // locating the LRU working-set entry
    int lru_pt_index = -1;
    {
      page_table_entry *lru_pt_entry = NULL;
      for (int itW = 0; itW < os->max_working_set; itW++)
      {
        int pt_index = proc->working_set[itW];
        page_table_entry *pt_entry = proc->page_table + pt_index;
        frame_table_entry *ft_entry = os->frame_table + pt_entry->frame;
        if (!ft_entry->locked && (lru_pt_index < 0 || pt_entry->last_access < lru_pt_entry->last_access))
        {
          lru_pt_entry = pt_entry;
          lru_pt_index = pt_index;
        }
      }
    }

    // if all page-frames are locked, then we have a problem
    // it means that the process cannot run,
    // and it cannot be suspended... the only choice is
    // to kill the process
    if (lru_pt_index < 0)
    {
      // TODO: kill the process
      return;
    }

    // we can now write the frame to disk, and then reuse the frame for another page
    page_table_entry *lru_pt_entry = proc->page_table + lru_pt_index;
    frame_table_entry *lru_frame = os->frame_table + lru_pt_entry->frame;
    printf("Writing page %d / frame %d to disk, to free page-table entry %d", lru_pt_index, lru_pt_entry->frame, lru_pt_index);

    int frame_to_use = lru_pt_entry->frame;
    lru_pt_entry->is_on_memory = false;
    lru_pt_entry->is_on_disk = true; // Where on disk? We assumed it is not to be known... it's there!
    lru_pt_entry->frame = -1;

    // after device finishes with the load op, the process will go to the page-ready queue
    proc->state = PROC_STATE_BLOCKED_SWAPPING;

    device *target_device = os->devices + swap_device;

    // setting data to remember what are the steps after the IO operation completes:
    // - load the page into the free frame and associate them
    // - continue with the program as if nothing happened
    proc->pending_op_type = OP_PAGE_LOAD;
    proc->store_op.page_number = page_number;
    proc->store_op.frame_number = frame_to_use;
    proc->store_op.device = target_device;
    proc->store_op.device_addr = 0;

    // Before doing a write IO, we need to know what frame contains the data,
    // and lock it so that it cannot be swapped out.
    // After the operation completes the frame will be unlocked.
    lru_frame->locked = true;

    enqueue_on_device(time, os, target_device, proc); // out of memory impossible here, we are reusing a frame
  }
  else
  {
    // after device finishes with the load op, the process will go to the page-ready queue
    proc->state = PROC_STATE_BLOCKED_SWAPPING;

    device *target_device = os->devices + swap_device;

    // setting data to remember what are the steps after the IO operation completes:
    // - load the page into the free frame and associate them
    // - continue with the program as if nothing happened
    proc->pending_op_type = OP_PAGE_LOAD;
    proc->store_op.page_number = page_number;
    proc->store_op.frame_number = -1; // no frame will be indicated to load the data, will be decided just before reading
    proc->store_op.device = target_device;
    proc->store_op.device_addr = 0; // we don't simulate the actual device op, so this addr is not used

    // adding the process to the wait queue for the device
    if (enqueue_on_device(time, os, target_device, proc) == ERR_OUT_OF_MEMORY)
      pq_enqueue(os->require_frame_queue, proc);
  }
}

int os_get_freeing_frames_count(os *os, int swap_device)
{
  int freeing_frames = 0;
  process_queue *blk_queue = os->devices[swap_device].blocked_queue;
  for (int it = 0; it < blk_queue->count; it++)
  {
    process *blk_proc;
    pq_get(blk_queue, it, &blk_proc);
    if (blk_proc->pending_op_type == OP_SWAP_OUT)
    {
      for (int itW = 0; itW < os->max_working_set; itW++)
        if (blk_proc->working_set[itW] < 0)
          freeing_frames++;
    }
  }
  process *blk_proc = os->devices[swap_device].current_process;
  if (blk_proc->pending_op_type == OP_SWAP_OUT)
  {
    for (int itW = 0; itW < os->max_working_set; itW++)
      if (blk_proc->working_set[itW] < 0)
        freeing_frames++;
  }
  return freeing_frames;
}

int proc_get_swapout_working_set_count(os *os, process *proc)
{
  int count = 0;
  for (int it = 0; it < os->max_working_set; it++)
    if (proc->swapout_pages[it] >= 0)
      count++;
  return count;
}

int main()
{
  setANSI();
  print_constants();
  simulation_plan *plan = safe_malloc(sizeof(simulation_plan), NULL);
  get_simulation_plan(plan);

  os *os = safe_malloc(sizeof(struct os), NULL);
  ;

  {
    plan_os_settings conf;
    (*plan->get_os_settings)(plan, &conf);
    int time_slice = clamp(conf.time_slice, 0, MAX_TIME_SLICE);
    int max_working_set = clamp(conf.max_working_set, 0, MAX_WORKING_SET);
    int memory_frames = clamp(conf.memory_frames, 0, MAX_SUPPORTED_FRAMES);
    int wait_frame_queue_capacity = clamp(conf.wait_frame_queue_capacity, 0, MAX_WAIT_FRAME_QUEUE_SIZE);
    const unsigned int page_bit_field = MAX_SUPPORTED_FRAMES - 1;

    printf("\n");
    printf($web_skyblue "Settings:" $cdef "\n");
    log_val_i(time_slice, 10, "");

    //  it is here because the plan does not know how many queues the OS have for the CPU
    //  it's a matter of leting the plan devine the number of queues
    // allowing the sim plan to print something before starting
    // - set_time: is used to show global plan information (print the whole plan if possible)
    // - print_time_final_state: is used to show the headers of the info that will be printed at the end of each iteration
    printf("\n");
    printf($web_skyblue "Plan info:" $cdef "\n");
    if (plan->set_time != NULL)
      if (!(*plan->set_time)(plan, -1, NULL))
        return 0;

    // initializing OS structure
    os_init(os, MAX_NUMBER_OF_DEVICES, MAX_PROCESSES, MAX_PRIORITY_LEVEL, max_working_set, memory_frames, time_slice, wait_frame_queue_capacity);

    for (int itdev = 0; itdev < MAX_NUMBER_OF_DEVICES; itdev++)
    {
      sim_plan_device dev;
      if (plan->create_device(plan, itdev, &dev))
        device_init(os->devices + itdev, dev.name, dev.job_duration, dev.ret_queue, MAX_PROCESSES);
      else
        (os->devices + itdev)->is_connected = false;
    }

    // TODO: move this before OS initialization above
    printf("\n");
    printf($web_skyblue "Starting simulation:" $cdef "\n");
    (*plan->print_time_final_state)(plan, -1, os);
  }

  //
  // Main simulation loop
  //
  // Simulation loop for things happening inside each time unit.
  // It is divided in two parts:
  // - an infinitesimal time at the very beginning of the time unit
  //    it is here that most of the events happen, and the results
  //    of this part will tell the state in which the time unit runs
  // - the time unit run
  //    represents the program running in the given state for 1 time unit
  int proc_count = 0;
  int swap_device = (*plan->get_swap_device)(plan);
  if (swap_device < 0)
  {
    printf("Error! Must have a swap device.");
    exit(1);
  }

  for (int time = 0;; time++)
  {
    // telling the current time to the simulation plan
    if (plan->set_time != NULL)
      if (!(*plan->set_time)(plan, time, os))
        break;

    // # Simulate incoming processes
    if (proc_count < MAX_PROCESSES)
      proc_count += incoming_processes(plan, os, time);

    // # Checking what processes are waiting for too long and upgrading them.
    // Processes start moving from lower to higher priority when
    // they are ready for a long time, without being actually processed.
    // At each time unit, we check the next of each queue to see if
    // can be upgraded. Each queue has it's own maximum wait time,
    // each being a multiple of the previous.
    promote_waiting_processes(os, time);

    while (1)
    {
      bool continue_parent = false;

      // # Checking devices for finished jobs.
      // This must be inside the loop, because
      // there are immediate devices that don't
      // use any time at all, but they do preempt
      // the CPU from the process that asked for IO.
      device *dev = NULL;
      while (device_next_finished(os->devices, &dev, MAX_NUMBER_OF_DEVICES, time))
      {
        process *proc = dev->current_process;
        proc->ready_since = time;

        // Executing the pending operation
        if (proc->pending_op_type == OP_PAGE_LOAD)
        {
          // the page load operation will start another IO operation
          // to load the desired page to the indicated frame, but
          // it will not enqueue the operation, it will just do it
          // since it already has the device...
          // if it was possible to load a page from another device
          // then we would have to enqueue on that device
          int pt_index = proc->store_op.page_number;
          int ft_index = proc->store_op.frame_number;
          // pretend that we used store_op info... not using because the simulation doesn't need it
          // but it were not a simulation, then we'd use them to load real data from storage
          proc->pending_op_type = OP_PAGE_LOADED;
          dev->current_job_end == time + dev->job_duration;
          continue;
        }
        else if (proc->pending_op_type == OP_PAGE_LOADED)
        {
          // the page loaded operation will associate the loaded
          // frame with the page
          proc->pending_op_type = OP_NONE;
          int pt_index = proc->store_op.page_number;
          int ft_index = proc->store_op.frame_number;
          page_table_entry *pt_entry = proc->page_table + pt_index;
          pt_entry->frame = ft_index;
          pt_entry->is_on_memory = true;
          pt_entry->last_access = time;
        }
        else if (proc->pending_op_type == OP_SWAP_OUT)
        {
          // changing the state of the OS to indicate that more free frames are available
          proc->pending_op_type = OP_NONE;
          memset(proc->working_set, -1, os->max_working_set);
          int swapout_working_set_count = proc_get_swapout_working_set_count(os, proc);
          os->freeing_frame_count -= swapout_working_set_count;
          os->free_frame_count += swapout_working_set_count;

          // finding a process to assign the pages to
          process *next_wait_proc;
          while (pq_get(os->require_frame_queue, 0, &next_wait_proc) == OK)
          {
            int swapin_working_set_count = proc_get_swapout_working_set_count(os, next_wait_proc);
            int required_frames = next_wait_proc->pending_op_type == OP_SWAP_IN ? swapin_working_set_count : 1;
            if (os->free_frame_count > required_frames)
            {
              pq_dequeue(os->require_frame_queue, &next_wait_proc); // no test is needed, we did a pq_get already, we are sure the item is there
              memcpy(next_wait_proc->working_set, next_wait_proc->swapout_pages, os->max_working_set * sizeof(int));
              for (int itP = 0; itP < os->max_working_set; itP++)
              {
                int page_number = next_wait_proc->working_set[itP];
                if (page_number < 0)
                  continue;
                int frame_number = os_find_free_frame(os);
                page_table_entry *pt_entry = next_wait_proc->page_table + page_number;
                pt_entry->frame = frame_number;
                pt_entry->is_on_memory = true;
                pt_entry->last_access = time;

                // Enqueueing the swap-device load operation.
                // In a real OS, we would need to issue a read
                // operation for each saved page, but in this
                // simulation a single read will be done
                next_wait_proc->pending_op_type = OP_SWAPPED_IN;
                enqueue_on_device(time, os, os->devices + swap_device, next_wait_proc); // out of memory not possible, we have the frames already
              }
            }
          }
        }
        else if (proc->pending_op_type == OP_SWAPPED_IN)
        {
          // after the swap-in operation finishes we enqueue
          // the process in the highest possible priority queue,
          // the page ready queue, processes in this queue
          // cannot be swapped-out, they will run at least
          // for a small amount
          if (pq_enqueue(os->scheduler->page_ready_queue, proc) == OK)
          {
            // TODO:
          }
        }

        // What queue should this process return to?
        // depends on the kind of operation it was executing:
        // - waiting for page to load: highest possible priority
        // - normal IO: return queue indicated by the device
        process_queue *queue_to_ret_to;
        if (proc->state == PROC_STATE_BLOCKED_SWAPPING)
          queue_to_ret_to = os->scheduler->page_ready_queue;
        else
          queue_to_ret_to = os->scheduler->queues + dev->ret_queue;

        // if the device uses a memory frame, unlock it
        if (proc->store_op.frame_number >= 0)
        {
          os->frame_table[proc->store_op.frame_number].locked = false;
          proc->store_op.frame_number = -1;
        }

        if (pq_enqueue(queue_to_ret_to, proc) == OK)
        {
          // checking whether there is a process waiting for the device and set it as the current
          process *next_proc;
          if (pq_dequeue(dev->blocked_queue, &next_proc) == OK)
          {
            if (exec_on_device(time, os, dev, next_proc) == ERR_OUT_OF_MEMORY)
            {
              // waiting for a free frame in the queue for that purpose
              pq_enqueue(os->require_frame_queue, proc);

              continue_parent = true;
              break;
            }
          }
          else
            dev->current_process = NULL;
        }
      }

      if (continue_parent)
        continue;

      // # Swap-out processes to fulfill the requirements
      // of procs waiting for a free memory frame.
      while (os->require_frame_queue->count > 0)
      {
        // check how much memory is going to be free
        int cnt_freeing_frames = os_get_freeing_frames_count(os, swap_device);
        int cnt_free_frames = os->free_frame_count;
        int cnt_expected_free_frames = cnt_freeing_frames + cnt_free_frames;

        int cnt_required_free_frames = 0;
        for (int it = 0; it < os->require_frame_queue->count; it++)
        {
          process *proc_req;
          pq_get(os->require_frame_queue, it, &proc_req);
          if (proc_req->pending_op_type == OP_PAGE_LOAD)
            cnt_required_free_frames++;
          else if (proc_req->pending_op_type == OP_SWAP_IN)
          {
            int swapout_working_set_count = proc_get_swapout_working_set_count(os, proc_req);
            cnt_required_free_frames += swapout_working_set_count;
          }
        }

        // there is a soft-limit of the number of freeing frames
        // the actual number may be greater, but it won't get a
        // lot greater because we will stop trying to swap-out more
        // processes if the value is over the limit
        if (cnt_freeing_frames > 4)
          break;

        // if less free frames are required than what is being provided
        // then don't try to swap-out any more processes
        if (cnt_required_free_frames > cnt_expected_free_frames)
          break;

        process *waiting_proc;
        pq_get(os->require_frame_queue, 0, &waiting_proc);
        
        int frame = os_find_free_frame(os);

        // finding a process to swap-out
        process *swapout_proc = NULL;
        for (int itD = 0; itD < os->max_devices; itD++)
        {
          device *dev = os->devices + itD;
          if (!dev->is_connected)
            continue;

          for (int itQ = 0; itQ < dev->blocked_queue->count; itQ++)
          {
            process *blk_proc;
            pq_get(dev->blocked_queue, itQ, &blk_proc);
            // a process can get here with the following states:
            // - PROC_STATE_BLOCKED_SUSPEND
            // - PROC_STATE_BLOCKED_SWAPPING
            // - PROC_STATE_BLOCKED
            // We need it to be PROC_STATE_BLOCKED
            if (blk_proc->state == PROC_STATE_BLOCKED)
            {
              swapout_proc = blk_proc;
              swapout_proc->state = PROC_STATE_BLOCKED_SWAPPING;
              break;
            }
          }
        }

        // find a READY process that is very low in the priority queues
        if (swapout_proc == NULL)
        {
          for (int itQ = 0; itQ < os->scheduler->queue_count; itQ++)
          {
            process_queue *queue = os->scheduler->queues + (os->scheduler->queue_count - itQ - 1);
            for (int it = 0; it < queue->count; it++)
            {
              process *ready_proc = queue->items[it];
              // a process can get here with the following states:
              // - PROC_STATE_READY_SUSPEND
              // - PROC_STATE_READY_SWAPPING
              // - PROC_STATE_READY
              // We need it to be PROC_STATE_READY
              if (ready_proc->state == PROC_STATE_READY)
              {
                swapout_proc = ready_proc;
                swapout_proc->state = PROC_STATE_READY_SWAPPING;
                break;
              }
            }
            if (swapout_proc != NULL)
              break;
          }
        }

        if (swapout_proc != NULL)
        {
          int swapout_working_set_count = proc_get_swapout_working_set_count(os, swapout_proc);
          os->freeing_frame_count += swapout_working_set_count;

          // setting the new state
          swapout_proc->pending_op_type = OP_SWAP_OUT;
          swapout_proc->store_op.page_number = -1; // swap-out operation swaps every page present in a memory frame
          swapout_proc->store_op.frame_number = -1;
          swapout_proc->store_op.device = os->devices + swap_device;
          swapout_proc->store_op.device_addr = 0;
          enqueue_on_device(time, os, os->devices + swap_device, swapout_proc); // out of memory not possible, it's a write-to-disk operation
        }
      }

      // # Selecting the next process if CPU is available.
      if (os->scheduler->current_process == NULL)
      {
        process *proc;
        if (select_next_process(os->scheduler, &proc) == OK)
        {
          // check if PC points to a page that is available
          int page_number = proc->pc >> 12; // 4KB is the size of each frame/page

          // each process has a table that contains info about all of its pages
          page_table_entry *pt_entry = proc->page_table + page_number;

          if (pt_entry->is_on_memory)
          {
            // page found
            printf("Requested page %d, frame %d\n", page_number, pt_entry->frame);
            proc->state = PROC_STATE_RUNNING;
            os->scheduler->current_process = proc;
            os->scheduler->time_slice_end = time + os->time_slice;
          }
          else
          {
            // page fault
            handle_page_fault(os, proc, page_number, swap_device, time);
            // current process is still NULL
            continue;
          }
        }
      }

      // # If process is running and it wants to access the memory
      if (os->scheduler->current_process != NULL)
      {
        process *proc;
        while ((proc = os->scheduler->current_process) != NULL)
        {
          unsigned int pc;
          if (!(*plan->execute_memory)(plan, time, proc->pid, &pc))
            break;

          proc->pc = pc;
          int page_number = pc >> 12;
          page_table_entry *pt_entry = proc->page_table + page_number;
          if (pt_entry->is_on_memory)
          {
            // page found
            printf("Requested page %d, frame %d\n", page_number, pt_entry->frame);
            // process still running, nothing else to do
          }
          else
          {
            // page fault
            handle_page_fault(os, proc, page_number, swap_device, time);
            os->scheduler->current_process = NULL;
            continue;
          }
        }
      }

      // # Checking whether process has finished and disposing of used resources.
      if (os->scheduler->current_process != NULL && (*plan->is_process_finished)(plan, time, os->scheduler->current_process->pid))
      {
        process_dispose(os->scheduler->current_process); // asking the process to dispose it's owned resources
        safe_free(os->scheduler->current_process, os);   // disposing of used memory
        os->scheduler->current_process = NULL;           // freeing cpu
        continue;
      }

      // # Checking whether the running process must be preempted.
      if (os->scheduler->current_process != NULL && os->scheduler->time_slice_end == time)
      {
        process *preempted_proc = os->scheduler->current_process;
        os->scheduler->current_process = NULL;
        int priority = clamp(preempted_proc->current_priority + 1, 0, MAX_PRIORITY_LEVEL - 1);
        preempted_proc->current_priority = priority;
        preempted_proc->ready_since = time;
        pq_enqueue(os->scheduler->queues + priority, preempted_proc);
        continue;
      }

      // # Checking whether the process wants to do an IO operation.
      // If it wants, then it will release the CPU for another process.
      if (os->scheduler->current_process != NULL)
      {
        int io_requested_device = (*plan->requires_io)(plan, time, os->scheduler->current_process->pid);
        if (io_requested_device >= 0)
        {
          // moving current process to the device wait queue
          device *target_device = os->devices + io_requested_device;

          // This is a limitation, we don't know what devices are storage devices
          // if we knew, we'd have to indicate OP_READ or OP_WRITE, and also
          // initialize the `store_op` data structure of the process.
          // This happens because read and write operations use memory frames,
          // it means they become locked, to deny the possibility of a swap-out,
          // after finishing the operation the frame must be unlocked.
          // The unlock operation is one of the pending operations that must be done.
          os->scheduler->current_process->pending_op_type = OP_NONE;

          enqueue_on_device(time, os, target_device, os->scheduler->current_process); // out of memory not possible here... we won't use memory frames
          os->scheduler->current_process = NULL;
          continue;
        }
      }

      break;
    }

    // # Running the current process.
    // This represents the act of running a single time unit.
    // increment running process internal duration
    if (os->scheduler->current_process != NULL)
    {
      (*plan->run_one_time_unit)(plan, time, os->scheduler->current_process->pid);
    }

    // writing the state at the end of current time unit
    if (time % 2)
      printf($web_steelblue);
    else
      printf($white);
    (*plan->print_time_final_state)(plan, time, os);
  }

  os_dispose(os);
  safe_free(os, NULL);

  (*plan->dispose)(plan);
  safe_free(plan, NULL);
}
