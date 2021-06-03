#pragma once
#include <ostream>
#include <vulkan/vulkan.hpp>

struct UniformObject;
class Model;
class Engine;
class MvBuffer;

struct Collection
{
  Engine *engine = nullptr;

  // owns
  std::vector<std::string> modelNames;
  std::unique_ptr<std::vector<Model>> models;

  // view & projection matrix objects
  std::unique_ptr<UniformObject> viewUniform;
  std::unique_ptr<UniformObject> projectionUniform;

  Collection (Engine *p_Engine);
  ~Collection ();

  // Calls update for each object
  void update (void);

  // Destroys Vulkan resources allocated by this handler
  void cleanup (void);

  // Loads new model
  void addNewModel (Container *pool, const char *filename);

  uint32_t getObjectCount (void);
  uint32_t getTriangleCount (void);
  uint32_t getVertexCount (void);
};
