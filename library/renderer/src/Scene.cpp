#include "Scene.h"

#include <glm/gtx/quaternion.hpp>

#include "Context3D.h"
#include "Renderer.h"

namespace renderer {

constexpr auto g_upVector = glm::vec3{0.0f, 0.0f, 1.0f};

Scene::Scene(Renderer &renderer, Context3D &context3D, ShadowMap &shadowMap,
             DebugLinesRenderer &debugLinesRenderer, ResourceManager &resourceManager) :
    m_renderer(renderer),
    m_context3D(context3D),
    m_shadowMap(shadowMap),
    m_debugLinesRenderer(debugLinesRenderer),
    m_resourceManager(resourceManager)
{
   m_shadowMapCamera.set_position(glm::vec3{-76.0f, 0.0f, -27.0f});
   m_shadowMapCamera.set_orientation(glm::quat{
           glm::vec3{0.18f, 0.0f, 4.71f}
   });
}

void Scene::update()
{
   const auto [width, height] = m_renderer.screen_resolution();
   m_camera.set_viewport_size(width, height);

   const auto camMat       = m_camera.matrix();
   const auto camMatShadow = m_shadowMapCamera.matrix();
   // const auto camMat = glm::mat4(1);
   // const auto camMatShadow = glm::mat4(1);

   for (const auto &obj : m_instancedObjects) {
      obj.ubo->viewProj      = camMat;
      obj.shadowMap.ubo->mvp = camMatShadow * obj.ubo->model;
      obj.ubo->shadowMapMVP  = obj.shadowMap.ubo->mvp;
   }
}

void Scene::add_object(SceneObject object)
{
   m_objects.emplace_back(std::move(object));
}

void Scene::compile_scene()
{
   m_instancedObjects.clear();
   m_debugLines.clear();

   for (const auto &obj : m_objects) {
      const auto &model   = m_resourceManager.model(obj.model);
      auto instance       = m_context3D.instance_model(obj.model, m_shadowMap);
      const auto modelMat = glm::scale(glm::translate(glm::mat4(1), obj.position), obj.scale) *
                            glm::mat4_cast(obj.rotation);
      instance.position    = obj.position;
      instance.ubo->model  = modelMat;
      instance.ubo->normal = glm::transpose(glm::inverse(glm::mat3(modelMat)));
      m_instancedObjects.push_back(std::move(instance));

      auto &debugBoudingBox = m_debugLines.emplace_back(
              m_debugLinesRenderer.create_line_list_from_bouding_box(model.boudingBox));
      debugBoudingBox.model = modelMat;
   }
}

void Scene::render() const
{
   m_context3D.set_camera_position(m_camera.position());

   for (const auto &obj : m_instancedObjects) {
      if (not m_camera.is_bouding_box_visible(obj.boudingBox, obj.ubo->model))
         continue;

      m_context3D.draw_model(obj);
   }
}

void Scene::render_shadow_map() const
{
   m_context3D.set_light_position(m_shadowMapCamera.position());
   // TODO: Render only objects visiable by camera.

   for (const auto &obj : m_instancedObjects) {
      if (not m_shadowMapCamera.is_bouding_box_visible(obj.boudingBox, obj.ubo->model))
         continue;

      m_shadowMap.draw_model(m_context3D, obj);
   }
}

void Scene::render_debug_lines() const
{
   for (const auto &obj : m_debugLines) {
      m_debugLinesRenderer.draw(m_context3D.command_list(), obj, m_camera);
   }
}

void Scene::set_camera(const glm::vec3 position, const glm::quat orientation)
{
   m_camera.set_position(position);
   m_camera.set_orientation(orientation);
}

const Camera &Scene::camera() const
{
   return m_camera;
}

Camera &Scene::camera()
{
   return m_camera;
}

}// namespace renderer