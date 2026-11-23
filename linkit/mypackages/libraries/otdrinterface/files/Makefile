#Base Makefile
AR ?= ar
INCLUDES = -I./inc/
TARGET = otdrinterface

AR_FLAGS += -r

OBJDIR= .obj

OTDRINTERFACE_LIBDIR = lib

OTDRINTERFACE_LIB = $(OTDRINTERFACE_LIBDIR)/lib$(TARGET).a

OTDRINTERFACE_LIB_SO = $(OTDRINTERFACE_LIBDIR)/lib$(TARGET).so

SRCS = otdr_interface.c serial.c socket_client.c power_socket_client.c

LIB = -llog-maker -ljson-c -lpthread -lm

OTDRINTERFACE_OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

.PRECIOUS: $(OBJDIR)/%.o

all: clean directories $(OTDRINTERFACE_LIB) $(OTDRINTERFACE_LIB_SO)

$(OTDRINTERFACE_LIB) : $(OTDRINTERFACE_OBJS)
	$(AR) $(AR_FLAGS) $(OTDRINTERFACE_LIB) $(OTDRINTERFACE_OBJS)

$(OTDRINTERFACE_LIB_SO) : $(OTDRINTERFACE_OBJS)
	$(CC) $(LDFLAGS) -shared -fPIC -std=gnu99 $(OTDRINTERFACE_OBJS) $(LIB) -o $(OTDRINTERFACE_LIB_SO)

$(OBJDIR)/%.o : src/%.c
	@[ -d $$(dirname $@) ] || \
                (mkdir -p $$(dirname $@))
	$(CC) -std=gnu99 -fPIC $(CFLAGS)  $(INCLUDES) -c $^ -o $@

directories:
	@[ -d $(OTDRINTERFACE_LIBDIR) ] || \
               (mkdir -p $(OTDRINTERFACE_LIBDIR))
clean:
	rm -rf lib/
	rm -rf .obj/
	rm -f src/serial.o
	rm -f src/socket_client.o

