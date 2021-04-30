#ifndef HEADERS_MVENGINE_H_
#define HEADERS_MVENGINE_H_

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <random>

#include "mvAllocator.h"
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
        VkDescriptorSetLayout sampler_layout = nullptr;
        VkDescriptorSetLayout uniform_layout = nullptr;

        Engine &operator=(const Engine &) = delete;
        Engine(const Engine &) = delete;

        std::unique_ptr<Allocator> descriptor_allocator;

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

            // cleanup layouts
            if (uniform_layout)
            {
                vkDestroyDescriptorSetLayout(device->device, uniform_layout, nullptr);
                uniform_layout = nullptr;
            }
            if (sampler_layout)
            {
                vkDestroyDescriptorSetLayout(device->device, sampler_layout, nullptr);
                sampler_layout = nullptr;
            }

            // objects
            // destroy model data
            for (auto &model : models)
            {
                if (device)
                {
                    if (model.image.sampler)
                    {
                        vkDestroySampler(device->device, model.image.sampler, nullptr);
                        model.image.sampler = nullptr;
                    }
                    if (model.image.image_view)
                    {
                        vkDestroyImageView(device->device, model.image.image_view, nullptr);
                        model.image.image_view = nullptr;
                    }
                    if (model.image.image)
                    {
                        vkDestroyImage(device->device, model.image.image, nullptr);
                        model.image.image = nullptr;
                    }
                    if (model.image.memory)
                    {
                        vkFreeMemory(device->device, model.image.memory, nullptr);
                        model.image.memory = nullptr;
                    }
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

        void add_new_model(mv::Allocator::Container *pool, const char *filename);

        void recreate_swapchain(void);

        void go(void);
        void record_command_buffer(uint32_t imageIndex);
        void draw(size_t &current_frame, uint32_t &current_image_index);

    protected:
        void prepare_uniforms(void);

        void create_descriptor_layout(VkDescriptorType type,
                                      uint32_t count,
                                      uint32_t binding,
                                      VkPipelineStageFlags stage_flags,
                                      VkDescriptorSetLayout &layout);

        void create_descriptor_sets(GlobalUniforms *view_proj_ubo_container, bool should_create_layout = true);
        void prepare_pipeline(void);
        void cleanup_swapchain(void);
    };
};

#endif