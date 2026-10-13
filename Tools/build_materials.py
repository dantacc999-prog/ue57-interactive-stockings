"""UE 5.7 Editor Python: build standalone gauze parents/instances, with no model assets.

Run with exec(compile(open(PATH, encoding='utf-8').read(), PATH, 'exec')).
Set STOCKINGS_REPO_ROOT in the execution globals when __file__ is unavailable.
"""
import json,re
from pathlib import Path
import unreal

HERE=Path(STOCKINGS_REPO_ROOT) if globals().get('STOCKINGS_REPO_ROOT') else Path(__file__).resolve().parents[1]
DEST='/Game/InteractiveStockings'
E=unreal.MaterialEditingLibrary
AT=unreal.AssetToolsHelpers.get_asset_tools()
PRESETS=json.loads((HERE/'Materials/presets.json').read_text())
def node(m,cls,x=0,y=0):return E.create_material_expression(m,cls,x,y)
def connect(a,b,name='',output=''):
 assert E.connect_material_expressions(a,output,b,name),name
def scalar(m,name,value):
 n=node(m,unreal.MaterialExpressionScalarParameter,-1100,0);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def vector(m,name,value):
 n=node(m,unreal.MaterialExpressionVectorParameter,-1100,300);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',unreal.LinearColor(*value));return n
def mask(m,source,rgba):
 n=node(m,unreal.MaterialExpressionComponentMask,500,0)
 for k in 'rgba':n.set_editor_property(k,k in rgba)
 connect(source,n);return n
def material(name):
 if unreal.load_asset(DEST+'/'+name):raise RuntimeError('Asset already exists: '+name+'; choose a new DEST for regeneration')
 m=AT.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew());m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);return m
def build(variant,ordinary):
 suffix=variant.title()+('_Ordinary' if ordinary else '_Interactive');m=material('M_'+suffix)
 code=(HERE/('Materials/'+variant+'_gauze.hlsl')).read_text()
 if ordinary:
  code=code.replace('float density=clamp(FabricDensity*2.4,.62,2.4);',
   'float referenceArea=length(cross(ddx(ToePosition),ddy(ToePosition)));\n'
   'float deformedArea=length(cross(ddx(WorldPosition),ddy(WorldPosition)));\n'
   'float poseDensity=clamp(referenceArea/max(deformedArea,1.e-6),.62,2.4);\n'
   'float density=clamp(FabricDensity*2.4*poseDensity,.62,2.4);' if variant=='white' else
   'float density=clamp(FabricDensity*2.4,.62,2.4);')
 p=PRESETS[variant];c=node(m,unreal.MaterialExpressionCustom,200,0);c.set_editor_property('code',code);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
 used=[k for k in p['custom_inputs'] if re.search(r'\b'+re.escape(k)+r'\b',code)]
 inputs=[]
 for k in used:
  i=unreal.CustomInput();i.set_editor_property('input_name',k);inputs.append(i)
 c.set_editor_property('inputs',inputs)
 shared={
  'MacroNormal':node(m,unreal.MaterialExpressionVertexNormalWS),
  'FiberTangentWS':node(m,unreal.MaterialExpressionVertexTangentWS),
  'ViewDirection':node(m,unreal.MaterialExpressionCameraVectorWS),
  'WorldPosition':node(m,unreal.MaterialExpressionWorldPosition),
  'FabricUV':node(m,unreal.MaterialExpressionTextureCoordinate),
  'Density':scalar(m,'OpticalDensity',.30 if variant=='white' else .22),
  'WeaveRoughness':scalar(m,'WeaveRoughness',.5),
  'SkinControls':vector(m,'SkinControls',[0,0,0,1]),
 }
 if ordinary:shared['FabricDensity']=scalar(m,'WornFiberDensity',1/2.4)
 else:shared['FabricDensity']=mask(m,node(m,unreal.MaterialExpressionVertexColor),'r')
 rest=node(m,unreal.MaterialExpressionPreSkinnedPosition);interp=node(m,unreal.MaterialExpressionVertexInterpolator);connect(rest,interp,'VS');shared['ToePosition']=interp
 zero=node(m,unreal.MaterialExpressionConstant3Vector);zero.set_editor_property('constant',unreal.LinearColor(0,0,0,1))
 tr=node(m,unreal.MaterialExpressionTransformPosition);tr.set_editor_property('transform_source_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL);tr.set_editor_property('transform_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD);connect(zero,tr)
 height=node(m,unreal.MaterialExpressionConstant3Vector);height.set_editor_property('constant',unreal.LinearColor(0,0,60,1));anchor=node(m,unreal.MaterialExpressionAdd);connect(tr,anchor,'A');connect(height,anchor,'B');shared['ActorOrigin']=anchor
 for k in used:
  if k in shared:n=shared[k];output='PS' if k=='ToePosition' else ''
  elif k in p['scalars']:n=scalar(m,k,p['scalars'][k]);output=''
  elif k in p['vectors']:n=vector(m,k,p['vectors'][k]);output='RGB'
  else:raise RuntimeError('Unknown Custom input '+k)
  connect(n,c,k,output)
 rgb=mask(m,c,'rgb');alpha=mask(m,c,'a');eye=node(m,unreal.MaterialExpressionEyeAdaptationInverse,800,0);connect(rgb,eye,'LightValueInput');one=node(m,unreal.MaterialExpressionConstant);one.set_editor_property('r',1);connect(one,eye,'AlphaInput')
 assert E.connect_material_property(eye,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 assert E.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
 offset=node(m,unreal.MaterialExpressionMultiply);connect(shared['MacroNormal'],offset,'A');connect(scalar(m,'ShellOffsetCm',p['scalars']['ShellOffsetCm']),offset,'B');assert E.connect_material_property(offset,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
 E.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
 mi=AT.create_asset('MI_'+suffix,DEST,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew());E.set_material_instance_parent(mi,m);E.update_material_instance(mi);assert unreal.EditorAssetLibrary.save_loaded_asset(mi,False)
 print('Built',mi.get_path_name())

for variant in ['black','white']:
 for ordinary in [False,True]:build(variant,ordinary)
