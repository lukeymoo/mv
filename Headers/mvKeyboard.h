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
            Event(Event::Type t, uint16_t c) noexcept
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

        static const int key_count = 86;

        enum Key
        {
            l_shift,
            r_shift,
            l_ctrl,
            r_ctrl,
            l_alt,
            r_alt,
            l_super,
            r_super,
            menu,
            escape,
            semicolon,
            slash,
            equal,
            minus,
            l_bracket,
            r_bracket,
            comma,
            period,
            apostrophe,
            backslash,
            grave,
            space,
            sk_return,
            backspace,
            tab,
            prior,
            next,
            end,
            home,
            insert,
            sk_delete,
            pause,
            f1,
            f2,
            f3,
            f4,
            f5,
            f6,
            f7,
            f8,
            f9,
            f10,
            f11,
            f12,
            f13,
            f14,
            f15,
            left,
            right,
            up,
            down,
            a,
            b,
            c,
            d,
            e,
            f,
            g,
            h,
            i,
            j,
            k,
            l,
            m,
            n,
            o,
            p,
            q,
            r,
            s,
            t,
            u,
            v,
            w,
            x,
            y,
            z,
            sk_0,
            sk_1,
            sk_2,
            sk_3,
            sk_4,
            sk_5,
            sk_6,
            sk_7,
            sk_8,
            sk_9,
        };

        // enum Key
        // {
        //     l_shift = XK_Shift_L,
        //     r_shift = XK_Shift_R,
        //     l_ctrl = XK_Control_L,
        //     r_ctrl = XK_Control_R,
        //     a_alt = XK_Alt_L,
        //     r_alt = XK_Alt_R,
        //     l_super = XK_Super_L,
        //     r_super = XK_Super_R,
        //     menu = XK_Menu,
        //     escape = XK_Escape,
        //     semicolon = XK_semicolon,
        //     slash = XK_slash,
        //     equal = XK_equal,
        //     minus = XK_minus,
        //     l_bracket = XK_bracketleft,
        //     r_bracket = XK_bracketright,
        //     comma = XK_comma,
        //     period = XK_period,
        //     apostrophe = XK_apostrophe,
        //     backslash = XK_backslash,
        //     grave = XK_grave,
        //     space = XK_space,
        //     sk_return = XK_Return,
        //     backspace = XK_BackSpace,
        //     tab = XK_Tab,
        //     prior = XK_Prior,
        //     next = XK_Next,
        //     end = XK_End,
        //     home = XK_Home,
        //     insert = XK_Insert,
        //     sk_delete = XK_Delete,
        //     pause = XK_Pause,
        //     f1 = XK_F1,
        //     f2 = XK_F2,
        //     f3 = XK_F3,
        //     f4 = XK_F4,
        //     f5 = XK_F5,
        //     f6 = XK_F6,
        //     f7 = XK_F7,
        //     f8 = XK_F8,
        //     f9 = XK_F9,
        //     f10 = XK_F10,
        //     f11 = XK_F11,
        //     f12 = XK_F12,
        //     f13 = XK_F13,
        //     f14 = XK_F14,
        //     f15 = XK_F15,
        //     left = XK_Left,
        //     right = XK_Right,
        //     up = XK_Up,
        //     down = XK_Down,
        //     a = XK_a,
        //     b = XK_b,
        //     c = XK_c,
        //     d = XK_d,
        //     e = XK_e,
        //     f = XK_f,
        //     g = XK_g,
        //     h = XK_h,
        //     i = XK_i,
        //     j = XK_j,
        //     k = XK_k,
        //     l = XK_l,
        //     m = XK_m,
        //     n = XK_n,
        //     o = XK_o,
        //     p = XK_p,
        //     q = XK_q,
        //     r = XK_r,
        //     s = XK_s,
        //     t = XK_t,
        //     u = XK_u,
        //     v = XK_v,
        //     w = XK_w,
        //     x = XK_x,
        //     y = XK_y,
        //     z = XK_z,
        //     sk_0 = XK_0,
        //     sk_1 = XK_1,
        //     sk_2 = XK_2,
        //     sk_3 = XK_3,
        //     sk_4 = XK_4,
        //     sk_5 = XK_5,
        //     sk_6 = XK_6,
        //     sk_7 = XK_7,
        //     sk_8 = XK_8,
        //     sk_9 = XK_9,
        // }; // 86 keys

        /* Key event handling */
        bool is_key_pressed(uint16_t keycode);
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
        void on_key_press(uint16_t keycode);
        void on_key_release(uint16_t keycode);
        void on_char(uint16_t key);
        void clear_state(void);

        template <typename T>
        static void trim_buffer(std::queue<T> &buffer);

    private:
        static constexpr unsigned int n_keys = 86u;
        static constexpr unsigned int max_buffer_size = 16u;

        std::bitset<n_keys> keystates;
        std::queue<Keyboard::Event> keybuffer;
        std::queue<uint64_t> charbuffer;
    };
};

#endif