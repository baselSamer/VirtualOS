#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../emu/Files/Files.h" // Assuming you fixed the FIles typo!
#include "./Memory.h"
#include "../kernal.h"
#include "../../emu/Mem/Mem.h"
#include "../../emu/Console/Console.h"

/* Prints a summary header about the size and format of an on-disk swap file to the console. */
static void printSwapFormatSummary(const char *swap_filename, int pid, int total_slots) {
    printToConsole("  | DISK    | PID %d stored in %s", pid, swap_filename);
    printToConsole("  | DISK    | Format: [int total_slots=%d] + [%d MemoryWord entries]", total_slots, total_slots);
}

/* Evaluates whether a given string is NULL or made entirely of whitespace. */
static int isEmptyLine(const char *line) {
    if (line == NULL) {
        return 1;
    }

    while (*line != '\0') {
        if (*line != ' ' && *line != '\t' && *line != '\r') {
            return 0;
        }
        line++;
    }

    return 1;
}

/* Loads a process from a file into main memory, creating its PCB, variables, and instructions. */
int load_process_to_memory(Emulator *emu, const char* filepath, int new_pid, kernal_state *state) {
    void *buffer = NULL;
    
    size_t file_size = read_raw_data(filepath, &buffer);
    if (file_size == 0 || buffer == NULL) {
        printToConsole("Error: Could not find file %s", filepath);
        return -1;
    }
    
    char *file_text = malloc(file_size + 1);
    if (file_text == NULL) {
        free(buffer);
        return -1;
    }
    memcpy(file_text, buffer, file_size);
    file_text[file_size] = '\0';
    free(buffer);

    int skip_empty_lines = 0;
    if (state != NULL && state->skip_empty_lines_on_load) {
        skip_empty_lines = 1;
    }

    int line_count = 0;
    if (skip_empty_lines) {
        char *scan_text = malloc(file_size + 1);
        if (scan_text == NULL) {
            free(file_text);
            return -1;
        }
        memcpy(scan_text, file_text, file_size + 1);

        char *line = strtok(scan_text, "\n");
        while (line != NULL) {
            if (!isEmptyLine(line)) {
                line_count++;
            }
            line = strtok(NULL, "\n");
        }

        free(scan_text);
    } else {
        for (size_t i = 0; i < file_size; i++) {
            if (file_text[i] == '\n') {
                line_count++;
            }
        }
        if (file_size > 0 && file_text[file_size - 1] != '\n') {
            line_count++;
        }
    }
    
    int total_slots_needed = 1 + 3 + line_count;
    
    if (total_slots_needed > 40) {
        printToConsole("Error: Process %d (size %d) is strictly larger than physical memory!", new_pid, total_slots_needed);
        free(file_text);
        return -1;
    }

    int start_index = find_empty_space(emu, total_slots_needed);
    
    if (start_index == -1) {
        printToConsole("Memory full! Process %d goes straight to swap disk...", new_pid);

        char swap_filename[64];
        sprintf(swap_filename, "swap_pid_%d.bin", new_pid);
        FILE *swap_file = fopen(swap_filename, "wb");
        if (swap_file == NULL) {
            printToConsole("Fatal Error: Could not create swap file.");
            free(file_text);
            return -1;
        }

        // write total_slots_needed FIRST
        fwrite(&total_slots_needed, sizeof(int), 1, swap_file);

        // create PCB struct
        struct MemoryWord pcb_word;
        pcb_word.type = MEM_TYPE_PCB;
        pcb_word.content.pcb_data.ProcessID = new_pid;
        pcb_word.content.pcb_data.state = READY;
        pcb_word.content.pcb_data.pc = 4; // since start is 0
        pcb_word.content.pcb_data.bounds[0] = 1;
        pcb_word.content.pcb_data.bounds[1] = 3;
        pcb_word.content.pcb_data.bounds[2] = 4;
        pcb_word.content.pcb_data.bounds[3] = total_slots_needed - 1;
        fwrite(&pcb_word, sizeof(struct MemoryWord), 1, swap_file);

        // create 3 variables
        for (int i = 1; i <= 3; i++) {
            struct MemoryWord var_word;
            var_word.type = MEM_TYPE_VARIABLE;
            var_word.content.var_data.name[0] = '\0';
            var_word.content.var_data.value[0] = '\0';
            fwrite(&var_word, sizeof(struct MemoryWord), 1, swap_file);
        }

        // write instructions
        char *line = strtok(file_text, "\n");
        while (line != NULL) {
            if (skip_empty_lines && isEmptyLine(line)) {
                line = strtok(NULL, "\n");
                continue;
            }

            struct MemoryWord inst_word;
            inst_word.type = MEM_TYPE_INSTRUCTION;
            strncpy(inst_word.content.inst_data.code_line, line, 63);
            inst_word.content.inst_data.code_line[63] = '\0'; 
            fwrite(&inst_word, sizeof(struct MemoryWord), 1, swap_file);
            line = strtok(NULL, "\n"); 
        }

        fclose(swap_file);
        printSwapFormatSummary(swap_filename, new_pid, total_slots_needed);
    } else {
        struct MemoryWord *pcb_word = malloc(sizeof(struct MemoryWord));
        pcb_word->type = MEM_TYPE_PCB;
        pcb_word->content.pcb_data.ProcessID = new_pid;
        pcb_word->content.pcb_data.state = READY;
        pcb_word->content.pcb_data.pc = start_index + 4; 
        
        pcb_word->content.pcb_data.bounds[0] = start_index + 1;
        pcb_word->content.pcb_data.bounds[1] = start_index + 3;
        pcb_word->content.pcb_data.bounds[2] = start_index + 4;
        pcb_word->content.pcb_data.bounds[3] = start_index + total_slots_needed - 1;
        
        writeMem(emu, start_index, pcb_word);
        
        for (int i = 1; i <= 3; i++) {
            struct MemoryWord *var_word = malloc(sizeof(struct MemoryWord));
            var_word->type = MEM_TYPE_VARIABLE;
            var_word->content.var_data.name[0] = '\0'; 
            var_word->content.var_data.value[0] = '\0';
            writeMem(emu, start_index + i, var_word);
        }
        
        int current_slot = start_index + 4;
        char *line = strtok(file_text, "\n");
        
        while (line != NULL) {
            if (skip_empty_lines && isEmptyLine(line)) {
                line = strtok(NULL, "\n");
                continue;
            }

            struct MemoryWord *inst_word = malloc(sizeof(struct MemoryWord));
            inst_word->type = MEM_TYPE_INSTRUCTION;
            strncpy(inst_word->content.inst_data.code_line, line, 63);
            inst_word->content.inst_data.code_line[63] = '\0'; 
            writeMem(emu, current_slot, inst_word);
            
            current_slot++;
            line = strtok(NULL, "\n"); 
        }
    }
    
    free(file_text);
    return 0;
}

/* Scans main memory for consecutive empty slots, returning the start index or -1 if none found. */
// Returns the starting index if it finds space, or -1 if memory is full
int find_empty_space(Emulator *emu, int required_slots) {
    int consecutive_empty = 0;
    int start_index = -1;

    for (int i = 0; i < 40; i++) {
        if (readMem(emu, i) == NULL) {
            if (consecutive_empty == 0) {
                start_index = i;
            }
            consecutive_empty++;

            if (consecutive_empty == required_slots) {
                return start_index;
            }
        } else {
           
            consecutive_empty = 0;
            start_index = -1;
        }
    }
    
    return -1;
}


// Returns 0 on success, -1 on failure
/* Evicts a process (preferably blocked) from main memory to disk to free up space. */
int swap_out(Emulator *emu) {
    int victim_start = -1;
    struct MemoryWord *victim_pcb = NULL;
    
    PCB *active_process = getActivePCB(emu);
    int active_pid = (active_process != NULL) ? active_process->ProcessID : -1;

    for (int i = 0; i < 40; i++) {
        struct MemoryWord *word = (struct MemoryWord *)readMem(emu, i);
        if (word != NULL && word->type == MEM_TYPE_PCB) {
            
            if (word->content.pcb_data.ProcessID == active_pid) continue;
            
            if (word->content.pcb_data.state == BLOCKED) {
                victim_start = i;
                victim_pcb = word;
                break;
            }
        }
    }

    if (victim_start == -1) {
        for (int i = 0; i < 40; i++) {
            struct MemoryWord *word = (struct MemoryWord *)readMem(emu, i);
            if (word != NULL && word->type == MEM_TYPE_PCB) {
                
                if (word->content.pcb_data.ProcessID == active_pid) continue;
                
                victim_start = i;
                victim_pcb = word;
                break;
            }
        }
    }

    if (victim_start == -1) {
        printToConsole("Fatal OS Error: Memory is full but no valid victim found to swap out!");
        return -1; 
    }

    int victim_end = victim_pcb->content.pcb_data.bounds[3];
    int victim_pid = victim_pcb->content.pcb_data.ProcessID;

    char swap_filename[64];
    sprintf(swap_filename, "swap_pid_%d.bin", victim_pid);
    FILE *swap_file = fopen(swap_filename, "wb");
    
    if (swap_file == NULL) {
        printToConsole("OS Error: Could not create swap file for PID %d", victim_pid);
        return -1;
    }

    // Save the total number of slots as the very first thing in the file
    int total_slots = victim_end - victim_start + 1;
    fwrite(&total_slots, sizeof(int), 1, swap_file);

    for (int i = victim_start; i <= victim_end; i++) {
        struct MemoryWord *word = (struct MemoryWord *)readMem(emu, i);
        
        // Write the raw struct data directly to the disk
        fwrite(word, sizeof(struct MemoryWord), 1, swap_file);
        
        // Free the memory and tell the hardware the slot is empty
        free(word);
        writeMem(emu, i, NULL); 
    }

    fclose(swap_file);
    printToConsole("  | SWAP    | Swapped OUT PID %d", victim_pid);
    printSwapFormatSummary(swap_filename, victim_pid, total_slots);
    
    return 0;
}

/* Locates a process variable by name and updates its value in memory. */
/* Set variable in memory */
int set_variable(Emulator *emu, int pid, const char *name, const char *value) {
    PCB *pcb = findPCB_FromID(emu, pid);
    if (pcb == NULL) return 0;
    
    int start_bound = pcb->bounds[0];
    
    /* Variables are at slots start_bound, start_bound + 1, start_bound + 2 */
    for (int i = 0; i <= 2; i++) {
        struct MemoryWord *word = (struct MemoryWord*)readMem(emu, start_bound + i);
        if (word != NULL && word->type == MEM_TYPE_VARIABLE) {
            /* If empty or matches name, assign here */
            if (strlen(word->content.var_data.name) == 0 || strcmp(word->content.var_data.name, name) == 0) {
                strncpy(word->content.var_data.name, name, 31);
                word->content.var_data.name[31] = '\0';
                strncpy(word->content.var_data.value, value, 63);
                word->content.var_data.value[63] = '\0';
                return 1;
            }
        }
    }
    return 0;
}

/* Locates a process variable by name and retrieves its current value. */
/* Get variable from memory */
char* get_variable(Emulator *emu, int pid, const char *name) {
    PCB *pcb = findPCB_FromID(emu, pid);
    if (pcb == NULL) return NULL;
    
    int start_bound = pcb->bounds[0];
    
    for (int i = 0; i <= 2; i++) {
        struct MemoryWord *word = (struct MemoryWord*)readMem(emu, start_bound + i);
        if (word != NULL && word->type == MEM_TYPE_VARIABLE) {
            if (strcmp(word->content.var_data.name, name) == 0) {
                return word->content.var_data.value;
            }
        }
    }
    return NULL;
}

/* Retrieves a previously evicted process from disk and restores it to main memory. */
// Returns 0 on success, -1 on failure
int swap_in(Emulator *emu, int target_pid) {
    char swap_filename[64];
    sprintf(swap_filename, "swap_pid_%d.bin", target_pid);
    
    //open the swap file for reading
    FILE *swap_file = fopen(swap_filename, "rb");
    if (swap_file == NULL) {
        printToConsole("OS Error: Swap file for PID %d not found.", target_pid);
        return -1;
    }

    int total_slots;
    fread(&total_slots, sizeof(int), 1, swap_file);

    int new_start_index = find_empty_space(emu, total_slots);
    
    while (new_start_index == -1) {
        printToConsole("Memory full! Evicting another process to swap in PID %d...", target_pid);
        if (swap_out(emu) == -1) {
            printToConsole("Fatal Error: Swap failed during swap_in.");
            fclose(swap_file);
            return -1; 
        }
        
        new_start_index = find_empty_space(emu, total_slots);
    }

    int offset = 0; // The difference between the old address and new address
    
    for (int i = 0; i < total_slots; i++) {
        struct MemoryWord *word = malloc(sizeof(struct MemoryWord));
        
        // Read the exact struct back from the file
        fread(word, sizeof(struct MemoryWord), 1, swap_file);
        
        if (word->type == MEM_TYPE_PCB) {
            int old_start = word->content.pcb_data.bounds[0] - 1; // PCB sits one before var_start
            offset = new_start_index - old_start; // Calculate the shift distance
            
            word->content.pcb_data.bounds[0] += offset;
            word->content.pcb_data.bounds[1] += offset;
            word->content.pcb_data.bounds[2] += offset;
            word->content.pcb_data.bounds[3] += offset;
            
            word->content.pcb_data.pc += offset;
            
            word->content.pcb_data.state = READY; 
        }
        
        writeMem(emu, new_start_index + i, word);
    }

    fclose(swap_file);
    printToConsole("  | SWAP    | Swapped IN PID %d at RAM index %d", target_pid, new_start_index);
    printSwapFormatSummary(swap_filename, target_pid, total_slots);
    
    if (remove(swap_filename) == 0) {
        printToConsole("  | SWAP    | Removed swap file %s after restore", swap_filename);
    } else {
        printToConsole("Warning: Could not delete swap file %s.", swap_filename);
    }

    return 0;
}


/* Searches the emulator's memory space and returns a pointer to the PCB struct with the specified Process ID. */
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