
ADDITIONAL_LIBS_HOST="optaneparlab01"
# CPPFLAGS:=  -std=c++17 -pedantic -Wall -Wextra -Werror -fopenmp -fPIC  -O3
ifdef ENABLE_NTIER
ifeq ($(ENABLE_NTIER),1)
CPPFLAGS:=  -std=c++17 -pedantic -fPIC  -O3 -DENABLE_NTIER
else
CPPFLAGS:=  -std=c++17 -pedantic -fPIC  -O3
endif
else
CPPFLAGS:=  -std=c++17 -pedantic -fPIC  -O3
endif
# FLAGS:= -lpfm -lpthread
FLAGS:= -lpthread -lnuma
INCLUDE_HEADERS:= -Iutils/ -Icore -Icore/region_logic -Icore/mapping_logic 
HOST=$(shell hostname)



all: tracker_d.o test.o utils/space_helper.cpp


HEADERS := $(wildcard utils/*.h)
# HEADERS += $(wildcard core/*.h)
HEADERS += $(shell find core/ -type f -name '*.h')

UTILS_SRCS := $(wildcard utils/*.cpp)
UTILS_OBJS := $(UTILS_SRCS:cpp=o)

# CORE_SRCS := $(wildcard core/*.cpp)
CORE_SRCS := $(shell find core/ -type f -name '*.cpp')
CORE_OBJS := $(CORE_SRCS:cpp=o)
CORE_CLEAN_OBJS:=$(shell find core/ -type f -name '*.o')


utils/%.o : utils/%.cpp Makefile  $(HEADERS)
	g++ -c $(CPPFLAGS) $< -o $@ $(INCLUDE_HEADERS) $(FLAGS)

core/%.o : core/%.cpp Makefile $(HEADERS)
	g++ -c $(CPPFLAGS) $< -o $@ $(INCLUDE_HEADERS) $(FLAGS)

tracker_d.o:  $(UTILS_OBJS) $(CORE_OBJS) tracker_d.cpp $(HEADERS) 

	@echo "--------------"
	@echo $(UTILS_OBJS)
	@echo "--------------"
	@echo $(CORE_OBJS)
	@echo "--------------"
	g++ $(CPPFLAGS) $(UTILS_OBJS) $(CORE_OBJS)  tracker_d.cpp -o $@  $(FLAGS) $(INCLUDE_HEADERS)


# test.o: test.cpp
# 	g++ -std=c++17  $^ -o $@  -DOPTANEMACHINE $(FLAGS)



clean:
	
	@rm -rf *.o $(UTILS_OBJS) $(CORE_CLEAN_OBJS) utils/*.o
	@rm -rf perf.data*
	# clean_dir.sh
