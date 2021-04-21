#include "mvTimer.h"

mv::Timer::Timer(void)
{
	start = std::chrono::high_resolution_clock::now();
	stop = std::chrono::high_resolution_clock::now();
	return;
}

mv::Timer::~Timer(void)
{
	return;
}

double mv::Timer::getElaspedMS() noexcept
{
	if (isRunning) {
		auto t = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start);
		return t.count();
	}
	else {
		auto t = std::chrono::duration<double, std::milli>(stop - start);
		return t.count();
	}
}

void mv::Timer::restart() noexcept
{
	isRunning = true;
	start = std::chrono::high_resolution_clock::now();
	return;
}

bool mv::Timer::startTimer() noexcept
{
	if (isRunning) {
		return false;
	}
	else {
		start = std::chrono::high_resolution_clock::now();
		isRunning = true;
		return true;
	}
}

bool mv::Timer::stopTimer() noexcept
{
	if (!isRunning) {
		return false;
	}
	else {
		stop = std::chrono::high_resolution_clock::now();
		isRunning = false;
		return true;
	}
}