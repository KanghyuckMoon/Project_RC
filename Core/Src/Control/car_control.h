#ifndef CAR_CONTROL_H
#define CAR_CONTROL_H
#include <stdint.h>

#ifndef CAR_COMMAND_TIMEOUT_MS
#define CAR_COMMAND_TIMEOUT_MS 500U
#endif
/* The wheels need more than 70% PWM to move. */
#define CAR_AUTO_MIN_PWM 71
#ifndef CAR_AUTO_PWM
#define CAR_AUTO_PWM 80
#endif
/* Match the left/right response used by MoonCar's manual steering. */
#ifndef CAR_MOTOR_A_IS_RIGHT
#define CAR_MOTOR_A_IS_RIGHT 1
#endif

typedef enum {
    CAR_MANUAL,
    CAR_AUTO_STOPPED,
    CAR_AUTO_MOVING
} CarState;

typedef enum {
    CAR_AUTO_SCAN,
    CAR_AUTO_FORWARD,
    CAR_AUTO_TURN_LEFT,
    CAR_AUTO_TURN_RIGHT,
    CAR_AUTO_REVERSE,
    CAR_AUTO_SETTLE
} CarAutoPhase;

typedef struct {
    float cm; /* Zero if the sensor has no usable measurement. */
    uint32_t sampled_ms;
    uint8_t no_echo;
} CarRange;

typedef struct {
    CarRange front, left, right;
} CarAutoRanges;

typedef struct {
    CarState state;
    CarAutoPhase auto_phase;
    uint32_t auto_phase_ms;
    uint8_t auto_turn_attempts, auto_reversed;
    int speed, motor_a, motor_b;
    uint8_t manual_motion;
    uint32_t last_motion_ms;
} CarControl;

void CarControl_Init(CarControl *car);
/* Called only in the main loop. rx_ms is the receive timestamp, not dequeue time. */
/* Returns 1 for a recognized command, 0 for an ignored byte. */
uint8_t CarControl_Command(CarControl *car, uint8_t byte, uint32_t rx_ms);
void CarControl_Update(CarControl *car, uint32_t now_ms, const CarAutoRanges *ranges);
/* Reset to manual stop on startup or a transport fault. */
void CarControl_Stop(CarControl *car);
#endif
