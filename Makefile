CC := g++ -std=c++11 -g -pg -fopenmp
CFLAGS := -Wall -Wextra -Werror -Iinclude

SRCS=$(wildcard src/*.cpp)
OBJS=$(patsubst %.cpp,%.o,$(SRCS))
DEPS=$(patsubst %.cpp,%.d,$(SRCS))

EXECUTABLE := mbrot
LIBRARIES := -L. -lSDL2main -lSDL2
ifeq ($(shell uname),Linux)
LIBRARIES += -lm
endif

COM := $(CC) $(CFLAGS)

.PHONY: clean all

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(OBJS) $(LIBRARIES) -o $(EXECUTABLE)

DEPFLAGS = -MT $@ -MMD -MP -MF src/$*.Td
COMPILE.cpp = $(CC) $(DEPFLAGS) $(CFLAGS) -c

src/%.o: src/%.cpp src/%.d
	$(COMPILE.cpp) $< -o $@
	mv -f src/$*.Td src/$*.d

src/%.d: ;
$(info $(DEPS))
-include $(DEPS)

clean:
	$(warning clean)
	rm -f src/*.o
	rm -f src/*.d
	rm -f $(EXECUTABLE).exe
	rm -f $(EXECUTABLE)
