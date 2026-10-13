// Fine woven gauze: rounded warp/weft filaments and actual skin-visible pores.
float3 V=normalize(ViewDirection);
float3 Ng=normalize(MacroNormal);
Ng*=dot(Ng,V)<0.?-1.:1.;
float3 U=normalize(FiberTangentWS-Ng*dot(FiberTangentWS,Ng)+.000001);
float3 W=normalize(cross(Ng,U));
float density=clamp(FabricDensity*2.4,.62,2.4);
float2 q=FabricUV*float2(MeshColumns,MeshRows);
q+=float2(.035*sin(q.y*6.283185),.025*sin(q.x*6.283185));
float2 footprint=max(fwidth(q),.0001);
float resolve=1.-smoothstep(.35,1.1,max(footprint.x,footprint.y));
float2 cell=frac(q)-.5;
float radius=clamp(MeshRadius*sqrt(density),.08,.34);
float2 yarn=1.-smoothstep(radius-footprint*.5,radius+footprint*.5,abs(cell));
float meanCoverage=1.-pow(1.-2.*radius,2.);
float mask=lerp(meanCoverage,1.-(1.-yarn.x)*(1.-yarn.y),resolve);
// Alternating over/under crossings, with a cylindrical filament cross-section.
float over=frac((floor(q.x)+floor(q.y))*.5)*2.;
float wu=yarn.x*(1.-yarn.y)+yarn.x*yarn.y*over;
float wv=yarn.y*(1.-yarn.x)+yarn.x*yarn.y*(1.-over);
float2 slope=clamp(cell/max(radius,.01),-.85,.85);
float3 N=normalize(Ng+MeshSlope*resolve*(U*slope.x*wu+W*slope.y*wv));
float localDensity=max(.35,1.-SkinControls.r*RevealStrength)*(1.+SkinControls.b*ReinforceStrength);
float edge=pow(1.-saturate(dot(Ng,V)),max(.1,EdgePower));
float opticalDensity=lerp(.24,.38,saturate(Density));
float depth=opticalDensity*Thickness*localDensity*density*SheerDensityScale*(1.+edge*EdgeThickness);
// Fiber amount is distributed over the strands, rather than tinting the holes.
// A bundle contains finer unresolved filaments as well as the visible yarn.
// Keep their optical veil, while the resolved openings transmit more skin.
float fiberAmount=lerp(1.,mask/max(meanCoverage,.1),MeshContrast);
float coverage=1.-exp(-max(0.,depth)*fiberAmount);
// Hide only the low toe cap enclosed by the shoe; barefoot mode restores it.
float shoeToe=(1.-smoothstep(3.1,3.5,ToePosition.z))*smoothstep(10.5,12.,ToePosition.y);
coverage*=1.-saturate(ShoesWorn)*shoeToe;
float3 L=normalize(KeyPosition+ActorOrigin-WorldPosition);
float3 Lf=normalize(FillPosition+ActorOrigin-WorldPosition);
float3 H=normalize(L+V+.000001);
float wrapped=saturate(dot(N,L)*.5+.5);
float ramp=smoothstep(RampCenter-max(.005,RampSoftness),RampCenter+max(.005,RampSoftness),wrapped);
float3 light=AmbientStrength+KeyStrength*lerp(ShadowTint,KeyTint,ramp)+FillStrength*saturate(dot(N,Lf))*FillTint;
float rough=clamp(Roughness+(WeaveRoughness-.5)*.15,.45,.85);
float NoH=saturate(dot(N,H));
float NoL=saturate(dot(N,L));
float spec=pow(NoH,lerp(100.,18.,rough))*SmoothSpecular;
float su=pow(sqrt(saturate(1.-pow(dot(U,H),2.))),FiberSharpness);
float sv=pow(sqrt(saturate(1.-pow(dot(W,H),2.))),FiberSharpness);
float fiber=(su*wu+sv*wv)/max(wu+wv,.001);
// At a distance, resolve to a restrained average fiber sheen.
fiber=lerp((su+sv)*.5,fiber,resolve);
spec+=fiber*FiberSpecular*smoothstep(.15,.75,NoH);
float fuzz=pow(saturate(1.-dot(V,H)),3.)*FuzzStrength*(.3+.7*wrapped);
float3 sheen=SpecularTint*KeyTint*(spec*NoL+fuzz);
float3 result=FabricTint*light+sheen;
if(DebugView>.5&&DebugView<1.5)result=coverage.xxx;
if(DebugView>1.5&&DebugView<2.5)result=mask.xxx;
if(DebugView>2.5&&DebugView<3.5)result=(N*.5+.5);
if(DebugView>3.5&&DebugView<4.5)result=sheen;
// Opacity already masks yarn highlights; open pores do not carry a skin copy.
return float4(max(0.,result)*OutputScale,clamp(coverage,0.,.99));
