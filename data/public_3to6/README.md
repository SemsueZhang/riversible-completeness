# 精选公开可逆电路（3～6 线路）

2026-10-25 按用户要求将测试集缩减为 10 个输入：3 线路 4 个，4、5、6 线路各 2 个。删除其余 57 个原有电路（包括全部 7 线路），恢复 4 个真实 3 线路基准。原始注释与门序列保留，没有重复拼接或添加抵消门来凑门数。

选取考虑功能种类和门数跨度，不是随机抽样，也不支持对同线路数全部电路作统计推广。3 线路为此前至少 10 门要求的例外。6 线路经用户明确选择，从此前两算法都能完成的 mod5adder 实现中选取，因此只代表该功能的不同实现，存在成功样本选择偏差。

## 来源

- [RevLib](https://revlib.org/realizations.php)：原生 REAL 电路；恢复的 3 线路文件于 2026-10-25 重新下载，与此前来源缓存逐字节一致。
- [Maslov Reversible Benchmarks](https://reversiblebenchmarks.github.io/index.html)：此前按原门顺序一对一转换的 TFC 电路。`nth_prime5_inc_25_103` 保留原作者使用/再分发限制注释，仅用于本地实验。
- 每个来源 URL、文件 SHA-256 与转换来源信息见 [manifest.json](manifest.json)。

## 输入目录

| 线路 | 电路 | 门数 | 来源 | 选取理由 |
|---|---|---:|---|---|
| 3 | [ex-1_166](n3/ex-1_166.real) | 4 | [原始文件](https://revlib.org/doc/real/ex-1_166.real) | 公开 3 线路综合基准，4 门。 |
| 3 | [ham3_102](n3/ham3_102.real) | 5 | [原始文件](https://revlib.org/doc/real/ham3_102.real) | ham3 的纯 MCT 实现，5 门。 |
| 3 | [miller_11](n3/miller_11.real) | 5 | [原始文件](https://revlib.org/doc/real/miller_11.real) | Miller 基准，5 门。 |
| 3 | [3_17_13](n3/3_17_13.real) | 6 | [原始文件](https://revlib.org/doc/real/3_17_13.real) | 3_17 综合基准，6 门。 |
| 4 | [mod10_171](n4/mod10_171.real) | 10 | [原始文件](https://revlib.org/doc/real/mod10_171.real) | 模十功能，4 线路较短实现。 |
| 4 | [hwb4_49](n4/hwb4_49.real) | 17 | [原始文件](https://revlib.org/doc/real/hwb4_49.real) | HWB 功能，4 线路原池中门数最多的实现。 |
| 5 | [nth_prime5_inc_25_103](n5/nth_prime5_inc_25_103.real) | 25 | [原始文件](https://reversiblebenchmarks.github.io/nth_prime5_inc_25_103.tfc) | nth-prime 功能，5 线路中等门数实现。 |
| 5 | [hwb5_53](n5/hwb5_53.real) | 55 | [原始文件](https://revlib.org/doc/real/hwb5_53.real) | HWB 功能，5 线路原池中门数最多的实现。 |
| 6 | [mod5adder-15](n6/mod5adder-15.real) | 15 | [原始文件](https://reversiblebenchmarks.github.io/mod5adder-15.tfc) | 用户选择优先完整结果；可完成 mod5adder 实现中的较短电路。 |
| 6 | [mod5adder-17-81](n6/mod5adder-17-81.real) | 17 | [原始文件](https://reversiblebenchmarks.github.io/mod5adder-17-81.tfc) | 用户选择优先完整结果；较长 mod5adder 实现，控制/目标布局不同。 |

范式归约使用 [normalization.tsv](normalization.tsv)。当前未选择相等判定对，`equivalence.tsv` 仅含说明；不能直接作为相等测试输入。

本次 FL/FXL 规则数和运行时间见[测试报告](../../results/fl-fxl-selected-20261025/report.md)。历史测试结果保留在 `results/`，其路径与数据规模是当时的快照；被删除的输入不会为旧脚本另存副本。
