#include "PostProcessingRenderer.h"

#include "triglav/graphics_api/CommandList.h"
#include "triglav/graphics_api/DescriptorWriter.h"
#include "triglav/graphics_api/PipelineBuilder.h"
#include "triglav/render_core/RenderCore.hpp"

using namespace triglav::name_literals;
using triglav::ResourceType;
using triglav::render_core::checkResult;
using triglav::resource::ResourceManager;

namespace triglav::renderer {

PostProcessingRenderer::PostProcessingRenderer(graphics_api::Device &device,
                                               graphics_api::RenderTarget &renderTarget,
                                               ResourceManager &resourceManager) :
    m_device(device),
    m_pipeline(checkResult(
            graphics_api::PipelineBuilder(m_device, renderTarget)
                    .fragment_shader(resourceManager.get<ResourceType::FragmentShader>("post_processing.fshader"_name))
                    .vertex_shader(resourceManager.get<ResourceType::VertexShader>("post_processing.vshader"_name))
                    // Descriptor layout
                    .descriptor_binding(graphics_api::DescriptorType::ImageSampler,
                                        graphics_api::PipelineStage::FragmentShader)
                    .descriptor_binding(graphics_api::DescriptorType::ImageSampler,
                                        graphics_api::PipelineStage::FragmentShader)
                    .enable_depth_test(false)
                    .use_push_descriptors(true)
                    .vertex_topology(graphics_api::VertexTopology::TriangleStrip)
                    .push_constant(graphics_api::PipelineStage::FragmentShader, sizeof(PushConstants))
                    .build())),
    m_sampler(resourceManager.get<ResourceType::Sampler>("linear_repeat_mlod0.sampler"_name))
{
}

void PostProcessingRenderer::draw(render_core::FrameResources &resources,
                                  graphics_api::CommandList &cmdList) const
{
   cmdList.bind_pipeline(m_pipeline);

   auto &shading = resources.node("shading"_name_id).framebuffer("shading"_name_id);
   auto &ui = resources.node("user_interface"_name_id).framebuffer("ui"_name_id);

   graphics_api::DescriptorWriter writer(m_device);
   writer.set_sampled_texture(0, shading.texture("shading"_name_id), m_sampler);
   writer.set_sampled_texture(1, ui.texture("user_interface"_name_id), m_sampler);
   cmdList.push_descriptors(0, writer);

   PushConstants constants{
           .enableFXAA = resources.has_flag("fxaa"_name_id),
           .hideUI     = resources.has_flag("hide_ui"_name_id),
   };
   cmdList.push_constant(graphics_api::PipelineStage::FragmentShader, constants);
   cmdList.draw_primitives(4, 0);
}

}// namespace triglav::renderer