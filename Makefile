CC=g++ -std=c++11 -g -pg -fopenmp
CFLAGS=-Wall -Wextra -Werror -Iinclude

EXECUTABLE=mbrot
ifeq ($(shell uname),Linux)
LIBRARIES=-L. -lSDL2main -lSDL2 -lm
else
LIBRARIES=-L. -lSDL2main -lSDL2
endif

COM=$(CC) $(CFLAGS)

.PHONY: clean all

all:
	@makedepend.py --compile "$(COM) -c @src -o @out" \
	               --link    "$(CC) @objs $(LIBRARIES) -o $(EXECUTABLE)"

clean:
	rm -f src/*.o
	rm -f $(EXECUTABLE).exe
	rm -f $(EXECUTABLE)
