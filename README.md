# OS Simulator Project

## Overview

This project simulates core components of an operating system, focusing on process scheduling and memory management using a paging mechanism. It provides a framework for loading processes defined in external files, scheduling them using a Multi-Level Queue (MLQ) scheduler (if enabled), and managing their memory requirements within simulated physical RAM and swap space when paging is enabled.

## Features

*   **Process Management:**
    *   Loads process descriptions from files ([`loader.c`](d:\git_workspace\OS_Assignment\src\loader.c)). Process files define instructions like `CALC`, `ALLOC`, `FREE`, `READ`, `WRITE`, and `MALLOC` (only if `MM_PAGING` is enabled).
    *   Represents processes using Process Control Blocks (PCBs) ([`common.h`](d:\git_workspace\OS_Assignment\include\common.h)).
    *   Simulates CPU execution of process instructions ([`cpu.c`](d:\git_workspace\OS_Assignment\src\cpu.c)).
*   **Scheduling:**
    *   Implements a Multi-Level Queue (MLQ) scheduler if `MLQ_SCHED` is defined in [`os-cfg.h`](d:\git_workspace\OS_Assignment\include\os-cfg.h) ([`sched.c`](d:\git_workspace\OS_Assignment\src\sched.c), [`sched.h`](d:\git_workspace\OS_Assignment\include\sched.h)).
    *   Supports process priorities when MLQ is enabled.
    *   Includes basic ready/run queues otherwise ([`queue.c`](d:\git_workspace\OS_Assignment\src\queue.c)).
*   **Memory Management:**
    *   Supports two modes via the `MM_PAGING` flag in [`os-cfg.h`](d:\git_workspace\OS_Assignment\include\os-cfg.h):
        *   **Paging Enabled (`MM_PAGING` defined):**
            *   Simulates physical memory (RAM) and swap space(s) ([`mm-memphy.c`](d:\git_workspace\OS_Assignment\src\mm-memphy.c), [`os-mm.h`](d:\git_workspace\OS_Assignment\include\os-mm.h)).
            *   Implements virtual memory management using paging ([`mm-vm.c`](d:\git_workspace\OS_Assignment\src\mm-vm.c), [`mm.c`](d:\git_workspace\OS_Assignment\src\mm.c)).
            *   Manages Virtual Memory Areas (VMAs) and memory regions ([`os-mm.h`](d:\git_workspace\OS_Assignment\include\os-mm.h)).
            *   Handles page allocation, page table management (PGD in [`mm_struct`](d:\git_workspace\OS_Assignment\include\os-mm.h)), and memory access (read/write) operations ([`mm.h`](d:\git_workspace\OS_Assignment\include\mm.h), [`mm-vm.c`](d:\git_workspace\OS_Assignment\src\mm-vm.c)).
            *   Supports dynamic memory allocation (`pgmalloc`) within the simulated environment ([`mm-vm.c`](d:\git_workspace\OS_Assignment\src\mm-vm.c)).
            *   Optional heap growth direction configuration via `MM_PAGING_HEAP_GODOWN` ([`os-cfg.h`](d:\git_workspace\OS_Assignment\include\os-cfg.h)).
        *   **Paging Disabled (`MM_PAGING` not defined):**
            *   Uses a simpler, likely segmented, memory model ([`mem.c`](d:\git_workspace\OS_Assignment\src\mem.c), [`mem.h`](d:\git_workspace\OS_Assignment\include\mem.h)). *(Note: This appears to be a legacy or alternative implementation)*.
*   **Concurrency:**
    *   Uses pthreads for simulating concurrent CPU execution and process loading ([`os.c`](d:\git_workspace\OS_Assignment\src\os.c)).
    *   Uses a timer mechanism for time slicing ([`timer.c`](d:\git_workspace\OS_Assignment\src\timer.c), [`timer.h`](d:\git_workspace\OS_Assignment\include\timer.h)).
*   **Configuration:**
    *   Reads simulation parameters (time slice, number of CPUs, process details, memory sizes) from a configuration file ([`os.c`](d:\git_workspace\OS_Assignment\src\os.c) - `read_config`).
*   **Debugging:**
    *   Includes options for dumping I/O operations, page tables, and memory contents (`IODUMP`, `PAGETBL_DUMP` in [`os-cfg.h`](d:\git_workspace\OS_Assignment\include\os-cfg.h)).
    *   Memory dump functions available in [`mm-memphy.c`](d:\git_workspace\OS_Assignment\src\mm-memphy.c) (`MEMPHY_dump`) and [`mem.c`](d:\git_workspace\OS_Assignment\src\mem.c) (`dump`).

## Configuration (`config.txt`)

The simulation behavior is controlled by a configuration file (e.g., `input/os_1_mlq_paging`). This file specifies:

1.  `<time_slice> <num_cpus> <num_processes>`
2.  **(Only if `MM_PAGING` is defined AND `MM_FIXED_MEMSZ` is NOT defined in [`os-cfg.h`](d:\git_workspace\OS_Assignment\include\os-cfg.h))**
    `<mem_ram_sz> <mem_swp0_sz> [mem_swp1_sz] [mem_swp2_sz] [mem_swp3_sz]` (Up to 4 swap sizes)
3.  **(Only if `MM_PAGING` AND `MM_PAGING_HEAP_GODOWN` are defined AND `MM_FIXED_MEMSZ` is NOT defined in [`os-cfg.h`](d:\git_workspace\OS_Assignment\include\os-cfg.h))**
    `<vmemsz>` (Virtual memory size for heap)
4.  For each process (repeated `<num_processes>` times):
    *   `<proc_start_time> <proc_file_name> [<proc_prio>]`
        *   `<proc_prio>` is only read if `MLQ_SCHED` is defined in [`os-cfg.h`](d:\git_workspace\OS_Assignment\include\os-cfg.h).
        *   `<proc_file_name>` is relative to the `input/proc/` directory.

**Example `config.txt` structure (assuming MLQ and Paging with dynamic memory sizes):**

```
10 2 3
1048576 16777216 0 0 0
3145728
0 p0 137
1 p1 138
2 p2 139
```

Process description files (e.g., `input/proc/p0`) contain:

1.  `<default_priority> <num_instructions>`
2.  A list of instructions (e.g., `CALC`, `ALLOC`, `MALLOC`, `FREE`, `READ`, `WRITE`) with their arguments, one per line.

## Build Instructions

Use the provided [`Makefile`](d:\git_workspace\OS_Assignment\Makefile) to build the project.

1.  **Ensure a C compiler (like GCC) and `make` are installed.**
2.  **Navigate to the project's root directory in your terminal.**
3.  **Run the make command:**
    ```bash
    make
    ```
    This will compile the source files located in the `src` directory, placing object files in an `obj` directory (created if it doesn't exist) and creating the final executable named `os` in the root directory.
4.  **To clean up build files:**
    ```bash
    make clean
    ```

## Usage Instructions

Run the compiled simulator by providing the path to the configuration file as a command-line argument:

```bash
.\os <path_to_config_file>
```

**Example:**

```bash
.\os input/os_1_mlq_paging
```

The simulator will then:
1.  Read the specified configuration file (`os.c` - `read_config`).
2.  Initialize memory structures (RAM, Swap if `MM_PAGING`) (`os.c`, `mm-memphy.c`).
3.  Initialize the scheduler (`os.c`, `sched.c`).
4.  Start the timer thread (`timer.c`).
5.  Start CPU threads (`os.c` - `cpu_routine`) and the loader thread (`os.c` - `ld_routine`).
6.  The loader thread loads processes according to their start times, initializing their memory management structures if `MM_PAGING` is enabled (`os.c`, `loader.c`, `mm.c`).
7.  CPUs fetch processes from the scheduler (`sched.c` - `get_proc`) and execute their instructions (`cpu.c` - `run`).
8.  The simulation runs until all processes are loaded and completed. Output is printed based on simulation events and debugging flags set in `os-cfg.h`.

## Code Structure

*   Makefile: Defines build rules for the project.
*   include: Header files defining structures, constants, and function prototypes.
    *   `common.h`: Common data structures (PCB, Instruction).
    *   `os-cfg.h`: Build-time configuration flags (like `MM_PAGING`, `MLQ_SCHED`).
    *   `os-mm.h`: Paging-specific memory structures (VMA, MM struct, MemPhy).
    *   `mm.h`: Main memory management interface (Paging).
    *   `mem.h`: Interface for the alternative/legacy memory model.
    *   `loader.h`: Process loader interface.
    *   `sched.h`: Scheduler interface.
    *   `queue.h`: Basic queue implementation.
    *   `timer.h`: Timer interface.
    *   `cpu.h`: CPU execution interface.
    *   `bitops.h`: Bit manipulation macros.
*   src: Source code files implementing the simulator logic.
    *   `os.c`: Main simulation driver, configuration reading, thread management.
    *   `cpu.c`: CPU instruction execution simulation.
    *   `loader.c`: Loading process code from files.
    *   `sched.c`: Scheduler implementation (MLQ if enabled).
    *   `queue.c`: Basic queue implementation.
    *   `timer.c`: Timer implementation.
    *   `mm.c`: Core memory management logic (Paging).
    *   `mm-vm.c`: Virtual memory management logic (Paging).
    *   `mm-memphy.c`: Physical memory/swap simulation (Paging).
    *   `mem.c`: Alternative/legacy memory management implementation.
    *   `paging.c`: Standalone test file (not part of the main `os` executable).
*   input: Directory for configuration and process files.
    *   Contains various configuration files (e.g., `os_1_mlq_paging`).
    *   `proc/`: Contains process description files (e.g., `p0`, `p1`).
*   `obj/`: Directory created by the Makefile to store intermediate object files.

