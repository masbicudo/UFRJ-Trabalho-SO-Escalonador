#ifndef PROCESS_QUEUE_H_
#define PROCESS_QUEUE_H_

typedef struct _process_queue process_queue;

#include "os.h"

typedef struct _process_queue {
    process** items;
    int current;
    int count;
    int capacity;
} process_queue;

int pq_init(process_queue* pq, int capacity);
void pq_dispose(process_queue* pq);
int pq_enqueue(process_queue* pq, process* process);
int pq_dequeue(process_queue* pq, process** out);
int pq_get(process_queue* pq, int index, process** out);

#endif
