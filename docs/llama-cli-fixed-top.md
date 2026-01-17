# llama-cli Fixed Top Display Mode

## 概述

llama-cli 支持类似 `top` 命令的固定顶端显示模式，可以在终端顶部实时展示模型性能指标和 KV cache 占用情况，同时保持正常输出在下方向上滚动。

## 功能特点

1. **实时性能指标**：每秒更新以下指标：
   - Prompt: 处理输入提示的速度 (t/s)
   - Generation: 生成响应的速度 (t/s)
   - KV Cache: 目前占用的键值缓存内存 (GB)

2. **固定顶端显示**：顶端显示区域保持固定，正常输出从第三行开始显示并向上滚动。

3. **兼容性**：与其他 llama-cli 选项兼容，保留所有原始功能。

## 使用方法

### 启用固定顶端显示

在启动 llama-cli 时添加 `--fixed-top` 选项：

```bash
llama-cli -m /path/to/your/model --fixed-top
```

### 与其他选项结合使用

```bash
# 同时启用颜色输出
llama-cli -m /path/to/your/model --fixed-top --color

# 同时显示详细时间
llama-cli -m /path/to/your/model --fixed-top --show-timings
```

## 技术实现

### 核心实现机制

1. **ANSI终端控制**：使用ANSI控制码实现终端光标控制和内容更新。

2. **实时更新机制**：在生成循环中定期更新顶端显示内容：
   - 每0.5秒更新一次性能指标
   - 使用非阻塞方式，不影响生成性能

3. **内存计算**：通过 `llama_memory_breakdown()` 函数计算 KV cache 占用。

### 文件修改说明

#### `common/console.h`
- 添加 `enable_fixed_top()` 接口
- 添加 `update_top_bar()` 接口
- 修改 `log()` 接口以支持固定顶端模式

#### `common/console.cpp`
- 实现固定顶端显示功能
- 集成ANSI终端控制代码

#### `tools/cli/cli.cpp`
- 添加 `--fixed-top` 选项支持
- 在 `generate_completion()` 中实现实时更新
- 在 `main()` 中初始化固定顶端模式

## 示例输出

```
Prompt: 320.8 t/s | Generation: 22.8 t/s | KV Cache: 0.45 GB

模型响应内容第一行
模型响应内容第二行
模型响应内容第三行
...
```

## 性能考虑

- **更新频率**：默认0.5秒更新一次，平衡显示效果和性能消耗
- **内存计算**：通过已有的 `llama_memory_breakdown()` 函数获取，额外开销极小
- **终端操作**：使用ANSI控制码，无需额外依赖库

## 兼容性

- **终端要求**：需要支持ANSI控制码的终端（如GNOME Terminal、iTerm2、Windows Terminal）
- **系统支持**：Linux、macOS、Windows（使用WSL或Windows Terminal）
- **与其他选项兼容性**：完全兼容 `--show-timings`, `--color`, `--simple-io` 等选项

## 开发和测试

### 编译测试

```bash
make clean
make llama-cli
./build/bin/llama-cli -m models/7B/ggml-model-f16.bin --fixed-top
```

### 单元测试

1. 测试基本功能是否正常显示
2. 测试实时更新是否正确
3. 测试与其他选项的兼容性
4. 测试在不同终端环境下的表现

## 已知限制

- **Windows CMD兼容性**：Windows命令提示符不支持完整的ANSI控制码，建议使用Windows Terminal或WSL
- **终端大小调整**：固定顶端显示可能无法正确响应终端大小变化，需要重启CLI应用

## 未来改进

1. 支持动态调整终端大小
2. 添加更多性能指标（如CPU占用、内存占用）
3. 允许配置更新频率
4. 添加颜色编码以更清晰地显示不同指标
