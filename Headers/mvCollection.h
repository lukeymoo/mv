#ifndef HEADERS_MVCOLLECTION_H_
#define HEADERS_MVCOLLECTION_H_

#include <ostream>

#include "mvAllocator.h"
#include "mvDevice.h"
#include "mvHelper.h"
#include "mvMap.h"
#include "mvModel.h"

namespace mv
{
    struct UniformObject
    {
        alignas(16) glm::mat4 matrix = glm::mat4(0);
        mv::Buffer mvBuffer;
        vk::DescriptorSet descriptor;
    };

    struct Collection
    {
        bool shouldOutputDebug = false;
        // owns
        std::unique_ptr<std::vector<mv::Model>> models;

        // infos
        std::vector<std::string> modelNames;

        // view & projection matrix objects
        std::unique_ptr<struct mv::UniformObject> viewUniform;
        std::unique_ptr<struct mv::UniformObject> projectionUniform;

        Collection(const mv::Device &p_MvDevice);
        ~Collection();

        // Load terrain normal
        void loadTerrain(void);

        // Load model
        void loadModel(const mv::Device &p_MvDevice, mv::Allocator &p_DescriptorAllocator, const char *p_Filename);

        // Creates object with specified model data
        void createObject(const mv::Device &p_MvDevice, mv::Allocator &p_DescriptorAllocator, std::string p_ModelName);

        // Calls update for each object
        void update(void);

        // Destroys Vulkan resources allocated by this handler
        void cleanup(const mv::Device &p_MvDevice, mv::Allocator &p_DescriptorAllocator);

        inline uint32_t getObjectCount(void)
        {
            uint32_t count = 0;
            for (const auto &model : *models)
            {
                count += model.objects->size();
            }
            return count;
        }
        inline uint32_t getTriangleCount(void)
        {
            uint32_t count = 0;
            for (const auto &model : *models)
            {
                count += model.triangleCount * model.objects->size();
            }
            return count;
        }
        inline uint32_t getVertexCount(void)
        {
            uint32_t count = 0;
            for (const auto &model : *models)
            {
                count += model.totalIndices * model.objects->size();
            }
            return count;
        }
    };
}; // namespace mv

#endif