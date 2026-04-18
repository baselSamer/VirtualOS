#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../emu/Files/Files.h" // Assuming you fixed the FIles typo!
#include "./Memory.h"
#include "../../emu/Mem/Mem.h"

int load_process_to_memory(Emulator *emu, const char* filepath, int new_pid) {
    void *buffer = NULL;
    
    // 1. Ask the Hardware (Emulator) to read the disk
    size_t file_size = read_raw_data(filepath, &buffer);
    if (file_size == 0 || buffer == NULL) {
        printf("Error: Could not find file %s\n", filepath);
        return -1;
    }
    
    // Cast the raw buffer to a standard C string
    char *file_text = (char *)buffer;
    
    // 2. Count the lines
    int line_count = 0;
    for (size_t i = 0; i < file_size; i++) {
        if (file_text[i] == '\n') {
            line_count++;
        }
    }
    line_count++; 
    
    int total_slots_needed = 1 + 3 + line_count;
    
    // 3. Find Space 
    int start_index = find_empty_space(emu, total_slots_needed);
    
    if (start_index == -1) {
        // MEMORY IS FULL! Trigger the swap out.
        printf("Memory full! Triggering swap for new Process %d...\n", new_pid);
        
        if (swap_out(emu) == -1) {
            printf("Fatal Error: Swap failed.\n");
            free(buffer);
            return -1;
        }
        
        // Now that we kicked someone out, look for space again!
        start_index = find_empty_space(emu, total_slots_needed);
        
        // If it's STILL -1, the new process is literally too big to fit in 40 words
        if (start_index == -1) {
            printf("Error: Process %d is too large for memory!\n", new_pid);
            free(buffer);
            return -1;
        }
    }
    
    // 4. Allocate PCB (Slot 0)
    struct MemoryWord *pcb_word = malloc(sizeof(struct MemoryWord));
    pcb_word->type = MEM_TYPE_PCB;
    pcb_word->content.pcb_data.ProcessID = new_pid;
    pcb_word->content.pcb_data.state = READY;
    pcb_word->content.pcb_data.pc = start_index + 4; 
    
    // --> MERGED FIX 1: Set the bounds! [start, total_end, code_start, total_end]
    pcb_word->content.pcb_data.bounds[0] = start_index;
    pcb_word->content.pcb_data.bounds[1] = start_index + total_slots_needed - 1;
    pcb_word->content.pcb_data.bounds[2] = start_index + 4;
    pcb_word->content.pcb_data.bounds[3] = start_index + total_slots_needed - 1;
    
    writeMem(emu, start_index, pcb_word);
    
    // 5. Allocate 3 blank Variables
    for (int i = 1; i <= 3; i++) {
        struct MemoryWord *var_word = malloc(sizeof(struct MemoryWord));
        var_word->type = MEM_TYPE_VARIABLE;
        
        // --> MERGED FIX 2: Make sure they are explicitly empty!
        var_word->content.var_data.name[0] = '\0'; 
        var_word->content.var_data.value[0] = '\0';
        
        writeMem(emu, start_index + i, var_word);
    }
    
    // 6. Split the file text by Newlines and save them as Instructions
    int current_slot = start_index + 4;
    char *line = strtok(file_text, "\n"); 
    
    while (line != NULL) {
        struct MemoryWord *inst_word = malloc(sizeof(struct MemoryWord));
        inst_word->type = MEM_TYPE_INSTRUCTION;
        
        strncpy(inst_word->content.inst_data.code_line, line, 63);
        inst_word->content.inst_data.code_line[63] = '\0'; 
        
        writeMem(emu, current_slot, inst_word);
        
        current_slot++;
        line = strtok(NULL, "\n"); 
    }
    
    free(buffer);
    return 0;
}

// Returns the starting index if it finds space, or -1 if memory is full
int find_empty_space(Emulator *emu, int required_slots) {
    int consecutive_empty = 0;
    int start_index = -1;

    for (int i = 0; i < 40; i++) { // 40 is our hardcoded physical limit
        if (readMem(emu, i) == NULL) {
            if (consecutive_empty == 0) {
                start_index = i; // Mark the start of a potential block
            }
            consecutive_empty++;

            if (consecutive_empty == required_slots) {
                return start_index; // We found enough space!
            }
        } else {
            // Hit an occupied slot, reset the counter
            consecutive_empty = 0;
            start_index = -1;
        }
    }
    
    return -1; // Not enough contiguous space found
}


// Returns 0 on success, -1 on failure
// Returns 0 on success, -1 on failure
int swap_out(Emulator *emu) {
    int victim_start = -1;
    struct MemoryWord *victim_pcb = NULL;
    
    // Get the currently running process so we NEVER evict it
    PCB *active_process = getActivePCB(emu);
    int active_pid = (active_process != NULL) ? active_process->ProcessID : -1;

    // PASS 1: Hunt for a BLOCKED process first (They are the best victims!)
    for (int i = 0; i < 40; i++) {
        struct MemoryWord *word = (struct MemoryWord *)readMem(emu, i);
        if (word != NULL && word->type == MEM_TYPE_PCB) {
            
            if (word->content.pcb_data.ProcessID == active_pid) continue; // PROTECT THE CPU!
            
            if (word->content.pcb_data.state == BLOCKED) {
                victim_start = i;
                victim_pcb = word;
                break;
            }
        }
    }

    // PASS 2: If no blocked process was found, settle for ANY process that isn't running
    if (victim_start == -1) {
        for (int i = 0; i < 40; i++) {
            struct MemoryWord *word = (struct MemoryWord *)readMem(emu, i);
            if (word != NULL && word->type == MEM_TYPE_PCB) {
                
                if (word->content.pcb_data.ProcessID == active_pid) continue; // PROTECT THE CPU!
                
                victim_start = i;
                victim_pcb = word;
                break;
            }
        }
    }

    if (victim_start == -1) {
        printf("Fatal OS Error: Memory is full but no valid victim found to swap out!\n");
        return -1; 
    }

    // Get the victim's details from its PCB
    int victim_end = victim_pcb->content.pcb_data.bounds[1];
    int victim_pid = victim_pcb->content.pcb_data.ProcessID;

    // 2. Open a temporary swap file (using standard C file I/O for OS-level tasks)
    char swap_filename[64];
    sprintf(swap_filename, "swap_pid_%d.bin", victim_pid);
    FILE *swap_file = fopen(swap_filename, "wb");
    
    if (swap_file == NULL) {
        printf("OS Error: Could not create swap file for PID %d\n", victim_pid);
        return -1;
    }

    // Save the total number of slots as the very first thing in the file
    int total_slots = victim_end - victim_start + 1;
    fwrite(&total_slots, sizeof(int), 1, swap_file);

    // 3. Write all the structs to the file and free the RAM
    for (int i = victim_start; i <= victim_end; i++) {
        struct MemoryWord *word = (struct MemoryWord *)readMem(emu, i);
        
        // Write the raw struct data directly to the disk
        fwrite(word, sizeof(struct MemoryWord), 1, swap_file);
        
        // Free the memory and tell the hardware the slot is empty
        free(word);
        writeMem(emu, i, NULL); 
    }

    fclose(swap_file);
    printf("SWAP: Successfully swapped out Process %d to disk.\n", victim_pid);
    
    return 0;
}

// Returns 0 on success, -1 on failure
int swap_in(Emulator *emu, int target_pid) {
    char swap_filename[64];
    sprintf(swap_filename, "swap_pid_%d.bin", target_pid);
    
    // 1. Open the swap file for reading
    FILE *swap_file = fopen(swap_filename, "rb");
    if (swap_file == NULL) {
        printf("OS Error: Swap file for PID %d not found.\n", target_pid);
        return -1;
    }

    // 2. Read the total slots required (We saved this first in swap_out!)
    int total_slots;
    fread(&total_slots, sizeof(int), 1, swap_file);

    // 3. Find Space in RAM (Evict someone else if necessary)
    int new_start_index = find_empty_space(emu, total_slots);
    
    if (new_start_index == -1) {
        printf("Memory full! Evicting another process to swap in PID %d...\n", target_pid);
        if (swap_out(emu) == -1) {
            fclose(swap_file);
            return -1; // Failed to clear space
        }
        
        new_start_index = find_empty_space(emu, total_slots);
        if (new_start_index == -1) {
            printf("Fatal Error: Still no space after swap_out!\n");
            fclose(swap_file);
            return -1;
        }
    }

    // 4. Read the structs into memory and handle the Relocation Math
    int offset = 0; // The difference between the old address and new address
    
    for (int i = 0; i < total_slots; i++) {
        struct MemoryWord *word = malloc(sizeof(struct MemoryWord));
        
        // Read the exact struct back from the file
        fread(word, sizeof(struct MemoryWord), 1, swap_file);
        
        // If this is the PCB (which is always the first word), update its pointers!
        if (word->type == MEM_TYPE_PCB) {
            int old_start = word->content.pcb_data.bounds[0];
            offset = new_start_index - old_start; // Calculate the shift distance
            
            // Shift the bounds to the new parking spot
            word->content.pcb_data.bounds[0] += offset;
            word->content.pcb_data.bounds[1] += offset;
            word->content.pcb_data.bounds[2] += offset;
            word->content.pcb_data.bounds[3] += offset;
            
            // Shift the Program Counter so Team 2 reads the correct instruction
            word->content.pcb_data.pc += offset;
            
            // Make sure the state is ready to be picked up by your Scheduler
            word->content.pcb_data.state = READY; 
        }
        
        // Give the word to the hardware at its new location
        writeMem(emu, new_start_index + i, word);
    }

    fclose(swap_file);
    
    // 5. Cleanup: Delete the swap file from the hard drive since it's back in RAM
    if (remove(swap_filename) == 0) {
        printf("SWAP: Successfully swapped in Process %d at index %d.\n", target_pid, new_start_index);
    } else {
        printf("Warning: Could not delete swap file %s.\n", swap_filename);
    }

    return 0;
}


PCB* findPCB_FromID(Emulator *emu,int id){
    struct MemoryWord *word;
    for(int i=0;i<40;i++){
        word = (struct MemoryWord*)readMem(emu, i);
        if(!(word==NULL)&&word->type==MEM_TYPE_PCB&&word->content.pcb_data.ProcessID==id){
            return &word->content.pcb_data;
        }
    }
    return NULL;
}