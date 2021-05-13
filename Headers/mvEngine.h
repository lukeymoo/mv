#ifndef HEADERS_MVENGINE_H_
#define HEADERS_MVENGINE_H_

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <chrono>
#include <random>
#include <unordered_map>

#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "mvAllocator.h"
#include "mvCamera.h"
#include "mvCollection.h"
#include "mvWindow.h"

using namespace std::chrono_literals;

namespace mv
{

    class Engine : public mv::MWindow
    {
      public:
        // delete copy
        Engine &operator=(const Engine &) = delete;
        Engine(const Engine &) = delete;

        /*
          DEBUG RENDER JOBS
          these will be removed for release versions
        */
        std::unordered_map<std::string, bool> to_render_map = {
            std::pair<std::string, bool>{"reticle_raycast", true},
        };

        struct mv::_Mesh reticle_mesh; // Vertex buffer & memory
        mv::Object reticle_obj;        // Uniform object(model matrix, updated via push constants)
        void *reticle_mapped = nullptr;
        /*
          DEBUG RENDER JOBS
        */

        // Owns
        std::unique_ptr<Allocator> descriptor_allocator; // descriptor pool/set manager
        std::unique_ptr<Collection> collection_handler;  // model/obj manager
        std::unique_ptr<Camera> camera;                  // camera manager(view/proj matrix handler)

        // Containers for pipelines
        std::unique_ptr<std::unordered_map<std::string, vk::Pipeline>> pipelines;
        std::unique_ptr<std::unordered_map<std::string, vk::PipelineLayout>> pipeline_layouts;

        Engine(int w, int h, const char *title);
        ~Engine();

        void add_new_model(mv::Allocator::Container *pool, const char *filename);

        void recreate_swapchain(void);

        void go(void);
        // all the initial descriptor allocator & collection handler calls
        inline void go_setup(void);
        void record_command_buffer(uint32_t imageIndex);
        void draw(size_t &current_frame, uint32_t &current_image_index);

        /*
          Helper methods
        */
        // Create quick one time submit command buffer
        inline vk::CommandBuffer begin_command_buffer(void)
        {
            // sanity check
            if (!mv_device)
                throw std::runtime_error("attempted to create one time submit command buffer but mv device "
                                         "handler not initialized\n");

            if (!mv_device->command_pool)
                throw std::runtime_error("attempted to create one time submit command buffer but command "
                                         "pool is not initialized\n");

            vk::CommandBufferAllocateInfo alloc_info;
            alloc_info.level = vk::CommandBufferLevel::ePrimary;
            alloc_info.commandPool = *mv_device->command_pool;
            alloc_info.commandBufferCount = 1;

            // allocate command buffer
            std::vector<vk::CommandBuffer> cmdbuf = mv_device->logical_device->allocateCommandBuffers(alloc_info);

            if (cmdbuf.size() < 1)
                throw std::runtime_error("Failed to create one time submit command buffer");

            // if more than 1 was created clean them up
            // shouldn't happen
            if (cmdbuf.size() > 1)
            {
                std::vector<vk::CommandBuffer> to_destroy;
                for (auto &buf : cmdbuf)
                {
                    if (&buf - &cmdbuf[0] > 0)
                    {
                        to_destroy.push_back(buf);
                    }
                }
                mv_device->logical_device->freeCommandBuffers(*mv_device->command_pool, to_destroy);
            }

            vk::CommandBufferBeginInfo begin_info;
            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

            cmdbuf.at(0).begin(begin_info);

            return cmdbuf.at(0);
        }

        inline void end_command_buffer(vk::CommandBuffer buffer)
        {
            // sanity check
            if (!mv_device)
                throw std::runtime_error("attempted to end command buffer recording but mv device handler is nullptr");

            if (!mv_device->command_pool)
                throw std::runtime_error("attempted to end command buffer recording but command pool is nullptr");

            if (!mv_device->graphics_queue)
                throw std::runtime_error("attempted to end command buffer recording but graphics queue is nullptr");

            if (!buffer)
                throw std::runtime_error("attempted to end command buffer recording but the buffer passed "
                                         "as parameter is nullptr");

            buffer.end();

            vk::SubmitInfo submit_info;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &buffer;

            // submit buffer
            mv_device->graphics_queue->submit(submit_info);
            mv_device->graphics_queue->waitIdle();

            // free buffer
            mv_device->logical_device->freeCommandBuffers(*mv_device->command_pool, buffer);
            return;
        }

      protected:
        void prepare_uniforms(void);

        // void create_descriptor_sets(GlobalUniforms *view_proj_ubo_container, bool
        // should_create_layout = true);
        void prepare_pipeline(void);

        void cleanup_swapchain(void);
    };

    /*
        DEBUG FUNCTIONS
      */
    // dont know how to do this..use tan ?
    static inline void generate_ray(void)
    {
        return;
    }

    // set vertex 1 of reticle mesh to position
    static inline void raycast_p1(const mv::Engine &engine, glm::vec3 pos)
    {
        Vertex e;
        e.position = {pos, 1.0f};
        e.color = {0.0f, 0.0f, 1.0f, 1.0f};
        // Map vertex 2 -- stays with object 1
        void *reticle_mapped = nullptr;
        reticle_mapped =
            engine.mv_device->logical_device->mapMemory(engine.reticle_mesh.vertex_memory, 0, sizeof(Vertex));
        if (!reticle_mapped)
            throw std::runtime_error("Failed to map reticle mesh vertex memory VERTEX 2");

        memcpy(reticle_mapped, &e, sizeof(Vertex));
        engine.mv_device->logical_device->unmapMemory(engine.reticle_mesh.vertex_memory);
        return;
    }

    // set vertex 2 of reticle mesh to position
    static inline void raycast_p2(const mv::Engine &engine, glm::vec3 pos)
    {
        Vertex e;
        e.position = {pos, 1.0f};
        e.color = {0.0f, 0.0f, 1.0f, 1.0f};
        // Map vertex 2 -- stays with object 1
        void *reticle_mapped = nullptr;
        reticle_mapped = engine.mv_device->logical_device->mapMemory(engine.reticle_mesh.vertex_memory, sizeof(Vertex),
                                                                     sizeof(Vertex));
        if (!reticle_mapped)
            throw std::runtime_error("Failed to map reticle mesh vertex memory VERTEX 2");

        memcpy(reticle_mapped, &e, sizeof(Vertex));
        engine.mv_device->logical_device->unmapMemory(engine.reticle_mesh.vertex_memory);
        return;
    }

}; // namespace mv

#endif