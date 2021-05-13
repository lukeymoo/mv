#define TINYOBJLOADER_IMPLEMENTATION
#include "mvModel.h"

/*
    MODEL METHODS
*/
mv::Model::Model(void)
{
    objects = std::make_unique<std::vector<struct Object>>();
    loadedMeshes = std::make_unique<std::vector<struct Mesh>>();
    loadedTextures = std::make_unique<std::vector<struct Texture>>();
}

mv::Model::~Model()
{
}

void mv::Model::load(const mv::Device &p_MvDevice, mv::Allocator &p_DescriptorAllocator, const char *p_Filename)
{

    modelName = p_Filename;

    Assimp::Importer importer;
    const aiScene *aiScene = importer.ReadFile(p_Filename, aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);

    if (!aiScene)
    {
        throw std::runtime_error("Assimp failed to load model");
    }

    // process model data
    processNode(p_MvDevice, aiScene->mRootNode, aiScene);

    // Create buffer for each mesh
    for (auto &mesh : *loadedMeshes)
    {
        // if the model required textures
        // create the descriptor sets for them
        if (!mesh.textures.empty())
        {
            hasTexture = true;

            vk::DescriptorSetLayout samplerLayout = p_DescriptorAllocator.getLayout("sampler_layout");
            for (auto &texture : mesh.textures)
            {
                p_DescriptorAllocator.allocateSet(p_MvDevice, samplerLayout, texture.descriptor);
                p_DescriptorAllocator.updateSet(p_MvDevice, texture.mvImage.descriptor, texture.descriptor, 0);
            }
        }
        // create vertex buffer and load vertices
        p_MvDevice.createBuffer(vk::BufferUsageFlagBits::eVertexBuffer,
                                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                mesh.vertices.size() * sizeof(Vertex), &mesh.vertexBuffer, &mesh.vertexMemory,
                                mesh.vertices.data());

        // create index buffer, load indices data into it
        p_MvDevice.createBuffer(vk::BufferUsageFlagBits::eIndexBuffer,
                                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                mesh.indices.size() * sizeof(uint32_t), &mesh.indexBuffer, &mesh.indexMemory,
                                mesh.indices.data());
    }

    std::cout << "\t :: Loaded model => " << p_Filename << "\n";
    std::cout << "\t\t Meshes => " << loadedMeshes->size() << "\n";
    std::cout << "\t\t Textures => " << loadedTextures->size() << "\n";
    return;
}

void mv::Model::processNode(const mv::Device &p_MvDevice, aiNode *p_Node, const aiScene *p_Scene)
{
    for (uint32_t i = 0; i < p_Node->mNumMeshes; i++)
    {
        aiMesh *mesh = p_Scene->mMeshes[p_Node->mMeshes[i]];
        loadedMeshes->push_back(processMesh(p_MvDevice, mesh, p_Scene));
    }

    // recall function for children of this node
    for (uint32_t i = 0; i < p_Node->mNumChildren; i++)
    {
        processNode(p_MvDevice, p_Node->mChildren[i], p_Scene);
    }
    return;
}

struct mv::Mesh mv::Model::processMesh(const mv::Device &p_MvDevice, aiMesh *p_Mesh, const aiScene *p_Scene)
{
    std::vector<uint32_t> inds;
    std::vector<struct Vertex> verts;
    std::vector<struct Texture> texs;

    for (uint32_t i = 0; i < p_Mesh->mNumVertices; i++)
    {
        Vertex v = {};

        // get vertices
        v.position = {
            p_Mesh->mVertices[i].x,
            p_Mesh->mVertices[i].y,
            p_Mesh->mVertices[i].z,
            1.0f,
        };

        // get uv
        if (p_Mesh->mTextureCoords[0])
        {
            // Last two elements are for padding--not read in shader
            v.uv = {
                static_cast<float>(p_Mesh->mTextureCoords[0][i].x),
                static_cast<float>(p_Mesh->mTextureCoords[0][i].y),
                0.0f,
                0.0f,
            };
        }

        aiColor4D materialColor;
        aiGetMaterialColor(p_Scene->mMaterials[p_Mesh->mMaterialIndex], AI_MATKEY_COLOR_DIFFUSE, &materialColor);
        v.color = {
            materialColor.r,
            materialColor.g,
            materialColor.b,
            materialColor.a,
        };

        verts.push_back(v);
    }

    for (uint32_t i = 0; i < p_Mesh->mNumFaces; i++)
    {
        aiFace face = p_Mesh->mFaces[i];

        for (uint32_t j = 0; j < face.mNumIndices; j++)
        {
            // get indices
            inds.push_back(face.mIndices[j]);
        }
    }

    // get material textures
    if (p_Mesh->mMaterialIndex >= 0)
    {
        aiMaterial *material = p_Scene->mMaterials[p_Mesh->mMaterialIndex];
        std::vector<struct Texture> diffuseMaps =
            loadMaterialTextures(p_MvDevice, material, aiTextureType_DIFFUSE, "texture_diffuse", p_Scene);
        texs.insert(texs.end(), std::make_move_iterator(diffuseMaps.begin()),
                    std::make_move_iterator(diffuseMaps.end()));
    }

    // construct _Mesh then return
    struct Mesh m;
    m.vertices = verts;
    m.indices = inds;
    m.textures = std::move(texs);
    return m;
}

std::vector<struct mv::Texture> mv::Model::loadMaterialTextures(const mv::Device &p_MvDevice, aiMaterial *p_Material,
                                                                aiTextureType p_Type,
                                                                [[maybe_unused]] std::string p_TypeName,
                                                                [[maybe_unused]] const aiScene *p_Scene)
{
    std::vector<struct Texture> textures;
    for (uint32_t i = 0; i < p_Material->GetTextureCount(p_Type); i++)
    {
        aiString t_name;
        p_Material->GetTexture(p_Type, i, &t_name);
        bool skip = false;
        // check if this material texture has already been loaded
        for (uint32_t j = 0; j < loadedTextures->size(); j++)
        {
            if (strcmp(loadedTextures->at(j).path.c_str(), t_name.C_Str()) == 0)
            {
                textures.push_back(std::move(loadedTextures->at(j)));
                skip = true;
                break;
            }
        }

        // if not already loaded, load it
        if (!skip)
        {
            struct Texture tex;
            // TODO
            // if embedded compressed texture type
            std::string filename = std::string(t_name.C_Str());
            filename = "models/" + filename;
            std::replace(filename.begin(), filename.end(), '\\', '/');

            tex.path = filename;
            tex.type = p_Type;
            mv::Image::ImageCreateInfo createInfo;
            createInfo.format = vk::Format::eR8G8B8A8Srgb;
            createInfo.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            createInfo.tiling = vk::ImageTiling::eOptimal;
            createInfo.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

            // load texture
            tex.mvImage.create(p_MvDevice, createInfo, filename);
            // add to vector for return
            textures.push_back(std::move(tex));
            // add to loaded_textures to save processing time in event of duplicate
            loadedTextures->push_back(std::move(tex));
        }
    }
    return textures;
}

/*
    MESH METHODS
*/
void mv::Mesh::bindBuffers(vk::CommandBuffer &p_CommandBuffer)
{
    vk::DeviceSize offsets = 0;
    p_CommandBuffer.bindVertexBuffers(0, 1, &vertexBuffer, &offsets);
    p_CommandBuffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);
    return;
}

void mv::Mesh::cleanup(const mv::Device &p_MvDevice)
{
    if (vertexBuffer)
    {
        p_MvDevice.logicalDevice->destroyBuffer(vertexBuffer);
        vertexBuffer = nullptr;
    }

    if (vertexMemory)
    {
        p_MvDevice.logicalDevice->freeMemory(vertexMemory);
        vertexMemory = nullptr;
    }

    if (indexBuffer)
    {
        p_MvDevice.logicalDevice->destroyBuffer(indexBuffer);
        indexBuffer = nullptr;
    }
    if (indexMemory)
    {
        p_MvDevice.logicalDevice->freeMemory(indexMemory);
        indexMemory = nullptr;
    }

    // cleanup textures
    if (!textures.empty())
    {
        for (auto &texture : textures)
        {
            texture.mvImage.destroy(p_MvDevice);
        }
    }
    return;
}