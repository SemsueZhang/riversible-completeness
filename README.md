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

对精选数据集批量运行并保存结果：

```sh
mkdir -p results
build/bench_normalize --algorithm fl2025 --manifest data/public_3to6/normalization.tsv --output results/normalization.csv
```

判断两个输入是否等价：

```sh
build/bench_equivalence --algorithm fxl2026 --left A.real --right B.real
```

`--algorithm` 可选 `fl2025`、`fxl2026`、`nf`、`dc`、`all`。对多值输入选择 `all` 时，两个二值方法返回 `not_applicable`。支持单文件输入和 TSV 清单；现有公开数据集不提供判等对。

生成三值输入及带标签的判等对：

```sh
build/generate_data --out generated/ternary-n3 --d 3 --n 3 --gates 8 --pairs 10 --edits 4 --seed 20261022
```

每个基础电路生成一对相等输入和一对不相等输入。生成器不调用待测算法。生成目录和结果文件必须尚不存在，避免覆盖数据。

## 输出与限制

```text
algorithm,input_a,input_b,repeat,status,equal,rule_count,time_ms
```

默认每个样本、方法、重复轮次的资源限制：

```text
--seconds 60 --max-states 65536 --max-word 100000000 --max-work 2000000
```

`max_word` 限制中间字长，`max_work` 限制内部构造工作量；两者都不是规则总次数。超限、输入错误或计数溢出时不输出完整次数，也不据此给出等价性答案。`--repeat K` 每次建立独立实例和缓存。

规则数表示指定规则系统和调度的基本步数，不是门数、硬件成本或最短推导长度。模板缓存命中仍累加完整规则成本。DC 的范式入口先构造目标范式，再通过直接消去连接输入与目标，目标构造时间计入耗时。

## 测试与文档

仓库保留 10 个公开二值电路及来源哈希，见[数据说明](data/public_3to6/README.md)。运行结果和生成数据保存在本地忽略目录中。

- [算法、调度与实现边界](docs/algorithms.md)
- [输入输出格式](docs/formats.md)
- [规则计数、计时及状态](docs/measurement.md)
- [验证方法及其覆盖范围](docs/verification.md)
- [文献版本与快照哈希](docs/source-snapshot.md)

`make verify` 显式运行扩展验证和稀疏索引检查；基础验证可单独运行：

```sh
make build/verify_binary
build/verify_binary
```
