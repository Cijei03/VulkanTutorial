#version 460

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;

layout (location = 0) out vec4 PixelPosition;
layout (location = 1) out vec4 PixelNormal;

void main()
{
	PixelPosition = vec4(VertexPosition, 0.0f);
	PixelNormal = vec4(VertexNormal, 0.0f);
}