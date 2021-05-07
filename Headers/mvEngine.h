#ifndef HEADERS_MVENGINE_H_
#define HEADERS_MVENGINE_H_

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <chrono>
#include <random>

#include "mvAllocator.h"
#include "mvCamera.h"
#include "mvWindow.h"
#include "mvCollection.h"

using namespace std::chrono_literals;

namespace mv
{
    class Engine : public mv::MWindow
    {
    public:
        VkPipeline pipeline_w_sampler = nullptr;
        VkPipeline pipeline_no_sampler = nullptr;

        VkPipelineLayout pipeline_layout_w_sampler = nullptr;
        VkPipelineLayout pipeline_layout_no_sampler = nullptr;

        Engine &operator=(const Engine &) = delete;
        Engine(const Engine &) = delete;

        std::shared_ptr<Allocator> descriptor_allocator; // descriptor pool/set manager
        std::unique_ptr<Collection> collection_handler;  // model/obj manager
        std::unique_ptr<Camera> camera;                  // camera manager(view/proj matrix handler)

        Engine(int w, int h, const char *title);
        ~Engine() // Cleanup
        {
            if (device)
            {
                if (device->device)
                {
                    vkDeviceWaitIdle(device->device);
                }
            }

            // pipelines
            if (pipeline_w_sampler)
            {
                vkDestroyPipeline(device->device, pipeline_w_sampler, nullptr);
                pipeline_w_sampler = nullptr;
            }
            if (pipeline_no_sampler)
            {
                vkDestroyPipeline(device->device, pipeline_no_sampler, nullptr);
                pipeline_no_sampler = nullptr;
            }

            // pipeline layouts
            if (pipeline_layout_w_sampler)
            {
                vkDestroyPipelineLayout(device->device, pipeline_layout_w_sampler, nullptr);
                pipeline_layout_w_sampler = nullptr;
            }
            if (pipeline_layout_no_sampler)
            {
                vkDestroyPipelineLayout(device->device, pipeline_layout_no_sampler, nullptr);
                pipeline_layout_no_sampler = nullptr;
            }

            // collection struct will handle cleanup of models & objs
            if (collection_handler)
            {
                collection_handler->cleanup();
            }
        }

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