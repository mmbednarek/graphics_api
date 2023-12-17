#include "DescriptorWriter.h"

#include "Buffer.h"
#include "Sampler.h"
#include "Texture.h"
#include "vulkan/Util.h"


#include <Device.h>

namespace graphics_api {

DescriptorWriter::DescriptorWriter(const Device &device, const DescriptorView &descView) :
   m_device(device.vulkan_device()),
   m_descriptorSet(descView.vulkan_descriptor_set())
{

}

DescriptorWriter::~DescriptorWriter()
{
   this->update();
}

void DescriptorWriter::set_raw_uniform_buffer(const uint32_t binding, const Buffer &buffer)
{
   VkWriteDescriptorSet writeDescriptorSet{};
   writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   writeDescriptorSet.dstSet          = m_descriptorSet;
   writeDescriptorSet.dstBinding      = binding;
   writeDescriptorSet.dstArrayElement = 0;
   writeDescriptorSet.descriptorCount = 1;
   writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

   VkDescriptorBufferInfo bufferInfo{};
   bufferInfo.offset              = 0;
   bufferInfo.range               = buffer.size();
   bufferInfo.buffer              = buffer.vulkan_buffer();
   writeDescriptorSet.pBufferInfo = &m_descriptorBufferInfos.emplace_back(bufferInfo);

   m_descriptorWrites.emplace_back(writeDescriptorSet);
}

void DescriptorWriter::set_sampled_texture(const uint32_t binding, const Texture &texture,
                                           const Sampler &sampler)
{
   VkWriteDescriptorSet writeDescriptorSet{};
   writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   writeDescriptorSet.dstSet          = m_descriptorSet;
   writeDescriptorSet.dstBinding      = binding;
   writeDescriptorSet.dstArrayElement = 0;
   writeDescriptorSet.descriptorCount = 1;
   writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

   VkDescriptorImageInfo imageInfo{};
   imageInfo.imageLayout         = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   imageInfo.imageView           = texture.vulkan_image_view();
   imageInfo.sampler             = sampler.vulkan_sampler();
   writeDescriptorSet.pImageInfo = &m_descriptorImageInfos.emplace_back(imageInfo);

   m_descriptorWrites.emplace_back(writeDescriptorSet);
}

VkDescriptorSet DescriptorWriter::vulkan_descriptor_set() const
{
   return m_descriptorSet;
}

void DescriptorWriter::update()
{
   vkUpdateDescriptorSets(m_device, m_descriptorWrites.size(), m_descriptorWrites.data(), 0, nullptr);
}

}// namespace graphics_api
