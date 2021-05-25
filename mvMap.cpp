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
    try
    {
        std::string name = p_Filename.substr(0, p_Filename.find(".")) + ".bin";
        std::cout << "Optimized output filename => " << name << "\n";
        if(std::filesystem::exists(name))
        {
            std::cout << "Found optimized filename\n";
            std::ifstream file(name, std::ios::binary);
            if(!file.is_open())
                std::cout << "Failed to open optimized output file\n";
            
            file.seekg(0);

            // Read vertex size
            size_t vertexSize = 0;
            size_t indexSize = 0;

            std::string sizeStr;
            std::getline(file, sizeStr, '\n');
            // find :
            size_t pos = sizeStr.find(':');
            if(pos == std::string::npos)
                throw std::runtime_error("Failed to read vertex & index data sizes :: corrupted file");

            if(sizeStr.substr(0, pos).compare("v") == 0)
            {
                pos+=1; // skip :
                vertexSize = static_cast<uint32_t>(std::stol(sizeStr.substr(pos)));
            }
            
            if(sizeStr.substr(0, pos).compare("i") == 0)
            {
                pos+=1; // skip :
                indexSize = static_cast<uint32_t>(std::stol(sizeStr.substr(pos)));
            }
            
            sizeStr.clear();
            std::getline(file, sizeStr, '\n');

            pos = sizeStr.find(':');
            if(pos == std::string::npos)
                throw std::runtime_error("Failed to read vertex & index data sizes :: corrupted file");

            if(sizeStr.substr(0, pos).compare("v") == 0)
            {
                pos+=1; // skip :
                vertexSize = static_cast<uint32_t>(std::stol(sizeStr.substr(pos)));
            }
            
            if(sizeStr.substr(0, pos).compare("i") == 0)
            {
                pos+=1; // skip :
                indexSize = static_cast<uint32_t>(std::stol(sizeStr.substr(pos)));
            }
            
            if(vertexSize <= 0 || indexSize <= 0)
                throw std::runtime_error("Vertex or index size was specified as <= 0 in map file :: corrupted file");

            std::vector<Vertex> inVertices(vertexSize);
            std::vector<uint32_t> inIndices(indexSize);

            std::cout << "Created buffer of size " << vertexSize << " for vertex data\n";
            std::cout << "Created buffer of size " << indexSize << " for index data\n";

            std::string head;
            std::getline(file, head);
            if(head.length() <= 0)
                throw std::runtime_error("Failed to read data header from file");
            
            if(!head.compare(":start vertex"))
            {
                // Load vertices
                std::string line;
                std::regex re(R"([,\s]+)");
                bool reading = true;
                size_t index = 0;

                do
                {
                    Vertex data;

                    // Grab POSITION
                    std::getline(file, line, '\n');

                    if(!line.compare(":end"))
                        break;
                    
                    line.erase(0, 1); // erase '{'
                    line.erase(line.find('}'), 1); // erase '}'

                    std::vector<std::string> tokenized = Helper::tokenize(line, re);

                    data.position.x = std::stof(tokenized.at(0));
                    data.position.y = std::stof(tokenized.at(1));
                    data.position.z = std::stof(tokenized.at(2));
                    data.position.w = 1.0f; // ignore cause it should be 1.0f otherwise we can't render lines

                    // Grab COLOR
                    std::getline(file, line, '\n');
                    if(!line.compare(":end"))
                        break;
                    
                    line.erase(0, 1); // erase '{'
                    line.erase(line.find('}'), 1); // erase '}'

                    tokenized = Helper::tokenize(line, re);

                    data.color.r = std::stof(tokenized.at(0));
                    data.color.g = std::stof(tokenized.at(1));
                    data.color.b = std::stof(tokenized.at(2));
                    data.color.a = std::stof(tokenized.at(3));

                    // GRAB UV
                    std::getline(file, line, '\n');
                    if(!line.compare(":end"))
                        break;
                    
                    line.erase(0, 1); // erase '{'
                    line.erase(line.find('}'), 1); // erase '}'

                    tokenized = Helper::tokenize(line, re);

                    data.uv.x = std::stof(tokenized.at(0));
                    data.uv.y = std::stof(tokenized.at(1));
                    // data.uv.z = std::stof(tokenized.at(2))
                    // data.uv.w = std::stof(tokenized.at(3))
                    data.uv.z = 0.0f; // DONT CARE
                    data.uv.w = 0.0f; // DONT CARE

                    // Push to vector
                    inVertices.at(index) = std::move(data);
                    index++;

                } while (reading);
            }

            // Get next header
            std::getline(file, head);
            if(head.length() <= 0)
                throw std::runtime_error("Failed to read data header from file");
            
            if(!head.compare(":start index"))
            {
                std::string line;
                std::regex re(R"([,\s]+)");
                std::getline(file, line, '{'); line.clear(); // Read past '{'
                std::getline(file, line, '\n');

                std::vector<std::string> strIndices = Helper::tokenize(line, re);
                // Convert to 32 bit integers
                size_t index = 0;
                for(const auto& str : strIndices)
                {
                    inIndices.at(index) = static_cast<uint32_t>(std::stoul(str));
                    index++;
                }
                
            }

            if(inVertices.size() && inIndices.size())
            {
                vertexCount = inVertices.size();
                indexCount = inIndices.size();
                return {inVertices, inIndices};
            }
        }
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

        // Generate indices/Remove duplicate vertices
        optimize(name, vertices, indices);
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

void MapHandler::optimize(const std::string p_OptimizedFilename, std::vector<Vertex>& p_Vertices, std::vector<uint32_t>& p_Indices)
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

    // Output to file
    writeRaw(p_OptimizedFilename, p_Vertices, p_Indices);
    return;
}

void MapHandler::writeRaw(const std::string p_OptimizedFilename, std::vector<Vertex>& p_Vertices, std::vector<uint32_t>& p_Indices)
{
    // Sanity check
    if(p_Vertices.empty() || p_Indices.empty())
        throw std::runtime_error("Vertex or index data containers are empty; No mesh data to output to file");

    std::ofstream file(p_OptimizedFilename, std::ios::binary | std::ios::trunc);

    if(!file.is_open())
        throw std::runtime_error("Failed to open file to output optimized mesh data");
    

    // Output vertices vector
    /*
        Start symbol is :start [data type] || Ex: :start vertex or :start index

        End symbol is :end || Ends last :start symbol; If another :start is
        found before finding an :end the operation will be aborted

        Specifying sizes avoids 28 million copies
        as a copy occurs every time we pushback a new vertex element
    */
   file << "v:" << std::to_string(p_Vertices.size()) << "\n";
   file << "i:" << std::to_string(p_Indices.size()) << "\n";
   file << ":start vertex\n";
   for(const auto& vertex : p_Vertices)
   {
       // Output position
       file << "{" << vertex.position.x << ", " << vertex.position.y << ", " << vertex.position.z << ", " << vertex.position.w << "}\n";
       // Output color
       file << "{" << vertex.color.r << ", " << vertex.color.g << ", " << vertex.color.b << ", " << vertex.color.a << "}\n";
       // Output uv
       file << "{" << vertex.uv.r << ", " << vertex.uv.g << ", " << vertex.uv.b << ", " << vertex.uv.a << "}\n";
   }
   file << ":end\n";

   file << ":start index\n";
   file << "{";
   for(const auto& index : p_Indices)
   {
       file << index << ", ";
   }
   file << "}\n";

    file.close();
    return;
}

void MapHandler::bindBuffer(vk::CommandBuffer& p_CommandBuffer)
{
    p_CommandBuffer.bindVertexBuffers(0, vertexBuffer, vk::DeviceSize{0});
    p_CommandBuffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);
    return;
}