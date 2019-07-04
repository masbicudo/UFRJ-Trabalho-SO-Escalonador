#ifndef SIM_PLAN_H_
#define SIM_PLAN_H_

#include <stdbool.h>

typedef struct simulation_plan simulation_plan;
typedef struct simulation_entry simulation_entry;
typedef struct simulation_entry_type simulation_entry_type;
typedef struct sim_plan_device sim_plan_device;

typedef int sim_plan_incoming_processes(simulation_plan *plan, int time);
typedef void sim_plan_create_process(simulation_plan *plan, int time, int pid);
typedef bool sim_plan_is_process_finished(simulation_plan *plan, int time, int pid);
typedef void sim_plan_run_one_time_unit(simulation_plan *plan, int time, int pid);
typedef int sim_plan_requires_io(simulation_plan *plan, int time, int pid);
typedef bool sim_plan_create_device(simulation_plan *plan, int device_index, sim_plan_device* out);
typedef void sim_plan_dispose(simulation_plan *plan);

struct simulation_plan
{
  void *data;
  sim_plan_incoming_processes *incoming_processes;
  sim_plan_create_process *create_process;
  sim_plan_is_process_finished *is_process_finished;
  sim_plan_run_one_time_unit *run_one_time_unit;
  sim_plan_requires_io *requires_io;
  sim_plan_create_device *create_device;
  sim_plan_dispose *dispose;
};

struct sim_plan_device {
  char* name;
  int job_duration;
  int ret_queue;
};

#endif