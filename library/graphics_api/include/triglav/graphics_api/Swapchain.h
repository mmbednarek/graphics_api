#pragma once

#include "Framebuffer.h"
#include "IRenderTarget.hpp"
#include "Texture.h"
#include "vulkan/ObjectWrapper.hpp"

#include <optional>

namespace triglav::graphics_api {

class RenderPass;
class Semaphore;
class QueueManager;

DECLARE_VLK_WRAPPED_CHILD_OBJECT(SwapchainKHR, Device)

class Swapchain final : public IRenderTarget
{
 public:
   Swapchain(QueueManager &queueManager, const ColorFormat &colorFormat, const ColorFormat &depthFormat, SampleCount sampleCount,
             const Resolution &resolution, Texture depthAttachment, std::optional<Texture> colorAttachment,
             std::vector<vulkan::ImageView> imageViews, vulkan::SwapchainKHR swapchain);

   [[nodiscard]] Subpass vulkan_subpass() override;
   [[nodiscard]] std::vector<VkAttachmentDescription> vulkan_attachments() override;
   [[nodiscard]] std::vector<VkSubpassDependency> vulkan_subpass_dependencies() override;
   [[nodiscard]] Resolution resolution() const;
   [[nodiscard]] ColorFormat color_format() const;
   [[nodiscard]] SampleCount sample_count() const override;
   [[nodiscard]] int color_attachment_count() const override;

   [[nodiscard]] VkSwapchainKHR vulkan_swapchain() const;
   [[nodiscard]] uint32_t get_available_framebuffer(const Semaphore &semaphore) const;

   [[nodiscard]] Result<std::vector<Framebuffer>> create_framebuffers(const RenderPass &renderPass);
   [[nodiscard]] Status present(const Semaphore &semaphore, uint32_t framebufferIndex);

 private:
   std::reference_wrapper<QueueManager> m_queueManager;
   ColorFormat m_colorFormat;
   ColorFormat m_depthFormat;
   SampleCount m_sampleCount;
   Resolution m_resolution;
   Texture m_depthAttachment;
   std::optional<Texture> m_colorAttachment;// std::nullopt if multisampling is disabled
   std::vector<vulkan::ImageView> m_imageViews;

   vulkan::SwapchainKHR m_swapchain;
};

}// namespace triglav::graphics_api