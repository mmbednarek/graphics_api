#pragma once

#include "Core.h"
#include "graphics_api/Device.h"
#include "graphics_api/PipelineBuilder.h"
#include "graphics_api/PlatformSurface.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "SkyBox.h"

#include <optional>
#include <string_view>

namespace renderer {

struct Object3d
{
   graphics_api::DescriptorGroup descGroup;
   std::vector<graphics_api::Buffer> uniformBuffers;
   std::vector<graphics_api::MappedMemory> uniformBufferMappings;
};

struct RendererObjects
{
   uint32_t width{};
   uint32_t height{};
   graphics_api::DeviceUPtr device;
   std::unique_ptr<ResourceManager> resourceManager;
   graphics_api::RenderPass renderPass;
   graphics_api::Pipeline pipeline;
   graphics_api::Semaphore framebufferReadySemaphore;
   graphics_api::Semaphore renderFinishedSemaphore;
   graphics_api::Fence inFlightFence;
   graphics_api::CommandList commandList;
};

class Renderer
{
 public:
   explicit Renderer(RendererObjects &&objects);
   void on_render();
   void on_resize(uint32_t width, uint32_t height);
   void on_close() const;
   void on_mouse_relative_move(float dx, float dy);
   void on_key_pressed(uint32_t key);
   void on_key_released(uint32_t key);
   void on_mouse_wheel_turn(float x);
   [[nodiscard]] graphics_api::Texture load_texture(std::string_view path) const;
   [[nodiscard]] graphics_api::Shader load_shader(graphics_api::ShaderStage stage,
                                                  std::string_view path) const;
   [[nodiscard]] graphics_api::PipelineBuilder create_pipeline();
   [[nodiscard]] ResourceManager& resource_manager() const;

   template<typename TUbo>
   [[nodiscard]] graphics_api::Buffer create_ubo_buffer() const
   {
      return checkResult(m_device->create_buffer(graphics_api::BufferPurpose::UniformBuffer, sizeof(TUbo)));
   }

   [[nodiscard]] graphics_api::Device &device() const;

 private:
   void update_uniform_data(uint32_t frame);
   void write_to_texture(std::string_view path, graphics_api::Texture &texture);

   bool m_receivedMouseInput{false};
   float m_lastMouseX{};
   float m_lastMouseY{};

   float m_yaw{0};
   float m_pitch{0};
   float m_distance{12};
   glm::vec3 m_position{};
   bool m_isMovingForward{false};
   bool m_isMovingBackwards{false};
   bool m_isMovingLeft{false};
   bool m_isMovingRight{false};
   bool m_isMovingUp{false};
   bool m_isMovingDown{false};

   uint32_t m_width{};
   uint32_t m_height{};
   graphics_api::DeviceUPtr m_device;
   graphics_api::RenderPass m_renderPass;
   graphics_api::Pipeline m_pipeline;
   graphics_api::Semaphore m_framebufferReadySemaphore;
   graphics_api::Semaphore m_renderFinishedSemaphore;
   graphics_api::Fence m_inFlightFence;
   graphics_api::CommandList m_commandList;
   std::unique_ptr<ResourceManager> m_resourceManager;

   SkyBox m_skyBox;
   std::optional<Object3d> m_house;
};

Renderer init_renderer(const graphics_api::Surface &surface, uint32_t width, uint32_t height);

}// namespace renderer