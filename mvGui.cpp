#include "mvGui.h"

mv::GuiHandler::GuiHandler(GLFWwindow *p_GLFWwindow, const vk::Instance &p_Instance,
                           const vk::PhysicalDevice &p_PhysicalDevice, const vk::Device &p_LogicalDevice,
                           const mv::Swap &p_MvSwap, const vk::CommandPool &p_CommandPool,
                           const vk::Queue &p_GraphicsQueue,
                           std::unordered_map<std::string, vk::RenderPass> &p_RenderPassMap,
                           const vk::DescriptorPool &p_DescriptorPool)
{
    // Ensure gui not already in render pass map
    if (p_RenderPassMap.find("gui") != p_RenderPassMap.end())
    {
        std::cout << "ImGui already initialized, skipping...\n";
        return;
    }
    // Create render pass
    createRenderPass(p_RenderPassMap, p_LogicalDevice, p_MvSwap.colorFormat);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    [[maybe_unused]] ImGuiIO &io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = p_Instance,
        .PhysicalDevice = p_PhysicalDevice,
        .Device = p_LogicalDevice,
        .QueueFamily = p_MvSwap.graphicsIndex,
        .Queue = p_GraphicsQueue,
        .PipelineCache = nullptr,
        .DescriptorPool = p_DescriptorPool,
        .MinImageCount = static_cast<uint32_t>(p_MvSwap.buffers->size()),
        .ImageCount = static_cast<uint32_t>(p_MvSwap.buffers->size()),
        .Allocator = nullptr,
        .CheckVkResultFn = nullptr,
    };
    ImGui_ImplGlfw_InitForVulkan(p_GLFWwindow, true);
    ImGui_ImplVulkan_Init(&initInfo, p_RenderPassMap.at("gui"));

    /*
      Upload ImGui render fonts
    */
    vk::CommandBuffer guiCommandBuffer = Helper::beginCommandBuffer(p_LogicalDevice, p_CommandPool);

    ImGui_ImplVulkan_CreateFontsTexture(guiCommandBuffer);

    Helper::endCommandBuffer(p_LogicalDevice, p_CommandPool, guiCommandBuffer, p_GraphicsQueue);

    ImGui_ImplVulkan_DestroyFontUploadObjects();
    return;
}

mv::GuiHandler::~GuiHandler()
{
    return;
}

void mv::GuiHandler::createRenderPass(std::unordered_map<std::string, vk::RenderPass> &p_RenderPassMap,
                                      const vk::Device &p_LogicalDevice, const vk::Format &p_AttachmentColorFormat)
{
    std::array<vk::AttachmentDescription, 1> guiAttachments;

    // ImGui attachment
    guiAttachments[0].format = p_AttachmentColorFormat;
    guiAttachments[0].samples = vk::SampleCountFlagBits::e1;
    guiAttachments[0].loadOp = vk::AttachmentLoadOp::eLoad;
    guiAttachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    guiAttachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    guiAttachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    guiAttachments[0].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
    guiAttachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference guiRef;
    guiRef.attachment = 0;
    guiRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription guiPass;
    guiPass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    guiPass.colorAttachmentCount = 1;
    guiPass.pColorAttachments = &guiRef;

    std::vector<vk::SubpassDescription> guiSubpasses = {guiPass};

    std::array<vk::SubpassDependency, 1> guiDependencies;
    // ImGui subpass
    guiDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    guiDependencies[0].dstSubpass = 0;
    guiDependencies[0].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    guiDependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    guiDependencies[0].srcAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    guiDependencies[0].dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    guiDependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

    vk::RenderPassCreateInfo guiRenderPassInfo;
    guiRenderPassInfo.attachmentCount = static_cast<uint32_t>(guiAttachments.size());
    guiRenderPassInfo.pAttachments = guiAttachments.data();
    guiRenderPassInfo.subpassCount = static_cast<uint32_t>(guiSubpasses.size());
    guiRenderPassInfo.pSubpasses = guiSubpasses.data();
    guiRenderPassInfo.dependencyCount = static_cast<uint32_t>(guiDependencies.size());
    guiRenderPassInfo.pDependencies = guiDependencies.data();

    vk::RenderPass newPass = p_LogicalDevice.createRenderPass(guiRenderPassInfo);

    p_RenderPassMap.insert({
        "gui",
        std::move(newPass),
    });

    return;
}

std::vector<vk::Framebuffer> mv::GuiHandler::createFramebuffers(const vk::Device &p_LogicalDevice,
                                                                const vk::RenderPass &p_GuiRenderPass,
                                                                std::vector<struct SwapchainBuffer> &p_SwapchainBuffers,
                                                                uint32_t p_SwapExtentWidth, uint32_t p_SwapExtentHeight)
{
    std::vector<vk::Framebuffer> framebuffers;

    // Attachment for ImGui rendering
    std::array<vk::ImageView, 1> imguiAttachments;

    // ImGui must only have 1 attachment
    vk::FramebufferCreateInfo guiFrameInfo;
    guiFrameInfo.renderPass = p_GuiRenderPass;
    guiFrameInfo.attachmentCount = static_cast<uint32_t>(imguiAttachments.size());
    guiFrameInfo.pAttachments = imguiAttachments.data();
    guiFrameInfo.width = p_SwapExtentWidth;
    guiFrameInfo.height = p_SwapExtentHeight;
    guiFrameInfo.layers = 1;

    framebuffers.resize(static_cast<uint32_t>(p_SwapchainBuffers.size()));
    for (size_t i = 0; i < p_SwapchainBuffers.size(); i++)
    {
        imguiAttachments[0] = p_SwapchainBuffers.at(i).view;
        framebuffers.at(i) = p_LogicalDevice.createFramebuffer(guiFrameInfo);
    }
    return framebuffers;
}

void mv::GuiHandler::newFrame(void)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    return;
}

void mv::GuiHandler::renderFrame(void)
{
    ImGui::Render();
    return;
}

void mv::GuiHandler::cleanup(const vk::Device &p_LogicalDevice)
{
    p_LogicalDevice.waitIdle();

    ImGui_ImplVulkan_Shutdown();
    ImGui::DestroyContext();
    return;
}

void mv::GuiHandler::doRenderPass(const vk::RenderPass &p_RenderPass, const vk::Framebuffer &p_Framebuffer,
                                  const vk::CommandBuffer &p_CommandBuffer, vk::Extent2D &p_RenderAreaExtent)
{
    vk::RenderPassBeginInfo guiPassInfo;
    guiPassInfo.renderPass = p_RenderPass;
    guiPassInfo.framebuffer = p_Framebuffer;
    guiPassInfo.renderArea.offset.x = 0;
    guiPassInfo.renderArea.offset.y = 0;
    guiPassInfo.renderArea.extent.width = p_RenderAreaExtent.width;
    guiPassInfo.renderArea.extent.height = p_RenderAreaExtent.height;

    p_CommandBuffer.beginRenderPass(guiPassInfo, vk::SubpassContents::eInline);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_CommandBuffer);

    p_CommandBuffer.endRenderPass();
}

void mv::GuiHandler::update(const vk::Extent2D &p_SwapExtent, float p_RenderDelta, float p_FrameDelta)
{
    auto updateEnd = std::chrono::high_resolution_clock::now();
    float timeSince = std::chrono::duration<float, std::ratio<1L, 1L>>(updateEnd - lastDeltaUpdate).count();
    if (timeSince >= 1.0f)
    {
        storedRenderDelta = p_RenderDelta;
        storedFrameDelta = p_FrameDelta;
        lastDeltaUpdate = std::chrono::high_resolution_clock::now();
    }
    ImGui::BeginMainMenuBar();

    /*
        File Menu
    */
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Open", nullptr))
        {
            // Load map file
        }
        if (ImGui::MenuItem("Save", nullptr))
        {
            // Save map file
        }
        if (ImGui::MenuItem("Save As...", nullptr))
        {
            // Save map file & force naming
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();

    /*
        Engine status data
    */
    // Bottom left window
    ImGuiWindowFlags engineDataFlags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;
    ImGui::SetNextWindowPos(ImVec2(0, p_SwapExtent.height - 32));
    ImGui::SetNextWindowSize(ImVec2(p_SwapExtent.width, 32));
    ImGui::Begin("Status", nullptr, engineDataFlags);
    ImGui::Text("Render rate: %f ms | Frame time: %f ms", storedRenderDelta, storedFrameDelta);
    ImGui::End();
}