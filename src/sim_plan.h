#ifndef SIM_PLAN_H_
#define SIM_PLAN_H_

#include <stdbool.h>

typedef struct simulation_plan simulation_plan;
typedef struct simulation_entry simulation_entry;
typedef struct simulation_entry_type simulation_entry_type;
typedef struct sim_proc sim_proc;

typedef int incoming_processes(simulation_plan *plan, int time);
typedef void create_process(simulation_plan *plan, int time, int pid);
typedef bool is_process_finished(simulation_plan *plan, int time, int pid);
typedef void run_one_time_unit(simulation_plan *plan, int time, int pid);
typedef int requires_io(simulation_plan *plan, int time, int pid);

struct simulation_plan
{
  void* data;
  incoming_processes *incoming_processes;
  create_process *create_process;
  is_process_finished *is_process_finished;
  run_one_time_unit* run_one_time_unit;
  requires_io* requires_io;
};

struct sim_proc
{
  int id;
  char *name;

  int remaining_duration;

  bool requires_io;
  float avg_disk_use;    // avg disk usage per unit of time
  float avg_tape_use;    // avg tape usage per unit of time
  float avg_printer_use; // avg printer usage per unit of time
};

#endif