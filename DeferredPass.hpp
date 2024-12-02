#pragma once

#include "RenderPass.hpp"
#include <vector>

struct DeferredAdditionalRequiredInfo
{
	VkImageView* GBufferPositionView;
	VkImageView* GBufferNormalView;
	VkBuffer* LightSpaceUniformBuffer;
	VkImageView* VarianceShadowMapView;
};

class DeferredPass : public RenderPass
{
private:
	VkShaderModule DeferredVertexShaderModule;
	VkShaderModule DeferredFragmentShaderModule;
	std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;

	VkImage ResultImage;
	VkImageView ResultImageView;
	VkDeviceMemory ResultImageMemory;

	VkRenderPass DeferredRenderPass;
	VkFramebuffer DeferredFramebuffer;

	VkDescriptorSetLayout DeferredDescriptorSetLayout;
	VkDescriptorPool DeferredDescriptorPool;
	std::vector<VkDescriptorSet> DeferredDescriptorSets = std::vector<VkDescriptorSet>(1);

	VkPipelineLayout PipelineLayout;	
	VkPipeline Pipeline;

	VkSampler VarianceShadowMapSampler;

public:
	DeferredPass(VkDevice Device, const uint32_t GraphicsQueueIndex, const VkPhysicalDeviceMemoryProperties2& DeviceMemoryProperties, DeferredAdditionalRequiredInfo& AdditionalInfo);

	virtual void FreeGPUResources() override;

	virtual void SetupShaders() override;

	virtual void SetupPipeline() override;

	void RecordCommandBuffer(VkCommandBuffer CommandBuffer);

	virtual ~DeferredPass() = default;

	struct
	{
		VkImage* ResultImage;
	} SharedResources;
};