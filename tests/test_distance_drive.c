#include "distance_drive.h"
#include "wheel_distance.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void Start(float target, uint32_t now)
{
    drive_target_cm = target;
    drive_start_request = 1;
    DistanceDrive_Update(now);
    assert(drive_start_request == 0);
}

int main(void)
{
#ifdef TEST_UNCALIBRATED
    wheel_a_rpm_at_70 = 0.0f;
    wheel_b_rpm_at_70 = 0.0f;
#endif
    WheelDistance_Reset(0);
    assert(drive_motor_pwm == 0);
    DistanceDrive_Update(0);
    assert(drive_status == DRIVE_IDLE);
#ifdef TEST_UNCALIBRATED
    Start(100.0f, 0);
    assert(drive_status == DRIVE_NOT_CALIBRATED);
    assert(drive_motor_pwm == 0);
#else
    Start(100.0f, 100);
    assert(drive_status == DRIVE_RUNNING);
    assert(drive_motor_pwm == 70);
    assert(drive_planned_duration_ms == 3184);
    assert(drive_active_target_cm == 100.0f);
    /* Editing target or requesting a start during a run cannot extend it. */
    Start(200.0f, 101);
    assert(drive_active_target_cm == 100.0f);
    uint32_t elapsed;
    for (elapsed = 1; elapsed <= 3200; ++elapsed)
    {
        WheelDistance_Update(100 + elapsed, drive_motor_pwm, drive_motor_pwm);
        DistanceDrive_Update(100 + elapsed);
        if (drive_status != DRIVE_RUNNING) break;
    }
    assert(drive_status == DRIVE_COMPLETE);
    assert(drive_motor_pwm == 0);
    assert(vehicle_estimated_travel_cm >= 99.99f);
    assert(vehicle_estimated_travel_cm < 100.04f);
    float stopped_distance = vehicle_estimated_travel_cm;
    WheelDistance_Update(5000, 0, 0);
    DistanceDrive_Update(5000);
    assert(drive_motor_pwm == 0);
    assert(vehicle_estimated_travel_cm == stopped_distance);

    Start(10.0f, 6000);
    assert(drive_status == DRIVE_RUNNING);
    assert(vehicle_estimated_travel_cm == 0.0f);
    drive_stop_request = 1;
    drive_start_request = 1;
    DistanceDrive_Update(6001);
    assert(drive_status == DRIVE_CANCELLED);
    assert(drive_motor_pwm == 0);
    assert(drive_start_request == 0 && drive_stop_request == 0);

    const float invalid[] = {0.0f, -1.0f, NAN, INFINITY, 3.4e38f};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        Start(invalid[i], 7000);
        assert(drive_status == DRIVE_INVALID_DISTANCE);
        assert(drive_motor_pwm == 0);
    }
    /* Timestamp wrap and automatic stop without another start request. */
    Start(31.4159265f, UINT32_MAX - 499U);
    assert(drive_status == DRIVE_RUNNING);
    DistanceDrive_Update(499);
    assert(drive_status == DRIVE_RUNNING);
    DistanceDrive_Update(501);
    assert(drive_status == DRIVE_COMPLETE);
    assert(drive_motor_pwm == 0);
    Start(1.0e-30f, 8000);
    assert(drive_planned_duration_ms == 1);
    DistanceDrive_Update(8001);
    assert(drive_status == DRIVE_COMPLETE);
#endif
    /* No encoder or preconfigured RPM is needed for a timed calibration run. */
    wheel_a_rpm_at_70 = 0.0f;
    wheel_b_rpm_at_70 = 0.0f;
    WheelDistance_Reset(9000);
    drive_calibration_request = 1;
    DistanceDrive_Update(9000);
    assert(drive_status == DRIVE_CALIBRATING && drive_motor_pwm == 70);
    Start(100.0f, 9100);
    assert(drive_status == DRIVE_CALIBRATING);
    drive_calibration_apply_request = 1;
    DistanceDrive_Update(10999);
    assert(drive_status == DRIVE_CALIBRATING && drive_motor_pwm == 70);
    DistanceDrive_Update(11000);
    assert(drive_status == DRIVE_CALIBRATION_READY && drive_motor_pwm == 0);
    assert(drive_calibration_elapsed_ms == 2000);
    drive_calibration_measured_cm = NAN;
    drive_calibration_apply_request = 1;
    DistanceDrive_Update(11001);
    assert(drive_status == DRIVE_INVALID_DISTANCE && !distance_calibrated);
    drive_calibration_measured_cm = 40.0f;
    drive_calibration_apply_request = 1;
    DistanceDrive_Update(11002);
    assert(drive_status == DRIVE_IDLE && distance_calibrated);
    assert(distance_rpm_from_measurement == 1);
    assert(fabsf(WheelDistance_VehicleSpeedCmPerSecond(70) - 20.0f) < 0.001f);
    Start(50.0f, 12000);
    assert(drive_status == DRIVE_RUNNING);
    assert(drive_planned_duration_ms >= 2500 && drive_planned_duration_ms <= 2501);
    DistanceDrive_Update(14501);
    assert(drive_status == DRIVE_COMPLETE && drive_motor_pwm == 0);
    drive_calibration_request = 1;
    DistanceDrive_Update(15000);
    drive_stop_request = 1;
    DistanceDrive_Update(15100);
    assert(drive_status == DRIVE_CANCELLED && drive_motor_pwm == 0);
    drive_calibration_apply_request = 1;
    DistanceDrive_Update(15101);
    assert(drive_status == DRIVE_INVALID_DISTANCE);
    puts("distance drive tests passed");
    return 0;
}
