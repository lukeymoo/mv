#include "mvGui.h"
#include "mvGuiStructures.h"

// For interfacing with camera
#include "mvCamera.h"

// For interfacing with map handler
#include "mvMap.h"

// For mvMap methods
#include "mvModel.h"

// Helper funcs
#include "mvHelper.h"

extern LogHandler logger;

// void NoShow(GuiHandler *);
// void showOpenDialog(GuiHandler *);
// void showSaveDialog(GuiHandler *);
// void showQuitDialog(GuiHandler *);
// void selectNewTerrainFile(GuiHandler *);

GuiHandler::GuiHandler (GLFWwindow *p_GLFWwindow,
                        MapHandler *p_MapHandler,
                        Camera *p_CameraHandler,
                        const vk::Instance &p_Instance,
                        const vk::PhysicalDevice &p_PhysicalDevice,
                        const vk::Device &p_LogicalDevice,
                        const Swap &p_MvSwap,
                        const vk::CommandPool &p_CommandPool,
                        const vk::Queue &p_GraphicsQueue,
                        std::unordered_map<RenderPassType, vk::RenderPass> &p_RenderPassMap,
                        const vk::DescriptorPool &p_DescriptorPool)
{
  if (!p_CameraHandler)
    throw std::runtime_error ("Invalid camera handle passed to gui handler");

  if (!p_MapHandler)
    throw std::runtime_error ("Invalid map handler passed to gui handler");

  this->ptrLogger = &logger;
  this->ptrCamera = p_CameraHandler;
  this->ptrMapHandler = p_MapHandler;

  // Explicitly initialize before others
  GuiModals = {
    { eFileMenu, ModalEntry<FileMenu>{} },
    { eDebug, ModalEntry<DebugModal>{} },
    { eMap, ModalEntry<MapModal>{} },
    { eCamera, ModalEntry<CameraModal>{} },
  };

  // check if map is loaded
  if (ptrMapHandler->isMapLoaded)
    {
      setLoadedTerrainFile (ptrMapHandler->filename);
    }

  switch (ptrCamera->type)
    {
      using enum ::CameraType;
    case eInvalid:
      {
        throw std::runtime_error ("Camera handler not initialized before Gui : Camera type is "
                                  "set to eInvalid");
        break;
      }
    case eFreeLook:
      {
        getModal<MapModal> (GuiModals).cameraItem.select (MapModal::CameraItem::Type::eFreeLook);
        break;
      }
    case eThirdPerson:
      {
        getModal<MapModal> (GuiModals).cameraItem.select (MapModal::CameraItem::Type::eThirdPerson);
        break;
      }
    case eFirstPerson:
      {
        getModal<MapModal> (GuiModals).cameraItem.select (MapModal::CameraItem::Type::eFirstPerson);
        break;
      }
    case eIsometric:
      {
        getModal<MapModal> (GuiModals).cameraItem.select (MapModal::CameraItem::Type::eIsometric);
        break;
      }
    }
  if (!p_GLFWwindow)
    throw std::runtime_error ("Invalid GLFW window handle passed to Gui handler");

  window = p_GLFWwindow;
  // Ensure gui not already in render pass map
  if (p_RenderPassMap.find (eImGui) != p_RenderPassMap.end ())
    {
      logger.logMessage (LogHandlerMessagePriority::eWarning,
                         "ImGui already initialized, skipping...");
      return;
    }
  // Create render pass
  createRenderPass (p_RenderPassMap, p_LogicalDevice, p_MvSwap.colorFormat);

  IMGUI_CHECKVERSION ();
  ImGui::CreateContext ();

  ImGui::StyleColorsDark ();

  ImGui_ImplVulkan_InitInfo initInfo;
  memset (&initInfo, 0, sizeof (ImGui_ImplVulkan_InitInfo));

  initInfo.Instance = p_Instance;
  initInfo.PhysicalDevice = p_PhysicalDevice;
  initInfo.Device = p_LogicalDevice;
  initInfo.QueueFamily = p_MvSwap.graphicsIndex;
  initInfo.Queue = p_GraphicsQueue;
  initInfo.PipelineCache = nullptr;
  initInfo.DescriptorPool = p_DescriptorPool;
  initInfo.MinImageCount = static_cast<uint32_t> (p_MvSwap.buffers.size ());
  initInfo.ImageCount = static_cast<uint32_t> (p_MvSwap.buffers.size ());
  initInfo.Allocator = nullptr;
  initInfo.CheckVkResultFn = nullptr;
  initInfo.Subpass = 0;
  initInfo.MSAASamples = static_cast<VkSampleCountFlagBits> (vk::SampleCountFlagBits::e1);

  ImGui_ImplGlfw_InitForVulkan (p_GLFWwindow, true);
  ImGui_ImplVulkan_Init (&initInfo, p_RenderPassMap.at (eImGui));

  /*
    Upload ImGui render fonts
  */
  vk::CommandBuffer guiCommandBuffer = Helper::beginCommandBuffer (p_LogicalDevice, p_CommandPool);

  ImGui_ImplVulkan_CreateFontsTexture (guiCommandBuffer);

  Helper::endCommandBuffer (p_LogicalDevice, p_CommandPool, guiCommandBuffer, p_GraphicsQueue);

  ImGui_ImplVulkan_DestroyFontUploadObjects ();
  return;
}

GuiHandler::~GuiHandler () { return; }

void
GuiHandler::createRenderPass (std::unordered_map<RenderPassType, vk::RenderPass> &p_RenderPassMap,
                              const vk::Device &p_LogicalDevice,
                              const vk::Format &p_AttachmentColorFormat)
{
  std::array<vk::AttachmentDescription, 1> guiAttachments;

  // ImGui attachment
  guiAttachments[0].format = p_AttachmentColorFormat;
  guiAttachments[0].samples = vk::SampleCountFlagBits::e1;
  guiAttachments[0].loadOp = vk::AttachmentLoadOp::eLoad;
  guiAttachments[0].storeOp = vk::AttachmentStoreOp::eStore;
  guiAttachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  guiAttachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  guiAttachments[0].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
  guiAttachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

  vk::AttachmentReference guiRef;
  guiRef.attachment = 0;
  guiRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::SubpassDescription guiPass;
  guiPass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  guiPass.colorAttachmentCount = 1;
  guiPass.pColorAttachments = &guiRef;

  std::vector<vk::SubpassDescription> guiSubpasses = { guiPass };

  std::array<vk::SubpassDependency, 1> guiDependencies;
  // ImGui subpass
  guiDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  guiDependencies[0].dstSubpass = 0;
  guiDependencies[0].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  guiDependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  guiDependencies[0].srcAccessMask
      = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  guiDependencies[0].dstAccessMask
      = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  guiDependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

  vk::RenderPassCreateInfo guiRenderPassInfo;
  guiRenderPassInfo.attachmentCount = static_cast<uint32_t> (guiAttachments.size ());
  guiRenderPassInfo.pAttachments = guiAttachments.data ();
  guiRenderPassInfo.subpassCount = static_cast<uint32_t> (guiSubpasses.size ());
  guiRenderPassInfo.pSubpasses = guiSubpasses.data ();
  guiRenderPassInfo.dependencyCount = static_cast<uint32_t> (guiDependencies.size ());
  guiRenderPassInfo.pDependencies = guiDependencies.data ();

  vk::RenderPass newPass = p_LogicalDevice.createRenderPass (guiRenderPassInfo);

  p_RenderPassMap.insert ({
      eImGui,
      std::move (newPass),
  });

  return;
}

std::vector<vk::Framebuffer>
GuiHandler::createFramebuffers (const vk::Device &p_LogicalDevice,
                                const vk::RenderPass &p_GuiRenderPass,
                                std::vector<struct SwapchainBuffer> &p_SwapchainBuffers,
                                uint32_t p_SwapExtentWidth,
                                uint32_t p_SwapExtentHeight)
{
  std::vector<vk::Framebuffer> framebuffers;

  // Attachment for ImGui rendering
  std::array<vk::ImageView, 1> imguiAttachments;

  // ImGui must only have 1 attachment
  vk::FramebufferCreateInfo guiFrameInfo;
  guiFrameInfo.renderPass = p_GuiRenderPass;
  guiFrameInfo.attachmentCount = static_cast<uint32_t> (imguiAttachments.size ());
  guiFrameInfo.pAttachments = imguiAttachments.data ();
  guiFrameInfo.width = p_SwapExtentWidth;
  guiFrameInfo.height = p_SwapExtentHeight;
  guiFrameInfo.layers = 1;

  framebuffers.resize (static_cast<uint32_t> (p_SwapchainBuffers.size ()));
  for (size_t i = 0; i < p_SwapchainBuffers.size (); i++)
    {
      imguiAttachments[0] = p_SwapchainBuffers.at (i).view;
      framebuffers.at (i) = p_LogicalDevice.createFramebuffer (guiFrameInfo);
    }
  return framebuffers;
}

void
GuiHandler::newFrame (void)
{
  ImGui_ImplVulkan_NewFrame ();
  ImGui_ImplGlfw_NewFrame ();
  ImGui::NewFrame ();
  return;
}

void
GuiHandler::renderFrame (void)
{
  ImGui::Render ();
  return;
}

void
GuiHandler::cleanup (const vk::Device &p_LogicalDevice)
{
  p_LogicalDevice.waitIdle ();

  ImGui_ImplVulkan_Shutdown ();
  ImGui::DestroyContext ();
  return;
}

void
GuiHandler::doRenderPass (const vk::RenderPass &p_RenderPass,
                          const vk::Framebuffer &p_Framebuffer,
                          const vk::CommandBuffer &p_CommandBuffer,
                          vk::Extent2D &p_RenderAreaExtent)
{
  constexpr auto defaultClearColor = std::array<float, 4> ({ { 0.0f, 0.0f, 0.0f, 1.0f } });
  std::array<vk::ClearValue, 1> cls;
  cls[0].color = defaultClearColor;

  vk::RenderPassBeginInfo guiPassInfo;
  guiPassInfo.renderPass = p_RenderPass;
  guiPassInfo.framebuffer = p_Framebuffer;
  guiPassInfo.renderArea.offset.x = 0;
  guiPassInfo.renderArea.offset.y = 0;
  guiPassInfo.renderArea.extent.width = p_RenderAreaExtent.width;
  guiPassInfo.renderArea.extent.height = p_RenderAreaExtent.height;
  guiPassInfo.clearValueCount = static_cast<uint32_t> (cls.size ());
  guiPassInfo.pClearValues = cls.data ();

  p_CommandBuffer.beginRenderPass (guiPassInfo, vk::SubpassContents::eInline);

  ImGui_ImplVulkan_RenderDrawData (ImGui::GetDrawData (), p_CommandBuffer);

  p_CommandBuffer.endRenderPass ();
  return;
}

void
GuiHandler::update (const vk::Extent2D &p_SwapExtent,
                    float p_RenderDelta,
                    float p_FrameDelta,
                    uint32_t p_ModelCount,
                    uint32_t p_ObjectCount,
                    uint32_t p_VertexCount)
{
  /*
      Determine if should update engine status deltas
  */
  auto updateEnd = std::chrono::high_resolution_clock::now ();
  float timeSince
      = std::chrono::duration<float, std::ratio<1L, 1L>> (updateEnd - lastDeltaUpdate).count ();
  frameCount++;
  if (timeSince >= 1.0f)
    {
      storedRenderDelta = p_RenderDelta;
      storedFrameDelta = p_FrameDelta;
      lastDeltaUpdate = std::chrono::high_resolution_clock::now ();
      displayFPS = frameCount;
      frameCount = 0;
    }

  /*
      FUNCTION LAMBDAS
  */

  /*
      KEYBOARD SHORTCUTS
  */
  // Show quit menu if no popups
  if (getIO ().KeysDown[GLFW_KEY_ESCAPE])
    if (show == noShow)
      show = showQuitDialog;
  // MENU SHORTCUTS
  if (getIO ().KeysDown[GLFW_KEY_LEFT_CONTROL])
    {
      // Open file dialog
      if (getIO ().KeysDown[GLFW_KEY_O])
        show = showOpenDialog;

      // Save file as dialog
      if (getIO ().KeysDown[GLFW_KEY_LEFT_SHIFT] || getIO ().KeysDown[GLFW_KEY_RIGHT_SHIFT])
        if (getIO ().KeysDown[GLFW_KEY_S])
          show = showSaveDialog;

      // Quit Dialog
      if (getIO ().KeysDown[GLFW_KEY_Q])
        show = showQuitDialog;
    }

  // Render main menu
  renderMenu ();

  // Map config modal
  renderMapConfigModal ();

  // Render debug console
  renderDebugModal (logger.getMessages ());

  // Render camera modal
  renderCameraModal ();

  // display popup menu callback if any
  show (this);

  /*
      Engine status data
  */
  ImGuiWindowFlags engineDataFlags
      = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;
  ImGui::SetNextWindowPos (ImVec2 (0, p_SwapExtent.height - 32));
  ImGui::SetNextWindowSize (
      ImVec2 (p_SwapExtent.width - getModal<DebugModal> (GuiModals).width, 32));
  ImGui::Begin ("Status", nullptr, engineDataFlags);
  ImGui::Text ("Render time: %.2f ms | Frame time: %.2f ms | FPS: %i | Model "
               "Count: %i | Object Count: %i | Vertex "
               "Count: %i",
               storedRenderDelta,
               storedFrameDelta,
               displayFPS,
               p_ModelCount,
               p_ObjectCount,
               p_VertexCount);
  ImGui::End ();

  // Clear key states
  getIO ().ClearInputCharacters ();
  return;
}

ImGuiIO &
GuiHandler::getIO (void)
{
  return ImGui::GetIO ();
}

inline void
GuiHandler::renderMenu (void)
{
  ImGui::BeginMainMenuBar ();

  /*
      File Menu
  */
  if (ImGui::BeginMenu ("File"))
    {
      // Open file button
      if (ImGui::MenuItem ("Open", "Ctrl+O"))
        {
          show = showOpenDialog;
        }

      if (ImGui::MenuItem ("Save", nullptr))
        {
          // Test if new file
        }

      if (ImGui::MenuItem ("Save As...", "Ctrl+Shift+S"))
        {
          show = showSaveDialog;
        }

      // Quit button
      if (ImGui::MenuItem ("Quit", "Ctrl+Q"))
        {
          // are you sure?
          show = showQuitDialog;
        }

      ImGui::EndMenu ();
    }

  ImGui::EndMainMenuBar ();
} // end renderMenu

inline void
GuiHandler::renderMapConfigModal (void)
{
  // Terrain info
  auto viewport = ImGui::GetMainViewport ();
  auto pos = viewport->Size;
  pos.x -= getModal<MapModal> (GuiModals).width;
  pos.y = menuBarHeight;
  ImGui::SetNextWindowPos (pos);
  ImGui::SetNextWindowSize (
      ImVec2 (getModal<MapModal> (GuiModals).width, getModal<MapModal> (GuiModals).height));

  ImGui::Begin ("Map Details", nullptr, getModal<MapModal> (GuiModals).windowFlags);

  /*
      Selected terrain map
  */
  ImGui::Text ("Terrain file: ");
  ImGui::SameLine ();
  ImGui::Selectable (getModal<MapModal> (GuiModals).terrainItem.filename.c_str (),
                     &getModal<MapModal> (GuiModals).terrainItem.isSelected,
                     getModal<MapModal> (GuiModals).terrainItem.flags);
  if (ImGui::IsItemHovered ())
    {
      if (ImGui::IsMouseDoubleClicked (0))
        show = selectNewTerrainFile;
    }
  else
    {
      getModal<MapModal> (GuiModals).terrainItem.isSelected = false;
    }
  // Reload heightmap
  ImGui::Spacing ();
  ImGui::Spacing ();
  ImGui::Button ("Reload heightmap");
  if (ImGui::IsMouseDoubleClicked (ImGuiMouseButton_Left) && ImGui::IsItemHovered ())
    {
      // Confirm map regeneration
      try
        {
          if (getModal<MapModal> (GuiModals).terrainItem.fullPath != std::string ("None"))
            ptrMapHandler->readHeightMap (getModal<MapModal> (GuiModals).terrainItem.fullPath,
                                          true);
        }
      catch (std::exception &e)
        {
          std::cout << ":: Failed to reload height map ::\n";
          std::cout << std::string (e.what ()) << "\n";
        }
    }

  /*
      Camera configuration
  */
  ImGui::Spacing ();
  ImGui::Text ("Camera style");
  ImGui::SameLine ();

  ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (1.0f, 0.75f, 0.0f, 1.0f));
  if (ImGui::TreeNode (
          getModal<MapModal> (GuiModals)
              .cameraItem.typeMap.at (getModal<MapModal> (GuiModals).cameraItem.selectedType)
              .first.c_str ()))
    {
      using CameraItemType = gui::MapModal::CameraItem::Type;

      ImGui::Dummy (ImVec2 (64, 1));
      ImGui::SameLine ();

      // FREE LOOK
      if (ImGui::MenuItem (
              getModal<MapModal> (GuiModals)
                  .cameraItem.getTypeName (CameraItemType::eFreeLook)
                  .c_str (),
              nullptr,
              getModal<MapModal> (GuiModals).cameraItem.getIsSelectedRef (
                  CameraItemType::eFreeLook),
              !getModal<MapModal> (GuiModals).cameraItem.getIsSelected (CameraItemType::eFreeLook)))
        {
          if (ptrCamera->setCameraType (CameraType::eFreeLook,
                                        glm::vec3 (ptrCamera->position.x,
                                                   ptrCamera->position.y - 2.0f,
                                                   ptrCamera->position.z)))
            {
              getModal<MapModal> (GuiModals).cameraItem.deselectAllBut (CameraItemType::eFreeLook);
              getModal<MapModal> (GuiModals).cameraItem.selectedType
                  = MapModal::CameraItem::Type::eFreeLook;
            }
          else
            {
              getModal<MapModal> (GuiModals).cameraItem.deselectAllBut (
                  getModal<MapModal> (GuiModals).cameraItem.selectedType);
            }
        }

      ImGui::Dummy (ImVec2 (64, 1));
      ImGui::SameLine ();

      // THIRD PERSON
      //   getModal<MapModal>(GuiModals).cameraItem.getTypeName (CameraItemType::eThirdPerson)
      if (ImGui::MenuItem (getModal<MapModal> (GuiModals)
                               .cameraItem.getTypeName (CameraItemType::eThirdPerson)
                               .c_str (),
                           nullptr,
                           getModal<MapModal> (GuiModals).cameraItem.getIsSelectedRef (
                               CameraItemType::eThirdPerson),
                           !getModal<MapModal> (GuiModals).cameraItem.getIsSelected (
                               CameraItemType::eThirdPerson)))
        {
          if (ptrCamera->setCameraType (CameraType::eThirdPerson, { 0.0f, 0.0f, 0.0f }))
            {
              std::cout << "switching to third person\n";
              getModal<MapModal> (GuiModals).cameraItem.deselectAllBut (
                  CameraItemType::eThirdPerson);
              getModal<MapModal> (GuiModals).cameraItem.selectedType
                  = MapModal::CameraItem::Type::eThirdPerson;
            }
          else
            {
              std::cout << "switching to free look\n";
              getModal<MapModal> (GuiModals).cameraItem.deselectAllBut (
                  getModal<MapModal> (GuiModals).cameraItem.selectedType);
            }
        }
      ImGui::TreePop ();
    }
  ImGui::PopStyleColor (1);
  ImGui::End ();
  return;
}

// Load .map file; Contains terrain mesh data, required models, objects(coords,
// rotation) & other data
void
GuiHandler::loadMapFile (std::string p_Path, std::string p_Filename)
{
  std::cout << "[+] User requested to load map file\n";

  // Clear modals
  hasFocus = false;
  show = noShow;
  getModal<MapModal> (GuiModals).selectTerrainModal.isOpen = false;

  return;
}

// Generate terrain mesh from PNG & output optimized mesh data to files
// Or if already exists, load the pre optimized mesh data
void
GuiHandler::loadHeightMap (std::string p_Path, std::string p_Filename)
{
  std::cout << "User requested heightmap : " << p_Path << p_Filename << "\n";

  // Clear modals
  hasFocus = false;
  show = noShow;
  getModal<MapModal> (GuiModals).selectTerrainModal.isOpen = false;

  try
    {
      // Load heightmap
      ptrMapHandler->readHeightMap (this, p_Path + p_Filename);
    }
  catch (std::exception &e)
    {
      auto msg = ":: Fatal error in gui handler ::\n  Attempted to load map file " + p_Path
                 + p_Filename + " but exception occurred => " + std::string (e.what ());

      std::cerr << msg << "\n";
      glfwSetWindowShouldClose (window, GLFW_TRUE);
    }

  return;
}

/*
    MODAL METHODS
*/
template <typename P>
gui::ModalFunction
gui::ModalFunction::operator= (P *ptr)
{
  toExec = ptr;
  return *this;
}

void
noShow (GuiHandler *p_This)
{
  return;
}

void
showOpenDialog (GuiHandler *p_This)
{
  using enum GuiHandler::FocusModal;

  if (!ImGui::IsPopupOpen ("Load map file...",
                           ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    {
      getModal<FileMenu> (p_This->GuiModals).openModal.isOpen = true;
      ImGui::SetNextWindowSize (ImVec2 (800, 600));
      ImGui::OpenPopup ("Load map file...", ImGuiPopupFlags_NoOpenOverExistingPopup);
    }

  if (ImGui::BeginPopupModal ("Load map file...",
                              &getModal<FileMenu> (p_This->GuiModals).openModal.isOpen,
                              getModal<FileMenu> (p_This->GuiModals).openModal.flags))
    {
      p_This->grabFocus (eFile);
      getModal<FileMenu> (p_This->GuiModals).openModal.isOpen = true;

      // Split directory
      std::vector<std::pair<int, std::string>> directoryButtons
          = splitPath (p_This->fileSystem.currentDirectory.string ());

      // Output directory path as buttons
      auto outputSplitPathAsButtons = [&] (std::pair<int, std::string> p_Directory) {
        if (ImGui::Button (p_Directory.second.c_str ()))
          {
            std::string newDirectory;
            for (int i = 0; i <= p_Directory.first; i++)
              {
                newDirectory.append (directoryButtons.at (i).second + "/");
              }
            p_This->fileSystem.currentDirectory = "/" + newDirectory;
          }
        ImGui::SameLine ();
        ImGui::Text ("/");
        ImGui::SameLine ();
      };
      std::for_each (directoryButtons.begin (), directoryButtons.end (), outputSplitPathAsButtons);

      std::vector<std::pair<gui::FileSystem::FileType, std::pair<std::string, bool>>>
          filesInDirectory;

      // Get directory entries in current path
      try
        {
          auto directoryIterator
              = std::filesystem::directory_iterator (p_This->fileSystem.currentDirectory);
          for (const auto &file : directoryIterator)
            {
              gui::FileSystem::FileType entryType;
              try
                {
                  if (file.is_directory ())
                    {
                      entryType = gui::FileSystem::FileType::eDirectory;
                    }
                  else if (file.is_regular_file ())
                    {
                      entryType = gui::FileSystem::FileType::eFile;
                    }
                }
              catch (std::filesystem::filesystem_error &e)
                {
                  p_This->ptrLogger->logMessage ("File system error => " + std::string (e.what ()));
                }

              std::string filePath = file.path ().string ();

              std::string filename = filePath.replace (
                  0, p_This->fileSystem.currentDirectory.string ().length (), "");

              // Remove slash if it exists
              auto slash = filename.find ('/');
              if (slash != std::string::npos)
                filename.replace (slash, 1, "");

              // If not hidden file or hidden directory
              if (filename.at (0) == '.')
                continue;

              // get extension
              // our map files will be .mvmap
              auto extStart = filename.find ('.');
              if (entryType == gui::FileSystem::FileType::eFile)
                {
                  if (extStart == std::string::npos)
                    {
                      continue;
                    }
                  else
                    {
                      auto ext = filename.substr (extStart, filename.length () - 1);
                      if (ext != ".mvmap")
                        continue;
                    }
                }

              // { TYPE, { NAME, ISSELECTED } }
              filesInDirectory.push_back ({
                  entryType,
                  {
                      filename,
                      false,
                  },
              });
            }
        }
      catch (std::range_error &e)
        {
          p_This->ptrLogger->logMessage ("Range error => " + std::string (e.what ()));
        }
      catch (std::filesystem::filesystem_error &e)
        {
          p_This->ptrLogger->logMessage ("Directory error => " + std::string (e.what ()));
        }

      ImGui::Spacing ();
      ImGui::NewLine ();

      ImGui::BeginChild ("ContentRegion");

      ImGui::BeginGroup ();

      // Display directory entries, color coded...
      // Red for directory
      // White for file
      p_This->outputSelectableList (
          filesInDirectory, p_This->fileSystem.currentDirectory, gui::ListType::eMapFiles);

      ImGui::EndGroup ();

      ImGui::EndChild ();

      ImGui::EndPopup ();
    }
  else
    {
      p_This->hasFocus = false;
      getModal<FileMenu> (p_This->GuiModals).openModal.isOpen = false;
      p_This->show = noShow;
    }
  return;
}

void
showSaveDialog (GuiHandler *p_This)
{
  p_This->ptrLogger->logMessage ("Not implemented yet");
  p_This->releaseFocus ();
  return;
}

void
showQuitDialog (GuiHandler *p_This)
{
  if (!ImGui::IsPopupOpen ("Are you sure?",
                           ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    {
      getModal<FileMenu> (p_This->GuiModals).quitModal.isOpen = true;
      auto viewport = ImGui::GetMainViewport ();
      auto center = viewport->GetCenter ();
      center.x -= 125;
      center.y -= 200;
      const auto min = ImVec2 (200, 50);
      const auto max = ImVec2 (200, 50);
      ImGui::SetNextWindowSizeConstraints (min, max);
      ImGui::SetNextWindowSize (max, ImGuiPopupFlags_NoOpenOverExistingPopup);
      ImGui::SetNextWindowPos (center);
      ImGui::OpenPopup ("Are you sure?", ImGuiPopupFlags_NoOpenOverExistingPopup);
    }

  if (ImGui::BeginPopupModal ("Are you sure?",
                              &getModal<FileMenu> (p_This->GuiModals).quitModal.isOpen,
                              getModal<FileMenu> (p_This->GuiModals).quitModal.flags))
    {
      using enum GuiHandler::FocusModal;
      p_This->grabFocus (eQuit);

      if (ImGui::Button ("Cancel"))
        {
          p_This->releaseFocus ();
          ImGui::CloseCurrentPopup ();
        }

      ImGui::SameLine ();

      ImGui::Dummy (ImVec2 (75, 0));

      ImGui::SameLine ();

      if (ImGui::Button ("Quit"))
        {
          p_This->releaseFocus ();
          ImGui::CloseCurrentPopup ();
          glfwSetWindowShouldClose (p_This->window, GLFW_TRUE);
        }

      ImGui::EndPopup ();
    }
  else
    {
      p_This->hasFocus = false;
    }
}

void
selectNewTerrainFile (GuiHandler *p_This)
{
  if (!ImGui::IsPopupOpen ("Select terrain file",
                           ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    {
      getModal<MapModal> (p_This->GuiModals).selectTerrainModal.isOpen = true;
      ImGui::SetNextWindowSize (ImVec2 (800, 600));
      ImGui::OpenPopup ("Select terrain file", ImGuiPopupFlags_NoOpenOverExistingPopup);
    }

  if (ImGui::BeginPopupModal (
          "Select terrain file",
          &getModal<MapModal> (p_This->GuiModals).selectTerrainModal.isOpen,
          getModal<MapModal> (p_This->GuiModals).selectTerrainModal.windowFlags))
    {
      p_This->hasFocus = true;

      // Split current path for terrain modal
      std::vector<std::pair<int, std::string>> directoryButtons = splitPath (
          getModal<MapModal> (p_This->GuiModals).selectTerrainModal.currentDirectory.string ());

      // Output split path as buttons
      auto outputSplitPathAsButtons = [&] (std::pair<int, std::string> p_Directory) {
        if (ImGui::Button (p_Directory.second.c_str ()))
          {
            std::string newDirectory;
            for (int i = 0; i <= p_Directory.first; i++)
              {
                newDirectory.append (directoryButtons.at (i).second + "/");
              }
            getModal<MapModal> (p_This->GuiModals).selectTerrainModal.currentDirectory
                = "/" + newDirectory;
          }
        ImGui::SameLine ();
        ImGui::Text ("/");
        ImGui::SameLine ();
      };
      std::for_each (directoryButtons.begin (), directoryButtons.end (), outputSplitPathAsButtons);

      std::vector<std::pair<gui::FileSystem::FileType, std::pair<std::string, bool>>>
          filesInDirectory;

      // Get directory entries in current path
      try
        {
          auto directoryIterator = std::filesystem::directory_iterator (
              getModal<MapModal> (p_This->GuiModals).selectTerrainModal.currentDirectory);
          for (const auto &file : directoryIterator)
            {
              gui::FileSystem::FileType entryType;
              try
                {
                  if (file.is_directory ())
                    {
                      entryType = gui::FileSystem::FileType::eDirectory;
                    }
                  else if (file.is_regular_file ())
                    {
                      entryType = gui::FileSystem::FileType::eFile;
                    }
                }
              catch (std::filesystem::filesystem_error &e)
                {
                  p_This->ptrLogger->logMessage ("File system error => " + std::string (e.what ()));
                }

              std::string filePath = file.path ().string ();

              std::string filename
                  = filePath.replace (0,
                                      getModal<MapModal> (p_This->GuiModals)
                                          .selectTerrainModal.currentDirectory.string ()
                                          .length (),
                                      "");

              // Remove slash if it exists
              auto slash = filename.find ('/');
              if (slash != std::string::npos)
                filename.replace (slash, 1, "");

              // If hidden file/directory, skip
              if (filename.at (0) == '.')
                continue;

              // get extension
              // our terrain files will be .ter (just a png normal
              // file)
              auto extStart = filename.find ('.');
              if (entryType == gui::FileSystem::FileType::eFile)
                {
                  if (extStart == std::string::npos)
                    {
                      continue;
                    }
                  else
                    {
                      auto ext = filename.substr (extStart, filename.length () - 1);
                      if (ext != ".png")
                        continue;
                    }
                }

              // { TYPE, { NAME, ISSELECTED } }
              filesInDirectory.push_back ({
                  entryType,
                  {
                      filename,
                      false,
                  },
              });
            }
        }
      catch (std::range_error &e)
        {
          p_This->ptrLogger->logMessage ("Range error => " + std::string (e.what ()));
        }
      catch (std::filesystem::filesystem_error &e)
        {
          p_This->ptrLogger->logMessage ("Directory error => " + std::string (e.what ()));
        }

      ImGui::Spacing ();
      ImGui::NewLine ();

      ImGui::BeginChild ("ContentRegion");

      ImGui::BeginGroup ();

      // Display directory entries, color coded...
      // Red for directory
      // White for file
      p_This->outputSelectableList (
          filesInDirectory,
          getModal<MapModal> (p_This->GuiModals).selectTerrainModal.currentDirectory,
          gui::ListType::eHeightMaps);

      ImGui::EndGroup ();

      ImGui::EndChild ();

      ImGui::EndPopup ();
    }
  else
    {
      p_This->hasFocus = false;
      p_This->show = noShow;
      getModal<MapModal> (p_This->GuiModals).selectTerrainModal.isOpen = false;
    }
}

template <typename F, typename T>
F
handleInputThunk (T t)
{
  auto parent = static_cast<GuiHandler *> (t->UserData);
  parent->handleInput (t);
  return (F)0;
}

void
GuiHandler::renderCameraModal (void)
{
  auto viewport = ImGui::GetMainViewport ();
  auto pos = viewport->Size;
  pos.x -= getModal<CameraModal> (GuiModals).width;
  pos.y = menuBarHeight + getModal<CameraModal> (GuiModals).height;
  ImGui::SetNextWindowPos (pos);
  ImGui::SetNextWindowSize (
      ImVec2 (getModal<CameraModal> (GuiModals).width, getModal<CameraModal> (GuiModals).height));
  ImGui::Begin ("Camera", nullptr, getModal<CameraModal> (GuiModals).windowFlags);

  ImGui::Text ("Camera Position");
  ImGui::Spacing ();

  ImGui::Text ("X: ");
  ImGui::SameLine ();
  if (ImGui::InputText ("###XINPUT",
                        &getModal<CameraModal> (GuiModals).xInputBuffer,
                        getModal<CameraModal> (GuiModals).inputFlags,
                        handleInputThunk,
                        this))
    {
      hasFocus = false;
      // Parse input as uint32_t
      try
        {
          auto pos = Helper::stouint32 (getModal<CameraModal> (GuiModals).xInputBuffer);
          ptrCamera->position.x = pos;
        }
      catch (std::exception &e)
        {
          std::cout << "Could not parse input => " << std::string (e.what ()) << "\n";
        }
    }
  // If not focused, update X pos
  if (!ImGui::IsItemActive ())
    getModal<CameraModal> (GuiModals).xInputBuffer = std::to_string (ptrCamera->position.x);
  else
    hasFocus = true;
  ImGui::Spacing ();

  ImGui::Text ("Y: ");
  ImGui::SameLine ();
  if (ImGui::InputText ("###YINPUT",
                        &getModal<CameraModal> (GuiModals).yInputBuffer,
                        getModal<CameraModal> (GuiModals).inputFlags,
                        handleInputThunk,
                        this))
    {
      hasFocus = false;
      try
        {
          auto pos = Helper::stouint32 (getModal<CameraModal> (GuiModals).yInputBuffer);
          ptrCamera->position.y = pos;
        }
      catch (std::exception &e)
        {
          std::cout << "Could not parse input => " << std::string (e.what ()) << "\n";
        }
    }
  if (!ImGui::IsItemActive ())
    getModal<CameraModal> (GuiModals).yInputBuffer = std::to_string (ptrCamera->position.y);
  else
    hasFocus = true;
  ImGui::Spacing ();

  ImGui::Text ("Z: ");
  ImGui::SameLine ();
  if (ImGui::InputText ("###ZINPUT",
                        &getModal<CameraModal> (GuiModals).zInputBuffer,
                        getModal<CameraModal> (GuiModals).inputFlags,
                        handleInputThunk,
                        this))
    {
      hasFocus = false;
      try
        {
          auto pos = Helper::stouint32 (getModal<CameraModal> (GuiModals).zInputBuffer);
          ptrCamera->position.z = pos;
        }
      catch (std::exception &e)
        {
          std::cout << "Could not parse input => " << std::string (e.what ()) << "\n";
        }
    }
  if (!ImGui::IsItemActive ())
    getModal<CameraModal> (GuiModals).zInputBuffer = std::to_string (ptrCamera->position.z);
  else
    hasFocus = true;
  ImGui::End ();
  return;
} // end asset modal

void
GuiHandler::renderDebugModal (DebugMessageList p_DebugMessageList)
{
  // Terrain info
  auto viewport = ImGui::GetMainViewport ();
  auto pos = viewport->Size;
  pos.x -= getModal<DebugModal> (GuiModals).width;

  if (getModal<DebugModal> (GuiModals).isOpen)
    {
      pos.y -= getModal<DebugModal> (GuiModals).height;
      ImGui::SetNextWindowSize (
          ImVec2 (getModal<DebugModal> (GuiModals).width, getModal<DebugModal> (GuiModals).height));
    }
  else
    {
      pos.y -= 32;
      ImGui::SetNextWindowSize (ImVec2 (getModal<DebugModal> (GuiModals).width, 1));
    }
  ImGui::PushStyleVar (ImGuiStyleVar_FramePadding, ImVec2 (0, 8));
  ImGui::SetNextWindowPos (pos);

  // If collapsed, skip
  if (!ImGui::Begin ("Debug Console", nullptr, getModal<DebugModal> (GuiModals).windowFlags))
    {
      hasFocus = false;
      getModal<DebugModal> (GuiModals).isOpen = false;
      ImGui::PopStyleVar ();
      ImGui::End ();

      if (ImGui::IsWindowHovered ())
        hover = true;
      else
        hover = false;
      return;
    }
  if (ImGui::IsWindowHovered ())
    hover = true;
  else
    hover = false;
  ImGui::PopStyleVar ();
  // grabFocus (eNone);
  getModal<DebugModal> (GuiModals).isOpen = true;

  // Display all received debug messages
  for (const auto &message : p_DebugMessageList)
    {
      if (message.first == LogHandlerMessagePriority::eError)
        {
          ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (1.0f, 0.10f, 0.0f, 1.0f));
          ImGui::Text ("[-] %s", message.second.c_str ());
          ImGui::PopStyleColor ();
        }
      else if (message.first == LogHandlerMessagePriority::eWarning)
        {
          ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (1.0f, 0.5f, 0.0f, 1.0f));
          ImGui::Text ("[!] %s", message.second.c_str ());
          ImGui::PopStyleColor ();
        }
      else
        {
          ImGui::Text (" %s", message.second.c_str ());
        }
    }
  // Check if scrolled to bottom
  if (ImGui::GetScrollY () == ImGui::GetScrollMaxY ())
    getModal<DebugModal> (GuiModals).autoScroll = true;
  else
    getModal<DebugModal> (GuiModals).autoScroll = false;

  if (getModal<DebugModal> (GuiModals).autoScroll)
    ImGui::SetScrollHereY (1.0f);

  ImGui::End ();
} // end debug console

inline void
GuiHandler::grabFocus (FocusModal p_WhichModal)
{
  hasFocus = true;
  switch (p_WhichModal)
    {
    case eNone:
      {
        hasFocus = true; // just block game loop kbd input
        break;
      }
    case eOpenModal:
      {
        hasFocus = true;
        getModal<FileMenu> (GuiModals).openModal.isOpen = true;
        break;
      }
    case eQuit:
      {
        hasFocus = true;
        getModal<FileMenu> (GuiModals).quitModal.isOpen = true;
        break;
      }
    }
  return;
}

inline void
GuiHandler::releaseFocus (void)
{
  hasFocus = false;
  show = noShow;
  return;
}

std::vector<std::pair<int, std::string>>
splitPath (std::string p_Path)
{
  int buttonIndex = 0;
  std::vector<std::pair<int, std::string>> directoryButtons;
  std::istringstream stream (p_Path);

  std::string token;

  while (std::getline (stream, token, '/'))
    {
      if (token.size () > 0)
        {
          directoryButtons.push_back ({
              buttonIndex,
              token,
          });
          buttonIndex++;
        }
    }
  return directoryButtons;
}