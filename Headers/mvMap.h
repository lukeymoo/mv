#pragma once

#include <vulkan/vulkan.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

class GuiHandler;
struct Vertex;
class Engine;

class MapHandler
{
public:
  MapHandler (Engine *p_ParentEngine);
  ~MapHandler ();

  bool isMapLoaded = false;

  std::string filename = "None";

  vk::Buffer vertexBuffer;
  vk::DeviceMemory vertexMemory;

  vk::Buffer indexBuffer;
  vk::DeviceMemory indexMemory;

  size_t vertexCount = 0;
  size_t indexCount = 0;

  // offset of vertices each index start/count correspond to
  std::vector<size_t> vertexOffsets;
  // { index start, index count }
  std::vector<std::pair<uint32_t, uint32_t>> indexOffsets;

  Engine *ptrEngine = nullptr;

  // Load map file while also updating Gui
  void readHeightMap (GuiHandler *p_Gui, std::string p_Filename);

  // Load map file without updating gui
  std::pair<std::vector<Vertex>, std::vector<uint32_t>>
  readHeightMap (std::string p_Filename);

  void bindBuffer (vk::CommandBuffer &p_CommandBuffer);

  void cleanup (const vk::Device &p_LogicalDevice);

private:
  // Generates array of quads from array of xyz points
  std::vector<Vertex> generateMesh (size_t p_XLength, size_t p_ZLength,
                                    std::vector<Vertex> &p_HeightValues);

  std::pair<std::vector<Vertex>, std::vector<uint32_t>>
  optimize (std::vector<Vertex> &p_Vertices);

  void writeRaw (std::string p_FilenameIndexAppended,
                 std::vector<Vertex> &p_VertexContainer,
                 std::vector<uint32_t> &p_IndexContainer);

  void writeVertexFile (std::string p_FilenameIndexAppended,
                        std::vector<Vertex> &p_VertexContainer);

  void writeIndexFile (std::string p_FilenameIndexAppended,
                       std::vector<uint32_t> &p_IndexContainer);

  // Manipulates the filename that was originally requested by load
  // removes extension and appends _v.bin && _i.bin to look for the optimized
  // file Vertices file & indices file
  bool isAlreadyOptimized (std::string p_OriginalFilenameRequested);

  void readVertexFile (std::string p_OriginalFilenameRequested,
                       std::vector<Vertex> &p_VertexContainer);

  void readIndexFile (std::string p_OriginalFilenameRequested,
                      std::vector<uint32_t> &p_IndexContainer);

  void allocate (std::vector<Vertex> &p_VertexContainer,
                 std::vector<uint32_t> &p_IndexContainer);

  // Removes duplicate values; if horizontal|vertical -> prunes x|z axis
  // This method also sorts the vertices based on target axis
  // Does not manipulate splitValues container so ptrs are still valid
  void cleanEdge (std::vector<Vertex *> &p_Edge, bool isHorizontal);

  // Provide edge params left->right or top->bottom
  // creates quads between two provided edges
  void stitchEdge (std::vector<Vertex *> &p_FirstEdge,
                   std::vector<Vertex *> &p_SecondEdge,
                   std::vector<Vertex> &p_Vertices, bool isVertical);

  enum WindingStyle
  {
    eFirstCornerToFirstEdge = 0,
    eFirstCornerToSecondCorner,
    eFirstCornerToSecondEdge,
    eFirstEdgeToFirstCorner,
    eFirstEdgeToSecondEdge,
    eCenter
  };
  // Needs first vertex that starts the edge
  void stitchCorner (Vertex *p_FirstCorner, Vertex *p_FirstEdgeVertex,
                     Vertex *p_SecondCorner, Vertex *p_SecondEdgeVertex,
                     std::vector<Vertex> &p_Vertices, WindingStyle p_Style);

  // stitch center corners where chunks intersect
  // void stitchCorner (Vertex *p_TLCorner, Vertex *p_TRCorner,
  //                    Vertex *p_BRCorner, Vertex *p_BLCorner,
  //                    std::vector<Vertex> &p_Vertices);

  std::string getBaseFilename (std::string &p_Filename);

  // converts to _v.bin filename
  std::string filenameToBinV (std::string &p_Filename);
  // converts to _i.bin filename
  std::string filenameToBinI (std::string &p_Filename);
};
