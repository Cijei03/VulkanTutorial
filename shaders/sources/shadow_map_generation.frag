#version 460

layout (location = 0) out vec2 PixelMoments;

void main()
{
	const float PixelDepth = gl_FragCoord.z;

	PixelMoments = vec2
	(
		PixelDepth,
		pow(PixelDepth, 2)
	);
}