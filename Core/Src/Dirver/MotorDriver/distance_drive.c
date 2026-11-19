#include "distance_drive.h"
#include "wheel_distance.h"
#include <math.h>

volatile float drive_target_cm = 100.0f;
volatile uint8_t drive_start_request;
volatile uint8_t drive_stop_request;
volatile DriveStatus drive_status;
volatile float drive_active_target_cm;
volatile uint32_t drive_planned_duration_ms;
int drive_motor_pwm;
volatile uint8_t drive_calibration_request;
volatile uint8_t drive_calibration_apply_request;
volatile float drive_calibration_measured_cm;
volatile uint32_t drive_calibration_elapsed_ms;
static uint8_t calibration_sample_ready;

static uint32_t start_ms;

void DistanceDrive_Update(uint32_t now_ms)
{
    if (drive_stop_request)
    {
        drive_stop_request = 0;
        drive_start_request = 0;
        drive_calibration_request = 0;
        drive_calibration_apply_request = 0;
        calibration_sample_ready = 0;
        drive_motor_pwm = 0;
        drive_status = DRIVE_CANCELLED;
        return;
    }

    if (drive_calibration_request)
    {
        drive_calibration_request = 0;
        if (drive_status != DRIVE_RUNNING && drive_status != DRIVE_CALIBRATING)
        {
            drive_start_request = 0;
            drive_calibration_apply_request = 0;
            calibration_sample_ready = 0;
            drive_calibration_measured_cm = 0.0f;
            drive_calibration_elapsed_ms = 0;
            drive_active_target_cm = 0.0f;
            drive_planned_duration_ms = 2000;
            WheelDistance_Reset(now_ms);
            start_ms = now_ms;
            drive_status = DRIVE_CALIBRATING;
            drive_motor_pwm = 70;
        }
    }

    if (drive_calibration_apply_request)
    {
        drive_calibration_apply_request = 0;
        if (drive_status != DRIVE_RUNNING && drive_status != DRIVE_CALIBRATING)
        {
            if (calibration_sample_ready &&
                WheelDistance_CalibrateTravel(drive_calibration_measured_cm,
                                             drive_calibration_elapsed_ms))
            {
                calibration_sample_ready = 0;
                WheelDistance_Reset(now_ms);
                drive_status = DRIVE_IDLE;
            }
            else
            {
                drive_status = DRIVE_INVALID_DISTANCE;
            }
        }
    }

    if (drive_status == DRIVE_CALIBRATING)
    {
        drive_start_request = 0;
        if ((uint32_t)(now_ms - start_ms) >= drive_planned_duration_ms)
        {
            drive_calibration_elapsed_ms = now_ms - start_ms;
            calibration_sample_ready = 1;
            drive_motor_pwm = 0;
            drive_status = DRIVE_CALIBRATION_READY;
        }
        return;
    }

    if (drive_start_request)
    {
        drive_start_request = 0;
        if (drive_status != DRIVE_RUNNING)
        {
            calibration_sample_ready = 0;
            float target = drive_target_cm;
            float speed_cm_s = WheelDistance_VehicleSpeedCmPerSecond(70);
            drive_motor_pwm = 0;
            drive_active_target_cm = 0.0f;
            drive_planned_duration_ms = 0;
            if (!isfinite(target) || target <= 0.0f)
            {
                drive_status = DRIVE_INVALID_DISTANCE;
                return;
            }
            if (!isfinite(speed_cm_s) || speed_cm_s <= 0.0f)
            {
                drive_status = DRIVE_NOT_CALIBRATED;
                return;
            }

            float duration_ms = ceilf((target / speed_cm_s) * 1000.0f);
            /* Keep the duration representable before converting to uint32_t. */
            if (!isfinite(duration_ms) || duration_ms > 2147483520.0f)
            {
                drive_status = DRIVE_INVALID_DISTANCE;
                return;
            }
            if (duration_ms < 1.0f) duration_ms = 1.0f;
            WheelDistance_Reset(now_ms);
            start_ms = now_ms;
            drive_active_target_cm = target;
            drive_planned_duration_ms = (uint32_t)duration_ms;
            drive_status = DRIVE_RUNNING;
            drive_motor_pwm = 70;
        }
    }

    if (drive_status == DRIVE_RUNNING &&
        (vehicle_estimated_travel_cm >= drive_active_target_cm ||
         (uint32_t)(now_ms - start_ms) >= drive_planned_duration_ms))
    {
        drive_motor_pwm = 0;
        drive_status = DRIVE_COMPLETE;
    }
}
