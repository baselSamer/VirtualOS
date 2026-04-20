#ifndef PROJECT_ETHOS_KERNAL_H
#define PROJECT_ETHOS_KERNAL_H

#include "../emu/Core/Emulator.h"
#include "Scheduler/Scheduler.h"

typedef struct Node {
    char *path;
    int id;
    struct Node* next;
} Node;

typedef struct Mutex
{
    int ConsoleRead;
    int ConsoleWrite;
    Node *file_mutexes;

    Queue input_queue;
    Queue output_queue;
    
} Mutex;

typedef struct flags {
    int blocked;
} flags;

typedef struct kernal_state {
    int current_tick_count;
    
    flags *flags;
    
    // Mutex state
    Mutex *mutexes;

    Queue ready_queue;
    Queue ready_queue_1;
    Queue ready_queue_2;   
    Queue ready_queue_3;
    Queue general_blocked_queue;

    // Scheduler state
    SchedulingAlgorithm current_algo;
    int time_quantum;
    int rr_time_quantum_counter;
    int mlfq_time_quantum_counter;
    int active_process_queue_index;
    int num_scheduled_processes;
    ArrivalConfig *scheduled_processes;
    
} kernal_state;

int start(Emulator *emu);
int start_exution(Emulator *emu, kernal_state *state);
int createNewFlags(kernal_state *state);

#endif