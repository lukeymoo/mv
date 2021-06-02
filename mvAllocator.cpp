#include "mvAllocator.h"
#include "mvEngine.h"
#include "mvModel.h"

// For LogHandler
#include "mvHelper.h"

extern LogHandler logger;

Allocator::Allocator (Engine *p_Engine)
{
  if (!p_Engine)
    throw std::runtime_error ("Invalid engine handle passed to allocator");
  engine = p_Engine;
  return;
}

Allocator::~Allocator () {}

Allocator::Container *
Allocator::get (void)
{
  return &containers.at (currentPool);
}

void
Allocator::cleanup (void)
{
  if (!layouts.empty ())
    {
      for (auto &layout : layouts)
        {
          if (layout.second)
            engine->logicalDevice.destroyDescriptorSetLayout (layout.second);
        }
    }

  if (!containers.empty ())
    {
      for (auto &container : containers)
        {
          if (container.pool)
            engine->logicalDevice.destroyDescriptorPool (container.pool);
        }
    }
  return;
}

vk::DescriptorSetLayout
Allocator::getLayout (vk::DescriptorType p_LayoutType)
{
  if (layouts.empty ())
    throw std::runtime_error ("Attempted to get layout " + descriptorTypeToString (p_LayoutType)
                              + " but none were created");

  for (const auto &map : layouts)
    {
      if (map.first == p_LayoutType)
        {
          return map.second;
        }
    }
  throw std::runtime_error ("Requested layout but not found => "
                            + descriptorTypeToString (p_LayoutType));
}

void
Allocator::createLayout (vk::DescriptorType p_DescriptorType,
                         uint32_t p_Count,
                         vk::ShaderStageFlags p_ShaderStageFlags,
                         uint32_t p_Binding)
{
  vk::DescriptorSetLayoutBinding bindInfo;
  bindInfo.binding = p_Binding;
  bindInfo.descriptorType = p_DescriptorType;
  bindInfo.descriptorCount = p_Count;
  bindInfo.stageFlags = p_ShaderStageFlags;

  vk::DescriptorSetLayoutCreateInfo layoutInfo;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &bindInfo;

  createLayout (layoutInfo);
}

void
Allocator::createLayout (vk::DescriptorSetLayoutCreateInfo &p_CreateInfo)
{
  bool layoutAlreadyExists = false;
  vk::DescriptorType requestedLayoutType = p_CreateInfo.pBindings->descriptorType;
  if (!layouts.empty ())
    {
      // ensure name not already created
      for (const auto &map : layouts)
        {
          if (map.first == requestedLayoutType)
            {
              layoutAlreadyExists = true;
              break;
            }
        }
    }

  if (layoutAlreadyExists)
    {
      logger.logMessage (LogHandlerMessagePriority::eWarning,
                         "Request to create descriptor set layout => "
                             + descriptorTypeToString (requestedLayoutType)
                             + " already exists; Skipping...");
      return;
    }

  // add to map
  layouts.insert ({
      requestedLayoutType,
      engine->logicalDevice.createDescriptorSetLayout (p_CreateInfo),
  });
  logger.logMessage ("Descriptor set layout => " + descriptorTypeToString (requestedLayoutType)
                     + " created");
  return;
}

void
Allocator::allocateSet (vk::DescriptorSetLayout &p_DescriptorLayout,
                        vk::DescriptorSet &p_DestinationSet)
{
  Container *poolContainer = &containers.at (currentPool);

  // ensure pool exist
  if (!poolContainer || !poolContainer->pool)
    throw std::runtime_error ("No pool ever allocated, attempting to allocate descriptor set");

  logger.logMessage ("Allocating descriptor set");

  if (!p_DescriptorLayout)
    {
      std::ostringstream oss;
      oss << "Descriptor allocated was requested to allocate a set however "
             "the user provided "
             "nullptr for layout\n";
      oss << "File => " << __FILE__ << "Line => " << __LINE__ << std::endl;
      throw std::runtime_error (oss.str ().c_str ());
    }

  vk::DescriptorSetAllocateInfo allocInfo;
  allocInfo.descriptorPool = poolContainer->pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &p_DescriptorLayout;

  vk::Result result = engine->logicalDevice.allocateDescriptorSets (&allocInfo, &p_DestinationSet);

  switch (result)
    {
      using enum vk::Result;
    case eSuccess:
      {
        // ensure allocator index is up to date
        currentPool = poolContainer->index;
        break;
      }
    case eErrorOutOfPoolMemory:
      {
        poolContainer->status = Container::Status::Full;
        // allocate new pool
        // use new pool to allocate set
        // change passed container to new one
        poolContainer
            = retryAllocateSet (p_DescriptorLayout, p_DestinationSet, poolContainer->count);
        break;
      }
    case eErrorFragmentedPool:
      {
        poolContainer->status = Container::Status::Fragmented;
        // allocate new pool
        // use new pool to allocate set
        // change passed container to new one
        poolContainer
            = retryAllocateSet (p_DescriptorLayout, p_DestinationSet, poolContainer->count);
        break;
      }
    default:
      {
        throw std::runtime_error ("Allocator failed to allocate descriptor set, fatal error");
      }
    };
  return;
}

void
Allocator::allocateSet (Container *p_PoolContainer,
                        vk::DescriptorSetLayout &p_DescriptorLayout,
                        vk::DescriptorSet &p_DestinationSet)
{
  // sanity check container object
  if (!p_PoolContainer)
    throw std::runtime_error ("Invalid container passed to allocate set :: descriptor handler");

  // ensure pool exist
  if (!p_PoolContainer->pool)
    throw std::runtime_error ("No pool ever allocated, attempting to allocate descriptor set");

  if (!p_DescriptorLayout)
    {
      std::ostringstream oss;
      oss << "Descriptor allocated was requested to allocate a set however "
             "the user provided "
             "nullptr for layout\n";
      oss << "File => " << __FILE__ << "Line => " << __LINE__ << std::endl;
      throw std::runtime_error (oss.str ().c_str ());
    }

  logger.logMessage ("Allocating descriptor set");

  vk::DescriptorSetAllocateInfo allocInfo;
  allocInfo.descriptorPool = p_PoolContainer->pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &p_DescriptorLayout;

  vk::Result result = engine->logicalDevice.allocateDescriptorSets (&allocInfo, &p_DestinationSet);
  switch (result)
    {
      using enum vk::Result;
    case eSuccess:
      {
        // ensure allocator index is up to date
        currentPool = p_PoolContainer->index;
        break;
      }
    case eErrorOutOfPoolMemory:
      {
        p_PoolContainer->status = Container::Status::Full;
        // allocate new pool
        // use new pool to allocate set
        // change passed container to new one
        p_PoolContainer
            = retryAllocateSet (p_DescriptorLayout, p_DestinationSet, p_PoolContainer->count);
        break;
      }
    case eErrorFragmentedPool:
      {
        p_PoolContainer->status = Container::Status::Fragmented;
        // allocate new pool
        // use new pool to allocate set
        // change passed container to new one
        p_PoolContainer
            = retryAllocateSet (p_DescriptorLayout, p_DestinationSet, p_PoolContainer->count);
        break;
      }
    default:
      {
        throw std::runtime_error ("Allocator failed to allocate descriptor set, fatal error");
      }
    };
}

Allocator::Container *
Allocator::retryAllocateSet (vk::DescriptorSetLayout p_DescriptorLayout,
                             vk::DescriptorSet p_DestinationDescriptorSet,
                             uint32_t p_PoolMaxDescriptorSets)
{
  auto newPool = allocatePool (p_PoolMaxDescriptorSets);
  allocateSet (newPool, p_DescriptorLayout, p_DestinationDescriptorSet);
  return newPool;
}

// void
// Allocator::updateSet (vk::DescriptorBufferInfo &p_BufferDescriptor,
//                       vk::DescriptorSet &p_TargetDescriptorSet,
//                       uint32_t p_DestinationBinding)
// {
//   Container *poolContainer = &containers.at (currentPool);

//   if (!poolContainer)
//     throw std::runtime_error ("Failed to get current pool handle, updating "
//                               "set :: descriptor handler");

//   // ensure pool exist
//   if (!poolContainer->pool)
//     throw std::runtime_error ("No pool ever allocated, attempting to update "
//                               "descriptor set :: descriptor handler");

//   vk::WriteDescriptorSet updateInfo;
//   updateInfo.dstBinding = p_DestinationBinding;
//   updateInfo.dstSet = p_TargetDescriptorSet;
//   updateInfo.descriptorType = vk::DescriptorType::eUniformBuffer;
//   updateInfo.descriptorCount = 1;
//   updateInfo.pBufferInfo = &p_BufferDescriptor;

//   engine->logicalDevice.updateDescriptorSets (updateInfo, nullptr);
//   currentPool = poolContainer->index;
//   return;
// }

void
Allocator::updateSet (vk::DescriptorBufferInfo &p_BufferDescriptor,
                      vk::DescriptorSet &p_TargetDescriptorSet,
                      vk::DescriptorType p_DescriptorType,
                      uint32_t p_DestinationBinding)
{
  Container *poolContainer = &containers.at (currentPool);

  if (!poolContainer)
    throw std::runtime_error ("Failed to get current pool handle, updating "
                              "set :: descriptor handler");

  // ensure pool exist
  if (!poolContainer->pool)
    throw std::runtime_error ("No pool ever allocated, attempting to update "
                              "descriptor set :: descriptor handler");

  vk::WriteDescriptorSet updateInfo;
  updateInfo.dstBinding = p_DestinationBinding;
  updateInfo.dstSet = p_TargetDescriptorSet;
  updateInfo.descriptorType = p_DescriptorType;
  updateInfo.descriptorCount = 1;
  updateInfo.pBufferInfo = &p_BufferDescriptor;

  engine->logicalDevice.updateDescriptorSets (updateInfo, nullptr);
  currentPool = poolContainer->index;
  return;
}

// void Allocator::updateSet(const vk::Device &p_LogicalDevice, Container
// *p_PoolContainer,
//                               vk::DescriptorBufferInfo &p_BufferDescriptor,
//                               vk::DescriptorSet &p_TargetDescriptorSet,
//                               uint32_t p_DestinationBinding)
// {
//     vk::WriteDescriptorSet updateInfo;
//     updateInfo.dstBinding = p_DestinationBinding;
//     updateInfo.dstSet = p_TargetDescriptorSet;
//     updateInfo.descriptorType = vk::DescriptorType::eUniformBuffer;
//     updateInfo.descriptorCount = 1;
//     updateInfo.pBufferInfo = &p_BufferDescriptor;

//     p_LogicalDevice.logicalDevice.updateDescriptorSets(updateInfo, nullptr);
//     currentPool = p_PoolContainer->index;
//     return;
// }

void
Allocator::updateSet (vk::DescriptorImageInfo &p_ImageDescriptor,
                      vk::DescriptorSet &p_TargetDescriptorSet,
                      uint32_t p_DestinationBinding)
{
  Container *poolContainer = &containers.at (currentPool);

  if (!poolContainer)
    throw std::runtime_error ("Failed to get current pool handle, updating "
                              "set :: descriptor handler");

  // ensure pool exist
  if (!poolContainer->pool)
    throw std::runtime_error ("No pool ever allocated, attempting to update "
                              "descriptor set :: descriptor handler");

  vk::WriteDescriptorSet updateInfo;
  updateInfo.dstBinding = p_DestinationBinding;
  updateInfo.dstSet = p_TargetDescriptorSet;
  updateInfo.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  updateInfo.descriptorCount = 1;
  updateInfo.pImageInfo = &p_ImageDescriptor;

  engine->logicalDevice.updateDescriptorSets (updateInfo, nullptr);
  currentPool = poolContainer->index;
  return;
}

// void Allocator::updateSet(const vk::Device &p_LogicalDevice, Container
// *p_PoolContainer,
//                               vk::DescriptorImageInfo &p_ImageDescriptor,
//                               vk::DescriptorSet &p_TargetDescriptorSet,
//                               uint32_t p_DestinationBinding)
// {
//     vk::WriteDescriptorSet updateInfo;
//     updateInfo.dstBinding = p_DestinationBinding;
//     updateInfo.dstSet = p_TargetDescriptorSet;
//     updateInfo.descriptorType = vk::DescriptorType::eCombinedImageSampler;
//     updateInfo.descriptorCount = 1;
//     updateInfo.pImageInfo = &p_ImageDescriptor;

//     p_LogicalDevice.logicalDevice.updateDescriptorSets(updateInfo, nullptr);
//     currentPool = p_PoolContainer->index;
//     return;
// }

/*
    CONTAINER METHODS
*/
Allocator::Container::Container (ContainerInitStruct &p_ContainerInitStruct)
{
  this->parentAllocator = p_ContainerInitStruct.parentAllocator;
  this->poolContainersArray = p_ContainerInitStruct.poolContainersArray;
  this->count = p_ContainerInitStruct.count;
}

Allocator::Container::~Container () { return; }

Allocator::Container *
Allocator::allocatePool (uint32_t p_Count)
{
  logger.logMessage ("[+] Allocating descriptor pool of max sets => " + std::to_string (p_Count));

  using enum vk::DescriptorType;
  std::vector<vk::DescriptorPoolSize> poolSizes = {
    { eSampler, 1000 },
    { eCombinedImageSampler, 1000 },
    { eSampledImage, 1000 },
    { eStorageImage, 1000 },
    { eUniformTexelBuffer, 1000 },
    { eStorageTexelBuffer, 1000 },
    { eUniformBuffer, 1000 },
    { eStorageBuffer, 1000 },
    { eUniformBufferDynamic, 1000 },
    { eStorageBufferDynamic, 1000 },
    { eInputAttachment, 1000 },
  };

  if (poolSizes.empty ())
    throw std::runtime_error ("Unsupported descriptor pool type requested");

  vk::DescriptorPoolCreateInfo poolInfo;
  poolInfo.poolSizeCount = static_cast<uint32_t> (poolSizes.size ());
  poolInfo.pPoolSizes = poolSizes.data ();
  poolInfo.maxSets = p_Count;

  struct ContainerInitStruct containerInitStruct;
  containerInitStruct.parentAllocator = this;
  containerInitStruct.poolContainersArray = &containers;
  containerInitStruct.count = p_Count;

  // assign `self` after move to vector
  Container np (containerInitStruct);
  np.pool = engine->logicalDevice.createDescriptorPool (poolInfo);
  np.status = Container::Status::Clear;
  containers.push_back (std::move (np));
  // give object its addr & index
  containers.back ().self = &containers.back ();
  containers.back ().index = containers.size () - 1;
  currentPool = containers.size () - 1;

  return &containers.back ();
}

void
Allocator::loadModel (std::vector<Model> *p_Models,
                      std::vector<std::string> *p_ModelNames,
                      const char *p_Filename)
{
  bool alreadyLoaded = false;
  for (const auto &name : *p_ModelNames)
    {
      if (name.compare (p_Filename) == 0) // if same
        {
          logger.logMessage (LogHandlerMessagePriority::eWarning,
                             "Skipping already loaded model name => " + std::string (p_Filename));
          alreadyLoaded = true;
          break;
        }
    }

  if (!alreadyLoaded)
    {
      logger.logMessage ("Loading model => " + std::string (p_Filename));
      // make space for new model
      p_Models->push_back (Model ());
      // call model routine _load
      p_Models->back ().load (engine, *this, p_Filename, false);

      // add filename to model_names container
      p_ModelNames->push_back (p_Filename);
    }
  return;
}

void
Allocator::createObject (std::vector<Model> *p_Models, std::string p_ModelName)
{
  if (!p_Models)
    throw std::runtime_error ("Attempted to create object, invalid model "
                              "container passed to allocator");

  // ensure model exists
  bool modelExist = false;
  bool modelIndex = 0;

  for (auto &model : *p_Models)
    {
      if (model.modelName == p_ModelName)
        {
          modelExist = true;
          modelIndex = (&model - &(*p_Models)[0]);
        }
    }

  if (!modelExist)
    {
      std::ostringstream oss;
      oss << "[!] Requested creation of object of model type => " << p_ModelName
          << " failed as that model has never been loaded" << std::endl;
      throw std::runtime_error (oss.str ().c_str ());
    }

  logger.logMessage ("Creating object of model type => " + p_ModelName);

  // create new object element in specified model
  p_Models->at (modelIndex).objects->push_back (Object ());

  logger.logMessage (" -- Model type object count is now => "
                     + std::to_string (p_Models->at (modelIndex).objects->size ()));

  // object's std pair uniform
  // { vk::DescriptorSet, Buffer }
  engine->createBuffer (vk::BufferUsageFlagBits::eUniformBuffer,
                        vk::MemoryPropertyFlagBits::eHostVisible
                            | vk::MemoryPropertyFlagBits::eHostCoherent,
                        &p_Models->at (modelIndex).objects->back ().uniform.second,
                        sizeof (Object::Matrix));
  p_Models->at (modelIndex).objects->back ().uniform.second.map (engine->logicalDevice);

  vk::DescriptorSetLayout uniformLayout = getLayout (vk::DescriptorType::eUniformBuffer);

  allocateSet (uniformLayout, p_Models->at (modelIndex).objects->back ().uniform.first);
  updateSet (p_Models->at (modelIndex).objects->back ().uniform.second.bufferInfo,
             p_Models->at (modelIndex).objects->back ().uniform.first,
             vk::DescriptorType::eUniformBuffer,
             0);
  return;
}

std::string
Allocator::descriptorTypeToString (vk::DescriptorType p_Type)
{
  switch (p_Type)
    {
      using enum vk::DescriptorType;
    case eCombinedImageSampler:
      {
        return "[Combined Image Sampler]";
        break;
      }
    case eUniformBuffer:
      {
        return "[Uniform Buffer]";
        break;
      }
    default:
      {
        return "[Unsupported descriptor type]";
        break;
      }
    }
}