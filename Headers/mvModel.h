#pragma once

#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>

#include "mvAllocator.h"
#include "mvBuffer.h"
#include "mvImage.h"

static constexpr float MOVESPEED = 0.05f;

class Texture
{
  public:
    Texture(){};
    ~Texture(){};
    std::string type;
    std::string path;
    Image mvImage;
    vk::DescriptorSet descriptor;
};

class Object
{
  public:
    Object(glm::vec3 p_Position, glm::vec3 p_Rotation)
    {
        this->position = p_Position;
        this->rotation = p_Rotation;
    }
    Object(void)
    {
    }
    ~Object()
    {
    }

    // allow move
    Object(Object &&) = default;
    Object &operator=(Object &&) = default;

    // delete copy
    Object(const Object &) = delete;
    Object &operator=(const Object &) = delete;

    struct Matrix
    {
        alignas(16) glm::mat4 model;
        // uv, normals implemented as Vertex attributes
    } matrix;

    // clang-format off
        float               scaleFactor = 1.0f;
        uint32_t            modelIndex = 0;
        glm::vec3           rotation = {0.0f, 0.0f, 0.0f};
        glm::vec3           position = {0.0f, 0.0f, 0.0f};
        glm::vec3           frontFace = {0.0f, 0.0f, 0.0f};
        std::pair<vk::DescriptorSet, MvBuffer> uniform;
    // clang-format on

    inline void rotateToAngle(float angle)
    {
        rotation.y = angle;
        return;
    }

    void getFrontFace(void)
    {
        glm::vec3 fr;
        fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
        fr.y = sin(glm::radians(rotation.x));
        fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
        fr = -glm::normalize(fr);

        frontFace = fr;
        return;
    }

    inline void move(float p_OrbitAngle, glm::vec4 p_UnitVectorAxis)
    {
        position += rotateVector(p_OrbitAngle, p_UnitVectorAxis) * MOVESPEED;
        return;
    }

    // w component should be 1.0f
    inline glm::vec3 rotateVector(float p_OrbitAngle, glm::vec4 p_TargetAxii)
    {
        // target_axis is our default directional vector

        // construct rotation matrix from orbit angle
        glm::mat4 rotationMatrix = glm::mat4(1.0);
        rotationMatrix =
            glm::rotate(rotationMatrix, glm::radians(p_OrbitAngle), glm::vec3(0.0f, 1.0f, 0.0f));

        // multiply our def vec

        glm::vec4 x_col = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 y_col = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 z_col = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 w_col = {0.0f, 0.0f, 0.0f, 0.0f};

        // x * x_column
        x_col.x = p_TargetAxii.x * rotationMatrix[0][0];
        x_col.y = p_TargetAxii.x * rotationMatrix[0][1];
        x_col.z = p_TargetAxii.x * rotationMatrix[0][2];
        x_col.w = p_TargetAxii.x * rotationMatrix[0][3];

        // y * y_column
        y_col.x = p_TargetAxii.y * rotationMatrix[1][0];
        y_col.y = p_TargetAxii.y * rotationMatrix[1][1];
        y_col.z = p_TargetAxii.y * rotationMatrix[1][2];
        y_col.w = p_TargetAxii.y * rotationMatrix[1][3];

        // z * z_column
        z_col.x = p_TargetAxii.z * rotationMatrix[2][0];
        z_col.y = p_TargetAxii.z * rotationMatrix[2][1];
        z_col.z = p_TargetAxii.z * rotationMatrix[2][2];
        z_col.w = p_TargetAxii.z * rotationMatrix[2][3];

        // w * w_column
        w_col.x = p_TargetAxii.w * rotationMatrix[3][0];
        w_col.y = p_TargetAxii.w * rotationMatrix[3][1];
        w_col.z = p_TargetAxii.w * rotationMatrix[3][2];
        w_col.w = p_TargetAxii.w * rotationMatrix[3][3];

        glm::vec4 f = x_col + y_col + z_col + w_col;

        // extract relevant data & return
        return {f.x, f.y, f.z};
    }

    inline void update(void)
    {
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0), position);

        glm::mat4 rotationMatrix = glm::mat4(1.0);
        rotationMatrix =
            glm::rotate(rotationMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        rotationMatrix =
            glm::rotate(rotationMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        rotationMatrix =
            glm::rotate(rotationMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0), glm::vec3(scaleFactor));

        matrix.model = translationMatrix * rotationMatrix * scaleMatrix;

        // update obj uniform

        // memcpy(uniformBuffer.mapped, &matrix.model, sizeof(struct
        // Object::Matrix));

        // std pair uniform
        // { vk::DescriptorSet, Buffer }
        memcpy(uniform.second.mapped, &matrix.model, sizeof(Object::Matrix));
    }
};

// Contains...
// glm::vec4 position
// glm::vec4 color
// glm::vec4 uv
struct Vertex
{
    glm::vec4 position = {0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 color = {0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 uv = {0.0f, 0.0f, 0.0f, 0.0f};

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        vk::VertexInputBindingDescription bindingDescription;
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = vk::VertexInputRate::eVertex;
        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions;

        // position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        // texture uv coordinates
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, uv);

        // color
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[2].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

// Collection of data that makes up the various components of a model
struct Mesh
{
    // we should remove this data after everything is loaded
    std::vector<uint32_t> indices;
    std::vector<struct Vertex> vertices;
    std::vector<Texture> textures;

    // TODO
    // Consider linking all related mesh objects into a single buffer.
    // Will use offsets to access data relevant to particular mesh the
    // application is drawing
    vk::Buffer vertexBuffer;
    vk::Buffer indexBuffer;

    vk::DeviceMemory vertexMemory;
    vk::DeviceMemory indexMemory;

    int mtlIndex = -1;

    // Bind vertex & index buffers if exist
    void bindBuffers(vk::CommandBuffer &p_CommandBuffer);
    void cleanup(const vk::Device &p_LogicalDevice);
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

    Model(void);
    ~Model();

    // All meshes combined
    vk::Buffer vertexBuffer;
    vk::Buffer indexBuffer;
    vk::DeviceMemory vertexMemory;
    vk::DeviceMemory indexMemory;

    uint32_t totalVertices = 0;
    uint32_t totalIndices = 0;
    uint32_t triangleCount = 0;

    // { vk::DescriptorSet, Texture }
    // Texture -> Image
    // Image -> vk::ImageInfo
    // vk::DescriptorSet -> vk::ImageInfo
    std::vector<std::pair<vk::DescriptorSet, Texture>> textureDescriptors;

    std::string modelName;
    bool hasTexture = false; // do not assume model has texture
    std::unique_ptr<std::vector<Object>> objects;

    // { {vertex offset, Texture vkDescriptorSet index}, { Index start,
    // Index count } } texture descriptor set is either a valid index to
    // textureDescriptors or -1
    std::vector<std::pair<std::pair<uint32_t, int>, std::pair<uint32_t, uint32_t>>> bufferOffsets;
    std::unique_ptr<std::vector<struct Mesh>> loadedMeshes;
    std::unique_ptr<std::vector<Texture>> loadedTextures;

    void load(Engine *p_Engine, Allocator &p_DescriptorAllocator, const char *p_Filename,
              bool p_OutputDebug = true);

    void processNode(Engine *p_Engine, aiNode *p_Node, const aiScene *p_Scene);

    Mesh processMesh(Engine *p_Engine, aiMesh *p_Mesh, const aiScene *p_Scene);

    std::vector<Texture> loadMaterialTextures(Engine *p_Engine, aiMaterial *p_Material,
                                              aiTextureType p_Type,
                                              [[maybe_unused]] std::string p_TypeName,
                                              [[maybe_unused]] const aiScene *p_Scene,
                                              int &p_MtlIndex);

    void bindBuffers(vk::CommandBuffer &p_CommandBuffer);
    void cleanup(Engine *p_Engine);
};
