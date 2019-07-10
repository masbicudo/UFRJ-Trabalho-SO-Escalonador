#ifndef OS_H_
#define OS_H_

#include <stdbool.h>
#include "map.h"
#include "queue.h"

typedef struct os os;
typedef struct process process;
typedef struct process_queue process_queue;
typedef struct scheduler scheduler;
typedef struct device device;

// https://en.wikipedia.org/wiki/Process_management_(computing)
#define PROC_STATE_START 0
#define PROC_STATE_READY 1
#define PROC_STATE_BLOCKED 2
#define PROC_STATE_READY_SUSPEND 3
#define PROC_STATE_BLOCKED_SUSPEND 4
#define PROC_STATE_RUNNING 5
#define PROC_STATE_EXIT 6

#define PROC_STATE_WAITING_PAGE 7 // waiting for a page to be loaded from disk


#define OP_NONE 0
#define OP_PAGE_LOAD 1
#define OP_PAGE_LOADED 2

typedef struct page_table_entry page_table_entry;
typedef struct process process;
typedef struct frame_table_entry frame_table_entry;
typedef struct scheduler scheduler;
typedef struct device device;
typedef struct os os;
typedef struct storage_device_operation storage_device_operation;
typedef struct process_queue process_queue;

typedef struct page_table_entry
{
    bool is_on_memory;
    bool is_on_disk;
    int frame;
    int last_access; // real hardware set a bit to indicate that the page was used

} page_table_entry;

typedef struct storage_device_operation
{
    int page_number;  // number of the page that will be loaded
    int frame_number; // number of the frame to use

} storage_device_operation;

typedef struct process
{
    int pid;
    bool blocked;
    int current_priority; // current priority level (greater means less priority)
    int ready_since;      // when this process had become ready for the last time

    int state; // the state of the process

    unsigned int pc;              // pointer to the next instruction (we only use the part that represents the frame though)
    page_table_entry *page_table; // table that maps pages to frames
    int *working_set;             // list of pages that are mapped to memory frames

    int pending_op_type;               // operation to do when device respond
    storage_device_operation store_op; // storage operation info

} process;

typedef struct frame_table_entry
{
    bool locked; // whether the frame can be swapped out or not
    bool used;   // whether the frame is used or free

} frame_table_entry;

typedef struct scheduler
{
    process_queue *page_ready_queue; // special queue for immediate execution when a page is loaded
    process_queue *queues;           // pointer to a list of queues by priority
    int queue_count;                 // number of queues
    process *current_process;        // current process
    int time_slice_end;              // when the time slice of the current process will end

} scheduler;

typedef struct device
{
    int job_duration;             // (simulation) fixed amount of time needed to complete a job
    char *name;                   // name of the device
    process_queue *blocked_queue; // pointer to a queue of blocked processes waiting for this device
    process *current_process;     // current process using this device
    int current_job_end;          // (simulation) when the os will receive a signal indicating that the job is done
    int ret_queue;                // return queue for processes whose job on this device has finished
    bool is_connected;            // returns whether device is connected

} device;

typedef struct os
{
    map pid_map;          // map of PIDs to process pointers
    int next_pid;         // next pid number to assign
    scheduler *scheduler; // process scheduler that manages ready/running processes
    device *devices;      // list of devices connected and available to the operating system

    // global information
    int max_processes;
    int max_working_set;
    int time_slice;

    // memory management
    int frame_count;
    frame_table_entry *frame_table;

    process_queue *wait_frame_queue;

} os;

// Quando um processo quer usar um dispositivo, ele simplesmente pede ao
// sistema operacional dizendo o que quer fazer com o dispositivo.
// O sistema operacional irá colocar o processo em modo de espera e
// inserir o mesmo na fila de espera do dispositivo. Quando o
// dispositivo estiver livre, o sistema operacional recebe um aviso que
// o fará iniciar a próxima tarefa da fila.

// Quando um processo deseja

/**
 * Process queue
 */
typedef struct process_queue
{
    process **items;
    int current;
    int count;
    int capacity;

} process_queue;

static inline int pq_init(process_queue *pq, int capacity) { return queue_init((queue *)pq, capacity); }
static inline void pq_dispose(process_queue *pq) { queue_dispose((queue *)pq); }
static inline int pq_enqueue(process_queue *pq, process *item) { return queue_enqueue((queue *)pq, (void *)item); }
static inline int pq_dequeue(process_queue *pq, process **out) { return queue_dequeue((queue *)pq, (void **)out); }
static inline int pq_get(process_queue *pq, int index, process **out) { return queue_get((queue *)pq, index, (void **)out); }

int scheduler_init(scheduler *scheduler, int queue_capacity, int num_queues);
void scheduler_dispose(scheduler *scheduler);

int device_init(device *device, char *name, int job_duration, int ret_queue, int proc_queue_size);
void device_dispose(device *device);

int os_init(os *os, int max_devices, int max_processes, int max_priority_level, int max_working_set, int frame_count, int time_slice);
void os_dispose(os *os);

int enqueue_on_device(int time, device *device, process *process);
int select_next_process(scheduler *scheduler, process **out);

int process_init(process *p, int pid, int max_page_table_size, int max_working_set);
void process_dispose(process *process);

void storage_device_find_free_frame();

#endif
