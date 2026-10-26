# 可逆逻辑规则计数

C++17 实现四种可逆电路方法，提供范式归约、等价性判断和可复现的数据生成入口。输出的实验指标为基本规则总次数 `rule_count` 和运行时间 `time_ms`。

| 标识 | 方法 | 输入范围 | 主要实现 |
|---|---|---|---|
| `fl2025` | Feng–Li Theorem 2 / Proposition 3 | 二值 | `src/fl2025.cpp` |
| `fxl2026` | Feng–Xiong–Li Section III / Algorithms 1–4 | 二值 | `src/fxl2026.cpp` |
| `nf` | 多值论文第三节：控制归一化、递归路径化、块消去 | 二值及多值 | `src/nf.cpp` |
| `dc` | 多值论文第四节：网格运输、递增链直接消去 | 二值及多值 | `src/dc.cpp` |

四种方法共用反射 Gray 路径，门从左至右执行，线路和取值从 0 编号。FL2025 的坐标对选择采用受限的确定性调度，任意线路数下的终止性尚缺少一般性证明；具体构造和适用边界见[算法说明](docs/algorithms.md)。

## 构建

只需 C++17 编译器和 Make：

```sh
make -j4
```

生成 `build/bench_normalize`、`build/bench_equivalence` 和 `build/generate_data`。构建过程不运行算法、测试或数据生成。

也可使用 CMake：

```sh
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j4
```

## 使用

对一个二值电路归约到范式：

```sh
build/bench_normalize --algorithm fl2025 --input data/public_3to6/n3/ex-1_166.real
```

对精选数据集批量运行，输出 CSV：

```sh
build/bench_normalize --algorithm fl2025 --manifest data/public_3to6/normalization.tsv
```

默认先按每条线路作为门目标的次数降序排列，频次高的线路使用更小下标，次数相同则保留原线路顺序。这精确最小化全部输入门的目标下标之和。两种二值方法共用这一排列；目标和所有控制位置同步重映射，原始数据文件保持不变。

保留原始线路顺序的对照运行：

```sh
build/bench_normalize --algorithm fl2025 --input data/public_3to6/n3/ex-1_166.real --wire-order original
```

也可显式指定默认的 `--wire-order target-frequency`。CSV 附加 `wire_order,wire_map`，其中 `wire_map[k]` 为原第 k 条线路的新下标；原始顺序模式保留原有八列。目标次数统计、排序和重映射计入耗时，不计为基本规则应用。这一目标不保证归约规则数或运行时间最小。

判断两个输入是否等价：

```sh
build/bench_equivalence --algorithm fxl2026 --left A.real --right B.real
```

`--algorithm` 可选 `fl2025`、`fxl2026`、`nf`、`dc`、`all`。对多值输入选择 `all` 时，两个二值方法返回 `not_applicable`。支持单文件输入和 TSV 清单；现有公开数据集不提供判等对。判等时合并两侧目标次数生成一个共同排列，对两侧同步应用；输入文件本身的线路对应关系需事先统一。

生成三值输入及带标签的判等对：

```sh
build/generate_data --out generated/ternary-n3 --d 3 --n 3 --gates 8 --pairs 10 --edits 4 --seed 20261022
```

每个基础电路生成一对相等输入和一对不相等输入。生成器不调用待测算法。生成目录和结果文件必须尚不存在，避免覆盖数据。

## 输出与限制

```text
algorithm,input_a,input_b,repeat,status,equal,rule_count,time_ms,wire_order,wire_map
```

默认每个样本、方法、重复轮次的资源限制：

```text
--seconds 60 --max-states 65536 --max-word 100000000 --max-work 2000000
```

`max_word` 限制中间字长，`max_work` 限制内部构造工作量；两者都不是规则总次数。超限、输入错误或计数溢出时不输出完整次数，也不据此给出等价性答案。`--repeat K` 每次建立独立实例和缓存。

规则数表示指定规则系统和调度的基本步数，不是门数、硬件成本或最短推导长度。模板缓存命中仍累加完整规则成本。DC 的范式入口先构造目标范式，再通过直接消去连接输入与目标，目标构造时间计入耗时。

## 精选集实验结果与平台

### 目标频次排序（当前默认）

2026-10-26 对全部 10 个 RevLib 原始 REAL 文件重新测试 FL2025 和 FXL2026。只在内存中重映射线路，不改下载原件。每个电路、每种方法重复 3 次，共 **60 次，全部完成**；三次规则数一致，下表时间为三次内部耗时的算术平均，没有沿用之前的耗时。

3 线路选 4 个电路，4～6 线路各 2 个，全部为纯 MCT、无固定输入和垃圾输出。5 线路的两个实现对应同一 HWB5 函数，6 线路的两个实现对应同一模五加法函数；这是固定的精选实例集，不代表同线路数电路的总体分布。本轮没有替换样本或提高资源上限。

#### 排序目标与线路映射

设原线路 k 在输入门中作为目标的次数为 f_k，新下标为 p(k)，最小化 S(p)=∑_k f_k p(k)，线路从 0 编号。按 f_k 降序分配新下标；频次相同保留原顺序。若 f_i < f_j 却分配到 p(i) < p(j)，交换二者会使 S 减少 (f_j−f_i)(p(j)−p(i))，故该排序达到全局最小，不需要随机排列或枚举搜索。

例如频次 `[2,5,0,5]` 得到映射 `[2,0,3,1]`，S 从 20 降到 9。这里最小化的是输入门的目标下标之和，不是最终范式长度、规则总数或耗时。平局按原顺序处理，也不保证不同原始编号得到唯一相同的坐标系。

以下频次按原线路下标排列，`wire_map[k]` 表示原线路 k 的新下标。FL、FXL 及各次重复对同一电路使用相同映射；判等入口则合并两侧频次产生一个共同映射。

| 电路 | 原线路目标频次 | old-to-new 映射 | 原始 S | 最小 S |
|---|---|---|---:|---:|
| ex-1_166 | `[2,2,0]` | `[0,1,2]` | 2 | 2 |
| ham3_102 | `[1,2,2]` | `[2,0,1]` | 6 | 4 |
| miller_11 | `[1,2,2]` | `[2,0,1]` | 6 | 4 |
| 3_17_13 | `[1,1,4]` | `[1,2,0]` | 9 | 3 |
| mod10_171 | `[1,2,4,3]` | `[3,2,0,1]` | 19 | 10 |
| hwb4_49 | `[6,3,4,4]` | `[0,3,1,2]` | 23 | 21 |
| hwb5_55 | `[2,5,4,7,6]` | `[4,2,3,0,1]` | 58 | 36 |
| hwb5_53 | `[12,15,12,10,6]` | `[1,0,2,3,4]` | 93 | 90 |
| mod5adder_128 | `[0,0,0,5,5,5]` | `[3,4,5,0,1,2]` | 60 | 15 |
| mod5adder_129 | `[2,0,0,6,5,4]` | `[3,4,5,0,1,2]` | 58 | 19 |

#### 归约结果

| 线路 | 电路 | 输入门数 | FL 规则数 | FL 时间（ms） | FXL 规则数 | FXL 时间（ms） |
|---|---|---:|---:|---:|---:|---:|
| 3 | ex-1_166 | 4 | 44 | 0.115 | 46 | 0.101 |
| 3 | ham3_102 | 5 | 1,347 | 0.443 | 663 | 0.320 |
| 3 | miller_11 | 5 | 368 | 0.235 | 93 | 0.131 |
| 3 | 3_17_13 | 6 | 1,728 | 0.425 | 3,338 | 0.648 |
| 4 | mod10_171 | 10 | 20,851 | 1.646 | 39,475 | 2.001 |
| 4 | hwb4_49 | 17 | 251,771 | 3.771 | 836,646 | 13.587 |
| 5 | hwb5_55 | 24 | 7,127,948 | 32.175 | 113,245,262 | 766.202 |
| 5 | hwb5_53 | 55 | 10,019,334 | 34.876 | 169,613,940 | 1068.361 |
| 6 | mod5adder_128 | 15 | 4,994,923 | 25.607 | 72,058,371 | 403.936 |
| 6 | mod5adder_129 | 17 | 8,290,063 | 34.988 | 280,946,652 | 1505.475 |

全部 10 个电路的两种方法均在既定资源上限内完成。该排序保证输入门目标下标之和最小；归约规则数和耗时仍由具体电路及规则调度决定，不保证随该总和单调下降。

20 个“输入—方法”组合全部在计时外通过完整二值状态模拟、线路映射语义、路径化与范式端点、独立函数表构造的规范字和测量规则数一致性检查。线路排序另通过 368 个全排列最小值检查和 53,375 个完整状态枚举语义用例；原有 242 个基础电路回归及命令行共同映射判等检查通过。这些有限验证不替代算法的一般正确性证明，具体边界见[验证说明](docs/verification.md)。

#### 本轮实验平台与配置

| 项目 | 配置 |
|---|---|
| 处理器 | Apple M5，10 个物理核心 / 10 个逻辑核心 |
| 物理内存 | 24 GiB（25,769,803,776 字节） |
| 操作系统 | macOS 26.6.2，构建号 25G83；ARM64 |
| C++ 编译器 | Apple clang 21.0.0（clang-2100.1.1.101）；目标 arm64-apple-darwin25.6.0 |
| 构建工具 | GNU Make 3.81 |
| 编译选项 | `-O2 -std=c++17 -Wall -Wextra -Wpedantic`；`-Iinclude` |
| 执行方式 | 串行、单个算法单线程；按电路和重复轮次交替 FL / FXL 顺序 |
| 重复与缓存 | 每项 3 次；每次新进程、新缓存，无单独预热 |
| 线路预处理 | 目标频次降序，平局保留原顺序；统计、排序、校验及映射计入时间，不计规则数 |
| 内部计时 | 包括 Gray 路径、模板构造与缓存、路径化、范式归约和算法对象清理；不含读取、进程启动、CSV 输出和独立正确性检查 |
| 算法限制 | 每次 300 秒、65,536 状态、100,000,000 中间字长、200,000,000 内部工作量 |
| 进程外保护 | 330 秒；每 250 ms 采样子进程 RSS，超过 6 GiB 则终止 |
| 本轮运行区间 | 2026-10-26 11:32:41–11:32:54（Asia/Shanghai） |

平台信息在本轮运行前重新采集。未设置 CPU 亲和性或隔离其他系统负载，三次均值尤其是亚毫秒差异不构成统计显著性结论。资源上限限制本次执行规模，不表示算法理论能力；本轮没有触发任何上限。`max_work` 是内部构造操作数，不是基本规则数，完成项的规则数可超过该值。

单次运行命令如下，其他电路和 FL 方法相应替换输入及方法名；整轮由外部串行脚本为每次测量启动新进程并执行上述保护。达到任一上限时应记录失败并跳过该项剩余重复，不更换输入或放宽上限。

```sh
build/bench_normalize --algorithm fxl2026 --input data/public_3to6/n6/mod5adder_128.real --wire-order target-frequency --seconds 300 --max-states 65536 --max-word 100000000 --max-work 200000000
```

<details>
<summary>目标频次排序逐次运行记录（CSV，重复号从 1 开始）</summary>

```csv
id,algorithm,repeat,status,rule_count,time_ms,wire_order,wire_map
ex-1_166,fl2025,1,ok,44,0.138083,target-frequency,"[0,1,2]"
ex-1_166,fxl2026,1,ok,46,0.102500,target-frequency,"[0,1,2]"
ex-1_166,fxl2026,2,ok,46,0.098875,target-frequency,"[0,1,2]"
ex-1_166,fl2025,2,ok,44,0.104250,target-frequency,"[0,1,2]"
ex-1_166,fl2025,3,ok,44,0.101875,target-frequency,"[0,1,2]"
ex-1_166,fxl2026,3,ok,46,0.101750,target-frequency,"[0,1,2]"
ham3_102,fxl2026,1,ok,663,0.332834,target-frequency,"[2,0,1]"
ham3_102,fl2025,1,ok,1347,0.483291,target-frequency,"[2,0,1]"
ham3_102,fl2025,2,ok,1347,0.412708,target-frequency,"[2,0,1]"
ham3_102,fxl2026,2,ok,663,0.313625,target-frequency,"[2,0,1]"
ham3_102,fxl2026,3,ok,663,0.312208,target-frequency,"[2,0,1]"
ham3_102,fl2025,3,ok,1347,0.434167,target-frequency,"[2,0,1]"
miller_11,fl2025,1,ok,368,0.238750,target-frequency,"[2,0,1]"
miller_11,fxl2026,1,ok,93,0.129500,target-frequency,"[2,0,1]"
miller_11,fxl2026,2,ok,93,0.138416,target-frequency,"[2,0,1]"
miller_11,fl2025,2,ok,368,0.233750,target-frequency,"[2,0,1]"
miller_11,fl2025,3,ok,368,0.231959,target-frequency,"[2,0,1]"
miller_11,fxl2026,3,ok,93,0.125667,target-frequency,"[2,0,1]"
3_17_13,fxl2026,1,ok,3338,0.670167,target-frequency,"[1,2,0]"
3_17_13,fl2025,1,ok,1728,0.428625,target-frequency,"[1,2,0]"
3_17_13,fl2025,2,ok,1728,0.447708,target-frequency,"[1,2,0]"
3_17_13,fxl2026,2,ok,3338,0.660500,target-frequency,"[1,2,0]"
3_17_13,fxl2026,3,ok,3338,0.612792,target-frequency,"[1,2,0]"
3_17_13,fl2025,3,ok,1728,0.398709,target-frequency,"[1,2,0]"
mod10_171,fl2025,1,ok,20851,1.605166,target-frequency,"[3,2,0,1]"
mod10_171,fxl2026,1,ok,39475,1.907750,target-frequency,"[3,2,0,1]"
mod10_171,fxl2026,2,ok,39475,2.117417,target-frequency,"[3,2,0,1]"
mod10_171,fl2025,2,ok,20851,1.595667,target-frequency,"[3,2,0,1]"
mod10_171,fl2025,3,ok,20851,1.738625,target-frequency,"[3,2,0,1]"
mod10_171,fxl2026,3,ok,39475,1.978792,target-frequency,"[3,2,0,1]"
hwb4_49,fxl2026,1,ok,836646,13.859250,target-frequency,"[0,3,1,2]"
hwb4_49,fl2025,1,ok,251771,3.824500,target-frequency,"[0,3,1,2]"
hwb4_49,fl2025,2,ok,251771,3.772583,target-frequency,"[0,3,1,2]"
hwb4_49,fxl2026,2,ok,836646,13.143333,target-frequency,"[0,3,1,2]"
hwb4_49,fxl2026,3,ok,836646,13.759208,target-frequency,"[0,3,1,2]"
hwb4_49,fl2025,3,ok,251771,3.716333,target-frequency,"[0,3,1,2]"
hwb5_55,fl2025,1,ok,7127948,32.018166,target-frequency,"[4,2,3,0,1]"
hwb5_55,fxl2026,1,ok,113245262,778.523083,target-frequency,"[4,2,3,0,1]"
hwb5_55,fxl2026,2,ok,113245262,765.275916,target-frequency,"[4,2,3,0,1]"
hwb5_55,fl2025,2,ok,7127948,32.338459,target-frequency,"[4,2,3,0,1]"
hwb5_55,fl2025,3,ok,7127948,32.169625,target-frequency,"[4,2,3,0,1]"
hwb5_55,fxl2026,3,ok,113245262,754.807917,target-frequency,"[4,2,3,0,1]"
hwb5_53,fxl2026,1,ok,169613940,1066.441042,target-frequency,"[1,0,2,3,4]"
hwb5_53,fl2025,1,ok,10019334,32.726042,target-frequency,"[1,0,2,3,4]"
hwb5_53,fl2025,2,ok,10019334,37.654500,target-frequency,"[1,0,2,3,4]"
hwb5_53,fxl2026,2,ok,169613940,1103.166958,target-frequency,"[1,0,2,3,4]"
hwb5_53,fxl2026,3,ok,169613940,1035.476209,target-frequency,"[1,0,2,3,4]"
hwb5_53,fl2025,3,ok,10019334,34.246792,target-frequency,"[1,0,2,3,4]"
mod5adder_128,fl2025,1,ok,4994923,25.577666,target-frequency,"[3,4,5,0,1,2]"
mod5adder_128,fxl2026,1,ok,72058371,400.819666,target-frequency,"[3,4,5,0,1,2]"
mod5adder_128,fxl2026,2,ok,72058371,405.201416,target-frequency,"[3,4,5,0,1,2]"
mod5adder_128,fl2025,2,ok,4994923,25.460375,target-frequency,"[3,4,5,0,1,2]"
mod5adder_128,fl2025,3,ok,4994923,25.783584,target-frequency,"[3,4,5,0,1,2]"
mod5adder_128,fxl2026,3,ok,72058371,405.788292,target-frequency,"[3,4,5,0,1,2]"
mod5adder_129,fxl2026,1,ok,280946652,1500.521250,target-frequency,"[3,4,5,0,1,2]"
mod5adder_129,fl2025,1,ok,8290063,33.420917,target-frequency,"[3,4,5,0,1,2]"
mod5adder_129,fl2025,2,ok,8290063,39.900584,target-frequency,"[3,4,5,0,1,2]"
mod5adder_129,fxl2026,2,ok,280946652,1505.615542,target-frequency,"[3,4,5,0,1,2]"
mod5adder_129,fxl2026,3,ok,280946652,1510.288333,target-frequency,"[3,4,5,0,1,2]"
mod5adder_129,fl2025,3,ok,8290063,31.641750,target-frequency,"[3,4,5,0,1,2]"
```

</details>

<details>
<summary>目标频次排序运行配置、源码和二进制哈希</summary>

```json
{
  "date": "2026-10-26",
  "task": "binary_normalization",
  "algorithms": [
    "fl2025",
    "fxl2026"
  ],
  "repetitions": 3,
  "repeat_policy": "Three repetitions for successful circuit/method pairs; after the first resource limit, record it and skip remaining repetitions of that pair without substituting another input.",
  "seconds": 300,
  "max_states": 65536,
  "max_word": 100000000,
  "max_work": 200000000,
  "outer_timeout_seconds": 330,
  "rss_limit_bytes": 6442450944,
  "rss_sample_seconds": 0.25,
  "order": "serial; alternate algorithm order by circuit and repetition; fresh process and cache; no warmup",
  "platform": "macOS-26.6.2-arm64-arm-64bit-Mach-O",
  "machine": "arm64",
  "os_version": "26.6.2",
  "os_build": "25G83",
  "hardware": {
    "processor": "Apple M5",
    "physical_cores": 10,
    "logical_cores": 10,
    "memory_bytes": 25769803776
  },
  "compiler": "Apple clang version 21.0.0 (clang-2100.1.1.101)\nTarget: arm64-apple-darwin25.6.0\nThread model: posix\nInstalledDir: /Library/Developer/CommandLineTools/usr/bin",
  "make": "GNU Make 3.81",
  "cppflags": "-Iinclude",
  "cxxflags": "-O2 -std=c++17 -Wall -Wextra -Wpedantic",
  "git_head": "b8a340b6fb85484cc0da3cdc4eda99d8228e8ea4",
  "binary_sha256": "5ab2c509eecbe45fa8567df935402244971faf8b53f952c0b456711b37f4cb43",
  "source_sha256": {
    "src/benchmark.cpp": "d036371016b9d814e71a3228698f3cb0bd667e7b49dbc150200f491cc94ce66a",
    "src/compiler.cpp": "b07c2562966501c2da2c96253dcb8ba7d16b377c01290ece8e9c15ecb3220200",
    "src/core.cpp": "e150d01209d57728a75556dffcd19e39db8e8d499b5d5585bed86d25db571be2",
    "src/dc.cpp": "1626a547b6afd35eb4082512f1a8baa16be6ba7c56bb7006206ddd18eabae04a",
    "src/experiments.cpp": "92a1a26868796505e2ce8338ac928c70533ef2bdf33b1dbb2458ecd589b8cb95",
    "src/fl2025.cpp": "c6ee760c0cde64577c88161715ccd65f68ddeaf3ec01c9a41712dc587034eab6",
    "src/fxl2026.cpp": "9df845d85cc775278f232c76988a4a22d6564c73a2e7229020fd94d1afb34fec",
    "src/indexed_path.cpp": "d1003ee4caa2c97af08ef6c2244a67abbfda9d05e6f68617027f60a6f2478a43",
    "src/io.cpp": "d5f7e00bf26c4817e1a7c8886a8b9a8f1eb9c1e3811ae569abd33c07ed9fb48f",
    "src/nf.cpp": "0bfea2e0e4ef4ab01148b8c8dbcd16d93397d48f7a1e34584e5ef05fcab978d2",
    "src/path.cpp": "ad5837a60a2e1fd771646fd30addbcae387b2ee4143c9f33b806af830d3b84c6",
    "src/routing.cpp": "fc3ec68a18575851cae6c745130a2941d0f0c14937e06b7e4ea339fd8376a947",
    "src/wires.cpp": "1834aad2890f858645747d7e17f43a8f9a5c6abedaca2d2da3ce7b463aee14ca",
    "include/rv/cli.hpp": "69fe0bb9c18f5810d975f6e6a6e3df31c19ce3a34fa6150250f61a3025820be1",
    "include/rv/compiler.hpp": "e897e1e43e3e9adf64399e256668d5e5b7cb6409bd21420a61f64bc7d70fe891",
    "include/rv/core.hpp": "a29b8d2df4cdf8d6b7d65de1da8c0dae98c10c9b066fa5733b8880bad56f65c1",
    "apps/bench_equivalence.cpp": "ab0a8d7331c49154b2abbeea4bab77e2e7ba928261935e5a7310e1c317bb568d",
    "apps/bench_normalize.cpp": "2f11663c8589a06afff0cd85e182eff3a6919b09f9cb77871f4b60afc9f3fd40",
    "apps/generate_data.cpp": "40ff7c8ebda7e128fc6050cc7983feab4a6ef93a19b477de46287817db8ffb31"
  },
  "manifest_sha256": "c80b7c2fe06b2ea41de8a733b6242b9f08efb3076034de77de61f48e4e362790",
  "wire_order": "target-frequency",
  "preprocessing": "Minimize input gate target-index sum: descending target frequency, stable input-order ties; counting, sorting and remapping included in internal time.",
  "start_local": "2026-10-26T11:32:41.909525+08:00",
  "target_objectives": {
    "ex-1_166": {
      "target_frequency": [
        2,
        2,
        0
      ],
      "wire_map": [
        0,
        1,
        2
      ],
      "original_sum": 2,
      "minimum_sum": 2
    },
    "ham3_102": {
      "target_frequency": [
        1,
        2,
        2
      ],
      "wire_map": [
        2,
        0,
        1
      ],
      "original_sum": 6,
      "minimum_sum": 4
    },
    "miller_11": {
      "target_frequency": [
        1,
        2,
        2
      ],
      "wire_map": [
        2,
        0,
        1
      ],
      "original_sum": 6,
      "minimum_sum": 4
    },
    "3_17_13": {
      "target_frequency": [
        1,
        1,
        4
      ],
      "wire_map": [
        1,
        2,
        0
      ],
      "original_sum": 9,
      "minimum_sum": 3
    },
    "mod10_171": {
      "target_frequency": [
        1,
        2,
        4,
        3
      ],
      "wire_map": [
        3,
        2,
        0,
        1
      ],
      "original_sum": 19,
      "minimum_sum": 10
    },
    "hwb4_49": {
      "target_frequency": [
        6,
        3,
        4,
        4
      ],
      "wire_map": [
        0,
        3,
        1,
        2
      ],
      "original_sum": 23,
      "minimum_sum": 21
    },
    "hwb5_55": {
      "target_frequency": [
        2,
        5,
        4,
        7,
        6
      ],
      "wire_map": [
        4,
        2,
        3,
        0,
        1
      ],
      "original_sum": 58,
      "minimum_sum": 36
    },
    "hwb5_53": {
      "target_frequency": [
        12,
        15,
        12,
        10,
        6
      ],
      "wire_map": [
        1,
        0,
        2,
        3,
        4
      ],
      "original_sum": 93,
      "minimum_sum": 90
    },
    "mod5adder_128": {
      "target_frequency": [
        0,
        0,
        0,
        5,
        5,
        5
      ],
      "wire_map": [
        3,
        4,
        5,
        0,
        1,
        2
      ],
      "original_sum": 60,
      "minimum_sum": 15
    },
    "mod5adder_129": {
      "target_frequency": [
        2,
        0,
        0,
        6,
        5,
        4
      ],
      "wire_map": [
        3,
        4,
        5,
        0,
        1,
        2
      ],
      "original_sum": 58,
      "minimum_sum": 19
    }
  },
  "wire_maps": {
    "ex-1_166": [
      0,
      1,
      2
    ],
    "ham3_102": [
      2,
      0,
      1
    ],
    "miller_11": [
      2,
      0,
      1
    ],
    "3_17_13": [
      1,
      2,
      0
    ],
    "mod10_171": [
      3,
      2,
      0,
      1
    ],
    "hwb4_49": [
      0,
      3,
      1,
      2
    ],
    "hwb5_55": [
      4,
      2,
      3,
      0,
      1
    ],
    "hwb5_53": [
      1,
      0,
      2,
      3,
      4
    ],
    "mod5adder_128": [
      3,
      4,
      5,
      0,
      1,
      2
    ],
    "mod5adder_129": [
      3,
      4,
      5,
      0,
      1,
      2
    ]
  },
  "end_local": "2026-10-26T11:32:54.245114+08:00",
  "readme_csv_sha256": "f00812a4756da7b286fcfce46d8c93e95c150d0ee730418f4b66d82298b2375b",
  "raw_csv_sha256": "ccc9929b84e77af394efd002770afe8c15f2e8f4c2bf92ea13e753ec22a38292",
  "validation": "All 20 completed input/method pairs passed full truth tables, canonical endpoints and rule counts outside timing; 368 objective-optimality cases, 53375 wire-map semantic cases, 242 original-order binary regression inputs, and CLI shared-map equivalence checks passed.",
  "observed_rss_note": "RSS sampled every 250 ms; 0 means the process completed before a sample, not zero actual memory use."
}
```

</details>

## 测试与文档

仓库保留 10 个 RevLib 原生二值电路及来源哈希，见[数据说明](data/public_3to6/README.md)。精选集实验结果与平台汇总于本页；生成数据保存在本地忽略目录中。

- [算法、调度与实现边界](docs/algorithms.md)
- [输入输出格式](docs/formats.md)
- [规则计数、计时及状态](docs/measurement.md)
- [验证方法及其覆盖范围](docs/verification.md)
- [文献版本与快照哈希](docs/source-snapshot.md)

`make verify-wires` 单独检查线路重排；`make verify` 显式运行全部验证，包括扩展、稀疏索引及线路重排。基础验证可单独运行：

```sh
make build/verify_binary
build/verify_binary
```
