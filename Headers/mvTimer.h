#ifndef HEADERS_MVTIMER_H_
#define HEADERS_MVTIMER_H_

#include <chrono>

namespace mv
{
    class Timer
    {
    public:
        Timer(void);
        ~Timer(void);

        double getElaspedMS() noexcept;

        void restart() noexcept;
        bool startTimer() noexcept;
        bool stopTimer() noexcept;

    private:
        bool isRunning = false;
        std::chrono::time_point<std::chrono::high_resolution_clock> start;
        std::chrono::time_point<std::chrono::high_resolution_clock> stop;
    };
};

#endif