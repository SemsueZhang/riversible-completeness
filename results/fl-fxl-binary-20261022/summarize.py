import csv,json,statistics
from collections import Counter
from pathlib import Path
base=Path(__file__).resolve().parent
initial=list(csv.DictReader((base/'raw.csv').open()))
lookup={(r['id'],r['algorithm']):dict(r,work_limit='2000000',attempt_source='raw.csv') for r in initial}
if (base/'retry.csv').exists():
    for r in csv.DictReader((base/'retry.csv').open()):
        lookup[(r['id'],r['algorithm'])]=dict(r,work_limit='20000000',attempt_source='retry.csv')
rows=list(lookup.values())
assert len(rows)==126
with (base/'effective.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
paired={}
for r in rows: paired.setdefault((int(r['wires']),r['id']),{})[r['algorithm']]=r
stats=[]
for n in range(4,8):
    allpairs=[p for (w,_),p in paired.items() if w==n]
    valid=[p for p in allpairs if set(p)=={'fl2025','fxl2026'} and all(r['status']=='ok' for r in p.values())]
    s={'wires':n,'circuits':30 if n==4 else 11,'attempted':len(allpairs),'paired_ok':len(valid)}
    for alg in ('fl2025','fxl2026'):
        s[alg+'_status']=dict(Counter((p[alg]['error'].split(': ')[-1] if p[alg]['status']=='resource_limit' else p[alg]['status']) for p in allpairs if alg in p))
    for key in ('rule_count','time_ms'):
        s[key+'_mean_ratio']=statistics.mean(float(p['fl2025'][key])/float(p['fxl2026'][key]) for p in valid) if valid else None
        for alg in ('fl2025','fxl2026'):
            s[alg+'_'+key+'_mean']=statistics.mean(float(p[alg][key]) for p in valid) if valid else None
        s[key+'_ratio_of_means']=s['fl2025_'+key+'_mean']/s['fxl2026_'+key+'_mean'] if valid else None
    stats.append(s)
(base/'summary.json').write_text(json.dumps(stats,ensure_ascii=False,indent=2)+'\n')
with (base/'summary.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(stats[0]));w.writeheader();w.writerows(stats)
text='''# FL2025 与 FXL2026 二值范式归约测试

当前本地数据集包含 63 个二值电路（4～7 线路，至少 10 门）。首轮逐电路、逐算法串行运行一次，交替算法先后顺序，无预热、无跨样本缓存；仅对内部工作量超限的 38 次运行另行重试一次。有效样本均只有一次成功计时。构建参数为 `-O2 -std=c++17 -Wall -Wextra -Wpedantic`。

主要统计口径：对同一线路组内两者均成功的电路，分别计算 `FL 规则数 / FXL 规则数` 和 `FL 时间 / FXL 时间`，再取算术平均。比率小于 1 表示本次实现中 FL 的指标更小。不是先对各自成功集合求均值，也不以超时时间代替完整耗时。

每个算法每个电路的资源限制：60 秒、65,536 个状态、200,000 字长、首轮 2,000,000 次内部构造工作；工作量超限的运行用 20,000,000 上限重试，其余参数不变。外部进程保护上限 75 秒。未修改算法或源代码。已成功的运行不重试，重试仅替代首轮失败；不累加失败尝试时间。资源限制下的失败不表示理论上不能完成。计时是 C++ 内部 steady_clock 算法耗时，排除文件读取和进程启动。

| 线路数 | 配对成功 / 总数 | 规则数 FL/FXL 平均比率 | 时间 FL/FXL 平均比率 |
|---|---:|---:|---:|
'''
fmt=lambda x:'—' if x is None else f'{x:.6f}'
for s in stats:text+=f"| {s['wires']} | {s['paired_ok']} / {s['circuits']} | {fmt(s['rule_count_mean_ratio'])} | {fmt(s['time_ms_mean_ratio'])} |\n"
text+='\n## 同一配对样本集上的均值\n\n| 线路数 | FL 平均规则数 | FXL 平均规则数 | FL 平均毫秒 | FXL 平均毫秒 | 规则数均值之比 | 时间均值之比 |\n|---|---:|---:|---:|---:|---:|---:|\n'
for s in stats:
    keys=['fl2025_rule_count_mean','fxl2026_rule_count_mean','fl2025_time_ms_mean','fxl2026_time_ms_mean','rule_count_ratio_of_means','time_ms_ratio_of_means']
    text+=f"| {s['wires']} | "+' | '.join(fmt(s[k]) for k in keys)+' |\n'
text+='\n## 完成情况\n\n| 线路数 | FL 状态 | FXL 状态 |\n|---|---|---|\n'
for s in stats:text+=f"| {s['wires']} | {s['fl2025_status']} | {s['fxl2026_status']} |\n"
text+='''
规则次数是各自论文规则系统、当前确定性调度中的基本规则总次数，两套规则的单步表达能力不同。`ok` 表示本次实现完成；此次未额外执行完整证书回放或独立语义验证，不能把性能结果当作全算法正确性证明。每个有效配对各有一次成功测量，时间比率属于单次测量。配对成功仅 1 个的组，其值是该单个电路的比率，不能解释为整组平均性能；没有配对成功的组不提供数值。

首轮逐项结果及失败原因见 [raw.csv](raw.csv)，重试见 [retry.csv](retry.csv)，合并后的每项最终记录见 [effective.csv](effective.csv)；完整配置、二进制和源代码哈希见 [config.json](config.json)；机器可读汇总见 [summary.csv](summary.csv) 和 [summary.json](summary.json)。运行脚本为 [run.py](run.py) 和 [retry.py](retry.py)，重试配置见 [retry-config.json](retry-config.json)，聚合脚本为 [summarize.py](summarize.py)，测试算法本身均为 C++。
'''
(base/'report.md').write_text(text)
print(json.dumps(stats,ensure_ascii=False,indent=2))
