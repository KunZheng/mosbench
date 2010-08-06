CXXFLAGS := -O2 -Wall -MD # -g
#CXXFLAGS += -I$(LDB)
#LDFLAGS += -L/Users/kaashoek/hack/db-4.7.25.NC/build_unix/
#LDFLAGS += -pg -lprofiler

OBJS := args.o pedsort.o

all: pedsort

pedsort: $(OBJS)
	g++ -o $@ $(LDFLAGS) $^ -ldb -lpthread

-include $(OBJS:.o=.d)

.PHONY: clean
clean: 
	rm pedsort $(OBJS) $(OBJS:.o=.d)
