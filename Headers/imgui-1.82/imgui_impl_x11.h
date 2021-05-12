#pragma once
#include "imgui.h" // IMGUI_IMPL_API

struct XlibWindow;

IMGUI_IMPL_API bool ImGui_ImplGlfw_InitForOpenGL(XlibWindow *window, bool install_callbacks);
IMGUI_IMPL_API bool ImGui_ImplGlfw_InitForVulkan(XlibWindow *window, bool install_callbacks);
IMGUI_IMPL_API bool ImGui_ImplGlfw_InitForOther(XlibWindow *window, bool install_callbacks);
IMGUI_IMPL_API void ImGui_ImplGlfw_Shutdown();
IMGUI_IMPL_API void ImGui_ImplGlfw_NewFrame();

// GLFW callbacks
// - When calling Init with 'install_callbacks=true': GLFW callbacks will be installed for you. They
// will call user's previously installed callbacks, if any.
// - When calling Init with 'install_callbacks=false': GLFW callbacks won't be installed. You will
// need to call those function yourself from your own GLFW callbacks.
IMGUI_IMPL_API void ImGui_ImplXlib_MouseButtonCallback(XlibWindow *window, int button, int action,
                                                       int mods);
IMGUI_IMPL_API void ImGui_ImplXlib_ScrollCallback(XlibWindow *window, double xoffset,
                                                  double yoffset);
IMGUI_IMPL_API void ImGui_ImplXlib_KeyCallback(XlibWindow *window, int key, int scancode,
                                               int action, int mods);
IMGUI_IMPL_API void ImGui_ImplXlib_CharCallback(XlibWindow *window, unsigned int c);