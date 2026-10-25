import csv, hashlib, json, statistics
from collections import Counter
from pathlib import Path
base=Path(__file__).resolve().parent
root=base.parents[1]
cfg=json.loads((base/'config.json').read_text())
assert 'end_local' in cfg, 'Run is incomplete'
assert hashlib.sha256((root/cfg['baseline']).read_bytes()).hexdigest()==cfg['baseline_sha256']
for name,digest in cfg['source_sha256'].items():
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest, name
assert hashlib.sha256((root/'build/bench_normalize').read_bytes()).hexdigest()==cfg['binary_sha256']
old=list(csv.DictReader((root/cfg['baseline']).open()))
new=list(csv.DictReader((base/'raw.csv').open()))
key=lambda r:(r['id'],r['algorithm'])
retries={key(r):r for r in new}
assert len(new)==len(retries)==cfg['completed_attempts']
assert set(retries)<={key(r) for r in old if r['status']!='ok'}
assert {key(r) for r in old if r['wires']=='6' and r['status']!='ok'} <= set(retries)
rows=[]
for r in old:
    retry=key(r) in retries
    row=dict(retries[key(r)] if retry else r)
    row['source_run']='fl-fxl-word100m-20261022' if retry else 'fl-fxl-optimized-20261022'
    row['max_word']='100000000' if retry else '2000000'
    rows.append(row)
with (base/'effective.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
reason=lambda r:r['error'].split(': ')[-1] if r['status']=='resource_limit' else r['status']
summary=[]
for n in range(4,8):
    group=[r for r in rows if int(r['wires'])==n]
    pairs={}
    for r in group:pairs.setdefault(r['id'],{})[r['algorithm']]=r
    good=[p for p in pairs.values() if all(r['status']=='ok' for r in p.values())]
    s=dict(wires=n,total=len(pairs),paired_ok=len(good))
    for method in ('fl2025','fxl2026'):
        s[method+'_status']=dict(Counter(reason(r) for r in group if r['algorithm']==method))
    for metric in ('rule_count','time_ms'):
        s[metric+'_mean_ratio']=statistics.mean(float(p['fl2025'][metric])/float(p['fxl2026'][metric]) for p in good) if good else None
        s[metric+'_ratio_of_means']=sum(float(p['fl2025'][metric]) for p in good)/sum(float(p['fxl2026'][metric]) for p in good) if good else None
    summary.append(s)
(base/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
with (base/'summary.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
fmt=lambda x:'—' if x is None else f'{x:.6f}'
t='''# FL / FXL 未完成样本重测（字长上限 1 亿）

原计划重测上一轮 28 条非成功记录。用户随后要求停止 n=7：6 线路 FXL 的 8 条已全部重测；7 线路已落盘 13 条，另 1 条被中断、6 条未启动。批次已停止，未重新运行已成功样本。算法为二值范式归约，C++17 / -O2，逐项串行执行，每条一次，无预热。

参数：`--seconds 300 --max-states 65536 --max-word 100000000 --max-work 200000000`；外部进程保护 330 秒。相较上一轮只提高字长上限 50 倍。未保存完整推导历史，但仍保存当前门序列。

## 本轮重测结果

| 线路数 | 算法 | 重测条数 | 本轮状态 |
|---|---|---:|---|
'''
for n in (6,7):
    for method in ('fl2025','fxl2026'):
        g=[r for r in new if int(r['wires'])==n and r['algorithm']==method]
        if g:t+=f'| {n} | {method} | {len(g)} | {dict(Counter(reason(r) for r in g))} |\n'
t+='\n## 合并此前成功结果后的覆盖率与比率\n\n`effective.csv` 每条带来源轮次和字长上限；旧成功记录原样保留，有本轮完整状态记录的旧失败项由本轮记录替换；被取消或尚未启动的项保留上一轮结果，不视为本轮已测。时间比率会混合两轮运行，只是单次实测描述，不是同轮重复测量或统计显著性结论。失败前耗时不作为完成耗时，也不进入比率。规则比率、时间比率均为成功配对上每个电路 FL/FXL 的算术平均。\n\n| 线路数 | 配对成功 / 总数 | FL 状态 | FXL 状态 | 规则数平均比率 | 时间平均比率 |\n|---|---:|---|---|---:|---:|\n'
for s in summary:t+=f"| {s['wires']} | {s['paired_ok']}/{s['total']} | {s['fl2025_status']} | {s['fxl2026_status']} | {fmt(s['rule_count_mean_ratio'])} | {fmt(s['time_ms_mean_ratio'])} |\n"
t+='\n## 本轮逐项记录（仅包含返回状态的 21 条）\n\n| 线路数 | 电路 | 算法 | 状态 / 原因 | 耗时（秒） | 完成时规则总数 |\n|---|---|---|---|---:|---:|\n'
for r in new:
    secs=float(r['time_ms'])/1000 if r.get('time_ms') else None
    t+=f"| {r['wires']} | {r['id']} | {r['algorithm']} | {reason(r)} | {fmt(secs)} | {r['rule_count'] or '—'} |\n"
t+='\n失败行的耗时只表示终止前的运行时间。`seconds` 是算法内 300 秒限制，`external_timeout` 是进程外 330 秒保护；`max_word` / `contextual peak word` 是中间字长限制；`max_work` 是内部构造工作量限制，并非基本规则总次数上限。未完成不代表算法理论上无法完成。\n\n[本轮原始数据](raw.csv) · [合并数据及来源](effective.csv) · [配置与哈希](config.json) · [汇总](summary.csv) · [上一轮报告](../fl-fxl-optimized-20261022/report.md)\n'
(base/'report.md').write_text(t)
print(json.dumps(summary,ensure_ascii=False,indent=2))
