# Compiling

    gcc -I src/libs -I src/os src/os/*.c src/libs/*.c src/*.c -o a.exe -g

# Generating numbers

### Random Number Generators

- [Mersene Twister](https://en.wikipedia.org/wiki/Mersenne_Twister)
    - [Mersene Twister](https://www.cs.hmc.edu/~geoff/mtwist.html)
- [Permuted congruential generator](https://en.wikipedia.org/wiki/Permuted_congruential_generator)
    - [Implementation](http://www.pcg-random.org/)
    - [Example](https://riptutorial.com/c/example/1323/permuted-congruential-generator)

### Distributions

- [Generating Poisson-distributed random variables](https://en.wikipedia.org/wiki/Poisson_distribution#Generating_Poisson-distributed_random_variables)

### Online number generators

- [Random.org](https://www.random.org/)
- [Next prime number](https://www.dcode.fr/next-prime-number)

# Data Structures

- [uthash](http://troydhanson.github.io/uthash/index.html)
- [utarray](https://troydhanson.github.io/uthash/utarray.html)

# Console

- [ANSI escape codes](https://en.wikipedia.org/wiki/ANSI_escape_code)
- [Using ANSI escape codes on Windows, macOS and Linux terminals](https://solarianprogrammer.com/2019/04/08/c-programming-ansi-escape-codes-windows-macos-linux-terminals/)
- [Google Image search: printf color c windows GetStdHandle](https://www.google.com/search?q=printf+color+c+windows+GetStdHandle&tbm=isch)
- [Google Search: printf color c windows ENABLE_VIRTUAL_TERMINAL_PROCESSING](https://www.google.com/search?q=printf+color+c+windows+ENABLE_VIRTUAL_TERMINAL_PROCESSING)

# Debugging tools

- [Application Verifier](https://docs.microsoft.com/pt-br/windows/win32/win7appqual/application-verifier)

# TODO

- txt plan: check that all processes have an end time
- txt plan: allow unordered spid creation
- txt plan: change order of params -- time, spid, instruction, arg
- max_wait_time should come from the simulation plan

# Limitations

- No check after invoking malloc,
    this could cause out of memory bugs.

- All queues support max amount of processes,
    this is not realistic, it should grow depending on the system load.

- Last access time is not really something that can be done in real hardware,
    what is done is to set the access bit to 0 every N time units,
    and check which of them were set to 1 in the mean time.
    We are not doing this in this simulation...
    we just store the current time unit on each access.

- The wait frame queue should be limited to avoid swaping-out all processes.
    If the system has N processes waiting in the queue, then the
    OS will try to swap-out processes to free at least N frames.

## Memory
- Installed RAM is given in terms of frames, not in terms of bytes
- Swap size is not specified, we assumed no limits at all

## Assumptions

- the program starts always at the virtual address 0
- the program has no limit to the amount of virtual memory available
- there will be no writes, except for swap-out operations
- there is no Translation Lookaside Buffer
- each process has it's own page table, i.e. it is not a inverted page table
- page replacement policy is local LRU with a fixed size maximum working set
- swap in/out does not know the address of the frame inside the secondary storage
- there is only one CPU, and only one currently executing frame
- frames executing or being swapped-in/ou are marked as in_use
- each frame/page is 4KB
- the processing unit is a process, there is no threads
- the OS will reserve the 5 first frames for itself
