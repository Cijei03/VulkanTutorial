#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "wavefront_loader.hpp"
#include "Helpers.hpp"
#include "Actor.hpp"

#include "GBufferGenerationPass.hpp"
#include "ShadowMapGenerationPass.hpp"
#include "DeferredPass.hpp"

#define TUTORIAL_VK_DEVICE_VENDOR_NONE 0
#define TUTORIAL_VK_DEVICE_VENDOR_AMD 1
#define TUTORIAL_VK_DEVICE_VENDOR_NVIDIA 2
#define TUTORIAL_VK_DEVICE_VENDOR_INTEL 3

//#define TUTORIAL_VK_DEBUG_DEALLOCATIONS false // Uncomment this macro for omit app loop and scene loading. For allocations/deallocations Vulkan debug purpose.

#ifndef TUTORIAL_VK_DEBUG_DEALLOCATIONS
	#define TUTORIAL_VK_DEBUG_DEALLOCATIONS true
#endif

//#define TUTORIAL_VK_FORCE_DEVICE_VENDOR TUTORIAL_VK_DEVICE_VENDOR_NVIDIA // Force device vendor while querying Vulkan device. Typically used for debugging purpose.

//#define TUTORIAL_VK_DEBUG_COMMAND_BUFFER_SUBMIT // Uncommented causes that main app loop will end after 1 frame rendering. For debug command buffer recording purpose.

#if TUTORIAL_VK_FORCE_DEVICE_VENDOR == TUTORIAL_VK_DEVICE_VENDOR_INTEL
	#error "Currently implementation for Intel GPUs is not present."
#endif

VkApplicationInfo AppInfo
{
	.sType = VkStructureType::VK_STRUCTURE_TYPE_APPLICATION_INFO,
	.pNext = nullptr,
	.pApplicationName = "Vulkan Tutorial",
	.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
	.pEngineName = "Vulkan Engine",
	.engineVersion = VK_MAKE_VERSION(1, 0, 0),
	.apiVersion = VK_MAKE_VERSION(1, 3, 0)
};

struct DeviceInfo
{
	std::string HardwareName;
	std::string DriverVersion;
	size_t TotalMemoryInMB;
	size_t FreeMemoryInMB;

} DeviceInfos;

enum QueueFamilyIndex
{
	Graphics
};

enum DeviceMemoryTypeIndex
{
	DeviceMemory,
	HostVisibleMemory
};

// Retrieve info about hardware.
VkPhysicalDeviceMemoryBudgetPropertiesEXT PhysicalDeviceMemoryBudgetInfo
{
	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
	.pNext = nullptr,
	.heapBudget = {},
	.heapUsage = {}
};
VkPhysicalDeviceMemoryProperties2 DeviceMemoryInfo
{
	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
	.pNext = &PhysicalDeviceMemoryBudgetInfo,
	.memoryProperties = {}
};



struct SwapchainCreationInfo
{
	VkPresentModeKHR ExposedPresentMode;
	VkSurfaceFormatKHR ExposedSurfaceFormat;
};

std::vector<uint32_t> DeviceMemoryTypeIndices(3, 0);

VkDevice CreateDevice(VkInstance Instance, DeviceInfo& Infos, VkSurfaceKHR SwapchainSurface, std::vector<uint32_t>& QueueFamilyIndices, SwapchainCreationInfo& SwapchainInfo)
{
	const std::vector<VkQueueFlagBits> RequiredQueueBits
	{
		VK_QUEUE_GRAPHICS_BIT,
	};
	const std::vector<QueueFamilyIndex> RequiredQueueIndices
	{
		Graphics
	};
	const std::vector<float> RequiredQueuePriorities
	{
		1.0f
	};
	const std::vector<const char*> RequiredDeviceExtensions
	{
		"VK_KHR_swapchain"
	};

	VkDevice DeviceCache = 0;

	uint32_t DevicesCount = 0;
	vkEnumeratePhysicalDevices(Instance, &DevicesCount, nullptr);
	std::vector<VkPhysicalDevice> Devices(DevicesCount);
	vkEnumeratePhysicalDevices(Instance, &DevicesCount, Devices.data());

	if (Devices.empty())
	{
		std::cerr << "No available Vulkan device detected." << std::endl;
	}
	
	VkPhysicalDeviceCoherentMemoryFeaturesAMD CoherentMemoryFeatureAMD
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD,
		.pNext = nullptr
	};
	VkPhysicalDeviceProperties2 DeviceProperties;
	VkPhysicalDeviceDriverProperties DeviceDriverProperties;

	for (const auto& PhysicalDevice : Devices)
	{
		DeviceProperties.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		DeviceProperties.pNext = &DeviceDriverProperties;
		DeviceDriverProperties.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
		DeviceDriverProperties.pNext = nullptr;
		vkGetPhysicalDeviceProperties2(PhysicalDevice, &DeviceProperties);

		const auto SupportedMajorVKVersion = VK_VERSION_MAJOR(DeviceProperties.properties.apiVersion);
		const auto SupportedMinorVKVersion = VK_VERSION_MINOR(DeviceProperties.properties.apiVersion);

		// App requires Vulkan 1.3 Core version support.
		if (SupportedMajorVKVersion < 1)
		{
			continue;
		}
		else if (SupportedMajorVKVersion == 1)
		{
			if (SupportedMinorVKVersion < 3)
			{
				continue;
			}
		}

		// Prevent to use lava pipe.
		if (DeviceProperties.properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU)
			continue;

		// Check that device supports required queues.
		uint32_t QueueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilies.data());
		
		QueueFamilyIndices = std::vector<uint32_t>(RequiredQueueBits.size(), -1);
		for (size_t RequiredQueueBitID = 0; RequiredQueueBitID < RequiredQueueBits.size(); RequiredQueueBitID++)
		{
			for (size_t QueueFamilyID = 0; QueueFamilyID < QueueFamilies.size(); QueueFamilyID++)
			{
				if (QueueFamilies[QueueFamilyID].queueFlags & RequiredQueueBits[RequiredQueueBitID])
				{
					QueueFamilyIndices[RequiredQueueIndices[RequiredQueueBitID]] = QueueFamilyID;
					break;
				}
			}
		}

		// Check that GPU supports AMD specific extensions
		{
			VkPhysicalDeviceFeatures2 Features
			{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				.pNext = &CoherentMemoryFeatureAMD,
			};

			vkGetPhysicalDeviceFeatures2(PhysicalDevice, &Features);
		}
		
		// If not support - queue next device.
		bool QueueBitsSupported = true;
		for (const auto& QueueFamily : QueueFamilyIndices)
		{
			if (QueueFamily == -1)
			{
				QueueBitsSupported = false;
				break;
			}
		}

		if (!QueueBitsSupported)
		{
			continue;
		}

		// Check for hardware color space and color format support.
		uint32_t SupportedFormatsCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, SwapchainSurface, &SupportedFormatsCount, nullptr);
		std::vector<VkSurfaceFormatKHR> SupportedFormats(SupportedFormatsCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, SwapchainSurface, &SupportedFormatsCount, SupportedFormats.data());

		if (!SupportedFormats.size())
			continue;

		SwapchainInfo.ExposedSurfaceFormat = SupportedFormats[0];

		// Check for present mode support.
		uint32_t SupportedSurfacePresentModesCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, SwapchainSurface, &SupportedSurfacePresentModesCount, nullptr);
		std::vector<VkPresentModeKHR> PresentModes(SupportedSurfacePresentModesCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, SwapchainSurface, &SupportedSurfacePresentModesCount, PresentModes.data());
		bool PresentModeSupported = false;

		if (!PresentModes.size())
			continue;

		 SwapchainInfo.ExposedPresentMode = PresentModes[0];

		// Describe needed features.
		VkPhysicalDeviceVulkan13Features Vulkan13Features
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = nullptr,
			.synchronization2 = true
		};
		VkPhysicalDeviceVulkan12Features Vulkan12Features
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = &Vulkan13Features,
			.separateDepthStencilLayouts = true
		};
		
		
#if TUTORIAL_VK_FORCE_DEVICE_VENDOR == TUTORIAL_VK_DEVICE_VENDOR_AMD
		if (DeviceProperties.properties.deviceName[0] != 'A')
			continue;
#endif
#if TUTORIAL_VK_FORCE_DEVICE_VENDOR == TUTORIAL_VK_DEVICE_VENDOR_NVIDIA
		if (DeviceProperties.properties.deviceName[0] != 'N')
			continue;
#endif

		//std::cout <<  << std::endl;

		if (CoherentMemoryFeatureAMD.deviceCoherentMemory)
		{
			Vulkan13Features.pNext = &CoherentMemoryFeatureAMD;
		}

		// Create device and queues.
		VkDeviceQueueCreateInfo DeviceQueueCreationInfo{};
		DeviceQueueCreationInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		DeviceQueueCreationInfo.queueCount = 1;
		DeviceQueueCreationInfo.queueFamilyIndex = QueueFamilyIndices[0];
		DeviceQueueCreationInfo.pQueuePriorities = RequiredQueuePriorities.data();

		VkDeviceCreateInfo DeviceCreationInfo{};
		DeviceCreationInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		DeviceCreationInfo.pQueueCreateInfos = &DeviceQueueCreationInfo;
		DeviceCreationInfo.queueCreateInfoCount = 1;
		DeviceCreationInfo.enabledExtensionCount = RequiredDeviceExtensions.size();
		DeviceCreationInfo.ppEnabledExtensionNames = RequiredDeviceExtensions.data();
		DeviceCreationInfo.pNext = &Vulkan12Features;		

		vkCreateDevice(PhysicalDevice, &DeviceCreationInfo, nullptr, &DeviceCache);

		vkGetPhysicalDeviceMemoryProperties2(PhysicalDevice, &DeviceMemoryInfo);
		
		Infos.HardwareName = std::string(DeviceProperties.properties.deviceName);
		Infos.DriverVersion = std::string(DeviceDriverProperties.driverInfo);
		for (int i = 0; i < DeviceMemoryInfo.memoryProperties.memoryHeapCount; i++)
		{
			if (DeviceMemoryInfo.memoryProperties.memoryHeaps[i].flags & VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			{
				Infos.TotalMemoryInMB = DeviceMemoryInfo.memoryProperties.memoryHeaps[i].size / 1024 / 1024;
				Infos.FreeMemoryInMB = PhysicalDeviceMemoryBudgetInfo.heapBudget[i] / 1024 / 1024;
				break;
			}
		}

		for (int i = 0; i < DeviceMemoryInfo.memoryProperties.memoryTypeCount; i++)
		{
			if (DeviceMemoryInfo.memoryProperties.memoryTypes[i].propertyFlags & VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			{
				DeviceMemoryTypeIndices[DeviceMemoryTypeIndex::DeviceMemory] = i;
			}
			if (DeviceMemoryInfo.memoryProperties.memoryTypes[i].propertyFlags & VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
			{
				DeviceMemoryTypeIndices[DeviceMemoryTypeIndex::HostVisibleMemory] = i;
			}
		}
		
		break;
	}

	return DeviceCache;
}

void SetupActor(VkDevice Device, SceneActor& Actor, const tnr::m3d::wavefront::tnrObject& LoadedObjectData)
{
	std::vector<RequiredMemory> BufferMemorySegments(2);

	VkMemoryRequirements MemRequirementsCache{};

	// Create GPU buffers.
	{
		// Create vertex position buffer.
		{
			VkBufferCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.size = sizeof(LoadedObjectData.Positions[0]) * LoadedObjectData.Positions.size(),
				.usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 0,
				.pQueueFamilyIndices = nullptr
			};

			vkCreateBuffer(Device, &CreationInfo, nullptr, &Actor.VertexBuffers[SceneActor::BufferType::Position]);

			vkGetBufferMemoryRequirements(Device, Actor.VertexBuffers[SceneActor::BufferType::Position], &MemRequirementsCache);
			BufferMemorySegments[SceneActor::BufferType::Position] = ComputeMemorySegments(MemRequirementsCache);
		}

		// Create vertex normal buffer.
		{
			VkBufferCreateInfo CreationInfo
			{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.size = sizeof(LoadedObjectData.Normals[0]) * LoadedObjectData.Normals.size(),
				.usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 0,
				.pQueueFamilyIndices = nullptr
			};

			vkCreateBuffer(Device, &CreationInfo, nullptr, &Actor.VertexBuffers[SceneActor::BufferType::Normal]);

			VkMemoryRequirements MemRequirementsCache{};
			vkGetBufferMemoryRequirements(Device, Actor.VertexBuffers[SceneActor::BufferType::Normal], &MemRequirementsCache);
			BufferMemorySegments[SceneActor::BufferType::Normal] = ComputeMemorySegments(MemRequirementsCache);
		}
	}
	
	// Allocate memory for buffers.
	{
		size_t SummedSize = 0;
		for (const auto Offset : BufferMemorySegments)
		{
			SummedSize += Offset.SegmentsCount * Offset.SegmentSize;
		}

		VkMemoryAllocateInfo SceneBufferAllocationInfo
		{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = nullptr,
			.allocationSize = SummedSize,
			.memoryTypeIndex = QueryMemoryTypeIndex(VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, MemRequirementsCache.memoryTypeBits, DeviceMemoryInfo)
		};

		vkAllocateMemory(Device, &SceneBufferAllocationInfo, nullptr, &Actor.ActorBuffersGPUMemory);
	}

	// Associate memory with buffers.
	{
		size_t SummedSize = 0;
		for (size_t i = 0; i < BufferMemorySegments.size(); i++)
		{
			vkBindBufferMemory(Device, Actor.VertexBuffers[i], Actor.ActorBuffersGPUMemory, SummedSize);
			SummedSize += BufferMemorySegments[i].SegmentsCount * BufferMemorySegments[i].SegmentSize;
		}
	}

	// Upload data into buffers.
	{
		VkMappedMemoryRange RangeInfo
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
			.pNext = nullptr,
			.memory = Actor.ActorBuffersGPUMemory,
			.offset = 0,
			.size = VK_WHOLE_SIZE
		};

		void* MapAddress = nullptr;
		auto result = vkMapMemory(Device, Actor.ActorBuffersGPUMemory, 0, VK_WHOLE_SIZE, 0, &MapAddress);
		char* ArithmethicableAddress = reinterpret_cast<char*>(MapAddress);
		std::memcpy(ArithmethicableAddress, LoadedObjectData.Positions.data(), LoadedObjectData.Positions.size() * sizeof(LoadedObjectData.Positions[0]));
		ArithmethicableAddress += BufferMemorySegments[SceneActor::BufferType::Position].SegmentsCount * BufferMemorySegments[SceneActor::BufferType::Position].SegmentSize;
		std::memcpy(ArithmethicableAddress, LoadedObjectData.Normals.data(), LoadedObjectData.Normals.size() * sizeof(LoadedObjectData.Normals[0]));

		vkFlushMappedMemoryRanges(Device, 1, &RangeInfo);

		vkUnmapMemory(Device, Actor.ActorBuffersGPUMemory);
	}

	Actor.VerticesCount = LoadedObjectData.Positions.size();
}

std::vector<SceneActor> Actors;

void LoadScene(VkDevice Device)
{
	if (TUTORIAL_VK_DEBUG_DEALLOCATIONS)
	{
		std::cout << "Loading scene from disk..." << std::endl;

		const auto StartTime = std::chrono::system_clock::now();

		tnr::m3d::wavefront::tnrWavefrontLoader Loader(tnr::m3d::wavefront::tnrWavefrontOpenFlag::FLIP_POSITION_Y_AXIS, "vulkan_scene.obj", "vulkan_scene.mtl");

		const auto FinishTime = std::chrono::system_clock::now();

		auto Geometry = Loader.GetLoadedObjects();

		for (const auto& Obj : Geometry)
		{
			std::cout << "\nLoaded object" << std::endl;
			std::cout << "\tName: " << Obj.ObjectName << std::endl;
			std::cout << "\tTriangles: " << Obj.Positions.size() / 3 << std::endl;
		}

		std::cout << "\nLoading finished in " << std::chrono::duration_cast<std::chrono::seconds>(FinishTime - StartTime).count() << "s.\n" << std::endl; // Reference time is 24 seconds.
	
		std::cout << "Uploading scene into GPU memory...";

		for (const auto& Obj : Geometry)
		{
			SceneActor UnitializedActor{};
			SetupActor(Device, UnitializedActor, Obj);

			Actors.push_back(UnitializedActor);
		}

		std::cout << "Finished.\n" << std::endl;
	}

	if (!TUTORIAL_VK_DEBUG_DEALLOCATIONS)
	{
		std::cout << "Loading scene actors cancelled due debugging purposes." << std::endl;
	}
}

void MakeImageTransition(VkCommandBuffer CommandBuffer, VkImage TransitionedImage, VkImageLayout LayoutBeforeTransition, VkImageLayout LayoutAfterTransition, uint32_t GraphicsQueueIndex)
{
	VkImageSubresourceRange SubresourceInfo
	{
		.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	VkImageMemoryBarrier2 MemoryBarrierInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		.srcAccessMask = VkAccessFlagBits::VK_ACCESS_MEMORY_WRITE_BIT,
		.dstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		.dstAccessMask = VkAccessFlagBits::VK_ACCESS_MEMORY_WRITE_BIT,
		.oldLayout = LayoutBeforeTransition,
		.newLayout = LayoutAfterTransition,
		.srcQueueFamilyIndex = GraphicsQueueIndex,
		.dstQueueFamilyIndex = GraphicsQueueIndex,
		.image = TransitionedImage,
		.subresourceRange = SubresourceInfo
	};

	VkDependencyInfo DependencyInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.dependencyFlags = 0,
		.memoryBarrierCount = 0,
		.pMemoryBarriers = nullptr,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers = nullptr,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &MemoryBarrierInfo
	};

	vkCmdPipelineBarrier2(CommandBuffer, &DependencyInfo);
}

int main()
{
	// Instance initialization.
	VkInstance Instance;
	{
		std::vector<const char*> InstanceExtensions
		{
			"VK_KHR_surface",
			"VK_KHR_win32_surface"
		};
		std::vector<const char*> ValidationLayer
		{
			"VK_LAYER_KHRONOS_validation"
		};
		VkInstanceCreateInfo CreationInfo{};
		CreationInfo.enabledExtensionCount = 2;
		CreationInfo.ppEnabledExtensionNames = InstanceExtensions.data();
		CreationInfo.enabledLayerCount = 1;
		CreationInfo.ppEnabledLayerNames = ValidationLayer.data();
		CreationInfo.pApplicationInfo = &AppInfo;
		CreationInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		if (vkCreateInstance(&CreationInfo, nullptr, &Instance) != VK_SUCCESS)
		{
			std::cerr << "Failed to create Vulkan Instance." << std::endl;
			exit(0);
		}
	}

	// Presentation window creation.
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* PresentationWindow = glfwCreateWindow(1600, 900, "Vulkan Tutorial", nullptr, nullptr);
	if (!PresentationWindow)
	{
		std::cerr << "Failed to create window." << std::endl;
		exit(0);
	}
	

	// Window surface creation.
	VkSurfaceKHR Surface{};
	{
		VkWin32SurfaceCreateInfoKHR CreationInfo{};
		CreationInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		CreationInfo.hwnd = glfwGetWin32Window(PresentationWindow);
		CreationInfo.hinstance = GetModuleHandleA(nullptr);

		if (vkCreateWin32SurfaceKHR(Instance, &CreationInfo, nullptr, &Surface) != VK_SUCCESS)
		{
			std::cerr << "Failed to associate window with Vulkan surface." << std::endl;
			exit(0);
		}
	}

	// Device selection.
	std::vector<uint32_t> QueueFamiliesIndices;
	VkQueue GraphicsQueue{};
	SwapchainCreationInfo SupportedSwapchainCapabilities;
	VkDevice Device = CreateDevice(Instance, DeviceInfos, Surface, QueueFamiliesIndices, SupportedSwapchainCapabilities);
	if (Device)
	{
		std::cout << "Selected device:" << std::endl;
		std::cout << "GPU: " << DeviceInfos.HardwareName << std::endl;
		std::cout << "Driver: " << DeviceInfos.DriverVersion << std::endl;
		std::cout << "Total memory: " << DeviceInfos.TotalMemoryInMB << "MB" << std::endl;
		std::cout << "Available free memory: " << DeviceInfos.FreeMemoryInMB << "MB" << std::endl;
	}
	else
	{
		std::cerr << "No compatible with Vulkan 1.3 device found." << std::endl;
		exit(0);
	}
	vkGetDeviceQueue(Device, QueueFamiliesIndices[QueueFamilyIndex::Graphics], 0, &GraphicsQueue);

	// Swapchain creation.
	VkSwapchainKHR Swapchain{};
	{
		VkSwapchainCreateInfoKHR CreationInfo{};
		CreationInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		CreationInfo.surface = Surface;
		CreationInfo.minImageCount = 2;
		CreationInfo.imageFormat = SupportedSwapchainCapabilities.ExposedSurfaceFormat.format;
		CreationInfo.imageColorSpace = SupportedSwapchainCapabilities.ExposedSurfaceFormat.colorSpace;
		CreationInfo.imageExtent.width = 1600;
		CreationInfo.imageExtent.height = 900;
		CreationInfo.imageArrayLayers = 1;
		CreationInfo.imageUsage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		CreationInfo.imageSharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
		CreationInfo.pQueueFamilyIndices = reinterpret_cast<const uint32_t*>(QueueFamiliesIndices.data());
		CreationInfo.preTransform = VkSurfaceTransformFlagBitsKHR::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		CreationInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		CreationInfo.presentMode = SupportedSwapchainCapabilities.ExposedPresentMode;
		CreationInfo.clipped = VK_TRUE;

		if (vkCreateSwapchainKHR(Device, &CreationInfo, nullptr, &Swapchain) != VK_SUCCESS)
		{
			std::cerr << "Failed to create swapchain." << std::endl;
			exit(0);
		}
	}
	// Retrieve swapchain buffers.
	std::vector<VkImage> SwapchainBuffers;
	{
		uint32_t ImagesCount = 0;
		vkGetSwapchainImagesKHR(Device, Swapchain, &ImagesCount, nullptr);
		SwapchainBuffers.resize(ImagesCount);
		vkGetSwapchainImagesKHR(Device, Swapchain, &ImagesCount, SwapchainBuffers.data());
	}

	// Create swapchain buffer view.
	std::vector<VkImageView> SwapchainBuffersViews;
	{
		for (const auto& SwapchainBuffer : SwapchainBuffers)
		{
			VkImageViewCreateInfo CreationInfo{};
			CreationInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			CreationInfo.image = SwapchainBuffer;
			CreationInfo.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
			CreationInfo.format = SupportedSwapchainCapabilities.ExposedSurfaceFormat.format;
			CreationInfo.components.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
			CreationInfo.components.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
			CreationInfo.components.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
			CreationInfo.components.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
			CreationInfo.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
			CreationInfo.subresourceRange.baseMipLevel = 0;
			CreationInfo.subresourceRange.levelCount = 1;
			CreationInfo.subresourceRange.baseArrayLayer = 0;
			CreationInfo.subresourceRange.layerCount = 1;

			VkImageView BufferView;
			vkCreateImageView(Device, &CreationInfo, nullptr, &BufferView);

			SwapchainBuffersViews.push_back(BufferView);
		}
	}

	LoadScene(Device);

	// Setup depth buffer.
	VkImage DepthBuffer{};
	VkImageView DepthBufferView{};
	VkDeviceMemory DepthBufferMemory;
	{
		const uint32_t QueueIndex = static_cast<uint32_t>(QueueFamiliesIndices[QueueFamilyIndex::Graphics]);

		VkImageCreateInfo ImageCreationInfo
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.imageType = VkImageType::VK_IMAGE_TYPE_2D,
			.format = VkFormat::VK_FORMAT_D32_SFLOAT,
			.extent =
			VkExtent3D
			{
				.width = 1600,
				.height = 900,
				.depth = 1
			},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
			.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
			.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 1,
			.pQueueFamilyIndices = &QueueIndex,
			.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED
		};

		vkCreateImage(Device, &ImageCreationInfo, nullptr, &DepthBuffer);

		VkMemoryRequirements MemoryRequirements{};
		vkGetImageMemoryRequirements(Device, DepthBuffer, &MemoryRequirements);

		VkMemoryAllocateInfo AllocationInfo
		{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = nullptr,
			.allocationSize = MemoryRequirements.size,
			.memoryTypeIndex = QueryMemoryTypeIndex(VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MemoryRequirements.memoryTypeBits, DeviceMemoryInfo)
		};

		vkAllocateMemory(Device, &AllocationInfo, nullptr, &DepthBufferMemory);

		vkBindImageMemory(Device, DepthBuffer, DepthBufferMemory, 0);

		VkImageSubresourceRange SubresourceRangeInfo
		{
			.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		};

		VkImageViewCreateInfo ImageViewCreationInfo
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = DepthBuffer,
			.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
			.format = VkFormat::VK_FORMAT_D32_SFLOAT,
			.components = VkComponentMapping
			{
				.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange = SubresourceRangeInfo
		};

		vkCreateImageView(Device, &ImageViewCreationInfo, nullptr, &DepthBufferView);
	}

	// Create command buffers.
	VkCommandPool CommandPool{};
	VkCommandBuffer CommandBuffer{};
	{
		VkCommandPoolCreateInfo CreationInfo{};
		CreationInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		CreationInfo.queueFamilyIndex = QueueFamiliesIndices[QueueFamilyIndex::Graphics];
		CreationInfo.flags = VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		vkCreateCommandPool(Device, &CreationInfo, nullptr, &CommandPool);

		{
			VkCommandBufferAllocateInfo AllocationInfo
			{
				.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = CommandPool,
				.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};

			vkAllocateCommandBuffers(Device, &AllocationInfo, &CommandBuffer);
		}
	}

	std::unique_ptr<GBufferGenerationPass> GBufferGeneration = std::make_unique<GBufferGenerationPass>(Device, QueueFamiliesIndices[QueueFamilyIndex::Graphics], DeviceMemoryInfo, DepthBufferView);
	GBufferGeneration->SetupShaders();
	GBufferGeneration->SetupPipeline();

	std::unique_ptr<ShadowMapGenerationPass> ShadowMapGeneration = std::make_unique<ShadowMapGenerationPass>(Device, QueueFamiliesIndices[QueueFamilyIndex::Graphics], DeviceMemoryInfo);
	ShadowMapGeneration->SetupShaders();
	ShadowMapGeneration->SetupPipeline();

	DeferredAdditionalRequiredInfo AdditionalInfo
	{
		.GBufferPositionView = GBufferGeneration->SharedResources.GBufferPositionImageViewLink,
		.GBufferNormalView = GBufferGeneration->SharedResources.GBufferNormalImageViewLink,
		.LightSpaceUniformBuffer = ShadowMapGeneration->SharedResources.LightSpaceUniformBuffer,
		.VarianceShadowMapView = ShadowMapGeneration->SharedResources.VarianceShadowMap
	};

	std::unique_ptr<DeferredPass> DeferredShading = std::make_unique<DeferredPass>(Device, QueueFamiliesIndices[QueueFamilyIndex::Graphics], DeviceMemoryInfo, AdditionalInfo);
	DeferredShading->SetupShaders();
	DeferredShading->SetupPipeline();

	VkFence PresentationFence{};
	{
		VkFenceCreateInfo CreationInfo{};
		CreationInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		CreationInfo.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;

		vkCreateFence(Device, &CreationInfo, nullptr, &PresentationFence);
	}
	VkSemaphore QueueSemaphore{};
	VkSemaphore AcquireNextImageSemaphore{};
	{
		VkSemaphoreCreateInfo CreationInfo
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0
		};

		vkCreateSemaphore(Device, &CreationInfo, nullptr, &QueueSemaphore);
		vkCreateSemaphore(Device, &CreationInfo, nullptr, &AcquireNextImageSemaphore);
	}

	// Setup presentation.	
	uint32_t ImageIndex = 0;
	VkPresentInfoKHR PresentInfo
	{
		.sType = VkStructureType::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &QueueSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &Swapchain,
		.pImageIndices = &ImageIndex,
		.pResults = nullptr
	};

	// Record command buffer.
	VkCommandBufferBeginInfo BeginInfo
	{
		.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pInheritanceInfo = nullptr
	};
	

	VkSemaphoreSubmitInfo QueueSemaphoreSubmitInfo
	{
		.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext = nullptr,
		.semaphore = QueueSemaphore,
		.value = 1,
		.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
		.deviceIndex = 0
	};
	VkSemaphoreSubmitInfo PresentationSemaphoreSubmitInfo
	{
		.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext = nullptr,
		.semaphore = AcquireNextImageSemaphore,
		.value = 1,
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.deviceIndex = 0
	};

	VkCommandBufferSubmitInfo CmdBufSubmitInfo
	{
		.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext = nullptr,
		.commandBuffer = CommandBuffer,
		.deviceMask = 0
	};

	VkSubmitInfo2 SubmitInfo
	{
		.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pNext = nullptr,
		.flags = 0,
		.waitSemaphoreInfoCount = 1,
		.pWaitSemaphoreInfos = &PresentationSemaphoreSubmitInfo,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &CmdBufSubmitInfo,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &QueueSemaphoreSubmitInfo
	};


	// Main app loop.
	while (!glfwWindowShouldClose(PresentationWindow) && TUTORIAL_VK_DEBUG_DEALLOCATIONS)
	{
		glfwPollEvents();

		vkResetFences(Device, 1, &PresentationFence);		
		vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX, AcquireNextImageSemaphore, VK_NULL_HANDLE, &ImageIndex);
		vkBeginCommandBuffer(CommandBuffer, &BeginInfo);

		GBufferGeneration->RecordCommandBuffer(CommandBuffer, Actors);
		ShadowMapGeneration->RecordCommandBuffer(CommandBuffer, Actors);
		DeferredShading->RecordCommandBuffer(CommandBuffer);
		{
			MakeImageTransition(CommandBuffer, *DeferredShading->SharedResources.ResultImage, VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, QueueFamiliesIndices[QueueFamilyIndex::Graphics]);
			MakeImageTransition(CommandBuffer, SwapchainBuffers[ImageIndex], VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, QueueFamiliesIndices[QueueFamilyIndex::Graphics]);
			// There will be copy already rendered frame into swapchain.
			VkImageSubresourceLayers SrcLayersInfo
			{
				.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1
			};

			VkImageSubresourceLayers DstLayersInfo
			{
				.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1
			};

			VkImageBlit BlitInfo
			{
				.srcSubresource = SrcLayersInfo,
				.srcOffsets =
				{
					{
						.x = 0,
						.y = 0,
						.z = 0
					},
					{
						.x = 1600,
						.y = 900,
						.z = 1
					}
				},
				.dstSubresource = DstLayersInfo,
				.dstOffsets =
				{
					{
						.x = 0,
						.y = 0,
						.z = 0
					},
					{
						.x = 1600,
						.y = 900,
						.z = 1
					}
				}
			};

			vkCmdBlitImage(CommandBuffer, *DeferredShading->SharedResources.ResultImage, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SwapchainBuffers[ImageIndex], VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &BlitInfo, VkFilter::VK_FILTER_NEAREST);
			MakeImageTransition(CommandBuffer, SwapchainBuffers[ImageIndex], VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, QueueFamiliesIndices[QueueFamilyIndex::Graphics]);
		}

		vkEndCommandBuffer(CommandBuffer);
		vkQueueSubmit2(GraphicsQueue, 1, &SubmitInfo, PresentationFence);
		vkWaitForFences(Device, 1, &PresentationFence, true, UINT64_MAX);
		vkQueuePresentKHR(GraphicsQueue, &PresentInfo);

#ifdef TUTORIAL_VK_DEBUG_COMMAND_BUFFER_SUBMIT
		break;
#endif
	}

	// Clean up.
	vkDeviceWaitIdle(Device);

	GBufferGeneration->FreeGPUResources();
	ShadowMapGeneration->FreeGPUResources();
	DeferredShading->FreeGPUResources();

	for (auto& Actor : Actors)
	{
		for (auto& VertexBuffer : Actor.VertexBuffers)
		{
			vkDestroyBuffer(Device, VertexBuffer, nullptr);
		}
		vkFreeMemory(Device, Actor.ActorBuffersGPUMemory, nullptr);
	}

	vkDestroyImageView(Device, DepthBufferView, nullptr);
	vkDestroyImage(Device, DepthBuffer, nullptr);
	vkFreeMemory(Device, DepthBufferMemory, nullptr);

	vkDestroyCommandPool(Device, CommandPool, nullptr);
	vkDestroyFence(Device, PresentationFence, nullptr);
	vkDestroySemaphore(Device, QueueSemaphore, nullptr);
	vkDestroySemaphore(Device, AcquireNextImageSemaphore, nullptr);
	for (const auto& SwapchainBufferView : SwapchainBuffersViews)
	{
		vkDestroyImageView(Device, SwapchainBufferView, nullptr);
	}
	vkDestroySwapchainKHR(Device, Swapchain, nullptr);
	vkDestroySurfaceKHR(Instance, Surface, nullptr);
	vkDestroyDevice(Device, nullptr);
	vkDestroyInstance(Instance, nullptr);
	glfwTerminate();

	// Exit from app.
	return 0;
}