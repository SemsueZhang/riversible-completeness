# 可逆逻辑规则计数实验仓库

C++17 实现四个方法，提供“归约到范式”和“判断两个电路相等”两种实验入口。
实验指标只有 **基本规则总次数 `rule_count`** 和 **运行时间 `time_ms`**；不输出逐步推导、证书、分规则统计或其他性能指标。

**当前状态（2026-10-25）：测试集已缩减为 10 个公开电路，3 线路 4 个、4～6 线路各 2 个。FL/FXL 各运行 3 次，60 次全部完成；逐电路规则数与平均时间见[最新报告](results/fl-fxl-selected-20261025/report.md)。**

**历史状态（2026-10-22）：已优化并重测 FL2025、FXL2026 的二值范式归约，见[当时报告](results/fl-fxl-optimized-20261022/report.md)。257 个验证电路通过真值表及范式端点检查；这不等于完整规则证书回放。相等判定、NF/DC 性能测试及数据生成器尚未运行。**

| 标识 | 方法 | 输入 | 主要实现 |
|---|---|---|---|
| `fl2025` | Feng–Li 原文 Theorem 2 路径化与 Proposition 3 范式归约 | 仅二值 | `src/fl2025.cpp` |
| `fxl2026` | Feng–Xiong–Li，Section III / Algorithms 1–4 | 仅二值 | `src/fxl2026.cpp` |
| `nf` | 本文第三节：控制归一化、递归路径化、块消去 | 二值及多值 | `src/nf.cpp` |
| `dc` | 本文第四节：网格运输、递增链直接消去 | 二值及多值 | `src/dc.cpp` |

共用的基本规则展开位于 `src/compiler.cpp`，DC 网格运输位于 `src/routing.cpp`；FL/FXL 的生产块消去位于 `src/indexed_path.cpp`，各自向量参考调度保留在 `src/fl2025.cpp`、`src/path.cpp` 中；NF 仍使用向量调度。四个方法使用同一条反射 Gray 路径；门从左至右执行，线路和取值均从 0 编号。

FL2025 已于 2026-10-22 按用户要求改为原文 Theorem 2 的坐标序列消去构造：整体应用多元素 Rule 5，反向得到规定的回文路径字，再独立执行 Proposition 3 / Lemmas 4–5。旧版网格运输及额外范式对齐已移除。基本次数只计原 Rules 1–5，派生步骤展开计数；论文未固定的选择顺序见[算法映射](docs/algorithms.md)。这是原文构造的确定性实现，不是作者软件或最优调度；已编译并运行性能测试，完成有限样本的独立状态模拟与规范字检查。

## 编译

只需要 C++17 编译器和 Make，无第三方 C++ 库。

```sh
make -j4
```

生成三个可执行文件：

- `build/bench_normalize`：归约到范式。
- `build/bench_equivalence`：判断电路相等。
- `build/generate_data`：生成可复现的输入电路及测试清单。

也提供 CMake：

```sh
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j4
```

构建过程不执行任何测试或数据生成。

## 测试环境

2026-10-25 精选集实验在本地 Apple Silicon 主机上运行。操作系统与架构记录于[实验配置](results/fl-fxl-selected-20261025/config.json)；硬件和工具链信息于同日补录。

| 项目 | 配置 |
|---|---|
| 处理器 | Apple M5，10 个物理核心 / 10 个逻辑核心 |
| 物理内存 | 24 GiB（25,769,803,776 字节） |
| 操作系统 | macOS 26.6.2，构建号 25G83 |
| 架构 | ARM64 |
| C++ 编译器 | Apple clang 21.0.0（clang-2100.1.1.101）；目标 arm64-apple-darwin25.6.0 |
| 构建工具 | GNU Make 3.81 |
| 编译选项 | `-O2 -std=c++17 -Wall -Wextra -Wpedantic`；头文件路径 `-Iinclude` |
| 执行方式 | 单个算法单线程；各次实验串行，按电路及重复轮次交替 FL / FXL 顺序 |
| 重复次数 | 每个电路、每种算法 3 次，每次使用新进程和新缓存，无单独预热 |
| 规则数 | 每次完整归约的基本规则总次数；本轮同一电路、同一算法的 3 次计数一致 |
| 运行时间 | 3 次算法内部耗时的算术平均，单位毫秒；不含文件读取、进程启动和结果输出 |
| 资源限制 | 每次 300 秒、状态数 65,536、中间字长 100,000,000、内部工作量 200,000,000；进程外保护 330 秒 |

上述编译器版本为补录时本机 `c++ --version` 的输出，原始运行配置未独立记录编译器版本；不应将补录信息视为当时保存的完整工具链快照。实际源码、可执行文件和输入清单的 SHA-256 已记录在实验配置中。未设置 CPU 亲和性或隔离其他系统负载，计时表示本机实测结果，不构成统计显著性结论。完整数据见[逐电路报告](results/fl-fxl-selected-20261025/report.md)。

## 已准备的公开数据

[data/public_3to6/](data/public_3to6/README.md) 当前仅保留 10 个真实公开电路：3 线路 4 个，4、5、6 线路各 2 个。3 线路为至少 10 门筛选的例外；6 线路按用户选择保留两个此前均能完成的 mod5adder 实现，不能代表所有 6 线路功能。其余 57 个原有电路（含全部 7 线路）已删除。

来源、SHA-256、选取理由和目录见[测试集 README](data/public_3to6/README.md)。范式清单包含这 10 个输入；当前相等清单为空，未根据功能名称推断相等。

## 运行方式

归约到范式（当前精选 10 个公开输入）：

```sh
build/bench_normalize --algorithm all \
  --manifest data/public_3to6/normalization.tsv \
  --output normalization.csv
```

判断相等需自行指定两个输入（当前精选集未配置相等判定对）：

```sh
build/bench_equivalence --algorithm fl2025 --left A.real --right B.real \
  --output equivalence.csv
```

单个输入也可以使用 `--input FILE`，单个电路对使用 `--left A --right B`。
`--algorithm` 可选 `fl2025`、`fxl2026`、`nf`、`dc`、`all`。对多值输入选择 `all` 时，两个二值方法输出 `not_applicable`。

生成三值输入（同时产生相等与不相等电路对）：

```sh
build/generate_data --out generated/ternary-n3 \
  --d 3 --n 3 --gates 8 --pairs 10 --edits 4 --seed 20261022
```

`--pairs 10` 生成 10 个基础电路，每个基础电路各有一对相等、一对不相等输入，共 20 对。生成器只造数据，不调用任何待测算法。

```sh
build/bench_normalize --algorithm all \
  --manifest generated/ternary-n3/normalization.tsv
build/bench_equivalence --algorithm all \
  --manifest generated/ternary-n3/equivalence.tsv
```

输出目录、结果文件都要求尚不存在，避免覆盖已有数据。清单使用制表符，路径相对于清单所在目录解析；格式见 [输入与输出](docs/formats.md)。

## 输出与资源边界

CSV 字段：

```text
algorithm,input_a,input_b,repeat,status,equal,rule_count,time_ms
```

前六项只标识样本、执行状态和等价性答案；实验指标只有后两项。标准输出不打印额外统计，错误原因写到标准错误。

规则次数不是门数，也不是理论复杂度估计。共用局部模板缓存只节省构造时间，每次使用仍加入该模板完整的基本规则次数；不保存重写轨迹。正例包含将一个电路连接到另一个电路所需的配对插入次数。负例报告拒绝时已经使用的规则数。详见 [计数和计时](docs/measurement.md)。

DC 原本是等价性算法，其范式入口是显式适配：先由函数表构造唯一目标范式，再以 DC 连接输入与目标。目标构造时间纳入 `time_ms`，插入排序的普通操作不冒充基本规则应用。

默认每个样本、每个算法、每次重复的限制：

```text
--seconds 60 --max-states 65536 --max-word 100000000 --max-work 2000000
```

`--repeat K` 每次重新创建算法实例和缓存，不进行隐藏预热。`--max-work` 限制内部构造工作量，不是规则总次数上限。时间限制是协作式检查，不能保证中断一个正在执行的内存分配。实际内存仍随电路字和模板缓存增长。

超过限制、输入错误、计数溢出时，`rule_count` 留空，状态明确标识，不能视为相等或不相等结果。不相等电路的 `equal=false` 只由范式不一致或 DC 的非空终止链得出。

文献对应和当前实现边界见 [算法映射](docs/algorithms.md)。


## 优化与验证

已直接修改等长局部替换、辅助门删除、FXL 逐轮构造和 FL/FXL 范式阶段的位置索引，避免不必要的尾部搬移与重复全表扫描。不保存推导历史；只保留当前状态和有上限的局部模板摘要。规则数与算法的重写顺序保持不变。

原文对应、确定性选择及验证范围见[优化审计](docs/optimization-audit.md)。可用 `make verify` 显式运行验证套件；普通 `make` 不运行测试。

2026-10-22 重测采用 300 秒、2,000,000 中间字长和 200,000,000 内部工作量上限。配对成功数为 4 线路 30/30、5 线路 11/11、6 线路 3/11、7 线路 0/11。旧版和新版均完成的 84 条运行，其基本规则总数完全一致。[完整结果](results/fl-fxl-optimized-20261022/report.md)保留资源超限原因，未完成项不进入平均比率。

按后续要求，当前默认字长上限已改为 100,000,000（上次实验 2,000,000 的 50 倍）。重测保留 300 秒和 200,000,000 内部工作量上限。6 线路的 8 条 FXL 失败记录已重测，均触发工作量上限；7 线路按用户要求中止。详见[重测报告](results/fl-fxl-word100m-20261022/report.md)，历史报告仍对应原限制。

2026-10-25 精选集测试：10 个输入，FL/FXL 各运行 3 次，只报告规则总数和平均算法时间，见[最新逐电路报告](results/fl-fxl-selected-20261025/report.md)。旧轮次的 63 个样本报告保留为历史数据，不能视作当前测试集规模。
