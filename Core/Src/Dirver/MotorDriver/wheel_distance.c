#include "wheel_distance.h"
#include <math.h>

#define PI_F 3.14159265358979323846f
#define REFERENCE_PWM 70.0f

volatile float wheel_a_estimated_distance_cm;
volatile float wheel_b_estimated_distance_cm;
volatile float vehicle_estimated_displacement_cm;
volatile float vehicle_estimated_travel_cm;
volatile uint8_t distance_calibrated;
volatile uint8_t distance_rpm_from_measurement;
volatile float wheel_a_rpm_at_70 = WHEEL_A_RPM_AT_70;
volatile float wheel_b_rpm_at_70 = WHEEL_B_RPM_AT_70;

static uint32_t last_update_ms;

static int ValidWheel(float diameter, float rpm)
{
    return isfinite(diameter) && diameter > 0.0f &&
           isfinite(rpm) && rpm > 0.0f;
}

static float WheelStep(float diameter, float rpm, int speed, uint32_t elapsed_ms)
{
    if (!ValidWheel(diameter, rpm)) return 0.0f;
    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;

    /* PWM is not RPM. Reference RPM may be estimated or measured.
     * Distance = circumference * RPM * minutes, with signed wheel speed. */
    return PI_F * diameter * rpm * ((float)speed / REFERENCE_PWM) *
           ((float)elapsed_ms / 60000.0f);
}

void WheelDistance_Reset(uint32_t now_ms)
{
    last_update_ms = now_ms;
    wheel_a_estimated_distance_cm = 0.0f;
    wheel_b_estimated_distance_cm = 0.0f;
    vehicle_estimated_displacement_cm = 0.0f;
    vehicle_estimated_travel_cm = 0.0f;
    distance_calibrated = ValidWheel(WHEEL_A_DIAMETER_CM, wheel_a_rpm_at_70) &&
                          ValidWheel(WHEEL_B_DIAMETER_CM, wheel_b_rpm_at_70);
}

float WheelDistance_VehicleSpeedCmPerSecond(int speed)
{
    if (!ValidWheel(WHEEL_A_DIAMETER_CM, wheel_a_rpm_at_70) ||
        !ValidWheel(WHEEL_B_DIAMETER_CM, wheel_b_rpm_at_70)) return 0.0f;

    return (WheelStep(WHEEL_A_DIAMETER_CM, wheel_a_rpm_at_70, speed, 1000) +
            WheelStep(WHEEL_B_DIAMETER_CM, wheel_b_rpm_at_70, speed, 1000)) * 0.5f;
}

void WheelDistance_Update(uint32_t now_ms, int speed_a, int speed_b)
{
    /* Unsigned subtraction also handles the HAL tick rollover. */
    uint32_t elapsed_ms = now_ms - last_update_ms;
    last_update_ms = now_ms;
    float step_a = WheelStep(WHEEL_A_DIAMETER_CM, wheel_a_rpm_at_70,
                            speed_a, elapsed_ms);
    float step_b = WheelStep(WHEEL_B_DIAMETER_CM, wheel_b_rpm_at_70,
                            speed_b, elapsed_ms);
    wheel_a_estimated_distance_cm += step_a;
    wheel_b_estimated_distance_cm += step_b;

    if (distance_calibrated)
    {
        float center_step = (step_a + step_b) * 0.5f;
        vehicle_estimated_displacement_cm += center_step;
        vehicle_estimated_travel_cm += fabsf(center_step);
    }
}

int WheelDistance_CalibrateTravel(float measured_cm, uint32_t elapsed_ms)
{
    if (!isfinite(measured_cm) || measured_cm <= 0.0f || elapsed_ms == 0 ||
        !isfinite(WHEEL_A_DIAMETER_CM) || WHEEL_A_DIAMETER_CM <= 0.0f ||
        !isfinite(WHEEL_B_DIAMETER_CM) || WHEEL_B_DIAMETER_CM <= 0.0f) return 0;
    float rpm_a = (measured_cm / (PI_F * WHEEL_A_DIAMETER_CM)) *
                  (60000.0f / (float)elapsed_ms);
    float rpm_b = (measured_cm / (PI_F * WHEEL_B_DIAMETER_CM)) *
                  (60000.0f / (float)elapsed_ms);
    if (!ValidWheel(WHEEL_A_DIAMETER_CM, rpm_a) ||
        !ValidWheel(WHEEL_B_DIAMETER_CM, rpm_b)) return 0;
    wheel_a_rpm_at_70 = rpm_a;
    wheel_b_rpm_at_70 = rpm_b;
    distance_calibrated = 1;
    distance_rpm_from_measurement = 1;
    return 1;
}
