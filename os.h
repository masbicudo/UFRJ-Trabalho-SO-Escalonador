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
    int duration;

    bool requires_io;
    float disk_use_prob;
    float tape_use_prob;
    float printer_use_prob;
} process;

typedef struct {
    process_queue* queues; // pointer to a list of queues by priority
} scheduler;

typedef struct {
    int job_duration;
    char* name; // name of the device
    process_queue* blocked; // pointer to a list of blocked queues by device
    process* current;
    int current_job_end;
} device;

typedef struct {
    map pid_map; // map of PIDs to process pointers
    process* current_process; // current process
    scheduler scheduler; // process scheduler that manages ready/running processes
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
