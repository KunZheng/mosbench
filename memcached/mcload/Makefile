GXX     := g++
OPT     := -O3
CFLAGS  := -g -Wall -D_GNU_SOURCE $(OPT)
LDFLAGS := -lpthread

BINS  := mdc mdc_udp mdcs udp_send udp_recv

COMMON_H:= time_profile.h fifo.h method_thread.h atomic_32.h Makefile

MDC_O := mdc.o time_profile.o
MDC_H := $(COMMON_H)

MDC_UDP_O := mdc_udp.o time_profile.o
MDC_UDP_H := $(COMMON_H)

UDP_SEND_O := udp_send.o time_profile.o
UDP_SEND_H := $(COMMON_H)

UDP_RECV_O := udp_recv.o time_profile.o
UDP_RECV_H := $(COMMON_H)

MDCS_O := mdcs.o time_profile.o
MDCS_H := $(COMMON_H)

all: $(BINS)

mdc: $(MDC_O) $(MDC_H) 
	$(CXX) $(CFLAGS) -o mdc $(MDC_O) $(LDFLAGS)

mdc_udp: $(MDC_UDP_O) $(MDC_UDP_H) 
	$(CXX) $(CFLAGS) -o mdc_udp $(MDC_UDP_O) $(LDFLAGS)

udp_send: $(UDP_SEND_O) $(UDP_SEND_H) 
	$(CXX) $(CFLAGS) -o udp_send $(UDP_SEND_O) $(LDFLAGS)

udp_recv: $(UDP_RECV_O) $(UDP_RECV_H) 
	$(CXX) $(CFLAGS) -o udp_recv $(UDP_RECV_O) $(LDFLAGS)

mdcs: $(MDCS_O) $(MDCS_H) 
	$(CXX) $(CFLAGS) -o mdcs $(MDCS_O) $(LDFLAGS)

%.o : %.cc
	$(CXX) -c $(CFLAGS) $<

clean:
	rm -f $(BINS) *.o
