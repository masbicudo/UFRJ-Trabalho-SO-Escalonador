#include <stdio.h>
#include <stdlib.h>
#include "os.h"
#include "rand.h"
#include "consts.h"
#include <math.h>

int rnd_process_count(double avg);
int scheduler_init(scheduler* scheduler, int queue_capacity, int num_queues);
int device_init(device* device, char* name, int job_duration);
int os_init(os* os);

pcg32_random_t r;

int rnd_process_count(double avg) {
  // returns the number of processes that will appear in a unit of time
  // avg: the average number of processes per unit of time
  double p = (2.0*avg + 1.0 - sqrt(4.0*avg + 1.0))/2.0/avg;
  int x = 0;
  while (1) {
    double d = drand(&r);
    if (d > p) return x;
    x++;
  }
}



int scheduler_init(scheduler* scheduler, int queue_capacity, int queue_count) {
  scheduler->queues = malloc(sizeof(process_queue[queue_count]));
  scheduler->queue_count = queue_count;
  for (int it = 0; it < queue_count; it++)
    pq_init(scheduler->queues + it, queue_capacity);
  return OK;
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

int os_init(os* os) {
  os->current_process = 0;
  os->devices = malloc(sizeof(device[3]));
  device_init(os->devices + 0, "Disk"   , 3 );
  device_init(os->devices + 1, "Tape"   , 8 );
  device_init(os->devices + 2, "Printer", 15);
  map_init(&(os->pid_map), MAX_PROCESSES * 2, MAX_PROCESSES * 2 * 0.75, 2.0);
  os->scheduler = malloc(sizeof(scheduler));
  scheduler_init(os->scheduler, MAX_PROCESSES, 2);
  return OK;
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

int select_next_process(os* os, process* out) {
  scheduler* scheduler = os->scheduler;
  // iterating queues in order of priority
  for (int it = 0; it < scheduler->queue_count; it++) {
    process_queue* queue = scheduler->queues + it;
    if (queue->count > 0) {
      // get process out of the queue
      pq_dequeue(queue, &out);
      return OK;
    }
  }
  // all queues are empty
  return ERR_QUEUE_EMPTY;
}

int main() {
  printf("hello!\n");

  // seeding the random number generator
  pcg32_srandom_r(&r, 922337231LL, 6854775827LL); // 2 very large primes

  // testing RNG
  printf("%d\n", pcg32_random_r(&r));

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

  // TODO: initialize processes randomly

  for (int time = 0; ; time++) {
    if (os->current_process == 0) {
      // No current process selected.
      // Selecting from the blocked queues (priority first)
      select_next_process(os, os->current_process);
    }

    // Fetching current process information (PCB)
    process* p = os->current_process;

    // TODO Check if there are any free devices & consequently
    // any new processes ready to rejoin the ready queue

    // Are there any new processes wanting to join
    // the ready queue?
    if (drand(&r) < 0.1) {
    }

    // Does the process that's currently executing
    // want to do an IO operation in this time unit?
    if (p->requires_io) {
      bool io_was_requested = false;

      if (drand(&r) <= p->disk_use_prob) {
        enqueue_on_device(time, os->devices + 0, p);
        io_was_requested = true;
      }
      else if (drand(&r) <= p->tape_use_prob) {
        enqueue_on_device(time, os->devices + 1, p);
        io_was_requested = true;
      }
      else if (drand(&r) <= p->printer_use_prob) {
        enqueue_on_device(time, os->devices + 2, p);
        io_was_requested = true;
      }

      if (io_was_requested) {
        // Preempting current process
        os->current_process = 0;
        continue;
      }
    }
  }
  free(os);
}
