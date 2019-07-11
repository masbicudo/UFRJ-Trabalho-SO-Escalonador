#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "return_codes.h"
#include "safe_alloc.h"

int queue_init(queue *queue, int capacity)
{
    queue->current = 0;
    queue->count = 0;
    queue->capacity = capacity;
    queue->items = safe_malloc(capacity * sizeof(void *), queue);
    return OK;
}

void queue_dispose(queue *queue)
{
    safe_free(queue->items, queue);
}

int queue_enqueue(queue *queue, void *item)
{
    if (queue->count == queue->capacity)
        return ERR_QUEUE_FULL;

    int index = (queue->current + queue->count) % queue->capacity;
    queue->items[index] = item;
    queue->count++;

    return OK;
}

int queue_dequeue(queue *queue, void **out)
{
    if (queue->count == 0)
        return ERR_QUEUE_EMPTY;

    *out = queue->items[queue->current];
    queue->count--;
    queue->current = (queue->current + 1) % queue->capacity;

    return OK;
}

int queue_get(queue *queue, int index, void **out)
{
    if (index < 0 || index >= queue->count)
        return ERR_OUT_OF_BOUNDS;
    *out = queue->items[(queue->current + index) % queue->capacity];
    return OK;
}

int queue_rev_dequeue(queue *queue, void *item)
{
    if (queue->count == queue->capacity)
        return ERR_QUEUE_FULL;

    int index = queue->current - 1;
    if (index < 0) index += queue->capacity;
    queue->items[index] = item;
    queue->current = index;
    queue->count++;

    return OK;
}

int queue_rev_enqueue(queue *queue, void **out)
{
    if (queue->count == 0)
        return ERR_QUEUE_EMPTY;

    int index = queue->current + queue->count - 1;
    index = index < 0 ? index + queue->capacity : index % queue->capacity;
    *out = queue->items[index];
    queue->count--;

    return OK;
}

int queue_count_free(queue *queue)
{
    return queue->capacity - queue->count;
}
