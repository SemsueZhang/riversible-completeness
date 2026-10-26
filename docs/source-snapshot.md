# 文献版本与快照哈希

实现依据为 2026-10-22 核对的以下版本。仓库运行不依赖文献 PDF 或外部研究目录。

| 来源 | SHA-256 |
|---|---|
| 提供的 9 页 *A Complete and Irredundant Rule System for Multi-Valued Reversible Logic* | `223b0b6b5adcd51aac23a85785336fccb75fcc973f1d635cefc0b60043d8898a` |
| Feng–Li，[arXiv:2508.17273v2](https://arxiv.org/abs/2508.17273v2) | `0fa31a516ee4335928a345fc091fede6bfa5ddbc906e0b99865e078b65a870b5` |
| Feng–Xiong–Li，[arXiv:2607.19059v1](https://arxiv.org/abs/2607.19059v1) | `a5453248cd19b59539fd725c2ae4c0480207e68e87d6e7d611dbb8343df25295` |

FL2025 核对范围为原 Rules 1–5、派生 Rules 6–10、Lemmas 1–5、Proposition 3 和 Theorem 2。FXL2026 实现无辅助位、无垃圾输出的 Section III / Algorithms 1–4。

各方法与代码的对应见[算法说明](algorithms.md)，验证入口和结论边界见[验证说明](verification.md)。
