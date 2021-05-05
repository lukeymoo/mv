#ifndef HEADERS_MVMOUSE_H_
#define HEADERS_MVMOUSE_H_

// Appropriate for windows
// #define WHEEL_DELTA 120
#define WHEEL_DELTA 1

#include <X11/Xlib.h>
#include <queue>
#include <stdexcept>

namespace mv
{
    struct mouse
    {
        template <typename T>
        static void trim_buffer(std::queue<T> buffer, uint32_t max_size)
        {
            while (buffer.size() > max_size)
            {
                buffer.pop();
            }
            return;
        }
        /*
            Mouse event structure
        */
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
            event(etype type, const mv::mouse &parent) noexcept
            {
                this->type = type;
                this->x = parent.current_x;
                this->y = parent.current_y;
                this->is_left_pressed = parent.is_left_pressed;
                this->is_middle_pressed = parent.is_middle_pressed;
                this->is_right_pressed = parent.is_right_pressed;
                return;
            }
            ~event() {}

            etype type = etype::invalid;
            int x = 0;
            int y = 0;

            bool is_left_pressed = false;
            bool is_middle_pressed = false;
            bool is_right_pressed = false;

            inline bool is_left_button(void) const noexcept
            {
                return is_left_pressed;
            }
            inline bool is_middle_button(void) const noexcept
            {
                return is_middle_pressed;
            }
            inline bool is_right_button(void) const noexcept
            {
                return is_right_pressed;
            }

            inline bool is_valid(void) const noexcept
            {
                return (type != etype::invalid);
            }

            inline etype get_type(void) const noexcept
            {
                return type;
            }
        };

        /*
            End of mouse event structure
        */

    public:
        enum delta_calc_style
        {
            from_center = 0, // fps/freelook deltas
            from_last
        };

        mouse(Display *display, Window *window)
        {
            this->display = display;
            this->window = window;
        }
        ~mouse() {}

        static constexpr int max_buffer_size = 16;

    private:
        Display *display = nullptr;
        Window *window = nullptr;
        int window_width = 0;
        int window_height = 0;
        int center_x = 0;
        int center_y = 0;

        int last_x = 0;
        int last_y = 0;
        int current_x = 0;
        int current_y = 0;
        // delta from last x,y & current
        int delta_x = 0;
        int delta_y = 0;

        bool drag = false;
        int drag_startx = 0;
        int drag_starty = 0;
        // delta from drag start x,y & current
        int drag_delta_x = 0;
        int drag_delta_y = 0;

        bool is_left_pressed = false;
        bool is_middle_pressed = false;
        bool is_right_pressed = false;

        int wheel_delta_carry = 0;

        bool in_window = false;

        float custom_value = 0.0f;

        // how is delta_x/y calculated
        delta_calc_style delta_style = delta_calc_style::from_last;
        std::queue<mouse::event> mouse_buffer;

    public:
        inline void query_pointer(void)
        {
            if (display == nullptr)
            {
                throw std::runtime_error("Attempted to query mouse pointer but no valid x11 display handle was given to mouse handler");
            }
            if (window == nullptr)
            {
                throw std::runtime_error("Attempted to query mouse pointer but no x11 window has been given to mouse handler");
            }

            Window root, child;
            int gx, gy;
            unsigned int buttons;

            int mx = 0;
            int my = 0;
            XQueryPointer(display, (*window), &root, &child, &gx, &gy, &mx, &my, &buttons);

            last_x = current_x;
            last_y = current_y;
            current_x = mx;
            current_y = my;

            calculate_delta();
            return;
        }
        inline mouse::event read(void) noexcept
        {
            event e(mouse::event::etype::invalid, 0, 0, false, false, false);
            if (!mouse_buffer.empty())
            {
                e = mouse_buffer.front();
                mouse_buffer.pop();
            }
            return e;
        }
        inline void set_window_properties(int width, int height) noexcept
        {
            this->window_width = width;
            this->window_height = height;
            this->center_x = (width / 2);
            this->center_y = (height / 2);
            return;
        }
        inline void set_delta_style(delta_calc_style style)
        {
            if (style == delta_calc_style::from_center)
            {
                if (!window_width || !window_height)
                {
                    throw std::runtime_error("To calculate delta using from_center you must specify the "
                                             "window or swapchain width/height; Be aware of border widths etc\n");
                }
            }
            this->delta_style = style;
            return;
        }
        inline int get_x(void) const noexcept
        {
            return current_x;
        }
        inline int get_y(void) const noexcept
        {
            return current_y;
        }
        inline int get_last_x(void) const noexcept
        {
            return last_x;
        }
        inline int get_last_y(void) const noexcept
        {
            return last_y;
        }
        inline int get_delta_x(void) const noexcept
        {
            return delta_x;
        }
        inline int get_delta_y(void) const noexcept
        {
            return delta_y;
        }
        inline int get_drag_delta_startx(void) const noexcept
        {
            return drag_startx;
        }
        inline int get_drag_delta_starty(void) const noexcept
        {
            return drag_starty;
        }
        inline int get_drag_delta_x(void) const noexcept
        {
            return drag_delta_x;
        }
        inline int get_drag_delta_y(void) const noexcept
        {
            return drag_delta_y;
        }
        inline bool is_empty(void) const noexcept
        {
            return mouse_buffer.empty();
        }
        inline void start_drag(void) noexcept
        {
            drag_startx = current_x;
            drag_starty = current_y;
            drag = true;
            return;
        }
        inline void start_drag(float custom_val) noexcept
        {
            custom_value = custom_val;
            drag_startx = current_x;
            drag_starty = current_y;
            drag = true;
            return;
        }
        inline void end_drag(void) noexcept
        {
            custom_value = 0.0f;
            drag_startx = 0;
            drag_starty = 0;
            drag_delta_x = 0;
            drag_delta_y = 0;
            drag = false;
            return;
        }
        inline float get_stored(void) const noexcept
        {
            return custom_value;
        }
        inline bool is_dragging(void) const noexcept
        {
            return drag;
        }

        inline void calculate_delta(void)
        {
            // if dragging calculate drag delta
            if (drag)
            {
                drag_delta_x = current_x - drag_startx;
                drag_delta_y = current_y - drag_starty;
            }
            if (delta_style == delta_calc_style::from_center)
            {
                delta_x = current_x - center_x;
                delta_y = current_y - center_y;
            }
            else if (delta_style == delta_calc_style::from_last)
            {
                delta_x = current_x - last_x;
                delta_y = current_y - last_y;
            }
            return;
        }

        inline void on_left_press(int nx, int ny) noexcept
        {
            last_x = current_x;
            last_y = current_y;
            current_x = nx;
            current_y = ny;
            calculate_delta();
            is_left_pressed = true;
            mouse_buffer.push(event(event::etype::l_down, nx, ny, is_left_pressed, is_middle_pressed, is_right_pressed));
            trim_buffer(mouse_buffer, max_buffer_size);
            return;
        }
        inline void on_left_release(int nx, int ny) noexcept
        {
            last_x = current_x;
            last_y = current_y;
            current_x = nx;
            current_y = ny;
            calculate_delta();
            is_left_pressed = false;
            mouse_buffer.push(event(event::etype::l_release, nx, ny, is_left_pressed, is_middle_pressed, is_right_pressed));
            trim_buffer(mouse_buffer, max_buffer_size);
            return;
        }

        inline void on_right_press(int nx, int ny) noexcept
        {
            last_x = current_x;
            last_y = current_y;
            current_x = nx;
            current_y = ny;
            calculate_delta();
            is_right_pressed = true;
            mouse_buffer.push(event(event::etype::r_down, nx, ny, is_left_pressed, is_middle_pressed, is_right_pressed));
            trim_buffer(mouse_buffer, max_buffer_size);
            return;
        }
        inline void on_right_release(int nx, int ny) noexcept
        {
            last_x = current_x;
            last_y = current_y;
            current_x = nx;
            current_y = ny;
            calculate_delta();
            is_right_pressed = false;
            mouse_buffer.push(event(event::etype::r_release, nx, ny, is_left_pressed, is_middle_pressed, is_right_pressed));
            trim_buffer(mouse_buffer, max_buffer_size);
            return;
        }

        inline void on_middle_press(int nx, int ny) noexcept
        {
            last_x = current_x;
            last_y = current_y;
            current_x = nx;
            current_y = ny;
            calculate_delta();
            is_middle_pressed = true;
            mouse_buffer.push(event(event::etype::m_down, nx, ny, is_left_pressed, is_middle_pressed, is_right_pressed));
            trim_buffer(mouse_buffer, max_buffer_size);
            return;
        }
        inline void on_middle_release(int nx, int ny) noexcept
        {
            last_x = current_x;
            last_y = current_y;
            current_x = nx;
            current_y = ny;
            calculate_delta();
            is_middle_pressed = false;
            mouse_buffer.push(event(event::etype::m_release, nx, ny, is_left_pressed, is_middle_pressed, is_right_pressed));
            trim_buffer(mouse_buffer, max_buffer_size);
            return;
        }

        // used internally -- use onDelta
        inline void on_wheel_up(int nx, int ny) noexcept
        {
            last_x = current_x;
            last_y = current_y;
            current_x = nx;
            current_y = ny;
            calculate_delta();
            mouse_buffer.push(event(event::etype::wheel_up, nx, ny, is_left_pressed, is_middle_pressed, is_right_pressed));
            trim_buffer(mouse_buffer, max_buffer_size);
            return;
        }
        // used internally -- use onDelta
        inline void on_wheel_down(int nx, int ny) noexcept
        {
            last_x = current_x;
            last_y = current_y;
            current_x = nx;
            current_y = ny;
            calculate_delta();
            mouse_buffer.push(event(event::etype::wheel_down, nx, ny, is_left_pressed, is_middle_pressed, is_right_pressed));
            trim_buffer(mouse_buffer, max_buffer_size);
            return;
        }
        // use this one for mouse wheel
        inline void on_wheel_delta(int nx, int ny, int w_delta) noexcept
        {
            wheel_delta_carry += w_delta;
            if (wheel_delta_carry >= WHEEL_DELTA)
            {
                wheel_delta_carry -= WHEEL_DELTA;
                on_wheel_up(nx, ny);
            }
            else if (wheel_delta_carry <= -WHEEL_DELTA)
            {
                wheel_delta_carry += WHEEL_DELTA;
                on_wheel_down(nx, ny);
            }
            return;
        }
        inline void on_mouse_enter(void) noexcept
        {
            in_window = true;
            mouse_buffer.push(event(event::etype::enter, *this));
            trim_buffer(mouse_buffer, max_buffer_size);
            return;
        }
        inline void on_mouse_leave(void) noexcept
        {
            in_window = false;
            mouse_buffer.push(event(event::etype::leave, *this));
            trim_buffer(mouse_buffer, max_buffer_size);
            return;
        }
    };
};

#endif