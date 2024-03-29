#pragma once

#include "GraphicsApi.hpp"
#include "vulkan/ObjectWrapper.hpp"

#include <vector>

namespace triglav::graphics_api {

DECLARE_VLK_WRAPPED_CHILD_OBJECT(RenderPass, Device);

class RenderPass
{
 public:
   RenderPass(vulkan::RenderPass renderPass, SampleCount sampleCount, int colorAttachmentCount);

   [[nodiscard]] VkRenderPass vulkan_render_pass() const;
   [[nodiscard]] SampleCount sample_count() const;
   [[nodiscard]] int color_attachment_count() const;

 private:
   vulkan::RenderPass m_renderPass;
   SampleCount m_sampleCount;
   int m_colorAttachmentCount;
};

}// namespace graphics_api