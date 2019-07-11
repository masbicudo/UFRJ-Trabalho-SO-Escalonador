#ifndef QUEUE_H_
#define QUEUE_H_

typedef struct _queue queue;

typedef struct _queue {
    void** items;
    int current;
    int count;
    int capacity;
} queue;

int queue_init(queue* queue, int capacity);
void queue_dispose(queue* queue);
int queue_enqueue(queue* queue, void* process);
int queue_dequeue(queue* queue, void** out);
int queue_get(queue* queue, int index, void** out);
int queue_rev_dequeue(queue* queue, void* process);
int queue_rev_enqueue(queue* queue, void** out);
int queue_count_free(queue* queue);

#endif
