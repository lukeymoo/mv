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
        std::vector<mv::Model> models;

        VkPipeline pipeline = nullptr;
        VkPipelineLayout pipeline_layout = nullptr;

        VkDescriptorPool descriptor_pool = nullptr;
        VkDescriptorSetLayout model_layout = nullptr;
        VkDescriptorSetLayout view_layout = nullptr;
        VkDescriptorSetLayout projection_layout = nullptr;

        Engine &operator=(const Engine &) = delete;
        Engine(const Engine &) = delete;

        Engine(int w, int h, const char *title);
        ~Engine() // Cleanup
        {
            vkDeviceWaitIdle(device->device);

            if (pipeline)
            {
                vkDestroyPipeline(device->device, pipeline, nullptr);
                pipeline = nullptr;
            }
            if (pipeline_layout)
            {
                vkDestroyPipelineLayout(device->device, pipeline_layout, nullptr);
                pipeline_layout = nullptr;
            }

            // cleanup descriptor sets
            // free pool
            if (descriptor_pool)
            {
                vkDestroyDescriptorPool(device->device, descriptor_pool, nullptr);
                descriptor_pool = nullptr;
            }

            // cleanup layout
            if (model_layout)
            {
                vkDestroyDescriptorSetLayout(device->device, model_layout, nullptr);
                model_layout = nullptr;
            }
            if (view_layout)
            {
                vkDestroyDescriptorSetLayout(device->device, view_layout, nullptr);
                view_layout = nullptr;
            }
            if (projection_layout)
            {
                vkDestroyDescriptorSetLayout(device->device, projection_layout, nullptr);
                projection_layout = nullptr;
            }

            // objects
            // destroy model data
            for (auto &model : models)
            {
                if (device)
                {
                    if (model.vertices.buffer)
                    {
                        vkDestroyBuffer(device->device, model.vertices.buffer, nullptr);
                        vkFreeMemory(device->device, model.vertices.memory, nullptr);
                        model.vertices.buffer = nullptr;
                        model.vertices.memory = nullptr;
                    }
                    if (model.indices.buffer)
                    {
                        vkDestroyBuffer(device->device, model.indices.buffer, nullptr);
                        vkFreeMemory(device->device, model.indices.memory, nullptr);
                        model.indices.buffer = nullptr;
                        model.indices.memory = nullptr;
                    }
                }
            }
            // destroy uniforms
            for (auto &model : models)
            {
                for (auto &obj : model.objects)
                {
                    obj.uniform_buffer.destroy();
                }
            }
        }

        // contains view & projection matrix data
        // as well as mv buffer objects for both
        GlobalUniforms global_uniforms;
        std::unique_ptr<Camera> camera;

        void add_new_model(const char *filename);

        void recreate_swapchain(void);

        void go(void);
        void record_command_buffer(uint32_t imageIndex);
        void draw(size_t &current_frame, uint32_t &current_image_index);

    protected:
        void prepare_uniforms(void);

        void create_descriptor_sets(GlobalUniforms *view_proj_ubo_container, bool should_create_layout = true);
        void prepare_pipeline(void);
        void cleanup_swapchain(void);
    };
};

#endif