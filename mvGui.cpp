#include "mvGui.h"

// For interfacing with camera
#include "mvCamera.h"

// For interfacing with map handler
#include "mvMap.h"

extern LogHandler logger;

GuiHandler::GuiHandler (
    GLFWwindow *p_GLFWwindow, MapHandler *p_MapHandler,
    Camera *p_CameraHandler, const vk::Instance &p_Instance,
    const vk::PhysicalDevice &p_PhysicalDevice,
    const vk::Device &p_LogicalDevice, const Swap &p_MvSwap,
    const vk::CommandPool &p_CommandPool, const vk::Queue &p_GraphicsQueue,
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
        throw std::runtime_error (
            "Camera not initialized before debugger : Camera type is "
            "set to eInvalid");
        break;
      }
    case eFreeLook:
      {
        mapModal.cameraItem.selectedType
            = MapModal::CameraItem::Type::eFreeLook;
        mapModal.cameraItem.select ();
        break;
      }
    case eThirdPerson:
      {
        mapModal.cameraItem.selectedType
            = MapModal::CameraItem::Type::eThirdPerson;
        mapModal.cameraItem.select ();
        break;
      }
    case eFirstPerson:
      {
        mapModal.cameraItem.selectedType
            = MapModal::CameraItem::Type::eFirstPerson;
        mapModal.cameraItem.select ();
        break;
      }
    case eIsometric:
      {
        mapModal.cameraItem.selectedType
            = MapModal::CameraItem::Type::eIsometric;
        mapModal.cameraItem.select ();
        break;
      }
    }
  if (!p_GLFWwindow)
    throw std::runtime_error (
        "Invalid GLFW window handle passed to Gui handler");

  window = p_GLFWwindow;
  // Ensure gui not already in render pass map
  if (p_RenderPassMap.find (eImGui) != p_RenderPassMap.end ())
    {
      logger.logMessage (LogHandler::MessagePriority::eWarning,
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
  initInfo.MSAASamples
      = static_cast<VkSampleCountFlagBits> (vk::SampleCountFlagBits::e1);

  ImGui_ImplGlfw_InitForVulkan (p_GLFWwindow, true);
  ImGui_ImplVulkan_Init (&initInfo, p_RenderPassMap.at (eImGui));

  /*
    Upload ImGui render fonts
  */
  vk::CommandBuffer guiCommandBuffer
      = Helper::beginCommandBuffer (p_LogicalDevice, p_CommandPool);

  ImGui_ImplVulkan_CreateFontsTexture (guiCommandBuffer);

  Helper::endCommandBuffer (p_LogicalDevice, p_CommandPool, guiCommandBuffer,
                            p_GraphicsQueue);

  ImGui_ImplVulkan_DestroyFontUploadObjects ();
  return;
}

GuiHandler::~GuiHandler () { return; }

void
GuiHandler::createRenderPass (
    std::unordered_map<RenderPassType, vk::RenderPass> &p_RenderPassMap,
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
  guiDependencies[0].srcStageMask
      = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  guiDependencies[0].dstStageMask
      = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  guiDependencies[0].srcAccessMask
      = vk::AccessFlagBits::eColorAttachmentRead
        | vk::AccessFlagBits::eColorAttachmentWrite;
  guiDependencies[0].dstAccessMask
      = vk::AccessFlagBits::eColorAttachmentRead
        | vk::AccessFlagBits::eColorAttachmentWrite;
  guiDependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

  vk::RenderPassCreateInfo guiRenderPassInfo;
  guiRenderPassInfo.attachmentCount
      = static_cast<uint32_t> (guiAttachments.size ());
  guiRenderPassInfo.pAttachments = guiAttachments.data ();
  guiRenderPassInfo.subpassCount
      = static_cast<uint32_t> (guiSubpasses.size ());
  guiRenderPassInfo.pSubpasses = guiSubpasses.data ();
  guiRenderPassInfo.dependencyCount
      = static_cast<uint32_t> (guiDependencies.size ());
  guiRenderPassInfo.pDependencies = guiDependencies.data ();

  vk::RenderPass newPass
      = p_LogicalDevice.createRenderPass (guiRenderPassInfo);

  p_RenderPassMap.insert ({
      eImGui,
      std::move (newPass),
  });

  return;
}

std::vector<vk::Framebuffer>
GuiHandler::createFramebuffers (
    const vk::Device &p_LogicalDevice, const vk::RenderPass &p_GuiRenderPass,
    std::vector<struct SwapchainBuffer> &p_SwapchainBuffers,
    uint32_t p_SwapExtentWidth, uint32_t p_SwapExtentHeight)
{
  std::vector<vk::Framebuffer> framebuffers;

  // Attachment for ImGui rendering
  std::array<vk::ImageView, 1> imguiAttachments;

  // ImGui must only have 1 attachment
  vk::FramebufferCreateInfo guiFrameInfo;
  guiFrameInfo.renderPass = p_GuiRenderPass;
  guiFrameInfo.attachmentCount
      = static_cast<uint32_t> (imguiAttachments.size ());
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
  vk::RenderPassBeginInfo guiPassInfo;
  guiPassInfo.renderPass = p_RenderPass;
  guiPassInfo.framebuffer = p_Framebuffer;
  guiPassInfo.renderArea.offset.x = 0;
  guiPassInfo.renderArea.offset.y = 0;
  guiPassInfo.renderArea.extent.width = p_RenderAreaExtent.width;
  guiPassInfo.renderArea.extent.height = p_RenderAreaExtent.height;

  p_CommandBuffer.beginRenderPass (guiPassInfo, vk::SubpassContents::eInline);

  ImGui_ImplVulkan_RenderDrawData (ImGui::GetDrawData (), p_CommandBuffer);

  p_CommandBuffer.endRenderPass ();
  return;
}

void
GuiHandler::update (const vk::Extent2D &p_SwapExtent, float p_RenderDelta,
                    float p_FrameDelta, uint32_t p_ModelCount,
                    uint32_t p_ObjectCount, uint32_t p_VertexCount,
                    uint32_t p_TriangleCount)
{
  /*
      Determine if should update engine status deltas
  */
  auto updateEnd = std::chrono::high_resolution_clock::now ();
  float timeSince = std::chrono::duration<float, std::ratio<1L, 1L>> (
                        updateEnd - lastDeltaUpdate)
                        .count ();
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
      if (getIO ().KeysDown[GLFW_KEY_LEFT_SHIFT]
          || getIO ().KeysDown[GLFW_KEY_RIGHT_SHIFT])
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

  // Render asset manager
  renderAssetModal ();

  // display popup menu callback if any
  show (this);

  /*
      Engine status data
  */
  ImGuiWindowFlags engineDataFlags = ImGuiWindowFlags_NoResize
                                     | ImGuiWindowFlags_NoMove
                                     | ImGuiWindowFlags_NoTitleBar;
  ImGui::SetNextWindowPos (ImVec2 (0, p_SwapExtent.height - 32));
  ImGui::SetNextWindowSize (
      ImVec2 (p_SwapExtent.width - debugModal.width, 32));
  ImGui::Begin ("Status", nullptr, engineDataFlags);
  ImGui::Text ("Render time: %.2f ms | Frame time: %.2f ms | FPS: %i | Model "
               "Count: %i | Object Count: %i | Vertex "
               "Count: %i | Triangle Count: %i",
               storedRenderDelta, storedFrameDelta, displayFPS, p_ModelCount,
               p_ObjectCount, p_VertexCount, p_TriangleCount);
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
GuiHandler::renderMapConfigModal (void)
{
  // Terrain info
  auto viewport = ImGui::GetMainViewport ();
  auto pos = viewport->Size;
  pos.x -= mapModal.width;
  pos.y = menuBarHeight;
  ImGui::SetNextWindowPos (pos);
  ImGui::SetNextWindowSize (ImVec2 (mapModal.width, mapModal.height));

  ImGui::Begin ("Map Details", nullptr, mapModal.windowFlags);

  /*
      Selected terrain map
  */
  ImGui::Text ("Terrain file: ");
  ImGui::SameLine ();
  ImGui::Selectable (mapModal.terrainItem.filename.c_str (),
                     &mapModal.terrainItem.isSelected,
                     mapModal.terrainItem.flags);
  if (ImGui::IsItemHovered ())
    {
      if (ImGui::IsMouseDoubleClicked (0))
        show = selectNewTerrainFile;
    }
  else
    {
      mapModal.terrainItem.isSelected = false;
    }

  /*
      Camera configuration
  */
  ImGui::Spacing ();
  ImGui::Text ("Camera style");
  ImGui::SameLine ();

  ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (1.0f, 0.75f, 0.0f, 1.0f));
  if (ImGui::TreeNode (
          mapModal.cameraItem.typeMap.at (mapModal.cameraItem.selectedType)
              .first.c_str ()))
    {
      using CameraItemType = GuiHandler::MapModal::CameraItem::Type;

      ImGui::Dummy (ImVec2 (64, 1));
      ImGui::SameLine ();

      // FREE LOOK
      if (ImGui::MenuItem (
              mapModal.cameraItem.getTypeName (CameraItemType::eFreeLook)
                  .c_str (),
              nullptr,
              mapModal.cameraItem.getIsSelectedRef (CameraItemType::eFreeLook),
              !mapModal.cameraItem.getIsSelected (CameraItemType::eFreeLook)))
        {
          if (ptrCamera->setCameraType (
                  CameraType::eFreeLook,
                  glm::vec3 (ptrCamera->position.x,
                             ptrCamera->position.y - 2.0f,
                             ptrCamera->position.z)))
            {
              mapModal.cameraItem.deselectAllBut (CameraItemType::eFreeLook);
              mapModal.cameraItem.selectedType
                  = MapModal::CameraItem::Type::eFreeLook;
            }
          else
            {
              mapModal.cameraItem.deselectAllBut (
                  mapModal.cameraItem.selectedType);
            }
        }

      ImGui::Dummy (ImVec2 (64, 1));
      ImGui::SameLine ();

      // THIRD PERSON
      if (ImGui::MenuItem (
              mapModal.cameraItem.getTypeName (CameraItemType::eThirdPerson)
                  .c_str (),
              nullptr,
              mapModal.cameraItem.getIsSelectedRef (
                  CameraItemType::eThirdPerson),
              !mapModal.cameraItem.getIsSelected (
                  CameraItemType::eThirdPerson)))
        {
          if (ptrCamera->setCameraType (CameraType::eThirdPerson,
                                        { 0.0f, 0.0f, 0.0f }))
            {
              std::cout << "switching to third person\n";
              mapModal.cameraItem.deselectAllBut (
                  CameraItemType::eThirdPerson);
              mapModal.cameraItem.selectedType
                  = MapModal::CameraItem::Type::eThirdPerson;
            }
          else
            {
              std::cout << "switching to free look\n";
              mapModal.cameraItem.deselectAllBut (
                  mapModal.cameraItem.selectedType);
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
  mapModal.selectTerrainModal.isOpen = false;

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
  mapModal.selectTerrainModal.isOpen = false;

  // Load heightmap
  ptrMapHandler->readHeightMap (this, p_Path + p_Filename);

  return;
}