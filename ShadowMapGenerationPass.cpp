#include "ShadowMapGenerationPass.hpp"
#include "Helpers.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

ShadowMapGenerationPass::ShadowMapGenerationPass(VkDevice Device, const uint32_t GraphicsQueueIndex, const VkPhysicalDeviceMemoryProperties2& DeviceMemoryProperties) : RenderPass(Device)
{
	// Shadow map creation.
	{
		const auto MipMapLevels = log(this->ShadowMapResolution) + 1;

		// Setup image.
		{
			VkImageCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.imageType = VkImageType::VK_IMAGE_TYPE_2D,
				.format = VkFormat::VK_FORMAT_R32G32_SFLOAT,
				.extent =
				{
					.width = ShadowMapResolution,
					.height = ShadowMapResolution,
					.depth = 1
				},
				.mipLevels = static_cast<uint32_t>(MipMapLevels),
				.arrayLayers = 1,
				.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
				.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
				.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
				.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 1,
				.pQueueFamilyIndices = &GraphicsQueueIndex,
				.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED
			};

			vkCreateImage(Device, &CreationInfo, nullptr, &this->VarianceShadowMap);
		}
		// Allocate memory.
		{
			VkMemoryRequirements MemoryRequirements;
			vkGetImageMemoryRequirements(Device, this->VarianceShadowMap, &MemoryRequirements);

			const auto MemorySegments = ComputeMemorySegments(MemoryRequirements);

			VkMemoryAllocateInfo AllocationInfo
			{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = nullptr,
				.allocationSize = MemorySegments.SegmentsCount * MemorySegments.SegmentSize,
				.memoryTypeIndex = QueryMemoryTypeIndex(VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MemoryRequirements.memoryTypeBits, DeviceMemoryProperties)
			};

			vkAllocateMemory(Device, &AllocationInfo, nullptr, &this->VarianceShadowMapMemory);

			vkBindImageMemory(Device, this->VarianceShadowMap, this->VarianceShadowMapMemory, 0);
		}

		// Setup image view.
		{
			VkImageSubresourceRange Range
			{
				.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			};

			VkImageViewCreateInfo ViewCreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = this->VarianceShadowMap,
				.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
				.format = VkFormat::VK_FORMAT_R32G32_SFLOAT,
				.components =
				{
					.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY
				},
				.subresourceRange = Range
			};

			vkCreateImageView(Device, &ViewCreationInfo, nullptr, &this->VarianceShadowMapView);

			this->SharedResources.VarianceShadowMap = &this->VarianceShadowMapView;
		}
	}

	// Setup depth buffer.
	{
		// Create image.
		{
			VkImageCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.imageType = VkImageType::VK_IMAGE_TYPE_2D,
				.format = VkFormat::VK_FORMAT_D32_SFLOAT,
				.extent =
				{
					.width = ShadowMapResolution,
					.height = ShadowMapResolution,
					.depth = 1
				},
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
				.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
				.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 1,
				.pQueueFamilyIndices = &GraphicsQueueIndex,
				.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED
			};

			vkCreateImage(Device, &CreationInfo, nullptr, &this->DepthBuffer);
		}
		// Allocate memory.
		{
			VkMemoryRequirements MemoryRequirements;
			vkGetImageMemoryRequirements(Device, this->DepthBuffer, &MemoryRequirements);

			const size_t MemoryToAlign = MemoryRequirements.size % MemoryRequirements.alignment;
			const size_t MemoryWithoutAlign = (MemoryRequirements.size - MemoryToAlign) / MemoryRequirements.alignment;

			const size_t RequiredSegments = MemoryWithoutAlign + (MemoryToAlign > 0 ? 1 : 0);

			VkMemoryAllocateInfo AllocationInfo
			{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = nullptr,
				.allocationSize = RequiredSegments * MemoryRequirements.alignment,
				.memoryTypeIndex = QueryMemoryTypeIndex(VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MemoryRequirements.memoryTypeBits, DeviceMemoryProperties)
			};

			vkAllocateMemory(Device, &AllocationInfo, nullptr, &this->DepthBufferMemory);

			vkBindImageMemory(Device, this->DepthBuffer, this->DepthBufferMemory, 0);
		}

		// Setup image view.
		{
			VkImageSubresourceRange Range
			{
				.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			};

			VkImageViewCreateInfo ViewCreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = this->DepthBuffer,
				.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
				.format = VkFormat::VK_FORMAT_D32_SFLOAT,
				.components =
				{
					.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY
				},
				.subresourceRange = Range
			};

			vkCreateImageView(Device, &ViewCreationInfo, nullptr, &this->DepthBufferView);
		}
	}

	// Setup render pass.
	{
		std::vector<VkAttachmentDescription> Attachments
		{
			VkAttachmentDescription
			{
				.flags = 0,
				.format = VkFormat::VK_FORMAT_R32G32_SFLOAT,
				.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			},
			VkAttachmentDescription
			{
				.flags = 0,
				.format = VkFormat::VK_FORMAT_D32_SFLOAT,
				.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
			}
		};

		VkAttachmentReference ColorAttachment
		{
			.attachment = 0,
			.layout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};
		VkAttachmentReference DepthAttachment
		{
			.attachment = 1,
			.layout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription SubpassInfo
		{
			.flags = 0,
			.pipelineBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &ColorAttachment,
			.pResolveAttachments = nullptr,
			.pDepthStencilAttachment = &DepthAttachment,
			.preserveAttachmentCount = 0,
			.pPreserveAttachments = nullptr
		};

		VkSubpassDependency SubpassDependencies
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.dstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.srcAccessMask = 0,
			.dstAccessMask = VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0
		};

		VkRenderPassCreateInfo RenderPassCreationInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 2,
			.pAttachments = Attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &SubpassInfo,
			.dependencyCount = 1,
			.pDependencies = &SubpassDependencies
		};

		vkCreateRenderPass(Device, &RenderPassCreationInfo, nullptr, &this->ShadowMapGenerationRenderPass);
	}

	// Setup light space uniform buffer.
	{
		struct LightSpaceBufferContent
		{
			glm::mat4 ViewMatrix;
			glm::mat4 ProjectionMatrix;
			glm::vec4 LightDirection;
		};

		const size_t RequestedBufferSize = sizeof(LightSpaceBufferContent);

		// Setup buffer.
		{
			VkBufferCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.size = RequestedBufferSize,
				.usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 1,
				.pQueueFamilyIndices = &GraphicsQueueIndex
			};

			vkCreateBuffer(Device, &CreationInfo, nullptr, &this->LightSpaceUniformBuffer);
		}

		// Allocate memory for buffer.
		{
			VkMemoryRequirements MemoryRequirements;
			vkGetBufferMemoryRequirements(Device, this->LightSpaceUniformBuffer, &MemoryRequirements);

			const VkMemoryPropertyFlagBits MemoryFlags = static_cast<VkMemoryPropertyFlagBits>(VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			const auto MemorySegments = ComputeMemorySegments(MemoryRequirements);

			VkMemoryAllocateInfo AllocationInfo
			{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = nullptr,
				.allocationSize =  MemorySegments.SegmentSize * MemorySegments.SegmentsCount,
				.memoryTypeIndex = QueryMemoryTypeIndex(MemoryFlags, MemoryRequirements.memoryTypeBits, DeviceMemoryProperties),
			};

			vkAllocateMemory(Device, &AllocationInfo, nullptr, &this->LightSpaceUniformBufferMemory);

			vkBindBufferMemory(Device, this->LightSpaceUniformBuffer, this->LightSpaceUniformBufferMemory, 0);
		}
		
		// Update buffer content.
		{
			const glm::vec3 CameraDirection = glm::normalize(glm::vec3(-10, 25, 4));
			const glm::vec3 EyePosition = CameraDirection * glm::vec3(20) * glm::vec3(1, -1, 1);

			LightSpaceBufferContent Content
			{
				.ViewMatrix = glm::mat4(1.0f),
				.ProjectionMatrix = glm::mat4(1.0f),
				.LightDirection = glm::vec4(CameraDirection, 1.0f)
			};

			Content.ViewMatrix = glm::lookAt(EyePosition, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			Content.ProjectionMatrix = glm::perspective(glm::radians(90.0f), 2048.0f / 2048.0f, 0.1f, 30.0f);
			//Content.ProjectionMatrix = glm::ortho(-70.0f, 70.0f, -70.0f, 70.0f, 0.1f, 180.0f);


			void* MappedBuffer = nullptr;
			vkMapMemory(Device, this->LightSpaceUniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &MappedBuffer);

			std::memcpy(MappedBuffer, &Content, sizeof(Content));

			VkMappedMemoryRange MappedMemoryRange
			{
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.pNext = nullptr,
				.memory = this->LightSpaceUniformBufferMemory,
				.offset = 0,
				.size = VK_WHOLE_SIZE
			};
			vkFlushMappedMemoryRanges(Device, 1, &MappedMemoryRange);

			vkUnmapMemory(Device, this->LightSpaceUniformBufferMemory);

			this->SharedResources.LightSpaceUniformBuffer = &this->LightSpaceUniformBuffer;
		}
	}

	// Setup descriptors.
	{
		// Setup descriptor set layout.
		{
			VkDescriptorSetLayoutBinding SetLayoutBinding
			{
				.binding = 0,
				.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
				.pImmutableSamplers = nullptr
			};

			VkDescriptorSetLayoutCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.bindingCount = 1,
				.pBindings = &SetLayoutBinding
			};

			vkCreateDescriptorSetLayout(Device, &CreationInfo, nullptr, &this->LightSpaceDescriptorSetLayout);
		}

		// Setup descriptor pool.
		{
			VkDescriptorPoolSize DescriptorPoolSizeInfo
			{
				.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1
			};

			VkDescriptorPoolCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.maxSets = 1,
				.poolSizeCount = 1,
				.pPoolSizes = &DescriptorPoolSizeInfo
			};

			vkCreateDescriptorPool(Device, &CreationInfo, nullptr, &this->LightSpaceDescriptorPool);
		}

		// Setup descriptors itself.
		{
			// Allocate.
			{
				VkDescriptorSetAllocateInfo AllocateInfo
				{
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
					.pNext = nullptr,
					.descriptorPool = this->LightSpaceDescriptorPool,
					.descriptorSetCount = 1,
					.pSetLayouts = &this->LightSpaceDescriptorSetLayout
				};

				VkDescriptorSet DescriptorSet;

				vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet);

				this->LightSpaceDescriptorSets.push_back(DescriptorSet);
			}

			// Update.
			{
				VkDescriptorBufferInfo DescriptorBufferInfo
				{
					.buffer = this->LightSpaceUniformBuffer,
					.offset = 0,
					.range = VK_WHOLE_SIZE
				};

				VkWriteDescriptorSet DescriptorSetsToUpdate
				{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.pNext = nullptr,
					.dstSet = this->LightSpaceDescriptorSets[0],
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pImageInfo = nullptr,
					.pBufferInfo = &DescriptorBufferInfo,
					.pTexelBufferView = nullptr
				};

				vkUpdateDescriptorSets(Device, 1, &DescriptorSetsToUpdate, 0, nullptr);
			}
		}
	}

	// Setup pipeline layout.
	{
		VkPipelineLayoutCreateInfo CreationInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &this->LightSpaceDescriptorSetLayout,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};

		vkCreatePipelineLayout(Device, &CreationInfo, nullptr, &this->ShadowMapGenerationPipelineLayout);
	}

	// Setup framebuffer.
	{
		VkImageView Attachments[] = { this->VarianceShadowMapView, DepthBufferView };

		VkFramebufferCreateInfo CreationInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = this->ShadowMapGenerationRenderPass,
			.attachmentCount = 2,
			.pAttachments = Attachments,
			.width = this->ShadowMapResolution,
			.height = this->ShadowMapResolution,
			.layers = 1
		};

		vkCreateFramebuffer(Device, &CreationInfo, nullptr, &ShadowMapGenerationFramebuffer);
	}
}

void ShadowMapGenerationPass::SetupShaders()
{
	this->ShadowMapGenerationVertexShaderModule = CreateShaderModule(Device, "shaders/shadow_map_generation_vert.spv");
	this->ShadowMapGenerationFragmentShaderModule = CreateShaderModule(Device, "shaders/shadow_map_generation_frag.spv");

	ShaderStages =
	{
		VkPipelineShaderStageCreateInfo
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT,
			.module = ShadowMapGenerationVertexShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr
		},
		VkPipelineShaderStageCreateInfo
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = ShadowMapGenerationFragmentShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr
		}
	};
}

void ShadowMapGenerationPass::SetupPipeline()
{
	std::vector<VkVertexInputBindingDescription> VertexInputBindings
	{
		VkVertexInputBindingDescription
		{
			.binding = 0,
			.stride = 12,
			.inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX
		}
	};

	std::vector<VkVertexInputAttributeDescription> VertexInputAttributes
	{
		VkVertexInputAttributeDescription
		{
			.location = 0,
			.binding = 0,
			.format = VkFormat::VK_FORMAT_R32G32B32_SFLOAT,
			.offset = 0
		}
	};

	VkPipelineVertexInputStateCreateInfo VertexInputStateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = VertexInputBindings.data(),
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = VertexInputAttributes.data()
	};

	VkPipelineInputAssemblyStateCreateInfo InputAssemblyStateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = false
	};

	VkViewport Viewport
	{
		.x = 0,
		.y = 0,
		.width = ShadowMapResolution,
		.height = ShadowMapResolution,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	VkRect2D Scissor
	{
		.offset =
		{
			.x = 0,
			.y = 0
		},
		.extent =
		{
			.width = ShadowMapResolution,
			.height = ShadowMapResolution
		}
	};

	VkPipelineViewportStateCreateInfo ViewportStateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.viewportCount = 1,
		.pViewports = &Viewport,
		.scissorCount = 1,
		.pScissors = &Scissor
	};

	VkPipelineRasterizationStateCreateInfo RasterizationStateInfo
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

	VkSampleMask SampleMask = UINT32_MAX;

	VkPipelineMultisampleStateCreateInfo MultiSampleStateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = false,
		.minSampleShading = 0.0f,
		.pSampleMask = &SampleMask,
		.alphaToCoverageEnable = false,
		.alphaToOneEnable = false
	};

	VkPipelineDepthStencilStateCreateInfo DepthStencilStateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthTestEnable = true,
		.depthWriteEnable = true,
		.depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = false,
		.stencilTestEnable = false,
		.front = VkStencilOp::VK_STENCIL_OP_KEEP,
		.back = VkStencilOp::VK_STENCIL_OP_KEEP,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};

	VkPipelineColorBlendAttachmentState AttachmentBlendStateInfo
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
	};

	VkPipelineColorBlendStateCreateInfo BlendStateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.logicOpEnable = false,
		.logicOp = VkLogicOp::VK_LOGIC_OP_AND,
		.attachmentCount = 1,
		.pAttachments = &AttachmentBlendStateInfo,
	};

	VkGraphicsPipelineCreateInfo PipelineCreationInfo
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stageCount = 2,
		.pStages = this->ShaderStages.data(),
		.pVertexInputState = &VertexInputStateInfo,
		.pInputAssemblyState = &InputAssemblyStateInfo,
		.pTessellationState = nullptr,
		.pViewportState = &ViewportStateInfo,
		.pRasterizationState = &RasterizationStateInfo,
		.pMultisampleState = &MultiSampleStateInfo,
		.pDepthStencilState = &DepthStencilStateInfo,
		.pColorBlendState = &BlendStateInfo,
		.pDynamicState = nullptr,
		.layout = this->ShadowMapGenerationPipelineLayout,
		.renderPass = this->ShadowMapGenerationRenderPass,
		.subpass = 0,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = -1
	};

	vkCreateGraphicsPipelines(this->Device, nullptr, 1, &PipelineCreationInfo, nullptr, &this->ShadowMapGenerationPipeline);
}

void ShadowMapGenerationPass::FreeGPUResources()
{
	auto Device = this->Device;

	vkDestroyImage(Device, this->VarianceShadowMap, nullptr);
	vkDestroyImageView(Device, this->VarianceShadowMapView, nullptr);
	vkFreeMemory(Device, this->VarianceShadowMapMemory, nullptr);

	vkDestroyImage(Device, this->DepthBuffer, nullptr);
	vkDestroyImageView(Device, this->DepthBufferView, nullptr);
	vkFreeMemory(Device, this->DepthBufferMemory, nullptr);

	vkDestroyShaderModule(Device, this->ShadowMapGenerationVertexShaderModule, nullptr);
	vkDestroyShaderModule(Device, this->ShadowMapGenerationFragmentShaderModule, nullptr);

	vkDestroyRenderPass(Device, this->ShadowMapGenerationRenderPass, nullptr);

	vkDestroyPipelineLayout(Device, this->ShadowMapGenerationPipelineLayout, nullptr);
	vkDestroyPipeline(Device, this->ShadowMapGenerationPipeline, nullptr);

	vkDestroyFramebuffer(Device, this->ShadowMapGenerationFramebuffer, nullptr);

	vkDestroyBuffer(Device, this->LightSpaceUniformBuffer, nullptr);
	vkFreeMemory(Device, this->LightSpaceUniformBufferMemory, nullptr);

	vkDestroyDescriptorSetLayout(Device, this->LightSpaceDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(Device, this->LightSpaceDescriptorPool, nullptr);
}

void ShadowMapGenerationPass::RecordCommandBuffer(VkCommandBuffer CommandBuffer, const std::vector<SceneActor>& Actors)
{
	std::vector<VkClearValue> ClearValues
	{
		VkClearValue
		{
			.color = { 0.05f, 0.05f, 0.05f, 1.0f }
		},
		VkClearValue
		{
			.depthStencil =
			{
				.depth = 1.0f,
				.stencil = UINT32_MAX
			}
		}
	};

	VkRenderPassBeginInfo BeginRenderPassInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = this->ShadowMapGenerationRenderPass,
		.framebuffer = this->ShadowMapGenerationFramebuffer,
		.renderArea =
		{
			.offset
			{
				.x = 0,
				.y = 0
			},
			.extent
			{
				.width = this->ShadowMapResolution,
				.height = this->ShadowMapResolution
			}
		},
		.clearValueCount = static_cast<uint32_t>(ClearValues.size()),
		.pClearValues = ClearValues.data(),
	};

	vkCmdBeginRenderPass(CommandBuffer, &BeginRenderPassInfo, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(CommandBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, this->ShadowMapGenerationPipeline);

	vkCmdBindDescriptorSets(CommandBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, this->ShadowMapGenerationPipelineLayout, 0, 1, this->LightSpaceDescriptorSets.data(), 0, nullptr);

	for (const auto& Actor : Actors)
	{
		vkCmdBindVertexBuffers(CommandBuffer, 0, 1, Actor.VertexBuffers.data(), Actor.Offsets.data());
		vkCmdDraw(CommandBuffer, Actor.VerticesCount, 1, 0, 0);
	}

	vkCmdEndRenderPass(CommandBuffer);
}