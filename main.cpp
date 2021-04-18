#include "mvEngine.h"

#include <iostream>

int main(int argc, char *argv[])
{
  try
  {
    // Create class called Engine for example
    // configure that class to extend mv::MWindow class
    // Initialize and call Engine::prepare()
    // prepare comes from parent class Window
    // Then call extended class function go() with will contain game loop

    mv::Engine wnd(WINDOW_WIDTH, WINDOW_HEIGHT, "Bloody Day");
    //mv::MWindow wnd(WINDOW_WIDTH, WINDOW_HEIGHT, "Bloody Day");
    if (wnd.goodInit)
    {
      // main game loop, define in extended class
      wnd.go();
    }
    else
    {
      std::cout << std::endl
                << "\t[-] Bad initialization, program ending!" << std::endl;
    }
  }
  catch (mv::BException &e)
  {
    std::cout << std::endl
              << ":: Moogine Engine Exception :: From -> " << e.getType() << std::endl
              << e.getErrorDescription() << std::endl
              << std::endl;
    std::exit(-1);
  }
  catch (std::exception &e)
  {
    std::cout << std::endl
              << ":: Standard Library Exception :: " << std::endl
              << e.what() << std::endl
              << std::endl;
    std::exit(-1);
  }
  catch (...)
  {
    std::cout << std::endl
              << ":: Unhandled Exception ::" << std::endl
              << "Unhandled Exception! No further information available"
              << std::endl
              << std::endl;
    std::exit(-1);
  }
  return 0;
}