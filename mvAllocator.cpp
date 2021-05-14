#include "mvAllocator.h"

mv::Allocator::Allocator(void)
{
    containers = std::make_unique<std::vector<struct Container>>();
    layouts = std::make_unique<std::unordered_map<std::string, vk::DescriptorSetLayout>>();
    return;
}

mv::Allocator::~Allocator()
{
}

mv::Allocator::Container *mv::Allocator::get(void)
{
    if (!containers || containers->empty())
        throw std::runtime_error("Containers never allocated, attempted to get ptr to current pool "
                                 ":: descriptor handler");

    return &containers->at(currentPool);
}

void mv::Allocator::cleanup(const mv::Device &p_MvDevice)
{
    if (layouts)
    {
        if (!layouts->empty())
        {
            for (auto &layout : *layouts)
            {
                if (layout.second)
                    p_MvDevice.logicalDevice->destroyDescriptorSetLayout(layout.second);
            }
        }
        layouts.reset();
    }

    if (containers)
    {
        if (!containers->empty())
        {
            for (auto &container : *containers)
            {
                if (container.pool)
                    p_MvDevice.logicalDevice->destroyDescriptorPool(container.pool);
            }
        }
        containers.reset();
    }
    return;
}

vk::DescriptorSetLayout mv::Allocator::getLayout(std::string p_LayoutName)
{
    if (!layouts)
        throw std::runtime_error("Requested layout but container never initialized => " + p_LayoutName);

    for (const auto &map : *layouts)
    {
        if (map.first == p_LayoutName)
        {
            return map.second;
        }
    }
    throw std::runtime_error("Requested layout but not found => " + p_LayoutName);
}

void mv::Allocator::createLayout(const mv::Device &p_MvDevice, std::string p_LayoutName,
                                 vk::DescriptorType p_DescriptorType, uint32_t p_Count,
                                 vk::ShaderStageFlagBits p_ShaderStageFlags, uint32_t p_Binding)
{
    vk::DescriptorSetLayoutBinding bindInfo;
    bindInfo.binding = p_Binding;
    bindInfo.descriptorType = p_DescriptorType;
    bindInfo.descriptorCount = p_Count;
    bindInfo.stageFlags = p_ShaderStageFlags;

    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &bindInfo;

    createLayout(p_MvDevice, p_LayoutName, layoutInfo);
}

void mv::Allocator::createLayout(const mv::Device &p_MvDevice, std::string p_LayoutName,
                                 vk::DescriptorSetLayoutCreateInfo &p_CreateInfo)
{

    bool layoutAlreadyExists = false;

    if (!layouts)
    {
        layouts = std::make_unique<std::unordered_map<std::string, vk::DescriptorSetLayout>>();
    }
    else
    {
        // ensure name not already created
        for (const auto &map : *layouts)
        {
            if (map.first == p_LayoutName)
            {
                layoutAlreadyExists = true;
                break;
            }
        }
        if (layoutAlreadyExists)
        {
            std::cout << "[-] Request to create descriptor set layout => " << p_LayoutName
                      << " already exists; Skipping...\n";
            return;
        }
    }

    // add to map
    layouts->insert({p_LayoutName, p_MvDevice.logicalDevice->createDescriptorSetLayout(p_CreateInfo)});
    std::cout << "[+] Descriptor set layout => " << p_LayoutName << " created\n";
    return;
}

void mv::Allocator::allocateSet(const mv::Device &p_MvDevice, vk::DescriptorSetLayout &p_DescriptorLayout,
                                vk::DescriptorSet &p_DestinationSet)
{
    Container *poolContainer = &containers->at(currentPool);

    // ensure pool exist
    if (!poolContainer->pool)
        throw std::runtime_error("No pool ever allocated, attempting to allocate descriptor set");

    std::cout << "[-] Allocating descriptor set" << std::endl;
    if (!p_DescriptorLayout)
    {
        std::ostringstream oss;
        oss << "Descriptor allocated was requested to allocate a set however the user provided "
               "nullptr for layout\n";
        oss << "File => " << __FILE__ << "Line => " << __LINE__ << std::endl;
        throw std::runtime_error(oss.str().c_str());
    }

    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = poolContainer->pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &p_DescriptorLayout;

    vk::Result result = p_MvDevice.logicalDevice->allocateDescriptorSets(&allocInfo, &p_DestinationSet);

    if (result == vk::Result::eSuccess)
    {
        // ensure allocator index is up to date
        currentPool = poolContainer->index;
    }
    else if (result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool)
    {
        if (result == vk::Result::eErrorOutOfPoolMemory)
        {
            poolContainer->status = Container::Status::Full;
        }
        else
        {
            poolContainer->status = Container::Status::Fragmented;
        }
        // allocate new pool
        auto newPool = allocatePool(p_MvDevice, poolContainer->count);
        // use new pool to allocate set
        allocateSet(p_MvDevice, newPool, p_DescriptorLayout, p_DestinationSet);
        // change passed container to new one
        poolContainer = newPool;
    }
    else
    {
        throw std::runtime_error("Allocator failed to allocate descriptor set, fatal error");
    }
    return;
}

void mv::Allocator::allocateSet(const mv::Device &p_MvDevice, Container *p_PoolContainer,
                                vk::DescriptorSetLayout &p_DescriptorLayout, vk::DescriptorSet &p_DestinationSet)
{
    if (!p_PoolContainer)
        throw std::runtime_error("Invalid container passed to allocate set :: descriptor handler");

    // ensure pool exist
    if (!p_PoolContainer->pool)
        throw std::runtime_error("No pool ever allocated, attempting to allocate descriptor set");

    std::cout << "[-] Allocating descriptor set" << std::endl;
    if (!p_DescriptorLayout)
    {
        std::ostringstream oss;
        oss << "Descriptor allocated was requested to allocate a set however the user provided "
               "nullptr for layout\n";
        oss << "File => " << __FILE__ << "Line => " << __LINE__ << std::endl;
        throw std::runtime_error(oss.str().c_str());
    }

    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = p_PoolContainer->pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &p_DescriptorLayout;

    vk::Result result = p_MvDevice.logicalDevice->allocateDescriptorSets(&allocInfo, &p_DestinationSet);
    if (result == vk::Result::eSuccess)
    {
        // ensure allocator index is up to date
        currentPool = p_PoolContainer->index;
    }
    else if (result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool)
    {
        if (result == vk::Result::eErrorOutOfPoolMemory)
        {
            p_PoolContainer->status = Container::Status::Full;
        }
        else
        {
            p_PoolContainer->status = Container::Status::Fragmented;
        }
        // allocate new pool
        auto newPool = allocatePool(p_MvDevice, p_PoolContainer->count);
        // use new pool to allocate set
        allocateSet(p_MvDevice, newPool, p_DescriptorLayout, p_DestinationSet);
        // change passed container to new one
        p_PoolContainer = newPool;
    }
    else
    {
        throw std::runtime_error("Allocator failed to allocate descriptor set, fatal error");
    }
}

void mv::Allocator::updateSet(const mv::Device &p_MvDevice, vk::DescriptorBufferInfo &p_BufferDescriptor,
                              vk::DescriptorSet &p_TargetDescriptorSet, uint32_t p_DestinationBinding)
{
    Container *poolContainer = &containers->at(currentPool);

    if (!poolContainer)
        throw std::runtime_error("Failed to get current pool handle, updating set :: descriptor handler");

    // ensure pool exist
    if (!poolContainer->pool)
        throw std::runtime_error("No pool ever allocated, attempting to update descriptor set :: descriptor handler");

    vk::WriteDescriptorSet updateInfo;
    updateInfo.dstBinding = p_DestinationBinding;
    updateInfo.dstSet = p_TargetDescriptorSet;
    updateInfo.descriptorType = vk::DescriptorType::eUniformBuffer;
    updateInfo.descriptorCount = 1;
    updateInfo.pBufferInfo = &p_BufferDescriptor;

    p_MvDevice.logicalDevice->updateDescriptorSets(updateInfo, nullptr);
    currentPool = poolContainer->index;
    return;
}

// void mv::Allocator::updateSet(const mv::Device &p_MvDevice, Container *p_PoolContainer,
//                               vk::DescriptorBufferInfo &p_BufferDescriptor, vk::DescriptorSet &p_TargetDescriptorSet,
//                               uint32_t p_DestinationBinding)
// {
//     vk::WriteDescriptorSet updateInfo;
//     updateInfo.dstBinding = p_DestinationBinding;
//     updateInfo.dstSet = p_TargetDescriptorSet;
//     updateInfo.descriptorType = vk::DescriptorType::eUniformBuffer;
//     updateInfo.descriptorCount = 1;
//     updateInfo.pBufferInfo = &p_BufferDescriptor;

//     p_MvDevice.logicalDevice->updateDescriptorSets(updateInfo, nullptr);
//     currentPool = p_PoolContainer->index;
//     return;
// }

void mv::Allocator::updateSet(const mv::Device &p_MvDevice, vk::DescriptorImageInfo &p_ImageDescriptor,
                              vk::DescriptorSet &p_TargetDescriptorSet, uint32_t p_DestinationBinding)
{
    Container *poolContainer = &containers->at(currentPool);

    if (!poolContainer)
        throw std::runtime_error("Failed to get current pool handle, updating set :: descriptor handler");

    // ensure pool exist
    if (!poolContainer->pool)
        throw std::runtime_error("No pool ever allocated, attempting to update descriptor set :: descriptor handler");

    vk::WriteDescriptorSet updateInfo;
    updateInfo.dstBinding = p_DestinationBinding;
    updateInfo.dstSet = p_TargetDescriptorSet;
    updateInfo.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    updateInfo.descriptorCount = 1;
    updateInfo.pImageInfo = &p_ImageDescriptor;

    p_MvDevice.logicalDevice->updateDescriptorSets(updateInfo, nullptr);
    currentPool = poolContainer->index;
    return;
}

// void mv::Allocator::updateSet(const mv::Device &p_MvDevice, Container *p_PoolContainer,
//                               vk::DescriptorImageInfo &p_ImageDescriptor, vk::DescriptorSet &p_TargetDescriptorSet,
//                               uint32_t p_DestinationBinding)
// {
//     vk::WriteDescriptorSet updateInfo;
//     updateInfo.dstBinding = p_DestinationBinding;
//     updateInfo.dstSet = p_TargetDescriptorSet;
//     updateInfo.descriptorType = vk::DescriptorType::eCombinedImageSampler;
//     updateInfo.descriptorCount = 1;
//     updateInfo.pImageInfo = &p_ImageDescriptor;

//     p_MvDevice.logicalDevice->updateDescriptorSets(updateInfo, nullptr);
//     currentPool = p_PoolContainer->index;
//     return;
// }

/*
    CONTAINER METHODS
*/
mv::Allocator::Container::Container(struct ContainerInitStruct &p_ContainerInitStruct)
{
    this->parentAllocator = p_ContainerInitStruct.parentAllocator;
    this->poolContainersArray = p_ContainerInitStruct.poolContainersArray;
    this->count = p_ContainerInitStruct.count;
}

mv::Allocator::Container::~Container()
{
    return;
}

struct mv::Allocator::Container *mv::Allocator::allocatePool(const mv::Device &p_MvDevice, uint32_t p_Count)
{

    std::cout << "[+] Allocating descriptor pool of max sets => " << p_Count << std::endl;
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eSampler, 1000},
        {vk::DescriptorType::eCombinedImageSampler, 1000},
        {vk::DescriptorType::eSampledImage, 1000},
        {vk::DescriptorType::eStorageImage, 1000},
        {vk::DescriptorType::eUniformTexelBuffer, 1000},
        {vk::DescriptorType::eStorageTexelBuffer, 1000},
        {vk::DescriptorType::eUniformBuffer, 1000},
        {vk::DescriptorType::eStorageBuffer, 1000},
        {vk::DescriptorType::eUniformBufferDynamic, 1000},
        {vk::DescriptorType::eStorageBufferDynamic, 1000},
        {vk::DescriptorType::eInputAttachment, 1000},
    };

    if (poolSizes.empty())
        throw std::runtime_error("Unsupported descriptor pool type requested");

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = p_Count;

    struct ContainerInitStruct containerInitStruct;
    containerInitStruct.parentAllocator = this;
    containerInitStruct.poolContainersArray = containers.get();
    containerInitStruct.count = p_Count;

    // assign `self` after move to vector
    Container np(containerInitStruct);
    np.pool = p_MvDevice.logicalDevice->createDescriptorPool(poolInfo);
    np.status = Container::Status::Clear;
    containers->push_back(std::move(np));
    // give object its addr & index
    containers->back().self = &containers->back();
    containers->back().index = containers->size() - 1;
    currentPool = containers->size() - 1;

    return &containers->back();
}