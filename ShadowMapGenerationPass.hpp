#pragma once

#include "RenderPass.hpp"
#include <vector>
#include "Actor.hpp"

class ShadowMapGenerationPass : public RenderPass
{
private:
	VkImage VarianceShadowMap;
	VkImageView VarianceShadowMapView;
	VkDeviceMemory VarianceShadowMapMemory;

	VkImage DepthBuffer;
	VkImageView DepthBufferView;
	VkDeviceMemory DepthBufferMemory;

	VkShaderModule ShadowMapGenerationVertexShaderModule;
	VkShaderModule ShadowMapGenerationFragmentShaderModule;
	std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;

	VkPipelineLayout ShadowMapGenerationPipelineLayout{};
	VkPipeline ShadowMapGenerationPipeline{};
	VkRenderPass ShadowMapGenerationRenderPass;

	static constexpr uint32_t ShadowMapResolution = 2048;

	VkFramebuffer ShadowMapGenerationFramebuffer{};

	VkBuffer LightSpaceUniformBuffer;
	VkDeviceMemory LightSpaceUniformBufferMemory;

	VkDescriptorSetLayout LightSpaceDescriptorSetLayout;
	VkDescriptorPool LightSpaceDescriptorPool;
	std::vector<VkDescriptorSet> LightSpaceDescriptorSets;

public:
	ShadowMapGenerationPass(VkDevice Device, const uint32_t GraphicsQueueIndex, const VkPhysicalDeviceMemoryProperties2& DeviceMemoryProperties);

	virtual void FreeGPUResources() override;

	virtual void SetupShaders() override;

	virtual void SetupPipeline() override;

	void RecordCommandBuffer(VkCommandBuffer CommandBuffer, const std::vector<SceneActor>& Actors);

	virtual ~ShadowMapGenerationPass() = default;

	struct
	{
		VkBuffer* LightSpaceUniformBuffer;
		VkImageView* VarianceShadowMap;
	} SharedResources;
};