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

    class Engine : public mv::Window
    {
      public:
        // delete copy
        Engine &operator=(const Engine &) = delete;
        Engine(const Engine &) = delete;

        /*
          DEBUG RENDER JOBS
          these will be removed for release versions
        */
        std::unordered_map<std::string, bool> toRenderMap = {
            std::pair<std::string, bool>{"reticle_raycast", true},
        };

        struct mv::Mesh reticleMesh; // Vertex buffer & memory
        mv::Object reticleObj;       // Uniform object(model matrix, updated via push constants)
        void *reticleMapped = nullptr;
        /*
          END DEBUG RENDER JOBS
        */

        // Owns
        std::unique_ptr<Allocator> descriptorAllocator; // descriptor pool/set manager
        std::unique_ptr<Collection> collectionHandler;  // model/obj manager
        std::unique_ptr<Camera> camera;                 // camera manager(view/proj matrix handler)

        // Containers for pipelines
        std::unique_ptr<std::unordered_map<std::string, vk::Pipeline>> pipelines;
        std::unique_ptr<std::unordered_map<std::string, vk::PipelineLayout>> pipelineLayouts;

        Engine(int w, int h, const char *title);
        ~Engine();

        void addNewModel(mv::Allocator::Container *pool, const char *filename);

        void recreateSwapchain(void);

        void go(void);
        // all the initial descriptor allocator & collection handler calls
        inline void goSetup(void);
        void recordCommandBuffer(uint32_t imageIndex);
        void draw(size_t &current_frame, uint32_t &current_image_index);

        /*
          Helper methods
        */
        // Create quick one time submit command buffer
        inline vk::CommandBuffer beginCommandBuffer(void)
        {
            // sanity check
            if (!mvDevice)
                throw std::runtime_error("attempted to create one time submit command buffer but mv device "
                                         "handler not initialized\n");

            if (!mvDevice->commandPool)
                throw std::runtime_error("attempted to create one time submit command buffer but command "
                                         "pool is not initialized\n");

            vk::CommandBufferAllocateInfo allocInfo;
            allocInfo.level = vk::CommandBufferLevel::ePrimary;
            allocInfo.commandPool = *mvDevice->commandPool;
            allocInfo.commandBufferCount = 1;

            // allocate command buffer
            std::vector<vk::CommandBuffer> cmdbuf = mvDevice->logicalDevice->allocateCommandBuffers(allocInfo);

            if (cmdbuf.size() < 1)
                throw std::runtime_error("Failed to create one time submit command buffer");

            // if more than 1 was created clean them up
            // shouldn't happen
            if (cmdbuf.size() > 1)
            {
                std::vector<vk::CommandBuffer> toDestroy;
                for (auto &buf : cmdbuf)
                {
                    if (&buf - &cmdbuf[0] > 0)
                    {
                        toDestroy.push_back(buf);
                    }
                }
                mvDevice->logicalDevice->freeCommandBuffers(*mvDevice->commandPool, toDestroy);
            }

            vk::CommandBufferBeginInfo beginInfo;
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

            cmdbuf.at(0).begin(beginInfo);

            return cmdbuf.at(0);
        }

        inline void endCommandBuffer(vk::CommandBuffer buffer)
        {
            // sanity check
            if (!mvDevice)
                throw std::runtime_error("attempted to end command buffer recording but mv device handler is nullptr");

            if (!mvDevice->commandPool)
                throw std::runtime_error("attempted to end command buffer recording but command pool is nullptr");

            if (!mvDevice->graphicsQueue)
                throw std::runtime_error("attempted to end command buffer recording but graphics queue is nullptr");

            if (!buffer)
                throw std::runtime_error("attempted to end command buffer recording but the buffer passed "
                                         "as parameter is nullptr");

            buffer.end();

            vk::SubmitInfo submitInfo;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &buffer;

            // submit buffer
            mvDevice->graphicsQueue->submit(submitInfo);
            mvDevice->graphicsQueue->waitIdle();

            // free buffer
            mvDevice->logicalDevice->freeCommandBuffers(*mvDevice->commandPool, buffer);
            return;
        }

      protected:
        void prepareUniforms(void);

        // void create_descriptor_sets(GlobalUniforms *view_proj_ubo_container, bool
        // should_create_layout = true);
        void preparePipeline(void);

        void cleanupSwapchain(void);
    };

    /*
        DEBUG FUNCTIONS
      */
    // dont know how to do this..use tan ?
    static inline void generateRay(void)
    {
        return;
    }

    // set vertex 1 of reticle mesh to position
    static inline void raycastP1(const mv::Engine &engine, glm::vec3 pos)
    {
        Vertex e;
        e.position = {pos, 1.0f};
        e.color = {0.0f, 0.0f, 1.0f, 1.0f};
        // Map vertex 2 -- stays with object 1
        void *reticleMapped = nullptr;
        reticleMapped = engine.mvDevice->logicalDevice->mapMemory(engine.reticleMesh.vertexMemory, 0, sizeof(Vertex));
        if (!reticleMapped)
            throw std::runtime_error("Failed to map reticle mesh vertex memory VERTEX 2");

        memcpy(reticleMapped, &e, sizeof(Vertex));
        engine.mvDevice->logicalDevice->unmapMemory(engine.reticleMesh.vertexMemory);
        return;
    }

    // set vertex 2 of reticle mesh to position
    static inline void raycastP2(const mv::Engine &engine, glm::vec3 pos)
    {
        Vertex e;
        e.position = {pos, 1.0f};
        e.color = {0.0f, 0.0f, 1.0f, 1.0f};
        // Map vertex 2 -- stays with object 1
        void *reticleMapped = nullptr;
        reticleMapped =
            engine.mvDevice->logicalDevice->mapMemory(engine.reticleMesh.vertexMemory, sizeof(Vertex), sizeof(Vertex));
        if (!reticleMapped)
            throw std::runtime_error("Failed to map reticle mesh vertex memory VERTEX 2");

        memcpy(reticleMapped, &e, sizeof(Vertex));
        engine.mvDevice->logicalDevice->unmapMemory(engine.reticleMesh.vertexMemory);
        return;
    }

}; // namespace mv

#endif