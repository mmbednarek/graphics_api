#include "Instance.h"

#include "vulkan/Util.h"

#include <spdlog/spdlog.h>

namespace triglav::graphics_api {

namespace {


template<VkPhysicalDeviceType CType>
bool physical_device_pick_predicate(VkPhysicalDevice physicalDevice)
{
   VkPhysicalDeviceProperties props{};
   vkGetPhysicalDeviceProperties(physicalDevice, &props);
   return props.deviceType == CType;
}

auto create_physical_device_pick_predicate(const DevicePickStrategy strategy)
{
   switch (strategy) {
   case DevicePickStrategy::PreferDedicated:
      return physical_device_pick_predicate<VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU>;
   case DevicePickStrategy::PreferIntegrated:
      return physical_device_pick_predicate<VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU>;
   }

   return +[](VkPhysicalDevice /*physicalDevice*/) -> bool { return true; };
}

VKAPI_ATTR VkBool32 VKAPI_CALL validation_layers_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                          VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                          const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
   spdlog::level::level_enum logLevel{spdlog::level::off};

   switch (messageSeverity) {
   case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      logLevel = spdlog::level::trace;
      break;
   case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      logLevel = spdlog::level::info;
      break;
   case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      logLevel = spdlog::level::warn;
      break;
   case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      logLevel = spdlog::level::err;
      break;
   case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
      break;
   }

   spdlog::log(logLevel, "vk-validation: {}", pCallbackData->pMessage);
   return VK_FALSE;
}

}// namespace

DECLARE_VLK_ENUMERATOR(get_physical_devices, VkPhysicalDevice, vkEnumeratePhysicalDevices)
DECLARE_VLK_ENUMERATOR(get_queue_family_properties, VkQueueFamilyProperties, vkGetPhysicalDeviceQueueFamilyProperties)
DECLARE_VLK_ENUMERATOR(get_device_extension_properties, VkExtensionProperties, vkEnumerateDeviceExtensionProperties)

Instance::Instance(vulkan::Instance&& instance) :
    m_instance(std::move(instance))
#if GAPI_ENABLE_VALIDATION
    ,
    m_debugMessenger(*m_instance)
#endif
{
#if GAPI_ENABLE_VALIDATION
   VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo{};
   debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   debugMessengerInfo.pfnUserCallback = validation_layers_callback;
   debugMessengerInfo.pUserData = nullptr;

   if (const auto res = m_debugMessenger.construct(&debugMessengerInfo); res != VK_SUCCESS) {
      spdlog::error("failed to enable validation layers: {}", static_cast<i32>(res));
   }

#endif
}

Result<Surface> Instance::create_surface(const desktop::ISurface& surface) const
{
   vulkan::SurfaceKHR vulkan_surface(*m_instance);
   if (const auto res = vulkan_surface.construct(&surface); res != VK_SUCCESS)
      return std::unexpected(Status::UnsupportedDevice);

   return Surface{std::move(vulkan_surface)};
}

Result<DeviceUPtr> Instance::create_device(const Surface& surface, const DevicePickStrategy strategy) const
{
   auto physicalDevices = vulkan::get_physical_devices(*m_instance);
   auto pickedDevice = std::find_if(physicalDevices.begin(), physicalDevices.end(), create_physical_device_pick_predicate(strategy));
   if (pickedDevice == physicalDevices.end()) {
      pickedDevice = physicalDevices.begin();
   }

   auto queueFamilies = vulkan::get_queue_family_properties(*pickedDevice);

   std::vector<QueueFamilyInfo> queueFamilyInfos{};
   queueFamilyInfos.reserve(queueFamilies.size());

   u32 queueIndex{};
   for (const auto& family : queueFamilies) {
      VkBool32 canPresent{};
      if (vkGetPhysicalDeviceSurfaceSupportKHR(*pickedDevice, queueIndex, surface.vulkan_surface(), &canPresent) != VK_SUCCESS)
         return std::unexpected(Status::UnsupportedDevice);

      QueueFamilyInfo info{};
      info.index = queueIndex;
      info.queueCount = family.queueCount;
      info.flags = vulkan::vulkan_queue_flags_to_work_type_flags(family.queueFlags, canPresent);

      if (info.flags != WorkType::None) {
         queueFamilyInfos.emplace_back(info);
      }

      ++queueIndex;
   }

   std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
   deviceQueueCreateInfos.resize(queueFamilyInfos.size());

   u32 maxQueues = 0;
   for (const auto& info : queueFamilyInfos) {
      const auto queueCount = info.queueCount;
      if (queueCount > maxQueues) {
         maxQueues = queueCount;
      }
   }

   std::vector<float> queuePriorities{};
   queuePriorities.resize(maxQueues);
   std::ranges::fill(queuePriorities, 1.0f);

   auto deviceQueueCreateInfoIt = deviceQueueCreateInfos.begin();
   for (const auto& info : queueFamilyInfos) {
      deviceQueueCreateInfoIt->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      deviceQueueCreateInfoIt->queueCount = info.queueCount;
      deviceQueueCreateInfoIt->queueFamilyIndex = info.index;
      deviceQueueCreateInfoIt->pQueuePriorities = queuePriorities.data();
      ++deviceQueueCreateInfoIt;
   }

   std::vector<const char*> vulkanDeviceExtensions{
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
      "VK_KHR_shader_non_semantic_info",
   };

   const auto extensionProperties = vulkan::get_device_extension_properties(*pickedDevice, nullptr);
   for (const auto& property : extensionProperties) {
      const std::string extensionName{property.extensionName};
      if (extensionName == "VK_KHR_portability_subset") {
         vulkanDeviceExtensions.emplace_back("VK_KHR_portability_subset");
         break;
      }
   }

   VkPhysicalDeviceHostQueryResetFeatures hostQueryResetFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES};
   hostQueryResetFeatures.hostQueryReset = true;

   VkPhysicalDeviceFeatures2 deviceFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
   deviceFeatures.pNext = &hostQueryResetFeatures;
   deviceFeatures.features.sampleRateShading = true;
   deviceFeatures.features.logicOp = true;
   deviceFeatures.features.fillModeNonSolid = true;
   deviceFeatures.features.wideLines = true;
   deviceFeatures.features.samplerAnisotropy = true;

   VkDeviceCreateInfo deviceInfo{};
   deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   deviceInfo.pNext = &deviceFeatures;
   deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
   deviceInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
   deviceInfo.enabledExtensionCount = vulkanDeviceExtensions.size();
   deviceInfo.ppEnabledExtensionNames = vulkanDeviceExtensions.data();

   vulkan::Device device;
   if (const auto res = device.construct(*pickedDevice, &deviceInfo); res != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   return std::make_unique<Device>(std::move(device), *pickedDevice, std::move(queueFamilyInfos));
}

#if GAPI_ENABLE_VALIDATION
constexpr std::array g_vulkanInstanceLayers{
   "VK_LAYER_KHRONOS_validation",
};
#else
constexpr std::array<const char*, 0> g_vulkanInstanceLayers{};
#endif

Result<Instance> Instance::create_instance()
{
   VkApplicationInfo appInfo{};
   appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   appInfo.pApplicationName = "TRIGLAV Example";
   appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
   appInfo.pEngineName = "TRIGLAV Engine";
   appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
   appInfo.apiVersion = VK_API_VERSION_1_3;

   const std::array g_vulkanInstanceExtensions
   {
      VK_KHR_SURFACE_EXTENSION_NAME, triglav::desktop::vulkan_extension_name(),
#if GAPI_ENABLE_VALIDATION
         VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
   };

   VkInstanceCreateInfo instanceInfo{};
   instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   instanceInfo.pApplicationInfo = &appInfo;
   instanceInfo.enabledLayerCount = 0;
   instanceInfo.ppEnabledLayerNames = nullptr;
   instanceInfo.enabledExtensionCount = g_vulkanInstanceExtensions.size();
   instanceInfo.ppEnabledExtensionNames = g_vulkanInstanceExtensions.data();
   instanceInfo.enabledLayerCount = g_vulkanInstanceLayers.size();
   instanceInfo.ppEnabledLayerNames = g_vulkanInstanceLayers.data();

   vulkan::Instance instance;
   if (const auto res = instance.construct(&instanceInfo); res != VK_SUCCESS) {
      return std::unexpected(Status::UnsupportedDevice);
   }

   return Instance{std::move(instance)};
}

}// namespace triglav::graphics_api
