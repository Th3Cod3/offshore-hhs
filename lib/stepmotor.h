#ifndef STEPMOTOR_H
#define STEPMOTOR_H

#define STEPMOTOR_STOP 0
#define STEPMOTOR_FORWARD 1
#define STEPMOTOR_BACKWARD 2
#define STEPMOTOR_GRAP 3
#define STEPMOTOR_RELEASE 3

typedef struct
{
    volatile uint8_t *ddrMoveDir;
    volatile uint8_t *ddrMoveStep;
    volatile uint8_t *portMoveDir;
    volatile uint8_t *portMoveStep;
    uint8_t pinMoveDir;
    uint8_t pinMoveStep;
    volatile uint8_t *ddrGrapDir;
    volatile uint8_t *ddrGrapStep;
    volatile uint8_t *portGrapDir;
    volatile uint8_t *portGrapStep;
    uint8_t pinGrapDir;
    uint8_t pinGrapStep;
    volatile uint8_t *ddrLimit;
    volatile uint8_t *portLimit;
    volatile uint8_t *pinLimit;
    uint8_t limit;
} StepMotor;

extern uint8_t stepmotor_pending_step();
extern uint8_t stepmotor_start_limit(StepMotor motor);
extern uint8_t stepmotor_end_limit(StepMotor motor);
extern void stepmotor_enable_timer();
extern void stepmotor_disable_timer();
extern void stepmotor_instruction(StepMotor motor, char instruction);
extern void stepmotor_init(StepMotor motor);

#endif
