#include "Gui.h"
#include "../Memory/Memory.h"
#include "../../emu/Mem/Mem.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ========== CONSTANTS ========== */
#define WIN_W       920
#define WIN_H       720
#define GRID_X      30
#define GRID_Y      70
#define CELL_W      100
#define CELL_H      60
#define GRID_COLS   8
#define GRID_ROWS   5
#define STATUS_H    45

/* GUI States */
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

#define ID_TXT_ARRIVE   3001
#define ID_BTN_BROWSE   3002
#define ID_BTN_PREVPRO  3003
#define ID_BTN_NEXTPRO  3004

/* macOS Design Palette */
#define MAC_BG          RGB(245, 245, 247)
#define MAC_SURFACE     RGB(255, 255, 255)
#define MAC_TEXT        RGB(29, 29, 31)
#define MAC_TEXT_SEC    RGB(134, 134, 139)
#define MAC_BLUE        RGB(0, 122, 255)
#define MAC_BORDER      RGB(210, 210, 215)

/* Tab indices */
#define TAB_MEMORY  0
#define TAB_SWAP    1
#define TAB_SYSTEM  2

/* PID colors */
static COLORREF pid_colors[] = {
    RGB(230, 230, 230),  /* 0: Empty */
    RGB(84, 199, 252),   /* 1: Light Blue */
    RGB(255, 149, 0),    /* 2: Orange */
    RGB(255, 59, 48),    /* 3: Red */
    RGB(76, 217, 100),   /* 4: Green */
    RGB(88, 86, 214),    /* 5: Purple */
    RGB(255, 204, 0),    /* 6: Yellow */
    RGB(255, 45, 85),    /* 7: Pink */
    RGB(90, 200, 250),   /* 8: Teal */
    RGB(142, 142, 147),  /* 9: Grey */
    RGB(198, 156, 109),  /* 10: Brown */
};
#define NUM_PID_COLORS 11

/* ========== GLOBAL STATE ========== */
static Emulator *g_emu = NULL;
static kernal_state *g_state = NULL;
static HWND g_hwnd = NULL;

static HANDLE g_step_event = NULL;
static HANDLE g_config_done_event = NULL;
static HANDLE g_gui_thread = NULL;

static int g_gui_mode = GUI_STATE_CONFIG_ALGO;
static int g_active_tab = TAB_MEMORY;
static int g_hovered_cell = -1;
static volatile int g_running = 1;

static HFONT g_font_normal = NULL;
static HFONT g_font_bold = NULL;
static HFONT g_font_title = NULL;
static HFONT g_font_small = NULL;

/* Config State */
static int g_cfg_algo = 0;
static int g_cfg_quantum = 2;
static int g_cfg_num_procs = 0;
static int g_cfg_proc_idx = 0;

static int *g_temp_arrivals = NULL;
static char **g_temp_paths = NULL;

/* Child HWNDs */
static HWND hCboAlgo, hTxtQuantum, hTxtProcs, hBtnNext;
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
    SetTextColor(hdc, MAC_TEXT);
    SelectObject(hdc, g_font_bold);
    TextOut(hdc, x, y, label, (int)strlen(label));
    
    SelectObject(hdc, g_font_normal);
    if (q->count == 0) {
        SetTextColor(hdc, MAC_TEXT_SEC);
        TextOut(hdc, x + 10, y + 22, "(empty)", 7);
        return;
    }
    
    SetTextColor(hdc, MAC_TEXT);
    int bx = x + 10;
    for (int i = 0; i < q->count; i++) {
        int idx = (q->front + i) % MAX_SIZE;
        int pid = q->process_ids[idx];
        
        DrawRoundRect(hdc, bx, y + 20, 45, 25, 8, getColorForPid(pid), MAC_BORDER, 1);
        
        sprintf(buf, "P%d", pid);
        SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, bx + 12, y + 24, buf, (int)strlen(buf));
        
        bx += 50;
    }
}

/* ========== STATE: CONFIG ALGO ========== */
static void buildConfigAlgoUI(HWND hwnd) {
    DestroyAllChildWindows(hwnd);
    
    CreateWindow("STATIC", "Scheduling Algorithm:", WS_CHILD | WS_VISIBLE, 
                 300, 200, 200, 20, hwnd, NULL, NULL, NULL);
    hCboAlgo = CreateWindow("COMBOBOX", "", CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
                 300, 220, 320, 150, hwnd, (HMENU)ID_CBO_ALGO, NULL, NULL);
    SendMessage(hCboAlgo, CB_ADDSTRING, 0, (LPARAM)"Round Robin (RR)");
    SendMessage(hCboAlgo, CB_ADDSTRING, 0, (LPARAM)"Highest Response Ratio Next (HRRN)");
    SendMessage(hCboAlgo, CB_ADDSTRING, 0, (LPARAM)"Multi-Level Feedback Queue (MLFQ)");
    SendMessage(hCboAlgo, CB_SETCURSEL, 0, 0);

    CreateWindow("STATIC", "Time Quantum (RR only):", WS_CHILD | WS_VISIBLE, 
                 300, 270, 200, 20, hwnd, NULL, NULL, NULL);
    hTxtQuantum = CreateWindow("EDIT", "2", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | WS_TABSTOP,
                 300, 290, 80, 25, hwnd, (HMENU)ID_TXT_QUANTUM, NULL, NULL);

    CreateWindow("STATIC", "Number of Processes:", WS_CHILD | WS_VISIBLE, 
                 300, 340, 200, 20, hwnd, NULL, NULL, NULL);
    hTxtProcs = CreateWindow("EDIT", "3", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | WS_TABSTOP,
                 300, 360, 80, 25, hwnd, (HMENU)ID_TXT_PROCS, NULL, NULL);

    hBtnNext = CreateWindow("BUTTON", "Next ->", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 500, 420, 120, 35, hwnd, (HMENU)ID_BTN_NEXT, NULL, NULL);
                 
    EnumChildWindows(hwnd, (WNDENUMPROC)SendMessage, (LPARAM)WM_SETFONT);
    SendMessage(hwnd, WM_SETFONT, (WPARAM)g_font_normal, MAKELPARAM(TRUE, 0));
    
    // Default system font for child controls
    HFONT sysFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hCboAlgo, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hTxtQuantum, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hTxtProcs, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hBtnNext, WM_SETFONT, (WPARAM)sysFont, 0);
}

static void drawConfigAlgoTab(HDC hdc, RECT *client) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, MAC_TEXT);
    SelectObject(hdc, g_font_title);
    TextOut(hdc, 300, 120, "System Configuration", 20);
    
    SelectObject(hdc, g_font_normal);
    SetTextColor(hdc, MAC_TEXT_SEC);
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
    SetTextColor(hdc, MAC_TEXT);
    SelectObject(hdc, g_font_title);
    
    char buf[128];
    sprintf(buf, "Process %d of %d", g_cfg_proc_idx + 1, g_cfg_num_procs);
    TextOut(hdc, 300, 120, buf, (int)strlen(buf));
    
    SelectObject(hdc, g_font_normal);
    SetTextColor(hdc, MAC_TEXT_SEC);
    TextOut(hdc, 300, 160, "Step 2: Configure incoming processes", 36);
    
    // Draw Current Path
    SetTextColor(hdc, MAC_TEXT);
    TextOut(hdc, 300, 270, "Selected File:", 14);
    
    char* curPath = g_temp_paths[g_cfg_proc_idx];
    if (curPath != NULL && strlen(curPath) > 0) {
        SetTextColor(hdc, MAC_BLUE);
        TextOut(hdc, 300, 340, curPath, (int)strlen(curPath));
    } else {
        SetTextColor(hdc, RGB(220, 50, 50));
        TextOut(hdc, 300, 340, "No file selected. Please browse.", 32);
    }
}

/* ========== STATE: SIMULATION ========== */
static void buildSimulationUI(HWND hwnd) {
    DestroyAllChildWindows(hwnd);
    
    hTabMem = CreateWindow("BUTTON", "Memory", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 480, 8, 80, 28, hwnd, (HMENU)ID_TAB_MEM, NULL, NULL);
    hTabSwap = CreateWindow("BUTTON", "Swap", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 565, 8, 80, 28, hwnd, (HMENU)ID_TAB_SWAP, NULL, NULL);
    hTabSys = CreateWindow("BUTTON", "System", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 650, 8, 80, 28, hwnd, (HMENU)ID_TAB_SYS, NULL, NULL);
    hBtnStep = CreateWindow("BUTTON", "Step >>", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                 745, 8, 70, 28, hwnd, (HMENU)ID_BTN_STEP, NULL, NULL);

    HFONT sysFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hTabMem, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hTabSwap, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hTabSys, WM_SETFONT, (WPARAM)sysFont, 0);
    SendMessage(hBtnStep, WM_SETFONT, (WPARAM)sysFont, 0);
}

/* ========== TAB: MEMORY (SIMULATION) ========== */
static void drawMemoryTab(HDC hdc, RECT *client) {
    int slot_pids[40];
    computeSlotOwnership(slot_pids);
    
    char buf[128];
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, MAC_TEXT);
    TextOut(hdc, GRID_X, GRID_Y - 20, "Physical Memory (40 words)", 26);
    
    SelectObject(hdc, g_font_normal);
    
    for (int i = 0; i < 40; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        int x = GRID_X + col * CELL_W;
        int y = GRID_Y + row * CELL_H;
        
        struct MemoryWord *word = (struct MemoryWord*)readMem(g_emu, i);
        int pid = slot_pids[i];
        COLORREF color = getColorForPid(pid);
        COLORREF border = (i == g_hovered_cell) ? MAC_BLUE : MAC_BORDER;
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
            SetTextColor(hdc, MAC_TEXT);
            sprintf(buf, "%s", memTypeToStr(word->type));
            TextOut(hdc, x + 30, y + 18, buf, (int)strlen(buf));
            if (pid > 0) {
                sprintf(buf, "P%d", pid);
                TextOut(hdc, x + 35, y + 36, buf, (int)strlen(buf));
            }
        } else {
            SetTextColor(hdc, MAC_TEXT_SEC);
            TextOut(hdc, x + 25, y + 22, "FREE", 4);
        }
    }
    
    /* Legend */
    int ly = GRID_Y + GRID_ROWS * CELL_H + 15;
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, MAC_TEXT);
    TextOut(hdc, GRID_X, ly, "Legend:", 7);
    
    SelectObject(hdc, g_font_normal);
    int lx = GRID_X + 70;
    
    /* Empty */
    DrawRoundRect(hdc, lx, ly, 20, 16, 4, pid_colors[0], MAC_BORDER, 1);
    TextOut(hdc, lx + 25, ly, "Empty", 5);
    lx += 90;
    
    for (int p = 1; p < NUM_PID_COLORS; p++) {
        int found = 0;
        for (int s = 0; s < 40; s++) {
            if (slot_pids[s] == p) { found = 1; break; }
        }
        if (!found) continue;
        
        DrawRoundRect(hdc, lx, ly, 20, 16, 4, pid_colors[p], MAC_BORDER, 1);
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
    
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, MAC_TEXT);
    TextOut(hdc, GRID_X, y - 20, "Swap Memory (on disk)", 21);
    
    SelectObject(hdc, g_font_normal);
    int found_any = 0;
    
    for (int p = 1; p <= 10; p++) {
        char filename[64];
        sprintf(filename, "swap_pid_%d.bin", p);
        FILE *f = fopen(filename, "rb");
        if (f != NULL) {
            found_any = 1;
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fclose(f);
            
            DrawRoundRect(hdc, GRID_X, y, 600, 40, 10, getColorForPid(p), MAC_BORDER, 1);
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, MAC_TEXT);
            sprintf(buf, "  PID %d  |  File: %s  |  Size: %ld bytes", p, filename, size);
            TextOut(hdc, GRID_X + 15, y + 10, buf, (int)strlen(buf));
            
            y += 50;
        }
    }
    
    if (!found_any) {
        SetTextColor(hdc, MAC_TEXT_SEC);
        TextOut(hdc, GRID_X + 10, y + 10, "No processes currently swapped to disk.", 39);
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
    SetTextColor(hdc, MAC_TEXT);
    TextOut(hdc, x, y, "Active Process", 14);
    y += 22;
    
    SelectObject(hdc, g_font_normal);
    PCB *active = getActivePCB(g_emu);
    if (active != NULL) {
        DrawRoundRect(hdc, x, y, 420, 75, 12, getColorForPid(active->ProcessID), MAC_BORDER, 1);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, MAC_TEXT);
        sprintf(buf, "  PID: %d   State: %s   PC: %d", active->ProcessID, stateToStr(active->state), active->pc);
        TextOut(hdc, x + 5, y + 8, buf, (int)strlen(buf));
        sprintf(buf, "  Bounds: [%d, %d, %d, %d]", active->bounds[0], active->bounds[1], active->bounds[2], active->bounds[3]);
        TextOut(hdc, x + 5, y + 28, buf, (int)strlen(buf));
        
        if (g_state->current_algo == SCHED_RR) {
            sprintf(buf, "  RR Quantum: %d / %d", g_state->rr_time_quantum_counter, g_state->time_quantum);
            TextOut(hdc, x + 5, y + 48, buf, (int)strlen(buf));
        } else if (g_state->current_algo == SCHED_MLFQ) {
            sprintf(buf, "  MLFQ Level: %d   Quantum: %d / %d", 
                    g_state->active_process_queue_index, 
                    g_state->mlfq_time_quantum_counter,
                    1 << g_state->active_process_queue_index);
            TextOut(hdc, x + 5, y + 48, buf, (int)strlen(buf));
        }
    } else {
        SetTextColor(hdc, MAC_TEXT_SEC);
        TextOut(hdc, x + 10, y + 5, "No active process (CPU IDLE)", 28);
    }
    y += 90;
    
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, MAC_TEXT);
    sprintf(buf, "Scheduler: %s", algoToStr(g_state->current_algo));
    TextOut(hdc, x, y, buf, (int)strlen(buf));
    y += 28;
    
    drawQueue(hdc, x, y, "Ready Queue:", &g_state->ready_queue, slot_pids);
    y += 50;
    
    if (g_state->current_algo == SCHED_MLFQ) {
        drawQueue(hdc, x, y, "Ready Queue L1 (q=1):", &g_state->ready_queue_1, slot_pids);
        y += 50;
        drawQueue(hdc, x, y, "Ready Queue L2 (q=2):", &g_state->ready_queue_2, slot_pids);
        y += 50;
        drawQueue(hdc, x, y, "Ready Queue L3 (q=4):", &g_state->ready_queue_3, slot_pids);
        y += 50;
    }
    
    drawQueue(hdc, x, y, "Blocked Queue:", &g_state->general_blocked_queue, slot_pids);
    y += 55;
    
    /* HRRN Ratios */
    if (g_state->current_algo == SCHED_HRRN && g_state->ready_queue.count > 0) {
        SelectObject(hdc, g_font_bold);
        SetTextColor(hdc, MAC_TEXT);
        TextOut(hdc, x, y, "HRRN Response Ratios:", 21);
        y += 22;
        
        SelectObject(hdc, g_font_normal);
        SetTextColor(hdc, MAC_TEXT_SEC);
        sprintf(buf, "  %-8s %-10s %-10s %-10s %-10s", "PID", "Arrival", "Wait", "Burst", "Ratio");
        TextOut(hdc, x, y, buf, (int)strlen(buf));
        y += 18;
        
        HPEN pen = CreatePen(PS_SOLID, 1, MAC_BORDER);
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
                
                SetTextColor(hdc, MAC_TEXT);
                sprintf(buf, "  P%-6d %-10d %-10d %-10d %-10.2f", pid, arrival, wait, burst, ratio);
                TextOut(hdc, x, y, buf, (int)strlen(buf));
                y += 18;
            }
        }
    }
}

/* ========== STATUS BAR ========== */
static void drawStatusBar(HDC hdc, RECT *client) {
    int y = client->bottom - STATUS_H;
    
    HBRUSH bg = CreateSolidBrush(MAC_BG);
    RECT r = { 0, y, client->right, client->bottom };
    FillRect(hdc, &r, bg);
    DeleteObject(bg);
    
    HPEN pen = CreatePen(PS_SOLID, 1, MAC_BORDER);
    HPEN oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, 0, y, NULL);
    LineTo(hdc, client->right, y);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_font_normal);
    char buf[512];
    
    if (g_gui_mode != GUI_STATE_SIMULATION) {
        SetTextColor(hdc, MAC_TEXT_SEC);
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
        SetTextColor(hdc, MAC_TEXT);
        TextOut(hdc, 10, y + 12, buf, (int)strlen(buf));
    } else {
        SetTextColor(hdc, MAC_TEXT_SEC);
        if (g_active_tab == TAB_MEMORY) {
            TextOut(hdc, 10, y + 12, "Hover over a memory cell to see details", 39);
        }
    }
    
    /* Tick counter */
    sprintf(buf, "Tick: %d", g_state->current_tick_count);
    SetTextColor(hdc, MAC_TEXT);
    SelectObject(hdc, g_font_bold);
    TextOut(hdc, client->right - 100, y + 12, buf, (int)strlen(buf));
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
    HBRUSH bgBrush = CreateSolidBrush(MAC_SURFACE);
    FillRect(memDC, &client, bgBrush);
    DeleteObject(bgBrush);
    
    /* macOS Window Header Bar */
    HBRUSH hdrBrush = CreateSolidBrush(MAC_BG);
    RECT hdrRect = { 0, 0, client.right, 45 };
    FillRect(memDC, &hdrRect, hdrBrush);
    DeleteObject(hdrBrush);
    
    HPEN hdrPen = CreatePen(PS_SOLID, 1, MAC_BORDER);
    HPEN oldPen = SelectObject(memDC, hdrPen);
    MoveToEx(memDC, 0, 45, NULL);
    LineTo(memDC, client.right, 45);
    SelectObject(memDC, oldPen);
    DeleteObject(hdrPen);
    
    /* Title */
    SetBkMode(memDC, TRANSPARENT);
    SelectObject(memDC, g_font_bold);
    SetTextColor(memDC, MAC_TEXT);
    TextOut(memDC, 15, 12, "Project Ethos OS", 16);
    
    /* Tick in header */
    if (g_gui_mode == GUI_STATE_SIMULATION) {
        char tickBuf[64];
        sprintf(tickBuf, "TICK: %d", g_state->current_tick_count);
        TextOut(memDC, client.right - 130, 12, tickBuf, (int)strlen(tickBuf));
    }
    
    /* Render Core Content based on state */
    switch (g_gui_mode) {
        case GUI_STATE_CONFIG_ALGO:
            drawConfigAlgoTab(memDC, &client);
            break;
        case GUI_STATE_CONFIG_PROCS:
            drawConfigProcsTab(memDC, &client);
            break;
        case GUI_STATE_SIMULATION:
            if (g_emu != NULL && g_state != NULL) {
                switch (g_active_tab) {
                    case TAB_MEMORY:  drawMemoryTab(memDC, &client);  break;
                    case TAB_SWAP:    drawSwapTab(memDC, &client);    break;
                    case TAB_SYSTEM:  drawSystemTab(memDC, &client);  break;
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
            buildConfigAlgoUI(hwnd);
            return 0;
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
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
                if (col >= 0 && col < GRID_COLS && row >= 0 && row < GRID_ROWS &&
                    mx >= GRID_X && my >= GRID_Y) {
                    new_hover = row * GRID_COLS + col;
                    if (new_hover >= 40) new_hover = -1;
                }
                if (new_hover != g_hovered_cell) {
                    g_hovered_cell = new_hover;
                    InvalidateRect(hwnd, NULL, FALSE);
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
    g_gui_mode = GUI_STATE_CONFIG_ALGO;
    g_cfg_proc_idx = 0;
    
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
    g_hwnd = NULL;
}
