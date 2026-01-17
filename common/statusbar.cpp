#include "statusbar.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#   define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>
#endif

namespace statusbar {

// ANSI escape sequences
#define ANSI_CLEAR_SCREEN     "\033[2J"
#define ANSI_CLEAR_LINE       "\033[K"
#define ANSI_CURSOR_HOME      "\033[H"
#define ANSI_SAVE_CURSOR      "\033[s"
#define ANSI_RESTORE_CURSOR   "\033[u"
#define ANSI_SET_SCROLL_REGION(n) "\033[" #n ";r"
#define ANSI_CURSOR_POS(r, c) "\033[" #r ";" #c "H"

// Colors and styles
#define ANSI_COLOR_CYAN       "\x1b[36m"
#define ANSI_COLOR_BLUE       "\x1b[34m"
#define ANSI_COLOR_GREEN      "\x1b[32m"
#define ANSI_COLOR_YELLOW     "\x1b[33m"
#define ANSI_COLOR_BOLD       "\x1b[1m"
#define ANSI_COLOR_RESET      "\x1b[0m"

// State
static bool g_active = false;
static bool g_simple_io = true;
static int g_terminal_width = 80;
static bool g_use_color = true;

// Get terminal width
static int get_terminal_width() {
#if defined(_WIN32)
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
            return csbi.srWindow.Right - csbi.srWindow.Left + 1;
        }
    }
    return 80;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80;
#endif
}

// Draw the separator line
static void draw_separator() {
    if (!g_active) return;

    // Move to line 2, column 1
    fprintf(stdout, "\033[2;1H");
    fprintf(stdout, ANSI_CLEAR_LINE);

    // Set cyan color for separator
    if (g_use_color) {
        fprintf(stdout, ANSI_COLOR_CYAN);
    }

    // Draw separator line - use more compatible characters
    for (int i = 0; i < g_terminal_width; i++) {
        fprintf(stdout, "─");
    }

    // Reset color
    if (g_use_color) {
        fprintf(stdout, ANSI_COLOR_RESET);
    }

    fflush(stdout);
}

// Redraw both status line and separator
static void redraw_status_bar() {
    if (!g_active) return;

    // Move to line 1 and draw status
    fprintf(stdout, "\033[1;1H");
    fprintf(stdout, ANSI_CLEAR_LINE);
    if (g_use_color) {
        fprintf(stdout, ANSI_COLOR_BOLD ANSI_COLOR_BLUE);
    }
    fprintf(stdout, "Initializing...");
    if (g_use_color) {
        fprintf(stdout, ANSI_COLOR_RESET);
    }

    // Draw separator on line 2
    draw_separator();

    // Move cursor to content area (line 3)
    fprintf(stdout, "\033[3;1H");
    fflush(stdout);
}

// Initialize the status bar
bool init() {
    // Check if we should use simple I/O
    const char * simple_io_env = getenv("LLAMA_SIMPLE_IO");
    if (simple_io_env != nullptr && strcmp(simple_io_env, "1") == 0) {
        g_simple_io = true;
        return false;
    }

    // Check if status bar is force-enabled (for testing)
    const char * force_statusbar = getenv("LLAMA_FORCE_STATUSBAR");
    bool force_enabled = force_statusbar != nullptr && strcmp(force_statusbar, "1") == 0;

    // Check if stdout is a terminal
#if !defined(_WIN32)
    if (!isatty(STDOUT_FILENO) && !force_enabled) {
        g_simple_io = true;
        return false;
    }
#endif

    g_simple_io = false;
    g_terminal_width = get_terminal_width();

    // Check if color is disabled
    const char * no_color_env = getenv("LLAMA_NO_COLOR");
    const char * cli_color_env = getenv("LLAMA_CLI_COLOR");  // Check for CLI color setting
    if (no_color_env != nullptr || (cli_color_env != nullptr && strcmp(cli_color_env, "0") == 0)) {
        g_use_color = false;
    }

    // Clear screen first
    fprintf(stdout, ANSI_CLEAR_SCREEN);
    fprintf(stdout, ANSI_CURSOR_HOME);
    fflush(stdout);

    // Mark as active BEFORE drawing
    g_active = true;

    // Draw initial status bar (lines 1-2)
    redraw_status_bar();

    // NOW set scroll region from line 3 onwards (after status bar is drawn)
    fprintf(stdout, "\033[3;r");

    // Ensure cursor is in content area
    fprintf(stdout, "\033[3;1H");
    fflush(stdout);

    return true;
}

// Cleanup and restore terminal state
void cleanup() {
    if (!g_active) return;

    // Restore normal scrolling
    fprintf(stdout, "\033[r");

    // Move to end of content
    fprintf(stdout, ANSI_CURSOR_HOME);
    fprintf(stdout, ANSI_CLEAR_LINE);

    g_active = false;
    fflush(stdout);
}

// Update the status bar with new metrics
void update(const metrics_data & data) {
    if (!g_active) return;

    // Save current cursor position
    fprintf(stdout, ANSI_SAVE_CURSOR);

    // Move to line 1 and update status
    fprintf(stdout, "\033[1;1H");
    fprintf(stdout, ANSI_CLEAR_LINE);

    // Set bold and color for the metrics line
    if (g_use_color) {
        fprintf(stdout, ANSI_COLOR_BOLD ANSI_COLOR_BLUE);
    }

    // Calculate KV cache percentage
    float kv_percent = 0.0f;
    if (data.kv_cache_total > 0) {
        kv_percent = (float)data.kv_cache_used / data.kv_cache_total * 100.0f;
    }

    // Format and display metrics
    // Format: "Prompt: 320.8 t/s | Gen: 22.8 t/s | KV: 2048/8192 (25%)"
    fprintf(stdout, "Prompt: %.1f t/s | Gen: %.1f t/s | KV: %d/%d (%.0f%%)",
            data.prompt_per_second,
            data.generation_per_second,
            data.kv_cache_used,
            data.kv_cache_total,
            kv_percent);

    // Reset color
    if (g_use_color) {
        fprintf(stdout, ANSI_COLOR_RESET);
    }

    // Ensure separator line is still visible on line 2
    fprintf(stdout, "\033[2;1H");
    if (g_use_color) {
        fprintf(stdout, ANSI_COLOR_CYAN);
    }
    for (int i = 0; i < g_terminal_width; i++) {
        fprintf(stdout, "─");
    }
    if (g_use_color) {
        fprintf(stdout, ANSI_COLOR_RESET);
    }

    // Restore cursor position
    fprintf(stdout, ANSI_RESTORE_CURSOR);
    fflush(stdout);
}

// Write to the scrollable content area
void log(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
}

// Write error message to the scrollable content area
void error(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

// Flush output
void flush() {
    fflush(stdout);
}

// Check if status bar is active
bool is_active() {
    return g_active;
}

} // namespace statusbar
