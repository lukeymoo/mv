#include "mvInput.h"

/*
  KEYBOARD METHODS
*/
template <typename T> void mv::Keyboard::trim_buffer(std::queue<T> buffer, uint32_t max_size)
{
    while (buffer.size() > max_size)
    {
        buffer.pop();
    }
    return;
}

mv::Keyboard::event mv::Keyboard::read(void)
{
    Keyboard::event e(Keyboard::event::etype::invalid, GLFW_KEY_UNKNOWN);
    if (!key_buffer.empty())
    {
        e = key_buffer.front();
        key_buffer.pop();
    }
    return e;
}

void mv::Keyboard::on_key_press(int code)
{
    key_states[code] = true;
    key_buffer.push(Keyboard::event(Keyboard::event::etype::press, code));
    trim_buffer(key_buffer, mv::Keyboard::max_buffer_size);
    return;
}

void mv::Keyboard::on_key_release(int code)
{
    key_states[code] = false;
    key_buffer.push(Keyboard::event(Keyboard::event::etype::release, code));
    trim_buffer(key_buffer, mv::Keyboard::max_buffer_size);
    return;
}

void mv::Keyboard::clear_state(void)
{
    key_states.reset();
    return;
}

bool mv::Keyboard::is_keystate(int code)
{
    // sanity check
    if (code < 0)
    {
        return false;
    }
    return mv::Keyboard::key_states[code];
}

bool mv::Keyboard::is_key(GLFWwindow *window, int code)
{
    // sanity check
    if (code < 0)
    {
        return false;
    }
    return glfwGetKey(window, code);
}

/*
  MOUSE METHODS
*/
template <typename T> void mv::Mouse::trim_buffer(std::queue<T> buffer, uint32_t max_size)
{
    while (buffer.size() > max_size)
    {
        buffer.pop();
    }
    return;
}

void mv::Mouse::update(int nx, int ny) noexcept
{
    last_x = current_x;
    last_y = current_y;
    current_x = nx;
    current_y = ny;
    calculate_delta();
    // don't push motion events to buffer
    // theyll quickly crowd out button presses/scroll events
    return;
}

mv::Mouse::event mv::Mouse::read(void) noexcept
{
    Mouse::event e(Mouse::event::etype::invalid, 0, 0, false, false, false);
    if (!mouse_buffer.empty())
    {
        e = mouse_buffer.front();
        mouse_buffer.pop();
    }
    return e;
}

void mv::Mouse::on_left_press(void) noexcept
{
    is_left_pressed = true;
    mouse_buffer.push(
        event(event::etype::l_down, current_x, current_y, is_left_pressed, is_middle_pressed, is_right_pressed));
    trim_buffer(mouse_buffer, max_buffer_size);
    return;
}
void mv::Mouse::on_left_release(void) noexcept
{
    is_left_pressed = false;
    mouse_buffer.push(
        event(event::etype::l_release, current_x, current_y, is_left_pressed, is_middle_pressed, is_right_pressed));
    trim_buffer(mouse_buffer, max_buffer_size);
    return;
}

void mv::Mouse::on_right_press(void) noexcept
{
    is_right_pressed = true;
    mouse_buffer.push(
        event(event::etype::r_down, current_x, current_y, is_left_pressed, is_middle_pressed, is_right_pressed));
    trim_buffer(mouse_buffer, max_buffer_size);
    return;
}
void mv::Mouse::on_right_release(void) noexcept
{
    is_right_pressed = false;
    mouse_buffer.push(
        event(event::etype::r_release, current_x, current_y, is_left_pressed, is_middle_pressed, is_right_pressed));
    trim_buffer(mouse_buffer, max_buffer_size);
    return;
}

void mv::Mouse::on_middle_press(void) noexcept
{
    is_middle_pressed = true;
    mouse_buffer.push(
        event(event::etype::m_down, current_x, current_y, is_left_pressed, is_middle_pressed, is_right_pressed));
    trim_buffer(mouse_buffer, max_buffer_size);
    return;
}
void mv::Mouse::on_middle_release(void) noexcept
{
    is_middle_pressed = false;
    mouse_buffer.push(
        event(event::etype::m_release, current_x, current_y, is_left_pressed, is_middle_pressed, is_right_pressed));
    trim_buffer(mouse_buffer, max_buffer_size);
    return;
}

// used internally -- use onDelta
void mv::Mouse::on_wheel_up(int nx, int ny) noexcept
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
void mv::Mouse::on_wheel_down(int nx, int ny) noexcept
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

void mv::Mouse::calculate_delta(void) noexcept
{
    // if dragging calculate drag delta
    if (is_dragging)
    {
        // drag_delta_x = current_x - drag_startx;
        // drag_delta_y = current_y - drag_starty;
        drag_delta_x = current_x - last_x;
        drag_delta_y = current_y - last_y;
    }
    if (delta_style == delta_styles::from_center)
    {
        delta_x = current_x - center_x;
        delta_y = current_y - center_y;
    }
    else if (delta_style == delta_styles::from_last)
    {
        delta_x = current_x - last_x;
        delta_y = current_y - last_y;
    }
    return;
}

void mv::Mouse::start_drag(void) noexcept
{
    drag_startx = current_x;
    drag_starty = current_y;
    is_dragging = true;
    return;
}
void mv::Mouse::start_drag(float orbit, float pitch) noexcept
{
    stored_orbit = orbit;
    stored_pitch = pitch;
    drag_startx = current_x;
    drag_starty = current_y;
    is_dragging = true;
    return;
}
void mv::Mouse::end_drag(void) noexcept
{
    stored_orbit = 0.0f;
    stored_pitch = 0.0f;
    drag_startx = 0;
    drag_starty = 0;
    drag_delta_x = 0;
    drag_delta_y = 0;
    is_dragging = false;
    return;
}

void mv::Mouse::clear(void) noexcept
{
    delta_x = 0;
    delta_y = 0;
    drag_delta_x = 0;
    drag_delta_y = 0;
    return;
}