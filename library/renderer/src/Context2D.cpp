#include "Context2D.h"

#include "graphics_api/CommandList.h"
#include "graphics_api/DescriptorWriter.h"
#include "graphics_api/PipelineBuilder.h"

#include <glm/gtx/matrix_transform_2d.hpp>

namespace renderer {

struct Vertex2D
{
   glm::vec3 location;
};

Context2D::Context2D(graphics_api::Device &device, graphics_api::RenderPass &renderPass,
                     ResourceManager &resourceManager) :
    m_device(device),
    m_renderPass(renderPass),
    m_resourceManager(resourceManager),
    m_pipeline(checkResult(graphics_api::PipelineBuilder(m_device, renderPass)
                                   .fragment_shader(resourceManager.shader("fsh:sprite"_name))
                                   .vertex_shader(resourceManager.shader("vsh:sprite"_name))
                                   // Descriptor layout
                                   .descriptor_binding(graphics_api::DescriptorType::UniformBuffer,
                                                       graphics_api::ShaderStage::Vertex)
                                   .descriptor_binding(graphics_api::DescriptorType::ImageSampler,
                                                       graphics_api::ShaderStage::Fragment)
                                   .vertex_topology(graphics_api::VertexTopology::TriangleStrip)
                                   .enable_depth_test(false)
                                   .build())),
    m_descriptorPool(checkResult(m_pipeline.create_descriptor_pool(40, 40, 40))),
    m_sampler(checkResult(device.create_sampler(true)))
{
}

Sprite Context2D::create_sprite(const Name textureName)
{
   const auto &texture = m_resourceManager.texture(textureName);
   return this->create_sprite_from_texture(texture);
}

Sprite Context2D::create_sprite_from_texture(const graphics_api::Texture &texture)
{
   graphics_api::UniformBuffer<SpriteUBO> ubo(m_device);

   auto descriptors = checkResult(m_descriptorPool.allocate_array(1));

   {
      graphics_api::DescriptorWriter writer(m_device, descriptors[0]);
      writer.set_uniform_buffer(0, ubo);
      writer.set_sampled_texture(1, texture, m_sampler);
   }

   return Sprite{static_cast<float>(texture.width()), static_cast<float>(texture.height()), std::move(ubo),
                 std::move(descriptors)};
}

void Context2D::set_active_command_list(graphics_api::CommandList *commandList)
{
   m_commandList = commandList;
}

void Context2D::begin_render() const
{
   assert(m_commandList);
   m_commandList->bind_pipeline(m_pipeline);
}

void Context2D::draw_sprite(const Sprite &sprite, const glm::vec2 position, const glm::vec2 scale) const
{
   assert(m_commandList != nullptr);

   const auto [viewportWidth, viewportHeight] = m_renderPass.resolution();

   const auto scaleX = 2.0f * scale.x * sprite.width / static_cast<float>(viewportWidth);
   const auto scaleY = 2.0f * scale.y * sprite.height / static_cast<float>(viewportHeight);
   const auto transX = (position.x - 0.5f * static_cast<float>(viewportWidth)) / scale.x / sprite.width;
   const auto transY = (position.y - 0.5f * static_cast<float>(viewportHeight)) / scale.y / sprite.height;

   const auto scaleMat   = glm::scale(glm::mat3(1), glm::vec2(scaleX, scaleY));
   sprite.ubo->transform = glm::translate(scaleMat, glm::vec2(transX, transY));

   m_commandList->bind_descriptor_set(sprite.descriptors[0]);
   m_commandList->draw_primitives(4, 0);
}

}// namespace renderer