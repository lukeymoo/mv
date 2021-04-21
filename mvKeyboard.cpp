#include "mvKeyboard.h"

namespace mv
{
    Keyboard::Keyboard()
    {
        return;
    }

    Keyboard::~Keyboard()
    {
        return;
    }

    /*
        Key events
    */
    bool Keyboard::is_key_pressed(unsigned char keycode)
    {
        return keystates[keycode];
    }

    Keyboard::Event Keyboard::read_key(void)
    {
        // Ensure buffer is not empty
        if (keybuffer.size() > 0)
        {
            Keyboard::Event e = keybuffer.front();
            keybuffer.pop();
            return e;
        }
        else
        {
            return Keyboard::Event();
        }
    }

    bool Keyboard::is_key_empty(void)
    {
        return keybuffer.empty();
    }

    void Keyboard::clear_key(void)
    {
        keybuffer = std::queue<Keyboard::Event>();
    }

    /*
    Char event handling
*/
    char Keyboard::read_char(void)
    {
        // Ensure not empty
        if (charbuffer.size() > 0)
        {
            unsigned char c = charbuffer.front();
            charbuffer.pop();
            return c;
        }
        else
        {
            return 0;
        }
    }

    bool Keyboard::is_char_empty(void)
    {
        return charbuffer.empty();
    }

    void Keyboard::clear_char(void)
    {
        charbuffer = std::queue<char>();
        return;
    }

    // Clears both
    void Keyboard::clear(void)
    {
        clear_char();
        clear_key();
        return;
    }

    /*
    Generates events for our handler
    Manages buffer
*/
    void Keyboard::on_key_press(unsigned char keycode)
    {
        keystates[keycode] = true;
        keybuffer.push(Keyboard::Event(Keyboard::Event::Type::Press, keycode));
        trim_buffer(keybuffer);
        return;
    }

    void Keyboard::on_key_release(unsigned char keycode)
    {
        keystates[keycode] = false;
        keybuffer.push(Keyboard::Event(Keyboard::Event::Type::Release, keycode));
        trim_buffer(keybuffer);
        return;
    }

    void Keyboard::on_char(unsigned char key)
    {
        charbuffer.push(key);
        return;
    }

    void Keyboard::clear_state(void)
    {
        keystates.reset();
    }

    template <typename T>
    void Keyboard::trim_buffer(std::queue<T> &buffer)
    {
        while (buffer.size() > max_buffer_size)
        {
            buffer.pop();
        }
        return;
    }
};