INCLUDES += -I./inc/

TEST_BINDIR = bin

OBJDIR = .obj

TEST_BIN = $(TEST_BINDIR)/ossdn-test

LIB = -lmraa -locminterface -lotdrinterface -lhweventcallback -lm -lpthread -llog-maker -li2cinterface -lledinterface -lbatterymanagement
SRCS = ossdn_test.c

TEST_OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
.PRECIOUS: $(OBJDIR)/%.o


all: clean directories $(TEST_BIN)

$(TEST_BIN) : $(TEST_OBJS)
	$(CC) $(LDFLAGS) -std=gnu99 $(INCLUDES) $(TEST_OBJS) $(LIB) -o $@

$(OBJDIR)/%.o : src/%.c
	@[ -d $$(dirname $@) ] || \
                (mkdir -p $$(dirname $@))
	$(CC) -std=gnu99 $(CFLAGS) $(INCLUDES) -c $^ -o $@

directories:
	@[ -d $(TEST_BINDIR) ] || \
                (mkdir -p $(TEST_BINDIR))

clean:
	rm -rf .obj/
	rm -rf bin/

#install:
#	    echo

