#include "mvEngine.h"

#include <iostream>

namespace mv {
  std::bitset<350> key_states;
  std::queue<mv::keyboard::event> key_buffer;
}; // namespace mv

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  try {
    mv::Engine wnd(WINDOW_WIDTH, WINDOW_HEIGHT, "Bloody Day");
    if (wnd.good_init) {
      wnd.go();
    } else {
      std::cout << std::endl << "\t[-] Bad initialization, program ending!" << std::endl;
    }
  } catch (std::exception &e) {
    std::cout << std::endl
              << ":: Standard Library Exception :: " << std::endl
              << e.what() << std::endl
              << std::endl;
    std::exit(-1);
  } catch (...) {
    std::cout << std::endl
              << ":: Unhandled Exception ::" << std::endl
              << "Unhandled Exception! No further information available" << std::endl
              << std::endl;
    std::exit(-1);
  }
  return 0;
}