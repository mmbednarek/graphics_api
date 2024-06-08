#include "TextureLoader.h"

#include "triglav/graphics_api/Device.h"
#include "triglav/graphics_api/Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using triglav::graphics_api::SampleCount;
using triglav::graphics_api::TextureUsage;

namespace triglav::resource {

graphics_api::Texture Loader<ResourceType::Texture>::load_gpu(graphics_api::Device& device, const io::Path& path)
{
   int texWidth, texHeight, texChannels;
   const stbi_uc* pixels = stbi_load(path.string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
   assert(pixels != nullptr);

   auto texture = GAPI_CHECK(device.create_texture(
      GAPI_FORMAT(RGBA, sRGB), {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight)},
      TextureUsage::Sampled | TextureUsage::TransferDst | TextureUsage::TransferSrc, SampleCount::Single, graphics_api::g_maxMipMaps));
   GAPI_CHECK_STATUS(texture.write(device, pixels));
   return texture;
}

}// namespace triglav::resource
