// Custom node returns float3. Connect your own Original toon radiance and skin albedo.
// ComponentOriginWS must be the component's local (0,0,0) transformed to World.
float3 key=ComponentOriginWS+float3(-95.,130.,160.);
float whiteNdl=saturate(dot(normalize(NormalWS),normalize(key-PositionWS)));
float wrapped=smoothstep(-1.,1.,dot(normalize(NormalWS),normalize(LightDirection)));
float3 blackSkin=SkinAlbedo*SkinColor*lerp(.68,1.,wrapped);
float3 whiteSkin=SkinAlbedo*SkinColor*float3(.92,.79,.75)*(.35+.65*whiteNdl);
float covered=CoveredUV*(1.-smoothstep(SkinCoverageTopZWS-.5,SkinCoverageTopZWS+1.5,PositionWS.z));
float worn=CoveredUV*(1.-smoothstep(72.,73.,RestPosition.z));
covered=lerp(covered,worn,saturate(StaticCoverage));
return lerp(Original,lerp(blackSkin,whiteSkin,saturate(WhiteVariant)),covered);
