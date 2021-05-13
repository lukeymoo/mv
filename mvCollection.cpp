#include "mvCollection.h"

mv::Collection::Collection(const mv::Device &p_MvDevice)
{

    // initialize containers
    viewUniform = std::make_unique<struct mv::UniformObject>();
    projectionUniform = std::make_unique<struct mv::UniformObject>();
    models = std::make_unique<std::vector<mv::Model>>();

    // create uniform buffer for view matrix
    p_MvDevice.createBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                            &viewUniform->mvBuffer, sizeof(struct mv::UniformObject));
    viewUniform->mvBuffer.map(p_MvDevice);

    // create uniform buffer for projection matrix
    p_MvDevice.createBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                            &projectionUniform->mvBuffer, sizeof(struct mv::UniformObject));
    projectionUniform->mvBuffer.map(p_MvDevice);
}

mv::Collection::~Collection()
{
}

void mv::Collection::loadModel(const mv::Device &p_MvDevice, mv::Allocator &p_DescriptorAllocator,
                               const char *p_Filename)
{
    bool alreadyLoaded = false;
    for (const auto &name : modelNames)
    {
        if (name.compare(p_Filename) == 0) // if same
        {
            std::cout << "[-] Skipping already loaded model name => " << p_Filename << "\n";
            alreadyLoaded = true;
            break;
        }
    }

    if (!alreadyLoaded)
    {
        std::cout << "[+] Loading model => " << p_Filename << "\n";
        // make space for new model
        models->push_back(mv::Model());
        // call model routine _load
        models->back().load(p_MvDevice, p_DescriptorAllocator, p_Filename);

        // add filename to model_names container
        modelNames.push_back(p_Filename);
    }
    return;
}

void mv::Collection::createObject(const mv::Device &p_MvDevice, mv::Allocator &p_DescriptorAllocator,
                                  std::string p_ModelName)
{

    if (!models)
        throw std::runtime_error("Requested to create object but models container never "
                                 "initialized :: collection handler");

    // ensure model exists
    bool modelExist = false;
    bool modelIndex = 0;

    for (auto &model : *models)
    {
        if (model.modelName == p_ModelName)
        {
            modelExist = true;
            modelIndex = (&model - &(*models)[0]);
        }
    }

    if (!modelExist)
    {
        std::ostringstream oss;
        oss << "[!] Requested creation of object of model type => " << p_ModelName
            << " failed as that model has never been loaded" << std::endl;
        throw std::runtime_error(oss.str().c_str());
    }

    std::cout << "[+] Creating object of model type => " << p_ModelName << std::endl;

    // create new object element in specified model
    models->at(modelIndex).objects->push_back(mv::Object());

    std::cout << "\t -- Model type object count is now => " << models->at(modelIndex).objects->size() << "\n";

    // create uniform buffer for object
    p_MvDevice.createBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                            &models->at(modelIndex).objects->back().uniformBuffer, sizeof(Object::Matrix));
    models->at(modelIndex).objects->back().uniformBuffer.map(p_MvDevice);

    vk::DescriptorSetLayout uniformLayout = p_DescriptorAllocator.getLayout("uniform_layout");

    // allocate descriptor set for object's model uniform
    p_DescriptorAllocator.allocateSet(p_MvDevice, uniformLayout, models->at(modelIndex).objects->back().meshDescriptor);
    p_DescriptorAllocator.updateSet(p_MvDevice, models->at(modelIndex).objects->back().uniformBuffer.descriptor,
                                    models->at(modelIndex).objects->back().meshDescriptor, 0);
    return;
}

void mv::Collection::update(void)
{
    if (!models)
        throw std::runtime_error("Collection handler was requested to update but models never "
                                 "initialized :: collection handler");
    for (auto &model : *models)
    {
        if (!model.objects)
        {
            std::ostringstream oss;
            oss << "In collection handler update of objects => " << model.modelName
                << " object container never initialized\n";
            throw std::runtime_error(oss.str());
        }
        for (auto &object : *model.objects)
        {
            object.update();
        }
    }
    return;
}

void mv::Collection::cleanup(const mv::Device &p_MvDevice, mv::Allocator &p_DescriptorAllocator)
{
    p_MvDevice.logicalDevice->waitIdle();

    // cleanup descriptor_allocator
    p_DescriptorAllocator.cleanup(p_MvDevice);

    // cleanup models & objects buffers
    for (auto &model : *models)
    {
        if (!model.objects)
        {
            std::ostringstream oss;
            oss << "In collection handler cleanup of objects => " << model.modelName
                << " object container never initialized\n";
            throw std::runtime_error(oss.str());
        }
        if (!model.loadedMeshes)
        {
            std::ostringstream oss;
            oss << "In collection handler cleanup of objects => " << model.modelName
                << " mesh container never initialized\n";
            throw std::runtime_error(oss.str());
        }
        for (auto &object : *model.objects)
        {
            object.uniformBuffer.destroy(p_MvDevice);
        }
        for (auto &mesh : *model.loadedMeshes)
        {
            mesh.cleanup(p_MvDevice);
        }
    }

    viewUniform->mvBuffer.destroy(p_MvDevice);
    projectionUniform->mvBuffer.destroy(p_MvDevice);
    return;
}