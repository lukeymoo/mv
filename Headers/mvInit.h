#ifndef HEADERS_MVINIT_H_
#define HEADERS_MVINIT_H_

#include <vulkan/vulkan.h>

namespace mv
{
    namespace initializer
    {
        inline VkSemaphoreCreateInfo semaphore_create_info()
        {
            VkSemaphoreCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            return info;
        }

        inline VkSubmitInfo submit_info()
        {
            VkSubmitInfo info{};
            info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            return info;
        }

        inline VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool cmdPool,
                                                                     VkCommandBufferLevel level,
                                                                     uint32_t bufferCount)
        {
            VkCommandBufferAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ai.pNext = nullptr;
            ai.level = level;
            ai.commandBufferCount = bufferCount;
            ai.commandPool = cmdPool;
            return ai;
        }

        inline VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0)
        {
            VkFenceCreateInfo fi{};
            fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fi.flags = flags;
            return fi;
        }

        inline VkBufferCreateInfo buffer_create_info()
        {
            VkBufferCreateInfo buffer_info = {};
            buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            return buffer_info;
        }

        inline VkBufferCreateInfo buffer_create_info(VkBufferUsageFlags usage,
                                                   VkDeviceSize size)
        {
            VkBufferCreateInfo buffer_info = {};
            buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_info.usage = usage;
            buffer_info.size = size;
            return buffer_info;
        }

        inline VkMemoryAllocateInfo memory_allocate_info()
        {
            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            return alloc_info;
        }

        inline VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info(VkPrimitiveTopology topology,
                                                                             VkPipelineInputAssemblyStateCreateFlags flags,
                                                                             VkBool32 primitive_restart_enable)
        {
            VkPipelineInputAssemblyStateCreateInfo ia_state_create_info = {};
            ia_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            ia_state_create_info.topology = topology;
            ia_state_create_info.flags = flags;
            ia_state_create_info.primitiveRestartEnable = primitive_restart_enable;
            return ia_state_create_info;
        }

        inline VkPipelineRasterizationStateCreateInfo rasterization_state_info(VkPolygonMode polygon_mode,
                                                                             VkCullModeFlags cull_mode,
                                                                             VkFrontFace front_face,
                                                                             VkPipelineRasterizationStateCreateFlags flags = 0)
        {
            VkPipelineRasterizationStateCreateInfo raster_state_create_info = {};
            raster_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster_state_create_info.pNext = nullptr;
            raster_state_create_info.flags = flags;
            raster_state_create_info.depthClampEnable = VK_FALSE;
            raster_state_create_info.rasterizerDiscardEnable = VK_FALSE;
            raster_state_create_info.polygonMode = polygon_mode;
            raster_state_create_info.cullMode = cull_mode;
            raster_state_create_info.frontFace = front_face;
            raster_state_create_info.depthBiasEnable = VK_FALSE;
            raster_state_create_info.depthBiasConstantFactor = 0.0f;
            raster_state_create_info.depthBiasClamp = 0.0f;
            raster_state_create_info.depthBiasSlopeFactor = 0.0f;
            raster_state_create_info.lineWidth = 1.0f;
            return raster_state_create_info;
        }

        inline VkPipelineColorBlendAttachmentState color_blend_attachment_state(VkColorComponentFlags color_write_mask,
                                                                             VkBool32 blend_enable)
        {
            VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
            color_blend_attachment_state.colorWriteMask = color_write_mask;
            color_blend_attachment_state.blendEnable = blend_enable;
            return color_blend_attachment_state;
        }

        inline VkPipelineColorBlendStateCreateInfo color_blend_state_info(uint32_t attachment_count,
                                                                       const VkPipelineColorBlendAttachmentState *p_attachments)
        {
            VkPipelineColorBlendStateCreateInfo color_blend_state_info = {};
            color_blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blend_state_info.attachmentCount = attachment_count;
            color_blend_state_info.pAttachments = p_attachments;
            return color_blend_state_info;
        }

        inline VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info(VkBool32 depth_test_enable,
                                                                           VkBool32 depth_write_enable,
                                                                           VkCompareOp depth_compare_op)
        {
            VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
            depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil_state_create_info.depthTestEnable = depth_test_enable;
            depth_stencil_state_create_info.depthWriteEnable = depth_write_enable;
            depth_stencil_state_create_info.depthCompareOp = depth_compare_op;
            depth_stencil_state_create_info.back.compareOp = VK_COMPARE_OP_ALWAYS;
            return depth_stencil_state_create_info;
        }

        inline VkPipelineViewportStateCreateInfo viewport_state_info(uint32_t viewport_count,
                                                                   uint32_t scissor_count,
                                                                   VkPipelineViewportStateCreateFlags flags = 0)
        {
            VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
            viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport_state_create_info.viewportCount = viewport_count;
            viewport_state_create_info.scissorCount = scissor_count;
            viewport_state_create_info.flags = flags;
            return viewport_state_create_info;
        }

        inline VkPipelineMultisampleStateCreateInfo multisample_state_info(VkSampleCountFlagBits rasterization_samples,
                                                                         VkPipelineMultisampleStateCreateFlags flags = 0)
        {
            VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {};
            multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisample_state_create_info.rasterizationSamples = rasterization_samples;
            multisample_state_create_info.flags = flags;
            return multisample_state_create_info;
        }

        inline VkGraphicsPipelineCreateInfo pipeline_create_info(VkPipelineLayout layout,
                                                               VkRenderPass render_pass,
                                                               VkPipelineCreateFlags flags = 0)
        {
            VkGraphicsPipelineCreateInfo pipeline_create_info = {};
            pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipeline_create_info.layout = layout;
            pipeline_create_info.renderPass = render_pass;
            pipeline_create_info.flags = flags;
            pipeline_create_info.basePipelineIndex = -1;
            pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
            return pipeline_create_info;
        }

        inline VkGraphicsPipelineCreateInfo pipeline_create_info()
        {
            VkGraphicsPipelineCreateInfo pipeline_create_info{};
            pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipeline_create_info.basePipelineIndex = -1;
            pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
            return pipeline_create_info;
        }
    };
};

#endif