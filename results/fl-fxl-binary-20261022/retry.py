import csv,json,subprocess,time
from pathlib import Path
base=Path(__file__).resolve().parent;root=base.parents[1]
rows=list(csv.DictReader((base/'raw.csv').open()))
selected=[r for r in rows if r['error'].endswith('resource_limit: max_work')]
config={'reason':'Retry max_work failures with 10x construction budget; same binary and all other limits.','max_work':20000000,'seconds':60,'max_word':200000,'max_states':65536,'outer_timeout_seconds':75,'attempts':len(selected),'start_local':time.strftime('%Y-%m-%d %H:%M:%S %z')}
(base/'retry-config.json').write_text(json.dumps(config,indent=2)+'\n')
with (base/'retry.csv').open('x',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();f.flush()
    for i,old in enumerate(selected):
        cmd=[str(root/'build/bench_normalize'),'--algorithm',old['algorithm'],'--input',old['input_a'],'--seconds','60','--max-states','65536','--max-word','200000','--max-work','20000000']
        try:
            r=subprocess.run(cmd,cwd=root,text=True,capture_output=True,timeout=75)
            got=list(csv.DictReader(r.stdout.splitlines()))
            row=got[0] if len(got)==1 else {'algorithm':old['algorithm'],'input_a':old['input_a'],'status':'process_error'}
            row['error']=r.stderr.strip()
        except subprocess.TimeoutExpired:
            row={'algorithm':old['algorithm'],'input_a':old['input_a'],'status':'external_timeout','error':'Process exceeded 75 seconds; terminated.'}
        row.update({k:old[k] for k in ('wires','gates','id')});w.writerow(row);f.flush()
        print(f"{i+1}/{len(selected)} n={row['wires']} {row['id']} {row['algorithm']}: {row['status']} {row.get('time_ms','')} ms {row['error'].split(': ')[-1]}",flush=True)
config['end_local']=time.strftime('%Y-%m-%d %H:%M:%S %z');(base/'retry-config.json').write_text(json.dumps(config,indent=2)+'\n')
