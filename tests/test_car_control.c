#include "car_control.h"
#include <assert.h>
#include <stdio.h>

#ifndef TEST_EXPECTED_AUTO_PWM
#define TEST_EXPECTED_AUTO_PWM 80
#endif

static CarAutoRanges ranges;

static void Expect(CarControl *car, uint32_t now, int a, int b)
{
    CarControl_Update(car, now, &ranges);
    assert(car->motor_a == a && car->motor_b == b);
}

static void SetRanges(uint32_t now, float front, float left, float right)
{
    ranges = (CarAutoRanges){ {front, now, 0}, {left, now, 0}, {right, now, 0} };
}

static void ExpectWheels(CarControl *car, uint32_t now, int left, int right)
{
    Expect(car, now, CAR_MOTOR_A_IS_RIGHT ? right : left,
           CAR_MOTOR_A_IS_RIGHT ? left : right);
}

static void TestOpenSpace(void)
{
    const int pwm = TEST_EXPECTED_AUTO_PWM;
    CarControl car;
    CarControl_Init(&car);
    assert(CarControl_Command(&car, 'A', 1));
    assert(car.state == CAR_AUTO_MOVING);
    SetRanges(2, 0, 0, 0);
    ranges.front.no_echo = ranges.left.no_echo = ranges.right.no_echo = 1;
    Expect(&car, 2, 0, 0); /* Unknown front clearance keeps the wheels stopped. */
    ranges.front = (CarRange){400, 3, 0};
    Expect(&car, 3, pwm, pwm);
    ranges.front.cm = 30;
    Expect(&car, 10, 0, 0);
    Expect(&car, 110, 0, 0);
    SetRanges(120, 30, 0, 0);
    ranges.left.no_echo = ranges.right.no_echo = 1;
    Expect(&car, 120, 0, 0); /* Turning and reversing require measured space. */
    ranges.right = (CarRange){40, 121, 0};
    ExpectWheels(&car, 121, pwm, -pwm);
}

static void TestAvoidance(void)
{
    const int pwm = TEST_EXPECTED_AUTO_PWM;
    const int outer = pwm <= 90 ? pwm + 10 : 100;
    CarControl car;
    CarControl_Init(&car);
    SetRanges(0, 100, 100, 100);
    assert(CarControl_Command(&car, 'A', 1));
    Expect(&car, 1, 0, 0); /* Samples from before starting are ignored. */
    SetRanges(10, 75, 23, 25); /* Drive through a narrow, centered path. */
    Expect(&car, 10, pwm, pwm);
    SetRanges(20, 75, 20, 90);
    ExpectWheels(&car, 20, outer, CAR_AUTO_MIN_PWM);
    SetRanges(30, 75, 90, 20);
    ExpectWheels(&car, 30, CAR_AUTO_MIN_PWM, outer);

    SetRanges(220, 45, 100, 30);
    Expect(&car, 220, 0, 0);
    Expect(&car, 319, 0, 0);
    Expect(&car, 320, 0, 0);
    SetRanges(330, 42, 100, 30);
    ExpectWheels(&car, 330, -pwm, pwm);
    assert(CarControl_Command(&car, 'F', 400)); /* Repeated F preserves the turn timer. */
    ExpectWheels(&car, 509, -pwm, pwm);
    Expect(&car, 510, 0, 0);

    SetRanges(600, 100, 100, 100);
    Expect(&car, 610, 0, 0);
    Expect(&car, 620, 0, 0); /* Turn-time samples cannot restart motion. */
    SetRanges(650, 70, 90, 100);
    Expect(&car, 650, pwm, pwm);

    SetRanges(700, 80, 12, 90);
    Expect(&car, 700, 0, 0);
    Expect(&car, 800, 0, 0);
    SetRanges(810, 80, 12, 90);
    ExpectWheels(&car, 810, pwm, -pwm);
    SetRanges(820, 80, 12, 18);
    Expect(&car, 820, 0, 0); /* Abort if the turn direction becomes obstructed. */
    Expect(&car, 920, 0, 0);
    SetRanges(940, 0, 90, 90);
    Expect(&car, 940, 0, 0);
    SetRanges(950, 70, 0, 90);
    Expect(&car, 950, 0, 0);
    SetRanges(960, 70, 80, 90);
    Expect(&car, 960, pwm, pwm);
    SetRanges(970, 0, 80, 90);
    Expect(&car, 970, 0, 0);

    assert(CarControl_Command(&car, 'S', 980));
    SetRanges(990, 120, 120, 120);
    Expect(&car, 990, 0, 0);
    assert(CarControl_Command(&car, 'F', UINT32_MAX - 30U));
    SetRanges(UINT32_MAX - 10U, 20, 40, 80);
    ExpectWheels(&car, UINT32_MAX - 10U, pwm, -pwm);
    SetRanges(100, 80, 90, 100);
    ExpectWheels(&car, 100, pwm, -pwm);
    Expect(&car, 169, 0, 0); /* Turn limit survives tick rollover. */
    Expect(&car, 269, 0, 0);
    SetRanges(300, 20, 80, 40);
    ExpectWheels(&car, 300, -pwm, pwm);
    assert(CarControl_Command(&car, 'S', 301));
    Expect(&car, 301, 0, 0);
}

static void TestReverseRecovery(void)
{
    const int pwm = TEST_EXPECTED_AUTO_PWM;
    CarControl car;
    CarControl_Init(&car);
    CarControl_Command(&car, 'A', 0);
    SetRanges(10, 25, 20, 21);
    Expect(&car, 10, -pwm, -pwm);
    Expect(&car, 129, -pwm, -pwm);
    Expect(&car, 130, 0, 0);
    Expect(&car, 230, 0, 0);
    SetRanges(240, 25, 20, 21);
    Expect(&car, 240, 0, 0); /* Never repeatedly back up with no path ahead. */
    SetRanges(250, 25, 55, 21);
    ExpectWheels(&car, 250, -pwm, pwm);
    Expect(&car, 430, 0, 0);
    Expect(&car, 530, 0, 0);
    SetRanges(540, 65, 60, 60);
    Expect(&car, 540, pwm, pwm);
    Expect(&car, 1040, pwm, pwm); /* Restores the reverse allowance after forward travel. */
    SetRanges(1050, 40, 25, 25);
    Expect(&car, 1050, 0, 0);
    Expect(&car, 1150, 0, 0);
    SetRanges(1160, 15, 40, 40);
    Expect(&car, 1160, -pwm, -pwm); /* Back off before turning very close to a wall. */
    assert(CarControl_Command(&car, 'S', 1161));
    Expect(&car, 1161, 0, 0);
}

static void TestBoundedTurns(void)
{
    const int pwm = TEST_EXPECTED_AUTO_PWM;
    CarControl car;
    CarControl_Init(&car);
    CarControl_Command(&car, 'A', 0);
    uint32_t now = 1;
    for (unsigned i = 0; i < 7; ++i) {
        SetRanges(now, 30, 70, 70);
        if (i == 3) {
            Expect(&car, now, -pwm, -pwm);
            now += 120;
        } else {
            ExpectWheels(&car, now, pwm, -pwm);
            now += 180;
        }
        Expect(&car, now, 0, 0);
        now += 100;
        Expect(&car, now, 0, 0);
        ++now;
    }
    SetRanges(now, 30, 70, 70);
    Expect(&car, now, 0, 0);
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
    assert(car.state == CAR_AUTO_MOVING);
    assert(!CarControl_Command(&car, 'L', 1002));
    Expect(&car, 1002, 0, 0);

    assert(CarControl_Command(&car, 'F', 1003));
    assert(car.state == CAR_AUTO_MOVING);
    SetRanges(1003, 100, 100, 100);
    Expect(&car, 1003, TEST_EXPECTED_AUTO_PWM, TEST_EXPECTED_AUTO_PWM);
    Expect(&car, 10000, TEST_EXPECTED_AUTO_PWM, TEST_EXPECTED_AUTO_PWM);
    assert(!CarControl_Command(&car, 'R', 10001));
    assert(!CarControl_Command(&car, '0', 10002));
    assert(CarControl_Command(&car, 'A', 10003));
    Expect(&car, 10003, TEST_EXPECTED_AUTO_PWM, TEST_EXPECTED_AUTO_PWM);

    assert(CarControl_Command(&car, 'S', 10004));
    assert(car.state == CAR_AUTO_STOPPED);
    Expect(&car, 11000, 0, 0);
    assert(CarControl_Command(&car, 'F', 11001));
    SetRanges(11001, 100, 100, 100);
    Expect(&car, 11001, TEST_EXPECTED_AUTO_PWM, TEST_EXPECTED_AUTO_PWM);
    assert(CarControl_Command(&car, 'M', 11002));
    assert(car.state == CAR_MANUAL);
    Expect(&car, 11002, 0, 0);
    CarControl_Command(&car, 'L', 11003);
    Expect(&car, 11003, -70, -70);

    CarControl_Command(&car, 'A', 11004);
    CarControl_Command(&car, 'F', 11005);
    SetRanges(11005, 100, 100, 100);
    Expect(&car, 11005, TEST_EXPECTED_AUTO_PWM, TEST_EXPECTED_AUTO_PWM);
    CarControl_Stop(&car); /* Transport fault resets the mode and motor output. */
    assert(car.state == CAR_MANUAL);
    Expect(&car, 11005, 0, 0);
    TestAvoidance();
    TestOpenSpace();
    TestReverseRecovery();
    TestBoundedTurns();
    puts("car control tests passed");
}
