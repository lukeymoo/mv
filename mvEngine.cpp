#include "mvEngine.h"

mv::Engine::Engine(int w, int h, const char *title)
    : Window(w, h, title)
{
    return;
}

void mv::Engine::go(void)
{
    std::cout << "[+] Preparing vulkan" << std::endl;
    prepare();

    // Prepare uniforms
    prepareUniforms();

    // Load model data, hardcoded file "models/viking_room.obj" for now
    model.load(device, swapChain.imageCount);
    createDescriptorSets();

    preparePipeline();

    // Record commands to already created command buffers(per swap)
    // Create pipeline and tie all resources together

    while ((event = xcb_wait_for_event(connection))) // replace this as it is a blocking call
    {
        switch (event->response_type & ~0x80)
        {
        default:
            break;
        }

        free(event);
    }
    return;
}

void mv::Engine::prepareUniforms(void)
{
    for (auto &obj : objects)
    {
        device->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             &obj.uniformBuffer,
                             sizeof(Object::Matrices));
    }
    return;
}

void mv::Engine::createDescriptorSets(void)
{
    // create layout
    VkDescriptorSetLayoutBinding model = {};
    model.binding = 0;
    model.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    model.descriptorCount = 1;
    model.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {model};

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo = {};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    descriptorLayoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device->device, &descriptorLayoutInfo, nullptr, &descriptorLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    /*
        model view projection uniform
    */
    VkDescriptorPoolSize objmvp = {};
    objmvp.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    objmvp.descriptorCount = 1 + static_cast<uint32_t>(objects.size());

    std::vector<VkDescriptorPoolSize> poolSizes = {objmvp};

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(poolSizes.size());

    if (vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor pool for model");
    }

    // Allocate descriptor sets for objs
    for (auto &obj : objects)
    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorLayout;
        if (vkAllocateDescriptorSets(device->device, &allocInfo, &obj.descriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate descriptor set for object");
        }
        // Now update to bind buffers
        VkWriteDescriptorSet wrm = {};
        wrm.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wrm.dstBinding = 0;
        wrm.dstSet = obj.descriptorSet;
        wrm.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wrm.descriptorCount = 1;
        wrm.pBufferInfo = &obj.uniformBuffer.descriptor;

        std::vector<VkWriteDescriptorSet> writes = {wrm};

        vkUpdateDescriptorSets(device->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    return;
}

void mv::Engine::preparePipeline(void)
{
    VkPipelineLayoutCreateInfo plineInfo = {};
    plineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plineInfo.pNext = nullptr;
    plineInfo.flags = 0;
    plineInfo.setLayoutCount = 1;
    plineInfo.pSetLayouts = &descriptorLayout;
    // TODO
    // push constants
    if (vkCreatePipelineLayout(device->device, &plineInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescription = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo viState = {};
    viState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    viState.vertexBindingDescriptionCount = 1;
    viState.pVertexBindingDescriptions = &bindingDescription;
    viState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
    viState.pVertexAttributeDescriptions = attributeDescription.data();
    VkPipelineInputAssemblyStateCreateInfo iaState = mv::initializer::inputAssemblyStateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rsState = mv::initializer::rasterizationStateInfo(VK_POLYGON_MODE_LINE, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState cbaState = mv::initializer::colorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo cbState = mv::initializer::colorBlendStateInfo(1, &cbaState);
    VkPipelineDepthStencilStateCreateInfo dsState = mv::initializer::depthStencilStateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkViewport vp = {};
    VkRect2D sc = {};
    vp.x = 0;
    vp.y = 0;
    vp.width = windowWidth;
    vp.height = windowHeight;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    
    sc.offset = {0, 0};
    sc.extent = swapChain.swapExtent;

    VkPipelineViewportStateCreateInfo vpState = mv::initializer::viewportStateInfo(1, 1, 0);
    vpState.pViewports = &vp;
    vpState.pScissors = &sc;
    VkPipelineMultisampleStateCreateInfo msState = mv::initializer::multisampleStateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

    // Load shaders
    auto vertexShader = readFile("shaders/vertex.spv");
    auto fragmentShader = readFile("shaders/fragment.spv");

    // Ensure we have files
    if (vertexShader.empty() || fragmentShader.empty())
    {
        throw std::runtime_error("Failed to load fragment or vertex shader spv files");
    }

    VkShaderModule vertexShaderModule = createShaderModule(vertexShader);
    VkShaderModule fragmentShaderModule = createShaderModule(fragmentShader);

    // describe vertex shader stage
    VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
    vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageInfo.pNext = nullptr;
    vertexShaderStageInfo.flags = 0;
    vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageInfo.module = vertexShaderModule;
    vertexShaderStageInfo.pName = "main";
    vertexShaderStageInfo.pSpecializationInfo = nullptr;

    // describe fragment shader stage
    VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};
    fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageInfo.pNext = nullptr;
    fragmentShaderStageInfo.flags = 0;
    fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageInfo.module = fragmentShaderModule;
    fragmentShaderStageInfo.pName = "main";
    fragmentShaderStageInfo.pSpecializationInfo = nullptr;

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {vertexShaderStageInfo, fragmentShaderStageInfo};

    VkGraphicsPipelineCreateInfo pipelineObjInfo = mv::initializer::pipelineCreateInfo(pipelineLayout, m_RenderPass);
    pipelineObjInfo.pInputAssemblyState = &iaState;
    pipelineObjInfo.pRasterizationState = &rsState;
    pipelineObjInfo.pColorBlendState = &cbState;
    pipelineObjInfo.pDepthStencilState = &dsState;
    pipelineObjInfo.pViewportState = &vpState;
    pipelineObjInfo.pMultisampleState = &msState;
    pipelineObjInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineObjInfo.pStages = shaderStages.data();
    pipelineObjInfo.pVertexInputState = &viState;

    if (vkCreateGraphicsPipelines(device->device, m_PipelineCache, 1, &pipelineObjInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device->device, vertexShaderModule, nullptr);
    vkDestroyShaderModule(device->device, fragmentShaderModule, nullptr);
    return;
}

void mv::Engine::recordCommandBuffer(uint32_t imageIndex)
{
    // VkCommandBufferBeginInfo beginInfo = {};
    // beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // beginInfo.pNext = nullptr;
    // beginInfo.flags = 0;
    // beginInfo.pInheritanceInfo = nullptr;

    // if (vkBeginCommandBuffer(cmdBuffers[imageIndex], &beginInfo) != VK_SUCCESS)
    // {
    //     throw std::runtime_error("Failed to begin recording to command buffer");
    // }

    // VkRenderPassBeginInfo passInfo = {};
    // passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    // passInfo.pNext = nullptr;
    // passInfo.renderPass = m_RenderPass;
    // passInfo.framebuffer = frameBuffers[imageIndex];
    // passInfo.clearValueCount = 1;
    // passInfo.pClearValues = &clearvals

    return;
}