#include "mvModel.h"
#include "mvHelper.h"

extern LogHandler logger;

/*
    MODEL METHODS
*/
Model::Model (void)
{
  objects = std::make_unique<std::vector<Object>> ();
  loadedMeshes = std::make_unique<std::vector<struct Mesh>> ();
  loadedTextures = std::make_unique<std::vector<Texture>> ();
}

Model::~Model () {}

void
Model::load (Engine *p_Engine,
             Allocator &p_DescriptorAllocator,
             const char *p_Filename,
             bool p_OutputDebug)
{

  modelName = p_Filename;

  Assimp::Importer importer;
  const aiScene *aiScene = importer.ReadFile (
      p_Filename,
      aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs | aiProcess_Triangulate
          | aiProcess_ValidateDataStructure | aiProcess_SplitLargeMeshes);

  if (!aiScene)
    {
      throw std::runtime_error ("Assimp failed to load model");
    }

  // process model data
  processNode (p_Engine, aiScene->mRootNode, aiScene);

  // Arrange data in contiguous arrays for loading into gpu memory
  std::vector<Vertex> allVertices;
  std::vector<uint32_t> allIndices;

  // Get total size of all meshes
  uint32_t vertexTotalSize = 0;
  uint32_t indexTotalSize = 0;

  uint32_t vertexOffset = 0;
  uint32_t indexStart = 0;
  for (auto &mesh : *loadedMeshes)
    {
      // For every texture loaded
      if (mesh.mtlIndex >= 0)
        {
          hasTexture = true;
          // { vk::DescriptorSet, Texture }
          vk::DescriptorSetLayout samplerLayout
              = p_DescriptorAllocator.getLayout (vk::DescriptorType::eCombinedImageSampler);

          p_DescriptorAllocator.allocateSet (samplerLayout,
                                             textureDescriptors.at (mesh.mtlIndex).first);

          p_DescriptorAllocator.updateSet (
              textureDescriptors.at (mesh.mtlIndex).second.mvImage.descriptor,
              textureDescriptors.at (mesh.mtlIndex).first,
              0);
        }

      auto meshVertexSize = static_cast<uint32_t> (mesh.vertices.size ()) * sizeof (Vertex);
      auto meshIndexSize = static_cast<uint32_t> (mesh.indices.size ()) * sizeof (uint32_t);

      // Append mesh data
      allVertices.insert (allVertices.end (), mesh.vertices.begin (), mesh.vertices.end ());
      allIndices.insert (allIndices.end (), mesh.indices.begin (), mesh.indices.end ());

      // clang-format off
        //
        // { {vertex offset, textureDescriptors index}, { Index start, Index count } }
        //
      // clang-format on
      bufferOffsets.push_back (
          { { vertexOffset, mesh.mtlIndex }, { indexStart, mesh.indices.size () } });

      indexStart += mesh.indices.size ();
      // vertexOffset += meshVertexSize;
      vertexOffset += mesh.vertices.size ();
      vertexTotalSize += meshVertexSize;
      indexTotalSize += meshIndexSize;
    }

  // Create large contiguous buffers for model data
  // Data loaded in allVertices & allIndices
  totalVertices = allVertices.size ();
  totalIndices = allIndices.size ();
  triangleCount = totalIndices / 3;

  // Create vertex buffer
  p_Engine->createBuffer (vk::BufferUsageFlagBits::eVertexBuffer,
                          vk::MemoryPropertyFlagBits::eHostCoherent
                              | vk::MemoryPropertyFlagBits::eHostVisible,
                          vertexTotalSize,
                          &vertexBuffer,
                          &vertexMemory,
                          allVertices.data ());
  // Create index buffer
  p_Engine->createBuffer (vk::BufferUsageFlagBits::eIndexBuffer,
                          vk::MemoryPropertyFlagBits::eHostCoherent
                              | vk::MemoryPropertyFlagBits::eHostVisible,
                          indexTotalSize,
                          &indexBuffer,
                          &indexMemory,
                          allIndices.data ());

  if (p_OutputDebug || true)
    {
      logger.logMessage ("\t :: Loaded model => " + std::string (p_Filename) + "\n\t\t Meshes => "
                         + std::to_string (loadedMeshes->size ()) + "\n\t\t Textures => "
                         + std::to_string (loadedTextures->size ()));
    }
  return;
}

void
Model::processNode (Engine *p_Engine, aiNode *p_Node, const aiScene *p_Scene)
{
  for (uint32_t i = 0; i < p_Node->mNumMeshes; i++)
    {
      aiMesh *mesh = p_Scene->mMeshes[p_Node->mMeshes[i]];
      loadedMeshes->push_back (processMesh (p_Engine, mesh, p_Scene));
    }

  // recall function for children of this node
  for (uint32_t i = 0; i < p_Node->mNumChildren; i++)
    {
      processNode (p_Engine, p_Node->mChildren[i], p_Scene);
    }
  return;
}

struct Mesh
Model::processMesh (Engine *p_Engine, aiMesh *p_Mesh, const aiScene *p_Scene)
{
  std::vector<uint32_t> inds;
  std::vector<struct Vertex> verts;
  std::vector<Texture> texs;

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
            static_cast<float> (p_Mesh->mTextureCoords[0][i].x),
            static_cast<float> (p_Mesh->mTextureCoords[0][i].y),
            0.0f,
            0.0f,
          };
        }

      aiColor4D materialColor;
      aiGetMaterialColor (
          p_Scene->mMaterials[p_Mesh->mMaterialIndex], AI_MATKEY_COLOR_DIFFUSE, &materialColor);
      v.color = {
        materialColor.r,
        materialColor.g,
        materialColor.b,
        materialColor.a,
      };

      verts.push_back (v);
    }

  for (uint32_t i = 0; i < p_Mesh->mNumFaces; i++)
    {
      aiFace face = p_Mesh->mFaces[i];

      for (uint32_t j = 0; j < face.mNumIndices; j++)
        {
          // get indices
          inds.push_back (face.mIndices[j]);
        }
    }

  // construct _Mesh then return
  struct Mesh m;

  // get material textures
  aiMaterial *material = p_Scene->mMaterials[p_Mesh->mMaterialIndex];
  std::vector<Texture> diffuseMaps = loadMaterialTextures (
      p_Engine, material, aiTextureType_DIFFUSE, "texture_diffuse", p_Scene, m.mtlIndex);
  texs.insert (texs.end (),
               std::make_move_iterator (diffuseMaps.begin ()),
               std::make_move_iterator (diffuseMaps.end ()));

  m.vertices = verts;
  m.indices = inds;
  m.textures = std::move (texs);
  return m;
}

std::vector<Texture>
Model::loadMaterialTextures (Engine *p_Engine,
                             aiMaterial *p_Material,
                             aiTextureType p_Type,
                             [[maybe_unused]] std::string p_TypeName,
                             [[maybe_unused]] const aiScene *p_Scene,
                             int &p_MtlIndex)
{
  std::vector<Texture> textures;
  for (uint32_t i = 0; i < p_Material->GetTextureCount (p_Type); i++)
    {
      aiString t_name;
      p_Material->GetTexture (p_Type, i, &t_name);
      bool skip = false;
      // check if this material texture has already been loaded
      for (uint32_t j = 0; j < loadedTextures->size (); j++)
        {
          if (strcmp (loadedTextures->at (j).path.c_str (), t_name.C_Str ()) == 0)
            {
              textures.push_back (std::move (loadedTextures->at (j)));
              skip = true;
              break;
            }
        }

      // if not already loaded, load it
      if (!skip)
        {
          Texture tex;
          // TODO
          // if embedded compressed texture type
          std::string filename = std::string (t_name.C_Str ());
          filename = "models/" + filename;
          std::replace (filename.begin (), filename.end (), '\\', '/');

          tex.path = filename;
          tex.type = p_Type;
          Image::ImageCreateInfo createInfo;
          createInfo.format = vk::Format::eR8G8B8A8Srgb;
          createInfo.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
          createInfo.tiling = vk::ImageTiling::eOptimal;
          createInfo.usage
              = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

          // load texture
          tex.mvImage.create (p_Engine->physicalDevice,
                              p_Engine->logicalDevice,
                              p_Engine->commandPool,
                              p_Engine->graphicsQueue,
                              createInfo,
                              filename);
          // add to vector for return
          textures.push_back (tex);
          // add to loaded_textures to save processing time in event of
          // duplicate
          loadedTextures->push_back (tex);
          vk::DescriptorSet descriptor;
          textureDescriptors.push_back (std::make_pair (descriptor, tex));
          p_MtlIndex = textureDescriptors.size () - 1;
        }
    }
  return textures;
}

void
Model::bindBuffers (vk::CommandBuffer &p_CommandBuffer)
{
  vk::DeviceSize offset = 0;
  p_CommandBuffer.bindVertexBuffers (0, vertexBuffer, offset);
  p_CommandBuffer.bindIndexBuffer (indexBuffer, 0, vk::IndexType::eUint32);
  return;
}
void
Model::cleanup (Engine *p_Engine)
{
  if (vertexBuffer)
    {
      p_Engine->logicalDevice.destroyBuffer (vertexBuffer);
      vertexBuffer = nullptr;
    }

  if (vertexMemory)
    {
      p_Engine->logicalDevice.freeMemory (vertexMemory);
      vertexMemory = nullptr;
    }

  if (indexBuffer)
    {
      p_Engine->logicalDevice.destroyBuffer (indexBuffer);
      indexBuffer = nullptr;
    }
  if (indexMemory)
    {
      p_Engine->logicalDevice.freeMemory (indexMemory);
      indexMemory = nullptr;
    }

  if (!textureDescriptors.empty ())
    {
      for (auto &descriptor : textureDescriptors)
        {
          descriptor.second.mvImage.destroy ();
        }
    }

  // Cleaned up by previous loop
  // if (!loadedTextures->empty())
  // {
  //     for (auto &texture : *loadedTextures)
  //     {
  //         texture.mvImage.destroy(p_LogicalDevice);
  //     }
  //     loadedTextures->clear();
  // }
  return;
}

/*
    MESH METHODS
*/
void
Mesh::bindBuffers (vk::CommandBuffer &p_CommandBuffer)
{
  vk::DeviceSize offsets = 0;
  p_CommandBuffer.bindVertexBuffers (0, 1, &vertexBuffer, &offsets);
  p_CommandBuffer.bindIndexBuffer (indexBuffer, 0, vk::IndexType::eUint32);
  return;
}

std::ostream &
operator<< (std::ostream &p_OutputStream, const Vertex &p_Vertex)
{
  p_OutputStream << p_Vertex.position.x << " " << p_Vertex.position.y << " " << p_Vertex.position.z
                 << " " << p_Vertex.position.w << "|" << p_Vertex.color.r << " " << p_Vertex.color.g
                 << " " << p_Vertex.color.b << " " << p_Vertex.color.a << "|" << p_Vertex.uv.x
                 << " " << p_Vertex.uv.y << " " << 0.0f << " " << 0.0f << "|" << p_Vertex.normal.x
                 << " " << p_Vertex.normal.y << " " << p_Vertex.normal.z << " " << p_Vertex.normal.w
                 << "\n";
  return p_OutputStream;
}

std::istream &
operator>> (std::istream &p_InputStream, Vertex &p_DestObject)
{
  std::string input;
  std::getline (p_InputStream, input);

  // If empty, disregard
  if (input.empty ())
    return p_InputStream;

  // Grab each segment of line
  // pos | color | uv | normal
  // x y z w|r g b a|u v 0 0|x y z w

  size_t posDelimit = input.find ('|');
  if (posDelimit == std::string::npos)
    {
      std::cout << "Current input : " << input << "\n";
      throw std::runtime_error ("Error reading position values, corrupted file");
    }

  size_t colorDelimit = input.find ('|', posDelimit + 1);
  if (colorDelimit == std::string::npos)
    {
      std::cout << "Current input : " << input << "\n";
      throw std::runtime_error ("Error reading color values, corrupted file");
    }

  size_t uvDelimit = input.find ('|', colorDelimit + 1);
  if (uvDelimit == std::string::npos)
    {
      std::cout << "Current input : " << input << "\n";
      throw std::runtime_error ("Error reading UV values, corrupted file");
    }

  std::string strPosition (input.substr (0, posDelimit));
  std::string strColor (&input.at (posDelimit + 1), &input.at (colorDelimit));
  std::string strUV (&input.at (colorDelimit + 1), &input.at (uvDelimit));
  std::string strNormal (input.substr (uvDelimit + 1));

  std::istringstream posStream (strPosition);
  std::istringstream colorStream (strColor);
  std::istringstream uvStream (strUV);
  std::istringstream normalStream (strNormal);

  std::vector<std::string> position{ std::istream_iterator<std::string> (posStream), {} };
  std::vector<std::string> color{ std::istream_iterator<std::string> (colorStream), {} };
  std::vector<std::string> uv{ std::istream_iterator<std::string> (uvStream), {} };
  std::vector<std::string> normal{ std::istream_iterator<std::string> (normalStream), {} };

  p_DestObject.position.x = std::stof (position.at (0));
  p_DestObject.position.y = std::stof (position.at (1));
  p_DestObject.position.z = std::stof (position.at (2));
  p_DestObject.position.w = std::stof (position.at (3));
  p_DestObject.color.r = std::stof (color.at (0));
  p_DestObject.color.g = std::stof (color.at (1));
  p_DestObject.color.b = std::stof (color.at (2));
  p_DestObject.color.a = std::stof (color.at (3));
  p_DestObject.uv.x = std::stof (uv.at (0));
  p_DestObject.uv.y = std::stof (uv.at (1));
  // p_DestObject.uv.z = std::stof(uv.at(2));
  // p_DestObject.uv.w = std::stof(uv.at(3));
  p_DestObject.uv.z = 0.0f; // Last 2 values of uv disregard
  p_DestObject.uv.w = 0.0f;
  p_DestObject.normal.x = std::stof (normal.at (0));
  p_DestObject.normal.y = std::stof (normal.at (1));
  //   p_DestObject.normal.z = std::stof (normal.at (2));
  //   p_DestObject.normal.w = std::stof (normal.at (3));
  p_DestObject.normal.z = 0; // last 2 normal, disregard
  p_DestObject.normal.w = 0;

  return p_InputStream;
}