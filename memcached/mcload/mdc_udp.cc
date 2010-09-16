#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <linux/unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <fcntl.h>

#include <vector>
#include <sstream>

#include "atomic_32.h"
#include "fifo.h"
#include "method_thread.h"
#include "time_profile.h"

#define TIMEOUT		100000
//#define TIMEOUT		10

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

char localhost[64];

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

/*
struct udp_header {
  uint16_t id;          //Request ID
  uint16_t seq_num;     //Sequence number
  uint16_t pkts;        //Total number of datagrams in this message
  uint16_t rsvd;        //Reserved for future use; must be 0
};
*/

class worker_t {
 public:
  worker_t(fifo<int> *j) : join(j) {
    atomic_set(&counter, 0);
    atomic_set(&drops, 0);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    rand_data.state = (int32_t*)rand_state;
    initstate_r(tv.tv_usec, rand_state, sizeof(rand_state), &rand_data);
  }

  int get_count() {
    int count = atomic_read(&counter);
    atomic_set(&counter, 0);
    return count;
  }

  int get_drops() {
    int drop = atomic_read(&drops);
    atomic_set(&drops, 0);
    return drop;
  }

  void work() {
    uint16_t id = 0;
    int s[num_servers];

    for (int i = 0; i < num_servers; i++) {
        struct sockaddr_in sbind;
	//struct timeval tv;
	//tv.tv_sec = 0;
	//tv.tv_usec = TIMEOUT;

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
        //errno_check(setsockopt(s[i], SOL_SOCKET, SO_RCVTIMEO, (void *) &tv, sizeof(tv)));
        fprintf(stderr, "connected to %s\n", servers[i].host);
    }

    char rcv_buf[1024]; 
    char packet_buf[64]; 
    char *packet_data = packet_buf + 8;
    uint16_t *packet_buf16 = (uint16_t *)packet_buf;
    packet_buf16[1] = htons(0);
    packet_buf16[2] = htons(1);
    packet_buf16[3] = htons(0);

    //*packet_data++ = 'g';
    //*packet_data++ = 'e';
    //*packet_data++ = 't';
    //*packet_data++ = ' ';

#define REQ "get 0123456789\r\n"
    memcpy(packet_data, REQ, strlen(REQ));

    while (true) {
      int sn;

      if (num_servers == 1)
        sn = 0;
      else
        sn = rand() % num_servers;
      
      for (int i = 0; i < BATCH; i++) {
        //int i;
        packet_buf16[0] = htons(id++);

        //for (i = 0; i < 10; i++) {
        //  *(packet_data + i) = (char)('a' + rand() % ('z' - 'a'));
        //}

        //*(packet_data + i + 1) = '\r';
        //*(packet_data + i + 2) = '\n';
        //*(packet_data + i + 3) = '\0';

        /*
        for (uint32_t i = 0; i < strlen(packet_buf + 8) + 8; i++) {
          if (i < 8)
            printf("%d %d\n", i, packet_buf[i]);
          else
            printf("%d %c\n", i, packet_buf[i]);
        }
        */
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

  void worker_thread(int cpu_id) {
    tid = gettid();
    work();
    join->enq(tid);
  }

  void start(int cpu_id) {
    assert(join);
    if(method_thread(this, true, &worker_t::worker_thread, cpu_id) == 0){
      perror("pthread_create");
      exit(1);
    }
  }

 private:
  fifo<int> *join;
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

class timert_t {
 public:
  timert_t(std::vector<worker_t> *w) : workers(w) {}

  void work() {
    struct sockaddr_in sin;
    struct hostent *hp;
    int s, fd;

    errno_check(fd = open("/tmp/mdc.log", O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR));

    if(!(hp = gethostbyname(master_host))) {
      fprintf(stderr, "unable to resolve host: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    memset(&sin, 0, sizeof(sin));

    sin.sin_family = AF_INET;
    sin.sin_addr = *((struct in_addr*) hp->h_addr_list[0]);
    sin.sin_port = htons(master_port);

    errno_check(s = socket(PF_INET, SOCK_STREAM, 0));
    errno_check(connect(s, (struct sockaddr *)&sin, sizeof(sin)));

    while (1) {
      char buf[128];
      int sum_count = 0;
      int sum_drops = 0;

      for (uint32_t i = 0; i < workers->size(); i++)
        sum_count += workers->at(i).get_count();

      for (uint32_t i = 0; i < workers->size(); i++)
        sum_drops += workers->at(i).get_drops();

      snprintf(buf, 128, "%s %d %d\n", localhost, sum_count, sum_drops);
      write(s, buf, strlen(buf));
      write(fd, buf, strlen(buf));

      sleep(1);
    }
  }

  void worker_thread(int cpu_id) {
    tid = gettid();
    work();
  }

  void start(int cpu_id) {
    if(method_thread(this, true, &timert_t::worker_thread, cpu_id) == 0){
      perror("pthread_create");
      exit(1);
    }
  }

 private:
  std::vector<worker_t> *workers;
  int tid;
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

int
main(int argc, char **argv) {
  int num_workers = 0;
  const char *hostip;
  fifo<int> join;
  int opt;

  while ((opt = getopt(argc, argv, "p:s:")) != -1) {
    switch (opt) {
    case 'p':
      base_port = atoi(optarg);
      fprintf(stderr, "base_port = %d\n", base_port);
      break;
    case 's':
      source_port = atoi(optarg);
      fprintf(stderr, "source_port = %d\n", source_port);
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

  //printf("connecting to %s\n", hostip);
  //printf("workers: %d servers: %d port: %d\n", num_workers, num_servers, base_port);

  int fd;
  ssize_t len;
  errno_check(fd = open("/etc/hostname", O_RDONLY));
  len = read(fd, localhost, 64);
  errno_check(len);
  if (len < 64)
    localhost[len-1] = '\0';
  else
    errno_check(-1);

  std::vector<worker_t> *workers;
  workers = new std::vector<worker_t>(num_workers, &join);
  servers = new struct server[num_servers];

  init(hostip);

  timert_t timer(workers);
  timer.start(0);

  time_profile_init();
  timer_start(0, "global");

  for (int i = 0; i < (int)workers->size(); i++)
    workers->at(i).start(i);

  int done = 0;
  while (done < num_workers) {
    join.deq();
    done++;
  }

  timer_stop(0, "global");
  timer_print("global");

  return 0;
}

// vim: set sw=2 ts=8 sts=8 expandtab textwidth=80 ff=unix:
