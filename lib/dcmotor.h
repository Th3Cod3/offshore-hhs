#ifndef DCMOTOR_H
#define DCMOTOR_H

#define DCMOTOR_STOP 0
#define DCMOTOR_FORWARD 1
#define DCMOTOR_BACKWARD 2

typedef struct
{
    volatile uint8_t *ddrA;
    volatile uint8_t *ddrB;
    volatile uint8_t *portA;
    volatile uint8_t *portB;
    uint8_t pinA;
    uint8_t pinB;
    volatile uint8_t *ddrLimitA;
    volatile uint8_t *ddrLimitB;
    volatile uint8_t *portLimitA;
    volatile uint8_t *portLimitB;
    volatile uint8_t *pinLimitA;
    volatile uint8_t *pinLimitB;
    uint8_t limitA;
    uint8_t limitB;
} DcMotor;

extern void dcmotor_instruction(DcMotor motor, char instruction);
extern uint8_t dcmotor_start_limit(DcMotor motor);
extern uint8_t dcmotor_end_limit(DcMotor motor);
extern void dcmotor_init(DcMotor motor);

#endif
