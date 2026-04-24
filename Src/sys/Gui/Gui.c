#include "Gui.h"
#include "../Memory/Memory.h"
#include "../../emu/Console/Console.h"
#include "../../emu/Mem/Mem.h"
#include "../../emu/Core/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ========== CONSTANTS ========== */
#define WIN_W       1050
#define WIN_H       720
#define GRID_X      230
#define GRID_Y      120
#define CELL_W      100
#define CELL_H      60
#define GRID_COLS   8
#define GRID_ROWS   5
#define STATUS_H    45
#define SWAP_MAX_SLOTS 40
#define SWAP_MAX_PIDS 10
#define SWAP_CELL_W 90
#define SWAP_CELL_H 54
#define SWAP_GRID_COLS 8

/* GUI States */
#define GUI_STATE_WELCOME       -1
#define GUI_STATE_CONFIG_ALGO   0
#define GUI_STATE_CONFIG_PROCS  1
#define GUI_STATE_SIMULATION    2

/* Control IDs */
#define ID_TAB_MEM      1001
#define ID_TAB_SWAP     1002
#define ID_TAB_SYS      1003
#define ID_BTN_STEP     1004

#define ID_CBO_ALGO     2001
#define ID_TXT_QUANTUM  2002
#define ID_TXT_PROCS    2003
#define ID_BTN_NEXT     2004
#define ID_CHK_AUTO_REL 2005
#define ID_CHK_SKIP_EMP 2006
#define ID_CHK_TERM_SYN 2007
#define ID_CHK_MLFQ_L0  2008

#define ID_TXT_ARRIVE   3001
#define ID_BTN_BROWSE   3002
#define ID_BTN_PREVPRO  3003
#define ID_BTN_NEXTPRO  3004

#define ID_BTN_START    1000

/* Win11 Dark Mode Palette */
#define WIN11_BG        RGB(32, 32, 32)
#define WIN11_SURFACE   RGB(40, 40, 40)
#define WIN11_OVERLAY   RGB(45, 45, 48)
#define WIN11_TEXT      RGB(255, 255, 255)
#define WIN11_TEXT_SEC  RGB(160, 160, 160)
#define WIN11_ACCENT    RGB(0, 120, 212)
#define WIN11_HOVER     RGB(50, 50, 50)
#define WIN11_BORDER    RGB(64, 64, 64)

/* Tab indices */
#define TAB_MEMORY  0
#define TAB_SWAP    1
#define TAB_SYSTEM  2
#define TAB_LOGS    3

/* PID colors */
static COLORREF pid_colors[] = {
    RGB(60, 60, 60),    /* 0: Empty (Surface) */
    RGB(0, 191, 255),   /* 1: Light Blue */
    RGB(255, 140, 0),   /* 2: Orange */
    RGB(255, 59, 48),   /* 3: Red */
    RGB(50, 215, 75),   /* 4: Green */
    RGB(191, 90, 242),  /* 5: Purple */
    RGB(255, 214, 10),  /* 6: Yellow */
    RGB(255, 55, 95),   /* 7: Pink */
    RGB(100, 210, 255), /* 8: Teal */
    RGB(142, 142, 147), /* 9: Grey */
    RGB(172, 142, 104), /* 10: Brown */
};
#define NUM_PID_COLORS 11

/* ========== GLOBAL STATE ========== */
static Emulator *g_emu = NULL;
static kernal_state *g_state = NULL;
static HWND g_hwnd = NULL;

static HANDLE g_step_event = NULL;
static HANDLE g_config_done_event = NULL;
static HANDLE g_gui_thread = NULL;

static int g_gui_mode = GUI_STATE_WELCOME;
static int g_active_tab = TAB_MEMORY;
static int g_hovered_cell = -1;
static int g_hovered_swap_cell = -1;
static int g_logs_scroll_offset = 0;
static volatile int g_running = 1;

static int g_swap_selected_pid = -1;
static int g_swap_words_count = 0;
static struct MemoryWord g_swap_words[SWAP_MAX_SLOTS];
static int g_swap_has_header = 0;
static long g_swap_file_size = 0;
static MemoryWordType g_swap_first_entry_type = MEM_TYPE_PCB;
static int g_swap_grid_x = GRID_X;
static int g_swap_grid_y = GRID_Y + 130;
static int g_swap_grid_count = 0;
static RECT g_swap_pid_chip_rects[SWAP_MAX_PIDS];
static int g_swap_pid_chip_values[SWAP_MAX_PIDS];
static int g_swap_pid_chip_count = 0;

static HFONT g_font_normal = NULL;
static HFONT g_font_bold = NULL;
static HFONT g_font_title = NULL;
static HFONT g_font_small = NULL;

/* Config State */
static int g_cfg_algo = 0;
static int g_cfg_quantum = 2;
static int g_cfg_num_procs = 0;
static int g_cfg_proc_idx = 0;
static int g_cfg_auto_release_resources = 0;
static int g_cfg_skip_empty_lines = 0;
static int g_cfg_terminate_on_syntax_error = 0;
static int g_cfg_mlfq_unblock_to_l0 = 0;

static int *g_temp_arrivals = NULL;
static char **g_temp_paths = NULL;

/* Child HWNDs */
static HWND hCboAlgo, hTxtQuantum, hTxtProcs, hBtnNext;
static HWND hChkAutoRelease, hChkSkipEmpty, hChkTerminateSyntax, hChkMlfqUnblockL0;
static HWND hTxtArrive, hBtnBrowse, hBtnPrevPro, hBtnNextPro;
static HWND hTabMem, hTabSwap, hTabSys, hBtnStep;

/* ========== HELPERS ========== */

static COLORREF getColorForPid(int pid) {
    if (pid <= 0 || pid >= NUM_PID_COLORS) return pid_colors[0];
    return pid_colors[pid];
}

static const char* stateToStr(int state) {
    switch (state) {
        case CREATED:    return "CREATED";
        case READY:      return "READY";
        case RUNNING:    return "RUNNING";
        case BLOCKED:    return "BLOCKED";
        case TERMINATED: return "TERMINATED";
        default:         return "???";
    }
}

static const char* memTypeToStr(MemoryWordType type) {
    switch (type) {
        case MEM_TYPE_PCB:         return "PCB";
        case MEM_TYPE_VARIABLE:    return "VAR";
        case MEM_TYPE_INSTRUCTION: return "INS";
        default:                   return "???";
    }
}

static COLORREF memTypeToColor(MemoryWordType type) {
    switch (type) {
        case MEM_TYPE_PCB:         return RGB(0, 191, 255);
        case MEM_TYPE_VARIABLE:    return RGB(255, 140, 0);
        case MEM_TYPE_INSTRUCTION: return RGB(50, 215, 75);
        default:                   return RGB(90, 90, 90);
    }
}

static int loadSwapWordsForPid(int pid) {
    char filename[64];
    FILE *f;
    int total_slots = 0;

    g_swap_words_count = 0;
    g_swap_has_header = 0;
    g_swap_file_size = 0;
    g_swap_first_entry_type = MEM_TYPE_PCB;

    sprintf(filename, "swap_pid_%d.bin", pid);
    f = fopen(filename, "rb");
    if (f == NULL) {
        return 0;
    }

    fseek(f, 0, SEEK_END);
    g_swap_file_size = ftell(f);
    rewind(f);

    if (fread(&total_slots, sizeof(int), 1, f) != 1) {
        fclose(f);
        return 0;
    }
    g_swap_has_header = 1;

    if (total_slots < 0) total_slots = 0;
    if (total_slots > SWAP_MAX_SLOTS) total_slots = SWAP_MAX_SLOTS;

    for (int i = 0; i < total_slots; i++) {
        if (fread(&g_swap_words[i], sizeof(struct MemoryWord), 1, f) != 1) {
            break;
        }
        g_swap_words_count++;
    }

    if (g_swap_words_count > 0) {
        g_swap_first_entry_type = g_swap_words[0].type;
    }

    fclose(f);
    return 1;
}

static const char* algoToStr(SchedulingAlgorithm algo) {
    switch (algo) {
        case SCHED_RR:   return "Round Robin (RR)";
        case SCHED_HRRN: return "Highest Response Ratio Next (HRRN)";
        case SCHED_MLFQ: return "Multi-Level Feedback Queue (MLFQ)";
        default:         return "Unknown";
    }
}

static void computeSlotOwnership(int *slot_pids) {
    for (int i = 0; i < 40; i++) slot_pids[i] = 0;
    for (int i = 0; i < 40; i++) {
        struct MemoryWord *word = (struct MemoryWord*)readMem(g_emu, i);
        if (word != NULL && word->type == MEM_TYPE_PCB) {
            int pid = word->content.pcb_data.ProcessID;
            int start = word->content.pcb_data.bounds[0];
            int end = word->content.pcb_data.bounds[1];
            for (int j = start; j <= end && j < 40; j++) {
                slot_pids[j] = pid;
            }
        }
    }
}

static void DestroyAllChildWindows(HWND hwnd) {
    HWND child;
    while ((child = GetWindow(hwnd, GW_CHILD)) != NULL) {
        DestroyWindow(child);
    }
}

static void BrowseFile(void) {
    OPENFILENAME ofn;
    char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileName(&ofn) == TRUE) {
        if (g_temp_paths[g_cfg_proc_idx]) free(g_temp_paths[g_cfg_proc_idx]);
        g_temp_paths[g_cfg_proc_idx] = strdup(ofn.lpstrFile);
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

/* ========== DRAWING HELPERS ========== */
static void DrawRoundRect(HDC hdc, int x, int y, int w, int h, int radius, COLORREF bg, COLORREF border, int thickness) {
    HBRUSH hBrush = CreateSolidBrush(bg);
    HPEN hPen = CreatePen(PS_SOLID, thickness, border);
    HGDIOBJ oldBrush = SelectObject(hdc, hBrush);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    
    RoundRect(hdc, x, y, x + w, y + h, radius, radius);
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

static void drawQueue(HDC hdc, int x, int y, const char *label, Queue *q, int *slot_pids) {
    char buf[256];
    SetTextColor(hdc, WIN11_TEXT);
    SelectObject(hdc, g_font_bold);
    TextOut(hdc, x, y, label, (int)strlen(label));
    
    SelectObject(hdc, g_font_normal);
    if (q->count == 0) {
        SetTextColor(hdc, WIN11_TEXT_SEC);
        TextOut(hdc, x + 10, y + 22, "(empty)", 7);
        return;
    }
    
    SetTextColor(hdc, WIN11_TEXT);
    int bx = x + 10;
    for (int i = 0; i < q->count; i++) {
        int idx = (q->front + i) % MAX_SIZE;
        int pid = q->process_ids[idx];
        
        DrawRoundRect(hdc, bx, y + 20, 45, 25, 8, getColorForPid(pid), WIN11_BORDER, 1);
        
        sprintf(buf, "P%d", pid);
        SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, bx + 12, y + 24, buf, (int)strlen(buf));
        
        bx += 50;
    }
}

static int mlfqQuantumForLevel(int level) {
    return 1 << level;
}

/* ========== STATE: WELCOME ========== */
static HWND hBtnStart;

static void buildWelcomeUI(HWND hwnd) {
    DestroyAllChildWindows(hwnd);
    
    hBtnStart = CreateWindow("BUTTON", "Start", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 WIN_W/2 - 60, WIN_H/2 + 50, 120, 40, hwnd, (HMENU)ID_BTN_START, NULL, NULL);
                 
    HFONT sysFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hBtnStart, WM_SETFONT, (WPARAM)sysFont, 0);
}

static void drawWelcomeTab(HDC hdc, RECT *client) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, WIN11_TEXT);
    SelectObject(hdc, g_font_title);
    
    const char *title = "Welcome to Ethos";
    SIZE size;
    GetTextExtentPoint32(hdc, title, strlen(title), &size);
    TextOut(hdc, (WIN_W - size.cx) / 2, WIN_H/2 - 50, title, strlen(title));
    
    SelectObject(hdc, g_font_normal);
    SetTextColor(hdc, WIN11_TEXT_SEC);
    const char *sub = "Project Ethos OS Simulator v1.0";
    GetTextExtentPoint32(hdc, sub, strlen(sub), &size);
    TextOut(hdc, (WIN_W - size.cx) / 2, WIN_H/2 - 10, sub, strlen(sub));
}

/* ========== STATE: CONFIG ALGO ========== */
static void buildConfigAlgoUI(HWND hwnd) {
    DestroyAllChildWindows(hwnd);

    char quantumBuf[16];
    char procsBuf[16];
    int procsDefault;
    int comboIndex;

    if (g_cfg_quantum <= 0) {
        g_cfg_quantum = 2;
    }
    procsDefault = (g_cfg_num_procs > 0) ? g_cfg_num_procs : 3;

    if (g_cfg_algo == SCHED_HRRN) comboIndex = 1;
    else if (g_cfg_algo == SCHED_MLFQ) comboIndex = 2;
    else comboIndex = 0;

    sprintf(quantumBuf, "%d", g_cfg_quantum);
    sprintf(procsBuf, "%d", procsDefault);
    
    CreateWindow("STATIC", "Scheduling Algorithm:", WS_CHILD | WS_VISIBLE, 
                 300, 200, 200, 20, hwnd, NULL, NULL, NULL);
    hCboAlgo = CreateWindow("COMBOBOX", "", CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
                 300, 220, 320, 150, hwnd, (HMENU)ID_CBO_ALGO, NULL, NULL);
    SendMessage(hCboAlgo, CB_ADDSTRING, 0, (LPARAM)"Round Robin (RR)");
    SendMessage(hCboAlgo, CB_ADDSTRING, 0, (LPARAM)"Highest Response Ratio Next (HRRN)");
    SendMessage(hCboAlgo, CB_ADDSTRING, 0, (LPARAM)"Multi-Level Feedback Queue (MLFQ)");
    SendMessage(hCboAlgo, CB_SETCURSEL, comboIndex, 0);

    CreateWindow("STATIC", "Time Quantum (RR only):", WS_CHILD | WS_VISIBLE, 
                 300, 270, 200, 20, hwnd, NULL, NULL, NULL);
    hTxtQuantum = CreateWindow("EDIT", quantumBuf, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | WS_TABSTOP,
                 300, 290, 80, 25, hwnd, (HMENU)ID_TXT_QUANTUM, NULL, NULL);
    EnableWindow(hTxtQuantum, (comboIndex == 0));

    CreateWindow("STATIC", "Number of Processes:", WS_CHILD | WS_VISIBLE, 
                 300, 340, 200, 20, hwnd, NULL, NULL, NULL);
    hTxtProcs = CreateWindow("EDIT", procsBuf, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | WS_TABSTOP,
                 300, 360, 80, 25, hwnd, (HMENU)ID_TXT_PROCS, NULL, NULL);

    hChkAutoRelease = CreateWindow("BUTTON", "Auto release resources on process termination", 
                 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                 300, 395, 360, 24, hwnd, (HMENU)ID_CHK_AUTO_REL, NULL, NULL);
    SendMessage(hChkAutoRelease, BM_SETCHECK, g_cfg_auto_release_resources ? BST_CHECKED : BST_UNCHECKED, 0);

    hChkSkipEmpty = CreateWindow("BUTTON", "Skip empty lines while loading programs", 
                 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                 300, 423, 340, 24, hwnd, (HMENU)ID_CHK_SKIP_EMP, NULL, NULL);
    SendMessage(hChkSkipEmpty, BM_SETCHECK, g_cfg_skip_empty_lines ? BST_CHECKED : BST_UNCHECKED, 0);

    hChkTerminateSyntax = CreateWindow("BUTTON", "Terminate process on syntax error (non-empty lines)", 
                 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                 300, 451, 380, 24, hwnd, (HMENU)ID_CHK_TERM_SYN, NULL, NULL);
    SendMessage(hChkTerminateSyntax, BM_SETCHECK, g_cfg_terminate_on_syntax_error ? BST_CHECKED : BST_UNCHECKED, 0);

    hChkMlfqUnblockL0 = CreateWindow("BUTTON", "MLFQ: unblocked process returns to L0 queue", 
                 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                 300, 479, 360, 24, hwnd, (HMENU)ID_CHK_MLFQ_L0, NULL, NULL);
    SendMessage(hChkMlfqUnblockL0, BM_SETCHECK, g_cfg_mlfq_unblock_to_l0 ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(hChkMlfqUnblockL0, (comboIndex == 2));

    hBtnNext = CreateWindow("BUTTON", "Next ->", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 500, 525, 120, 35, hwnd, (HMENU)ID_BTN_NEXT, NULL, NULL);
                 
    EnumChildWindows(hwnd, (WNDENUMPROC)SendMessage, (LPARAM)WM_SETFONT);
    SendMessage(hwnd, WM_SETFONT, (WPARAM)g_font_normal, MAKELPARAM(TRUE, 0));
    
    // Default system font for child controls
    HFONT sysFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hCboAlgo, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hTxtQuantum, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hTxtProcs, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hChkAutoRelease, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hChkSkipEmpty, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hChkTerminateSyntax, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hChkMlfqUnblockL0, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hBtnNext, WM_SETFONT, (WPARAM)sysFont, 0);
}

static void drawConfigAlgoTab(HDC hdc, RECT *client) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, WIN11_TEXT);
    SelectObject(hdc, g_font_title);
    TextOut(hdc, 300, 120, "System Configuration", 20);
    
    SelectObject(hdc, g_font_normal);
    SetTextColor(hdc, WIN11_TEXT_SEC);
    TextOut(hdc, 300, 160, "Step 1: Set up the OS Kernel Scheduler", 38);
}

/* ========== STATE: CONFIG PROCS ========== */
static void buildConfigProcsUI(HWND hwnd) {
    DestroyAllChildWindows(hwnd);
    
    CreateWindow("STATIC", "Arrival Time (clock tick):", WS_CHILD | WS_VISIBLE, 
                 300, 200, 200, 20, hwnd, NULL, NULL, NULL);
                 
    char arrivalBuf[16];
    sprintf(arrivalBuf, "%d", g_temp_arrivals[g_cfg_proc_idx]);
    
    hTxtArrive = CreateWindow("EDIT", arrivalBuf, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | WS_TABSTOP,
                 300, 220, 80, 25, hwnd, (HMENU)ID_TXT_ARRIVE, NULL, NULL);

    hBtnBrowse = CreateWindow("BUTTON", "Browse Program File...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 300, 300, 180, 30, hwnd, (HMENU)ID_BTN_BROWSE, NULL, NULL);

    hBtnPrevPro = CreateWindow("BUTTON", "<- Back", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 300, 420, 100, 35, hwnd, (HMENU)ID_BTN_PREVPRO, NULL, NULL);

    const char* nextTxt = (g_cfg_proc_idx == g_cfg_num_procs - 1) ? "Start Simulation" : "Next Process ->";
    hBtnNextPro = CreateWindow("BUTTON", nextTxt, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 420, 420, 150, 35, hwnd, (HMENU)ID_BTN_NEXTPRO, NULL, NULL);

    HFONT sysFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hTxtArrive, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hBtnBrowse, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hBtnPrevPro, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hBtnNextPro, WM_SETFONT, (WPARAM)sysFont, 0);
}

static void drawConfigProcsTab(HDC hdc, RECT *client) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, WIN11_TEXT);
    SelectObject(hdc, g_font_title);
    
    char buf[128];
    sprintf(buf, "Process %d of %d", g_cfg_proc_idx + 1, g_cfg_num_procs);
    TextOut(hdc, 300, 120, buf, (int)strlen(buf));
    
    SelectObject(hdc, g_font_normal);
    SetTextColor(hdc, WIN11_TEXT_SEC);
    TextOut(hdc, 300, 160, "Step 2: Configure incoming processes", 36);
    
    // Draw Current Path
    SetTextColor(hdc, WIN11_TEXT);
    TextOut(hdc, 300, 270, "Selected File:", 14);
    
    char* curPath = g_temp_paths[g_cfg_proc_idx];
    if (curPath != NULL && strlen(curPath) > 0) {
        SetTextColor(hdc, WIN11_ACCENT);
        TextOut(hdc, 300, 340, curPath, (int)strlen(curPath));
    } else {
        SetTextColor(hdc, RGB(220, 50, 50));
        TextOut(hdc, 300, 340, "No file selected. Please browse.", 32);
    }
}

/* ========== STATE: SIMULATION ========== */
static void buildSimulationUI(HWND hwnd) {
    DestroyAllChildWindows(hwnd);
    
    // Bottom right corner for Step button
    hBtnStep = CreateWindow("BUTTON", "Step >>", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 WIN_W - 130, WIN_H - 150, 100, 40, hwnd, (HMENU)ID_BTN_STEP, NULL, NULL);

    HFONT sysFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hBtnStep, WM_SETFONT, (WPARAM)sysFont, 0);
}

/* ========== TAB: MEMORY (SIMULATION) ========== */
static void drawMemoryTab(HDC hdc, RECT *client) {
    int slot_pids[40];
    computeSlotOwnership(slot_pids);
    
    char buf[128];
    SelectObject(hdc, g_font_title);
    SetTextColor(hdc, WIN11_TEXT);
    TextOut(hdc, GRID_X, GRID_Y - 50, "Physical Memory", 15);
    
    /* Calculate usage */
    int used_slots = 0;
    for (int i=0; i<40; i++) if (slot_pids[i] != 0) used_slots++;
    
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, WIN11_ACCENT);
    sprintf(buf, "%d / 40 Slots Used", used_slots);
    TextOut(hdc, GRID_X, GRID_Y - 20, buf, (int)strlen(buf));
    
    SelectObject(hdc, g_font_normal);
    
    for (int i = 0; i < 40; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        int x = GRID_X + col * CELL_W;
        int y = GRID_Y + row * CELL_H;
        
        struct MemoryWord *word = (struct MemoryWord*)readMem(g_emu, i);
        int pid = slot_pids[i];
        COLORREF color = getColorForPid(pid);
        COLORREF border = (i == g_hovered_cell) ? WIN11_ACCENT : WIN11_BORDER;
        int thickness = (i == g_hovered_cell) ? 2 : 1;
        
        DrawRoundRect(hdc, x, y, CELL_W - 4, CELL_H - 4, 10, color, border, thickness);
        
        SetBkMode(hdc, TRANSPARENT);
        
        /* Slot index */
        SelectObject(hdc, g_font_small);
        SetTextColor(hdc, RGB(90, 90, 90));
        sprintf(buf, "%d", i);
        TextOut(hdc, x + 6, y + 4, buf, (int)strlen(buf));
        
        /* Center text */
        SelectObject(hdc, g_font_normal);
        if (word != NULL) {
            SetTextColor(hdc, WIN11_TEXT);
            sprintf(buf, "%s", memTypeToStr(word->type));
            TextOut(hdc, x + 30, y + 18, buf, (int)strlen(buf));
            if (pid > 0) {
                sprintf(buf, "P%d", pid);
                TextOut(hdc, x + 35, y + 36, buf, (int)strlen(buf));
            }
        } else {
            SetTextColor(hdc, WIN11_TEXT_SEC);
            TextOut(hdc, x + 25, y + 22, "FREE", 4);
        }
    }
    
    /* Legend */
    int ly = GRID_Y + GRID_ROWS * CELL_H + 15;
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, WIN11_TEXT);
    TextOut(hdc, GRID_X, ly, "Legend:", 7);
    
    SelectObject(hdc, g_font_normal);
    int lx = GRID_X + 70;
    
    /* Empty */
    DrawRoundRect(hdc, lx, ly, 20, 16, 4, pid_colors[0], WIN11_BORDER, 1);
    TextOut(hdc, lx + 25, ly, "Empty", 5);
    lx += 90;
    
    for (int p = 1; p < NUM_PID_COLORS; p++) {
        int found = 0;
        for (int s = 0; s < 40; s++) {
            if (slot_pids[s] == p) { found = 1; break; }
        }
        if (!found) continue;
        
        DrawRoundRect(hdc, lx, ly, 20, 16, 4, pid_colors[p], WIN11_BORDER, 1);
        sprintf(buf, "PID %d", p);
        TextOut(hdc, lx + 25, ly, buf, (int)strlen(buf));
        lx += 90;
        if (lx > 750) { lx = GRID_X + 70; ly += 22; }
    }
}

/* ========== TAB: SWAP (SIMULATION) ========== */
static void drawSwapTab(HDC hdc, RECT *client) {
    char buf[256];
    int y = GRID_Y;
    int available_pids[SWAP_MAX_PIDS];
    int available_count = 0;
    int selected_exists = 0;
    int chip_x = GRID_X;
    int chip_y = y + 8;
    int chip_w = 66;
    int chip_h = 28;
    int chip_gap = 10;
    int chip_line_h = 36;
    int info_y;
    
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, WIN11_TEXT);
    TextOut(hdc, GRID_X, y - 20, "Swap Memory (on disk)", 21);
    
    g_swap_pid_chip_count = 0;
    g_swap_grid_count = 0;

    for (int p = 1; p <= SWAP_MAX_PIDS; p++) {
        char filename[64];
        FILE *f;
        sprintf(filename, "swap_pid_%d.bin", p);
        f = fopen(filename, "rb");
        if (f != NULL) {
            fclose(f);
            available_pids[available_count++] = p;
            if (p == g_swap_selected_pid) selected_exists = 1;
        }
    }

    if (available_count == 0) {
        g_swap_selected_pid = -1;
        g_hovered_swap_cell = -1;
        SetTextColor(hdc, WIN11_TEXT_SEC);
        SelectObject(hdc, g_font_normal);
        TextOut(hdc, GRID_X + 10, y + 12, "No processes currently swapped to disk.", 39);
        return;
    }

    if (!selected_exists) {
        g_swap_selected_pid = available_pids[0];
        g_hovered_swap_cell = -1;
    }

    SelectObject(hdc, g_font_normal);
    SetTextColor(hdc, WIN11_TEXT_SEC);
    TextOut(hdc, GRID_X, chip_y - 20, "Swapped PID:", 11);

    for (int i = 0; i < available_count; i++) {
        int pid = available_pids[i];
        int w = chip_w;
        int h = chip_h;
        int is_selected = (pid == g_swap_selected_pid);
        COLORREF bg = is_selected ? getColorForPid(pid) : WIN11_BG;
        COLORREF border = is_selected ? WIN11_ACCENT : WIN11_BORDER;
        int thickness = is_selected ? 2 : 1;

        if (chip_x + w > GRID_X + 760) {
            chip_x = GRID_X;
            chip_y += chip_line_h;
        }

        DrawRoundRect(hdc, chip_x, chip_y, w, h, 9, bg, border, thickness);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, WIN11_TEXT);
        sprintf(buf, "PID %d", pid);
        TextOut(hdc, chip_x + 12, chip_y + 6, buf, (int)strlen(buf));

        if (g_swap_pid_chip_count < SWAP_MAX_PIDS) {
            g_swap_pid_chip_rects[g_swap_pid_chip_count].left = chip_x;
            g_swap_pid_chip_rects[g_swap_pid_chip_count].top = chip_y;
            g_swap_pid_chip_rects[g_swap_pid_chip_count].right = chip_x + w;
            g_swap_pid_chip_rects[g_swap_pid_chip_count].bottom = chip_y + h;
            g_swap_pid_chip_values[g_swap_pid_chip_count] = pid;
            g_swap_pid_chip_count++;
        }
        chip_x += w + chip_gap;
    }

    loadSwapWordsForPid(g_swap_selected_pid);

    info_y = chip_y + chip_h + 12;
    DrawRoundRect(hdc, GRID_X, info_y, 760, 62, 10, WIN11_BG, WIN11_BORDER, 1);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, WIN11_TEXT);
    sprintf(buf, "  PID %d  |  File: swap_pid_%d.bin  |  Size: %ld bytes", g_swap_selected_pid, g_swap_selected_pid, g_swap_file_size);
    TextOut(hdc, GRID_X + 15, info_y + 10, buf, (int)strlen(buf));

    if (g_swap_has_header) {
        if (g_swap_words_count > 0) {
            sprintf(buf, "  Format: [int total_slots=%d] + [%d x MemoryWord], FirstEntry=%s", 
                    g_swap_words_count, g_swap_words_count, memTypeToStr(g_swap_first_entry_type));
        } else {
            sprintf(buf, "  Format: [int total_slots=0] + [0 x MemoryWord]");
        }
    } else {
        sprintf(buf, "  Format: unreadable swap header");
    }
    TextOut(hdc, GRID_X + 15, info_y + 34, buf, (int)strlen(buf));

    g_swap_grid_x = GRID_X;
    g_swap_grid_y = info_y + 74;
    g_swap_grid_count = g_swap_words_count;

    for (int i = 0; i < g_swap_words_count; i++) {
        int col = i % SWAP_GRID_COLS;
        int row = i / SWAP_GRID_COLS;
        int cell_x = g_swap_grid_x + col * SWAP_CELL_W;
        int cell_y = g_swap_grid_y + row * SWAP_CELL_H;
        COLORREF border = (i == g_hovered_swap_cell) ? WIN11_ACCENT : WIN11_BORDER;
        int thickness = (i == g_hovered_swap_cell) ? 2 : 1;

        DrawRoundRect(hdc, cell_x, cell_y, SWAP_CELL_W - 6, SWAP_CELL_H - 6, 8,
                      memTypeToColor(g_swap_words[i].type), border, thickness);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, WIN11_TEXT);
        sprintf(buf, "S%02d", i);
        TextOut(hdc, cell_x + 8, cell_y + 6, buf, (int)strlen(buf));

        sprintf(buf, "%s", memTypeToStr(g_swap_words[i].type));
        TextOut(hdc, cell_x + 24, cell_y + 24, buf, (int)strlen(buf));
    }
}

/* ========== TAB: SYSTEM STATUS (SIMULATION) ========== */
static void drawSystemTab(HDC hdc, RECT *client) {
    char buf[256];
    int slot_pids[40];
    computeSlotOwnership(slot_pids);
    int y = GRID_Y - 10;
    int x = GRID_X;
    
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, WIN11_TEXT);
    TextOut(hdc, x, y, "Active Process", 14);
    y += 22;
    
    SelectObject(hdc, g_font_normal);
    PCB *active = getActivePCB(g_emu);
    if (active != NULL) {
        DrawRoundRect(hdc, x, y, 420, 75, 12, getColorForPid(active->ProcessID), WIN11_BORDER, 1);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, WIN11_TEXT);
        sprintf(buf, "  PID: %d   State: %s   PC: %d", active->ProcessID, stateToStr(active->state), active->pc);
        TextOut(hdc, x + 5, y + 8, buf, (int)strlen(buf));
        sprintf(buf, "  Bounds: [%d, %d, %d, %d]", active->bounds[0], active->bounds[1], active->bounds[2], active->bounds[3]);
        TextOut(hdc, x + 5, y + 28, buf, (int)strlen(buf));
        
        if (g_state->current_algo == SCHED_RR) {
            sprintf(buf, "  RR Quantum: %d / %d", g_state->rr_time_quantum_counter, g_state->time_quantum);
            TextOut(hdc, x + 5, y + 48, buf, (int)strlen(buf));
        } else if (g_state->current_algo == SCHED_MLFQ) {
            sprintf(buf, "  MLFQ L%d Quantum: %d / %d", 
                    g_state->active_process_queue_index, 
                    g_state->mlfq_time_quantum_counter,
                    mlfqQuantumForLevel(g_state->active_process_queue_index));
            TextOut(hdc, x + 5, y + 48, buf, (int)strlen(buf));
        }
    } else {
        SetTextColor(hdc, WIN11_TEXT_SEC);
        TextOut(hdc, x + 10, y + 5, "No active process (CPU IDLE)", 28);
    }
    y += 90;
    
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, WIN11_TEXT);
    sprintf(buf, "Scheduler: %s", algoToStr(g_state->current_algo));
    TextOut(hdc, x, y, buf, (int)strlen(buf));
    y += 28;
    
    drawQueue(hdc, x, y, "Ready Queue L0 (q=1):", &g_state->ready_queue, slot_pids);
    y += 50;
    
    if (g_state->current_algo == SCHED_MLFQ) {
        drawQueue(hdc, x, y, "Ready Queue L1 (q=2):", &g_state->ready_queue_1, slot_pids);
        y += 50;
        drawQueue(hdc, x, y, "Ready Queue L2 (q=4):", &g_state->ready_queue_2, slot_pids);
        y += 50;
        drawQueue(hdc, x, y, "Ready Queue L3 (q=8):", &g_state->ready_queue_3, slot_pids);
        y += 50;
    }
    
    drawQueue(hdc, x, y, "Blocked: Console Read", &g_state->mutexes->input_queue, slot_pids);
    y += 50;
    drawQueue(hdc, x, y, "Blocked: Console Write", &g_state->mutexes->output_queue, slot_pids);
    y += 50;
    
    drawQueue(hdc, x, y, "Blocked: File", &g_state->mutexes->file_queue, slot_pids);
    y += 50;
    
    /* HRRN Ratios */
    if (g_state->current_algo == SCHED_HRRN && g_state->ready_queue.count > 0) {
        SelectObject(hdc, g_font_bold);
        SetTextColor(hdc, WIN11_TEXT);
        TextOut(hdc, x, y, "HRRN Response Ratios:", 21);
        y += 22;
        
        SelectObject(hdc, g_font_normal);
        SetTextColor(hdc, WIN11_TEXT_SEC);
        sprintf(buf, "  %-8s %-10s %-10s %-10s %-10s", "PID", "Arrival", "Wait", "Burst", "Ratio");
        TextOut(hdc, x, y, buf, (int)strlen(buf));
        y += 18;
        
        HPEN pen = CreatePen(PS_SOLID, 1, WIN11_BORDER);
        HPEN oldPen = SelectObject(hdc, pen);
        MoveToEx(hdc, x, y, NULL);
        LineTo(hdc, x + 400, y);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
        y += 4;
        
        int current_time = g_state->current_tick_count;
        for (int i = 0; i < g_state->ready_queue.count; i++) {
            int idx = (g_state->ready_queue.front + i) % MAX_SIZE;
            int pid = g_state->ready_queue.process_ids[idx];
            PCB *pcb = findPCB_FromID(g_emu, pid);
            
            int arrival = 0;
            for (int j = 0; j < g_state->num_scheduled_processes; j++) {
                if (g_state->scheduled_processes[j].pid == pid) {
                    arrival = g_state->scheduled_processes[j].spawn_time;
                    break;
                }
            }
            
            if (pcb != NULL) {
                int wait = current_time - arrival;
                int burst = (pcb->bounds[3] - pcb->bounds[2]) + 1;
                float ratio = (float)(wait + burst) / (float)burst;
                
                SetTextColor(hdc, WIN11_TEXT);
                sprintf(buf, "  P%-6d %-10d %-10d %-10d %-10.2f", pid, arrival, wait, burst, ratio);
                TextOut(hdc, x, y, buf, (int)strlen(buf));
                y += 18;
            }
        }
    }
}

/* ========== TAB: SYSTEM LOGS (SIMULATION) ========== */
static void drawLogsTab(HDC hdc, RECT *client) {
    char buf[128];
    int x = GRID_X;
    int y = GRID_Y - 50;
    int logs_top;
    int logs_bottom;
    int line_h = 16;
    int max_visible;
    int max_offset;
    int start;
    int end;
    
    SelectObject(hdc, g_font_title);
    SetTextColor(hdc, WIN11_TEXT);
    TextOut(hdc, x, y, "System Execution Logs", 21);
    
    y += 40;
    logs_top = y;
    logs_bottom = client->bottom - STATUS_H - 8;
    SelectObject(hdc, g_font_small); /* Use small font for logs */
    SetTextColor(hdc, RGB(180, 255, 180)); /* Hacker Green log tint */
    
    if (g_gui_log_count == 0) {
        TextOut(hdc, x, y, "Logs empty.", 11);
        return;
    }
    
    max_visible = (logs_bottom - logs_top) / line_h;
    if (max_visible < 1) max_visible = 1;

    max_offset = g_gui_log_count - max_visible;
    if (max_offset < 0) max_offset = 0;

    if (g_logs_scroll_offset > max_offset) g_logs_scroll_offset = max_offset;
    if (g_logs_scroll_offset < 0) g_logs_scroll_offset = 0;

    start = g_gui_log_count - max_visible - g_logs_scroll_offset;
    if (start < 0) start = 0;
    end = start + max_visible;
    if (end > g_gui_log_count) end = g_gui_log_count;

    for (int i = start; i < end; i++) {
        TextOut(hdc, x, y, g_gui_logs[i], strlen(g_gui_logs[i]));
        y += line_h;
    }

    SetTextColor(hdc, WIN11_TEXT_SEC);
    sprintf(buf, "Mouse wheel/Up/Down to scroll (%d/%d)", g_logs_scroll_offset, max_offset);
    TextOut(hdc, x, logs_bottom + 2, buf, (int)strlen(buf));
}

/* ========== STATUS BAR ========== */
static void drawStatusBar(HDC hdc, RECT *client) {
    int y = client->bottom - STATUS_H;
    
    HBRUSH bg = CreateSolidBrush(WIN11_BG);
    RECT r = { 0, y, client->right, client->bottom };
    FillRect(hdc, &r, bg);
    DeleteObject(bg);
    
    HPEN pen = CreatePen(PS_SOLID, 1, WIN11_BORDER);
    HPEN oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, 0, y, NULL);
    LineTo(hdc, client->right, y);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_font_normal);
    char buf[512];
    
    if (g_gui_mode != GUI_STATE_SIMULATION) {
        SetTextColor(hdc, WIN11_TEXT_SEC);
        TextOut(hdc, 10, y + 12, "Configuration Mode", 18);
        return;
    }
    
    if (g_active_tab == TAB_MEMORY && g_hovered_cell >= 0 && g_hovered_cell < 40) {
        struct MemoryWord *word = (struct MemoryWord*)readMem(g_emu, g_hovered_cell);
        int slot_pids[40];
        computeSlotOwnership(slot_pids);
        int pid = slot_pids[g_hovered_cell];
        
        if (word == NULL) {
            sprintf(buf, "Slot %d: EMPTY (free memory)", g_hovered_cell);
        } else {
            switch (word->type) {
                case MEM_TYPE_PCB:
                    sprintf(buf, "Slot %d: PCB | PID=%d  State=%s  PC=%d  Bounds=[%d,%d,%d,%d]",
                            g_hovered_cell, word->content.pcb_data.ProcessID,
                            stateToStr(word->content.pcb_data.state),
                            word->content.pcb_data.pc,
                            word->content.pcb_data.bounds[0], word->content.pcb_data.bounds[1],
                            word->content.pcb_data.bounds[2], word->content.pcb_data.bounds[3]);
                    break;
                case MEM_TYPE_VARIABLE:
                    sprintf(buf, "Slot %d: VARIABLE | Owner=PID %d  Name=\"%s\"  Value=\"%s\"",
                            g_hovered_cell, pid,
                            word->content.var_data.name, word->content.var_data.value);
                    break;
                case MEM_TYPE_INSTRUCTION:
                    sprintf(buf, "Slot %d: INSTRUCTION | Owner=PID %d  Code=\"%s\"",
                            g_hovered_cell, pid,
                            word->content.inst_data.code_line);
                    break;
                default:
                    sprintf(buf, "Slot %d: UNKNOWN TYPE", g_hovered_cell);
            }
        }
        SetTextColor(hdc, WIN11_TEXT);
        TextOut(hdc, 10, y + 12, buf, (int)strlen(buf));
    } else if (g_active_tab == TAB_SWAP && g_swap_selected_pid != -1 && g_hovered_swap_cell >= 0 && g_hovered_swap_cell < g_swap_words_count) {
        struct MemoryWord *word = &g_swap_words[g_hovered_swap_cell];
        if (word->type == MEM_TYPE_PCB) {
            sprintf(buf, "Swap PID %d Slot %d: PCB | State=%s PC=%d Bounds=[%d,%d,%d,%d]",
                    g_swap_selected_pid, g_hovered_swap_cell,
                    stateToStr(word->content.pcb_data.state),
                    word->content.pcb_data.pc,
                    word->content.pcb_data.bounds[0], word->content.pcb_data.bounds[1],
                    word->content.pcb_data.bounds[2], word->content.pcb_data.bounds[3]);
        } else if (word->type == MEM_TYPE_VARIABLE) {
            sprintf(buf, "Swap PID %d Slot %d: VARIABLE | Name=\"%s\" Value=\"%s\"",
                    g_swap_selected_pid, g_hovered_swap_cell,
                    word->content.var_data.name, word->content.var_data.value);
        } else if (word->type == MEM_TYPE_INSTRUCTION) {
            sprintf(buf, "Swap PID %d Slot %d: INSTRUCTION | Code=\"%s\"",
                    g_swap_selected_pid, g_hovered_swap_cell,
                    word->content.inst_data.code_line);
        } else {
            sprintf(buf, "Swap PID %d Slot %d: UNKNOWN", g_swap_selected_pid, g_hovered_swap_cell);
        }
        SetTextColor(hdc, WIN11_TEXT);
        TextOut(hdc, 10, y + 12, buf, (int)strlen(buf));
    } else {
        SetTextColor(hdc, WIN11_TEXT_SEC);
        if (g_active_tab == TAB_MEMORY) {
            TextOut(hdc, 10, y + 12, "Hover over a memory cell to see details", 39);
        } else if (g_active_tab == TAB_SWAP) {
            TextOut(hdc, 10, y + 12, "Hover over a swap cell to see details", 37);
        }
    }
}

/* ========== WM_PAINT MAIN ========== */
static void paintWindow(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client; GetClientRect(hwnd, &client);
    
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HBITMAP oldBmp = SelectObject(memDC, memBmp);
    
    /* Background Surface */
    HBRUSH bgBrush = CreateSolidBrush(WIN11_SURFACE);
    FillRect(memDC, &client, bgBrush);
    DeleteObject(bgBrush);
    /* Render Core Content based on state */
    switch (g_gui_mode) {
        case GUI_STATE_WELCOME:
            drawWelcomeTab(memDC, &client);
            break;
        case GUI_STATE_CONFIG_ALGO:
            drawConfigAlgoTab(memDC, &client);
            break;
        case GUI_STATE_CONFIG_PROCS:
            drawConfigProcsTab(memDC, &client);
            break;
        case GUI_STATE_SIMULATION:
            if (g_emu != NULL && g_state != NULL) {
                /* Sidebar Drawing */
                HBRUSH sbBrush = CreateSolidBrush(WIN11_OVERLAY);
                RECT sbRect = { 0, 0, 200, client.bottom };
                FillRect(memDC, &sbRect, sbBrush);
                DeleteObject(sbBrush);
                
                SetBkMode(memDC, TRANSPARENT);
                SelectObject(memDC, g_font_title);
                SetTextColor(memDC, WIN11_TEXT);
                TextOut(memDC, 20, 20, "Ethos", 5);
                
                SelectObject(memDC, g_font_normal);
                const char* tab_names[] = { "Memory Metrics", "Disk Swap View", "System Queue", "System Logs" };
                for (int i=0; i<4; i++) {
                    int ty = 60 + (i * 40);
                    if (g_active_tab == i) {
                        HBRUSH actBrush = CreateSolidBrush(WIN11_BG);
                        RECT aRect = { 0, ty, 200, ty + 40 };
                        FillRect(memDC, &aRect, actBrush);
                        DeleteObject(actBrush);
                        /* Accent line */
                        HBRUSH accent = CreateSolidBrush(WIN11_ACCENT);
                        RECT lRect = { 0, ty, 4, ty + 40 };
                        FillRect(memDC, &lRect, accent);
                        DeleteObject(accent);
                        SetTextColor(memDC, WIN11_TEXT);
                    } else {
                        SetTextColor(memDC, WIN11_TEXT_SEC);
                    }
                    TextOut(memDC, 20, ty + 10, tab_names[i], strlen(tab_names[i]));
                }
                
                /* Clock / Tick Count */
                SelectObject(memDC, g_font_bold);
                SetTextColor(memDC, WIN11_TEXT);
                char tickBuf[64];
                char runBuf[192];
                char trimmed_cmd[70];
                sprintf(tickBuf, "Tick Count: %04d", g_state->current_tick_count);
                TextOut(memDC, client.right - 180, 20, tickBuf, strlen(tickBuf));

                if (g_state->current_running_command[0] != '\0') {
                    strncpy(trimmed_cmd, g_state->current_running_command, sizeof(trimmed_cmd) - 1);
                    trimmed_cmd[sizeof(trimmed_cmd) - 1] = '\0';
                } else {
                    strncpy(trimmed_cmd, "IDLE", sizeof(trimmed_cmd) - 1);
                    trimmed_cmd[sizeof(trimmed_cmd) - 1] = '\0';
                }

                if (strlen(trimmed_cmd) > 44) {
                    trimmed_cmd[44] = '.';
                    trimmed_cmd[45] = '.';
                    trimmed_cmd[46] = '.';
                    trimmed_cmd[47] = '\0';
                }

                if (g_state->current_running_pid > 0) {
                    sprintf(runBuf, "PID:%d | %s", g_state->current_running_pid, trimmed_cmd);
                } else {
                    sprintf(runBuf, "PID:-- | %s", trimmed_cmd);
                }

                SelectObject(memDC, g_font_normal);
                SetTextColor(memDC, WIN11_TEXT_SEC);
                TextOut(memDC, client.right - 620, 22, runBuf, (int)strlen(runBuf));
                
                switch (g_active_tab) {
                    case TAB_MEMORY:  drawMemoryTab(memDC, &client);  break;
                    case TAB_SWAP:    drawSwapTab(memDC, &client);    break;
                    case TAB_SYSTEM:  drawSystemTab(memDC, &client);  break;
                    case TAB_LOGS:    drawLogsTab(memDC, &client);    break;
                }
            }
            break;
    }
    
    drawStatusBar(memDC, &client);
    
    BitBlt(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
}

/* ========== CONTROLLER LOGIC ========== */
static void onConfigAlgoNext(void) {
    int algo_idx = SendMessage(hCboAlgo, CB_GETCURSEL, 0, 0);
    g_cfg_algo = (algo_idx == 0) ? SCHED_RR : (algo_idx == 1) ? SCHED_HRRN : SCHED_MLFQ;
    
    char buf[64];
    GetWindowText(hTxtQuantum, buf, 64);
    g_cfg_quantum = atoi(buf);
    
    GetWindowText(hTxtProcs, buf, 64);
    g_cfg_num_procs = atoi(buf);

    g_cfg_auto_release_resources = (SendMessage(hChkAutoRelease, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
    g_cfg_skip_empty_lines = (SendMessage(hChkSkipEmpty, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
    g_cfg_terminate_on_syntax_error = (SendMessage(hChkTerminateSyntax, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
    g_cfg_mlfq_unblock_to_l0 = (SendMessage(hChkMlfqUnblockL0, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
    if (g_cfg_algo != SCHED_MLFQ) {
        g_cfg_mlfq_unblock_to_l0 = 0;
    }
    
    if (g_cfg_num_procs <= 0 || g_cfg_num_procs > 10) {
        MessageBox(g_hwnd, "Number of processes must be between 1 and 10.", "Error", MB_ICONERROR);
        return;
    }
    
    g_temp_arrivals = malloc(sizeof(int) * g_cfg_num_procs);
    g_temp_paths = malloc(sizeof(char*) * g_cfg_num_procs);
    for (int i=0; i<g_cfg_num_procs; i++) {
        g_temp_arrivals[i] = 0;
        g_temp_paths[i] = NULL;
    }
    
    g_cfg_proc_idx = 0;
    g_gui_mode = GUI_STATE_CONFIG_PROCS;
    buildConfigProcsUI(g_hwnd);
    InvalidateRect(g_hwnd, NULL, TRUE);
}

static void onConfigProcsSwitch(int processIndex) {
    // Save current proc data
    char buf[256];
    GetWindowText(hTxtArrive, buf, 256);
    g_temp_arrivals[g_cfg_proc_idx] = atoi(buf);
    
    // Switch target
    g_cfg_proc_idx = processIndex;
    
    // Rebuild UI for target
    buildConfigProcsUI(g_hwnd);
    InvalidateRect(g_hwnd, NULL, TRUE);
}

static void onConfigStartSim(void) {
    // Save last proc data
    char buf[256];
    GetWindowText(hTxtArrive, buf, 256);
    g_temp_arrivals[g_cfg_proc_idx] = atoi(buf);
    
    // Validate
    for (int i = 0; i < g_cfg_num_procs; i++) {
        if (g_temp_paths[i] == NULL || strlen(g_temp_paths[i]) == 0) {
            sprintf(buf, "Missing file path for Process %d!", i+1);
            MessageBox(g_hwnd, buf, "Error", MB_ICONERROR);
            return;
        }
    }
    
    // Write directly to Kernel State
    g_state->current_algo = g_cfg_algo;
    g_state->time_quantum = g_cfg_quantum;
    g_state->auto_release_resources = g_cfg_auto_release_resources;
    g_state->skip_empty_lines_on_load = g_cfg_skip_empty_lines;
    g_state->terminate_on_syntax_error = g_cfg_terminate_on_syntax_error;
    g_state->mlfq_unblock_to_l0 = g_cfg_mlfq_unblock_to_l0;
    g_state->num_scheduled_processes = g_cfg_num_procs;
    g_state->scheduled_processes = malloc(sizeof(ArrivalConfig) * g_cfg_num_procs);
    
    for (int i = 0; i < g_cfg_num_procs; i++) {
        g_state->scheduled_processes[i].pid = i + 1;
        g_state->scheduled_processes[i].spawn_time = g_temp_arrivals[i];
        g_state->scheduled_processes[i].filepath = g_temp_paths[i]; 
    }
    
    free(g_temp_arrivals);
    g_temp_arrivals = NULL;
    g_temp_paths = NULL; 
    
    // Launch simulation interface
    g_gui_mode = GUI_STATE_SIMULATION;
    buildSimulationUI(g_hwnd);
    InvalidateRect(g_hwnd, NULL, TRUE);
    
    // Unblock the kernel execution thread!
    SetEvent(g_config_done_event);
}

/* ========== WINDOW PROCEDURE ========== */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            buildWelcomeUI(hwnd);
            return 0;
            
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, WIN11_TEXT);
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        
        case WM_CTLCOLOREDIT: {
            static HBRUSH hEditBrush = NULL;
            if (!hEditBrush) hEditBrush = CreateSolidBrush(WIN11_SURFACE);
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, WIN11_TEXT);
            SetBkColor(hdcEdit, WIN11_SURFACE);
            return (LRESULT)hEditBrush;
        }
        
        case WM_CTLCOLORBTN: {
            static HBRUSH hBtnBrush = NULL;
            if (!hBtnBrush) hBtnBrush = CreateSolidBrush(WIN11_SURFACE);
            return (LRESULT)hBtnBrush;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                // WELCOME BUTTON
                case ID_BTN_START: 
                    g_gui_mode = GUI_STATE_CONFIG_ALGO; 
                    buildConfigAlgoUI(hwnd); 
                    InvalidateRect(hwnd, NULL, TRUE); 
                    break;
                    
                // CONFIG BUTTONS
                case ID_BTN_NEXT:   onConfigAlgoNext(); break;
                case ID_BTN_BROWSE: BrowseFile(); break;
                case ID_BTN_PREVPRO: 
                    if (g_cfg_proc_idx > 0) onConfigProcsSwitch(g_cfg_proc_idx - 1); 
                    else { g_gui_mode = GUI_STATE_CONFIG_ALGO; buildConfigAlgoUI(hwnd); InvalidateRect(hwnd, NULL, TRUE); }
                    break;
                case ID_BTN_NEXTPRO:
                    if (g_cfg_proc_idx < g_cfg_num_procs - 1) onConfigProcsSwitch(g_cfg_proc_idx + 1);
                    else onConfigStartSim();
                    break;
                    
                // SIMULATION BUTTONS
                case ID_TAB_MEM:  g_active_tab = TAB_MEMORY; InvalidateRect(hwnd, NULL, FALSE); break;
                case ID_TAB_SWAP: g_active_tab = TAB_SWAP; InvalidateRect(hwnd, NULL, FALSE); break;
                case ID_TAB_SYS:  g_active_tab = TAB_SYSTEM; InvalidateRect(hwnd, NULL, FALSE); break;
                case ID_BTN_STEP: SetEvent(g_step_event); break;
                
                // EVENTS
                case ID_CBO_ALGO:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        int algo_idx = SendMessage(hCboAlgo, CB_GETCURSEL, 0, 0);
                        EnableWindow(hTxtQuantum, (algo_idx == 0)); 
                        EnableWindow(hChkMlfqUnblockL0, (algo_idx == 2));
                    }
                    break;
            }
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            if (g_gui_mode == GUI_STATE_SIMULATION && g_active_tab == TAB_MEMORY) {
                int mx = LOWORD(lParam);
                int my = HIWORD(lParam);
                int col = (mx - GRID_X) / CELL_W;
                int row = (my - GRID_Y) / CELL_H;
                int new_hover = -1;
                /* Offset mx by 200 for the taskbar sidebar! */
                if (col >= 0 && col < GRID_COLS && row >= 0 && row < GRID_ROWS &&
                    mx >= GRID_X && my >= GRID_Y) {
                    new_hover = row * GRID_COLS + col;
                    if (new_hover >= 40) new_hover = -1;
                }
                if (new_hover != g_hovered_cell) {
                    g_hovered_cell = new_hover;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            } else if (g_gui_mode == GUI_STATE_SIMULATION && g_active_tab == TAB_SWAP) {
                int mx = LOWORD(lParam);
                int my = HIWORD(lParam);
                int new_hover = -1;

                if (g_swap_grid_count > 0 && mx >= g_swap_grid_x && my >= g_swap_grid_y) {
                    int col = (mx - g_swap_grid_x) / SWAP_CELL_W;
                    int row = (my - g_swap_grid_y) / SWAP_CELL_H;
                    if (col >= 0 && col < SWAP_GRID_COLS && row >= 0) {
                        new_hover = row * SWAP_GRID_COLS + col;
                        if (new_hover >= g_swap_grid_count) new_hover = -1;
                    }
                }

                if (new_hover != g_hovered_swap_cell) {
                    g_hovered_swap_cell = new_hover;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (g_gui_mode == GUI_STATE_SIMULATION && g_active_tab == TAB_LOGS) {
                short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta > 0) g_logs_scroll_offset++;
                else if (delta < 0 && g_logs_scroll_offset > 0) g_logs_scroll_offset--;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        }

        case WM_KEYDOWN: {
            if (g_gui_mode == GUI_STATE_SIMULATION && g_active_tab == TAB_LOGS) {
                if (wParam == VK_UP) {
                    g_logs_scroll_offset++;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    if (g_logs_scroll_offset > 0) g_logs_scroll_offset--;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            break;
        }
        
        case WM_LBUTTONDOWN: {
            if (g_gui_mode == GUI_STATE_SIMULATION) {
                int mx = LOWORD(lParam);
                int my = HIWORD(lParam);
                if (mx < 200) {
                    if (my >= 60 && my < 100) { g_active_tab = TAB_MEMORY; g_hovered_swap_cell = -1; InvalidateRect(hwnd, NULL, FALSE); }
                    else if (my >= 100 && my < 140) { g_active_tab = TAB_SWAP; InvalidateRect(hwnd, NULL, FALSE); }
                    else if (my >= 140 && my < 180) { g_active_tab = TAB_SYSTEM; g_hovered_swap_cell = -1; InvalidateRect(hwnd, NULL, FALSE); }
                    else if (my >= 180 && my < 220) { g_active_tab = TAB_LOGS; g_hovered_swap_cell = -1; InvalidateRect(hwnd, NULL, FALSE); }
                } else if (g_active_tab == TAB_SWAP) {
                    POINT p;
                    p.x = mx;
                    p.y = my;
                    for (int i = 0; i < g_swap_pid_chip_count; i++) {
                        if (PtInRect(&g_swap_pid_chip_rects[i], p)) {
                            g_swap_selected_pid = g_swap_pid_chip_values[i];
                            g_hovered_swap_cell = -1;
                            InvalidateRect(hwnd, NULL, FALSE);
                            break;
                        }
                    }
                }
            }
            return 0;
        }
        
        case WM_PAINT:
            paintWindow(hwnd);
            return 0;
        
        case WM_CLOSE:
            g_running = 0;
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/* ========== GUI THREAD ========== */
static DWORD WINAPI guiThreadFunc(LPVOID param) {
    (void)param;
    
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "EthosGuiClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassEx(&wc);
    
    // Use macOS-style modern proportional fonts instead of fixed width Consolas
    g_font_normal = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_font_bold = CreateFont(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_font_title = CreateFont(26, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_font_small = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    
    g_hwnd = CreateWindowEx(
        0, "EthosGuiClass", "Project Ethos OS - GUI Mode",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H,
        NULL, NULL, GetModuleHandle(NULL), NULL);
    
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    
    MSG msg;
    while (g_running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (g_font_normal) DeleteObject(g_font_normal);
    if (g_font_bold)   DeleteObject(g_font_bold);
    if (g_font_title)  DeleteObject(g_font_title);
    if (g_font_small)  DeleteObject(g_font_small);
    
    return 0;
}

/* ========== PUBLIC API ========== */

void startGui(Emulator *emu, kernal_state *state) {
    g_emu = emu;
    g_state = state;
    g_running = 1;
    g_gui_mode = GUI_STATE_WELCOME;
    g_cfg_proc_idx = 0;
    g_cfg_algo = SCHED_RR;
    g_cfg_quantum = 2;
    g_cfg_num_procs = 0;
    g_cfg_auto_release_resources = 0;
    g_cfg_skip_empty_lines = 0;
    g_cfg_terminate_on_syntax_error = 0;
    g_cfg_mlfq_unblock_to_l0 = 0;
    
    g_use_gui_logs = 1;
    setConsoleOutputPassthrough(0);
    g_logs_scroll_offset = 0;
    g_hovered_swap_cell = -1;
    g_swap_selected_pid = -1;
    g_swap_words_count = 0;
    g_swap_grid_count = 0;
    g_swap_pid_chip_count = 0;
    
    g_step_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_config_done_event = CreateEvent(NULL, TRUE, FALSE, NULL); // Manual reset event
    
    g_gui_thread = CreateThread(NULL, 0, guiThreadFunc, NULL, 0, NULL);
    Sleep(300);
}

void updateGui(void) {
    if (g_hwnd != NULL) {
        InvalidateRect(g_hwnd, NULL, FALSE);
        UpdateWindow(g_hwnd);
    }
}

void waitForGuiStep(void) {
    if (g_step_event != NULL) {
        updateGui();
        WaitForSingleObject(g_step_event, INFINITE);
    }
}

void waitForGuiConfig(void) {
    if (g_config_done_event != NULL) {
        // Main thread blocking until GUI config is completely done.
        WaitForSingleObject(g_config_done_event, INFINITE);
    }
}

void stopGui(void) {
    g_running = 0;
    if (g_hwnd != NULL) {
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
    }
    if (g_gui_thread != NULL) {
        WaitForSingleObject(g_gui_thread, 3000);
        CloseHandle(g_gui_thread);
        g_gui_thread = NULL;
    }
    if (g_step_event != NULL) {
        CloseHandle(g_step_event);
        g_step_event = NULL;
    }
    if (g_config_done_event != NULL) {
        CloseHandle(g_config_done_event);
        g_config_done_event = NULL;
    }
    g_use_gui_logs = 0;
    setConsoleOutputPassthrough(1);
    g_hwnd = NULL;
}
