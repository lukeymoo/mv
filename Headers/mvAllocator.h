#ifndef HEADERS_MVALLOCATOR_H_
#define HEADERS_MVALLOCATOR_H_

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "mvDevice.h"
#include "mvHelper.h"

namespace mv
{
    struct Allocator
    {
        bool shouldOutputDebug = false;
        // forward decl
        struct Container;

        // owns
        std::unique_ptr<std::vector<struct Container>> containers;
        std::unique_ptr<std::unordered_map<std::string, vk::DescriptorSetLayout>> layouts;

        // infos
        uint32_t currentPool = 0;

        // disallow copy
        Allocator(const Allocator &) = delete;
        Allocator &operator=(const Allocator &) = delete;

        // allow move
        Allocator(Allocator &&) = default;
        Allocator &operator=(Allocator &&) = default;

        Allocator(void);
        ~Allocator();

        void cleanup(const mv::Device &p_MvDevice);

        // returns handle to current pool in use
        Container *get(void);

        void createLayout(const mv::Device &p_MvDevice, std::string p_LayoutName, vk::DescriptorType p_DescriptorType,
                          uint32_t p_Count, vk::ShaderStageFlagBits p_ShaderStageFlags, uint32_t p_Binding);

        void createLayout(const mv::Device &p_MvDevice, std::string p_LayoutName,
                          vk::DescriptorSetLayoutCreateInfo &p_CreateInfo);

        vk::DescriptorSetLayout getLayout(std::string p_LayoutName);

        /*
            Allocate descriptor set
            Manual descriptor pool container selection
        */
        void allocateSet(const mv::Device &p_MvDevice, vk::DescriptorSetLayout &p_DescriptorLayout,
                         vk::DescriptorSet &p_DestinationSet);

        /*
            Allocate descriptor set
            Automatically uses latest allocated pool as source for allocation
        */
        void allocateSet(const mv::Device &p_MvDevice, Container *p_PoolContainer,
                         vk::DescriptorSetLayout &p_DescriptorLayout, vk::DescriptorSet &p_DestinationSet);

        /*
            Bind a buffer descriptor to a target descriptor set
            Automatically selects latest allocated pool
        */
        void updateSet(const mv::Device &p_MvDevice, vk::DescriptorBufferInfo &p_BufferDescriptor,
                       vk::DescriptorSet &p_TargetDescriptorSet, uint32_t p_DestinationBinding);

        /*
            Bind buffer descriptor to target descriptor set
            Manual pool container selection
        */
        // void updateSet(const mv::Device &p_MvDevice, Container *p_PoolContainer,
        //                vk::DescriptorBufferInfo &p_BufferDescriptor, vk::DescriptorSet &p_TargetDescriptorSet,
        //                uint32_t p_DestinationBinding);

        /*
            Bind image descriptor to a target descriptor set
            Automatically selects latest allocated pool
        */
        void updateSet(const mv::Device &p_MvDevice, vk::DescriptorImageInfo &p_ImageDescriptor,
                       vk::DescriptorSet &p_TargetDescriptorSet, uint32_t p_DestinationBinding);

        /*
            Bind image descriptor to a target descriptor set
            Manual pool container selection
        */
        // void updateSet(const mv::Device &p_MvDevice, Container *p_PoolContainer,
        //                vk::DescriptorImageInfo &p_ImageDescriptor, vk::DescriptorSet &p_TargetDescriptorSet,
        //                uint32_t p_DestinationBinding);

        struct ContainerInitStruct
        {
            uint32_t count = 1000; // max sets specified at pool allocation

            // READ ONLY USE
            mv::Allocator *parentAllocator = nullptr; // ptr to allocator all pools have been allocated with
            std::vector<mv::Allocator::Container> *poolContainersArray; // ptr to container for all pools
        };

        struct Container
        {
            enum Status
            {
                Clear,
                Usable,
                Fragmented,
                Full
            };

            // owns
            vk::DescriptorPool pool; // not unique_ptr because managed at higher level

            // references
            Allocator *parentAllocator = nullptr;
            std::vector<Container> *poolContainersArray = nullptr; // pointer to container for all pools

            // infos
            uint32_t index = 0;      // index to self in pools_array
            uint32_t count = 0;      // max sets requested from this pool on allocation
            vk::DescriptorType type; // type of descriptors this pool was created for
            Container *self = nullptr;
            Container::Status status = Status::Clear;

            // Disallow copy
            Container(const Container &) = delete;
            Container &operator=(const Container &) = delete;

            // Allow move
            Container(Container &&) = default;
            Container &operator=(Container &&) = delete;

            Container(struct ContainerInitStruct &p_ContainerInitStruct);
            ~Container();
        };

        struct Container *allocatePool(const mv::Device &p_MvDevice, uint32_t p_Count);
    };
}; // namespace mv

#endif