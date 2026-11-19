#include "distance_drive.h"
#include "wheel_distance.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    WheelDistance_Reset(0);
    assert(distance_calibrated && !distance_rpm_from_measurement);
    assert(fabsf(wheel_a_rpm_at_70 - 155.55556f) < 0.001f);
    assert(fabsf(wheel_b_rpm_at_70 - 155.55556f) < 0.001f);
    /* 6.2 cm wheel diameter, estimated speed at PWM 70 = 50.49885 cm/s. */
    assert(fabsf(WheelDistance_VehicleSpeedCmPerSecond(70) - 50.49885f) < 0.001f);
    drive_target_cm = 50.0f;
    drive_start_request = 1;
    DistanceDrive_Update(0);
    assert(drive_status == DRIVE_RUNNING && drive_motor_pwm == 70);
    assert(drive_planned_duration_ms == 991);
    for (uint32_t ms = 1; ms <= 991; ++ms)
    {
        WheelDistance_Update(ms, drive_motor_pwm, drive_motor_pwm);
        DistanceDrive_Update(ms);
    }
    assert(drive_status == DRIVE_COMPLETE && drive_motor_pwm == 0);
    assert(fabsf(vehicle_estimated_travel_cm - 50.0f) < 0.05f);
    /* A real travel measurement replaces the initial driver/motor model. */
    assert(WheelDistance_CalibrateTravel(40.0f, 2000));
    assert(distance_rpm_from_measurement);
    assert(fabsf(WheelDistance_VehicleSpeedCmPerSecond(70) - 20.0f) < 0.001f);
    WheelDistance_Reset(1000);
    assert(distance_rpm_from_measurement);
    puts("motor estimate tests passed");
    return 0;
}
