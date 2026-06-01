CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -Isrc -Itests -MMD -MP
LDFLAGS  = -lm

SRCDIR  = src
OBJDIR  = obj

SRCS = $(SRCDIR)/main.c    \
       $(SRCDIR)/engine.c  \
       $(SRCDIR)/config.c  \
       $(SRCDIR)/log.c     \
       $(SRCDIR)/audio.c   \
       $(SRCDIR)/tui.c     \
       $(SRCDIR)/measure.c

OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)
-include $(DEPS)

TARGET = breathe

TEST_BINS = tests/test_engine tests/test_audio tests/test_config tests/test_log tests/test_measure_stats

.PHONY: all clean test

all: $(OBJDIR) $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TEST_BINS)
	@echo ""
	@echo "Running tests..."
	@failed=0; for t in $(TEST_BINS); do \
		echo ""; \
		echo "--- $$t ---"; \
		./$$t || failed=$$((failed+1)); \
	done; \
	echo ""; \
	echo "Test suites run. Failures: $$failed"; \
	exit $$failed

tests/test_engine: tests/test_engine.c src/engine.c src/engine.h tests/test_runner.h
	$(CC) $(CFLAGS) -o $@ tests/test_engine.c -lm

tests/test_audio: tests/test_audio.c src/audio.c src/audio.h tests/test_runner.h
	$(CC) $(CFLAGS) -o $@ tests/test_audio.c -lm

tests/test_config: tests/test_config.c src/config.c src/engine.c src/engine.h src/config.h tests/test_runner.h
	$(CC) $(CFLAGS) -o $@ tests/test_config.c -lm

tests/test_log: tests/test_log.c src/log.c src/log.h tests/test_runner.h
	$(CC) $(CFLAGS) -o $@ tests/test_log.c -lm

tests/test_measure_stats: tests/test_measure_stats.c src/measure.c src/measure.h tests/test_runner.h
	$(CC) $(CFLAGS) -o $@ tests/test_measure_stats.c -lm

clean:
	rm -rf $(OBJDIR) $(TARGET) tests/test_engine tests/test_audio tests/test_config tests/test_log tests/test_measure_stats
	rm -f tests/*.d
