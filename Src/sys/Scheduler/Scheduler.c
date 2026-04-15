#include "Scheduler.h"
#include "../kernal.h" // Include the clipboard so block/unblock can use it
#include <stdio.h>

// --- INITIALIZATION ---
void initQueue(Queue* q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

// --- QUEUE LOGIC ---
bool isEmpty(Queue* q) {
    return q->count == 0;
}

void enqueue(Queue* q, int process_id) {
    if (q->count == MAX_SIZE) {
        printf("Error: Queue is full!\n");
        return;
    }
    q->rear = (q->rear + 1) % MAX_SIZE;
    q->process_ids[q->rear] = process_id;
    q->count++;
}

int dequeue(Queue* q) {
    if (isEmpty(q)) return -1;
    
    int process_id = q->process_ids[q->front];
    q->front = (q->front + 1) % MAX_SIZE;
    q->count--;
    
    return process_id;
}

// Handles removal from general queue when unblocked by a specific resource
void removeFromGeneralQueue(Queue* q, int process_id) {
    if (isEmpty(q)) return;

    int current_count = q->count;
    for (int i = 0; i < current_count; i++) {
        int temp_id = dequeue(q);
        if (temp_id != process_id) {
            enqueue(q, temp_id); // Put back if it's not the one we are unblocking
        }
    }
}

// --- RESOURCE MANAGEMENT (MUTEX LOGIC) ---

// Adds the process to BOTH queues as requested by the rubric
void blockProcess(kernal_state *state, int pid, Queue* resourceQueue) {
    printf("Process %d is blocked and moving to queues.\n", pid);
    
    enqueue(resourceQueue, pid);
    enqueue(&state->general_blocked_queue, pid);
}

// Removes from BOTH queues and puts back in ready queue
void unblockProcess(kernal_state *state, Queue* resourceQueue) {
    if (isEmpty(resourceQueue)) return;

    // Pop from the specific resource line
    int pid = dequeue(resourceQueue);
    
    // Fish it out of the general queue
    removeFromGeneralQueue(&state->general_blocked_queue, pid);
    
    // Move it back to the ready queue so the CPU can run it again
    enqueue(&state->ready_queue, pid); 
    
    printf("Process %d unblocked and moved to Ready Queue.\n", pid);
}