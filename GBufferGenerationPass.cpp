#include "GBufferGenerationPass.hpp"
#include "Helpers.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

GBufferGenerationPass::GBufferGenerationPass(VkDevice Device, const uint32_t GraphicsQueueIndex, const VkPhysicalDeviceMemoryProperties2& DeviceMemoryProperties, VkImageView DepthBuffer) : RenderPass(Device)
{
	// Setup uniform buffer.
	{
		// Setup descriptor layout.
		{
			VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding
			{
				.binding = 0,
				.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT,
				.pImmutableSamplers = nullptr
			};

			VkDescriptorSetLayoutCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.bindingCount = 1,
				.pBindings = &DescriptorSetLayoutBinding
			};

			vkCreateDescriptorSetLayout(Device, &CreationInfo, nullptr, &DeferredPassSetLayout);
		}

		// Setup uniform buffer itself.
		{
			VkBufferCreateInfo BufferCreationInfo
			{
				.sType = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.size = 128,
				.usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 1,
				.pQueueFamilyIndices = &GraphicsQueueIndex
			};

			vkCreateBuffer(Device, &BufferCreationInfo, nullptr, &SceneTransformationUBO);

			VkMemoryRequirements MemoryRequirements;
			vkGetBufferMemoryRequirements(Device, SceneTransformationUBO, &MemoryRequirements);

			VkMemoryAllocateInfo AllocationInfo
			{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = nullptr,
				.allocationSize = MemoryRequirements.size,
				.memoryTypeIndex = QueryMemoryTypeIndex(VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, MemoryRequirements.memoryTypeBits, DeviceMemoryProperties)
			};

			vkAllocateMemory(Device, &AllocationInfo, nullptr, &SceneTransformationUBOMemory);

			vkBindBufferMemory(Device, SceneTransformationUBO, SceneTransformationUBOMemory, 0);
		}

		// Update uniform buffer.
		{
			using namespace glm;

			void* MappedBufferPtr = nullptr;
			vkMapMemory(Device, SceneTransformationUBOMemory, 0, VK_WHOLE_SIZE, 0, &MappedBufferPtr);

			struct BufferContent
			{
				mat4 ViewMatrix;
				f32mat4 ProjectionMatrix;
			} Content
			{
				.ViewMatrix = mat4(1.0f),
				.ProjectionMatrix = mat4(1.0f)
			};

			Content.ViewMatrix = glm::rotate(Content.ViewMatrix, glm::radians(-48.0f), glm::vec3(0.5f, 0.7f, 0.0f));
			//Content.ViewMatrix = glm::translate(Content.ViewMatrix, glm::vec3(-9.0f, -5.0f, -10.0f));
			Content.ViewMatrix = glm::translate(Content.ViewMatrix, glm::vec3(-9.0f, 8.0f, -8.0f));

			Content.ProjectionMatrix = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);

			std::memcpy(MappedBufferPtr, &Content, sizeof(Content));

			VkMappedMemoryRange MappedRange
			{
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.pNext = nullptr,
				.memory = SceneTransformationUBOMemory,
				.offset = 0,
				.size = VK_WHOLE_SIZE
			};

			vkFlushMappedMemoryRanges(Device, 1, &MappedRange);

			vkUnmapMemory(Device, SceneTransformationUBOMemory);
		}

		// Setup descriptor pool.
		{
			std::vector<VkDescriptorPoolSize> PoolSizes
			{
				{
					.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 1
				}
			};

			VkDescriptorPoolCreateInfo DescriptorPoolCreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.maxSets = 1,
				.poolSizeCount = static_cast<uint32_t>(PoolSizes.size()),
				.pPoolSizes = PoolSizes.data()
			};

			vkCreateDescriptorPool(Device, &DescriptorPoolCreationInfo, nullptr, &DeferredPassDescriptorPool);
		}

		// Setup descriptor set.
		{
			VkDescriptorSetAllocateInfo AllocateInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = DeferredPassDescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &DeferredPassSetLayout
			};

			VkDescriptorSet LocalDescSet{};
			vkAllocateDescriptorSets(Device, &AllocateInfo, &LocalDescSet);

			DescriptorSets.push_back(LocalDescSet);
		}

		// Setup descriptors.
		{
			VkDescriptorBufferInfo DescriptorBufferInfo
			{
				.buffer = SceneTransformationUBO,
				.offset = 0,
				.range = VK_WHOLE_SIZE
			};

			VkWriteDescriptorSet DescriptorConfiguration
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = DescriptorSets[0],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &DescriptorBufferInfo,
				.pTexelBufferView = nullptr
			};

			vkUpdateDescriptorSets(Device, 1, &DescriptorConfiguration, 0, nullptr);
		}
	}


	// Setup G-buffer itself.
	{
		// Create images.
		{
			VkImageCreateInfo ImageCreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.imageType = VkImageType::VK_IMAGE_TYPE_2D,
				.format = VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,
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
				.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
				.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 1,
				.pQueueFamilyIndices = &GraphicsQueueIndex,
				.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED
			};

			vkCreateImage(Device, &ImageCreationInfo, nullptr, &GBufferPositionImage);
			vkCreateImage(Device, &ImageCreationInfo, nullptr, &GBufferNormalImage);
		}

		// Allocate memory.
		{
			VkMemoryRequirements MemoryRequirements{};
			vkGetImageMemoryRequirements(Device, GBufferPositionImage, &MemoryRequirements);

			const size_t MemoryToAlign = MemoryRequirements.size % MemoryRequirements.alignment;
			const size_t MemoryWithoutAlign = (MemoryRequirements.size - MemoryToAlign) / MemoryRequirements.alignment;

			const size_t RequiredSegments = MemoryWithoutAlign + (MemoryToAlign > 0 ? 1 : 0);

			VkMemoryAllocateInfo AllocationInfo
			{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = nullptr,
				.allocationSize = RequiredSegments * MemoryRequirements.alignment * 2,
				.memoryTypeIndex = QueryMemoryTypeIndex(VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MemoryRequirements.memoryTypeBits, DeviceMemoryProperties)
			};

			vkAllocateMemory(Device, &AllocationInfo, nullptr, &GBufferMemory);
			vkBindImageMemory(Device, GBufferPositionImage, GBufferMemory, 0);
			vkBindImageMemory(Device, GBufferNormalImage, GBufferMemory, RequiredSegments * MemoryRequirements.alignment);
		}

		// Create image views.
		{
			VkImageSubresourceRange SubresourceViewInfo
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
				.image = GBufferPositionImage,
				.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
				.format = VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,
				.components =
				{
					.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY
				},
				.subresourceRange = SubresourceViewInfo
			};

			vkCreateImageView(Device, &CreationInfo, nullptr, &GBufferPositionImageView);

			CreationInfo.image = GBufferNormalImage;
			vkCreateImageView(Device, &CreationInfo, nullptr, &GBufferNormalImageView);

			this->SharedResources.GBufferPositionImageViewLink = &this->GBufferPositionImageView;
			this->SharedResources.GBufferNormalImageViewLink = &this->GBufferNormalImageView;
		}
	}

	// Setup render pass.
	{
		VkAttachmentDescription ColorAttachmentInfo
		{
			.flags = 0,
			.format = VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,
			.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		VkAttachmentDescription DepthAttachmentInfo
		{
			.flags = 0,
			.format = VkFormat::VK_FORMAT_D32_SFLOAT,
			.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
		};

		std::vector<VkAttachmentReference> ColorAttachments
		{
			{
				.attachment = 0,
				.layout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
			{
				.attachment = 1,
				.layout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			}
		};

		VkAttachmentReference DepthAttachmentReference
		{
			.attachment = 2,
			.layout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDependency SubpassDependencyInfo
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.dstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.srcAccessMask = 0,
			.dstAccessMask = VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0
		};

		VkSubpassDescription SubpassInfo
		{
			.flags = 0,
			.pipelineBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = static_cast<uint32_t>(ColorAttachments.size()),
			.pColorAttachments = ColorAttachments.data(),
			.pResolveAttachments = nullptr,
			.pDepthStencilAttachment = &DepthAttachmentReference,
			.preserveAttachmentCount = 0,
			.pPreserveAttachments = nullptr
		};

		VkAttachmentDescription Attachments[] = { ColorAttachmentInfo, ColorAttachmentInfo, DepthAttachmentInfo };

		VkRenderPassCreateInfo CreationInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 3,
			.pAttachments = Attachments,
			.subpassCount = 1,
			.pSubpasses = &SubpassInfo,
			.dependencyCount = 1,
			.pDependencies = &SubpassDependencyInfo
		};

		vkCreateRenderPass(Device, &CreationInfo, nullptr, &SceneRenderPass);
	}

	// Setup framebuffer.
	{
		VkImageView Attachments[] = { GBufferPositionImageView, GBufferNormalImageView, DepthBuffer };

		VkFramebufferCreateInfo CreationInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = SceneRenderPass,
			.attachmentCount = 3,
			.pAttachments = Attachments,
			.width = 1280,
			.height = 720,
			.layers = 1
		};

		vkCreateFramebuffer(Device, &CreationInfo, nullptr, &GBufferGenerationPassFramebuffer);
	}

	// Setup pipeline layout.
	{
		VkPipelineLayoutCreateInfo CreationInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &DeferredPassSetLayout,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};

		vkCreatePipelineLayout(Device, &CreationInfo, nullptr, &PipelineLayout);
	}
}

void GBufferGenerationPass::FreeGPUResources()
{
	vkDestroyDescriptorPool(Device, DeferredPassDescriptorPool, nullptr);

	vkDestroyBuffer(Device, SceneTransformationUBO, nullptr);
	vkFreeMemory(Device, SceneTransformationUBOMemory, nullptr);
	vkDestroyDescriptorSetLayout(Device, DeferredPassSetLayout, nullptr);

	vkDestroyImage(Device, GBufferPositionImage, nullptr);
	vkDestroyImage(Device, GBufferNormalImage, nullptr);
	vkDestroyImageView(Device, GBufferPositionImageView, nullptr);
	vkDestroyImageView(Device, GBufferNormalImageView, nullptr);
	vkFreeMemory(Device, GBufferMemory, nullptr);

	vkDestroyFramebuffer(Device, GBufferGenerationPassFramebuffer, nullptr);

	vkDestroyPipeline(Device, Pipeline, nullptr);
	vkDestroyRenderPass(Device, SceneRenderPass, nullptr);
	vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);

	vkDestroyShaderModule(Device, GBufferGenerationVertexShaderModule, nullptr);
	vkDestroyShaderModule(Device, GBufferGenerationFragmentShaderModule, nullptr);
}

void GBufferGenerationPass::SetupShaders()
{
	GBufferGenerationVertexShaderModule = CreateShaderModule(Device, "shaders/gbuffer_generation_pass_vert.spv");
	GBufferGenerationFragmentShaderModule = CreateShaderModule(Device, "shaders/gbuffer_generation_pass_frag.spv");

	ShaderStages =
	{
		VkPipelineShaderStageCreateInfo
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT,
			.module = GBufferGenerationVertexShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr
		},
			VkPipelineShaderStageCreateInfo
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = GBufferGenerationFragmentShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr
		}
	};
}

void GBufferGenerationPass::SetupPipeline()
{
	std::vector<VkVertexInputBindingDescription> VertexInputBindings
	{
		VkVertexInputBindingDescription
		{
			.binding = 0,
			.stride = 12,
			.inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX
		},
		VkVertexInputBindingDescription
		{
			.binding = 1,
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
		},
		VkVertexInputAttributeDescription
		{
			.location = 1,
			.binding = 1,
			.format = VkFormat::VK_FORMAT_R32G32B32_SFLOAT,
			.offset = 0
		}
	};

	VkPipelineVertexInputStateCreateInfo VertexInputInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.vertexBindingDescriptionCount = 2,
		.pVertexBindingDescriptions = VertexInputBindings.data(),
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = VertexInputAttributes.data()
	};

	VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo
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
		},
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
		.attachmentCount = 2,
		.pAttachments = PipelineBlendStates.data(),
		.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
	};

	VkPipelineDepthStencilStateCreateInfo DepthStencilState
	{
		.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
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
		.pDepthStencilState = &DepthStencilState,
		.pColorBlendState = &ColorBlendInfo,
		.pDynamicState = nullptr,
		.layout = PipelineLayout,
		.renderPass = SceneRenderPass,
		.subpass = 0,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = -1
	};

	vkCreateGraphicsPipelines(Device, nullptr, 1, &CreationInfo, nullptr, &Pipeline);
}

void GBufferGenerationPass::RecordCommandBuffer(VkCommandBuffer CommandBuffer, const std::vector<SceneActor>& Actors)
{
	std::vector<VkClearValue> ClearValues
	{
		VkClearValue
		{
			.color = { 0.05f, 0.05f, 0.05f, 1.0f }
		},
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
		.renderPass = SceneRenderPass,
		.framebuffer = GBufferGenerationPassFramebuffer,
		.renderArea = 
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
		},
		.clearValueCount = static_cast<uint32_t>(ClearValues.size()),
		.pClearValues = ClearValues.data(),
	};

	vkCmdBeginRenderPass(CommandBuffer, &BeginRenderPassInfo, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);

	vkCmdBindDescriptorSets(CommandBuffer, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, DescriptorSets.data(), 0, nullptr);

	for (const auto& Actor : Actors)
	{
		vkCmdBindVertexBuffers(CommandBuffer, 0, 2, Actor.VertexBuffers.data(), Actor.Offsets.data());
		vkCmdDraw(CommandBuffer, Actor.VerticesCount, 1, 0, 0);
	}

	vkCmdEndRenderPass(CommandBuffer);
}