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

#define log_val_i(key,col,comment) (printf($white"  %-"#col"."#col"s "$web_lightsteelblue"%2d   "$green" %s"$cdef"\n", #key, key, comment))
#define log_val_f(key,col,comment) (printf($white"  %-"#col"."#col"s "$web_lightsteelblue"%5.2f" $green" %s"$cdef"\n", #key, key, comment))
#define log_error_i(err,key,col,comment) (printf($web_lightsalmon"  %-"#col"."#col"s "$red"%2d   "$web_red" %s"$cdef"\n", #key, key, comment), (err)++)
#define log_error_f(err,key,col,comment) (printf($web_lightsalmon"  %-"#col"."#col"s "$red"%5.2f" $web_red" %s"$cdef"\n", #key, key, comment), (err)++)

bool ispowerof2(unsigned int x)
{
  return x && !(x & (x - 1));
}

int main()
{
  setANSI();

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
  if (err > 0)
  {
    printf("  "$red"%d"$cdef" errors occured!", err);
    exit(1);
  }

  const unsigned int page_bit_field = MAX_SUPPORTED_FRAMES - 1;

  simulation_plan *plan = safe_malloc(sizeof(simulation_plan), NULL);
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
    plan_txt_init(plan, "plans/one_proc_test_plan.txt", MAX_PROCESSES);
  }

  plan_os_settings conf;
  (*plan->get_os_settings)(plan, &conf);
  int time_slice = clamp(conf.time_slice, 0, MAX_TIME_SLICE);

  printf("\n");
  printf($web_skyblue"Settings:"$cdef"\n");
  log_val_i(time_slice, 10, "");

  //  it is here because the plan does not know how many queues the OS have for the CPU
  //  it's a matter of leting the plan devine the number of queues
  // allowing the sim plan to print something before starting
  // - set_time: is used to show global plan information (print the whole plan if possible)
  // - print_time_final_state: is used to show the headers of the info that will be printed at the end of each iteration
  printf("\n");
  printf($web_skyblue"Plan info:"$cdef"\n");
  if (plan->set_time != NULL)
    if (!(*plan->set_time)(plan, -1, NULL))
      return 0;

  // initializing OS structure
  os *os = safe_malloc(sizeof(struct os), NULL);
  os_init(os, MAX_NUMBER_OF_DEVICES, MAX_PROCESSES, MAX_PRIORITY_LEVEL);

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
  printf($web_skyblue"Starting simulation:"$cdef"\n");
  (*plan->print_time_final_state)(plan, -1, os);

  scheduler *sch = os->scheduler;

  // TODO: initialize processes randomly
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
        process_init(new_proc, pid);
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
    for (int itq = 1; itq < sch->queue_count; itq++)
    {
      process_queue *queue = sch->queues + itq;
      process *proc = 0;
      if (pq_get(queue, 0, &proc) == OK)
      {
        if (proc->ready_since + max_wait_time < time)
        {
          pq_dequeue(queue, &proc);
          pq_enqueue(sch->queues + itq - 1, proc);
        }
      }
      max_wait_time = clamp(max_wait_time, 1, INT32_MAX / 8) * 8;
    }

    while (1)
    {
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
          device->current_process->ready_since = time;

          // What queue should this process return to?
          // depends on the kind of operation it was executing:
          // - waiting for page to load: highest possible priority
          // - normal IO: return queue indicated by the device
          process_queue *queue_to_ret_to;
          if (device->current_process->state == PROC_STATE_WAITING_PAGE)
          {
            queue_to_ret_to = sch->page_ready_queue;
            page_load_operation *pg_load = (page_load_operation*)&(device->current_process->pending_operation);
            // change page table entry to associate the new page with the frame
            device->current_process->page_table[pg_load->page_table_index].page = pg_load->page_number;
          }
          else
          {
            queue_to_ret_to = sch->queues + device->ret_queue;
          }

          if (pq_enqueue(queue_to_ret_to, device->current_process) == OK)
          {
            // checking whether there is a process waiting for the device and set it as the current
            if (pq_dequeue(device->blocked_queue, &(device->current_process)) == OK)
              device->current_job_end = time + device->job_duration;
            else
              device->current_process = NULL;
          }
        }
      }

      // # Selecting the next process if CPU is available.
      if (sch->current_process == NULL)
      {
        process *proc;
        if (select_next_process(sch, &proc) == OK)
        {
          // check if PC points to a page that is available
          const unsigned int bit_field = MAX_SUPPORTED_FRAMES - 1;
          int page_number = (proc->pc >> 12) & page_bit_field; // 4KB is the size of each frame/page

          // each process has a table that contains info about all of its pages
          bool frame_number = -1;
          int free_page_table_entry = -1;
          for (int itT = 0; itT < os->max_working_set; itT++)
          {
            page_table_entry *entry = proc->page_table + itT;
            if (entry->page == page_number)
            {
              frame_number = entry->frame;
              break;
            }
            if (free_page_table_entry < 0 && entry->frame < 0)
              free_page_table_entry = itT;
          }

          if (frame_number >= 0)
          {
            // page found
            proc->state = PROC_STATE_RUNNING;
            sch->current_process = proc;
            sch->time_slice_end = time + time_slice;
          }
          else
          {
            // page fault

            // we need to find a free page-table entry to fill with the needed page data
            if (free_page_table_entry < 0)
            {
              // free page-table entry not found, we need to write a page to disk
              // then load the requested page into the writen entry
              int lru_page_number = -1;
              {
                page_table_entry *lru_page = NULL;
                for (int itT = 0; itT < os->max_working_set; itT++)
                {
                  page_table_entry *page_entry = proc->page_table + itT;
                  frame_table_entry *frame_entry = os->frame_table + page_entry->frame;
                  if (!frame_entry->locked && (lru_page_number < 0 || page_entry->last_access < lru_page->last_access))
                  {
                    lru_page = page_entry;
                    lru_page_number = itT;
                  }
                }
              }

              // if all page-frames are locked, then we have a problem
              // it means that the process cannot run,
              // and it cannot be suspended... the only choice is
              // to kill the process
              if (lru_page_number < 0)
              {
                // TODO: kill the process
              }
              else
              {
                // we can now write the frame to disk, and then reuse the frame for another page
                page_table_entry *lru_page = proc->page_table + lru_page_number;
                frame_table_entry *lru_frame = os->frame_table + lru_page->frame;
                printf("Writing page %d / frame %d to disk, to free page-table entry %d", lru_page->page, lru_page->frame, lru_page_number);

                // setting data to remember what are the next steps
                lru_page->page = -1; // invalid page number, to indicate that the entry is free
                proc->state = PROC_STATE_WAITING_PAGE;
                page_load_operation *pg_load = (page_load_operation *)&(proc->pending_operation);
                pg_load->page_number = page_number;

                lru_frame->locked = true;

                // Where is the page?
                // Don't know what device it is in... we assumed it is unknown
                // It just magically loads the page, end of story!
                device *target_device = os->devices + swap_device;
                // when this IO completes, the page will be free
                enqueue_on_device(time, target_device, proc);

                sch->current_process = NULL;
                continue;
              }
            }
            else
            {
              // moving current process to the device wait queue
              device *target_device = os->devices + swap_device;
              // after device finishes with the load op, the process will go to the page-ready queue
              proc->state = PROC_STATE_WAITING_PAGE;

              // Finding a frame to load the page
              // before doing IO, we need to know what frame will receive the data
              // DMA operations need to know everything beforehand
              int free_frame_number = -1;
              for (int itF = 0; itF < os->frame_count; itF++)
              {
                frame_table_entry *frame = os->frame_table + itF;
                if (frame->owner_pid == -1)
                {
                  free_frame_number = itF;
                  break;
                }
              }

              // if there is no free frame to use, then we need to swap-out a process
              if (free_frame_number < 0)
              {
                // finding a process to swap-out
                process *swapout_proc = NULL;
                for (int itD = 0; itD < MAX_NUMBER_OF_DEVICES; itD++)
                {
                  device *dev = os->devices + itD;
                  if (!dev->is_connected)
                    continue;
                  for (int itQ = 0; itQ < dev->blocked_queue->count; itQ++)
                  {
                    process *blk_proc = dev->blocked_queue->items + itQ;
                    // a process can get here with the following states:
                    // - PROC_STATE_BLOCKED_SUSPEND
                    // - PROC_STATE_WAITING_PAGE
                    // - PROC_STATE_BLOCKED
                    // We need it to be PROC_STATE_BLOCKED
                    if (blk_proc->state == PROC_STATE_BLOCKED)
                    {
                      swapout_proc = blk_proc;
                      break;
                    }
                  }
                }
              }
              else
              {
                // a frame that is target of a DMA operation cannot be swapped
                frame_table_entry *free_frame = os->frame_table + free_frame_number;
                free_frame->locked = true;
                free_frame->owner_pid = proc->pid;

                // adding the process to the wait queue for the device
                enqueue_on_device(time, target_device, proc);
                sch->current_process = NULL;
                continue;
              }
            }
          }
        }
        else
        {
          // if a process was not found we set the running process to NULL
          sch->current_process = NULL;
        }
      }

      // # Checking whether process has finished and disposing of used resources.
      if (sch->current_process != 0 && (*plan->is_process_finished)(plan, time, sch->current_process->pid))
      {
        process_dispose(sch->current_process); // asking the process to dispose it's owned resources
        safe_free(sch->current_process, os);   // disposing of used memory
        sch->current_process = NULL;           // freeing cpu
        continue;
      }

      // # Checking whether the running process must be preempted.
      if (sch->current_process != NULL && sch->time_slice_end == time)
      {
        process *preempted_proc = sch->current_process;
        sch->current_process = NULL;
        int priority = clamp(preempted_proc->current_priority + 1, 0, MAX_PRIORITY_LEVEL - 1);
        preempted_proc->current_priority = priority;
        preempted_proc->ready_since = time;
        pq_enqueue(sch->queues + priority, preempted_proc);
        continue;
      }

      // # Checking whether the process wants to do an IO operation.
      // If it wants, then it will release the CPU for another process.
      if (sch->current_process != NULL)
      {
        int io_requested_device = (*plan->requires_io)(plan, time, sch->current_process->pid);
        if (io_requested_device >= 0)
        {
          // moving current process to the device wait queue
          device* target_device = os->devices + io_requested_device;
          enqueue_on_device(time, target_device, sch->current_process);
          sch->current_process = NULL;
          continue;
        }
      }

      break;
    }

    // # Running the current process.
    // increment running process internal duration
    if (sch->current_process != NULL)
    {
      (*plan->run_one_time_unit)(plan, time, sch->current_process->pid);
    }

    // writing current state to the console
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
