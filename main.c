#include <stdio.h>
#include <stdlib.h>
#include "os.h"
#include "rand.h"
#include "consts.h"

#define MAX_PROCESSOS 10

pcg32_random_t r;

int scheduler_init(scheduler* scheduler, int queues_count, int queue_capacity) {
    scheduler->queues = malloc(sizeof(process_queue[queues_count]));

    for (int it = 0; it < queues_count; it++)
        pq_init(scheduler->queues + it, queue_capacity);

    return OK;
}

int device_init(device* device, char* name, int job_duration) {
    device->name = name;
    device->job_duration = job_duration;
    return OK;
}

int os_init(os* os) {
    os->devices = malloc(sizeof(device[3]));
    device_init(os->devices + 0, "Disk"   , 3 );
    device_init(os->devices + 1, "Tape"   , 8 );
    device_init(os->devices + 2, "Printer", 15);

    os->current_process = 0;
    map_init(&(os->pid_map), MAX_PROCESSOS * 2, MAX_PROCESSOS * 2 * 0.75, 2.0);
    scheduler_init(&(os->scheduler), 3, MAX_PROCESSOS);

    return OK;
}

int create_process(os* os) {
    os->devices = malloc(sizeof(device[3]));
    return OK;
}

int main()
{
    printf("hello!\n");

    // todo:
    // - inicializar a estrutura do SO
    // - criar dispositivos
    // - criar alguns processos aleatoriamente
    
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

    for (int time = 0; ; time++) {
        // pegar o processo atual
        process* p = os->current_process;

        // vai aparecer um processo novo?
        if (drand(&r) < 0.1) {
            // 
        }

        // processo em execução vai fazer I/O?
        if (p->requires_io) {
            if (drand(&r) <= p->disk_use_prob) {
                // colocar na fila do dispositivo
            }
            if (drand(&r) <= p->disk_use_prob) {
                // 
            }
            if (drand(&r) <= p->disk_use_prob) {
                // 
            }
        }

        // simulação dos dispositivos
        
    }
    free(os);
}
