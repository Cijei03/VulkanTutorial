#pragma once

#include <cstdint>
#include "RenderPass.hpp"
#include <vector>
#include "Actor.hpp"

class GBufferGenerationPass : public RenderPass
{
private:
	VkImage GBufferPositionImage{};
	VkImage GBufferNormalImage{};
	VkImageView GBufferPositionImageView{};
	VkImageView GBufferNormalImageView{};
	VkDeviceMemory GBufferMemory{};

	VkFramebuffer GBufferGenerationPassFramebuffer{};

	VkDescriptorSetLayout DeferredPassSetLayout{};
	VkBuffer SceneTransformationUBO{};
	VkDeviceMemory SceneTransformationUBOMemory{};
	VkDescriptorPool DeferredPassDescriptorPool{};
	std::vector<VkDescriptorSet> DescriptorSets;

	VkRenderPass SceneRenderPass{};

	std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;
	VkShaderModule GBufferGenerationVertexShaderModule{};
	VkShaderModule GBufferGenerationFragmentShaderModule{};

	VkPipelineLayout PipelineLayout{};
	VkPipeline Pipeline{};
public:
	GBufferGenerationPass(VkDevice Device, const uint32_t GraphicsQueueIndex, const VkPhysicalDeviceMemoryProperties2& DeviceMemoryProperties, VkImageView DepthBuffer);

	virtual void FreeGPUResources() override;

	void RecordCommandBuffer(VkCommandBuffer CommandBuffer, const std::vector<SceneActor>& Actors);

	virtual void SetupShaders() override;

	virtual void SetupPipeline() override;

	virtual ~GBufferGenerationPass() = default;

	struct
	{
		VkImageView* GBufferPositionImageViewLink = nullptr;
		VkImageView* GBufferNormalImageViewLink = nullptr;
	} SharedResources;
};