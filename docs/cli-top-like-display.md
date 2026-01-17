# llama-cli 顶部固定显示功能实现计划

## 1. 概述

本文档描述了为 `llama-cli` 添加类似 `top` 命令的顶部固定显示功能的实现计划。该功能将在终端顶部的固定区域（2行）显示实时指标，而主要内容在其下方正常滚动。

## 2. 需求分析

### 2.1 用户需求
- 在终端顶部显示固定的状态栏（2行）
- 实时显示以下指标：
  - Prompt: XXX.X t/s
  - Generation: XXX.X t/s
  - KV Cache 使用情况（当前 token 数 / 总 token 数或百分比）
- 状态栏下方的内容正常滚动
- 保持现有功能不变

### 2.2 状态栏布局

```
┌─────────────────────────────────────────────────────────────────┐
│ Prompt: 320.8 t/s | Gen: 22.8 t/s | KV: 2048/8192 (25%)         │  ← 第1行：状态信息
├─────────────────────────────────────────────────────────────────┤  ← 第2行：分隔线
│                                                                   │
│  （可滚动内容区域...）                                              │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### 2.3 技术需求
- 不破坏现有项目结构
- 遵循现有代码风格和约定
- 确保跨平台兼容性（Windows、macOS、Linux）
- 最小化性能影响

## 3. 现有代码分析

### 3.1 当前显示实现

**文件：`tools/cli/cli.cpp`**

当前的 timing 显示（第 370-375 行）：
```cpp
if (params.show_timings) {
    console::set_display(DISPLAY_TYPE_INFO);
    console::log("\n");
    console::log("[ Prompt: %.1f t/s | Generation: %.1f t/s ]\n",
                 timings.prompt_per_second, timings.predicted_per_second);
    console::set_display(DISPLAY_TYPE_RESET);
}
```

**关键发现：**
- Timing 信息在每次生成完成后显示
- 显示不是实时的（生成过程中）
- 使用 `console` 命名空间进行输出

### 3.2 Console 系统

**文件：`common/console.h` 和 `common/console.cpp`**

- 提供 `log()`、`error()`、`set_display()`、`flush()` 函数
- 支持不同显示类型的 ANSI 颜色代码
- 已有高级 readline 功能
- 已有光标移动工具（`move_cursor()`）
- 跨平台支持（Windows/POSIX）

**现有关键功能：**
```cpp
// 显示类型
enum display_type {
    DISPLAY_TYPE_RESET = 0,
    DISPLAY_TYPE_INFO,
    DISPLAY_TYPE_PROMPT,
    DISPLAY_TYPE_REASONING,
    DISPLAY_TYPE_USER_INPUT,
    DISPLAY_TYPE_ERROR
};

// 光标移动
static void move_cursor(int delta);

// 颜色支持（ANSI 代码）
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
// ... 等
```

### 3.3 Timing 数据结构

**文件：`tools/server/server-task.h`（第 191-209 行）**

```cpp
struct result_timings {
    int32_t cache_n = -1;

    int32_t prompt_n = -1;
    double prompt_ms;
    double prompt_per_token_ms;
    double prompt_per_second;

    int32_t predicted_n = -1;
    double predicted_ms;
    double predicted_per_token_ms;
    double predicted_per_second;

    // 可选的推测性指标
    int32_t draft_n = 0;
    int32_t draft_n_accepted = 0;
};
```

### 3.4 Context 和 Slot 信息

**文件：`tools/server/server-context.cpp`**

Server slot 结构包含：
```cpp
struct server_slot {
    int id;
    int32_t n_ctx = 0;      // 每个 slot 的上下文大小
    int32_t n_decoded = 0;  // 已解码的 token 数量
    int32_t n_prompt_tokens_processed = 0;
    // ... 更多字段
};
```

**关键 llama.h API：**
```cpp
LLAMA_API uint32_t llama_n_ctx(const struct llama_context * ctx);
LLAMA_API uint32_t llama_n_ctx_seq(const struct llama_context * ctx);
LLAMA_API void llama_memory_breakdown_print(const struct llama_context * ctx);
```

## 4. 设计方案

### 4.1 架构概览

```
+-------------------+
|  固定状态栏 (2行)  |  ← 新组件：common/statusbar.{h,cpp}
+-------------------+
|                   |
|  滚动内容区       |  ← 现有内容区域
|                   |
+-------------------+
```

### 4.2 状态栏布局详细设计

```
第1行：Prompt: 320.8 t/s | Gen: 22.8 t/s | KV: 2048/8192 (25%)
第2行：────────────────────────────────────────────────────────
```

**第1行（状态信息）包含：**
- Prompt t/s（处理速度）
- Generation t/s（生成速度）
- KV Cache 使用情况（已用/总数，百分比）

**第2行（分隔线）：**
- 使用 `─` 字符绘制水平分隔线
- 或使用 ANSI 双线字符 `═`

### 4.3 新组件

#### 4.3.1 状态栏模块

**新建文件：**
- `common/statusbar.h`
- `common/statusbar.cpp`

**职责：**
- 管理固定顶部2行显示区域
- 实时更新指标
- 处理光标定位的 ANSI 转义序列
- 维护内容滚动区域（从第3行开始）

**关键函数：**
```cpp
namespace statusbar {
    // 初始化状态栏（固定2行高度）
    void init();

    // 清理并重置终端
    void cleanup();

    // 更新指标显示
    void update(const metrics_data & data);

    // 写入可滚动内容区域（状态栏下方）
    void log(const char * fmt, ...);

    // 刷新显示
    void flush();
}
```

**指标数据结构：**
```cpp
struct metrics_data {
    double prompt_per_second = 0.0;
    double generation_per_second = 0.0;
    int32_t kv_cache_used = 0;
    int32_t kv_cache_total = 0;
};
```

#### 4.3.2 CLI 集成

**修改文件：`tools/cli/cli.cpp`**

**变更：**
1. 在 `common_params` 中添加 `--status-bar` 标志
2. 启用时初始化状态栏
3. 在生成循环中更新指标
4. 启用时使用 `statusbar::log()` 输出内容

**集成示例：**
```cpp
// 在 main() 中
if (params.status_bar) {
    statusbar::init();
}

// 生成过程中
if (params.status_bar) {
    metrics_data metrics;
    metrics.prompt_per_second = timings.prompt_per_second;
    metrics.generation_per_second = timings.predicted_per_second;
    metrics.kv_cache_used = get_kv_cache_used(ctx);
    metrics.kv_cache_total = llama_n_ctx(ctx);
    statusbar::update(metrics);
}
```

### 4.4 ANSI 转义序列

实现将使用以下 ANSI 序列：

| 序列 | 用途 |
|------|------|
| `\033[2J` | 清屏 |
| `\033[H` | 移动光标到起始位置 |
| `\033[3;r` | 设置从第3行开始的滚动区域 |
| `\033[3;1H` | 移动光标到第3行第1列 |
| `\033[K` | 清除当前行 |
| `\033[s` | 保存光标位置 |
| `\033[u` | 恢复光标位置 |

### 4.5 状态栏实现细节

```cpp
// 初始化
void statusbar::init() {
    // 清屏
    printf("\033[2J");

    // 设置滚动区域从第3行开始
    printf("\033[3;r");

    // 移动光标到第1行，绘制初始状态栏
    printf("\033[1;1H");
    printf("\033[K");  // 清除第1行
    printf("Initializing...");

    // 绘制分隔线
    printf("\033[2;1H");
    printf("\033[K");  // 清除第2行
    for (int i = 0; i < terminal_width; i++) {
        printf("─");
    }

    // 移动光标到内容区域（第3行）
    printf("\033[3;1H");
    fflush(stdout);
}

// 更新状态栏
void statusbar::update(const metrics_data & data) {
    // 保存当前光标位置
    printf("\033[s");

    // 移动到第1行
    printf("\033[1;1H");
    printf("\033[K");  // 清除第1行

    // 计算百分比
    float kv_percent = 0.0f;
    if (data.kv_cache_total > 0) {
        kv_percent = (float)data.kv_cache_used / data.kv_cache_total * 100.0f;
    }

    // 绘制状态信息
    printf("Prompt: %.1f t/s | Gen: %.1f t/s | KV: %d/%d (%.0f%%)",
           data.prompt_per_second,
           data.generation_per_second,
           data.kv_cache_used,
           data.kv_cache_total,
           kv_percent);

    // 恢复光标位置
    printf("\033[u");
    fflush(stdout);
}

// 写入内容
void statusbar::log(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}
```

## 5. 实现步骤

### 第一阶段：核心状态栏基础设施

1. **创建 `common/statusbar.h`**
   - 定义 `metrics_data` 结构
   - 声明状态栏函数
   - 添加配置常量

2. **创建 `common/statusbar.cpp`**
   - 实现终端初始化
   - 实现滚动区域设置（从第3行开始）
   - 实现状态栏绘制（2行）
   - 实现光标管理

3. **添加到构建系统**
   - 更新 `common/CMakeLists.txt`
   - 添加新的源文件

### 第二阶段：CLI 集成

4. **修改 `common/arg.h` 和 `common/arg.cpp`**
   - 在 `common_params` 中添加 `--status-bar` 参数

5. **修改 `tools/cli/cli.cpp`**
   - 启用时初始化状态栏
   - 在生成过程中收集指标
   - 实时更新状态栏
   - 适当路由内容输出

### 第三阶段：指标收集

6. **实现 KV cache 使用计算**
   - 添加辅助函数计算已用/总 KV cache
   - 集成 `server_context` 获取 slot 信息

7. **实时更新**
   - 在 token 生成期间更新指标
   - 处理时间计算

### 第四阶段：完善和测试

8. **边界情况处理**
   - 终端大小调整检测
   - 退出时优雅清理
   - 不支持终端的回退方案

9. **跨平台测试**
   - Windows（cmd、PowerShell）
   - Linux（各种终端）
   - macOS（Terminal、iTerm2）

## 6. KV Cache 使用计算

KV cache 使用情况可以从 `server_slot` 信息计算：

```cpp
// 辅助函数计算 KV cache 使用
struct kv_cache_info {
    int32_t used_tokens = 0;
    int32_t total_tokens = 0;
    float percentage = 0.0f;
};

kv_cache_info get_kv_cache_info(const server_context & ctx_server) {
    kv_cache_info info;

    // 获取总上下文大小
    auto * llama_ctx = ctx_server.get_llama_context();
    if (llama_ctx) {
        info.total_tokens = llama_n_ctx(llama_ctx);

        // 从活动 slot 计算已用 token
        // 这需要访问 slot 信息
        // slot 信息存储在 server_context_impl 内部

        // 对于 CLI 模式，我们可以从以下数据估算：
        // - 已处理的 prompt tokens
        // - 已生成的 tokens
    }

    return info;
}
```

## 7. 向后兼容性

- 状态栏通过 `--status-bar` 标志**可选启用**
- 默认行为保持不变
- 终端不支持所需功能时优雅降级
- `simple_io` 模式完全跳过状态栏

## 8. 文件结构摘要

```
llama.cpp/
├── common/
│   ├── statusbar.h      [新建]
│   ├── statusbar.cpp    [新建]
│   ├── console.h        [修改 - 如需添加辅助声明]
│   ├── arg.h            [修改 - 添加 --status-bar 参数]
│   └── arg.cpp          [修改]
├── tools/
│   └── cli/
│       └── cli.cpp      [修改 - 集成状态栏]
└── docs/
    └── cli-top-like-display.md  [新建 - 本文档]
```

## 9. 测试清单

- [ ] 启动时状态栏正确显示（2行）
- [ ] 第1行显示状态信息
- [ ] 第2行显示分隔线
- [ ] 生成过程中指标实时更新
- [ ] Prompt t/s 准确
- [ ] Generation t/s 准确
- [ ] KV cache 使用反映实际使用情况
- [ ] 内容从第3行开始正常滚动
- [ ] 处理终端大小调整
- [ ] 退出时恢复终端状态
- [ ] 与 `--no-color` 标志兼容
- [ ] 与 `--simple-io` 模式兼容（禁用）
- [ ] 验证跨平台兼容性

## 10. 未来增强

- 支持可配置的状态栏布局
- 支持自定义指标显示
- 颜色编码阈值（例如，高缓存使用率显示红色）
- 添加进度条（可选）
