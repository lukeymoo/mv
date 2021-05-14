#ifndef HEADERS_MVCOLLECTION_H_
#define HEADERS_MVCOLLECTION_H_

#include <ostream>

#include "mvAllocator.h"
#include "mvDevice.h"
#include "mvModel.h"

namespace mv
{
    struct UniformObject
    {
        alignas(16) glm::mat4 matrix = glm::mat4(1.0);
        mv::Buffer mvBuffer;
        vk::DescriptorSet descriptor;
    };

    struct Collection
    {
        // owns
        std::unique_ptr<std::vector<mv::Model>> models;

        // infos
        std::vector<std::string> modelNames;

        // view & projection matrix objects
        std::unique_ptr<struct mv::UniformObject> viewUniform;
        std::unique_ptr<struct mv::UniformObject> projectionUniform;

        Collection(const mv::Device &p_MvDevice);
        ~Collection();

        // load model
        void loadModel(const mv::Device &p_MvDevice, mv::Allocator &p_DescriptorAllocator, const char *p_Filename);

        // creates object with specified model data
        void createObject(const mv::Device &p_MvDevice, mv::Allocator &p_DescriptorAllocator, std::string p_ModelName);

        void update(void);

        void cleanup(const mv::Device &p_MvDevice, mv::Allocator &p_DescriptorAllocator);
    };
}; // namespace mv

#endif