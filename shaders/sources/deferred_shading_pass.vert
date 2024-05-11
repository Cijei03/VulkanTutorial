#version 460

void main ()
{
	vec2 VertexPosition[4] = 
	{
		vec2(-1.0f, 1.0f),
		vec2(-1.0f, -1.0f),
		vec2(1.0f, 1.0f),
		vec2(1.0f, -1.0f)
	};

	gl_Position = vec4(VertexPosition[gl_VertexID], 0.0f, 1.0f);
}