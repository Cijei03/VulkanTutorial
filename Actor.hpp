#pragma once
#include <vector>

#include <vulkan/vulkan.h>

struct SceneActor
{
	std::vector<VkBuffer> VertexBuffers = std::vector<VkBuffer>(2);
	const std::vector<VkDeviceSize> Offsets = std::vector<VkDeviceSize>(2, 0);
	VkDeviceMemory ActorBuffersGPUMemory;
	size_t VerticesCount = 0;
	enum BufferType
	{
		Position,
		Normal
	};
};