#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "math_utils.h"
#include "os.h"
#include "return_codes.h"
#include "sim_plan_rand.h"
#include "sim_plan_txt.h"
#include "ansi_console.h"

#define COLORS_ALL
#include "ansi_colors.h"

#define MAX_PROCESSES 10 // maximum number of processes that will be created by the simulation
#define MAX_TIME_SLICE 10 // this is the maximum time-slice allowed, even if the simulation-plan asks for more
#define MAX_PRIORITY_LEVEL 2
#define PROB_NEW_PROCESS 0.1
#define AVG_PROC_DURATION 10.0
#define PROB_NEW_PROC_CPU_BOUND 0.5

// don't change the following consts
#define MAX_NUMBER_OF_DEVICES 10

#define log_val_i(key,col,comment) (printf($white"  %-"#col"."#col"s "$web_lightsteelblue"%2d   "$green" %s"$cdef"\n", #key, key, comment))
#define log_val_f(key,col,comment) (printf($white"  %-"#col"."#col"s "$web_lightsteelblue"%5.2f" $green" %s"$cdef"\n", #key, key, comment))

int main()
{
  setANSI();

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
  log_val_i(MAX_NUMBER_OF_DEVICES, 23, "");

  simulation_plan *plan = malloc(sizeof(simulation_plan));
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

  printf("\n");
  printf($web_skyblue"Starting simulation:"$cdef"\n");

  os *os = malloc(sizeof(os));
  os_init(os, MAX_NUMBER_OF_DEVICES, MAX_PROCESSES, MAX_PRIORITY_LEVEL);

  for (int itdev = 0; itdev < MAX_NUMBER_OF_DEVICES; itdev++)
  {
    sim_plan_device dev;
    if (plan->create_device(plan, itdev, &dev))
      device_init(os->devices + itdev, dev.name, dev.job_duration, dev.ret_queue, MAX_PROCESSES);
    else
      (os->devices + itdev)->is_connected = false;
  }

  scheduler *sch = os->scheduler;

  // TODO: initialize processes randomly
  int proc_count = 0;

  for (int time = 0;; time++)
  {
    // telling the current time to the simulation plan
    if (plan->set_time != NULL)
      (*plan->set_time)(plan, time);

    // no more processes can be created because
    // we already created the maximum number of processes
    if (proc_count == MAX_PROCESSES)
      break;

    // # Simulate incoming processes.
    int new_proc_count = (*plan->incoming_processes)(plan, time);
    for (int it = 0; it < new_proc_count; it++)
    {
      // each process can be:
      // - cpu bound
      // - io bound
      if (proc_count <= MAX_PROCESSES)
      {
        proc_count++;
        process *new_proc = malloc(sizeof(process));

        int pid = os->next_pid++;

        (*plan->create_process)(plan, time, pid);

        process_init(new_proc, pid);
        pq_enqueue(os->scheduler->queues + 0, new_proc);
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
      // # Selecting the next process if cpu is available.
      if (sch->current_process == NULL)
      {
        if (select_next_process(sch, &(sch->current_process)) == OK)
        {
          sch->time_slice_end = time + time_slice;
        }
        else
        {
          // if a process was not found we set the running process to NULL
          sch->current_process = NULL;
        }
      }

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
          process_queue *queue_to_ret_to = sch->queues + device->ret_queue;
          if (pq_enqueue(queue_to_ret_to, device->current_process) == OK)
          {
            device->current_process = NULL;
            // checking whether there is a process waiting for the device and set it as the current
            if (pq_dequeue(device->blocked_queue, &(device->current_process)) == OK)
            {
              device->current_job_end = time + device->job_duration;
            }
          }
        }
      }

      // # Checking whether process has finished and disposing of used resources.
      if (sch->current_process != 0 && (*plan->is_process_finished)(plan, time, sch->current_process->pid))
      {
        process_dispose(sch->current_process); // asking the process to dispose it's owned resources
        free(sch->current_process);            // disposing of used memory
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
          enqueue_on_device(time, os->devices + io_requested_device, sch->current_process);
          sch->current_process = NULL;
          continue;
        }
      }

      // # Running the current process.
      // increment running process internal duration
      if (sch->current_process != NULL)
        (*plan->run_one_time_unit)(plan, time, sch->current_process->pid);

      break;
    }
  }

  os_dispose(os);
  free(os);

  (*plan->dispose)(plan);
  free(plan);
}
