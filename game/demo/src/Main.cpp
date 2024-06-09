#include "GameInstance.h"

#include "triglav/desktop/Entrypoint.hpp"
#include "triglav/desktop/IDisplay.hpp"
#include "triglav/desktop/ISurface.hpp"
#include "triglav/desktop/ISurfaceEventListener.hpp"
#include "triglav/io/CommandLine.h"
#include "triglav/renderer/Renderer.h"
#include "triglav/resource/PathManager.h"
#include "triglav/threading/ThreadPool.h"

#include <spdlog/spdlog.h>

using triglav::desktop::DefaultSurfaceEventListener;
using triglav::desktop::IDisplay;
using triglav::desktop::InputArgs;
using triglav::desktop::ISurface;
using triglav::desktop::MouseButton;
using triglav::io::CommandLine;
using triglav::resource::PathManager;
using triglav::threading::ThreadPool;

using namespace triglav::name_literals;

constexpr auto g_initialWidth = 1280;
constexpr auto g_initialHeight = 720;

int triglav_main(InputArgs& args, IDisplay& display)
{
   // Parse program arguments
   CommandLine::the().parse(args.arg_count, args.args);

   // Initialize global thread pool
   triglav::threading::set_thread_id(triglav::threading::g_mainThread);

   const auto threadCount = CommandLine::the().arg_int("threadCount"_name).value_or(8);
   ThreadPool::the().initialize(threadCount);

   spdlog::info("content path: {}", PathManager::the().content_path().string());
   spdlog::info("build path: {}", PathManager::the().build_path().string());
   spdlog::info("initializing renderer");

   demo::GameInstance instance(display, {g_initialWidth, g_initialHeight});
   instance.loop(display);

   ThreadPool::the().quit();

   return EXIT_SUCCESS;
}
