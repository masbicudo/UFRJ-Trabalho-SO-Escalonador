#ifndef OS_H_
#define OS_H_

typedef struct _process process;

#include <stdbool.h>
#include "map.h"
#include "process_queue.h"

// https://en.wikipedia.org/wiki/Process_management_(computing)
#define PROC_STATE_START 0
#define PROC_STATE_READY 1
#define PROC_STATE_BLOCKED 2
#define PROC_STATE_READY_SUSPEND 3
#define PROC_STATE_BLOCKED_SUSPEND 4
#define PROC_STATE_RUNNING 5
#define PROC_STATE_EXIT 6

typedef struct _process {
    int pid;
    bool blocked;
    int remaining_duration;
    int current_priority; // current priority level (greater means less priority)
    int ready_since; // when this process had become ready for the last time

    bool requires_io;
    float avg_disk_use; // avg disk usage per unit of time
    float avg_tape_use; // avg tape usage per unit of time
    float avg_printer_use; // avg printer usage per unit of time
} process;

typedef struct {
    process_queue* queues; // pointer to a list of queues by priority
    int queue_count; // number of queues
    process* current_process; // current process
    int time_slice_end; // when the time slice of the current process will end
} scheduler;

typedef struct {
    int job_duration; // (simulation) fixed amount of time needed to complete a job
    char* name; // name of the device
    process_queue* blocked_queue; // pointer to a queue of blocked processes waiting for this device
    process* current; // current process using this device
    int current_job_end; // (simulation) when the os will receive a signal indicating that the job is done
} device;

typedef struct {
    map pid_map; // map of PIDs to process pointers
    int next_pid; // next pid number to assign
    scheduler* scheduler; // process scheduler that manages ready/running processes
    device* devices; // list of devices connected and available to the operating system
} os;

// Quando um processo quer usar um dispositivo, ele simplesmente pede ao
// sistema operacional dizendo o que quer fazer com o dispositivo.
// O sistema operacional irá colocar o processo em modo de espera e
// inserir o mesmo na fila de espera do dispositivo. Quando o
// dispositivo estiver livre, o sistema operacional recebe um aviso que
// o fará iniciar a próxima tarefa da fila.

// Quando um processo deseja 


#endif
