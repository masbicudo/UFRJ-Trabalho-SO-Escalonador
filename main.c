#include <stdio.h>
#include <stdlib.h>
#include "os.h"
#include "rand.h"
#include "return_codes.h"
#include <math.h>

#define MAX_PROCESSES 10
#define MAX_TIME_SLICE 4
#define MAX_PRIORITY_LEVEL 2
#define PROB_NEW_PROCESS 0.1
#define AVG_PROC_DURATION 10.0
#define PROB_NEW_PROC_CPU_BOUND 0.5

// don't change the following consts
#define NUMBER_OF_DEVICES 3

double rnd_poisson(pcg32_random_t* r, double avg_iterations);
int scheduler_init(scheduler* scheduler, int queue_capacity, int num_queues);
int device_init(device* device, char* name, int job_duration, int ret_queue);
int os_init(os* os);

double rnd_poisson(pcg32_random_t* r, double avg_iterations) {
  // returns the number of processes that will appear in a unit of time
  // avg_iterations: the average number of iterations to get the result
  double p = 1.0 / avg_iterations;
  int x = 1;
  while (1) {
    double d = drand(r);
    if (d < p) return x / avg_iterations;
    x++;
  }
}

int scheduler_init(scheduler* scheduler, int queue_capacity, int queue_count) {
  scheduler->current_process = 0;
  scheduler->queues = malloc(sizeof(process_queue[queue_count]));
  scheduler->queue_count = queue_count;
  for (int it = 0; it < queue_count; it++)
    pq_init(scheduler->queues + it, queue_capacity);
  return OK;
}

void scheduler_dispose(scheduler* scheduler) {
  for (int it = 0; it < scheduler->queue_count; it++)
    pq_dispose(scheduler->queues + it);
  free(scheduler->queues);
}

int device_init(device* device, char* name, int job_duration, int ret_queue) {
  device->name = name;
  device->job_duration = job_duration;
  device->current_process = 0;
  device->current_job_end = -1;
  device->ret_queue = ret_queue;
  // Initializing device blocked queue
  device->blocked_queue = malloc(sizeof(process_queue));
  pq_init(device->blocked_queue, MAX_PROCESSES);
  return OK;
}

void device_dispose(device* device) {
  pq_dispose(device->blocked_queue);
  free(device->blocked_queue);
}

int os_init(os* os) {
  os->next_pid = 1;
  os->devices = malloc(sizeof(device[NUMBER_OF_DEVICES]));
  device_init(os->devices + 0, "Disk"   , 3 , 1);
  device_init(os->devices + 1, "Tape"   , 8 , 0);
  device_init(os->devices + 2, "Printer", 15, 0);
  map_init(&(os->pid_map), MAX_PROCESSES * 2, MAX_PROCESSES * 2 * 0.75, 2.0);
  os->scheduler = malloc(sizeof(scheduler));
  scheduler_init(os->scheduler, MAX_PROCESSES, MAX_PRIORITY_LEVEL);
  return OK;
}

void os_dispose(os* os) {
  scheduler_dispose(os->scheduler);
  free(os->scheduler);
  map_dispose(&(os->pid_map));
  device_dispose(os->devices + 2);
  device_dispose(os->devices + 1);
  device_dispose(os->devices + 0);
  free(os->devices);
}

int create_process(os* os) {
  return OK;
}

int enqueue_on_device(int time, device* device, process* process) {
  if ((device->blocked_queue)->count == 0 && device->current_process == 0) {
    // There are no processes waiting to use the device and
    // no process is currently using the device, which means we can
    // give this process the control over this device.
    device->current_process = process;
    device->current_job_end = time + device->job_duration;
    return OK;
  }

  // Either there is a process currently using this device
  // or there are processes on the queue waiting to get control
  // over it. Either way, this process must be enqueued.
  int r = pq_enqueue(device->blocked_queue, process);
  return r;
}

int select_next_process(scheduler* scheduler, process** out) {
  // iterating queues in order of priority
  for (int it = 0; it < scheduler->queue_count; it++) {
    process_queue* queue = scheduler->queues + it;
    if (queue->count > 0) {
      // get process out of the queue
      pq_dequeue(queue, out);
      return OK;
    }
  }
  // all queues are empty
  return ERR_QUEUE_EMPTY;
}

void process_init(process* p, int pid, int duration, float avg_disk_use, float avg_tape_use, float avg_printer_use) {
  p->pid = pid;

  p->blocked = 0; // not blocked

  p->remaining_duration = duration;
  p->current_priority = 0; // will start at priority queue 0 (greatest priority)
  p->ready_since = -1; // never became ready (this is a new process)
  
  p->requires_io = (avg_disk_use != 0.0) || (avg_tape_use != 0.0) || (avg_printer_use != 0.0);
  p->avg_disk_use = avg_disk_use;
  p->avg_tape_use = avg_tape_use;
  p->avg_printer_use = avg_printer_use;
}

void process_dispose(process* process) {
}

int clamp(int val, int min, int max) {
  if (val > max) return max;
  if (val < min) return min;
  return val;
}

int main() {
  printf("hello!\n");

  // seeding the random number generator
  pcg32_random_t* r = malloc(sizeof(pcg32_random_t));
  pcg32_srandom_r(r, 922337231LL, 6854775827LL); // 2 very large primes

  // float soma = 0;
  // float std = 0;
  // int k;
  // for (k = 0; k < 20; k++) {
  //   double v = rnd_poisson(r, 100);
  //   soma += v;
  //   std += (v - 1)*(v - 1);
  //   printf("%f\n", v);
  // }
  // printf("mean = %f\nstd = %f\n", soma / k, sqrt(std / k));
  // return 0;

  // // testing RNG
  // printf("%d\n", pcg32_random_r(r));

  // // testing hash-map
  // map* map = malloc(sizeof(map));
  // if (map_init(map, 16, 12, 2.0f) == OK) {
  //   char buffer[200];
  //   map_info(map, buffer, 200);
  //   printf("%s\n", buffer);
  // }

  os* os = malloc(sizeof(os));
  os_init(os);

  scheduler* sch = os->scheduler;

  // TODO: initialize processes randomly
  int proc_count = 0;

  for (int time = 0; ; time++) {
    // TODO: exit when no processes are alive

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
        if (prob < PROB_NEW_PROC_CPU_BOUND) {
          process_init(new_proc, os->next_pid, duration, 0, 0, 0);
        }
        else {
          prob = (prob - PROB_NEW_PROC_CPU_BOUND) / (1 - PROB_NEW_PROC_CPU_BOUND);
          float disk = prob < 0.33                 ? 0.5 : 0.0; // avg number of disk calls per time unit
          float tape = prob >= 0.33 && prob < 0.67 ? 0.3 : 0.0; // avg number of tape calls per time unit
          float printer = prob >= 0.67             ? 0.1 : 0.0; // avg number of printer calls per time unit
          process_init(new_proc, os->next_pid, duration, disk, tape, printer);
        }

        pq_enqueue(os->scheduler->queues + 0, new_proc);
      }
    }

    // # Checking whether process has finished and disposing of used resources.
    if (sch->current_process != 0 && sch->current_process->remaining_duration == 0) {
      process_dispose(sch->current_process); // asking the process to dispose it's owned resources
      free(sch->current_process); // disposing of used memory
      sch->current_process = 0; // freeing cpu
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
