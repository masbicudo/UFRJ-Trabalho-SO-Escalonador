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

int rnd_poisson(pcg32_random_t* r, double avg);
int scheduler_init(scheduler* scheduler, int queue_capacity, int num_queues);
int device_init(device* device, char* name, int job_duration);
int os_init(os* os);

int rnd_poisson(pcg32_random_t* r, double avg) {
  // returns the number of processes that will appear in a unit of time
  // avg: the average number of processes per unit of time
  double p = (2.0*avg + 1.0 - sqrt(4.0*avg + 1.0))/2.0/avg;
  int x = 0;
  while (1) {
    double d = drand(r);
    if (d > p) return x;
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

int device_init(device* device, char* name, int job_duration) {
  device->name = name;
  device->job_duration = job_duration;
  device->current = 0;
  device->current_job_end = -1;
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
  os->devices = malloc(sizeof(device[3]));
  device_init(os->devices + 0, "Disk"   , 3 );
  device_init(os->devices + 1, "Tape"   , 8 );
  device_init(os->devices + 2, "Printer", 15);
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
  if ((device->blocked_queue)->count == 0 && device->current == 0) {
    // There are no processes waiting to use the device and
    // no process is currently using the device, which means we can
    // give this process the control over this device.
    device->current = process;
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

  // testing RNG
  printf("%d\n", pcg32_random_r(r));

  // testing hash-map
  map* map = malloc(sizeof(map));
  if (map_init(map, 16, 12, 2.0f) == OK) {
    char buffer[200];
    map_info(map, buffer, 200);
    printf("%s\n", buffer);
  }

  return 0;

  os* os = malloc(sizeof(os));
  os_init(os);

  scheduler* sch = os->scheduler;

  // TODO: initialize processes randomly
  int proc_count = 0;

  for (int time = 0; ; time++) {
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
      pq_get(queue, 0, &proc);
      if (proc->ready_since + max_wait_time < time) {
        pq_dequeue(queue, &proc);
        pq_enqueue(sch->queues + itq - 1, proc);
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

    // # Simulate incoming processes.
    int new_proc_count = rnd_poisson(r, PROB_NEW_PROCESS);
    for (int it = 0; it < new_proc_count; it++) {
      // each process can be:
      // - cpu bound
      // - io bound
      if (proc_count < MAX_PROCESSES) {
        process* new_proc = malloc(sizeof(process));
        
        double prob = drand(r);
        if (prob < PROB_NEW_PROC_CPU_BOUND) {
          process_init(new_proc, os->next_pid, rnd_poisson(r, AVG_PROC_DURATION), 0, 0, 0);
        }
        else {
          prob = (prob - PROB_NEW_PROC_CPU_BOUND) / (1 - PROB_NEW_PROC_CPU_BOUND);
          float disk = prob < 0.33                 ? 0.5 : 0.0; // avg number of disk calls per time unit
          float tape = prob >= 0.33 && prob < 0.67 ? 0.3 : 0.0; // avg number of tape calls per time unit
          float printer = prob >= 0.67             ? 0.1 : 0.0; // avg number of printer calls per time unit
          process_init(new_proc, os->next_pid, rnd_poisson(r, AVG_PROC_DURATION), disk, tape, printer);
        }

        pq_enqueue(os->scheduler->queues + 0, new_proc);
      }
    }

    // 

    // Running the current process.
    process* run = sch->current_process;
    if (run != 0) {
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
          run = 0;
          continue;
        }
      }
    }
  }

  os_dispose(os);
  free(os);
}
