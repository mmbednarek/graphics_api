#include "ShaderLoader.h"

#include "graphics_api/Device.h"
#include "triglav/io/File.h"

#include <fstream>

namespace triglav::resource {

graphics_api::Shader Loader<ResourceType::FragmentShader>::load_gpu(graphics_api::Device &device,
                                                                    const std::string_view path)
{
   return GAPI_CHECK(device.create_shader(graphics_api::ShaderStage::Fragment, "main", io::read_whole_file(path)));
}

graphics_api::Shader Loader<ResourceType::VertexShader>::load_gpu(graphics_api::Device &device,
                                                                  const std::string_view path)
{
   return GAPI_CHECK(device.create_shader(graphics_api::ShaderStage::Vertex, "main", io::read_whole_file(path)));
}

}// namespace triglav::resource
