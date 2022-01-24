#include <avr/io.h>
#include "lib/lcdpcf8574.h"
#include "lib/dcmotor.h"

#ifdef DEBUG_EN
#include <util/delay.h>
#define DEBUG_PIN PORTB7
#define DEBUG_SIGNAL(delay)  \
    DDRB |= _BV(DEBUG_PIN);  \
    PORTB ^= _BV(DEBUG_PIN); \
    _delay_ms(delay);        \
    PORTB ^= _BV(DEBUG_PIN);
#else
#define DEBUG_SIGNAL(delay)
#endif

#define LCD_HIGH 0
#define LCD_LOW 1

#define LCD_ENCODER_A PC3
#define LCD_ENCODER_B PC2
#define LCD_ENCODER_BUTTON PC1

#define Y_ENCODER_A PC4
#define Y_ENCODER_B PC5
#define X_ENCODER_A PC6
#define X_ENCODER_B PC7

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

uint8_t position[] = {0, 0, 0};
uint8_t moveToPosition[] = {0, 0, 0};
uint8_t boxDimension[] = {10, 10, 10};
uint8_t boxMargin[] = {10, 10, 10};
uint8_t grid[] = {'A', 0};

char menuText[][LCD_DISP_LENGTH + 1] = {"Besturing", "Instellingen", "Projectinfo"};
char projectInfo[][LCD_DISP_LENGTH + 1] = {"Druk voor terug", "Projectinfo", "Naam project:", "Project Onshore", "Gemaakt door:", "Projectgroep B1", "Leden:", "-Roy -Hicham", "-Ivan -Yefri", "-Sebastiaan", "Druk voor terug"};
char projectDrive[][LCD_DISP_LENGTH + 1] = {"Handmatig", "Raster", "Terug"};
char projectDriveManual[][LCD_DISP_LENGTH + 1] = {"x=", "y=", "z=", "Terug"};
char projectDriveGrid[][LCD_DISP_LENGTH + 1] = {"Raster=", "Starten", "Terug"};

const uint8_t optionsAmount = sizeof(menuText) / (LCD_DISP_LENGTH + 1);
const uint8_t optionsInfoAmount = sizeof(projectInfo) / (LCD_DISP_LENGTH + 1);
const uint8_t optionsDriveAmount = sizeof(projectDrive) / (LCD_DISP_LENGTH + 1);
const uint8_t optionsDriveManualAmount = sizeof(projectDriveManual) / (LCD_DISP_LENGTH + 1);
const uint8_t optionsDriveGridAmount = sizeof(projectDriveGrid) / (LCD_DISP_LENGTH + 1);

uint8_t complement = 0;

void readScreenEncoder(uint8_t *lcdEncoderState)
{
    uint8_t lcdEncoderA = !(PINC & _BV(LCD_ENCODER_A));
    uint8_t lcdEncoderB = !(PINC & _BV(LCD_ENCODER_B));
    uint8_t lcdEncoderButton = !(PINC & _BV(LCD_ENCODER_BUTTON));

    if (lcdEncoderButton)
    {
        _delay_us(500); // prevent debounce
        *lcdEncoderState = ENCODER_STATE_BUTTON;
    }
    else if (*lcdEncoderState != ENCODER_STATE_NONE && !lcdEncoderA && !lcdEncoderB)
    {
        *lcdEncoderState = ENCODER_STATE_NONE;
    }
    else if (*lcdEncoderState == ENCODER_STATE_NONE && lcdEncoderA && lcdEncoderB)
    {
        DEBUG_SIGNAL(1)
        return;
    }
    else if (lcdEncoderA && *lcdEncoderState == ENCODER_STATE_NONE)
    {
        *lcdEncoderState = ENCODER_STATE_A;
    }
    else if (lcdEncoderB && *lcdEncoderState == ENCODER_STATE_NONE)
    {
        *lcdEncoderState = ENCODER_STATE_B;
    }
    else if (lcdEncoderB && *lcdEncoderState == ENCODER_STATE_A)
    {
        *lcdEncoderState = ENCODER_STATE_LEFT;
    }
    else if (lcdEncoderA && *lcdEncoderState == ENCODER_STATE_B)
    {
        *lcdEncoderState = ENCODER_STATE_RIGHT;
    }
}

void readXEncoder()
{
    static uint8_t xEncoderState = ENCODER_STATE_NONE;
    uint8_t xEncoderA = (PINC & _BV(X_ENCODER_A));
    uint8_t xEncoderB = (PINC & _BV(X_ENCODER_B));

    if (xEncoderState != ENCODER_STATE_NONE && !xEncoderA && !xEncoderB)
    {
        xEncoderState = ENCODER_STATE_NONE;
    }
    else if (xEncoderState == ENCODER_STATE_NONE && xEncoderA && xEncoderB)
    {
        return;
    }
    else if (xEncoderA && xEncoderState == ENCODER_STATE_NONE)
    {
        xEncoderState = ENCODER_STATE_A;
    }
    else if (xEncoderB && xEncoderState == ENCODER_STATE_NONE)
    {
        xEncoderState = ENCODER_STATE_B;
    }
    else if (xEncoderB && xEncoderState == ENCODER_STATE_A)
    {
        if (xEncoderState != ENCODER_STATE_LEFT)
            position[0]--;
        xEncoderState = ENCODER_STATE_LEFT;
    }
    else if (xEncoderA && xEncoderState == ENCODER_STATE_B)
    {
        if (xEncoderState != ENCODER_STATE_RIGHT)
            position[0]++;
        xEncoderState = ENCODER_STATE_RIGHT;
    }
}

void readYEncoder()
{
    static uint8_t yEncoderState = ENCODER_STATE_NONE;
    uint8_t yEncoderA = (PINC & _BV(Y_ENCODER_A));
    uint8_t yEncoderB = (PINC & _BV(Y_ENCODER_B));

    if (yEncoderState != ENCODER_STATE_NONE && !yEncoderA && !yEncoderB)
    {
        yEncoderState = ENCODER_STATE_NONE;
    }
    else if (yEncoderState == ENCODER_STATE_NONE && yEncoderA && yEncoderB)
    {
        return;
    }
    else if (yEncoderA && yEncoderState == ENCODER_STATE_NONE)
    {
        yEncoderState = ENCODER_STATE_A;
    }
    else if (yEncoderB && yEncoderState == ENCODER_STATE_NONE)
    {
        yEncoderState = ENCODER_STATE_B;
    }
    else if (yEncoderB && yEncoderState == ENCODER_STATE_A)
    {
        if (yEncoderState != ENCODER_STATE_LEFT)
            position[1]--;
        yEncoderState = ENCODER_STATE_LEFT;
    }
    else if (yEncoderA && yEncoderState == ENCODER_STATE_B)
    {
        if (yEncoderState != ENCODER_STATE_RIGHT)
            position[1]++;
        yEncoderState = ENCODER_STATE_RIGHT;
    }
}

uint8_t moveOptionSelector(uint8_t *optionSelector, uint8_t lcdEncoderState, uint8_t totalOptions, uint8_t optionsOnScreen)
{
    // -1 of the index and -1 complement = -2
    if (lcdEncoderState == ENCODER_STATE_LEFT && (*optionSelector < totalOptions - optionsOnScreen || complement < optionsOnScreen - 1))
    {
        DEBUG_SIGNAL(10);
        if (complement < optionsOnScreen - 1)
        {
            complement++;
        }
        else
        {
            *optionSelector += 1;
        }
    }
    else if (lcdEncoderState == ENCODER_STATE_RIGHT && (*optionSelector || complement))
    {
        if (complement)
        {
            complement--;
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

void chooseOption(uint8_t *optionSelector, uint8_t *projectOption)
{
    uint8_t prevProjectOption = *projectOption;
    if (*projectOption == PROJECT_OPTION_NONE)
    {
        switch ((*optionSelector) + complement)
        {
        case PROJECT_OPTION_DRIVE - 1:
            *projectOption = PROJECT_OPTION_DRIVE;
            break;
        case PROJECT_OPTION_SETTINGS - 1:
            break;
        case PROJECT_OPTION_INFO - 1:
            *projectOption = PROJECT_OPTION_INFO;
            break;
        }
    }
    else if (*projectOption == PROJECT_OPTION_DRIVE)
    {
        switch ((*optionSelector) + complement)
        {
        case PROJECT_OPTION_DRIVE_MANUAL - 11:
            *projectOption = PROJECT_OPTION_DRIVE_MANUAL;
            break;
        case PROJECT_OPTION_DRIVE_GRID - 11:
            *projectOption = PROJECT_OPTION_DRIVE_GRID;
            break;
        default:
            *projectOption = PROJECT_OPTION_NONE;
            break;
        }
    }
    else if (*projectOption == PROJECT_OPTION_INFO)
    {
        *projectOption = PROJECT_OPTION_NONE;
    }
    else if (*projectOption == PROJECT_OPTION_DRIVE_MANUAL || *projectOption == PROJECT_OPTION_DRIVE_GRID)
    {
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

void changeOption(uint8_t *projectOption, uint8_t *optionSelector, uint8_t lcdEncoderState)
{
    uint8_t optionsOnScreen = 2;
    if (lcdEncoderState == ENCODER_STATE_BUTTON)
    {
        chooseOption(optionSelector, projectOption);
    }

    if (*projectOption == PROJECT_OPTION_NONE)
    {
        moveOptionSelector(optionSelector, lcdEncoderState, optionsAmount, optionsOnScreen);
        for (uint8_t i = 0; i < 2; i++)
        {
            !i ? lcd_clrscr() : lcd_gotoxy(0, 1);
            complement - i ? lcd_putc('-') : lcd_putc('>');
            lcd_puts(menuText[*optionSelector + i]);
        }
    }
    else if (*projectOption == PROJECT_OPTION_DRIVE)
    {
        moveOptionSelector(optionSelector, lcdEncoderState, optionsDriveAmount, optionsOnScreen);
        for (uint8_t i = 0; i < 2; i++)
        {
            !i ? lcd_clrscr() : lcd_gotoxy(0, 1);
            complement - i ? lcd_putc('-') : lcd_putc('>');
            lcd_puts(projectDrive[*optionSelector + i]);
        }
    }
    else if (*projectOption == PROJECT_OPTION_DRIVE_MANUAL)
    {
        optionsOnScreen = 4;
        const uint8_t backOption = 3;
        if (lcdEncoderState == ENCODER_STATE_BUTTON && *optionSelector + complement < backOption)
        {
            moveOptionSelector(optionSelector, ENCODER_STATE_LEFT, optionsDriveManualAmount, optionsOnScreen);
        }
        else if (lcdEncoderState == ENCODER_STATE_BUTTON || (lcdEncoderState != ENCODER_STATE_BUTTON && *optionSelector + complement == backOption))
        {
            if (lcdEncoderState != ENCODER_STATE_BUTTON)
            {
                *projectOption = PROJECT_OPTION_DRIVE;
            }
            complement = 0;
            *optionSelector = 0;
        }
        else if (lcdEncoderState == ENCODER_STATE_LEFT)
        {
            moveToPosition[*optionSelector + complement]++;
        }
        else if (lcdEncoderState == ENCODER_STATE_RIGHT)
        {
            moveToPosition[*optionSelector + complement]--;
        }
        for (uint8_t i = 0; i < optionsOnScreen; i++)
        {
            !i ? lcd_clrscr() : lcd_gotoxy(i % 2 ? LCD_DISP_LENGTH / 2 : 0, i > 1 ? 1 : 0);
            complement - i ? lcd_putc('-') : lcd_putc('>');
            lcd_puts(projectDriveManual[*optionSelector + i]);
            if (i < backOption)
                lcd_puti(moveToPosition[*optionSelector + i]);
        }
    }
    else if (*projectOption == PROJECT_OPTION_DRIVE_GRID)
    {
        optionsOnScreen = 4;
        const uint8_t backOption = 3;
        if (lcdEncoderState == ENCODER_STATE_BUTTON && *optionSelector + complement < backOption)
        {
            moveOptionSelector(optionSelector, ENCODER_STATE_LEFT, optionsDriveGridAmount, optionsOnScreen);
        }
        else if (lcdEncoderState == ENCODER_STATE_BUTTON || (lcdEncoderState != ENCODER_STATE_BUTTON && *optionSelector + complement == backOption))
        {
            if (lcdEncoderState != ENCODER_STATE_BUTTON)
            {
                *projectOption = PROJECT_OPTION_DRIVE;
            }
            complement = 0;
            *optionSelector = 0;
        }
        else if (lcdEncoderState == ENCODER_STATE_LEFT)
        {
            moveToPosition[*optionSelector + complement]++;
        }
        else if (lcdEncoderState == ENCODER_STATE_RIGHT)
        {
            moveToPosition[*optionSelector + complement]--;
        }
        for (uint8_t i = 0; i < optionsOnScreen; i++)
        {
            !i ? lcd_clrscr() : lcd_gotoxy(!(i % 2) ? LCD_DISP_LENGTH / 2 : 0, 1);
            complement - i ? lcd_putc('-') : lcd_putc('>');
            lcd_puts(projectDriveGrid[*optionSelector + i]);
            if (i == 0){
                lcd_putc(' ');
                lcd_putc(grid[0] + 'A');
                lcd_puti(grid[1]);
            }
        }
    }
    else if (*projectOption == PROJECT_OPTION_INFO)
    {
        moveOptionSelector(optionSelector, lcdEncoderState, optionsInfoAmount, optionsOnScreen);
        for (uint8_t i = 0; i < 2; i++)
        {
            !i ? lcd_clrscr() : lcd_gotoxy(0, 1);
            lcd_puts(projectInfo[*optionSelector + i]);
        }
    }
}

uint8_t validateLcdState(uint8_t lcdEncoderState, uint8_t lcdEncoderPrevState)
{
    uint8_t hasActionsState = lcdEncoderState == ENCODER_STATE_RIGHT || lcdEncoderState == ENCODER_STATE_LEFT || lcdEncoderState == ENCODER_STATE_BUTTON;
    uint8_t sameAsPrevState = lcdEncoderPrevState == lcdEncoderState;
    return hasActionsState && !sameAsPrevState;
}

void moveMotors(DcMotor motorX, DcMotor motorY)
{
    readXEncoder();
    readYEncoder();
    for (uint8_t i = 0; i < 3; i++)
    {
        if (position[i] != moveToPosition[i])
        {
            if (i == 0)
            {
                if (position[i] < moveToPosition[i])
                {
                    dcmotor_instruction(motorX, DCMOTOR_FORWARD);
                }
                else
                {
                    dcmotor_instruction(motorX, DCMOTOR_BACKWARD);
                }
            }
            else if (i == 1)
            {
                if (position[i] < moveToPosition[i])
                {
                    dcmotor_instruction(motorY, DCMOTOR_FORWARD);
                }
                else
                {
                    dcmotor_instruction(motorY, DCMOTOR_BACKWARD);
                }
            }
        }
        else
        {
            if (i == 0)
            {
                dcmotor_instruction(motorX, DCMOTOR_STOP);
            }
            else if (i == 1)
            {
                dcmotor_instruction(motorY, DCMOTOR_STOP);
            }
        }
    }
}

int main(void)
{
    lcd_init(LCD_DISP_ON);
    lcd_led(LCD_HIGH);
    DDRC &= ~(_BV(LCD_ENCODER_A) | _BV(LCD_ENCODER_B) | _BV(LCD_ENCODER_BUTTON));
    DDRC &= ~(_BV(X_ENCODER_A) | _BV(X_ENCODER_B));
    DDRC &= ~(_BV(Y_ENCODER_A) | _BV(Y_ENCODER_B));
    PORTC |= _BV(Y_ENCODER_A) | _BV(Y_ENCODER_B);
    PORTC |= _BV(X_ENCODER_A) | _BV(X_ENCODER_B);
    PORTC |= _BV(LCD_ENCODER_A) | _BV(LCD_ENCODER_B) | _BV(LCD_ENCODER_BUTTON);

    uint8_t lcdEncoderState = ENCODER_STATE_NONE;
    uint8_t projectOption = PROJECT_OPTION_NONE;
    uint8_t lcdEncoderPrevState = ENCODER_STATE_UNKNOWN;
    uint8_t optionSelector = 0;
    DcMotor motorX, motorY;

    motorX.ddrA = &DDRL;
    motorX.portA = &PORTL;
    motorX.pinA = PL5;
    motorX.ddrB = &DDRL;
    motorX.portB = &PORTL;
    motorX.pinB = PL4;
    motorX.ddrLimitA = &DDRA;
    motorX.portLimitA = &PORTA;
    motorX.pinLimitA = &PINA;
    motorX.limitA = PA0;
    motorX.ddrLimitB = &DDRA;
    motorX.portLimitB = &PORTA;
    motorX.pinLimitB = &PINA;
    motorX.limitB = PA1;

    motorY.ddrA = &DDRL;
    motorY.portA = &PORTL;
    motorY.pinA = PL7;
    motorY.ddrB = &DDRL;
    motorY.portB = &PORTL;
    motorY.pinB = PL6;
    motorY.ddrLimitA = &DDRA;
    motorY.portLimitA = &PORTA;
    motorY.pinLimitA = &PINA;
    motorY.limitA = PA2;
    motorY.ddrLimitB = &DDRA;
    motorY.portLimitB = &PORTA;
    motorY.pinLimitB = &PINA;
    motorY.limitB = PA3;

    changeOption(&projectOption, &optionSelector, lcdEncoderState);
    uint8_t state = DCMOTOR_STOP;

    while (1)
    {
        readScreenEncoder(&lcdEncoderState);
        if (lcdEncoderState == ENCODER_STATE_BUTTON)
        {
            dcmotor_instruction(motorX, DCMOTOR_FORWARD);
            dcmotor_instruction(motorY, DCMOTOR_FORWARD);
        }
        else
        {
            dcmotor_instruction(motorX, DCMOTOR_BACKWARD);
            dcmotor_instruction(motorY, DCMOTOR_BACKWARD);
        }

        continue;

        if (validateLcdState(lcdEncoderState, lcdEncoderPrevState))
        {
            changeOption(&projectOption, &optionSelector, lcdEncoderState);
        }

        moveMotors(motorX, motorY);

        lcdEncoderPrevState = lcdEncoderState;
    }
}
