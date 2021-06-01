#include "mvEngine.h"
#include "mvAllocator.h"
#include "mvCollection.h"
#include "mvHelper.h"
#include "mvModel.h"

extern LogHandler logger;

Engine::Engine(int w, int h, const char *title) : Window(w, h, title), mapHandler(this)
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

    // cleanup map handler
    mapHandler.cleanup(logicalDevice);

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
        coreFramebuffers.clear();
    }

    // destroy gui framebuffers
    if (!guiFramebuffers.empty())
    {
        for (auto &buffer : guiFramebuffers)
        {
            if (buffer)
                logicalDevice.destroyFramebuffer(buffer, nullptr);
        }
        guiFramebuffers.clear();
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
        pipelines.clear();
    }

    // destroy pipeline layouts
    if (!pipelineLayouts.empty())
    {
        for (auto &layout : pipelineLayouts)
        {
            if (layout.second)
                logicalDevice.destroyPipelineLayout(layout.second);
        }
        pipelineLayouts.clear();
    }

    // destroy render pass
    if (!renderPasses.empty())
    {
        for (auto &pass : renderPasses)
        {
            if (pass.second)
                logicalDevice.destroyRenderPass(pass.second, nullptr);
        }
        renderPasses.clear();
    }

    // cleanup command pool
    if (commandPool)
    {
        logicalDevice.destroyCommandPool(commandPool);
    }

    // cleanup swapchain
    swapchain.cleanup(instance, logicalDevice, false);

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

    // Create layouts for render passes
    prepareLayouts();

    // create render pass : core
    setupRenderPass();

    // If ImGui enabled, recreate it's resources
    if (gui)
    {
        gui->cleanup(logicalDevice);
        // Reinstall glfw callbacks to avoid ImGui creating
        // an infinite callback circle
        glfwSetKeyCallback(window, keyCallback);
        glfwSetCursorPosCallback(window, mouseMotionCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetScrollCallback(window, mouseScrollCallback);
        glfwSetCharCallback(window, NULL);
        // Add our base app class to glfw window for callbacks
        glfwSetWindowUserPointer(window, this);
        gui = std::make_unique<GuiHandler>(window, &mapHandler, &camera, instance, physicalDevice, logicalDevice,
                                           swapchain, commandPool, graphicsQueue, renderPasses, allocator->get()->pool);

        guiFramebuffers = gui->createFramebuffers(logicalDevice, renderPasses.at(eImGui), swapchain.buffers,
                                                  swapchain.swapExtent.width, swapchain.swapExtent.height);
    }

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

    prepareLayouts();
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
    gui = std::make_unique<GuiHandler>(window, &mapHandler, &camera, instance, physicalDevice, logicalDevice, swapchain,
                                       commandPool, graphicsQueue, renderPasses, allocator->get()->pool);

    guiFramebuffers = gui->createFramebuffers(logicalDevice, renderPasses.at(eImGui), swapchain.buffers,
                                              swapchain.swapExtent.width, swapchain.swapExtent.height);

    auto renderStart = chrono::now();
    auto renderStop = chrono::now();

#ifndef NDEBUG
    // FORCE TESTING SWAPCHAIN RECREATION TO ENSURE
    // WE AREN'T MISSING REINITIALIZATION METHODS
    recreateSwapchain();

    // try
    // {
    //     mapHandler.readHeightMap(gui.get(), "heightmaps/scaled.png");
    // }
    // catch (std::exception &e)
    // {
    //     std::cout << ":: Fatal error reading heightmap :: \n" << e.what() << "\n";
    //     std::exit(1);
    // }
#endif

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
                    camera.rotate({-mouse.dragDeltaY, -mouse.dragDeltaX, 0.0f}, 1.0f);

                    mouse.storedOrbit = camera.orbitAngle;
                    mouse.storedPitch = camera.pitch;
                    mouse.clear();
                }

                // WASD
                if (keyboard.isKeyState(GLFW_KEY_W))
                    camera.move(camera.rotation.y, {0.0f, 0.0f, -1.0f}, (keyboard.isKeyState(GLFW_KEY_LEFT_SHIFT)));
                if (keyboard.isKeyState(GLFW_KEY_A))
                    camera.move(camera.rotation.y, {-1.0f, 0.0f, 0.0f}, (keyboard.isKeyState(GLFW_KEY_LEFT_SHIFT)));
                if (keyboard.isKeyState(GLFW_KEY_S))
                    camera.move(camera.rotation.y, {0.0f, 0.0f, 1.0f}, (keyboard.isKeyState(GLFW_KEY_LEFT_SHIFT)));
                if (keyboard.isKeyState(GLFW_KEY_D))
                    camera.move(camera.rotation.y, {1.0f, 0.0f, 0.0f}, (keyboard.isKeyState(GLFW_KEY_LEFT_SHIFT)));

                // space + alt
                if (keyboard.isKeyState(GLFW_KEY_SPACE))
                    camera.moveUp(keyboard.isKeyState(GLFW_KEY_LEFT_SHIFT));
                if (keyboard.isKeyState(GLFW_KEY_LEFT_ALT))
                    camera.moveDown(keyboard.isKeyState(GLFW_KEY_LEFT_SHIFT));
            }

            /*
                THIRD PERSON CAMERA
            */
            if (camera.type == CameraType::eThirdPerson && !gui->hasFocus)
            {

                /*
                    Mouse scroll wheel
                */
                if (mouseEvent.type == Mouse::Event::Type::eWheelUp && !gui->hover)
                {
                    camera.adjustZoom(-(camera.zoomStep));
                }
                if (mouseEvent.type == Mouse::Event::Type::eWheelDown && !gui->hover)
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
                if (keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) && !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle, glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + left + right -- go straight
                if (keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) && keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle, glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + right
                if (keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) && keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle, glm::vec4(1.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + left
                if (keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) && !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle, glm::vec4(-1.0f, 0.0f, -1.0f, 1.0f));
                }

                // backward only
                if (!keyboard.isKeyState(GLFW_KEY_W) && keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) && !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + left
                if (!keyboard.isKeyState(GLFW_KEY_W) && keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) && !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle, glm::vec4(-1.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + right
                if (!keyboard.isKeyState(GLFW_KEY_W) && keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) && keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + left + right -- go back
                if (!keyboard.isKeyState(GLFW_KEY_W) && keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) && keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }

                // left only
                if (!keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    keyboard.isKeyState(GLFW_KEY_A) && !keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle, glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f));
                }

                // right only
                if (!keyboard.isKeyState(GLFW_KEY_W) && !keyboard.isKeyState(GLFW_KEY_S) &&
                    !keyboard.isKeyState(GLFW_KEY_A) && keyboard.isKeyState(GLFW_KEY_D))
                {
                    camera.target->move(camera.orbitAngle, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
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
                    allocator->createObject(collectionHandler->models.get(), "models/_viking_room.fbx");
                    collectionHandler->models->at(0).objects->back().position = glm::vec3(x, y, z);
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

            gui->update(swapchain.swapExtent,
                        std::chrono::duration<float, std::ratio<1L, 1000L>>(renderStop - renderStart).count(),
                        std::chrono::duration<float, std::chrono::milliseconds::period>(deltaTime).count(),
                        static_cast<uint32_t>(collectionHandler->modelNames.size()),
                        collectionHandler->getObjectCount(),
                        collectionHandler->getVertexCount() + mapHandler.indexCount);

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

void Engine::prepareLayouts(void)
{
    using enum PipelineTypes;
    vk::DescriptorSetLayout uniformLayout = allocator->getLayout(vk::DescriptorType::eUniformBuffer);
    vk::DescriptorSetLayout samplerLayout = allocator->getLayout(vk::DescriptorType::eCombinedImageSampler);

    std::vector<vk::DescriptorSetLayout> layoutWSampler = {
        uniformLayout, // Object uniform
        uniformLayout, // View uniform
        uniformLayout, // Projection uniform
        samplerLayout, // Object texture sampler
    };
    std::vector<vk::DescriptorSetLayout> layoutNoSampler = {
        uniformLayout, // Object uniform
        uniformLayout, // View uniform
        uniformLayout, // Projection uniform
    };
    std::vector<vk::DescriptorSetLayout> layoutTerrainMeshWSampler = {
        uniformLayout, // View uniform
        uniformLayout, // Projection uniform
        samplerLayout, // Terrain Texture Sampler
    };
    std::vector<vk::DescriptorSetLayout> layoutTerrainMeshNoSampler = {
        uniformLayout, // View uniform
        uniformLayout, // Projection uniform
    };

    // Pipeline for models with textures
    vk::PipelineLayoutCreateInfo pLineWithSamplerInfo;
    pLineWithSamplerInfo.setLayoutCount = static_cast<uint32_t>(layoutWSampler.size());
    pLineWithSamplerInfo.pSetLayouts = layoutWSampler.data();

    // Pipeline with no textures
    vk::PipelineLayoutCreateInfo pLineNoSamplerInfo;
    pLineNoSamplerInfo.setLayoutCount = static_cast<uint32_t>(layoutNoSampler.size());
    pLineNoSamplerInfo.pSetLayouts = layoutNoSampler.data();

    // Pipeline for terrain vertices
    vk::PipelineLayoutCreateInfo pLineTerrainMeshNoSamplerInfo;
    pLineTerrainMeshNoSamplerInfo.setLayoutCount = static_cast<uint32_t>(layoutTerrainMeshNoSampler.size());
    pLineTerrainMeshNoSamplerInfo.pSetLayouts = layoutTerrainMeshNoSampler.data();

    vk::PipelineLayoutCreateInfo pLineTerrainMeshWSamplerInfo;
    pLineTerrainMeshWSamplerInfo.setLayoutCount = static_cast<uint32_t>(layoutTerrainMeshWSampler.size());
    pLineTerrainMeshWSamplerInfo.pSetLayouts = layoutTerrainMeshWSampler.data();

    // Model, View, Projection
    // Color, UV, Sampler
    pipelineLayouts.insert({
        eMVPWSampler,
        logicalDevice.createPipelineLayout(pLineWithSamplerInfo),
    });

    // Model, View, Projection
    // Color, UV
    pipelineLayouts.insert({
        eMVPNoSampler,
        logicalDevice.createPipelineLayout(pLineNoSamplerInfo),
    });

    // View, Projection
    // Color, UV, Sampler
    pipelineLayouts.insert({eVPWSampler, logicalDevice.createPipelineLayout(pLineTerrainMeshWSamplerInfo)});

    // View, Projection
    // Color, UV
    pipelineLayouts.insert({eVPNoSampler, logicalDevice.createPipelineLayout(pLineTerrainMeshNoSamplerInfo)});
    return;
}

/*
  Creation of all the graphics pipelines
*/
void Engine::preparePipeline(void)
{
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
    debugIaState.topology = vk::PrimitiveTopology::ePointList;

    vk::PipelineRasterizationStateCreateInfo rsState;
    rsState.depthClampEnable = VK_FALSE;
    rsState.rasterizerDiscardEnable = VK_FALSE;
    rsState.polygonMode = vk::PolygonMode::eFill;
    rsState.cullMode = vk::CullModeFlagBits::eBack;
    rsState.frontFace = vk::FrontFace::eCounterClockwise;
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
    vp.y = 0;
    vp.width = static_cast<float>(windowWidth);
    vp.height = static_cast<float>(windowHeight);
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
    msState.rasterizationSamples = swapchain.sampleCount;
    msState.sampleShadingEnable = VK_TRUE;
    msState.minSampleShading = 1.0f;

    // Load shaders
    auto vsMVP = readFile("shaders/vsMVP.spv");
    auto vsVP = readFile("shaders/vsVP.spv");

    auto fsMVPSampler = readFile("shaders/fsMVPSampler.spv");
    auto fsMVPNoSampler = readFile("shaders/fsMVPNoSampler.spv");

    auto fsVPSampler = readFile("shaders/fsVPSampler.spv");
    auto fsVPNoSampler = readFile("shaders/fsVPNoSampler.spv");

    // Ensure we have files
    if (vsMVP.empty() || vsVP.empty() || fsMVPSampler.empty() || fsMVPNoSampler.empty() || fsVPSampler.empty() ||
        fsVPNoSampler.empty())
        throw std::runtime_error("Failed to load fragment or vertex shader spv files");

    vk::ShaderModule vsModuleMVP = createShaderModule(vsMVP);
    vk::ShaderModule vsModuleVP = createShaderModule(vsVP);

    vk::ShaderModule fsModuleMVPSampler = createShaderModule(fsMVPSampler);
    vk::ShaderModule fsModuleMVPNoSampler = createShaderModule(fsMVPNoSampler);

    vk::ShaderModule fsModuleVPSampler = createShaderModule(fsVPSampler);
    vk::ShaderModule fsModuleVPNoSampler = createShaderModule(fsVPNoSampler);

    /*
        VERTEX SHADER
        MODEL VIEW PROJECTION MATRIX
    */
    vk::PipelineShaderStageCreateInfo vsStageInfoMVP;
    vsStageInfoMVP.stage = vk::ShaderStageFlagBits::eVertex;
    vsStageInfoMVP.module = vsModuleMVP;
    vsStageInfoMVP.pName = "main";
    vsStageInfoMVP.pSpecializationInfo = nullptr;

    /*
        VERTEX SHADER
        VIEW PROJECTION MATRIX
    */
    vk::PipelineShaderStageCreateInfo vsStageInfoVP;
    vsStageInfoVP.stage = vk::ShaderStageFlagBits::eVertex;
    vsStageInfoVP.module = vsModuleVP;
    vsStageInfoVP.pName = "main";
    vsStageInfoVP.pSpecializationInfo = nullptr;

    /*
        FRAGMENT SHADER
        COLOR, UV, SAMPLER -- USE MVP VERTEX SHADER
    */
    vk::PipelineShaderStageCreateInfo fsStageInfoMVPSampler;
    fsStageInfoMVPSampler.stage = vk::ShaderStageFlagBits::eFragment;
    fsStageInfoMVPSampler.module = fsModuleMVPSampler;
    fsStageInfoMVPSampler.pName = "main";
    fsStageInfoMVPSampler.pSpecializationInfo = nullptr;

    /*
        FRAGMENT SHADER
        COLOR, UV -- USE MVP VERTEX SHADER
    */
    vk::PipelineShaderStageCreateInfo fsStageInfoMVPNoSampler;
    fsStageInfoMVPNoSampler.stage = vk::ShaderStageFlagBits::eFragment;
    fsStageInfoMVPNoSampler.module = fsModuleMVPNoSampler;
    fsStageInfoMVPNoSampler.pName = "main";
    fsStageInfoMVPNoSampler.pSpecializationInfo = nullptr;

    /*
        FRAGMENT SHADER
        COLOR, UV, SAMPLER -- USE VP VERTEX SHADER
    */
    vk::PipelineShaderStageCreateInfo fsStageInfoVPSampler;
    fsStageInfoVPSampler.stage = vk::ShaderStageFlagBits::eFragment;
    fsStageInfoVPSampler.module = fsModuleVPSampler;
    fsStageInfoVPSampler.pName = "main";
    fsStageInfoVPSampler.pSpecializationInfo = nullptr;

    /*
         FRAGMENT SHADER
         COLOR, UV -- USE VP VERTEX SHADER
    */
    vk::PipelineShaderStageCreateInfo fsStageInfoVPNoSampler;
    fsStageInfoVPNoSampler.stage = vk::ShaderStageFlagBits::eFragment;
    fsStageInfoVPNoSampler.module = fsModuleVPNoSampler;
    fsStageInfoVPNoSampler.pName = "main";
    fsStageInfoVPNoSampler.pSpecializationInfo = nullptr;

    // Vertex | Fragment
    // M/V/P uniforms | Color, UV, Sampler
    std::vector<vk::PipelineShaderStageCreateInfo> ssMVPWSampler = {vsStageInfoMVP, fsStageInfoMVPSampler};
    std::vector<vk::PipelineShaderStageCreateInfo> ssMVPNoSampler = {vsStageInfoMVP, fsStageInfoMVPNoSampler};
    // Vertex | Fragment
    // V/P uniforms | Color, UV, Sampler
    std::vector<vk::PipelineShaderStageCreateInfo> ssVPWSampler = {vsStageInfoVP, fsStageInfoVPSampler};
    // Vertex | Fragment
    // V/P uniforms | Color, UV
    std::vector<vk::PipelineShaderStageCreateInfo> ssVPNoSampler = {vsStageInfoVP, fsStageInfoVPNoSampler};

    // Create pipeline for terrain mesh W Sampler
    // V/P uniforms + color, uv, sampler
    vk::GraphicsPipelineCreateInfo plVPSamplerInfo;
    plVPSamplerInfo.renderPass = renderPasses.at(eCore);
    plVPSamplerInfo.layout = pipelineLayouts.at(eVPWSampler);
    plVPSamplerInfo.pInputAssemblyState = &iaState;
    plVPSamplerInfo.pRasterizationState = &rsState;
    plVPSamplerInfo.pColorBlendState = &cbState;
    plVPSamplerInfo.pDepthStencilState = &dsState;
    plVPSamplerInfo.pViewportState = &vpState;
    plVPSamplerInfo.pMultisampleState = &msState;
    plVPSamplerInfo.stageCount = static_cast<uint32_t>(ssVPWSampler.size());
    plVPSamplerInfo.pStages = ssVPWSampler.data();
    plVPSamplerInfo.pVertexInputState = &viState;
    plVPSamplerInfo.pDynamicState = nullptr;

    {
        vk::ResultValue result = logicalDevice.createGraphicsPipeline(nullptr, plVPSamplerInfo);
        if (result.result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create graphics pipeline for terrain mesh w/sampler");

        pipelines.insert({eVPWSampler, result.value});
        if (!pipelines.at(eVPWSampler))
            throw std::runtime_error("Failed to create pipeline for terrain mesh with sampler");
    }

    // Create pipeline for terrain mesh NO sampler
    // V/P uniforms + color, uv
    vk::GraphicsPipelineCreateInfo plVPNoSamplerInfo;
    plVPNoSamplerInfo.renderPass = renderPasses.at(eCore);
    plVPNoSamplerInfo.layout = pipelineLayouts.at(eVPNoSampler);
    plVPNoSamplerInfo.pInputAssemblyState = &iaState;
    // plVPNoSamplerInfo.pInputAssemblyState = &debugIaState;
    plVPNoSamplerInfo.pRasterizationState = &rsState;
    plVPNoSamplerInfo.pColorBlendState = &cbState;
    plVPNoSamplerInfo.pDepthStencilState = &dsState;
    plVPNoSamplerInfo.pViewportState = &vpState;
    plVPNoSamplerInfo.pMultisampleState = &msState;
    plVPNoSamplerInfo.stageCount = static_cast<uint32_t>(ssVPNoSampler.size());
    plVPNoSamplerInfo.pStages = ssVPNoSampler.data();
    plVPNoSamplerInfo.pVertexInputState = &viState;
    plVPNoSamplerInfo.pDynamicState = nullptr;

    {
        vk::ResultValue result = logicalDevice.createGraphicsPipeline(nullptr, plVPNoSamplerInfo);
        if (result.result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create graphics pipeline for terrain mesh NO sampler");

        pipelines.insert({eVPNoSampler, result.value});
        if (!pipelines.at(eVPNoSampler))
            throw std::runtime_error("Failed to create pipeline for terrain mesh with no sampler");
    }

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
    // M/V/P uniforms + color, uv, sampler
    vk::GraphicsPipelineCreateInfo plMVPSamplerInfo;
    plMVPSamplerInfo.renderPass = renderPasses.at(eCore);
    plMVPSamplerInfo.layout = pipelineLayouts.at(eMVPWSampler);
    plMVPSamplerInfo.pInputAssemblyState = &iaState;
    plMVPSamplerInfo.pRasterizationState = &rsState;
    plMVPSamplerInfo.pColorBlendState = &cbState;
    plMVPSamplerInfo.pDepthStencilState = &dsState;
    plMVPSamplerInfo.pViewportState = &vpState;
    plMVPSamplerInfo.pMultisampleState = &msState;
    plMVPSamplerInfo.stageCount = static_cast<uint32_t>(ssMVPWSampler.size());
    plMVPSamplerInfo.pStages = ssMVPWSampler.data();
    plMVPSamplerInfo.pVertexInputState = &viState;
    plMVPSamplerInfo.pDynamicState = nullptr;

    /* Graphics pipeline with sampler -- NO dynamic states */
    {
        vk::ResultValue result = logicalDevice.createGraphicsPipeline(nullptr, plMVPSamplerInfo);
        if (result.result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create graphics pipeline with sampler");

        pipelines.insert({
            eMVPWSampler,
            result.value,
        });
        if (!pipelines.at(eMVPWSampler))
            throw std::runtime_error("Failed to create generic object pipeline with sampler");
    }

    plMVPSamplerInfo.pInputAssemblyState = &debugIaState;
    plMVPSamplerInfo.pDynamicState = &dynamicInfo;

    /* Graphics pipeline with sampler -- WITH dynamic states */
    {
        vk::ResultValue result = logicalDevice.createGraphicsPipeline(nullptr, plMVPSamplerInfo);
        if (result.result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create graphics pipeline with sampler & dynamic states");

        pipelines.insert({
            eMVPWSamplerDynamic,
            result.value,
        });
        if (!pipelines.at(eMVPWSamplerDynamic))
            throw std::runtime_error("Failed to create generic object pipeline "
                                     "with sampler & dynamic states");
    }

    // Create pipeline with NO sampler
    // M/V/P uniforms + color, uv
    vk::GraphicsPipelineCreateInfo plMVPNoSamplerInfo;
    plMVPNoSamplerInfo.renderPass = renderPasses.at(eCore);
    plMVPNoSamplerInfo.layout = pipelineLayouts.at(eMVPNoSampler);
    plMVPNoSamplerInfo.pInputAssemblyState = &iaState;
    plMVPNoSamplerInfo.pRasterizationState = &rsState;
    plMVPNoSamplerInfo.pColorBlendState = &cbState;
    plMVPNoSamplerInfo.pDepthStencilState = &dsState;
    plMVPNoSamplerInfo.pViewportState = &vpState;
    plMVPNoSamplerInfo.pMultisampleState = &msState;
    plMVPNoSamplerInfo.stageCount = static_cast<uint32_t>(ssMVPNoSampler.size());
    plMVPNoSamplerInfo.pStages = ssMVPNoSampler.data();
    plMVPNoSamplerInfo.pVertexInputState = &viState;
    plMVPNoSamplerInfo.pDynamicState = nullptr;

    // Create graphics pipeline NO sampler
    {
        vk::ResultValue result = logicalDevice.createGraphicsPipeline(nullptr, plMVPNoSamplerInfo);
        if (result.result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create graphics pipeline with no sampler");

        pipelines.insert({
            eMVPNoSampler,
            result.value,
        });
        if (!pipelines.at(eMVPNoSampler))
            throw std::runtime_error("Failed to create generic object pipeline with no sampler");
    }

    // create pipeline no sampler | dynamic state
    plMVPNoSamplerInfo.pInputAssemblyState = &debugIaState;
    plMVPNoSamplerInfo.pDynamicState = &dynamicInfo;

    {
        vk::ResultValue result = logicalDevice.createGraphicsPipeline(nullptr, plMVPNoSamplerInfo);
        if (result.result != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create graphics pipeline with no "
                                     "sampler & dynamic states");

        pipelines.insert({eMVPNoSamplerDynamic, result.value});
        if (!pipelines.at(eMVPNoSamplerDynamic))
            throw std::runtime_error("Failed to create generic object pipeline "
                                     "with no sampler & dynamic states");
    }

    logicalDevice.destroyShaderModule(vsModuleMVP);
    logicalDevice.destroyShaderModule(vsModuleVP);

    logicalDevice.destroyShaderModule(fsModuleMVPSampler);
    logicalDevice.destroyShaderModule(fsModuleMVPNoSampler);

    logicalDevice.destroyShaderModule(fsModuleVPSampler);
    logicalDevice.destroyShaderModule(fsModuleVPNoSampler);
    return;
}

void Engine::recordCommandBuffer(uint32_t p_ImageIndex)
{
    // command buffer begin
    vk::CommandBufferBeginInfo beginInfo;

    // render pass info
    std::array<vk::ClearValue, 3> cls;
    cls[0].color = defaultClearColor;
    cls[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};
    cls[2].color = defaultClearColor;

    vk::RenderPassBeginInfo passInfo;
    passInfo.renderPass = renderPasses.at(eCore);
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
    commandBuffers.at(p_ImageIndex).beginRenderPass(passInfo, vk::SubpassContents::eInline);

    // Render heightmap
    if (mapHandler.isMapLoaded)
    {
        mapHandler.bindBuffer(commandBuffers.at(p_ImageIndex));

        commandBuffers.at(p_ImageIndex).bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.at(eVPWSampler));

        std::vector<vk::DescriptorSet> toBind = {
            collectionHandler->viewUniform->descriptor,
            collectionHandler->projectionUniform->descriptor,
            mapHandler.terrainTextureDescriptor,
        };
        commandBuffers.at(p_ImageIndex)
            .bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.at(eVPWSampler), 0, toBind, nullptr);

        // for (const auto &vOffset : mapHandler.vertexOffsets)
        //   {
        //     // Index offset
        //     // { index start, index count }
        //     commandBuffers.at (p_ImageIndex)
        //         .drawIndexed (mapHandler.indexOffsets
        //                           .at (&vOffset -
        //                           &mapHandler.vertexOffsets[0]) .second,
        //                       1,
        //                       mapHandler.indexOffsets
        //                           .at (&vOffset -
        //                           &mapHandler.vertexOffsets[0]) .first,
        //                       vOffset, 0);
        //   }
        commandBuffers.at(p_ImageIndex).drawIndexed(mapHandler.indexCount, 1, 0, 0, 0);
        // commandBuffers.at(p_ImageIndex).draw(mapHandler.vertexCount, 1, 0, 0);
    }

    for (auto &model : *collectionHandler->models)
    {
        // for each model select the appropriate pipeline
        if (model.hasTexture && currentlyBound != eMVPWSampler)
        {
            commandBuffers.at(p_ImageIndex).bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.at(eMVPWSampler));
            currentlyBound = eMVPWSampler;
        }
        else if (!model.hasTexture && currentlyBound != eMVPNoSampler)
        {
            commandBuffers.at(p_ImageIndex).bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.at(eMVPNoSampler));
            currentlyBound = eMVPNoSampler;
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
                std::vector<vk::DescriptorSet> toBind = {object.uniform.first,
                                                         collectionHandler->viewUniform->descriptor,
                                                         collectionHandler->projectionUniform->descriptor};
                if (offset.first.second >= 0)
                {
                    toBind.push_back(model.textureDescriptors.at(offset.first.second).first);
                    commandBuffers.at(p_ImageIndex)
                        .bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.at(eMVPWSampler), 0,
                                            toBind, nullptr);
                }
                else
                {
                    commandBuffers.at(p_ImageIndex)
                        .bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayouts.at(eMVPNoSampler), 0,
                                            toBind, nullptr);
                }

                // { { vertex offset, Texture index }, { Index start, Index count
                // } }
                commandBuffers.at(p_ImageIndex)
                    .drawIndexed(offset.second.second, 1, offset.second.first, offset.first.first, 0);
            }
        }
    }

    commandBuffers.at(p_ImageIndex).endRenderPass();

    /*
     IMGUI RENDER
    */
    gui->doRenderPass(renderPasses.at(eImGui), guiFramebuffers.at(p_ImageIndex), commandBuffers.at(p_ImageIndex),
                      swapchain.swapExtent);
    /*
      END IMGUI RENDER
    */

    commandBuffers.at(p_ImageIndex).end();

    return;
}

void Engine::draw(size_t &p_CurrentFrame, uint32_t &p_CurrentImageIndex)
{
    if (glfwWindowShouldClose(window))
        return;
    vk::Result res = logicalDevice.waitForFences(inFlightFences.at(p_CurrentFrame), VK_TRUE, UINT64_MAX);
    if (res != vk::Result::eSuccess)
        throw std::runtime_error("Error occurred while waiting for fence");

    vk::Result result = logicalDevice.acquireNextImageKHR(swapchain.swapchain, UINT64_MAX, semaphores.presentComplete,
                                                          nullptr, &p_CurrentImageIndex);

    switch (result)
    {
        using enum vk::Result;
        case eErrorOutOfDateKHR:
            {
                recreateSwapchain();
                return;
            }
        case eSuboptimalKHR:
            {
                using enum LogHandlerMessagePriority;
                logger.logMessage(eWarning, "Swapchain is no longer optimal : not recreating");
                break;
            }
        case eSuccess:
            {
                break;
            }
        default: // unhandled error occurred
            {
                throw std::runtime_error("Unhandled error case while acquiring a swapchain image for "
                                         "rendering");
            }
    }

    if (waitFences.at(p_CurrentImageIndex))
    {
        vk::Result res = logicalDevice.waitForFences(waitFences.at(p_CurrentImageIndex), VK_TRUE, UINT64_MAX);
        if (res != vk::Result::eSuccess)
            throw std::runtime_error("Error occurred while waiting for fence");
    }

    recordCommandBuffer(p_CurrentImageIndex);

    // mark in use
    waitFences.at(p_CurrentImageIndex) = inFlightFences.at(p_CurrentFrame);

    vk::SubmitInfo submitInfo;
    vk::Semaphore waitSemaphores[] = {semaphores.presentComplete};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers.at(p_CurrentImageIndex);
    vk::Semaphore renderSemaphores[] = {semaphores.renderComplete};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = renderSemaphores;

    logicalDevice.resetFences(inFlightFences.at(p_CurrentFrame));

    result = graphicsQueue.submit(1, &submitInfo, inFlightFences.at(p_CurrentFrame));

    switch (result)
    {
        using enum vk::Result;
        case eErrorOutOfDateKHR:
            {
                recreateSwapchain();
                return;
            }
        case eSuboptimalKHR:
            {
                using enum LogHandlerMessagePriority;
                logger.logMessage(eWarning, "Swapchain is no longer optimal : not recreating");
                break;
            }
        case eSuccess:
            {
                break;
            }
        default: // unhandled error occurred
            {
                throw std::runtime_error("Unhandled error case while submitting queue");
            }
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
        using enum vk::Result;
        case eErrorOutOfDateKHR:
            {
                recreateSwapchain();
                return;
                break;
            }
        case eSuboptimalKHR:
            {
                using enum LogHandlerMessagePriority;
                logger.logMessage(eWarning, "Swapchain is no longer optimal : not recreating");
                break;
            }
        case eSuccess:
            {
                break;
            }
        default: // unhandled error occurred
            {
                throw std::runtime_error("Unhandled error case while presenting");
            }
    }
    p_CurrentFrame = (p_CurrentFrame + 1) % MAX_IN_FLIGHT;
    return;
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
    allocator->createLayout(vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, 0);

    /*
        TEXTURE SAMPLER LAYOUT
    */
    allocator->createLayout(vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, 0);

    /*
        INITIALIZE MODEL/OBJECT HANDLER
        CREATES BUFFERS FOR VIEW & PROJECTION MATRICES
    */

    vk::DescriptorSetLayout uniformLayout = allocator->getLayout(vk::DescriptorType::eUniformBuffer);
    /*
        VIEW MATRIX UNIFORM OBJECT
    */
    allocator->allocateSet(uniformLayout, collectionHandler->viewUniform->descriptor);
    allocator->updateSet(collectionHandler->viewUniform->mvBuffer.bufferInfo,
                         collectionHandler->viewUniform->descriptor, 0);

    /*
        PROJECTION MATRIX UNIFORM OBJECT
    */
    allocator->allocateSet(uniformLayout, collectionHandler->projectionUniform->descriptor);
    allocator->updateSet(collectionHandler->projectionUniform->mvBuffer.bufferInfo,
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
    cameraParams.aspect = (float)swapchain.swapExtent.width / (float)swapchain.swapExtent.height;
    cameraParams.nearz = 0.1f;
    cameraParams.farz = 5000.0f;
    cameraParams.position = glm::vec3(512.0f, 40.0f, 512.0f);
    cameraParams.viewUniformObject = collectionHandler->viewUniform.get();
    cameraParams.projectionUniformObject = collectionHandler->projectionUniform.get();

    cameraParams.type = CameraType::eFreeLook;
    cameraParams.target = nullptr;

    camera = Camera(cameraParams);

    /*
        Hard coding settings
    */
    // camera.rotation = { 0.0f, -90.0f, 0.0f };
    camera.zoomLevel = 7.0f;
    mouse.deltaStyle = Mouse::DeltaStyles::eFromLastPosition;
    mouse.isDragging = false;
    return;
}

/*
  GLFW CALLBACKS
*/

void mouseMotionCallback(GLFWwindow *p_GLFWwindow, double p_XPosition, double p_YPosition)
{
    auto engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));
    engine->mouse.update(p_XPosition, p_YPosition);
    return;
}

void mouseScrollCallback(GLFWwindow *p_GLFWwindow, [[maybe_unused]] double p_XOffset, double p_YOffset)
{
    auto engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));

    if (p_YOffset > 0)
        engine->mouse.onWheelUp();
    if (p_YOffset < 0)
        engine->mouse.onWheelDown();

    return;
}

void mouseButtonCallback(GLFWwindow *p_GLFWwindow, int p_Button, int p_Action, [[maybe_unused]] int p_Modifiers)
{
    auto engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));

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

void keyCallback(GLFWwindow *p_GLFWwindow, int p_Key, [[maybe_unused]] int p_ScanCode, int p_Action,
                 [[maybe_unused]] int p_Modifier)
{
    // get get pointer
    auto engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(p_GLFWwindow));

    if (p_Key == GLFW_KEY_ESCAPE && p_Action == GLFW_PRESS)
    {
        // glfwSetWindowShouldClose(p_GLFWwindow, true);
        return;
    }

    if (p_Action == GLFW_PRESS)
    {
        engine->keyboard.onKeyPress(p_Key);
        // engine->gui->getIO().AddInputCharacter(p_Key);
    }
    else if (p_Action == GLFW_RELEASE)
    {
        engine->keyboard.onKeyRelease(p_Key);
    }
    return;
}

void glfwErrCallback(int p_Error, const char *p_Description)
{
    logger.logMessage(LogHandlerMessagePriority::eError, "GLFW Error...\nCode => " + std::to_string(p_Error) +
                                                             "\nMessage=> " + std::string(p_Description));
    return;
}

// Create buffer with Vulkan buffer/memory objects
void Engine::createBuffer(vk::BufferUsageFlags p_BufferUsageFlags, vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                          vk::DeviceSize p_DeviceSize, vk::Buffer *p_VkBuffer, vk::DeviceMemory *p_DeviceMemory,
                          void *p_InitialData) const
{
    logger.logMessage("Allocating buffer of size => " + std::to_string(static_cast<uint32_t>(p_DeviceSize)));

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
    allocInfo.memoryTypeIndex = getMemoryType(memRequirements.memoryTypeBits, p_MemoryPropertyFlags);

    // Allocate memory
    *p_DeviceMemory = logicalDevice.allocateMemory(allocInfo);

    // Bind newly allocated memory to buffer
    logicalDevice.bindBufferMemory(*p_VkBuffer, *p_DeviceMemory, 0);

    // If data was passed to creation, load it
    if (p_InitialData != nullptr)
    {
        void *mapped = logicalDevice.mapMemory(*p_DeviceMemory, 0, p_DeviceSize);

        memcpy(mapped, p_InitialData, p_DeviceSize);

        logicalDevice.unmapMemory(*p_DeviceMemory);
    }

    return;
}

// create buffer with custom Buffer interface
void Engine::createBuffer(vk::BufferUsageFlags p_BufferUsageFlags, vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                          MvBuffer *p_MvBuffer, vk::DeviceSize p_DeviceSize, void *p_InitialData) const
{
    logger.logMessage("Allocating buffer of size => " + std::to_string(static_cast<uint32_t>(p_DeviceSize)));

    // create buffer
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.usage = p_BufferUsageFlags;
    bufferInfo.size = p_DeviceSize;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    p_MvBuffer->buffer = logicalDevice.createBuffer(bufferInfo);

    vk::MemoryRequirements memRequirements;
    vk::MemoryAllocateInfo allocInfo;

    memRequirements = logicalDevice.getBufferMemoryRequirements(p_MvBuffer->buffer);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = getMemoryType(memRequirements.memoryTypeBits, p_MemoryPropertyFlags);

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