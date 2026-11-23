#Base Makefile
AR ?= ar
INCLUDES = -I./inc/
TARGET = ocminterface

AR_FLAGS += -r

OBJDIR= .obj

OCMINTERFACE_LIBDIR = lib

OCMINTERFACE_LIB = $(OCMINTERFACE_LIBDIR)/lib$(TARGET).a

OCMINTERFACE_LIB_SO = $(OCMINTERFACE_LIBDIR)/lib$(TARGET).so

SRCS = ocm_interface.c serial.c power_socket_client.c

LIB = -llog-maker -ljson-c -lpthread -lm

OCMINTERFACE_OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

.PRECIOUS: $(OBJDIR)/%.o

all: clean directories $(OCMINTERFACE_LIB) $(OCMINTERFACE_LIB_SO)

$(OCMINTERFACE_LIB) : $(OCMINTERFACE_OBJS)
	$(AR) $(AR_FLAGS) $(OCMINTERFACE_LIB) $(OCMINTERFACE_OBJS)

$(OCMINTERFACE_LIB_SO) : $(OCMINTERFACE_OBJS)
	$(CC) $(LDFLAGS) -shared -fPIC -std=gnu99 $(OCMINTERFACE_OBJS) $(LIB) -o $(OCMINTERFACE_LIB_SO)

$(OBJDIR)/%.o : src/%.c
	@[ -d $$(dirname $@) ] || \
                (mkdir -p $$(dirname $@))
	$(CC) -std=gnu99 -fPIC $(CFLAGS) $(INCLUDES) -c $^ -o $@

directories:
	@[ -d $(OCMINTERFACE_LIBDIR) ] || \
               (mkdir -p $(OCMINTERFACE_LIBDIR))
clean:
	rm -rf lib/
	rm -rf .obj/
	rm -f transport/serial/src/serial.o
	rm -f transport/serial/src/semaphore.o
