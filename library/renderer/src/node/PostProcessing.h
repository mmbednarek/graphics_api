#pragma once

#include "triglav/render_core/IRenderNode.hpp"

#include "PostProcessingRenderer.h"

namespace triglav::renderer::node {

class PostProcessing : public render_core::IRenderNode
{
public:
   PostProcessing(PostProcessingRenderer &renderer, std::vector<graphics_api::Framebuffer> &framebuffers);

  [[nodiscard]] graphics_api::WorkTypeFlags work_types() const override;
   void record_commands(graphics_api::CommandList &cmdList) override;

  void set_index(u32 index);

private:
   PostProcessingRenderer &m_renderer;
   std::vector<graphics_api::Framebuffer> &m_framebuffers;
   u32 m_frameIndex{};
};

}