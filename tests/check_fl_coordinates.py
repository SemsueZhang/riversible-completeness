"""Check the FL coordinate-pair schedule on every binary edge for n <= 7.
This checks the combinatorial preconditions, not rule counts or full derivations.
"""
import json
from pathlib import Path

def gray(n):
    if not n: return [0]
    p=gray(n-1)
    return p+[x|(1<<(n-1)) for x in reversed(p)]

def simple(seq,start,end):
    seen={start}
    for t in seq:
        start ^= 1<<t
        if start in seen:return False
        seen.add(start)
    return start==end

result=[]
for n in range(1,8):
    path=gray(n); rank={x:i for i,x in enumerate(path)}
    count=0; deletions=0
    for x in range(1<<n):
        for t in range(n):
            y=x^(1<<t)
            if rank[x]>=rank[y]:continue
            seq=[(path[k]^path[k+1]).bit_length()-1 for k in range(rank[x],rank[y])]
            while len(seq)>1:
                best=0; choice=None
                for left in range(len(seq)):
                    used={seq[left]}
                    for right in range(left+1,len(seq)):
                        if seq[right]==seq[left]:
                            candidate=seq[:left]+seq[left+1:right]+seq[right+1:]
                            width=right-left-1
                            if width>best and simple(candidate,x,y):
                                best=width;choice=candidate
                            break
                        if seq[right] in used:break
                        used.add(seq[right])
                assert choice is not None,(n,x,y,seq)
                seq=choice;deletions+=1
            assert seq==[t]
            count+=1
    result.append(dict(wires=n,full_control_gates=count,coordinate_deletions=deletions,status='ok'))
print(json.dumps(result,indent=2))
