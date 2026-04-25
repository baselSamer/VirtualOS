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

/* Resets the queue parameters to correctly initialize an empty queue structure. */
void initQueue(Queue* q);
/* Checks if a queue is entirely devoid of elements. */
bool isEmpty(Queue* q);
/* Appends a given Process ID to the back of the specified queue. */
void enqueue(Queue* q, int process_id);
/* Extracts and returns a Process ID from the front of the given queue. */
int dequeue(Queue* q);
/* Erases a designated Process ID from anywhere within an existing queue, leaving other members intact. */
void removeFromGeneralQueue(Queue* q, int process_id);


/* Prompts the user through the initial wizard to configure system algorithms and load processes. */
void initSchedulerConfig(Emulator *emu, struct kernal_state *state); // Prompts the user for inputs
/* Dispatches ticking control to the active algorithm logic, evaluating newly arrived or unblocked processes. */
void scheduler(Emulator *emu, struct kernal_state *state); // The main router


/* Evaluates scheduling under the Round Robin algorithm handling time quantum, resource blocking, and termination. */
void schedule_RR(Emulator *emu, struct kernal_state *state);
/* Implements Highest Response Ratio Next scheduling, picking a non-preemptive optimal process dynamically. */
void schedule_HRRN(Emulator *emu, struct kernal_state *state);
/* Invokes Multi-Level Feedback Queue scheduling logic to promote or demote process priority through aging execution. */
void schedule_MLFQ(Emulator *emu, struct kernal_state *state);

#endif