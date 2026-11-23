#Base Makefile
CFLAGS += -fPIC -O3 -Wall -lpthread -lrt
INCLUDES = -I./inc/
TARGET = log-maker

OBJDIR= .obj

LOGGER_LIBDIR = lib

LOGGER_LIB = $(LOGGER_LIBDIR)/lib$(TARGET).a

LOGGER_LIB_SO = $(LOGGER_LIBDIR)/lib$(TARGET).so


SRCS = logMgr.c

LOGGER_OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

.PRECIOUS: $(OBJDIR)/%.o

all: clean directories  $(LOGGER_LIB_SO)


$(LOGGER_LIB_SO) : $(LOGGER_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -fPIC -shared $(LOGGER_OBJS) -o $(LOGGER_LIB_SO)

$(OBJDIR)/%.o : src/%.c
	@[ -d $$(dirname $@) ] || \
		(mkdir -p $$(dirname $@))
	$(CC) $(CFLAGS) -fPIC -O3 -std=gnu99 -Wall $(INCLUDES) -c $^ -o $@

directories:
	@[ -d $(LOGGER_LIBDIR) ] || \
		(mkdir -p $(LOGGER_LIBDIR))
clean:
	rm -rf lib/
	rm -rf .obj/

