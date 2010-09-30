GXX     := g++
OPT     := -O3
CFLAGS  := -g -Wall -D_GNU_SOURCE $(OPT)
LDFLAGS := -lpthread

BINS  := mdc_udp

COMMON_H := method_thread.h atomic_32.h Makefile

MDC_UDP_O := mdc_udp.o
MDC_UDP_H := $(COMMON_H)

all: $(BINS)

mdc_udp: $(MDC_UDP_O) $(MDC_UDP_H) 
	$(CXX) $(CFLAGS) -o mdc_udp $(MDC_UDP_O) $(LDFLAGS)

%.o : %.cc
	$(CXX) -c $(CFLAGS) $<

clean:
	rm -f $(BINS) *.o
