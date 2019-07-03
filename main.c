#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "math_utils.h"
#include "os.h"
#include "rand_distributions.h"
#include "return_codes.h"

#define MAX_PROCESSES 10
#define MAX_TIME_SLICE 4
#define MAX_PRIORITY_LEVEL 2
#define PROB_NEW_PROCESS 0.1
#define AVG_PROC_DURATION 10.0
#define PROB_NEW_PROC_CPU_BOUND 0.5

// don't change the following consts
#define NUMBER_OF_DEVICES 3

#define LOG_PROC_NEW "proc-new"
#define LOG_PROC_END "proc-end"
#define LOG_PROC_OUT "proc-out"
#define LOG_PROC_IN  "proc-in "

int main() {
  // seeding the random number generator
  pcg32_random_t* r = malloc(sizeof(pcg32_random_t));
  pcg32_srandom_r(r, 922337231LL, 6854775827LL); // 2 very large primes

  os* os = malloc(sizeof(os));
  os_init(os, NUMBER_OF_DEVICES, MAX_PROCESSES, MAX_PRIORITY_LEVEL);
  device_init(os->devices + 0, "Disk"   , 3 , 1, MAX_PROCESSES);
  device_init(os->devices + 1, "Tape"   , 8 , 0, MAX_PROCESSES);
  device_init(os->devices + 2, "Printer", 15, 0, MAX_PROCESSES);

  scheduler* sch = os->scheduler;

  // TODO: initialize processes randomly
  int proc_count = 0;

  for (int time = 0; ; time++) {
    if (proc_count == MAX_PROCESSES) break;

    // # Simulate incoming processes.
    int new_proc_count = drand(r) < PROB_NEW_PROCESS ? rnd_poisson(r, 10) : 0;
    for (int it = 0; it < new_proc_count; it++) {
      // each process can be:
      // - cpu bound
      // - io bound
      if (proc_count <= MAX_PROCESSES) {
        proc_count++;
        process* new_proc = malloc(sizeof(process));
        
        double prob = drand(r);
        int duration = 1 + (AVG_PROC_DURATION - 1)*rnd_poisson(r, 4);
        int pid = os->next_pid++;
        if (prob < PROB_NEW_PROC_CPU_BOUND) {
          printf("t=%4d %s  pid=%2d  duration=%2d  disk=%f  tape=%f  printer=%f\n", time, LOG_PROC_NEW, pid, duration, 0.0, 0.0, 0.0);
          process_init(new_proc, pid, duration, 0, 0, 0);
        }
        else {
          prob = (prob - PROB_NEW_PROC_CPU_BOUND) / (1 - PROB_NEW_PROC_CPU_BOUND);
          float disk = prob < 0.33                 ? 0.5 : 0.0; // avg number of disk calls per time unit
          float tape = prob >= 0.33 && prob < 0.67 ? 0.3 : 0.0; // avg number of tape calls per time unit
          float printer = prob >= 0.67             ? 0.1 : 0.0; // avg number of printer calls per time unit
          printf("t=%4d %s  pid=%2d  duration=%2d  disk=%f  tape=%f  printer=%f\n", time, LOG_PROC_NEW, pid, duration, disk, tape, printer);
          process_init(new_proc, pid, duration, disk, tape, printer);
        }
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
    for (int itq = 1; itq < sch->queue_count; itq++) {
      process_queue* queue = sch->queues + itq;
      process* proc = 0;
      if (pq_get(queue, 0, &proc) == OK) {
        if (proc->ready_since + max_wait_time < time) {
          pq_dequeue(queue, &proc);
          pq_enqueue(sch->queues + itq - 1, proc);
        }
      }
      max_wait_time = clamp(max_wait_time, 1, INT32_MAX / 8) * 8;
    }

    // # Checking devices for finished jobs.
    for (int it = 0; it < NUMBER_OF_DEVICES; it++) {
      device* device = os->devices + it;
      if (device->current_job_end == time) {
        device->current_process->ready_since = time;
        process_queue* queue_to_ret_to = sch->queues + device->ret_queue;
        if (pq_enqueue(queue_to_ret_to, device->current_process) == OK) {
          device->current_process = 0;
          // checking whether there is a process waiting for the device and set it as the current
          if (pq_dequeue(device->blocked_queue, &(device->current_process)) == OK) {
            device->current_job_end = time + device->job_duration;
          }
        }
      }
    }

    // # Checking whether process has finished and disposing of used resources.
    if (sch->current_process != 0 && sch->current_process->remaining_duration == 0) {
      process_dispose(sch->current_process); // asking the process to dispose it's owned resources
      free(sch->current_process); // disposing of used memory
      sch->current_process = 0; // freeing cpu
    }

    // # Checking whether the running process must be preempted.
    if (sch->current_process != 0 && sch->time_slice_end == time) {
      process* preempted_proc = sch->current_process;
      sch->current_process = 0;
      int priority = clamp(preempted_proc->current_priority + 1, 0, MAX_PRIORITY_LEVEL - 1);
      preempted_proc->current_priority = priority;
      pq_enqueue(sch->queues + priority, preempted_proc);
    }

    // # Selecting the next process if cpu is available.
    if (sch->current_process == 0) {
      if (select_next_process(sch, &(sch->current_process)) == OK) {
        sch->time_slice_end = time + MAX_TIME_SLICE;
      }
    }

    // # Running the current process.
    process* run = sch->current_process;
    if (run != 0) {
      run->remaining_duration--;
      
      // Does the process that's currently executing
      // want to do an IO operation at this time unit?
      if (run->requires_io) {
        bool io_was_requested = false;

        if (drand(r) <= run->avg_disk_use) {
          enqueue_on_device(time, os->devices + 0, run);
          io_was_requested = true;
        }
        else if (drand(r) <= run->avg_tape_use) {
          enqueue_on_device(time, os->devices + 1, run);
          io_was_requested = true;
        }
        else if (drand(r) <= run->avg_printer_use) {
          enqueue_on_device(time, os->devices + 2, run);
          io_was_requested = true;
        }

        if (io_was_requested) {
          // Preempting current process
          sch->current_process = 0;
        }
      }

    }
  }

  os_dispose(os);
  free(os);
}
