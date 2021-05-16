#ifndef HEADERS_MVGUI_H_
#define HEADERS_MVGUI_H_

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
// clang-format on

#include <iostream>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <algorithm>

#include "mvHelper.h"
#include "mvSwap.h"

namespace mv
{
    class GuiHandler
    {
      public:
        GuiHandler(GLFWwindow *p_GLFWwindow, const vk::Instance &p_Instance, const vk::PhysicalDevice &p_PhysicalDevice,
                   const vk::Device &p_LogicalDevice, const mv::Swap &p_MvSwap, const vk::CommandPool &p_CommandPool,
                   const vk::Queue &p_GraphicsQueue, std::unordered_map<std::string, vk::RenderPass> &p_RenderPassMap,
                   const vk::DescriptorPool &p_DescriptorPool);
        ~GuiHandler();

        bool hasFocus = false;

      private:
        GLFWwindow *window;
        std::chrono::_V2::system_clock::time_point lastDeltaUpdate = std::chrono::high_resolution_clock::now();

        float storedFrameDelta = 0.0f;
        float storedRenderDelta = 0.0f;
        int storedFPS = 0;
        int displayFPS = 0;
        int frameCount = 0;

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

        // Effectively a virtual function for which dialog modal to display
        void (*show)(GuiHandler *p_This) = [](GuiHandler *) {};

        void (*noShow)(GuiHandler *p_This) = [](GuiHandler *) {};

      public:
        ImGuiIO &getIO(void);
        void update(GLFWwindow *p_GLFWwindow, const vk::Extent2D &p_SwapExtent, float p_RenderDelta,
                    float p_FrameDelta);

        std::vector<vk::Framebuffer> createFramebuffers(const vk::Device &p_LogicalDevice,
                                                        const vk::RenderPass &p_GuiRenderPass,
                                                        std::vector<struct SwapchainBuffer> &p_SwapchainBuffers,
                                                        uint32_t p_SwapExtentWidth, uint32_t p_SwapExtentHeight);
        void newFrame(void);
        void renderFrame(void);

        void doRenderPass(const vk::RenderPass &p_RenderPass, const vk::Framebuffer &p_Framebuffer,
                          const vk::CommandBuffer &p_CommandBuffer, vk::Extent2D &p_RenderAreaExtent);

        void cleanup(const vk::Device &p_LogicalDevice);

      private:
        void createRenderPass(std::unordered_map<std::string, vk::RenderPass> &p_RenderPassMap,
                              const vk::Device &p_LogicalDevice, const vk::Format &p_AttachmentColorFormat);

        /*
          OPEN FILE DIALOG
        */
        void (*showOpenDialog)(GuiHandler *p_This) = [](GuiHandler *p_This) {
            if (!ImGui::IsPopupOpen("Load map file...", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
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

                // Get current working directory
                int buttonIndex = 0;
                std::vector<std::pair<int, std::string>> directoryButtons;
                std::istringstream stream(p_This->fileSystem.currentDirectory.string());

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

                auto outputWorkingDirectoryAsButtons = [&](std::pair<int, std::string> directory) {
                    if (ImGui::Button(directory.second.c_str()))
                    {
                        std::string newDirectory;
                        for (size_t i = 0; i <= directory.first; i++)
                        {
                            newDirectory.append(directoryButtons.at(i).second + "/");
                        }
                        p_This->fileSystem.currentDirectory = "/" + newDirectory;
                    }
                    ImGui::SameLine();
                    ImGui::Text("/");
                    ImGui::SameLine();
                };

                std::for_each(directoryButtons.begin(), directoryButtons.end(), outputWorkingDirectoryAsButtons);

                // Display current directory tree
                std::vector<std::pair<FileSystem::FileType, std::pair<std::string, bool>>> filesInDirectory;

                try
                {
                    auto directoryIterator = std::filesystem::directory_iterator(p_This->fileSystem.currentDirectory);
                    for (const auto &file : directoryIterator)
                    {
                        bool shouldDisplay = false;
                        FileSystem::FileType entryType;
                        try
                        {
                            if (file.is_directory())
                            {
                                shouldDisplay = true;
                                entryType = FileSystem::FileType::eDirectory;
                            }
                            else if (file.is_regular_file())
                            {
                                shouldDisplay = true;
                                entryType = FileSystem::FileType::eFile;
                            }
                        }
                        catch (std::filesystem::filesystem_error &e)
                        {
                            shouldDisplay = false;
                            std::cout << "File system error => " << e.what() << "\n";
                        }

                        std::string filePath = file.path().string();

                        std::string filename =
                            filePath.replace(0, p_This->fileSystem.currentDirectory.string().length(), "");

                        // Remove slash if it exists
                        auto slash = filename.find('/');
                        if (slash != std::string::npos)
                            filename.replace(slash, 1, "");

                        // If not hidden file or hidden directory
                        if (filename.at(0) == '.')
                            continue;

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
                    std::cout << "Range error => " << e.what() << "\n";
                }
                catch (std::filesystem::filesystem_error &e)
                {
                    std::cout << "Directory error => " << e.what() << "\n";
                }

                ImGui::Spacing();
                ImGui::NewLine();

                ImGui::BeginChild("ContentRegion");

                ImGui::BeginGroup();

                // Display directory entries, color coded...
                // Red for directory
                // White for file
                for (auto &file : filesInDirectory)
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
                            p_This->fileSystem.currentDirectory.append(file.second.first + "/");
                    }

                    // if (ImGui::Button(file.second.c_str()))
                    // {
                    //     if (file.first == FileSystem::FileType::eDirectory)
                    //     {
                    //         p_This->fileSystem.currentDirectory.append(file.second + "/");
                    //     }
                    // }

                    ImGui::PopStyleColor(2);
                }

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
        void (*showSaveDialog)(GuiHandler *p_This) = [](GuiHandler *) { std::cout << "Not implemented yet\n"; };

        /*
          QUIT CONFIRM DIALOG
        */
        void (*showQuitDialog)(GuiHandler *p_This) = [](GuiHandler *p_This) {
            if (!ImGui::IsPopupOpen("Are you sure?", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
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
        };
    };
}; // namespace mv

#endif