#pragma once

#include "Camera.h"
#include "DebugLinesRenderer.h"
#include "ModelRenderer.h"
#include "OrthoCamera.h"

#include "triglav/Name.hpp"
#include "triglav/render_core/Model.hpp"

#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace triglav::renderer {

class ModelRenderer;
class Renderer;

struct SceneObject
{
   triglav::Name model;
   glm::vec3 position;
   glm::quat rotation;
   glm::vec3 scale;
};

class Scene
{
 public:
   Scene(Renderer &renderer, ModelRenderer &context3D, ShadowMap &shadowMap, DebugLinesRenderer &debugLines,
         triglav::resource::ResourceManager &resourceManager);

   void update();
   void add_object(SceneObject object);
   void compile_scene();
   void render() const;
   void render_shadow_map() const;
   void render_debug_lines() const;

   void load_level(LevelName name);

   void set_camera(glm::vec3 position, glm::quat orientation);

   [[nodiscard]] const Camera &camera() const;
   [[nodiscard]] Camera &camera();
   [[nodiscard]] const OrthoCamera &shadow_map_camera() const;

 private:
   Renderer &m_renderer;
   ModelRenderer &m_context3D;
   ShadowMap &m_shadowMap;
   triglav::resource::ResourceManager &m_resourceManager;
   DebugLinesRenderer &m_debugLinesRenderer;

   OrthoCamera m_shadowMapCamera{};
   Camera m_camera{};

   std::vector<SceneObject> m_objects;
   std::vector<triglav::render_core::InstancedModel> m_instancedObjects;
   std::vector<DebugLines> m_debugLines;
};

}// namespace renderer