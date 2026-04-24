#include "console.h"
#include "../../../emu/Console/Console.h"
#include "../../../sys/kernal.h"
#include <stdlib.h>
#include <string.h>

int console(Emulator *emu, kernal_state *state) {
    printToConsole("+--------------------------------------+");
    printToConsole("|       SCHEDULER CONFIGURATION        |");
    printToConsole("+--------------------------------------+");
    printToConsole("");

    printToConsole("  Select Scheduling Algorithm:");
    printToConsole("    [1] Round Robin (RR)");
    printToConsole("    [2] Highest Response Ratio Next (HRRN)");
    printToConsole("    [3] Multi-Level Feedback Queue (MLFQ)");
    printToConsole("");
    printToConsole("  Choice: ");
    int algo_choice;
    readFromConsole("%d", &algo_choice);
    state->current_algo = (SchedulingAlgorithm)algo_choice;

    const char *algo_name = "Unknown";
    if (algo_choice == 1) algo_name = "Round Robin (RR)";
    else if (algo_choice == 2) algo_name = "HRRN";
    else if (algo_choice == 3) algo_name = "MLFQ";
    printToConsole("  >> Algorithm: %s", algo_name);
    printToConsole("");

    // Only ask for Time Quantum if the algorithm actually uses it
    if (state->current_algo == SCHED_RR) {
        printToConsole("  Enter Time Quantum: ");
        readFromConsole("%d", &state->time_quantum);
        printToConsole("  >> Time Quantum: %d", state->time_quantum);
        printToConsole("");
    }

    printToConsole("  Enable auto release resources on process termination? [1=yes, 0=no]: ");
    readFromConsole("%d", &state->auto_release_resources);
    state->auto_release_resources = (state->auto_release_resources != 0) ? 1 : 0;
    printToConsole("  >> Auto release resources: %s", state->auto_release_resources ? "enabled" : "disabled");
    printToConsole("");

    printToConsole("  Enable skip empty lines while loading programs? [1=yes, 0=no]: ");
    readFromConsole("%d", &state->skip_empty_lines_on_load);
    state->skip_empty_lines_on_load = (state->skip_empty_lines_on_load != 0) ? 1 : 0;
    printToConsole("  >> Skip empty lines: %s", state->skip_empty_lines_on_load ? "enabled" : "disabled");
    printToConsole("");

    printToConsole("  Terminate process on syntax error (non-empty lines only)? [1=yes, 0=no]: ");
    readFromConsole("%d", &state->terminate_on_syntax_error);
    state->terminate_on_syntax_error = (state->terminate_on_syntax_error != 0) ? 1 : 0;
    printToConsole("  >> Terminate on syntax error: %s", state->terminate_on_syntax_error ? "enabled" : "disabled");
    printToConsole("");

    // Ask for the number of processes
    printToConsole("  Enter number of processes: ");
    int num_processes;
    readFromConsole("%d", &num_processes);
    state->num_scheduled_processes = num_processes;
    printToConsole("  >> Processes: %d", num_processes);
    printToConsole("");

    // Allocate the scheduled_processes array dynamically
    state->scheduled_processes = (ArrivalConfig*)malloc(num_processes * sizeof(ArrivalConfig));

    // For each process, collect arrival time and file path; PID is auto-generated
    for (int i = 0; i < num_processes; i++) {
        int pid = i + 1;

        printToConsole("  +-- Process %d (PID %d) ----------------", i + 1, pid);

        printToConsole("  | Arrival time (tick): ");
        int arrival_time;
        readFromConsole("%d", &arrival_time);

        printToConsole("  | File path: ");
        char path_buffer[256];
        readFromConsole(" %255[^\n]", path_buffer);

        state->scheduled_processes[i].spawn_time = arrival_time;
        state->scheduled_processes[i].filepath = strdup(path_buffer);
        state->scheduled_processes[i].pid = pid;

        printToConsole("  | >> PID: %d  Arrival: %d", pid, arrival_time);
        printToConsole("  | >> Path: %s", state->scheduled_processes[i].filepath);
        printToConsole("  +----------------------------------------");
        printToConsole("");
    }

    // Initialize scheduler counters
    state->rr_time_quantum_counter = 0;
    state->mlfq_time_quantum_counter = 0;
    state->active_process_queue_index = 0;

    printToConsole("+--------------------------------------+");
    printToConsole("|      CONFIGURATION COMPLETE          |");
    printToConsole("|      %d process(es) registered        |", num_processes);
    printToConsole("+--------------------------------------+");
    printToConsole("");

    return 0;
}