import csv, hashlib, json, platform, subprocess, time
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path(__file__).resolve().parent
baseline=root/'results/fl-fxl-optimized-20261022/raw.csv'
previous=list(csv.DictReader(baseline.open()))
failed=[r for r in previous if r['status']!='ok']
assert len(failed)==28
assert not (out/'raw.csv').exists() and not (out/'config.json').exists(), 'Existing results: do not overwrite'
config={'task':'normalization_failed_retry','baseline':str(baseline.relative_to(root)),'baseline_sha256':hashlib.sha256(baseline.read_bytes()).hexdigest(),'attempts':len(failed),'selection':'All non-ok records from baseline, no successful records rerun','repetitions':1,'seconds':300,'max_states':65536,'max_word':100000000,'max_work':200000000,'outer_timeout_seconds':330,'platform':platform.platform(),'machine':platform.machine(),'binary_sha256':hashlib.sha256((root/'build/bench_normalize').read_bytes()).hexdigest(),'source_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for d in ('src','include','apps') for p in sorted((root/d).rglob('*')) if p.is_file()},'manifest_sha256':hashlib.sha256((root/'data/public_4to7/manifest.json').read_bytes()).hexdigest(),'start_local':time.strftime('%Y-%m-%d %H:%M:%S %z')}
(out/'config.json').write_text(json.dumps(config,indent=2)+'\n')
fields=['wires','gates','id','algorithm','input_a','input_b','repeat','status','equal','rule_count','time_ms','error']
with (out/'raw.csv').open('x',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=fields); writer.writeheader();f.flush()
    for i,prev in enumerate(failed):
        method=prev['algorithm']
        print(f"START {i+1}/{len(failed)} n={prev['wires']} {prev['id']} {method}",flush=True)
        cmd=[str(root/'build/bench_normalize'),'--algorithm',method,'--input',prev['input_a'],'--seconds','300','--max-states','65536','--max-word','100000000','--max-work','200000000']
        try:
            r=subprocess.run(cmd,cwd=root,text=True,capture_output=True,timeout=330)
            rows=list(csv.DictReader(r.stdout.splitlines()))
            row=rows[0] if len(rows)==1 else {'algorithm':method,'input_a':prev['input_a'],'status':'process_error'}
            row['error']=r.stderr.strip()
            if r.returncode and not row['error']:
                row['error']=f'Process return code {r.returncode}'
        except subprocess.TimeoutExpired:
            row={'algorithm':method,'input_a':prev['input_a'],'status':'external_timeout','error':'Process exceeded 330 seconds; terminated.'}
        row.update(wires=prev['wires'],gates=prev['gates'],id=prev['id']);writer.writerow(row);f.flush()
        print(f"DONE {i+1}/{len(failed)} n={prev['wires']} {prev['id']} {method}: {row['status']} {row.get('time_ms','')} ms {row['error']}",flush=True)
config['end_local']=time.strftime('%Y-%m-%d %H:%M:%S %z')
(out/'config.json').write_text(json.dumps(config,indent=2)+'\n')
