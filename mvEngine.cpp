#include "mvEngine.h"

mv::Engine::Engine(int w, int h, const char *title)
    : Window(w, h, title)
{
    return;
}

mv::Engine::~Engine(void)
{
    return;
}

void mv::Engine::go(void)
{
    std::cout << "[+] Preparing vulkan" << std::endl;
    prepare();

    // Create uniform buffers
    // Create destriptor pool
    // Create descriptor set layout
    // Create descriptor sets
    // Record commands to already created command buffers(per swap)
    // Create pipeline and tie all resources together

    while ((event = xcb_wait_for_event(connection))) // replace this as it is a blocking call
    {
        switch (event->response_type & ~0x80)
        {
        default:
            break;
        }

        free(event);
    }
    return;
}

void mv::Engine::createDescriptorPool(void)
{
    /*
        Per model uniform buffer
        Per scene View/Projection matrix
    */
    VkDescriptorPoolSize uniformPools = {};
    uniformPools.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformPools.descriptorCount = 2;

    std::vector<VkDescriptorPoolSize> poolSizes = { uniformPools };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.maxSets = swapChain.imageCount; // per swap image set allocation
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    return;
}

void mv::Engine::createDescriptorLayout(void)
{
    return;
}

void mv::Engine::createDescriptorSets(void)
{
    return;
}

void mv::Engine::recordCommandBuffer(uint32_t imageIndex)
{
    // VkCommandBufferBeginInfo beginInfo = {};
    // beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // beginInfo.pNext = nullptr;
    // beginInfo.flags = 0;
    // beginInfo.pInheritanceInfo = nullptr;

    // if (vkBeginCommandBuffer(cmdBuffers[imageIndex], &beginInfo) != VK_SUCCESS)
    // {
    //     throw std::runtime_error("Failed to begin recording to command buffer");
    // }

    // VkRenderPassBeginInfo passInfo = {};
    // passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    // passInfo.pNext = nullptr;
    // passInfo.renderPass = m_RenderPass;
    // passInfo.framebuffer = frameBuffers[imageIndex];
    // passInfo.clearValueCount = 1;
    // passInfo.pClearValues = &clearvals

    return;
}