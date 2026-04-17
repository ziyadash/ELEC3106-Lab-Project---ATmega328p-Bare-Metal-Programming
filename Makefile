# define some compile time constants and useful values for the build process
MCU = atmega328p
F_CPU = 16000000UL
CC = avr-gcc
OBJCOPY = avr-objcopy
AVRDUDE = avrdude

# compiler flags, the usual ones plus defining the CPU frequency and the target microcontroller
CFLAGS = -Wall -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU)

MAIN_TARGET = main
TEST_TARGET = test

# compile code
all: $(MAIN_TARGET).hex

# build the test program
test: $(TEST_TARGET).hex

# convert C code to ELF executable
$(MAIN_TARGET).elf: $(MAIN_TARGET).c hal.c hal.h
	$(CC) $(CFLAGS) $(MAIN_TARGET).c hal.c -o $@

$(TEST_TARGET).elf: $(TEST_TARGET).c hal.c hal.h
	$(CC) $(CFLAGS) $(TEST_TARGET).c hal.c -o $@

# convert ELF eexecutable to hex format for flashing
$(MAIN_TARGET).hex: $(MAIN_TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(TEST_TARGET).hex: $(TEST_TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

# flash main code to board
flash-main: $(MAIN_TARGET).hex
	$(AVRDUDE) -c usbasp -p $(MCU) -B 10 -U flash:w:$(MAIN_TARGET).hex:i

# flash test code to board
flash-test: $(TEST_TARGET).hex
	$(AVRDUDE) -c usbasp -p $(MCU) -B 10 -U flash:w:$(TEST_TARGET).hex:i

# set fuse bits so the chip uses the 16MHz external crystal
fuse:
	$(AVRDUDE) -c usbasp -p $(MCU) -B 10 -U lfuse:w:0xFF:m

# clean existing binaries
clean:
	rm -f $(MAIN_TARGET).elf $(MAIN_TARGET).hex $(TEST_TARGET).elf $(TEST_TARGET).hex

.PHONY: all test flash-main flash-test fuse clean