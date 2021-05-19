#include "mvEngine.h"
#include "mvAllocator.h"
#include "mvCollection.h"
#include "mvModel.h"

extern LogHandler logger;

Engine::Engine(int w, int h, const char *title) : Window(w, h, title)
{
    return;
}

Engine::~Engine()
{
    logicalDevice.waitIdle();

    if (!pipelines.empty())
    {
        for (auto &pipeline : pipelines)
        {
            if (pipeline.second)
                logicalDevice.destroyPipeline(pipeline.second);
        }
    }

    if (!pipelineLayouts.empty())
    {
        for (auto &layout : pipelineLayouts)
        {
            if (layout.second)
                logicalDevice.destroyPipelineLayout(layout.second);
        }
    }

    // collection struct will handle cleanup of models & objs
    collectionHandler->cleanup();
    allocator->cleanup();
}

void Engine::cleanupSwapchain(void)
{
    // destroy command buffers
    if (!commandBuffers.empty())
    {
        logicalDevice.freeCommandBuffers(commandPool, commandBuffers);
    }

    // destroy framebuffers
    if (!coreFramebuffers.empty())
    {
        for (auto &buffer : coreFramebuffers)
        {
            if (buffer)
                logicalDevice.destroyFramebuffer(buffer, nullptr);
        }
    }

    if (depthStencil.image)
    {
        logicalDevice.destroyImage(depthStencil.image, nullptr);
    }
    if (depthStencil.view)
    {
        logicalDevice.destroyImageView(depthStencil.view, nullptr);
    }
    if (depthStencil.mem)
    {
        logicalDevice.freeMemory(depthStencil.mem, nullptr);
    }

    // destroy pipelines
    if (!pipelines.empty())
    {
        for (auto &pipeline : pipelines)
        {
            if (pipeline.second)
                logicalDevice.destroyPipeline(pipeline.second);
        }
    }

    // destroy pipeline layouts
    if (!pipelineLayouts.empty())
    {
        for (auto &layout : pipelineLayouts)
        {
            if (layout.second)
                logicalDevice.destroyPipelineLayout(layout.second);
        }
    }

    // destroy render pass
    if (!renderPasses.empty())
    {
        for (auto &pass : renderPasses)
        {
            if (pass.second)
                logicalDevice.destroyRenderPass(pass.second, nullptr);
        }
    }

    // cleanup command pool
    if (commandPool)
    {
        logicalDevice.destroyCommandPool(commandPool);
    }

    // cleanup swapchain
    swapchain.cleanup(instance, logicalDevice);

    return;
}

// Allows swapchain to keep up with window resize
void Engine::recreateSwapchain(void)
{
    logger.logMessage("Recreating swap chain");

    logicalDevice.waitIdle();

    // Cleanup
    cleanupSwapchain();

    commandPool = createCommandPool(queueIdx.graphics);

    // create swapchain
    swapchain.create(physicalDevice, logicalDevice, windowWidth, windowHeight);

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

void Engine::go(void)
{
    logger.logMessage("Preparing Vulkan");
    prepare();

    // Initialize here before use in later methods
    allocator = std::make_unique<Allocator>(this);
    collectionHandler = std::make_unique<Collection>(this);

    // setup descriptor allocator, collection handler & camera
    // load models & create objects here
    goSetup();

    preparePipeline();

    uint32_t imageIndex = 0;
    size_t currentFrame = 0;

    bool added = false;

    fps.startTimer();

    using chrono = std::chrono::high_resolution_clock;

    constexpr std::chrono::nanoseconds timestep(16ms);

    std::chrono::nanoseconds accumulated(0ns);
    auto startTime = chrono::now();

    /*
        Create and initialize ImGui handler
        Will create render pass & perform pre game loop ImGui initialization
    */
    gui = std::make_unique<GuiHandler>(
        window, &camera, instance, physicalDevice, logicalDevice, swapchain,
        commandPool, graphicsQueue, renderPasses, allocator->get()->pool);

    guiFramebuffers = gui->createFramebuffers(
        logicalDevice, renderPasses.at("gui"), swapchain.buffers,
        swapchain.swapExtent.width, swapchain.swapExtent.height);

    auto renderStart = chrono::now();
    auto renderStop = chrono::now();

    logger.logMessage("Entering game loop\n");

    while (!glfwWindowShouldClose(window))
    {
        auto deltaTime = chrono::now() - startTime;
        startTime = chrono::now();
        accumulated +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(deltaTime);

        glfwPollEvents();

        while (accumulated >= timestep)
        {
            accumulated -= timestep;

            // Get input events
            Keyboard::Event kbdEvent = keyboard.read();
            Mouse::Event mouseEvent = mouse.read();

            if (camera.type == CameraType::eFreeLook && !gui->hasFocus)
            {
                /*
                  Start mouse drag
                */
                if (mouseEvent.type == Mouse::Event::Type::eRightDown)
                {

                    if (!mouse.isDragging)
                    {
                        // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                        // GLFW_TRUE); glfwSetInputMode(window, GLFW_CURSOR,
                        // GLFW_CURSOR_DISABLED);
                        mouse.startDrag(camera.orbitAngle, camera.pitch);
                    }
                }
                if (mouseEvent.type == Mouse::Event::Type::eRightRelease)
                {
                    if (mouse.isDragging)
                    {
                        // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                        // GLFW_FALSE); glfwSetInputMode(window, GLFW_CURSOR,
                        // GLFW_CURSOR_NORMAL);
                        mouse.endDrag();
                    }
                }

                // if drag enabled check for release
                if (mouse.isDragging)
                {
                    // camera orbit
                    // if (mouse.dragDeltaX > 0)
                    // {
                    //     camera.lerpOrbit(abs(mouse.dragDeltaX));
                    // }
                    // else if (mouse.dragDeltaX < 0)
                    // {
                    //     camera.lerpOrbit(-abs(mouse.dragDeltaX));
                    // }

                    // // camera pitch
                    // if (mouse.dragDeltaY > 0)
                    // {
                    //     camera.lerpPitch(-abs(mouse.dragDeltaY));
                    // }
                    // else if (mouse.dragDeltaY < 0)
                    // {
                    //     camera.lerpPitch(abs(mouse.dragDeltaY));
                    // }

                    camera.rotate({mouse.dragDeltaY, -mouse.dragDeltaX, 0.0f},
                                  1.0f);

                    mouse.storedOrbit = camera.orbitAngle;
                    mouse.storedPitch = camera.pitch;
                    mouse.clear();
                }

                // WASD
                if (keyboard.isKeyState(GLFW_KEY_W))
                    camera.move(camera.rotation.y, {0.0f, 0.0f, -1.0f},
                                (keyboard.isKeyState(GLFW_KEY_LEFT_SHIFT)));
                if (keyboard.isKeyState(GLFW_KEY_A))
                    camera.move(camera.rotation.y, {-1.0f, 0.0f, 0.0f},
                                (keyboard.isKeyState(GLFW_KEY_LEFT_SHIFT)));
                if (keyboard.isKeyState(GLFW_KEY_S))
                    camera.move(camera.rotation.y, {0.0f, 0.0f, 1.0f},
                                (keyboard.isKeyState(GLFW_KEY_LEFT_SHIFT)));
                if (keyboard.isKeyState(GLFW_KEY_D))
                    camera.move(camera.rotation.y, {1.0f, 0.0f, 0.0f},
                                (keyboard.isKeyState(GLFW_KEY_LEFT_SHIFT)));

                // space + alt
                if (keyboard.isKeyState(GLFW_KEY_SPACE))
                    camera.moveUp();
                if (keyboard.isKeyState(GLFW_KEY_LEFT_ALT))
                    camera.moveDown();
            }

            /*
                THIRD PERSON CAMERA
            */
            if (camera.type == CameraType::eThirdPerson && !gui->hasFocus)
            {

                /*
                  Mouse scroll wheel
                */
                if (mouseEvent.type == Mouse::Event::Type::eWheelUp &&
                    !gui->hover)
                {
                    camera.adjustZoom(-(camera.zoomStep));
                }
                if (mouseEvent.type == Mouse::Event::Type::eWheelDown &&
                    !gui->hover)
                {
                    camera.adjustZoom(camera.zoomStep);
                }

                /*
                  Start mouse drag
                */
                if (mouseEvent.type == Mouse::Event::Type::eRightDown)
                {

                    if (!mouse.isDragging)
                    {
                        // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                        // GLFW_TRUE); glfwSetInputMode(window, GLFW_CURSOR,
                        // GLFW_CURSOR_DISABLED);
                        mouse.startDrag(camera.orbitAngle, camera.pitch);
                    }
                }
                if (mouseEvent.type == Mouse::Event::Type::eRightRelease)
                {
                    if (mouse.isDragging)
                    {
                        // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                        // GLFW_FALSE); glfwSetInputMode(window, GLFW_CURSOR,
                        // GLFW_CURSOR_NORMAL);
                        mouse.endDrag();
                    }
                }

                // if drag enabled check for release
                if (mouse.isDragging)
                {
                    // camera orbit
                    if (mouse.dragDeltaX > 0)
                    {
                        camera.lerpOrbit(-abs(mouse.dragDeltaX));
                    }
                    else if (mouse.dragDeltaX < 0)
                    {
                        // camera.adjustOrbit(abs(mouse.dragDeltaX));
                        camera.lerpOrbit(abs(mouse.dragDeltaX));
                    }

                    // camera pitch
                    if (mouse.dragDeltaY > 0)
                    {
                        camera.lerpPitch(abs(mouse.dragDeltaY));
                    }
                    else if (mouse.dragDeltaY < 0)
                    {
                        camera.lerpPitch(-abs(mouse.dragDeltaY));
                    }

                    camera.target->rotateToAngle(camera.orbitAngle);

                    mouse.storedOrbit = camera.orbitAngle;
                    mouse.storedPitch = camera.pitch;
                    mouse.clear();
                }

                // sort movement by key combination

                // forward only
                if (keyboard.isKeyState(GLFW_KEY_W) &&
                    !keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) &&
                    !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle,
                                        glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + left + right -- go straight
                if (keyboard.isKeyState(GLFW_KEY_W) &&
                    !keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) &&
                    keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle,
                                        glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + right
                if (keyboard.isKeyState(GLFW_KEY_W) &&
                    !keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) &&
                    keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle,
                                        glm::vec4(1.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + left
                if (keyboard.isKeyState(GLFW_KEY_W) &&
                    !keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) &&
                    !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle,
                                        glm::vec4(-1.0f, 0.0f, -1.0f, 1.0f));
                }

                // backward only
                if (!keyboard.isKeyState(GLFW_KEY_W) &&
                    keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) &&
                    !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle,
                                        glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + left
                if (!keyboard.isKeyState(GLFW_KEY_W) &&
                    keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) &&
                    !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle,
                                        glm::vec4(-1.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + right
                if (!keyboard.isKeyState(GLFW_KEY_W) &&
                    keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) &&
                    keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle,
                                        glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + left + right -- go back
                if (!keyboard.isKeyState(GLFW_KEY_W) &&
                    keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) &&
                    keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle,
                                        glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }

                // left only
                if (!keyboard.isKeyState(GLFW_KEY_W) &&
                    !keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) &&
                    !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle,
                                        glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f));
                }

                // right only
                if (!keyboard.isKeyState(GLFW_KEY_W) &&
                    !keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) &&
                    keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle,
                                        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
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
                    allocator->createObject(collectionHandler->models.get(),
                                            "models/_viking_room.fbx");
                    collectionHandler->models->at(0).objects->back().position =
                        glm::vec3(x, y, z);
                }
            } // end third person methods
            // update game objects
            collectionHandler->update();

            // update view and projection matrices
            camera.update();

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
            // raycastP2(*this,
            // {collectionHandler.models->at(1).objects->at(0).position.x,
            //                   collectionHandler.models->at(1).objects->at(0).position.y
            //                   - 1.5f,
            //                   collectionHandler.models->at(1).objects->at(0).position.z});
        }

        // Game editor rendering
        {
            gui->newFrame();

            gui->update(
                window, swapchain.swapExtent,
                std::chrono::duration<float, std::ratio<1L, 1000L>>(renderStop -
                                                                    renderStart)
                    .count(),
                std::chrono::duration<float, std::chrono::milliseconds::period>(
                    deltaTime)
                    .count(),
                static_cast<uint32_t>(collectionHandler->modelNames.size()),
                collectionHandler->getObjectCount(),
                collectionHandler->getVertexCount(),
                collectionHandler->getTriangleCount());

            gui->renderFrame();
        }

        // TODO
        // Add alpha to render
        // Render
        renderStart = chrono::now();
        draw(currentFrame, imageIndex);
        renderStop = chrono::now();
    }

    gui->cleanup(logicalDevice);

    int totalObjectCount = 0;
    for (const auto &model : *collectionHandler->models)
    {
        totalObjectCount += model.objects->size();
    }
    logger.logMessage("Exiting Vulkan..");
    logger.logMessage("Total objects => " + std::to_string(totalObjectCount));
    return;
}

/*
  Creation of all the graphics pipelines
*/
void Engine::preparePipeline(void)
{
    vk::DescriptorSetLayout uniformLayout =
        allocator->getLayout("uniform_layout");
    vk::DescriptorSetLayout samplerLayout =
        allocator->getLayout("sampler_layout");

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
    pLineWithSamplerInfo.setLayoutCount =
        static_cast<uint32_t>(layoutWSampler.size());
    pLineWithSamplerInfo.pSetLayouts = layoutWSampler.data();

    // Pipeline with no textures
    vk::PipelineLayoutCreateInfo pLineNoSamplerInfo;
    pLineNoSamplerInfo.setLayoutCount =
        static_cast<uint32_t>(layoutNoSampler.size());
    pLineNoSamplerInfo.pSetLayouts = layoutNoSampler.data();

    pipelineLayouts.insert({
        "sampler",
        logicalDevice.createPipelineLayout(pLineWithSamplerInfo),
    });

    pipelineLayouts.insert({
        "no_sampler",
        logicalDevice.createPipelineLayout(pLineNoSamplerInfo),
    });

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo viState;
    viState.vertexBindingDescriptionCount = 1;
    viState.pVertexBindingDescriptions = &bindingDescription;
    viState.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    viState.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo iaState;
    iaState.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineInputAssemblyStateCreateInfo
        debugIaState; // used for dynamic states
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
    cbaState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
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
    sc.extent = swapchain.swapExtent;

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
    if (vertexShader.empty() || fragmentShader.empty() ||
        fragmentShaderNoSampler.empty())
    {
        throw std::runtime_error(
            "Failed to load fragment or vertex shader spv files");
    }

    vk::ShaderModule vertexShaderModule = createShaderModule(vertexShader);
    vk::ShaderModule fragmentShaderModule = createShaderModule(fragmentShader);
    vk::ShaderModule fragmentShaderNoSamplerModule =
        createShaderModule(fragmentShaderNoSampler);

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
    pipelineWSamplerInfo.renderPass = renderPasses.at("core");
    pipelineWSamplerInfo.layout = pipelineLayouts.at("sampler");
    pipelineWSamplerInfo.pInputAssemblyState = &iaState;
    pipelineWSamplerInfo.pRasterizationState = &rsState;
    pipelineWSamplerInfo.pColorBlendState = &cbState;
    pipelineWSamplerInfo.pDepthStencilState = &dsState;
    pipelineWSamplerInfo.pViewportState = &vpState;
    pipelineWSamplerInfo.pMultisampleState = &msState;
    pipelineWSamplerInfo.stageCount =
        static_cast<uint32_t>(shaderStages.size());
    pipelineWSamplerInfo.pStages = shaderStages.data();
    pipelineWSamplerInfo.pVertexInputState = &viState;
    pipelineWSamplerInfo.pDynamicState = nullptr;

    /* Graphics pipeline with sampler -- NO dynamic states */
    vk::ResultValue result =
        logicalDevice.createGraphicsPipeline(nullptr, pipelineWSamplerInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error(
            "Failed to create graphics pipeline with sampler");

    pipelines.insert({
        "sampler",
        result.value,
    });

    pipelineWSamplerInfo.pInputAssemblyState = &debugIaState;
    pipelineWSamplerInfo.pDynamicState = &dynamicInfo;

    /* Graphics pipeline with sampler -- WITH dynamic states */
    result =
        logicalDevice.createGraphicsPipeline(nullptr, pipelineWSamplerInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error(
            "Failed to create graphics pipeline with sampler & dynamic states");

    pipelines.insert({
        "sampler_dynamic",
        result.value,
    });

    // Create pipeline with NO sampler
    vk::GraphicsPipelineCreateInfo pipelineNoSamplerInfo;
    pipelineNoSamplerInfo.renderPass = renderPasses.at("core");
    pipelineNoSamplerInfo.layout = pipelineLayouts.at("no_sampler");
    pipelineNoSamplerInfo.pInputAssemblyState = &iaState;
    pipelineNoSamplerInfo.pRasterizationState = &rsState;
    pipelineNoSamplerInfo.pColorBlendState = &cbState;
    pipelineNoSamplerInfo.pDepthStencilState = &dsState;
    pipelineNoSamplerInfo.pViewportState = &vpState;
    pipelineNoSamplerInfo.pMultisampleState = &msState;
    pipelineNoSamplerInfo.stageCount =
        static_cast<uint32_t>(shaderStagesNoSampler.size());
    pipelineNoSamplerInfo.pStages = shaderStagesNoSampler.data();
    pipelineNoSamplerInfo.pVertexInputState = &viState;
    pipelineNoSamplerInfo.pDynamicState = nullptr;

    // Create graphics pipeline NO sampler
    result =
        logicalDevice.createGraphicsPipeline(nullptr, pipelineNoSamplerInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error(
            "Failed to create graphics pipeline with no sampler");

    pipelines.insert({
        "no_sampler",
        result.value,
    });

    // create pipeline no sampler | dynamic state
    pipelineNoSamplerInfo.pInputAssemblyState = &debugIaState;
    pipelineNoSamplerInfo.pDynamicState = &dynamicInfo;

    result =
        logicalDevice.createGraphicsPipeline(nullptr, pipelineNoSamplerInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline with no "
                                 "sampler & dynamic states");

    pipelines.insert({
        "no_sampler_dynamic",
        result.value,

    });

    logicalDevice.destroyShaderModule(vertexShaderModule);
    logicalDevice.destroyShaderModule(fragmentShaderModule);
    logicalDevice.destroyShaderModule(fragmentShaderNoSamplerModule);
    return;
}

void Engine::recordCommandBuffer(uint32_t p_ImageIndex)
{
    // command buffer begin
    vk::CommandBufferBeginInfo beginInfo;

    // render pass info
    std::array<vk::ClearValue, 2> cls;
    cls[0].color = defaultClearColor;
    cls[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    vk::RenderPassBeginInfo passInfo;
    passInfo.renderPass = renderPasses.at("core");
    passInfo.framebuffer = coreFramebuffers.at(p_ImageIndex);
    passInfo.renderArea.offset.x = 0;
    passInfo.renderArea.offset.y = 0;
    passInfo.renderArea.extent.width = swapchain.swapExtent.width;
    passInfo.renderArea.extent.height = swapchain.swapExtent.height;
    passInfo.clearValueCount = static_cast<uint32_t>(cls.size());
    passInfo.pClearValues = cls.data();

    // begin recording
    commandBuffers.at(p_ImageIndex).begin(beginInfo);

    // start render pass
    commandBuffers.at(p_ImageIndex)
        .beginRenderPass(passInfo, vk::SubpassContents::eInline);

    for (auto &model : *collectionHandler->models)
    {
        // for each model select the appropriate pipeline
        if (model.hasTexture)
        {
            commandBuffers.at(p_ImageIndex)
                .bindPipeline(vk::PipelineBindPoint::eGraphics,
                              pipelines.at("sampler"));
        }
        else
        {
            commandBuffers.at(p_ImageIndex)
                .bindPipeline(vk::PipelineBindPoint::eGraphics,
                              pipelines.at("no_sampler"));
        }

        // Bind vertex & index buffer for model
        model.bindBuffers(commandBuffers.at(p_ImageIndex));

        // For each object
        for (auto &object : *model.objects)
        {
            // Iterate index offsets ;; draw
            for (auto &offset : model.bufferOffsets)
            {
                // { vk::DescriptorSet, Buffer }
                std::vector<vk::DescriptorSet> toBind = {
                    object.uniform.first,
                    collectionHandler->viewUniform->descriptor,
                    collectionHandler->projectionUniform->descriptor};
                if (offset.first.second >= 0)
                {
                    toBind.push_back(
                        model.textureDescriptors.at(offset.first.second).first);
                    commandBuffers.at(p_ImageIndex)
                        .bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                            pipelineLayouts.at("sampler"), 0,
                                            toBind, nullptr);
                }
                else
                {
                    commandBuffers.at(p_ImageIndex)
                        .bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                            pipelineLayouts.at("no_sampler"), 0,
                                            toBind, nullptr);
                }

                // { { vertex offset, Texture index }, { Index start, Index
                // count } }
                commandBuffers.at(p_ImageIndex)
                    .drawIndexed(offset.second.second, 1, offset.second.first,
                                 offset.first.first, 0);
            }
        }
    }

    commandBuffers.at(p_ImageIndex).endRenderPass();

    /*
     IMGUI RENDER
    */
    gui->doRenderPass(renderPasses.at("gui"), guiFramebuffers.at(p_ImageIndex),
                      commandBuffers.at(p_ImageIndex), swapchain.swapExtent);
    /*
      END IMGUI RENDER
    */

    commandBuffers.at(p_ImageIndex).end();

    return;
}

void Engine::draw(size_t &p_CurrentFrame, uint32_t &p_CurrentImageIndex)
{
    vk::Result res = logicalDevice.waitForFences(
        inFlightFences.at(p_CurrentFrame), VK_TRUE, UINT64_MAX);
    if (res != vk::Result::eSuccess)
        throw std::runtime_error("Error occurred while waiting for fence");

    vk::Result result = logicalDevice.acquireNextImageKHR(
        swapchain.swapchain, UINT64_MAX, semaphores.presentComplete, nullptr,
        &p_CurrentImageIndex);

    switch (result)
    {
        case vk::Result::eErrorOutOfDateKHR:
            recreateSwapchain();
            return;
            break;
        case vk::Result::eSuboptimalKHR:
            logger.logMessage(
                LogHandler::MessagePriority::eWarning,
                "Swapchain is no longer optimal : not recreating");
            break;
        case vk::Result::eSuccess:
            break;
        default: // unhandled error occurred
            throw std::runtime_error(
                "Unhandled error case while acquiring a swapchain image for "
                "rendering");
            break;
    }

    if (waitFences.at(p_CurrentImageIndex))
    {
        vk::Result res = logicalDevice.waitForFences(
            waitFences.at(p_CurrentImageIndex), VK_TRUE, UINT64_MAX);
        if (res != vk::Result::eSuccess)
            throw std::runtime_error("Error occurred while waiting for fence");
    }

    recordCommandBuffer(p_CurrentImageIndex);

    // mark in use
    waitFences.at(p_CurrentImageIndex) = inFlightFences.at(p_CurrentFrame);

    vk::SubmitInfo submitInfo;
    vk::Semaphore waitSemaphores[] = {semaphores.presentComplete};
    vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers.at(p_CurrentImageIndex);
    vk::Semaphore renderSemaphores[] = {semaphores.renderComplete};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = renderSemaphores;

    logicalDevice.resetFences(inFlightFences.at(p_CurrentFrame));

    result =
        graphicsQueue.submit(1, &submitInfo, inFlightFences.at(p_CurrentFrame));

    switch (result)
    {
        case vk::Result::eErrorOutOfDateKHR:
            recreateSwapchain();
            return;
            break;
        case vk::Result::eSuboptimalKHR:
            logger.logMessage(
                LogHandler::MessagePriority::eWarning,
                "Swapchain is no longer optimal : not recreating");
            break;
        case vk::Result::eSuccess:
            break;
        default: // unhandled error occurred
            throw std::runtime_error(
                "Unhandled error case while submitting queue");
            break;
    }

    vk::PresentInfoKHR presentInfo;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = renderSemaphores;
    vk::SwapchainKHR swapchains[] = {swapchain.swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &p_CurrentImageIndex;
    presentInfo.pResults = nullptr;

    result = graphicsQueue.presentKHR(&presentInfo);

    switch (result)
    {
        case vk::Result::eErrorOutOfDateKHR:
            recreateSwapchain();
            return;
            break;
        case vk::Result::eSuboptimalKHR:
            logger.logMessage(
                LogHandler::MessagePriority::eWarning,
                "Swapchain is no longer optimal : not recreating");
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
inline void Engine::goSetup(void)
{
    /*
      INITIALIZE DESCRIPTOR HANDLER
    */
    allocator->allocatePool(2000);

    /*
      MAT4 UNIFORM LAYOUT
    */
    allocator->createLayout("uniform_layout",
                            vk::DescriptorType::eUniformBuffer, 1,
                            vk::ShaderStageFlagBits::eVertex, 0);

    /*
      TEXTURE SAMPLER LAYOUT
    */
    allocator->createLayout("sampler_layout",
                            vk::DescriptorType::eCombinedImageSampler, 1,
                            vk::ShaderStageFlagBits::eFragment, 0);

    /*
      INITIALIZE MODEL/OBJECT HANDLER
      CREATES BUFFERS FOR VIEW & PROJECTION MATRICES
    */

    vk::DescriptorSetLayout uniformLayout =
        allocator->getLayout("uniform_layout");
    /*
      VIEW MATRIX UNIFORM OBJECT
    */
    allocator->allocateSet(uniformLayout,
                           collectionHandler->viewUniform->descriptor);
    allocator->updateSet(collectionHandler->viewUniform->mvBuffer.bufferInfo,
                         collectionHandler->viewUniform->descriptor, 0);

    /*
      PROJECTION MATRIX UNIFORM OBJECT
    */
    allocator->allocateSet(uniformLayout,
                           collectionHandler->projectionUniform->descriptor);
    allocator->updateSet(
        collectionHandler->projectionUniform->mvBuffer.bufferInfo,
        collectionHandler->projectionUniform->descriptor, 0);

    /*
      LOAD MODELS
    */
    /*
      CREATE OBJECTS
    */

    /*
      CONFIGURE AND CREATE CAMERA
    */
    struct CameraInitStruct cameraParams = {};
    cameraParams.fov = 50.0f;
    cameraParams.aspect =
        (float)swapchain.swapExtent.width / (float)swapchain.swapExtent.height;
    cameraParams.nearz = 0.1f;
    cameraParams.farz = 1000.0f;
    cameraParams.position = glm::vec3(0.0f, -3.0f, 7.0f);
    cameraParams.viewUniformObject = collectionHandler->viewUniform.get();
    cameraParams.projectionUniformObject =
        collectionHandler->projectionUniform.get();

    cameraParams.type = CameraType::eFreeLook;
    cameraParams.target = nullptr;

    camera = Camera(cameraParams);

    /*
      Hard coding settings
    */
    camera.zoomLevel = 7.0f;
    mouse.deltaStyle = Mouse::DeltaStyles::eFromLastPosition;
    mouse.isDragging = false;
    return;
}

/*
  GLFW CALLBACKS
*/

void mouseMotionCallback(GLFWwindow *p_GLFWwindow, double p_XPosition,
                         double p_YPosition)
{
    auto engine =
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));
    engine->mouse.update(p_XPosition, p_YPosition);
    return;
}

void mouseScrollCallback(GLFWwindow *p_GLFWwindow,
                         [[maybe_unused]] double p_XOffset, double p_YOffset)
{
    auto engine =
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));

    if (p_YOffset > 0)
        engine->mouse.onWheelUp();
    if (p_YOffset < 0)
        engine->mouse.onWheelDown();

    return;
}

void mouseButtonCallback(GLFWwindow *p_GLFWwindow, int p_Button, int p_Action,
                         [[maybe_unused]] int p_Modifiers)
{
    auto engine =
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));

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

void keyCallback(GLFWwindow *p_GLFWwindow, int p_Key,
                 [[maybe_unused]] int p_ScanCode, int p_Action,
                 [[maybe_unused]] int p_Modifier)
{
    // get get pointer
    auto engine =
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));

    if (p_Key == GLFW_KEY_ESCAPE && p_Action == GLFW_PRESS)
    {
        // glfwSetWindowShouldClose(p_GLFWwindow, true);
        return;
    }

    if (p_Action == GLFW_PRESS)
    {
        engine->keyboard.onKeyPress(p_Key);
        engine->gui->getIO().AddInputCharacter(p_Key);
    }
    else if (p_Action == GLFW_RELEASE)
    {
        engine->keyboard.onKeyRelease(p_Key);
    }
    return;
}

void glfwErrCallback(int p_Error, const char *p_Description)
{
    logger.logMessage(LogHandler::MessagePriority::eError,
                      "GLFW Error...\nCode => " + std::to_string(p_Error) +
                          "\nMessage=> " + std::string(p_Description));
    return;
}

// Create buffer with Vulkan buffer/memory objects
void Engine::createBuffer(vk::BufferUsageFlags p_BufferUsageFlags,
                          vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                          vk::DeviceSize p_DeviceSize, vk::Buffer *p_VkBuffer,
                          vk::DeviceMemory *p_DeviceMemory,
                          void *p_InitialData) const
{
    logger.logMessage("Allocating buffer of size => " +
                      std::to_string(static_cast<uint32_t>(p_DeviceSize)));

    vk::BufferCreateInfo bufferInfo;
    bufferInfo.usage = p_BufferUsageFlags;
    bufferInfo.size = p_DeviceSize;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    // create vulkan buffer
    *p_VkBuffer = logicalDevice.createBuffer(bufferInfo);

    // allocate memory for buffer
    vk::MemoryRequirements memRequirements;
    vk::MemoryAllocateInfo allocInfo;

    memRequirements = logicalDevice.getBufferMemoryRequirements(*p_VkBuffer);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        getMemoryType(memRequirements.memoryTypeBits, p_MemoryPropertyFlags);

    // Allocate memory
    *p_DeviceMemory = logicalDevice.allocateMemory(allocInfo);

    // Bind newly allocated memory to buffer
    logicalDevice.bindBufferMemory(*p_VkBuffer, *p_DeviceMemory, 0);

    // If data was passed to creation, load it
    if (p_InitialData != nullptr)
    {
        void *mapped =
            logicalDevice.mapMemory(*p_DeviceMemory, 0, p_DeviceSize);

        memcpy(mapped, p_InitialData, p_DeviceSize);

        logicalDevice.unmapMemory(*p_DeviceMemory);
    }

    return;
}

// create buffer with custom Buffer interface
void Engine::createBuffer(vk::BufferUsageFlags p_BufferUsageFlags,
                          vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                          MvBuffer *p_MvBuffer, vk::DeviceSize p_DeviceSize,
                          void *p_InitialData) const
{
    logger.logMessage("Allocating buffer of size => " +
                      std::to_string(static_cast<uint32_t>(p_DeviceSize)));

    // create buffer
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.usage = p_BufferUsageFlags;
    bufferInfo.size = p_DeviceSize;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    p_MvBuffer->buffer = logicalDevice.createBuffer(bufferInfo);

    vk::MemoryRequirements memRequirements;
    vk::MemoryAllocateInfo allocInfo;

    memRequirements =
        logicalDevice.getBufferMemoryRequirements(p_MvBuffer->buffer);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        getMemoryType(memRequirements.memoryTypeBits, p_MemoryPropertyFlags);

    // allocate memory
    p_MvBuffer->memory = logicalDevice.allocateMemory(allocInfo);

    p_MvBuffer->alignment = memRequirements.alignment;
    p_MvBuffer->size = p_DeviceSize;
    p_MvBuffer->usageFlags = p_BufferUsageFlags;
    p_MvBuffer->memoryPropertyFlags = p_MemoryPropertyFlags;

    // bind buffer & memory
    p_MvBuffer->bind(logicalDevice);

    p_MvBuffer->setupBufferInfo();

    // copy if necessary
    if (p_InitialData != nullptr)
    {
        p_MvBuffer->map(logicalDevice);

        memcpy(p_MvBuffer->mapped, p_InitialData, p_DeviceSize);

        p_MvBuffer->unmap(logicalDevice);
    }

    return;
}