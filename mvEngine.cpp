#include "mvEngine.h"

mv::Engine::Engine(int w, int h, const char *title)
    : MWindow(w, h, title)
{
    return;
}

void mv::Engine::cleanup_swapchain(void)
{
    // destroy command buffers
    destroy_command_buffers();
    // destroy framebuffers
    if (!frame_buffers.empty())
    {
        for (size_t i = 0; i < frame_buffers.size(); i++)
        {
            if (frame_buffers[i])
            {
                vkDestroyFramebuffer(m_device, frame_buffers[i], nullptr);
                frame_buffers[i] = nullptr;
            }
        }
    }

    cleanup_depth_stencil();

    // destroy pipeline
    if (pipeline)
    {
        vkDestroyPipeline(m_device, pipeline, nullptr);
        pipeline = nullptr;
    }
    if (m_pipeline_cache)
    {
        vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr);
        m_pipeline_cache = nullptr;
    }
    // destroy pipeline layout
    if (pipeline_layout)
    {
        vkDestroyPipelineLayout(m_device, pipeline_layout, nullptr);
        pipeline_layout = nullptr;
    }
    // destroy render pass
    if (m_render_pass)
    {
        vkDestroyRenderPass(m_device, m_render_pass, nullptr);
        m_render_pass = nullptr;
    }
    // descriptors pool
    if (descriptor_pool)
    {
        vkDestroyDescriptorPool(m_device, descriptor_pool, nullptr);
        descriptor_pool = nullptr;
    }
    // cleanup command pool
    if (m_command_pool)
    {
        vkDestroyCommandPool(m_device, m_command_pool, nullptr);
        m_command_pool = nullptr;
    }

    // cleanup swapchain
    swapchain.cleanup(false);
    return;
}

// Allows swapchain to keep up with window resize
void mv::Engine::recreate_swapchain(void)
{
    std::cout << "[+] recreating swapchain" << std::endl;
    vkDeviceWaitIdle(m_device);

    // Cleanup
    cleanup_swapchain();

    m_command_pool = device->create_command_pool(swapchain.queue_index);

    // create swapchain
    swapchain.create(&window_width, &window_height);

    // create descriptor pool, dont recreate layout
    create_descriptor_sets(false);

    // create render pass
    setup_render_pass();

    // create pipeline cache
    create_pipeline_cache();

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
    std::cout << "[+] Preparing vulkan" << std::endl;
    prepare();

    // configure camera before modes/uniform buffers
    camera = std::make_unique<Camera>(45.0f,
                                      static_cast<float>((swapchain.swap_extent.height / swapchain.swap_extent.height)),
                                      0.1f, 100.0f,
                                      glm::vec3(0.0f, 0.0f, 2.0f));

    models.resize(1);
    objects.resize(1); // models * count

    // Prepare uniforms
    prepare_uniforms();

    // Load models
    objects[0].model_index = 0;
    models[0].load(device, "models/car.obj");
    create_descriptor_sets();

    prepare_pipeline();

    // Record commands to already created command buffers(per swap)
    // Create pipeline and tie all resources together
    uint32_t imageIndex = 0;
    size_t currentFrame = 0;

    while (running)
    {
        double fpsdt = timer.getElaspedMS();
        timer.restart();

        while (XPending(display))
        {
            handle_x_event();
        }

        // Get input events
        Keyboard::Event kbdEvent = kbd.read_key();
        Mouse::Event mouseEvent = mouse.read();

        // Handle input events
        if (mouseEvent.is_left_pressed())
        {
            // get delta
            std::pair<int, int> mouse_delta = mouse.get_pos_delta();
            glm::vec3 rotation_delta = glm::vec3(mouse_delta.second, mouse_delta.first, 0.0f);
            camera->rotate(rotation_delta, fpsdt);
        }

        if (kbd.is_key_pressed('w'))
        {
            camera->move_forward(fpsdt);
        }
        if (kbd.is_key_pressed('a'))
        {
            camera->move_left(fpsdt);
        }
        if (kbd.is_key_pressed('s'))
        {
            camera->move_backward(fpsdt);
        }
        if (kbd.is_key_pressed('d'))
        {
            camera->move_right(fpsdt);
        }

        Object::Matrices tm;
        //tm.view = glm::lookAt(camera->get_position(), camera->get_forward_direction(), camera->get_default_up_direction());
        tm.view = camera->matrices.view;
        tm.projection = camera->matrices.perspective;

        for (auto &obj : objects)
        {
            tm.model = obj.matrices.model;
            memcpy(obj.uniform_buffer.mapped, &tm, sizeof(Object::Matrices));
        }

        draw(currentFrame, imageIndex);
    }
    return;
}

void mv::Engine::prepare_uniforms(void)
{
    glm::vec3 rotation = glm::vec3(90.0f, 0.0f, 0.0f);
    glm::mat4 rotation_matrix = glm::mat4(1.0);

    rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0), {0.0f, -0.8f, -10.0f});

    Object::Matrices tm;
    tm.model = translation_matrix * rotation_matrix;
    tm.view = camera->matrices.view;
    tm.projection = camera->matrices.perspective;

    for (auto &obj : objects)
    {
        device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              &obj.uniform_buffer,
                              sizeof(Object::Matrices));
        if (obj.uniform_buffer.map() != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map object uniform buffer");
        }
    }

    for (auto &obj : objects)
    {
        obj.matrices = tm;
        memcpy(obj.uniform_buffer.mapped, &tm, sizeof(Object::Matrices));
    }
    return;
}

void mv::Engine::create_descriptor_sets(bool should_create_layout)
{
    if (should_create_layout)
    {
        // create layout
        VkDescriptorSetLayoutBinding model = {};
        model.binding = 0;
        model.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        model.descriptorCount = 1;
        model.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::vector<VkDescriptorSetLayoutBinding> bindings = {model};

        VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {};
        descriptor_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        descriptor_layout_info.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device->device, &descriptor_layout_info, nullptr, &descriptor_layout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor set layout");
        }
    }

    /*
        model view projection uniform
    */
    VkDescriptorPoolSize objmvp = {};
    objmvp.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    objmvp.descriptorCount = 1 + static_cast<uint32_t>(objects.size());

    std::vector<VkDescriptorPoolSize> pool_sizes = {objmvp};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.pNext = nullptr;
    pool_info.flags = 0;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<uint32_t>(pool_sizes.size()) * objects.size();

    if (vkCreateDescriptorPool(device->device, &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor pool for model");
    }

    // Allocate descriptor sets for objs
    for (auto &obj : objects)
    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &descriptor_layout;
        if (vkAllocateDescriptorSets(device->device, &alloc_info, &obj.descriptor_set) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate descriptor set for object");
        }
        // Now update to bind buffers
        VkWriteDescriptorSet wrm = {};
        wrm.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wrm.dstBinding = 0;
        wrm.dstSet = obj.descriptor_set;
        wrm.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wrm.descriptorCount = 1;
        wrm.pBufferInfo = &obj.uniform_buffer.descriptor;

        std::vector<VkWriteDescriptorSet> writes = {wrm};

        vkUpdateDescriptorSets(device->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    return;
}

void mv::Engine::prepare_pipeline(void)
{
    VkPipelineLayoutCreateInfo pline_info = {};
    pline_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pline_info.pNext = nullptr;
    pline_info.flags = 0;
    pline_info.setLayoutCount = 1;
    pline_info.pSetLayouts = &descriptor_layout;
    // TODO
    // push constants
    if (vkCreatePipelineLayout(device->device, &pline_info, nullptr, &pipeline_layout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    auto binding_description = Vertex::get_binding_description();
    auto attribute_description = Vertex::get_attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vi_state = {};
    vi_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi_state.vertexBindingDescriptionCount = 1;
    vi_state.pVertexBindingDescriptions = &binding_description;
    vi_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_description.size());
    vi_state.pVertexAttributeDescriptions = attribute_description.data();
    VkPipelineInputAssemblyStateCreateInfo ia_state = mv::initializer::input_assembly_state_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rs_state = mv::initializer::rasterization_state_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
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

    // Ensure we have files
    if (vertex_shader.empty() || fragment_shader.empty())
    {
        throw std::runtime_error("Failed to load fragment or vertex shader spv files");
    }

    VkShaderModule vertex_shader_module = create_shader_module(vertex_shader);
    VkShaderModule fragment_shader_module = create_shader_module(fragment_shader);

    // describe vertex shader stage
    VkPipelineShaderStageCreateInfo vertex_shader_stage_info{};
    vertex_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_shader_stage_info.pNext = nullptr;
    vertex_shader_stage_info.flags = 0;
    vertex_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_shader_stage_info.module = vertex_shader_module;
    vertex_shader_stage_info.pName = "main";
    vertex_shader_stage_info.pSpecializationInfo = nullptr;

    // describe fragment shader stage
    VkPipelineShaderStageCreateInfo fragment_shader_stage_info{};
    fragment_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_shader_stage_info.pNext = nullptr;
    fragment_shader_stage_info.flags = 0;
    fragment_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_shader_stage_info.module = fragment_shader_module;
    fragment_shader_stage_info.pName = "main";
    fragment_shader_stage_info.pSpecializationInfo = nullptr;

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {vertex_shader_stage_info, fragment_shader_stage_info};

    VkGraphicsPipelineCreateInfo pipeline_obj_info = mv::initializer::pipeline_create_info(pipeline_layout, m_render_pass);
    pipeline_obj_info.pInputAssemblyState = &ia_state;
    pipeline_obj_info.pRasterizationState = &rs_state;
    pipeline_obj_info.pColorBlendState = &cb_state;
    pipeline_obj_info.pDepthStencilState = &ds_state;
    pipeline_obj_info.pViewportState = &vp_state;
    pipeline_obj_info.pMultisampleState = &ms_state;
    pipeline_obj_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_obj_info.pStages = shader_stages.data();
    pipeline_obj_info.pVertexInputState = &vi_state;
    pipeline_obj_info.pDynamicState = nullptr;

    if (vkCreateGraphicsPipelines(device->device, m_pipeline_cache, 1, &pipeline_obj_info, nullptr, &pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device->device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(device->device, fragment_shader_module, nullptr);
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
    vkCmdBindPipeline(command_buffers[image_index],
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);

    for (const auto &obj : objects)
    {
        models[obj.model_index].bindBuffers(command_buffers[image_index]);
        vkCmdBindDescriptorSets(command_buffers[image_index],
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout,
                                0,
                                1,
                                &objects[0].descriptor_set,
                                0,
                                nullptr);
        // Call model draw
        // Reformat later for instanced drawing of each model type we bind
        //vkCmdDraw(command_buffers[image_index], static_cast<uint32_t>(models[obj.model_index].vertices.count), 1, 0, 0);
        vkCmdDrawIndexed(command_buffers[image_index], static_cast<uint32_t>(models[obj.model_index].indices.count), 1, 0, 0, 0);
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

    cur_frame = (cur_frame + 1) & MAX_IN_FLIGHT;

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

    // TODO
    // case checking

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
}