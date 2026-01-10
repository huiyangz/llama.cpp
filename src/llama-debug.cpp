#include "llama-debug.h"
#include "llama.h"
#include <cstring>
#include <iostream>

llama_debug_manager::llama_debug_manager()
    : active_granularity(LLAMA_DEBUG_NONE), enabled(false) {}

llama_debug_manager::~llama_debug_manager() { clear_hooks(); }

bool llama_debug_manager::register_hook(const llama_debug_hook &hook) {
  if (hook.name[0] == '\0' || !hook.callback) {
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
                           [name](const llama_debug_hook &h) {
                             return std::strcmp(h.name, name) == 0;
                           });
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
  paused.store(true); // 启用调试时自动暂停
  std::cout << "[DEBUG] Debug enabled with granularity=" << granularity
            << ", auto-paused" << std::endl;
}

void llama_debug_manager::disable() {
  enabled = false;
  active_granularity = LLAMA_DEBUG_NONE;
  resume(); // 确保不会暂停
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

void llama_debug_manager::set_current_node(const char *name) {
  if (name) {
    std::strncpy(cur_node, name, sizeof(cur_node) - 1);
    cur_node[sizeof(cur_node) - 1] = '\0';

    // 如果正在捕获 Schema，且该节点尚未记录
    if (is_capturing_schema) {
      bool found = false;
      for (const auto &node : layer_schema) {
        if (node.name == name) {
          found = true;
          break;
        }
      }
      if (!found) {
        llama_debug_node_info info;
        info.name = name;
        info.op = cur_op;

        // 解析当前 inputs
        if (std::strlen(cur_inputs) > 0) {
          std::string inputs_str = cur_inputs;
          std::size_t pos = 0;
          while ((pos = inputs_str.find(',')) != std::string::npos) {
            info.inputs.push_back(inputs_str.substr(0, pos));
            inputs_str.erase(0, pos + 1);
          }
          if (!inputs_str.empty()) {
            info.inputs.push_back(inputs_str);
          }
        }
        layer_schema.push_back(info);
      }
    }
  } else {
    cur_node[0] = '\0';
  }
}

void llama_debug_manager::set_current_op(const char *op) {
  if (op) {
    std::strncpy(cur_op, op, sizeof(cur_op) - 1);
    cur_op[sizeof(cur_op) - 1] = '\0';
  } else {
    cur_op[0] = '\0';
  }
}

void llama_debug_manager::set_current_inputs(const char *inputs) {
  if (inputs) {
    std::strncpy(cur_inputs, inputs, sizeof(cur_inputs) - 1);
    cur_inputs[sizeof(cur_inputs) - 1] = '\0';
  } else {
    cur_inputs[0] = '\0';
  }
}

void llama_debug_manager::wait_if_checkpoint(
    enum llama_debug_granularity checkpoint_type) {
  if (!enabled)
    return;

  bool should_wait = false;

  // 1. 如果当前是“单步”模式，任何检查点都停
  if (active_granularity & LLAMA_DEBUG_OPERATION) {
    should_wait = true;
  }
  // 2. 如果是“逐层”模式，仅在层检查点且层号变化时停
  else if ((active_granularity & LLAMA_DEBUG_LAYER) &&
           (checkpoint_type == LLAMA_DEBUG_LAYER)) {
    if (cur_layer != last_paused_layer) {
      should_wait = true;
    }
  }
  // 3. 如果是“逐 Token”模式，仅在 Token 结束时停
  else if ((active_granularity & LLAMA_DEBUG_TOKEN) &&
           (checkpoint_type == LLAMA_DEBUG_TOKEN)) {
    should_wait = true;
  }

  if (should_wait || paused.load()) {
    std::cout << "[DEBUG] Paused at checkpoint: type=" << checkpoint_type
              << " layer=" << cur_layer << " node=" << cur_node << std::endl;

    last_paused_layer = cur_layer;
    paused.store(true); // 进入暂停状态

    std::unique_lock<std::mutex> lock(pause_mutex);
    pause_cv.wait(lock,
                  [this] { return !paused.load() || should_step.load(); });

    if (should_step.exchange(false)) {
      // 如果是单步触发，通常我们保持 paused=true，下次 checkpoint 还会进这里
      // 但如果用户点的是“继续”，resume() 会把 paused 设为 false
      std::cout << "[DEBUG] Step triggered, moving to next checkpoint..."
                << std::endl;
    } else {
      std::cout << "[DEBUG] Resumed from wait" << std::endl;
    }
  }
}

bool llama_debug_manager::dispatch_event(enum llama_debug_event_type event,
                                         void *event_data) {
  std::lock_guard<std::mutex> lock(pause_mutex);

  const char *event_name =
      (event == LLAMA_DEBUG_EVENT_TOKEN_START) ? "TOKEN_START"
      : (event == LLAMA_DEBUG_EVENT_TOKEN_END) ? "TOKEN_END"
                                               : "PAUSE_ACK";
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

void llama_debug_manager::start_capturing_schema() {
  layer_schema.clear();
  is_capturing_schema = true;
  has_schema = false;
}

void llama_debug_manager::stop_capturing_schema() {
  is_capturing_schema = false;
  if (!layer_schema.empty()) {
    has_schema = true;
  }
}
