#define LCD_HIGH 0
#define LCD_LOW 1
#include <avr/io.h>
#include <util/delay.h>
#include "lib/lcdpcf8574.h"

int main(void)
{
    lcd_init(LCD_DISP_ON);
    lcd_led(LCD_HIGH);
    lcd_home();
    lcd_puts("test");
    while(1) {
    }
}

