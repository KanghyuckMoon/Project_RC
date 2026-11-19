#include "car_control.h"

static void EnterStoppedState(CarControl *car, CarState state)
{
    car->state = state;
    car->manual_motion = 'S';
    car->motor_a = car->motor_b = 0;
}

void CarControl_Stop(CarControl *car)
{
    EnterStoppedState(car, CAR_MANUAL);
}

void CarControl_Init(CarControl *car)
{
    *car = (CarControl){ .speed = 70 };
    CarControl_Stop(car);
}

uint8_t CarControl_Command(CarControl *car, uint8_t byte, uint32_t rx_ms)
{
    if (byte == 'S')
    {
        EnterStoppedState(car, car->state == CAR_MANUAL ? CAR_MANUAL : CAR_AUTO_STOPPED);
        return 1;
    }
    if (byte == 'M')
    {
        CarControl_Stop(car);
        return 1;
    }
    if (byte == 'A')
    {
        if (car->state == CAR_MANUAL)
            EnterStoppedState(car, CAR_AUTO_STOPPED);
        return 1;
    }
    if (car->state != CAR_MANUAL)
    {
        if (byte != 'F') return 0;
        car->state = CAR_AUTO_MOVING;
        return 1;
    }
    if (byte >= '0' && byte <= '9')
    {
        car->speed = (byte - '0') * 10;
        return 1;
    }
    if (byte == 'q')
    {
        car->speed = 100;
        return 1;
    }
    /* Match the current CarController button assignments. */
    if (byte == 'B') byte = 'L';
    else if (byte == 'L') byte = 'B';

    switch (byte)
    {
    case 'F': case 'B': case 'L': case 'R':
    case 'G': case 'I': case 'H': case 'J':
        car->manual_motion = byte;
        car->last_motion_ms = rx_ms;
        return 1;
    default: return 0; /* Separators/accessory bytes never extend the motion lease. */
    }
}

void CarControl_Update(CarControl *car, uint32_t now_ms)
{
    car->motor_a = car->motor_b = 0;
    if (car->state == CAR_AUTO_MOVING)
    {
        /* Temporary straight drive until ultrasonic steering is integrated. */
        car->motor_a = car->motor_b = CAR_AUTO_PWM;
        return;
    }
    if (car->state == CAR_AUTO_STOPPED) return;

    if ((uint32_t)(now_ms - car->last_motion_ms) >= CAR_COMMAND_TIMEOUT_MS)
    {
        car->manual_motion = 'S';
        return;
    }
    int speed = car->speed;
    /* A is assumed the left wheel and B the right wheel. */
    switch (car->manual_motion)
    {
    case 'F': car->motor_a = speed; car->motor_b = speed; break;
    case 'B': car->motor_a = -speed; car->motor_b = -speed; break;
    case 'L': car->motor_a = -speed; car->motor_b = speed; break;
    case 'R': car->motor_a = speed; car->motor_b = -speed; break;
    case 'G': car->motor_a = speed / 2; car->motor_b = speed; break;
    case 'I': car->motor_a = speed; car->motor_b = speed / 2; break;
    case 'H': car->motor_a = -speed / 2; car->motor_b = -speed; break;
    case 'J': car->motor_a = -speed; car->motor_b = -speed / 2; break;
    default: break;
    }
}
