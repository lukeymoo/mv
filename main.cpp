#include "mvEngine.h"
#include "mvHelper.h"

#include <iostream>

LogHandler logger;

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    try
    {
        Engine wnd(WINDOW_WIDTH, WINDOW_HEIGHT, "Bloody Day");
        if (wnd.good_init)
        {
            wnd.go();
        }
        else
        {
            std::cout << std::endl
                      << "\t[-] Bad initialization, program ending!"
                      << std::endl;
        }
    }
    catch (std::exception &e)
    {
        std::cout << std::endl
                  << ":: Standard Library Exception :: " << std::endl
                  << e.what() << std::endl
                  << std::endl;
        std::exit(-1);
    }
    catch (...)
    {
        std::cout << std::endl
                  << ":: Unhandled Exception ::" << std::endl
                  << "Unhandled Exception! No further information available"
                  << std::endl
                  << std::endl;
        std::exit(-1);
    }
    return 0;
}