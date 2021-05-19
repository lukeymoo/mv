#include "mvTimer.h"

Timer::Timer(void)
{
    start = std::chrono::high_resolution_clock::now();
    stop = std::chrono::high_resolution_clock::now();
    return;
}

Timer::~Timer(void)
{
    return;
}

double Timer::getElaspedMS() noexcept
{
    if (isRunning)
    {
        // auto t = std::chrono::duration<double,
        // std::milli>(std::chrono::high_resolution_clock::now() - start);
        auto t = std::chrono::duration<double, std::ratio<1L, 1L>>(
            std::chrono::high_resolution_clock::now() - start);
        return t.count();
    }
    else
    {
        // auto t = std::chrono::duration<double, std::milli>(stop - start);
        auto t =
            std::chrono::duration<double, std::ratio<1L, 1L>>(stop - start);
        return t.count();
    }
}

void Timer::restart() noexcept
{
    isRunning = true;
    start = std::chrono::high_resolution_clock::now();
    return;
}

bool Timer::startTimer() noexcept
{
    if (isRunning)
    {
        return false;
    }
    else
    {
        start = std::chrono::high_resolution_clock::now();
        isRunning = true;
        return true;
    }
}

bool Timer::stopTimer() noexcept
{
    if (!isRunning)
    {
        return false;
    }
    else
    {
        stop = std::chrono::high_resolution_clock::now();
        isRunning = false;
        return true;
    }
}