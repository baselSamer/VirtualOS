#include "Gui.h"
#include "../Memory/Memory.h"
#include "../../emu/Mem/Mem.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

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

/* Button IDs */
#define ID_TAB_MEM    1001
#define ID_TAB_SWAP   1002
#define ID_TAB_SYS    1003
#define ID_BTN_STEP   1004

/* Tab indices */
#define TAB_MEMORY  0
#define TAB_SWAP    1
#define TAB_SYSTEM  2

/* PID colors */
static COLORREF pid_colors[] = {
    RGB(200, 200, 200),  /* 0: Empty */
    RGB(100, 149, 237),  /* PID 1: Cornflower Blue */
    RGB(60, 179, 113),   /* PID 2: Sea Green */
    RGB(205, 92, 92),    /* PID 3: Indian Red */
    RGB(218, 165, 32),   /* PID 4: Goldenrod */
    RGB(147, 112, 219),  /* PID 5: Purple */
    RGB(0, 206, 209),    /* PID 6: Turquoise */
    RGB(255, 140, 0),    /* PID 7: Orange */
    RGB(119, 136, 153),  /* PID 8: Slate */
    RGB(222, 184, 135),  /* PID 9: Tan */
    RGB(144, 238, 144),  /* PID 10: Light Green */
};
#define NUM_PID_COLORS 11

/* ========== GLOBAL STATE ========== */
static Emulator *g_emu = NULL;
static kernal_state *g_state = NULL;
static HWND g_hwnd = NULL;
static HANDLE g_step_event = NULL;
static HANDLE g_gui_thread = NULL;
static int g_active_tab = TAB_MEMORY;
static int g_hovered_cell = -1;
static volatile int g_running = 1;
static HFONT g_font_normal = NULL;
static HFONT g_font_bold = NULL;
static HFONT g_font_small = NULL;

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
        case SCHED_RR:   return "Round Robin";
        case SCHED_HRRN: return "HRRN";
        case SCHED_MLFQ: return "MLFQ";
        default:         return "Unknown";
    }
}

/* Build a map: slot_index -> owning PID (0 = empty) */
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

/* Draw a queue's contents as colored boxes */
static void drawQueue(HDC hdc, int x, int y, const char *label, Queue *q, int *slot_pids) {
    char buf[256];
    SetTextColor(hdc, RGB(30, 30, 30));
    SelectObject(hdc, g_font_bold);
    TextOut(hdc, x, y, label, (int)strlen(label));
    
    SelectObject(hdc, g_font_normal);
    if (q->count == 0) {
        TextOut(hdc, x + 10, y + 22, "(empty)", 7);
        return;
    }
    
    int bx = x + 10;
    for (int i = 0; i < q->count; i++) {
        int idx = (q->front + i) % MAX_SIZE;
        int pid = q->process_ids[idx];
        
        HBRUSH brush = CreateSolidBrush(getColorForPid(pid));
        RECT r = { bx, y + 20, bx + 45, y + 42 };
        FillRect(hdc, &r, brush);
        DeleteObject(brush);
        
        /* Draw border */
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
        HPEN oldPen = SelectObject(hdc, pen);
        HBRUSH oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, bx, y + 20, bx + 45, y + 42);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(pen);
        
        sprintf(buf, "P%d", pid);
        SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, bx + 12, y + 22, buf, (int)strlen(buf));
        
        bx += 50;
    }
}

/* ========== TAB: MEMORY ========== */
static void drawMemoryTab(HDC hdc, RECT *client) {
    int slot_pids[40];
    computeSlotOwnership(slot_pids);
    
    char buf[128];
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, RGB(30, 30, 30));
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
        
        /* Fill cell */
        HBRUSH brush = CreateSolidBrush(color);
        RECT r = { x, y, x + CELL_W - 2, y + CELL_H - 2 };
        FillRect(hdc, &r, brush);
        DeleteObject(brush);
        
        /* Highlight hovered cell */
        if (i == g_hovered_cell) {
            HPEN pen = CreatePen(PS_SOLID, 3, RGB(255, 50, 50));
            HPEN oldPen = SelectObject(hdc, pen);
            HBRUSH oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, x, y, x + CELL_W - 2, y + CELL_H - 2);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(pen);
        } else {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
            HPEN oldPen = SelectObject(hdc, pen);
            HBRUSH oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, x, y, x + CELL_W - 2, y + CELL_H - 2);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(pen);
        }
        
        SetBkMode(hdc, TRANSPARENT);
        
        /* Slot index top-left */
        SelectObject(hdc, g_font_small);
        SetTextColor(hdc, RGB(50, 50, 50));
        sprintf(buf, "%d", i);
        TextOut(hdc, x + 4, y + 2, buf, (int)strlen(buf));
        
        /* Type + PID center */
        SelectObject(hdc, g_font_normal);
        if (word != NULL) {
            SetTextColor(hdc, RGB(20, 20, 20));
            sprintf(buf, "%s", memTypeToStr(word->type));
            TextOut(hdc, x + 30, y + 18, buf, (int)strlen(buf));
            if (pid > 0) {
                sprintf(buf, "P%d", pid);
                TextOut(hdc, x + 35, y + 36, buf, (int)strlen(buf));
            }
        } else {
            SetTextColor(hdc, RGB(130, 130, 130));
            TextOut(hdc, x + 25, y + 22, "FREE", 4);
        }
    }
    
    /* Legend */
    int ly = GRID_Y + GRID_ROWS * CELL_H + 15;
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, RGB(30, 30, 30));
    TextOut(hdc, GRID_X, ly, "Legend:", 7);
    
    SelectObject(hdc, g_font_normal);
    int lx = GRID_X + 70;
    
    /* Empty */
    HBRUSH br = CreateSolidBrush(pid_colors[0]);
    RECT lr = { lx, ly, lx + 20, ly + 16 };
    FillRect(hdc, &lr, br);
    DeleteObject(br);
    TextOut(hdc, lx + 25, ly, "Empty", 5);
    lx += 90;
    
    /* Show colors for PIDs that exist */
    for (int p = 1; p < NUM_PID_COLORS; p++) {
        int found = 0;
        for (int s = 0; s < 40; s++) {
            if (slot_pids[s] == p) { found = 1; break; }
        }
        if (!found) continue;
        
        br = CreateSolidBrush(pid_colors[p]);
        lr.left = lx; lr.top = ly; lr.right = lx + 20; lr.bottom = ly + 16;
        FillRect(hdc, &lr, br);
        DeleteObject(br);
        sprintf(buf, "PID %d", p);
        TextOut(hdc, lx + 25, ly, buf, (int)strlen(buf));
        lx += 90;
        if (lx > 750) { lx = GRID_X + 70; ly += 22; }
    }
}

/* ========== TAB: SWAP ========== */
static void drawSwapTab(HDC hdc, RECT *client) {
    char buf[256];
    int y = GRID_Y;
    
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, RGB(30, 30, 30));
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
            
            /* Draw colored box */
            HBRUSH brush = CreateSolidBrush(getColorForPid(p));
            RECT r = { GRID_X, y, GRID_X + 600, y + 40 };
            FillRect(hdc, &r, brush);
            DeleteObject(brush);
            
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
            HPEN oldPen = SelectObject(hdc, pen);
            HBRUSH oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, GRID_X, y, GRID_X + 600, y + 40);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(pen);
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(20, 20, 20));
            sprintf(buf, "  PID %d  |  File: %s  |  Size: %ld bytes", p, filename, size);
            TextOut(hdc, GRID_X + 10, y + 10, buf, (int)strlen(buf));
            
            y += 50;
        }
    }
    
    if (!found_any) {
        SetTextColor(hdc, RGB(100, 100, 100));
        TextOut(hdc, GRID_X + 10, y + 10, "No processes currently swapped to disk.", 39);
    }
}

/* ========== TAB: SYSTEM STATUS ========== */
static void drawSystemTab(HDC hdc, RECT *client) {
    char buf[256];
    int slot_pids[40];
    computeSlotOwnership(slot_pids);
    int y = GRID_Y - 10;
    int x = GRID_X;
    
    /* --- Active PCB --- */
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, RGB(30, 30, 30));
    TextOut(hdc, x, y, "Active Process", 14);
    y += 22;
    
    SelectObject(hdc, g_font_normal);
    PCB *active = getActivePCB(g_emu);
    if (active != NULL) {
        HBRUSH br = CreateSolidBrush(getColorForPid(active->ProcessID));
        RECT r = { x, y, x + 420, y + 75 };
        FillRect(hdc, &r, br);
        DeleteObject(br);
        
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
        HPEN oldPen = SelectObject(hdc, pen);
        HBRUSH oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, x, y, x + 420, y + 75);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(pen);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(20, 20, 20));
        sprintf(buf, "  PID: %d   State: %s   PC: %d", active->ProcessID, stateToStr(active->state), active->pc);
        TextOut(hdc, x + 5, y + 8, buf, (int)strlen(buf));
        sprintf(buf, "  Bounds: [%d, %d, %d, %d]", active->bounds[0], active->bounds[1], active->bounds[2], active->bounds[3]);
        TextOut(hdc, x + 5, y + 28, buf, (int)strlen(buf));
        
        /* Show algo-specific counter */
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
        SetTextColor(hdc, RGB(100, 100, 100));
        TextOut(hdc, x + 10, y + 5, "No active process (CPU IDLE)", 28);
    }
    y += 90;
    
    /* --- Scheduler Info --- */
    SelectObject(hdc, g_font_bold);
    SetTextColor(hdc, RGB(30, 30, 30));
    sprintf(buf, "Scheduler: %s", algoToStr(g_state->current_algo));
    TextOut(hdc, x, y, buf, (int)strlen(buf));
    if (g_state->current_algo == SCHED_RR) {
        sprintf(buf, "   (Quantum = %d)", g_state->time_quantum);
        TextOut(hdc, x + 200, y, buf, (int)strlen(buf));
    }
    y += 28;
    
    /* --- Queues --- */
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
    
    /* --- HRRN Ratios --- */
    if (g_state->current_algo == SCHED_HRRN && g_state->ready_queue.count > 0) {
        SelectObject(hdc, g_font_bold);
        SetTextColor(hdc, RGB(30, 30, 30));
        TextOut(hdc, x, y, "HRRN Response Ratios:", 21);
        y += 22;
        
        /* Table header */
        SelectObject(hdc, g_font_normal);
        SetTextColor(hdc, RGB(60, 60, 60));
        sprintf(buf, "  %-8s %-10s %-10s %-10s %-10s", "PID", "Arrival", "Wait", "Burst", "Ratio");
        TextOut(hdc, x, y, buf, (int)strlen(buf));
        y += 18;
        
        /* Draw a line */
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
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
                
                SetTextColor(hdc, RGB(20, 20, 20));
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
    
    /* Background */
    HBRUSH bg = CreateSolidBrush(RGB(230, 230, 230));
    RECT r = { 0, y, client->right, client->bottom };
    FillRect(hdc, &r, bg);
    DeleteObject(bg);
    
    /* Separator line */
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    HPEN oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, 0, y, NULL);
    LineTo(hdc, client->right, y);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_font_normal);
    
    char buf[512];
    
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
        SetTextColor(hdc, RGB(30, 30, 30));
        TextOut(hdc, 10, y + 5, buf, (int)strlen(buf));
    } else {
        SetTextColor(hdc, RGB(100, 100, 100));
        if (g_active_tab == TAB_MEMORY) {
            TextOut(hdc, 10, y + 5, "Hover over a memory cell to see details", 39);
        }
    }
    
    /* Tick counter on right */
    sprintf(buf, "Tick: %d", g_state->current_tick_count);
    SetTextColor(hdc, RGB(30, 30, 30));
    SelectObject(hdc, g_font_bold);
    TextOut(hdc, client->right - 100, y + 5, buf, (int)strlen(buf));
}

/* ========== WM_PAINT MAIN ========== */
static void paintWindow(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    
    RECT client;
    GetClientRect(hwnd, &client);
    
    /* Double buffer */
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HBITMAP oldBmp = SelectObject(memDC, memBmp);
    
    /* Clear background */
    HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 245));
    FillRect(memDC, &client, bgBrush);
    DeleteObject(bgBrush);
    
    /* Header bar */
    HBRUSH hdrBrush = CreateSolidBrush(RGB(55, 71, 79));
    RECT hdrRect = { 0, 0, client.right, 45 };
    FillRect(memDC, &hdrRect, hdrBrush);
    DeleteObject(hdrBrush);
    
    /* Title */
    SetBkMode(memDC, TRANSPARENT);
    SelectObject(memDC, g_font_bold);
    SetTextColor(memDC, RGB(255, 255, 255));
    TextOut(memDC, 15, 12, "Project Ethos - System Monitor", 30);
    
    /* Tick in header */
    char tickBuf[64];
    sprintf(tickBuf, "TICK: %d", g_state->current_tick_count);
    TextOut(memDC, client.right - 130, 12, tickBuf, (int)strlen(tickBuf));
    
    /* Draw active tab content */
    if (g_emu != NULL && g_state != NULL) {
        switch (g_active_tab) {
            case TAB_MEMORY:  drawMemoryTab(memDC, &client);  break;
            case TAB_SWAP:    drawSwapTab(memDC, &client);    break;
            case TAB_SYSTEM:  drawSystemTab(memDC, &client);  break;
        }
        drawStatusBar(memDC, &client);
    }
    
    /* Blit */
    BitBlt(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, SRCCOPY);
    
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
    
    EndPaint(hwnd, &ps);
}

/* ========== WINDOW PROCEDURE ========== */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            /* Tab buttons */
            CreateWindow("BUTTON", "Memory", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         480, 8, 80, 28, hwnd, (HMENU)ID_TAB_MEM, NULL, NULL);
            CreateWindow("BUTTON", "Swap", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         565, 8, 80, 28, hwnd, (HMENU)ID_TAB_SWAP, NULL, NULL);
            CreateWindow("BUTTON", "System", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         650, 8, 80, 28, hwnd, (HMENU)ID_TAB_SYS, NULL, NULL);
            /* Step button */
            CreateWindow("BUTTON", "Step >>", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         745, 8, 70, 28, hwnd, (HMENU)ID_BTN_STEP, NULL, NULL);
            return 0;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_TAB_MEM:
                    g_active_tab = TAB_MEMORY;
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case ID_TAB_SWAP:
                    g_active_tab = TAB_SWAP;
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case ID_TAB_SYS:
                    g_active_tab = TAB_SYSTEM;
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case ID_BTN_STEP:
                    SetEvent(g_step_event);
                    break;
            }
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            if (g_active_tab == TAB_MEMORY) {
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
    
    /* Register window class */
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
    
    /* Create fonts */
    g_font_normal = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Consolas");
    g_font_bold = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Consolas");
    g_font_small = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Consolas");
    
    /* Create window */
    g_hwnd = CreateWindowEx(
        0, "EthosGuiClass", "Project Ethos - System Monitor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H,
        NULL, NULL, GetModuleHandle(NULL), NULL);
    
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    
    /* Message loop */
    MSG msg;
    while (g_running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    /* Cleanup fonts */
    if (g_font_normal) DeleteObject(g_font_normal);
    if (g_font_bold)   DeleteObject(g_font_bold);
    if (g_font_small)  DeleteObject(g_font_small);
    
    return 0;
}

/* ========== PUBLIC API ========== */

void startGui(Emulator *emu, kernal_state *state) {
    g_emu = emu;
    g_state = state;
    g_running = 1;
    g_active_tab = TAB_MEMORY;
    g_hovered_cell = -1;
    
    /* Create step event (auto-reset) */
    g_step_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    
    /* Launch GUI thread */
    g_gui_thread = CreateThread(NULL, 0, guiThreadFunc, NULL, 0, NULL);
    
    /* Give the window time to appear */
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
        /* Update the GUI before waiting */
        updateGui();
        /* Block until Step button is clicked */
        WaitForSingleObject(g_step_event, INFINITE);
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
    g_hwnd = NULL;
}
