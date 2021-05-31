#pragma once

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "imgui-1.82/misc/cpp/imgui_stdlib.h"
// clang-format on

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "mvMisc.h"
#include "mvSwap.h"

struct Camera;
class MapHandler;
class LogHandler;
enum class LogHandlerMessagePriority;

class GuiHandler;
extern void noShow(GuiHandler *);
extern void showOpenDialog(GuiHandler *);
extern void showSaveDialog(GuiHandler *);
extern void showQuitDialog(GuiHandler *);
extern void selectNewTerrainFile(GuiHandler *);

template <typename T>
constexpr auto type_name() noexcept
{
    std::string_view name = "Error: unsupported compiler", prefix, suffix;
#ifdef __clang__
    name = __PRETTY_FUNCTION__;
    prefix = "auto type_name() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name = __PRETTY_FUNCTION__;
    prefix = "constexpr auto type_name() [with T = ";
    suffix = "]";
#elif defined(_MSC_VER)
    name = __FUNCSIG__;
    prefix = "auto __cdecl type_name<";
    suffix = ">(void) noexcept";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

template <typename F, typename T>
F handleInputThunk(T t);

class GuiHandler
{
  public:
    GuiHandler(GLFWwindow *p_GLFWwindow, MapHandler *p_MapHandler, Camera *p_CameraHandler,
               const vk::Instance &p_Instance, const vk::PhysicalDevice &p_PhysicalDevice,
               const vk::Device &p_LogicalDevice, const Swap &p_MvSwap, const vk::CommandPool &p_CommandPool,
               const vk::Queue &p_GraphicsQueue, std::unordered_map<RenderPassType, vk::RenderPass> &p_RenderPassMap,
               const vk::DescriptorPool &p_DescriptorPool);
    ~GuiHandler();

    bool hasFocus = false;
    bool hover = false;

    // Set the selectedTerrainItem to specified name
    inline void setLoadedTerrainFile(std::string p_Filename)
    {
        // store full path
        mapModal.terrainItem.fullPath = p_Filename;

        // use only base filename
        size_t basePos = p_Filename.find('/');
        do
        {
            p_Filename.erase(0, basePos + 1);
        } while ((basePos = p_Filename.find('/')) != std::string::npos);

        mapModal.terrainItem.filename = p_Filename;
        return;
    }

  public:
    GLFWwindow *window = nullptr;
    std::chrono::_V2::system_clock::time_point lastDeltaUpdate = std::chrono::high_resolution_clock::now();

    float storedFrameDelta = 0.0f;
    float storedRenderDelta = 0.0f;
    int storedFPS = 0;
    int displayFPS = 0;
    int frameCount = 0;

    LogHandler *ptrLogger = nullptr;
    Camera *ptrCamera = nullptr;
    MapHandler *ptrMapHandler = nullptr;

    enum ListType
    {
        eHeightMaps = 0,
        eMapFiles,
    };

    struct CameraModal
    {
        const int width = 300;
        const int height = 100;
        std::string xInputBuffer;
        std::string yInputBuffer;
        void *userData;
        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_EnterReturnsTrue |
                                         ImGuiInputTextFlags_CallbackCharFilter;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;
    } cameraModal;

    struct DebugModal
    {
        const int width = 800;
        const int height = 600;
        bool isOpen = true;
        bool autoScroll = true;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    } debugModal;

    struct MapModal
    {
        struct
        {
            bool isSelected = false;
            std::string filename = "None";
            std::string fullPath = "None";
            ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;
        } terrainItem; // Selectable flags

        struct CameraItem
        {
            enum Type
            {
                eUndefined,
                eFreeLook,
                eFirstPerson,
                eThirdPerson,
                eIsometric,
            };
            // { TYPE, { ITEM TEXT, SELECTED? } }
            std::unordered_map<CameraItem::Type, std::pair<std::string, bool>> typeMap = {
                {eUndefined, {"Undefined", false}},      {eFreeLook, {"Free Look", false}},
                {eFirstPerson, {"First Person", false}}, {eThirdPerson, {"Third Person", false}},
                {eIsometric, {"Isometric", false}},
            };
            Type selectedType = eUndefined;

            inline std::string getTypeName(Type p_EnumType)
            {
                return typeMap.at(p_EnumType).first;
            }

            inline bool getIsSelected(Type p_EnumType)
            {
                return typeMap.at(p_EnumType).second;
            }

            inline bool *getIsSelectedRef(Type p_EnumType)
            {
                return &typeMap.at(p_EnumType).second;
            }
            inline void select(Type p_EnumType)
            {
                deselectAllBut(p_EnumType);
                for (auto &pair : typeMap)
                {
                    if (pair.first == p_EnumType)
                        pair.second.second = true;
                }
                return;
            }
            inline void select(void)
            {
                deselectAllBut(selectedType);
                for (auto &pair : typeMap)
                {
                    if (pair.first == selectedType)
                        pair.second.second = true;
                }
                return;
            }
            inline void deselectAll(void)
            {
                for (auto &pair : typeMap)
                {
                    pair.second.second = false;
                }
                return;
            }
            inline void deselectAllBut(Type p_EnumType)
            {
                for (auto &pair : typeMap)
                {
                    if (pair.first == p_EnumType)
                        continue;
                    pair.second.second = false;
                }
            }
        } cameraItem;

        struct
        {
            bool isOpen;
            std::filesystem::path currentDirectory = std::filesystem::current_path();
            ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        } selectTerrainModal;

        const int width = 300;
        const int height = 150;
        // Parent modal
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;
    } mapModal;

    struct FileSystem
    {
        enum FileType
        {
            eFile = 0,
            eDirectory
        };
        std::filesystem::path currentDirectory = std::filesystem::current_path();

        std::vector<std::pair<FileType, std::string>> listDirectory(std::filesystem::path p_Path);
    } fileSystem;

    struct FileMenu
    {
        struct OpenModal
        {
            bool isOpen = false;
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove;
        } openModal;

        struct SaveAsModal
        {
            bool isOpen = false;
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        } saveAsModal;

        struct QuitModal
        {
            bool isOpen = false;
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        } quitModal;

    } fileMenu;

    const int menuBarHeight = 18;

    struct ModalFunction
    {
        void (*toExec)(GuiHandler *p_This) = noShow;

      public:
        // Display no modal
        void operator()(GuiHandler *p_This)
        {
            toExec(p_This);
            return;
        }

        // For testing which child is `in use.`
        template <typename P>
        bool operator==(P *ptr) const
        {
            if (toExec == ptr)
            {
                return true;
            }
            return false;
        }

        // Allow copy assignment
        template <typename P>
        ModalFunction operator=(P *ptr);
    };
    ModalFunction show;

  public:
    ImGuiIO &getIO(void);
    void update(const vk::Extent2D &p_SwapExtent, float p_RenderDelta, float p_FrameDelta, uint32_t p_ModelCount,
                uint32_t p_ObjectCount, uint32_t p_VertexCount);

    std::vector<vk::Framebuffer> createFramebuffers(const vk::Device &p_LogicalDevice,
                                                    const vk::RenderPass &p_GuiRenderPass,
                                                    std::vector<struct SwapchainBuffer> &p_SwapchainBuffers,
                                                    uint32_t p_SwapExtentWidth, uint32_t p_SwapExtentHeight);
    void newFrame(void);
    void renderFrame(void);

    void doRenderPass(const vk::RenderPass &p_RenderPass, const vk::Framebuffer &p_Framebuffer,
                      const vk::CommandBuffer &p_CommandBuffer, vk::Extent2D &p_RenderAreaExtent);

    void cleanup(const vk::Device &p_LogicalDevice);

    // Should only call if recreating swapchain
    // Do not call outside of that special event as this function
    // is automatically called by gui constructor
    void createRenderPass(std::unordered_map<RenderPassType, vk::RenderPass> &p_RenderPassMap,
                          const vk::Device &p_LogicalDevice, const vk::Format &p_AttachmentColorFormat);

    // Generate mesh from PNG image or load existing mesh files if exists
    void loadHeightMap(std::string p_Path, std::string p_Filename);

    // Load .map file which contains every from terrain data, to objects & their
    // placements/rotations
    void loadMapFile(std::string p_Path, std::string p_Filename);

    // Handles input for camera modal
    template <typename T>
    inline void handleInput(T t)
    {
        if (t->EventFlag & ImGuiInputTextFlags_CallbackCharFilter)
        {
            std::cout << "Testing char code => " << std::to_string(t->EventChar) << "\n";

            if (t->EventChar < 48 || t->EventChar > 57)
            {
                t->EventChar = 0;
                std::cout << "Only numerical characters allowed\n";
            }
        }
        return;
    }

  public:
    /*
      OPEN FILE DIALOG
    */
    inline std::vector<std::pair<int, std::string>> splitPath(std::string p_Path)
    {
        int buttonIndex = 0;
        std::vector<std::pair<int, std::string>> directoryButtons;
        std::istringstream stream(p_Path);

        std::string token;

        while (std::getline(stream, token, '/'))
        {
            if (token.size() > 0)
            {
                directoryButtons.push_back({
                    buttonIndex,
                    token,
                });
                buttonIndex++;
            }
        }
        return directoryButtons;
    }

    inline void outputSelectableList(std::vector<std::pair<FileSystem::FileType, std::pair<std::string, bool>>> &p_List,
                                     std::filesystem::path &p_TargetPath, ListType p_ListType)
    {
        for (auto &file : p_List)
        {
            if (file.first == FileSystem::FileType::eFile)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            }
            else if (file.first == FileSystem::FileType::eDirectory)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 0.5f));

            // If user clicks entry & is directory; append to path
            ImGui::Selectable(file.second.first.c_str(), &file.second.second, ImGuiSelectableFlags_AllowDoubleClick);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                if (file.first == FileSystem::FileType::eDirectory)
                {
                    p_TargetPath.append(file.second.first + "/");
                }
                else if (file.first == FileSystem::FileType::eFile)
                {
                    // Select appropriate action
                    if (p_ListType == ListType::eHeightMaps)
                    {
                        loadHeightMap(p_TargetPath, file.second.first);
                    }
                    else if (p_ListType == ListType::eMapFiles)
                    {
                        loadMapFile(p_TargetPath, file.second.first);
                    }
                }
            }

            ImGui::PopStyleColor(2);
        }
        return;
    }

  private:
    // Render main menu
    inline void renderMenu(void);

    // Render terrain modal
    void renderMapConfigModal(void);

    void renderCameraModal(void);

    using DebugMessageList = std::vector<std::pair<LogHandlerMessagePriority, std::string>>;
    void renderDebugModal(DebugMessageList p_DebugMessageList);
};
