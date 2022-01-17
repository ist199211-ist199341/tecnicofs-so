# Makefile, v1.1
# Sistemas Operativos, DEI/IST/ULisboa 2021-22
#
# This makefile should be run from the *root* of the project

CC ?= gcc
LD ?= gcc

# space separated list of directories with header files
INCLUDE_DIRS := fs client .
# this creates a space separated list of -I<dir> where <dir> is each of the values in INCLUDE_DIRS
INCLUDES = $(addprefix -I, $(INCLUDE_DIRS))

SOURCES  := $(wildcard */*.c)
HEADERS  := $(wildcard */*.h)
OBJECTS  := $(SOURCES:.c=.o)
TARGET_EXECS := fs/tfs_server
TARGET_EXECS += tests/test1
TARGET_EXECS += tests/copy_to_external_simple
TARGET_EXECS += tests/copy_to_external_errors
TARGET_EXECS += tests/write_10_blocks_spill
TARGET_EXECS += tests/write_10_blocks_simple
TARGET_EXECS += tests/write_more_than_10_blocks_simple
TARGET_EXECS += tests/write_more_than_10_blocks_spill
TARGET_EXECS += tests/thread_write_new_files
TARGET_EXECS += tests/thread_trunc_append
TARGET_EXECS += tests/thread_read_same_file
TARGET_EXECS += tests/thread_create_files
TARGET_EXECS += tests/thread_copy_to_external
TARGET_EXECS += tests/thread_same_fd
TARGET_EXECS += tests/thread_create_same_file
TARGET_EXECS += tests/lib_destroy_after_all_closed_test
TARGET_EXECS += tests/client_server_simple_test

# VPATH is a variable used by Makefile which finds *sources* and makes them available throughout the codebase
# vpath %.h <DIR> tells make to look for header files in <DIR>
vpath # clears VPATH
vpath %.h $(INCLUDE_DIRS)

# Multi-threading flags
LDFLAGS += -pthread
# fsanitize flags
# LDFLAGS += -fsanitize=thread
# LDFLAGS += -fsanitize=undefined
# LDFLAGS += -fsanitize=address

CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L
CFLAGS += $(INCLUDES)

# Warnings
CFLAGS += -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused
# Warning suppressions
CFLAGS += -Wno-sign-compare

# optional debug symbols: run make DEBUG=no to deactivate them
ifneq ($(strip $(DEBUG)), no)
  CFLAGS += -g
endif

# optional O3 optimization symbols: run make OPTIM=no to deactivate them
ifeq ($(strip $(OPTIM)), no)
  CFLAGS += -O0
else
  CFLAGS += -O3
endif

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean depend fmt test

all: $(TARGET_EXECS)


# The following target can be used to invoke clang-format on all the source and header
# files. clang-format is a tool to format the source code based on the style specified 
# in the file '.clang-format'.
# More info available here: https://clang.llvm.org/docs/ClangFormat.html

# The $^ keyword is used in Makefile to refer to the right part of the ":" in the 
# enclosing rule. See https://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

fmt: $(SOURCES) $(HEADERS)
	clang-format -i $^



# Note the lack of a rule.
# make uses a set of default rules, one of which compiles C binaries
# the CC, LD, CFLAGS and LDFLAGS are used in this rule

fs/tfs_server: fs/operations.o fs/state.o fs/utils.o
tests/test1: tests/test1.o fs/operations.o fs/state.o fs/utils.o
tests/copy_to_external_errors: tests/copy_to_external_errors.o fs/operations.o fs/state.o fs/utils.o
tests/copy_to_external_simple: tests/copy_to_external_simple.o fs/operations.o fs/state.o fs/utils.o
tests/write_10_blocks_spill: tests/write_10_blocks_spill.o fs/operations.o fs/state.o fs/utils.o
tests/write_10_blocks_simple: tests/write_10_blocks_simple.o fs/operations.o fs/state.o fs/utils.o
tests/write_more_than_10_blocks_simple: tests/write_more_than_10_blocks_simple.o fs/operations.o fs/state.o fs/utils.o
tests/write_more_than_10_blocks_spill: tests/write_more_than_10_blocks_spill.o fs/operations.o fs/state.o fs/utils.o
tests/thread_write_new_files: tests/thread_write_new_files.o fs/operations.o fs/state.o fs/utils.o
tests/thread_trunc_append: tests/thread_trunc_append.o fs/operations.o fs/state.o fs/utils.o
tests/thread_read_same_file: tests/thread_read_same_file.o fs/operations.o fs/state.o fs/utils.o
tests/thread_create_files: tests/thread_create_files.o fs/operations.o fs/state.o fs/utils.o
tests/thread_copy_to_external: tests/thread_copy_to_external.o fs/operations.o fs/state.o fs/utils.o
tests/thread_same_fd: tests/thread_same_fd.o fs/operations.o fs/state.o fs/utils.o
tests/thread_create_same_file: tests/thread_create_same_file.o fs/operations.o fs/state.o fs/utils.o
tests/block_destroy_simple: tests/block_destroy_simple.o fs/operations.o fs/state.o fs/utils.o
tests/client_server_simple_test: tests/client_server_simple_test.o client/tecnicofs_client_api.o fs/utils.o
tests/lib_destroy_after_all_closed_test: fs/operations.o fs/state.o fs/utils.o

clean:
	rm -f $(OBJECTS) $(TARGET_EXECS)

# This generates a dependency file, with some default dependencies gathered from the include tree
# The dependencies are gathered in the file autodep. You can find an example illustrating this GCC feature, without Makefile, at this URL: https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/GCC/options/MM
# Run `make depend` whenever you add new includes in your files
depend : $(SOURCES)
	$(CC) $(INCLUDES) -MM $^ > autodep

# Script that compiles the project and runs all the tests, should output "Successful test."
test: $(TARGET_EXECS)
	for x in `echo "$(TARGET_EXECS)" | sed "s/ /\n/g" | sed "s/^tests\///g"`; do (cd tests && ./$$x); done
