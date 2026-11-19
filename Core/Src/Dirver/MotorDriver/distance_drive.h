#ifndef DISTANCE_DRIVE_H
#define DISTANCE_DRIVE_H

#include <stdint.h>

typedef enum
{
    DRIVE_IDLE = 0,
    DRIVE_RUNNING = 1,
    DRIVE_COMPLETE = 2,
    DRIVE_CANCELLED = 3,
    DRIVE_INVALID_DISTANCE = 4,
    DRIVE_NOT_CALIBRATED = 5,
    DRIVE_CALIBRATING = 6,
    DRIVE_CALIBRATION_READY = 7
} DriveStatus;

/* Debugger input: set target first, then set drive_start_request to 1.
 * Requests are consumed once; another start while running is ignored. */
extern volatile float drive_target_cm;
extern volatile uint8_t drive_start_request;
extern volatile uint8_t drive_stop_request;
extern volatile DriveStatus drive_status;
extern volatile float drive_active_target_cm;
extern volatile uint32_t drive_planned_duration_ms;

/* Run for 2 seconds at PWM 70 without needing an RPM setting. After status 7,
 * enter the measured straight travel distance and set apply_request to 1. */
extern volatile uint8_t drive_calibration_request;
extern volatile uint8_t drive_calibration_apply_request;
extern volatile float drive_calibration_measured_cm;
extern volatile uint32_t drive_calibration_elapsed_ms;

/* Both outputs use PWM 70 while running, and zero otherwise. */
extern int drive_motor_pwm;
void DistanceDrive_Update(uint32_t now_ms);

#endif
