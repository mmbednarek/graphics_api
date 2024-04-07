#pragma once

#include "triglav/graphics_api/Device.h"
#include "triglav/graphics_api/Pipeline.h"
#include "triglav/graphics_api/Texture.h"
#include "triglav/render_core/FrameResources.h"

#include "triglav/resource/ResourceManager.h"

namespace triglav::renderer {

class PostProcessingRenderer
{
 public:
   struct PushConstants
   {
      int enableFXAA{};
   };

   PostProcessingRenderer(graphics_api::Device &device, graphics_api::RenderTarget &renderTarget,
                          resource::ResourceManager &resourceManager, graphics_api::Texture &shadedBuffer);

   void draw(render_core::FrameResources &resources, graphics_api::CommandList &cmdList,
             bool enableFXAA) const;

 private:
   graphics_api::Device &m_device;
   graphics_api::Pipeline m_pipeline;
   graphics_api::Sampler &m_sampler;
   graphics_api::Texture &m_shadedBuffer;
};

}// namespace triglav::renderer