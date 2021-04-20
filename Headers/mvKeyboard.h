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
            Keyboard::Event::Type get_type(void) const noexcept
            {
                return type;
            }
            bool is_press(void) const noexcept
            {
                return type == Type::Press;
            }
            bool is_release(void) const noexcept
            {
                return type == Type::Release;
            }
            bool is_valid(void) const noexcept
            {
                return type == Type::Invalid;
            }
            unsigned char get_code(void) const noexcept
            {
                return code;
            }
        };

    public:
        Keyboard();
        ~Keyboard();

        /* Key event handling */
        bool is_key_pressed(unsigned char keycode);
        Keyboard::Event read_key(void);
        bool is_key_empty(void);
        void clear_key(void);

        /* Char event handling */
        char read_char(void);
        bool is_char_empty(void);
        void clear_char(void);

        // handles both
        void clear(void);

    private:
        /* Handler */
        void on_key_press(unsigned char keycode);
        void on_key_release(unsigned char keycode);
        void on_char(unsigned char key);
        void clear_state(void);

        template <typename T>
        static void trim_buffer(std::queue<T> &buffer);

    private:
        static constexpr unsigned int n_keys = 256u;
        static constexpr unsigned int max_buffer_size = 16u;

        std::bitset<n_keys> keystates;
        std::queue<Keyboard::Event> keybuffer;
        std::queue<char> charbuffer;
    };
};

#endif