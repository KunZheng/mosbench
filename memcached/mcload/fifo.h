#ifndef fifo_h
#define fifo_h 

// fifo template
// synchronized with mutex and cond

#include <assert.h>
#include <errno.h>
#include <list>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

template<class T>
class fifo {
 public:
  fifo();
  ~fifo();
  void enq(T);
  T deq();
 private:
  std::list<T> q;
  pthread_mutex_t m;
  pthread_cond_t c; // q went non-empty
};

template<class T>
fifo<T>::fifo()
{
  assert(pthread_mutex_init(&m, 0) == 0);
  assert(pthread_cond_init(&c, 0) == 0);
}

template<class T>
fifo<T>::~fifo()
{
  assert(pthread_mutex_destroy(&m) == 0);
  assert(pthread_cond_destroy(&c) == 0);
}

template<class T> void
fifo<T>::enq(T e)
{
  assert(pthread_mutex_lock(&m) == 0);
  q.push_back(e);
  assert(pthread_cond_broadcast(&c) == 0);
  assert(pthread_mutex_unlock(&m) == 0);
}

template<class T> T
fifo<T>::deq()
{
  while(1){
    bool gotone = false;
    T e = 0;

    // did someone call pthread_cancel() on us?
    // let's check here outside of the atomic section.
    // and hope that our caller holds no locks!
    int oldstate;
    assert(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate) == 0);
    pthread_testcancel();
    assert(pthread_setcancelstate(oldstate, &oldstate) == 0);

    assert(pthread_mutex_lock(&m) == 0);
    if(q.empty()){
      // assert(pthread_cond_wait(&c, &m) == 0);
      // wait for the condition variable, but time out periodically
      // to check (above) for cancel.
      struct timeval tv;
      struct timespec ts;
      gettimeofday(&tv, 0);
      ts.tv_sec = tv.tv_sec;
      ts.tv_nsec = tv.tv_usec * 1000;
      ts.tv_sec += 10;
      int ret = pthread_cond_timedwait(&c, &m, &ts);
      assert(ret == 0 || ret == ETIMEDOUT);
    } else {
      e = q.front();
      q.pop_front();
      gotone = true;
    }
    assert(pthread_mutex_unlock(&m) == 0);

    if(gotone)
      return e;
  }
}

#endif
