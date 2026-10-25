import csv,hashlib,json,platform,subprocess,time
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
base=Path(__file__).resolve().parent;root=base.parents[1]
stamp=lambda:datetime.now(ZoneInfo('Asia/Shanghai')).isoformat()
hashfile=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest=root/'data/public_3to6/manifest.json'
cs=json.loads(manifest.read_text())['circuits']
assert len(cs)==10 and not (base/'raw.csv').exists() and not (base/'config.json').exists()
cfg=dict(date='2026-10-25',task='binary_normalization',algorithms=['fl2025','fxl2026'],repetitions=3,seconds=300,max_states=65536,max_word=100000000,max_work=200000000,outer_timeout_seconds=330,order='serial; alternate method order by circuit and repetition; fresh process/cache each run; no warmup',platform=platform.platform(),machine=platform.machine(),binary_sha256=hashfile(root/'build/bench_normalize'),source_sha256={str(p.relative_to(root)):hashfile(p) for d in ('src','include','apps') for p in sorted((root/d).rglob('*')) if p.is_file()},manifest_sha256=hashfile(manifest),start_local=stamp())
(base/'config.json').write_text(json.dumps(cfg,ensure_ascii=False,indent=2)+'\n')
fields=['wires','gates','id','algorithm','input_a','input_b','repeat','status','equal','rule_count','time_ms','error']
with (base/'raw.csv').open('x',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader();f.flush()
 for i,c in enumerate(cs):
  assert hashfile(root/c['benchmark_file'])==c['benchmark_sha256']
  for rep in range(3):
   methods=['fl2025','fxl2026'] if (i+rep)%2==0 else ['fxl2026','fl2025']
   for method in methods:
    cmd=[str(root/'build/bench_normalize'),'--algorithm',method,'--input',str(root/c['benchmark_file']),'--seconds','300','--max-states','65536','--max-word','100000000','--max-work','200000000']
    try:
     r=subprocess.run(cmd,cwd=root,capture_output=True,text=True,timeout=330)
     rows=list(csv.DictReader(r.stdout.splitlines()))
     row=rows[0] if len(rows)==1 else dict(algorithm=method,input_a=c['benchmark_file'],status='process_error')
     row['error']=r.stderr.strip() or (f'Process return code {r.returncode}' if r.returncode else '')
    except subprocess.TimeoutExpired:
     row=dict(algorithm=method,input_a=c['benchmark_file'],status='external_timeout',error='Process exceeded 330 seconds; terminated.')
    row.update(wires=c['wires'],gates=c['gates'],id=c['id'],repeat=rep)
    w.writerow(row);f.flush()
    print(f"n={c['wires']} {c['id']} {method} rep={rep+1}: {row['status']} rules={row.get('rule_count','')} ms={row.get('time_ms','')}",flush=True)
    if row['status']!='ok':
     print('Resource failure recorded; no automatic limit escalation.',flush=True)
cfg['end_local']=stamp()
assert cfg['binary_sha256']==hashfile(root/'build/bench_normalize')
for name,h in cfg['source_sha256'].items():assert hashfile(root/name)==h
(base/'config.json').write_text(json.dumps(cfg,ensure_ascii=False,indent=2)+'\n')
