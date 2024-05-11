#version 460

layout (location = 0) in vec3 inVertexPosition;

layout (set = 0, binding = 0) uniform LightSpaceData
{
	mat4 LightSpaceView;
	mat4 LightSpaceProjection;
	vec3 LightSpaceDirection;
};

void main()
{
	gl_Position = LightSpaceProjection * LightSpaceView * vec4(inVertexPosition, 1.0f);
}