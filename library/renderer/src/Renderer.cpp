#include "Renderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "AssetMap.hpp"
#include "geometry/Mesh.h"
#include "graphics_api/PipelineBuilder.h"
#include "Name.hpp"
#include "ResourceManager.h"

#include "Core.h"


#include <geometry/DebugMesh.h>

namespace renderer {

constexpr auto g_colorFormat = GAPI_COLOR_FORMAT(BGRA, sRGB);
constexpr auto g_depthFormat = GAPI_COLOR_FORMAT(D, Float32);
constexpr auto g_sampleCount = graphics_api::SampleCount::Bits8;

Renderer::Renderer(RendererObjects &&objects) :
    m_width(objects.width),
    m_height(objects.height),
    m_device(std::move(objects.device)),
    m_swapchain(std::move(objects.swapchain)),
    m_renderPass(std::move(objects.renderPass)),
    m_framebuffers(checkResult(m_swapchain.create_framebuffers(m_renderPass))),
    m_framebufferReadySemaphore(std::move(objects.framebufferReadySemaphore)),
    m_renderFinishedSemaphore(std::move(objects.renderFinishedSemaphore)),
    m_inFlightFence(std::move(objects.inFlightFence)),
    m_commandList(std::move(objects.commandList)),
    m_resourceManager(std::move(objects.resourceManager)),
    m_skyBox(*this),
    m_context3D(*m_device, m_renderPass, *m_resourceManager),
    m_shadowMap(*m_device, *m_resourceManager),
    m_scene(*this, m_context3D, m_shadowMap)
{
   auto sphere = geometry::create_sphere(40, 20, 4.0f);
   sphere.set_material(0, "gold");
   sphere.triangulate();
   sphere.recalculate_tangents();
   auto gpuBox = sphere.upload_to_device(*m_device);

   m_resourceManager->add_mesh_and_model("mdl:sphere"_name, gpuBox);
   m_resourceManager->add_material("mat:bark"_name, Material{"tex:bark"_name, "tex:bark/normal"_name, 1.0f});
   m_resourceManager->add_material("mat:leaves"_name,
                                   Material{"tex:leaves"_name, "tex:bark/normal"_name, 1.0f});
   m_resourceManager->add_material("mat:gold"_name, Material{"tex:gold"_name, "tex:bark/normal"_name, 1.0f});
   m_resourceManager->add_material("mat:grass"_name,
                                   Material{"tex:grass"_name, "tex:grass/normal"_name, 1.0f});
   m_resourceManager->add_material("mat:pine"_name, Material{"tex:pine"_name, "tex:pine/normal"_name, 1.0f});

   m_scene.add_object(SceneObject{
           .model{"mdl:terrain"_name},
           .position{0, 0, 0},
           .rotation{1, 0, 0, 0},
           .scale{1, 1, 1},
   });
   //   m_scene.add_object(SceneObject{
   //      .model{"mdl:tree"_name},
   //      .position{10, 0, 0},
   //      .rotation{1, 0, 0, 0},
   //      .scale{0.9, 0.9, 0.9},
   //   });
   //   m_scene.add_object(SceneObject{
   //      .model{"mdl:tree"_name},
   //      .position{-10, 0, 0},
   //      .rotation{glm::vec3{0, 0, glm::radians(45.0f)}},
   //      .scale{1, 1, 1},
   //   });
   //   m_scene.add_object(SceneObject{
   //      .model{"mdl:tree"_name},
   //      .position{0, 10, 0},
   //      .rotation{glm::vec3{0, 0, glm::radians(90.0f)}},
   //      .scale{1.1, 1.1, 1.1},
   //   });
   m_scene.add_object(SceneObject{
           .model{"mdl:tree"_name},
           .position{0, -10, 0},
           .rotation{glm::vec3{0, 0, glm::radians(135.0f)}},
           .scale{1.2, 1.2, 1.2},
   });
   m_scene.add_object(SceneObject{
           .model{"mdl:pine"_name},
           .position{0, 0, 0},
           .rotation{glm::vec3{0, 0, glm::radians(270.0f)}},
           .scale{30, 30, 30},
   });
   m_scene.compile_scene();
}

void Renderer::on_render()
{
   const auto framebufferIndex = m_swapchain.get_available_framebuffer(m_framebufferReadySemaphore);
   this->update_uniform_data(framebufferIndex);
   m_inFlightFence.await();

   checkStatus(m_commandList.begin());
   m_context3D.set_active_command_list(&m_commandList);

   {
      std::array<graphics_api::ClearValue, 1> clearValues{
              graphics_api::DepthStenctilValue{1.0f, 0.0f}
      };
      m_commandList.begin_render_pass(m_shadowMap.framebuffer(), clearValues);

      m_shadowMap.on_begin_render(m_context3D);
      m_scene.render_shadow_map();

      m_commandList.end_render_pass();
   }

   {
      std::array<graphics_api::ClearValue, 3> clearValues{
              graphics_api::ColorPalette::Black,
              graphics_api::DepthStenctilValue{1.0f, 0.0f},
              graphics_api::ColorPalette::Black,
      };
      m_commandList.begin_render_pass(m_framebuffers[framebufferIndex], clearValues);

      m_skyBox.on_render(m_commandList, m_yaw, m_pitch, static_cast<float>(m_width),
                         static_cast<float>(m_height));

      m_context3D.begin_render();
      m_scene.render();

      m_commandList.end_render_pass();
   }

   checkStatus(m_commandList.finish());

   checkStatus(m_device->submit_command_list(m_commandList, m_framebufferReadySemaphore,
                                             m_renderFinishedSemaphore, m_inFlightFence));
   checkStatus(m_swapchain.present(m_renderFinishedSemaphore, framebufferIndex));
}

void Renderer::on_close() const
{
   m_inFlightFence.await();
}

void Renderer::on_mouse_relative_move(const float dx, const float dy)
{
   m_yaw -= dx * 0.01f;
   while (m_yaw < 0) {
      m_yaw += 2 * M_PI;
   }
   while (m_yaw >= 2 * M_PI) {
      m_yaw -= 2 * M_PI;
   }

   m_pitch += dy * 0.01f;
   m_pitch = std::clamp(m_pitch, -static_cast<float>(M_PI) / 2.0f + 0.01f,
                        static_cast<float>(M_PI) / 2.0f - 0.01f);
}

void Renderer::on_key_pressed(const uint32_t key)
{
   if (key == 17) {
      m_isMovingForward = true;
   } else if (key == 31) {
      m_isMovingBackwards = true;
   } else if (key == 30) {
      m_isMovingLeft = true;
   } else if (key == 32) {
      m_isMovingRight = true;
   } else if (key == 18) {
      m_isMovingDown = true;
   } else if (key == 16) {
      m_isMovingUp = true;
   } else if (key == 19) {
      m_isLightMovingBackwards = true;
   } else if (key == 33) {
      m_isLightMovingForward = true;
   }
}

void Renderer::on_key_released(const uint32_t key)
{
   if (key == 17) {
      m_isMovingForward = false;
   } else if (key == 31) {
      m_isMovingBackwards = false;
   } else if (key == 30) {
      m_isMovingLeft = false;
   } else if (key == 32) {
      m_isMovingRight = false;
   } else if (key == 18) {
      m_isMovingDown = false;
   } else if (key == 16) {
      m_isMovingUp = false;
   } else if (key == 19) {
      m_isLightMovingBackwards = false;
   } else if (key == 33) {
      m_isLightMovingForward = false;
   }
}

void Renderer::on_mouse_wheel_turn(const float x)
{
   m_distance += x;
   m_distance = std::clamp(m_distance, 1.0f, 100.0f);
}

graphics_api::PipelineBuilder Renderer::create_pipeline()
{
   return {*m_device, m_renderPass};
}

ResourceManager &Renderer::resource_manager() const
{
   return *m_resourceManager;
}

std::tuple<uint32_t, uint32_t> Renderer::screen_resolution() const
{
   return {m_width, m_height};
}

void Renderer::on_resize(const uint32_t width, const uint32_t height)
{
   const graphics_api::Resolution resolution{width, height};

   m_device->await_all();

   m_framebuffers.clear();

   m_swapchain = checkResult(
           m_device->create_swapchain(m_swapchain.color_format(), graphics_api::ColorSpace::sRGB,
                                      g_depthFormat, m_swapchain.sample_count(), resolution, &m_swapchain));
   m_renderPass   = checkResult(m_device->create_render_pass(m_swapchain));
   m_framebuffers = checkResult(m_swapchain.create_framebuffers(m_renderPass));

   m_width  = width;
   m_height = height;
}

graphics_api::Device &Renderer::device() const
{
   return *m_device;
}

void Renderer::update_uniform_data(const uint32_t frame)
{
   const auto yVector = glm::vec4{0.0f, 1.0f, 0.0f, 1.0f};
   const auto xVector = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};
   const auto zVector = glm::vec3{0.0f, 0.0f, 1.0f};
   auto yawMatrix     = glm::rotate(glm::mat4(1), m_yaw, glm::vec3{0.0f, 0.0f, 1.0f});
   auto pitchMatrix   = glm::rotate(glm::mat4(1), m_pitch, glm::vec3{1.0f, 0.0f, 0.0f});
   auto forwardVector = glm::vec3(yawMatrix * pitchMatrix * yVector);
   auto rightVector   = glm::vec3(yawMatrix * pitchMatrix * xVector);

   if (m_isMovingForward) {
      m_position += 0.005f * forwardVector;
   } else if (m_isMovingBackwards) {
      m_position -= 0.005f * forwardVector;
   } else if (m_isMovingLeft) {
      m_position -= 0.005f * rightVector;
   } else if (m_isMovingRight) {
      m_position += 0.005f * rightVector;
   } else if (m_isMovingUp) {
      m_position += 0.005f * zVector;
   } else if (m_isMovingDown) {
      m_position -= 0.005f * zVector;
   } else if (m_isLightMovingForward) {
      m_lightX += 0.005f;
   } else if (m_isLightMovingBackwards) {
      m_lightX -= 0.005f;
   }

   m_scene.set_camera(Camera{m_position, forwardVector});
   m_scene.set_shadow_x(m_lightX);
   m_scene.update();
}

Renderer init_renderer(const graphics_api::Surface &surface, const uint32_t width, const uint32_t height)
{
   auto device = checkResult(graphics_api::initialize_device(surface));

   graphics_api::Resolution resolution{
           .width  = width,
           .height = height,
   };

   const auto [minResolution, maxResolution] = device->get_surface_resolution_limits();
   resolution.width  = std::clamp(resolution.width, minResolution.width, maxResolution.width);
   resolution.height = std::clamp(resolution.height, minResolution.height, maxResolution.height);

   auto swapchain  = checkResult(device->create_swapchain(g_colorFormat, graphics_api::ColorSpace::sRGB,
                                                          g_depthFormat, g_sampleCount, resolution));
   auto renderPass = checkResult(device->create_render_pass(swapchain));

   auto resourceManager = std::make_unique<ResourceManager>(*device);

   for (const auto &[name, path] : g_assetMap) {
      resourceManager->load_asset(name, path);
   }

   auto framebufferReadySemaphore = checkResult(device->create_semaphore());
   auto renderFinishedSemaphore   = checkResult(device->create_semaphore());
   auto inFlightFence             = checkResult(device->create_fence());
   auto commandList               = checkResult(device->create_command_list());

   return Renderer(RendererObjects{
           .width                     = width,
           .height                    = height,
           .device                    = std::move(device),
           .swapchain                 = std::move(swapchain),
           .resourceManager           = std::move(resourceManager),
           .renderPass                = std::move(renderPass),
           .framebufferReadySemaphore = std::move(framebufferReadySemaphore),
           .renderFinishedSemaphore   = std::move(renderFinishedSemaphore),
           .inFlightFence             = std::move(inFlightFence),
           .commandList               = std::move(commandList),
   });
}
}// namespace renderer
