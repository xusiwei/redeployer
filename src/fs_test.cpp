//
// Created by xu on 18-2-25.
//

#include "FunctionScheduler.h"
#include <iostream>

using namespace std;
using namespace std::chrono;

long get_milliseconds()
{
	static auto init = system_clock::now();
	return duration_cast<milliseconds>(system_clock::now() - init).count();
}

void test_function_scheduler()
{
	FunctionScheduler scheduler;

	get_milliseconds();
	scheduler.start();

	scheduler.schedule([]() {
		cout << "scheduled " << " on ticks " << get_milliseconds() << " ...\n";
	}, milliseconds(1000));

	scheduler.schedule([]() {
		static int count = 0;
		cout << "schedule " << ++count << " on ticks " << get_milliseconds() << " ...\n";
	}, milliseconds(500), milliseconds(100));

	this_thread::sleep_for(seconds(3));
	cout << "shutdown...\n";
	scheduler.shutdown();
}

int main()
{
	test_function_scheduler();
	return 0;
}
