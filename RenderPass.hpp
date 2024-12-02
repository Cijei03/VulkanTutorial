#pragma once
#include <vulkan/vulkan.h>

class RenderPass
{
protected:
	VkDevice Device{};
public:
	RenderPass(VkDevice Device);

	virtual void FreeGPUResources() = 0;

	virtual void SetupShaders() = 0;

	virtual void SetupPipeline() = 0;

	virtual ~RenderPass() = default;
};