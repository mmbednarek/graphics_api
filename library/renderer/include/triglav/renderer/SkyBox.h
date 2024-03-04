#pragma once

#include "triglav/graphics_api/CommandList.h"
#include "triglav/graphics_api/Pipeline.h"
#include "triglav/graphics_api/Texture.h"
#include "triglav/render_core/RenderCore.hpp"
#include "triglav/resource/ResourceManager.h"

namespace triglav::renderer {

class Renderer;

struct SkyBoxUBO
{
   alignas(16) glm::mat4 view;
   alignas(16) glm::mat4 proj;
};

class SkyBox
{
 public:
   explicit SkyBox(Renderer &renderer);

   void on_render(graphics_api::CommandList &commandList, float yaw, float pitch, float width,
                  float height) const;

 private:
   triglav::resource::ResourceManager &m_resourceManager;
   triglav::render_core::GpuMesh m_mesh;
   graphics_api::Pipeline m_pipeline;
   graphics_api::DescriptorPool m_descPool;
   graphics_api::DescriptorArray m_descArray;
   graphics_api::DescriptorView m_descriptorSet;
   graphics_api::Buffer m_uniformBuffer;
   graphics_api::MappedMemory m_uniformBufferMapping;
   graphics_api::Sampler& m_sampler;
};


}// namespace renderer