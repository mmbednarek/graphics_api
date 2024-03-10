#include "Util.h"
#include "GraphicsApi.hpp"


#include <vulkan/vulkan.h>

namespace triglav::graphics_api::vulkan {
Result<VkFormat> to_vulkan_color_format(const ColorFormat &format)
{
   switch (format.order) {
   case ColorFormatOrder::R:
      switch (format.parts[0]) {
      case ColorFormatPart::sRGB: return VK_FORMAT_R8_SRGB;
      case ColorFormatPart::UNorm16: return VK_FORMAT_R16_UNORM;
      case ColorFormatPart::UInt: return VK_FORMAT_R8_UINT;
      case ColorFormatPart::Float16: return VK_FORMAT_R16_SFLOAT;
      case ColorFormatPart::Float32: return VK_FORMAT_R32_SFLOAT;
      default: break;
      }
      return std::unexpected{Status::UnsupportedFormat};
   case ColorFormatOrder::RGBA:
      switch (format.parts[0]) {
      case ColorFormatPart::sRGB: return VK_FORMAT_R8G8B8A8_SRGB;
      case ColorFormatPart::UNorm16: return VK_FORMAT_R16G16B16A16_UNORM;
      case ColorFormatPart::UInt: return VK_FORMAT_R8G8B8A8_UINT;
      case ColorFormatPart::Float16: return VK_FORMAT_R16G16B16A16_SFLOAT;
      case ColorFormatPart::Float32: return VK_FORMAT_R32G32B32A32_SFLOAT;
      default: break;
      }
      return std::unexpected{Status::UnsupportedFormat};
   case ColorFormatOrder::BGRA:
      switch (format.parts[0]) {
      case ColorFormatPart::sRGB: return VK_FORMAT_B8G8R8A8_SRGB;
      case ColorFormatPart::UInt: return VK_FORMAT_B8G8R8A8_UINT;
      default: break;
      }
      return std::unexpected{Status::UnsupportedFormat};
   case ColorFormatOrder::DS:
      if (format.parts[0] == ColorFormatPart::UNorm16 && format.parts[1] == ColorFormatPart::UInt) {
         return VK_FORMAT_D16_UNORM_S8_UINT;
      }
      return std::unexpected{Status::UnsupportedFormat};
   case ColorFormatOrder::D:
      switch (format.parts[0]) {
      case ColorFormatPart::Float32: return VK_FORMAT_D32_SFLOAT;
      default: break;
      }
      return std::unexpected{Status::UnsupportedFormat};
   case ColorFormatOrder::RG:
      switch (format.parts[0]) {
      case ColorFormatPart::sRGB: return VK_FORMAT_R8G8_SRGB;
      case ColorFormatPart::UNorm16: return VK_FORMAT_R16G16_UNORM;
      case ColorFormatPart::UInt: return VK_FORMAT_R8G8_UINT;
      case ColorFormatPart::Float16: return VK_FORMAT_R16G16_SFLOAT;
      case ColorFormatPart::Float32: return VK_FORMAT_R32G32_SFLOAT;
      default: break;
      }
      return std::unexpected{Status::UnsupportedFormat};
   case ColorFormatOrder::RGB:
      switch (format.parts[0]) {
      case ColorFormatPart::sRGB: return VK_FORMAT_R8G8B8_SRGB;
      case ColorFormatPart::UNorm16: return VK_FORMAT_R16G16B16_UNORM;
      case ColorFormatPart::UInt: return VK_FORMAT_R8G8B8_UINT;
      case ColorFormatPart::Float16: return VK_FORMAT_R16G16B16_SFLOAT;
      case ColorFormatPart::Float32: return VK_FORMAT_R32G32B32_SFLOAT;
      default: break;
      }
      return std::unexpected{Status::UnsupportedFormat};
   }
   return std::unexpected{Status::UnsupportedFormat};
}

Result<ColorFormat> to_color_format(const VkFormat format)
{
   switch (format) {
   case VK_FORMAT_B8G8R8A8_SRGB: return GAPI_FORMAT(BGRA, sRGB);
   case VK_FORMAT_R8G8B8A8_SRGB: return GAPI_FORMAT(RGBA, sRGB);
   case VK_FORMAT_R32G32B32A32_SFLOAT: return GAPI_FORMAT(RGBA, Float32);
   case VK_FORMAT_D16_UNORM_S8_UINT: return GAPI_FORMAT(DS, UNorm16, UInt);
   default: break;
   }
   return std::unexpected{Status::UnsupportedFormat};
}

Result<VkColorSpaceKHR> to_vulkan_color_space(ColorSpace colorSpace)
{
   switch (colorSpace) {
   case ColorSpace::sRGB: return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
   case ColorSpace::HDR: return VK_COLOR_SPACE_HDR10_ST2084_EXT;
   }

   return std::unexpected{Status::UnsupportedColorSpace};
}

Result<ColorSpace> to_color_space(const VkColorSpaceKHR colorSpace)
{
   switch (colorSpace) {
   case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: return ColorSpace::sRGB;
   case VK_COLOR_SPACE_HDR10_ST2084_EXT: return ColorSpace::HDR;
   default: break;
   }

   return std::unexpected{Status::UnsupportedColorSpace};
}

WorkTypeFlags vulkan_queue_flags_to_work_type_flags(const VkQueueFlags flags, const bool canPresent)
{
   WorkTypeFlags result{WorkType::None};
   if (flags & VK_QUEUE_GRAPHICS_BIT) {
      result |= WorkType::Graphics;
   }
   if (flags & VK_QUEUE_TRANSFER_BIT) {
      result |= WorkType::Transfer;
   }
   if (flags & VK_QUEUE_COMPUTE_BIT) {
      result |= WorkType::Compute;
   }
   if (canPresent) {
      result |= WorkType::Presentation;
   }

   return result;
}

Result<VkSampleCountFlagBits> to_vulkan_sample_count(const SampleCount sampleCount)
{
   switch (sampleCount) {
   case SampleCount::Single: return VK_SAMPLE_COUNT_1_BIT;
   case SampleCount::Double: return VK_SAMPLE_COUNT_2_BIT;
   case SampleCount::Quadruple: return VK_SAMPLE_COUNT_4_BIT;
   case SampleCount::Octuple: return VK_SAMPLE_COUNT_8_BIT;
   case SampleCount::Sexdecuple: return VK_SAMPLE_COUNT_16_BIT;
   }

   return std::unexpected{Status::UnsupportedFormat};
}

Result<VkImageLayout> to_vulkan_image_layout(const AttachmentType type, const AttachmentLifetime lifetime)
{
   switch (type) {
   case AttachmentType::Color: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   case AttachmentType::Depth: {
      // If we preserve the depth buffer, we need to make it optimal for reading.
      if (lifetime == AttachmentLifetime::ClearPreserve)
         return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
   }
   case AttachmentType::Presentable: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
   }

   return std::unexpected{Status::UnsupportedFormat};
}

std::tuple<VkAttachmentLoadOp, VkAttachmentStoreOp>
to_vulkan_load_store_ops(const AttachmentLifetime lifetime)
{
   switch (lifetime) {
   case AttachmentLifetime::ClearPreserve: return {VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE};
   case AttachmentLifetime::ClearDiscard:
      return {VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE};
   case AttachmentLifetime::KeepPreserve: return {VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE};
   case AttachmentLifetime::KeepDiscard:
      return {VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_DONT_CARE};
   case AttachmentLifetime::IgnorePreserve:
      return {VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE};
   case AttachmentLifetime::IgnoreDiscard:
      return {VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE};
   }

   return {VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_NONE};
}

VkShaderStageFlagBits to_vulkan_shader_stage(const PipelineStage stage)
{
   switch (stage) {
   case PipelineStage::VertexShader: return VK_SHADER_STAGE_VERTEX_BIT;
   case PipelineStage::FragmentShader: return VK_SHADER_STAGE_FRAGMENT_BIT;
   }

   return static_cast<VkShaderStageFlagBits>(0);
}

VkShaderStageFlags to_vulkan_shader_stage_flags(PipelineStageFlags flags)
{
   VkShaderStageFlags result{};
   if (flags & PipelineStage::VertexShader) {
      result |= VK_SHADER_STAGE_VERTEX_BIT;
   }
   if (flags & PipelineStage::FragmentShader) {
      result |= VK_SHADER_STAGE_FRAGMENT_BIT;
   }
   return result;
}

VkPipelineStageFlags to_vulkan_pipeline_stage_flags(const PipelineStageFlags flags)
{
   VkPipelineStageFlags  result{};
   if (flags & PipelineStage::Entrypoint) {
      result |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
   }
   if (flags & PipelineStage::VertexShader) {
      result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
   }
   if (flags & PipelineStage::FragmentShader) {
      result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
   }
   if (flags & PipelineStage::Transfer) {
      result |= VK_PIPELINE_STAGE_TRANSFER_BIT;
   }
   return result;
}

VkDescriptorType to_vulkan_descriptor_type(DescriptorType descriptorType)
{
   switch (descriptorType) {
   case DescriptorType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   case DescriptorType::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
   case DescriptorType::ImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   }
   return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

VkImageLayout to_vulkan_image_layout(const TextureState resourceState)
{
   switch (resourceState) {
   case TextureState::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
   case TextureState::TransferSource: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   case TextureState::TransferDestination: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   case TextureState::ShaderRead: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   }

   return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkAccessFlags to_vulkan_access_flags(const TextureState resourceState)
{
   switch (resourceState) {
   case TextureState::Undefined: return 0;
   case TextureState::TransferSource: return VK_ACCESS_TRANSFER_READ_BIT;
   case TextureState::TransferDestination: return VK_ACCESS_TRANSFER_WRITE_BIT;
   case TextureState::ShaderRead: return VK_ACCESS_SHADER_READ_BIT;
   }

   return 0;
}

VkFilter to_vulkan_filter(const FilterType filterType)
{
   switch (filterType) {
   case FilterType::Linear: return VK_FILTER_LINEAR;
   case FilterType::NearestNeighbour: return VK_FILTER_NEAREST;
   }
   return VK_FILTER_MAX_ENUM;
}

VkSamplerAddressMode to_vulkan_sampler_address_mode(const TextureAddressMode mode)
{
   switch (mode) {
   case TextureAddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
   case TextureAddressMode::Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
   case TextureAddressMode::Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   case TextureAddressMode::Border: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
   case TextureAddressMode::MirrorOnce: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
   }

   return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
}

}// namespace triglav::graphics_api::vulkan
