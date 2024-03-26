#include "Framebuffer.h"

#include "Texture.h"

namespace triglav::graphics_api {

Framebuffer::Framebuffer(const Resolution resolution, const VkRenderPass renderPass,
                         vulkan::Framebuffer framebuffer, std::vector<Texture> textures) :
    m_resolution(resolution),
    m_renderPass(renderPass),
    m_framebuffer(std::move(framebuffer)),
    m_textures(std::move(textures))
{
}

Resolution Framebuffer::resolution() const
{
   return m_resolution;
}

VkFramebuffer Framebuffer::vulkan_framebuffer() const
{
   return *m_framebuffer;
}

VkRenderPass Framebuffer::vulkan_render_pass() const
{
   return m_renderPass;
}

Texture &Framebuffer::texture(const u32 index)
{
   return m_textures.at(index);
}

}// namespace triglav::graphics_api
