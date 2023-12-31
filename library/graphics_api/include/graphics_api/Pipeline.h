#pragma once

#include "DescriptorPool.h"
#include "GraphicsApi.hpp"
#include "vulkan/ObjectWrapper.hpp"

namespace graphics_api {

DECLARE_VLK_WRAPPED_CHILD_OBJECT(PipelineLayout, Device)
DECLARE_VLK_WRAPPED_CHILD_OBJECT(DescriptorSetLayout, Device)

class Buffer;
class Texture;

namespace vulkan {
using Pipeline = WrappedObject<VkPipeline, vkCreateGraphicsPipelines, vkDestroyPipeline, VkDevice>;
}

class Pipeline
{
 public:
   Pipeline(vulkan::PipelineLayout layout, vulkan::Pipeline pipeline,
            vulkan::DescriptorSetLayout descriptorSetLayout);

   [[nodiscard]] VkPipeline vulkan_pipeline() const;
   [[nodiscard]] const vulkan::PipelineLayout &layout() const;
   [[nodiscard]] Result<DescriptorPool> create_descriptor_pool(uint32_t uniformBufferCount,
                                                               uint32_t sampledImageCount,
                                                               uint32_t maxDescriptorCount);

 private:
   vulkan::PipelineLayout m_layout;
   vulkan::Pipeline m_pipeline;
   vulkan::DescriptorSetLayout m_descriptorSetLayout;
};

}// namespace graphics_api
