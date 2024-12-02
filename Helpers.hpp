#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <vulkan/vulkan.h>

static uint32_t QueryMemoryTypeIndex(VkMemoryPropertyFlagBits PreferredMemoryType, uint32_t RequiredMemoryTypes, const VkPhysicalDeviceMemoryProperties2& DeviceMemoryInfo)
{
	for (uint32_t i = 0; i < DeviceMemoryInfo.memoryProperties.memoryTypeCount; i++)
	{
		if ((RequiredMemoryTypes & (1 << i)) && (PreferredMemoryType & DeviceMemoryInfo.memoryProperties.memoryTypes[i].propertyFlags))
		{
			return i;
		}
	}

	return 0;
}

static VkShaderModule CreateShaderModule(VkDevice Device, const std::string& ShaderFilePath)
{
	std::vector<char> ShaderCodeBytes(std::filesystem::file_size(ShaderFilePath), 0);
	// Load file content.
	{
		std::ifstream ShaderFile(ShaderFilePath, std::ios::binary);

		ShaderFile.read(ShaderCodeBytes.data(), ShaderCodeBytes.size());
	}

	// Create Vulkan shader module.
	VkShaderModule ShaderModule{};

	VkShaderModuleCreateInfo CreationInfo
	{
		.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.codeSize = ShaderCodeBytes.size(),
		.pCode = reinterpret_cast<const uint32_t*>(ShaderCodeBytes.data())
	};

	vkCreateShaderModule(Device, &CreationInfo, nullptr, &ShaderModule);

	return ShaderModule;
}

struct RequiredMemory
{
	size_t SegmentSize;
	size_t SegmentsCount;
};

static RequiredMemory ComputeMemorySegments(VkMemoryRequirements Requirements)
{
	const size_t MemoryToAlign = Requirements.size % Requirements.alignment;
	const size_t MemoryWithoutAlign = (Requirements.size - MemoryToAlign) / Requirements.alignment;

	const size_t RequiredSegments = MemoryWithoutAlign + (MemoryToAlign > 0 ? 1 : 0);

	return RequiredMemory
	{
		.SegmentSize = Requirements.alignment,
		.SegmentsCount = RequiredSegments
	};
}