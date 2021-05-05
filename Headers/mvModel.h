#ifndef HEADERS_MVMODEL_H_
#define HEADERS_MVMODEL_H_

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

// TODO
// replace with assimp
#include "tiny_loader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "mvDevice.h"
#include "mvImage.h"
#include "mvAllocator.h"

const float MOVESPEED = 0.005f;

namespace mv
{
    struct _Texture
    {
        std::string type;
        std::string path;
        mv::Image mv_image;
        VkDescriptorSet descriptor;
    };

    struct Object
    {
        struct Matrices
        {
            alignas(16) glm::mat4 model;
            // uv, normals implemented as Vertex attributes
        } matrices;

        VkDescriptorSet model_descriptor;
        VkDescriptorSet texture_descriptor;
        mv::Buffer uniform_buffer;
        glm::vec3 rotation;
        glm::vec3 position;
        uint32_t model_index;
        glm::vec3 front_face;
        float scale_factor = 1.0f;

        void update(void)
        {
            glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0), position);

            glm::mat4 rotation_matrix = glm::mat4(1.0);
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

            glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0), glm::vec3(scale_factor));

            matrices.model = translation_matrix * rotation_matrix * scale_matrix;

            // update obj uniform
            memcpy(uniform_buffer.mapped, &matrices.model, sizeof(matrices));
        }

        void get_front_face(void)
        {
            glm::vec3 fr;
            fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
            fr.y = sin(glm::radians(rotation.x));
            fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
            fr = -glm::normalize(fr);

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

    // Collection of data that makes up the various components of a model
    struct _Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<_Texture> textures;

        // TODO
        // Consider linking all related mesh objects into a single buffer.
        // Will use offsets to access data relevant to particular mesh the
        // application is drawing
        VkBuffer vertex_buffer = nullptr;
        VkBuffer index_buffer = nullptr;

        VkDeviceMemory vertex_memory = nullptr;
        VkDeviceMemory index_memory = nullptr;

        void bindBuffers(VkCommandBuffer cmdbuffer)
        {
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &vertex_buffer, offsets);
            vkCmdBindIndexBuffer(cmdbuffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
            return;
        }

        void cleanup(mv::Device *device)
        {
            if (vertex_buffer)
            {
                vkDestroyBuffer(device->device, vertex_buffer, nullptr);
                vertex_buffer = nullptr;
            }
            if (vertex_memory)
            {
                vkFreeMemory(device->device, vertex_memory, nullptr);
                vertex_memory = nullptr;
            }

            if (index_buffer)
            {
                vkDestroyBuffer(device->device, index_buffer, nullptr);
                index_buffer = nullptr;
            }
            if (index_memory)
            {
                vkFreeMemory(device->device, index_memory, nullptr);
                index_memory = nullptr;
            }

            // cleanup textures
            for (auto &texture : textures)
            {
                texture.mv_image.destroy();
            }
            return;
        }
    };

    class Model
    {
    public:
        // objects of this model type
        std::vector<std::unique_ptr<mv::Object>> objects;

    public:
        Model(){};
        // Resize with copy was losing data
        Model(const Model &m) = delete;
        // Prefer move operations over copy
        Model(Model &&) = default;
        ~Model(void) {}

        mv::Device *device;
        // mv::Image image;
        bool has_texture = false; // do not assume model has texture
        VkDescriptorPool descriptor_pool = nullptr;
        std::string model_name;

        /*
            Assimp Loader Params
        */
        std::vector<_Mesh> _meshes;
        std::vector<_Texture> _loaded_textures;

        void _load(mv::Device *dvc, std::shared_ptr<mv::Allocator> descriptor_allocator, const char *filename)
        {
            assert(dvc);
            device = dvc;

            model_name = filename;

            Assimp::Importer importer;
            const aiScene *ai_scene = importer.ReadFile(filename, aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);

            if (!ai_scene)
            {
                throw std::runtime_error("Assimp failed to load model");
            }

            // process model data
            std::cout << "[+] Processing model => " << filename << std::endl;
            _process_node(ai_scene->mRootNode, ai_scene);

            std::cout << "\tMeshes loaded => " << _meshes.size() << std::endl;

            // Create buffer for each mesh
            for (auto &mesh : _meshes)
            {
                // if the model required textures
                // create the descriptor sets for them
                if (!mesh.textures.empty())
                {
                    std::cout << "inside model loader" << std::endl;
                    has_texture = true;
                    VkDescriptorSetLayout sampler_layout = descriptor_allocator->get_layout("sampler_layout");
                    for (auto &texture : mesh.textures)
                    {
                        descriptor_allocator->allocate_set(descriptor_allocator->get(),
                                                           sampler_layout,
                                                           texture.descriptor);
                        descriptor_allocator->update_set(descriptor_allocator->get(),
                                                         texture.mv_image.descriptor,
                                                         texture.descriptor, 0);
                    }
                }
                std::cout << std::endl
                          << "[+] Creating buffers for mesh => " << &mesh << std::endl;
                // create vertex buffer and load vertices
                device->create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                      mesh.vertices.size() * sizeof(Vertex),
                                      &mesh.vertex_buffer,
                                      &mesh.vertex_memory,
                                      mesh.vertices.data());

                // create index buffer, load indices data into it
                device->create_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                      mesh.indices.size() * sizeof(uint32_t),
                                      &mesh.index_buffer,
                                      &mesh.index_memory,
                                      mesh.indices.data());
            }

            return;
        }

        void _process_node(aiNode *node, const aiScene *scene)
        {
            for (uint32_t i = 0; i < node->mNumMeshes; i++)
            {
                aiMesh *l_mesh = scene->mMeshes[node->mMeshes[i]];
                std::cout << "\t\tLoading mesh" << std::endl;
                _meshes.push_back(_process_mesh(l_mesh, scene));
            }

            // recall function for children of this node
            for (uint32_t i = 0; i < node->mNumChildren; i++)
            {
                _process_node(node->mChildren[i], scene);
            }
            return;
        }

        _Mesh _process_mesh(aiMesh *mesh, const aiScene *scene)
        {
            std::cout << "\t\tProcessing mesh..." << std::endl;
            std::vector<Vertex> verts;
            std::vector<uint32_t> inds;
            std::vector<_Texture> texs;

            for (uint32_t i = 0; i < mesh->mNumVertices; i++)
            {
                Vertex v = {};

                // get vertices
                v.position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f};

                // get uv
                if (mesh->mTextureCoords[0])
                {
                    v.uv = {
                        // uv
                        (float)mesh->mTextureCoords[0][i].x,
                        (float)mesh->mTextureCoords[0][i].y,
                        0.0f, // fill last 2 float as ubo padding
                        0.0f};
                }

                aiColor4D cc;
                aiGetMaterialColor(scene->mMaterials[mesh->mMaterialIndex], AI_MATKEY_COLOR_DIFFUSE, &cc);
                v.color = {cc.r, cc.g, cc.b, cc.a};

                verts.push_back(v);
            }

            for (uint32_t i = 0; i < mesh->mNumFaces; i++)
            {
                aiFace face = mesh->mFaces[i];

                for (uint32_t j = 0; j < face.mNumIndices; j++)
                {
                    // get indices
                    inds.push_back(face.mIndices[j]);
                }
            }

            // get material textures
            if (mesh->mMaterialIndex >= 0)
            {
                aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];
                std::vector<_Texture> diffuse_maps = _load_material_textures(mat, aiTextureType_DIFFUSE, "texture_diffuse", scene);
                texs.insert(texs.end(), diffuse_maps.begin(), diffuse_maps.end());
            }

            // construct _Mesh then return
            _Mesh m;
            m.vertices = verts;
            m.indices = inds;
            m.textures = texs;
            return m;
        }

        std::vector<_Texture> _load_material_textures(aiMaterial *mat, aiTextureType type, std::string type_name, const aiScene *scene)
        {
            std::vector<_Texture> textures;
            for (uint32_t i = 0; i < mat->GetTextureCount(type); i++)
            {
                aiString t_name;
                mat->GetTexture(type, i, &t_name);
                bool skip = false;
                // check if this material texture has already been loaded
                for (uint32_t j = 0; j < _loaded_textures.size(); j++)
                {
                    if (strcmp(_loaded_textures[j].path.c_str(), t_name.C_Str()) == 0)
                    {
                        textures.push_back(_loaded_textures[j]);
                        skip = true;
                        break;
                    }
                }

                // if not already loaded, load it
                if (!skip)
                {
                    _Texture tex;
                    // TODO
                    // if embedded compressed texture type
                    std::string filename = std::string(t_name.C_Str());
                    filename = "models/" + filename;
                    std::replace(filename.begin(), filename.end(), '\\', '/');

                    tex.path = filename;
                    tex.type = type;
                    mv::Image::ImageCreateInfo create_info = {};
                    create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
                    create_info.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
                    create_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

                    // load texture
                    tex.mv_image.create(device, create_info, filename);
                    // add to vector for return
                    textures.push_back(tex);
                    // add to loaded_textures to save processing time in event of duplicate
                    _loaded_textures.push_back(tex);
                }
            }
            std::cout << "\t\t\tLoaded => " << textures.size() << " material textures" << std::endl;
            return textures;
        }
    };
};

#endif