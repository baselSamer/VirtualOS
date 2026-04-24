#include "Scheduler.h"
#include "../kernal.h" // Include the clipboard so block/unblock can use it
#include "../Memory/Memory.h" 
#include "../../emu/Mem/Mem.h"
#include "../../apps/systemApps/Console/console.h"
#include "../../emu/Console/Console.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>


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
        printToConsole("Error: Queue is full!");
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

static void queueToText(const Queue *q, char *out, size_t out_size) {
    if (out_size == 0) return;
    if (q == NULL) {
        snprintf(out, out_size, "[]");
        return;
    }

    if (q->count == 0) {
        snprintf(out, out_size, "[]");
        return;
    }

    size_t used = 0;
    int written = snprintf(out, out_size, "[");
    if (written < 0) {
        out[0] = '\0';
        return;
    }
    used = (size_t)written;

    for (int i = 0; i < q->count && used < out_size; i++) {
        int idx = (q->front + i) % MAX_SIZE;
        written = snprintf(out + used, out_size - used, "%sP%d", i == 0 ? "" : ", ", q->process_ids[idx]);
        if (written < 0) {
            break;
        }
        used += (size_t)written;
    }

    if (used < out_size) {
        snprintf(out + used, out_size - used, "]");
    } else {
        out[out_size - 1] = '\0';
    }
}

static void printQueueSnapshot(kernal_state *state, const char *event_name) {
    char l0[192], l1[192], l2[192], l3[192];
    char bin[192], bout[192], bfile[192], bgeneral[192];

    queueToText(&state->ready_queue, l0, sizeof(l0));
    queueToText(&state->ready_queue_1, l1, sizeof(l1));
    queueToText(&state->ready_queue_2, l2, sizeof(l2));
    queueToText(&state->ready_queue_3, l3, sizeof(l3));
    queueToText(&state->mutexes->input_queue, bin, sizeof(bin));
    queueToText(&state->mutexes->output_queue, bout, sizeof(bout));
    queueToText(&state->mutexes->file_queue, bfile, sizeof(bfile));
    queueToText(&state->general_blocked_queue, bgeneral, sizeof(bgeneral));

    printToConsole("  | QUEUES  | %s", event_name);
    if (state->current_algo == SCHED_MLFQ) {
        printToConsole("  | QUEUES  | L0=%s  L1=%s  L2=%s  L3=%s", l0, l1, l2, l3);
    } else {
        printToConsole("  | QUEUES  | Ready=%s", l0);
    }
    printToConsole("  | QUEUES  | BlockedIn=%s  BlockedOut=%s  BlockedFile=%s  BlockedAll=%s", bin, bout, bfile, bgeneral);
}

static void autoReleaseProcessResources(kernal_state *state, int pid) {
    if (state == NULL || !state->auto_release_resources) {
        return;
    }

    int released_any = 0;

    if (state->mutexes->ConsoleRead == pid) {
        state->mutexes->ConsoleRead = -1;
        state->flags->unblocked_con_read = 1;
        released_any = 1;
    }

    if (state->mutexes->ConsoleWrite == pid) {
        state->mutexes->ConsoleWrite = -1;
        state->flags->unblocked_con_write = 1;
        released_any = 1;
    }

    if (state->mutexes->File == pid) {
        state->mutexes->File = -1;
        state->flags->unblocked_file = 1;
        released_any = 1;
    }

    if (released_any) {
        printToConsole("  | SCHED   | Auto-released resources held by P%d", pid);
    }
}

// --- RESOURCE MANAGEMENT (MUTEX LOGIC) ---

// Adds the process to BOTH queues as requested by the rubric
void blockProcess(kernal_state *state, int pid, Queue* resourceQueue) {
    char event[96];
    printToConsole("  | SCHED   | Process %d BLOCKED -> queues", pid);
    
    enqueue(resourceQueue, pid);
    enqueue(&state->general_blocked_queue, pid);
    snprintf(event, sizeof(event), "Process %d blocked", pid);
    printQueueSnapshot(state, event);
}

// Removes from BOTH queues and puts back in ready queue
void unblockProcess(kernal_state *state, Queue* resourceQueue) {
    if (isEmpty(resourceQueue)) return;
    char event[96];

    // Pop from the specific resource line
    int pid = dequeue(resourceQueue);
    
    // Fish it out of the general queue
    removeFromGeneralQueue(&state->general_blocked_queue, pid);
    
    // For MLFQ, reinsert at L1 so it does not jump back to the top queue.
    if (state->current_algo == SCHED_MLFQ) {
        enqueue(&state->ready_queue_1, pid);
        printToConsole("  | SCHED   | Process %d UNBLOCKED -> ready queue L1", pid);
    } else {
        // RR/HRRN use the single ready queue.
        enqueue(&state->ready_queue, pid);
        printToConsole("  | SCHED   | Process %d UNBLOCKED -> ready queue", pid);
    }

    snprintf(event, sizeof(event), "Process %d unblocked", pid);
    printQueueSnapshot(state, event);
}



// --- INITIALIZATION FUNCTION ---
void initSchedulerConfig(Emulator *emu, kernal_state *state) {
    console(emu, state);
}

void scheduler(Emulator *emu, kernal_state *state) {
    int current_time = state->current_tick_count;

    // ==========================================
    // PHASE 0: PROCESS UNBLOCK FLAGS
    // ==========================================
    if (state->flags->unblocked_con_read) {
        unblockProcess(state, &state->mutexes->input_queue);
        state->flags->unblocked_con_read = 0;
    }
    if (state->flags->unblocked_con_write) {
        unblockProcess(state, &state->mutexes->output_queue);
        state->flags->unblocked_con_write = 0;
    }
    if (state->flags->unblocked_file) {
        unblockProcess(state, &state->mutexes->file_queue);
        state->flags->unblocked_file = 0;
    }

    // ==========================================
    // PHASE 1: CHECK FOR NEW ARRIVALS (Common to all algorithms)
    // ==========================================
    for (int i = 0; i < state->num_scheduled_processes; i++) {
        if (state->scheduled_processes[i].spawn_time == current_time) {
            printToConsole("  | SPAWN   | Loading Process %d...", state->scheduled_processes[i].pid);
            
            if (load_process_to_memory(emu, state->scheduled_processes[i].filepath, state->scheduled_processes[i].pid, state) == 0) {
                char event[96];
                enqueue(&state->ready_queue, state->scheduled_processes[i].pid);
                printToConsole("  | SPAWN   | Process %d -> Ready Queue", state->scheduled_processes[i].pid);
                snprintf(event, sizeof(event), "Process %d arrived", state->scheduled_processes[i].pid);
                printQueueSnapshot(state, event);
            }
        }
    }

    // ==========================================
    // PHASE 2: ROUTE TO THE SELECTED ALGORITHM
    // ==========================================
    switch (state->current_algo) {
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
            printToConsole("Fatal Error: Unknown scheduling algorithm.");
            break;
    }
}

void schedule_RR(Emulator *emu, kernal_state *state) {
    PCB *active_pcb = getActivePCB(emu);
    int current_time = state->current_tick_count;

    // 1. MANAGE THE RUNNING PROCESS
    if (active_pcb != NULL) {
        if (active_pcb->state == TERMINATED) {
            char event[96];
            autoReleaseProcessResources(state, active_pcb->ProcessID);
            printToConsole("  | RR      | Process %d finished, freeing memory", active_pcb->ProcessID);
            for (int i = active_pcb->bounds[0]; i <= active_pcb->bounds[1]; i++) {
                freeMemLoc(emu, i); 
            }
            snprintf(event, sizeof(event), "Process %d finished", active_pcb->ProcessID);
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            state->rr_time_quantum_counter = 0; 
            printQueueSnapshot(state, event);
        }
        else if (state->flags->blocked != BLOCKED_NONE) {
            active_pcb->state = BLOCKED;
            
            Queue *target_q = &state->general_blocked_queue;
            if (state->flags->blocked == BLOCKED_CON_READ) target_q = &state->mutexes->input_queue;
            else if (state->flags->blocked == BLOCKED_CON_WRITE) target_q = &state->mutexes->output_queue;
            else if (state->flags->blocked == BLOCKED_FILE) {
                target_q = &state->mutexes->file_queue;
            }
            blockProcess(state, active_pcb->ProcessID, target_q);
            
            state->flags->blocked = BLOCKED_NONE; 
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            state->rr_time_quantum_counter = 0;
        }
        // Use the DYNAMIC time_quantum provided by the user!
        else {
            state->rr_time_quantum_counter++; 
            if (state->rr_time_quantum_counter >= state->time_quantum) {
                char event[96];
                int preempted_pid = active_pcb->ProcessID;
                printToConsole("  | RR      | Process %d quantum expired -> preempted", active_pcb->ProcessID);
                active_pcb->state = READY;
                enqueue(&state->ready_queue, active_pcb->ProcessID); 
                setActivePCB(emu, NULL);
                active_pcb = NULL;
                state->rr_time_quantum_counter = 0;
                snprintf(event, sizeof(event), "Process %d preempted", preempted_pid);
                printQueueSnapshot(state, event);
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
        state->rr_time_quantum_counter = 0; 
        printToConsole("  | RR      | CPU -> Process %d", next_pid);
        printQueueSnapshot(state, "Process chosen");
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
            char event[96];
            autoReleaseProcessResources(state, active_pcb->ProcessID);
            printToConsole("  | HRRN    | Process %d terminated, freeing memory", active_pcb->ProcessID);
            for (int i = active_pcb->bounds[0]; i <= active_pcb->bounds[1]; i++) {
                freeMemLoc(emu, i); 
            }
            snprintf(event, sizeof(event), "Process %d finished", active_pcb->ProcessID);
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            printQueueSnapshot(state, event);
        }
        
        // CASE B: The Process was Blocked by a Mutex
        else if (state->flags->blocked != BLOCKED_NONE) {
            printToConsole("  | HRRN    | Process %d blocked, evicting", active_pcb->ProcessID);
            active_pcb->state = BLOCKED;
            
            Queue *target_q = &state->general_blocked_queue;
            if (state->flags->blocked == BLOCKED_CON_READ) target_q = &state->mutexes->input_queue;
            else if (state->flags->blocked == BLOCKED_CON_WRITE) target_q = &state->mutexes->output_queue;
            else if (state->flags->blocked == BLOCKED_FILE) {
                target_q = &state->mutexes->file_queue;
            }
            blockProcess(state, active_pcb->ProcessID, target_q);
            
            state->flags->blocked = BLOCKED_NONE; 
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
                printToConsole("  | HRRN    | Warning: Process %d not in RAM", process_id);
            } 
            else {
                // 1. Find Arrival Time
                int arrival_time = 0;
                for (int j = 0; j < state->num_scheduled_processes; j++) {
                    if (state->scheduled_processes[j].pid == process_id) {
                        arrival_time = state->scheduled_processes[j].spawn_time;
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
                printToConsole("  | HRRN    | Process %d not in RAM, swapping in", winner_pid);
                swap_in(emu, winner_pid);
                winning_pcb = findPCB_FromID(emu, winner_pid);
            }

            if (winning_pcb != NULL) {
                printToConsole("  | HRRN    | CPU -> Process %d (ratio: %.2f)", winner_pid, highest_ratio);
                winning_pcb->state = RUNNING;
                setActivePCB(emu, winning_pcb);
                printQueueSnapshot(state, "Process chosen");
            }
        }
    }
}

void schedule_MLFQ(Emulator *emu, kernal_state *state) {
    PCB *active_pcb = getActivePCB(emu);
    int current_time = state->current_tick_count;

    // ==========================================
    // 1. MANAGE THE RUNNING PROCESS
    // ==========================================
    if (active_pcb != NULL) {
        
        // CASE A: Terminated
        if (active_pcb->state == TERMINATED) {
            char event[96];
            autoReleaseProcessResources(state, active_pcb->ProcessID);
            printToConsole("  | MLFQ    | Process %d terminated, freeing memory", active_pcb->ProcessID);
            for (int i = active_pcb->bounds[0]; i <= active_pcb->bounds[1]; i++) {
                freeMemLoc(emu, i); 
            }
            snprintf(event, sizeof(event), "Process %d finished", active_pcb->ProcessID);
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            state->mlfq_time_quantum_counter = 0;
            printQueueSnapshot(state, event);
        }
        
        // CASE B: Blocked by a Mutex (I/O)
        else if (state->flags->blocked != BLOCKED_NONE) {
            printToConsole("  | MLFQ    | Process %d blocked, evicting", active_pcb->ProcessID);
            active_pcb->state = BLOCKED;
            
            Queue *target_q = &state->general_blocked_queue;
            if (state->flags->blocked == BLOCKED_CON_READ) target_q = &state->mutexes->input_queue;
            else if (state->flags->blocked == BLOCKED_CON_WRITE) target_q = &state->mutexes->output_queue;
            else if (state->flags->blocked == BLOCKED_FILE) {
                target_q = &state->mutexes->file_queue;
            }
            blockProcess(state, active_pcb->ProcessID, target_q);
            
            state->flags->blocked = BLOCKED_NONE;
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            state->mlfq_time_quantum_counter = 0;
        }
        
        // CASE C: Time Slice Check & DEMOTION
        else {
            state->mlfq_time_quantum_counter++;

            // Calculate q = 2^i based on the queue level index
            // Level 0 -> q=1, Level 1 -> q=2, Level 2 -> q=4
            int current_limit = 1 << state->active_process_queue_index; 

            if (state->mlfq_time_quantum_counter >= current_limit) {
                char event[96];
                int demoted_pid = active_pcb->ProcessID;
                printToConsole("  | MLFQ    | Process %d (L%d) quantum %d expired -> demoted", 
                       active_pcb->ProcessID, state->active_process_queue_index, current_limit);
                
                active_pcb->state = READY;
                
                // Demotion Logic based on the index
                if (state->active_process_queue_index == 0) {
                    enqueue(&state->ready_queue_1, active_pcb->ProcessID); 
                } 
                else if (state->active_process_queue_index == 1) {
                    enqueue(&state->ready_queue_2, active_pcb->ProcessID); 
                }
                else if (state->active_process_queue_index == 2) {
                    enqueue(&state->ready_queue_3, active_pcb->ProcessID); 
                }
                else if (state->active_process_queue_index == 3) {
                    // The last queue uses Round Robin, so it just goes to the back of the same line
                    enqueue(&state->ready_queue_3, active_pcb->ProcessID); 
                }
                
                setActivePCB(emu, NULL);
                active_pcb = NULL;
                state->mlfq_time_quantum_counter = 0;
                snprintf(event, sizeof(event), "Process %d quantum expired", demoted_pid);
                printQueueSnapshot(state, event);
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
            state->active_process_queue_index = 0;
        } 
        else if (!isEmpty(&state->ready_queue_1)) {
            next_pid = dequeue(&state->ready_queue_1);
            state->active_process_queue_index = 1;
        }
        else if (!isEmpty(&state->ready_queue_2)) {
            next_pid = dequeue(&state->ready_queue_2);
            state->active_process_queue_index = 2;
        }
        else if (!isEmpty(&state->ready_queue_3)) {
            next_pid = dequeue(&state->ready_queue_3);
            state->active_process_queue_index = 3;
        }

        // ==========================================
        // 3. LOAD THE WINNER INTO THE CPU
        // ==========================================
        if (next_pid != -1) {
            PCB *winning_pcb = findPCB_FromID(emu, next_pid);
            
            if (winning_pcb == NULL) {
                printToConsole("  | MLFQ    | Process %d not in RAM, swapping in", next_pid);
                swap_in(emu, next_pid);
                winning_pcb = findPCB_FromID(emu, next_pid);
            }

            if (winning_pcb != NULL) {
                printToConsole("  | MLFQ    | CPU -> Process %d from Level %d", next_pid, state->active_process_queue_index);
                winning_pcb->state = RUNNING;
                setActivePCB(emu, winning_pcb);
                printQueueSnapshot(state, "Process chosen");
            }
        }
    }
}