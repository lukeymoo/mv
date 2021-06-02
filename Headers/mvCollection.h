#pragma once
#include <ostream>
#include <vulkan/vulkan.hpp>

struct UniformObject;
class Model;
class Engine;
class MvBuffer;

struct Collection
{
  bool shouldOutputDebug = false;

  // owns
  std::unique_ptr<std::vector<Model>> models;

  // infos
  std::vector<std::string> modelNames;
  Engine *engine = nullptr;

  // debug storage buffer
  std::unique_ptr<MvBuffer> computeStorageBuffer;

  // view & projection matrix objects
  std::unique_ptr<UniformObject> viewUniform;
  std::unique_ptr<UniformObject> projectionUniform;

  Collection (Engine *p_Engine);
  ~Collection ();

  // Calls update for each object
  void update (void);

  // Destroys Vulkan resources allocated by this handler
  void cleanup (void);

  uint32_t getObjectCount (void);
  uint32_t getTriangleCount (void);
  uint32_t getVertexCount (void);
};
