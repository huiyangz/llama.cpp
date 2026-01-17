// Test program for statusbar layout

#include "statusbar.h"
#include <thread>
#include <chrono>
#include <cmath>

int main() {
    // Initialize status bar
    if (!statusbar::init()) {
        printf("Failed to initialize status bar (terminal not supported)\n");
        return 1;
    }

    // Simulate some activity
    double prompt_tps = 0.0;
    double gen_tps = 0.0;
    int32_t kv_used = 0;
    const int32_t kv_total = 8192;

    statusbar::log("\n");
    statusbar::log("Status Bar Layout Test\n");
    statusbar::log("=======================\n");
    statusbar::log("\n");
    statusbar::flush();

    // Warm up phase
    statusbar::log("Warming up...\n");
    for (int i = 0; i < 20; i++) {
        prompt_tps = 100.0 + i * 10.0;
        kv_used = 512 + i * 50;

        statusbar::metrics_data data;
        data.prompt_per_second = prompt_tps;
        data.generation_per_second = 0.0;
        data.kv_cache_used = kv_used;
        data.kv_cache_total = kv_total;
        statusbar::update(data);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    statusbar::log("\nGeneration phase:\n");
    statusbar::log("------------------\n");

    // Generation phase
    for (int i = 0; i < 50; i++) {
        // Simulate changing metrics
        prompt_tps = 300.0 + sin(i * 0.2) * 50.0;
        gen_tps = 20.0 + sin(i * 0.3) * 5.0;
        kv_used = 2048 + i * 30;

        statusbar::metrics_data data;
        data.prompt_per_second = prompt_tps;
        data.generation_per_second = gen_tps;
        data.kv_cache_used = kv_used;
        data.kv_cache_total = kv_total;
        statusbar::update(data);

        // Output some content
        if (i % 5 == 0) {
            statusbar::log("Generating token %d...\n", i);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    statusbar::log("\nTest complete!\n");
    statusbar::log("Notice how the content above scrolls while the status bar stays fixed.\n");
    statusbar::log("\nPress Ctrl+C to exit...\n");

    // Keep running to show the static state
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup (unreachable in this test)
    statusbar::cleanup();

    return 0;
}
