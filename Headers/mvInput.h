#pragma once

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

// #define WHEEL_DELTA 120
#define WHEEL_DELTA 1

#include <bitset>
#include <queue>

// Keyboard
namespace mv
{
    class Keyboard
    {
      public:
        struct event
        {
            enum etype
            {
                press,
                release,
                invalid
            };
            event(etype type, int code) noexcept
            {
                this->type = type;
                this->code = code;
            }
            ~event()
            {
            }
            etype type = etype::invalid;
            int code = GLFW_KEY_UNKNOWN;

            inline bool is_press(void) const noexcept
            {
                return type == etype::press;
            }
            inline bool is_release(void) const noexcept
            {
                return type == etype::release;
            }
            inline bool is_valid(void) const noexcept
            {
                return type != etype::invalid;
            }
        }; // end event structure

        const uint32_t max_buffer_size = 24;

        std::bitset<350> key_states;
        std::queue<struct mv::Keyboard::event> key_buffer;

        template <typename T> void trim_buffer(std::queue<T> buffer, uint32_t max_size);

        mv::Keyboard::event read(void);

        void on_key_press(int code);
        void on_key_release(int code);
        void clear_state(void);
        bool is_keystate(int code);
        bool is_key(GLFWwindow *window, int code);

    }; // namespace keyboard
};     // namespace mv

// Mouse
namespace mv
{
    class Mouse
    {
      public:
        struct event
        {
            enum etype
            {
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

            event(etype type, int x, int y, bool l_button, bool m_button, bool r_button) noexcept
            {
                this->type = type;
                this->x = x;
                this->y = y;
                this->is_left_pressed = l_button;
                this->is_middle_pressed = m_button;
                this->is_right_pressed = r_button;
                return;
            }
            ~event()
            {
            }

            etype type = etype::invalid;
            int x = 0;
            int y = 0;

            bool is_left_pressed = false;
            bool is_middle_pressed = false;
            bool is_right_pressed = false;

            inline bool is_valid(void) const noexcept
            {
                return (type != etype::invalid);
            }
        }; // end event

        template <typename T> void trim_buffer(std::queue<T> buffer, uint32_t max_size);

        const uint32_t max_buffer_size = 24;
        std::queue<struct mv::Mouse::event> mouse_buffer;

        enum delta_styles
        {
            from_center = 0,
            from_last
        };
        delta_styles delta_style = delta_styles::from_center;

        int last_x = 0;
        int last_y = 0;
        int current_x = 0;
        int current_y = 0;
        int delta_x = 0;
        int delta_y = 0;
        int center_x = 0;
        int center_y = 0;

        bool is_left_pressed = false;
        bool is_middle_pressed = false;
        bool is_right_pressed = false;

        bool is_dragging = false;
        int drag_startx = 0;
        int drag_starty = 0;
        int drag_delta_x = 0;
        int drag_delta_y = 0;

        float stored_orbit = 0.0f;
        float stored_pitch = 0.0f;

        Mouse::event read(void) noexcept;
        void update(int nx, int ny) noexcept;

        void on_left_press(void) noexcept;
        void on_left_release(void) noexcept;
        void on_right_press(void) noexcept;
        void on_right_release(void) noexcept;
        void on_middle_press(void) noexcept;
        void on_middle_release(void) noexcept;

        void on_wheel_up(int nx, int ny) noexcept;
        void on_wheel_down(int nx, int ny) noexcept;
        void calculate_delta(void) noexcept;
        void start_drag(void) noexcept;
        void start_drag(float orbit, float pitch) noexcept;
        void end_drag(void) noexcept;
        void clear(void) noexcept;
    }; // namespace mouse
};     // namespace mv
