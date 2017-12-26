CC=g++ -std=c++11
CFLAGS=-Wall -Wextra -Werror
CFLAGS+=-g -pg -fopenmp
COM=$(CC) $(CFLAGS)
SOURCES := $(wildcard src/*.cpp)
SOURCES := $(filter-out $(wildcard src/*_L.cpp), $(SOURCES))
OBJ_FILES := $(patsubst %.cpp,%.o,$(SOURCES))
I=-Iinclude
EXECUTABLE=mbrot
ifeq ($(shell uname),Linux)
LIBRARIES=-L. -lSDL2main -lSDL2 -lm
else
LIBRARIES=-L. -lSDL2main -lSDL2
endif

.PHONY: clean all

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ_FILES)
	$(COM) $^ $(LIBRARIES) -o $(EXECUTABLE)

src/%.o: src/%.cpp
	$(COM) $(D) $(I) -c -o $@ $<

clean:
	rm -f src/*.o
	rm -f src/*.d
	rm -f $(EXECUTABLE).exe
