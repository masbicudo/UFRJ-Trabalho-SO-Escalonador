#include <stdio.h>
#include <stdlib.h>
#include "os.h"
#include "rand.h"
#include "return_codes.h"
#include <math.h>

#define MAX_PROCESSES 10
#define MAX_TIME_SLICE 4
#define MAX_PRIORITY_LEVEL 2

int rnd_process_count(pcg32_random_t* r, double avg);
int scheduler_init(scheduler* scheduler, int queue_capacity, int num_queues);
int device_init(device* device, char* name, int job_duration);
int os_init(os* os);

int rnd_process_count(pcg32_random_t* r, double avg) {
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

void process_init_io(process* p, int pid, int duration, float disk_use_prob, float tape_use_prob, float printer_use_prob) {
  p->pid = pid;

  p->blocked = 0; // not blocked

  p->remaining_duration = duration;
  p->current_priority = 0; // will start at priority queue 0 (greatest priority)
  p->ready_since = -1; // never became ready (this is a new process)
  
  p->requires_io = (disk_use_prob != 0.0) || (tape_use_prob != 0.0) || (printer_use_prob != 0.0);
  p->disk_use_prob = disk_use_prob;
  p->tape_use_prob = tape_use_prob;
  p->printer_use_prob = printer_use_prob;
}

void process_init_cpu(process* p, int pid, int duration) {
  p->pid = pid;

  p->blocked = 0; // not blocked

  p->remaining_duration = duration;
  p->current_priority = 0; // will start at priority queue 0 (greatest priority)
  p->ready_since = -1; // never became ready (this is a new process)

  p->requires_io = 0;
  p->disk_use_prob = 0;
  p->tape_use_prob = 0;
  p->printer_use_prob = 0;
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
  double prob_new_processes_per_time_unit = 0.1;
  int proc_count = 0;

  for (int time = 0; ; time++) {
    // checking whether process has finished
    if (sch->current_process != 0 && sch->current_process->remaining_duration == 0) {
      process_dispose(sch->current_process);
      free(sch->current_process);
      sch->current_process = 0;
    }

    // checking what processes are waiting for too long
    int max_wait_time = 8;
    for (int itq = 1; itq < sch->queue_count; itq++) {
      process_queue* queue = sch->queues + itq;
      // Processes start moving from lower to higher priority when
      // they are ready for a long time, without being actually processed.
      // At each time unit, we check the next of each queue to see if
      // can be upgraded. Each queue has it's own maximum wait time.
      process* proc = 0;
      pq_get(queue, 0, &proc);
      if (proc->ready_since + max_wait_time < time) {
        pq_dequeue(queue, &proc);
        pq_enqueue(sch->queues + itq - 1, proc);
      }
      max_wait_time = clamp(max_wait_time, 1, INT32_MAX / 8) * 8;
    }

    // checking whether process must be preempted
    if (sch->current_process != 0 && sch->time_slice_end == time) {
      process* preempted_proc = sch->current_process;
      sch->current_process = 0;
      int priority = clamp(preempted_proc->current_priority + 1, 0, MAX_PRIORITY_LEVEL - 1);
      preempted_proc->current_priority = priority;
      pq_enqueue(sch->queues + priority, preempted_proc);
    }

    // selecting the next process if cpu is available
    if (sch->current_process == 0) {
      if (select_next_process(sch, &(sch->current_process)) == OK) {
        sch->time_slice_end = time + MAX_TIME_SLICE;
      }
    }

    // simulate incoming processes
    int new_proc_count = rnd_process_count(r, prob_new_processes_per_time_unit);
    for (int it = 0; it < new_proc_count; it++) {
      // each process can be:
      // - cpu bound
      // - io bound
      if (proc_count < MAX_PROCESSES) {
        process* new_proc = malloc(sizeof(process));
        pq_enqueue(os->scheduler->queues + 0, new_proc);
      }
    }

    // TODO Check if there are any free devices & consequently
    // any new processes ready to rejoin the ready queue

    // running the current process
    if (sch->current_process != 0) {
    }

    // Does the process that's currently executing
    // want to do an IO operation in this time unit?
    if (sch->current_process->requires_io) {
      bool io_was_requested = false;

      if (drand(r) <= sch->current_process->disk_use_prob) {
        enqueue_on_device(time, os->devices + 0, sch->current_process);
        io_was_requested = true;
      }
      else if (drand(r) <= sch->current_process->tape_use_prob) {
        enqueue_on_device(time, os->devices + 1, sch->current_process);
        io_was_requested = true;
      }
      else if (drand(r) <= sch->current_process->printer_use_prob) {
        enqueue_on_device(time, os->devices + 2, sch->current_process);
        io_was_requested = true;
      }

      if (io_was_requested) {
        // Preempting current process
        sch->current_process = 0;
        continue;
      }
    }
  }
  os_dispose(os);
  free(os);
}
