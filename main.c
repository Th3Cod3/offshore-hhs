#include <avr/io.h>
#include <avr/interrupt.h>
#include "lib/lcdpcf8574.h"
#include "lib/dcmotor.h"
#include "lib/stepmotor.h"
#include "lib/debug.h"

#define LCD_HIGH 0
#define LCD_LOW 1

#define LCD_ENCODER_A PC3
#define LCD_ENCODER_B PC2
#define LCD_ENCODER_BUTTON PC1

#define Y_ENCODER_A PC4
#define Y_ENCODER_B PC5
#define X_ENCODER_A PC6
#define X_ENCODER_B PC7

#define Z_STEPPER_DIR PL2
#define Z_STEPPER_STEP PL3
#define GRIP_STEPPER_DIR PL1
#define GRIP_STEPPER_STEP PL0

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
#define PROJECT_OPTION_CONFIG 2
#define PROJECT_OPTION_INFO 3
#define PROJECT_OPTION_DRIVE_MANUAL 11
#define PROJECT_OPTION_DRIVE_GRID 12
#define PROJECT_OPTION_CONFIG_CALIBRATION 21
#define PROJECT_OPTION_CONFIG_BOX 22

#define LCD_REFESH 1
#define LCD_NO_REFESH 2

//             {"Kraan naar A0"},
//     {"Calibratie", "Doos", "Terug"},
//         {"Verplaats x->", "Verplaats y->", "Verplaats z->"},
//         {"l=", "h=", "d=", "Terug"},

uint8_t position[] = {0, 0, 0};
uint8_t moveToPosition[] = {0, 0, 0};
uint8_t boxDimension[] = {10, 10, 10};
uint8_t boxMargin[] = {10, 10, 10};
uint8_t grid[] = {0, 0};
uint8_t tolerance = 2;
uint8_t emergency = 0;

ISR(INT4_vect)
{
    emergency = 1;
}

ISR(TIMER0_OVF_vect)
{
    stepmotor_disable_timer();
}

ISR(TIMER0_COMPA_vect)
{
    if (!emergency)
    {
        PORTL |= _BV(Z_STEPPER_STEP);
        PORTL |= _BV(GRIP_STEPPER_STEP);
        readZSteps();
    }
}

char menuText[][LCD_DISP_LENGTH + 1] = {"Besturing", "Instellingen", "Projectinfo"};
char projectInfo[][LCD_DISP_LENGTH + 1] = {"Druk voor terug", "Projectinfo", "Naam project:", "Project Onshore", "Gemaakt door:", "Projectgroep B1", "Leden:", "-Roy -Hicham", "-Ivan -Yefri", "-Sebastiaan", "Druk voor terug"};
char projectDrive[][LCD_DISP_LENGTH + 1] = {"Handmatig", "Raster", "Terug"};
char projectDriveManual[][LCD_DISP_LENGTH + 1] = {"x=", "y=", "z=", "Terug"};
char projectDriveGrid[][LCD_DISP_LENGTH + 1] = {"Raster=", "Starten", "Terug"};
char projectConfig[][LCD_DISP_LENGTH + 1] = {"Calibratie", "Doos", "Terug"};

const uint8_t optionsAmount = sizeof(menuText) / (LCD_DISP_LENGTH + 1);
const uint8_t optionsInfoAmount = sizeof(projectInfo) / (LCD_DISP_LENGTH + 1);
const uint8_t optionsConfigAmount = sizeof(projectConfig) / (LCD_DISP_LENGTH + 1);
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
        _delay_ms(1); // prevent debounce
        *lcdEncoderState = ENCODER_STATE_BUTTON;
    }
    else if (*lcdEncoderState != ENCODER_STATE_NONE && !lcdEncoderA && !lcdEncoderB)
    {
        *lcdEncoderState = ENCODER_STATE_NONE;
    }
    else if (*lcdEncoderState == ENCODER_STATE_NONE && lcdEncoderA && lcdEncoderB)
    {
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
    static int8_t toleranceCounter = 0;
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
        if (xEncoderState != ENCODER_STATE_LEFT && toleranceCounter == tolerance)
        {
            position[0]--;
            toleranceCounter = 0;
        }
        toleranceCounter++;
        xEncoderState = ENCODER_STATE_LEFT;
    }
    else if (xEncoderA && xEncoderState == ENCODER_STATE_B)
    {
        if (xEncoderState != ENCODER_STATE_RIGHT && toleranceCounter == -tolerance)
        {
            position[0]++;
            toleranceCounter = 0;
        }
        toleranceCounter--;
        xEncoderState = ENCODER_STATE_RIGHT;
    }
}

void readYEncoder()
{
    static uint8_t yEncoderState = ENCODER_STATE_NONE;
    static int8_t toleranceCounter = 0;
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
        if (yEncoderState != ENCODER_STATE_LEFT && toleranceCounter == tolerance)
        {
            position[1]--;
            toleranceCounter = 0;
        }
        toleranceCounter++;
        yEncoderState = ENCODER_STATE_LEFT;
    }
    else if (yEncoderA && yEncoderState == ENCODER_STATE_B)
    {
        if (yEncoderState != ENCODER_STATE_RIGHT && toleranceCounter == -tolerance)
        {
            position[1]++;
            toleranceCounter = 0;
        }
        toleranceCounter--;
        yEncoderState = ENCODER_STATE_RIGHT;
    }
}

void readZSteps()
{
    if (PORTL & _BV(Z_STEPPER_STEP))
    {
        position[2]--;
    }
    else
    {
        position[2]++;
    }
}

uint8_t moveOptionSelector(uint8_t *optionSelector, uint8_t lcdEncoderState, uint8_t totalOptions, uint8_t optionsOnScreen)
{
    // -1 of the index and -1 complement = -2
    if (lcdEncoderState == ENCODER_STATE_LEFT && (*optionSelector < totalOptions - optionsOnScreen || complement < optionsOnScreen - 1))
    {
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
        case PROJECT_OPTION_CONFIG - 1:
            *projectOption = PROJECT_OPTION_CONFIG;
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
    else if (*projectOption == PROJECT_OPTION_CONFIG)
    {
        switch ((*optionSelector) + complement)
        {
        case PROJECT_OPTION_CONFIG_CALIBRATION - 21:
            *projectOption = PROJECT_OPTION_CONFIG_CALIBRATION;
            break;
        case PROJECT_OPTION_CONFIG_BOX - 21:
            // *projectOption = PROJECT_OPTION_CONFIG_BOX;
            break;
        default:
            *projectOption = PROJECT_OPTION_NONE;
            break;
        }
    }
    else if (*projectOption == PROJECT_OPTION_DRIVE_MANUAL || *projectOption == PROJECT_OPTION_DRIVE_GRID)
    {
    }
    else
    {
        DEBUG_SIGNAL(10)
    }

    if (prevProjectOption != *projectOption)
    {
        complement = 0;
        *optionSelector = 0;
    }
}

void changeOption(uint8_t *projectOption, uint8_t *optionSelector, uint8_t lcdEncoderState, DcMotor motorX, DcMotor motorY)
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
    else if (*projectOption == PROJECT_OPTION_CONFIG)
    {
        moveOptionSelector(optionSelector, lcdEncoderState, optionsConfigAmount, optionsOnScreen);
        for (uint8_t i = 0; i < 2; i++)
        {
            !i ? lcd_clrscr() : lcd_gotoxy(0, 1);
            complement - i ? lcd_putc('-') : lcd_putc('>');
            lcd_puts(projectConfig[*optionSelector + i]);
        }
    }
    else if (*projectOption == PROJECT_OPTION_CONFIG_CALIBRATION)
    {
        lcd_clrscr();
        lcd_puts("Bezig...");
        while (!dcmotor_start_limit(motorX) || !dcmotor_start_limit(motorY))
        {
            dcmotor_instruction(motorX, DCMOTOR_BACKWARD);
            dcmotor_instruction(motorY, DCMOTOR_BACKWARD);
        }
        position[0] = 0;
        position[1] = 0;
        moveToPosition[0] = 0;
        moveToPosition[1] = 0;
        lcd_clrscr();
        lcd_puts("Start positie");
        *projectOption = PROJECT_OPTION_CONFIG;
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
            grid[*optionSelector + complement]++;
        }
        else if (lcdEncoderState == ENCODER_STATE_RIGHT)
        {
            grid[*optionSelector + complement]--;
        }

        for (uint8_t i = 0; i < optionsOnScreen; i++)
        {
            !i ? lcd_clrscr() : lcd_gotoxy(!(i % 2) ? LCD_DISP_LENGTH / 2 : 0, 1);
            complement - i ? lcd_putc('-') : lcd_putc('>');
            lcd_puts(projectDriveGrid[*optionSelector + i]);
            if (i == 0)
            {
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

void moveMotors(DcMotor motorX, DcMotor motorY, StepMotor motorZ)
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
            else if (i == 2)
            {
                if (position[i] < moveToPosition[i])
                {
                    stepmotor_instruction(motorZ, STEPMOTOR_FORWARD);
                }
                else
                {
                    stepmotor_instruction(motorZ, STEPMOTOR_BACKWARD);
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
            else if (i == 2)
            {
                stepmotor_instruction(motorZ, STEPMOTOR_STOP);
            }
        }
    }
}

void initEmergency(DcMotor motorX, DcMotor motorY, StepMotor motorZ)
{
    lcd_clrscr();
    lcd_puts("NOODSITUATIE!!!");
    dcmotor_instruction(motorX, DCMOTOR_STOP);
    dcmotor_instruction(motorY, DCMOTOR_STOP);
    stepmotor_instruction(motorZ, DCMOTOR_STOP);
}

int main(void)
{
    lcd_init(LCD_DISP_ON);
    lcd_led(LCD_HIGH);
    DDRC &= ~(_BV(LCD_ENCODER_A) | _BV(LCD_ENCODER_B) | _BV(LCD_ENCODER_BUTTON)); // inputs
    DDRC &= ~(_BV(X_ENCODER_A) | _BV(X_ENCODER_B));                               // input
    DDRC &= ~(_BV(Y_ENCODER_A) | _BV(Y_ENCODER_B));                               // input
    PORTC |= _BV(Y_ENCODER_A) | _BV(Y_ENCODER_B);                                 // pull-up
    PORTC |= _BV(X_ENCODER_A) | _BV(X_ENCODER_B);                                 // pull-up
    PORTC |= _BV(LCD_ENCODER_A) | _BV(LCD_ENCODER_B) | _BV(LCD_ENCODER_BUTTON);   // pull-up

    // external interrupt
    DDRE &= ~_BV(PE4); // input
    PORTE |= _BV(PE4); // pull-up
    EICRB |= _BV(ISC41);
    EIMSK |= _BV(INT4);
    sei();

    uint8_t lcdEncoderState = ENCODER_STATE_NONE;
    uint8_t projectOption = PROJECT_OPTION_NONE;
    // projectOption = PROJECT_OPTION_DRIVE_GRID;
    uint8_t lcdEncoderPrevState = ENCODER_STATE_UNKNOWN;
    uint8_t optionSelector = 0;
    uint8_t emergencyPrev = 0;
    DcMotor motorX, motorY;
    StepMotor motorZ;

    motorZ.ddrMoveDir = &DDRL;
    motorZ.portMoveDir = &PORTL;
    motorZ.pinMoveDir = Z_STEPPER_DIR;
    motorZ.ddrMoveStep = &DDRL;
    motorZ.portMoveStep = &PORTL;
    motorZ.pinMoveStep = Z_STEPPER_STEP;

    motorZ.ddrGrapDir = &DDRL;
    motorZ.portGrapDir = &PORTL;
    motorZ.pinGrapDir = GRIP_STEPPER_DIR;
    motorZ.ddrGrapStep = &DDRL;
    motorZ.portGrapStep = &PORTL;
    motorZ.pinGrapStep = GRIP_STEPPER_STEP;

    // motorZ.ddrLimit = &DDRA;
    // motorZ.portLimit = &PORTA;
    // motorZ.pinLimit = &PINA;
    // motorZ.limit = PA0;
    motorZ.ddrLimit = 0;
    motorZ.portLimit = 0;
    motorZ.pinLimit = 0;
    motorZ.limit = 0;

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

    dcmotor_init(motorX);
    dcmotor_init(motorY);
    stepmotor_init(motorZ);

    changeOption(&projectOption, &optionSelector, lcdEncoderState, motorX, motorY);

    while (1)
    {
        readScreenEncoder(&lcdEncoderState);

        if (emergency)
        {
            if (validateLcdState(lcdEncoderState, lcdEncoderPrevState) || !emergencyPrev)
            {
                emergencyPrev = 1;
                initEmergency(motorX, motorY, motorZ);
            }
            if (lcdEncoderState == ENCODER_STATE_BUTTON && !(PINE & _BV(PE4)))
            {
                emergency = 0;
            }
            lcdEncoderPrevState = lcdEncoderState;
            continue;
        }

        if (validateLcdState(lcdEncoderState, lcdEncoderPrevState) || emergencyPrev)
        {
            changeOption(&projectOption, &optionSelector, lcdEncoderState, motorX, motorY);
        }

        moveMotors(motorX, motorY, motorZ);

        lcdEncoderPrevState = lcdEncoderState;
        emergencyPrev = 0;
    }
}
