#ifndef HEADERS_MVMODEL_H_
#define HEADERS_MVMODEL_H_

#include <vulkan/vulkan.hpp>

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

static constexpr float MOVESPEED = 0.05f;

namespace mv
{
    struct _Texture
    {
        std::string type;
        std::string path;
        mv::Image mv_image;
        vk::DescriptorSet descriptor;
    };

    struct Object
    {
        Object(glm::vec3 position, glm::vec3 rotation)
        {
            this->position = position;
            this->rotation = rotation;
        }
        Object(void) {}
        ~Object() {}
        struct Matrices
        {
            alignas(16) glm::mat4 model;
            // uv, normals implemented as Vertex attributes
        } matrices;

        vk::DescriptorSet model_descriptor;
        vk::DescriptorSet texture_descriptor;
        mv::Buffer uniform_buffer;
        glm::vec3 rotation = glm::vec3(1.0);
        glm::vec3 position = glm::vec3(1.0);
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

        inline void rotate_to_face(float angle)
        {
            rotation.y = angle;
            return;
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

        inline void move(float orbit_angle, glm::vec4 axis)
        {
            position += rotate_vector(orbit_angle, axis) * MOVESPEED;
            return;
        }

        void move_left(float orbit_angle)
        {
            // position -= glm::normalize(glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED;
            position += rotate_vector(orbit_angle, glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f)) * MOVESPEED;
            return;
        }
        void move_right(float orbit_angle)
        {
            // position += glm::normalize(glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED;
            position += rotate_vector(orbit_angle, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)) * MOVESPEED;
            return;
        }
        void move_forward(float orbit_angle)
        {
            // position += camera_front * MOVESPEED;
            position += rotate_vector(orbit_angle, glm::vec4(0.0f, 0.0f, -1.0f, 1.0f)) * MOVESPEED;
            return;
        }
        void move_backward(float orbit_angle)
        {
            position += rotate_vector(orbit_angle, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)) * MOVESPEED;
            return;
        }

        // w component should be 1.0f
        inline glm::vec3 rotate_vector(float orbit_angle, glm::vec4 target_axis)
        {
            // target_axis is our default directional vector

            // construct rotation matrix from orbit angle
            glm::mat4 rot_mat = glm::mat4(1.0);
            rot_mat = glm::rotate(rot_mat, glm::radians(orbit_angle), glm::vec3(0.0f, 1.0f, 0.0f));

            // multiply our def vec

            glm::vec4 x_col = {0.0f, 0.0f, 0.0f, 0.0f};
            glm::vec4 y_col = {0.0f, 0.0f, 0.0f, 0.0f};
            glm::vec4 z_col = {0.0f, 0.0f, 0.0f, 0.0f};
            glm::vec4 w_col = {0.0f, 0.0f, 0.0f, 0.0f};

            // x * x_column
            x_col.x = target_axis.x * rot_mat[0][0];
            x_col.y = target_axis.x * rot_mat[0][1];
            x_col.z = target_axis.x * rot_mat[0][2];
            x_col.w = target_axis.x * rot_mat[0][3];

            // y * y_column
            y_col.x = target_axis.y * rot_mat[1][0];
            y_col.y = target_axis.y * rot_mat[1][1];
            y_col.z = target_axis.y * rot_mat[1][2];
            y_col.w = target_axis.y * rot_mat[1][3];

            // z * z_column
            z_col.x = target_axis.z * rot_mat[2][0];
            z_col.y = target_axis.z * rot_mat[2][1];
            z_col.z = target_axis.z * rot_mat[2][2];
            z_col.w = target_axis.z * rot_mat[2][3];

            // w * w_column
            w_col.x = target_axis.w * rot_mat[3][0];
            w_col.y = target_axis.w * rot_mat[3][1];
            w_col.z = target_axis.w * rot_mat[3][2];
            w_col.w = target_axis.w * rot_mat[3][3];

            glm::vec4 f = x_col + y_col + z_col + w_col;

            // extract relevant data & return
            return {f.x, f.y, f.z};
        }
    };

    struct Vertex
    {
        glm::vec4 position;
        glm::vec4 uv;
        glm::vec4 color;

        static vk::VertexInputBindingDescription get_binding_description()
        {
            vk::VertexInputBindingDescription binding_description;
            binding_description.binding = 0;
            binding_description.stride = sizeof(Vertex);
            binding_description.inputRate = vk::VertexInputRate::eVertex;
            return binding_description;
        }

        static std::array<vk::VertexInputAttributeDescription, 3> get_attribute_descriptions()
        {
            // Temp container to be returned
            std::array<vk::VertexInputAttributeDescription, 3> attribute_descriptions{};

            // position
            attribute_descriptions[0].binding = 0;
            attribute_descriptions[0].location = 0;
            attribute_descriptions[0].format = vk::Format::eR32G32B32A32Sfloat;
            attribute_descriptions[0].offset = offsetof(Vertex, position);

            // texture uv coordinates
            attribute_descriptions[1].binding = 0;
            attribute_descriptions[1].location = 1;
            attribute_descriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
            attribute_descriptions[1].offset = offsetof(Vertex, uv);

            // color
            attribute_descriptions[2].binding = 0;
            attribute_descriptions[2].location = 2;
            attribute_descriptions[2].format = vk::Format::eR32G32B32A32Sfloat;
            attribute_descriptions[2].offset = offsetof(Vertex, color);

            return attribute_descriptions;
        }
    };

    // Collection of data that makes up the various components of a model
    struct _Mesh
    {
        // we should remove this data after everything is loaded
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<_Texture> textures;

        // TODO
        // Consider linking all related mesh objects into a single buffer.
        // Will use offsets to access data relevant to particular mesh the
        // application is drawing
        vk::Buffer vertex_buffer;
        vk::Buffer index_buffer;

        vk::DeviceMemory vertex_memory;
        vk::DeviceMemory index_memory;

        void bindBuffers(vk::CommandBuffer &command_buffer)
        {
            vk::DeviceSize offsets = 0;
            command_buffer.bindVertexBuffers(0, 1, &vertex_buffer, &offsets);
            command_buffer.bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint32);
            return;
        }

        void cleanup(std::weak_ptr<mv::Device> mv_device)
        {
            auto m_dvc = mv_device.lock();
            if (!m_dvc)
                throw std::runtime_error("Passed invalid mv device handler :: model handler");

            if (vertex_buffer)
            {
                m_dvc->logical_device->destroyBuffer(vertex_buffer);
                vertex_buffer = nullptr;
            }
            if (vertex_memory)
            {
                m_dvc->logical_device->freeMemory(vertex_memory);
                vertex_memory = nullptr;
            }

            if (index_buffer)
            {
                m_dvc->logical_device->destroyBuffer(index_buffer);
                index_buffer = nullptr;
            }
            if (index_memory)
            {
                m_dvc->logical_device->freeMemory(index_memory);
                index_memory = nullptr;
            }

            // cleanup textures
            if (!textures.empty())
            {
                for (auto &texture : textures)
                {
                    texture.mv_image.destroy();
                }
            }
            return;
        }
    };

    class Model
    {
    public:
        // remove copy operations
        Model(const Model &) = delete;
        Model &operator=(const Model &) = delete;

        // allow move operations
        Model(Model &&) = default;
        Model &operator=(Model &&) = default;

        Model(void)
        {
            std::cout << "[+] Model container created\n";
            objects = std::make_unique<std::vector<mv::Object>>();
            _meshes = std::make_unique<std::vector<_Mesh>>();
            _loaded_textures = std::make_unique<std::vector<_Texture>>();
            std::cout << "\t-- Objects container size => " << objects->size() << "\n";
            std::cout << "\t-- _meshes container size => " << _meshes->size() << "\n";
            std::cout << "\t-- _loaded_textures container size => " << _loaded_textures->size() << "\n";
        }
        ~Model() {}

        std::string model_name;
        bool has_texture = false; // do not assume model has texture

        // owns
        std::unique_ptr<std::vector<mv::Object>> objects;
        std::unique_ptr<std::vector<_Mesh>> _meshes;
        std::unique_ptr<std::vector<_Texture>> _loaded_textures;

        // references
        std::weak_ptr<mv::Device> mv_device;
        std::weak_ptr<mv::Allocator> descriptor_allocator;

        void _load(std::weak_ptr<mv::Device> mv_device,
                   std::weak_ptr<mv::Allocator> descriptor_allocator,
                   const char *filename)
        {
            auto m_dvc = mv_device.lock();
            auto m_alloc = descriptor_allocator.lock();

            if (!m_dvc)
                throw std::runtime_error("Invalid mv device handle passed :: model handler");

            if (!m_alloc)
                throw std::runtime_error("Invalid descriptor allocator handle passed :: model handler");

            this->mv_device = mv_device;
            this->descriptor_allocator = descriptor_allocator;

            model_name = filename;

            Assimp::Importer importer;
            const aiScene *ai_scene = importer.ReadFile(filename,
                                                        aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);

            if (!ai_scene)
            {
                throw std::runtime_error("Assimp failed to load model");
            }

            // process model data
            _process_node(ai_scene->mRootNode, ai_scene);

            // Create buffer for each mesh
            for (auto &mesh : *_meshes)
            {
                // if the model required textures
                // create the descriptor sets for them
                if (!mesh.textures.empty())
                {
                    has_texture = true;

                    vk::DescriptorSetLayout sampler_layout = m_alloc->get_layout("sampler_layout");
                    for (auto &texture : mesh.textures)
                    {
                        m_alloc->allocate_set(m_alloc->get(),
                                              sampler_layout,
                                              texture.descriptor);
                        m_alloc->update_set(m_alloc->get(),
                                            texture.mv_image.descriptor,
                                            texture.descriptor, 0);
                    }
                }
                // create vertex buffer and load vertices
                m_dvc->create_buffer(vk::BufferUsageFlagBits::eVertexBuffer,
                                     vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                     mesh.vertices.size() * sizeof(Vertex),
                                     &mesh.vertex_buffer,
                                     &mesh.vertex_memory,
                                     mesh.vertices.data());

                // create index buffer, load indices data into it
                m_dvc->create_buffer(vk::BufferUsageFlagBits::eIndexBuffer,
                                     vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                     mesh.indices.size() * sizeof(uint32_t),
                                     &mesh.index_buffer,
                                     &mesh.index_memory,
                                     mesh.indices.data());
            }

            std::cout << "\t :: Loaded model => " << filename << "\n";
            std::cout << "\t\t Meshes => " << _meshes->size() << "\n";
            std::cout << "\t\t Textures => " << _loaded_textures->size() << "\n";
            std::cout << "\t\t Default object count => " << objects->size() << "\n";
            return;
        }

        void _process_node(aiNode *node, const aiScene *scene)
        {
            for (uint32_t i = 0; i < node->mNumMeshes; i++)
            {
                aiMesh *l_mesh = scene->mMeshes[node->mMeshes[i]];
                _meshes->push_back(_process_mesh(l_mesh, scene));
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
                texs.insert(texs.end(),
                            std::make_move_iterator(diffuse_maps.begin()),
                            std::make_move_iterator(diffuse_maps.end()));
            }

            // construct _Mesh then return
            _Mesh m;
            m.vertices = verts;
            m.indices = inds;
            m.textures = std::move(texs);
            return m;
        }

        std::vector<_Texture> _load_material_textures(aiMaterial *mat, aiTextureType type, [[maybe_unused]] std::string type_name, [[maybe_unused]] const aiScene *scene)
        {
            std::vector<_Texture> textures;
            for (uint32_t i = 0; i < mat->GetTextureCount(type); i++)
            {
                aiString t_name;
                mat->GetTexture(type, i, &t_name);
                bool skip = false;
                // check if this material texture has already been loaded
                for (uint32_t j = 0; j < _loaded_textures->size(); j++)
                {
                    if (strcmp(_loaded_textures->at(j).path.c_str(), t_name.C_Str()) == 0)
                    {
                        textures.push_back(std::move(_loaded_textures->at(j)));
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
                    mv::Image::ImageCreateInfo create_info;
                    create_info.format = vk::Format::eR8G8B8A8Srgb;
                    create_info.memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
                    create_info.tiling = vk::ImageTiling::eOptimal;
                    create_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

                    // load texture
                    tex.mv_image.create(mv_device, create_info, filename);
                    // add to vector for return
                    textures.push_back(std::move(tex));
                    // add to loaded_textures to save processing time in event of duplicate
                    _loaded_textures->push_back(std::move(tex));
                }
            }
            return textures;
        }
    };
};

#endif