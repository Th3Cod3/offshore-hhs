TARGET     = main.c
LIBS       = $(wildcard lib/*.c)
OBJS       = $(LIBS:.c=.o)
F_CPU      = 16000000UL
PORT       = COM6
DEVICE     = atmega2560
PROGRAMMER = wiring
BAUD       = 115200
COMPILE    = avr-gcc -Wall -Os -mmcu=$(DEVICE) -D F_CPU=$(F_CPU) -std=gnu99

default: compile upload

%.o: %.c
	$(COMPILE) -c $< -o $@

compile: $(OBJS)
	$(COMPILE) -o $(TARGET).elf $(OBJS)
	avr-objcopy -j .text -j .data -O ihex $(TARGET).elf $(TARGET).hex
	avr-size --format=avr --mcu=$(DEVICE) $(TARGET).elf

upload:
	avrdude -v -D -p $(DEVICE) -c $(PROGRAMMER) -P $(PORT) -b $(BAUD) -Uflash:w:$(TARGET).hex:i

clean:
	rm $(TARGET).o
	rm $(TARGET).elf
	rm $(TARGET).hex