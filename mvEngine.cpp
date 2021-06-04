#include "mvEngine.h"
#include "mvAllocator.h"
#include "mvCollection.h"
#include "mvHelper.h"
#include "mvModel.h"
#include "mvRender.h"

extern LogHandler logger;

Engine::Engine (int w, int h, const char *title) : Window (w, h, title)
{

  auto rawMouseSupport = glfwRawMouseMotionSupported ();

  if (!rawMouseSupport)
    throw std::runtime_error ("Raw mouse motion not supported");
  // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, true);

  // Set keyboard callback
  glfwSetKeyCallback (window, keyCallback);

  // Set mouse motion callback
  glfwSetCursorPosCallback (window, mouseMotionCallback);

  // Set mouse button callback
  glfwSetMouseButtonCallback (window, mouseButtonCallback);

  // Set mouse wheel callback
  glfwSetScrollCallback (window, mouseScrollCallback);

  // For imgui circular callback when recreating swap
  glfwSetCharCallback (window, NULL);

  // Add our base app class to glfw window for callbacks
  glfwSetWindowUserPointer (window, this);
  return;
}

Engine::~Engine ()
{
  logicalDevice->waitIdle ();

  // render related methods
  if (render)
    render->cleanup ();

  // cleanup map handler
  if (scene)
    scene->cleanup ();

  // collection struct will handle cleanup of models & objs
  if (collection)
    collection->cleanup ();

  if (allocator)
    allocator->cleanup ();
}

void
Engine::cleanupSwapchain (void)
{
  std::cout << "--cleaning up swap chain--\n";

  render->reset ();

  // cleanup swapchain
  swapchain->cleanup (*instance, *logicalDevice, false);

  return;
}

// Allows swapchain to keep up with window resize
void
Engine::recreateSwapchain (void)
{
  logger.logMessage ("Recreating swap chain");

  logicalDevice->waitIdle ();

  // Cleanup
  cleanupSwapchain ();

  render->createCommandPools (REQUESTED_COMMAND_QUEUES);

  // create swapchain
  swapchain->create (*physicalDevice, *logicalDevice, windowWidth, windowHeight);

  // Create layouts for render passes
  render->prepareLayouts ();

  // create render pass : core
  render->setupRenderPass ();

  // If ImGui enabled, recreate it's resources
  if (gui)
    {
      gui->cleanup (*logicalDevice);
      // Reinstall glfw callbacks to avoid ImGui creating
      // an infinite callback circle
      glfwSetKeyCallback (window, keyCallback);
      glfwSetCursorPosCallback (window, mouseMotionCallback);
      glfwSetMouseButtonCallback (window, mouseButtonCallback);
      glfwSetScrollCallback (window, mouseScrollCallback);
      glfwSetCharCallback (window, NULL);
      // Add our base app class to glfw window for callbacks
      glfwSetWindowUserPointer (window, this);

      auto &[pool, commandBuffers] = commandPools->at (vk::QueueFlagBits::eGraphics);

      gui = std::make_unique<GuiHandler> (window,
                                          scene.get (),
                                          &camera,
                                          instance,
                                          physicalDevice,
                                          logicalDevice,
                                          swapchain,
                                          pool,
                                          commandQueues->at (vk::QueueFlagBits::eGraphics)->at (0),
                                          renderPasses,
                                          allocator->get ()->pool);

      if (!framebuffers.contains (eImGuiFramebuffer))
        {
          framebuffers.insert ({ eImGuiFramebuffer, { {} } });
        }
      framebuffers->at (eImGuiFramebuffer) = gui->createFramebuffers (logicalDevice,
                                                                      renderPasses->at (eImGui),
                                                                      swapchain.buffers,
                                                                      swapchain.swapExtent.width,
                                                                      swapchain.swapExtent.height);
      std::cout << "Created gui frame buffers, size => "
                << framebuffers->at (eImGuiFramebuffer).size () << "\n";
      for (const auto &buf : framebuffers->at (eImGuiFramebuffer))
        {
          std::cout << "gui buffer => " << buf << "\n";
        }
    }

  setupDepthStencil ();

  // create graphics pipeline + pipeline layout
  prepareGraphicsPipelines ();
  prepareComputePipelines ();
  setupFramebuffers ();
  createCommandBuffers ();
}

void
Engine::go (void)
{
  logger.logMessage ("Preparing Vulkan");
  prepare ();

  // Initialize here before use in later methods
  allocator = std::make_unique<Allocator> (this);
  allocator->allocatePool (2000);

  collectionHandler = std::make_unique<Collection> (this);

  // setup descriptor allocator, collection handler & camera
  // load models & create objects here
  goSetup ();

  prepareLayouts ();
  prepareGraphicsPipelines ();
  prepareComputePipelines ();

  uint32_t imageIndex = 0;
  size_t currentFrame = 0;

  bool added = false;

  fps.startTimer ();

  using namespace std::chrono_literals;
  using chrono = std::chrono::high_resolution_clock;

  constexpr std::chrono::nanoseconds timestep (16ms);

  std::chrono::nanoseconds accumulated (0ns);
  auto startTime = chrono::now ();

  /*
      Create and initialize ImGui handler
      Will create render pass & perform pre game loop ImGui initialization
  */
  gui = std::make_unique<GuiHandler> (
      window,
      &mapHandler,
      &camera,
      instance,
      physicalDevice,
      logicalDevice,
      swapchain,
      commandPoolsBuffers->at (vk::QueueFlagBits::eGraphics)->at (0).first,
      commandQueues->at (vk::QueueFlagBits::eGraphics)->at (0),
      renderPasses,
      allocator->get ()->pool);

  if (!framebuffers.contains (eImGuiFramebuffer))
    {
      framebuffers.insert ({ eImGuiFramebuffer, { {} } });
    }

  framebuffers->at (eImGuiFramebuffer) = gui->createFramebuffers (logicalDevice,
                                                                  renderPasses->at (eImGui),
                                                                  swapchain.buffers,
                                                                  swapchain.swapExtent.width,
                                                                  swapchain.swapExtent.height);
  std::cout << "Created gui frame buffers, size => " << framebuffers->at (eImGuiFramebuffer).size ()
            << "\n";
  for (const auto &buf : framebuffers->at (eImGuiFramebuffer))
    {
      std::cout << "gui buffer => " << buf << "\n";
    }

  auto renderStart = chrono::now ();
  auto renderStop = chrono::now ();

#ifndef NDEBUG
  // For debugging purposes
  recreateSwapchain ();

  try
    {
      mapHandler.readHeightMap (gui.get (), "heightmaps/scaled.png");
    }
  catch (std::exception &e)
    {
      std::cout << ":: Fatal error reading heightmap :: \n" << e.what () << "\n";
      std::exit (1);
    }
#endif

  while (!glfwWindowShouldClose (window))
    {
      auto deltaTime = chrono::now () - startTime;
      startTime = chrono::now ();
      accumulated += std::chrono::duration_cast<std::chrono::nanoseconds> (deltaTime);

      glfwPollEvents ();

      while (accumulated >= timestep)
        {
          accumulated -= timestep;

          // Get input events
          Keyboard::Event kbdEvent = keyboard.read ();
          Mouse::Event mouseEvent = mouse.read ();

          if (camera.type == CameraType::eFreeLook && !gui->hasFocus)
            {
              /*
                  Start mouse drag
              */
              if (mouseEvent.type == Mouse::Event::Type::eRightDown)
                {

                  if (!mouse.isDragging)
                    {
                      // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                      // GLFW_TRUE); glfwSetInputMode(window, GLFW_CURSOR,
                      // GLFW_CURSOR_DISABLED);
                      mouse.startDrag (camera.orbitAngle, camera.pitch);
                    }
                }
              if (mouseEvent.type == Mouse::Event::Type::eRightRelease)
                {
                  if (mouse.isDragging)
                    {
                      // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                      // GLFW_FALSE); glfwSetInputMode(window, GLFW_CURSOR,
                      // GLFW_CURSOR_NORMAL);
                      mouse.endDrag ();
                    }
                }

              // if drag enabled check for release
              if (mouse.isDragging)
                {
                  camera.rotate ({ -mouse.dragDeltaY, -mouse.dragDeltaX, 0.0f }, 1.0f);

                  mouse.storedOrbit = camera.orbitAngle;
                  mouse.storedPitch = camera.pitch;
                  mouse.clear ();
                }

              // WASD
              if (keyboard.isKeyState (GLFW_KEY_W))
                camera.move (camera.rotation.y,
                             { 0.0f, 0.0f, -1.0f },
                             (keyboard.isKeyState (GLFW_KEY_LEFT_SHIFT)));
              if (keyboard.isKeyState (GLFW_KEY_A))
                camera.move (camera.rotation.y,
                             { -1.0f, 0.0f, 0.0f },
                             (keyboard.isKeyState (GLFW_KEY_LEFT_SHIFT)));
              if (keyboard.isKeyState (GLFW_KEY_S))
                camera.move (camera.rotation.y,
                             { 0.0f, 0.0f, 1.0f },
                             (keyboard.isKeyState (GLFW_KEY_LEFT_SHIFT)));
              if (keyboard.isKeyState (GLFW_KEY_D))
                camera.move (camera.rotation.y,
                             { 1.0f, 0.0f, 0.0f },
                             (keyboard.isKeyState (GLFW_KEY_LEFT_SHIFT)));

              // t -- freeze frustum updates to compute
              if (kbdEvent.code == GLFW_KEY_T && kbdEvent.type == Keyboard::Event::Type::ePress)
                {
                  mapHandler.freezeCull = !mapHandler.freezeCull;
                  std::cout << "Freeze cull status: " << mapHandler.freezeCull << "\n";
                }

              // space + alt
              if (keyboard.isKeyState (GLFW_KEY_SPACE))
                camera.moveUp (keyboard.isKeyState (GLFW_KEY_LEFT_SHIFT));
              if (keyboard.isKeyState (GLFW_KEY_LEFT_ALT))
                camera.moveDown (keyboard.isKeyState (GLFW_KEY_LEFT_SHIFT));
            }

          /*
              THIRD PERSON CAMERA
          */
          if (camera.type == CameraType::eThirdPerson && !gui->hasFocus)
            {

              /*
                  Mouse scroll wheel
              */
              if (mouseEvent.type == Mouse::Event::Type::eWheelUp && !gui->hover)
                {
                  camera.adjustZoom (-(camera.zoomStep));
                }
              if (mouseEvent.type == Mouse::Event::Type::eWheelDown && !gui->hover)
                {
                  camera.adjustZoom (camera.zoomStep);
                }

              /*
                  Start mouse drag
              */
              if (mouseEvent.type == Mouse::Event::Type::eRightDown)
                {

                  if (!mouse.isDragging)
                    {
                      // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                      // GLFW_TRUE); glfwSetInputMode(window, GLFW_CURSOR,
                      // GLFW_CURSOR_DISABLED);
                      mouse.startDrag (camera.orbitAngle, camera.pitch);
                    }
                }
              if (mouseEvent.type == Mouse::Event::Type::eRightRelease)
                {
                  if (mouse.isDragging)
                    {
                      // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                      // GLFW_FALSE); glfwSetInputMode(window, GLFW_CURSOR,
                      // GLFW_CURSOR_NORMAL);
                      mouse.endDrag ();
                    }
                }

              // if drag enabled check for release
              if (mouse.isDragging)
                {
                  // camera orbit
                  if (mouse.dragDeltaX > 0)
                    {
                      camera.lerpOrbit (-abs (mouse.dragDeltaX));
                    }
                  else if (mouse.dragDeltaX < 0)
                    {
                      // camera.adjustOrbit(abs(mouse.dragDeltaX));
                      camera.lerpOrbit (abs (mouse.dragDeltaX));
                    }

                  // camera pitch
                  if (mouse.dragDeltaY > 0)
                    {
                      camera.lerpPitch (abs (mouse.dragDeltaY));
                    }
                  else if (mouse.dragDeltaY < 0)
                    {
                      camera.lerpPitch (-abs (mouse.dragDeltaY));
                    }

                  camera.target->rotateToAngle (camera.orbitAngle);

                  mouse.storedOrbit = camera.orbitAngle;
                  mouse.storedPitch = camera.pitch;
                  mouse.clear ();
                }

              // sort movement by key combination

              // forward only
              if (keyboard.isKeyState (GLFW_KEY_W) && !keyboard.isKeyState (GLFW_KEY_S)
                  && !keyboard.isKeyState (GLFW_KEY_A) && !keyboard.isKeyState (GLFW_KEY_D))
                {
                  camera.target->move (camera.orbitAngle, glm::vec4 (0.0f, 0.0f, -1.0f, 1.0f));
                }

              // forward + left + right -- go straight
              if (keyboard.isKeyState (GLFW_KEY_W) && !keyboard.isKeyState (GLFW_KEY_S)
                  && keyboard.isKeyState (GLFW_KEY_A) && keyboard.isKeyState (GLFW_KEY_D))
                {
                  camera.target->move (camera.orbitAngle, glm::vec4 (0.0f, 0.0f, -1.0f, 1.0f));
                }

              // forward + right
              if (keyboard.isKeyState (GLFW_KEY_W) && !keyboard.isKeyState (GLFW_KEY_S)
                  && !keyboard.isKeyState (GLFW_KEY_A) && keyboard.isKeyState (GLFW_KEY_D))
                {
                  camera.target->move (camera.orbitAngle, glm::vec4 (1.0f, 0.0f, -1.0f, 1.0f));
                }

              // forward + left
              if (keyboard.isKeyState (GLFW_KEY_W) && !keyboard.isKeyState (GLFW_KEY_S)
                  && keyboard.isKeyState (GLFW_KEY_A) && !keyboard.isKeyState (GLFW_KEY_D))
                {
                  camera.target->move (camera.orbitAngle, glm::vec4 (-1.0f, 0.0f, -1.0f, 1.0f));
                }

              // backward only
              if (!keyboard.isKeyState (GLFW_KEY_W) && keyboard.isKeyState (GLFW_KEY_S)
                  && !keyboard.isKeyState (GLFW_KEY_A) && !keyboard.isKeyState (GLFW_KEY_D))
                {
                  camera.target->move (camera.orbitAngle, glm::vec4 (0.0f, 0.0f, 1.0f, 1.0f));
                }

              // backward + left
              if (!keyboard.isKeyState (GLFW_KEY_W) && keyboard.isKeyState (GLFW_KEY_S)
                  && keyboard.isKeyState (GLFW_KEY_A) && !keyboard.isKeyState (GLFW_KEY_D))
                {
                  camera.target->move (camera.orbitAngle, glm::vec4 (-1.0f, 0.0f, 1.0f, 1.0f));
                }

              // backward + right
              if (!keyboard.isKeyState (GLFW_KEY_W) && keyboard.isKeyState (GLFW_KEY_S)
                  && !keyboard.isKeyState (GLFW_KEY_A) && keyboard.isKeyState (GLFW_KEY_D))
                {
                  camera.target->move (camera.orbitAngle, glm::vec4 (1.0f, 0.0f, 1.0f, 1.0f));
                }

              // backward + left + right -- go back
              if (!keyboard.isKeyState (GLFW_KEY_W) && keyboard.isKeyState (GLFW_KEY_S)
                  && keyboard.isKeyState (GLFW_KEY_A) && keyboard.isKeyState (GLFW_KEY_D))
                {
                  camera.target->move (camera.orbitAngle, glm::vec4 (0.0f, 0.0f, 1.0f, 1.0f));
                }

              // left only
              if (!keyboard.isKeyState (GLFW_KEY_W) && !keyboard.isKeyState (GLFW_KEY_S)
                  && keyboard.isKeyState (GLFW_KEY_A) && !keyboard.isKeyState (GLFW_KEY_D))
                {
                  camera.target->move (camera.orbitAngle, glm::vec4 (-1.0f, 0.0f, 0.0f, 1.0f));
                }

              // right only
              if (!keyboard.isKeyState (GLFW_KEY_W) && !keyboard.isKeyState (GLFW_KEY_S)
                  && !keyboard.isKeyState (GLFW_KEY_A) && keyboard.isKeyState (GLFW_KEY_D))
                {
                  camera.target->move (camera.orbitAngle, glm::vec4 (1.0f, 0.0f, 0.0f, 1.0f));
                }

              // debug -- add new objects to world with random position
              if (keyboard.isKeyState (GLFW_KEY_SPACE) && added == false)
                {
                  // added = true;
                  int min = 0;
                  int max = 90;
                  int z_max = 60;

                  std::random_device rd;
                  std::default_random_engine eng (rd ());
                  std::uniform_int_distribution<int> xy_distr (min, max);
                  std::uniform_int_distribution<int> z_distr (min, z_max);

                  float x = xy_distr (eng);
                  float y = xy_distr (eng);
                  float z = z_distr (eng);
                  allocator->createObject (collectionHandler->models.get (),
                                           "models/_viking_room.fbx");
                  collectionHandler->models->at (0).objects->back ().position = glm::vec3 (x, y, z);
                }
            } // end third person methods
          // update game objects
          collectionHandler->update ();

          // update view and projection matrices
          camera.update ();
          if (!mapHandler.freezeCull)
            {
              mapHandler.clipSpace->matrix = collectionHandler->projectionUniform->matrix
                                             * collectionHandler->viewUniform->matrix;
              mapHandler.update ();
            }

          /*
              Update ray cast vertices
          */
          // int min = 0;
          // int max = 90;
          // int z_max = 60;

          // std::random_device rd;
          // std::default_random_engine eng(rd());
          // std::uniform_int_distribution<int> xy_distr(min, max);
          // std::uniform_int_distribution<int> z_distr(min, z_max);

          // float x = xy_distr(eng);
          // float y = xy_distr(eng);
          // float z = z_distr(eng);
          // raycast_p1(*this, {x, y, z});
          // raycastP2(*this,
          // {collectionHandler.models->at(1).objects->at(0).position.x,
          //                   collectionHandler.models->at(1).objects->at(0).position.y
          //                   - 1.5f,
          //                   collectionHandler.models->at(1).objects->at(0).position.z});
        }

      // Game editor rendering
      {
        gui->newFrame ();

        gui->update (
            swapchain.swapExtent,
            std::chrono::duration<float, std::ratio<1L, 1000L>> (renderStop - renderStart).count (),
            std::chrono::duration<float, std::chrono::milliseconds::period> (deltaTime).count (),
            static_cast<uint32_t> (collectionHandler->modelNames.size ()),
            collectionHandler->getObjectCount (),
            collectionHandler->getVertexCount () + mapHandler.indexCount);

        gui->renderFrame ();
      }

      // TODO
      // Add alpha to render
      // Render
      renderStart = chrono::now ();

      draw (currentFrame, imageIndex);
      renderStop = chrono::now ();
    } // end game loop

  gui->cleanup (logicalDevice);

  int totalObjectCount = 0;
  for (const auto &model : *collectionHandler->models)
    {
      totalObjectCount += model.objects->size ();
    }
  logger.logMessage ("Exiting Vulkan..");
  logger.logMessage ("Total objects => " + std::to_string (totalObjectCount));
  return;
}

// initialize descriptor allocator, collection handler & camera
void
Engine::goSetup (void)
{
  using enum vk::ShaderStageFlagBits;

  /*
      uniform for vertex/compute
  */
  allocator->createLayout (vk::DescriptorType::eUniformBuffer, 1, eVertex | eCompute, 0);

  /*
      sampler
  */
  allocator->createLayout (vk::DescriptorType::eCombinedImageSampler, 1, eFragment, 0);

  /*
    SSBO for compute
  */
  allocator->createLayout (vk::DescriptorType::eStorageBuffer, 1, eCompute, 0);

  /*
      INITIALIZE MODEL/OBJECT HANDLER
      CREATES BUFFERS FOR VIEW & PROJECTION MATRICES
  */

  // auto uniformLayout = allocator->getLayout (vk::DescriptorType::eUniformBuffer);
  /*
      VIEW MATRIX UNIFORM OBJECT
  */
  // DONE BY INTERNAL MV BUFFER OF UNIFORM OBJECT
  // allocator->allocateSet (uniformLayout, collectionHandler->viewUniform->descriptor);
  // allocator->updateSet (collectionHandler->viewUniform->mvBuffer.bufferInfo,
  //                       collectionHandler->viewUniform->descriptor,
  //                       vk::DescriptorType::eUniformBuffer,
  //                       0);

  /*
      PROJECTION MATRIX UNIFORM OBJECT
  */
  // DONE BY INTERNAL MV BUFFER OF UNIFORM OBJECT
  // allocator->allocateSet (uniformLayout, collectionHandler->projectionUniform->descriptor);
  // allocator->updateSet (collectionHandler->projectionUniform->mvBuffer.bufferInfo,
  //                       collectionHandler->projectionUniform->descriptor,
  //                       vk::DescriptorType::eUniformBuffer,
  //                       0);

  /*
      LOAD MODELS
  */
  allocator->loadModel (
      collectionHandler->models.get (), &collectionHandler->modelNames, "models/Male.obj");
  allocator->createObject (collectionHandler->models.get (), "models/Male.obj");
  collectionHandler->models->at (0).objects->at (0).position = { 0.0f, 0.0f, 0.0f };
  /*
      CREATE OBJECTS
  */

  /*
      CONFIGURE AND CREATE CAMERA
  */
  struct CameraInitStruct cameraParams = {};
  cameraParams.fov = 50.0f;
  cameraParams.aspect = static_cast<float> (swapchain.swapExtent.width)
                        / static_cast<float> (swapchain.swapExtent.height);
  cameraParams.nearz = 0.1f;
  cameraParams.farz = 5000.0f;
  cameraParams.position = glm::vec3 (1, -10, 3.0f);
  cameraParams.viewUniformObject = collectionHandler->viewUniform.get ();
  cameraParams.projectionUniformObject = collectionHandler->projectionUniform.get ();

  cameraParams.type = CameraType::eFreeLook;
  cameraParams.target = nullptr;

  camera = Camera (cameraParams);

  /*
      Hard coding settings
  */
  // camera.rotation = { 0.0f, -90.0f, 0.0f };
  camera.zoomLevel = 7.0f;
  camera.rotation = { -45.0f, 0.0f, 0.0f };
  mouse.deltaStyle = Mouse::DeltaStyles::eFromLastPosition;
  mouse.isDragging = false;
  return;
}

/*
  GLFW CALLBACKS
*/

void
mouseMotionCallback (GLFWwindow *p_GLFWwindow, double p_XPosition, double p_YPosition)
{
  auto engine = reinterpret_cast<Engine *> (glfwGetWindowUserPointer (p_GLFWwindow));
  engine->mouse.update (p_XPosition, p_YPosition);
  return;
}

void
mouseScrollCallback (GLFWwindow *p_GLFWwindow, [[maybe_unused]] double p_XOffset, double p_YOffset)
{
  auto engine = reinterpret_cast<Engine *> (glfwGetWindowUserPointer (p_GLFWwindow));

  if (p_YOffset > 0)
    engine->mouse.onWheelUp ();
  if (p_YOffset < 0)
    engine->mouse.onWheelDown ();

  return;
}

void
mouseButtonCallback (GLFWwindow *p_GLFWwindow,
                     int p_Button,
                     int p_Action,
                     [[maybe_unused]] int p_Modifiers)
{
  auto engine = reinterpret_cast<Engine *> (glfwGetWindowUserPointer (p_GLFWwindow));

  switch (p_Button)
    {
    case GLFW_MOUSE_BUTTON_1: // LMB
      {
        if (p_Action == GLFW_PRESS)
          {
            engine->mouse.onLeftPress ();
          }
        else if (p_Action == GLFW_RELEASE)
          {
            engine->mouse.onLeftRelease ();
          }
        break;
      }
    case GLFW_MOUSE_BUTTON_2: // RMB
      {
        if (p_Action == GLFW_PRESS)
          {
            engine->mouse.onRightPress ();
          }
        else if (p_Action == GLFW_RELEASE)
          {
            engine->mouse.onRightRelease ();
          }
        break;
      }
    case GLFW_MOUSE_BUTTON_3: // MMB
      {
        if (p_Action == GLFW_PRESS)
          {
            engine->mouse.onMiddlePress ();
          }
        else if (p_Action == GLFW_RELEASE)
          {
            engine->mouse.onMiddleRelease ();
          }
        break;
      }
    }
  return;
}

void
keyCallback (GLFWwindow *p_GLFWwindow,
             int p_Key,
             [[maybe_unused]] int p_ScanCode,
             int p_Action,
             [[maybe_unused]] int p_Modifier)
{
  // get get pointer
  auto engine = reinterpret_cast<Engine *> (glfwGetWindowUserPointer (p_GLFWwindow));

  if (p_Key == GLFW_KEY_ESCAPE && p_Action == GLFW_PRESS)
    {
      // glfwSetWindowShouldClose(p_GLFWwindow, true);
      return;
    }

  if (p_Action == GLFW_PRESS)
    {
      engine->keyboard.onKeyPress (p_Key);
      // engine->gui->getIO().AddInputCharacter(p_Key);
    }
  else if (p_Action == GLFW_RELEASE)
    {
      engine->keyboard.onKeyRelease (p_Key);
    }
  return;
}

void
glfwErrCallback (int p_Error, const char *p_Description)
{
  logger.logMessage (LogHandlerMessagePriority::eError,
                     "GLFW Error...\nCode => " + std::to_string (p_Error) + "\nMessage=> "
                         + std::string (p_Description));
  return;
}