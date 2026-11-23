#Base Makefile
AR ?= ar
INCLUDES = -I./inc/
TARGET = batterymanagement

AR_FLAGS += -r

OBJDIR= .obj

BATTERYMANAGEMENT_LIBDIR = lib

BATTERYMANAGEMENT_LIB = $(BATTERYMANAGEMENT_LIBDIR)/lib$(TARGET).a

BATTERYMANAGEMENT_LIB_SO = $(BATTERYMANAGEMENT_LIBDIR)/lib$(TARGET).so

SRCS = battery_management.c calc_crc8.c freescale_interface.c i2c_uC_comm.c

LIB = -lmraa -llog-maker

BATTERYMANAGEMENT_OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

.PRECIOUS: $(OBJDIR)/%.o

all: clean directories $(BATTERYMANAGEMENT_LIB) $(BATTERYMANAGEMENT_LIB_SO)

$(BATTERYMANAGEMENT_LIB) : $(BATTERYMANAGEMENT_OBJS)
	$(AR) $(AR_FLAGS) $(BATTERYMANAGEMENT_LIB) $(BATTERYMANAGEMENT_OBJS)

$(BATTERYMANAGEMENT_LIB_SO) : $(BATTERYMANAGEMENT_OBJS)
	$(CC) $(LDFLAGS) -shared -fPIC -std=gnu99 $(BATTERYMANAGEMENT_OBJS) $(LIB) -o $(BATTERYMANAGEMENT_LIB_SO)

$(OBJDIR)/%.o : src/%.c
	@[ -d $$(dirname $@) ] || \
                (mkdir -p $$(dirname $@))
	$(CC) -std=gnu99 -fPIC $(CFLAGS) $(INCLUDES) -c $^ -o $@

directories:
	@[ -d $(BATTERYMANAGEMENT_LIBDIR) ] || \
               (mkdir -p $(BATTERYMANAGEMENT_LIBDIR))
clean:
	rm -rf lib/
	rm -rf .obj/
