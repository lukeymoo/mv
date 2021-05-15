#include "mvEngine.h"

mv::Engine::Engine(int w, int h, const char *title) : Window(w, h, title)
{
    pipelines = std::make_unique<std::unordered_map<std::string, vk::Pipeline>>();
    pipelineLayouts = std::make_unique<std::unordered_map<std::string, vk::PipelineLayout>>();
    renderPasses = std::make_unique<std::unordered_map<std::string, vk::RenderPass>>();
    return;
}

mv::Engine::~Engine()
{
    if (!mvDevice)
        return;

    mvDevice->logicalDevice->waitIdle();

    if (pipelines)
    {
        for (auto &pipeline : *pipelines)
        {
            if (pipeline.second)
                mvDevice->logicalDevice->destroyPipeline(pipeline.second);
        }
        pipelines.reset();
    }

    if (pipelineLayouts)
    {
        for (auto &layout : *pipelineLayouts)
        {
            if (layout.second)
                mvDevice->logicalDevice->destroyPipelineLayout(layout.second);
        }
        pipelineLayouts.reset();
    }

    /*
      DEBUGGING CLEANUP
    */
    if (reticleMesh.vertexBuffer)
    {
        mvDevice->logicalDevice->destroyBuffer(reticleMesh.vertexBuffer);
        mvDevice->logicalDevice->freeMemory(reticleMesh.vertexMemory);
    }
    if (reticleObj.uniformBuffer.buffer)
    {
        reticleObj.uniformBuffer.destroy(*mvDevice);
    }

    // collection struct will handle cleanup of models & objs
    if (collectionHandler)
    {
        collectionHandler->cleanup(*mvDevice, *descriptorAllocator);
    }
}

void mv::Engine::cleanupSwapchain(void)
{
    // destroy command buffers
    if (commandBuffers)
    {
        if (!commandBuffers->empty())
        {
            mvDevice->logicalDevice->freeCommandBuffers(*commandPool, *commandBuffers);
        }
        commandBuffers.reset();
        commandBuffers = std::make_unique<std::vector<vk::CommandBuffer>>();
    }

    // destroy framebuffers
    if (coreFramebuffers)
    {
        if (!coreFramebuffers->empty())
        {
            for (auto &buffer : *coreFramebuffers)
            {
                if (buffer)
                    mvDevice->logicalDevice->destroyFramebuffer(buffer, nullptr);
            }
            coreFramebuffers.reset();
            coreFramebuffers = std::make_unique<std::vector<vk::Framebuffer>>();
        }
    }

    if (depthStencil)
    {
        if (depthStencil->image)
        {
            mvDevice->logicalDevice->destroyImage(depthStencil->image, nullptr);
        }
        if (depthStencil->view)
        {
            mvDevice->logicalDevice->destroyImageView(depthStencil->view, nullptr);
        }
        if (depthStencil->mem)
        {
            mvDevice->logicalDevice->freeMemory(depthStencil->mem, nullptr);
        }
        depthStencil.reset();
        depthStencil = std::make_unique<struct mv::Window::DepthStencilStruct>();
    }

    // destroy pipelines
    if (pipelines)
    {
        for (auto &pipeline : *pipelines)
        {
            if (pipeline.second)
                mvDevice->logicalDevice->destroyPipeline(pipeline.second);
        }
        pipelines.reset();
        pipelines = std::make_unique<std::unordered_map<std::string, vk::Pipeline>>();
    }

    // destroy pipeline layouts
    if (pipelineLayouts)
    {
        for (auto &layout : *pipelineLayouts)
        {
            if (layout.second)
                mvDevice->logicalDevice->destroyPipelineLayout(layout.second);
        }
        pipelineLayouts.reset();
        pipelineLayouts = std::make_unique<std::unordered_map<std::string, vk::PipelineLayout>>();
    }

    // destroy render pass
    if (renderPasses)
    {
        for (auto &pass : *renderPasses)
        {
            if (pass.second)
                mvDevice->logicalDevice->destroyRenderPass(pass.second, nullptr);
        }
        renderPasses.reset();
        renderPasses = std::make_unique<std::unordered_map<std::string, vk::RenderPass>>();
    }

    // cleanup command pool
    if (commandPool)
    {
        mvDevice->logicalDevice->destroyCommandPool(*commandPool);
        commandPool.reset();
        commandPool = std::make_unique<vk::CommandPool>();
    }

    // cleanup swapchain
    swapchain->cleanup(*instance, *mvDevice, false);

    return;
}

// Allows swapchain to keep up with window resize
void mv::Engine::recreateSwapchain(void)
{
    std::cout << "[+] recreating swapchain" << std::endl;

    if (!mvDevice)
        throw std::runtime_error("mv device handler is somehow null, tried to recreate swap chain :: "
                                 "main");

    mvDevice->logicalDevice->waitIdle();

    // Cleanup
    cleanupSwapchain();

    *commandPool = mvDevice->createCommandPool(mvDevice->queueIdx.graphics);

    // create swapchain
    swapchain->create(*physicalDevice, *mvDevice, windowWidth, windowHeight);

    // create render pass
    setupRenderPass();

    setupDepthStencil();

    // create pipeline layout
    // create pipeline
    preparePipeline();
    // create framebuffers
    setupFramebuffer();
    // create command buffers
    createCommandBuffers();
}

void mv::Engine::go(void)
{
    std::cout << "[+] Preparing vulkan\n";
    prepare();

    // setup descriptor allocator, collection handler & camera
    // load models & create objects here
    goSetup();

    preparePipeline();

    // basic check
    assert(camera);
    assert(collectionHandler);
    assert(descriptorAllocator);

    uint32_t imageIndex = 0;
    size_t currentFrame = 0;

    bool added = false;

    [[maybe_unused]] int mouseCenterx = (swapchain->swapExtent.width / 2) + 50;
    [[maybe_unused]] int mouseCentery = (swapchain->swapExtent.height / 2) - 100;

    fps.startTimer();
    [[maybe_unused]] int fpsCounter = 0;

    using chrono = std::chrono::high_resolution_clock;

    constexpr std::chrono::nanoseconds timestep(16ms);

    std::chrono::nanoseconds accumulated(0ns);
    auto startTime = chrono::now();

    /*
        Create and initialize ImGui handler
        Will create render pass & perform pre game loop ImGui initialization
    */
    // clang-format off
    gui = std::make_unique<GuiHandler>(window,
                                        *instance,
                                        *physicalDevice,
                                        *mvDevice->logicalDevice,
                                        *swapchain,
                                        *mvDevice->commandPool,
                                        *mvDevice->graphicsQueue,
                                        *renderPasses,
                                        descriptorAllocator->get()->pool);
    // clang-format on

    *guiFramebuffers = gui->createFramebuffers(*mvDevice->logicalDevice, renderPasses->at("gui"), *swapchain->buffers,
                                               swapchain->swapExtent.width, swapchain->swapExtent.height);

    std::cout << "swap chain width => " << swapchain->swapExtent.width << "\n"
              << "swapchain height => " << swapchain->swapExtent.height << "\taspect ratio => "
              << (float)swapchain->swapExtent.width / swapchain->swapExtent.height << "\n";

    auto renderStart = chrono::now();
    auto renderStop = chrono::now();
    while (!glfwWindowShouldClose(window))
    {
        auto deltaTime = chrono::now() - startTime;
        startTime = chrono::now();
        accumulated += std::chrono::duration_cast<std::chrono::nanoseconds>(deltaTime);

        glfwPollEvents();

        while (accumulated >= timestep)
        {
            accumulated -= timestep;

            // Get input events
            mv::Keyboard::Event kbdEvent = keyboard.read();
            mv::Mouse::Event mouseEvent = mouse.read();

            if (camera->type == CameraType::eThirdPerson && !gui->hasFocus)
            {

                /*
                  Mouse scroll wheel
                */
                // if (mouse_event.type == mouse.event::etype::wheel_up) {
                //   camera->adjust_zoom(-(camera->zoom_step));
                // }
                // if (mouse_event.type == mouse.event::etype::wheel_down) {
                //   camera->adjust_zoom(camera->zoom_step);
                // }

                /*
                  Start mouse drag
                */
                if (mouseEvent.type == Mouse::Event::Type::eRightDown)
                {

                    if (!mouse.isDragging)
                    {
                        // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
                        // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        mouse.startDrag(camera->orbitAngle, camera->pitch);
                    }
                }
                if (mouseEvent.type == Mouse::Event::Type::eRightRelease)
                {
                    if (mouse.isDragging)
                    {
                        // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
                        // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                        mouse.endDrag();
                    }
                }

                // if drag enabled check for release
                if (mouse.isDragging)
                {
                    // camera orbit
                    if (mouse.dragDeltaX > 0)
                    {
                        camera->lerpOrbit(-abs(mouse.dragDeltaX));
                    }
                    else if (mouse.dragDeltaX < 0)
                    {
                        camera->adjustOrbit(abs(mouse.dragDeltaX));
                        camera->lerpOrbit(abs(mouse.dragDeltaX));
                    }

                    // camera pitch
                    if (mouse.dragDeltaY > 0)
                    {
                        camera->lerpPitch(abs(mouse.dragDeltaY));
                    }
                    else if (mouse.dragDeltaY < 0)
                    {
                        camera->lerpPitch(-abs(mouse.dragDeltaY));
                    }

                    camera->target->rotateToAngle(camera->orbitAngle);

                    mouse.storedOrbit = camera->orbitAngle;
                    mouse.storedPitch = camera->pitch;
                    mouse.clear();
                }

                /*
                  DEBUGGING CONTROLS
                */

                // PRINT OUT CAMERA STATE TO CONSOLE
                if (kbdEvent.type == Keyboard::Event::ePress && kbdEvent.code == GLFW_KEY_LEFT_CONTROL)
                {
                    std::cout << "Mouse data\n";
                    std::cout << "Orbit => " << camera->orbitAngle << "\n";
                    std::cout << "Pitch => " << camera->pitch << "\n";
                    std::cout << "Zoom level => " << camera->zoomLevel << "\n";
                }

                // TOGGLE RENDER PLAYER AIM RAYCAST
                if (kbdEvent.type == Keyboard::Event::ePress && kbdEvent.code == GLFW_KEY_LEFT_ALT)
                {
                    // toRenderMap.at("reticle_raycast") = !toRenderMap.at("reticle_raycast");
                    // std::cout << "Render player aiming raycast => " << toRenderMap.at("reticle_raycast") << "\n";
                    std::cout << "Ray cast permanently disabled\n";
                }

                /*
                  END DEBUGGING CONTROLS
                */

                // sort movement by key combination

                // forward only
                if (keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) && !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbitAngle, glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + left + right -- go straight
                if (keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) && keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbitAngle, glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + right
                if (keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) && keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbitAngle, glm::vec4(1.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + left
                if (keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) && !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbitAngle, glm::vec4(-1.0f, 0.0f, -1.0f, 1.0f));
                }

                // backward only
                if (!keyboard.isKeyState(GLFW_KEY_W) && keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) && !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbitAngle, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + left
                if (!keyboard.isKeyState(GLFW_KEY_W) && keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) && !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbitAngle, glm::vec4(-1.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + right
                if (!keyboard.isKeyState(GLFW_KEY_W) && keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) && keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbitAngle, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + left + right -- go back
                if (!keyboard.isKeyState(GLFW_KEY_W) && keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) && keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbitAngle, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }

                // left only
                if (!keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) && !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbitAngle, glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f));
                }

                // right only
                if (!keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) && keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbitAngle, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                }

                // debug -- add new objects to world with random position
                if (keyboard.isKeyState(GLFW_KEY_SPACE) && added == false)
                {
                    // added = true;
                    int min = 0;
                    int max = 90;
                    int z_max = 60;

                    std::random_device rd;
                    std::default_random_engine eng(rd());
                    std::uniform_int_distribution<int> xy_distr(min, max);
                    std::uniform_int_distribution<int> z_distr(min, z_max);

                    float x = xy_distr(eng);
                    float y = xy_distr(eng);
                    float z = z_distr(eng);
                    collectionHandler->createObject(*mvDevice, *descriptorAllocator, "models/_viking_room.fbx");
                    collectionHandler->models->at(0).objects->back().position = glm::vec3(x, y, z);
                }
            }
            // update game objects
            collectionHandler->update();

            // update view and projection matrices
            camera->update();

            /*
              Update ray cast vertices
            */
            // int min = 0;
            // int max = 90;
            // int z_max = 60;

            // std::random_device rd;
            // std::default_random_engine eng(rd());
            // std::uniform_int_distribution<int> xy_distr(min, max);
            // std::uniform_int_distribution<int> z_distr(min, z_max);

            // float x = xy_distr(eng);
            // float y = xy_distr(eng);
            // float z = z_distr(eng);
            // raycast_p1(*this, {x, y, z});
            // raycastP2(*this, {collectionHandler->models->at(1).objects->at(0).position.x,
            //                   collectionHandler->models->at(1).objects->at(0).position.y - 1.5f,
            //                   collectionHandler->models->at(1).objects->at(0).position.z});
        }

        // Game editor rendering
        {
            gui->newFrame();

            gui->update(window, swapchain->swapExtent,
                        std::chrono::duration<float, std::ratio<1L, 1000L>>(renderStop - renderStart).count(),
                        std::chrono::duration<float, std::chrono::milliseconds::period>(deltaTime).count());

            gui->renderFrame();
        }

        // TODO
        // Add alpha to render
        // Render
        renderStart = chrono::now();
        draw(currentFrame, imageIndex);
        renderStop = chrono::now();
    }

    gui->cleanup(*mvDevice->logicalDevice);

    int totalObjectCount = 0;
    for (const auto &model : *collectionHandler->models)
    {
        totalObjectCount += model.objects->size();
    }
    std::cout << "Total objects => " << totalObjectCount << std::endl;
    return;
}

/*
  Creation of all the graphics pipelines
*/
void mv::Engine::preparePipeline(void)
{
    vk::DescriptorSetLayout uniformLayout = descriptorAllocator->getLayout("uniform_layout");
    vk::DescriptorSetLayout samplerLayout = descriptorAllocator->getLayout("sampler_layout");

    std::vector<vk::DescriptorSetLayout> layoutWSampler = {
        uniformLayout,
        uniformLayout,
        uniformLayout,
        samplerLayout,
    };
    std::vector<vk::DescriptorSetLayout> layoutNoSampler = {
        uniformLayout,
        uniformLayout,
        uniformLayout,
    };

    // Pipeline for models with textures
    vk::PipelineLayoutCreateInfo pLineWithSamplerInfo;
    pLineWithSamplerInfo.setLayoutCount = static_cast<uint32_t>(layoutWSampler.size());
    pLineWithSamplerInfo.pSetLayouts = layoutWSampler.data();

    // Pipeline with no textures
    vk::PipelineLayoutCreateInfo pLineNoSamplerInfo;
    pLineNoSamplerInfo.setLayoutCount = static_cast<uint32_t>(layoutNoSampler.size());
    pLineNoSamplerInfo.pSetLayouts = layoutNoSampler.data();

    pipelineLayouts->insert({
        "sampler",
        mvDevice->logicalDevice->createPipelineLayout(pLineWithSamplerInfo),
    });

    pipelineLayouts->insert({
        "no_sampler",
        mvDevice->logicalDevice->createPipelineLayout(pLineNoSamplerInfo),
    });

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo viState;
    viState.vertexBindingDescriptionCount = 1;
    viState.pVertexBindingDescriptions = &bindingDescription;
    viState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    viState.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo iaState;
    iaState.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineInputAssemblyStateCreateInfo debugIaState; // used for dynamic states
    debugIaState.topology = vk::PrimitiveTopology::eLineList;

    vk::PipelineRasterizationStateCreateInfo rsState;
    rsState.depthClampEnable = VK_FALSE;
    rsState.rasterizerDiscardEnable = VK_FALSE;
    rsState.polygonMode = vk::PolygonMode::eFill;
    rsState.cullMode = vk::CullModeFlagBits::eNone;
    rsState.frontFace = vk::FrontFace::eClockwise;
    rsState.depthBiasEnable = VK_FALSE;
    rsState.depthBiasConstantFactor = 0.0f;
    rsState.depthBiasClamp = 0.0f;
    rsState.depthBiasSlopeFactor = 0.0f;
    rsState.lineWidth = 1.0f;

    vk::PipelineColorBlendAttachmentState cbaState;
    cbaState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    cbaState.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo cbState;
    cbState.attachmentCount = 1;
    cbState.pAttachments = &cbaState;

    vk::PipelineDepthStencilStateCreateInfo dsState;
    dsState.depthTestEnable = VK_TRUE;
    dsState.depthWriteEnable = VK_TRUE;
    dsState.depthCompareOp = vk::CompareOp::eLessOrEqual;
    dsState.back.compareOp = vk::CompareOp::eAlways;

    vk::Viewport vp;
    vp.x = 0;
    vp.y = windowHeight;
    vp.width = static_cast<float>(windowWidth);
    vp.height = -static_cast<float>(windowHeight);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vk::Rect2D sc;
    sc.offset = vk::Offset2D{0, 0};
    sc.extent = swapchain->swapExtent;

    vk::PipelineViewportStateCreateInfo vpState;
    vpState.viewportCount = 1;
    vpState.pViewports = &vp;
    vpState.scissorCount = 1;
    vpState.pScissors = &sc;

    vk::PipelineMultisampleStateCreateInfo msState;
    msState.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // Load shaders
    auto vertexShader = readFile("shaders/vertex.spv");
    auto fragmentShader = readFile("shaders/fragment.spv");
    auto fragmentShaderNoSampler = readFile("shaders/fragment_no_sampler.spv");

    // Ensure we have files
    if (vertexShader.empty() || fragmentShader.empty() || fragmentShaderNoSampler.empty())
    {
        throw std::runtime_error("Failed to load fragment or vertex shader spv files");
    }

    vk::ShaderModule vertexShaderModule = createShaderModule(vertexShader);
    vk::ShaderModule fragmentShaderModule = createShaderModule(fragmentShader);
    vk::ShaderModule fragmentShaderNoSamplerModule = createShaderModule(fragmentShaderNoSampler);

    // describe vertex shader stage
    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo;
    vertexShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertexShaderStageInfo.module = vertexShaderModule;
    vertexShaderStageInfo.pName = "main";
    vertexShaderStageInfo.pSpecializationInfo = nullptr;

    // describe fragment shader stage WITH sampler
    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo;
    fragmentShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragmentShaderStageInfo.module = fragmentShaderModule;
    fragmentShaderStageInfo.pName = "main";
    fragmentShaderStageInfo.pSpecializationInfo = nullptr;

    // fragment shader stage NO sampler
    vk::PipelineShaderStageCreateInfo fragmentShaderStageNoSamplerInfo;
    fragmentShaderStageNoSamplerInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragmentShaderStageNoSamplerInfo.module = fragmentShaderNoSamplerModule;
    fragmentShaderStageNoSamplerInfo.pName = "main";
    fragmentShaderStageNoSamplerInfo.pSpecializationInfo = nullptr;

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
        vertexShaderStageInfo,
        fragmentShaderStageInfo,
    };
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStagesNoSampler = {
        vertexShaderStageInfo,
        fragmentShaderStageNoSamplerInfo,
    };

    /*
      Dynamic state extension
    */
    std::array<vk::DynamicState, 1> dynStates = {
        vk::DynamicState::ePrimitiveTopologyEXT,
    };
    vk::PipelineDynamicStateCreateInfo dynamicInfo;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
    dynamicInfo.pDynamicStates = dynStates.data();

    // create pipeline WITH sampler
    vk::GraphicsPipelineCreateInfo pipelineWSamplerInfo;
    pipelineWSamplerInfo.renderPass = renderPasses->at("core");
    pipelineWSamplerInfo.layout = pipelineLayouts->at("sampler");
    pipelineWSamplerInfo.pInputAssemblyState = &iaState;
    pipelineWSamplerInfo.pRasterizationState = &rsState;
    pipelineWSamplerInfo.pColorBlendState = &cbState;
    pipelineWSamplerInfo.pDepthStencilState = &dsState;
    pipelineWSamplerInfo.pViewportState = &vpState;
    pipelineWSamplerInfo.pMultisampleState = &msState;
    pipelineWSamplerInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineWSamplerInfo.pStages = shaderStages.data();
    pipelineWSamplerInfo.pVertexInputState = &viState;
    pipelineWSamplerInfo.pDynamicState = nullptr;

    /* Graphics pipeline with sampler -- NO dynamic states */
    vk::ResultValue result = mvDevice->logicalDevice->createGraphicsPipeline(nullptr, pipelineWSamplerInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline with sampler");

    pipelines->insert({
        "sampler",
        result.value,
    });

    pipelineWSamplerInfo.pInputAssemblyState = &debugIaState;
    pipelineWSamplerInfo.pDynamicState = &dynamicInfo;

    /* Graphics pipeline with sampler -- WITH dynamic states */
    result = mvDevice->logicalDevice->createGraphicsPipeline(nullptr, pipelineWSamplerInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline with sampler & dynamic states");

    pipelines->insert({
        "sampler_dynamic",
        result.value,
    });

    // Create pipeline with NO sampler
    vk::GraphicsPipelineCreateInfo pipelineNoSamplerInfo;
    pipelineNoSamplerInfo.renderPass = renderPasses->at("core");
    pipelineNoSamplerInfo.layout = pipelineLayouts->at("no_sampler");
    pipelineNoSamplerInfo.pInputAssemblyState = &iaState;
    pipelineNoSamplerInfo.pRasterizationState = &rsState;
    pipelineNoSamplerInfo.pColorBlendState = &cbState;
    pipelineNoSamplerInfo.pDepthStencilState = &dsState;
    pipelineNoSamplerInfo.pViewportState = &vpState;
    pipelineNoSamplerInfo.pMultisampleState = &msState;
    pipelineNoSamplerInfo.stageCount = static_cast<uint32_t>(shaderStagesNoSampler.size());
    pipelineNoSamplerInfo.pStages = shaderStagesNoSampler.data();
    pipelineNoSamplerInfo.pVertexInputState = &viState;
    pipelineNoSamplerInfo.pDynamicState = nullptr;

    // Create graphics pipeline NO sampler
    result = mvDevice->logicalDevice->createGraphicsPipeline(nullptr, pipelineNoSamplerInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline with no sampler");

    pipelines->insert({
        "no_sampler",
        result.value,
    });

    // create pipeline no sampler | dynamic state
    pipelineNoSamplerInfo.pInputAssemblyState = &debugIaState;
    pipelineNoSamplerInfo.pDynamicState = &dynamicInfo;

    result = mvDevice->logicalDevice->createGraphicsPipeline(nullptr, pipelineNoSamplerInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline with no sampler & dynamic states");

    pipelines->insert({
        "no_sampler_dynamic",
        result.value,

    });

    mvDevice->logicalDevice->destroyShaderModule(vertexShaderModule);
    mvDevice->logicalDevice->destroyShaderModule(fragmentShaderModule);
    mvDevice->logicalDevice->destroyShaderModule(fragmentShaderNoSamplerModule);
    return;
}

void mv::Engine::recordCommandBuffer(uint32_t p_ImageIndex)
{
    // command buffer begin
    vk::CommandBufferBeginInfo beginInfo;

    // render pass info
    std::array<vk::ClearValue, 2> cls;
    cls[0].color = defaultClearColor;
    cls[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    vk::RenderPassBeginInfo passInfo;
    passInfo.renderPass = renderPasses->at("core");
    passInfo.framebuffer = coreFramebuffers->at(p_ImageIndex);
    passInfo.renderArea.offset.x = 0;
    passInfo.renderArea.offset.y = 0;
    passInfo.renderArea.extent.width = swapchain->swapExtent.width;
    passInfo.renderArea.extent.height = swapchain->swapExtent.height;
    passInfo.clearValueCount = static_cast<uint32_t>(cls.size());
    passInfo.pClearValues = cls.data();

    // begin recording
    commandBuffers->at(p_ImageIndex).begin(beginInfo);

    // start render pass
    commandBuffers->at(p_ImageIndex).beginRenderPass(passInfo, vk::SubpassContents::eInline);

    /*
      DEBUG RENDER METHODS
    */
    for (const auto &job : toRenderMap)
    {

        // player's aiming ray cast
        if (job.first == "reticle_raycast" && job.second)
        {

            commandBuffers->at(p_ImageIndex)
                .bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines->at("no_sampler_dynamic"));

            pfn_vkCmdSetPrimitiveTopology(commandBuffers->at(p_ImageIndex),
                                          static_cast<VkPrimitiveTopology>(vk::PrimitiveTopology::eLineList));

            // render line
            std::vector<vk::DescriptorSet> toBind = {reticleObj.meshDescriptor,
                                                     collectionHandler->viewUniform->descriptor,
                                                     collectionHandler->projectionUniform->descriptor};
            commandBuffers->at(p_ImageIndex)
                .bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts->at("no_sampler"), 0, toBind,
                                    nullptr);

            commandBuffers->at(p_ImageIndex).bindVertexBuffers(0, reticleMesh.vertexBuffer, {0});
            commandBuffers->at(p_ImageIndex).draw(2, 1, 0, 0);
        }
    }

    /*
      END DEBUG RENDER METHODS
    */

    for (auto &model : *collectionHandler->models)
    {
        // for each model select the appropriate pipeline
        if (model.hasTexture)
        {
            commandBuffers->at(p_ImageIndex).bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines->at("sampler"));
        }
        else
        {
            commandBuffers->at(p_ImageIndex)
                .bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines->at("no_sampler"));
        }
        for (auto &object : *model.objects)
        {
            for (auto &mesh : *model.loadedMeshes)
            {
                std::vector<vk::DescriptorSet> toBind = {object.meshDescriptor,
                                                         collectionHandler->viewUniform->descriptor,
                                                         collectionHandler->projectionUniform->descriptor};
                if (model.hasTexture)
                {
                    // TODO
                    // allow for multiple texture descriptors
                    toBind.push_back(mesh.textures.at(0).descriptor);
                    commandBuffers->at(p_ImageIndex)
                        .bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts->at("sampler"), 0, toBind,
                                            nullptr);
                }
                else
                {
                    commandBuffers->at(p_ImageIndex)
                        .bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts->at("no_sampler"), 0,
                                            toBind, nullptr);
                }
                // Bind mesh buffers
                mesh.bindBuffers(commandBuffers->at(p_ImageIndex));

                // Indexed draw
                commandBuffers->at(p_ImageIndex).drawIndexed(static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
            }
        }
    }

    commandBuffers->at(p_ImageIndex).endRenderPass();

    /*
     IMGUI RENDER
    */
    gui->doRenderPass(renderPasses->at("gui"), guiFramebuffers->at(p_ImageIndex), commandBuffers->at(p_ImageIndex),
                      swapchain->swapExtent);
    /*
      END IMGUI RENDER
    */

    commandBuffers->at(p_ImageIndex).end();

    return;
}

void mv::Engine::draw(size_t &p_CurrentFrame, uint32_t &p_CurrentImageIndex)
{
    vk::Result res = mvDevice->logicalDevice->waitForFences(inFlightFences->at(p_CurrentFrame), VK_TRUE, UINT64_MAX);
    if (res != vk::Result::eSuccess)
        throw std::runtime_error("Error occurred while waiting for fence");

    vk::Result result = mvDevice->logicalDevice->acquireNextImageKHR(
        *swapchain->swapchain, UINT64_MAX, semaphores->presentComplete, nullptr, &p_CurrentImageIndex);

    switch (result)
    {
        case vk::Result::eErrorOutOfDateKHR:
            recreateSwapchain();
            return;
            break;
        case vk::Result::eSuboptimalKHR:
            std::cout << "[/] Swapchain is no longer optimal : not recreating" << std::endl;
            break;
        case vk::Result::eSuccess:
            break;
        default: // unhandled error occurred
            throw std::runtime_error("Unhandled error case while acquiring a swapchain image for "
                                     "rendering");
            break;
    }

    if (waitFences->at(p_CurrentImageIndex))
    {
        vk::Result res =
            mvDevice->logicalDevice->waitForFences(waitFences->at(p_CurrentImageIndex), VK_TRUE, UINT64_MAX);
        if (res != vk::Result::eSuccess)
            throw std::runtime_error("Error occurred while waiting for fence");
    }

    recordCommandBuffer(p_CurrentImageIndex);

    // mark in use
    waitFences->at(p_CurrentImageIndex) = inFlightFences->at(p_CurrentFrame);

    vk::SubmitInfo submitInfo;
    vk::Semaphore waitSemaphores[] = {semaphores->presentComplete};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers->at(p_CurrentImageIndex);
    vk::Semaphore renderSemaphores[] = {semaphores->renderComplete};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = renderSemaphores;

    mvDevice->logicalDevice->resetFences(inFlightFences->at(p_CurrentFrame));

    result = mvDevice->graphicsQueue->submit(1, &submitInfo, inFlightFences->at(p_CurrentFrame));

    switch (result)
    {
        case vk::Result::eErrorOutOfDateKHR:
            recreateSwapchain();
            return;
            break;
        case vk::Result::eSuboptimalKHR:
            std::cout << "[/] Swapchain is no longer optimal : not recreating" << std::endl;
            break;
        case vk::Result::eSuccess:
            break;
        default: // unhandled error occurred
            throw std::runtime_error("Unhandled error case while submitting queue");
            break;
    }

    vk::PresentInfoKHR presentInfo;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = renderSemaphores;
    vk::SwapchainKHR swapchains[] = {*swapchain->swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &p_CurrentImageIndex;
    presentInfo.pResults = nullptr;

    result = mvDevice->graphicsQueue->presentKHR(&presentInfo);

    switch (result)
    {
        case vk::Result::eErrorOutOfDateKHR:
            recreateSwapchain();
            return;
            break;
        case vk::Result::eSuboptimalKHR:
            std::cout << "[/] Swapchain is no longer optimal : not recreating" << std::endl;
            break;
        case vk::Result::eSuccess:
            break;
        default: // unhandled error occurred
            throw std::runtime_error("Unhandled error case while presenting");
            break;
    }
    p_CurrentFrame = (p_CurrentFrame + 1) % MAX_IN_FLIGHT;
}

/*
  initialize descriptor allocator, collection handler & camera
*/
inline void mv::Engine::goSetup(void)
{
    /*
      INITIALIZE DESCRIPTOR HANDLER
    */
    descriptorAllocator = std::make_unique<mv::Allocator>();
    descriptorAllocator->allocatePool(*mvDevice, 2000);

    /*
      MAT4 UNIFORM LAYOUT
    */
    descriptorAllocator->createLayout(*mvDevice, "uniform_layout", vk::DescriptorType::eUniformBuffer, 1,
                                      vk::ShaderStageFlagBits::eVertex, 0);

    /*
      TEXTURE SAMPLER LAYOUT
    */
    descriptorAllocator->createLayout(*mvDevice, "sampler_layout", vk::DescriptorType::eCombinedImageSampler, 1,
                                      vk::ShaderStageFlagBits::eFragment, 0);

    /*
      INITIALIZE MODEL/OBJECT HANDLER
      CREATES BUFFERS FOR VIEW & PROJECTION MATRICES
    */
    collectionHandler = std::make_unique<Collection>(*mvDevice);

    vk::DescriptorSetLayout uniformLayout = descriptorAllocator->getLayout("uniform_layout");
    /*
      VIEW MATRIX UNIFORM OBJECT
    */
    descriptorAllocator->allocateSet(*mvDevice, uniformLayout, collectionHandler->viewUniform->descriptor);
    descriptorAllocator->updateSet(*mvDevice, collectionHandler->viewUniform->mvBuffer.descriptor,
                                   collectionHandler->viewUniform->descriptor, 0);

    /*
      PROJECTION MATRIX UNIFORM OBJECT
    */
    descriptorAllocator->allocateSet(*mvDevice, uniformLayout, collectionHandler->projectionUniform->descriptor);
    descriptorAllocator->updateSet(*mvDevice, collectionHandler->projectionUniform->mvBuffer.descriptor,
                                   collectionHandler->projectionUniform->descriptor, 0);

    /*
      LOAD MODELS
    */
    collectionHandler->loadModel(*mvDevice, *descriptorAllocator, "models/_viking_room.fbx");
    collectionHandler->loadModel(*mvDevice, *descriptorAllocator, "models/Security2.fbx");

    /*
      CREATE OBJECTS
    */
    collectionHandler->createObject(*mvDevice, *descriptorAllocator, "models/_viking_room.fbx");
    collectionHandler->models->at(0).objects->at(0).position = glm::vec3(0.0f, 0.0f, -5.0f);
    collectionHandler->models->at(0).objects->at(0).rotation = glm::vec3(0.0f, 90.0f, 0.0f);

    collectionHandler->createObject(*mvDevice, *descriptorAllocator, "models/Security2.fbx");
    collectionHandler->models->at(1).objects->at(0).rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    collectionHandler->models->at(1).objects->at(0).position = glm::vec3(0.0f, 0.0f, 0.0f);
    collectionHandler->models->at(1).objects->at(0).scaleFactor = 0.0125f;

    /*
      CREATE DEBUG POINTS
    */
    Vertex p1;
    p1.position = {0.0f, 0.0f, 0.0f, 1.0f};
    p1.color = {1.0f, 1.0f, 1.0f, 1.0f};
    p1.uv = {0.0f, 0.0f, 0.0f, 0.0f};
    reticleMesh.vertices = {p1, p1};

    // Create vertex buffer
    mvDevice->createBuffer(vk::BufferUsageFlagBits::eVertexBuffer,
                           vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                           reticleMesh.vertices.size() * sizeof(mv::Vertex), &reticleMesh.vertexBuffer,
                           &reticleMesh.vertexMemory, reticleMesh.vertices.data());

    // Create uniform -- not used tbh
    reticleObj.position = {0.0f, 0.0f, 0.0f};
    mvDevice->createBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                           &reticleObj.uniformBuffer, sizeof(mv::Object::Matrix));
    reticleObj.uniformBuffer.map(*mvDevice);
    reticleObj.update();
    // glm::mat4 nm = glm::mat4(1.0);
    // memcpy(reticle_obj.uniform_buffer.mapped, &nm, sizeof(glm::mat4));

    // allocate descriptor for reticle
    descriptorAllocator->allocateSet(*mvDevice, uniformLayout, reticleObj.meshDescriptor);
    descriptorAllocator->updateSet(*mvDevice, reticleObj.uniformBuffer.descriptor, reticleObj.meshDescriptor, 0);

    /*
      CONFIGURE AND CREATE CAMERA
    */
    struct CameraInitStruct cameraParams;
    cameraParams.fov = 60.0f;
    cameraParams.aspect = (float)swapchain->swapExtent.width / (float)swapchain->swapExtent.height;
    cameraParams.nearz = 0.1f;
    cameraParams.farz = 200.0f;
    cameraParams.position = glm::vec3(0.0f, 3.0f, -7.0f);
    cameraParams.viewUniformObject = collectionHandler->viewUniform.get();
    cameraParams.projectionUniformObject = collectionHandler->projectionUniform.get();

    cameraParams.type = CameraType::eThirdPerson;
    cameraParams.target = &collectionHandler->models->at(1).objects->at(0);

    camera = std::make_unique<Camera>(cameraParams);

    /*
      Hard coding settings
    */
    camera->zoomLevel = 7.0f;
    mouse.deltaStyle = Mouse::DeltaStyles::eFromLastPosition;
    mouse.isDragging = false;
    return;
}

/*
  GLFW CALLBACKS
*/

void mv::mouseMotionCallback(GLFWwindow *p_GLFWwindow, double p_XPosition, double p_YPosition)
{
    auto engine = reinterpret_cast<mv::Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));
    engine->mouse.update(p_XPosition, p_YPosition);
    return;
}

void mv::mouseButtonCallback(GLFWwindow *p_GLFWwindow, int p_Button, int p_Action, [[maybe_unused]] int p_Modifiers)
{
    auto engine = reinterpret_cast<mv::Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));

    switch (p_Button)
    {
        case GLFW_MOUSE_BUTTON_1: // LMB
            {
                if (p_Action == GLFW_PRESS)
                {
                    engine->mouse.onLeftPress();
                }
                else if (p_Action == GLFW_RELEASE)
                {
                    engine->mouse.onLeftRelease();
                }
                break;
            }
        case GLFW_MOUSE_BUTTON_2: // RMB
            {
                if (p_Action == GLFW_PRESS)
                {
                    engine->mouse.onRightPress();
                }
                else if (p_Action == GLFW_RELEASE)
                {
                    engine->mouse.onRightRelease();
                }
                break;
            }
        case GLFW_MOUSE_BUTTON_3: // MMB
            {
                if (p_Action == GLFW_PRESS)
                {
                    engine->mouse.onMiddlePress();
                }
                else if (p_Action == GLFW_RELEASE)
                {
                    engine->mouse.onMiddleRelease();
                }
                break;
            }
    }
    return;
}

void mv::keyCallback(GLFWwindow *p_GLFWwindow, int p_Key, [[maybe_unused]] int p_ScanCode, int p_Action,
                     [[maybe_unused]] int p_Modifier)
{
    // get get pointer
    auto engine = reinterpret_cast<mv::Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));

    if (p_Key == GLFW_KEY_ESCAPE && p_Action == GLFW_PRESS)
    {
        // glfwSetWindowShouldClose(p_GLFWwindow, true);
        std::cout << "Escape key exiting has been disabled...\n";
        return;
    }

    if (p_Action == GLFW_PRESS)
    {
        engine->keyboard.onKeyPress(p_Key);
    }
    else if (p_Action == GLFW_RELEASE)
    {
        engine->keyboard.onKeyRelease(p_Key);
    }
    return;
}

void mv::glfwErrCallback(int p_Error, const char *p_Description)
{
    std::cout << "GLFW Error...\n"
              << "Code => " << p_Error << "\n"
              << "Message => " << p_Description << "\n";
    return;
}