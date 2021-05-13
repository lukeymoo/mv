#include "mvEngine.h"

mv::Engine::Engine(int w, int h, const char *title) : MWindow(w, h, title)
{
    pipelines = std::make_unique<std::unordered_map<std::string, vk::Pipeline>>();
    pipeline_layouts = std::make_unique<std::unordered_map<std::string, vk::PipelineLayout>>();
    return;
}

mv::Engine::~Engine()
{
    if (!mv_device)
        return;

    mv_device->logical_device->waitIdle();

    if (pipelines)
    {
        for (auto &pipeline : *pipelines)
        {
            if (pipeline.second)
                mv_device->logical_device->destroyPipeline(pipeline.second);
        }
        pipelines.reset();
    }

    if (pipeline_layouts)
    {
        for (auto &layout : *pipeline_layouts)
        {
            if (layout.second)
                mv_device->logical_device->destroyPipelineLayout(layout.second);
        }
        pipeline_layouts.reset();
    }

    /*
      DEBUGGING CLEANUP
    */
    if (reticle_mesh.vertex_buffer)
    {
        mv_device->logical_device->destroyBuffer(reticle_mesh.vertex_buffer);
        mv_device->logical_device->freeMemory(reticle_mesh.vertex_memory);
    }
    if (reticle_obj.uniform_buffer.buffer)
    {
        reticle_obj.uniform_buffer.destroy(*mv_device);
    }

    // collection struct will handle cleanup of models & objs
    if (collection_handler)
    {
        collection_handler->cleanup(*mv_device, *descriptor_allocator);
    }
}

void mv::Engine::cleanup_swapchain(void)
{
    // destroy command buffers
    if (command_buffers)
    {
        if (!command_buffers->empty())
        {
            mv_device->logical_device->freeCommandBuffers(*command_pool, *command_buffers);
        }
        command_buffers.reset();
        command_buffers = std::make_unique<std::vector<vk::CommandBuffer>>();
    }

    // destroy framebuffers
    if (frame_buffers)
    {
        if (!frame_buffers->empty())
        {
            for (auto &buffer : *frame_buffers)
            {
                if (buffer)
                {
                    mv_device->logical_device->destroyFramebuffer(buffer, nullptr);
                }
            }
            frame_buffers.reset();
            frame_buffers = std::make_unique<std::vector<vk::Framebuffer>>();
        }
    }

    if (depth_stencil)
    {
        if (depth_stencil->image)
        {
            mv_device->logical_device->destroyImage(depth_stencil->image, nullptr);
        }
        if (depth_stencil->view)
        {
            mv_device->logical_device->destroyImageView(depth_stencil->view, nullptr);
        }
        if (depth_stencil->mem)
        {
            mv_device->logical_device->freeMemory(depth_stencil->mem, nullptr);
        }
        depth_stencil.reset();
        depth_stencil = std::make_unique<struct mv::MWindow::depth_stencil_struct>();
    }

    // destroy pipelines
    if (pipelines)
    {
        for (auto &pipeline : *pipelines)
        {
            if (pipeline.second)
                mv_device->logical_device->destroyPipeline(pipeline.second);
        }
        pipelines.reset();
        pipelines = std::make_unique<std::unordered_map<std::string, vk::Pipeline>>();
    }

    // destroy pipeline layouts
    if (pipeline_layouts)
    {
        for (auto &layout : *pipeline_layouts)
        {
            if (layout.second)
                mv_device->logical_device->destroyPipelineLayout(layout.second);
        }
        pipeline_layouts.reset();
        pipeline_layouts = std::make_unique<std::unordered_map<std::string, vk::PipelineLayout>>();
    }

    // destroy render pass
    if (render_pass)
    {
        mv_device->logical_device->destroyRenderPass(*render_pass, nullptr);
        render_pass.reset();
        render_pass = std::make_unique<vk::RenderPass>();
    }

    // cleanup command pool
    if (command_pool)
    {
        mv_device->logical_device->destroyCommandPool(*command_pool);
        command_pool.reset();
        command_pool = std::make_unique<vk::CommandPool>();
    }

    // cleanup swapchain
    swapchain->cleanup(*instance, *mv_device, false);

    return;
}

// Allows swapchain to keep up with window resize
void mv::Engine::recreate_swapchain(void)
{
    std::cout << "[+] recreating swapchain" << std::endl;

    if (!mv_device)
        throw std::runtime_error("mv device handler is somehow null, tried to recreate swap chain :: "
                                 "main");

    mv_device->logical_device->waitIdle();

    // Cleanup
    cleanup_swapchain();

    *command_pool = mv_device->create_command_pool(swapchain->graphics_index);

    // create swapchain
    swapchain->create(*physical_device, *mv_device, window_width, window_height);

    // create render pass
    setup_render_pass();

    setup_depth_stencil();

    // create pipeline layout
    // create pipeline
    prepare_pipeline();
    // create framebuffers
    setup_framebuffer();
    // create command buffers
    create_command_buffers();
}

void mv::Engine::go(void)
{
    std::cout << "[+] Preparing vulkan\n";
    prepare();

    // setup descriptor allocator, collection handler & camera
    // load models & create objects here
    go_setup();

    /*
      Hard coding settings
    */
    camera->zoom_level = 5.0f;
    mouse.delta_style = mouse.delta_styles::from_last;
    mouse.is_dragging = false;
    // if (camera->type == Camera::camera_type::first_person || Camera::camera_type::free_look) {
    //   mouse.set_delta_style(mouse.delta_calc_style::from_center);
    // } else {
    //   mouse.set_delta_style(mouse.delta_calc_style::from_last);
    // }

    prepare_pipeline();

    // basic check
    assert(camera);
    assert(collection_handler);
    assert(descriptor_allocator);

    uint32_t image_index = 0;
    size_t current_frame = 0;

    bool added = false;

    [[maybe_unused]] int mouse_centerx = (swapchain->swap_extent.width / 2) + 50;
    [[maybe_unused]] int mouse_centery = (swapchain->swap_extent.height / 2) - 100;

    fps.startTimer();
    [[maybe_unused]] int fps_counter = 0;

    using chrono = std::chrono::high_resolution_clock;

    constexpr std::chrono::nanoseconds timestep(16ms);

    std::chrono::nanoseconds accumulated(0ns);
    auto start_time = chrono::now();

    /*
      IMGUI Params
    */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    [[maybe_unused]] ImGuiIO &io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();

    bool hello_world_active = true;

    ImGui_ImplVulkan_InitInfo init_info{
        .Instance = *instance,
        .PhysicalDevice = *physical_device,
        .Device = *mv_device->logical_device,
        .QueueFamily = mv_device->queue_idx.graphics,
        .Queue = *mv_device->graphics_queue,
        .PipelineCache = nullptr,
        .DescriptorPool = descriptor_allocator->get()->pool,
        .MinImageCount = static_cast<uint32_t>(swapchain->images->size()),
        .ImageCount = static_cast<uint32_t>(swapchain->images->size()),
        .Allocator = nullptr,
        .CheckVkResultFn = nullptr,
    };
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_Init(&init_info, *render_pass);

    /*
      Upload ImGui render fonts
    */
    {
        vk::CommandBuffer gui_font = Engine::begin_command_buffer();

        ImGui_ImplVulkan_CreateFontsTexture(gui_font);

        Engine::end_command_buffer(gui_font);

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    while (!glfwWindowShouldClose(window))
    {
        auto delta_time = chrono::now() - start_time;
        start_time = chrono::now();
        accumulated += std::chrono::duration_cast<std::chrono::nanoseconds>(delta_time);

        glfwPollEvents();

        while (accumulated >= timestep)
        {
            accumulated -= timestep;

            // Get input events
            mv::Keyboard::event kbd_event = keyboard.read();
            mv::Mouse::event mouse_event = mouse.read();

            if (camera->type == Camera::camera_type::third_person)
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
                  Start & Stop mouse drag
                */
                if (mouse_event.type == mv::Mouse::event::etype::r_down)
                {
                    if (mouse.is_dragging)
                    {
                        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                        mouse.end_drag();
                    }
                    else
                    {
                        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        mouse.start_drag(camera->orbit_angle, camera->pitch);
                    }
                }
                // end drag
                // if (mouse_event.type == mv::mouse.event::etype::r_release && mouse.is_dragging) {
                //   // XUndefineCursor(display, window);
                //   camera->realign_orbit();
                //   mouse.end_drag();
                // }

                // if drag enabled check for release
                if (mouse.is_dragging)
                {
                    // camera orbit
                    if (mouse.drag_delta_x > 0)
                    {
                        camera->lerp_orbit(-abs(mouse.drag_delta_x));
                    }
                    else if (mouse.drag_delta_x < 0)
                    {
                        camera->adjust_orbit(abs(mouse.drag_delta_x));
                        camera->lerp_orbit(abs(mouse.drag_delta_x));
                    }

                    // camera pitch
                    if (mouse.drag_delta_y > 0)
                    {
                        camera->lerp_pitch(abs(mouse.drag_delta_y));
                    }
                    else if (mouse.drag_delta_y < 0)
                    {
                        camera->lerp_pitch(-abs(mouse.drag_delta_y));
                    }

                    camera->target->rotate_to_face(camera->orbit_angle);

                    mouse.stored_orbit = camera->orbit_angle;
                    mouse.stored_pitch = camera->pitch;
                    mouse.clear();
                }

                /*
                  DEBUGGING CONTROLS
                */

                // PRINT OUT CAMERA STATE TO CONSOLE
                if (kbd_event.type == Keyboard::event::press && kbd_event.code == GLFW_KEY_LEFT_CONTROL)
                {
                    std::cout << "Mouse data\n";
                    std::cout << "Orbit => " << camera->orbit_angle << "\n";
                    std::cout << "Pitch => " << camera->pitch << "\n";
                    std::cout << "Zoom level => " << camera->zoom_level << "\n";
                }

                // TOGGLE RENDER PLAYER AIM RAYCAST
                if (kbd_event.type == Keyboard::event::press && kbd_event.code == GLFW_KEY_LEFT_ALT)
                {
                    to_render_map.at("reticle_raycast") = !to_render_map.at("reticle_raycast");
                    std::cout << "Render player aiming raycast => " << to_render_map.at("reticle_raycast") << "\n";
                }

                /*
                  END DEBUGGING CONTROLS
                */

                // sort movement by key combination

                // forward only
                if (keyboard.is_keystate(GLFW_KEY_W) && !keyboard.is_keystate(GLFW_KEY_S) &&
                    !keyboard.is_keystate(GLFW_KEY_A) && !keyboard.is_keystate(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + left + right -- go straight
                if (keyboard.is_keystate(GLFW_KEY_W) && !keyboard.is_keystate(GLFW_KEY_S) &&
                    keyboard.is_keystate(GLFW_KEY_A) && keyboard.is_keystate(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + right
                if (keyboard.is_keystate(GLFW_KEY_W) && !keyboard.is_keystate(GLFW_KEY_S) &&
                    !keyboard.is_keystate(GLFW_KEY_A) && keyboard.is_keystate(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(1.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + left
                if (keyboard.is_keystate(GLFW_KEY_W) && !keyboard.is_keystate(GLFW_KEY_S) &&
                    keyboard.is_keystate(GLFW_KEY_A) && !keyboard.is_keystate(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(-1.0f, 0.0f, -1.0f, 1.0f));
                }

                // backward only
                if (!keyboard.is_keystate(GLFW_KEY_W) && keyboard.is_keystate(GLFW_KEY_S) &&
                    !keyboard.is_keystate(GLFW_KEY_A) && !keyboard.is_keystate(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + left
                if (!keyboard.is_keystate(GLFW_KEY_W) && keyboard.is_keystate(GLFW_KEY_S) &&
                    keyboard.is_keystate(GLFW_KEY_A) && !keyboard.is_keystate(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(-1.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + right
                if (!keyboard.is_keystate(GLFW_KEY_W) && keyboard.is_keystate(GLFW_KEY_S) &&
                    !keyboard.is_keystate(GLFW_KEY_A) && keyboard.is_keystate(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + left + right -- go back
                if (!keyboard.is_keystate(GLFW_KEY_W) && keyboard.is_keystate(GLFW_KEY_S) &&
                    keyboard.is_keystate(GLFW_KEY_A) && keyboard.is_keystate(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }

                // left only
                if (!keyboard.is_keystate(GLFW_KEY_W) && !keyboard.is_keystate(GLFW_KEY_S) &&
                    keyboard.is_keystate(GLFW_KEY_A) && !keyboard.is_keystate(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f));
                }

                // right only
                if (!keyboard.is_keystate(GLFW_KEY_W) && !keyboard.is_keystate(GLFW_KEY_S) &&
                    !keyboard.is_keystate(GLFW_KEY_A) && keyboard.is_keystate(GLFW_KEY_D))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                }

                // debug -- add new objects to world with random position
                if (keyboard.is_keystate(GLFW_KEY_SPACE) && added == false)
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
                    collection_handler->create_object(*mv_device, *descriptor_allocator, "models/_viking_room.fbx");
                    collection_handler->models->at(0).objects->back().position = glm::vec3(x, y, z);
                }
            }
            // update game objects
            collection_handler->update();

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
            raycast_p2(*this, {collection_handler->models->at(1).objects->at(0).position.x,
                               collection_handler->models->at(1).objects->at(0).position.y - 1.5f,
                               collection_handler->models->at(1).objects->at(0).position.z});
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render imgui
        if (hello_world_active)
            ImGui::ShowDemoWindow(&hello_world_active);

        ImGui::Render();

        // TODO
        // Add alpha to render
        // Render
        draw(current_frame, image_index);
    }

    // Cleanup IMGUI
    ImGui_ImplVulkan_Shutdown();
    ImGui::DestroyContext();

    int total_object_count = 0;
    for (const auto &model : *collection_handler->models)
    {
        total_object_count += model.objects->size();
    }
    std::cout << "Total objects => " << total_object_count << std::endl;
    return;
}

/*
  Creation of all the graphics pipelines
*/
void mv::Engine::prepare_pipeline(void)
{
    vk::DescriptorSetLayout uniform_layout = descriptor_allocator->get_layout("uniform_layout");
    vk::DescriptorSetLayout sampler_layout = descriptor_allocator->get_layout("sampler_layout");

    std::vector<vk::DescriptorSetLayout> layout_w_sampler = {uniform_layout, uniform_layout, uniform_layout,
                                                             sampler_layout};
    std::vector<vk::DescriptorSetLayout> layout_no_sampler = {uniform_layout, uniform_layout, uniform_layout};

    // Pipeline for models with textures
    vk::PipelineLayoutCreateInfo w_sampler_info;
    w_sampler_info.setLayoutCount = static_cast<uint32_t>(layout_w_sampler.size());
    w_sampler_info.pSetLayouts = layout_w_sampler.data();

    // Pipeline with no textures
    vk::PipelineLayoutCreateInfo no_sampler_info;
    no_sampler_info.setLayoutCount = static_cast<uint32_t>(layout_no_sampler.size());
    no_sampler_info.pSetLayouts = layout_no_sampler.data();

    pipeline_layouts->insert(std::pair<std::string, vk::PipelineLayout>(
        "sampler", mv_device->logical_device->createPipelineLayout(w_sampler_info)));

    pipeline_layouts->insert(std::pair<std::string, vk::PipelineLayout>(
        "no_sampler", mv_device->logical_device->createPipelineLayout(no_sampler_info)));

    auto binding_description = Vertex::get_binding_description();
    auto attribute_description = Vertex::get_attribute_descriptions();

    vk::PipelineVertexInputStateCreateInfo vi_state;
    vi_state.vertexBindingDescriptionCount = 1;
    vi_state.pVertexBindingDescriptions = &binding_description;
    vi_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_description.size());
    vi_state.pVertexAttributeDescriptions = attribute_description.data();

    vk::PipelineInputAssemblyStateCreateInfo ia_state;
    ia_state.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineInputAssemblyStateCreateInfo debug_ia_state; // used for dynamic states
    debug_ia_state.topology = vk::PrimitiveTopology::eLineList;

    vk::PipelineRasterizationStateCreateInfo rs_state;
    rs_state.depthClampEnable = VK_FALSE;
    rs_state.rasterizerDiscardEnable = VK_FALSE;
    rs_state.polygonMode = vk::PolygonMode::eFill;
    rs_state.cullMode = vk::CullModeFlagBits::eNone;
    rs_state.frontFace = vk::FrontFace::eClockwise;
    rs_state.depthBiasEnable = VK_FALSE;
    rs_state.depthBiasConstantFactor = 0.0f;
    rs_state.depthBiasClamp = 0.0f;
    rs_state.depthBiasSlopeFactor = 0.0f;
    rs_state.lineWidth = 1.0f;

    vk::PipelineColorBlendAttachmentState cba_state;
    cba_state.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    cba_state.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo cb_state;
    cb_state.attachmentCount = 1;
    cb_state.pAttachments = &cba_state;

    vk::PipelineDepthStencilStateCreateInfo ds_state;
    ds_state.depthTestEnable = VK_TRUE;
    ds_state.depthWriteEnable = VK_TRUE;
    ds_state.depthCompareOp = vk::CompareOp::eLessOrEqual;
    ds_state.back.compareOp = vk::CompareOp::eAlways;

    vk::Viewport vp;
    vp.x = 0;
    vp.y = window_height;
    vp.width = static_cast<float>(window_width);
    vp.height = -static_cast<float>(window_height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vk::Rect2D sc;
    sc.offset = vk::Offset2D{0, 0};
    sc.extent = swapchain->swap_extent;

    vk::PipelineViewportStateCreateInfo vp_state;
    vp_state.viewportCount = 1;
    vp_state.pViewports = &vp;
    vp_state.scissorCount = 1;
    vp_state.pScissors = &sc;

    vk::PipelineMultisampleStateCreateInfo ms_state;
    ms_state.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // Load shaders
    auto vertex_shader = read_file("shaders/vertex.spv");
    auto fragment_shader = read_file("shaders/fragment.spv");
    auto fragment_shader_no_sampler = read_file("shaders/fragment_no_sampler.spv");

    // Ensure we have files
    if (vertex_shader.empty() || fragment_shader.empty() || fragment_shader_no_sampler.empty())
    {
        throw std::runtime_error("Failed to load fragment or vertex shader spv files");
    }

    vk::ShaderModule vertex_shader_module = create_shader_module(vertex_shader);
    vk::ShaderModule fragment_shader_module = create_shader_module(fragment_shader);
    vk::ShaderModule fragment_shader_no_sampler_module = create_shader_module(fragment_shader_no_sampler);

    // describe vertex shader stage
    vk::PipelineShaderStageCreateInfo vertex_shader_stage_info;
    vertex_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
    vertex_shader_stage_info.module = vertex_shader_module;
    vertex_shader_stage_info.pName = "main";
    vertex_shader_stage_info.pSpecializationInfo = nullptr;

    // describe fragment shader stage WITH sampler
    vk::PipelineShaderStageCreateInfo fragment_shader_stage_info;
    fragment_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    fragment_shader_stage_info.module = fragment_shader_module;
    fragment_shader_stage_info.pName = "main";
    fragment_shader_stage_info.pSpecializationInfo = nullptr;

    // fragment shader stage NO sampler
    vk::PipelineShaderStageCreateInfo fragment_shader_stage_no_sampler_info;
    fragment_shader_stage_no_sampler_info.stage = vk::ShaderStageFlagBits::eFragment;
    fragment_shader_stage_no_sampler_info.module = fragment_shader_no_sampler_module;
    fragment_shader_stage_no_sampler_info.pName = "main";
    fragment_shader_stage_no_sampler_info.pSpecializationInfo = nullptr;

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = {vertex_shader_stage_info,
                                                                    fragment_shader_stage_info};
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages_no_sampler = {vertex_shader_stage_info,
                                                                               fragment_shader_stage_no_sampler_info};

    /*
      Dynamic state extension
    */
    std::array<vk::DynamicState, 1> dyn_states = {
        vk::DynamicState::ePrimitiveTopologyEXT,
    };
    vk::PipelineDynamicStateCreateInfo dynamic_info;
    dynamic_info.dynamicStateCount = static_cast<uint32_t>(dyn_states.size());
    dynamic_info.pDynamicStates = dyn_states.data();

    // create pipeline WITH sampler
    vk::GraphicsPipelineCreateInfo pipeline_w_sampler_info;
    pipeline_w_sampler_info.renderPass = *render_pass;
    pipeline_w_sampler_info.layout = pipeline_layouts->at("sampler");
    pipeline_w_sampler_info.pInputAssemblyState = &ia_state;
    pipeline_w_sampler_info.pRasterizationState = &rs_state;
    pipeline_w_sampler_info.pColorBlendState = &cb_state;
    pipeline_w_sampler_info.pDepthStencilState = &ds_state;
    pipeline_w_sampler_info.pViewportState = &vp_state;
    pipeline_w_sampler_info.pMultisampleState = &ms_state;
    pipeline_w_sampler_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_w_sampler_info.pStages = shader_stages.data();
    pipeline_w_sampler_info.pVertexInputState = &vi_state;
    pipeline_w_sampler_info.pDynamicState = nullptr;

    // Create graphics pipeline with sampler
    std::cout << "\n[+] Creating graphics pipeline with sampler -- rigid\n";
    vk::ResultValue result = mv_device->logical_device->createGraphicsPipeline(nullptr, pipeline_w_sampler_info);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline with sampler");
    pipelines->insert(std::pair<std::string, vk::Pipeline>("sampler", result.value));

    // Graphics pipeline with sampler | dynamic state
    pipeline_w_sampler_info.pInputAssemblyState = &debug_ia_state;
    pipeline_w_sampler_info.pDynamicState = &dynamic_info;

    std::cout << "\n[+] Creating graphics pipeline with sampler -- dynamic states\n";
    result = mv_device->logical_device->createGraphicsPipeline(nullptr, pipeline_w_sampler_info);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline with sampler & dynamic states");
    pipelines->insert(std::pair<std::string, vk::Pipeline>("sampler_dynamic", result.value));

    // Create pipeline with NO sampler
    vk::GraphicsPipelineCreateInfo pipeline_no_sampler_info;
    pipeline_no_sampler_info.renderPass = *render_pass;
    pipeline_no_sampler_info.layout = pipeline_layouts->at("no_sampler");
    pipeline_no_sampler_info.pInputAssemblyState = &ia_state;
    pipeline_no_sampler_info.pRasterizationState = &rs_state;
    pipeline_no_sampler_info.pColorBlendState = &cb_state;
    pipeline_no_sampler_info.pDepthStencilState = &ds_state;
    pipeline_no_sampler_info.pViewportState = &vp_state;
    pipeline_no_sampler_info.pMultisampleState = &ms_state;
    pipeline_no_sampler_info.stageCount = static_cast<uint32_t>(shader_stages_no_sampler.size());
    pipeline_no_sampler_info.pStages = shader_stages_no_sampler.data();
    pipeline_no_sampler_info.pVertexInputState = &vi_state;
    pipeline_no_sampler_info.pDynamicState = nullptr;

    // Create graphics pipeline NO sampler
    std::cout << "\n[+] Creating graphics pipeline no sampler -- rigid\n";
    result = mv_device->logical_device->createGraphicsPipeline(nullptr, pipeline_no_sampler_info);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline with no sampler");

    pipelines->insert(std::pair<std::string, vk::Pipeline>("no_sampler", result.value));

    // create pipeline no sampler | dynamic state
    pipeline_no_sampler_info.pInputAssemblyState = &debug_ia_state;
    pipeline_no_sampler_info.pDynamicState = &dynamic_info;

    std::cout << "\n[+] Creating graphics pipeline no sampler -- dynamic states\n";
    result = mv_device->logical_device->createGraphicsPipeline(nullptr, pipeline_no_sampler_info);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create graphics pipeline with no sampler & dynamic states");

    pipelines->insert(std::pair<std::string, vk::Pipeline>("no_sampler_dynamic", result.value));

    mv_device->logical_device->destroyShaderModule(vertex_shader_module);
    mv_device->logical_device->destroyShaderModule(fragment_shader_module);
    mv_device->logical_device->destroyShaderModule(fragment_shader_no_sampler_module);
    return;
}

void mv::Engine::record_command_buffer(uint32_t image_index)
{
    // command buffer begin
    vk::CommandBufferBeginInfo begin_info;

    // render pass info
    std::array<vk::ClearValue, 2> cls;
    cls[0].color = default_clear_color;
    cls[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    vk::RenderPassBeginInfo pass_info;
    pass_info.renderPass = *render_pass;
    pass_info.framebuffer = frame_buffers->at(image_index);
    pass_info.renderArea.offset.x = 0;
    pass_info.renderArea.offset.y = 0;
    pass_info.renderArea.extent.width = swapchain->swap_extent.width;
    pass_info.renderArea.extent.height = swapchain->swap_extent.height;
    pass_info.clearValueCount = static_cast<uint32_t>(cls.size());
    pass_info.pClearValues = cls.data();

    // begin recording
    command_buffers->at(image_index).begin(begin_info);

    // start render pass
    command_buffers->at(image_index).beginRenderPass(pass_info, vk::SubpassContents::eInline);

    /*
      DEBUG RENDER METHODS
    */
    for (const auto &job : to_render_map)
    {

        // player's aiming ray cast
        if (job.first == "reticle_raycast" && job.second)
        {

            command_buffers->at(image_index)
                .bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines->at("no_sampler_dynamic"));

            pfn_vkCmdSetPrimitiveTopology(command_buffers->at(image_index),
                                          static_cast<VkPrimitiveTopology>(vk::PrimitiveTopology::eLineList));

            // render line
            std::vector<vk::DescriptorSet> to_bind = {reticle_obj.model_descriptor,
                                                      collection_handler->view_uniform->descriptor,
                                                      collection_handler->projection_uniform->descriptor};
            command_buffers->at(image_index)
                .bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layouts->at("no_sampler"), 0, to_bind,
                                    nullptr);

            command_buffers->at(image_index).bindVertexBuffers(0, reticle_mesh.vertex_buffer, {0});
            command_buffers->at(image_index).draw(2, 1, 0, 0);
        }
    }

    /*
      END DEBUG RENDER METHODS
    */

    for (auto &model : *collection_handler->models)
    {
        // for each model select the appropriate pipeline
        if (model.has_texture)
        {
            command_buffers->at(image_index).bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines->at("sampler"));
        }
        else
        {
            command_buffers->at(image_index)
                .bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines->at("no_sampler"));
        }
        for (auto &object : *model.objects)
        {
            for (auto &mesh : *model._meshes)
            {
                std::vector<vk::DescriptorSet> to_bind = {object.model_descriptor,
                                                          collection_handler->view_uniform->descriptor,
                                                          collection_handler->projection_uniform->descriptor};
                if (model.has_texture)
                {
                    // TODO
                    // allow for multiple texture descriptors
                    to_bind.push_back(mesh.textures.at(0).descriptor);
                    command_buffers->at(image_index)
                        .bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layouts->at("sampler"), 0,
                                            to_bind, nullptr);
                }
                else
                {
                    command_buffers->at(image_index)
                        .bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layouts->at("no_sampler"), 0,
                                            to_bind, nullptr);
                }
                // Bind mesh buffers
                mesh.bindBuffers(command_buffers->at(image_index));

                // Indexed draw
                command_buffers->at(image_index).drawIndexed(static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
            }
        }
    }

    /*
     IMGUI RENDER
    */
    /*
      END IMGUI RENDER
    */

    command_buffers->at(image_index).endRenderPass();

    command_buffers->at(image_index).end();

    return;
}

void mv::Engine::draw(size_t &cur_frame, uint32_t &cur_index)
{
    vk::Result res = mv_device->logical_device->waitForFences(in_flight_fences->at(cur_frame), VK_TRUE, UINT64_MAX);
    if (res != vk::Result::eSuccess)
        throw std::runtime_error("Error occurred while waiting for fence");

    vk::Result result = mv_device->logical_device->acquireNextImageKHR(
        *swapchain->swapchain, UINT64_MAX, semaphores->present_complete, nullptr, &cur_index);

    switch (result)
    {
        case vk::Result::eErrorOutOfDateKHR:
            recreate_swapchain();
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

    if (wait_fences->at(cur_index))
    {
        vk::Result res = mv_device->logical_device->waitForFences(wait_fences->at(cur_index), VK_TRUE, UINT64_MAX);
        if (res != vk::Result::eSuccess)
            throw std::runtime_error("Error occurred while waiting for fence");
    }

    record_command_buffer(cur_index);

    // mark in use
    wait_fences->at(cur_index) = in_flight_fences->at(cur_frame);

    vk::SubmitInfo submit_info;
    vk::Semaphore waitSemaphores[] = {semaphores->present_complete};
    vk::PipelineStageFlags wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = waitSemaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers->at(cur_index);
    vk::Semaphore render_semaphores[] = {semaphores->render_complete};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = render_semaphores;

    mv_device->logical_device->resetFences(in_flight_fences->at(cur_frame));

    result = mv_device->graphics_queue->submit(1, &submit_info, in_flight_fences->at(cur_frame));

    switch (result)
    {
        case vk::Result::eErrorOutOfDateKHR:
            recreate_swapchain();
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

    vk::PresentInfoKHR present_info;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = render_semaphores;
    vk::SwapchainKHR swapchains[] = {*swapchain->swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &cur_index;
    present_info.pResults = nullptr;

    result = mv_device->graphics_queue->presentKHR(&present_info);

    switch (result)
    {
        case vk::Result::eErrorOutOfDateKHR:
            recreate_swapchain();
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
    cur_frame = (cur_frame + 1) % MAX_IN_FLIGHT;
}

/*
  initialize descriptor allocator, collection handler & camera
*/
inline void mv::Engine::go_setup(void)
{
    /*
      INITIALIZE DESCRIPTOR HANDLER
    */
    descriptor_allocator = std::make_unique<mv::Allocator>();
    descriptor_allocator->allocate_pool(*mv_device, 2000);

    /*
      MAT4 UNIFORM LAYOUT
    */
    descriptor_allocator->create_layout(*mv_device, "uniform_layout", vk::DescriptorType::eUniformBuffer, 1,
                                        vk::ShaderStageFlagBits::eVertex, 0);

    /*
      TEXTURE SAMPLER LAYOUT
    */
    descriptor_allocator->create_layout(*mv_device, "sampler_layout", vk::DescriptorType::eCombinedImageSampler, 1,
                                        vk::ShaderStageFlagBits::eFragment, 0);

    /*
      INITIALIZE MODEL/OBJECT HANDLER
      CREATES BUFFERS FOR VIEW & PROJECTION MATRICES
    */
    collection_handler = std::make_unique<Collection>(*mv_device);

    vk::DescriptorSetLayout uniform_layout = descriptor_allocator->get_layout("uniform_layout");
    /*
      VIEW MATRIX UNIFORM OBJECT
    */
    descriptor_allocator->allocate_set(*mv_device, uniform_layout, collection_handler->view_uniform->descriptor);
    descriptor_allocator->update_set(*mv_device, collection_handler->view_uniform->mv_buffer.descriptor,
                                     collection_handler->view_uniform->descriptor, 0);

    /*
      PROJECTION MATRIX UNIFORM OBJECT
    */
    descriptor_allocator->allocate_set(*mv_device, uniform_layout, collection_handler->projection_uniform->descriptor);
    descriptor_allocator->update_set(*mv_device, collection_handler->projection_uniform->mv_buffer.descriptor,
                                     collection_handler->projection_uniform->descriptor, 0);

    /*
      LOAD MODELS
    */
    collection_handler->load_model(*mv_device, *descriptor_allocator, "models/_viking_room.fbx");
    collection_handler->load_model(*mv_device, *descriptor_allocator, "models/Security2.fbx");

    /*
      CREATE OBJECTS
    */
    collection_handler->create_object(*mv_device, *descriptor_allocator, "models/_viking_room.fbx");
    collection_handler->models->at(0).objects->at(0).position = glm::vec3(0.0f, 0.0f, -5.0f);
    collection_handler->models->at(0).objects->at(0).rotation = glm::vec3(0.0f, 90.0f, 0.0f);

    collection_handler->create_object(*mv_device, *descriptor_allocator, "models/Security2.fbx");
    collection_handler->models->at(1).objects->at(0).rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    collection_handler->models->at(1).objects->at(0).position = glm::vec3(0.0f, 0.0f, 0.0f);
    collection_handler->models->at(1).objects->at(0).scale_factor = 0.0125f;

    /*
      CREATE DEBUG POINTS
    */
    Vertex p1;
    p1.position = {0.0f, 0.0f, 0.0f, 1.0f};
    p1.color = {1.0f, 1.0f, 1.0f, 1.0f};
    p1.uv = {0.0f, 0.0f, 0.0f, 0.0f};
    reticle_mesh.vertices = {p1, p1};

    // Create vertex buffer
    mv_device->create_buffer(vk::BufferUsageFlagBits::eVertexBuffer,
                             vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                             reticle_mesh.vertices.size() * sizeof(mv::Vertex), &reticle_mesh.vertex_buffer,
                             &reticle_mesh.vertex_memory, reticle_mesh.vertices.data());

    // Create uniform -- not used tbh
    reticle_obj.position = {0.0f, 0.0f, 0.0f};
    mv_device->create_buffer(vk::BufferUsageFlagBits::eUniformBuffer,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                             &reticle_obj.uniform_buffer, sizeof(mv::Object::matrices));
    reticle_obj.uniform_buffer.map(*mv_device);
    reticle_obj.update();
    // glm::mat4 nm = glm::mat4(1.0);
    // memcpy(reticle_obj.uniform_buffer.mapped, &nm, sizeof(glm::mat4));

    // allocate descriptor for reticle
    descriptor_allocator->allocate_set(*mv_device, uniform_layout, reticle_obj.model_descriptor);
    descriptor_allocator->update_set(*mv_device, reticle_obj.uniform_buffer.descriptor, reticle_obj.model_descriptor,
                                     0);

    /*
      CONFIGURE AND CREATE CAMERA
    */
    camera_init_struct camera_params;
    camera_params.fov = 45.0f * ((float)swapchain->swap_extent.width / swapchain->swap_extent.height);
    camera_params.aspect =
        static_cast<float>(((float)swapchain->swap_extent.height / (float)swapchain->swap_extent.height));
    camera_params.nearz = 0.1f;
    camera_params.farz = 200.0f;
    camera_params.position = glm::vec3(0.0f, 3.0f, -7.0f);
    camera_params.view_uniform_object = collection_handler->view_uniform.get();
    camera_params.projection_uniform_object = collection_handler->projection_uniform.get();

    camera_params.camera_type = Camera::camera_type::third_person;
    camera_params.target = &collection_handler->models->at(1).objects->at(0);

    camera = std::make_unique<Camera>(camera_params);
    return;
}

/*
  GLFW CALLBACKS
*/

void mv::mouse_motion_callback(GLFWwindow *window, double xpos, double ypos)
{
    auto engine = reinterpret_cast<mv::Engine *>(glfwGetWindowUserPointer(window));
    engine->mouse.update(xpos, ypos);
    return;
}

void mv::mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    auto engine = reinterpret_cast<mv::Engine *>(glfwGetWindowUserPointer(window));

    switch (button)
    {
        case GLFW_MOUSE_BUTTON_1: // LMB
            {
                if (action == GLFW_PRESS)
                {
                    engine->mouse.on_left_press();
                }
                else if (action == GLFW_RELEASE)
                {
                    engine->mouse.on_left_release();
                }
                break;
            }
        case GLFW_MOUSE_BUTTON_2: // RMB
            {
                if (action == GLFW_PRESS)
                {
                    engine->mouse.on_right_press();
                }
                else if (action == GLFW_RELEASE)
                {
                    engine->mouse.on_right_release();
                }
                break;
            }
        case GLFW_MOUSE_BUTTON_3: // MMB
            {
                if (action == GLFW_PRESS)
                {
                    engine->mouse.on_middle_press();
                }
                else if (action == GLFW_RELEASE)
                {
                    engine->mouse.on_middle_release();
                }
                break;
            }
    }
    return;
}

void mv::key_callback(GLFWwindow *window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
{
    // get get pointer
    auto engine = reinterpret_cast<mv::Engine *>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
        return;
    }

    if (action == GLFW_PRESS)
    {
        engine->keyboard.on_key_press(key);
    }
    else if (action == GLFW_RELEASE)
    {
        engine->keyboard.on_key_release(key);
    }
    return;
}

void mv::glfw_err_callback(int error, const char *desc)
{
    std::cout << "GLFW Error...\n"
              << "Code => " << error << "\n"
              << "Message => " << desc << "\n";
    return;
}