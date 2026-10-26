# 精选公开可逆电路（3～6 线路）

数据集包含 10 个输入：3 线路 4 个，4、5、6 线路各 2 个。保留原始注释与门序列，没有重复拼接或添加抵消门来增加门数。

选取考虑功能种类和门数跨度，不是随机抽样，也不支持对同线路数全部电路作统计推广。3 线路电路为 4～6 门，其余为 10～55 门。6 线路仅包含两种此前 FL/FXL 均能完成的 mod5adder 实现，存在成功样本选择偏差，只代表这一功能的不同实现。

## 来源

- [RevLib](https://revlib.org/realizations.php)：原生 REAL 电路。
- [Maslov Reversible Benchmarks](https://reversiblebenchmarks.github.io/index.html)：按原门顺序一对一转换的 TFC 电路。`nth_prime5_inc_25_103` 保留原作者使用/再分发限制注释，仅用于本地实验。
- 每个来源 URL、文件 SHA-256 与转换来源信息见 [manifest.json](manifest.json)。

## 输入目录

| 线路 | 电路 | 门数 | 来源 | 选取理由 |
|---|---|---:|---|---|
| 3 | [ex-1_166](n3/ex-1_166.real) | 4 | [原始文件](https://revlib.org/doc/real/ex-1_166.real) | 公开 3 线路综合基准，4 门。 |
| 3 | [ham3_102](n3/ham3_102.real) | 5 | [原始文件](https://revlib.org/doc/real/ham3_102.real) | ham3 的纯 MCT 实现，5 门。 |
| 3 | [miller_11](n3/miller_11.real) | 5 | [原始文件](https://revlib.org/doc/real/miller_11.real) | Miller 基准，5 门。 |
| 3 | [3_17_13](n3/3_17_13.real) | 6 | [原始文件](https://revlib.org/doc/real/3_17_13.real) | 3_17 综合基准，6 门。 |
| 4 | [mod10_171](n4/mod10_171.real) | 10 | [原始文件](https://revlib.org/doc/real/mod10_171.real) | 模十功能，4 线路较短实现。 |
| 4 | [hwb4_49](n4/hwb4_49.real) | 17 | [原始文件](https://revlib.org/doc/real/hwb4_49.real) | HWB 功能，4 线路、17 门实现。 |
| 5 | [nth_prime5_inc_25_103](n5/nth_prime5_inc_25_103.real) | 25 | [原始文件](https://reversiblebenchmarks.github.io/nth_prime5_inc_25_103.tfc) | nth-prime 功能，5 线路中等门数实现。 |
| 5 | [hwb5_53](n5/hwb5_53.real) | 55 | [原始文件](https://revlib.org/doc/real/hwb5_53.real) | HWB 功能，5 线路、55 门实现。 |
| 6 | [mod5adder-15](n6/mod5adder-15.real) | 15 | [原始文件](https://reversiblebenchmarks.github.io/mod5adder-15.tfc) | 较短的 mod5adder 实现。 |
| 6 | [mod5adder-17-81](n6/mod5adder-17-81.real) | 17 | [原始文件](https://reversiblebenchmarks.github.io/mod5adder-17-81.tfc) | 较长的 mod5adder 实现，控制/目标布局不同。 |

范式归约使用 [normalization.tsv](normalization.tsv)。当前没有判等对清单；不能仅凭功能名称推断两个文件相等。来源、转换信息和文件哈希统一记录于 [manifest.json](manifest.json)。

FL2025 / FXL2026 的逐电路规则数、平均耗时及实验平台见[仓库 README](../../README.md#精选集实验结果与平台)。
