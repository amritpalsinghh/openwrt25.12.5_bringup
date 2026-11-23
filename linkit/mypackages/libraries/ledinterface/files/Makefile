#Base Makefile
AR ?= ar
INCLUDES = -I./inc/
TARGET = ledinterface

AR_FLAGS += -r

OBJDIR= .obj

LEDINTERFACE_LIBDIR = lib

LEDINTERFACE_LIB = $(LEDINTERFACE_LIBDIR)/lib$(TARGET).a

LEDINTERFACE_LIB_SO = $(LEDINTERFACE_LIBDIR)/lib$(TARGET).so

SRCS = led_control.c calc_crc8.c freescale_interface.c i2c_uC_comm.c

LIB = -lmraa -llog-maker

LEDINTERFACE_OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

.PRECIOUS: $(OBJDIR)/%.o

all: clean directories $(LEDINTERFACE_LIB) $(LEDINTERFACE_LIB_SO)

$(LEDINTERFACE_LIB) : $(LEDINTERFACE_OBJS)
	$(AR) $(AR_FLAGS) $(LEDINTERFACE_LIB) $(LEDINTERFACE_OBJS)

$(LEDINTERFACE_LIB_SO) : $(LEDINTERFACE_OBJS)
	$(CC) $(LDFLAGS) -shared -fPIC -std=gnu99 $(LEDINTERFACE_OBJS) $(LIB) -o $(LEDINTERFACE_LIB_SO)

$(OBJDIR)/%.o : src/%.c
	@[ -d $$(dirname $@) ] || \
                (mkdir -p $$(dirname $@))
	$(CC) -std=gnu99 -fPIC $(CFLAGS) $(INCLUDES) -c $^ -o $@

directories:
	@[ -d $(LEDINTERFACE_LIBDIR) ] || \
               (mkdir -p $(LEDINTERFACE_LIBDIR))
clean:
	rm -rf lib/
	rm -rf .obj/
