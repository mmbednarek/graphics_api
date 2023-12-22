#pragma once

#include "Buffer.h"
#include "GraphicsApi.hpp"
#include "Pipeline.h"

namespace graphics_api {

class Pipeline;
class Framebuffer;

class CommandList
{
 public:
   CommandList(VkCommandBuffer commandBuffer, VkDevice device, VkCommandPool commandPool);
   ~CommandList();

   CommandList(const CommandList &other)            = delete;
   CommandList &operator=(const CommandList &other) = delete;
   CommandList(CommandList &&other) noexcept;
   CommandList &operator=(CommandList &&other) noexcept;

   [[nodiscard]] Status reset();
   [[nodiscard]] Status begin_one_time();
   [[nodiscard]] Status begin_graphic_commands(const Framebuffer &framebuffer, const Color &clearColor);
   [[nodiscard]] Status finish() const;
   [[nodiscard]] VkCommandBuffer vulkan_command_buffer() const;

   void bind_pipeline(const Pipeline &pipeline) const;
   void bind_descriptor_set(const DescriptorView &descriptorSet) const;
   void draw_primitives(int vertexCount, int vertexOffset) const;
   void draw_indexed_primitives(int indexCount, int indexOffset, int vertexOffset) const;
   void bind_vertex_buffer(const Buffer &buffer, uint32_t layoutIndex) const;
   void bind_index_buffer(const Buffer &buffer) const;
   void copy_buffer(const Buffer &source, const Buffer &dest);
   void copy_buffer_to_texture(const Buffer &source, const Texture &destination);
   void set_is_one_time(bool value);

   template<typename TIndexArray>
   void bind_index_array(const TIndexArray &array) const
   {
      this->bind_index_buffer(array.buffer());
   }

   template<typename TVertexArray>
   void bind_vertex_array(const TVertexArray &array, const uint32_t binding = 0) const
   {
      this->bind_vertex_buffer(array.buffer(), binding);
   }

   template<typename TMesh>
   void draw_mesh(const TMesh &mesh) const
   {
      this->bind_vertex_array(mesh.vertices, 0);
      this->bind_index_array(mesh.indices);
      this->draw_indexed_primitives(mesh.indices.count(), 0, 0);
   }

 private:
   VkCommandBuffer m_commandBuffer;
   VkDevice m_device;
   VkCommandPool m_commandPool;
   bool m_isOneTime;
};
}// namespace graphics_api
