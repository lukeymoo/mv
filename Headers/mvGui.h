#pragma once

#include "mvGuiStructures.h"
#include "mvMisc.h"
#include "mvSwap.h"

// Intermediate method for handling textbox input in ImGui
template <typename F, typename T>
F handleInputThunk (T t);

using namespace gui;
class GuiHandler
{
public:
  GuiHandler (GLFWwindow *p_GLFWwindow,
              MapHandler *p_MapHandler,
              Camera *p_CameraHandler,
              const vk::Instance &p_Instance,
              const vk::PhysicalDevice &p_PhysicalDevice,
              const vk::Device &p_LogicalDevice,
              const Swap &p_MvSwap,
              const vk::CommandPool &p_CommandPool,
              const vk::Queue &p_GraphicsQueue,
              std::unordered_map<RenderPassType, vk::RenderPass> &p_RenderPassMap,
              const vk::DescriptorPool &p_DescriptorPool);
  ~GuiHandler ();

  bool hasFocus = false;
  bool hover = false;

  // Set the selectedTerrainItem to specified name
  inline void
  setLoadedTerrainFile (std::string p_Filename)
  {
    // store full path
    getModal<MapModal> (GuiModals).terrainItem.fullPath = p_Filename;

    // use only base filename
    size_t basePos = p_Filename.find ('/');
    do
      {
        p_Filename.erase (0, basePos + 1);
      }
    while ((basePos = p_Filename.find ('/')) != std::string::npos);

    getModal<MapModal> (GuiModals).terrainItem.filename = p_Filename;
    return;
  }

public:
  GLFWwindow *window = nullptr;
  std::chrono::_V2::system_clock::time_point lastDeltaUpdate
      = std::chrono::high_resolution_clock::now ();

  float storedFrameDelta = 0.0f;
  float storedRenderDelta = 0.0f;
  int storedFPS = 0;
  int displayFPS = 0;
  int frameCount = 0;

  LogHandler *ptrLogger = nullptr;
  Camera *ptrCamera = nullptr;
  MapHandler *ptrMapHandler = nullptr;

  // holds data about current directory
  FileSystem fileSystem;

  // Container for all modals
  // Access via getModal<T>(GuiModals);
  std::unordered_map<ModalType, std::any> GuiModals;

  const int menuBarHeight = 18;
  gui::ModalFunction show;

public:
  ImGuiIO &getIO (void);

  void update (const vk::Extent2D &p_SwapExtent,
               float p_RenderDelta,
               float p_FrameDelta,
               uint32_t p_ModelCount,
               uint32_t p_ObjectCount,
               uint32_t p_VertexCount);

  std::vector<vk::Framebuffer>
  createFramebuffers (const vk::Device &p_LogicalDevice,
                      const vk::RenderPass &p_GuiRenderPass,
                      std::vector<struct SwapchainBuffer> &p_SwapchainBuffers,
                      uint32_t p_SwapExtentWidth,
                      uint32_t p_SwapExtentHeight);
  void newFrame (void);

  void renderFrame (void);

  void doRenderPass (const vk::RenderPass &p_RenderPass,
                     const vk::Framebuffer &p_Framebuffer,
                     const vk::CommandBuffer &p_CommandBuffer,
                     vk::Extent2D &p_RenderAreaExtent);

  void cleanup (const vk::Device &p_LogicalDevice);

  // Should only call if recreating swapchain
  // Do not call outside of that special event as this function
  // is automatically called by gui constructor
  void createRenderPass (std::unordered_map<RenderPassType, vk::RenderPass> &p_RenderPassMap,
                         const vk::Device &p_LogicalDevice,
                         const vk::Format &p_AttachmentColorFormat);

  // Generate mesh from PNG image or load existing mesh files if exists
  void loadHeightMap (std::string p_Path, std::string p_Filename);

  // Load .map file which contains every from terrain data, to objects & their
  // placements/rotations
  void loadMapFile (std::string p_Path, std::string p_Filename);

  // Handles input for camera modal
  template <typename T>
  inline void
  handleInput (T t)
  {
    if (t->EventFlag & ImGuiInputTextFlags_CallbackCharFilter)
      {
        if (t->EventChar < 48 || t->EventChar > 57)
          {
            t->EventChar = 0;
            std::cout << "Only numerical characters allowed\n";
          }
      }
    return;
  }

public:
  inline void
  outputSelectableList (
      std::vector<std::pair<FileSystem::FileType, std::pair<std::string, bool>>> &p_List,
      std::filesystem::path &p_TargetPath,
      ListType p_ListType)
  {
    for (auto &file : p_List)
      {
        if (file.first == FileSystem::FileType::eFile)
          {
            ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (1.0f, 1.0f, 1.0f, 1.0f));
          }
        else if (file.first == FileSystem::FileType::eDirectory)
          {
            ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.75f, 0.25f, 0.25f, 1.0f));
          }
        ImGui::PushStyleColor (ImGuiCol_Button, ImVec4 (0.25f, 0.25f, 0.25f, 0.5f));

        // If user clicks entry & is directory; append to path
        ImGui::Selectable (
            file.second.first.c_str (), &file.second.second, ImGuiSelectableFlags_AllowDoubleClick);
        if (ImGui::IsItemHovered () && ImGui::IsMouseDoubleClicked (0))
          {
            if (file.first == FileSystem::FileType::eDirectory)
              {
                p_TargetPath.append (file.second.first + "/");
              }
            else if (file.first == FileSystem::FileType::eFile)
              {
                // Select appropriate action
                if (p_ListType == ListType::eHeightMaps)
                  {
                    loadHeightMap (p_TargetPath, file.second.first);
                  }
                else if (p_ListType == ListType::eMapFiles)
                  {
                    loadMapFile (p_TargetPath, file.second.first);
                  }
              }
          }

        ImGui::PopStyleColor (2);
      }
    return;
  }

  enum FocusModal
  {
    eNone = 0,
    eFile,
    eOpenModal,
    eSaveModal,
    eQuit,
  };
  // Set appropriate state values for which modal to display
  // Block main loop from receiving keyboard inputs
public:
  inline void grabFocus (FocusModal p_WhichModal);
  // Reset all modal states & resume allowing game loop to receive keyboard
  // inputs
  inline void releaseFocus (void);

private:
  // Render main menu
  inline void renderMenu (void);

  // Render terrain modal
  void renderMapConfigModal (void);

  void renderCameraModal (void);

  using DebugMessageList = std::vector<std::pair<LogHandlerMessagePriority, std::string>>;
  void renderDebugModal (DebugMessageList p_DebugMessageList);
};
