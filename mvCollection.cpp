#include "mvCollection.h"
#include "mvAllocator.h"
#include "mvBuffer.h"
#include "mvModel.h"

extern LogHandler logger;

Collection::Collection (Engine *p_Engine)
{
  if (!p_Engine)
    throw std::runtime_error ("Invalid engine object passed to collection");

  engine = p_Engine;

  // Initialize models container
  models = std::make_unique<std::vector<Model>> ();

  // Initialize compute storage
  computeStorageBuffer = std::make_unique<MvBuffer> ();

  // Initialize view & projection uniform ptrs
  viewUniform = std::make_unique<UniformObject> ();
  projectionUniform = std::make_unique<UniformObject> ();

  // create uniform buffer for view matrix
  engine->createBuffer (vk::BufferUsageFlagBits::eUniformBuffer,
                        vk::MemoryPropertyFlagBits::eHostVisible
                            | vk::MemoryPropertyFlagBits::eHostCoherent,
                        &viewUniform->mvBuffer,
                        sizeof (UniformObject));
  viewUniform->mvBuffer.map (p_Engine->logicalDevice);

  // create uniform buffer for projection matrix
  p_Engine->createBuffer (vk::BufferUsageFlagBits::eUniformBuffer,
                          vk::MemoryPropertyFlagBits::eHostVisible
                              | vk::MemoryPropertyFlagBits::eHostCoherent,
                          &projectionUniform->mvBuffer,
                          sizeof (UniformObject));
  projectionUniform->mvBuffer.map (p_Engine->logicalDevice);
}

Collection::~Collection () {}

void
Collection::update (void)
{
  if (!models)
    throw std::runtime_error ("Collection handler was requested to update but models never "
                              "initialized :: collection handler");
  for (auto &model : *models)
    {
      if (!model.objects)
        {
          std::ostringstream oss;
          oss << "In collection handler update of objects => " << model.modelName
              << " object container never initialized\n";
          throw std::runtime_error (oss.str ());
        }
      for (auto &object : *model.objects)
        {
          object.update ();
        }
    }
  return;
}

void
Collection::cleanup (void)
{
  engine->logicalDevice.waitIdle ();

  // cleanup models & objects buffers
  if (models)
    for (auto &model : *models)
      {
        model.cleanup (engine);

        if (model.objects)
          for (auto &object : *model.objects)
            {
              object.uniform.second.destroy (engine->logicalDevice);
            }
      }
  if (viewUniform)
    viewUniform->mvBuffer.destroy (engine->logicalDevice);
  if (projectionUniform)
    projectionUniform->mvBuffer.destroy (engine->logicalDevice);
  if (computeStorageBuffer)
    computeStorageBuffer->destroy (engine->logicalDevice);
  return;
}

uint32_t
Collection::getObjectCount (void)
{
  uint32_t count = 0;
  for (const auto &model : *models)
    {
      count += model.objects->size ();
    }
  return count;
}

uint32_t
Collection::getTriangleCount (void)
{
  uint32_t count = 0;
  for (const auto &model : *models)
    {
      count += model.triangleCount * model.objects->size ();
    }
  return count;
}

uint32_t
Collection::getVertexCount (void)
{
  uint32_t count = 0;
  for (const auto &model : *models)
    {
      count += model.totalIndices * model.objects->size ();
    }
  return count;
}