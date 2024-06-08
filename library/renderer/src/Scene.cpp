#include "Scene.h"

#include "Renderer.h"

#include "triglav/world/Level.h"

#include <cmath>
#include <glm/gtc/quaternion.hpp>

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

using triglav::ResourceType;

namespace triglav::renderer {

constexpr auto g_upVector = glm::vec3{0.0f, 0.0f, 1.0f};

glm::mat4 SceneObject::model_matrix() const
{
   return glm::scale(glm::translate(glm::mat4(1), this->position), this->scale) * glm::mat4_cast(this->rotation);
}

Scene::Scene(resource::ResourceManager& resourceManager) :
    m_resourceManager(resourceManager)
{
   m_shadowMapCamera.set_position(glm::vec3{-68.0f, 0.0f, -30.0f});
   m_shadowMapCamera.set_orientation(glm::quat{glm::vec3{0.24f, 0.0f, 4.71f}});
   m_shadowMapCamera.set_near_far_planes(-100.0f, 200.0f);
   m_shadowMapCamera.set_viewspace_width(150.0f);
}

void Scene::update(graphics_api::Resolution& resolution)
{
   const auto [width, height] = resolution;
   m_camera.set_viewport_size(width, height);

   this->OnViewportChange.publish(resolution);
}

void Scene::add_object(SceneObject object)
{
   auto& emplacedObj = m_objects.emplace_back(std::move(object));
   this->OnObjectAddedToScene.publish(emplacedObj);
}

void Scene::load_level(const LevelName name)
{
   auto& level = m_resourceManager.get<ResourceType::Level>(name);
   auto& root = level.root();

   for (const auto& mesh : root.static_meshes()) {
      this->add_object(SceneObject{
         .model = mesh.meshName,
         .position = mesh.transform.position,
         .rotation = glm::quat(mesh.transform.rotation),
         .scale = mesh.transform.scale,
      });
   }
}

void Scene::set_camera(const glm::vec3 position, const glm::quat orientation)
{
   m_camera.set_position(position);
   m_camera.set_orientation(orientation);
}

const Camera& Scene::camera() const
{
   return m_camera;
}

Camera& Scene::camera()
{
   return m_camera;
}

const OrthoCamera& Scene::shadow_map_camera() const
{
   return m_shadowMapCamera;
}

float Scene::yaw() const
{
   return m_yaw;
}

float Scene::pitch() const
{
   return m_pitch;
}

void Scene::update_orientation(const float delta_yaw, const float delta_pitch)
{
   m_yaw += delta_yaw;
   while (m_yaw < 0) {
      m_yaw += 2 * M_PI;
   }
   while (m_yaw >= 2 * M_PI) {
      m_yaw -= 2 * M_PI;
   }

   m_pitch += delta_pitch;
   m_pitch = std::clamp(m_pitch, -static_cast<float>(M_PI) / 2.0f + 0.01f, static_cast<float>(M_PI) / 2.0f - 0.01f);

   this->camera().set_orientation(glm::quat{glm::vec3{m_pitch, 0.0f, m_yaw}});
}

}// namespace triglav::renderer