#pragma once

#include "RectangleRenderer.h"
#include "TextRenderer.h"

#include "triglav/graphics_api/RenderTarget.h"
#include "triglav/graphics_api/Texture.h"
#include "triglav/render_core/IRenderNode.hpp"

#include <map>
#include <string_view>

namespace triglav::renderer::node {

struct TextProperty
{
   TextObject label;
   TextObject value;

   TextProperty(TextObject label, TextObject value) :
       label(std::move(label)),
       value(std::move(value))
   {
   }
};

struct LabelGroup
{
   TextObject m_groupTitle;
   std::vector<NameID> m_labels;
};

class UserInterface : public render_core::IRenderNode
{
 public:
   UserInterface(graphics_api::Device &device, graphics_api::Resolution resolution,
                 resource::ResourceManager &resourceManager);

   [[nodiscard]] graphics_api::WorkTypeFlags work_types() const override;
   std::unique_ptr<render_core::NodeFrameResources> create_node_resources() override;
   void record_commands(render_core::FrameResources &frameResources,
                        render_core::NodeFrameResources &resources,
                        graphics_api::CommandList &cmdList) override;
   void add_label_group(NameID id, std::string_view name);
   void add_label(NameID group, NameID id, std::string_view name);
   void set_value(NameID id, std::string_view value);

 private:
   resource::ResourceManager &m_resourceManager;
   graphics_api::RenderTarget m_textureRenderTarget;
   RectangleRenderer m_rectangleRenderer;
   TextRenderer m_textRenderer;
   Rectangle m_rectangle;
   TextObject m_titleLabel;
   std::map<NameID, TextProperty> m_properties;
   std::map<NameID, LabelGroup> m_labelGroups;
};

}// namespace triglav::renderer::node
