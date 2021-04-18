#ifndef HEADERS_MVKEYBOARD_H_
#define HEADERS_MVKEYBOARD_H_

#include <bitset>
#include <queue>

namespace mv
{
    class Keyboard
    {
        friend class MWindow;

    public:
        class Event
        {
        public:
            enum class Type
            {
                Press,
                Release,
                Invalid
            };

        private:
            Type type;
            unsigned char code;

        public:
            Event() noexcept
                : type(Event::Type::Invalid), code(0)
            {
            }
            Event(Event::Type t, unsigned char c) noexcept
                : type(t), code(c)
            {
            }
            Keyboard::Event::Type getType(void) const noexcept
            {
                return type;
            }
            bool isPress(void) const noexcept
            {
                return type == Type::Press;
            }
            bool isRelease(void) const noexcept
            {
                return type == Type::Release;
            }
            bool isValid(void) const noexcept
            {
                return type == Type::Invalid;
            }
            unsigned char getCode(void) const noexcept
            {
                return code;
            }
        };

    public:
        Keyboard();
        ~Keyboard();

        /* Key event handling */
        bool isKeyPressed(unsigned char keycode);
        Keyboard::Event readKey(void);
        bool isKeyEmpty(void);
        void clearKey(void);

        /* Char event handling */
        char readChar(void);
        bool isCharEmpty(void);
        void clearChar(void);

        // handles both
        void clear(void);

    private:
        /* Handler */
        void onKeyPress(unsigned char keycode);
        void onKeyRelease(unsigned char keycode);
        void onChar(unsigned char key);
        void clearState(void);

        template <typename T>
        static void trimBuffer(std::queue<T> &buffer);

    private:
        static constexpr unsigned int nKeys = 256u;
        static constexpr unsigned int maxBufferSize = 16u;

        std::bitset<nKeys> keystates;
        std::queue<Keyboard::Event> keybuffer;
        std::queue<char> charbuffer;
    };
};

#endif