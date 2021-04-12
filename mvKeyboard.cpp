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
    bool Keyboard::isKeyPressed(unsigned char keycode)
    {
        return keystates[keycode];
    }

    Keyboard::Event Keyboard::readKey(void)
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

    bool Keyboard::isKeyEmpty(void)
    {
        return keybuffer.empty();
    }

    void Keyboard::clearKey(void)
    {
        keybuffer = std::queue<Keyboard::Event>();
    }

    /*
    Char event handling
*/
    char Keyboard::readChar(void)
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

    bool Keyboard::isCharEmpty(void)
    {
        return charbuffer.empty();
    }

    void Keyboard::clearChar(void)
    {
        charbuffer = std::queue<char>();
        return;
    }

    // Clears both
    void Keyboard::clear(void)
    {
        clearChar();
        clearKey();
        return;
    }

    /*
    Generates events for our handler
    Manages buffer
*/
    void Keyboard::onKeyPress(unsigned char keycode)
    {
        keystates[keycode] = true;
        keybuffer.push(Keyboard::Event(Keyboard::Event::Type::Press, keycode));
        trimBuffer(keybuffer);
        return;
    }

    void Keyboard::onKeyRelease(unsigned char keycode)
    {
        keystates[keycode] = false;
        keybuffer.push(Keyboard::Event(Keyboard::Event::Type::Release, keycode));
        trimBuffer(keybuffer);
        return;
    }

    void Keyboard::onChar(unsigned char key)
    {
        charbuffer.push(key);
        return;
    }

    void Keyboard::clearState(void)
    {
        keystates.reset();
    }

    template <typename T>
    void Keyboard::trimBuffer(std::queue<T> &buffer)
    {
        while (buffer.size() > maxBufferSize)
        {
            buffer.pop();
        }
        return;
    }
};