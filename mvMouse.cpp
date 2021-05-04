#include "mvMouse.h"

namespace mv
{
    /* Getter functions */
    std::pair<int, int> Mouse::get_pos() const noexcept
    {
        return {x, y};
    }

    std::pair<int, int> Mouse::get_pos_delta(void) const noexcept
    {
        return {mouse_x_delta, mouse_y_delta};
    }

    int Mouse::get_pos_x(void) const noexcept
    {
        return x;
    }

    int Mouse::get_pos_y(void) const noexcept
    {
        return y;
    }

    Mouse::Event Mouse::read(void) noexcept
    {
        // Ensure non empty buffer
        if (buffer.size() > 0)
        {
            Mouse::Event e;
            e = buffer.front();
            buffer.pop();
            return e;
        }
        else
        {
            return Mouse::Event();
        }
    }

    bool Mouse::is_left_pressed(void) const noexcept
    {
        return left_is_pressed;
    }

    bool Mouse::is_middle_pressed(void) const noexcept
    {
        return middle_is_pressed;
    }

    bool Mouse::is_right_pressed(void) const noexcept
    {
        return right_is_pressed;
    }

    bool Mouse::is_in_window(void) const noexcept
    {
        return in_window;
    }

    /*
    On event handlers
    Internal Functions
*/
    void Mouse::clear_state() noexcept
    {
        buffer = std::queue<Mouse::Event>();
        return;
    }

    void Mouse::on_mouse_move(int nx, int ny) noexcept
    {
        last_x = x;
        last_y = y;
        x = nx;
        y = ny;
        if (delta_style == delta_style::from_last_pos)
        {
            mouse_x_delta = x - last_x;
            mouse_y_delta = last_y - y;
        }
        else
        {
            mouse_x_delta = x - center_x;
            mouse_y_delta = y - center_y;
        }

        buffer.push(Mouse::Event(Mouse::Event::Type::Move, *this));
        trim_buffer();
        return;
    }

    void Mouse::on_left_press(int nx, int ny) noexcept
    {
        last_x = x;
        last_y = y;
        x = nx;
        y = ny;
        if (delta_style == delta_style::from_last_pos)
        {
            mouse_x_delta = x - last_x;
            mouse_y_delta = last_y - y;
        }
        else
        {
            mouse_x_delta = x - center_x;
            mouse_y_delta = y - center_y;
        }

        left_is_pressed = true;
        buffer.push(Mouse::Event(Mouse::Event::Type::LDown, *this));
        trim_buffer();
        return;
    }

    void Mouse::on_right_press(int nx, int ny) noexcept
    {
        last_x = x;
        last_y = y;
        x = nx;
        y = ny;
        if (delta_style == delta_style::from_last_pos)
        {
            mouse_x_delta = x - last_x;
            mouse_y_delta = last_y - y;
        }
        else
        {
            mouse_x_delta = x - center_x;
            mouse_y_delta = y - center_y;
        }

        right_is_pressed = true;
        buffer.push(Mouse::Event(Mouse::Event::Type::RDown, *this));
        trim_buffer();
        return;
    }

    void Mouse::on_left_release(int nx, int ny) noexcept
    {
        last_x = x;
        last_y = y;
        x = nx;
        y = ny;
        if (delta_style == delta_style::from_last_pos)
        {
            mouse_x_delta = x - last_x;
            mouse_y_delta = last_y - y;
        }
        else
        {
            mouse_x_delta = x - center_x;
            mouse_y_delta = y - center_y;
        }

        left_is_pressed = false;
        buffer.push(Mouse::Event(Mouse::Event::Type::LRelease, *this));
        trim_buffer();
        return;
    }

    void Mouse::on_right_release(int nx, int ny) noexcept
    {
        last_x = x;
        last_y = y;
        x = nx;
        y = ny;
        if (delta_style == delta_style::from_last_pos)
        {
            mouse_x_delta = x - last_x;
            mouse_y_delta = last_y - y;
        }
        else
        {
            mouse_x_delta = x - center_x;
            mouse_y_delta = y - center_y;
        }

        right_is_pressed = false;
        buffer.push(Mouse::Event(Mouse::Event::Type::RRelease, *this));
        trim_buffer();
        return;
    }

    void Mouse::on_middle_press(int nx, int ny) noexcept
    {
        last_x = x;
        last_y = y;
        x = nx;
        y = ny;
        if (delta_style == delta_style::from_last_pos)
        {
            mouse_x_delta = x - last_x;
            mouse_y_delta = last_y - y;
        }
        else
        {
            mouse_x_delta = x - center_x;
            mouse_y_delta = y - center_y;
        }

        middle_is_pressed = true;
        buffer.push(Mouse::Event(Mouse::Event::Type::MDown, *this));
        trim_buffer();
        return;
    }

    void Mouse::on_middle_release(int nx, int ny) noexcept
    {
        last_x = x;
        last_y = y;
        x = nx;
        y = ny;
        if (delta_style == delta_style::from_last_pos)
        {
            mouse_x_delta = x - last_x;
            mouse_y_delta = last_y - y;
        }
        else
        {
            mouse_x_delta = x - center_x;
            mouse_y_delta = y - center_y;
        }

        middle_is_pressed = false;
        buffer.push(Mouse::Event(Mouse::Event::Type::MRelease, *this));
        trim_buffer();
        return;
    }

    void Mouse::on_wheel_up(int nx, int ny) noexcept
    {
        last_x = x;
        last_y = y;
        x = nx;
        y = ny;
        if (delta_style == delta_style::from_last_pos)
        {
            mouse_x_delta = x - last_x;
            mouse_y_delta = last_y - y;
        }
        else
        {
            mouse_x_delta = x - center_x;
            mouse_y_delta = y - center_y;
        }

        buffer.push(Mouse::Event(Mouse::Event::Type::WheelUp, *this));
        trim_buffer();
        return;
    }

    void Mouse::on_wheel_down(int nx, int ny) noexcept
    {
        last_x = x;
        last_y = y;
        x = nx;
        y = ny;
        if (delta_style == delta_style::from_last_pos)
        {
            mouse_x_delta = x - last_x;
            mouse_y_delta = last_y - y;
        }
        else
        {
            mouse_x_delta = x - center_x;
            mouse_y_delta = y - center_y;
        }

        buffer.push(Mouse::Event(Mouse::Event::Type::WheelDown, *this));
        trim_buffer();
        return;
    }

    void Mouse::on_wheel_delta(int nx, int ny, int wDelta) noexcept
    {
        wheel_delta_carry += wDelta;
        // Generates wheel event when delta exceeds WHEEL_DELTA
        if (wheel_delta_carry >= WHEEL_DELTA)
        {
            wheel_delta_carry -= WHEEL_DELTA; // reset delta to near 0
            on_wheel_up(nx, ny);
        }
        else if (wheel_delta_carry <= -WHEEL_DELTA)
        {
            wheel_delta_carry += WHEEL_DELTA;
            on_wheel_down(nx, ny);
        }
        return;
    }

    void Mouse::on_mouse_enter(void) noexcept
    {
        in_window = true;
        buffer.push(Mouse::Event(Mouse::Event::Type::Enter, *this));
        trim_buffer();
        return;
    }

    void Mouse::on_mouse_leave(void) noexcept
    {
        in_window = false;
        buffer.push(Mouse::Event(Mouse::Event::Type::Leave, *this));
        trim_buffer();
        return;
    }

    void Mouse::trim_buffer(void) noexcept
    {
        while (buffer.size() > max_buffer_size)
        {
            buffer.pop();
        }
        return;
    }
};