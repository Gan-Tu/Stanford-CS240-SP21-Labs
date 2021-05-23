AR = $(CROSS_COMPILE)ar
CC = $(CROSS_COMPILE)g++$(CC_VERSION)

OBJ_DIR = obj
SRC_DIR = src
INCLUDE_DIR = include
BIN_DIR = bin
LIBS_DIR = libs
TOP_TEST_DIR = test
TEST_DIR = test/src
UNAME = $(shell uname)

FUSE_LDFLAGS = $(shell pkg-config fuse --libs)
DB_LDFLAGS = -ldb
ifeq ($(UNAME),Linux)
  NANOMSG_LDFLAGS = -lanl -lrt $(shell pkg-config libbsd --libs) -lpthread
else
  NANOMSG_LDFLAGS =
endif

SERVER_LIBS = nanomsg
SERVER_LIB_AS = $(addsuffix .a,$(addprefix $(LIBS_DIR)/lib,$(SERVER_LIBS)))
SERVER_LIB_FLAGS = $(NANOMSG_LDFLAGS) $(DB_LDFLAGS) \
        -L$(LIBS_DIR) $(addprefix -l,$(SERVER_LIBS))

CLIENT_LIBS = nanomsg
CLIENT_LIB_AS = $(addsuffix .a,$(addprefix $(LIBS_DIR)/lib,$(CLIENT_LIBS)))
CLIENT_LIB_FLAGS = $(NANOMSG_LDFLAGS) $(FUSE_LDFLAGS) \
		-L$(LIBS_DIR) $(addprefix -l,$(CLIENT_LIBS))

LIBS_INCLUDE_DIR = $(LIBS_DIR)/$(INCLUDE_DIR)

LDFLAGS = -ggdb $(EXTERN_LIBS_FLAGS)
FUSE_CFLAGS = $(shell pkg-config fuse --cflags)
CCFLAGS = -ggdb -Wall -Wextra -Werror -Wswitch-default -Wwrite-strings \
	-O3 -Iinclude -Itest/include -std=gnu99 $(CFLAGS) $(FUSE_CFLAGS) \
	-I$(LIBS_INCLUDE_DIR) -x c

COMMON_C_SRCS = $(addprefix common/,strings.c comm.c)
rwild=$(foreach d,$(wildcard $1*),$(call rwild,$d/,$2) \
	$(filter $(subst *,%,$2),$d))

CLIENT_C_SRCS = $(addprefix client/,main.c request.c fuseops.c) $(COMMON_C_SRCS)
CLIENT_OBJS = $(CLIENT_C_SRCS:%.c=$(OBJ_DIR)/%.o)
CLIENT_BIN_OBJS = $(CLIENT_OBJS) $(OBJ_DIR)/client-main.o
CLIENT_BIN = $(BIN_DIR)/client

SERVER_C_SRCS = $(addprefix server/,main.c handlers.c fhandledb.c) $(COMMON_C_SRCS)
SERVER_OBJS = $(SERVER_C_SRCS:%.c=$(OBJ_DIR)/%.o)
SERVER_BIN_OBJS = $(SERVER_OBJS) $(OBJ_DIR)/server-main.o
SERVER_BIN = $(BIN_DIR)/server

SNFS_OBJS = $(sort $(CLIENT_OBJS) $(SERVER_OBJS))
SNFS_LIB_AS = $(sort $(CLIENT_LIB_AS) $(SERVER_LIB_AS))

TEST_SRCS = test.c test_utils.c mock.c helpers.c \
		phase1_tests.c phase2_tests.c phase3_tests.c phase4_tests.c \
		phase5_tests.c phase6_tests.c phase7_tests.c unphased_tests.c
TEST_OBJS = $(TEST_SRCS:%.c=$(OBJ_DIR)/%.o)
TEST_BIN = $(BIN_DIR)/test

EXTRA_CREDIT_TEST_SRCS = extra_credit_test.c test_utils.c mock.c helpers.c \
		extra_credit_create_file_tests.c \
		extra_credit_delete_file_tests.c \
		extra_credit_rename_file_tests.c
EXTRA_CREDIT_TEST_OBJS = $(EXTRA_CREDIT_TEST_SRCS:%.c=$(OBJ_DIR)/%.o)
EXTRA_CREDIT_TEST_BIN = $(BIN_DIR)/extra_credit_test

SUBMIT_TAR = lab2.tar.gz
SUBMISSION_SITE = "https://web.stanford.edu/class/cs240/labs/submission/"

.PHONY: all clean test submission server client extra_credit_test

vpath % $(SRC_DIR) $(TEST_DIR)

all: $(CLIENT_BIN) $(SERVER_BIN)

# Creates $(LIBS_DIR)/libnanomsg.a
-include libs/build/nanomsg.mk

$(BIN_DIR):
	@mkdir -p $@

$(OBJ_DIR)/%.o: %.c $(EXTERN_INCLUDES)
	@mkdir -p $(@D)
	@echo + $@ [cc $<]
	@$(CC) $(CCFLAGS) -c $< -o $@

$(OBJ_DIR)/%-main.o: $(SRC_DIR)/main.c
	@mkdir -p $(@D)
	@echo + $@ [cc $<]
	@$(CC) $(CCFLAGS) -DSNFS_IS_$(shell echo $(*) | tr a-z A-Z)=1 -c $< -o $@

$(SERVER_BIN): $(SERVER_BIN_OBJS) $(SERVER_LIB_AS) | $(BIN_DIR)
	@echo + $@ [ld $^]
	@$(CC) -o $@ $^ $(LDFLAGS) $(SERVER_LIB_FLAGS)

$(CLIENT_BIN): $(CLIENT_BIN_OBJS) $(CLIENT_LIB_AS) | $(BIN_DIR)
	@echo + $@ [ld $^]
	@$(CC) -o $@ $^ $(LDFLAGS) $(CLIENT_LIB_FLAGS)

$(TEST_BIN): $(TEST_OBJS) $(SNFS_OBJS) $(SNFS_LIB_AS) | $(BIN_DIR)
	@echo + $@ [ld $^]
	@$(CC) -o $@ $^ $(LDFLAGS) $(CLIENT_LIB_FLAGS) $(SERVER_LIB_FLAGS)

$(EXTRA_CREDIT_TEST_BIN): $(EXTRA_CREDIT_TEST_OBJS) $(SNFS_OBJS) $(SNFS_LIB_AS) | $(BIN_DIR)
	@echo + $@ [ld $^]
	@$(CC) -o $@ $^ $(LDFLAGS) $(CLIENT_LIB_FLAGS) $(SERVER_LIB_FLAGS)

$(SUBMIT_TAR): $(call rwild,$(SRC_DIR)/,*) $(call rwild,$(INCLUDE_DIR)/,*) $(call rwild,$(TOP_TEST_DIR)/,*)
	tar -zcf $@ $(SRC_DIR) $(INCLUDE_DIR) $(TOP_TEST_DIR)

test: $(TEST_BIN)
	@-sudo $(TEST_BIN)
	@-sudo pkill test

extra_credit_test: $(EXTRA_CREDIT_TEST_BIN)
	@-sudo $(EXTRA_CREDIT_TEST_BIN)
	@-sudo pkill extra_credit_test

server: $(SERVER_BIN)

client: $(CLIENT_BIN)

submission: $(SUBMIT_TAR)
	@echo "Your submission file "$^" was successfully created."
	@echo "Submit it at $(SUBMISSION_SITE)."

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(SUBMIT_TAR)

superclean:
	@echo "You probably don't want to do this!"
	@echo "You'll need to rebuild the shared libraries if you do."
	@echo "I've warned ya. 'make superclean-everything' if you dare."

superclean-everything:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(SUBMIT_TAR) $(SNFS_LIB_AS) \
	  $(LIBS_DIR)/$(INCLUDE_DIR)
	-make -C $(LIBS_DIR)/nanomsg clean
	rm -f $(LIBS_DIR)/nanomsg/Makefile
