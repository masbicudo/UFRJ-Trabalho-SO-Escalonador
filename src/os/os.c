#include <stdlib.h>
#include "os.h"
#include "return_codes.h"
#include "safe_alloc.h"
#include "memory.h"

int scheduler_init(scheduler *scheduler, int queue_capacity, int queue_count)
{
    scheduler->current_process = 0;
    scheduler->page_ready_queue = safe_malloc(sizeof(queue), scheduler);
    pq_init(scheduler->page_ready_queue, queue_capacity);
    scheduler->queues = safe_malloc(queue_count * sizeof(queue), scheduler);
    scheduler->queue_count = queue_count;
    for (int it = 0; it < queue_count; it++)
        pq_init(scheduler->queues + it, queue_capacity);
    return OK;
}

void scheduler_dispose(scheduler *scheduler)
{
    for (int it = 0; it < scheduler->queue_count; it++)
        pq_dispose(scheduler->queues + it);
    safe_free(scheduler->queues, scheduler);
    pq_dispose(scheduler->page_ready_queue);
    safe_free(scheduler->page_ready_queue, scheduler);
}

int device_init(device *device, char *name, int job_duration, int ret_queue, int proc_queue_size)
{
    device->name = name;
    device->job_duration = job_duration;
    device->current_process = 0;
    device->current_job_end = -1;
    device->ret_queue = ret_queue;
    device->is_connected = true;
    // Initializing device blocked queue
    device->blocked_queue = safe_malloc(sizeof(queue), device);
    pq_init(device->blocked_queue, proc_queue_size);
    return OK;
}

void device_dispose(device *device)
{
    pq_dispose(device->blocked_queue);
    safe_free(device->blocked_queue, device);
}

int os_init(os *os, int max_devices, int max_processes, int max_priority_level, int max_working_set, int frame_count, int time_slice)
{
    os->next_pid = 1;
    os->devices = safe_malloc(max_devices * sizeof(device), os);
    os->max_processes = max_processes;
    map_init(&(os->pid_map), max_processes * 2, max_processes * 2 * 0.75, 2.0);
    os->scheduler = safe_malloc(sizeof(scheduler), os);
    scheduler_init(os->scheduler, max_processes, max_priority_level);

    os->max_working_set = max_working_set;
    os->max_devices = max_devices;
    os->frame_count = frame_count;

    os->frame_table = safe_malloc(frame_count * sizeof(frame_table_entry), os);
    memset(os->frame_table, 0, frame_count * sizeof(frame_table_entry));
    // reserve some frames for the OS
    for (int it = 0; it < 5; it++)
        os->frame_table[it].used = true;
    os->wait_frame_queue = safe_malloc(sizeof(process_queue), os);
    pq_init(os->wait_frame_queue, os->max_processes);

    os->time_slice = time_slice;

    return OK;
}

void os_dispose(os *os)
{
    pq_dispose(os->wait_frame_queue);
    safe_free(os->wait_frame_queue, os);
    safe_free(os->frame_table, os);

    scheduler_dispose(os->scheduler);
    safe_free(os->scheduler, os);
    map_dispose(&(os->pid_map));
    device_dispose(os->devices + 2);
    device_dispose(os->devices + 1);
    device_dispose(os->devices + 0);
    safe_free(os->devices, os);
}

int os_find_free_frame(os *os)
{
    int free_frame_number = -1;
    for (int it = 0; it < os->frame_count; it++)
    {
        if (!os->frame_table[it].used)
        {
            free_frame_number = it;
            break;
        }
    }

    return free_frame_number;
}

int exec_on_device(int time, os *os, device *device, process *process)
{
    // If this device is a storage device, when reading we need a target memory frame
    // to load the data... if a frame number was not provided, then find a free frame.
    // Finding a free frame may cause a process to be swapped-out if memory is full.
    if (process->pending_op_type == OP_PAGE_LOAD && process->store_op.frame_number < 0)
    {
        // If a frame is not found, we cannot enqueue the read operation
        // because there is no place in memory to put the data.
        // In this case, we will enqueue the process in a list of
        // processes that are waiting for a free frame, so that the
        // OS will provide it later and then continue the read operation.
        int frame = os_find_free_frame(os);
        if (frame < 0)
            return ERR_OUT_OF_MEMORY;
        process->store_op.frame_number = frame;
        os->frame_table[frame].used = true;
        os->frame_table[frame].locked = true;
    }

    device->current_process = process;
    device->current_job_end = time + device->job_duration;
    return OK;
}

int enqueue_on_device(int time, os *os, device *device, process *process)
{
    if (device->current_process == NULL && device->blocked_queue->count == 0)
    {
        // There are no processes waiting to use the device and
        // no process is currently using the device, which means we can
        // give this process the control over this device.
        return exec_on_device(time, os, device, process);
    }

    // Either there is a process currently using this device
    // or there are processes on the queue waiting to get control
    // over it. Either way, this process must be enqueued.
    int r = pq_enqueue(device->blocked_queue, process);
    return r;
}

int select_next_process(scheduler *scheduler, process **out)
{
    // we first look at the page-ready process queue
    // because it contains processees that were about
    // to run, but didn't because of a page-fault...
    // they have waited the page to load, and now,
    // they can finally reclaim their CPU time
    if (scheduler->page_ready_queue->count > 0)
    {
        pq_dequeue(scheduler->page_ready_queue, out);
        return OK;
    }

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

int process_init(process *p, int pid, int max_page_table_size, int max_working_set)
{
    p->pid = pid;

    p->blocked = 0; // not blocked

    p->current_priority = 0; // will start at priority queue 0 (greatest priority)
    p->ready_since = -1;     // never became ready (this is a new process)

    p->state = PROC_STATE_START;

    p->pc = 0;
    p->page_table = safe_malloc(max_page_table_size * sizeof(page_table_entry), p);
    memset(p->page_table, 0, max_page_table_size * sizeof(page_table_entry));
    p->working_set = safe_malloc(max_working_set * sizeof(int), p);
    memset(p->working_set, -1, max_working_set * sizeof(int));

    p->pending_op_type = OP_NONE;
    p->store_op.page_number = -1;
    p->store_op.frame_number = -1;

    return OK;
}

void process_dispose(process *process)
{
    safe_free(process->working_set, process);
    safe_free(process->page_table, process);
}
