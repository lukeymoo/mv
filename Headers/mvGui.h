#pragma once

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
// clang-format on

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <unordered_map>

#include "mvHelper.h"
#include "mvSwap.h"
#include "mvMisc.h"


struct Camera;

class GuiHandler
{
  public:
    GuiHandler(GLFWwindow *p_GLFWwindow, Camera *p_CameraHandler, const vk::Instance &p_Instance,
               const vk::PhysicalDevice &p_PhysicalDevice, const vk::Device &p_LogicalDevice,
               const Swap &p_MvSwap, const vk::CommandPool &p_CommandPool,
               const vk::Queue &p_GraphicsQueue,
               std::unordered_map<RenderPassType, vk::RenderPass> &p_RenderPassMap,
               const vk::DescriptorPool &p_DescriptorPool);
    ~GuiHandler();

    bool hasFocus = false;
    bool hover = false;

  private:
    GLFWwindow *window;
    std::chrono::_V2::system_clock::time_point lastDeltaUpdate =
        std::chrono::high_resolution_clock::now();

    float storedFrameDelta = 0.0f;
    float storedRenderDelta = 0.0f;
    int storedFPS = 0;
    int displayFPS = 0;
    int frameCount = 0;

    LogHandler *ptrLogger;
    Camera *ptrCamera;

    struct AssetModal
    {
        const int width = 300;
        const int height = 100;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;
    } assetModal;

    struct DebugModal
    {
        const int width = 600;
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
            // clang-format off
                std::unordered_map<CameraItem::Type, std::pair<std::string, bool>> typeMap = {
                    {eUndefined, {"Undefined", false}},
                    {eFreeLook, {"Free Look", false}},
                    {eFirstPerson, {"First Person", false}},
                    {eThirdPerson, {"Third Person", false}},
                    {eIsometric, {"Isometric", false}},
                };
            // clang-format on
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
            ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar |
                                           ImGuiWindowFlags_NoScrollWithMouse |
                                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        } selectTerrainModal;

        const int width = 300;
        const int height = 100;
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
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoScrollWithMouse |
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        } saveAsModal;

        struct QuitModal
        {
            bool isOpen = false;
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoScrollWithMouse |
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        } quitModal;

    } fileMenu;

    const int menuBarHeight = 18;

    // Effectively a virtual function for which dialog modal to display
    void (*noShow)(GuiHandler *p_This) = [](GuiHandler *) {};
    void (*show)(GuiHandler *p_This) = noShow;

  public:
    ImGuiIO &getIO(void);
    void update(const vk::Extent2D &p_SwapExtent, float p_RenderDelta, float p_FrameDelta,
                uint32_t p_ModelCount, uint32_t p_ObjectCount, uint32_t p_VertexCount,
                uint32_t p_TriangleCount);

    std::vector<vk::Framebuffer> createFramebuffers(
        const vk::Device &p_LogicalDevice, const vk::RenderPass &p_GuiRenderPass,
        std::vector<struct SwapchainBuffer> &p_SwapchainBuffers, uint32_t p_SwapExtentWidth,
        uint32_t p_SwapExtentHeight);
    void newFrame(void);
    void renderFrame(void);

    void doRenderPass(const vk::RenderPass &p_RenderPass, const vk::Framebuffer &p_Framebuffer,
                      const vk::CommandBuffer &p_CommandBuffer, vk::Extent2D &p_RenderAreaExtent);

    void cleanup(const vk::Device &p_LogicalDevice);

  private:
    void createRenderPass(std::unordered_map<RenderPassType, vk::RenderPass> &p_RenderPassMap,
                          const vk::Device &p_LogicalDevice,
                          const vk::Format &p_AttachmentColorFormat);

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

    inline void outputSelectableList(
        std::vector<std::pair<FileSystem::FileType, std::pair<std::string, bool>>> &p_List,
        std::filesystem::path &p_TargetPath)
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
            ImGui::Selectable(file.second.first.c_str(), &file.second.second,
                              ImGuiSelectableFlags_AllowDoubleClick);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                if (file.first == FileSystem::FileType::eDirectory)
                    p_TargetPath.append(file.second.first + "/");
            }

            // if (ImGui::Button(file.second.c_str()))
            // {
            //     if (file.first == FileSystem::FileType::eDirectory)
            //     {
            //         p_This->fileSystem.currentDirectory.append(file.second
            //         + "/");
            //     }
            // }

            ImGui::PopStyleColor(2);
        }
        return;
    }

    void (*showOpenDialog)(GuiHandler *p_This) = [](GuiHandler *p_This) {
        if (!ImGui::IsPopupOpen("Load map file...",
                                ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            p_This->fileMenu.openModal.isOpen = true;
            ImGui::SetNextWindowSize(ImVec2(800, 600));
            ImGui::OpenPopup("Load map file...", ImGuiPopupFlags_NoOpenOverExistingPopup);
        }

        if (ImGui::BeginPopupModal("Load map file...", &p_This->fileMenu.openModal.isOpen,
                                   p_This->fileMenu.openModal.flags))
        {
            p_This->hasFocus = true;
            p_This->fileMenu.openModal.isOpen = true;

            // Split directory
            std::vector<std::pair<int, std::string>> directoryButtons =
                p_This->splitPath(p_This->fileSystem.currentDirectory.string());

            // Output directory path as buttons
            auto outputSplitPathAsButtons = [&](std::pair<int, std::string> p_Directory) {
                if (ImGui::Button(p_Directory.second.c_str()))
                {
                    std::string newDirectory;
                    for (int i = 0; i <= p_Directory.first; i++)
                    {
                        newDirectory.append(directoryButtons.at(i).second + "/");
                    }
                    p_This->fileSystem.currentDirectory = "/" + newDirectory;
                }
                ImGui::SameLine();
                ImGui::Text("/");
                ImGui::SameLine();
            };
            std::for_each(directoryButtons.begin(), directoryButtons.end(),
                          outputSplitPathAsButtons);

            std::vector<std::pair<FileSystem::FileType, std::pair<std::string, bool>>>
                filesInDirectory;

            // Get directory entries in current path
            try
            {
                auto directoryIterator =
                    std::filesystem::directory_iterator(p_This->fileSystem.currentDirectory);
                for (const auto &file : directoryIterator)
                {
                    FileSystem::FileType entryType;
                    try
                    {
                        if (file.is_directory())
                        {
                            entryType = FileSystem::FileType::eDirectory;
                        }
                        else if (file.is_regular_file())
                        {
                            entryType = FileSystem::FileType::eFile;
                        }
                    }
                    catch (std::filesystem::filesystem_error &e)
                    {
                        p_This->ptrLogger->logMessage("File system error => " +
                                                      std::string(e.what()));
                    }

                    std::string filePath = file.path().string();

                    std::string filename = filePath.replace(
                        0, p_This->fileSystem.currentDirectory.string().length(), "");

                    // Remove slash if it exists
                    auto slash = filename.find('/');
                    if (slash != std::string::npos)
                        filename.replace(slash, 1, "");

                    // If not hidden file or hidden directory
                    if (filename.at(0) == '.')
                        continue;

                    // get extension
                    // our map files will be .mvmap
                    auto extStart = filename.find('.');
                    if (entryType == FileSystem::FileType::eFile)
                    {
                        if (extStart == std::string::npos)
                        {
                            continue;
                        }
                        else
                        {
                            auto ext = filename.substr(extStart, filename.length() - 1);
                            if (ext != ".mvmap")
                                continue;
                        }
                    }

                    // { TYPE, { NAME, ISSELECTED } }
                    filesInDirectory.push_back({
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
                p_This->ptrLogger->logMessage("Range error => " + std::string(e.what()));
            }
            catch (std::filesystem::filesystem_error &e)
            {
                p_This->ptrLogger->logMessage("Directory error => " + std::string(e.what()));
            }

            ImGui::Spacing();
            ImGui::NewLine();

            ImGui::BeginChild("ContentRegion");

            ImGui::BeginGroup();

            // Display directory entries, color coded...
            // Red for directory
            // White for file
            p_This->outputSelectableList(filesInDirectory, p_This->fileSystem.currentDirectory);

            ImGui::EndGroup();

            ImGui::EndChild();

            ImGui::EndPopup();
        }
        else
        {
            p_This->hasFocus = false;
            p_This->fileMenu.openModal.isOpen = false;
            p_This->show = p_This->noShow;
        }
    };

    /*
      SAVE FILE DIALOG
    */
    void (*showSaveDialog)(GuiHandler *p_This) = [](GuiHandler *p_This) {
        p_This->ptrLogger->logMessage("Not implemented yet");
    };

    /*
      QUIT CONFIRM DIALOG
    */
    void (*showQuitDialog)(GuiHandler *p_This) = [](GuiHandler *p_This) {
        if (!ImGui::IsPopupOpen("Are you sure?",
                                ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            p_This->fileMenu.quitModal.isOpen = true;
            auto viewport = ImGui::GetMainViewport();
            auto center = viewport->GetCenter();
            center.x -= 125;
            center.y -= 200;
            const auto min = ImVec2(200, 50);
            const auto max = ImVec2(200, 50);
            ImGui::SetNextWindowSizeConstraints(min, max);
            ImGui::SetNextWindowSize(max, ImGuiPopupFlags_NoOpenOverExistingPopup);
            ImGui::SetNextWindowPos(center);
            ImGui::OpenPopup("Are you sure?", ImGuiPopupFlags_NoOpenOverExistingPopup);
        }

        if (ImGui::BeginPopupModal("Are you sure?", &p_This->fileMenu.quitModal.isOpen,
                                   p_This->fileMenu.quitModal.flags))
        {
            p_This->hasFocus = true;

            if (ImGui::Button("Cancel"))
            {
                p_This->hasFocus = false;
                p_This->fileMenu.quitModal.isOpen = false;
                p_This->show = p_This->noShow;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            ImGui::Dummy(ImVec2(75, 0));

            ImGui::SameLine();

            if (ImGui::Button("Quit"))
            {
                p_This->hasFocus = false;
                p_This->fileMenu.quitModal.isOpen = false;
                p_This->show = p_This->noShow;
                ImGui::CloseCurrentPopup();
                glfwSetWindowShouldClose(p_This->window, GLFW_TRUE);
            }

            ImGui::EndPopup();
        }
        else
        {
            p_This->hasFocus = false;
            p_This->fileMenu.quitModal.isOpen = false;
            p_This->show = p_This->noShow;
        }
    }; // end showQuitDialog

    void (*selectNewTerrainFile)(GuiHandler *p_This) = [](GuiHandler *p_This) {
        if (!ImGui::IsPopupOpen("Select terrain file",
                                ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        {
            p_This->mapModal.selectTerrainModal.isOpen = true;
            ImGui::SetNextWindowSize(ImVec2(800, 600));
            ImGui::OpenPopup("Select terrain file", ImGuiPopupFlags_NoOpenOverExistingPopup);
        }

        if (ImGui::BeginPopupModal("Select terrain file",
                                   &p_This->mapModal.selectTerrainModal.isOpen,
                                   p_This->mapModal.selectTerrainModal.windowFlags))
        {
            p_This->hasFocus = true;

            // Split current path for terrain modal
            std::vector<std::pair<int, std::string>> directoryButtons =
                p_This->splitPath(p_This->mapModal.selectTerrainModal.currentDirectory.string());

            // Output split path as buttons
            auto outputSplitPathAsButtons = [&](std::pair<int, std::string> p_Directory) {
                if (ImGui::Button(p_Directory.second.c_str()))
                {
                    std::string newDirectory;
                    for (int i = 0; i <= p_Directory.first; i++)
                    {
                        newDirectory.append(directoryButtons.at(i).second + "/");
                    }
                    p_This->mapModal.selectTerrainModal.currentDirectory = "/" + newDirectory;
                }
                ImGui::SameLine();
                ImGui::Text("/");
                ImGui::SameLine();
            };
            std::for_each(directoryButtons.begin(), directoryButtons.end(),
                          outputSplitPathAsButtons);

            std::vector<std::pair<FileSystem::FileType, std::pair<std::string, bool>>>
                filesInDirectory;

            // Get directory entries in current path
            try
            {
                auto directoryIterator = std::filesystem::directory_iterator(
                    p_This->mapModal.selectTerrainModal.currentDirectory);
                for (const auto &file : directoryIterator)
                {
                    FileSystem::FileType entryType;
                    try
                    {
                        if (file.is_directory())
                        {
                            entryType = FileSystem::FileType::eDirectory;
                        }
                        else if (file.is_regular_file())
                        {
                            entryType = FileSystem::FileType::eFile;
                        }
                    }
                    catch (std::filesystem::filesystem_error &e)
                    {
                        p_This->ptrLogger->logMessage("File system error => " +
                                                      std::string(e.what()));
                    }

                    std::string filePath = file.path().string();

                    std::string filename = filePath.replace(
                        0, p_This->mapModal.selectTerrainModal.currentDirectory.string().length(),
                        "");

                    // Remove slash if it exists
                    auto slash = filename.find('/');
                    if (slash != std::string::npos)
                        filename.replace(slash, 1, "");

                    // If hidden file/directory, skip
                    if (filename.at(0) == '.')
                        continue;

                    // get extension
                    // our terrain files will be .ter (just a png normal
                    // file)
                    auto extStart = filename.find('.');
                    if (entryType == FileSystem::FileType::eFile)
                    {
                        if (extStart == std::string::npos)
                        {
                            continue;
                        }
                        else
                        {
                            auto ext = filename.substr(extStart, filename.length() - 1);
                            if (ext != ".ter")
                                continue;
                        }
                    }

                    // { TYPE, { NAME, ISSELECTED } }
                    filesInDirectory.push_back({
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
                p_This->ptrLogger->logMessage("Range error => " + std::string(e.what()));
            }
            catch (std::filesystem::filesystem_error &e)
            {
                p_This->ptrLogger->logMessage("Directory error => " + std::string(e.what()));
            }

            ImGui::Spacing();
            ImGui::NewLine();

            ImGui::BeginChild("ContentRegion");

            ImGui::BeginGroup();

            // Display directory entries, color coded...
            // Red for directory
            // White for file
            p_This->outputSelectableList(filesInDirectory,
                                         p_This->mapModal.selectTerrainModal.currentDirectory);

            ImGui::EndGroup();

            ImGui::EndChild();

            ImGui::EndPopup();
        }
        else
        {
            p_This->hasFocus = false;
            p_This->show = p_This->noShow;
            p_This->mapModal.selectTerrainModal.isOpen = false;
        }
    };

  private:
    // Render main menu
    inline void renderMenu(void)
    {
        ImGui::BeginMainMenuBar();

        /*
            File Menu
        */
        if (ImGui::BeginMenu("File"))
        {
            /*
                OPEN FILE BUTTON
            */
            if (ImGui::MenuItem("Open", "Ctrl+O"))
            {
                show = showOpenDialog;
            }

            if (ImGui::MenuItem("Save", nullptr))
            {
                // Test if new file
            }

            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
            {
                show = showSaveDialog;
            }

            /*
                QUIT APPLICATION BUTTON
            */
            if (ImGui::MenuItem("Quit", "Ctrl+Q"))
            {
                // are you sure?
                show = showQuitDialog;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    } // end renderMenu

    // Render terrain modal
    inline void renderMapConfigModal(void);

    inline void renderAssetModal(void)
    {
        auto viewport = ImGui::GetMainViewport();
        auto pos = viewport->Size;
        pos.x -= assetModal.width;
        pos.y = mapModal.height + menuBarHeight;
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(ImVec2(assetModal.width, assetModal.height));
        ImGui::Begin("Assets", nullptr, assetModal.windowFlags);

        ImGui::End();
        return;
    } // end asset modal

    inline void renderDebugModal(
        std::vector<std::pair<LogHandler::MessagePriority, std::string>> p_DebugMessageList)
    {
        // Terrain info
        auto viewport = ImGui::GetMainViewport();
        auto pos = viewport->Size;
        pos.x -= debugModal.width;

        if (debugModal.isOpen)
        {
            pos.y -= debugModal.height;
            ImGui::SetNextWindowSize(ImVec2(debugModal.width, debugModal.height));
        }
        else
        {
            pos.y -= 32;
            ImGui::SetNextWindowSize(ImVec2(debugModal.width, 1));
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 8));
        ImGui::SetNextWindowPos(pos);

        // If collapsed, skip
        if (!ImGui::Begin("Debug Console", nullptr, debugModal.windowFlags))
        {
            debugModal.isOpen = false;
            ImGui::PopStyleVar();
            ImGui::End();

            if (ImGui::IsWindowHovered())
                hover = true;
            else
                hover = false;
            return;
        }
        if (ImGui::IsWindowHovered())
            hover = true;
        else
            hover = false;
        ImGui::PopStyleVar();
        debugModal.isOpen = true;

        // Display all received debug messages
        for (const auto &message : p_DebugMessageList)
        {
            if (message.first == LogHandler::MessagePriority::eError)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.10f, 0.0f, 1.0f));
                ImGui::Text("[-] %s", message.second.c_str());
                ImGui::PopStyleColor();
            }
            else if (message.first == LogHandler::MessagePriority::eWarning)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
                ImGui::Text("[!] %s", message.second.c_str());
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::Text(" %s", message.second.c_str());
            }
        }
        // Check if scrolled to bottom
        if (ImGui::GetScrollY() == ImGui::GetScrollMaxY())
            debugModal.autoScroll = true;
        else
            debugModal.autoScroll = false;

        if (debugModal.autoScroll)
            ImGui::SetScrollHereY(1.0f);

        ImGui::End();
    } // end debug console
};
