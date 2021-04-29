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
    create_descriptor_sets(&global_uniforms);

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

    // Created in Engine class
    // GlobalUniforms global_uniforms;

    // create view matrix buffer
    device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &global_uniforms.ubo_view,
                          sizeof(GlobalUniforms::view));
    if (global_uniforms.ubo_view.map() != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to map view matrix buffer");
    }

    // create projection matrix buffer
    device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                          &global_uniforms.ubo_projection,
                          sizeof(GlobalUniforms::projection));
    if (global_uniforms.ubo_projection.map() != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to map projection matrix buffer");
    }

    // Initialze model types
    uint32_t model_type_count = 2;

    models.resize(model_type_count);

    // ensure every child OBJECT of MODEL type knows it's MODEL type index
    for (int i = 0; i < model_type_count; i++)
    {
        for (auto &obj : models[i].objects)
        {
            obj.model_index = i;
        }
    }

    // configure every OBJECTs position, rotation BEFORE uniform buffer creation
    // the OBJECTs count will be set by a map editor of some sort later
    models[0].resize_object_container(1);
    models[1].resize_object_container(1);
    models[0].objects[0].position = glm::vec3(0.0f, 0.0f, 0.0f);
    models[0].objects[0].rotation = glm::vec3(180.0f, 0.0f, 0.0f);

    models[1].objects[0].position = glm::vec3(0.0f, 0.0f, -5.0f);
    models[1].objects[0].rotation = glm::vec3(0.0f, 0.0f, 0.0f);

    // configure camera before uniform buffer creation
    camera_init_struct camera_params;
    camera_params.fov = 50.0f * ((float)swapchain.swap_extent.width / swapchain.swap_extent.height);
    camera_params.aspect = static_cast<float>(((float)swapchain.swap_extent.height / (float)swapchain.swap_extent.height));
    camera_params.nearz = 0.01f;
    camera_params.farz = 100.0f;
    camera_params.position = glm::vec3(0.0f, 3.0f, -7.0f);
    camera_params.matrices = &global_uniforms;

    camera_params.camera_type = Camera::Type::third_person;
    camera_params.target = &models[0].objects[0];

    camera = std::make_unique<Camera>(camera_params);

    // Prepare uniforms
    prepare_uniforms();

    // Load models
    // set each object model index to it's Model class container
    models[0].load(device, "models/player.obj");
    models[1].load(device, "models/car.obj");

    // Create descriptor pool allocator
    descriptor_allocator = std::make_unique<Allocator>(device->device);

    create_descriptor_sets(&global_uniforms);

    prepare_pipeline();

    // Record commands to already created command buffers(per swap)
    // Create pipeline and tie all resources together
    uint32_t imageIndex = 0;
    size_t currentFrame = 0;
    bool added = false;
    fps.startTimer();
    int fps_counter = 0;
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
        if (camera->get_type() == Camera::Type::first_person)
        {
            if (mouseEvent.get_type() == Mouse::Event::Type::Move && mouse.is_in_window())
            {
                // get delta
                std::pair<int, int> mouse_delta = mouse.get_pos_delta();
                glm::vec3 rotation_delta = glm::vec3(mouse_delta.second, mouse_delta.first, 0.0f);
                camera->rotate(rotation_delta, fpsdt);
            }
        }

        // First person
        if (camera->get_type() == Camera::Type::first_person)
        {
            if (kbd.is_key_pressed(' ')) // space
            {
                camera->move_up(fpsdt);
            }
            if (kbd.is_key_pressed(65507)) // ctrl key
            {
                camera->move_down(fpsdt);
            }

            // lateral movements
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
        }
        if (camera->get_type() == Camera::Type::third_person)
        {
            /*
                For testing dynamic allocation of descriptor set
            */
            if (kbdEvent.get_type() == Keyboard::Event::Type::Press && kbdEvent.get_code() == ' ' && added == false)
            {
                //added = true;
                uint32_t tc = 0;
                add_new_model(descriptor_allocator->get(), "models/player.obj");
                for (const auto &model : models)
                {
                    for (const auto &obj : model.objects)
                    {
                        tc ++;
                    }
                }
                std::cout << "\tNew Model Added, COUNT => " << tc + 2 << std::endl;
            }
            /* ---------------------*/

            if (kbd.is_key_pressed('i'))
            {
                models[0].objects[0].move_forward(fpsdt);
            }
            if (kbd.is_key_pressed('k'))
            {
                models[0].objects[0].move_backward(fpsdt);
            }
            if (kbd.is_key_pressed('j'))
            {
                models[0].objects[0].move_left(fpsdt);
            }
            if (kbd.is_key_pressed('l'))
            {
                models[0].objects[0].move_right(fpsdt);
            }

            if (kbd.is_key_pressed('w'))
            {
                camera->decrease_pitch(fpsdt);
            }
            if (kbd.is_key_pressed('s'))
            {
                camera->increase_pitch(fpsdt);
            }
            if (kbd.is_key_pressed('a'))
            {
                // change camera orbit angle
                camera->decrease_orbit(fpsdt); // counter clockwise rotation
            }
            if (kbd.is_key_pressed('d'))
            {
                // change camera orbit angle
                camera->increase_orbit(fpsdt); // clockwise rotation
            }
        }

        /*
            Do object/uniform updates
        */
        for (auto &model : models)
        {
            for (auto &obj : model.objects)
            {
                obj.update();
            }
        }
        // updates view matrix & performs memcpy
        camera->update();

        // This will be reworked after uniform buffers are split
        // There is no need to update the view & projection matrix for every single model
        // Every object will share a projection matrix for the duration of a swap chains life
        // The view matrix will be updated per frame
        // the model matrix will be updated per object with push constants
        Object::Matrices tm;

        // tm.view = camera->get_view();
        // tm.projection = camera->get_projection();

        // good place to call is on startup & swap chain recreation
        // pseudo code
        // if (proj.mat == changed)
        // {
        //     memcpy(proj.mat, &newdata, sizeof(GlobalUniforms::projection));
        // }

        // memcpy for model ubos of each object
        for (auto &model : models)
        {
            for (auto &obj : model.objects)
            {
                // TODO
                // replace with push constants in the cmd buffer recreation
                // local object model matrix is updated via calls to obj->update()
                tm.model = obj.matrices.model;
                memcpy(obj.uniform_buffer.mapped, &tm, sizeof(Object::Matrices));
            }
        }

        // Render
        draw(currentFrame, imageIndex);

        fps_counter += 1;
        if (fps.getElaspedMS() > 1000)
        {
            //printf("FPS => %i\n", fps_counter);
            fps_counter = 0;
            fps.restart();
        }
    }

    // cleanup global uniforms
    vkDeviceWaitIdle(m_device);
    global_uniforms.ubo_projection.destroy();
    global_uniforms.ubo_view.destroy();
    return;
}

void mv::Engine::add_new_model(mv::Allocator::Container *pool, const char *filename)
{
    // resize models
    models.push_back(mv::Model());
    // add object to model
    models[(models.size() - 1)].resize_object_container(1);

    // iterate models
    for (size_t i = 0; i < models.size(); i++)
    {
        // iterate objects assign model indices
        for (size_t j = 0; j < models[i].objects.size(); j++)
        {
            models[i].objects[j].model_index = i;
        }
    }

    // hardcoded for testing purposes
    int min = 0;
    int max = 30;
    int z_max = 30;

    std::random_device rd;
    std::default_random_engine eng(rd());
    std::uniform_int_distribution<int> xy_distr(min, max);
    std::uniform_int_distribution<int> z_distr(min, z_max);

    float x = xy_distr(eng);
    float y = xy_distr(eng);
    float z = z_distr(eng);
    models[(models.size() - 1)].objects[0].position = glm::vec3(x, y, -z);
    models[(models.size() - 1)].objects[0].rotation = glm::vec3(180.0f, 0.0f, 0.0f);

    // prepare descriptor for model
    device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &models[(models.size() - 1)].objects[0].uniform_buffer,
                          sizeof(Object::Matrices));
    if (models[(models.size() - 1)].objects[0].uniform_buffer.map() != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate buffer for newly created model");
    }

    // Load model
    models[(models.size() - 1)].load(device, filename);

    // allocate descriptor set for object of new model type
    pool = pool->allocate_set(model_layout, models[(models.size() - 1)].objects[0].descriptor_set);
    // point descriptor to models ubo
    pool = pool->update_set(models[(models.size() - 1)].objects[0].uniform_buffer.descriptor,
                     models[(models.size() - 1)].objects[0].descriptor_set,
                     0);

    return;
}

void mv::Engine::prepare_uniforms(void)
{
    for (auto &model : models)
    {
        for (auto &obj : model.objects)
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
    }
    return;
}

void mv::Engine::create_descriptor_layout(VkDescriptorType type, uint32_t count, uint32_t binding, VkDescriptorSetLayout &layout)
{
    VkDescriptorSetLayoutBinding bind_info = {};
    bind_info.binding = binding;
    bind_info.descriptorType = type;
    bind_info.descriptorCount = count;
    bind_info.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

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

void mv::Engine::create_descriptor_sets(GlobalUniforms *view_proj_ubo_container, bool should_create_layout)
{
    if (should_create_layout)
    {
        // Currently mvp ubo objects are all identical
        // so we create one layout
        // in future we may need two as model ubo will contain other data such as
        // model uv, textures, normals, animation data, etc
        create_descriptor_layout(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                 1,
                                 0,
                                 model_layout);
    }

    // Create uniform buffer pool
    mv::Allocator::Container *pool;
    pool = descriptor_allocator->allocate_pool(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                               10);

    // Allocate sets for view & matrix ubos
    pool = pool->allocate_set(model_layout, global_uniforms.view_descriptor_set);
    pool = pool->update_set(global_uniforms.ubo_view.descriptor, global_uniforms.view_descriptor_set, 0);

    pool = pool->allocate_set(model_layout, global_uniforms.proj_descriptor_set);
    pool = pool->update_set(global_uniforms.ubo_projection.descriptor, global_uniforms.proj_descriptor_set, 0);

    // allocate for per model ubo
    for (auto &model : models)
    {
        for (auto &obj : model.objects)
        {
            pool = pool->allocate_set(model_layout, obj.descriptor_set);
            pool = pool->update_set(obj.uniform_buffer.descriptor, obj.descriptor_set, 0);
        }
    }

    /*
        model view projection uniform
    */
    // get total count of objects
    // uint32_t total_object_count = 0;
    // for (const auto &model : models)
    // {
    //     total_object_count += model.objects.size();
    // }

    // assert(total_object_count); // ensure non zero

    // VkDescriptorPoolSize obj_model = {};
    // obj_model.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // obj_model.descriptorCount = 1 + static_cast<uint32_t>(total_object_count);

    // VkDescriptorPoolSize view_mat = {};
    // view_mat.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // view_mat.descriptorCount = 1;

    // VkDescriptorPoolSize proj_mat = {};
    // proj_mat.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // proj_mat.descriptorCount = 1;

    // std::vector<VkDescriptorPoolSize> pool_sizes = {obj_model, view_mat, proj_mat};

    // VkDescriptorPoolCreateInfo pool_info = {};
    // pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    // pool_info.pNext = nullptr;
    // pool_info.flags = 0;
    // pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    // pool_info.pPoolSizes = pool_sizes.data();
    // pool_info.maxSets = 2000;

    // if (vkCreateDescriptorPool(device->device, &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS)
    // {
    //     throw std::runtime_error("Failed to create descriptor pool for model");
    // }

    // // allocate descriptor set for view & projection matrices
    // VkDescriptorSetAllocateInfo view_alloc_info = {};
    // view_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    // view_alloc_info.descriptorPool = descriptor_pool;
    // view_alloc_info.descriptorSetCount = 1;
    // view_alloc_info.pSetLayouts = &view_layout;
    // if (vkAllocateDescriptorSets(device->device,
    //                              &view_alloc_info,
    //                              &view_proj_ubo_container->view_descriptor_set) != VK_SUCCESS)
    // {
    //     throw std::runtime_error("Failed to allocate descriptor set for view matrix");
    // }

    // // projection alloc info
    // VkDescriptorSetAllocateInfo proj_alloc_info = {};
    // proj_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    // proj_alloc_info.descriptorPool = descriptor_pool;
    // proj_alloc_info.descriptorSetCount = 1;
    // proj_alloc_info.pSetLayouts = &projection_layout;
    // if (vkAllocateDescriptorSets(device->device,
    //                              &proj_alloc_info,
    //                              &view_proj_ubo_container->proj_descriptor_set) != VK_SUCCESS)
    // {
    //     throw std::runtime_error("Failed to allocate descriptor set for projection matrix");
    // }

    // // update bindings for view descriptor
    // VkWriteDescriptorSet view_write = {};
    // view_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // view_write.dstBinding = 0; // view binding
    // view_write.dstSet = view_proj_ubo_container->view_descriptor_set;
    // view_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // view_write.descriptorCount = 1;
    // view_write.pBufferInfo = &view_proj_ubo_container->ubo_view.descriptor;

    // // projection descriptor set
    // VkWriteDescriptorSet proj_write = {};
    // proj_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // proj_write.dstBinding = 0; // view binding
    // proj_write.dstSet = view_proj_ubo_container->proj_descriptor_set;
    // proj_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // proj_write.descriptorCount = 1;
    // proj_write.pBufferInfo = &view_proj_ubo_container->ubo_projection.descriptor;

    // std::vector<VkWriteDescriptorSet> vp_write_sets = {view_write, proj_write};
    // vkUpdateDescriptorSets(device->device,
    //                        static_cast<uint32_t>(vp_write_sets.size()),
    //                        vp_write_sets.data(),
    //                        0, nullptr);

    // // Allocate descriptor sets for objs
    // for (auto &model : models)
    // {
    //     for (auto &obj : model.objects)
    //     {
    //         VkDescriptorSetAllocateInfo alloc_info = {};
    //         alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    //         alloc_info.descriptorPool = descriptor_pool;
    //         alloc_info.descriptorSetCount = 1;
    //         alloc_info.pSetLayouts = &model_layout;
    //         if (vkAllocateDescriptorSets(device->device, &alloc_info, &obj.descriptor_set) != VK_SUCCESS)
    //         {
    //             throw std::runtime_error("Failed to allocate descriptor set for object");
    //         }
    //         // Now update to bind buffers
    //         VkWriteDescriptorSet wrm = {};
    //         wrm.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //         wrm.dstBinding = 0; // model is always binding 0
    //         wrm.dstSet = obj.descriptor_set;
    //         wrm.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    //         wrm.descriptorCount = 1;
    //         wrm.pBufferInfo = &obj.uniform_buffer.descriptor;

    //         std::vector<VkWriteDescriptorSet> writes = {wrm};

    //         vkUpdateDescriptorSets(device->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    //     }
    // }
    return;
}

void mv::Engine::prepare_pipeline(void)
{
    // TODO
    // currently all descriptor sets are based off one set
    // this will change later when shaders require more model data in ubo
    std::vector<VkDescriptorSetLayout> layouts = {model_layout, model_layout, model_layout};
    VkPipelineLayoutCreateInfo pline_info = {};
    pline_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pline_info.pNext = nullptr;
    pline_info.flags = 0;
    pline_info.setLayoutCount = static_cast<uint32_t>(layouts.size());
    pline_info.pSetLayouts = layouts.data();
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
    VkPipelineRasterizationStateCreateInfo rs_state = mv::initializer::rasterization_state_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
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

    // for each model bind it's buffers
    for (auto &model : models)
    {
        model.bindBuffers(command_buffers[image_index]);
        // VkDeviceSize offsets[] = {0};
        // vkCmdBindVertexBuffers(command_buffers[image_index], 0, 1, &model.vertices.buffer, offsets);
        // vkCmdBindIndexBuffer(command_buffers[image_index], model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
        // TODO
        // draw each object
        // will eventually instance draw to be more efficient
        for (const auto &obj : model.objects)
        {
            std::vector<VkDescriptorSet> to_bind = {
                obj.descriptor_set,
                global_uniforms.view_descriptor_set,
                global_uniforms.proj_descriptor_set};
            vkCmdBindDescriptorSets(command_buffers[image_index],
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline_layout,
                                    0,
                                    static_cast<uint32_t>(to_bind.size()),
                                    to_bind.data(),
                                    0,
                                    nullptr);
            // Call model draw
            // Reformat later for instanced drawing of each model type we bind
            vkCmdDraw(command_buffers[image_index], static_cast<uint32_t>(model.vertices.count), 1, 0, 0);
            //vkCmdDrawIndexed(command_buffers[image_index], static_cast<uint32_t>(models[obj.model_index].indices.count), 1, 0, 0, 0);
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