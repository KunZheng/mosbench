#include <sys/mman.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>

#include "bench.h"

#define ITERS     50
#define BUF_CNT   32768

static struct {
    uint64_t buf[BUF_CNT];
    char __pad0[128];
    volatile int run;
} *shared;

volatile int reset_total;
uint64_t total_cycles;
uint64_t total_iters;

static void slaver(int r, int w)
{
    unsigned int i;
    char cmd;
    int x;

    while (shared->run) {
        x = read(r, &cmd, 1);
        if (x == 0 || x < 0)
            break;
        for (i = 0; i < BUF_CNT; i++)
            shared->buf[i] = i;

        x = write(w, &cmd, 1);
        if (x == 0 || x < 0)
            break;
    }
    printf("slaver done\n");
}

static void show_ave(int u)
{
    float ave;

    ave = ((float)total_cycles) / ((float)total_iters);
    printf("%f\n", ave);

    reset_total = 1;
    alarm(2);
}

static inline void do_something(volatile uint64_t *ptr)
{
	short inc = 0x0100;

	asm volatile ("lock; xaddw %w0, %1\n"
		: "+Q" (inc), "+m" (*ptr)
		:
		: "memory", "cc");
}

static void master(int r, int w)
{
    uint32_t seed;
    char cmd;
    int x;
    int i;

    cmd = 0;
    seed = getpid();

    signal(SIGALRM, show_ave);
    alarm(2);

    while (shared->run) {
        uint64_t st, en;

        x = write(w, &cmd, 1);
        if (x == 0 || x < 0)
            break;
        x = read(r, &cmd, 1);
        if (x == 0 || x < 0)
            break;

        st = read_tsc();
        for (i = 0; i < ITERS; i++) {
            int k;

            k = rnd(&seed) % BUF_CNT;
            do_something(&shared->buf[k]);
#if 0
            if (++shared->buf[k] == 0)
                die("oops %u", k);
#endif
        }
        en = read_tsc();

        if (reset_total) {
            reset_total = 0;
            total_cycles = 0;
            total_iters = 0;
        }

        total_cycles += en - st;
        total_iters += ITERS;
    }

    printf("master done\n");
}

static void initshared(void)
{
    shared = mmap(0, sizeof(*shared), PROT_READ|PROT_WRITE, 
                  MAP_SHARED|MAP_ANONYMOUS, 0, 0);
    if (shared == MAP_FAILED)
        die("mmap failed");
}

int main(int ac, char **av)
{
    pid_t p;
    int sr[2];
    int mr[2];
    int cpu0;
    int cpu1;

    if (ac != 3)
        die("usage: %s cpu0 cpu1", basename(av[0]));
    
    cpu0 = atoi(av[1]);
    cpu1 = atoi(av[2]);

    setaffinity(cpu0);
    initshared();
    shared->run = 1;

    if (pipe(sr))
        edie("pipe");
    if (pipe(mr))
        edie("pipe");

    p = fork();
    if (p < 0)
        edie("fork");
    else if (p == 0) {
        setaffinity(cpu1);
        close(mr[0]);
        close(sr[1]);
        slaver(sr[0], mr[1]);
        return 0;
    }
    close(mr[1]);
    close(sr[0]);

    master(mr[0], sr[1]);

    return 0;
}
