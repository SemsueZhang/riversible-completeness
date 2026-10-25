import csv,json,hashlib,statistics
from collections import Counter
from pathlib import Path
b=Path(__file__).resolve().parent;root=b.parents[1]
cfg=json.loads((b/'config.json').read_text());assert 'end_local' in cfg
rows=list(csv.DictReader((b/'raw.csv').open()));assert len(rows)==60
assert all(r['status']=='ok' for r in rows)
assert len({(r['id'],r['algorithm'],r['repeat']) for r in rows})==60
m=root/'data/public_3to6/manifest.json';assert hashlib.sha256(m.read_bytes()).hexdigest()==cfg['manifest_sha256']
cs=json.loads(m.read_text())['circuits']
assert Counter(c['wires'] for c in cs)=={3:4,4:2,5:2,6:2}
old={(r['id'],r['algorithm']):r for r in csv.DictReader((root/'results/fl-fxl-optimized-20261022/raw.csv').open())}
summary=[];compared=0
for c in cs:
 assert hashlib.sha256((root/c['benchmark_file']).read_bytes()).hexdigest()==c['benchmark_sha256']
 s={k:c[k] for k in ('wires','gates','id')}
 for method in ('fl2025','fxl2026'):
  rr=[r for r in rows if r['id']==c['id'] and r['algorithm']==method];assert len(rr)==3
  assert len({r['rule_count'] for r in rr})==1
  s[method+'_rule_count']=int(rr[0]['rule_count'])
  s[method+'_time_ms_mean']=statistics.mean(float(r['time_ms']) for r in rr)
  if (c['id'],method) in old:
   prev=old[(c['id'],method)];assert prev['status']=='ok' and prev['rule_count']==rr[0]['rule_count'];compared+=1
 summary.append(s)
assert compared==12
with (b/'summary.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
(b/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
t='''# 精选电路 FL / FXL 二值范式归约结果

2026-10-25，仅测试 FL2025 和 FXL2026 的二值范式归约；未运行相等判定性能测试。

当前仅保留 10 个真实公开电路：3 线路 4 个，4、5、6 线路各 2 个。已删除原有 57 个未选电路（含全部 7 线路），另从 RevLib 恢复 4 个 3 线路电路。来源及选取理由见[测试集 README](../../data/public_3to6/README.md)。

3 线路为此前至少 10 门要求的例外，没有人为增加门数。4、5 线路按功能差异和门数跨度选取。6 线路按用户明确选择，保留此前双方均可完成的两个 mod5adder 实现：这里存在成功样本选择偏差，只能比较这两种实现，不能代表所有 6 线路功能或推广为整个线路组的平均表现。

## 逐电路结果

每个电路、每种算法独立运行 3 次，全部 60 条成功。三次规则总数逐项相同，表中规则数为单次归约的基本规则总次数，时间为三次算法内部耗时的算术平均（单位毫秒）。串行执行，按电路与重复轮次交替算法顺序，每次使用新进程和新缓存，无单独预热；计时不含文件读取、进程启动和 CSV 输出。小于毫秒的时间仅作本机实测描述，不作统计显著性结论。

| 线路 | 电路 | 输入门数 | FL 规则数 | FL 时间（ms） | FXL 规则数 | FXL 时间（ms） |
|---|---|---:|---:|---:|---:|---:|
'''
for s in summary:t+=f"| {s['wires']} | {s['id']} | {s['gates']} | {s['fl2025_rule_count']:,} | {s['fl2025_time_ms_mean']:.3f} | {s['fxl2026_rule_count']:,} | {s['fxl2026_time_ms_mean']:.3f} |\n"
t+='''
## 参数与核对

C++17 / `-O2`，显式参数为 `--seconds 300 --max-states 65536 --max-word 100000000 --max-work 200000000`，每个进程外部保护 330 秒。没有存储完整推导历史。工作量上限不是基本规则总次数上限。

6 个沿用电路的两种算法共 12 组规则数，与上一轮成功结果逐项相同。4 个恢复的 3 线路原文件与之前下载缓存逐字节一致；10 个输入与 manifest 的 SHA-256 全部核对通过。编译目标已为最新版本；源码与二进制哈希已记录。本次计时结果不等于完整基本规则证书验证；此前算法实现与验证边界见[审计说明](../../docs/optimization-audit.md)。

[全部 60 条原始记录](raw.csv) · [逐电路汇总](summary.csv) · [运行配置及哈希](config.json) · [运行脚本](run.py) · [汇总脚本](summarize.py) · [筛选记录](selection.json)

`previous-dataset-manifest.json` 只保留旧集的来源元数据，未保留已删除电路文件；历史 `results/` 中的旧规模报告不代表当前数据集。
'''
(b/'report.md').write_text(t)
print(t.split('## 参数与核对')[0])
