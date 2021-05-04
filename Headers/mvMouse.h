#ifndef HEADERS_MVMOUSE_H_
#define HEADERS_MVMOUSE_H_

// Appropriate for windows
// #define WHEEL_DELTA 120
#define WHEEL_DELTA 1

#include <queue>

namespace mv
{
    class Mouse
    {
        friend class MWindow;
        friend class Engine;

    public:
        class Event
        {
        public:
            enum class Type
            {
                LDown,
                LRelease,
                RDown,
                RRelease,
                MDown, // middle
                MRelease,
                WheelUp,
                WheelDown,
                Move,  // mouse move
                Leave, // leave client area
                Enter, // enter client area
                Invalid
            };

        public:
            Event() noexcept
                : type(Event::Type::Invalid),
                  left_is_pressed(false),
                  right_is_pressed(false),
                  middle_is_pressed(false),
                  x(0), y(0)
            {
            }
            Event(Mouse::Event::Type ty, const Mouse &parent) noexcept
                : type(ty),
                  left_is_pressed(parent.left_is_pressed),
                  right_is_pressed(parent.right_is_pressed),
                  middle_is_pressed(parent.middle_is_pressed),
                  x(parent.x), y(parent.y)
            {
            }
            bool is_valid(void) const noexcept
            {
                return type != Event::Type::Invalid;
            }
            Mouse::Event::Type get_type(void) const noexcept
            {
                return type;
            }
            std::pair<int, int> get_pos(void) const noexcept
            {
                return {x, y};
            }
            int get_pos_x(void) const noexcept
            {
                return x;
            }
            int get_pos_y(void) const noexcept
            {
                return y;
            }
            bool is_left_pressed(void) const noexcept
            {
                return left_is_pressed;
            }
            bool is_middle_pressed(void) const noexcept
            {
                return middle_is_pressed;
            }
            bool is_right_pressed(void) const noexcept
            {
                return right_is_pressed;
            }

        private:
            Type type;
            bool left_is_pressed;
            bool right_is_pressed;
            bool middle_is_pressed;
            int x;
            int y;
        };

    public:
        enum delta_style
        {
            from_center = 0,
            from_last_pos
        };
        Mouse(int window_width, int window_height, delta_style delta_calc_style)
        {
            this->window_width = window_width;
            this->window_height = window_height;
            this->delta_style = delta_calc_style;

            center_x = window_width / 2;
            center_y = window_height / 2;
        }
        Mouse(const Mouse &) = delete;
        Mouse &operator=(const Mouse &) = delete;
        std::pair<int, int> get_pos() const noexcept;
        std::pair<int, int> get_pos_delta(void) const noexcept;
        int get_pos_x(void) const noexcept;
        int get_pos_y(void) const noexcept;
        Mouse::Event read(void) noexcept;
        bool is_left_pressed(void) const noexcept;
        bool is_middle_pressed(void) const noexcept;
        bool is_right_pressed(void) const noexcept;
        bool is_in_window(void) const noexcept;
        int deltatest = 0;
        void set_delta_style(delta_style style)
        {
            this->delta_style = style;
            return;
        }
        bool is_empty(void) const noexcept
        {
            return buffer.empty();
        }
        void clear_state() noexcept;

        void update_window_spec(int n_width, int n_height)
        {
            this->window_width = n_width;
            this->window_height = n_height;

            center_x = window_width / 2;
            center_y = window_height / 2;
            return;
        }

    private:
        void on_mouse_move(int nx, int ny) noexcept;
        void on_left_press(int nx, int ny) noexcept;
        void on_right_press(int nx, int ny) noexcept;
        void on_left_release(int nx, int ny) noexcept;
        void on_right_release(int nx, int ny) noexcept;
        void on_middle_press(int nx, int ny) noexcept;
        void on_middle_release(int nx, int ny) noexcept;
        void on_wheel_up(int nx, int ny) noexcept;                // used internally -- use onDelta
        void on_wheel_down(int nx, int ny) noexcept;              // used internally -- use onDelta
        void on_wheel_delta(int nx, int ny, int wDelta) noexcept; // use this one for mouse wheel
        void on_mouse_enter(void) noexcept;
        void on_mouse_leave(void) noexcept;
        void trim_buffer(void) noexcept;

    public:
        static constexpr unsigned int max_buffer_size = 16u;

        int x = 0;
        int y = 0;
        int last_x = 0;
        int last_y = 0;
        int mouse_x_delta = 0;
        int mouse_y_delta = 0;
        int wheel_delta_carry = 0;

        int center_x = 0;
        int center_y = 0;

        int window_width = 0;
        int window_height = 0;

        bool left_is_pressed = false;
        bool middle_is_pressed = false;
        bool right_is_pressed = false;
        bool in_window = false;

        delta_style delta_style = delta_style::from_center;

        std::queue<Mouse::Event> buffer;
    };
};

#endif