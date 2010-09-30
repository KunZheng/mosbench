#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <linux/unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <vector>
#include <sstream>

// XXX Can get rid of this, too
#include "atomic_32.h"
#include "method_thread.h"

#define TIMEOUT		100000

#define BATCH           20

struct server {
  const char *host;
  int port;
  struct sockaddr_in sin;
};

struct server *servers;
int num_servers = 0;
int base_port = 11211;
int source_port = -1;

const char *master_host = "hooverdam.lcs.mit.edu";
const int master_port = 4344;

#define errno_check(expr)                                               \
    do {                                                                \
        int __r = (expr);                                               \
        if (__r < 0) {                                                  \
            fprintf(stderr, "%s:%u: %s - %s\n",                         \
                                  __FILE__, __LINE__, #expr,            \
                                  strerror(errno));                     \
            exit(EXIT_FAILURE);                                         \
        }                                                               \
    } while (0)

static pid_t gettid(void)
{
  return syscall(__NR_gettid);
}

class worker_t {
 public:
  worker_t() {
    atomic_set(&counter, 0);
    atomic_set(&drops, 0);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    rand_data.state = (int32_t*)rand_state;
    initstate_r(tv.tv_usec, rand_state, sizeof(rand_state), &rand_data);
  }

  int get_count() {
    return atomic_read(&counter);
  }

  int get_drops() {
    return atomic_read(&drops);
  }

  void reset() {
    atomic_set(&counter, 0);
    atomic_set(&drops, 0);
  }

  void work() {
    uint16_t id = 0;
    int s[num_servers];

    for (int i = 0; i < num_servers; i++) {
        struct sockaddr_in sbind;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT;

        errno_check(s[i] = socket(PF_INET, SOCK_DGRAM, 0));

        if (source_port != -1) {
            memset(&sbind, 0, sizeof(sbind));
            sbind.sin_family = AF_INET;
            sbind.sin_port = htons(source_port);
            errno_check(bind(s[i], (struct sockaddr *)&sbind, sizeof(sbind)));
        }

        errno_check(connect(s[i],
                            (struct sockaddr *)&servers[i].sin,
                            sizeof(servers[i].sin)));
        errno_check(setsockopt(s[i], SOL_SOCKET, SO_RCVTIMEO, (void *) &tv, sizeof(tv)));
        printf("connected to %s\n", servers[i].host);
    }

    char rcv_buf[1024]; 
    char packet_buf[64]; 
    char *packet_data = packet_buf + 8;
    uint16_t *packet_buf16 = (uint16_t *)packet_buf;
    packet_buf16[1] = htons(0);
    packet_buf16[2] = htons(1);
    packet_buf16[3] = htons(0);

#define REQ "get 0123456789\r\n"
    memcpy(packet_data, REQ, strlen(REQ));

    while (true) {
      int sn;

      if (num_servers == 1)
        sn = 0;
      else
        sn = rand() % num_servers;
      
      for (int i = 0; i < BATCH; i++) {
        packet_buf16[0] = htons(id++);
        errno_check(write(s[sn], packet_buf, 8 + 4 + 10 + 3));
      }

      for (int i = 0; i < BATCH; i++) {
        int r;
        r = read(s[sn], rcv_buf, sizeof(rcv_buf));
        if (r < 0) {
          atomic_inc(&drops);
          break;
        } else {
          atomic_inc(&counter);
        }
      }
    }

    for (int i = 0; i < num_servers; i++) {
        errno_check(close(s[i]));
    }
  }

  void worker_thread() {
    tid = gettid();
    work();
  }

  void start() {
    if(method_thread(this, true, &worker_t::worker_thread) == 0){
      perror("pthread_create");
      exit(1);
    }
  }

 private:
  int tid;
  atomic_t counter;
  atomic_t drops;

  struct random_data rand_data;
  char rand_state[64];

  int32_t rand() {
    int32_t rand;
    random_r(&rand_data, &rand);
    return rand;
  }
};

void
init(const char *hostip) {
  for (int i = 0; i < num_servers; i++) {
    struct hostent *hp;

    servers[i].host = hostip;
    servers[i].port = base_port + i;

    if(!(hp = gethostbyname(servers[i].host))) {
      fprintf(stderr, "unable to resolve host: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    servers[i].sin.sin_family = AF_INET;
    servers[i].sin.sin_addr = *((struct in_addr*) hp->h_addr_list[0]);
    servers[i].sin.sin_port = htons(servers[i].port);
  }
}

void usage(const char *prog_name) {
    fprintf(stderr, "%s [-p base_port] [-s source_port] hostip num_workers num_servers\n", prog_name);
}

std::vector<worker_t> *workers;

void reset_counts(int x) {
  for (int i = 0; i < (int)workers->size(); i++)
    workers->at(i).reset();
}

void print_count(int x) {
  int total = 0, drops = 0;
  for (int i = 0; i < (int)workers->size(); i++) {
    int t = workers->at(i).get_count(),
      d = workers->at(i).get_drops();
    printf("Worker %d: %d requests, %d drops\n", i, t, d);
    total += t;
    drops += d;
  }
  printf("%d requests, %d drops\n", total, drops);
  fflush(stdout);
}

void on_int(int x) {
  exit(0);
}

int
main(int argc, char **argv) {
  int num_workers = 0;
  const char *hostip;
  int opt;

  while ((opt = getopt(argc, argv, "p:s:")) != -1) {
    switch (opt) {
    case 'p':
      base_port = atoi(optarg);
      printf("base_port = %d\n", base_port);
      break;
    case 's':
      source_port = atoi(optarg);
      printf("source_port = %d\n", source_port);
      break;
    default:
      fprintf(stderr, "unknown option -%c\n", opt);
      usage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if ((argc - optind) < 3) {
    fprintf(stderr, "Not enough arguments\n");
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  hostip = argv[optind];
  num_workers = atoi(argv[optind + 1]);
  num_servers = atoi(argv[optind + 2]);

  workers = new std::vector<worker_t>(num_workers);
  servers = new struct server[num_servers];

  init(hostip);

  // Install signal handlers.  We do this late so we'll die if we get a
  // signal before the workers are up.
  signal(SIGUSR1, reset_counts);
  signal(SIGUSR2, print_count);
  signal(SIGINT, on_int);

  // Block signals while we're starting the workers so they'll inherit
  // this signal mask.
  sigset_t sigs;
  sigemptyset(&sigs);
  sigaddset(&sigs, SIGUSR1);
  sigaddset(&sigs, SIGUSR2);
  sigaddset(&sigs, SIGINT);
  pthread_sigmask(SIG_BLOCK, &sigs, NULL);

  for (int i = 0; i < (int)workers->size(); i++)
    workers->at(i).start();

  // Unblock signals in just this thread
  pthread_sigmask(SIG_UNBLOCK, &sigs, NULL);

  // Wait for workers to start
  for (int i = 0; i < (int)workers->size(); i++)
    while (workers->at(i).get_count() == 0 &&
           workers->at(i).get_drops() == 0)
      usleep(100000);           // 100 ms

  while (true)
    pause();

  return 0;
}

// vim: set sw=2 ts=8 sts=8 expandtab textwidth=80 ff=unix:
