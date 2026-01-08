#ifndef LLAMA_DEBUG_H
#define LLAMA_DEBUG_H

#include "llama.h"
#include "ggml.h"
#include <cstdint>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>

// 调试粒度级别
enum llama_debug_granularity : uint32_t {
    LLAMA_DEBUG_NONE = 0,
    LLAMA_DEBUG_TOKEN = 1 << 0,
    LLAMA_DEBUG_LAYER = 1 << 1,
    LLAMA_DEBUG_OPERATION = 1 << 2,
};

// 调试事件类型
enum llama_debug_event_type : uint32_t {
    LLAMA_DEBUG_EVENT_TOKEN_START,
    LLAMA_DEBUG_EVENT_TOKEN_END,
    LLAMA_DEBUG_EVENT_PAUSE_ACK,
};

// Token 事件数据
struct llama_debug_token_event {
    const llama_token *token_ids;  // Token ID 数组
    int32_t n_tokens;              // Token 数量
    const llama_pos *positions;    // 位置数组
    const char *phase;             // "decode_start" 或 "decode_end"
    const float *logits;           // Logits（仅 decode_end 时有效）
    int32_t n_vocab;               // 词汇表大小
};

// 钩子回调类型
typedef bool (*llama_debug_hook_fn)(
    enum llama_debug_event_type event,
    void *event_data,
    void *user_data
);

// 钩子注册结构
struct llama_debug_hook {
    const char *name;
    llama_debug_hook_fn callback;
    void *user_data;
};

// 调试管理器主类
class llama_debug_manager {
private:
    std::vector<llama_debug_hook> hooks;
    enum llama_debug_granularity active_granularity;
    bool enabled;

    // 暂停/恢复状态
    std::atomic<bool> paused{false};
    std::atomic<bool> should_step{false};
    std::mutex pause_mutex;
    std::condition_variable pause_cv;

public:
    llama_debug_manager();
    ~llama_debug_manager();

    // 钩子管理
    bool register_hook(const llama_debug_hook &hook);
    bool unregister_hook(const char *name);
    void clear_hooks();

    // 控制接口
    void enable(enum llama_debug_granularity granularity);
    void disable();
    bool is_enabled() const { return enabled; }
    enum llama_debug_granularity get_granularity() const { return active_granularity; }

    // 暂停/恢复/单步
    void pause();
    void resume();
    void step();
    bool is_paused() const { return paused.load(); }
    void wait_if_paused();

    // 事件分发
    bool dispatch_event(enum llama_debug_event_type event, void *event_data);
};

#endif // LLAMA_DEBUG_H
