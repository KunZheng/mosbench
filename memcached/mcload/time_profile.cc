
#include "time_profile.h"

#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>

#include <string>
#include <map>

static const bool PRETTY_PRINT = false;

static int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y);
static void timeval_add(struct timeval *result, struct timeval *x, struct timeval *y);

struct timer_name_t {
  timer_name_t(int t, std::string n) : tid(t), name(n) {}

  int tid;
  std::string name;
};

static bool operator<(timer_name_t a, timer_name_t b) {
  if (a.tid < b.tid)
    return true;
  else if (a.tid > b.tid)
    return false;
  else return a.name < b.name;
}

struct my_timer_t {
  struct timeval tv_accum;

  struct timeval tv_last;
  bool running;
};

typedef std::map<timer_name_t, my_timer_t> timer_db_t;
static timer_db_t timer_db;
pthread_mutex_t timer_db_lock;

static my_timer_t &
getdb(int tid, const std::string &name) {
  pthread_mutex_lock(&timer_db_lock);
  my_timer_t &timer = timer_db[timer_name_t(tid, name)];
  pthread_mutex_unlock(&timer_db_lock);
  return timer;
}

static timer_db_t::iterator
getdbi(int tid, const std::string &name) {
  pthread_mutex_lock(&timer_db_lock);
  timer_db_t::iterator i = timer_db.find(timer_name_t(tid, name));
  pthread_mutex_unlock(&timer_db_lock);
  return i;
}

void
time_profile_init() {
  assert(pthread_mutex_init(&timer_db_lock, 0) == 0);
}

static my_timer_t & 
timer_init(int tid, const std::string &name) {
  my_timer_t &timer = getdb(tid, name);

  timer.tv_accum.tv_sec = 0;
  timer.tv_accum.tv_usec = 0;
  timer.running = false;

  return timer;
}

void
timer_start(int tid, const std::string &name) {
  timer_db_t::iterator i = getdbi(tid, name);
  my_timer_t &timer = (i == timer_db.end()) ? timer_init(tid, name) : i->second;

  assert(!timer.running);
  gettimeofday(&timer.tv_last, 0);
  timer.running = true;
}

void
timer_stop(int tid, const std::string &name) {
  timer_db_t::iterator i = getdbi(tid, name);
  assert(i != timer_db.end());

  my_timer_t &timer = i->second;

  assert(timer.running);
  struct timeval tv;
  gettimeofday(&tv, 0);
  timeval_subtract(&tv, &tv, &timer.tv_last);
  timeval_add(&timer.tv_accum, &timer.tv_accum, &tv);
  timer.running = false;
}

struct timeval
timer_get(int tid, const std::string &name) {
  timer_db_t::iterator i = getdbi(tid, name);
  assert(i != timer_db.end());
  my_timer_t &timer = i->second;
  return timer.tv_accum;
}

void
timer_print(int tid, const std::string &name) {
  timer_db_t::iterator i = getdbi(tid, name);
  assert(i != timer_db.end());
  struct timeval &tv = i->second.tv_accum;
  if (PRETTY_PRINT) {
    fprintf(stderr, "%s %i %lu.%06lu\n", name.c_str(), tid, tv.tv_sec, tv.tv_usec);
  }
  else {
    fprintf(stderr, "%s %i %lu %lu\n", name.c_str(), tid, tv.tv_sec, tv.tv_usec);
  }
}

void
timer_print(const std::string &name, bool accum) {
  struct timeval tv_accum;
  timer_db_t::iterator i = timer_db.begin();
  
  tv_accum.tv_sec = 0;
  tv_accum.tv_usec = 0;

  for (; i !=  timer_db.end(); i++) {
    const timer_name_t &timer_name = i->first;
    if (timer_name.name == name) {
      struct timeval &tv = i->second.tv_accum;
      if (accum) {
        timeval_add(&tv_accum, &tv_accum, &tv);
      }
      else {
        if (PRETTY_PRINT) {
          fprintf(stderr, "%s %i %lu.%06lu\n", timer_name.name.c_str(), timer_name.tid, tv.tv_sec, tv.tv_usec);
        }
        else {
          fprintf(stderr, "%s %i %lu %lu\n", timer_name.name.c_str(), timer_name.tid, tv.tv_sec, tv.tv_usec);
        }
      }
    }
  }

  if (accum) {
    if (PRETTY_PRINT) {
      fprintf(stderr, "%s %i %lu.%06lu\n", name.c_str(), 0, tv_accum.tv_sec, tv_accum.tv_usec);
    }
    else {
      fprintf(stderr, "%s %i %lu %lu\n", name.c_str(), 0, tv_accum.tv_sec, tv_accum.tv_usec);
    }
  }
}

/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */
// taken from http://www.delorie.com/gnu/docs/glibc/libc_428.html
int
timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y) {
  struct timeval y2 = *y;

  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y2.tv_usec) {
    int nsec = (y2.tv_usec - x->tv_usec) / 1000000 + 1;
    y2.tv_usec -= 1000000 * nsec;
    y2.tv_sec += nsec;
  }
  if (x->tv_usec - y2.tv_usec > 1000000) {
    int nsec = (x->tv_usec - y2.tv_usec) / 1000000;
    y2.tv_usec += 1000000 * nsec;
    y2.tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y2.tv_sec;
  result->tv_usec = x->tv_usec - y2.tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y2.tv_sec;
}

void
timeval_add(struct timeval *result, struct timeval *x, struct timeval *y) {
  result->tv_sec = x->tv_sec + y->tv_sec;
  result->tv_usec = x->tv_usec + y->tv_usec;

  if (result->tv_usec > 1000000) {
    result->tv_sec += result->tv_usec / 1000000;
    result->tv_usec = result->tv_usec % 1000000;
  }
}

/*
int main() {
  int i = 1;
  struct timeval tv;

  timer_start(0, "my_timer");
  for (i = 0; i < 5000; i++) {}
  timer_stop(0, "my_timer");

  timer_print(0, "my_timer");
  timer_print(0, "my_timer2");

  timer_start(0, "my_timer2");
  for (i = 0; i < 5000; i++) {}
  timer_stop(0, "my_timer2");

  timer_print(0, "my_timer");
  timer_print(0, "my_timer2");

  return 0;
}
*/

