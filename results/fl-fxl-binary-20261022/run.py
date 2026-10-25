import csv, hashlib, json, platform, subprocess, time
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path(__file__).resolve().parent
meta=json.loads((root/'data/public_4to7/manifest.json').read_text())
config={'task':'normalization','repetitions':1,'seconds':60,'max_states':65536,'max_word':200000,'max_work':2000000,'outer_timeout_seconds':75,'platform':platform.platform(),'machine':platform.machine(),'binary_sha256':hashlib.sha256((root/'build/bench_normalize').read_bytes()).hexdigest(),'source_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for d in ('src','include','apps') for p in sorted((root/d).rglob('*')) if p.is_file()},'manifest_sha256':hashlib.sha256((root/'data/public_4to7/manifest.json').read_bytes()).hexdigest(),'start_local':time.strftime('%Y-%m-%d %H:%M:%S %z')}
(out/'config.json').write_text(json.dumps(config,indent=2)+'\n')
fields=['wires','gates','id','algorithm','input_a','input_b','repeat','status','equal','rule_count','time_ms','error']
with (out/'raw.csv').open('x',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=fields); writer.writeheader();f.flush()
    for i,c in enumerate(meta['circuits']):
        methods=['fl2025','fxl2026'] if i%2==0 else ['fxl2026','fl2025']
        for method in methods:
            cmd=[str(root/'build/bench_normalize'),'--algorithm',method,'--input',str(root/c['benchmark_file']),'--seconds','60','--max-states','65536','--max-word','200000','--max-work','2000000']
            try:
                r=subprocess.run(cmd,cwd=root,text=True,capture_output=True,timeout=75)
                rows=list(csv.DictReader(r.stdout.splitlines()))
                row=rows[0] if len(rows)==1 else {'algorithm':method,'input_a':c['benchmark_file'],'status':'process_error'}
                row['error']=r.stderr.strip()
            except subprocess.TimeoutExpired:
                row={'algorithm':method,'input_a':c['benchmark_file'],'status':'external_timeout','error':'Process exceeded 75 seconds; terminated.'}
            row.update(wires=c['wires'],gates=c['gates'],id=c['id']);writer.writerow(row);f.flush()
            print(f"{i+1}/63 n={c['wires']} {c['id']} {method}: {row['status']} {row.get('time_ms','')} ms {row['error'][:100]}",flush=True)
config['end_local']=time.strftime('%Y-%m-%d %H:%M:%S %z')
(out/'config.json').write_text(json.dumps(config,indent=2)+'\n')
