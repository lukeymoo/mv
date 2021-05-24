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

    // Returns vertex array & indices array
    std::pair<std::vector<Vertex>, std::vector<uint32_t>> readHeightMap(std::string p_Filename);

    void bindBuffer(vk::CommandBuffer& p_CommandBuffer);

    void cleanup(const vk::Device& p_LogicalDevice);
};
