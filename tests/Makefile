CXX ?= g++

CFLAGS = -std=c++11 -g -Wall -Wextra -Werror -I..
LDFLAGS = -lprimesieve

# This is where the test files go
SRC = maybe.cpp \
      unordered_map.cpp \
      fred_hash_map.cpp \
      DPH_with_array_buckets.cpp

BUILDDIR ?= build

OBJ = $(patsubst %.cpp,${BUILDDIR}/%.o,$(SRC))

.PHONY: directories

all: compile

compile: directories tests

directories: ${BUILDDIR}

build:
	mkdir -p ${BUILDDIR}

${BUILDDIR}/%.o: %.cpp build
	$(CXX) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

tests: $(OBJ)
	$(CXX) $(CFLAGS) -o tests tests.cpp $^ $(LDFLAGS)

clean:
	rm -rf ${BUILDDIR} tests

run: compile
	./tests

gdb: compile
	gdb --args ./tests --break
