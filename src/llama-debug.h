#ifndef LLAMA_DEBUG_H
#define LLAMA_DEBUG_H

#include "ggml.h"
#include "llama.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

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
  LLAMA_DEBUG_EVENT_LAYER_START,
  LLAMA_DEBUG_EVENT_NODE_START,
  LLAMA_DEBUG_EVENT_PAUSE_ACK,
};

// Node 事件数据
struct llama_debug_node_event {
  const char *name;
  int layer_idx;
  struct ggml_tensor *tensor;
};

// 发现的模型图节点信息
struct llama_debug_node_info {
  std::string name;
  std::string op;
  std::vector<std::string> inputs;
};

typedef bool (*llama_debug_callback)(enum llama_debug_event_type,
                                     void *event_data, void *user_data);

struct llama_debug_hook {
  char name[64];
  llama_debug_callback callback;
  void *user_data;
};

struct llama_debug_token_event {
  const llama_token *token_ids;
  int32_t n_tokens;
  const llama_pos *positions;
  const char *phase;
  const float *logits;
  int32_t n_vocab;
};

// 调试管理器主类
class llama_debug_manager {
private:
  std::vector<llama_debug_hook> hooks;
  enum llama_debug_granularity active_granularity;
  bool enabled;

  // 当前状态
  int32_t cur_layer = -1;
  int32_t last_paused_layer = -1;
  char cur_node[256] = "";
  char cur_op[64] = "";
  char cur_inputs[512] = ""; // 逗号分隔的输入节点名称
  bool token_cycle_ended = false;

  // 模型图 Schema 发现
  std::vector<llama_debug_node_info> layer_schema;
  bool is_capturing_schema = false;
  bool has_schema = false;

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
  enum llama_debug_granularity get_granularity() const {
    return active_granularity;
  }
  void set_granularity(enum llama_debug_granularity g) {
    active_granularity = g;
  }

  // 状态更新
  void set_current_layer(int32_t il) { cur_layer = il; }
  int32_t get_current_layer() const { return cur_layer; }
  void set_current_node(const char *name);
  const char *get_current_node() const { return cur_node; }
  void set_current_op(const char *op);
  const char *get_current_op() const { return cur_op; }
  void set_current_inputs(const char *inputs);
  const char *get_current_inputs() const { return cur_inputs; }

  // 暂停/恢复/单步
  void pause();
  void resume();
  void step();
  bool is_paused() const { return paused.load(); }
  void wait_if_checkpoint(enum llama_debug_granularity checkpoint_type);

  // 事件分发
  bool dispatch_event(enum llama_debug_event_type event, void *event_data);

  // Schema 发现
  void start_capturing_schema();
  void stop_capturing_schema();
  bool is_schema_captured() const { return has_schema; }
  const std::vector<llama_debug_node_info> &get_layer_schema() const {
    return layer_schema;
  }
};

#endif // LLAMA_DEBUG_H
