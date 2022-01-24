/*
dcmotor lib 0x01

copyright (c) Davide Gironi, 2012

Released under GPLv3.
Please refer to LICENSE file for licensing information.

Modify by Yefri Gonzalez (Th3Cod3)
*/

#include <avr/io.h>
#include "dcmotor.h"


void dcmotor_instruction(DcMotor motor, char instruction)
{
    switch (instruction)
    {
    case DCMOTOR_FORWARD:
        if (!(*motor.pinLimitA & _BV(motor.limitA)))
            return dcmotor_instruction(motor, DCMOTOR_STOP);
        *motor.portA |= _BV(motor.pinA);
        *motor.portB &= ~_BV(motor.pinB);
        return;

    case DCMOTOR_BACKWARD:
        if (!(*motor.pinLimitB & _BV(motor.limitB)))
            return dcmotor_instruction(motor, DCMOTOR_STOP);
        *motor.portA &= ~_BV(motor.pinA);
        *motor.portB |= _BV(motor.pinB);
        return;

    case DCMOTOR_STOP:
        *motor.portA &= ~_BV(motor.pinA);
        *motor.portB &= ~_BV(motor.pinB);
        return;
    }
}

/*
 * init a motor
 */
void dcmotor_init(DcMotor motor)
{
    *motor.ddrA |= _BV(motor.pinA);   // output
    *motor.ddrB |= _BV(motor.pinB);   // output
    *motor.ddrLimitA &= ~_BV(motor.limitA);   // output
    *motor.ddrLimitB &= ~_BV(motor.limitB);   // output
    *motor.portLimitA |= _BV(motor.limitA);   // output
    *motor.portLimitB |= _BV(motor.limitB);   // output
    dcmotor_instruction(motor, DCMOTOR_STOP);
}