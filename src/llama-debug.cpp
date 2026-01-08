#include "llama-debug.h"
#include "llama.h"
#include <cstring>
#include <iostream>

llama_debug_manager::llama_debug_manager()
    : active_granularity(LLAMA_DEBUG_NONE), enabled(false) {
}

llama_debug_manager::~llama_debug_manager() {
    clear_hooks();
}

bool llama_debug_manager::register_hook(const llama_debug_hook &hook) {
    if (!hook.name || !hook.callback) {
        return false;
    }

    std::lock_guard<std::mutex> lock(pause_mutex);
    hooks.push_back(hook);
    std::cout << "[DEBUG] Hook registered: " << hook.name << std::endl;
    return true;
}

bool llama_debug_manager::unregister_hook(const char *name) {
    std::lock_guard<std::mutex> lock(pause_mutex);
    auto it = std::remove_if(hooks.begin(), hooks.end(),
        [name](const llama_debug_hook &h) { return std::strcmp(h.name, name) == 0; });
    if (it != hooks.end()) {
        hooks.erase(it, hooks.end());
        return true;
    }
    return false;
}

void llama_debug_manager::clear_hooks() {
    std::lock_guard<std::mutex> lock(pause_mutex);
    hooks.clear();
}

void llama_debug_manager::enable(enum llama_debug_granularity granularity) {
    enabled = true;
    active_granularity = granularity;
    paused.store(true);  // 启用调试时自动暂停
    std::cout << "[DEBUG] Debug enabled with granularity=" << granularity << ", auto-paused" << std::endl;
}

void llama_debug_manager::disable() {
    enabled = false;
    active_granularity = LLAMA_DEBUG_NONE;
    resume();  // 确保不会暂停
    std::cout << "[DEBUG] Debug disabled" << std::endl;
}

void llama_debug_manager::pause() {
    paused.store(true);
    std::cout << "[DEBUG] Debug paused" << std::endl;
}

void llama_debug_manager::resume() {
    paused.store(false);
    should_step.store(false);
    pause_cv.notify_all();
    std::cout << "[DEBUG] Debug resumed" << std::endl;
}

void llama_debug_manager::step() {
    should_step.store(true);
    pause_cv.notify_all();
    std::cout << "[DEBUG] Debug step" << std::endl;
}

void llama_debug_manager::wait_if_paused() {
    if (paused.load()) {
        std::cout << "[DEBUG] Waiting at checkpoint..." << std::endl;
        std::unique_lock<std::mutex> lock(pause_mutex);
        pause_cv.wait(lock, [this] {
            return !paused.load() || should_step.load();
        });

        if (should_step.exchange(false)) {
            // 消费 step 信号
            std::cout << "[DEBUG] Step executed, continuing..." << std::endl;
            return;
        }
        std::cout << "[DEBUG] Resumed from wait" << std::endl;
    }
}

bool llama_debug_manager::dispatch_event(enum llama_debug_event_type event, void *event_data) {
    std::lock_guard<std::mutex> lock(pause_mutex);

    const char *event_name = (event == LLAMA_DEBUG_EVENT_TOKEN_START) ? "TOKEN_START" :
                             (event == LLAMA_DEBUG_EVENT_TOKEN_END) ? "TOKEN_END" :
                             "PAUSE_ACK";
    std::cout << "[DEBUG] Event dispatched: " << event_name << std::endl;

    for (const auto &hook : hooks) {
        if (hook.callback) {
            if (!hook.callback(event, event_data, hook.user_data)) {
                // 钩子返回 false 表示停止事件传播
                break;
            }
        }
    }
    return true;
}
