#pragma once

#include "triglav/desktop/ISurface.hpp"
#include "triglav/graphics_api/Device.h"
#include "triglav/graphics_api/RenderTarget.h"
#include "triglav/graphics_api/Swapchain.h"
#include "triglav/render_core/RenderGraph.h"
#include "triglav/renderer/TextRenderer.h"
#include "triglav/resource/ResourceManager.h"

#include <memory>

namespace demo {

class SplashScreen
{
 public:
   SplashScreen(triglav::graphics_api::Surface& surface, triglav::graphics_api::Device& device,
                triglav::resource::ResourceManager& resourceManager);

   void update();
   void on_close();

   void on_started_loading_asset(triglav::ResourceName resourceName);
   void on_finished_loading_asset(triglav::ResourceName resourceName, triglav::u32 loadedAssets, triglav::u32 totalAssets);

 private:
   triglav::graphics_api::Surface& m_surface;
   triglav::graphics_api::Device& m_device;
   triglav::resource::ResourceManager& m_resourceManager;
   triglav::graphics_api::Swapchain m_swapchain;
   triglav::graphics_api::RenderTarget m_renderTarget;
   std::vector<triglav::graphics_api::Framebuffer> m_framebuffers;
   triglav::renderer::TextRenderer m_textRenderer;
   triglav::graphics_api::CommandList m_commandList;
   triglav::graphics_api::Semaphore m_frameReadySemaphore;
   triglav::graphics_api::Semaphore m_targetSemaphore;
   triglav::graphics_api::Fence m_frameFinishedFence;
   triglav::renderer::TextObject m_textTitle;
   triglav::renderer::TextObject m_textDesc;
   triglav::renderer::TextObject m_textStatus;
   std::mutex m_updateTextMutex;

   triglav::resource::ResourceManager::OnStartedLoadingAssetDel::Sink<SplashScreen> m_onStartedLoadingAssetSink;
   triglav::resource::ResourceManager::OnFinishedLoadingAssetDel::Sink<SplashScreen> m_onFinishedLoadingAssetSink;
};

}// namespace demo