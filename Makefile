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

.PHONY: clean all

# Default behaviour is to make the executable
all: $(EXECUTABLE)

# This is the final compile step:
#   take all the object files in src/*.o and link them with the libraries
$(EXECUTABLE): $(OBJS)
	$(CC) $(OBJS) $(LIBRARIES) -o $(EXECUTABLE)

# To generate the .d files, I use the -M* options.
DEPFLAGS = -MT $@ -MMD -MP -MF src/$*.d
COMPILE.cpp = $(CC) $(DEPFLAGS) $(CFLAGS)

# To create a .o file, take the source code and create 2 files: the .o with the
#  compiled code, and the .d with the dependency info
src/%.o: src/%.cpp
	$(COMPILE.cpp) -c $< -o $@

# if the dependency doesnt exist, we dont have to worry because that means the
#  corresponding object also doesnt exist. if the .d does exist then it will
#  contain info about dependant header files. It will look something like this:
#
#     src/main.o: src/main.cpp include/global.h include/graphics.h
#     include/global.h:
#     include/graphics.h:
#     include/renderScene.h:
#     include/input.h:
#
#   meaning that if include/global.h, or include/graphics.h were updated (newer
#   modified time than the .o file) then the above rule will need to be re run
#   again, causing a recompile, then the recompile causes a relink and the new
#   program is generated.
#
# Because the deps are optional, it may seem correct to use
#
#     -include $(DEPS)
#
#   but this is wrong becuase it will search in system directores for it as
#   well. You can see this with `strace make -n 2>&1 | grep /include/src`.
#   Wildcard will only include them if they exist.
#
include $(wildcard $(DEPS))


clean:
	rm -f src/*.o
	rm -f src/*.d
	rm -f $(EXECUTABLE).exe
	rm -f $(EXECUTABLE)
