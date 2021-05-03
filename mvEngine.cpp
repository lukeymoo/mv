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

    // destroy pipelines
    if (pipeline_w_sampler)
    {
        vkDestroyPipeline(m_device, pipeline_w_sampler, nullptr);
        pipeline_w_sampler = nullptr;
    }
    if (pipeline_no_sampler)
    {
        vkDestroyPipeline(m_device, pipeline_no_sampler, nullptr);
        pipeline_no_sampler = nullptr;
    }

    // pipeline cache
    if (m_pipeline_cache)
    {
        vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr);
        m_pipeline_cache = nullptr;
    }

    // pipeline layouts
    if (pipeline_layout_w_sampler)
    {
        vkDestroyPipelineLayout(m_device, pipeline_layout_w_sampler, nullptr);
        pipeline_layout_w_sampler = nullptr;
    }
    if (pipeline_layout_no_sampler)
    {
        vkDestroyPipelineLayout(m_device, pipeline_layout_no_sampler, nullptr);
        pipeline_layout_no_sampler = nullptr;
    }

    // destroy render pass
    if (m_render_pass)
    {
        vkDestroyRenderPass(m_device, m_render_pass, nullptr);
        m_render_pass = nullptr;
    }
    // descriptors pool
    // if (descriptor_pool)
    // {
    //     vkDestroyDescriptorPool(m_device, descriptor_pool, nullptr);
    //     descriptor_pool = nullptr;
    // }
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

    mouse.update_window_spec(window_width, window_height);

    // create descriptor pool, dont recreate layout
    //create_descriptor_sets(&global_uniforms);

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

    // Create descriptor pool allocator
    descriptor_allocator = std::make_shared<Allocator>(device);

    // allocate pool
    descriptor_allocator->allocate_pool(2000);

    // create uniform buffer layout ( single mat4 object )
    descriptor_allocator->create_layout("uniform_layout",
                                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        1,
                                        VK_SHADER_STAGE_VERTEX_BIT,
                                        0);

    // create texture sampler layout
    descriptor_allocator->create_layout("sampler_layout",
                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        1,
                                        VK_SHADER_STAGE_FRAGMENT_BIT,
                                        0);

    // initialize model/object container
    // constructor by default creates `uniform_layout` for use with view & projection matrix buffers
    collection_handler = std::make_unique<Collection>(device, descriptor_allocator);

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
    camera_params.fov = 40.0f * ((float)swapchain.swap_extent.width / swapchain.swap_extent.height);
    camera_params.aspect = static_cast<float>(((float)swapchain.swap_extent.height / (float)swapchain.swap_extent.height));
    camera_params.nearz = 0.01f;
    camera_params.farz = 200.0f;
    camera_params.position = glm::vec3(0.0f, 3.0f, -7.0f);
    camera_params.view_uniform_object = &collection_handler->view_uniform;
    camera_params.projection_uniform_object = &collection_handler->projection_uniform;

    camera_params.camera_type = Camera::camera_type::third_person;
    camera_params.target = collection_handler->models.at(1).objects.at(0).get();

    camera = std::make_unique<Camera>(camera_params);

    prepare_pipeline();

    // Record commands to already created command buffers(per swap)
    // Create pipeline and tie all resources together
    uint32_t image_index = 0;
    size_t current_frame = 0;
    bool added = false;
    fps.startTimer();
    int fps_counter = 0;

    // points to either the last frame delta or the average if one has been calculated
    float *delta_to_use;

    // variables used to calculated average frame delta from last 3 frames
    float average_frame_delta = 0;
    uint32_t last_frame_index = 0;
    std::vector<float> last_frames_list(3, 0.0f);

    while (running)
    {
        assert(camera);
        assert(collection_handler);
        assert(descriptor_allocator);

        float fpsdt = timer.getElaspedMS();
        timer.restart();

        // if last_frames_list has been filled & an average calculated
        // use average_frame_delta for input functions
        if (last_frames_list[0] > 0.0f && last_frames_list[1] > 0.0f && last_frames_list[2] > 0.0f)
        {
            delta_to_use = &average_frame_delta;
        }
        else
        {
            delta_to_use = &fpsdt;
        }

        while (XPending(display))
        {
            handle_x_event();
        }
        if (camera->type == Camera::camera_type::free_look || camera->type == Camera::camera_type::first_person)
        {
            XWarpPointer(display, None, window, 0, 0, 0, 0, (window_width / 2), (window_height / 2));
            XFlush(display);
        }
        else // third person do not lock mouse pointer
        {
        }

        // use fpsdt until we have an average

        // Get input events
        Keyboard::Event kbd_event = kbd.read_key();
        Mouse::Event mouse_event = mouse.read();
        std::pair<int, int> i_mouse_delta = mouse.get_pos_delta();
        std::pair<float, float> mouse_delta;
        mouse_delta.first = (float)i_mouse_delta.first;
        mouse_delta.second = (float)i_mouse_delta.second;

        // Handle input events

        /*
            -----------------------------
            free look input tree
            -----------------------------
        */
        if (camera->type == Camera::camera_type::free_look)
        {
            mouse_delta.first *= 0.9f;
            mouse_delta.second *= 0.9f;
            if (mouse_event.get_type() == Mouse::Event::Type::Move)
            {
                glm::vec3 rotation_delta = glm::vec3(-(mouse_delta.second), mouse_delta.first, 0.0f);
                camera->rotate(rotation_delta, delta_to_use);
            }
            if (kbd.is_key_pressed(' ')) // space
            {
                camera->move_up(delta_to_use);
            }
            if (kbd.is_key_pressed(65507)) // ctrl key
            {
                camera->move_down(delta_to_use);
            }

            // lateral movements
            if (kbd.is_key_pressed('w'))
            {
                camera->move_forward(delta_to_use);
            }
            if (kbd.is_key_pressed('a'))
            {
                camera->move_left(delta_to_use);
            }
            if (kbd.is_key_pressed('s'))
            {
                camera->move_backward(delta_to_use);
            }
            if (kbd.is_key_pressed('d'))
            {
                camera->move_right(delta_to_use);
            }
        }
        /*
            -----------------------------
            first person input tree
            -----------------------------
        */
        else if (camera->type == Camera::camera_type::first_person)
        {
        }
        /*
            -----------------------------
            third person input tree
            -----------------------------
        */
        else if (camera->type == Camera::camera_type::third_person)
        {
            mouse_delta.first *= 0.5f;
            mouse_delta.second *= 0.5f;
            if (mouse_event.get_type() == Mouse::Event::Type::Move)
            {
                // handle mouse rotation
                if (mouse.is_middle_pressed() && mouse.is_in_window())
                {
                    if (mouse_delta.first < 0)
                    {
                        camera->increase_orbit(fabs(mouse_delta.first), delta_to_use);
                    }
                    else if (mouse_delta.first > 0)
                    {
                        camera->decrease_orbit(fabs(mouse_delta.first), delta_to_use);
                    }
                }
            }
            // handle mouse scroll wheel
            if (mouse_event.get_type() == Mouse::Event::Type::WheelUp)
            {
                camera->decrease_pitch(2.5f, delta_to_use);
            }
            if (mouse_event.get_type() == Mouse::Event::Type::WheelDown)
            {
                camera->increase_pitch(2.5f, delta_to_use);
            }



            if (kbd_event.get_type() == Keyboard::Event::Type::Press)
            {
                if (kbd_event.get_code() == ' ' && added == false)
                {
                    //added = true;
                    collection_handler->create_object("models/Male.obj");
                }
            }

            // TODO
            // change object movement to require float *
            if (kbd.is_key_pressed('i'))
            {
                camera->target->move_forward(fpsdt);
            }
            if (kbd.is_key_pressed('k'))
            {
                camera->target->move_backward(fpsdt);
            }
            if (kbd.is_key_pressed('j'))
            {
                camera->target->move_left(fpsdt);
            }
            if (kbd.is_key_pressed('l'))
            {
                camera->target->move_right(fpsdt);
            }

            if (kbd.is_key_pressed('w'))
            {
                camera->decrease_pitch(delta_to_use);
            }
            if (kbd.is_key_pressed('s'))
            {
                camera->increase_pitch(delta_to_use);
            }
            if (kbd.is_key_pressed('a'))
            {
                // change camera orbit angle
                camera->decrease_orbit(delta_to_use); // counter clockwise rotation
            }
            if (kbd.is_key_pressed('d'))
            {
                // change camera orbit angle
                camera->increase_orbit(delta_to_use); // clockwise rotation
            }
        }

        /*
            -----------------------------
            update objects
            -----------------------------
        */
        collection_handler->update();

        /*
            --------------------------------------
            update view matrix & if necessary proj
            --------------------------------------
        */
        camera->update();

        // Render
        draw(current_frame, image_index);

        fps_counter += 1;

        // use last 3 frames
        if (last_frame_index > 2)
        {
            std::cout << "Last 3 Delta => " << last_frames_list[0] << ", " << last_frames_list[1] << ", " << last_frames_list[2] << std::endl;
            // on third frame calculate average
            average_frame_delta = last_frames_list[0] + last_frames_list[1] + last_frames_list[2];
            average_frame_delta = average_frame_delta / 3;
            // reset index
            last_frame_index = 0;
            std::cout << "Average Frame Delta => " << average_frame_delta << std::endl;
        }
        else
        {
            last_frames_list.at(last_frame_index) = timer.getElaspedMS();
            last_frame_index += 1;
        }
        if (fps.getElaspedMS() > 1000)
        {
            std::cout << "FPS => " << fps_counter << std::endl;
            fps_counter = 0;
            fps.restart();
        }
    }
    return;
}

// void mv::Engine::add_new_model(mv::Allocator::Container *pool, const char *filename)
// {
//     // resize models
//     models.push_back(mv::Model());
//     // add object to model
//     models[(models.size() - 1)].resize_object_container(1);

//     // iterate models
//     for (size_t i = 0; i < models.size(); i++)
//     {
//         // iterate objects assign model indices
//         for (size_t j = 0; j < models[i].objects.size(); j++)
//         {
//             models[i].objects[j].model_index = i;
//         }
//     }

//     // Generates random xyz values within supplied min&max below
//     // hardcoded for testing purposes
//     int min = 0;
//     int max = 30;
//     int z_max = 30;

//     std::random_device rd;
//     std::default_random_engine eng(rd());
//     std::uniform_int_distribution<int> xy_distr(min, max);
//     std::uniform_int_distribution<int> z_distr(min, z_max);

//     float x = xy_distr(eng);
//     float y = xy_distr(eng);
//     float z = z_distr(eng);
//     models[(models.size() - 1)].objects[0].position = glm::vec3(x, y, -z);
//     models[(models.size() - 1)].objects[0].rotation = glm::vec3(180.0f, 0.0f, 0.0f);

//     // prepare descriptor for model
//     device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//                           &models[(models.size() - 1)].objects[0].uniform_buffer,
//                           sizeof(Object::Matrices));
//     if (models[(models.size() - 1)].objects[0].uniform_buffer.map() != VK_SUCCESS)
//     {
//         throw std::runtime_error("Failed to allocate buffer for newly created model");
//     }

//     // Load model
//     //models[(models.size() - 1)].load(device, filename);

//     // allocate descriptor set for object model matrix data
//     descriptor_allocator->allocate_set(pool, uniform_layout, models[(models.size() - 1)].objects[0].model_descriptor);
//     // allocate descriptor set for object texture data
//     descriptor_allocator->allocate_set(pool, sampler_layout, models[(models.size() - 1)].objects[0].texture_descriptor);

//     // bind object matrix data buffer & its descriptor
//     descriptor_allocator->update_set(pool, models[(models.size() - 1)].objects[0].uniform_buffer.descriptor,
//                                      models[(models.size() - 1)].objects[0].model_descriptor,
//                                      0);
//     descriptor_allocator->update_set(pool, models[(models.size() - 1)].image.descriptor,
//                                      models[(models.size() - 1)].objects[0].texture_descriptor,
//                                      0);

//     return;
// }

void mv::Engine::create_descriptor_layout(VkDescriptorType type, uint32_t count, VkPipelineStageFlags stage_flags, uint32_t binding, VkDescriptorSetLayout &layout)
{
    VkDescriptorSetLayoutBinding bind_info = {};
    bind_info.binding = binding;
    bind_info.descriptorType = type;
    bind_info.descriptorCount = count;
    bind_info.stageFlags = stage_flags;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &bind_info;

    // layout for model matrix
    if (vkCreateDescriptorSetLayout(device->device, &layout_info, nullptr, &layout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

void mv::Engine::prepare_pipeline(void)
{
    VkDescriptorSetLayout uniform_layout = descriptor_allocator->get_layout("uniform_layout");
    VkDescriptorSetLayout sampler_layout = descriptor_allocator->get_layout("sampler_layout");

    std::vector<VkDescriptorSetLayout> layout_w_sampler = {uniform_layout, uniform_layout, uniform_layout, sampler_layout};
    std::vector<VkDescriptorSetLayout> layout_no_sampler = {uniform_layout, uniform_layout, uniform_layout};
    //std::vector<VkDescriptorSetLayout> layouts = {uniform_layout, uniform_layout, uniform_layout};
    VkPipelineLayoutCreateInfo w_sampler_info = {};
    w_sampler_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    w_sampler_info.pNext = nullptr;
    w_sampler_info.flags = 0;
    w_sampler_info.setLayoutCount = static_cast<uint32_t>(layout_w_sampler.size());
    w_sampler_info.pSetLayouts = layout_w_sampler.data();

    if (vkCreatePipelineLayout(device->device, &w_sampler_info, nullptr, &pipeline_layout_w_sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout with sampler");
    }

    VkPipelineLayoutCreateInfo no_sampler_info = {};
    no_sampler_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    no_sampler_info.pNext = nullptr;
    no_sampler_info.flags = 0;
    no_sampler_info.setLayoutCount = static_cast<uint32_t>(layout_no_sampler.size());
    no_sampler_info.pSetLayouts = layout_no_sampler.data();

    if (vkCreatePipelineLayout(device->device, &no_sampler_info, nullptr, &pipeline_layout_no_sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout with no sampler");
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