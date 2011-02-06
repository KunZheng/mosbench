#if ENABLE_FORP
#include <errno.h>

#define FORP_PATH		"/sys/kernel/debug/forp"
#define FORP_ENABLE 		FORP_PATH "/forp-enable"

/* From linux/include/forp-patch.h */
#define FORP_ENABLE_DYN         0x01
#define FORP_ENABLE_ENTRY       0x02
#define FORP_ENABLE_ALL         (FORP_ENABLE_DYN|FORP_ENABLE_ENTRY)

static inline void forp_enable(unsigned int val)
{
	char buffer[32];
	char *b;
	int fd;
	int n;

	fd = open(FORP_ENABLE, O_RDWR);
	if (fd < 0) {
		if (errno == ENOENT)
			return;
		edie("forp_enable: open failed");
	}

	n = snprintf(buffer, sizeof(buffer), "%u", val);
	b = buffer;
	while (n) {
		int cc = write(fd, b, n);
		if (cc < 0)
			edie("forp_enable: write failed");
		n -= cc;
		b += cc;
	}
	close(fd);
}

static inline void forp_reset(void)
{
	forp_enable(0);
	forp_enable(FORP_ENABLE_ALL);
}

static inline void forp_stop(void)
{
	forp_enable(0);
}
#else /* ENABLE_FORP */
#define forp_enable(v) do { } while (0)
#define forp_reset() do { } while (0)
#define forp_stop() do { } while (0)
#endif /* ENABLE_FORP */
