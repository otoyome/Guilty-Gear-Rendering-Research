#pragma	once

#include <chrono>

class cpu_timer
{
public:
	cpu_timer();
	~cpu_timer();

	void begin();
	void end();
	float get_result();

private:
	bool end_called = true;
	std::chrono::steady_clock::time_point time;
	float time_delta = 0.0f;
};
