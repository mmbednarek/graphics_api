#pragma once

#include "CameraBase.h"

namespace triglav::renderer {

class OrthoCamera final : public CameraBase
{
 public:
   void set_viewport_size(float width, float height);
   void set_viewspace_width(float width);

   [[nodiscard]] const glm::mat4 &projection_matrix() const override;
   [[nodiscard]] float to_linear_depth(float depth) const override;

 private:
   float m_aspect{1.0f};
   float m_viewSpaceWidth{12.0f};

   mutable bool m_hasCachedProjectionMatrix{false};
   mutable glm::mat4 m_projectionMat{};
};

}// namespace renderer
