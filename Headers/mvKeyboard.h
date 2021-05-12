#ifndef HEADERS_MVKEYBOARD_H_
#define HEADERS_MVKEYBOARD_H_

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

#include <bitset>
#include <memory>
#include <queue>
#include <stdexcept>

namespace mv {
  static constexpr unsigned int max_buffer_size = 24;

  template <typename T> void trim_buffer(std::queue<T> buffer, uint32_t max_size) {
    while (buffer.size() > max_size) {
      buffer.pop();
    }
    return;
  }

  struct keyboard {
    struct event {
      enum etype { press, release, invalid };
      event(etype type, int code) noexcept {
        this->type = type;
        this->code = code;
      }
      ~event() {
      }
      etype type = etype::invalid;
      int code = GLFW_KEY_UNKNOWN;

      inline bool is_press(void) const noexcept {
        return type == etype::press;
      }
      inline bool is_release(void) const noexcept {
        return type == etype::release;
      }
      inline bool is_valid(void) const noexcept {
        return type != etype::invalid;
      }
    };

  public:
    GLFWwindow *window = nullptr;

    // prevent copy operations
    keyboard(const keyboard &) = delete;
    keyboard &operator=(const keyboard &) = delete;

    // default move operations
    keyboard(keyboard &&) = default;
    keyboard &operator=(keyboard &&) = default;

    keyboard(GLFWwindow *window) {
      if (!window)
        throw std::runtime_error("Invalid window handle passed to keyboard handler");

      this->window = window;
      return;
    }
    ~keyboard() {
    }
  };

}; // namespace mv
#endif