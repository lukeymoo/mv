#ifndef HEADERS_MVMODEL_H_
#define HEADERS_MVMODEL_H_

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "tiny_loader.h"

#include "mvDevice.h"

const std::string MODEL_PATH = "models/viking_room.obj";

namespace mv
{
    struct Vertex
    {
        glm::vec4 position;
        glm::vec4 color;

        static VkVertexInputBindingDescription getBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
        {
            // Temp container to be returned
            std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

            // Position
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, position);

            // Color
            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, color);

            return attributeDescriptions;
        }
    };

    class Model
    {
    public:
        Model(){};
        ~Model()
        {
            if (device)
            {
                if (vertices.buffer)
                {
                    vkDestroyBuffer(device->device, vertices.buffer, nullptr);
                    vkFreeMemory(device->device, vertices.memory, nullptr);
                }
                if (indices.buffer)
                {
                    vkDestroyBuffer(device->device, indices.buffer, nullptr);
                    vkFreeMemory(device->device, indices.memory, nullptr);
                }
                if (descriptorPool)
                {
                    vkDestroyDescriptorPool(device->device, descriptorPool, nullptr);
                }
            }
        }

        mv::Device *device;
        VkDescriptorPool descriptorPool = nullptr;

        struct Vertices
        {
            uint32_t count = 0;
            VkBuffer buffer = nullptr;
            VkDeviceMemory memory = nullptr;
        } vertices;

        struct Indices
        {
            uint32_t count = 0;
            VkBuffer buffer = nullptr;
            VkDeviceMemory memory = nullptr;
        } indices;

        tinyobj::attrib_t attribute;
        std::vector<tinyobj::material_t> materials;
        std::vector<tinyobj::shape_t> shapes;

        uint32_t imageCount = 0;

        void bindBuffers(VkCommandBuffer cmdbuffer)
        {
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &vertices.buffer, offsets);
            vkCmdBindIndexBuffer(cmdbuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
            return;
        }

        void load(mv::Device *dvc, uint32_t swapchain_image_count)
        {
            assert(dvc);

            device = dvc;

            std::vector<Vertex> t_vertices;
            std::vector<uint32_t> t_indices;

            // std::vector<Vertex> t_vertices = {
            //     {{-0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            //     {{0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            //     {{0.5f, 0.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            //     {{-0.5f, 0.5f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},

            //     {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            //     {{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            //     {{0.5f, 0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            //     {{-0.5f, 0.5f, -0.5f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}};

            // std::vector<uint32_t> t_indices = {
            //     0, 1, 2, 2, 3, 0,
            //     4, 5, 6, 6, 7, 4};
            std::string warn, err;

            if (!tinyobj::LoadObj(&attribute, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
            {
                throw std::runtime_error(warn + err);
            }

            for (const auto &shape : shapes)
            {
                for (const auto &index : shape.mesh.indices)
                {
                    Vertex vertex = {};

                    vertex.position = {
                        attribute.vertices[3 * index.vertex_index + 0],
                        attribute.vertices[3 * index.vertex_index + 1],
                        attribute.vertices[3 * index.vertex_index + 2],
                        1.0f};

                    vertex.color = {1.0f, 0.0f, 0.0f, 1.0f};

                    t_vertices.push_back(vertex);
                    vertices.count++;

                    t_indices.push_back(t_indices.size());
                    indices.count++;
                }
            }

            vertices.count = static_cast<uint32_t>(t_vertices.size());
            indices.count = static_cast<uint32_t>(t_indices.size());

            // create vertex buffer, load vertices data into it
            device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                 vertices.count * sizeof(Vertex),
                                 &vertices.buffer,
                                 &vertices.memory,
                                 t_vertices.data());

            // create index buffer, load indices data into it
            device->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                 indices.count * sizeof(uint32_t),
                                 &indices.buffer,
                                 &indices.memory,
                                 t_indices.data());

            // configure descriptor pool, layout, sets
            // std::vector<VkDescriptorPoolSize> poolSizes = {
            //     {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};

            // VkDescriptorPoolCreateInfo poolInfo = {};
            // poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            // poolInfo.pNext = nullptr;
            // poolInfo.flags = 0;
            // poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            // poolInfo.pPoolSizes = poolSizes.data();
            // poolInfo.maxSets = (static_cast<uint32_t>(poolSizes.size()) * imageCount);

            // if (vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
            // {
            //     throw std::runtime_error("Failed to create descriptor pool for model");
            // }

            // Descriptor layout done in Engine as the layout is global
        }
    };
};

#endif