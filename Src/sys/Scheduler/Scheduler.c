#include "Scheduler.h"
#include "../kernal.h"
#include "../Memory/Memory.h" 
#include "../../emu/Mem/Mem.h"
#include "../../apps/systemApps/Console/console.h"
#include "../../emu/Console/Console.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>


/* Resets the queue parameters to correctly initialize an empty queue structure. */
void initQueue(Queue* q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}


/* Checks if a queue is entirely devoid of elements. */
bool isEmpty(Queue* q) {
    return q->count == 0;
}

/* Appends a given Process ID to the back of the specified queue. */
void enqueue(Queue* q, int process_id) {
    if (q->count == MAX_SIZE) {
        printToConsole("Error: Queue is full!");
        return;
    }
    q->rear = (q->rear + 1) % MAX_SIZE;
    q->process_ids[q->rear] = process_id;
    q->count++;
}

/* Extracts and returns a Process ID from the front of the given queue. */
int dequeue(Queue* q) {
    if (isEmpty(q)) return -1;
    
    int process_id = q->process_ids[q->front];
    q->front = (q->front + 1) % MAX_SIZE;
    q->count--;
    
    return process_id;
}

// Handles removal from general queue when unblocked by a specific resource
/* Erases a designated Process ID from anywhere within an existing queue, leaving other members intact. */
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

/* Converts a queue's contents into a printable string format for logging. */
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

/* Prints a snapshot characterizing the current status of all scheduling queues. */
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

/* Automatically unblocks resources held by a specific process if the auto-release flag is enabled. */
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



// Adds the process to BOTH queues as requested by the rubric
/* Inserts a block process to both its targeted resource queue and the general blocked overarching queue. */
void blockProcess(kernal_state *state, int pid, Queue* resourceQueue) {
    char event[96];
    printToConsole("  | SCHED   | Process %d BLOCKED -> queues", pid);
    
    enqueue(resourceQueue, pid);
    enqueue(&state->general_blocked_queue, pid);
    snprintf(event, sizeof(event), "Process %d blocked", pid);
    printQueueSnapshot(state, event);
}

// Removes from BOTH queues and puts back in ready queue
/* Removes a process from its blocking resource queue and routes it to the suitable ready state queue. */
void unblockProcess(kernal_state *state, Queue* resourceQueue) {
    if (isEmpty(resourceQueue)) return;
    char event[96];

    int pid = dequeue(resourceQueue);
    
    removeFromGeneralQueue(&state->general_blocked_queue, pid);
    
    if (state->current_algo == SCHED_MLFQ) {
        if (state->mlfq_unblock_to_l0) {
            enqueue(&state->ready_queue, pid);
            printToConsole("  | SCHED   | Process %d UNBLOCKED -> ready queue L0", pid);
        } else {
            enqueue(&state->ready_queue_1, pid);
            printToConsole("  | SCHED   | Process %d UNBLOCKED -> ready queue L1", pid);
        }
    } else {
        enqueue(&state->ready_queue, pid);
        printToConsole("  | SCHED   | Process %d UNBLOCKED -> ready queue", pid);
    }

    snprintf(event, sizeof(event), "Process %d unblocked", pid);
    printQueueSnapshot(state, event);
}



// --- INITIALIZATION FUNCTION ---
/* Prompts the user through the initial wizard to configure system algorithms and load processes. */
void initSchedulerConfig(Emulator *emu, kernal_state *state) {
    console(emu, state);
}

/* Dispatches ticking control to the active algorithm logic, evaluating newly arrived or unblocked processes. */
void scheduler(Emulator *emu, kernal_state *state) {
    int current_time = state->current_tick_count;


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

/* Evaluates scheduling under the Round Robin algorithm handling time quantum, resource blocking, and termination. */
void schedule_RR(Emulator *emu, kernal_state *state) {
    PCB *active_pcb = getActivePCB(emu);
    int current_time = state->current_tick_count;

    if (active_pcb != NULL) {
        if (active_pcb->state == TERMINATED) {
            char event[96];
            autoReleaseProcessResources(state, active_pcb->ProcessID);
            printToConsole("  | RR      | Process %d finished, freeing memory", active_pcb->ProcessID);
            for (int i = active_pcb->bounds[0] - 1; i <= active_pcb->bounds[3]; i++) {
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

    if (active_pcb == NULL && !isEmpty(&state->ready_queue)) {
        int next_pid = dequeue(&state->ready_queue);
        int found_in_ram = 0;
        PCB *winning_pcb = NULL;
        
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



/* Implements Highest Response Ratio Next scheduling, picking a non-preemptive optimal process dynamically. */
void schedule_HRRN(Emulator *emu, kernal_state *state) {
    PCB *active_pcb = getActivePCB(emu);     
    int current_time = state->current_tick_count;

  
    if (active_pcb != NULL) {
        
        if (active_pcb->state == TERMINATED) {
            char event[96];
            autoReleaseProcessResources(state, active_pcb->ProcessID);
            printToConsole("  | HRRN    | Process %d terminated, freeing memory", active_pcb->ProcessID);
            for (int i = active_pcb->bounds[0] - 1; i <= active_pcb->bounds[3]; i++) {
                freeMemLoc(emu, i); 
            }
            snprintf(event, sizeof(event), "Process %d finished", active_pcb->ProcessID);
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            printQueueSnapshot(state, event);
        }
        
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
        

    }

 
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
                // Process is swapped out to disk — swap it back in to evaluate
                printToConsole("  | HRRN    | Process %d not in RAM, swapping in for evaluation", process_id);
                if (swap_in(emu, process_id) == 0) {
                    process_PCB = findPCB_FromID(emu, process_id);
                }
            }

            if (process_PCB != NULL) {
                //Find Arrival Time
                int arrival_time = 0;
                for (int j = 0; j < state->num_scheduled_processes; j++) {
                    if (state->scheduled_processes[j].pid == process_id) {
                        arrival_time = state->scheduled_processes[j].spawn_time;
                        break;
                    }
                }

                //Calculate Waiting and Execution Time
                int waiting_time = current_time - arrival_time;
                int execution_time = (process_PCB->bounds[3] - process_PCB->bounds[2]) + 1;
                
                float response_ratio = (float)(waiting_time + execution_time) / (float)execution_time;

                if (response_ratio > highest_ratio) {
                    highest_ratio = response_ratio;
                    winner_pid = process_id;
                }
            } else {
                printToConsole("  | HRRN    | Warning: Process %d could not be loaded, skipping", process_id);
            }

            enqueue(&state->ready_queue, process_id);
        }


        if (winner_pid != -1) {
            removeFromGeneralQueue(&state->ready_queue, winner_pid);

            PCB *winning_pcb = findPCB_FromID(emu, winner_pid);
            
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

/* Invokes Multi-Level Feedback Queue scheduling logic to promote or demote process priority through aging execution. */
void schedule_MLFQ(Emulator *emu, kernal_state *state) {
    PCB *active_pcb = getActivePCB(emu);
    int current_time = state->current_tick_count;


    if (active_pcb != NULL) {
        
        if (active_pcb->state == TERMINATED) {
            char event[96];
            autoReleaseProcessResources(state, active_pcb->ProcessID);
            printToConsole("  | MLFQ    | Process %d terminated, freeing memory", active_pcb->ProcessID);
            for (int i = active_pcb->bounds[0] - 1; i <= active_pcb->bounds[3]; i++) {
                freeMemLoc(emu, i); 
            }
            snprintf(event, sizeof(event), "Process %d finished", active_pcb->ProcessID);
            setActivePCB(emu, NULL);
            active_pcb = NULL;
            state->mlfq_time_quantum_counter = 0;
            printQueueSnapshot(state, event);
        }
        
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
        
        else {
            state->mlfq_time_quantum_counter++;

            int current_limit = 1 << state->active_process_queue_index; 

            if (state->mlfq_time_quantum_counter >= current_limit) {
                char event[96];
                int demoted_pid = active_pcb->ProcessID;
                printToConsole("  | MLFQ    | Process %d (L%d) quantum %d expired -> demoted", 
                       active_pcb->ProcessID, state->active_process_queue_index, current_limit);
                
                active_pcb->state = READY;
                
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


    if (active_pcb == NULL) {
        int next_pid = -1;
        
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