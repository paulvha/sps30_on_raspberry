###############################################################
# makefile for sps30. May 2019 / paulvha
#
# To create a build with only the SPS30 monitor type: 
#		make
#
# To create a build with the SPS30 and DYLOS monitor type:
# 		make BUILD=DYLOS
#
# To create a build with the SPS30 and SDS011 monitor type:
# 		make BUILD=SDS011
#
# To create a build with the SPS30 BOTH DYLOS and SDS011 monitor type:
# 		make BUILD=BOTH

###############################################################
BUILD := sps30

# Objects to build
OBJ := sps30lib.o sps30.o
OBJ_DYLOS := dylos/dylos.o
OBJ_SDS := sds011/serial.o sds011/sds011_lib.o sds011/sdsmon.o

# GCC flags
CXXFLAGS := -Wall -Werror -c
CC_DYLOS := -DDYLOS 
CC_SDS := -DSDS011

# set the right flags and objects to include
ifeq ($(BUILD),sps30)
fresh:

#include DYLOS DC1700
else ifeq ($(BUILD),DYLOS)
CXXFLAGS += $(CC_DYLOS) 
OBJ += $(OBJ_DYLOS)
fresh:

#include SDS011
else ifeq ($(BUILD),SDS011)
CXXFLAGS += $(CC_SDS) 
OBJ += $(OBJ_SDS)
fresh:

#include BOTH SDS + DYLOS
else ifeq ($(BUILD),BOTH)
CXXFLAGS += $(CC_SDS) $(CC_DYLOS)
OBJ += $(OBJ_SDS) $(OBJ_DYLOS)
fresh:

#others to add here
endif

# set variables
CC := gcc
DEPS := sps30lib.h bcm2835.h 
LIBS := -lbcm2835 -lm

# how to create .o from .c or .cpp files
.c.o: %c $(DEPS)
	$(CC) $(CXXFLAGS) -o $@ $<

.cpp.o: %c $(DEPS)
	$(CC) $(CXXFLAGS) -o $@ $<

.PHONY : clean sps30 fresh newsps 
	
sps30 : $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

clean :
	rm -f sps30 dylos/dylos.o sds011/sds011_lib.o sds011/serial.o sds011/sdsmon.o $(OBJ)

# sps30.o is removed as this is only impacted by including
# Dylos monitor SDS011 or not. 
newsps :
	rm -f sps30.o
	
# first execute newsps then build sps30
fresh : newsps sps30


