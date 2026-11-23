#Base Makefile
AR ?= ar
INCLUDES = -I./inc/
TARGET = hweventcallback

AR_FLAGS += -r

OBJDIR= .obj

HWEVENTCALLBACK_LIBDIR = lib

HWEVENTCALLBACK_LIB = $(HWEVENTCALLBACK_LIBDIR)/lib$(TARGET).a

HWEVENTCALLBACK_LIB_SO = $(HWEVENTCALLBACK_LIBDIR)/lib$(TARGET).so

SRCS = HardwareEventCallback.c

LIB =  -lmraa -lpthread -lotdrinterface 

HWEVENTCALLBACK_OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

.PRECIOUS: $(OBJDIR)/%.o

all: clean directories $(HWEVENTCALLBACK_LIB) $(HWEVENTCALLBACK_LIB_SO)

$(HWEVENTCALLBACK_LIB) : $(HWEVENTCALLBACK_OBJS)
	$(AR) $(AR_FLAGS) $(HWEVENTCALLBACK_LIB) $(HWEVENTCALLBACK_OBJS)

$(HWEVENTCALLBACK_LIB_SO) : $(HWEVENTCALLBACK_OBJS)
	$(CC) $(LDFLAGS) -shared -fPIC -std=gnu99 $(HWEVENTCALLBACK_OBJS) $(LIB) -o $(HWEVENTCALLBACK_LIB_SO)

$(OBJDIR)/%.o : src/%.c
	@[ -d $$(dirname $@) ] || \
                (mkdir -p $$(dirname $@))
	$(CC) -std=gnu99 -fPIC $(CFLAGS)  $(INCLUDES) -c $^ -o $@

directories:
	@[ -d $(HWEVENTCALLBACK_LIBDIR) ] || \
               (mkdir -p $(HWEVENTCALLBACK_LIBDIR))
clean:
	rm -rf lib/
	rm -rf .obj/

