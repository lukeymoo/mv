#define STBI_IMAGE_IMPLEMENTATION
#include "mvMap.h"

// For struct Vertex definition
#include "mvModel.h"

// Image loading/decoding methods
#include "stb_image.h"

// For string tokenizing
#include "mvHelper.h"

MapHandler::MapHandler()
{
    return;
}

MapHandler::~MapHandler()
{
    return;
}

void MapHandler::cleanup(const vk::Device& p_LogicalDevice)
{
    if(vertexBuffer)
        p_LogicalDevice.destroyBuffer(vertexBuffer);
    
    if(vertexMemory)
        p_LogicalDevice.freeMemory(vertexMemory);
    
    if(indexBuffer)
        p_LogicalDevice.destroyBuffer(indexBuffer);
    
    if(indexMemory)
        p_LogicalDevice.freeMemory(indexMemory);
    return;
}

std::pair<std::vector<Vertex>, std::vector<uint32_t>> MapHandler::readHeightMap(std::string p_Filename)
{
    // Look for already optimized filename
    if(isAlreadyOptimized(p_Filename))
    {
        // Read vertex file
        std::vector<Vertex> vertices;
        readVertexFile(p_Filename, vertices);
        std::cout << "Exited function, read => " << vertices.size() << " elements\n";

        // Read indices file
        std::vector<uint32_t> indices;
        readIndexFile(p_Filename, indices);
        std::cout << "Exited index function, read => " << indices.size() << " elements\n";

        // Ensure we setup sizes
        vertexCount = vertices.size();
        indexCount = indices.size();
        return {vertices, indices};
    }

    // Pre optimized file does not exist, load raw & generate optimizations

    try
    {
        // Check if file exists
        if (!std::filesystem::exists(p_Filename))
            throw std::runtime_error("File " + p_Filename + " does not exist");

        // Read the image in
        int tWidth = 0;
        int tHeight = 0;
        int tChannels = 0;
        stbi_uc *stbiRawImage =
            stbi_load(p_Filename.c_str(), &tWidth, &tHeight, &tChannels, STBI_rgb_alpha);

        if (!stbiRawImage)
            throw std::runtime_error("STBI failed to open file => " + p_Filename);

        // validate return values
        if (tWidth < 0 || tHeight < 0 || tChannels < 0)
            throw std::runtime_error("STBI returned invalid values :: width, "
                                     "height or color channels is < 0");

        size_t width = static_cast<size_t>(tWidth);
        size_t height = static_cast<size_t>(tHeight);
        [[maybe_unused]] size_t channels = static_cast<size_t>(tChannels);

        // Place into vector
        std::vector<unsigned char> rawImage(stbiRawImage, stbiRawImage + (width * height * 4));

        // cleanup raw c interface buf
        stbi_image_free(stbiRawImage);

        // Extract height values and place into new vector
        std::vector<Vertex> heightValues;
        int offset = 0;
        for(size_t j = 0; j < height; j++)
        {
            for(size_t i = 0; i < width; i++)
            {
                heightValues.push_back(Vertex{
                    {
                        static_cast<float>(i),
                        static_cast<float>(rawImage.at(offset)) * -1.0f,
                        static_cast<float>(j),
                        1.0f
                    },
                    {1.0f, 1.0f, 1.0f, 1.0f},
                    {0.0f, 0.0f, 0.0f, 0.0f}
                });
                offset += 4;
            }
        }

        std::cout << "Extracted " << std::to_string(heightValues.size()) << " height values\n";

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Generates terribly bloated & inefficient mesh data
        // Iterate image height
        for (size_t j = 0; j < (height - 1); j++)
        {
            // Iterate image width
            for (size_t i = 0; i < (width - 1); i++)
            {
                size_t topLeftIndex = (height * j) + i;
                size_t topRightIndex = (height * j) + (i + 1);
                size_t bottomLeftIndex = (height * (j + 1)) + i;
                size_t bottomRightIndex = (height * (j + 1)) + (i + 1);

                Vertex topLeft = heightValues.at(topLeftIndex);
                Vertex topRight = heightValues.at(topRightIndex);
                Vertex bottomLeft = heightValues.at(bottomLeftIndex);
                Vertex bottomRight = heightValues.at(bottomRightIndex);
                

                vertices.push_back(topLeft);
                vertices.push_back(bottomLeft);
                vertices.push_back(bottomRight);

                vertices.push_back(topLeft);
                vertices.push_back(bottomRight);
                vertices.push_back(topRight);
            }
        }

        // Cleans up the junk mesh we created
        optimize(vertices, indices);
        // Output to file
        writeRaw(p_Filename, vertices, indices);

        vertexCount = vertices.size();
        indexCount = indices.size();
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

void MapHandler::optimize(std::vector<Vertex>& p_Vertices, std::vector<uint32_t>& p_Indices)
{
    std::vector<Vertex> uniques;
    std::vector<uint32_t> indices;

    std::cout << "[+] Preparing to optimize " << p_Vertices.size() << " vertices\n";

    // Iterate unsorted vertices
    bool start = true;
    size_t startIndex = 0;
    for(const auto& unoptimized : p_Vertices)
    {
        bool isUnique = true;
        // Get start index for last 10k vertices
        if(!uniques.empty())
        {
            if(!start)
            {
                if(uniques.size() % 5000 == 0)
                {
                    startIndex += 5000;
                }
            }
            else
            {
                start = false;
            }
            
            for(size_t i = startIndex; i < uniques.size(); i++)
            {
                if(uniques.at(i).position == unoptimized.position)
                {
                    isUnique = false;
                    indices.push_back(i);
                }
            }
        }
        if(isUnique)
        {
            uniques.push_back(unoptimized);
            indices.push_back(uniques.size() - 1);
        }
    }
    
    std::cout << "Optimized => " << uniques.size() << "\n";
    std::cout << "Indices => " << indices.size() << "\n";

    p_Vertices = uniques;
    p_Indices = indices;
    return;
}

void MapHandler::bindBuffer(vk::CommandBuffer& p_CommandBuffer)
{
    p_CommandBuffer.bindVertexBuffers(0, vertexBuffer, vk::DeviceSize{0});
    p_CommandBuffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);
    return;
}

bool MapHandler::isAlreadyOptimized(std::string p_OriginalFilenameRequested)
{
    std::string nameV = filenameToBinV(p_OriginalFilenameRequested);
    std::string nameI = filenameToBinI(p_OriginalFilenameRequested);
    try
    {
        return (std::filesystem::exists(nameV) && std::filesystem::exists(nameI));
    }
    catch(std::filesystem::filesystem_error& e)
    {
        throw std::runtime_error("Filesystem error : " + std::string(e.what()));
    }
}

std::string MapHandler::filenameToBinV(std::string& p_Filename)
{
    return p_Filename.substr(0, p_Filename.find(".")) + "_v.bin";
}

std::string MapHandler::filenameToBinI(std::string& p_Filename)
{
    return p_Filename.substr(0, p_Filename.find(".")) + "_i.bin";
}

void MapHandler::writeRaw(std::string p_OriginalFilenameRequested,
                          std::vector<Vertex>& p_VertexContainer,
                          std::vector<uint32_t>& p_IndexContainer)
{
    {
        std::ofstream file(filenameToBinV(p_OriginalFilenameRequested), std::ios::trunc);
        std::ostream_iterator<Vertex> vertexOutputIterator(file);

        std::cout << "Outputting vertices to "
            << filenameToBinV(p_OriginalFilenameRequested) << "\n";

        file << "v " << p_VertexContainer.size() << "\n";
        
        std::copy(p_VertexContainer.begin(),
                  p_VertexContainer.end(),
                  vertexOutputIterator);
        
        std::cout << "Done\n";
        
        file.close();
    }

    // Output indices to file
    {
        std::ofstream file(filenameToBinI(p_OriginalFilenameRequested), std::ios::trunc);
        std::ostream_iterator<uint32_t> indexOutputIterator(file, "\n");

        std::cout << "Outputting indices to "
            << filenameToBinI(p_OriginalFilenameRequested) << "\n";

        file << "i " << p_IndexContainer.size() << "\n";
        
        std::copy(p_IndexContainer.begin(),
                  p_IndexContainer.end(),
                  indexOutputIterator);
        
        std::cout << "Done\n";

        file.close();
    }
    return;
}

void MapHandler::readVertexFile(std::string p_OriginalFilenameRequested,
                                std::vector<Vertex>& p_VertexContainer)
{
    std::cout << "Reading vertex bin file : "
                << filenameToBinV(p_OriginalFilenameRequested) << "\n";
    std::ifstream file(filenameToBinV(p_OriginalFilenameRequested));
    if(!file.is_open())
        throw std::runtime_error("Failed to open pre optimized file");

    // Read vertices count
    std::string strVertexCount;

    std::getline(file, strVertexCount);
    size_t vColon = strVertexCount.find(' ');
    if(vColon == std::string::npos)
        throw std::runtime_error("Could not determine vertex count from map file");

    vColon++;
    size_t vertexCount = 0;
    try
    {
        vertexCount = std::stol(strVertexCount.substr(vColon));
    }
    catch (std::invalid_argument& e)
    {
        throw std::runtime_error(
            "Invalid vertex count in map file, received => " + std::string(e.what())
            );
    }
    if(!vertexCount)
        throw std::runtime_error("Invalid vertex count in map file, = 0");


    p_VertexContainer.reserve(vertexCount);

    std::cout << "Beginning read of vertex data\n";

    std::vector<Vertex>::iterator iterator = p_VertexContainer.begin();
    std::insert_iterator<std::vector<Vertex>> vertexInserter(p_VertexContainer, iterator);

    std::copy(std::istream_iterator<Vertex>(file),
              std::istream_iterator<Vertex>(),
              vertexInserter);

    // std::copy(std::istream_iterator<Vertex>(file),
    //           std::istream_iterator<Vertex>(),
    //           std::back_inserter(p_VertexContainer));
    
    file.close();
    return;
}

void MapHandler::readIndexFile(std::string p_OriginalFilenameRequested,
                               std::vector<uint32_t>& p_IndexContainer)
{
    std::cout << "Reading index bin file : "
                << filenameToBinI(p_OriginalFilenameRequested) << "\n";
    std::ifstream file(filenameToBinI(p_OriginalFilenameRequested));
    if(!file.is_open())
        throw std::runtime_error("Failed to open pre optimized file");

    std::string strIndexCount;
    std::getline(file, strIndexCount);
    size_t iColon = strIndexCount.find(' ');
    if(iColon == std::string::npos)
        throw std::runtime_error("Could not determine index count");

    iColon++;
    size_t indexCount = 0;
    try
    {
        indexCount = std::stol(strIndexCount.substr(iColon));
    }
    catch (std::invalid_argument &e)
    {
        throw std::runtime_error(
            "Invalid index count in map file, received => " + std::string(e.what())
            );
    }
    if(!indexCount)
        throw std::runtime_error("Invalid index count in map file, = 0");

    p_IndexContainer.reserve(indexCount);

    std::cout << "Beginning read of index data\n";

    std::vector<uint32_t>::iterator iterator = p_IndexContainer.begin();
    std::insert_iterator<std::vector<uint32_t>> indexInserter(p_IndexContainer, iterator);

    std::copy(std::istream_iterator<uint32_t>(file),
              std::istream_iterator<uint32_t>(),
              indexInserter);

    // std::copy(std::istream_iterator<uint32_t>(file),
    //           std::istream_iterator<uint32_t>(),
    //           std::back_inserter(p_IndexContainer));
    
    file.close();
    return;
}