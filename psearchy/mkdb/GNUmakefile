LDB := /Users/kaashoek/hack/db-4.7.25.NC/build_unix/
#CXXFLAGS := -finstrument-functions -Wall -MD -I$(LDB)
CXXFLAGS := -O2 -Wall -MD -I$(LDB) # -g

pedsort_objects:= args.o pedsort.o

all: pedsort

pedsort : $(pedsort_objects) 
	g++ -o pedsort -L$(LDB)  $(pedsort_objects)  -ldb -lpthread # -pg -lprofiler


.PHONY : clean
clean : 
	rm pedsort $(shared_objects) $(pedsort_objects)
