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

#define log_val_i(key,col,comment) (printf($white"  %-"#col"."#col"s "$web_lightsteelblue"%4d   "$green" %s"$cdef"\n", #key, key, comment))
#define log_val_f(key,col,comment) (printf($white"  %-"#col"."#col"s "$web_lightsteelblue"%7.2f" $green" %s"$cdef"\n", #key, key, comment))
#define log_error_i(err,key,col,comment) (printf($web_lightsalmon"  %-"#col"."#col"s "$red"%4d   "$web_red" %s"$cdef"\n", #key, key, comment), (err)++)
#define log_error_f(err,key,col,comment) (printf($web_lightsalmon"  %-"#col"."#col"s "$red"%7.2f" $web_red" %s"$cdef"\n", #key, key, comment), (err)++)

bool ispowerof2(unsigned int x)
{
  return x && !(x & (x - 1));
}

void print_constants()
{
  int err = 0;
  printf($web_orange"CPU and Memory managers simulator"$cdef"\n");
  printf("\n");
  printf($web_skyblue"Alunos:"$cdef"\n");
  printf($white"  Miguel Angelo "$gray"("$web_lightsteelblue"116033119"$gray")"$cdef"\n");
  printf($white"  Erick Rocha   "$gray"("$web_lightsteelblue"111335299"$gray")"$cdef"\n");
  printf("\n");
  printf($web_skyblue"Constants:"$cdef"\n");
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
  if (err > 0)
  {
    printf("  "$red"%d"$cdef" errors occured!", err);
    exit(1);
  }
}

void get_simulation_plan(simulation_plan* plan)
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

int main()
{
  setANSI();
  print_constants();
  simulation_plan *plan = safe_malloc(sizeof(simulation_plan), NULL);
  get_simulation_plan(plan);

  os *os = safe_malloc(sizeof(struct os), NULL);;

  {
    plan_os_settings conf;
    (*plan->get_os_settings)(plan, &conf);
    int time_slice = clamp(conf.time_slice, 0, MAX_TIME_SLICE);
    int max_working_set = clamp(conf.max_working_set, 0, MAX_WORKING_SET);
    int memory_frames = clamp(conf.memory_frames, 0, MAX_SUPPORTED_FRAMES);
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
    os_init(os, MAX_NUMBER_OF_DEVICES, MAX_PROCESSES, MAX_PRIORITY_LEVEL, max_working_set, memory_frames, time_slice);

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
  for (int time = 0;; time++)
  {
    // telling the current time to the simulation plan
    if (plan->set_time != NULL)
      if (!(*plan->set_time)(plan, time, os))
        break;

    // no more processes can be created because
    // we already created the maximum number of processes
    if (proc_count == MAX_PROCESSES)
      break;

    // # Simulate incoming processes.
    int new_proc_count = (*plan->incoming_processes)(plan, time);
    for (int it = 0; it < new_proc_count; it++)
    {
      if (proc_count <= MAX_PROCESSES)
      {
        proc_count++;
        process_queue* target_queue = os->scheduler->queues + 0;
        process *new_proc = safe_malloc(sizeof(process), os);
        int pid = os->next_pid++;
        (*plan->create_process)(plan, time, pid);
        process_init(new_proc, pid, MAX_PAGE_TABLE_SIZE, os->max_working_set);
        pq_enqueue(target_queue, new_proc);
      }
    }

    // # Checking what processes are waiting for too long and upgrading them.
    // Processes start moving from lower to higher priority when
    // they are ready for a long time, without being actually processed.
    // At each time unit, we check the next of each queue to see if
    // can be upgraded. Each queue has it's own maximum wait time,
    // each being a multiple of the previous.
    int max_wait_time = 8;
    for (int itq = 1; itq < os->scheduler->queue_count; itq++)
    {
      process_queue *queue = os->scheduler->queues + itq;
      process *proc = 0;
      if (pq_get(queue, 0, &proc) == OK)
      {
        if (proc->ready_since + max_wait_time < time)
        {
          pq_dequeue(queue, &proc);
          pq_enqueue(os->scheduler->queues + itq - 1, proc);
        }
      }
      max_wait_time = clamp(max_wait_time, 1, INT32_MAX / 8) * 8;
    }

    while (1)
    {
      bool continue_parent = false;

      // # Checking devices for finished jobs.
      // This must be inside the loop, because
      // there are immediate devices that don't
      // use any time at all, but they do preempt
      // the CPU from the process that asked for IO.
      for (int it = 0; it < MAX_NUMBER_OF_DEVICES; it++)
      {
        device *device = os->devices + it;
        if (device->is_connected && device->current_job_end == time)
        {
          process *proc = device->current_process;
          proc->ready_since = time;

          // Executing the pending operation
          storage_device_operation *st_op = &(proc->store_op);
          if (proc->pending_op_type == OP_PAGE_LOAD)
          {
            // the page load operation will start another IO operation
            // to load the desired page to the indicated frame, but
            // it will not enqueue the operation, it will just do it
            // since it already has the device...
            // if it was possible to load a page from another device
            // then we would have to enqueue on that device
            int pt_index = st_op->page_number;
            int ft_index = st_op->frame_number;
            // pretend that we used st_op info... not using because the simulation doesn't need it
            // but it were not a simulation, then we'd use them to load real data from storage
            proc->pending_op_type = OP_PAGE_LOADED;
            device->current_job_end == time + device->job_duration;
            continue;
          }
          else if (proc->pending_op_type == OP_PAGE_LOADED)
          {
            // the page loaded operation will associate the loaded
            // frame with the page
            proc->pending_op_type = OP_NONE;
            int pt_index = st_op->page_number;
            int ft_index = st_op->frame_number;
            page_table_entry *pt_entry = proc->page_table + pt_index;
            pt_entry->frame = ft_index;
            pt_entry->is_on_memory = true;
            pt_entry->last_access = time;
          }

          // What queue should this process return to?
          // depends on the kind of operation it was executing:
          // - waiting for page to load: highest possible priority
          // - normal IO: return queue indicated by the device
          process_queue *queue_to_ret_to;
          if (proc->state == PROC_STATE_WAITING_PAGE)
            queue_to_ret_to = os->scheduler->page_ready_queue;
          else
            queue_to_ret_to = os->scheduler->queues + device->ret_queue;

          // if the device uses a memory frame, unlock it
          if (st_op->frame_number >= 0)
          {
            os->frame_table[st_op->frame_number].locked = false;
            st_op->frame_number = -1;
          }

          if (pq_enqueue(queue_to_ret_to, proc) == OK)
          {
            // checking whether there is a process waiting for the device and set it as the current
            process *next_proc;
            if (pq_dequeue(device->blocked_queue, &next_proc) == OK)
            {
              device->current_process = next_proc;

              // If this device is a storage device, when reading we need a target memory frame
              // to load the data... if a frame number was not provided, then find a free frame.
              // Finding a free frame may cause a process to be swapped-out if memory is full.
              if (next_proc->pending_op_type == OP_PAGE_LOAD && next_proc->store_op.frame_number < 0)
              {
                // TODO: find free frame will not cause swap-out,
                // it will enqueue the process in a wait-for-free-frame queue,
                // and just wait until a free-frame is provided by the OS

                storage_device_find_free_frame();

                continue_parent = true;
                break;
              }

              device->current_job_end = time + device->job_duration;
            }
            else
              device->current_process = NULL;
          }
        }
      }

      if (continue_parent)
        continue;

      // # Swap-out processes to fulfill the requirements of procs waiting for a free memory frame
      if (os->wait_frame_queue->count > 0)
      {
        // TODO: swap-out processes
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

            int free_ws_index = -1;
            for (int itW = 0; itW < os->max_working_set; itW++)
              if (proc->working_set[itW] >= 0 && free_ws_index < 0)
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
              }
              else
              {
                // we can now write the frame to disk, and then reuse the frame for another page
                page_table_entry *lru_pt_entry = proc->page_table + lru_pt_index;
                frame_table_entry *lru_frame = os->frame_table + lru_pt_entry->frame;
                printf("Writing page %d / frame %d to disk, to free page-table entry %d", lru_pt_index, lru_pt_entry->frame, lru_pt_index);

                int frame_to_use = lru_pt_entry->frame;
                lru_pt_entry->is_on_memory = false;
                lru_pt_entry->is_on_disk = true; // Where on disk? We assumed it is not to be known... it's there!
                lru_pt_entry->frame = -1;

                // after device finishes with the load op, the process will go to the page-ready queue
                proc->state = PROC_STATE_WAITING_PAGE;

                // setting data to remember what are the steps after the IO operation completes:
                // - load the page into the free frame and associate them
                // - continue with the program as if nothing happened
                proc->pending_op_type = OP_PAGE_LOAD;
                storage_device_operation *st_op = &(proc->store_op);
                st_op->page_number = page_number;
                st_op->frame_number = frame_to_use;

                // Before doing a write IO, we need to know what frame contains the data,
                // and lock it so that it cannot be swapped out.
                // After the operation completes the frame will be unlocked.
                lru_frame->locked = true;

                device *target_device = os->devices + swap_device;
                enqueue_on_device(time, target_device, proc);

                os->scheduler->current_process = NULL;
                continue;
              }
            }
            else
            {
              // after device finishes with the load op, the process will go to the page-ready queue
              proc->state = PROC_STATE_WAITING_PAGE;

              // setting data to remember what are the steps after the IO operation completes:
              // - load the page into the free frame and associate them
              // - continue with the program as if nothing happened
              proc->pending_op_type = OP_PAGE_LOAD;
              storage_device_operation *st_op = &(proc->store_op);
              st_op->page_number = page_number;
              st_op->frame_number = -1; // no frame will be indicated to load the data, will be decided just before reading

              // adding the process to the wait queue for the device
              device *target_device = os->devices + swap_device;
              enqueue_on_device(time, target_device, proc);
              os->scheduler->current_process = NULL;
              continue;
            }
          }
        }
        else
        {
          // if a process was not found we set the running process to NULL
          os->scheduler->current_process = NULL;
        }
      }

      // # Checking whether process has finished and disposing of used resources.
      if (os->scheduler->current_process != 0 && (*plan->is_process_finished)(plan, time, os->scheduler->current_process->pid))
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
          device* target_device = os->devices + io_requested_device;
          enqueue_on_device(time, target_device, os->scheduler->current_process);
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
