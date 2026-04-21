#include "Gui.h"
#include "../Memory/Memory.h"
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
#define TAB_CONSOLE 4

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

static HWND hTxtTerminalIn, hBtnTerminalSub;
static HANDLE g_terminal_event = NULL;
static char g_terminal_buffer[256] = {0};

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

    // Terminal Inputs (Hidden by default)
    hTxtTerminalIn = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                 GRID_X, WIN_H - 150, 500, 30, hwnd, (HMENU)1006, NULL, NULL);
    hBtnTerminalSub = CreateWindow("BUTTON", "Submit", WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
                 GRID_X + 510, WIN_H - 150, 100, 30, hwnd, (HMENU)1007, NULL, NULL);

    HFONT sysFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hBtnStep, WM_SETFONT, (WPARAM)sysFont, 0);
    
    HFONT consolas = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, "Consolas");
        
    SendMessage(hTxtTerminalIn, WM_SETFONT, (WPARAM)consolas, 0);
    SendMessage(hBtnTerminalSub, WM_SETFONT, (WPARAM)sysFont, 0);
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
    
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, WIN11_TEXT);
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
            
            DrawRoundRect(hdc, GRID_X, y, 600, 40, 10, getColorForPid(p), WIN11_BORDER, 1);
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, WIN11_TEXT);
            sprintf(buf, "  PID %d  |  File: %s  |  Size: %ld bytes", p, filename, size);
            TextOut(hdc, GRID_X + 15, y + 10, buf, (int)strlen(buf));
            
            y += 50;
        }
    }
    
    if (!found_any) {
        SetTextColor(hdc, WIN11_TEXT_SEC);
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
            sprintf(buf, "  MLFQ Level: %d   Quantum: %d / %d", 
                    g_state->active_process_queue_index, 
                    g_state->mlfq_time_quantum_counter,
                    1 << g_state->active_process_queue_index);
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
    
    drawQueue(hdc, x, y, "Blocked: Console Read", &g_state->mutexes->input_queue, slot_pids);
    y += 50;
    drawQueue(hdc, x, y, "Blocked: Console Write", &g_state->mutexes->output_queue, slot_pids);
    y += 50;
    
    Node *curr = g_state->mutexes->file_mutexes;
    int file_queues_drawn = 0;
    while (curr) {
        sprintf(buf, "Blocked: File '%s'", curr->path);
        drawQueue(hdc, x, y, buf, &curr->blocked_queue, slot_pids);
        y += 50;
        curr = curr->next;
        file_queues_drawn++;
    }
    y += 5; // Extra padding
    
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
    int x = GRID_X;
    int y = GRID_Y - 50;
    
    SelectObject(hdc, g_font_title);
    SetTextColor(hdc, WIN11_TEXT);
    TextOut(hdc, x, y, "System Execution Logs", 21);
    
    y += 40;
    SelectObject(hdc, g_font_small); /* Use small font for logs */
    SetTextColor(hdc, RGB(180, 255, 180)); /* Hacker Green log tint */
    
    if (g_gui_log_count == 0) {
        TextOut(hdc, x, y, "Logs empty.", 11);
        return;
    }
    
    for (int i=0; i<g_gui_log_count; i++) {
        TextOut(hdc, x, y, g_gui_logs[i], strlen(g_gui_logs[i]));
        y += 16;
    }
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
    } else {
        SetTextColor(hdc, WIN11_TEXT_SEC);
        if (g_active_tab == TAB_MEMORY) {
            TextOut(hdc, 10, y + 12, "Hover over a memory cell to see details", 39);
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
                const char* tab_names[] = { "Memory Metrics", "Disk Swap View", "System Queue", "System Logs", "Console Terminal" };
                for (int i=0; i<5; i++) {
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
                sprintf(tickBuf, "Tick Count: %04d", g_state->current_tick_count);
                TextOut(memDC, client.right - 180, 20, tickBuf, strlen(tickBuf));
                
                switch (g_active_tab) {
                    case TAB_MEMORY:  drawMemoryTab(memDC, &client);  break;
                    case TAB_SWAP:    drawSwapTab(memDC, &client);    break;
                    case TAB_SYSTEM:  drawSystemTab(memDC, &client);  break;
                    case TAB_LOGS:    drawLogsTab(memDC, &client);    break;
                    case TAB_CONSOLE: drawLogsTab(memDC, &client);    break;
                }
                
                if (g_active_tab == TAB_CONSOLE) {
                    ShowWindow(hTxtTerminalIn, SW_SHOW);
                    ShowWindow(hBtnTerminalSub, SW_SHOW);
                } else {
                    ShowWindow(hTxtTerminalIn, SW_HIDE);
                    ShowWindow(hBtnTerminalSub, SW_HIDE);
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
                
                case 1007: // Terminal Submit
                    GetWindowText(hTxtTerminalIn, g_terminal_buffer, sizeof(g_terminal_buffer));
                    SetWindowText(hTxtTerminalIn, "");
                    SetEvent(g_terminal_event);
                    break;
                
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
            }
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            if (g_gui_mode == GUI_STATE_SIMULATION) {
                int mx = LOWORD(lParam);
                int my = HIWORD(lParam);
                if (mx < 200) {
                    if (my >= 60 && my < 100) { g_active_tab = TAB_MEMORY; InvalidateRect(hwnd, NULL, FALSE); }
                    else if (my >= 100 && my < 140) { g_active_tab = TAB_SWAP; InvalidateRect(hwnd, NULL, FALSE); }
                    else if (my >= 140 && my < 180) { g_active_tab = TAB_SYSTEM; InvalidateRect(hwnd, NULL, FALSE); }
                    else if (my >= 180 && my < 220) { g_active_tab = TAB_LOGS; InvalidateRect(hwnd, NULL, FALSE); }
                    else if (my >= 220 && my < 260) { g_active_tab = TAB_CONSOLE; InvalidateRect(hwnd, NULL, FALSE); }
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
    
    g_use_gui_logs = 1;
    
    g_step_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_terminal_event = CreateEvent(NULL, FALSE, FALSE, NULL);
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
    if (g_terminal_event != NULL) {
        CloseHandle(g_terminal_event);
        g_terminal_event = NULL;
    }
    if (g_config_done_event != NULL) {
        CloseHandle(g_config_done_event);
        g_config_done_event = NULL;
    }
    g_hwnd = NULL;
}

char* guiWaitForInput(const char* prompt) {
    if (g_terminal_event == NULL) return NULL;
    
    // Switch active tab to Console dynamically
    g_active_tab = TAB_CONSOLE;
    emulatorLog("TERMINAL PROMPT: %s", prompt);
    updateGui();
    
    WaitForSingleObject(g_terminal_event, INFINITE);
    ResetEvent(g_terminal_event);
    
    char* copy = strdup(g_terminal_buffer);
    emulatorLog("USER INPUT: %s", copy);
    updateGui();
    
    return copy;
}
