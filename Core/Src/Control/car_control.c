#include "car_control.h"

#define FRONT_STOP_CM 45.0f
#define FRONT_CLEAR_CM 50.0f
#define SIDE_STOP_CM 12.0f
#define SIDE_STEER_CM 30.0f
#define SIDE_STEER_MARGIN_CM 6.0f
#define TURN_CLEAR_CM 30.0f
#define TURN_STOP_CM 18.0f
#define FRONT_TURN_MIN_CM 18.0f
#define TURN_MS 180U
#define REVERSE_MS 120U
#define SETTLE_MS 100U
#define RECOVERY_RESET_MS 500U
#define MAX_TURN_ATTEMPTS 3U

static void SetAutoPhase(CarControl *car, CarAutoPhase phase, uint32_t now_ms)
{
    car->auto_phase = phase;
    car->auto_phase_ms = now_ms;
}

static void EnterStoppedState(CarControl *car, CarState state)
{
    car->state = state;
    SetAutoPhase(car, CAR_AUTO_SCAN, 0);
    car->auto_turn_attempts = car->auto_reversed = 0;
    car->manual_motion = 'S';
    car->motor_a = car->motor_b = 0;
}

static void BeginAuto(CarControl *car, uint32_t now_ms)
{
    EnterStoppedState(car, CAR_AUTO_MOVING);
    SetAutoPhase(car, CAR_AUTO_SCAN, now_ms);
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
            BeginAuto(car, rx_ms);
        return 1;
    }
    if (car->state != CAR_MANUAL)
    {
        if (byte != 'F') return 0;
        if (car->state == CAR_AUTO_STOPPED)
            BeginAuto(car, rx_ms);
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

static uint8_t RangeSince(CarRange range, uint32_t now_ms, uint32_t elapsed_ms, uint8_t allow_no_echo)
{
    return (range.cm > 0.0f || (allow_no_echo && range.no_echo)) &&
           now_ms - range.sampled_ms <= elapsed_ms;
}

static uint8_t SideHasRoom(CarRange side)
{
    return side.no_echo || side.cm > SIDE_STOP_CM;
}

static void SetAutoWheels(CarControl *car, int left, int right)
{
    car->motor_a = CAR_MOTOR_A_IS_RIGHT ? right : left;
    car->motor_b = CAR_MOTOR_A_IS_RIGHT ? left : right;
}

static void ChooseAutoManeuver(CarControl *car, uint32_t now_ms, const CarAutoRanges *ranges)
{
    uint32_t elapsed_ms = now_ms - car->auto_phase_ms;
    if (!RangeSince(ranges->front, now_ms, elapsed_ms, 0) ||
        !RangeSince(ranges->left, now_ms, elapsed_ms, 1) ||
        !RangeSince(ranges->right, now_ms, elapsed_ms, 1))
        return;

    if (ranges->front.cm >= FRONT_CLEAR_CM && SideHasRoom(ranges->left) && SideHasRoom(ranges->right))
        SetAutoPhase(car, CAR_AUTO_FORWARD, now_ms);
    else if (ranges->front.cm >= FRONT_TURN_MIN_CM && car->auto_turn_attempts < MAX_TURN_ATTEMPTS &&
             (ranges->left.cm >= TURN_CLEAR_CM || ranges->right.cm >= TURN_CLEAR_CM))
    {
        ++car->auto_turn_attempts;
        SetAutoPhase(car, ranges->left.cm > ranges->right.cm ?
                     CAR_AUTO_TURN_LEFT : CAR_AUTO_TURN_RIGHT, now_ms);
    }
    else if (!car->auto_reversed && ranges->front.cm <= FRONT_STOP_CM &&
             ranges->left.cm > 0.0f && ranges->right.cm > 0.0f)
    {
        /* The three sensors face forward. Limit backing to one pulse per recovery. */
        car->auto_reversed = 1;
        car->auto_turn_attempts = 0;
        SetAutoPhase(car, CAR_AUTO_REVERSE, now_ms);
    }
}

static void DriveForward(CarControl *car, const CarAutoRanges *ranges, int pwm)
{
    float left = ranges->left.cm;
    float right = ranges->right.cm;
    int inner = CAR_AUTO_MIN_PWM;
    int outer = pwm <= 90 ? pwm + 10 : 100;

    if (left > 0.0f && left <= SIDE_STEER_CM &&
        (ranges->right.no_echo || right >= left + SIDE_STEER_MARGIN_CM))
        SetAutoWheels(car, outer, inner);
    else if (right > 0.0f && right <= SIDE_STEER_CM &&
             (ranges->left.no_echo || left >= right + SIDE_STEER_MARGIN_CM))
        SetAutoWheels(car, inner, outer);
    else
        SetAutoWheels(car, pwm, pwm);
}

static void UpdateAuto(CarControl *car, uint32_t now_ms, const CarAutoRanges *ranges)
{
    if (car->auto_phase == CAR_AUTO_SCAN) ChooseAutoManeuver(car, now_ms, ranges);

    uint32_t elapsed_ms = now_ms - car->auto_phase_ms;
    int pwm = CAR_AUTO_PWM;
    if (pwm < CAR_AUTO_MIN_PWM) pwm = CAR_AUTO_MIN_PWM;
    if (pwm > 100) pwm = 100;
    switch (car->auto_phase)
    {
    case CAR_AUTO_SCAN:
        return;
    case CAR_AUTO_FORWARD:
        if (ranges->front.cm <= FRONT_STOP_CM ||
            !SideHasRoom(ranges->left) || !SideHasRoom(ranges->right))
        {
            SetAutoPhase(car, CAR_AUTO_SETTLE, now_ms);
            return;
        }
        if (elapsed_ms >= RECOVERY_RESET_MS) car->auto_turn_attempts = car->auto_reversed = 0;
        DriveForward(car, ranges, pwm);
        return;
    case CAR_AUTO_TURN_LEFT:
    case CAR_AUTO_TURN_RIGHT:
    {
        uint8_t turn_left = car->auto_phase == CAR_AUTO_TURN_LEFT;
        CarRange toward = turn_left ? ranges->left : ranges->right;
        if (elapsed_ms >= TURN_MS || ranges->front.cm <= 0.0f || toward.cm <= TURN_STOP_CM)
        {
            SetAutoPhase(car, CAR_AUTO_SETTLE, now_ms);
            return;
        }
        SetAutoWheels(car, turn_left ? -pwm : pwm, turn_left ? pwm : -pwm);
        return;
    }
    case CAR_AUTO_REVERSE:
        if (elapsed_ms >= REVERSE_MS || ranges->front.cm <= 0.0f)
            SetAutoPhase(car, CAR_AUTO_SETTLE, now_ms);
        else
            SetAutoWheels(car, -pwm, -pwm);
        return;
    case CAR_AUTO_SETTLE:
        if (elapsed_ms >= SETTLE_MS) SetAutoPhase(car, CAR_AUTO_SCAN, now_ms);
        return;
    }
}

void CarControl_Update(CarControl *car, uint32_t now_ms, const CarAutoRanges *ranges)
{
    car->motor_a = car->motor_b = 0;
    if (car->state == CAR_AUTO_MOVING)
    {
        UpdateAuto(car, now_ms, ranges);
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
