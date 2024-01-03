#include "Renderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "geometry/DebugMesh.h"
#include "geometry/Mesh.h"
#include "graphics_api/PipelineBuilder.h"

#include "AssetMap.hpp"
#include "Core.h"
#include "GlyphAtlas.h"
#include "Name.hpp"
#include "ResourceManager.h"

namespace renderer {

constexpr auto g_colorFormat = GAPI_COLOR_FORMAT(BGRA, sRGB);
constexpr auto g_depthFormat = GAPI_COLOR_FORMAT(D, Float32);
constexpr auto g_sampleCount = graphics_api::SampleCount::Quadruple;

namespace {

graphics_api::Resolution create_viewport_resolution(const graphics_api::Device &device, const uint32_t width,
                                                    const uint32_t height)
{
   graphics_api::Resolution resolution{
           .width  = width,
           .height = height,
   };

   const auto [minResolution, maxResolution] = device.get_surface_resolution_limits();
   resolution.width  = std::clamp(resolution.width, minResolution.width, maxResolution.width);
   resolution.height = std::clamp(resolution.height, minResolution.height, maxResolution.height);

   return resolution;
}

std::unique_ptr<ResourceManager> create_resource_manager(graphics_api::Device &device,
                                                         font::FontManger &fontManger)
{
   auto manager = std::make_unique<ResourceManager>(device, fontManger);

   for (const auto &[name, path] : g_assetMap) {
      manager->load_asset(name, path);
   }

   auto sphere = geometry::create_sphere(40, 20, 4.0f);
   sphere.set_material(0, "gold");
   sphere.triangulate();
   sphere.recalculate_tangents();
   auto gpuSphere = sphere.upload_to_device(device);

   manager->add_mesh_and_model("mdl:sphere"_name, gpuSphere);
   manager->add_material("mat:bark"_name, Material{"tex:bark"_name, "tex:bark/normal"_name, 1.0f});
   manager->add_material("mat:leaves"_name, Material{"tex:leaves"_name, "tex:bark/normal"_name, 1.0f});
   manager->add_material("mat:gold"_name, Material{"tex:gold"_name, "tex:bark/normal"_name, 1.0f});
   manager->add_material("mat:grass"_name, Material{"tex:grass"_name, "tex:grass/normal"_name, 1.0f});
   manager->add_material("mat:pine"_name, Material{"tex:pine"_name, "tex:pine/normal"_name, 1.0f});

   std::vector<font::Rune> runes{};
   for (font::Rune ch = 'A'; ch <= 'Z'; ++ch) {
      runes.emplace_back(ch);
   }
   for (font::Rune ch = 'a'; ch <= 'z'; ++ch) {
      runes.emplace_back(ch);
   }
   for (font::Rune ch = '0'; ch <= '9'; ++ch) {
      runes.emplace_back(ch);
   }

   return manager;
}

std::vector<font::Rune> make_runes()
{
   std::vector<font::Rune> runes{};
   for (font::Rune ch = 'A'; ch <= 'Z'; ++ch) {
      runes.emplace_back(ch);
   }
   for (font::Rune ch = 'a'; ch <= 'z'; ++ch) {
      runes.emplace_back(ch);
   }
   for (font::Rune ch = '0'; ch <= '9'; ++ch) {
      runes.emplace_back(ch);
   }
   runes.emplace_back('.');
   runes.emplace_back(':');
   runes.emplace_back('-');
   runes.emplace_back(',');
   runes.emplace_back(' ');
   runes.emplace_back(281);
   return runes;
}

auto g_runes{make_runes()};

}// namespace

Renderer::Renderer(const graphics_api::Surface &surface, const uint32_t width, const uint32_t height) :
    m_device(checkResult(graphics_api::initialize_device(surface))),
    m_fontManger(),
    m_resourceManager(create_resource_manager(*m_device, m_fontManger)),
    m_resolution(create_viewport_resolution(*m_device, width, height)),
    m_swapchain(checkResult(m_device->create_swapchain(g_colorFormat, graphics_api::ColorSpace::sRGB,
                                                       g_depthFormat, g_sampleCount, m_resolution))),
    m_renderPass(checkResult(m_device->create_render_pass(m_swapchain))),
    m_framebuffers(checkResult(m_swapchain.create_framebuffers(m_renderPass))),
    m_framebufferReadySemaphore(checkResult(m_device->create_semaphore())),
    m_renderFinishedSemaphore(checkResult(m_device->create_semaphore())),
    m_inFlightFence(checkResult(m_device->create_fence())),
    m_commandList(checkResult(m_device->create_command_list())),
    m_context3D(*m_device, m_renderPass, *m_resourceManager),
    m_context2D(*m_device, m_renderPass, *m_resourceManager),
    m_shadowMap(*m_device, *m_resourceManager),
    m_scene(*this, m_context3D, m_shadowMap),
    m_skyBox(*this),
    m_glyphAtlasBold(*m_device, m_resourceManager->typeface("tfc:cantarell/bold"_name), g_runes, 24, 500,
                     500),
    m_glyphAtlas(*m_device, m_resourceManager->typeface("tfc:cantarell"_name), g_runes, 24, 500, 500),
    m_sprite(m_context2D.create_sprite_from_texture(m_shadowMap.depth_texture())),
    // m_sprite(m_context2D.create_sprite_from_texture(m_glyphAtlas.texture())),
    m_textRenderer(*m_device, m_renderPass, *m_resourceManager),
    m_titleLabel(m_textRenderer.create_text_object(m_glyphAtlas, "Triglav Engine Demo - 0.0.1")),
    m_framerateLabel(m_textRenderer.create_text_object(m_glyphAtlasBold, "Framerate")),
    m_framerateValue(m_textRenderer.create_text_object(m_glyphAtlas, "0")),
    m_positionLabel(m_textRenderer.create_text_object(m_glyphAtlasBold, "Position")),
    m_positionValue(m_textRenderer.create_text_object(m_glyphAtlas, "0, 0, 0"))
{

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
           .model{"mdl:teapot"_name},
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
   const auto deltaTime = calculate_frame_duration();
   const auto framerate = calculate_framerate(deltaTime);

   const auto framerateStr = std::format("{}", framerate);
   m_textRenderer.update_text_object(m_glyphAtlas, m_framerateValue, framerateStr);
   const auto camPos      = m_scene.camera().position();
   const auto positionStr = std::format("{:.2f}, {:.2f}, {:.2f}", camPos.x, camPos.y, camPos.z);
   m_textRenderer.update_text_object(m_glyphAtlas, m_positionValue, positionStr);

   const auto framebufferIndex = m_swapchain.get_available_framebuffer(m_framebufferReadySemaphore);
   this->update_uniform_data(deltaTime);
   m_inFlightFence.await();

   checkStatus(m_commandList.begin());
   m_context3D.set_active_command_list(&m_commandList);
   m_context2D.set_active_command_list(&m_commandList);

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

      m_skyBox.on_render(m_commandList, m_yaw, m_pitch, static_cast<float>(m_resolution.width),
                         static_cast<float>(m_resolution.height));

      m_context3D.begin_render();
      m_scene.render();

      // m_context2D.begin_render();
      // m_context2D.draw_sprite(m_sprite, {0.0f, 0.0f}, {0.2f, 0.2f});

      m_textRenderer.begin_render(m_commandList);
      auto textY = 16.0f + m_titleLabel.metric.height;
      m_textRenderer.draw_text_object(m_commandList, m_titleLabel, {16.0f, textY}, {1.0f, 1.0f, 1.0f});
      textY += 16.0f + m_framerateLabel.metric.height;
      m_textRenderer.draw_text_object(m_commandList, m_framerateLabel, {16.0f, textY}, {1.0f, 1.0f, 1.0f});
      m_textRenderer.draw_text_object(m_commandList, m_framerateValue,
                                      {16.0f + m_framerateLabel.metric.width + 8.0f, textY},
                                      {1.0f, 1.0f, 0.4f});
      textY += 8.0f + m_positionLabel.metric.height;
      m_textRenderer.draw_text_object(m_commandList, m_positionLabel, {16.0f, textY}, {1.0f, 1.0f, 1.0f});
      m_textRenderer.draw_text_object(m_commandList, m_positionValue,
                                      {16.0f + m_positionLabel.metric.width + 8.0f, textY},
                                      {1.0f, 1.0f, 0.4f});

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
   return {m_resolution.width, m_resolution.height};
}

float Renderer::calculate_frame_duration()
{
   static std::chrono::steady_clock::time_point last;

   const auto now  = std::chrono::steady_clock::now();
   const auto diff = now - last;
   last            = now;

   return static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(diff).count()) /
          1000000.0f;
}

float Renderer::calculate_framerate(const float frameDuration)
{
   static float lastResult = 60.0f;
   static float sum        = 0.0f;
   static int count        = 0;

   sum += 1.0f / frameDuration;
   ++count;

   if (count == 1000) {
      lastResult = sum / static_cast<float>(count);
      sum        = 0.0f;
      count      = 0;
   }

   return std::ceil(lastResult);
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

   m_resolution = {width, height};
}

graphics_api::Device &Renderer::device() const
{
   return *m_device;
}

const auto g_movingSpeed = 10.0f;

void Renderer::update_uniform_data(const float deltaTime)
{
   const auto rotation = glm::quat{glm::vec3{m_pitch, 0.0f, m_yaw}};

   const auto forwardVector = rotation * glm::vec3{0.0f, 1.0f, 0.0f};
   const auto rightVector = rotation * glm::vec3{1.0f, 0.0f, 0.0f};
   const auto zVector = glm::vec3{0.0f, 0.0f, 1.0f};

   if (m_isMovingForward) {
      m_position += deltaTime * g_movingSpeed * forwardVector;
   } else if (m_isMovingBackwards) {
      m_position -= deltaTime * g_movingSpeed * forwardVector;
   } else if (m_isMovingLeft) {
      m_position -= deltaTime * g_movingSpeed * rightVector;
   } else if (m_isMovingRight) {
      m_position += deltaTime * g_movingSpeed * rightVector;
   } else if (m_isMovingUp) {
      m_position += deltaTime * g_movingSpeed * zVector;
   } else if (m_isMovingDown) {
      m_position -= deltaTime * g_movingSpeed * zVector;
   } else if (m_isLightMovingForward) {
      m_lightX += deltaTime * g_movingSpeed;
   } else if (m_isLightMovingBackwards) {
      m_lightX -= deltaTime * g_movingSpeed;
   }

   m_scene.set_camera(m_position, rotation);
   // m_scene.set_shadow_x(m_lightX);
   m_scene.update();
}

}// namespace renderer
