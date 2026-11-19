#include "car_control.h"
#include <assert.h>
#include <stdio.h>

static void Expect(CarControl *car, uint32_t now, int a, int b)
{
    CarControl_Update(car, now);
    assert(car->motor_a == a && car->motor_b == b);
}

int main(void)
{
    CarControl car;
    CarControl_Init(&car);
    assert(car.state == CAR_MANUAL);
    Expect(&car, 0, 0, 0);
    const char commands[] = "FBLRGIHJ";
    const int outputs[][2] = {
        {70,70}, {-70,70}, {-70,-70}, {70,-70},
        {35,70}, {70,35}, {-35,-70}, {-70,-35}
    };
    for (unsigned i = 0; i < sizeof(commands) - 1; ++i)
    {
        CarControl_Command(&car, commands[i], 100);
        Expect(&car, 100, outputs[i][0], outputs[i][1]);
    }
    CarControl_Command(&car, 'S', 101);
    Expect(&car, 101, 0, 0);
    assert(car.state == CAR_MANUAL);
    CarControl_Command(&car, 'F', 200);
    Expect(&car, 699, 70, 70);
    /* Speed/accessory/newline packets cannot keep a lost direction alive. */
    CarControl_Command(&car, '9', 699);
    CarControl_Command(&car, 'V', 699);
    CarControl_Command(&car, '\n', 699);
    Expect(&car, 700, 0, 0);
    CarControl_Command(&car, 'q', 701);
    Expect(&car, 701, 0, 0); /* Timeout cleared the direction latch. */
    CarControl_Command(&car, 'F', 702);
    Expect(&car, 702, 100, 100);
    CarControl_Command(&car, '0', 703);
    Expect(&car, 703, 0, 0);
    CarControl_Command(&car, 'F', UINT32_MAX - 100U);
    CarControl_Command(&car, '7', UINT32_MAX - 100U);
    Expect(&car, 398, 70, 70);
    Expect(&car, 399, 0, 0);

    CarControl_Command(&car, 'F', 1000);
    Expect(&car, 1000, 70, 70);
    assert(CarControl_Command(&car, 'A', 1001));
    assert(car.state == CAR_AUTO_STOPPED);
    assert(!CarControl_Command(&car, 'L', 1002));
    Expect(&car, 1002, 0, 0);

    assert(CarControl_Command(&car, 'F', 1003));
    assert(car.state == CAR_AUTO_MOVING);
    Expect(&car, 1003, CAR_AUTO_PWM, CAR_AUTO_PWM);
    Expect(&car, 10000, CAR_AUTO_PWM, CAR_AUTO_PWM);
    assert(!CarControl_Command(&car, 'R', 10001));
    assert(!CarControl_Command(&car, '0', 10002));
    assert(CarControl_Command(&car, 'A', 10003));
    Expect(&car, 10003, CAR_AUTO_PWM, CAR_AUTO_PWM);

    assert(CarControl_Command(&car, 'S', 10004));
    assert(car.state == CAR_AUTO_STOPPED);
    Expect(&car, 11000, 0, 0);
    assert(CarControl_Command(&car, 'F', 11001));
    Expect(&car, 11001, CAR_AUTO_PWM, CAR_AUTO_PWM);
    assert(CarControl_Command(&car, 'M', 11002));
    assert(car.state == CAR_MANUAL);
    Expect(&car, 11002, 0, 0);
    CarControl_Command(&car, 'L', 11003);
    Expect(&car, 11003, -70, -70);

    CarControl_Command(&car, 'A', 11004);
    CarControl_Command(&car, 'F', 11005);
    Expect(&car, 11005, CAR_AUTO_PWM, CAR_AUTO_PWM);
    CarControl_Stop(&car); /* Transport fault resets the mode and motor output. */
    assert(car.state == CAR_MANUAL);
    Expect(&car, 11005, 0, 0);
    puts("car control tests passed");
}
