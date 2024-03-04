#pragma once

#include "Loader.hpp"

#include "triglav/Name.hpp"
#include "triglav/render_core/Model.hpp"

#include <string_view>

namespace triglav::resource {

template<>
struct Loader<ResourceType::Model>
{
   constexpr static ResourceLoadType type{ResourceLoadType::Graphics};

   static render_core::Model load_gpu(graphics_api::Device &device, std::string_view path);
};

}// namespace triglav::resource
