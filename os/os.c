#include <stdlib.h>
#include "os.h"
#include "return_codes.h"

int scheduler_init(scheduler *scheduler, int queue_capacity, int queue_count)
{
    scheduler->current_process = 0;
    scheduler->queues = malloc(queue_count * sizeof(queue));
    scheduler->queue_count = queue_count;
    for (int it = 0; it < queue_count; it++)
        pq_init(scheduler->queues + it, queue_capacity);
    return OK;
}

void scheduler_dispose(scheduler *scheduler)
{
    for (int it = 0; it < scheduler->queue_count; it++)
        pq_dispose(scheduler->queues + it);
    free(scheduler->queues);
}

int device_init(device *device, char *name, int job_duration, int ret_queue, int proc_queue_size)
{
    device->name = name;
    device->job_duration = job_duration;
    device->current_process = 0;
    device->current_job_end = -1;
    device->ret_queue = ret_queue;
    // Initializing device blocked queue
    device->blocked_queue = malloc(sizeof(queue));
    pq_init(device->blocked_queue, proc_queue_size);
    return OK;
}

void device_dispose(device *device)
{
    pq_dispose(device->blocked_queue);
    free(device->blocked_queue);
}

int os_init(os *os, int max_devices, int max_processes, int max_priority_level)
{
    os->next_pid = 1;
    os->devices = malloc(max_devices * sizeof(device));
    os->max_processes = max_processes;
    map_init(&(os->pid_map), max_processes * 2, max_processes * 2 * 0.75, 2.0);
    os->scheduler = malloc(sizeof(scheduler));
    scheduler_init(os->scheduler, max_processes, max_priority_level);
    return OK;
}

void os_dispose(os *os)
{
    scheduler_dispose(os->scheduler);
    free(os->scheduler);
    map_dispose(&(os->pid_map));
    device_dispose(os->devices + 2);
    device_dispose(os->devices + 1);
    device_dispose(os->devices + 0);
    free(os->devices);
}

int enqueue_on_device(int time, device *device, process *process)
{
    if ((device->blocked_queue)->count == 0 && device->current_process == 0)
    {
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

int select_next_process(scheduler *scheduler, process **out)
{
    // iterating queues in order of priority
    for (int it = 0; it < scheduler->queue_count; it++)
    {
        process_queue *queue = scheduler->queues + it;
        if (queue->count > 0)
        {
            // get process out of the queue
            pq_dequeue(queue, out);
            return OK;
        }
    }
    // all queues are empty
    return ERR_QUEUE_EMPTY;
}

int process_init(process *p, int pid, int duration, float avg_disk_use, float avg_tape_use, float avg_printer_use)
{
    p->pid = pid;

    p->blocked = 0; // not blocked

    p->remaining_duration = duration;
    p->current_priority = 0; // will start at priority queue 0 (greatest priority)
    p->ready_since = -1;     // never became ready (this is a new process)

    p->requires_io = (avg_disk_use != 0.0) || (avg_tape_use != 0.0) || (avg_printer_use != 0.0);
    p->avg_disk_use = avg_disk_use;
    p->avg_tape_use = avg_tape_use;
    p->avg_printer_use = avg_printer_use;
    return OK;
}

void process_dispose(process *process)
{
}
