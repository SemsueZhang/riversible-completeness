import csv,json,statistics,hashlib
from collections import Counter
from pathlib import Path
base=Path(__file__).resolve().parent;root=base.parents[1]
rows=list(csv.DictReader((base/'raw.csv').open()))
assert len(rows)==126
old={(r['id'],r['algorithm']):r for r in csv.DictReader((root/'results/fl-fxl-binary-20261022/effective.csv').open())}
pairs={}
matched=[]
for r in rows:
    pairs.setdefault((int(r['wires']),r['id']),{})[r['algorithm']]=r
    prev=old[(r['id'],r['algorithm'])]
    if r['status']==prev['status']=='ok':
        assert r['rule_count']==prev['rule_count'],(r,prev)
        matched.append(dict(wires=int(r['wires']),id=r['id'],algorithm=r['algorithm'],rule_count=r['rule_count'],old_time_ms=float(prev['time_ms']),new_time_ms=float(r['time_ms']),speedup=float(prev['time_ms'])/float(r['time_ms'])))
summary=[]
for n in range(4,8):
    ps=[p for (w,_),p in pairs.items() if w==n]
    good=[p for p in ps if all(x['status']=='ok' for x in p.values())]
    s=dict(wires=n,total=len(ps),paired_ok=len(good))
    for method in ('fl2025','fxl2026'):
        s[method+'_status']=dict(Counter(p[method]['error'].split(': ')[-1] if p[method]['status']=='resource_limit' else p[method]['status'] for p in ps))
    for k in ('rule_count','time_ms'):
        s[k+'_mean_ratio']=statistics.mean(float(p['fl2025'][k])/float(p['fxl2026'][k]) for p in good) if good else None
        for method in ('fl2025','fxl2026'):
            s[method+'_'+k+'_mean']=statistics.mean(float(p[method][k]) for p in good) if good else None
        s[k+'_ratio_of_means']=s['fl2025_'+k+'_mean']/s['fxl2026_'+k+'_mean'] if good else None
    summary.append(s)
for filename,rs in [('summary.csv',summary),('before-after.csv',matched)]:
    with (base/filename).open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rs[0]));w.writeheader();w.writerows(rs)
(base/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
cfg=json.loads((base/'config.json').read_text())
assert 'end_local' in cfg
for name,digest in cfg['source_sha256'].items():assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest,name
assert hashlib.sha256((root/'build/bench_normalize').read_bytes()).hexdigest()==cfg['binary_sha256']
fmt=lambda x:'—' if x is None else f'{x:.6f}'
t='''# FL / FXL 优化后重测

2026-10-22，C++17 / `-O2`，对当前 63 个公开二值电路执行范式归约。单线程、逐项串行、每项每算法一次，无预热，算法先后顺序按电路交替。未执行相等判定性能测试。

参数：`--seconds 300 --max-states 65536 --max-word 2000000 --max-work 200000000`；外部进程保护 330 秒。时间统计为算法内部耗时，不含文件读取和进程启动。未存储逐步推导历史，局部模板缓存有明确上限；规则总数未以宏调用数替代。

## 每个线路组的平均比率

比率定义：对同一电路计算 FL/FXL，再在两者均成功的配对样本上取算术平均。失败样本不参与，失败前耗时不替代完整归约耗时。

| 线路数 | 成功配对 / 总数 | 规则数 FL/FXL 平均比率 | 时间 FL/FXL 平均比率 |
|---|---:|---:|---:|
'''
for s in summary:t+=f"| {s['wires']} | {s['paired_ok']}/{s['total']} | {fmt(s['rule_count_mean_ratio'])} | {fmt(s['time_ms_mean_ratio'])} |\n"
t+='\n## 完成情况\n\n| 线路数 | FL | FXL |\n|---|---|---|\n'
for s in summary:t+=f"| {s['wires']} | {s['fl2025_status']} | {s['fxl2026_status']} |\n"
t+='\n`max_word` / `contextual peak word` 均表示中间字长（含局部证明放入当前上下文的峰值）达到上限，不是时间耗尽。`max_work` 是内部构造工作量上限，不是规则总次数上限。资源未完成不表示算法理论上不能完成，也不表示结果不相等。\n'
t+='\n## 配对样本上的均值与均值之比\n\n| 线路数 | FL 平均规则数 | FXL 平均规则数 | FL 平均毫秒 | FXL 平均毫秒 | 规则数均值之比 | 时间均值之比 |\n|---|---:|---:|---:|---:|---:|---:|\n'
for s in summary:
    keys=['fl2025_rule_count_mean','fxl2026_rule_count_mean','fl2025_time_ms_mean','fxl2026_time_ms_mean','rule_count_ratio_of_means','time_ms_ratio_of_means']
    t+=f"| {s['wires']} | "+' | '.join(fmt(s[k]) for k in keys)+' |\n'
t+=f'\n## 优化前后核对\n\n旧版与新版均成功的 **{len(matched)} 条算法运行**，基本规则总数逐条完全一致。以下速度比为旧时间 / 新时间，在相同电路上分别求比率后取算术平均；旧版数据中 6 线路部分来自工作量提高后的重试。它是两次实际运行的比较，没有多次重复、置信区间或硬件隔离保证。\n\n| 线路数 | 方法 | 可比较数量 | 旧/新时间平均比率 |\n|---|---|---:|---:|\n'
for n in range(4,8):
    for method in ('fl2025','fxl2026'):
        group=[r for r in matched if r['wires']==n and r['algorithm']==method]
        if group:t+=f"| {n} | {method} | {len(group)} | {statistics.mean(r['speedup'] for r in group):.3f} |\n"
t+='''
本次与旧报告的配对样本集不同，不能直接把两份跨算法平均比率的变化归因为算法优劣变化。4、5 线路的配对集完整；其他组按表中的实际成功范围解释。

## 实现与验证

[原文核对和优化说明](../../docs/optimization-audit.md) 列出所依据的论文条款、确定性选择、消除的额外复杂度与空间处理。基准实现未使用真值表捷径。

- 242 个基础电路的 484 条规则总数和范式摘要与修改前完全一致；基础套件通过 AddressSanitizer / UndefinedBehaviorSanitizer。
- 扩展共 257 个电路，通过完整二值状态枚举、独立按位模拟和规范字比较。
- 11 线路、150,000 门的长抵消字通过稀疏位置索引分支验证，两种方法均准确计 75,000 次取消。
- FL 的坐标选择对固定 Gray 路径上的 1～7 线路全部 769 个全控制门通过组合前提检查。
- 这些检查不等于全部实例的完整基本规则证书回放；未对任意 n 的所有输入给出验证结论。

原始数据：[raw.csv](raw.csv)；汇总：[summary.csv](summary.csv)；同实例优化前后比较：[before-after.csv](before-after.csv)。[config.json](config.json) 保存源码和二进制哈希、平台及参数；运行脚本为 [run.py](run.py)，汇总脚本为 [summarize.py](summarize.py)。算法和验证入口为 C++，Python 仅调度进程、汇总结果及检查坐标选择。
'''
(base/'report.md').write_text(t)
print(json.dumps(summary,ensure_ascii=False,indent=2))
print('Identical old/new successful counts:',len(matched))
