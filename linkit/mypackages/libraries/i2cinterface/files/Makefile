#Base Makefile
AR ?= ar
#CFLAGS = -fPIC -O3 -std=gnu99 -Wall -lpthread
INCLUDES = -I./inc/
TARGET = i2cinterface

AR_FLAGS += -r
#CC = arm-poky-linux-gnueabi-gcc

OBJDIR= .obj

I2CINTERFACE_LIBDIR = lib

I2CINTERFACE_LIB = $(I2CINTERFACE_LIBDIR)/lib$(TARGET).a

I2CINTERFACE_LIB_SO = $(I2CINTERFACE_LIBDIR)/lib$(TARGET).so

SRCS = occ_interface.c sfp_interface.c power_socket_client.c common_i2c_operation.c sfp_fw_upgrade.c freescale_interface.c calc_crc8.c i2c_uC_comm.c ctm_find_notch_positions.c savitzky_golay_filtering.c ctm14xx_simple_regression.c osa_calibration.c

LIB = -lmraa -lotdrinterface -locminterface -llog-maker -ljson-c

I2CINTERFACE_OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

.PRECIOUS: $(OBJDIR)/%.o

all: clean directories $(I2CINTERFACE_LIB) $(I2CINTERFACE_LIB_SO)

$(I2CINTERFACE_LIB) : $(I2CINTERFACE_OBJS)
	$(AR) $(AR_FLAGS) $(I2CINTERFACE_LIB) $(I2CINTERFACE_OBJS)

$(I2CINTERFACE_LIB_SO) : $(I2CINTERFACE_OBJS)
	$(CC) $(LDFLAGS) -shared -fPIC -std=gnu99 $(I2CINTERFACE_OBJS) $(LIB) -o $(I2CINTERFACE_LIB_SO)

$(OBJDIR)/%.o : src/%.c
	@[ -d $$(dirname $@) ] || \
                (mkdir -p $$(dirname $@))
	$(CC) -std=gnu99 -fPIC $(CFLAGS) $(INCLUDES) -c $^ -o $@

directories:
	@[ -d $(I2CINTERFACE_LIBDIR) ] || \
               (mkdir -p $(I2CINTERFACE_LIBDIR))
clean:
	rm -rf lib/
	rm -rf .obj/
