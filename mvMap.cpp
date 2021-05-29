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
  if (isAlreadyOptimized (p_Filename))
    {
      // Read vertex file
      std::vector<Vertex> vertices;
      readVertexFile (p_Filename, vertices);
      std::cout << "Read " << vertices.size () << " vertices\n";

      // Read indices file
      std::vector<uint32_t> indices;
      readIndexFile (p_Filename, indices);
      std::cout << "Read " << indices.size () << " indices\n";

      // Ensure we setup sizes
      vertexCount = vertices.size ();
      indexCount = indices.size ();

      allocate (vertices, indices);

      isMapLoaded = true;
      filename = p_Filename;

      return { vertices, indices };
    }

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

      if (tXLength % 2 != 0 || tXLength != tZLength)
        throw std::runtime_error (
            "Maps must be even squares where each side is divisible by 2");

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
      size_t splitWidth = xLength / 2;
      for (size_t j = 0; j < zLength; j++)
        {
          for (size_t i = 0; i < xLength; i++)
            {
              glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

              // if left edge
              if (i == 0 || i == splitWidth)
                {
                  if (j == 0 || j == splitWidth) // if top left
                    {
                      color = { 1.0f, 1.0f, 0.0f, 1.0f };
                    }
                  else if (j == splitWidth - 1
                           || j == zLength - 1) // if bottom left
                    {
                      color = { 1.0f, 0.5f, 0.5f, 1.0f };
                    }
                  else
                    {
                      color = { 1.0f, 0.0f, 0.0f, 1.0f };
                    }
                }
              // if top edge
              if (j == 0 || j == splitWidth)
                {
                  if (i == 0 || i == splitWidth) // top left
                    {
                      color = { 1.0f, 1.0f, 0.0f, 1.0f };
                    }
                  else if (i == splitWidth - 1
                           || i == xLength - 1) // top right
                    {
                      color = { 0.0f, 1.0f, 1.0f, 1.0f };
                    }
                  else
                    {
                      color = { 0.0f, 1.0f, 0.0f, 1.0f };
                    }
                }
              // right edge
              if (i == splitWidth - 1 || i == xLength - 1)
                {
                  if (j == 0 || j == splitWidth) // top right
                    {
                      color = { 0.0f, 1.0f, 1.0f, 1.0f };
                    }
                  else if (j == splitWidth - 1
                           || j == zLength - 1) // bottom right
                    {
                      color = { 0.5f, 0.5f, 1.0f, 1.0f };
                    }
                  else
                    {
                      color = { 0.0f, 0.0f, 1.0f, 1.0f };
                    }
                }
              // bottom edge
              if (j == splitWidth - 1 || j == zLength - 1)
                {
                  // bottom left
                  if (i == 0 || i == splitWidth)
                    {
                      color = { 1.0f, 0.5f, 0.5f, 1.0f };
                    }
                  else if (i == splitWidth - 1
                           || i == xLength - 1) // bottom right
                    {
                      color = { 0.5f, 0.5f, 1.0f, 1.0f };
                    }
                  else
                    {
                      color = { 0.5f, 0.5f, 0.5f, 1.0f };
                    }
                }
              heightValues.push_back (Vertex{
                  {
                      static_cast<float> (i),
                      static_cast<float> (rawImage.at (offset)) * -1.0f,
                      static_cast<float> (j),
                      1.0f,
                  },
                  color,
                  { 0.0f, 0.0f, 0.0f, 0.0f },
              });
              offset += 4;
            }
        }
      // Divide map in half -- we can use UP 4 cores
      // Anything less and we simply parse each section in a ring buffer of
      // threads
      if (xLength % 2 != 0)
        throw std::runtime_error (
            "Maps must be an even square; Where a side is divisible by 2");

      size_t numCores = (std::thread::hardware_concurrency () >= 4)
                            ? 4
                            : std::thread::hardware_concurrency ();

      std::vector<std::thread> threads (numCores);

      std::cout << "Created " << threads.size () << " threads awaiting jobs\n";

      std::vector<std::vector<Vertex>> splitValues;

      std::vector<Vertex>::iterator beginIterator = heightValues.begin ();
      std::vector<Vertex>::iterator bottomQuadIterator
          = heightValues.begin () + (xLength * splitWidth);
      std::vector<Vertex>::iterator endIterator = heightValues.end ();

      // Need to divide mesh into even squares
      // We lose dimensions of map when just fetching contiguous elements
      // copy top quadrants
      std::vector<Vertex> tlm;
      std::vector<Vertex> trm;
      bool leftOrRight = false; // false = left
      do
        {
          if (!leftOrRight) // copy left
            {
              std ::copy (beginIterator, std::next (beginIterator, splitWidth),
                          std::back_inserter (tlm));
            }
          else // copy right
            {
              std::copy (beginIterator, std::next (beginIterator, splitWidth),
                         std::back_inserter (trm));
            }
          // Flip
          leftOrRight = !leftOrRight;
          // Incr
          beginIterator += splitWidth;
        }
      while (beginIterator < bottomQuadIterator);

      // Copy bottom quadrants
      std::vector<Vertex> blm;
      std::vector<Vertex> brm;
      leftOrRight = false; // false = left
      do
        {

          if (!leftOrRight) // copy bottom left
            {
              std::copy (beginIterator, std::next (beginIterator, splitWidth),
                         std::back_inserter (blm));
            }
          else // copy bottom right
            {
              std::copy (beginIterator, std::next (beginIterator, splitWidth),
                         std::back_inserter (brm));
            }
          // flip
          leftOrRight = !leftOrRight;
          // incr
          beginIterator += splitWidth;
        }
      while (beginIterator < endIterator);

      splitValues = { tlm, trm, blm, brm };

      std::mutex mtx; // lock access to mesh vector<vector>

      size_t idx = 0;

      auto spinThreadGenerateMesh = [&] (std::thread &thread) {
        if (idx < splitValues.size ())
          {
            auto job = [&] (size_t ourIndex) {
              std::vector<Vertex> tmp;
              do
                {
                  if (!mtx.try_lock ())
                    continue;

                  auto cpy = splitValues.at (ourIndex);
                  mtx.unlock ();
                  tmp = generateMesh (splitWidth, splitWidth, cpy);
                  break; // done gen mesh
                }
              while (1);

              // save results
              do
                {
                  if (!mtx.try_lock ())
                    continue;
                  splitValues.at (ourIndex).clear ();
                  splitValues.at (ourIndex) = tmp;
                  mtx.unlock ();
                  break; // done saving
                }
              while (1);
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

      struct ChunkData
      {
        std::vector<Vertex *> leftEdge;
        std::vector<Vertex *> rightEdge;
        std::vector<Vertex *> topEdge;
        std::vector<Vertex *> bottomEdge;

        Vertex *tl = nullptr;
        Vertex *tr = nullptr;
        Vertex *bl = nullptr;
        Vertex *br = nullptr;
      };

      std::vector<ChunkData> chunks (4);

      // iterate each mesh vertices; find origin of edges
      size_t chunkidx = 0;
      for (auto &m : splitValues) // for each chunk
        {
          // iterate each vertex & find our tagged vertices
          for (auto &v : m)
            {
              // left edge
              if (v.color == glm::vec4 (1.0f, 0.0f, 0.0f, 1.0f))
                {
                  chunks.at (chunkidx).leftEdge.push_back (&v);
                }
              // top edge
              if (v.color == glm::vec4 (0.0f, 1.0f, 0.0f, 1.0f))
                {
                  chunks.at (chunkidx).topEdge.push_back (&v);
                }
              // right edge
              if (v.color == glm::vec4 (0.0f, 0.0f, 1.0f, 1.0f))
                {
                  chunks.at (chunkidx).rightEdge.push_back (&v);
                }
              // bottom edge
              if (v.color == glm::vec4 (0.5f, 0.5f, 0.5f, 1.0f))
                {
                  chunks.at (chunkidx).bottomEdge.push_back (&v);
                }

              // corner cases
              if (v.color == glm::vec4 (1.0f, 1.0f, 0.0f, 1.0f)) // top left
                {
                  chunks.at (chunkidx).tl = &v;
                }
              if (v.color == glm::vec4 (0.0f, 1.0f, 1.0f, 1.0f)) // top right
                {
                  chunks.at (chunkidx).tr = &v;
                }
              if (v.color == glm::vec4 (1.0f, 0.5f, 0.5f, 1.0f)) // bottom left
                {
                  chunks.at (chunkidx).bl = &v;
                }
              if (v.color
                  == glm::vec4 (0.5f, 0.5f, 1.0f, 1.0f)) // bottom right
                {
                  chunks.at (chunkidx).br = &v;
                }
            }
          chunkidx++;
        }

      std::vector<Vertex> stitches;

      /*
        Stitch all the edges of chunks & reset colors
      */
      {
        // clean edge before stitching
        cleanEdge (chunks.at (0).rightEdge, false);
        cleanEdge (chunks.at (1).leftEdge, false);
        // stitch tl -> tr
        stitchEdge (chunks.at (0).rightEdge, chunks.at (1).leftEdge, stitches,
                    true);

        // prune dups
        cleanEdge (chunks.at (2).rightEdge, false);
        cleanEdge (chunks.at (3).leftEdge, false);
        // stitch bl -> br
        stitchEdge (chunks.at (2).rightEdge, chunks.at (3).leftEdge, stitches,
                    true);

        // prune dups
        cleanEdge (chunks.at (0).bottomEdge, true);
        cleanEdge (chunks.at (2).topEdge, true);
        // stich tl -> bl
        stitchEdge (chunks.at (0).bottomEdge, chunks.at (2).topEdge, stitches,
                    false);

        // prune dups
        cleanEdge (chunks.at (1).bottomEdge, true);
        cleanEdge (chunks.at (3).topEdge, true);
        // stitch tr -> br
        stitchEdge (chunks.at (1).bottomEdge, chunks.at (3).topEdge, stitches,
                    false);

        // stitch tl -> tr corner
        stitchCorner (chunks.at (0).tr, chunks.at (0).rightEdge.at (0),
                      chunks.at (1).tl, chunks.at (1).leftEdge.at (0),
                      stitches, eFirstCornerToFirstEdge);

        // stitch tl -> bl corner
        stitchCorner (chunks.at (0).bl, chunks.at (0).bottomEdge.at (0),
                      chunks.at (2).tl, chunks.at (2).topEdge.at (0), stitches,
                      eFirstCornerToSecondCorner);

        // stitch bl -> br corner
        stitchCorner (
            chunks.at (2).br,
            chunks.at (2).rightEdge.at (chunks.at (2).rightEdge.size () - 1),
            chunks.at (3).bl,
            chunks.at (3).leftEdge.at (chunks.at (3).leftEdge.size () - 1),
            stitches, eFirstEdgeToFirstCorner);

        // stitch tr -> br corner
        stitchCorner (
            chunks.at (1).br,
            chunks.at (1).bottomEdge.at (chunks.at (1).bottomEdge.size () - 1),
            chunks.at (3).tr,
            chunks.at (3).topEdge.at (chunks.at (3).topEdge.size () - 1),
            stitches, eFirstEdgeToSecondEdge);

        // stitch tl -> bl RIGHT corner
        stitchCorner (
            chunks.at (0).br,
            chunks.at (0).bottomEdge.at (chunks.at (0).bottomEdge.size () - 1),
            chunks.at (2).tr,
            chunks.at (2).topEdge.at (chunks.at (2).topEdge.size () - 1),
            stitches, eFirstEdgeToSecondEdge);

        // stitch tr -> br LEFT CORNER
        stitchCorner (chunks.at (1).bl, chunks.at (1).bottomEdge.at (0),
                      chunks.at (3).tl, chunks.at (3).topEdge.at (0), stitches,
                      eFirstCornerToSecondCorner);
        // stitch bl -> br RIGHT CORNER
        stitchCorner (chunks.at (2).tr, chunks.at (2).rightEdge.at (0),
                      chunks.at (3).tl, chunks.at (3).leftEdge.at (0),
                      stitches, eFirstCornerToFirstEdge);

        // stitch tl -> tr
        stitchCorner (
            chunks.at (0).br,
            chunks.at (0).rightEdge.at (chunks.at (0).rightEdge.size () - 1),
            chunks.at (1).bl,
            chunks.at (1).leftEdge.at (chunks.at (1).leftEdge.size () - 1),
            stitches, eFirstEdgeToFirstCorner);

        // stitch center corners - provide params left->right order
        stitchCorner (chunks.at (0).br, chunks.at (1).bl, chunks.at (2).tr,
                      chunks.at (3).tl, stitches, eCenter);

        // append to splitvals after all stitches generated; otherwise we
        // invalidate our ptrs
        splitValues.at (0).insert (splitValues.at (0).end (),
                                   stitches.begin (), stitches.end ());

        // reset colors for all verts
        std::for_each (splitValues.begin (), splitValues.end (),
                       [] (std::vector<Vertex> &values) {
                         std::for_each (
                             values.begin (), values.end (),
                             [] (Vertex &vertex) {
                               vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
                             });
                       });
      }

      size_t tc = 0;
      for (const auto &v : splitValues)
        {
          tc += v.size ();
        }

      std::vector<std::pair<std::vector<Vertex>, std::vector<uint32_t>>>
          optimizedMesh (splitValues.size ());

      idx = 0;
      auto spinThreadOptimizeMesh = [&] (std::thread &thread) {
        if (idx < splitValues.size ())
          {
            // optimize mesh
            auto job = [&] (size_t ourIndex) {
              std::pair<std::vector<Vertex>, std::vector<uint32_t>> pair;
              // copy mesh data & optimize
              do
                {
                  if (!mtx.try_lock ())
                    continue;
                  // copy and release
                  auto cpy = splitValues.at (ourIndex);
                  mtx.unlock ();

                  // do work
                  pair = optimize (cpy);
                  break; // job done
                }
              while (1);

              // move mesh into container safely
              do
                {
                  // try lock
                  if (!mtx.try_lock ())
                    continue;
                  // if lock, do work
                  optimizedMesh.at (ourIndex) = pair;
                  mtx.unlock ();
                  break; // job done
                }
              while (1);
            };

            thread = std::thread (job, idx);
            idx++;
          }
      };

      std::for_each (threads.begin (), threads.end (), spinThreadOptimizeMesh);

      std::for_each (threads.begin (), threads.end (), joinThread);

      idx = 0;
      auto spinThreadWriteVertex = [&] (std::thread &thread) {
        if (idx < splitValues.size ())
          {
            auto job = [&] (size_t ourIndex) {
              auto fname = getBaseFilename (p_Filename);
              do
                {
                  // try lock
                  if (!mtx.try_lock ())
                    continue;
                  auto cpy = optimizedMesh.at (ourIndex).first;
                  mtx.unlock ();
                  // do work
                  writeVertexFile (fname + std::to_string (ourIndex), cpy);
                  break; // job done
                }
              while (1);
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
        if (idx < splitValues.size ())
          {
            auto job = [&] (size_t ourIndex) {
              auto fname = getBaseFilename (p_Filename);

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
                  break; // job done
                }
              while (1);
            };

            thread = std::thread (job, idx);
            idx++;
          }
      };

      std::for_each (threads.begin (), threads.end (), spinThreadWriteIndex);

      std::for_each (threads.begin (), threads.end (), joinThread);

      // free memory from old mesh data
      for (auto &p : splitValues)
        {
          p.clear ();
        }
      splitValues.clear ();

      std::vector<Vertex> vertices;
      std::vector<uint32_t> indices;

      // concat vertices
      for (auto &p : optimizedMesh)
        {
          vertexOffsets.push_back (vertices.size ());
          indexOffsets.push_back ({ indices.size (), p.second.size () });

          // increment each indice by indices.size() before copy to apply its
          // offset
          std::for_each (p.second.begin (), p.second.end (),
                         [&] (uint32_t &index) { index += vertices.size (); });

          // copy vertices
          std::copy (p.first.begin (), p.first.end (),
                     std::back_inserter (vertices));

          // copy indices
          std::copy (p.second.begin (), p.second.end (),
                     std::back_inserter (indices));
        }

      // align indices for each submesh (indice += start offset)
      // { index start, index count }

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

  // Generates terribly bloated & inefficient mesh data
  // Iterate image height
  for (size_t j = 0; j < (p_ZLength - 1); j++)
    {
      // Iterate image width
      for (size_t i = 0; i < (p_XLength - 1); i++)
        {
          const size_t topLeftIndex = (p_ZLength * j) + i;
          const size_t topRightIndex = (p_ZLength * j) + (i + 1);
          const size_t bottomLeftIndex = (p_ZLength * (j + 1)) + i;
          const size_t bottomRightIndex = (p_ZLength * (j + 1)) + (i + 1);

          Vertex topLeft = p_HeightValues.at (topLeftIndex);
          Vertex topRight = p_HeightValues.at (topRightIndex);
          Vertex bottomLeft = p_HeightValues.at (bottomLeftIndex);
          Vertex bottomRight = p_HeightValues.at (bottomRightIndex);

          vertices.push_back (topLeft);
          vertices.push_back (bottomLeft);
          vertices.push_back (bottomRight);

          vertices.push_back (bottomRight);
          vertices.push_back (topRight);
          vertices.push_back (topLeft);
        }
    }
  return vertices;
}

void
MapHandler::cleanEdge (std::vector<Vertex *> &p_Edge, bool isHorizontal)
{
  std::vector<Vertex *> optimized;
  if (isHorizontal)
    {
      // prune x axis
      for (auto &vptr : p_Edge)
        {
          bool unique = true;
          // search for x:z val
          if (!optimized.empty ())
            {
              // iterate optimized and look for dup
              for (const auto &o : optimized)
                {
                  if (o->position == vptr->position)
                    {
                      unique = false;
                    }
                }
            }
          if (unique) // add to optimized
            {
              optimized.push_back (vptr);
            }
        }
    }
  else
    {
      // prune z axis
      for (auto &vptr : p_Edge)
        {
          bool unique = true;
          // search for x:z val
          if (!optimized.empty ())
            {
              // iterate optimized and look for dup
              for (const auto &o : optimized)
                {
                  if (o->position == vptr->position)
                    {
                      unique = false;
                    }
                }
            }
          if (unique) // add to optimized
            {
              optimized.push_back (vptr);
            }
        }
    }
  p_Edge.clear ();
  p_Edge = optimized;
  return;
}

// must be same size
// stitches edges together with param 1 being origin
void
MapHandler::stitchEdge (std::vector<Vertex *> &p_FirstEdge,
                        std::vector<Vertex *> &p_SecondEdge,
                        std::vector<Vertex> &p_Vertices, bool p_IsVertical)
{
  // sanity check
  if (p_FirstEdge.size () != p_SecondEdge.size ())
    throw std::runtime_error ("Cannot stich edges of uneven lengths");

  if (p_IsVertical)
    {
      for (size_t i = 0; i < (p_FirstEdge.size () - 1); i++)
        {
          // grab verts for quad
          auto tl = p_FirstEdge.at (i);
          auto bl = p_FirstEdge.at (i + 1);
          auto br = p_SecondEdge.at (i + 1);
          auto tr = p_SecondEdge.at (i);

          tl->color = { 1.0f, 0.0f, 0.0f, 1.0f };
          bl->color = { 0.0f, 1.0f, 0.0f, 1.0f };
          br->color = { 0.0f, 1.0f, 1.0f, 1.0f };
          tr->color = { 0.0f, 1.0f, 1.0f, 1.0f };

          // push counter clockwise
          p_Vertices.push_back (*tl);
          p_Vertices.push_back (*bl);
          p_Vertices.push_back (*br);

          p_Vertices.push_back (*br);
          p_Vertices.push_back (*tr);
          p_Vertices.push_back (*tl);
        }
    }
  else
    {
      for (size_t i = 0; i < (p_FirstEdge.size () - 1); i++)
        {
          // grab verts for quad
          auto tl = p_FirstEdge.at (i);
          auto bl = p_SecondEdge.at (i);
          auto br = p_SecondEdge.at (i + 1);
          auto tr = p_FirstEdge.at (i + 1);

          // push counter clockwise
          p_Vertices.push_back (*tl);
          p_Vertices.push_back (*bl);
          p_Vertices.push_back (*br);

          p_Vertices.push_back (*br);
          p_Vertices.push_back (*tr);
          p_Vertices.push_back (*tl);
        }
    }
  return;
}

void
MapHandler::stitchCorner (Vertex *p_FirstCorner, Vertex *p_FirstEdgeVertex,
                          Vertex *p_SecondCorner, Vertex *p_SecondEdgeVertex,
                          std::vector<Vertex> &p_Vertices,
                          WindingStyle p_Style)
{
  using enum WindingStyle;
  if (p_Style == eFirstCornerToFirstEdge)
    {
      auto tl = p_FirstCorner;
      auto bl = p_FirstEdgeVertex;
      auto br = p_SecondEdgeVertex;
      auto tr = p_SecondCorner;

      p_Vertices.push_back (*tl);
      p_Vertices.push_back (*bl);
      p_Vertices.push_back (*br);

      p_Vertices.push_back (*br);
      p_Vertices.push_back (*tr);
      p_Vertices.push_back (*tl);
    }
  else if (p_Style == eFirstCornerToSecondCorner)
    {
      auto tl = p_FirstCorner;
      auto bl = p_SecondCorner;
      auto br = p_SecondEdgeVertex;
      auto tr = p_FirstEdgeVertex;

      p_Vertices.push_back (*tl);
      p_Vertices.push_back (*bl);
      p_Vertices.push_back (*br);

      p_Vertices.push_back (*br);
      p_Vertices.push_back (*tr);
      p_Vertices.push_back (*tl);
    }
  else if (p_Style == eFirstEdgeToFirstCorner)
    {
      auto tl = p_FirstEdgeVertex;
      auto bl = p_FirstCorner;
      auto br = p_SecondCorner;
      auto tr = p_SecondEdgeVertex;

      p_Vertices.push_back (*tl);
      p_Vertices.push_back (*bl);
      p_Vertices.push_back (*br);

      p_Vertices.push_back (*br);
      p_Vertices.push_back (*tr);
      p_Vertices.push_back (*tl);
    }
  else if (p_Style == eFirstEdgeToSecondEdge)
    {
      auto tl = p_FirstEdgeVertex;
      auto bl = p_SecondEdgeVertex;
      auto br = p_SecondCorner;
      auto tr = p_FirstCorner;

      p_Vertices.push_back (*tl);
      p_Vertices.push_back (*bl);
      p_Vertices.push_back (*br);

      p_Vertices.push_back (*br);
      p_Vertices.push_back (*tr);
      p_Vertices.push_back (*tl);
    }
  else if (p_Style == eCenter)
    {
      // param
      // 1
      // 3
      // 4
      // 2
      auto tl = p_FirstCorner;
      auto bl = p_SecondCorner;
      auto br = p_SecondEdgeVertex;
      auto tr = p_FirstEdgeVertex;

      p_Vertices.push_back (*tl);
      p_Vertices.push_back (*bl);
      p_Vertices.push_back (*br);

      p_Vertices.push_back (*br);
      p_Vertices.push_back (*tr);
      p_Vertices.push_back (*tl);
    }
  return;
}

// void
// MapHandler::stitchCorner (Vertex *p_TLCorner, Vertex *p_TRCorner,
//                           Vertex *p_BRCorner, Vertex *p_BLCorner,
//                           std::vector<Vertex> &p_Vertices)
// {
//   return;
// }