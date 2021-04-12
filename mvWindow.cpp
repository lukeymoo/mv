#include "mvWindow.h"

namespace mv
{

    Window::Exception::Exception(int l, std::string f, std::string description)
        : BException(l, f, description)
    {
        type = "Window Handler Exception";
        errorDescription = description;
        return;
    }

    Window::Exception::~Exception(void)
    {
        return;
    }

    /* Create window and initialize Vulkan */
    Window::Window(int w, int h, const char *title)
    {
        XInitThreads();

        // Open connection xserver
        display = XOpenDisplay(NULL);
        if (display == NULL)
        {
            W_EXCEPT("Failed to open connection to xserver");
        }

        screen = DefaultScreen(display);

        // Create window
        window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, w, h, 1, WhitePixel(display, screen), BlackPixel(display, screen));

        Atom del_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
        XSetWMProtocols(display, window, &del_window, 1);

        // Configure to handle specified events
        XSelectInput(display, window,
                     FocusChangeMask |
                         ButtonPressMask |
                         ButtonReleaseMask |
                         ExposureMask |
                         KeyPressMask |
                         KeyReleaseMask |
                         PointerMotionMask |
                         NoExpose |
                         SubstructureNotifyMask);

        XStoreName(display, window, title);

        // Display window
        XMapWindow(display, window);
        XFlush(display);

        int rs;
        bool detectableResult;

        /* Request X11 does not send autorepeat signals */
        std::cout << "[+] Configuring keyboard input" << std::endl;
        detectableResult = XkbSetDetectableAutoRepeat(display, true, &rs);

        if (!detectableResult)
        {
            W_EXCEPT("Please disable autorepeat before launching!");
        }

        return;
    }

    Window::~Window(void)
    {
        std::cout << "[+] Cleaning up window resources" << std::endl;

        if (display && window)
        {
            XDestroyWindow(display, window);
            XCloseDisplay(display);
        }
        std::cout << "Bye bye" << std::endl;
        return;
    }

    void Window::go(void)
    {
        size_t currentFrame = 0;
        uint32_t frames = 0;
        while (running)
        {
            while (XPending(display))
            {
                handleXEvent();
            }

            // Fetch keyboard and mouse events
            Keyboard::Event kbdEvent = kbd.readKey();
            Mouse::Event mouseEvent = mouse.read();

        } // End of game loop

        return;
    }

    XEvent Window::createEvent(const char *eventType)
    {
        XEvent cev;

        cev.xclient.type = ClientMessage;
        cev.xclient.window = window;
        cev.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", true);
        cev.xclient.format = 32;
        cev.xclient.data.l[0] = XInternAtom(display, eventType, false);
        cev.xclient.data.l[1] = CurrentTime;

        return cev;
    }

    void Window::handleXEvent(void)
    {
        // count time for processing events
        XNextEvent(display, &event);
        KeySym key;
        switch (event.type)
        {
        case KeyPress:
            key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Escape)
            {
                XEvent q = createEvent("WM_DELETE_WINDOW");
                XSendEvent(display, window, false, ExposureMask, &q);
            }
            kbd.onKeyPress(static_cast<unsigned char>(key));
            break;
        case KeyRelease:
            key = XLookupKeysym(&event.xkey, 0);
            kbd.onKeyRelease(static_cast<unsigned char>(key));
            break;
        case MotionNotify:
            mouse.onMouseMove(event.xmotion.x, event.xmotion.y);
        case Expose:
            break;
            // configured to only capture WM_DELETE_WINDOW so we exit here
        case ClientMessage:
            std::cout << "[+] Exit requested" << std::endl;
            running = false;
            break;
        default: // Unhandled events do nothing
            break;
        } // End of switch
        return;
    }
}