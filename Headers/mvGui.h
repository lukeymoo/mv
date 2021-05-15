#ifndef HEADERS_MVGUI_H_
#define HEADERS_MVGUI_H_

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
// clang-format on

#include <iostream>

#include "mvHelper.h"
#include "mvSwap.h"

namespace mv
{
    class GuiHandler
    {
      public:
        GuiHandler(GLFWwindow *p_GLFWwindow, const vk::Instance &p_Instance, const vk::PhysicalDevice &p_PhysicalDevice,
                   const vk::Device &p_LogicalDevice, const mv::Swap &p_MvSwap, const vk::CommandPool &p_CommandPool,
                   const vk::Queue &p_GraphicsQueue, std::unordered_map<std::string, vk::RenderPass> &p_RenderPassMap,
                   const vk::DescriptorPool &p_DescriptorPool);
        ~GuiHandler();

        void createRenderPass(std::unordered_map<std::string, vk::RenderPass> &p_RenderPassMap,
                              const vk::Device &p_LogicalDevice, const vk::Format &p_AttachmentColorFormat);

        std::vector<vk::Framebuffer> createFramebuffers(const vk::Device &p_LogicalDevice,
                                                        const vk::RenderPass &p_GuiRenderPass,
                                                        std::vector<struct SwapchainBuffer> &p_SwapchainBuffers,
                                                        uint32_t p_SwapExtentWidth, uint32_t p_SwapExtentHeight);
        void newFrame(void);
        void renderFrame(void);

        void doRenderPass(const vk::RenderPass &p_RenderPass, const vk::Framebuffer &p_Framebuffer,
                          const vk::CommandBuffer &p_CommandBuffer, vk::Extent2D &p_RenderAreaExtent);

        void cleanup(const vk::Device &p_LogicalDevice);
    };
}; // namespace mv

#endif