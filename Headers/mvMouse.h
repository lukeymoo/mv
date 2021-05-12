#ifndef HEADERS_MVMOUSE_H_
#define HEADERS_MVMOUSE_H_

// Appropriate for windows
// #define WHEEL_DELTA 120
#define WHEEL_DELTA 1

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

#include <iostream>
#include <memory>
#include <queue>
#include <stdexcept>

namespace mv {
  struct mouse {
    template <typename T> static void trim_buffer(std::queue<T> buffer, uint32_t max_size) {
      while (buffer.size() > max_size) {
        buffer.pop();
      }
      return;
    }
    /*
        Mouse event structure
    */
    struct event {
      enum etype {
        l_down,
        l_release,
        r_down,
        r_release,
        m_down, // middle
        m_release,
        wheel_up,
        wheel_down,
        move,  // mouse move
        leave, // leave client area
        enter, // enter client area
        invalid
      };

      event(etype type, int x, int y, bool l_button, bool m_button, bool r_button) noexcept {
        this->type = type;
        this->x = x;
        this->y = y;
        this->is_left_pressed = l_button;
        this->is_middle_pressed = m_button;
        this->is_right_pressed = r_button;
        return;
      }
      event(etype type, const mv::mouse &parent) noexcept {
        this->type = type;
        this->x = parent.current_x;
        this->y = parent.current_y;
        this->is_left_pressed = parent.is_left_pressed;
        this->is_middle_pressed = parent.is_middle_pressed;
        this->is_right_pressed = parent.is_right_pressed;
        return;
      }
      ~event() {
      }

      etype type = etype::invalid;
      int x = 0;
      int y = 0;

      bool is_left_pressed = false;
      bool is_middle_pressed = false;
      bool is_right_pressed = false;

      inline bool is_valid(void) const noexcept {
        return (type != etype::invalid);
      }
    };

    // End of mouse event structure

  public:
    enum delta_calc_style {
      from_center = 0, // fps/freelook deltas
      from_last
    };

    // delete copy operations
    mouse(const mouse &) = delete;
    mouse &operator=(const mouse &) = delete;

    // default move operations
    mouse(mouse &&) = default;
    mouse &operator=(mouse &&) = default;

    mouse(GLFWwindow *window) {
      if (!window)
        throw std::runtime_error("Invalid window handle passed to mouse handler");
      this->window = window;
    }
    ~mouse() {
    }

    static constexpr int max_buffer_size = 16;

  private:
    int window_width = 0;
    int window_height = 0;
    int center_x = 0;
    int center_y = 0;

    GLFWwindow *window = nullptr;

  public:
    int last_x = 0;
    int last_y = 0;
    int current_x = 0;
    int current_y = 0;

    // raw mouse delta
    int delta_x = 0;
    int delta_y = 0;

    bool is_dragging = false;
    // accumulated delta while is_dragging
    int drag_delta_x = 0;
    int drag_delta_y = 0;
    // where to warp pointer
    int drag_startx = 0;
    int drag_starty = 0;

    bool is_left_pressed = false;
    bool is_middle_pressed = false;
    bool is_right_pressed = false;

    int wheel_delta_carry = 0;

    bool in_window = false;

    float stored_pitch = 0.0f;
    float stored_orbit = 0.0f;

    // how is delta_x/y calculated
    delta_calc_style delta_style = delta_calc_style::from_last;
    std::queue<mouse::event> mouse_buffer;

  public:
    inline void clear(void) {
      // prevent accidental use of stale data by clearing
      delta_x = 0;
      delta_y = 0;
      drag_delta_x = 0;
      drag_delta_y = 0;
      return;
    }

    inline mouse::event read(void) noexcept {
      event e(mouse::event::etype::invalid, 0, 0, false, false, false);
      if (!mouse_buffer.empty()) {
        e = mouse_buffer.front();
        mouse_buffer.pop();
      }
      return e;
    }
    inline void set_window_properties(int width, int height) noexcept {
      this->window_width = width;
      this->window_height = height;
      this->center_x = (width / 2);
      this->center_y = (height / 2);
      return;
    }
    inline void set_delta_style(delta_calc_style style) {
      if (style == delta_calc_style::from_center) {
        if (!window_width || !window_height) {
          throw std::runtime_error(
              "To calculate delta using from_center you must specify the "
              "window or swapchain width/height; Be aware of border widths etc\n");
        }
      }
      this->delta_style = style;
      return;
    }
    inline void start_drag(void) noexcept {
      drag_startx = current_x;
      drag_starty = current_y;
      is_dragging = true;
      return;
    }
    inline void start_drag(float orbit, float pitch) noexcept {
      stored_orbit = orbit;
      stored_pitch = pitch;
      drag_startx = current_x;
      drag_starty = current_y;
      is_dragging = true;
      return;
    }
    inline void end_drag(void) noexcept {
      stored_orbit = 0.0f;
      stored_pitch = 0.0f;
      drag_startx = 0;
      drag_starty = 0;
      drag_delta_x = 0;
      drag_delta_y = 0;
      is_dragging = false;
      return;
    }

    inline void calculate_delta(void) {
      // if dragging calculate drag delta
      // if (is_dragging)
      // {
      //     drag_delta_x = current_x - drag_startx;
      //     drag_delta_y = current_y - drag_starty;
      // }
      // if (delta_style == delta_calc_style::from_center)
      // {
      //     delta_x = current_x - center_x;
      //     delta_y = current_y - center_y;
      // }
      // else if (delta_style == delta_calc_style::from_last)
      // {
      //     delta_x = current_x - last_x;
      //     delta_y = current_y - last_y;
      // }
      return;
    }

    inline void on_left_press(int nx, int ny) noexcept {
      last_x = current_x;
      last_y = current_y;
      current_x = nx;
      current_y = ny;
      calculate_delta();
      is_left_pressed = true;
      mouse_buffer.push(event(event::etype::l_down, nx, ny, is_left_pressed, is_middle_pressed,
                              is_right_pressed));
      trim_buffer(mouse_buffer, max_buffer_size);
      return;
    }
    inline void on_left_release(int nx, int ny) noexcept {
      last_x = current_x;
      last_y = current_y;
      current_x = nx;
      current_y = ny;
      calculate_delta();
      is_left_pressed = false;
      mouse_buffer.push(event(event::etype::l_release, nx, ny, is_left_pressed, is_middle_pressed,
                              is_right_pressed));
      trim_buffer(mouse_buffer, max_buffer_size);
      return;
    }

    inline void on_right_press(int nx, int ny) noexcept {
      last_x = current_x;
      last_y = current_y;
      current_x = nx;
      current_y = ny;
      calculate_delta();
      is_right_pressed = true;
      mouse_buffer.push(event(event::etype::r_down, nx, ny, is_left_pressed, is_middle_pressed,
                              is_right_pressed));
      trim_buffer(mouse_buffer, max_buffer_size);
      return;
    }
    inline void on_right_release(int nx, int ny) noexcept {
      last_x = current_x;
      last_y = current_y;
      current_x = nx;
      current_y = ny;
      calculate_delta();
      is_right_pressed = false;
      mouse_buffer.push(event(event::etype::r_release, nx, ny, is_left_pressed, is_middle_pressed,
                              is_right_pressed));
      trim_buffer(mouse_buffer, max_buffer_size);
      return;
    }

    inline void on_middle_press(int nx, int ny) noexcept {
      last_x = current_x;
      last_y = current_y;
      current_x = nx;
      current_y = ny;
      calculate_delta();
      is_middle_pressed = true;
      mouse_buffer.push(event(event::etype::m_down, nx, ny, is_left_pressed, is_middle_pressed,
                              is_right_pressed));
      trim_buffer(mouse_buffer, max_buffer_size);
      return;
    }
    inline void on_middle_release(int nx, int ny) noexcept {
      last_x = current_x;
      last_y = current_y;
      current_x = nx;
      current_y = ny;
      calculate_delta();
      is_middle_pressed = false;
      mouse_buffer.push(event(event::etype::m_release, nx, ny, is_left_pressed, is_middle_pressed,
                              is_right_pressed));
      trim_buffer(mouse_buffer, max_buffer_size);
      return;
    }

    // used internally -- use onDelta
    inline void on_wheel_up(int nx, int ny) noexcept {
      last_x = current_x;
      last_y = current_y;
      current_x = nx;
      current_y = ny;
      calculate_delta();
      mouse_buffer.push(event(event::etype::wheel_up, nx, ny, is_left_pressed, is_middle_pressed,
                              is_right_pressed));
      trim_buffer(mouse_buffer, max_buffer_size);
      return;
    }
    // used internally -- use onDelta
    inline void on_wheel_down(int nx, int ny) noexcept {
      last_x = current_x;
      last_y = current_y;
      current_x = nx;
      current_y = ny;
      calculate_delta();
      mouse_buffer.push(event(event::etype::wheel_down, nx, ny, is_left_pressed, is_middle_pressed,
                              is_right_pressed));
      trim_buffer(mouse_buffer, max_buffer_size);
      return;
    }
    // use this one for mouse wheel
    inline void on_wheel_delta(int nx, int ny, int w_delta) noexcept {
      wheel_delta_carry += w_delta;
      if (wheel_delta_carry >= WHEEL_DELTA) {
        wheel_delta_carry -= WHEEL_DELTA;
        on_wheel_up(nx, ny);
      } else if (wheel_delta_carry <= -WHEEL_DELTA) {
        wheel_delta_carry += WHEEL_DELTA;
        on_wheel_down(nx, ny);
      }
      return;
    }
    inline void on_mouse_enter(void) noexcept {
      in_window = true;
      mouse_buffer.push(event(event::etype::enter, *this));
      trim_buffer(mouse_buffer, max_buffer_size);
      return;
    }
    inline void on_mouse_leave(void) noexcept {
      in_window = false;
      mouse_buffer.push(event(event::etype::leave, *this));
      trim_buffer(mouse_buffer, max_buffer_size);
      return;
    }
  };
}; // namespace mv

#endif