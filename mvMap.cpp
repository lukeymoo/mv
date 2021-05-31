#define STBI_IMAGE_IMPLEMENTATION
#include "mvMap.h"

// For handling terrain related textures
#include "mvImage.h"

// For struct Vertex definition
#include "mvModel.h"

// Image loading/decoding methods
#include "stb_image.h"

// For interfacing with gui handler
#include "mvGui.h"

// For createBuffer methods & access to vk::Device
#include "mvEngine.h"

MapHandler::MapHandler(Engine *p_ParentEngine)
{
    if (!p_ParentEngine)
        throw std::runtime_error("Invalid core engine handler passed to map handler");

    this->ptrEngine = p_ParentEngine;
    return;
}

MapHandler::~MapHandler()
{
    return;
}

void MapHandler::cleanup(const vk::Device &p_LogicalDevice)
{
    if (terrainTexture)
    {
        terrainTexture->destroy();
        terrainTexture.reset();
    }
    if (terrainNormal)
    {
        terrainNormal->destroy();
        terrainNormal.reset();
    }
    if (vertexBuffer)
    {
        p_LogicalDevice.destroyBuffer(*vertexBuffer);
        vertexBuffer.reset();
    }

    if (vertexMemory)
    {
        p_LogicalDevice.freeMemory(*vertexMemory);
        vertexMemory.reset();
    }

    if (indexBuffer)
    {
        p_LogicalDevice.destroyBuffer(*indexBuffer);
        indexBuffer.reset();
    }

    if (indexMemory)
    {
        p_LogicalDevice.freeMemory(*indexMemory);
        indexMemory.reset();
    }
    return;
}

// Read heightmap & update gui mapModal accordingly
void MapHandler::readHeightMap(GuiHandler *p_Gui, std::string p_Filename, bool p_ForceReload)
{
    if (!ptrEngine)
        throw std::runtime_error("Pass map handler core engine before attempting to read heightmaps");

    // call real method
    auto [vertices, indices] = readHeightMap(p_Filename, p_ForceReload);

    // validate before return
    if (vertices.empty() || indices.empty())
        throw std::runtime_error("Empty vertices/indices container returned; Failed to read map mesh");

    // update gui
    p_Gui->setLoadedTerrainFile(filename);

    // Load default terrain texture
    if (!terrainTexture)
    {
        terrainTexture = std::make_unique<Image>(); // image
        terrainNormal = std::make_unique<Image>();  // normal map

        Image::ImageCreateInfo textureCreateInfo = {};
        textureCreateInfo.tiling = vk::ImageTiling::eOptimal;
        textureCreateInfo.format = vk::Format::eR8G8B8A8Srgb;
        textureCreateInfo.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        textureCreateInfo.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

        Image::ImageCreateInfo normalCreateInfo = {};
        normalCreateInfo.tiling = vk::ImageTiling::eOptimal;
        normalCreateInfo.format = vk::Format::eR8G8B8A8Srgb;
        normalCreateInfo.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        normalCreateInfo.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

        try
        {
            terrainTexture->create(ptrEngine->physicalDevice, ptrEngine->logicalDevice, ptrEngine->commandPool,
                                   ptrEngine->graphicsQueue, textureCreateInfo, "terrain/crackedMud.png");

            auto layout = ptrEngine->allocator->getLayout(vk::DescriptorType::eCombinedImageSampler);

            ptrEngine->allocator->allocateSet(layout, terrainTextureDescriptor);
            ptrEngine->allocator->updateSet(terrainTexture->descriptor, terrainTextureDescriptor, 0);

            terrainNormal->create(ptrEngine->physicalDevice, ptrEngine->logicalDevice, ptrEngine->commandPool,
                                  ptrEngine->graphicsQueue, normalCreateInfo, "terrain/crackedMudNormal.png");

            ptrEngine->allocator->allocateSet(layout, terrainNormalDescriptor);
            ptrEngine->allocator->updateSet(terrainNormal->descriptor, terrainNormalDescriptor, 0);
        }
        catch (std::exception &e)
        {
            // cleanup any resources partially created
            terrainTexture->destroy();
            terrainNormal->destroy();
            throw std::runtime_error("Failed to load terrain texture => " + std::string(e.what()));
        }

        std::cout << "Loaded terrain texture\n";
    }

    return;
}

std::pair<std::vector<Vertex>, std::vector<uint32_t>> MapHandler::readHeightMap(std::string p_Filename,
                                                                                bool p_ForceReload)
{
    //   // Look for already optimized filename
    if (isAlreadyOptimized(p_Filename) && !p_ForceReload)
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::cout << "Loading preoptimized files\n";
        loadPreoptimized(p_Filename, vertices, indices);

        // ensure we got something
        if (vertices.empty() || indices.empty())
            throw std::runtime_error("Failed to load map, empty vertices/indices");

        allocate(vertices, indices);

        isMapLoaded = true;
        filename = p_Filename;
        indexCount = indices.size();
        vertexCount = vertices.size();
        return {vertices, indices};
    }

    // Pre optimized file does not exist, load raw & generate optimized

    try
    {
        // Check if file exists
        if (!std::filesystem::exists(p_Filename))
            throw std::runtime_error("File " + p_Filename + " does not exist");

        size_t xLength = 0;
        size_t zLength = 0;
        size_t splitWidth = 0;
        [[maybe_unused]] size_t channels = 0;

        // Initial vertices from raw PNG values
        std::vector<Vertex> heightValues;

        {
            std::vector<unsigned char> rawImage;
            std::cout << "Loading PNG\n";
            loadPNG(xLength, zLength, channels, p_Filename, rawImage);

            // Extract height values and place into new vector
            std::cout << "Converting pixels to vertex objects\n";
            pngToVertices(xLength, rawImage, heightValues);
        }

        splitWidth = xLength / 2;

        // Divided map quadrants (4 quads)
        std::vector<std::vector<Vertex>> splitValues(4);

        std::vector<Vertex>::iterator beginIterator = heightValues.begin();
        std::vector<Vertex>::iterator bottomQuadIterator = heightValues.begin() + (xLength * splitWidth);
        std::vector<Vertex>::iterator endIterator = heightValues.end();

        { // split heightValues into 4 chunks
            std::cout << "Splitting into chunks\n";
            { // copy top left & top right map mesh quadrants
                size_t leftOrRight = 0;
                do
                {
                    if (!leftOrRight) // copy left
                    {
                        std ::copy(beginIterator, std::next(beginIterator, splitWidth),
                                   std::back_inserter(splitValues.at(leftOrRight)));
                        leftOrRight++;
                    }
                    else // copy right
                    {
                        std::copy(beginIterator, std::next(beginIterator, splitWidth),
                                  std::back_inserter(splitValues.at(leftOrRight)));
                        leftOrRight--;
                    }
                    beginIterator += splitWidth; // next chunk
                } while (beginIterator < bottomQuadIterator);
            }

            { // copy bottom left & bottom right quadrants
                // 2 = left, 3 = right
                size_t leftOrRight = 2;
                do
                {
                    if (leftOrRight == 2) // copy bottom left
                    {
                        std::copy(beginIterator, std::next(beginIterator, splitWidth),
                                  std::back_inserter(splitValues.at(leftOrRight)));
                        leftOrRight++;
                    }
                    else if (leftOrRight == 3) // copy bottom right
                    {
                        std::copy(beginIterator, std::next(beginIterator, splitWidth),
                                  std::back_inserter(splitValues.at(leftOrRight)));
                        leftOrRight--;
                    }
                    beginIterator += splitWidth; // next chunk
                } while (beginIterator < endIterator);
            }

            heightValues.clear(); // done with heightValues
        }

        size_t numThreads = 4;
        std::mutex mtx_splitValues;
        std::vector<std::thread> threads(numThreads);

        auto joinThread = [&](std::thread &thread) { thread.join(); };

        std::cout << "Converting pixel chunks to meshes\n";

        // generate proper mesh from chunkified heightvalues
        // heightvalues is only an array of single point vertices(1 vertex per height)
        size_t idx = 0;
        std::for_each(threads.begin(), threads.end(),
                      [&mtx_splitValues, &splitValues, &idx, splitWidth, this](std::thread &thread) {
                          thread = std::thread([&mtx_splitValues, &splitValues, idx, splitWidth, this]() {
                              std::vector<Vertex> tmp;
                              { // copy vertices
                                  std::lock_guard<std::mutex> lock{mtx_splitValues};
                                  tmp = splitValues.at(idx);
                              }
                              tmp = generateMesh(splitWidth, splitWidth, tmp);
                              { // store result
                                  std::lock_guard<std::mutex> lock{mtx_splitValues};
                                  splitValues.at(idx) = tmp;
                              }
                          });
                          idx++;
                      });

        std::for_each(threads.begin(), threads.end(), joinThread);

        std::vector<ChunkData> chunks(4);

        // grab each chunk's edge vertices we tagged earlier
        size_t chunkidx = 0;
        for (auto &m : splitValues) // for each chunk
        {
            // iterate each vertex & find our tagged vertices
            for (auto &v : m)
            {
                // left edge
                if (v.color == glm::vec4(1.0f, 0.0f, 0.0f, 1.0f))
                {
                    chunks.at(chunkidx).leftEdge.push_back(&v);
                }
                // top edge
                if (v.color == glm::vec4(0.0f, 1.0f, 0.0f, 1.0f))
                {
                    chunks.at(chunkidx).topEdge.push_back(&v);
                }
                // right edge
                if (v.color == glm::vec4(0.0f, 0.0f, 1.0f, 1.0f))
                {
                    chunks.at(chunkidx).rightEdge.push_back(&v);
                }
                // bottom edge
                if (v.color == glm::vec4(0.5f, 0.5f, 0.5f, 1.0f))
                {
                    chunks.at(chunkidx).bottomEdge.push_back(&v);
                }

                // corner cases
                if (v.color == glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)) // top left
                {
                    chunks.at(chunkidx).tl = &v;
                }
                if (v.color == glm::vec4(0.0f, 1.0f, 1.0f, 1.0f)) // top right
                {
                    chunks.at(chunkidx).tr = &v;
                }
                if (v.color == glm::vec4(1.0f, 0.5f, 0.5f, 1.0f)) // bottom left
                {
                    chunks.at(chunkidx).bl = &v;
                }
                if (v.color == glm::vec4(0.5f, 0.5f, 1.0f, 1.0f)) // bottom right
                {
                    chunks.at(chunkidx).br = &v;
                }
            }
            chunkidx++;
        }

        std::vector<Vertex> stitches;

        { // Stitch all the edges of chunks & reset colors
            std::cout << "Generating stitches to reconnect chunks\n";
            cleanAndStitch(chunks, stitches);
            chunks.clear(); // release mem

            // append to splitvals after all stitches generated; otherwise we
            // invalidate our ptrs
            splitValues.at(0).insert(splitValues.at(0).end(), stitches.begin(), stitches.end());
            stitches.clear(); // release mem

            // reset colors for all verts
            std::for_each(splitValues.begin(), splitValues.end(), [](std::vector<Vertex> &values) {
                std::for_each(values.begin(), values.end(), [](Vertex &vertex) {
                    vertex.color = {1.0f, 1.0f, 1.0f, 1.0f};
                });
            });
        }

        size_t tc = 0;
        for (const auto &v : splitValues)
        {
            tc += v.size();
        }

        std::mutex mtx_optimizedMesh;
        std::vector<std::pair<std::vector<Vertex>, std::vector<uint32_t>>> optimizedMesh(splitValues.size());

        idx = 0;
        // optimize chunks
        // store in optimizedMesh
        // write optimized data to respective .bin files
        std::cout << "Optimizing vertex data :: Started with " << tc << " vertices non-indexed\n";
        std::for_each(threads.begin(), threads.end(),
                      [&mtx_splitValues, &mtx_optimizedMesh, &idx, &optimizedMesh, &splitValues, p_Filename,
                       this](std::thread &thread) { // for each loop
                          thread = std::thread([&mtx_splitValues, &mtx_optimizedMesh, &splitValues, &optimizedMesh,
                                                p_Filename, idx, this]() {
                              // tmp container for vertices
                              std::vector<Vertex> cpy;
                              std::pair<std::vector<Vertex>, std::vector<uint32_t>> pair;
                              { // copy vertices
                                  std::lock_guard<std::mutex> lock{mtx_splitValues};
                                  cpy = splitValues.at(idx);
                              }
                              { // optimize
                                  pair = optimize(cpy);
                                  cpy.clear(); // release cpy
                                  // write files
                                  auto baseFilename = getBaseFilename(p_Filename);
                                  writeVertexFile(baseFilename + std::to_string(idx), pair.first);
                                  writeIndexFile(baseFilename + std::to_string(idx), pair.second);
                              }
                              { // place in optimizedMesh
                                  std::lock_guard<std::mutex> lock{mtx_optimizedMesh};
                                  optimizedMesh.at(idx) = pair;
                              }
                          });
                          idx++;
                      });

        std::for_each(threads.begin(), threads.end(), joinThread);

        // free memory from old mesh data
        for (auto &p : splitValues)
        {
            p.clear();
        }
        splitValues.clear();

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // concat vertices
        std::cout << "Rebinding chunks\n";
        for (auto &p : optimizedMesh)
        {
            // vertexOffsets.push_back (vertices.size ());
            // indexOffsets.push_back ({ indices.size (), p.second.size () });

            // increment each indice by indices.size() before copy to apply its
            // offset
            std::for_each(p.second.begin(), p.second.end(), [&](uint32_t &index) { index += vertices.size(); });
            // copy vertices
            std::copy(p.first.begin(), p.first.end(), std::back_inserter(vertices));
            // copy indices
            std::copy(p.second.begin(), p.second.end(), std::back_inserter(indices));
            // release memory
            p.first.clear();
            p.second.clear();
        }

        // align indices for each submesh (indice += start offset)
        // { index start, index count }

        vertexCount = vertices.size();
        indexCount = indices.size();
        std::cout << "Done\nVertex count : " << vertexCount << " Index Count : " << indexCount << "\n";
        allocate(vertices, indices);

        isMapLoaded = true;
        filename = p_Filename;

        return {vertices, indices};
    }
    catch (std::filesystem::filesystem_error &e)
    {
        throw std::runtime_error("Failed to open heightmap " + p_Filename + " => " + e.what());
    }
    catch (std::exception &e)
    {
        throw std::runtime_error("std error => " + std::string(e.what()));
    }
}

std::pair<std::vector<Vertex>, std::vector<uint32_t>> MapHandler::optimize(std::vector<Vertex> &p_Vertices)
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
        if (!uniques.empty())
        {
            if (!start)
            {
                if (uniques.size() % 5000 == 0)
                {
                    startIndex += 5000;
                }
            }
            else
            {
                start = false;
            }

            for (size_t i = startIndex; i < uniques.size(); i++)
            {
                if (uniques.at(i).position == unoptimized.position)
                {
                    isUnique = false;
                    indices.push_back(i);
                }
            }
        }
        if (isUnique)
        {
            uniques.push_back(unoptimized);
            indices.push_back(uniques.size() - 1);
        }
    }

    return {uniques, indices};
}

void MapHandler::bindBuffer(vk::CommandBuffer &p_CommandBuffer)
{
    // sanity check
    if (!vertexBuffer || !vertexMemory)
        throw std::runtime_error("Attempted to bind buffer without creating them :: map handler");
    p_CommandBuffer.bindVertexBuffers(0, *vertexBuffer, vk::DeviceSize{0});
    p_CommandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);
    return;
}

bool MapHandler::isAlreadyOptimized(std::string p_OriginalFilenameRequested)
{
    std::string base = getBaseFilename(p_OriginalFilenameRequested);
    std::cout << "Checking for base name : " << base << "\n";

    bool good = true;
    for (size_t idx = 0; idx < 4; idx++)
    {
        try
        {
            auto vfile = base + std::to_string(idx) + "_v.bin";
            auto ifile = base + std::to_string(idx) + "_i.bin";
            if (!std::filesystem::exists(vfile))
            {
                std::cout << "Failed to find vertex file " + vfile + "\n";
                good = false;
            }
            if (!std::filesystem::exists(ifile))
            {
                std::cout << "Failed to find index file " + ifile + "\n";
                good = false;
            }
        }
        catch (std::filesystem::filesystem_error &e)
        {
            throw std::runtime_error("Error occurred while checking for existing meshes " + std::string(e.what()));
        }
    }

    return good;
}

std::string MapHandler::getBaseFilename(const std::string &p_Filename)
{
    return p_Filename.substr(0, p_Filename.find("."));
}

std::string MapHandler::filenameToBinV(const std::string &p_Filename)
{
    return p_Filename.substr(0, p_Filename.find(".")) + "_v.bin";
}

std::string MapHandler::filenameToBinI(const std::string &p_Filename)
{
    return p_Filename.substr(0, p_Filename.find(".")) + "_i.bin";
}

void MapHandler::writeVertexFile(std::string p_FilenameIndexAppended, std::vector<Vertex> &p_VertexContainer)
{
    std::ofstream file(filenameToBinV(p_FilenameIndexAppended), std::ios::trunc);
    std::ostream_iterator<Vertex> vertexOutputIterator(file);

    file << "v " << p_VertexContainer.size() << "\n";

    std::copy(p_VertexContainer.begin(), p_VertexContainer.end(), vertexOutputIterator);

    file.close();
    return;
}

void MapHandler::writeIndexFile(std::string p_FilenameIndexAppended, std::vector<uint32_t> &p_IndexContainer)
{
    std::ofstream file(filenameToBinI(p_FilenameIndexAppended), std::ios::trunc);
    std::ostream_iterator<uint32_t> indexOutputIterator(file, "\n");

    file << "i " << p_IndexContainer.size() << "\n";

    std::copy(p_IndexContainer.begin(), p_IndexContainer.end(), indexOutputIterator);

    file.close();
    return;
}

void MapHandler::writeRaw(std::string p_FilenameIndexAppended, std::vector<Vertex> &p_VertexContainer,
                          std::vector<uint32_t> &p_IndexContainer)
{
    std::cout << "All output operations complete\n";
    return;
}

void MapHandler::readVertexFile(std::string p_FinalName, std::vector<Vertex> &p_VertexContainer)
{
    std::ifstream file(p_FinalName);
    if (!file.is_open())
        throw std::runtime_error("Failed to open pre optimized file");

    // Read vertices count
    std::string strVertexCount;

    std::getline(file, strVertexCount);
    size_t vColon = strVertexCount.find(' ');
    if (vColon == std::string::npos)
        throw std::runtime_error("Could not determine vertex count from map file");

    vColon++;
    size_t vertexCount = 0;
    try
    {
        vertexCount = std::stol(strVertexCount.substr(vColon));
    }
    catch (std::invalid_argument &e)
    {
        throw std::runtime_error("Invalid vertex count in map file, received => " + std::string(e.what()));
    }
    if (!vertexCount)
        throw std::runtime_error("Invalid vertex count in map file, = 0");

    p_VertexContainer.reserve(vertexCount);

    std::vector<Vertex>::iterator iterator = p_VertexContainer.begin();
    std::insert_iterator<std::vector<Vertex>> vertexInserter(p_VertexContainer, iterator);

    std::copy(std::istream_iterator<Vertex>(file), std::istream_iterator<Vertex>(), vertexInserter);

    // std::copy(std::istream_iterator<Vertex>(file),
    //           std::istream_iterator<Vertex>(),
    //           std::back_inserter(p_VertexContainer));

    file.close();
    return;
}

void MapHandler::readIndexFile(std::string p_FinalFilename, std::vector<uint32_t> &p_IndexContainer)
{
    std::ifstream file(p_FinalFilename);
    if (!file.is_open())
        throw std::runtime_error("Failed to open pre optimized file");

    std::string strIndexCount;
    std::getline(file, strIndexCount);
    size_t iColon = strIndexCount.find(' ');
    if (iColon == std::string::npos)
        throw std::runtime_error("Could not determine index count");

    iColon++;
    size_t indexCount = 0;
    try
    {
        indexCount = std::stol(strIndexCount.substr(iColon));
    }
    catch (std::invalid_argument &e)
    {
        throw std::runtime_error("Invalid index count in map file, received => " + std::string(e.what()));
    }
    if (!indexCount)
        throw std::runtime_error("Invalid index count in map file, = 0");

    p_IndexContainer.reserve(indexCount);

    std::vector<uint32_t>::iterator iterator = p_IndexContainer.begin();
    std::insert_iterator<std::vector<uint32_t>> indexInserter(p_IndexContainer, iterator);

    std::copy(std::istream_iterator<uint32_t>(file), std::istream_iterator<uint32_t>(), indexInserter);

    // std::copy(std::istream_iterator<uint32_t>(file),
    //           std::istream_iterator<uint32_t>(),
    //           std::back_inserter(p_IndexContainer));

    file.close();
    return;
}

void MapHandler::allocate(std::vector<Vertex> &p_VertexContainer, std::vector<uint32_t> &p_IndexContainer)
{
    {
        // If a map is already loaded, release preallocated vulkan memory
        if (isMapLoaded)
        {
            ptrEngine->logicalDevice.waitIdle();
            std::cout << "Map already loaded so cleaning up\n";
            if (vertexBuffer)
            {
                ptrEngine->logicalDevice.destroyBuffer(*vertexBuffer);
                vertexBuffer.reset();
            }
            if (vertexMemory)
            {
                ptrEngine->logicalDevice.freeMemory(*vertexMemory);
                vertexMemory.reset();
            }
            if (indexBuffer)
            {
                ptrEngine->logicalDevice.destroyBuffer(*indexBuffer);
                indexBuffer.reset();
            }
            if (indexMemory)
            {
                ptrEngine->logicalDevice.freeMemory(*indexMemory);
                indexMemory.reset();
            }
        }

        // ensure vk objects allocated
        if (!vertexBuffer)
            vertexBuffer = std::make_unique<vk::Buffer>();
        if (!vertexMemory)
            vertexMemory = std::make_unique<vk::DeviceMemory>();
        if (!indexBuffer)
            indexBuffer = std::make_unique<vk::Buffer>();
        if (!indexMemory)
            indexMemory = std::make_unique<vk::DeviceMemory>();
    }

    {
        std::cout << "Creating buffers for map mesh\n";
        std::cout << "Vertices count : " << p_VertexContainer.size() << "\n";
        std::cout << "Indices count : " << p_IndexContainer.size() << "\n";
        // Create vulkan resources for our map
        using enum vk::MemoryPropertyFlagBits;

        // Create vertex buffer
        ptrEngine->createBuffer(vk::BufferUsageFlagBits::eVertexBuffer, eHostCoherent | eHostVisible,
                                p_VertexContainer.size() * sizeof(Vertex), vertexBuffer.get(), vertexMemory.get(),
                                p_VertexContainer.data());

        // Create index buffer
        ptrEngine->createBuffer(vk::BufferUsageFlagBits::eIndexBuffer, eHostCoherent | eHostVisible,
                                p_IndexContainer.size() * sizeof(uint32_t), indexBuffer.get(), indexMemory.get(),
                                p_IndexContainer.data());
    }
    return;
}

std::vector<Vertex> MapHandler::generateMesh(size_t p_XLength, size_t p_ZLength, std::vector<Vertex> &p_HeightValues)
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

            Vertex topLeft = p_HeightValues.at(topLeftIndex);
            Vertex topRight = p_HeightValues.at(topRightIndex);
            Vertex bottomLeft = p_HeightValues.at(bottomLeftIndex);
            Vertex bottomRight = p_HeightValues.at(bottomRightIndex);

            topLeft.uv = {
                topLeft.position.x,
                topLeft.position.z,
                p_XLength,
                p_ZLength,
            };
            topRight.uv = {
                topRight.position.x,
                topRight.position.z,
                p_XLength,
                p_ZLength,
            };
            bottomLeft.uv = {
                bottomLeft.position.x,
                bottomLeft.position.z,
                p_XLength,
                p_ZLength,
            };
            bottomRight.uv = {
                bottomRight.position.x,
                bottomRight.position.z,
                p_XLength,
                p_ZLength,
            };

            vertices.push_back(topLeft);
            vertices.push_back(bottomLeft);
            vertices.push_back(bottomRight);

            vertices.push_back(bottomRight);
            vertices.push_back(topRight);
            vertices.push_back(topLeft);
        }
    }
    return vertices;
}

void MapHandler::cleanEdge(std::vector<Vertex *> &p_Edge, bool isHorizontal)
{
    std::vector<Vertex *> optimized;
    if (isHorizontal)
    {
        // prune x axis
        for (auto &vptr : p_Edge)
        {
            bool unique = true;
            // search for x:z val
            if (!optimized.empty())
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
                optimized.push_back(vptr);
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
            if (!optimized.empty())
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
                optimized.push_back(vptr);
            }
        }
    }
    p_Edge.clear();
    p_Edge = optimized;
    return;
}

// must be same size
// stitches edges together with param 1 being origin
void MapHandler::stitchEdge(std::vector<Vertex *> &p_FirstEdge, std::vector<Vertex *> &p_SecondEdge,
                            std::vector<Vertex> &p_Vertices, bool p_IsVertical)
{
    // sanity check
    if (p_FirstEdge.size() != p_SecondEdge.size())
        throw std::runtime_error("Cannot stich edges of uneven lengths");

    if (p_IsVertical)
    {
        for (size_t i = 0; i < (p_FirstEdge.size() - 1); i++)
        {
            // grab verts for quad
            auto tl = p_FirstEdge.at(i);
            auto bl = p_FirstEdge.at(i + 1);
            auto br = p_SecondEdge.at(i + 1);
            auto tr = p_SecondEdge.at(i);

            tl->color = {1.0f, 0.0f, 0.0f, 1.0f};
            bl->color = {0.0f, 1.0f, 0.0f, 1.0f};
            br->color = {0.0f, 1.0f, 1.0f, 1.0f};
            tr->color = {0.0f, 1.0f, 1.0f, 1.0f};

            // push counter clockwise
            p_Vertices.push_back(*tl);
            p_Vertices.push_back(*bl);
            p_Vertices.push_back(*br);

            p_Vertices.push_back(*br);
            p_Vertices.push_back(*tr);
            p_Vertices.push_back(*tl);
        }
    }
    else
    {
        for (size_t i = 0; i < (p_FirstEdge.size() - 1); i++)
        {
            // grab verts for quad
            auto tl = p_FirstEdge.at(i);
            auto bl = p_SecondEdge.at(i);
            auto br = p_SecondEdge.at(i + 1);
            auto tr = p_FirstEdge.at(i + 1);

            // push counter clockwise
            p_Vertices.push_back(*tl);
            p_Vertices.push_back(*bl);
            p_Vertices.push_back(*br);

            p_Vertices.push_back(*br);
            p_Vertices.push_back(*tr);
            p_Vertices.push_back(*tl);
        }
    }
    return;
}

void MapHandler::stitchCorner(Vertex *p_FirstCorner, Vertex *p_FirstEdgeVertex, Vertex *p_SecondCorner,
                              Vertex *p_SecondEdgeVertex, std::vector<Vertex> &p_Vertices, WindingStyle p_Style)
{
    using enum WindingStyle;
    if (p_Style == eFirstCornerToFirstEdge)
    {
        auto tl = p_FirstCorner;
        auto bl = p_FirstEdgeVertex;
        auto br = p_SecondEdgeVertex;
        auto tr = p_SecondCorner;

        p_Vertices.push_back(*tl);
        p_Vertices.push_back(*bl);
        p_Vertices.push_back(*br);

        p_Vertices.push_back(*br);
        p_Vertices.push_back(*tr);
        p_Vertices.push_back(*tl);
    }
    else if (p_Style == eFirstCornerToSecondCorner)
    {
        auto tl = p_FirstCorner;
        auto bl = p_SecondCorner;
        auto br = p_SecondEdgeVertex;
        auto tr = p_FirstEdgeVertex;

        p_Vertices.push_back(*tl);
        p_Vertices.push_back(*bl);
        p_Vertices.push_back(*br);

        p_Vertices.push_back(*br);
        p_Vertices.push_back(*tr);
        p_Vertices.push_back(*tl);
    }
    else if (p_Style == eFirstEdgeToFirstCorner)
    {
        auto tl = p_FirstEdgeVertex;
        auto bl = p_FirstCorner;
        auto br = p_SecondCorner;
        auto tr = p_SecondEdgeVertex;

        p_Vertices.push_back(*tl);
        p_Vertices.push_back(*bl);
        p_Vertices.push_back(*br);

        p_Vertices.push_back(*br);
        p_Vertices.push_back(*tr);
        p_Vertices.push_back(*tl);
    }
    else if (p_Style == eFirstEdgeToSecondEdge)
    {
        auto tl = p_FirstEdgeVertex;
        auto bl = p_SecondEdgeVertex;
        auto br = p_SecondCorner;
        auto tr = p_FirstCorner;

        p_Vertices.push_back(*tl);
        p_Vertices.push_back(*bl);
        p_Vertices.push_back(*br);

        p_Vertices.push_back(*br);
        p_Vertices.push_back(*tr);
        p_Vertices.push_back(*tl);
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

        p_Vertices.push_back(*tl);
        p_Vertices.push_back(*bl);
        p_Vertices.push_back(*br);

        p_Vertices.push_back(*br);
        p_Vertices.push_back(*tr);
        p_Vertices.push_back(*tl);
    }
    return;
}

void MapHandler::cleanAndStitch(std::vector<ChunkData> &p_Chunks, std::vector<Vertex> &p_Stitches)
{
    // clean edge before stitching
    cleanEdge(p_Chunks.at(0).rightEdge, false);
    cleanEdge(p_Chunks.at(1).leftEdge, false);
    // stitch tl -> tr
    stitchEdge(p_Chunks.at(0).rightEdge, p_Chunks.at(1).leftEdge, p_Stitches, true);

    // prune dups
    cleanEdge(p_Chunks.at(2).rightEdge, false);
    cleanEdge(p_Chunks.at(3).leftEdge, false);
    // stitch bl -> br
    stitchEdge(p_Chunks.at(2).rightEdge, p_Chunks.at(3).leftEdge, p_Stitches, true);

    // prune dups
    cleanEdge(p_Chunks.at(0).bottomEdge, true);
    cleanEdge(p_Chunks.at(2).topEdge, true);
    // stich tl -> bl
    stitchEdge(p_Chunks.at(0).bottomEdge, p_Chunks.at(2).topEdge, p_Stitches, false);

    // prune dups
    cleanEdge(p_Chunks.at(1).bottomEdge, true);
    cleanEdge(p_Chunks.at(3).topEdge, true);
    // stitch tr -> br
    stitchEdge(p_Chunks.at(1).bottomEdge, p_Chunks.at(3).topEdge, p_Stitches, false);

    // stitch tl -> tr corner
    stitchCorner(p_Chunks.at(0).tr, p_Chunks.at(0).rightEdge.at(0), p_Chunks.at(1).tl, p_Chunks.at(1).leftEdge.at(0),
                 p_Stitches, eFirstCornerToFirstEdge);

    // stitch tl -> bl corner
    stitchCorner(p_Chunks.at(0).bl, p_Chunks.at(0).bottomEdge.at(0), p_Chunks.at(2).tl, p_Chunks.at(2).topEdge.at(0),
                 p_Stitches, eFirstCornerToSecondCorner);

    // stitch bl -> br corner
    stitchCorner(p_Chunks.at(2).br, p_Chunks.at(2).rightEdge.at(p_Chunks.at(2).rightEdge.size() - 1), p_Chunks.at(3).bl,
                 p_Chunks.at(3).leftEdge.at(p_Chunks.at(3).leftEdge.size() - 1), p_Stitches, eFirstEdgeToFirstCorner);

    // stitch tr -> br corner
    stitchCorner(p_Chunks.at(1).br, p_Chunks.at(1).bottomEdge.at(p_Chunks.at(1).bottomEdge.size() - 1),
                 p_Chunks.at(3).tr, p_Chunks.at(3).topEdge.at(p_Chunks.at(3).topEdge.size() - 1), p_Stitches,
                 eFirstEdgeToSecondEdge);

    // stitch tl -> bl RIGHT corner
    stitchCorner(p_Chunks.at(0).br, p_Chunks.at(0).bottomEdge.at(p_Chunks.at(0).bottomEdge.size() - 1),
                 p_Chunks.at(2).tr, p_Chunks.at(2).topEdge.at(p_Chunks.at(2).topEdge.size() - 1), p_Stitches,
                 eFirstEdgeToSecondEdge);

    // stitch tr -> br LEFT CORNER
    stitchCorner(p_Chunks.at(1).bl, p_Chunks.at(1).bottomEdge.at(0), p_Chunks.at(3).tl, p_Chunks.at(3).topEdge.at(0),
                 p_Stitches, eFirstCornerToSecondCorner);
    // stitch bl -> br RIGHT CORNER
    stitchCorner(p_Chunks.at(2).tr, p_Chunks.at(2).rightEdge.at(0), p_Chunks.at(3).tl, p_Chunks.at(3).leftEdge.at(0),
                 p_Stitches, eFirstCornerToFirstEdge);

    // stitch tl -> tr
    stitchCorner(p_Chunks.at(0).br, p_Chunks.at(0).rightEdge.at(p_Chunks.at(0).rightEdge.size() - 1), p_Chunks.at(1).bl,
                 p_Chunks.at(1).leftEdge.at(p_Chunks.at(1).leftEdge.size() - 1), p_Stitches, eFirstEdgeToFirstCorner);

    // stitch center corners - provide params left->right order
    stitchCorner(p_Chunks.at(0).br, p_Chunks.at(1).bl, p_Chunks.at(2).tr, p_Chunks.at(3).tl, p_Stitches, eCenter);
    return;
}

void MapHandler::loadPreoptimized(std::string p_Filename, std::vector<Vertex> &p_Vertices,
                                  std::vector<uint32_t> &p_Indices)
{
    std::mutex mtx_chunkContainer;
    std::vector<std::pair<std::vector<Vertex>, std::vector<uint32_t>>> chunks(4);

    constexpr size_t numThreads = 4;

    std::vector<std::thread> threads(numThreads);

    std::cout << "Created " << threads.size() << " threads awaiting jobs\n ";

    auto joinThread = [](std::thread &thread) { thread.join(); };

    size_t idx = 0;
    std::for_each(threads.begin(), threads.end(),
                  [&mtx_chunkContainer, &chunks, &idx, p_Filename, this](std::thread &thread) {
                      thread = std::thread([&mtx_chunkContainer, &chunks, idx, p_Filename, this]() {
                          std::vector<Vertex> verts;
                          auto name = getBaseFilename(p_Filename) + std::to_string(idx) + "_v.bin";

                          readVertexFile(name, verts);

                          std::unique_lock<std::mutex> lock{mtx_chunkContainer};
                          chunks.at(idx).first = verts;
                      });
                      idx++;
                  });

    std::for_each(threads.begin(), threads.end(), joinThread);

    std::cout << "Done reading vertex files\n";

    // load indices
    idx = 0;
    std::for_each(threads.begin(), threads.end(),
                  [&mtx_chunkContainer, &chunks, &idx, p_Filename, this](std::thread &thread) {
                      thread = std::thread([&mtx_chunkContainer, &chunks, idx, p_Filename, this]() {
                          std::vector<uint32_t> inds;
                          auto name = getBaseFilename(p_Filename) + std::to_string(idx) + "_i.bin";
                          // read file
                          readIndexFile(name, inds);
                          std::unique_lock<std::mutex> lock{mtx_chunkContainer};
                          chunks.at(idx).second = inds;
                      });
                      idx++;
                  });

    std::for_each(threads.begin(), threads.end(), joinThread);

    std::cout << "Done reading index files\n";

    // ensure no empty chunks
    auto allLoaded =
        std::all_of(chunks.begin(), chunks.end(), [](std::pair<std::vector<Vertex>, std::vector<uint32_t>> &p) {
            if (p.first.empty() || p.second.empty())
                return false;

            return true;
        });

    if (!allLoaded)
        std::cout << "One or more chunk vertices failed to load\n";

    // concat all data
    for (auto &chunkpair : chunks)
    {
        // increment each indice by indices.size() before copy to apply
        // its offset
        std::for_each(chunkpair.second.begin(), chunkpair.second.end(),
                      [&p_Vertices](uint32_t &index) { index += p_Vertices.size(); });

        // copy vertices
        std::copy(chunkpair.first.begin(), chunkpair.first.end(), std::back_inserter(p_Vertices));

        // copy indices
        std::copy(chunkpair.second.begin(), chunkpair.second.end(), std::back_inserter(p_Indices));
    }
    return;
}

void MapHandler::loadPNG(size_t &p_XLengthRet, size_t &p_ZLengthRet, size_t &p_ChannelsRet, std::string &p_Filename,
                         std::vector<unsigned char> &p_ImageDataRet)
{
    int tmpX = 0;
    int tmpZ = 0;
    int tmpChannels = 0;
    stbi_uc *stbiRawImage = stbi_load(p_Filename.c_str(), &tmpX, &tmpZ, &tmpChannels, STBI_rgb_alpha);

    if (!stbiRawImage)
        throw std::runtime_error("STBI failed to open file => " + p_Filename);

    // validate return values
    if (tmpX <= 0 || tmpZ <= 0 || tmpChannels <= 0)
    {
        if (stbiRawImage)
            stbi_image_free(stbiRawImage);
        throw std::runtime_error("STBI returned invalid values :: width, "
                                 "height or color channels is < 0");
    }

    if (p_XLengthRet % 2 != 0 || p_XLengthRet != p_ZLengthRet)
    {
        if (stbiRawImage)
            stbi_image_free(stbiRawImage);
        throw std::runtime_error("Maps must be even squares where each side is divisible by 2");
    }

    p_XLengthRet = static_cast<size_t>(tmpX);
    p_ZLengthRet = static_cast<size_t>(tmpZ);
    p_ChannelsRet = static_cast<size_t>(tmpChannels);

    // Place into vector
    p_ImageDataRet = std::vector<unsigned char>(stbiRawImage, stbiRawImage + (p_XLengthRet * p_ZLengthRet * 4));

    // cleanup raw c interface buf
    stbi_image_free(stbiRawImage);
    return;
}

void MapHandler::pngToVertices(size_t p_ImageLength, std::vector<unsigned char> &p_RawImage,
                               std::vector<Vertex> &p_VertexContainer)
{
    int offset = 0;
    size_t splitWidth = p_ImageLength / 2;
    for (size_t j = 0; j < p_ImageLength; j++)
    {
        for (size_t i = 0; i < p_ImageLength; i++)
        {
            glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

            // if left edge
            if (i == 0 || i == splitWidth)
            {
                if (j == 0 || j == splitWidth) // if top left
                {
                    color = {1.0f, 1.0f, 0.0f, 1.0f};
                }
                else if (j == splitWidth - 1 || j == p_ImageLength - 1) // if bottom left
                {
                    color = {1.0f, 0.5f, 0.5f, 1.0f};
                }
                else
                {
                    color = {1.0f, 0.0f, 0.0f, 1.0f};
                }
            }
            // if top edge
            if (j == 0 || j == splitWidth)
            {
                if (i == 0 || i == splitWidth) // top left
                {
                    color = {1.0f, 1.0f, 0.0f, 1.0f};
                }
                else if (i == splitWidth - 1 || i == p_ImageLength - 1) // top right
                {
                    color = {0.0f, 1.0f, 1.0f, 1.0f};
                }
                else
                {
                    color = {0.0f, 1.0f, 0.0f, 1.0f};
                }
            }
            // right edge
            if (i == splitWidth - 1 || i == p_ImageLength - 1)
            {
                if (j == 0 || j == splitWidth) // top right
                {
                    color = {0.0f, 1.0f, 1.0f, 1.0f};
                }
                else if (j == splitWidth - 1 || j == p_ImageLength - 1) // bottom right
                {
                    color = {0.5f, 0.5f, 1.0f, 1.0f};
                }
                else
                {
                    color = {0.0f, 0.0f, 1.0f, 1.0f};
                }
            }
            // bottom edge
            if (j == splitWidth - 1 || j == p_ImageLength - 1)
            {
                // bottom left
                if (i == 0 || i == splitWidth)
                {
                    color = {1.0f, 0.5f, 0.5f, 1.0f};
                }
                else if (i == splitWidth - 1 || i == p_ImageLength - 1) // bottom right
                {
                    color = {0.5f, 0.5f, 1.0f, 1.0f};
                }
                else
                {
                    color = {0.5f, 0.5f, 0.5f, 1.0f};
                }
            }
            p_VertexContainer.push_back(Vertex{
                {
                    static_cast<float>(i),
                    static_cast<float>(p_RawImage.at(offset)) * -1.0f,
                    static_cast<float>(j),
                    1.0f,
                },
                color,
                {0.0f, 0.0f, 0.0f, 0.0f},
            });
            offset += 4;
        }
    }
    return;
}