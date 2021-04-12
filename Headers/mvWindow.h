#ifndef HEADERS_MVWINDOW_H_
#define HEADERS_MVWINDOW_H_

#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>

#include <iostream>
#include <memory>

#include "mvException.h"
#include "mvWindow.h"
#include "mvKeyboard.h"
#include "mvMouse.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

namespace mv
{
    class Window
    {
    public:
        class Exception : public BException
        {
        public:
            Exception(int line, std::string file, std::string message);
            ~Exception(void);

            std::string getType(void);
        };
        Window(int w, int h, const char *title);
        ~Window();

        void go(void);
        void handleXEvent(void);

        // Create a destroy event
        XEvent createEvent(const char *eventType);
        bool goodInit = true;
        Keyboard kbd;
        Mouse mouse;

    private:
        Display *display;
        Window window;
        XEvent event;
        int screen;
        bool running = true;
    };
};

#define W_EXCEPT(string) throw Exception(__LINE__, __FILE__, string)

#endif