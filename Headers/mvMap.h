#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <thread>
#include <vector>
#include <vulkan/vulkan.hpp>

class GuiHandler;
struct Vertex;
class Engine;
class Image;

class MapHandler
{
public:
  MapHandler (Engine *p_ParentEngine);
  ~MapHandler ();

  // All vertices per edge/corner in a given chunk
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

  // Pairs of triangles that make up each square in a map
  struct Triangle
  {
    Vertex *first = nullptr;
    Vertex *second = nullptr;
    Vertex *third = nullptr;
  };
  struct Quad
  {
    // TL -> BL -> BR
    Triangle leftHalf;
    // BR -> TR -> TL
    Triangle rightHalf;
  };

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<Quad> quadContainer;

  bool isMapLoaded = false;

  std::string filename = "None";

  std::unique_ptr<vk::Buffer> vertexBuffer;
  std::unique_ptr<vk::DeviceMemory> vertexMemory;

  std::unique_ptr<vk::Buffer> indexBuffer;
  std::unique_ptr<vk::DeviceMemory> indexMemory;

  std::unique_ptr<Image> terrainTexture;
  vk::DescriptorSet terrainTextureDescriptor;

  std::unique_ptr<Image> terrainNormal;
  vk::DescriptorSet terrainNormalDescriptor;

  size_t vertexCount = 0;
  size_t indexCount = 0;

  // offset of vertices each index start/count correspond to
  // std::vector<size_t> vertexOffsets;
  // { index start, index count }
  // std::vector<std::pair<uint32_t, uint32_t>> indexOffsets;

  Engine *ptrEngine = nullptr;

  // Load map file while also updating Gui
  void readHeightMap (GuiHandler *p_Gui, std::string p_Filename, bool p_ForceReload = false);

  // Load map file without updating gui
  std::pair<std::vector<Vertex>, std::vector<uint32_t>> readHeightMap (std::string p_Filename,
                                                                       bool p_ForceReload);

  void bindBuffer (vk::CommandBuffer &p_CommandBuffer);

  void cleanup (const vk::Device &p_LogicalDevice);

private:
  // Populations vector of quads with vertex start positions
  void getQuads (std::vector<Vertex> &p_VertexContainer,
                 std::vector<uint32_t> &p_IndexContainer,
                 std::vector<Quad> &p_QuadContainer);

  // Generates array of quads from array of xyz points
  std::vector<Vertex>
  generateMesh (size_t p_XLength, size_t p_ZLength, std::vector<Vertex> &p_HeightValues);

  void cleanAndStitch (std::vector<ChunkData> &p_Chunks, std::vector<Vertex> &p_Stitches);

  std::pair<std::vector<Vertex>, std::vector<uint32_t>> optimize (std::vector<Vertex> &p_Vertices);

  void writeVertexFile (std::string p_FilenameIndexAppended,
                        std::vector<Vertex> &p_VertexContainer);

  void writeIndexFile (std::string p_FilenameIndexAppended,
                       std::vector<uint32_t> &p_IndexContainer);

  // Manipulates the filename that was originally requested by load
  // removes extension and appends _v.bin && _i.bin to look for the optimized
  // file Vertices file & indices file
  bool isAlreadyOptimized (std::string p_OriginalFilenameRequested);

  void readVertexFile (std::string p_FinalFilename, std::vector<Vertex> &p_VertexContainer);

  void readIndexFile (std::string p_FinalFilename, std::vector<uint32_t> &p_IndexContainer);

  void allocate (std::vector<Vertex> &p_VertexContainer, std::vector<uint32_t> &p_IndexContainer);

  // Removes duplicate values; if horizontal|vertical -> prunes x|z axis
  // This method also sorts the vertices based on target axis
  // Does not manipulate splitValues container so ptrs are still valid
  void cleanEdge (std::vector<Vertex *> &p_Edge, bool isHorizontal);

  // Provide edge params left->right or top->bottom
  // creates quads between two provided edges
  void stitchEdge (std::vector<Vertex *> &p_FirstEdge,
                   std::vector<Vertex *> &p_SecondEdge,
                   std::vector<Vertex> &p_Vertices,
                   bool isVertical);

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
  void stitchCorner (Vertex *p_FirstCorner,
                     Vertex *p_FirstEdgeVertex,
                     Vertex *p_SecondCorner,
                     Vertex *p_SecondEdgeVertex,
                     std::vector<Vertex> &p_Vertices,
                     WindingStyle p_Style);

  // stitch center corners where chunks intersect
  // void stitchCorner (Vertex *p_TLCorner, Vertex *p_TRCorner,
  //                    Vertex *p_BRCorner, Vertex *p_BLCorner,
  //                    std::vector<Vertex> &p_Vertices);

  std::string getBaseFilename (const std::string &p_Filename);

  // converts to _v.bin filename
  std::string filenameToBinV (const std::string &p_Filename);
  // converts to _i.bin filename
  std::string filenameToBinI (const std::string &p_Filename);

  // Load vertices & indices data from an already preoptimized file
  void loadPreoptimized (std::string p_Filename,
                         std::vector<Vertex> &p_Vertices,
                         std::vector<uint32_t> &p_Indices);

  // minimizes exposure to c interface
  // encapsulate stbi loader; place needed data in stl container
  void loadPNG (size_t &p_XLengthRet,
                size_t &p_ZLengthRet,
                size_t &p_ChannelsRet,
                std::string &p_Filename,
                std::vector<unsigned char> &p_ImageDataRet);

  // Creates base vertices from PNG image; Will tag edge vertices via `color` field in Vertex
  // p_ImageLength is the length of a single side of the image; Image should be a 1:1 square
  void pngToVertices (size_t p_ImageLength,
                      std::vector<unsigned char> &p_RawImage,
                      std::vector<Vertex> &p_VertexContainer);
};
