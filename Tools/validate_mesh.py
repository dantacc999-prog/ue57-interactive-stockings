"""Inspect a current-frame GetAcceptedValidationMesh JSON; no model files needed."""
import argparse,json
from pathlib import Path
import numpy as np

def sat_gap(a,b):
 ea=np.roll(a,-1,axis=1)-a;eb=np.roll(b,-1,axis=1)-b
 na=np.cross(ea[:,0],ea[:,1]);nb=np.cross(eb[:,0],eb[:,1])
 axes=np.concatenate([na[:,None],nb[:,None],np.cross(na[:,None],ea),np.cross(nb[:,None],eb),np.cross(ea[:,:,None],eb[:,None,:]).reshape(-1,9,3)],axis=1)
 length=np.linalg.norm(axes,axis=2);valid=length>1e-12
 axes=np.divide(axes,length[:,:,None],out=np.zeros_like(axes),where=valid[:,:,None])
 pa=np.einsum('pvc,pac->pva',a,axes);pb=np.einsum('pvc,pac->pva',b,axes)
 gaps=np.maximum(pb.min(1)-pa.max(1),pa.min(1)-pb.max(1));gaps[~valid]=-np.inf
 return gaps.max(1)

def inspect(d):
 p=np.asarray(d['positions_cm']);tri=p[np.asarray(d['triangles']).reshape(-1,3)]
 swim=np.asarray(d['swim_positions_cm']);ix=np.asarray(d['swim_triangles'],dtype=int).reshape(-1,3);swim=swim[ix]
 count=0;candidate_count=0;minimum=None
 if len(swim):
  lo,hi=tri.min(1),tri.max(1);slo,shi=swim.min(1),swim.max(1);pairs=[]
  for i in np.flatnonzero(((lo<=shi.max(0))&(hi>=slo.min(0))).all(1)):
   ids=np.flatnonzero(((lo[i]<=shi)&(slo<=hi[i])).all(1));pairs.extend((int(i),int(j)) for j in ids)
  pairs=np.asarray(pairs,dtype=int).reshape(-1,2);candidate_count=len(pairs)
  for k in range(0,len(pairs),10000):
   q=pairs[k:k+10000];gap=sat_gap(tri[q[:,0]],swim[q[:,1]]);count+=int((gap<-1e-6).sum());g=float(gap.min());minimum=g if minimum is None else min(minimum,g)
 groups=np.asarray(d['seam_groups']);error=0.
 order=np.argsort(groups);splits=np.flatnonzero(np.diff(groups[order]))+1
 for ids in np.split(order,splits):error=max(error,float(np.linalg.norm(p[ids]-p[ids[0]],axis=1).max()))
 density=np.asarray(d['fiber_density']);render=np.asarray(d['render_fiber_density']);density_error=float(abs(density-render).max()) if len(render)==len(density) else None
 return {'swim_faces':len(swim),'candidate_pairs':candidate_count,'crossing_pairs':count,'minimum_candidate_sat_gap_cm':minimum,'max_seam_error_cm':error,'render_density_error':density_error,'render_density_quantization_step':2.4/255}

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('mesh',type=Path);args=parser.parse_args();print(json.dumps(inspect(json.loads(args.mesh.read_text(encoding='utf-8-sig'))),indent=2))
