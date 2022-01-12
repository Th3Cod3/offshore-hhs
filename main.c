#include <avr/io.h>
#include "lib/lcdpcf8574.h"

#ifdef DEBUG_EN
#include <util/delay.h>
#define DEBUG_PIN PORTB7
#define DEBUG_SIGNAL(delay)  \
    PORTB ^= _BV(DEBUG_PIN); \
    _delay_ms(delay);        \
    PORTB ^= _BV(DEBUG_PIN);
#else
#define DEBUG_SIGNAL(delay)
#endif

#define LCD_HIGH 0
#define LCD_LOW 1

#define LCD_ENCODER_A PORTA0
#define LCD_ENCODER_B PORTA1
#define LCD_ENCODER_BUTTON PORTA2

#define ENCODER_STATE_UNKNOWN 100
#define ENCODER_STATE_NONE 0
#define ENCODER_STATE_A 1
#define ENCODER_STATE_B 2
#define ENCODER_STATE_X 3
#define ENCODER_STATE_LEFT 4
#define ENCODER_STATE_RIGHT 6
#define ENCODER_STATE_BUTTON 7

#define PROJECT_OPTION_NONE 0
#define PROJECT_OPTION_DRIVE 1
#define PROJECT_OPTION_SETTINGS 2
#define PROJECT_OPTION_INFO 3
#define PROJECT_OPTION_DRIVE_MANUAL 11
#define PROJECT_OPTION_DRIVE_GRID 12
#define PROJECT_OPTION_SETTINGS_CALIBRATION 21
#define PROJECT_OPTION_SETTINGS_BOX 22

#define LCD_REFESH 1
#define LCD_NO_REFESH 2

// {"Besturing", "Instellingen", "Projectinfo"},
//     {"Handmatig", "Raster", "Terug"},
//         {"x=", "z=", "y=", "Terug"},
//         {"Raster=", "Starten", "Terug"},
//             {"Kraan naar A0"},
//     {"Calibratie", "Doos(mm)", "Terug"},
//         {"Verplaats x->", "Verplaats y->", "Verplaats z->"},
//         {"l=", "h=", "d=", "Terug"},
//     {"terug", "Projectinfo", "Naam project:", "Project Onshore", "Gemaakt door:", "Projectgroep B1", "Leden:", "-Roy -Hicham", "-Ivan -Yefri", "-Sebastiaan", "terug"}

char menuText[][LCD_DISP_LENGTH + 1] = {"Besturing", "Instellingen", "Projectinfo"};
char projectInfo[][LCD_DISP_LENGTH + 1] = {"Druk voor terug", "Projectinfo", "Naam project:", "Project Onshore", "Gemaakt door:", "Projectgroep B1", "Leden:", "-Roy -Hicham", "-Ivan -Yefri", "-Sebastiaan", "Druk voor terug"};
const char optionsAmount = sizeof(menuText) / (LCD_DISP_LENGTH + 1);
const char optionsInfoAmount = sizeof(projectInfo) / (LCD_DISP_LENGTH + 1);
char complement = 0;

void readScreenEncoder(char *lcdEncoderState)
{
    char lcdEncoderA = PINA & _BV(LCD_ENCODER_A);
    char lcdEncoderB = PINA & _BV(LCD_ENCODER_B);
    char lcdEncoderButton = PINA & _BV(LCD_ENCODER_BUTTON);

    if (lcdEncoderButton)
    {
        *lcdEncoderState = ENCODER_STATE_BUTTON;
    }
    else if (*lcdEncoderState != ENCODER_STATE_NONE && (!lcdEncoderA || !lcdEncoderB))
    {
        _delay_us(20); // prevent debounce
        *lcdEncoderState = ENCODER_STATE_NONE;
    }
    else if (*lcdEncoderState == ENCODER_STATE_NONE && lcdEncoderA && lcdEncoderB)
    {
        DEBUG_SIGNAL(1)
        return;
    }
    else if (lcdEncoderA && *lcdEncoderState == ENCODER_STATE_NONE)
    {
        _delay_us(10);
        *lcdEncoderState = ENCODER_STATE_A;
    }
    else if (lcdEncoderB && *lcdEncoderState == ENCODER_STATE_NONE)
    {
        _delay_us(10);
        *lcdEncoderState = ENCODER_STATE_B;
    }
    else if (lcdEncoderB && *lcdEncoderState == ENCODER_STATE_A)
    {
        *lcdEncoderState = ENCODER_STATE_RIGHT;
    }
    else if (lcdEncoderA && *lcdEncoderState == ENCODER_STATE_B)
    {
        *lcdEncoderState = ENCODER_STATE_LEFT;
    }
}

char moveOptionSelector(char *optionSelector, char lcdEncoderState, char totalOptions)
{
    // -1 of the index and -1 complement = -2
    if (lcdEncoderState == ENCODER_STATE_LEFT && *optionSelector < totalOptions - 2)
    {
        if (!complement)
        {
            complement = 1;
        }
        else
        {
            *optionSelector += 1;
        }
    }
    else if (lcdEncoderState == ENCODER_STATE_RIGHT && *optionSelector)
    {
        if (complement)
        {
            complement = 0;
        }
        else
        {
            *optionSelector -= 1;
        }
    }
    else
    {
        return LCD_NO_REFESH;
    }

    return LCD_REFESH;
}

void chooseOption(char *optionSelector, char *projectOption)
{
    char prevProjectOption = *projectOption;
    if (*projectOption == PROJECT_OPTION_NONE)
    {
        DEBUG_SIGNAL((*projectOption))
        _delay_ms(10);
        switch ((*optionSelector) + complement)
        {
        case PROJECT_OPTION_DRIVE - 1:
            break;
        case PROJECT_OPTION_SETTINGS - 1:
            break;
        case PROJECT_OPTION_INFO - 1:
            DEBUG_SIGNAL(100)
            *projectOption = PROJECT_OPTION_INFO;
            break;
        }
    }
    else if (*projectOption == PROJECT_OPTION_INFO)
    {
        *projectOption = PROJECT_OPTION_NONE;
    }
    else
    {
        DEBUG_SIGNAL(1000)
    }

    if (prevProjectOption != *projectOption)
    {
        complement = 0;
        *optionSelector = 0;
    }
}

void changeOption(char *projectOption, char *optionSelector, char lcdEncoderState)
{
    char prevProjectOption = *projectOption;
    if (lcdEncoderState == ENCODER_STATE_BUTTON)
    {
        chooseOption(optionSelector, projectOption);
    }

    if (*projectOption == PROJECT_OPTION_NONE)
    {
        moveOptionSelector(optionSelector, lcdEncoderState, optionsAmount);
        for (char i = 0; i < 2; i++)
        {
            i == 0 ? lcd_clrscr() : lcd_gotoxy(0, 1);
            complement - i ? lcd_putc('-') : lcd_putc('>');
            lcd_puts(menuText[*optionSelector + i]);
        }
    }
    else if (*projectOption == PROJECT_OPTION_INFO)
    {
        moveOptionSelector(optionSelector, lcdEncoderState, optionsInfoAmount);
        for (char i = 0; i < 2; i++)
        {
            i == 0 ? lcd_clrscr() : lcd_gotoxy(0, 1);
            lcd_puts(projectInfo[*optionSelector + i]);
        }
    }
}

char validateLcdState(char lcdEncoderState, char lcdEncoderPrevState)
{
    char hasActionsState = lcdEncoderState == ENCODER_STATE_RIGHT || lcdEncoderState == ENCODER_STATE_LEFT || lcdEncoderState == ENCODER_STATE_BUTTON;
    char sameAsPrevState = lcdEncoderPrevState == lcdEncoderState;
    return hasActionsState && !sameAsPrevState;
}

int main(void)
{
    lcd_init(LCD_DISP_ON);
    lcd_led(LCD_HIGH);
    DDRA &= ~(_BV(LCD_ENCODER_A) | _BV(LCD_ENCODER_B) | _BV(LCD_ENCODER_BUTTON));
    DDRB |= _BV(DEBUG_PIN);

    char lcdEncoderState = ENCODER_STATE_NONE;
    char projectOption = PROJECT_OPTION_NONE;
    char lcdEncoderPrevState = ENCODER_STATE_UNKNOWN;
    char optionSelector = 0;

    changeOption(&projectOption, &optionSelector, lcdEncoderState);
    while (1)
    {
        readScreenEncoder(&lcdEncoderState);

        if (validateLcdState(lcdEncoderState, lcdEncoderPrevState))
        {
            changeOption(&projectOption, &optionSelector, lcdEncoderState);
        }

        lcdEncoderPrevState = lcdEncoderState;
    }
}
