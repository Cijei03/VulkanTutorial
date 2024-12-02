#version 460

layout (set = 0, binding = 0) uniform LightSpaceData
{
	mat4 LightSpaceView;
	mat4 LightSpaceProjection;
	vec3 LightSpaceDirection;
};

layout (set = 0, binding = 1) uniform CameraData
{
	vec3 ViewPos;
};

layout (set = 0, binding = 2, input_attachment_index = 0) uniform subpassInput ScenePositionBuffer;
layout (set = 0, binding = 3, input_attachment_index = 1) uniform subpassInput SceneNormalBuffer;

layout (set = 0, binding = 4) uniform sampler2D VarianceShadowMapping;

layout (location = 0) out vec4 PixelColor;

float ComputeDiffuseLightFactor(const vec3 PixelNormal)
{
	return max(dot(PixelNormal, normalize(LightSpaceDirection)), 0);
}

float ComputeSpecularLightFactor(const vec3 PixelNormal)
{
	
	const float LightFactor = max(dot(PixelNormal, normalize(LightSpaceDirection)), 0);
	return pow(LightFactor, 4);
}

float linstep(float Min, float Max, float V)
{
    return clamp((V - Min) / (Max - Min), 0, 1);
}
float ReduceLightBleeding(float p_max, float Amount) 
{   
    // Remove the [0, Amount] tail and linearly rescale (Amount, 1].    
    return linstep(Amount, 1, p_max); 
}
float ChebyshevUpperBound(vec2 Moments, float t, float MinVariance, float ReduceLightBleedingFactor)
{   
    // One-tailed inequality valid if t > Moments.x    
    bool p = (t <= Moments.x);
    
    // Compute variance.
    float Variance = Moments.y - pow(Moments.x, 2);
    Variance = max(Variance, MinVariance);
    // Compute probabilistic upper bound.
     
    float d = t - Moments.x;
    float p_max = Variance / (Variance + pow(d, 2));   
    return ReduceLightBleeding(max(float(p), p_max), ReduceLightBleedingFactor);
}

float ComputeShadowFactor(const vec3 PixelPosition)
{
	vec4 LightSpacePixelPos = LightSpaceProjection * LightSpaceView * vec4(PixelPosition, 1.0f);
	LightSpacePixelPos.xyz /= LightSpacePixelPos.w;

	LightSpacePixelPos.xyz = LightSpacePixelPos.xyz * 0.5f + 0.5f;

	const vec2 Moments = texture(VarianceShadowMapping, LightSpacePixelPos.xy).rg;
	//if (LightSpacePixelPos.z > Moments.x )
	//	return 1.0f;
	//return 0.0;
	return ChebyshevUpperBound(Moments, LightSpacePixelPos.z, 0.0004, 0.98);
}


void main()
{
	// Load data from GBuffer.
	const vec3 PixelPosition = subpassLoad(ScenePositionBuffer).rgb;
	const vec3 PixelNormal = normalize(subpassLoad(SceneNormalBuffer).rgb);
	const vec3 PixelViewDirection = normalize(ViewPos - PixelPosition);

	// Compute light components.
	const float AmbientLightFactor = 0.05f;
	const float DiffuseLightFactor = ComputeDiffuseLightFactor(PixelNormal) * 2.0f;
	const float SpecularLightFactor = ComputeSpecularLightFactor(PixelNormal) * 2.0f;
	const float ShadowFactor = ComputeShadowFactor(PixelPosition);

	const vec3 PixelMaterialColor = vec3(1.0f);

	// Compute lighted color.
	const vec3 PixelAmbientLightColor = PixelMaterialColor * AmbientLightFactor;
	const vec3 PixelDiffuseLightColor = PixelMaterialColor * DiffuseLightFactor + PixelMaterialColor * SpecularLightFactor;

	const vec3 FinallyLightedPixelColor = PixelAmbientLightColor + (PixelDiffuseLightColor * ShadowFactor);


	// Map HDR into SDR.
	const float Gamma = 2.2;
	const float Exposure = 0.7;
  
    // exposure tone mapping
    vec3 Mapped = vec3(1.0) - exp(-FinallyLightedPixelColor * Exposure);
    // gamma correction 
    Mapped = pow(Mapped, vec3(1.0 / Gamma));
  
	PixelColor = vec4(Mapped, 1.0f);
}