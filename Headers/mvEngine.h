#ifndef HEADERS_MVENGINE_H_
#define HEADERS_MVENGINE_H_

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <unordered_map>
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
    // delete copy
        Engine &operator=(const Engine &) = delete;
        Engine(const Engine &) = delete;

        // vk::Pipeline pipeline_w_sampler;
        // vk::Pipeline pipeline_no_sampler;

        // vk::PipelineLayout pipeline_layout_w_sampler;
        // vk::PipelineLayout pipeline_layout_no_sampler;

        std::shared_ptr<Allocator> descriptor_allocator; // descriptor pool/set manager
        std::unique_ptr<Collection> collection_handler;  // model/obj manager
        std::unique_ptr<Camera> camera;                  // camera manager(view/proj matrix handler)

        // container for pipelines
        std::unique_ptr<std::unordered_map<std::string, vk::Pipeline>> pipelines;
        std::unique_ptr<std::unordered_map<std::string, vk::PipelineLayout>> pipeline_layouts;

        Engine(int w, int h, const char *title)
            : MWindow(w, h, title)
        {
            return;
        }
        ~Engine(void) // Cleanup
        {
            if (!mv_device)
                throw std::runtime_error("Logical device has been destroyed before proper cleanup");

            mv_device->logical_device->waitIdle();

            if (pipelines)
            {
                auto destroy_pipeline = [&, this](std::pair<std::string, vk::Pipeline> entry) {
                    if (entry.second)
                        mv_device->logical_device->destroyPipeline(entry.second);
                };

                std::all_of(pipelines->begin(), pipelines->end(), destroy_pipeline);
                pipelines.reset();
            }

            if (pipeline_layouts)
            {
                auto destroy_layout = [&, this](std::pair<std::string, vk::PipelineLayout> entry) {
                    if (entry.second)
                        mv_device->logical_device->destroyPipelineLayout(entry.second);
                };

                std::all_of(pipeline_layouts->begin(), pipeline_layouts->end(), destroy_layout);
                pipeline_layouts.reset();
            }

            // pipelines
            // if (pipeline_w_sampler)
            // {
            //     mv_device->logical_device
            //         vkDestroyPipeline(device->device, pipeline_w_sampler, nullptr);
            //     pipeline_w_sampler = nullptr;
            // }
            // if (pipeline_no_sampler)
            // {
            //     vkDestroyPipeline(device->device, pipeline_no_sampler, nullptr);
            //     pipeline_no_sampler = nullptr;
            // }

            // // pipeline layouts
            // if (pipeline_layout_w_sampler)
            // {
            //     vkDestroyPipelineLayout(device->device, pipeline_layout_w_sampler, nullptr);
            //     pipeline_layout_w_sampler = nullptr;
            // }
            // if (pipeline_layout_no_sampler)
            // {
            //     vkDestroyPipelineLayout(device->device, pipeline_layout_no_sampler, nullptr);
            //     pipeline_layout_no_sampler = nullptr;
            // }

            // collection struct will handle cleanup of models & objs
            if (collection_handler)
            {
                collection_handler->cleanup();
            }
        }

        void add_new_model(mv::Allocator::Container *pool, const char *filename);

        // void recreate_swapchain(void);

        void go(void);
        void record_command_buffer(uint32_t imageIndex);
        void draw(size_t &current_frame, uint32_t &current_image_index);

    protected:
        void prepare_uniforms(void);

        // void create_descriptor_sets(GlobalUniforms *view_proj_ubo_container, bool should_create_layout = true);
        void prepare_pipeline(void);

        // void cleanup_swapchain(void);
    };
};

#endif