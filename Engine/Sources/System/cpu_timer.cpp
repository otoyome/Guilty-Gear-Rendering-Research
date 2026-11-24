#include "System/cpu_timer.h"

cpu_timer::cpu_timer()
{
}

cpu_timer::~cpu_timer()
{
}

void cpu_timer::begin()
{
	if (!end_called)
		return;

	time = std::chrono::steady_clock::now();
	end_called = false;
}

void cpu_timer::end()
{
	if (end_called)
		return;

	auto end = std::chrono::steady_clock::now();
	auto delta = std::chrono::duration_cast<std::chrono::microseconds>(end - time);
	time_delta = static_cast<float>(static_cast<double>(delta.count()) / 1000.0);
	end_called = true;
}

float cpu_timer::get_result()
{
	return time_delta;
}

