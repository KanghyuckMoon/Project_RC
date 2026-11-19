#ifndef CAR_CONTROL_H
#define CAR_CONTROL_H
#include <stdint.h>

#ifndef CAR_COMMAND_TIMEOUT_MS
#define CAR_COMMAND_TIMEOUT_MS 500U
#endif
#ifndef CAR_AUTO_PWM
#define CAR_AUTO_PWM 40
#endif

typedef enum {
    CAR_MANUAL,
    CAR_AUTO_STOPPED,
    CAR_AUTO_MOVING
} CarState;

typedef struct {
    CarState state;
    int speed, motor_a, motor_b;
    uint8_t manual_motion;
    uint32_t last_motion_ms;
} CarControl;

void CarControl_Init(CarControl *car);
/* Called only in the main loop. rx_ms is the receive timestamp, not dequeue time. */
/* Returns 1 for a recognized command, 0 for an ignored byte. */
uint8_t CarControl_Command(CarControl *car, uint8_t byte, uint32_t rx_ms);
void CarControl_Update(CarControl *car, uint32_t now_ms);
/* Reset to manual stop on startup or a transport fault. */
void CarControl_Stop(CarControl *car);
#endif
