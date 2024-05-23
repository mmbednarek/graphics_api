#pragma once

#include "Loader.hpp"

#include "triglav/Name.hpp"
#include "triglav/render_core/GlyphAtlas.h"
#include "triglav/io/Path.h"

#include <string_view>

namespace triglav::resource {

class ResourceManager;

template<>
struct Loader<ResourceType::GlyphAtlas>
{
  constexpr static ResourceLoadType type{ResourceLoadType::GraphicsDependent};

   static render_core::GlyphAtlas load_gpu(ResourceManager &resourceManager, graphics_api::Device &device, const io::Path& path);
};

}// namespace triglav::resource
