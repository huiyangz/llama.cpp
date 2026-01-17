// Status bar for terminal UI

#pragma once

#include "common.h"

#include <cstdint>
#include <string>

namespace statusbar {

// Metrics data structure for status bar display
struct metrics_data {
    double prompt_per_second = 0.0;
    double generation_per_second = 0.0;
    size_t kv_cache_used = 0;
    size_t kv_cache_total = 0;
};

// Initialize the status bar
// Sets up a 2-line fixed header with scrolling content below
// Returns true on success, false if terminal doesn't support required features
bool init();

// Cleanup and restore terminal state
void cleanup();

// Update the status bar with new metrics
void update(const metrics_data & data);

// Write to the scrollable content area (below the status bar)
// This replaces console::log() when status bar is active
void log(const char * fmt, ...);

// Write error message to the scrollable content area
void error(const char * fmt, ...);

// Flush output
void flush();

// Check if status bar is active
bool is_active();

} // namespace statusbar
