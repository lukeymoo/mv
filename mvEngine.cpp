#include "mvEngine.h"

// void mv::Engine::cleanup_swapchain(void)
// {
//     // destroy command buffers
//     destroy_command_buffers();
//     // destroy framebuffers
//     if (!frame_buffers.empty())
//     {
//         for (size_t i = 0; i < frame_buffers.size(); i++)
//         {
//             if (frame_buffers[i])
//             {
//                 vkDestroyFramebuffer(m_device, frame_buffers[i], nullptr);
//                 frame_buffers[i] = nullptr;
//             }
//         }
//     }

//     cleanup_depth_stencil();

//     // destroy pipelines
//     if (pipeline_w_sampler)
//     {
//         vkDestroyPipeline(m_device, pipeline_w_sampler, nullptr);
//         pipeline_w_sampler = nullptr;
//     }
//     if (pipeline_no_sampler)
//     {
//         vkDestroyPipeline(m_device, pipeline_no_sampler, nullptr);
//         pipeline_no_sampler = nullptr;
//     }

//     // pipeline cache
//     if (m_pipeline_cache)
//     {
//         vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr);
//         m_pipeline_cache = nullptr;
//     }

//     // pipeline layouts
//     if (pipeline_layout_w_sampler)
//     {
//         vkDestroyPipelineLayout(m_device, pipeline_layout_w_sampler, nullptr);
//         pipeline_layout_w_sampler = nullptr;
//     }
//     if (pipeline_layout_no_sampler)
//     {
//         vkDestroyPipelineLayout(m_device, pipeline_layout_no_sampler, nullptr);
//         pipeline_layout_no_sampler = nullptr;
//     }

//     // destroy render pass
//     if (m_render_pass)
//     {
//         vkDestroyRenderPass(m_device, m_render_pass, nullptr);
//         m_render_pass = nullptr;
//     }
//     // descriptors pool
//     // if (descriptor_pool)
//     // {
//     //     vkDestroyDescriptorPool(m_device, descriptor_pool, nullptr);
//     //     descriptor_pool = nullptr;
//     // }
//     // cleanup command pool
//     if (m_command_pool)
//     {
//         vkDestroyCommandPool(m_device, m_command_pool, nullptr);
//         m_command_pool = nullptr;
//     }

//     // cleanup swapchain
//     swapchain.cleanup(false);

//     return;
// }

// Allows swapchain to keep up with window resize
// void mv::Engine::recreate_swapchain(void)
// {
//     std::cout << "[+] recreating swapchain" << std::endl;
//     vkDeviceWaitIdle(m_device);

//     // Cleanup
//     cleanup_swapchain();

//     m_command_pool = device->create_command_pool(swapchain.queue_index);

//     // create swapchain
//     swapchain.create(&window_width, &window_height);

//     // call after swapchain creation!!!
//     // needed for mouse delta calc when using from_center method
//     mouse->set_window_properties(window_width, window_height);

//     // create render pass
//     setup_render_pass();

//     // create pipeline cache
//     create_pipeline_cache();

//     setup_depth_stencil();

//     // create pipeline layout
//     // create pipeline
//     prepare_pipeline();
//     // create framebuffers
//     setup_framebuffer();
//     // create command buffers
//     create_command_buffers();
// }

void mv::Engine::go(void)
{
    std::cout << "[+] Preparing vulkan" << std::endl;
    prepare();

    // Create descriptor pool allocator
    descriptor_allocator = std::make_shared<Allocator>(std::weak_ptr<mv::Device>(mv_device));

    // allocate pool
    descriptor_allocator->allocate_pool(2000);

    // create uniform buffer layout ( single mat4 object )
    descriptor_allocator->create_layout("uniform_layout",
                                        vk::DescriptorType::eUniformBuffer,
                                        1,
                                        vk::ShaderStageFlagBits::eVertex,
                                        0);

    // create texture sampler layout
    descriptor_allocator->create_layout("sampler_layout",
                                        vk::DescriptorType::eCombinedImageSampler,
                                        1,
                                        vk::ShaderStageFlagBits::eFragment,
                                        0);

    // initialize model/object container
    // by default it contains uniform buffer for view & projection matrices
    collection_handler = std::make_unique<Collection>(std::weak_ptr<mv::Device>(mv_device),
                                                      std::weak_ptr<mv::Allocator>(descriptor_allocator));

    // get uniform layout to create descriptors for view & projection matrix ubos
    vk::DescriptorSetLayout uniform_layout = descriptor_allocator->get_layout("uniform_layout");

    // allocate & update descriptor set for view uniform buffer
    descriptor_allocator->allocate_set(uniform_layout,
                                       collection_handler->view_uniform.descriptor);
    descriptor_allocator->update_set(collection_handler->view_uniform.mv_buffer.descriptor,
                                     collection_handler->view_uniform.descriptor,
                                     0);

    // allocate & update descriptor set for projection uniform buffer
    descriptor_allocator->allocate_set(uniform_layout,
                                       collection_handler->projection_uniform.descriptor);
    descriptor_allocator->update_set(collection_handler->projection_uniform.mv_buffer.descriptor,
                                     collection_handler->projection_uniform.descriptor,
                                     0);

    // Load model
    collection_handler->load_model("models/_viking_room.fbx");
    collection_handler->load_model("models/Male.obj");

    // Create object
    collection_handler->create_object("models/_viking_room.fbx");
    collection_handler->models[0].objects[0]->position = glm::vec3(0.0f, 0.0f, -5.0f);
    collection_handler->models[0].objects[0]->rotation = glm::vec3(0.0f, 90.0f, 0.0f);

    collection_handler->create_object("models/Male.obj");
    collection_handler->models[1].objects[0]->rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    collection_handler->models[1].objects[0]->position = glm::vec3(0.0f, 0.0f, 0.0f);

    // configure camera before uniform buffer creation
    camera_init_struct camera_params;
    camera_params.fov = 45.0f * ((float)swapchain.swap_extent.width / swapchain.swap_extent.height);
    camera_params.aspect = static_cast<float>(((float)swapchain.swap_extent.height / (float)swapchain.swap_extent.height));
    camera_params.nearz = 0.1f;
    camera_params.farz = 200.0f;
    camera_params.position = glm::vec3(0.0f, 3.0f, -7.0f);
    camera_params.view_uniform_object = &collection_handler->view_uniform;
    camera_params.projection_uniform_object = &collection_handler->projection_uniform;

    camera_params.camera_type = Camera::camera_type::third_person;
    camera_params.target = collection_handler->models.at(1).objects.at(0).get();

    camera = std::make_unique<Camera>(camera_params);

    if (camera->type == Camera::camera_type::first_person || Camera::camera_type::free_look)
    {
        mouse->set_delta_style(mouse::delta_calc_style::from_center);
    }
    else
    {
        mouse->set_delta_style(mouse::delta_calc_style::from_last);
    }

    prepare_pipeline();

    // basic check
    assert(camera);
    assert(collection_handler);
    assert(descriptor_allocator);

    uint32_t image_index = 0;
    size_t current_frame = 0;

    bool added = false;

    fps.startTimer();
    [[maybe_unused]] int fps_counter = 0;

    using chrono = std::chrono::high_resolution_clock;

    constexpr std::chrono::nanoseconds timestep(16ms);

    std::chrono::nanoseconds accumulated(0ns);
    auto start_time = chrono::now();
    while (running)
    {
        auto delta_time = chrono::now() - start_time;
        start_time = chrono::now();
        accumulated += std::chrono::duration_cast<std::chrono::nanoseconds>(delta_time);

        while (xcb_generic_event_t *evt = xcb_poll_for_event(xcb_conn.get()))
        {
            handle_x_event(evt);
        }

        while (accumulated >= timestep)
        {
            accumulated -= timestep;

            // Get input events
            mv::keyboard::event kbd_event = kbd->read();
            mv::mouse::event mouse_event = mouse->read();

            if (camera->type == Camera::camera_type::third_person)
            {
                // handle mouse scroll wheel
                if (mouse_event.type == mouse::event::etype::wheel_up)
                {
                    camera->adjust_zoom(-(camera->zoom_step));
                }
                if (mouse_event.type == mouse::event::etype::wheel_down)
                {
                    camera->adjust_zoom(camera->zoom_step);
                }

                // start mouse drag
                if (mouse_event.type == mv::mouse::event::etype::r_down &&
                    !mouse->is_dragging)
                {
                    // XDefineCursor(display, window, mouse->hidden_cursor);
                    mouse->start_drag(camera->orbit_angle, camera->pitch);
                }
                // end drag
                if (mouse_event.type == mv::mouse::event::etype::r_release &&
                    mouse->is_dragging)
                {
                    // XUndefineCursor(display, window);
                    camera->realign_orbit();
                    mouse->end_drag();
                }

                // if drag enabled check for release
                if (mouse->is_dragging)
                {
                    // camera orbit
                    if (mouse->drag_delta_x > 0)
                    {
                        camera->adjust_orbit(-abs(mouse->drag_delta_x), mouse->stored_orbit);
                    }
                    else if (mouse->drag_delta_x < 0)
                    {
                        camera->adjust_orbit(abs(mouse->drag_delta_x), mouse->stored_orbit);
                    }

                    // camera pitch
                    if (mouse->drag_delta_y > 0)
                    {
                        camera->adjust_pitch(abs(mouse->drag_delta_y) * 0.25f, mouse->stored_pitch);
                    }
                    else if (mouse->drag_delta_y < 0)
                    {
                        camera->adjust_pitch(-abs(mouse->drag_delta_y) * 0.25f, mouse->stored_pitch);
                    }

                    // fetch new orbit & pitch
                    mouse->stored_orbit = camera->orbit_angle;
                    mouse->stored_pitch = camera->pitch;
                    // hide pointer & warp to drag start
                    // XIWarpPointer(display, mouse->deviceid, None, window, 0, 0, 0, 0, mouse->drag_startx, mouse->drag_starty);
                    // XFlush(display);
                    mouse->clear();
                    camera->target->rotate_to_face(camera->orbit_angle);
                }

                // sort movement by key combination

                // forward only
                if (kbd->is_keystate(mv::keyboard::key::w) &&
                    !kbd->is_keystate(mv::keyboard::key::d) &&
                    !kbd->is_keystate(mv::keyboard::key::a) &&
                    !kbd->is_keystate(mv::keyboard::key::s))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + right
                if (kbd->is_keystate(mv::keyboard::key::w) &&
                    kbd->is_keystate(mv::keyboard::key::d) &&
                    !kbd->is_keystate(mv::keyboard::key::a) &&
                    !kbd->is_keystate(mv::keyboard::key::s))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(1.0f, 0.0f, -1.0f, 1.0f));
                }

                // forward + left
                if (kbd->is_keystate(mv::keyboard::key::w) &&
                    !kbd->is_keystate(mv::keyboard::key::d) &&
                    kbd->is_keystate(mv::keyboard::key::a) &&
                    !kbd->is_keystate(mv::keyboard::key::s))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(-1.0f, 0.0f, -1.0f, 1.0f));
                }

                // backward only
                if (!kbd->is_keystate(mv::keyboard::key::w) &&
                    !kbd->is_keystate(mv::keyboard::key::d) &&
                    !kbd->is_keystate(mv::keyboard::key::a) &&
                    kbd->is_keystate(mv::keyboard::key::s))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + left
                if (!kbd->is_keystate(mv::keyboard::key::w) &&
                    !kbd->is_keystate(mv::keyboard::key::d) &&
                    kbd->is_keystate(mv::keyboard::key::a) &&
                    kbd->is_keystate(mv::keyboard::key::s))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(-1.0f, 0.0f, 1.0f, 1.0f));
                }

                // backward + right
                if (!kbd->is_keystate(mv::keyboard::key::w) &&
                    kbd->is_keystate(mv::keyboard::key::d) &&
                    !kbd->is_keystate(mv::keyboard::key::a) &&
                    kbd->is_keystate(mv::keyboard::key::s))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
                }

                // left only
                if (!kbd->is_keystate(mv::keyboard::key::w) &&
                    !kbd->is_keystate(mv::keyboard::key::d) &&
                    kbd->is_keystate(mv::keyboard::key::a) &&
                    !kbd->is_keystate(mv::keyboard::key::s))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f));
                }

                // right only
                if (!kbd->is_keystate(mv::keyboard::key::w) &&
                    kbd->is_keystate(mv::keyboard::key::d) &&
                    !kbd->is_keystate(mv::keyboard::key::a) &&
                    !kbd->is_keystate(mv::keyboard::key::s))
                {
                    camera->target->move(camera->orbit_angle, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                }

                // debug -- add new objects to world with random position
                if (kbd->is_keystate(mv::keyboard::key::space) && added == false)
                {
                    //added = true;
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
                    collection_handler->create_object("models/_viking_room.fbx");
                    collection_handler->models.at(0).objects.back()->position =
                        glm::vec3(x, y, z);
                }
            }
            // update game objects
            collection_handler->update();

            // update view and projection matrices
            camera->update();
        }

        // Render
        draw(current_frame, image_index);
    }
    int total_object_count = 0;
    for (const auto &model : collection_handler->models)
    {
        total_object_count += model.objects.size();
    }
    std::cout << "Total objects => " << total_object_count << std::endl;
    return;
}

void mv::Engine::prepare_pipeline(void)
{
    vk::DescriptorSetLayout uniform_layout = descriptor_allocator->get_layout("uniform_layout");
    vk::DescriptorSetLayout sampler_layout = descriptor_allocator->get_layout("sampler_layout");

    std::vector<vk::DescriptorSetLayout> layout_w_sampler = {uniform_layout,
                                                             uniform_layout,
                                                             uniform_layout,
                                                             sampler_layout};
    std::vector<vk::DescriptorSetLayout> layout_no_sampler = {uniform_layout,
                                                              uniform_layout,
                                                              uniform_layout};

    // Pipeline for models with textures
    vk::PipelineLayoutCreateInfo w_sampler_info;
    w_sampler_info.setLayoutCount = static_cast<uint32_t>(layout_w_sampler.size());
    w_sampler_info.pSetLayouts = layout_w_sampler.data();

    // Pipeline with no textures
    vk::PipelineLayoutCreateInfo no_sampler_info;
    no_sampler_info.setLayoutCount = static_cast<uint32_t>(layout_no_sampler.size());
    no_sampler_info.pSetLayouts = layout_no_sampler.data();

    pipeline_layouts->insert(
        std::pair<std::string, vk::PipelineLayout>("sampler",
                                                   mv_device->logical_device->createPipelineLayout(w_sampler_info)));

    pipeline_layouts->insert(
        std::pair<std::string, vk::PipelineLayout>("no_sampler",
                                                   mv_device->logical_device->createPipelineLayout(no_sampler_info)));

    auto binding_description = Vertex::get_binding_description();
    auto attribute_description = Vertex::get_attribute_descriptions();

    vk::PipelineVertexInputStateCreateInfo vi_state;
    vi_state.vertexBindingDescriptionCount = 1;
    vi_state.pVertexBindingDescriptions = &binding_description;
    vi_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_description.size());
    vi_state.pVertexAttributeDescriptions = attribute_description.data();
    VkPipelineInputAssemblyStateCreateInfo ia_state = mv::initializer::input_assembly_state_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rs_state = mv::initializer::rasterization_state_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState cba_state = mv::initializer::color_blend_attachment_state(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo cb_state = mv::initializer::color_blend_state_info(1, &cba_state);
    VkPipelineDepthStencilStateCreateInfo ds_state = mv::initializer::depth_stencil_state_info(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkViewport vp = {};
    VkRect2D sc = {};
    vp.x = 0;
    vp.y = window_height;
    vp.width = static_cast<float>(window_width);
    vp.height = -static_cast<float>(window_height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    sc.offset = {0, 0};
    sc.extent = swapchain.swap_extent;

    VkPipelineViewportStateCreateInfo vp_state = mv::initializer::viewport_state_info(1, 1, 0);
    vp_state.pViewports = &vp;
    vp_state.pScissors = &sc;
    VkPipelineMultisampleStateCreateInfo ms_state = mv::initializer::multisample_state_info(VK_SAMPLE_COUNT_1_BIT, 0);

    // Load shaders
    auto vertex_shader = read_file("shaders/vertex.spv");
    auto fragment_shader = read_file("shaders/fragment.spv");
    auto fragment_shader_no_sampler = read_file("shaders/fragment_no_sampler.spv");

    // Ensure we have files
    if (vertex_shader.empty() || fragment_shader.empty() || fragment_shader_no_sampler.empty())
    {
        throw std::runtime_error("Failed to load fragment or vertex shader spv files");
    }

    VkShaderModule vertex_shader_module = create_shader_module(vertex_shader);
    VkShaderModule fragment_shader_module = create_shader_module(fragment_shader);
    VkShaderModule fragment_shader_no_sampler_module = create_shader_module(fragment_shader_no_sampler);

    // describe vertex shader stage
    VkPipelineShaderStageCreateInfo vertex_shader_stage_info{};
    vertex_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_shader_stage_info.pNext = nullptr;
    vertex_shader_stage_info.flags = 0;
    vertex_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_shader_stage_info.module = vertex_shader_module;
    vertex_shader_stage_info.pName = "main";
    vertex_shader_stage_info.pSpecializationInfo = nullptr;

    // describe fragment shader stage WITH sampler
    VkPipelineShaderStageCreateInfo fragment_shader_stage_info{};
    fragment_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_shader_stage_info.pNext = nullptr;
    fragment_shader_stage_info.flags = 0;
    fragment_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_shader_stage_info.module = fragment_shader_module;
    fragment_shader_stage_info.pName = "main";
    fragment_shader_stage_info.pSpecializationInfo = nullptr;

    // fragment shader stage NO sampler
    VkPipelineShaderStageCreateInfo fragment_shader_stage_no_sampler_info{};
    fragment_shader_stage_no_sampler_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_shader_stage_no_sampler_info.pNext = nullptr;
    fragment_shader_stage_no_sampler_info.flags = 0;
    fragment_shader_stage_no_sampler_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_shader_stage_no_sampler_info.module = fragment_shader_no_sampler_module;
    fragment_shader_stage_no_sampler_info.pName = "main";
    fragment_shader_stage_no_sampler_info.pSpecializationInfo = nullptr;

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {vertex_shader_stage_info, fragment_shader_stage_info};
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages_no_sampler = {vertex_shader_stage_info, fragment_shader_stage_no_sampler_info};

    // create pipeline WITH sampler
    VkGraphicsPipelineCreateInfo pipeline_w_sampler_info = mv::initializer::pipeline_create_info(pipeline_layout_w_sampler, m_render_pass);
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

    if (vkCreateGraphicsPipelines(device->device, m_pipeline_cache, 1, &pipeline_w_sampler_info, nullptr, &pipeline_w_sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline with sampler");
    }

    // Create pipeline with NO sampler
    VkGraphicsPipelineCreateInfo pipeline_no_sampler_info = mv::initializer::pipeline_create_info(pipeline_layout_no_sampler, m_render_pass);
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

    if (vkCreateGraphicsPipelines(device->device, nullptr, 1, &pipeline_no_sampler_info, nullptr, &pipeline_no_sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline with no sampler");
    }

    vkDestroyShaderModule(device->device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(device->device, fragment_shader_module, nullptr);
    vkDestroyShaderModule(device->device, fragment_shader_no_sampler_module, nullptr);
    return;
}

void mv::Engine::record_command_buffer(uint32_t image_index)
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffers[image_index], &begin_info) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to begin recording to command buffer");
    }

    std::array<VkClearValue, 2> cls;
    cls[0].color = default_clear_color;
    cls[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo pass_info = {};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass_info.pNext = nullptr;
    pass_info.renderPass = m_render_pass;
    pass_info.framebuffer = frame_buffers[image_index];
    pass_info.renderArea.offset.x = 0;
    pass_info.renderArea.offset.y = 0;
    pass_info.renderArea.extent.width = swapchain.swap_extent.width;
    pass_info.renderArea.extent.height = swapchain.swap_extent.height;
    pass_info.clearValueCount = static_cast<uint32_t>(cls.size());
    pass_info.pClearValues = cls.data();

    vkCmdBeginRenderPass(command_buffers[image_index], &pass_info, VK_SUBPASS_CONTENTS_INLINE);

    for (auto &model : collection_handler->models)
    {
        // for each model select the appropriate pipeline
        if (model.has_texture)
        {
            vkCmdBindPipeline(command_buffers[image_index],
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline_w_sampler);
        }
        else
        {
            vkCmdBindPipeline(command_buffers[image_index],
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline_no_sampler);
        }
        for (auto &object : model.objects)
        {
            for (auto &mesh : model._meshes)
            {
                std::vector<VkDescriptorSet> to_bind = {
                    object->model_descriptor,
                    collection_handler->view_uniform.descriptor,
                    collection_handler->projection_uniform.descriptor};
                if (model.has_texture)
                {
                    to_bind.push_back(mesh.textures[0].descriptor);
                    vkCmdBindDescriptorSets(command_buffers[image_index],
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipeline_layout_w_sampler,
                                            0,
                                            static_cast<uint32_t>(to_bind.size()),
                                            to_bind.data(),
                                            0,
                                            nullptr);
                }
                else
                {
                    vkCmdBindDescriptorSets(command_buffers[image_index],
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipeline_layout_no_sampler,
                                            0,
                                            static_cast<uint32_t>(to_bind.size()),
                                            to_bind.data(),
                                            0,
                                            nullptr);
                }
                // Bind mesh buffers
                mesh.bindBuffers(command_buffers[image_index]);

                // Indexed draw
                vkCmdDrawIndexed(command_buffers[image_index],
                                 static_cast<uint32_t>(mesh.indices.size()),
                                 1, 0, 0, 0);
            }
        }
    }

    vkCmdEndRenderPass(command_buffers[image_index]);

    if (vkEndCommandBuffer(command_buffers[image_index]) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to end command buffer recording");
    }

    return;
}

void mv::Engine::draw(size_t &cur_frame, uint32_t &cur_index)
{
    vkWaitForFences(device->device, 1, &in_flight_fences[cur_frame], VK_TRUE, UINT64_MAX);

    VkResult result;
    result = vkAcquireNextImageKHR(device->device, swapchain.swapchain, UINT64_MAX, semaphores.present_complete, VK_NULL_HANDLE, &cur_index);
    switch (result)
    {
    case VK_ERROR_OUT_OF_DATE_KHR:
        recreate_swapchain();
        return;
        break;
    case VK_SUBOPTIMAL_KHR:
        std::cout << "[/] Swapchain is no longer optimal : not recreating" << std::endl;
        break;
    case VK_SUCCESS:
        break;
    default: // unhandled error occurred
        throw std::runtime_error("Unhandled exception while acquiring a swapchain image for rendering");
        break;
    }

    if (wait_fences[cur_index] != VK_NULL_HANDLE)
    {
        vkWaitForFences(device->device, 1, &wait_fences[cur_index], VK_TRUE, UINT64_MAX);
    }

    record_command_buffer(cur_index);

    // mark in use
    wait_fences[cur_index] = in_flight_fences[cur_frame];

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = {semaphores.present_complete};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = waitSemaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[cur_index];
    VkSemaphore render_semaphores[] = {semaphores.render_complete};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = render_semaphores;

    vkResetFences(device->device, 1, &in_flight_fences[cur_frame]);

    result = vkQueueSubmit(m_graphics_queue, 1, &submit_info, in_flight_fences[cur_frame]);
    switch (result)
    {
    case VK_ERROR_OUT_OF_DATE_KHR:
        recreate_swapchain();
        return;
        break;
    case VK_SUBOPTIMAL_KHR:
        std::cout << "[/] Swapchain is no longer optimal : not recreating" << std::endl;
        break;
    case VK_SUCCESS:
        break;
    default: // unhandled error occurred
        throw std::runtime_error("Unhandled exception while acquiring a swapchain image for rendering");
        break;
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = nullptr;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = render_semaphores;
    VkSwapchainKHR swapchains[] = {swapchain.swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &cur_index;
    present_info.pResults = nullptr;

    result = vkQueuePresentKHR(m_graphics_queue, &present_info);
    switch (result)
    {
    case VK_ERROR_OUT_OF_DATE_KHR:
        recreate_swapchain();
        return;
        break;
    case VK_SUBOPTIMAL_KHR:
        std::cout << "[/] Swapchain is no longer optimal : not recreating" << std::endl;
        break;
    case VK_SUCCESS:
        break;
    default: // unhandled error occurred
        throw std::runtime_error("Unhandled exception while acquiring a swapchain image for rendering");
        break;
    }
    cur_frame = (cur_frame + 1) % MAX_IN_FLIGHT;
}