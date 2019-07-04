#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "uthash.h"

#include "sim_plan_txt.h"

#define LOG_PROC_NEW "proc-new"
#define LOG_PROC_END "proc-end"
#define LOG_PROC_OUT "proc-out"
#define LOG_PROC_IN "proc-in "

#define ACTION_NEW 1
#define ACTION_END 2
#define ACTION_IO 3

int plan_txt_incoming_processes(simulation_plan *plan, int time)
{
  txt_sim_data *data = (txt_sim_data *)plan->data;
  int count = 0;
  timeline_entry *entry = NULL;
  while ((entry = (timeline_entry *)utarray_next(data->global_timeline, entry)))
  {
    if (entry->time == time && entry->action == ACTION_NEW)
    {
      int sim_pid = data->sim_proc_count;
      if (entry->sim_pid - 1 == sim_pid)
      {
        // allocating next available sim_pid
        data->sim_proc_count++;
        sim_proc *sim_proc = data->sim_procs + sim_pid;
        memset(sim_proc, 0, sizeof(sim_proc));

        // incrementing count
        count++;
      }
      else
      {
        printf("Error! process ids must be given in ones increment order, beginning with 1.");
        // Program exits if file pointer returns NULL.
        exit(1);
      }
    }
  }
  return count;
}
void plan_txt_create_process(simulation_plan *plan, int time, int pid)
{
  txt_sim_data *data = (txt_sim_data *)plan->data;

  // mapping pid to sim_pid
  int sim_pid;
  for (; sim_pid < data->sim_proc_count; sim_pid++)
  {
    sim_proc *sim_proc = data->sim_procs + sim_pid;
    if (sim_proc->pid == 0)
      break;
  }
  map_insert(&data->pid_map, pid, sim_pid);

  // filling the new sim_proc data:
  // - process duration
  // - IO request probability for each device
  sim_proc *sim_proc = &data->sim_procs[sim_pid];
  printf("t=%4d %s  pid=%2d  duration=%2d  disk=%f  tape=%f  printer=%f\n", time, LOG_PROC_NEW, pid);
}
int plan_txt_get_sim_pid(simulation_plan *plan, int pid)
{
  txt_sim_data *data = (txt_sim_data *)plan->data;
  int sim_pid;
  map_get(&data->pid_map, pid, &sim_pid);
  return sim_pid;
}
timeline_entry *find_entry(simulation_plan *plan, int time, int pid, int action_type)
{
  txt_sim_data *data = (txt_sim_data *)plan->data;
  int sim_pid = plan_txt_get_sim_pid(plan, pid);
  sim_proc *sim_proc = data->sim_procs + sim_pid;

  timeline_entry *entry = NULL;
  while ((entry = (timeline_entry *)utarray_next(data->global_timeline, entry)))
  {
    if (entry->time == time && entry->action == action_type && entry->sim_pid == sim_pid)
      return entry;
  }
  while ((entry = (timeline_entry *)utarray_next(data->proc_timeline, entry)))
  {
    if (entry->time == sim_proc->proc_time && entry->action == action_type && entry->sim_pid == sim_pid)
      return entry;
  }
  return 0;
}
bool plan_txt_is_process_finished(simulation_plan *plan, int time, int pid)
{
  timeline_entry *entry = find_entry(plan, time, pid, ACTION_END);
  if (entry != 0)
    return false;
  return true;
}
void plan_txt_run_one_time_unit(simulation_plan *plan, int time, int pid)
{
  txt_sim_data *data = (txt_sim_data *)plan->data;
  int sim_pid = plan_txt_get_sim_pid(plan, pid);
  sim_proc *sim_proc = data->sim_procs + sim_pid;
  sim_proc->proc_time++;
}
int plan_txt_request_io(simulation_plan *plan, int time, int pid)
{
  timeline_entry *entry = find_entry(plan, time, pid, ACTION_IO);
  if (entry != 0)
    return -1;
  return entry->device_id;
}
int match_spaces(char *code)
{
  char *ptr2 = code;
  while (*ptr2 == ' ' || *ptr2 == '\t')
    ptr2++;
  return ptr2 - code;
}
int match_non_spaces(char *code)
{
  char *ptr2 = code;
  while (*ptr2 != ' ' && *ptr2 != '\t' && *ptr2 != 0)
    ptr2++;
  return ptr2 - code;
}
int match_numbers(char *code, int *value)
{
  char *ptr2 = code;
  while (*ptr2 >= '0' && *ptr2 <= '9')
    ptr2++;
  int num_cnt = ptr2 - code;
  if (num_cnt && value != NULL)
    *value = atoi(code);
  return num_cnt;
}
bool read_char(char **code, char ch)
{
  if (**code != ch) return false;
  *code++;
  return true;
}
bool read_string(char** code, const char* str) {
  int it = 0;
  for (; str[it] != 0; it++) {
    if (str[it] != (*code)[it]) return false;
  }
  (*code) += it;
  return true;
}
bool match_head(char *code, char* name, int* value)
{
  int ret_value = 0;
  code += match_spaces(code);
  if (!read_char(&code, '[')) return false;
  code += match_spaces(code);
  if (!read_string(&code, "devices")) return false;
  code += match_numbers(code, &ret_value);
  code += match_spaces(code);
  if (!read_char(&code, ']')) return false;
  if (value) *value = ret_value;
  return true;
}
bool match_entry(char *code, int* num1, char** str2, int* str_len, int* num3, int* num4)
{
  int ret_num1 = 0;
  char* ret_str2 = 0;
  int ret_num3 = 0;
  int ret_num4 = 0;

  if (num1 != NULL)
  {
    code += match_spaces(code);

    int num_len = match_numbers(code, &ret_num1);
    if (num_len == 0)
      return false;
    code += num_len;
  }

  int ret_str_len = -1;
  if (str2 != NULL)
  {
    code += match_spaces(code);

    ret_str_len = match_non_spaces(code);
    ret_str2 = code;
    code += ret_str_len;
    if (ret_str_len <= 0)
      return false;
  }

  if (num3 != NULL)
  {
    code += match_spaces(code);

    int num_len = match_numbers(code, &ret_num3);
    if (num_len == 0)
      return false;
    code += num_len;
  }

  if (num4 != NULL)
  {
    code += match_spaces(code);

    int num_len = match_numbers(code, &ret_num4);
    if (num_len == 0)
      return false;
    code += num_len;
  }

  if (num1 != NULL) *num1 = ret_num1;
  if (str2 != NULL)
  {
    *str2 = ret_str2;
    if (str_len != NULL) *str_len = ret_str_len;
  }
  if (num3 != NULL) *num3 = ret_num3;
  if (num4 != NULL) *num4 = ret_num4;

  return true;
}
void plan_txt_dispose(simulation_plan *plan)
{
  txt_sim_data *data = (txt_sim_data *)plan->data;

  utarray_free(data->devices);
  utarray_free(data->global_timeline);
  utarray_free(data->proc_timeline);

  map_dispose(&data->pid_map);
  free(data->sim_procs);
  free(plan->data);
  free(plan);
}
char *trim(char *line)
{
  while (line[0] == ' ')
    line++;
  char *l2 = line;
  while (l2[0] != '#' && l2[0] != '\0')
    l2++;
  while (l2 - 1 > line && l2[-1] == ' ')
    l2--;
  l2[0] = '\0';
  return line;
}
void device_entry_dispose(void* ptr) {
  device_entry* entry = (device_entry*)ptr;
  free(entry->name);
}
UT_icd device_entry_ptr_icd = {sizeof(device_entry), 0, 0, device_entry_dispose};
UT_icd timeline_entry_ptr_icd = {sizeof(timeline_entry), 0, 0, 0};
void plan_txt_init(simulation_plan *plan, char *filename, int max_sim_procs)
{
  txt_sim_data *data = malloc(sizeof(txt_sim_data));
  plan->data = (void *)data;
  data->sim_proc_capacity = max_sim_procs;
  data->sim_procs = malloc(max_sim_procs * sizeof(sim_proc));
  map_init(&data->pid_map, max_sim_procs, max_sim_procs * 3 / 4, 0.75f);

  // we are going to read all the txt file at
  // once and fill a struct with the data
  FILE *fptr = fopen(filename, "r");
  if (fptr == NULL)
  {
    printf("Error! opening file");
    exit(1);
  }

  utarray_new(data->devices, &device_entry_ptr_icd);
  utarray_new(data->global_timeline, &timeline_entry_ptr_icd);
  utarray_new(data->proc_timeline, &timeline_entry_ptr_icd);

  char line[1001];
  line[1000] = 0;
  int mode = 0;
  int current_sim_pid = 0;
  int num1;
  char* str2;
  int num3;
  int num4;

  // reading the file stream
  // - empty lines and comments after char '#' are ignored
  // - everything before the first section is ignored
  // - everything that does not match a rule is ignored
  // these are the relevant sections:
  // - devices: contains a list of the available devices
  // - timeline: contains the global timeline, processes
  //             must be started from this timeline
  // - process N: contains the process timeline
  while (fgets(line, 1000, fptr))
  {
    char *trimmed = trim(line);
    int str_len;
    if (mode == 1 && match_entry(line, &num1, &str2, &str_len, &num3, &num4))
    {
      char *str_new = malloc(str_len + 1);
      strncpy(str_new, str2, str_len);
      str_new[str_len] = 0;

      // creating the device entry, and then adding to the array
      if (num1 >= 0 && num3 >= 0 && num4 >= 0)
      {
        device_entry entry;
        entry.id = num1;
        entry.name = str_new;
        entry.duration = num3;
        entry.return_queue = num4;
        utarray_push_back(data->devices, &entry);
      }
    }
    else if (mode == 2 && match_entry(line, &num1, &str2, &str_len, &num3, 0))
    {
      // 1
      int time = num1;
      // 2
      char *str_new = malloc(str_len + 1);
      strncpy(str_new, str2, str_len);
      str_new[str_len] = 0;
      // 3
      int sim_pid = num3;

      int action_id;
      int device_id = -1;
      if (strcmp("new", str_new))
      {
        action_id = ACTION_NEW;
        device_id = 0;
      }
      else if (strcmp("end", str_new))
      {
        action_id = ACTION_END;
        device_id = 0;
      }
      else
      {
        device_entry *p = NULL;
        while ((p = (device_entry *)utarray_next(data->devices, p)))
        {
          if (strcmp(p->name, str_new))
          {
            action_id = ACTION_IO;
            device_id = p->id;
          }
        }
      }
      free(str_new);

      // creating the timeline entry, and then adding to the array
      if (time >= 0 && action_id >= 0 && device_id >= 0 && sim_pid >= 0)
      {
        timeline_entry entry;
        entry.time = time;
        entry.action = action_id;
        entry.device_id = device_id;
        entry.sim_pid = sim_pid;
        utarray_push_back(data->devices, &entry);
      }
    }
    else if (mode == 3 && match_entry(line, &num1, &str2, &str_len, 0, 0))
    {
      // 1
      int time = num1;
      // 2
      char *str_new = malloc(str_len + 1);
      strncpy(str_new, str2, str_len);
      str_new[str_len] = 0;

      int action_id;
      int device_id = -1;
      if (strcmp("new", str_new))
      {
        action_id = ACTION_NEW;
        device_id = 0;
      }
      else if (strcmp("end", str_new))
      {
        action_id = ACTION_END;
        device_id = 0;
      }
      else
      {
        device_entry *p = NULL;
        while ((p = (device_entry *)utarray_next(data->devices, p)))
        {
          if (strcmp(p->name, str_new))
          {
            action_id = ACTION_IO;
            device_id = p->id;
          }
        }
      }
      free(str_new);

      // creating the timeline entry, and then adding to the array
      if (time >= 0 && action_id >= 0 && device_id >= 0 && current_sim_pid >= 0)
      {
        timeline_entry entry;
        entry.time = time;
        entry.action = action_id;
        entry.device_id = device_id;
        entry.sim_pid = current_sim_pid;
        utarray_push_back(data->proc_timeline, &entry);
      }
    }
    else if (trimmed[0] == '\0')
    {
      // empty line
    }
    else if (match_head(trimmed, "devices", 0))
    {
      mode = 1;
    }
    else if (match_head(trimmed, "timeline", 0))
    {
      mode = 2;
    }
    else if (match_head(trimmed, "process", &current_sim_pid))
    {
      mode = 3;
    }
  }
  fclose(fptr);

  plan->incoming_processes = &plan_txt_incoming_processes;
  plan->create_process = &plan_txt_create_process;
  plan->is_process_finished = &plan_txt_is_process_finished;
  plan->run_one_time_unit = &plan_txt_run_one_time_unit;
  plan->requires_io = &plan_txt_request_io;
  plan->dispose = &plan_txt_dispose;
}
