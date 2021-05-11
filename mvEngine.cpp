#include "mvEngine.h"

void mv::Engine::cleanup_swapchain(void) {
  // destroy command buffers
  if (command_buffers) {
    if (!command_buffers->empty()) {
      mv_device->logical_device->freeCommandBuffers(*command_pool, *command_buffers);
    }
    command_buffers.reset();
    command_buffers = std::make_unique<std::vector<vk::CommandBuffer>>();
  }

  // destroy framebuffers
  if (frame_buffers) {
    if (!frame_buffers->empty()) {
      for (auto &buffer : *frame_buffers) {
        if (buffer) {
          mv_device->logical_device->destroyFramebuffer(buffer, nullptr);
        }
      }
      frame_buffers.reset();
      frame_buffers = std::make_unique<std::vector<vk::Framebuffer>>();
    }
  }

  if (depth_stencil) {
    if (depth_stencil->image) {
      mv_device->logical_device->destroyImage(depth_stencil->image, nullptr);
    }
    if (depth_stencil->view) {
      mv_device->logical_device->destroyImageView(depth_stencil->view, nullptr);
    }
    if (depth_stencil->mem) {
      mv_device->logical_device->freeMemory(depth_stencil->mem, nullptr);
    }
    depth_stencil.reset();
    depth_stencil = std::make_unique<struct mv::MWindow::depth_stencil_struct>();
  }

  // destroy pipelines
  if (pipelines) {
    for (auto &pipeline : *pipelines) {
      if (pipeline.second)
        mv_device->logical_device->destroyPipeline(pipeline.second);
    }
    pipelines.reset();
    pipelines = std::make_unique<std::unordered_map<std::string, vk::Pipeline>>();
  }

  // destroy pipeline layouts
  if (pipeline_layouts) {
    for (auto &layout : *pipeline_layouts) {
      if (layout.second)
        mv_device->logical_device->destroyPipelineLayout(layout.second);
    }
    pipeline_layouts.reset();
    pipeline_layouts = std::make_unique<std::unordered_map<std::string, vk::PipelineLayout>>();
  }

  // destroy render pass
  if (render_pass) {
    mv_device->logical_device->destroyRenderPass(*render_pass, nullptr);
    render_pass.reset();
    render_pass = std::make_unique<vk::RenderPass>();
  }

  // cleanup command pool
  if (command_pool) {
    mv_device->logical_device->destroyCommandPool(*command_pool);
    command_pool.reset();
    command_pool = std::make_unique<vk::CommandPool>();
  }

  // cleanup swapchain
  swapchain->cleanup(*instance, *mv_device, false);

  return;
}

// Allows swapchain to keep up with window resize
void mv::Engine::recreate_swapchain(void) {
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

  // call after swapchain creation!!!
  // needed for mouse delta calc when using from_center method
  mouse->set_window_properties(window_width, window_height);

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

void mv::Engine::go(void) {
  std::cout << "[+] Preparing vulkan" << std::endl;
  prepare();

  // Create descriptor pool allocator
  descriptor_allocator = std::make_unique<mv::Allocator>();

  // allocate pool
  descriptor_allocator->allocate_pool(*mv_device, 2000);

  // create uniform buffer layout ( single mat4 object )
  descriptor_allocator->create_layout(*mv_device, "uniform_layout",
                                      vk::DescriptorType::eUniformBuffer, 1,
                                      vk::ShaderStageFlagBits::eVertex, 0);

  // create texture sampler layout
  descriptor_allocator->create_layout(*mv_device, "sampler_layout",
                                      vk::DescriptorType::eCombinedImageSampler, 1,
                                      vk::ShaderStageFlagBits::eFragment, 0);

  // initialize model/object container
  // by default it contains uniform buffer for view & projection matrices
  collection_handler = std::make_unique<Collection>(*mv_device);

  // get uniform layout to create descriptors for view & projection matrix ubos
  vk::DescriptorSetLayout uniform_layout = descriptor_allocator->get_layout("uniform_layout");

  // allocate & update descriptor set for view uniform buffer
  descriptor_allocator->allocate_set(*mv_device, uniform_layout,
                                     collection_handler->view_uniform->descriptor);
  descriptor_allocator->update_set(*mv_device,
                                   collection_handler->view_uniform->mv_buffer.descriptor,
                                   collection_handler->view_uniform->descriptor, 0);

  // allocate & update descriptor set for projection uniform buffer
  descriptor_allocator->allocate_set(*mv_device, uniform_layout,
                                     collection_handler->projection_uniform->descriptor);
  descriptor_allocator->update_set(*mv_device,
                                   collection_handler->projection_uniform->mv_buffer.descriptor,
                                   collection_handler->projection_uniform->descriptor, 0);

  // Load model
  collection_handler->load_model(*mv_device, *descriptor_allocator, "models/_viking_room.fbx");
  collection_handler->load_model(*mv_device, *descriptor_allocator, "models/Male.obj");

  // Create object
  collection_handler->create_object(*mv_device, *descriptor_allocator, "models/_viking_room.fbx");
  collection_handler->models->at(0).objects->at(0).position = glm::vec3(0.0f, 0.0f, -5.0f);
  collection_handler->models->at(0).objects->at(0).rotation = glm::vec3(0.0f, 90.0f, 0.0f);

  collection_handler->create_object(*mv_device, *descriptor_allocator, "models/Male.obj");
  collection_handler->models->at(1).objects->at(0).rotation = glm::vec3(0.0f, 0.0f, 0.0f);
  collection_handler->models->at(1).objects->at(0).position = glm::vec3(0.0f, 0.0f, 0.0f);

  // configure camera before uniform buffer creation
  camera_init_struct camera_params;
  camera_params.fov = 45.0f * ((float)swapchain->swap_extent.width / swapchain->swap_extent.height);
  camera_params.aspect = static_cast<float>(
      ((float)swapchain->swap_extent.height / (float)swapchain->swap_extent.height));
  camera_params.nearz = 0.1f;
  camera_params.farz = 200.0f;
  camera_params.position = glm::vec3(0.0f, 3.0f, -7.0f);
  camera_params.view_uniform_object = collection_handler->view_uniform.get();
  camera_params.projection_uniform_object = collection_handler->projection_uniform.get();

  camera_params.camera_type = Camera::camera_type::third_person;
  camera_params.target = &collection_handler->models->at(1).objects->at(0);

  camera = std::make_unique<Camera>(camera_params);

  if (camera->type == Camera::camera_type::first_person || Camera::camera_type::free_look) {
    mouse->set_delta_style(mouse::delta_calc_style::from_center);
  } else {
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
  while (running) {
    auto delta_time = chrono::now() - start_time;
    start_time = chrono::now();
    accumulated += std::chrono::duration_cast<std::chrono::nanoseconds>(delta_time);

    while (XPending(*display)) {
      handle_x_event();
    }

    while (accumulated >= timestep) {
      accumulated -= timestep;

      // Get input events
      mv::keyboard::event kbd_event = kbd->read();
      mv::mouse::event mouse_event = mouse->read();

      if (camera->type == Camera::camera_type::third_person) {
        // handle mouse scroll wheel
        if (mouse_event.type == mouse::event::etype::wheel_up) {
          camera->adjust_zoom(-(camera->zoom_step));
        }
        if (mouse_event.type == mouse::event::etype::wheel_down) {
          camera->adjust_zoom(camera->zoom_step);
        }

        // start mouse drag
        if (mouse_event.type == mv::mouse::event::etype::r_down && !mouse->is_dragging) {
          // XDefineCursor(display, window, mouse->hidden_cursor);
          mouse->start_drag(camera->orbit_angle, camera->pitch);
        }
        // end drag
        if (mouse_event.type == mv::mouse::event::etype::r_release && mouse->is_dragging) {
          // XUndefineCursor(display, window);
          camera->realign_orbit();
          mouse->end_drag();
        }

        // if drag enabled check for release
        if (mouse->is_dragging) {
          // camera orbit
          if (mouse->drag_delta_x > 0) {
            camera->adjust_orbit(-abs(mouse->drag_delta_x), mouse->stored_orbit);
          } else if (mouse->drag_delta_x < 0) {
            camera->adjust_orbit(abs(mouse->drag_delta_x), mouse->stored_orbit);
          }

          // camera pitch
          if (mouse->drag_delta_y > 0) {
            camera->adjust_pitch(abs(mouse->drag_delta_y) * 0.25f, mouse->stored_pitch);
          } else if (mouse->drag_delta_y < 0) {
            camera->adjust_pitch(-abs(mouse->drag_delta_y) * 0.25f, mouse->stored_pitch);
          }

          // fetch new orbit & pitch
          mouse->stored_orbit = camera->orbit_angle;
          mouse->stored_pitch = camera->pitch;
          // hide pointer & warp to drag start
          // XIWarpPointer(*display, mouse->deviceid, None, window, 0, 0, 0, 0, mouse->drag_startx,
          //               mouse->drag_starty);
          XWarpPointer(*display, None, window, 0, 0, 0, 0, mouse->drag_startx, mouse->drag_starty);
          XFlush(*display);
          mouse->clear();
          camera->target->rotate_to_face(camera->orbit_angle);
        }

        // sort movement by key combination

        // forward only
        if (kbd->is_keystate(mv::keyboard::key::w) && !kbd->is_keystate(mv::keyboard::key::d) &&
            !kbd->is_keystate(mv::keyboard::key::a) && !kbd->is_keystate(mv::keyboard::key::s)) {
          camera->target->move(camera->orbit_angle, glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
        }

        // forward + left + right -- go straight
        if (kbd->is_keystate(mv::keyboard::key::w) && kbd->is_keystate(mv::keyboard::key::d) &&
            kbd->is_keystate(mv::keyboard::key::a) && !kbd->is_keystate(mv::keyboard::key::s)) {
          camera->target->move(camera->orbit_angle, glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
        }

        // forward + right
        if (kbd->is_keystate(mv::keyboard::key::w) && kbd->is_keystate(mv::keyboard::key::d) &&
            !kbd->is_keystate(mv::keyboard::key::a) && !kbd->is_keystate(mv::keyboard::key::s)) {
          camera->target->move(camera->orbit_angle, glm::vec4(1.0f, 0.0f, -1.0f, 1.0f));
        }

        // forward + left
        if (kbd->is_keystate(mv::keyboard::key::w) && !kbd->is_keystate(mv::keyboard::key::d) &&
            kbd->is_keystate(mv::keyboard::key::a) && !kbd->is_keystate(mv::keyboard::key::s)) {
          camera->target->move(camera->orbit_angle, glm::vec4(-1.0f, 0.0f, -1.0f, 1.0f));
        }

        // backward only
        if (!kbd->is_keystate(mv::keyboard::key::w) && !kbd->is_keystate(mv::keyboard::key::d) &&
            !kbd->is_keystate(mv::keyboard::key::a) && kbd->is_keystate(mv::keyboard::key::s)) {
          camera->target->move(camera->orbit_angle, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
        }

        // backward + left
        if (!kbd->is_keystate(mv::keyboard::key::w) && !kbd->is_keystate(mv::keyboard::key::d) &&
            kbd->is_keystate(mv::keyboard::key::a) && kbd->is_keystate(mv::keyboard::key::s)) {
          camera->target->move(camera->orbit_angle, glm::vec4(-1.0f, 0.0f, 1.0f, 1.0f));
        }

        // backward + right
        if (!kbd->is_keystate(mv::keyboard::key::w) && kbd->is_keystate(mv::keyboard::key::d) &&
            !kbd->is_keystate(mv::keyboard::key::a) && kbd->is_keystate(mv::keyboard::key::s)) {
          camera->target->move(camera->orbit_angle, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
        }

        // backward + left + right -- go back
        if (!kbd->is_keystate(mv::keyboard::key::w) && kbd->is_keystate(mv::keyboard::key::d) &&
            kbd->is_keystate(mv::keyboard::key::a) && kbd->is_keystate(mv::keyboard::key::s)) {
          camera->target->move(camera->orbit_angle, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
        }

        // left only
        if (!kbd->is_keystate(mv::keyboard::key::w) && !kbd->is_keystate(mv::keyboard::key::d) &&
            kbd->is_keystate(mv::keyboard::key::a) && !kbd->is_keystate(mv::keyboard::key::s)) {
          camera->target->move(camera->orbit_angle, glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f));
        }

        // right only
        if (!kbd->is_keystate(mv::keyboard::key::w) && kbd->is_keystate(mv::keyboard::key::d) &&
            !kbd->is_keystate(mv::keyboard::key::a) && !kbd->is_keystate(mv::keyboard::key::s)) {
          camera->target->move(camera->orbit_angle, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        }

        // debug -- add new objects to world with random position
        if (kbd->is_keystate(mv::keyboard::key::space) && added == false) {
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
          collection_handler->create_object(*mv_device, *descriptor_allocator,
                                            "models/_viking_room.fbx");
          collection_handler->models->at(0).objects->back().position = glm::vec3(x, y, z);
        }
      }
      // update game objects
      collection_handler->update();

      // update view and projection matrices
      camera->update();
    }

    // TODO
    // Add alpha to render
    // Render
    draw(current_frame, image_index);
  }
  int total_object_count = 0;
  for (const auto &model : *collection_handler->models) {
    total_object_count += model.objects->size();
  }
  std::cout << "Total objects => " << total_object_count << std::endl;
  return;
}

void mv::Engine::prepare_pipeline(void) {
  vk::DescriptorSetLayout uniform_layout = descriptor_allocator->get_layout("uniform_layout");
  vk::DescriptorSetLayout sampler_layout = descriptor_allocator->get_layout("sampler_layout");

  std::vector<vk::DescriptorSetLayout> layout_w_sampler = {uniform_layout, uniform_layout,
                                                           uniform_layout, sampler_layout};
  std::vector<vk::DescriptorSetLayout> layout_no_sampler = {uniform_layout, uniform_layout,
                                                            uniform_layout};

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
  if (vertex_shader.empty() || fragment_shader.empty() || fragment_shader_no_sampler.empty()) {
    throw std::runtime_error("Failed to load fragment or vertex shader spv files");
  }

  vk::ShaderModule vertex_shader_module = create_shader_module(vertex_shader);
  vk::ShaderModule fragment_shader_module = create_shader_module(fragment_shader);
  vk::ShaderModule fragment_shader_no_sampler_module =
      create_shader_module(fragment_shader_no_sampler);

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
  std::vector<vk::PipelineShaderStageCreateInfo> shader_stages_no_sampler = {
      vertex_shader_stage_info, fragment_shader_stage_no_sampler_info};

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
  vk::ResultValue result =
      mv_device->logical_device->createGraphicsPipeline(nullptr, pipeline_w_sampler_info);
  if (result.result != vk::Result::eSuccess)
    throw std::runtime_error("Failed to create graphics pipeline with sampler");
  pipelines->insert(std::pair<std::string, vk::Pipeline>("sampler", result.value));

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
  result = mv_device->logical_device->createGraphicsPipeline(nullptr, pipeline_no_sampler_info);
  if (result.result != vk::Result::eSuccess)
    throw std::runtime_error("Failed to create graphics pipeline with no sampler");
  pipelines->insert(std::pair<std::string, vk::Pipeline>("no_sampler", result.value));

  mv_device->logical_device->destroyShaderModule(vertex_shader_module);
  mv_device->logical_device->destroyShaderModule(fragment_shader_module);
  mv_device->logical_device->destroyShaderModule(fragment_shader_no_sampler_module);
  return;
}

void mv::Engine::record_command_buffer(uint32_t image_index) {
  vk::CommandBufferBeginInfo begin_info;

  command_buffers->at(image_index).begin(begin_info);

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

  command_buffers->at(image_index).beginRenderPass(pass_info, vk::SubpassContents::eInline);

  for (auto &model : *collection_handler->models) {
    // for each model select the appropriate pipeline
    if (model.has_texture) {
      command_buffers->at(image_index)
          .bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines->at("sampler"));
    } else {
      command_buffers->at(image_index)
          .bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines->at("no_sampler"));
    }
    for (auto &object : *model.objects) {
      for (auto &mesh : *model._meshes) {
        std::vector<vk::DescriptorSet> to_bind = {
            object.model_descriptor, collection_handler->view_uniform->descriptor,
            collection_handler->projection_uniform->descriptor};
        if (model.has_texture) {
          // TODO
          // allow for multiple texture descriptors
          to_bind.push_back(mesh.textures.at(0).descriptor);
          command_buffers->at(image_index)
              .bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layouts->at("sampler"),
                                  0, to_bind, nullptr);
        } else {
          command_buffers->at(image_index)
              .bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                  pipeline_layouts->at("no_sampler"), 0, to_bind, nullptr);
        }
        // Bind mesh buffers
        mesh.bindBuffers(command_buffers->at(image_index));

        // Indexed draw
        command_buffers->at(image_index)
            .drawIndexed(static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
      }
    }
  }

  command_buffers->at(image_index).endRenderPass();

  command_buffers->at(image_index).end();

  return;
}

void mv::Engine::draw(size_t &cur_frame, uint32_t &cur_index) {
  vk::Result res = mv_device->logical_device->waitForFences(in_flight_fences->at(cur_frame),
                                                            VK_TRUE, UINT64_MAX);
  if (res != vk::Result::eSuccess)
    throw std::runtime_error("Error occurred while waiting for fence");

  vk::Result result = mv_device->logical_device->acquireNextImageKHR(
      *swapchain->swapchain, UINT64_MAX, semaphores->present_complete, nullptr, &cur_index);

  switch (result) {
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

  if (wait_fences->at(cur_index)) {
    vk::Result res =
        mv_device->logical_device->waitForFences(wait_fences->at(cur_index), VK_TRUE, UINT64_MAX);
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

  switch (result) {
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

  switch (result) {
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