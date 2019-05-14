#include <stdio.h>
#include <stdlib.h>
#include "os.h"
#include "rand.h"
#include "consts.h"

pcg32_random_t r;

int scheduler_init(scheduler* scheduler, int queue_capacity) {
  scheduler->queues = malloc(sizeof(process_queue[2]));
  pq_init(scheduler->queues + 0, queue_capacity, true);
  pq_init(scheduler->queues + 1, queue_capacity, false);
  return OK;
}

int device_init(device* device, char* name, int job_duration) {
  device->name = name;
  device->job_duration = job_duration;
  device->current = 0;
  device->current_job_end = -1;
  // Initializing device blocked queue
  pq_init(device->blocked, MAX_PROCESSES, false);
  return OK;
}

int os_init(os* os) {
  os->current_process = 0;
  os->devices = malloc(sizeof(device[3]));
  device_init(os->devices + 0, "Disk"   , 3 );
  device_init(os->devices + 1, "Tape"   , 8 );
  device_init(os->devices + 2, "Printer", 15);
  map_init(&(os->pid_map), MAX_PROCESSES * 2, MAX_PROCESSES * 2 * 0.75, 2.0);
  scheduler_init(&(os->scheduler), MAX_PROCESSES);
  return OK;
}

int create_process(os* os) {
  os->devices = malloc(sizeof(device[3]));
  return OK;
}

int enqueue_on_device(int time, device* device, process* process) {
  if ((device->blocked)->count == 0 && device->current == 0) {
    // There are no processes waiting to use the device and
    // no process is currently using the device, which means we can
    // give this process the control over this device.
    device->current = process;
    device->current_job_end = time + device->job_duration;
    return OK;
  }

  // Either there is a process currently using this device
  // or there are processes on the queue waiting to get contorl
  // over it. Either way, this process must be enqueued.
  pq_enqueue(device->blocked, process);
  return OK;
}

int select_next_process(os* os, process* out) {
  scheduler* scheduler = os->scheduler;

  process_queue* priority_q = scheduler->queues + 0;
  process_queue* q          = scheduler->queues + 1;

  if (priority_q->count > 0) {
    // Fetching next process from the priority queue
    pq_dequeue(priority_q, out);
    return OK;
  }
  else if (q->count > 0) {
    //  Fetching next process from regular queue
    pq_dequeue(q, out);
    return OK;
  }
  else {
    // Both queues are empty
    return ERR_QUEUE_EMPTY;
  }
}

int main() {
  printf("hello!\n");

  // seeding the random number generator
  pcg32_srandom_r(&r, 922337231LL, 6854775827LL); // 2 very large primes

  printf("%d\n", pcg32_random_r(&r));
  return 0;

  map* map = malloc(sizeof(map));
  if (map_init(map, 16, 12, 2.0f) == OK) {
    char buffer[200];
    map_info(map, buffer, 200);
    printf("%s\n", buffer);
  }

  os* os = malloc(sizeof(os));
  os_init(os);

  // TODO: initialize OS devices
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
