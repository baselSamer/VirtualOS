#include "emu/Core/Emulator_core.h"
#include "emu/Mem/Mem.h"
#include "emu/FIles/Files.h"
#include "emu/Console/Console.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int g_passes = 0;
static int g_fails = 0;

static void check_true(int condition, const char* name) {
    if (condition) {
        g_passes++;
        printToConsole("[PASS] %s", name);
    } else {
        g_fails++;
        printToConsole("[FAIL] %s", name);
    }
}

int main(void) {
    Emulator *emu;
    void **mem;
    int *slot_value;
    int *int_in;
    char *str_in;
    const char file_path[] = "Src/test_io.bin";
    const char write_blob[] = "ETHOS_FILE_IO_TEST";
    void *read_blob = NULL;
    size_t read_size = 0;
    int local_read = 0;

    printToConsole("Running Project Ethos tests...");

    emu = createEmulator();
    check_true(emu != NULL, "createEmulator returns non-null");
    if (emu == NULL) {
        printToConsole("Cannot continue tests without emulator instance.");
        return 1;
    }

    check_true(getActivePCB(emu) == NULL, "getActivePCB initial state is NULL");

    mem = getMEM(emu);
    check_true(mem != NULL, "getMEM returns non-null memory table");
    if (mem != NULL) {
        check_true(mem[0] == NULL && mem[1] == NULL && mem[39] == NULL,
                   "initMem initialized all checked slots to NULL");
    }

    slot_value = (int*)malloc(sizeof(int));
    if (slot_value != NULL) {
        *slot_value = 12345;
        writeMem(emu, 0, slot_value);
        check_true(readMem(emu, 0) == slot_value, "writeMem/readMem round-trip pointer");
        check_true(*(int*)readMem(emu, 0) == 12345, "writeMem/readMem round-trip value");
        freeMemLoc(emu, 0);
        check_true(readMem(emu, 0) == NULL, "freeMemLoc clears allocated slot");
    } else {
        check_true(0, "malloc for memory write test");
    }

    printToConsole("Input test data now: integer, word, integer (example: 42 dynamic_token 77)");
    readFromConsole("%d", &local_read);
    check_true(local_read == 42, "readFromConsole reads integer");

    str_in = NULL;
    check_true(readFromConsoleAlloc("%s", (void**)&str_in) == 0,
               "readFromConsoleAlloc supports dynamic string");
    if (str_in != NULL) {
        check_true(strcmp(str_in, "dynamic_token") == 0,
                   "readFromConsoleAlloc string content matches");
        free(str_in);
    }

    int_in = NULL;
    check_true(readFromConsoleAlloc("%d", (void**)&int_in) == 0,
               "readFromConsoleAlloc supports dynamic integer");
    if (int_in != NULL) {
        check_true(*int_in == 77, "readFromConsoleAlloc integer content matches");
        free(int_in);
    }

    check_true(write_raw_data(file_path, write_blob, sizeof(write_blob)) == 0,
               "write_raw_data writes test file");
    read_size = read_raw_data(file_path, &read_blob);
    check_true(read_size == sizeof(write_blob), "read_raw_data returns expected size");
    if (read_blob != NULL) {
        check_true(memcmp(read_blob, write_blob, sizeof(write_blob)) == 0,
                   "read_raw_data returns expected bytes");
        free(read_blob);
    } else {
        check_true(0, "read_raw_data returns non-null buffer");
    }
    remove(file_path);

    printToConsole("printToConsole smoke test message");

    destroyEmulator(emu);
    printToConsole("Tests complete: %d passed, %d failed", g_passes, g_fails);

    return (g_fails == 0) ? 0 : 1;
}