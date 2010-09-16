
#ifndef timer_profile_h
#define timer_profile_h

#include <string>

void time_profile_init();
void timer_start(int tid, const std::string &name);
void timer_stop(int tid, const std::string &name);
struct timeval timer_get(int tid, const std::string &name);
void timer_print(int tid, const std::string &name);
void timer_print(const std::string &name, bool accum = false);

#endif

