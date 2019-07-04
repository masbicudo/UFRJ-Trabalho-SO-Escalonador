#ifndef SIM_PLAN_MANUAL_H_
#define SIM_PLAN_MANUAL_H_

#include "utarray.h"
#include "sim_plan.h"
#include "map.h"
#include "rand.h"

typedef struct txt_sim_data txt_sim_data;
typedef struct timeline_entry timeline_entry;
typedef struct device_entry device_entry;

struct timeline_entry
{
  int time;
  int action;
  int device_id;
  int sim_pid;
};
struct device_entry
{
  int id;
  char *name;
  int duration;
  int return_queue;
};

struct txt_sim_data
{
  int sim_proc_count;
  int sim_proc_capacity;
  sim_proc *sim_procs;

  map pid_map;

  UT_array *global_timeline;
  UT_array *proc_timeline;
  UT_array *devices;
};

void plan_txt_dispose(simulation_plan *plan);
void plan_txt_init(simulation_plan *plan, char *filename, int max_sim_procs);

#endif