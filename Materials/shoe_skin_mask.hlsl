// Multiply your skin opacity by this value. RestPosition is passed through VertexInterpolator.
return 1.-saturate(ShoesWorn)*(1.-smoothstep(3.1,3.5,RestPosition.z));
