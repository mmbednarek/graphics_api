#include "Device.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <iostream>
#include <ostream>
#include <set>
#include <shared_mutex>
#include <vector>

#include "CommandList.h"
#include "vulkan/Util.h"

#undef max

using graphics_api::BufferPurpose;

namespace {
bool physical_device_pick_predicate(VkPhysicalDevice physicalDevice)
{
   VkPhysicalDeviceProperties props{};
   vkGetPhysicalDeviceProperties(physicalDevice, &props);
   return props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

bool graphics_family_predicate(const VkQueueFamilyProperties &properties)
{
   return (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
}

VkBufferUsageFlags map_buffer_purpose_to_usage_flags(const BufferPurpose purpose)
{
   switch (purpose) {
   case BufferPurpose::TransferBuffer: return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
   case BufferPurpose::VertexBuffer:
      return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
   case BufferPurpose::UniformBuffer: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
   case BufferPurpose::IndexBuffer:
      return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
   }
   return 0;
}

VkMemoryPropertyFlags map_buffer_purpose_to_memory_properties(const BufferPurpose purpose)
{
   switch (purpose) {
   case BufferPurpose::TransferBuffer://fallthrough
   case BufferPurpose::UniformBuffer:
      return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   case BufferPurpose::VertexBuffer://fallthrough
   case BufferPurpose::IndexBuffer: return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
   }
   return 0;
}

VKAPI_ATTR VkBool32 VKAPI_CALL validation_layers_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
   std::cerr << "[VALIDATION] " << pCallbackData->pMessage << '\n';
   return VK_FALSE;
}

}// namespace

namespace graphics_api {
DECLARE_VLK_ENUMERATOR(get_physical_devices, VkPhysicalDevice, vkEnumeratePhysicalDevices)
DECLARE_VLK_ENUMERATOR(get_queue_family_properties, VkQueueFamilyProperties,
                       vkGetPhysicalDeviceQueueFamilyProperties)
DECLARE_VLK_ENUMERATOR(get_surface_formats, VkSurfaceFormatKHR, vkGetPhysicalDeviceSurfaceFormatsKHR)
DECLARE_VLK_ENUMERATOR(get_swapchain_images, VkImage, vkGetSwapchainImagesKHR)

namespace {

bool is_surface_format_supported(const VkPhysicalDevice physicalDevice, const vulkan::SurfaceKHR &surface,
                                 const ColorFormat &colorFormat, const ColorSpace colorSpace)
{
   const auto formats = vulkan::get_surface_formats(physicalDevice, *surface);

   const auto targetVulkanFormat = vulkan::to_vulkan_color_format(colorFormat);
   if (not targetVulkanFormat.has_value()) {
      return false;
   }

   const auto targetVulkanColorSpace = vulkan::to_vulkan_color_space(colorSpace);
   if (not targetVulkanColorSpace.has_value()) {
      return false;
   }

   for (const auto [vulkanFormat, vulkanColorSpace] : formats) {
      if (vulkanFormat != targetVulkanFormat)
         continue;
      if (vulkanColorSpace != targetVulkanColorSpace)
         continue;

      return true;
   }

   return false;
}

}// namespace

Device::Device(vulkan::Instance instance,
#if GAPI_ENABLE_VALIDATION
               vulkan::DebugUtilsMessengerEXT debugMessenger,
#endif
               vulkan::SurfaceKHR surface, vulkan::Device device, const VkPhysicalDevice physicalDevice,
               const vulkan::QueueFamilyIndices &queueFamilies, vulkan::CommandPool commandPool) :
    m_instance(std::move(instance)),
#if GAPI_ENABLE_VALIDATION
    m_debugMessenger(std::move(debugMessenger)),
#endif
    m_surface(std::move(surface)),
    m_device(std::move(device)),
    m_physicalDevice(physicalDevice),
    m_queueFamilies(queueFamilies),
    m_commandPool(std::move(commandPool))
{
   vkGetDeviceQueue(*m_device, m_queueFamilies.graphicsQueue, 0, &m_graphicsQueue);
}

namespace {

uint32_t swapchain_image_count(const uint32_t min, const uint32_t max)
{
   if (max == 0) {
      return std::max(2u, min);
   }

   return std::min(std::max(2u, min), max);
}

}// namespace

Result<Swapchain> Device::create_swapchain(ColorFormat colorFormat, ColorSpace colorSpace,
                                           ColorFormat depthFormat, SampleCount sampleCount,
                                           const Resolution &resolution, Swapchain *oldSwapchain)
{
   if (not is_surface_format_supported(m_physicalDevice, m_surface, colorFormat, colorSpace))
      return std::unexpected(Status::UnsupportedFormat);

   const auto vulkanColorFormat = vulkan::to_vulkan_color_format(colorFormat);
   if (not vulkanColorFormat.has_value())
      return std::unexpected(Status::UnsupportedFormat);

   const auto vulkanDepthFormat = vulkan::to_vulkan_color_format(depthFormat);
   if (not vulkanDepthFormat.has_value())
      return std::unexpected(Status::UnsupportedFormat);

   const auto vulkanColorSpace = vulkan::to_vulkan_color_space(colorSpace);
   if (not vulkanColorSpace.has_value())
      return std::unexpected(Status::UnsupportedFormat);

   VkSurfaceCapabilitiesKHR capabilities;
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, *m_surface, &capabilities);

   VkSwapchainCreateInfoKHR swapchainInfo{};
   swapchainInfo.sType       = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   swapchainInfo.surface     = *m_surface;
   swapchainInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
   swapchainInfo.imageExtent = VkExtent2D{resolution.width, resolution.height};
   swapchainInfo.imageFormat = *vulkanColorFormat;
   swapchainInfo.imageUsage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
   if (oldSwapchain != nullptr) {
      swapchainInfo.oldSwapchain = oldSwapchain->vulkan_swapchain();
   }
   swapchainInfo.minImageCount =
           swapchain_image_count(capabilities.minImageCount, capabilities.maxImageCount);
   swapchainInfo.imageColorSpace  = *vulkanColorSpace;
   swapchainInfo.imageArrayLayers = 1;
   swapchainInfo.preTransform     = capabilities.currentTransform;
   swapchainInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   swapchainInfo.clipped          = true;

   const std::array queueFamilyIndices{m_queueFamilies.graphicsQueue, m_queueFamilies.presentQueue};
   if (m_queueFamilies.graphicsQueue != m_queueFamilies.presentQueue) {
      swapchainInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
      swapchainInfo.queueFamilyIndexCount = queueFamilyIndices.size();
      swapchainInfo.pQueueFamilyIndices   = queueFamilyIndices.data();
   } else {
      swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
   }

   vulkan::SwapchainKHR swapchain(*m_device);
   if (swapchain.construct(&swapchainInfo) != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedFormat);
   }

   auto depthAttachment =
           this->create_texture(depthFormat, resolution, TextureType::DepthBuffer, sampleCount);
   if (not depthAttachment.has_value()) {
      return std::unexpected(depthAttachment.error());
   }

   std::optional<Texture> colorAttachment{};

   if (sampleCount != SampleCount::Single) {
      auto texture =
              this->create_texture(colorFormat, resolution, TextureType::MultisampleImage, sampleCount);
      if (not texture.has_value()) {
         return std::unexpected(texture.error());
      }

      colorAttachment = std::move(*texture);
   }

   std::vector<vulkan::ImageView> swapchainImageViews;

   const auto images = vulkan::get_swapchain_images(*m_device, *swapchain);
   for (const auto image : images) {
      VkImageViewCreateInfo imageViewInfo{};
      imageViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      imageViewInfo.image                           = image;
      imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      imageViewInfo.format                          = *vulkanColorFormat;
      imageViewInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      imageViewInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      imageViewInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      imageViewInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      imageViewInfo.subresourceRange.baseMipLevel   = 0;
      imageViewInfo.subresourceRange.levelCount     = 1;
      imageViewInfo.subresourceRange.baseArrayLayer = 0;
      imageViewInfo.subresourceRange.layerCount     = 1;

      vulkan::ImageView imageView(*m_device);
      if (imageView.construct(&imageViewInfo) != VK_SUCCESS) {
         return std::unexpected(Status::UnsupportedDevice);
      }

      swapchainImageViews.emplace_back(std::move(imageView));
   }

   VkQueue presentQueue;
   vkGetDeviceQueue(*m_device, m_queueFamilies.presentQueue, 0, &presentQueue);

   return Swapchain(colorFormat, depthFormat, sampleCount, resolution, std::move(*depthAttachment),
                    std::move(colorAttachment), std::move(swapchainImageViews), presentQueue,
                    std::move(swapchain));
}

Result<RenderPass> Device::create_render_pass(IRenderTarget &renderTarget)
{
   const auto attachments  = renderTarget.vulkan_attachments();
   const auto subpass      = renderTarget.vulkan_subpass();
   const auto dependencies = renderTarget.vulkan_subpass_dependencies();

   VkRenderPassCreateInfo renderPassInfo{};
   renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassInfo.attachmentCount = attachments.size();
   renderPassInfo.pAttachments    = attachments.data();
   renderPassInfo.subpassCount    = 1;
   renderPassInfo.pSubpasses      = &subpass.description;
   renderPassInfo.dependencyCount = dependencies.size();
   renderPassInfo.pDependencies   = dependencies.data();

   vulkan::RenderPass renderPass(*m_device);
   if (const auto res = renderPass.construct(&renderPassInfo); res != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   VkPhysicalDeviceProperties properties{};
   vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

   return RenderPass(std::move(renderPass), renderTarget.resolution(), renderTarget.sample_count());
}

Result<Shader> Device::create_shader(const ShaderStage stage, const std::string_view entrypoint,
                                     const std::span<const uint8_t> code)
{
   VkShaderModuleCreateInfo shaderModuleInfo{};
   shaderModuleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   shaderModuleInfo.codeSize = code.size();
   shaderModuleInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

   vulkan::ShaderModule shaderModule(*m_device);
   if (shaderModule.construct(&shaderModuleInfo) != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   return Shader(std::string{entrypoint}, stage, std::move(shaderModule));
}

Result<CommandList> Device::create_command_list() const
{
   VkCommandBufferAllocateInfo allocateInfo{};
   allocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocateInfo.commandPool        = *m_commandPool;
   allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocateInfo.commandBufferCount = 1;

   VkCommandBuffer commandBuffer;
   if (vkAllocateCommandBuffers(*m_device, &allocateInfo, &commandBuffer) != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   return CommandList(commandBuffer, *m_device, *m_commandPool);
}

Result<Buffer> Device::create_buffer(const BufferPurpose purpose, const uint64_t size)
{
   VkBufferCreateInfo bufferInfo{};
   bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferInfo.size        = size;
   bufferInfo.usage       = map_buffer_purpose_to_usage_flags(purpose);
   bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   vulkan::Buffer buffer(*m_device);
   if (const auto res = buffer.construct(&bufferInfo); res != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   VkMemoryRequirements memRequirements;
   vkGetBufferMemoryRequirements(*m_device, *buffer, &memRequirements);

   VkMemoryAllocateInfo allocateInfo{};
   allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocateInfo.allocationSize  = memRequirements.size;
   allocateInfo.memoryTypeIndex = this->find_memory_type(memRequirements.memoryTypeBits,
                                                         map_buffer_purpose_to_memory_properties(purpose));

   vulkan::DeviceMemory memory(*m_device);
   if (memory.construct(&allocateInfo) != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   if (vkBindBufferMemory(*m_device, *buffer, *memory, 0) != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   return Buffer(size, std::move(buffer), std::move(memory));
}

Result<Fence> Device::create_fence() const
{
   VkFenceCreateInfo fenceInfo{};
   fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

   vulkan::Fence fence(*m_device);
   if (fence.construct(&fenceInfo) != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   return Fence(std::move(fence));
}

Result<Semaphore> Device::create_semaphore() const
{
   VkSemaphoreCreateInfo semaphoreInfo{};
   semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   vulkan::Semaphore semaphore(*m_device);
   if (semaphore.construct(&semaphoreInfo) != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   return Semaphore(std::move(semaphore));
}

namespace {

VkImageTiling to_vulkan_image_tiling(const TextureType type)
{
   switch (type) {
   case TextureType::SampledImage: return VK_IMAGE_TILING_LINEAR;
   case TextureType::DepthBuffer:       // fallthrough
   case TextureType::SampledDepthBuffer:// fallthrough
   case TextureType::MultisampleImage: return VK_IMAGE_TILING_OPTIMAL;
   }
   return {};
}

VkImageUsageFlags to_vulkan_image_usage(const TextureType type)
{
   switch (type) {
   case TextureType::SampledImage: return VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
   case TextureType::DepthBuffer: return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
   case TextureType::SampledDepthBuffer:
      return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
   case TextureType::MultisampleImage:
      return VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
   }
   return {};
}

VkImageAspectFlags to_vulkan_image_aspect(const TextureType type)
{
   switch (type) {
   case TextureType::SampledImage: return VK_IMAGE_ASPECT_COLOR_BIT;
   case TextureType::DepthBuffer: return VK_IMAGE_ASPECT_DEPTH_BIT;
   case TextureType::SampledDepthBuffer: return VK_IMAGE_ASPECT_DEPTH_BIT;
   case TextureType::MultisampleImage: return VK_IMAGE_ASPECT_COLOR_BIT;
   }
   return {};
}

}// namespace

Result<Texture> Device::create_texture(const ColorFormat &format, const Resolution &imageSize,
                                       const TextureType type, SampleCount sampleCount) const
{
   const auto vulkanColorFormat = *vulkan::to_vulkan_color_format(format);

   VkImageCreateInfo imageInfo{};
   imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   imageInfo.format        = vulkanColorFormat;
   imageInfo.extent        = VkExtent3D{imageSize.width, imageSize.height, 1};
   imageInfo.imageType     = VK_IMAGE_TYPE_2D;
   imageInfo.mipLevels     = 1;
   imageInfo.arrayLayers   = 1;
   imageInfo.tiling        = to_vulkan_image_tiling(type);
   imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   imageInfo.samples       = static_cast<VkSampleCountFlagBits>(sampleCount);
   imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
   imageInfo.usage         = to_vulkan_image_usage(type);

   vulkan::Image image(*m_device);
   if (image.construct(&imageInfo) != VK_SUCCESS)
      return std::unexpected(Status::UnsupportedDevice);

   VkMemoryRequirements memRequirements;
   vkGetImageMemoryRequirements(*m_device, *image, &memRequirements);

   VkMemoryAllocateInfo allocInfo{};
   allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = memRequirements.size;
   allocInfo.memoryTypeIndex =
           find_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

   vulkan::DeviceMemory imageMemory(*m_device);
   if (imageMemory.construct(&allocInfo) != VK_SUCCESS)
      return std::unexpected(Status::UnsupportedDevice);

   vkBindImageMemory(*m_device, *image, *imageMemory, 0);

   VkImageViewCreateInfo imageViewInfo{};
   imageViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   imageViewInfo.image                           = *image;
   imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   imageViewInfo.format                          = vulkanColorFormat;
   imageViewInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   imageViewInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   imageViewInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   imageViewInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   imageViewInfo.subresourceRange.aspectMask     = to_vulkan_image_aspect(type);
   imageViewInfo.subresourceRange.baseMipLevel   = 0;
   imageViewInfo.subresourceRange.levelCount     = 1;
   imageViewInfo.subresourceRange.baseArrayLayer = 0;
   imageViewInfo.subresourceRange.layerCount     = 1;

   vulkan::ImageView imageView(*m_device);
   if (imageView.construct(&imageViewInfo) != VK_SUCCESS)
      return std::unexpected(Status::UnsupportedDevice);

   return Texture(std::move(image), std::move(imageMemory), std::move(imageView), format, type,
                  imageSize.width, imageSize.height);
}

Result<Sampler> Device::create_sampler(const bool enableAnisotropy)
{
   VkPhysicalDeviceProperties properties{};
   vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

   VkSamplerCreateInfo samplerInfo{};
   samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   samplerInfo.magFilter               = VK_FILTER_LINEAR;
   samplerInfo.minFilter               = VK_FILTER_LINEAR;
   samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   samplerInfo.anisotropyEnable        = enableAnisotropy;
   samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
   samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
   samplerInfo.unnormalizedCoordinates = false;
   samplerInfo.compareEnable           = false;
   samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
   samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

   vulkan::Sampler sampler(*m_device);
   if (sampler.construct(&samplerInfo) != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   return Sampler(std::move(sampler));
}

Result<DepthRenderTarget> Device::create_depth_render_target(const ColorFormat &depthFormat,
                                                             const Resolution &resolution) const
{
   return DepthRenderTarget(*m_device, resolution, depthFormat);
}

std::pair<Resolution, Resolution> Device::get_surface_resolution_limits() const
{
   VkSurfaceCapabilitiesKHR capabilities;
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, *m_surface, &capabilities);

   return {
           Resolution{capabilities.minImageExtent.width, capabilities.minImageExtent.height},
           Resolution{capabilities.maxImageExtent.width, capabilities.maxImageExtent.height}
   };
}

Status Device::submit_command_list(const CommandList &commandList, const Semaphore &waitSemaphore,
                                   const Semaphore &signalSemaphore, const Fence &fence) const
{
   VkSubmitInfo submitInfo{};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

   const std::array waitSemaphores{waitSemaphore.vulkan_semaphore()};
   const std::array signalSemaphores{signalSemaphore.vulkan_semaphore()};
   static constexpr std::array<VkPipelineStageFlags, 1> waitStages{
           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
   const std::array commandBuffers{commandList.vulkan_command_buffer()};

   submitInfo.waitSemaphoreCount   = waitSemaphores.size();
   submitInfo.pWaitSemaphores      = waitSemaphores.data();
   submitInfo.pWaitDstStageMask    = waitStages.data();
   submitInfo.commandBufferCount   = commandBuffers.size();
   submitInfo.pCommandBuffers      = commandBuffers.data();
   submitInfo.signalSemaphoreCount = signalSemaphores.size();
   submitInfo.pSignalSemaphores    = signalSemaphores.data();

   if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, fence.vulkan_fence()) != VK_SUCCESS) {
      return Status::UnsupportedDevice;
   }

   return Status::Success;
}

Status Device::submit_command_list_one_time(const CommandList &commandList) const
{
   const auto vulkanCommandList = commandList.vulkan_command_buffer();

   VkSubmitInfo submitInfo{};
   submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers    = &vulkanCommandList;

   vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, nullptr);
   vkQueueWaitIdle(m_graphicsQueue);

   return Status::Success;
}

VkDevice Device::vulkan_device() const
{
   return *m_device;
}

void Device::await_all() const
{
   vkDeviceWaitIdle(*m_device);
}

uint32_t Device::find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
   VkPhysicalDeviceMemoryProperties memProperties;
   vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

   for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) &&
          (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
         return i;
      }
   }

   return 0;
}

constexpr std::array g_vulkanInstanceExtensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
#if GAPI_PLATFORM_WINDOWS
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif GAPI_PLATFORM_WAYLAND
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
#if GAPI_ENABLE_VALIDATION
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

constexpr std::array g_vulkanDeviceExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

constexpr std::array g_vulkanInstanceLayers{
#if GAPI_ENABLE_VALIDATION
        "VK_LAYER_KHRONOS_validation",
#endif
};

Result<DeviceUPtr> initialize_device(const Surface &surface)
{
   VkApplicationInfo appInfo{};
   appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   appInfo.pApplicationName   = "TRIGLAV Example";
   appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
   appInfo.pEngineName        = "TRIGLAV Engine";
   appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
   appInfo.apiVersion         = VK_API_VERSION_1_3;

   VkInstanceCreateInfo instanceInfo{};
   instanceInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   instanceInfo.pApplicationInfo        = &appInfo;
   instanceInfo.enabledLayerCount       = 0;
   instanceInfo.ppEnabledLayerNames     = nullptr;
   instanceInfo.enabledExtensionCount   = g_vulkanInstanceExtensions.size();
   instanceInfo.ppEnabledExtensionNames = g_vulkanInstanceExtensions.data();
   instanceInfo.enabledLayerCount       = g_vulkanInstanceLayers.size();
   instanceInfo.ppEnabledLayerNames     = g_vulkanInstanceLayers.data();

   vulkan::Instance instance;
   if (const auto res = instance.construct(&instanceInfo); res != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

#if GAPI_ENABLE_VALIDATION
   VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo{};
   debugMessengerInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   debugMessengerInfo.pfnUserCallback = validation_layers_callback;
   debugMessengerInfo.pUserData       = nullptr;

   vulkan::DebugUtilsMessengerEXT debugMessenger(*instance);
   if (const auto res = debugMessenger.construct(&debugMessengerInfo); res != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }
#endif

   auto physicalDevices = vulkan::get_physical_devices(*instance);
   auto pickedDevice =
           std::find_if(physicalDevices.begin(), physicalDevices.end(), physical_device_pick_predicate);
   if (pickedDevice == physicalDevices.end()) {
      pickedDevice = physicalDevices.begin();
   }

   vulkan::SurfaceKHR vulkan_surface(*instance);
   if (const auto res = vulkan::to_vulkan_surface(vulkan_surface, surface); res != Status::Success)
      return std::unexpected(res);

   auto queueFamilies = vulkan::get_queue_family_properties(*pickedDevice);

   auto graphicsQueue = std::find_if(queueFamilies.begin(), queueFamilies.end(), graphics_family_predicate);
   if (graphicsQueue == queueFamilies.end()) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   vulkan::QueueFamilyIndices queueFamilyIndices{
           .graphicsQueue = static_cast<uint32_t>(graphicsQueue - queueFamilies.begin()),
   };

   for (uint32_t i{0}; i < queueFamilies.size(); ++i) {
      VkBool32 supported;
      if (vkGetPhysicalDeviceSurfaceSupportKHR(*pickedDevice, i, *vulkan_surface, &supported) != VK_SUCCESS)
         return std::unexpected(Status::UnsupportedDevice);

      if (supported) {
         queueFamilyIndices.presentQueue = i;
         if (i != queueFamilyIndices.graphicsQueue) {
            break;
         }
      }
   }

   std::set<uint32_t> uniqueFamilies;
   uniqueFamilies.emplace(queueFamilyIndices.graphicsQueue);
   uniqueFamilies.emplace(queueFamilyIndices.presentQueue);

   std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
   deviceQueueCreateInfos.resize(uniqueFamilies.size());

   uint32_t maxQueues = 0;
   for (const auto index : uniqueFamilies) {
      const auto queueCount = queueFamilies[index].queueCount;
      if (queueCount > maxQueues) {
         maxQueues = queueCount;
      }
   }

   std::vector<float> queuePriorities{};
   queuePriorities.resize(maxQueues);
   std::ranges::fill(queuePriorities, 1.0f);

   std::transform(uniqueFamilies.begin(), uniqueFamilies.end(), deviceQueueCreateInfos.begin(),
                  [&queuePriorities, &queueFamilies](uint32_t index) {
                     VkDeviceQueueCreateInfo info{};
                     info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                     info.queueCount       = queueFamilies[index].queueCount;
                     info.queueFamilyIndex = index;
                     info.pQueuePriorities = queuePriorities.data();
                     return info;
                  });

   VkPhysicalDeviceFeatures deviceFeatures{
           .sampleRateShading = true,
           .logicOp           = true,
           .fillModeNonSolid  = true,
           .wideLines         = true,
           .samplerAnisotropy = true,
   };

   VkDeviceCreateInfo deviceInfo{};
   deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   deviceInfo.queueCreateInfoCount    = static_cast<uint32_t>(deviceQueueCreateInfos.size());
   deviceInfo.pQueueCreateInfos       = deviceQueueCreateInfos.data();
   deviceInfo.pEnabledFeatures        = &deviceFeatures;
   deviceInfo.enabledExtensionCount   = g_vulkanDeviceExtensions.size();
   deviceInfo.ppEnabledExtensionNames = g_vulkanDeviceExtensions.data();

   vulkan::Device device;
   if (const auto res = device.construct(*pickedDevice, &deviceInfo); res != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   VkCommandPoolCreateInfo commandPoolInfo{};
   commandPoolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   commandPoolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   commandPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsQueue;

   vulkan::CommandPool commandPool(*device);
   if (commandPool.construct(&commandPoolInfo) != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   return std::make_unique<Device>(std::move(instance),
#if GAPI_ENABLE_VALIDATION
                                   std::move(debugMessenger),
#endif
                                   std::move(vulkan_surface), std::move(device), *pickedDevice,
                                   queueFamilyIndices, std::move(commandPool));
}

}// namespace graphics_api
