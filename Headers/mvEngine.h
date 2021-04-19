#ifndef HEADERS_MVENGINE_H_
#define HEADERS_MVENGINE_H_

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "mvWindow.h"
#include "mvModel.h"

namespace mv
{
    class Engine : public mv::MWindow
    {
    public:
        struct Object
        {
            struct Matrices
            {
                alignas(16) glm::mat4 model;
                alignas(16) glm::mat4 view;
                alignas(16) glm::mat4 projection;
            } matrices;

            VkDescriptorSet descriptorSet;
            mv::Buffer uniformBuffer; // contains the matrices
            glm::vec3 rotation;
            uint32_t modelIndex;
        };
        std::vector<Object> objects;
        std::vector<mv::Model> models;

        VkPipeline pipeline = nullptr;
        VkPipelineLayout pipelineLayout = nullptr;

        VkDescriptorPool descriptorPool = nullptr;
        VkDescriptorSetLayout descriptorLayout = nullptr;

        Engine &operator=(const Engine &) = delete;
        Engine(const Engine &) = delete;

        Engine(int w, int h, const char *title);
        ~Engine() // Cleanup
        {
            vkDeviceWaitIdle(device->device);
            if (pipeline)
            {
                vkDestroyPipeline(device->device, pipeline, nullptr);
            }
            if (pipelineLayout)
            {
                vkDestroyPipelineLayout(device->device, pipelineLayout, nullptr);
            }

            // free pool
            if (descriptorPool)
            {
                vkDestroyDescriptorPool(device->device, descriptorPool, nullptr);
            }

            // cleanup layout
            if (descriptorLayout)
            {
                vkDestroyDescriptorSetLayout(device->device, descriptorLayout, nullptr);
            }

            // objects
            for (auto &obj : objects)
            {
                obj.uniformBuffer.destroy();
            }
        }

        void go(void);
        void recordCommandBuffer(uint32_t imageIndex);

    private:
        void prepareUniforms(void);
        void createDescriptorSets(void);
        void preparePipeline(void);
    };
};

#endif