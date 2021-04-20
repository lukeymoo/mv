#ifndef HEADERS_MVENGINE_H_
#define HEADERS_MVENGINE_H_

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE


#include "mvCamera.h"
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

            VkDescriptorSet descriptor_set;
            mv::Buffer uniform_buffer; // contains the matrices
            glm::vec3 rotation;
            uint32_t model_index;
        };
        std::vector<Object> objects;
        std::vector<mv::Model> models;

        VkPipeline pipeline = nullptr;
        VkPipelineLayout pipeline_layout = nullptr;

        VkDescriptorPool descriptor_pool = nullptr;
        VkDescriptorSetLayout descriptor_layout = nullptr;

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
            if (pipeline_layout)
            {
                vkDestroyPipelineLayout(device->device, pipeline_layout, nullptr);
            }

            // cleanup descriptor sets
            // free pool
            if (descriptor_pool)
            {
                vkDestroyDescriptorPool(device->device, descriptor_pool, nullptr);
            }

            // cleanup layout
            if (descriptor_layout)
            {
                vkDestroyDescriptorSetLayout(device->device, descriptor_layout, nullptr);
            }

            // objects
            for (auto &obj : objects)
            {
                obj.uniform_buffer.destroy();
            }
        }

        std::unique_ptr<Camera> camera;

        void recreate_swapchain(void);

        void go(void);
        void record_command_buffer(uint32_t imageIndex);
        void draw(size_t &current_frame, uint32_t &current_image_index);

    protected:
        void prepare_uniforms(void);
        void create_descriptor_sets(bool should_create_layout = true);
        void prepare_pipeline(void);
        void cleanup_swapchain(void);
    };
};

#endif