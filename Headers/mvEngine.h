#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <chrono>
#include <random>
#include <unordered_map>

#include "mvCamera.h"
#include "mvGui.h"
#include "mvMap.h"
#include "mvTimer.h"
#include "mvWindow.h"

constexpr vk::QueueFlags REQUESTED_COMMAND_QUEUES
    = vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eGraphics;

class Allocator;
struct Collection;
class Container;
class MvBuffer;
class Renderer;

void keyCallback (GLFWwindow *window, int key, int scancode, int action, int mods);
void mouseMotionCallback (GLFWwindow *window, double xpos, double ypos);
void mouseScrollCallback (GLFWwindow *p_GLFWwindow, double p_XOffset, double p_YOffset);
void mouseButtonCallback (GLFWwindow *window, int button, int action, int mods);
void glfwErrCallback (int error, const char *desc);

/*
  Game state manager
*/
class Engine : public Window
{
public:
  Engine (int w, int h, const char *title);
  ~Engine ();

  // delete copy
  Engine &operator= (const Engine &) = delete;
  Engine (const Engine &) = delete;

  Timer timer;
  Timer fps;

  std::unique_ptr<Camera> camera;         // camera manager(view/proj matrix handler)
  std::unique_ptr<MapHandler> scene;      // Map related methods
  std::unique_ptr<Allocator> allocator;   // descriptor pool/set manager
  std::unique_ptr<Collection> collection; // model/obj manager
  std::unique_ptr<GuiHandler> gui;        // ImGui manager
  std::unique_ptr<Renderer> render;       // Manages pipeline, presentation, compute

  void recreateSwapchain (void);

  void goSetup (void);
  void go (void);

private:
  void cleanupSwapchain (void);
};
