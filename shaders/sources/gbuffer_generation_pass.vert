#version 460

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

layout (set = 0, binding = 0) uniform SceneTransformationBuffer
{
	mat4 ViewMatrix;
	mat4 ProjectionMatrix;
};

layout (location = 0) out vec3 VertexPosition;
layout (location = 1) out vec3 VertexNormal;

void main()
{
	gl_Position = ProjectionMatrix * ViewMatrix * vec4(inPosition, 1.0f);
	VertexPosition = inPosition;
	VertexNormal = inNormal;
}