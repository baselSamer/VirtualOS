#ifndef PROJECT_ETHOS_SCHEDULER_H
#define PROJECT_ETHOS_SCHEDULER_H
#include <stdbool.h>
#include "../../emu/Core/Emulator.h"

struct kernal_state;

#define MAX_SIZE 40 

typedef enum {
    SCHED_RR = 1,
    SCHED_HRRN = 2,
    SCHED_MLFQ = 3
} SchedulingAlgorithm;

typedef struct Queue {
    int process_ids[MAX_SIZE];
    int front;
    int rear;
    int count;
} Queue;

typedef struct ArrivalConfig {
    int spawn_time;
    char* filepath;
    int pid;
} ArrivalConfig;

void initQueue(Queue* q);
bool isEmpty(Queue* q);
void enqueue(Queue* q, int process_id);
int dequeue(Queue* q);
void removeFromGeneralQueue(Queue* q, int process_id);


void initSchedulerConfig(Emulator *emu, struct kernal_state *state); // Prompts the user for inputs
void scheduler(Emulator *emu, struct kernal_state *state); // The main router


void schedule_RR(Emulator *emu, struct kernal_state *state);
void schedule_HRRN(Emulator *emu, struct kernal_state *state);
void schedule_MLFQ(Emulator *emu, struct kernal_state *state);

#endif