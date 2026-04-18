#include "Scheduler.h"
#include "../kernal.h" // Include the clipboard so block/unblock can use it
#include "../Memory/Memory.h" 
#include "../../emu/Mem/Mem.h"
#include <stdio.h>


SchedulingAlgorithm current_algo;
int time_quantum;
static int rr_time_quantum_counter = 0;

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



struct ArrivalConfig {
    int spawn_time;
    const char* filepath;
    int pid;
};

// We still have 3 programs, but their arrival times will be empty until the user types them
struct ArrivalConfig scheduled_processes[3] = {
    {-1, "Src/apps/User Apps/Program 1.txt", 1},
    {-1, "Src/apps/User Apps/Program 2.txt", 2},
    {-1, "Src/apps/User Apps/Program 3.txt", 3}
};

// --- INITIALIZATION FUNCTION ---
void initSchedulerConfig(void) {
    printf("=== OS SCHEDULER CONFIGURATION ===\n");
    printf("Select Scheduling Algorithm:\n");
    printf("1. Round Robin (RR)\n");
    printf("2. Highest Response Ratio Next (HRRN)\n");
    printf("3. Multi-Level Feedback Queue (MLFQ)\n");
    printf("Choice: ");
    scanf("%d", (int*)&current_algo);

    // Only ask for Time Quantum if the algorithm actually uses it
    if (current_algo == SCHED_RR) {
        printf("Enter Time Quantum: ");
        scanf("%d", &time_quantum);
    }

    // Get the dynamic spawn times
    for (int i = 0; i < 3; i++) {
        printf("Enter spawn time (clock tick) for Program %d: ", i + 1);
        scanf("%d", &scheduled_processes[i].spawn_time);
    }
    printf("==================================\n");
}

void scheduler(Emulator *emu, kernal_state *state) {
    int current_time = state->current_tick_count;

    // ==========================================
    // PHASE 1: CHECK FOR NEW ARRIVALS (Common to all algorithms)
    // ==========================================
    for (int i = 0; i < 3; i++) {
        if (scheduled_processes[i].spawn_time == current_time) {
            printf("[Tick %d] Spawning Process %d...\n", current_time, scheduled_processes[i].pid);
            
            if (load_process_to_memory(emu, scheduled_processes[i].filepath, scheduled_processes[i].pid) == 0) {
                enqueue(&state->ready_queue, scheduled_processes[i].pid);
                printf("[Tick %d] Process %d loaded to Ready Queue.\n", current_time, scheduled_processes[i].pid);
            }
        }
    }

    // ==========================================
    // PHASE 2: ROUTE TO THE SELECTED ALGORITHM
    // ==========================================
    switch (current_algo) {
        case SCHED_RR:
            schedule_RR(emu, state);
            break;
        case SCHED_HRRN:
            schedule_HRRN(emu, state);
            break;
        case SCHED_MLFQ:
            schedule_MLFQ(emu, state);
            break;
        default:
            printf("Fatal Error: Unknown scheduling algorithm.\n");
            break;
    }
}

void schedule_RR(Emulator *emu, kernal_state *state) {
    PCB *active_pcb = getActivePCB(emu);
    int current_time = state->current_tick_count;

    // 1. MANAGE THE RUNNING PROCESS
    if (active_pcb != NULL) {
        if (active_pcb->state == TERMINATED) {
            for (int i = active_pcb->bounds[0]; i <= active_pcb->bounds[1]; i++) {
                freeMemLoc(emu, i); 
            }
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            rr_time_quantum_counter = 0; 
        }
        else if (state->flags->blocked == 1) {
            active_pcb->state = BLOCKED;
            state->flags->blocked = 0; 
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            rr_time_quantum_counter = 0;
        }
        // Use the DYNAMIC time_quantum provided by the user!
        else {
            rr_time_quantum_counter++; 
            if (rr_time_quantum_counter >= time_quantum) {
                printf("[Tick %d] Process %d RR Time Slice Expired! Preempting...\n", current_time, active_pcb->ProcessID);
                active_pcb->state = READY;
                enqueue(&state->ready_queue, active_pcb->ProcessID); 
                setActivePCB(emu, NULL);
                active_pcb = NULL;
                rr_time_quantum_counter = 0;
            }
        }
    }

    // 2. PICK THE NEXT WINNER (Standard FIFO Queue for RR)
    if (active_pcb == NULL && !isEmpty(&state->ready_queue)) {
        int next_pid = dequeue(&state->ready_queue);
        int found_in_ram = 0;
        PCB *winning_pcb = NULL;
        
        // Find them in RAM or Swap them in
        for (int i = 0; i < 40; i++) {
            struct MemoryWord *word = (struct MemoryWord*)readMem(emu, i);
            if (word != NULL && word->type == MEM_TYPE_PCB && word->content.pcb_data.ProcessID == next_pid) {
                found_in_ram = 1;
                winning_pcb = &word->content.pcb_data;
                break;
            }
        }

        if (!found_in_ram) {
            if (swap_in(emu, next_pid) == 0) {
                for (int i = 0; i < 40; i++) {
                    struct MemoryWord *word = (struct MemoryWord*)readMem(emu, i);
                    if (word != NULL && word->type == MEM_TYPE_PCB && word->content.pcb_data.ProcessID == next_pid) {
                        winning_pcb = &word->content.pcb_data;
                        break;
                    }
                }
            }
        }

        winning_pcb->state = RUNNING;
        setActivePCB(emu, winning_pcb);
        rr_time_quantum_counter = 0; 
    }
}

// =================================================================
// SKELETONS FOR YOUR OTHER ALGORITHMS
// =================================================================

void schedule_HRRN(Emulator *emu, kernal_state *state) {
    PCB *active_pcb = getActivePCB(emu);     
    int current_time = state->current_tick_count;

    // ==========================================
    // 1. MANAGE THE RUNNING PROCESS
    // ==========================================
    if (active_pcb != NULL) {
        
        // CASE A: The Process Terminated
        if (active_pcb->state == TERMINATED) {
            printf("[Tick %d] Process %d Terminated. Freeing memory...\n", current_time, active_pcb->ProcessID);
            for (int i = active_pcb->bounds[0]; i <= active_pcb->bounds[1]; i++) {
                freeMemLoc(emu, i); 
            }
            setActivePCB(emu, NULL);
            active_pcb = NULL;
        }
        
        // CASE B: The Process was Blocked by a Mutex
        else if (state->flags->blocked == 1) {
            printf("[Tick %d] Process %d Blocked! Evicting from CPU...\n", current_time, active_pcb->ProcessID);
            active_pcb->state = BLOCKED;
            state->flags->blocked = 0; 
            setActivePCB(emu, NULL);
            active_pcb = NULL;
        }
        
        // CASE C: Still Running
        // We do NOTHING here! HRRN is Non-Preemptive. 
        // We let the active process keep the CPU until it finishes or blocks.
    }

    // ==========================================
    // 2. PICK THE NEXT WINNER (Calculate HRRN)
    // ==========================================
    if (active_pcb == NULL && !isEmpty(&state->ready_queue)) {
        
        float highest_ratio = -1.0;
        int winner_pid = -1;
        
        // Save the exact number of processes waiting in line
        int initial_count = state->ready_queue.count;

        // Cycle through every process in the queue
        for (int i = 0; i < initial_count; i++) {
            int process_id = dequeue(&state->ready_queue);
            PCB* process_PCB = findPCB_FromID(emu, process_id);

            if (process_PCB == NULL) {
                printf("Warning: Process %d not found in RAM during HRRN evaluation.\n", process_id);
            } 
            else {
                // 1. Find Arrival Time
                int arrival_time = 0;
                for (int j = 0; j < 3; j++) {
                    if (scheduled_processes[j].pid == process_id) {
                        arrival_time = scheduled_processes[j].spawn_time;
                        break;
                    }
                }

                // 2. Calculate Waiting and Execution Time
                int waiting_time = current_time - arrival_time;
                int execution_time = (process_PCB->bounds[3] - process_PCB->bounds[2]) + 1;
                
                // 3. Calculate Response Ratio (Must cast to float to preserve decimals!)
                float response_ratio = (float)(waiting_time + execution_time) / (float)execution_time;

                // 4. Check if it is the new High Score
                if (response_ratio > highest_ratio) {
                    highest_ratio = response_ratio;
                    winner_pid = process_id;
                }
            }

            // CRITICAL: Put the process back in line so we don't lose it!
            enqueue(&state->ready_queue, process_id);
        }

        // ==========================================
        // 3. LOAD THE WINNER INTO THE CPU
        // ==========================================
        if (winner_pid != -1) {
            // Notice how we are reusing the awesome function you built earlier!
            // This safely extracts the winner from the middle of the ready queue.
            removeFromGeneralQueue(&state->ready_queue, winner_pid);

            // Fetch the winner's PCB
            PCB *winning_pcb = findPCB_FromID(emu, winner_pid);
            
            // Just in case the winner was swapped out to the disk, bring them back
            if (winning_pcb == NULL) {
                printf("[Tick %d] Process %d is not in RAM. Swapping in...\n", current_time, winner_pid);
                swap_in(emu, winner_pid);
                winning_pcb = findPCB_FromID(emu, winner_pid);
            }

            if (winning_pcb != NULL) {
                printf("[Tick %d] CPU Given to Process %d (HRRN Ratio: %.2f).\n", current_time, winner_pid, highest_ratio);
                winning_pcb->state = RUNNING;
                setActivePCB(emu, winning_pcb);
            }
        }
    }
}

// Trackers specifically for MLFQ
static int mlfq_time_quantum_counter = 0;
static int active_process_queue_index = 0; // Tracks 'i' (0, 1, 2, or 3)

void schedule_MLFQ(Emulator *emu, kernal_state *state) {
    PCB *active_pcb = getActivePCB(emu);
    int current_time = state->current_tick_count;

    // ==========================================
    // 1. MANAGE THE RUNNING PROCESS
    // ==========================================
    if (active_pcb != NULL) {
        
        // CASE A: Terminated
        if (active_pcb->state == TERMINATED) {
            printf("[Tick %d] Process %d Terminated. Freeing memory...\n", current_time, active_pcb->ProcessID);
            for (int i = active_pcb->bounds[0]; i <= active_pcb->bounds[1]; i++) {
                freeMemLoc(emu, i); 
            }
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            mlfq_time_quantum_counter = 0;
        }
        
        // CASE B: Blocked by a Mutex (I/O)
        else if (state->flags->blocked == 1) {
            printf("[Tick %d] Process %d Blocked! Evicting...\n", current_time, active_pcb->ProcessID);
            active_pcb->state = BLOCKED;
            state->flags->blocked = 0;
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            mlfq_time_quantum_counter = 0;
        }
        
        // CASE C: Time Slice Check & DEMOTION
        else {
            mlfq_time_quantum_counter++;

            // Calculate q = 2^i based on the queue level index
            // In C, (1 << index) calculates 2 to the power of 'index'
            int current_limit = 1 << active_process_queue_index; 

            if (mlfq_time_quantum_counter >= current_limit) {
                printf("[Tick %d] Process %d (Level %d) exhausted quantum of %d! Preempting...\n", 
                       current_time, active_pcb->ProcessID, active_process_queue_index, current_limit);
                
                active_pcb->state = READY;
                
                // Demotion Logic based on the index
                if (active_process_queue_index == 0) {
                    enqueue(&state->ready_queue_1, active_pcb->ProcessID); 
                } 
                else if (active_process_queue_index == 1) {
                    enqueue(&state->ready_queue_2, active_pcb->ProcessID); 
                }
                else if (active_process_queue_index == 2) {
                    enqueue(&state->ready_queue_3, active_pcb->ProcessID); 
                }
                else if (active_process_queue_index == 3) {
                    // The last queue uses Round Robin, so it just goes to the back of the same line
                    enqueue(&state->ready_queue_3, active_pcb->ProcessID); 
                }
                
                setActivePCB(emu, NULL);
                active_pcb = NULL;
                mlfq_time_quantum_counter = 0;
            }
        }
    }

    // ==========================================
    // 2. PICK THE NEXT WINNER (Strict Priority)
    // ==========================================
    if (active_pcb == NULL) {
        int next_pid = -1;
        
        // Cascading checks: Always prioritize from highest (0) to lowest (3)
        if (!isEmpty(&state->ready_queue)) {
            next_pid = dequeue(&state->ready_queue);
            active_process_queue_index = 0;
        } 
        else if (!isEmpty(&state->ready_queue_1)) {
            next_pid = dequeue(&state->ready_queue_1);
            active_process_queue_index = 1;
        }
        else if (!isEmpty(&state->ready_queue_2)) {
            next_pid = dequeue(&state->ready_queue_2);
            active_process_queue_index = 2;
        }
        else if (!isEmpty(&state->ready_queue_3)) {
            next_pid = dequeue(&state->ready_queue_3);
            active_process_queue_index = 3;
        }

        // ==========================================
        // 3. LOAD THE WINNER INTO THE CPU
        // ==========================================
        if (next_pid != -1) {
            PCB *winning_pcb = findPCB_FromID(emu, next_pid);
            
            if (winning_pcb == NULL) {
                printf("[Tick %d] Process %d is not in RAM. Swapping in...\n", current_time, next_pid);
                swap_in(emu, next_pid);
                winning_pcb = findPCB_FromID(emu, next_pid);
            }

            if (winning_pcb != NULL) {
                printf("[Tick %d] CPU Given to Process %d from Level %d.\n", current_time, next_pid, active_process_queue_index);
                winning_pcb->state = RUNNING;
                setActivePCB(emu, winning_pcb);
            }
        }
    }
}