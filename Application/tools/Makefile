# ============================================================================
# Host verification harness — builds verify.c against scale.c + quantize.c
# on x86_64 Linux (gcc). FFT-dependent stages are tested on-target, not here.
#
# Usage (from this directory):
#     make            # build ./verify
#     make run        # build (if needed) and execute
#     make clean      # remove the binary
# ============================================================================

CC       = gcc
CFLAGS   = -O2 -Wall -Wextra -DHOST_BUILD -std=c11
INCLUDES = -iquote ../include -iquote .
SRCS     = ../src/scale.c ../src/quantize.c ../src/stat_norm.c verify.c
TARGET   = verify

$(TARGET): $(SRCS) test_vectors.h
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o $(TARGET) -lm

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
