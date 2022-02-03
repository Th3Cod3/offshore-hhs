/*
stepmotor lib 0x01

copyright (c) Davide Gironi, 2012

Released under GPLv3.
Please refer to LICENSE file for licensing information.

Modify by Yefri Gonzalez (Th3Cod3)
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include "stepmotor.h"
#include "debug.h"

uint8_t stepmotor_pending_step()
{
    return !(TIFR0 & _BV(TOV0));
}

uint8_t stepmotor_end_limit(StepMotor motor)
{
    if (!motor.ddrLimit)
    {
        return 0;
    }
    return !(*motor.pinLimit & _BV(motor.limit));
}

void stepmotor_enable_timer()
{
    TIMSK0 |= _BV(TOIE0) | _BV(OCIE0A);
    sei();
}

void stepmotor_disable_timer()
{
    TIMSK0 &= ~(_BV(TOIE0) | _BV(OCIE0A));
}

void stepmotor_instruction(StepMotor motor, char instruction)
{
    if (stepmotor_pending_step())
    {
        return;
    }

    switch (instruction)
    {
    case STEPMOTOR_FORWARD:
        if (stepmotor_end_limit(motor))
            return stepmotor_instruction(motor, STEPMOTOR_STOP);
        *motor.portMoveDir |= _BV(motor.pinMoveDir);
        *motor.portMoveStep &= ~_BV(motor.pinMoveStep);
        *motor.portGrapDir |= _BV(motor.pinGrapDir);
        *motor.portGrapStep &= ~_BV(motor.pinGrapStep);
        stepmotor_enable_timer();
        return;

    case STEPMOTOR_BACKWARD:
        if (stepmotor_end_limit(motor))
            return stepmotor_instruction(motor, STEPMOTOR_STOP);
        *motor.portMoveDir &= ~_BV(motor.pinMoveDir);
        *motor.portMoveStep &= ~_BV(motor.pinMoveStep);
        *motor.portGrapDir &= ~_BV(motor.pinGrapDir);
        *motor.portGrapStep &= ~_BV(motor.pinGrapStep);
        stepmotor_enable_timer();
        return;

    case STEPMOTOR_STOP:
        return;
    }
}

/*
 * init a motor
 */
void stepmotor_init(StepMotor motor)
{
    *motor.ddrGrapDir |= _BV(motor.pinGrapDir);   // output
    *motor.ddrGrapStep |= _BV(motor.pinGrapStep); // output
    *motor.ddrMoveDir |= _BV(motor.pinMoveDir);   // output
    *motor.ddrMoveStep |= _BV(motor.pinMoveStep); // output
    if (*motor.ddrLimit)
    {
        *motor.ddrLimit &= ~_BV(motor.limit); // input
        *motor.portLimit |= _BV(motor.limit); // input
    }
    TCCR0A = 0;
    OCR0A = 125;
    TCCR0B |= _BV(CS01);
    stepmotor_enable_timer();
    stepmotor_instruction(motor, STEPMOTOR_STOP);
}