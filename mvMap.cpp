#define STBI_IMAGE_IMPLEMENTATION
#include "mvMap.h"

// For struct Vertex definition
#include "mvModel.h"

// Image loading/decoding methods
#include "stb_image.h"

// For interfacing with gui handler
#include "mvGui.h"

// For createBuffer methods & access to vk::Device
#include "mvEngine.h"

MapHandler::MapHandler (Engine *p_ParentEngine)
{
  if (!p_ParentEngine)
    throw std::runtime_error (
        "Invalid core engine handler passed to map handler");

  this->ptrEngine = p_ParentEngine;
  return;
}

MapHandler::~MapHandler () { return; }

void
MapHandler::cleanup (const vk::Device &p_LogicalDevice)
{
  if (vertexBuffer)
    p_LogicalDevice.destroyBuffer (vertexBuffer);

  if (vertexMemory)
    p_LogicalDevice.freeMemory (vertexMemory);

  if (indexBuffer)
    p_LogicalDevice.destroyBuffer (indexBuffer);

  if (indexMemory)
    p_LogicalDevice.freeMemory (indexMemory);
  return;
}

// Read heightmap & update gui mapModal accordingly
void
MapHandler::readHeightMap (GuiHandler *p_Gui, std::string p_Filename)
{
  auto [vertices, indices] = readHeightMap (p_Filename);

  // validate before return
  if (vertices.empty () || indices.empty ())
    throw std::runtime_error (
        "Empty vertices/indices container returned; Failed to read map mesh");

  // update gui
  p_Gui->setLoadedTerrainFile (filename);

  return;
}

std::pair<std::vector<Vertex>, std::vector<uint32_t>>
MapHandler::readHeightMap (std::string p_Filename)
{
  //   // Look for already optimized filename
  //   if (isAlreadyOptimized (p_Filename))
  //     {
  //       // Read vertex file
  //       std::vector<Vertex> vertices;
  //       readVertexFile (p_Filename, vertices);
  //       std::cout << "Read " << vertices.size () << " vertices\n";

  //       // Read indices file
  //       std::vector<uint32_t> indices;
  //       readIndexFile (p_Filename, indices);
  //       std::cout << "Read " << indices.size () << " indices\n";

  //       // Ensure we setup sizes
  //       vertexCount = vertices.size ();
  //       indexCount = indices.size ();

  //       allocate (vertices, indices);

  //       isMapLoaded = true;
  //       filename = p_Filename;

  //       return { vertices, indices };
  //     }

  // Pre optimized file does not exist, load raw & generate optimizations

  try
    {
      // Check if file exists
      if (!std::filesystem::exists (p_Filename))
        throw std::runtime_error ("File " + p_Filename + " does not exist");

      // Read the image in
      int tXLength = 0;
      int tZLength = 0;
      int tChannels = 0;
      stbi_uc *stbiRawImage
          = stbi_load (p_Filename.c_str (), &tXLength, &tZLength, &tChannels,
                       STBI_rgb_alpha);

      if (!stbiRawImage)
        throw std::runtime_error ("STBI failed to open file => " + p_Filename);

      // validate return values
      if (tXLength < 0 || tZLength < 0 || tChannels < 0)
        throw std::runtime_error ("STBI returned invalid values :: width, "
                                  "height or color channels is < 0");

      size_t xLength = static_cast<size_t> (tXLength);
      size_t zLength = static_cast<size_t> (tZLength);
      [[maybe_unused]] size_t channels = static_cast<size_t> (tChannels);

      // Place into vector
      std::vector<unsigned char> rawImage (
          stbiRawImage, stbiRawImage + (xLength * zLength * 4));

      // cleanup raw c interface buf
      stbi_image_free (stbiRawImage);

      // Extract height values and place into new vector
      std::vector<Vertex> heightValues;
      int offset = 0;
      std::cout << "Getting height values from PNG\n";
      for (size_t j = 0; j < zLength; j++)
        {
          for (size_t i = 0; i < xLength; i++)
            {
              heightValues.push_back (Vertex{
                  {
                      static_cast<float> (i),
                      static_cast<float> (rawImage.at (offset)) * -1.0f,
                      static_cast<float> (j),
                      1.0f,
                  },
                  { 1.0f, 1.0f, 1.0f, 1.0f },
                  { 0.0f, 0.0f, 0.0f, 0.0f },
              });
              offset += 4;
            }
        }

      std::cout << heightValues.size () << " height values obtained\n";

      std::cout << "Hardware supports up to "
                << std::thread::hardware_concurrency ()
                << " concurrent threads\n";

      int i = static_cast<int> (std::thread::hardware_concurrency ());
      do
        {
          if (heightValues.size () % i == 0)
            break;
          i--;
        }
      while (i > 0);

      size_t splitSize = heightValues.size () / i;

      std::vector<std::thread> threads (static_cast<size_t> (i));

      std::cout << "Created " << threads.size () << " threads awaiting jobs\n";

      std::vector<std::vector<Vertex>> splitValues;

      std::vector<Vertex>::iterator beginIterator = heightValues.begin ();

      do
        {
          // Fetch and insert range
          std::vector<Vertex> tmp;
          std::copy (beginIterator, std::next (beginIterator, splitSize),
                     std::back_inserter (tmp));

          splitValues.push_back (tmp);

          beginIterator += splitSize;
          i--;
        }
      while (i > 0);

      std::mutex mtx; // lock access to mesh vector<vector>

      size_t idx = 0;
      auto spinThreadGenerateMesh = [&] (std::thread &thread) {
        if (idx < threads.size ())
          {
            auto job = [&] (size_t ourIndex) {
              bool copied = false;
              std::vector<Vertex> tmp;
              do
                {
                  if (!mtx.try_lock ())
                    continue;
                  // copy
                  auto cpy = splitValues.at (ourIndex);
                  // release
                  mtx.unlock ();
                  // do work
                  tmp = generateMesh ((xLength / threads.size ()),
                                      (zLength / threads.size ()), cpy);
                  copied = true;
                }
              while (!copied);
              bool done = false;
              do
                {
                  // try to lock
                  if (!mtx.try_lock ())
                    continue;
                  // if locked update vector
                  splitValues.at (ourIndex).clear ();
                  splitValues.at (ourIndex) = tmp;
                  mtx.unlock (); // release
                  done = true;   // exit
                }
              while (!done);
            };

            thread = std::thread (job, idx);
            idx++;
          }
      };

      auto joinThread = [&] (std::thread &thread) {
        if (thread.joinable ())
          {
            thread.join ();
          }
      };

      std::for_each (threads.begin (), threads.end (), spinThreadGenerateMesh);

      std::for_each (threads.begin (), threads.end (), joinThread);

      size_t tc = 0;
      for (const auto &v : splitValues)
        {
          tc += v.size ();
        }
      std::cout << "Pre optimized vertex count : " << tc << "\n";

      std::vector<std::pair<std::vector<Vertex>, std::vector<uint32_t>>>
          optimizedMesh (splitValues.size ());

      idx = 0;
      auto spinThreadOptimizeMesh = [&] (std::thread &thread) {
        if (idx < threads.size ())
          {
            // optimize mesh
            auto job = [&] (size_t ourIndex) {
              bool copied = false;
              std::pair<std::vector<Vertex>, std::vector<uint32_t>> pair;
              do
                {
                  if (!mtx.try_lock ())
                    continue;
                  // copy and release
                  auto cpy = splitValues.at (ourIndex);
                  mtx.unlock ();
                  copied = true;

                  // do work
                  pair = optimize (cpy);
                }
              while (!copied);

              bool done = false;
              do
                {
                  // try lock
                  if (!mtx.try_lock ())
                    continue;
                  // if lock, do work
                  optimizedMesh.at (ourIndex) = pair;
                  mtx.unlock ();
                  done = true;
                }
              while (!done);
            };

            thread = std::thread (job, idx);
            idx++;
          }
      };

      std::for_each (threads.begin (), threads.end (), spinThreadOptimizeMesh);

      std::for_each (threads.begin (), threads.end (), joinThread);

      // free memory from old mesh data
      for (auto &p : splitValues)
        {
          p.clear ();
        }
      splitValues.clear ();

      idx = 0;
      auto spinThreadWriteVertex = [&] (std::thread &thread) {
        if (idx < threads.size ())
          {
            auto job = [&] (size_t ourIndex) {
              auto fname = getBaseFilename (p_Filename);

              bool copied = false;
              do
                {
                  // try lock
                  if (!mtx.try_lock ())
                    continue;
                  // copy
                  auto cpy = optimizedMesh.at (ourIndex).first;
                  // release
                  mtx.unlock ();
                  // do work
                  writeVertexFile (fname + std::to_string (ourIndex), cpy);
                  copied = true;
                }
              while (!copied);
            };

            thread = std::thread (job, idx);
            idx++;
          }
      };

      // Write meshes to files
      std::for_each (threads.begin (), threads.end (), spinThreadWriteVertex);

      std::for_each (threads.begin (), threads.end (), joinThread);

      idx = 0;
      auto spinThreadWriteIndex = [&] (std::thread &thread) {
        if (idx < threads.size ())
          {
            auto job = [&] (size_t ourIndex) {
              auto fname = getBaseFilename (p_Filename);

              bool copied = false;
              do
                {
                  // try lock
                  if (!mtx.try_lock ())
                    continue;
                  // copy
                  auto cpy = optimizedMesh.at (ourIndex).second;
                  // release
                  mtx.unlock ();
                  // do work
                  writeIndexFile (fname + std::to_string (ourIndex), cpy);
                  copied = true;
                }
              while (!copied);
            };

            thread = std::thread (job, idx);
            idx++;
          }
      };

      std::for_each (threads.begin (), threads.end (), spinThreadWriteIndex);

      std::for_each (threads.begin (), threads.end (), joinThread);

      std::vector<Vertex> vertices;
      std::vector<uint32_t> indices;

      // concat vertices
      for (auto &p : optimizedMesh)
        {
          std::cout << "First mesh offsets v: " << vertices.size ()
                    << " i: " << indices.size () << "|" << p.second.size ()
                    << "\n";
          vertexOffsets.push_back (vertices.size ());
          indexOffsets.push_back ({ indices.size (), p.second.size () });
          // copy vertices
          std::copy (p.first.begin (), p.first.end (),
                     std::back_inserter (vertices));
          // copy indices
          std::copy (p.second.begin (), p.second.end (),
                     std::back_inserter (indices));
        }

      vertexCount = vertices.size ();
      indexCount = indices.size ();
      allocate (vertices, indices);

      isMapLoaded = true;
      filename = p_Filename;

      return { vertices, indices };
    }
  catch (std::filesystem::filesystem_error &e)
    {
      throw std::runtime_error ("Failed to open heightmap " + p_Filename
                                + " => " + e.what ());
    }
  catch (std::exception &e)
    {
      throw std::runtime_error ("std error => " + std::string (e.what ()));
    }
}

std::pair<std::vector<Vertex>, std::vector<uint32_t>>
MapHandler::optimize (std::vector<Vertex> &p_Vertices)
{
  std::vector<Vertex> uniques;
  std::vector<uint32_t> indices;

  // Iterate unsorted vertices
  bool start = true;
  size_t startIndex = 0;
  for (const auto &unoptimized : p_Vertices)
    {
      bool isUnique = true;
      // Get start index for last 10k vertices
      if (!uniques.empty ())
        {
          if (!start)
            {
              if (uniques.size () % 5000 == 0)
                {
                  startIndex += 5000;
                }
            }
          else
            {
              start = false;
            }

          for (size_t i = startIndex; i < uniques.size (); i++)
            {
              if (uniques.at (i).position == unoptimized.position)
                {
                  isUnique = false;
                  indices.push_back (i);
                }
            }
        }
      if (isUnique)
        {
          uniques.push_back (unoptimized);
          indices.push_back (uniques.size () - 1);
        }
    }

  return { uniques, indices };
}

void
MapHandler::bindBuffer (vk::CommandBuffer &p_CommandBuffer)
{
  p_CommandBuffer.bindVertexBuffers (0, vertexBuffer, vk::DeviceSize{ 0 });
  p_CommandBuffer.bindIndexBuffer (indexBuffer, 0, vk::IndexType::eUint32);
  return;
}

bool
MapHandler::isAlreadyOptimized (std::string p_OriginalFilenameRequested)
{
  std::string nameV = filenameToBinV (p_OriginalFilenameRequested);
  std::string nameI = filenameToBinI (p_OriginalFilenameRequested);
  try
    {
      return (std::filesystem::exists (nameV)
              && std::filesystem::exists (nameI));
    }
  catch (std::filesystem::filesystem_error &e)
    {
      throw std::runtime_error ("Filesystem error : "
                                + std::string (e.what ()));
    }
}

std::string
MapHandler::getBaseFilename (std::string &p_Filename)
{
  return p_Filename.substr (0, p_Filename.find ("."));
}

std::string
MapHandler::filenameToBinV (std::string &p_Filename)
{
  return p_Filename.substr (0, p_Filename.find (".")) + "_v.bin";
}

std::string
MapHandler::filenameToBinI (std::string &p_Filename)
{
  return p_Filename.substr (0, p_Filename.find (".")) + "_i.bin";
}

void
MapHandler::writeVertexFile (std::string p_FilenameIndexAppended,
                             std::vector<Vertex> &p_VertexContainer)
{
  std::ofstream file (filenameToBinV (p_FilenameIndexAppended),
                      std::ios::trunc);
  std::ostream_iterator<Vertex> vertexOutputIterator (file);

  file << "v " << p_VertexContainer.size () << "\n";

  std::copy (p_VertexContainer.begin (), p_VertexContainer.end (),
             vertexOutputIterator);

  file.close ();
  return;
}

void
MapHandler::writeIndexFile (std::string p_FilenameIndexAppended,
                            std::vector<uint32_t> &p_IndexContainer)
{
  std::ofstream file (filenameToBinI (p_FilenameIndexAppended),
                      std::ios::trunc);
  std::ostream_iterator<uint32_t> indexOutputIterator (file, "\n");

  file << "i " << p_IndexContainer.size () << "\n";

  std::copy (p_IndexContainer.begin (), p_IndexContainer.end (),
             indexOutputIterator);

  file.close ();
  return;
}

void
MapHandler::writeRaw (std::string p_FilenameIndexAppended,
                      std::vector<Vertex> &p_VertexContainer,
                      std::vector<uint32_t> &p_IndexContainer)
{
  std::cout << "All output operations complete\n";
  return;
}

void
MapHandler::readVertexFile (std::string p_OriginalFilenameRequested,
                            std::vector<Vertex> &p_VertexContainer)
{
  std::cout << "Reading vertex bin file : "
            << filenameToBinV (p_OriginalFilenameRequested) << "\n";
  std::ifstream file (filenameToBinV (p_OriginalFilenameRequested));
  if (!file.is_open ())
    throw std::runtime_error ("Failed to open pre optimized file");

  // Read vertices count
  std::string strVertexCount;

  std::getline (file, strVertexCount);
  size_t vColon = strVertexCount.find (' ');
  if (vColon == std::string::npos)
    throw std::runtime_error (
        "Could not determine vertex count from map file");

  vColon++;
  size_t vertexCount = 0;
  try
    {
      vertexCount = std::stol (strVertexCount.substr (vColon));
    }
  catch (std::invalid_argument &e)
    {
      throw std::runtime_error (
          "Invalid vertex count in map file, received => "
          + std::string (e.what ()));
    }
  if (!vertexCount)
    throw std::runtime_error ("Invalid vertex count in map file, = 0");

  p_VertexContainer.reserve (vertexCount);

  std::cout << "Beginning read of vertex data\n";

  std::vector<Vertex>::iterator iterator = p_VertexContainer.begin ();
  std::insert_iterator<std::vector<Vertex>> vertexInserter (p_VertexContainer,
                                                            iterator);

  std::copy (std::istream_iterator<Vertex> (file),
             std::istream_iterator<Vertex> (), vertexInserter);

  // std::copy(std::istream_iterator<Vertex>(file),
  //           std::istream_iterator<Vertex>(),
  //           std::back_inserter(p_VertexContainer));

  file.close ();
  return;
}

void
MapHandler::readIndexFile (std::string p_OriginalFilenameRequested,
                           std::vector<uint32_t> &p_IndexContainer)
{
  std::cout << "Reading index bin file : "
            << filenameToBinI (p_OriginalFilenameRequested) << "\n";
  std::ifstream file (filenameToBinI (p_OriginalFilenameRequested));
  if (!file.is_open ())
    throw std::runtime_error ("Failed to open pre optimized file");

  std::string strIndexCount;
  std::getline (file, strIndexCount);
  size_t iColon = strIndexCount.find (' ');
  if (iColon == std::string::npos)
    throw std::runtime_error ("Could not determine index count");

  iColon++;
  size_t indexCount = 0;
  try
    {
      indexCount = std::stol (strIndexCount.substr (iColon));
    }
  catch (std::invalid_argument &e)
    {
      throw std::runtime_error ("Invalid index count in map file, received => "
                                + std::string (e.what ()));
    }
  if (!indexCount)
    throw std::runtime_error ("Invalid index count in map file, = 0");

  p_IndexContainer.reserve (indexCount);

  std::cout << "Beginning read of index data\n";

  std::vector<uint32_t>::iterator iterator = p_IndexContainer.begin ();
  std::insert_iterator<std::vector<uint32_t>> indexInserter (p_IndexContainer,
                                                             iterator);

  std::copy (std::istream_iterator<uint32_t> (file),
             std::istream_iterator<uint32_t> (), indexInserter);

  // std::copy(std::istream_iterator<uint32_t>(file),
  //           std::istream_iterator<uint32_t>(),
  //           std::back_inserter(p_IndexContainer));

  file.close ();
  return;
}

void
MapHandler::allocate (std::vector<Vertex> &p_VertexContainer,
                      std::vector<uint32_t> &p_IndexContainer)
{
  {
    // If a map is already loaded, release preallocated vulkan memory
    if (isMapLoaded)
      {
        ptrEngine->logicalDevice.waitIdle ();
        std::cout << "Map already loaded so cleaning up\n";
        if (vertexBuffer)
          {
            ptrEngine->logicalDevice.destroyBuffer (vertexBuffer);
            vertexBuffer = nullptr;
          }
        if (vertexMemory)
          {
            ptrEngine->logicalDevice.freeMemory (vertexMemory);
            vertexMemory = nullptr;
          }
        if (indexBuffer)
          {
            ptrEngine->logicalDevice.destroyBuffer (indexBuffer);
            indexBuffer = nullptr;
          }
        if (indexMemory)
          {
            ptrEngine->logicalDevice.freeMemory (indexMemory);
            indexMemory = nullptr;
          }
      }
  }

  {
    std::cout << "Creating buffers for map mesh\n";
    std::cout << "Vertices count : " << p_VertexContainer.size () << "\n";
    std::cout << "Indices count : " << p_IndexContainer.size () << "\n";
    // Create vulkan resources for our map
    using enum vk::MemoryPropertyFlagBits;

    // Create vertex buffer
    ptrEngine->createBuffer (
        vk::BufferUsageFlagBits::eVertexBuffer, eHostCoherent | eHostVisible,
        p_VertexContainer.size () * sizeof (Vertex), &vertexBuffer,
        &vertexMemory, p_VertexContainer.data ());

    // Create index buffer
    ptrEngine->createBuffer (
        vk::BufferUsageFlagBits::eIndexBuffer, eHostCoherent | eHostVisible,
        p_IndexContainer.size () * sizeof (uint32_t), &indexBuffer,
        &indexMemory, p_IndexContainer.data ());
  }
  return;
}

std::vector<Vertex>
MapHandler::generateMesh (size_t p_XLength, size_t p_ZLength,
                          std::vector<Vertex> &p_HeightValues)
{
  std::vector<Vertex> vertices;

  std::cout << "gen mesh given " << p_HeightValues.size () << "\n";

  // Generates terribly bloated & inefficient mesh data
  // Iterate image height
  for (size_t j = 0; j < (p_ZLength - 1); j++)
    {
      // Iterate image width
      for (size_t i = 0; i < (p_XLength - 1); i++)
        {
          size_t topLeftIndex = (p_ZLength * j) + i;
          size_t topRightIndex = (p_ZLength * j) + (i + 1);
          size_t bottomLeftIndex = (p_ZLength * (j + 1)) + i;
          size_t bottomRightIndex = (p_ZLength * (j + 1)) + (i + 1);

          Vertex topLeft = p_HeightValues.at (topLeftIndex);
          Vertex topRight = p_HeightValues.at (topRightIndex);
          Vertex bottomLeft = p_HeightValues.at (bottomLeftIndex);
          Vertex bottomRight = p_HeightValues.at (bottomRightIndex);

          vertices.push_back (topLeft);
          vertices.push_back (bottomLeft);
          vertices.push_back (bottomRight);

          vertices.push_back (topLeft);
          vertices.push_back (bottomRight);
          vertices.push_back (topRight);
        }
    }
  std::cout << "done: " << vertices.size () << "\n";
  return vertices;
}