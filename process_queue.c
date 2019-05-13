#include <stdlib.h>
#include <string.h>
#include "process_queue.h"
#include "consts.h"
#include "os.h"

int pq_init(process_queue* pq, int capacity) {
    pq->capacity = capacity;
    return OK;
}

int pq_enqueue(process_queue* pq, process* process) {
    if (pq->count == pq->capacity)
        return ERR_QUEUE_FULL;

    int index = (pq->current + pq->count) % pq->capacity;
    pq->items[index] = process;
    pq->count++;

    return OK;
}

int pq_dequeue(process_queue* pq, int capacity, process* out) {
    if (pq->count == 0)
        return ERR_QUEUE_EMPTY;

    out = pq->items[pq->current];
    pq->current = (pq->current + 1) % pq->capacity;
    pq->count--;

    return OK;
}
