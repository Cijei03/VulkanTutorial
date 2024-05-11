#include "DeferredPass.hpp"
#include "Helpers.hpp"

DeferredPass::DeferredPass(VkDevice Device, const uint32_t GraphicsQueueIndex, const VkPhysicalDeviceMemoryProperties2& DeviceMemoryProperties, DeferredAdditionalRequiredInfo& AdditionalResources) : RenderPass(Device)
{
	// Setup result image.
	{
		// Setup image.
		{
			VkImageCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VkFormat::VK_FORMAT_R16G16B16A16_UNORM,
				.extent =
				{
					.width = 1280,
					.height = 720,
					.depth = 1
				},
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
				.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
				.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 1,
				.pQueueFamilyIndices = &GraphicsQueueIndex,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
			};

			vkCreateImage(Device, &CreationInfo, nullptr, &this->ResultImage);

			this->SharedResources.ResultImage = &this->ResultImage;
		}
		// Allocate memory.
		{
			VkMemoryRequirements MemoryRequirements;
			vkGetImageMemoryRequirements(Device, this->ResultImage, &MemoryRequirements);

			const auto MemorySegmentsInfo = ComputeMemorySegments(MemoryRequirements);
			
			VkMemoryAllocateInfo AllocationInfo
			{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = nullptr,
				.allocationSize = MemorySegmentsInfo.SegmentSize * MemorySegmentsInfo.SegmentsCount,
				.memoryTypeIndex = QueryMemoryTypeIndex(VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MemoryRequirements.memoryTypeBits, DeviceMemoryProperties)
			};
			
			vkAllocateMemory(Device, &AllocationInfo, nullptr, &this->ResultImageMemory);

			vkBindImageMemory(Device, this->ResultImage, this->ResultImageMemory, 0);
		}
		// Setup image view.
		{
			VkImageSubresourceRange RangeInfo
			{
				.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			};

			VkImageViewCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = this->ResultImage,
				.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
				.format = VkFormat::VK_FORMAT_R16G16B16A16_UNORM,
				.components =
				{
					.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY
				},
				.subresourceRange = RangeInfo
			};

			vkCreateImageView(Device, &CreationInfo, nullptr, &this->ResultImageView);
		}
	}

	// Setup render pass.
	{
		std::vector<VkAttachmentDescription> AttachmentsInfos;
		{
			VkAttachmentDescription ResultAttachmentInfo
			{
				.flags = 0,
				.format = VkFormat::VK_FORMAT_R16G16B16A16_UNORM,
				.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};

			VkAttachmentDescription ScenePositionAttachmentInfo
			{
				.flags = 0,
				.format = VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,
				.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD,
				.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_NONE,
				.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
			};

			VkAttachmentDescription SceneNormalAttachmentInfo
			{
				.flags = 0,
				.format = VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,
				.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD,
				.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_NONE,
				.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
			};
			AttachmentsInfos =
			{
				ResultAttachmentInfo,
				ScenePositionAttachmentInfo,
				SceneNormalAttachmentInfo
			};
		}
		std::vector<VkAttachmentReference> InputAttachments;
		{
			VkAttachmentReference ScenePositionAttachmentReferenceInfo
			{
				.attachment = 1,
				.layout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			VkAttachmentReference SceneNormalAttachmentReferenceInfo
			{
				.attachment = 2,
				.layout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};

			InputAttachments =
			{
				ScenePositionAttachmentReferenceInfo,
				SceneNormalAttachmentReferenceInfo
			};
		}
		VkAttachmentReference ResultAttachmentReferenceInfo
		{
			.attachment = 0,
			.layout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkSubpassDescription SubpassInfo
		{
			.flags = 0,
			.pipelineBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 2,
			.pInputAttachments = InputAttachments.data(),
			.colorAttachmentCount = 1,
			.pColorAttachments = &ResultAttachmentReferenceInfo,
			.pResolveAttachments = nullptr,
			.pDepthStencilAttachment = nullptr,
			.preserveAttachmentCount = 0,
			.pPreserveAttachments = nullptr
		};

		VkRenderPassCreateInfo RenderPassCreationInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 3,
			.pAttachments = AttachmentsInfos.data(),
			.subpassCount = 1,
			.pSubpasses = &SubpassInfo,
			.dependencyCount = 0,
			.pDependencies = nullptr
		};

		vkCreateRenderPass(Device, &RenderPassCreationInfo, nullptr, &this->DeferredRenderPass);
	}

	// Setup framebuffer.
	{
		std::vector<VkImageView> Attachments
		{
			this->ResultImageView,
			*AdditionalResources.GBufferPositionView,
			*AdditionalResources.GBufferNormalView
		};

		VkFramebufferCreateInfo CreationInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = this->DeferredRenderPass,
			.attachmentCount = 3,
			.pAttachments = Attachments.data(),
			.width = 1280,
			.height = 720,
			.layers = 1
		};

		vkCreateFramebuffer(Device, &CreationInfo, nullptr, &this->DeferredFramebuffer);
	}

	// Setup sampler.
	{
		VkSamplerCreateInfo CreationInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.magFilter = VkFilter::VK_FILTER_LINEAR,
			.minFilter = VkFilter::VK_FILTER_LINEAR,
			.mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			.addressModeV = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			.addressModeW = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			.mipLodBias = 0.0f,
			.anisotropyEnable = false,
			.maxAnisotropy = 0,
			.compareEnable = false,
			.compareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS,
			.minLod = 0.0f,
			.maxLod = 1.0f,
			.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			.unnormalizedCoordinates = false
		};

		vkCreateSampler(Device, &CreationInfo, nullptr, &this->VarianceShadowMapSampler);
	}

	// Setup pipeline layout.
	{
		// Setup descriptor set layout.
		{
			std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings;
			{
				VkDescriptorSetLayoutBinding LightSpaceUniformBufferBindingInfo
				{
					.binding = 0,
					.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.pImmutableSamplers = nullptr
				};

				VkDescriptorSetLayoutBinding GBufferPositionInputBindingInfo
				{
					.binding = 1,
					.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.pImmutableSamplers = nullptr
				};

				VkDescriptorSetLayoutBinding GBufferNormalInputBindingInfo
				{
					.binding = 2,
					.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.pImmutableSamplers = nullptr
				};

				VkDescriptorSetLayoutBinding VarianceShadowMapBindingInfo
				{
					.binding = 3,
					.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.pImmutableSamplers = &this->VarianceShadowMapSampler
				};

				SetLayoutBindings =
				{
					LightSpaceUniformBufferBindingInfo,
					GBufferPositionInputBindingInfo,
					GBufferNormalInputBindingInfo,
					VarianceShadowMapBindingInfo
				};
			}
			

			VkDescriptorSetLayoutCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.bindingCount = 4,
				.pBindings = SetLayoutBindings.data()
			};

			vkCreateDescriptorSetLayout(Device, &CreationInfo, nullptr, &this->DeferredDescriptorSetLayout);
		}

		VkPipelineLayoutCreateInfo CreationInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &this->DeferredDescriptorSetLayout,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};

		vkCreatePipelineLayout(Device, &CreationInfo, nullptr, &this->PipelineLayout);
	}

	// Setup descriptor sets.
	{
		// Setup descriptor pool.
		{
			std::vector<VkDescriptorPoolSize> DescriptorPoolSizeInfos;
			{
				VkDescriptorPoolSize LightSpaceUniformBufferPool
				{
					.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 1
				};

				VkDescriptorPoolSize GBufferInputPositionAttachmentPool
				{
					.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
					.descriptorCount = 1
				};

				VkDescriptorPoolSize GBufferInputNormalAttachmentPool
				{
					.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
					.descriptorCount = 1
				};

				VkDescriptorPoolSize VarianceShadowMapPool
				{
					.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = 1
				};

				DescriptorPoolSizeInfos =
				{
					LightSpaceUniformBufferPool,
					GBufferInputPositionAttachmentPool,
					GBufferInputNormalAttachmentPool,
					VarianceShadowMapPool
				};
			}

			VkDescriptorPoolCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.maxSets = 1,
				.poolSizeCount = 4,
				.pPoolSizes = DescriptorPoolSizeInfos.data()
			};

			vkCreateDescriptorPool(Device, &CreationInfo, nullptr, &this->DeferredDescriptorPool);
		}

		VkDescriptorSetAllocateInfo AllocateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = this->DeferredDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &this->DeferredDescriptorSetLayout
		};

		vkAllocateDescriptorSets(Device, &AllocateInfo, this->DeferredDescriptorSets.data());
	}

	// Update descriptors.
	{
		VkDescriptorBufferInfo LightSpaceUniformBufferInfo
		{
			.buffer = *AdditionalResources.LightSpaceUniformBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE
		};

		VkDescriptorImageInfo GBufferPositionImageInfo
		{
			.sampler = VK_NULL_HANDLE,
			.imageView = *AdditionalResources.GBufferPositionView,
			.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		VkDescriptorImageInfo GBufferNormalImageInfo
		{
			.sampler = VK_NULL_HANDLE,
			.imageView = *AdditionalResources.GBufferNormalView,
			.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		VkDescriptorImageInfo VarianceShadowMapImageInfo
		{
			.sampler = VK_NULL_HANDLE,
			.imageView = *AdditionalResources.VarianceShadowMapView,
			.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		std::vector< VkWriteDescriptorSet> WriteSetInfos;
		{
			VkWriteDescriptorSet LightSpaceUniformBufferDescriptorInfo
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = this->DeferredDescriptorSets[0],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &LightSpaceUniformBufferInfo,
				.pTexelBufferView = nullptr
			};

			VkWriteDescriptorSet GBufferPositionImageDescriptorInfo
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = this->DeferredDescriptorSets[0],
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				.pImageInfo = &GBufferPositionImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr
			};

			VkWriteDescriptorSet GBufferNormalImageDescriptorInfo
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = this->DeferredDescriptorSets[0],
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				.pImageInfo = &GBufferNormalImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr
			};

			VkWriteDescriptorSet VarianceShadowMapImageDescriptorInfo
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = this->DeferredDescriptorSets[0],
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &VarianceShadowMapImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr
			};

			WriteSetInfos =
			{
				LightSpaceUniformBufferDescriptorInfo,
				GBufferPositionImageDescriptorInfo,
				GBufferNormalImageDescriptorInfo,
				VarianceShadowMapImageDescriptorInfo
			};
		}		

		vkUpdateDescriptorSets(Device, 4, WriteSetInfos.data(), 0, nullptr);
	}
}

void DeferredPass::FreeGPUResources()
{
	auto Device = this->Device;

	vkDestroyShaderModule(Device, this->DeferredVertexShaderModule, nullptr);
	vkDestroyShaderModule(Device, this->DeferredFragmentShaderModule, nullptr);
	vkDestroyImage(Device, this->ResultImage, nullptr);
	vkFreeMemory(Device, this->ResultImageMemory, nullptr);
	vkDestroyImageView(Device, this->ResultImageView, nullptr);
	vkDestroyRenderPass(Device, this->DeferredRenderPass, nullptr);
	vkDestroyFramebuffer(Device, this->DeferredFramebuffer, nullptr);

	vkDestroyDescriptorPool(Device, this->DeferredDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(Device, this->DeferredDescriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(Device, this->PipelineLayout, nullptr);
	vkDestroyPipeline(Device, this->Pipeline, nullptr);

	vkDestroySampler(Device, this->VarianceShadowMapSampler, nullptr);
}
void DeferredPass::SetupShaders()
{
	this->DeferredVertexShaderModule = CreateShaderModule(Device, "shaders/deferred_shading_pass_vert.spv");
	this->DeferredFragmentShaderModule = CreateShaderModule(Device, "shaders/deferred_shading_pass_frag.spv");

	ShaderStages =
	{
		VkPipelineShaderStageCreateInfo
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT,
			.module = DeferredVertexShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr
		},
		VkPipelineShaderStageCreateInfo
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = DeferredFragmentShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr
		}
	};
}

void DeferredPass::SetupPipeline()
{
	VkPipelineVertexInputStateCreateInfo VertexInputInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr
	};

	VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
		.primitiveRestartEnable = false
	};

	VkViewport Viewport
	{
		.x = 0,
		.y = 0,
		.width = 1280,
		.height = 720,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	VkRect2D Scissor
	{
		.offset
		{
			.x = 0,
			.y = 0
		},
		.extent
		{
			.width = 1280,
			.height = 720
		}
	};

	VkPipelineViewportStateCreateInfo ViewportState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.viewportCount = 1,
		.pViewports = &Viewport,
		.scissorCount = 1,
		.pScissors = &Scissor
	};

	VkPipelineMultisampleStateCreateInfo SamplesInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = false,
		.minSampleShading = 0,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = false,
		.alphaToOneEnable = false
	};

	VkPipelineRasterizationStateCreateInfo RasterizerInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthClampEnable = false,
		.rasterizerDiscardEnable = false,
		.polygonMode = VkPolygonMode::VK_POLYGON_MODE_FILL,
		.cullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE,
		.frontFace = VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = false,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f
	};

	std::vector<VkPipelineColorBlendAttachmentState> PipelineBlendStates
	{
		{
			.blendEnable = false,
			.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VkBlendOp::VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA,
			.dstAlphaBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp = VkBlendOp::VK_BLEND_OP_ADD,
			.colorWriteMask = VkColorComponentFlagBits::VK_COLOR_COMPONENT_R_BIT
			| VkColorComponentFlagBits::VK_COLOR_COMPONENT_G_BIT
			| VkColorComponentFlagBits::VK_COLOR_COMPONENT_B_BIT
			| VkColorComponentFlagBits::VK_COLOR_COMPONENT_A_BIT
		}
	};

	VkPipelineColorBlendStateCreateInfo ColorBlendInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.logicOpEnable = false,
		.logicOp = VkLogicOp::VK_LOGIC_OP_AND,
		.attachmentCount = 1,
		.pAttachments = PipelineBlendStates.data(),
		.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
	};

	VkGraphicsPipelineCreateInfo CreationInfo
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stageCount = 2,
		.pStages = ShaderStages.data(),
		.pVertexInputState = &VertexInputInfo,
		.pInputAssemblyState = &InputAssemblyInfo,
		.pTessellationState = nullptr,
		.pViewportState = &ViewportState,
		.pRasterizationState = &RasterizerInfo,
		.pMultisampleState = &SamplesInfo,
		.pDepthStencilState = nullptr,
		.pColorBlendState = &ColorBlendInfo,
		.pDynamicState = nullptr,
		.layout = this->PipelineLayout,
		.renderPass = this->DeferredRenderPass,
		.subpass = 0,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = -1
	};

	vkCreateGraphicsPipelines(Device, nullptr, 1, &CreationInfo, nullptr, &this->Pipeline);
}

void DeferredPass::RecordCommandBuffer(VkCommandBuffer CommandBuffer)
{
	VkClearValue ClearValue
	{
		.color =
		{
			.float32 = { 0.0f, 0.0f, 0.0f, 1.0f }
		}
	};

	VkRenderPassBeginInfo Info
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = this->DeferredRenderPass,
		.framebuffer = this->DeferredFramebuffer,
		.renderArea =
		{
			.offset = {},
			.extent =
			{
				.width = 1280,
				.height = 720
			}
		},
		.clearValueCount = 1,
		.pClearValues = &ClearValue
	};

	vkCmdBeginRenderPass(CommandBuffer, &Info, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(CommandBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, this->Pipeline);

	vkCmdBindDescriptorSets(CommandBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 0, 1, DeferredDescriptorSets.data(), 0, nullptr);
	vkCmdDraw(CommandBuffer, 4, 1, 0, 0);

	vkCmdEndRenderPass(CommandBuffer);
}