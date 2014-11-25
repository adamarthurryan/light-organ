###############################################################################
# Makefile for the project controller
###############################################################################

## General Flags
PROJECT = controller
MCU = atxmega128a1
TARGET = controller.elf
CC = avr-gcc

CPP = avr-g++

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -std=gnu99   -DF_CPU=32000000UL -O3 -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=controller.map


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Include Directories
INCLUDES = -I"F:\foxy-win\electronics\projects\light organ\controller\..\..\..\avr-studio\xmega-appnote-library" -I"F:\foxy-win\electronics\projects\light organ\controller\..\..\..\avr-studio\xmega-library\display" 

## Library Directories
LIBDIRS = -L"F:\foxy-win\electronics\avr-studio\xmega-appnote-library\build-xmega128a1" 

## Libraries
LIBS = -lxmega-appnotes 

## Objects that must be built in order to link
OBJECTS = controller.o display.o display_device_spi.o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) controller.hex controller.eep controller.lss size

## Compile
controller.o: ../controller.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

display.o: ../../../../avr-studio/xmega-library/display/display.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

display_device_spi.o: ../../../../avr-studio/xmega-library/display/display_device_spi.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

size: ${TARGET}
	@echo
	@avr-size -C --mcu=${MCU} ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) controller.elf dep/* controller.hex controller.eep controller.lss controller.map


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

