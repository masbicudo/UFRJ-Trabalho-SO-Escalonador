#define MAX_PROCESSES 10  // maximum number of processes that will be created by the simulation
#define MAX_TIME_SLICE 10 // this is the maximum time-slice allowed, even if the simulation-plan asks for more
#define MAX_PRIORITY_LEVEL 2
#define PROB_NEW_PROCESS 0.1
#define AVG_PROC_DURATION 10.0
#define PROB_NEW_PROC_CPU_BOUND 0.5

#define MAX_NUMBER_OF_DEVICES 10  // indicates the maximum number of devices that can be connected to the OS

#define MAX_SUPPORTED_FRAMES 1024                                 // maximum number of frames, indicates the maximum amount of RAM that can be installed
#define MAX_WORKING_SET 64                                        // invariant maximum working set
#define MAX_PAGE_TABLE_SIZE (1024 * 4 / sizeof(page_table_entry)) // size of the page table for each process, it defines the virtual memory address space
#define MAX_WAIT_FRAME_QUEUE_SIZE 64                              // maximum number of waiting processes in the wait frame queue
