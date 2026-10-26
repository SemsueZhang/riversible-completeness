# RevLib 精选公开可逆电路（3～6 线路）

数据集包含 10 个直接从 [RevLib](https://revlib.org/realizations.php) 下载的 REAL 原文件：3 线路 4 个，4、5、6 线路各 2 个。保留原始字节、注释、线路顺序和门序；不做 TFC 转换，也不添加门。

3 线路电路为 4～6 门，其余为 10～55 门。5 线路的两个样本是 HWB5 的 24 门和 55 门实现，6 线路的两个样本是 mod5adder 的 15 门和 17 门实现；每组表示同一完整函数的不同电路实现。这是有目的的小样本，不能视为随机或具有总体代表性的选取。本轮实验开始前固定输入，不按本轮完成情况替换样本。

## 来源与输入目录

| 线路 | 电路 | 门数 | 原始文件 |
|---|---|---:|---|
| 3 | [ex-1_166](n3/ex-1_166.real) | 4 | [RevLib 原件](https://revlib.org/doc/real/ex-1_166.real) |
| 3 | [ham3_102](n3/ham3_102.real) | 5 | [RevLib 原件](https://revlib.org/doc/real/ham3_102.real) |
| 3 | [miller_11](n3/miller_11.real) | 5 | [RevLib 原件](https://revlib.org/doc/real/miller_11.real) |
| 3 | [3_17_13](n3/3_17_13.real) | 6 | [RevLib 原件](https://revlib.org/doc/real/3_17_13.real) |
| 4 | [mod10_171](n4/mod10_171.real) | 10 | [RevLib 原件](https://revlib.org/doc/real/mod10_171.real) |
| 4 | [hwb4_49](n4/hwb4_49.real) | 17 | [RevLib 原件](https://revlib.org/doc/real/hwb4_49.real) |
| 5 | [hwb5_55](n5/hwb5_55.real) | 24 | [RevLib 原件](https://revlib.org/doc/real/hwb5_55.real) |
| 5 | [hwb5_53](n5/hwb5_53.real) | 55 | [RevLib 原件](https://revlib.org/doc/real/hwb5_53.real) |
| 6 | [mod5adder_128](n6/mod5adder_128.real) | 15 | [RevLib 原件](https://revlib.org/doc/real/mod5adder_128.real) |
| 6 | [mod5adder_129](n6/mod5adder_129.real) | 17 | [RevLib 原件](https://revlib.org/doc/real/mod5adder_129.real) |

每个文件的来源 URL、下载日期、原件 SHA-256、规范化门序列哈希及格式元数据统一记录于 [manifest.json](manifest.json)。清单中的线路数和门数以文件内容为准。`redistribution_restriction: false` 仅表示文件注释中未发现显式的再分发限制，不表示额外授予了许可证。

`hwb5_53` 与 `hwb5_55` 的上游第一行均将功能误写为 `hwb4`；两者实际均为 5 线路 HWB5 实现，原件不作静默修订。`mod5adder_128` 与 `mod5adder_129` 保留原件的 `f e d c b a` 线路顺序。实验中的目标频次排序只在内存中应用，具体映射见仓库 README。

范式归约使用 [normalization.tsv](normalization.tsv)。当前没有判等对清单；不能仅凭功能名称推断相等。两个 HWB5 实现和两个 mod5adder 实现已分别通过全部二值输入的直接模拟等价性检查。

FL2025 / FXL2026 的逐电路规则数、平均耗时及本轮实验平台见[仓库 README](../../README.md#精选集实验结果与平台)。
