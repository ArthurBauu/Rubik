CC = cc
CFLAGS = -Wall -Wextra -Werror -O2 -std=c99 -Iinclude
BUILD_DIR = build
CORE_SOURCES = src/parser.c src/cube.c src/cubie.c src/solver.c src/generator.c
CORE_OBJECTS = $(CORE_SOURCES:src/%.c=$(BUILD_DIR)/%.o)
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs)
GL_LIBS = -lGL

.PHONY: all test clean visual

all: rubik $(BUILD_DIR)/librubik.a

visual: rubik-visual

$(BUILD_DIR)/visual_main.o: src/main.c include/rubik.h include/visual.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DWITH_VISUAL -c $< -o $@

$(BUILD_DIR)/visual.o: src/visual.c include/rubik.h include/visual.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/playback.o: src/playback.c include/rubik.h include/visual.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

rubik-visual: $(BUILD_DIR)/visual_main.o $(BUILD_DIR)/visual.o $(BUILD_DIR)/playback.o $(CORE_OBJECTS)
	$(CC) $(CFLAGS) $^ $(SDL_LIBS) $(GL_LIBS) -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c include/rubik.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/librubik.a: $(CORE_OBJECTS)
	ar rcs $@ $^

rubik: $(BUILD_DIR)/main.o $(CORE_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_core: tests/test_core.c $(CORE_OBJECTS) include/rubik.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_core.c $(CORE_OBJECTS) -o $@

$(BUILD_DIR)/test_playback: tests/test_playback.c $(BUILD_DIR)/playback.o $(BUILD_DIR)/parser.o $(BUILD_DIR)/cube.o include/visual.h
	$(CC) $(CFLAGS) tests/test_playback.c $(BUILD_DIR)/playback.o $(BUILD_DIR)/parser.o $(BUILD_DIR)/cube.o -o $@

test: rubik $(BUILD_DIR)/test_core $(BUILD_DIR)/test_playback
	./$(BUILD_DIR)/test_core
	./$(BUILD_DIR)/test_playback
	sh tests/test_cli.sh

clean:
	rm -rf $(BUILD_DIR) rubik rubik-visual
