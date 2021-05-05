#ifndef HEADERS_MVKEYBOARD_H_
#define HEADERS_MVKEYBOARD_H_

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/XF86keysym.h>
#include <X11/Xutil.h>
#include <bitset>
#include <queue>
#include <stdexcept>


/*
    ~~~ IMPORTANT ~~~
    IF NEW KEYS ARE ADDED TO ENUM BE SURE TO MODIFY max_key_count
*/

namespace mv
{
    struct keyboard
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
        
        keyboard(Display *display) noexcept
        {
            this->display = display;
        }
        ~keyboard() {}

        enum key
        {
            invalid = 0,
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

        struct event
        {
            enum etype
            {
                press,
                release,
                invalid
            };
            event(etype type, key code) noexcept
            {
                this->type = type;
                this->code = code;
            }
            ~event() {}
            etype type = etype::invalid;
            mv::keyboard::key code = key::invalid;

            inline etype get_type(void) const noexcept
            {
                return type;
            }
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
            inline key get_code(void)
            {
                return code;
            }
        };

    public:
        inline bool is_key(Display *display, mv::keyboard::key mv_key_code)
        {
            if (display == nullptr)
            {
                throw std::runtime_error("Keyboard handler pointer to x11 display is nullptr");
            }
            if ((int)mv_key_code < 0 || (int)mv_key_code > mv::keyboard::max_key_count)
            {
                return false;
            }
            KeySym key_sym;
            switch (mv_key_code)
            {
            case mv::keyboard::key::l_shift:
                key_sym = XK_Shift_L;
                break;
            case mv::keyboard::key::r_shift:
                key_sym = XK_Shift_R;
                break;
            case mv::keyboard::key::l_ctrl:
                key_sym = XK_Control_L;
                break;
            case mv::keyboard::key::r_ctrl:
                key_sym = XK_Control_R;
                break;
            case mv::keyboard::key::l_alt:
                key_sym = XK_Alt_L;
                break;
            case mv::keyboard::key::r_alt:
                key_sym = XK_Alt_R;
                break;
            case mv::keyboard::key::l_super:
                key_sym = XK_Super_L;
                break;
            case mv::keyboard::key::r_super:
                key_sym = XK_Super_R;
                break;
            case mv::keyboard::key::menu:
                key_sym = XK_Menu;
                break;
            case mv::keyboard::key::escape:
                key_sym = XK_Escape;
                break;
            case mv::keyboard::key::semicolon:
                key_sym = XK_semicolon;
                break;
            case mv::keyboard::key::slash:
                key_sym = XK_slash;
                break;
            case mv::keyboard::key::equal:
                key_sym = XK_equal;
                break;
            case mv::keyboard::key::minus:
                key_sym = XK_minus;
                break;
            case mv::keyboard::key::l_bracket:
                key_sym = XK_bracketleft;
                break;
            case mv::keyboard::key::r_bracket:
                key_sym = XK_bracketright;
                break;
            case mv::keyboard::key::comma:
                key_sym = XK_comma;
                break;
            case mv::keyboard::key::period:
                key_sym = XK_period;
                break;
            case mv::keyboard::key::apostrophe:
                key_sym = XK_apostrophe;
                break;
            case mv::keyboard::key::backslash:
                key_sym = XK_backslash;
                break;
            case mv::keyboard::key::grave:
                key_sym = XK_grave;
                break;
            case mv::keyboard::key::space:
                key_sym = XK_space;
                break;
            case mv::keyboard::key::sk_return:
                key_sym = XK_Return;
                break;
            case mv::keyboard::key::backspace:
                key_sym = XK_BackSpace;
                break;
            case mv::keyboard::key::tab:
                key_sym = XK_Tab;
                break;
            case mv::keyboard::key::prior:
                key_sym = XK_Prior;
                break;
            case mv::keyboard::key::next:
                key_sym = XK_Next;
                break;
            case mv::keyboard::key::end:
                key_sym = XK_End;
                break;
            case mv::keyboard::key::home:
                key_sym = XK_Home;
                break;
            case mv::keyboard::key::insert:
                key_sym = XK_Insert;
                break;
            case mv::keyboard::key::sk_delete:
                key_sym = XK_Delete;
                break;
            case mv::keyboard::key::pause:
                key_sym = XK_Pause;
                break;
            case mv::keyboard::key::f1:
                key_sym = XK_F1;
                break;
            case mv::keyboard::key::f2:
                key_sym = XK_F2;
                break;
            case mv::keyboard::key::f3:
                key_sym = XK_F3;
                break;
            case mv::keyboard::key::f4:
                key_sym = XK_F4;
                break;
            case mv::keyboard::key::f5:
                key_sym = XK_F5;
                break;
            case mv::keyboard::key::f6:
                key_sym = XK_F6;
                break;
            case mv::keyboard::key::f7:
                key_sym = XK_F7;
                break;
            case mv::keyboard::key::f8:
                key_sym = XK_F8;
                break;
            case mv::keyboard::key::f9:
                key_sym = XK_F9;
                break;
            case mv::keyboard::key::f10:
                key_sym = XK_F10;
                break;
            case mv::keyboard::key::f11:
                key_sym = XK_F11;
                break;
            case mv::keyboard::key::f12:
                key_sym = XK_F12;
                break;
            case mv::keyboard::key::f13:
                key_sym = XK_F13;
                break;
            case mv::keyboard::key::f14:
                key_sym = XK_F14;
                break;
            case mv::keyboard::key::f15:
                key_sym = XK_F15;
                break;
            case mv::keyboard::key::left:
                key_sym = XK_Left;
                break;
            case mv::keyboard::key::right:
                key_sym = XK_Right;
                break;
            case mv::keyboard::key::up:
                key_sym = XK_Up;
                break;
            case mv::keyboard::key::down:
                key_sym = XK_Down;
                break;
            case mv::keyboard::key::a:
                key_sym = XK_a;
                break;
            case mv::keyboard::key::b:
                key_sym = XK_b;
                break;
            case mv::keyboard::key::c:
                key_sym = XK_c;
                break;
            case mv::keyboard::key::d:
                key_sym = XK_d;
                break;
            case mv::keyboard::key::e:
                key_sym = XK_e;
                break;
            case mv::keyboard::key::f:
                key_sym = XK_f;
                break;
            case mv::keyboard::key::g:
                key_sym = XK_g;
                break;
            case mv::keyboard::key::h:
                key_sym = XK_h;
                break;
            case mv::keyboard::key::i:
                key_sym = XK_i;
                break;
            case mv::keyboard::key::j:
                key_sym = XK_j;
                break;
            case mv::keyboard::key::k:
                key_sym = XK_k;
                break;
            case mv::keyboard::key::l:
                key_sym = XK_l;
                break;
            case mv::keyboard::key::m:
                key_sym = XK_m;
                break;
            case mv::keyboard::key::n:
                key_sym = XK_n;
                break;
            case mv::keyboard::key::o:
                key_sym = XK_o;
                break;
            case mv::keyboard::key::p:
                key_sym = XK_p;
                break;
            case mv::keyboard::key::q:
                key_sym = XK_q;
                break;
            case mv::keyboard::key::r:
                key_sym = XK_r;
                break;
            case mv::keyboard::key::s:
                key_sym = XK_s;
                break;
            case mv::keyboard::key::t:
                key_sym = XK_t;
                break;
            case mv::keyboard::key::u:
                key_sym = XK_u;
                break;
            case mv::keyboard::key::v:
                key_sym = XK_v;
                break;
            case mv::keyboard::key::w:
                key_sym = XK_w;
                break;
            case mv::keyboard::key::x:
                key_sym = XK_x;
                break;
            case mv::keyboard::key::y:
                key_sym = XK_y;
                break;
            case mv::keyboard::key::z:
                key_sym = XK_z;
                break;
            case mv::keyboard::key::sk_0:
                key_sym = XK_0;
                break;
            case mv::keyboard::key::sk_1:
                key_sym = XK_1;
                break;
            case mv::keyboard::key::sk_2:
                key_sym = XK_2;
                break;
            case mv::keyboard::key::sk_3:
                key_sym = XK_3;
                break;
            case mv::keyboard::key::sk_4:
                key_sym = XK_4;
                break;
            case mv::keyboard::key::sk_5:
                key_sym = XK_5;
                break;
            case mv::keyboard::key::sk_6:
                key_sym = XK_6;
                break;
            case mv::keyboard::key::sk_7:
                key_sym = XK_7;
                break;
            case mv::keyboard::key::sk_8:
                key_sym = XK_8;
                break;
            case mv::keyboard::key::sk_9:
                key_sym = XK_9;
                break;
            default:
                key_sym = 0;
                break;
            }

            KeyCode key_code = XKeysymToKeycode(display, key_sym);

            if (key_code == 0)
            {
                return false;
            }

            char keys[32];
            XQueryKeymap(display, keys);

            bool status = ((keys[key_code / 8] & (1 << (key_code % 8))) != 0);

            // update keystates
            key_states[mv_key_code] = status;

            return status;
        }

        inline bool is_keystate(key mv_key_code) const noexcept
        {
            // sanity check
            if (mv_key_code < 0 || mv_key_code > max_key_count)
            {
                return false;
            }
            return key_states[mv_key_code];
        }

        inline mv::keyboard::key x11_to_mvkey(Display *display, KeyCode x_key_code) const noexcept
        {
            // sanity check
            if (x_key_code == 0)
            {
                return keyboard::key::invalid;
            }
            KeySym key_sym = XkbKeycodeToKeysym(display, x_key_code, 0, 0);
            if (key_sym == 0)
            {
                return keyboard::key::invalid;
            }

            // convert to our mv key code
            mv::keyboard::key mv_key_code = mv::keyboard::key::invalid;
            switch (key_sym)
            {
            case XK_Shift_L:
                mv_key_code = mv::keyboard::key::l_shift;
                break;
            case XK_Shift_R:
                mv_key_code = mv::keyboard::key::r_shift;
                break;
            case XK_Control_L:
                mv_key_code = mv::keyboard::key::l_ctrl;
                break;
            case XK_Control_R:
                mv_key_code = mv::keyboard::key::r_ctrl;
                break;
            case XK_Alt_L:
                mv_key_code = mv::keyboard::key::l_alt;
                break;
            case XK_Alt_R:
                mv_key_code = mv::keyboard::key::r_alt;
                break;
            case XK_Super_L:
                mv_key_code = mv::keyboard::key::l_super;
                break;
            case XK_Super_R:
                mv_key_code = mv::keyboard::key::r_super;
                break;
            case XK_Menu:
                mv_key_code = mv::keyboard::key::menu;
                break;
            case XK_Escape:
                mv_key_code = mv::keyboard::key::escape;
                break;
            case XK_semicolon:
                mv_key_code = mv::keyboard::key::semicolon;
                break;
            case XK_slash:
                mv_key_code = mv::keyboard::key::slash;
                break;
            case XK_equal:
                mv_key_code = mv::keyboard::key::equal;
                break;
            case XK_minus:
                mv_key_code = mv::keyboard::key::minus;
                break;
            case XK_bracketleft:
                mv_key_code = mv::keyboard::key::l_bracket;
                break;
            case XK_bracketright:
                mv_key_code = mv::keyboard::key::r_bracket;
                break;
            case XK_comma:
                mv_key_code = mv::keyboard::key::comma;
                break;
            case XK_period:
                mv_key_code = mv::keyboard::key::period;
                break;
            case XK_apostrophe:
                mv_key_code = mv::keyboard::key::apostrophe;
                break;
            case XK_backslash:
                mv_key_code = mv::keyboard::key::backslash;
                break;
            case XK_grave:
                mv_key_code = mv::keyboard::key::grave;
                break;
            case XK_space:
                mv_key_code = mv::keyboard::key::space;
                break;
            case XK_Return:
                mv_key_code = mv::keyboard::key::sk_return;
                break;
            case XK_BackSpace:
                mv_key_code = mv::keyboard::key::backspace;
                break;
            case XK_Tab:
                mv_key_code = mv::keyboard::key::tab;
                break;
            case XK_Prior:
                mv_key_code = mv::keyboard::key::prior;
                break;
            case XK_Next:
                mv_key_code = mv::keyboard::key::next;
                break;
            case XK_End:
                mv_key_code = mv::keyboard::key::end;
                break;
            case XK_Home:
                mv_key_code = mv::keyboard::key::home;
                break;
            case XK_Insert:
                mv_key_code = mv::keyboard::key::insert;
                break;
            case XK_Delete:
                mv_key_code = mv::keyboard::key::sk_delete;
                break;
            case XK_Pause:
                mv_key_code = mv::keyboard::key::pause;
                break;
            case XK_F1:
                mv_key_code = mv::keyboard::key::f1;
                break;
            case XK_F2:
                mv_key_code = mv::keyboard::key::f2;
                break;
            case XK_F3:
                mv_key_code = mv::keyboard::key::f3;
                break;
            case XK_F4:
                mv_key_code = mv::keyboard::key::f4;
                break;
            case XK_F5:
                mv_key_code = mv::keyboard::key::f5;
                break;
            case XK_F6:
                mv_key_code = mv::keyboard::key::f6;
                break;
            case XK_F7:
                mv_key_code = mv::keyboard::key::f7;
                break;
            case XK_F8:
                mv_key_code = mv::keyboard::key::f8;
                break;
            case XK_F9:
                mv_key_code = mv::keyboard::key::f9;
                break;
            case XK_F10:
                mv_key_code = mv::keyboard::key::f10;
                break;
            case XK_F11:
                mv_key_code = mv::keyboard::key::f11;
                break;
            case XK_F12:
                mv_key_code = mv::keyboard::key::f12;
                break;
            case XK_F13:
                mv_key_code = mv::keyboard::key::f13;
                break;
            case XK_F14:
                mv_key_code = mv::keyboard::key::f14;
                break;
            case XK_F15:
                mv_key_code = mv::keyboard::key::f15;
                break;
            case XK_Left:
                mv_key_code = mv::keyboard::key::left;
                break;
            case XK_Right:
                mv_key_code = mv::keyboard::key::right;
                break;
            case XK_Up:
                mv_key_code = mv::keyboard::key::up;
                break;
            case XK_Down:
                mv_key_code = mv::keyboard::key::down;
                break;
            case XK_a:
                mv_key_code = mv::keyboard::key::a;
                break;
            case XK_b:
                mv_key_code = mv::keyboard::key::b;
                break;
            case XK_c:
                mv_key_code = mv::keyboard::key::c;
                break;
            case XK_d:
                mv_key_code = mv::keyboard::key::d;
                break;
            case XK_e:
                mv_key_code = mv::keyboard::key::e;
                break;
            case XK_f:
                mv_key_code = mv::keyboard::key::f;
                break;
            case XK_g:
                mv_key_code = mv::keyboard::key::g;
                break;
            case XK_h:
                mv_key_code = mv::keyboard::key::h;
                break;
            case XK_i:
                mv_key_code = mv::keyboard::key::i;
                break;
            case XK_j:
                mv_key_code = mv::keyboard::key::j;
                break;
            case XK_k:
                mv_key_code = mv::keyboard::key::k;
                break;
            case XK_l:
                mv_key_code = mv::keyboard::key::l;
                break;
            case XK_m:
                mv_key_code = mv::keyboard::key::m;
                break;
            case XK_n:
                mv_key_code = mv::keyboard::key::n;
                break;
            case XK_o:
                mv_key_code = mv::keyboard::key::o;
                break;
            case XK_p:
                mv_key_code = mv::keyboard::key::p;
                break;
            case XK_q:
                mv_key_code = mv::keyboard::key::q;
                break;
            case XK_r:
                mv_key_code = mv::keyboard::key::r;
                break;
            case XK_s:
                mv_key_code = mv::keyboard::key::s;
                break;
            case XK_t:
                mv_key_code = mv::keyboard::key::t;
                break;
            case XK_u:
                mv_key_code = mv::keyboard::key::u;
                break;
            case XK_v:
                mv_key_code = mv::keyboard::key::v;
                break;
            case XK_w:
                mv_key_code = mv::keyboard::key::w;
                break;
            case XK_x:
                mv_key_code = mv::keyboard::key::x;
                break;
            case XK_y:
                mv_key_code = mv::keyboard::key::y;
                break;
            case XK_z:
                mv_key_code = mv::keyboard::key::z;
                break;
            case XK_0:
                mv_key_code = mv::keyboard::key::sk_0;
                break;
            case XK_1:
                mv_key_code = mv::keyboard::key::sk_1;
                break;
            case XK_2:
                mv_key_code = mv::keyboard::key::sk_2;
                break;
            case XK_3:
                mv_key_code = mv::keyboard::key::sk_3;
                break;
            case XK_4:
                mv_key_code = mv::keyboard::key::sk_4;
                break;
            case XK_5:
                mv_key_code = mv::keyboard::key::sk_5;
                break;
            case XK_6:
                mv_key_code = mv::keyboard::key::sk_6;
                break;
            case XK_7:
                mv_key_code = mv::keyboard::key::sk_7;
                break;
            case XK_8:
                mv_key_code = mv::keyboard::key::sk_8;
                break;
            case XK_9:
                mv_key_code = mv::keyboard::key::sk_9;
                break;
            default:
                mv_key_code = mv::keyboard::key::invalid;
                break;
            }
            return mv_key_code;
        }

        inline mv::keyboard::event read(void)
        {
            keyboard::event e(keyboard::event::etype::invalid, keyboard::key::invalid);
            if (!key_buffer.empty())
            {
                e = key_buffer.front();
                key_buffer.pop();
            }
            return e;
        }

        inline void on_key_press(mv::keyboard::key mv_key_code)
        {
            key_states[mv_key_code] = true;
            key_buffer.push(keyboard::event(keyboard::event::etype::press, mv_key_code));
            trim_buffer(key_buffer, mv::keyboard::max_buffer_size);
            return;
        }

        inline void on_key_release(mv::keyboard::key mv_key_code)
        {
            key_states[mv_key_code] = false;
            key_buffer.push(keyboard::event(keyboard::event::etype::release, mv_key_code));
            trim_buffer(key_buffer, mv::keyboard::max_buffer_size);
            return;
        }

        inline void clear_state(void)
        {
            key_states.reset();
            return;
        }

    private:
        static constexpr unsigned int max_key_count = 87;
        static constexpr unsigned int max_buffer_size = 16;
        // x11 pointer to display
        Display *display = nullptr;
        std::bitset<mv::keyboard::max_key_count> key_states;
        std::queue<keyboard::event> key_buffer;
    };
};
#endif