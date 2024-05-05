#pragma once

#include "FrameResources.h"
#include "IRenderNode.hpp"

#include "triglav/graphics_api/QueueManager.h"
#include "triglav/Name.hpp"

#include <map>
#include <memory>
#include <utility>

namespace triglav::render_core {

class FrameResources;

class RenderGraph
{
 public:
   explicit RenderGraph(graphics_api::Device &device);

   template<typename TNode, typename... TArgs>
   void emplace_node(Name name, TArgs &&...args)
   {
      m_nodes.emplace(name, std::make_unique<TNode>(std::forward<TArgs>(args)...));
   }

   template<typename TNode>
   TNode &node(const Name name)
   {
      return dynamic_cast<TNode &>(*m_nodes.at(name));
   }

   void add_external_node(Name node);
   void add_dependency(Name target, Name dependency);
   bool bake(Name targetNode);
   void initialize_nodes();
   void record_command_lists();
   void set_flag(Name flag, bool isEnabled);
   void update_resolution(const graphics_api::Resolution &resolution);
   [[nodiscard]] graphics_api::Status execute();
   void await();
   [[nodiscard]] graphics_api::Semaphore& target_semaphore();
   [[nodiscard]] graphics_api::Semaphore& semaphore(Name parent, Name child);
   [[nodiscard]] u32 triangle_count(Name node);
   FrameResources& active_frame_resources();
   void swap_frames();

   void clean();


 private:
   graphics_api::Device &m_device;
   std::set<Name> m_externalNodes;
   std::map<Name, std::unique_ptr<IRenderNode>> m_nodes;
   std::multimap<Name, Name> m_dependencies;
   std::vector<Name> m_nodeOrder;
   std::vector<graphics_api::Framebuffer> m_framebuffers;
   std::array<FrameResources, 2> m_frameResources;
   Name m_targetNode{};
   u32 m_activeFrame{0};
};

}// namespace triglav::render_core
