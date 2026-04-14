# define some compile time constants and useful values for the build process
MCU = atmega328p
F_CPU = 16000000UL
CC = avr-gcc
OBJCOPY = avr-objcopy
AVRDUDE = avrdude

# compiler flags, the usual ones plus defining the CPU frequency and the target microcontroller
CFLAGS = -Wall -Werror -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU)

TARGET = main

# compile code
all: $(TARGET).hex

# convert C code to ELF executable
$(TARGET).elf: $(TARGET).c
	$(CC) $(CFLAGS) $< -o $@

# convert ELF eexecutable to hex format for flashing
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

# flash code to board
flash: $(TARGET).hex
	$(AVRDUDE) -c usbasp -p $(MCU) -U flash:w:$(TARGET).hex:i

# clean existing binaries
clean:
	rm -f $(TARGET).elf $(TARGET).hex