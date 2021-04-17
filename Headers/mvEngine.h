#ifndef HEADERS_MVENGINE_H_
#define HEADERS_MVENGINE_H_

#include <glm/glm.hpp>

#include "mvWindow.h"
#include "mvModel.h"

namespace mv
{
    class Engine : public mv::Window
    {
    public:
        struct Object
        {
            struct Matrices
            {
                glm::mat4 model;
                glm::mat4 view;
                glm::mat4 projection;
            } matrices;

            VkDescriptorSet descriptorSet;
            mv::Buffer uniformBuffer; // contains the matrices
            glm::vec3 rotation;
        };
        std::array<Object, 1> objects;
        mv::Model model;

        VkPipeline pipeline = nullptr;
        VkPipelineLayout pipelineLayout = nullptr;

        VkDescriptorPool descriptorPool = nullptr;
        VkDescriptorSetLayout descriptorLayout = nullptr;

        Engine(int w, int h, const char *title);
        ~Engine() // Cleanup
        {
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