#ifndef SIM_PLAN_RAND_H_
#define SIM_PLAN_RAND_H_

#include "sim_plan.h"
#include "map.h"
#include "rand.h"

typedef struct rand_sim_data rand_sim_data;

struct rand_sim_data {
  pcg32_random_t *rng;
  
  int sim_proc_count;
  int sim_proc_capacity;
  sim_proc* sim_procs;

  map pid_map;

  float prob_new_process;
  float avg_proc_duration;
  float prob_new_proc_cpu_bound;
};

void plan_rand_dispose(simulation_plan *plan);

void plan_rand_init(
    simulation_plan *plan,
    uint64_t initstate,
    uint64_t initseq,
    int max_sim_procs,
    float prob_new_process,
    float avg_proc_duration,
    float prob_new_proc_cpu_bound
);

#endif