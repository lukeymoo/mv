#pragma once

#include <vulkan/vulkan.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

struct Vertex;

class MapHandler
{
  public:
    MapHandler();
    ~MapHandler();

    enum NoiseType
    {
        ePerlin = 0,
        eOpenSimplex,
    };

    vk::Buffer vertexBuffer;
    vk::DeviceMemory vertexMemory;

    vk::Buffer indexBuffer;
    vk::DeviceMemory indexMemory;

    size_t vertexCount = 0;
    size_t indexCount = 0;

    void optimize(std::vector<Vertex>& p_Vertices, std::vector<uint32_t>& p_Indices);

    void writeRaw(std::string p_OriginalFilenameRequested,
                  std::vector<Vertex>& p_VertexContainer,
                  std::vector<uint32_t>& p_IndexContainer);

    // Returns vertex array & indices array
    std::pair<std::vector<Vertex>, std::vector<uint32_t>> readHeightMap(std::string p_Filename);

    void bindBuffer(vk::CommandBuffer& p_CommandBuffer);

    void cleanup(const vk::Device& p_LogicalDevice);

  private:
    // Manipulates the filename that was originally requested by load
    // removes extension and appends _v.bin && _i.bin to look for the optimized file
    // Vertices file & indices file
    bool isAlreadyOptimized(std::string p_OriginalFilenameRequested);

    void readVertexFile(std::string p_OriginalFilenameRequested,
                        std::vector<Vertex>& p_VertexContainer);

    void readIndexFile(std::string p_OriginalFilenameRequested,
                        std::vector<uint32_t>& p_IndexContainer);

    // converts to _v.bin filename
    std::string filenameToBinV(std::string& p_Filename);
    // converts to _i.bin filename
    std::string filenameToBinI(std::string& p_Filename);
};
