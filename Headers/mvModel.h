#ifndef HEADERS_MVMODEL_H_
#define HEADERS_MVMODEL_H_

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "tiny_loader.h"

#include "mvDevice.h"

const std::string MODEL_PATH = "models/viking_room.obj";
const float MOVESPEED = 0.005f;

namespace mv
{
    struct GlobalUniforms
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 projection;

        mv::Buffer ubo_view;
        mv::Buffer ubo_projection;
        VkDescriptorSet view_descriptor_set;
        VkDescriptorSet proj_descriptor_set;
    };

    struct Object
    {
        struct Matrices
        {
            alignas(16) glm::mat4 model;
            // TODO
            // uv, texture, normals
        } matrices;

        VkDescriptorSet descriptor_set;
        mv::Buffer uniform_buffer;
        glm::vec3 rotation;
        glm::vec3 position;
        uint32_t model_index;
        glm::vec3 front_face;

        void update(void)
        {
            glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0), position);

            glm::mat4 rotation_matrix = glm::mat4(1.0);
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

            matrices.model = translation_matrix * rotation_matrix;
        }

        void get_front_face(void)
        {
            glm::vec3 fr;
            fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
            fr.y = sin(glm::radians(rotation.x));
            fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
            fr = glm::normalize(fr);

            front_face = fr;
            return;
        }

        void move_left(float frame_delta)
        {
            get_front_face();
            position -= glm::normalize(glm::cross(front_face, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED * frame_delta;
            return;
        }
        void move_right(float frame_delta)
        {
            get_front_face();
            position += glm::normalize(glm::cross(front_face, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED * frame_delta;
            return;
        }
        void move_forward(float frame_delta)
        {
            get_front_face();
            return;
        }
        void move_backward(float frame_delta)
        {
            get_front_face();
            return;
        }
    };

    struct Vertex
    {
        glm::vec4 position;
        glm::vec4 uv;
        glm::vec4 color;

        static VkVertexInputBindingDescription get_binding_description()
        {
            VkVertexInputBindingDescription binding_description{};
            binding_description.binding = 0;
            binding_description.stride = sizeof(Vertex);
            binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return binding_description;
        }

        static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions()
        {
            // Temp container to be returned
            std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions{};

            // position
            attribute_descriptions[0].binding = 0;
            attribute_descriptions[0].location = 0;
            attribute_descriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attribute_descriptions[0].offset = offsetof(Vertex, position);

            // texture uv coordinates
            attribute_descriptions[1].binding = 0;
            attribute_descriptions[1].location = 1;
            attribute_descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attribute_descriptions[1].offset = offsetof(Vertex, uv);

            // color
            attribute_descriptions[2].binding = 0;
            attribute_descriptions[2].location = 2;
            attribute_descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attribute_descriptions[2].offset = offsetof(Vertex, color);

            return attribute_descriptions;
        }
    };

    class Model
    {
    public:
        // objects of this model type
        std::vector<Object> objects;

        void resize_object_container(uint32_t count)
        {
            objects.resize(count);
            return;
        }

    public:
        Model(void){};
        // Resize with copy was losing data
        Model(const Model &m) = delete;
        // Prefer move operations over copy
        Model(Model &&) = default;
        ~Model(void){}

        mv::Device *device;
        VkDescriptorPool descriptor_pool = nullptr;

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

        uint32_t image_count = 0;

        void bindBuffers(VkCommandBuffer cmdbuffer)
        {
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &vertices.buffer, offsets);
            vkCmdBindIndexBuffer(cmdbuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
            return;
        }

        void load(mv::Device *dvc, const char *filename)
        {
            assert(dvc);

            device = dvc;

            std::vector<Vertex> t_vertices;
            std::vector<uint32_t> t_indices;

            std::string warn, err;

            if (!tinyobj::LoadObj(&attribute, &shapes, &materials, &warn, &err, filename))
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

                    // vec4 for maintaining alignment
                    // only x,y positions are used
                    vertex.uv = {
                        attribute.texcoords[2 * index.texcoord_index + 0],
                        attribute.texcoords[2 * index.texcoord_index + 1],
                        0.0f, 0.0f
                    };


                    // generate random colors
                    float random_r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                    float random_g = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                    float random_b = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                    float max = 1.0f;
                    float min = 0.8f;
                    float range = max - min;

                    float r = (random_r * range) + min;
                    float g = (random_g * range) + min;
                    float b = (random_b * range) + min;

                    vertex.color = {r, g, b, 1.0f};

                    t_vertices.push_back(vertex);
                    vertices.count++;

                    t_indices.push_back(t_indices.size());
                    indices.count++;
                }
            }

            vertices.count = static_cast<uint32_t>(t_vertices.size());
            indices.count = static_cast<uint32_t>(t_indices.size());

            // create vertex buffer, load vertices data into it
            device->create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                  vertices.count * sizeof(Vertex),
                                  &vertices.buffer,
                                  &vertices.memory,
                                  t_vertices.data());

            // create index buffer, load indices data into it
            device->create_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                  indices.count * sizeof(uint32_t),
                                  &indices.buffer,
                                  &indices.memory,
                                  t_indices.data());
        }
    };
};

#endif