#pragma once

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

class Model;
class Engine;

class Allocator
{
public:
  struct ContainerInitStruct;
  class Container
  {
  public:
    enum Status
    {
      Clear,
      Usable,
      Fragmented,
      Full
    };

    Container (ContainerInitStruct &p_ContainerInitStruct);
    ~Container ();

    // Disallow copy
    Container (const Container &) = delete;
    Container &operator= (const Container &) = delete;

    // Allow move
    Container (Container &&) = default;
    Container &operator= (Container &&) = delete;

    // owns
    vk::DescriptorPool pool;

    // references
    Allocator *parentAllocator = nullptr;
    std::vector<Container> *poolContainersArray = nullptr; // pointer to container for all pools

    // infos
    uint32_t index = 0;      // index to self in pools_array
    uint32_t count = 0;      // max sets requested from this pool on allocation
    vk::DescriptorType type; // type of descriptors this pool was created for
    Container *self = nullptr;
    Container::Status status = Status::Clear;
  };

private:
  Container *retryAllocateSet (vk::DescriptorSetLayout p_DescriptorLayout,
                               vk::DescriptorSet p_DestinationDescriptorSet,
                               uint32_t p_PoolMaxDescriptorSets);

public:
  Engine *engine = nullptr;

  std::vector<Container> containers;
  std::unordered_map<vk::DescriptorType, vk::DescriptorSetLayout> layouts;

  // infos
  uint32_t currentPool = 0;

  // disallow copy
  Allocator (const Allocator &) = delete;
  Allocator &operator= (const Allocator &) = delete;

  // allow move
  Allocator (Allocator &&) = default;
  Allocator &operator= (Allocator &&) = default;

  Allocator (Engine *p_Engine);
  ~Allocator ();

  // Load model
  void loadModel (std::vector<Model> *p_Models,
                  std::vector<std::string> *p_ModelNames,
                  const char *p_Filename);

  // Creates object with specified model data
  void createObject (std::vector<Model> *p_Models, std::string p_ModelName);

  void cleanup (void);

  // returns handle to current pool in use
  Container *get (void);

  Container *allocatePool (uint32_t p_Count);

  void createLayout (vk::DescriptorType p_DescriptorType,
                     uint32_t p_Count,
                     vk::ShaderStageFlagBits p_ShaderStageFlags,
                     uint32_t p_Binding);

  void createLayout (vk::DescriptorSetLayoutCreateInfo &p_CreateInfo);

  vk::DescriptorSetLayout getLayout (vk::DescriptorType p_LayoutType);

  /*
      Allocate descriptor set
      Manual descriptor pool container selection
  */
  void allocateSet (vk::DescriptorSetLayout &p_DescriptorLayout,
                    vk::DescriptorSet &p_DestinationSet);

  /*
      Allocate descriptor set
      Automatically uses latest allocated pool as source for allocation
  */
  void allocateSet (Container *p_PoolContainer,
                    vk::DescriptorSetLayout &p_DescriptorLayout,
                    vk::DescriptorSet &p_DestinationSet);

  /*
      Bind a buffer descriptor to a target descriptor set
      Automatically selects latest allocated pool
  */
  void updateSet (vk::DescriptorBufferInfo &p_BufferDescriptor,
                  vk::DescriptorSet &p_TargetDescriptorSet,
                  uint32_t p_DestinationBinding);

  /*
    Bind a buffer descriptor to target descriptor set
    Allows for manual selection of descriptorType
    Automatically selects latest allocated pool
  */
  void updateSet (vk::DescriptorBufferInfo &p_BufferDescriptor,
                  vk::DescriptorSet &p_TargetDescriptorSet,
                  vk::DescriptorType p_DescriptorType,
                  uint32_t p_DestinationBinding);

  /*
      Bind buffer descriptor to target descriptor set
      Manual pool container selection
  */
  // void updateSet(const vk::Device &p_LogicalDevice, Container
  // *p_PoolContainer,
  //                vk::DescriptorBufferInfo &p_BufferDescriptor,
  //                vk::DescriptorSet &p_TargetDescriptorSet, uint32_t
  //                p_DestinationBinding);

  /*
      Bind image descriptor to a target descriptor set
      Automatically selects latest allocated pool
  */
  void updateSet (vk::DescriptorImageInfo &p_ImageDescriptor,
                  vk::DescriptorSet &p_TargetDescriptorSet,
                  uint32_t p_DestinationBinding);

  /*
      Bind image descriptor to a target descriptor set
      Manual pool container selection
  */
  // void updateSet(const vk::Device &p_LogicalDevice, Container
  // *p_PoolContainer,
  //                vk::DescriptorImageInfo &p_ImageDescriptor,
  //                vk::DescriptorSet &p_TargetDescriptorSet, uint32_t
  //                p_DestinationBinding);

  struct ContainerInitStruct
  {
    uint32_t count = 1000; // max sets specified at pool allocation

    // ptr to allocator all pools have been allocated with
    Allocator *parentAllocator = nullptr;
    // ptr to container for all pools
    std::vector<Allocator::Container> *poolContainersArray = nullptr;
  };

private:
  std::string descriptorTypeToString (vk::DescriptorType p_Type);
};
