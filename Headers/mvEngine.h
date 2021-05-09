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
            pipelines = std::make_unique<std::unordered_map<std::string, vk::Pipeline>>();
            pipeline_layouts = std::make_unique<std::unordered_map<std::string, vk::PipelineLayout>>();
            return;
        }
        ~Engine() // Cleanup
        {
            if (!mv_device)
                return;

            mv_device->logical_device->waitIdle();

            if (pipelines)
            {
                for (auto &pipeline : *pipelines)
                {
                    if (pipeline.second)
                        mv_device->logical_device->destroyPipeline(pipeline.second);
                }
                pipelines.reset();
            }

            if (pipeline_layouts)
            {
                for (auto &layout : *pipeline_layouts)
                {
                    if (layout.second)
                        mv_device->logical_device->destroyPipelineLayout(layout.second);
                }
                pipeline_layouts.reset();
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

        // void create_descriptor_sets(GlobalUniforms *view_proj_ubo_container, bool should_create_layout = true);
        void prepare_pipeline(void);

        void cleanup_swapchain(void);
    };
};

#endif