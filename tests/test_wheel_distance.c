#include "wheel_distance.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void Near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.001f);
}

int main(void)
{
    const float circumference = 3.14159265358979323846f * 10.0f;
#ifdef TEST_UNCALIBRATED
    wheel_a_rpm_at_70 = 0.0f;
    wheel_b_rpm_at_70 = 0.0f;
#endif
    WheelDistance_Reset(0);
#ifdef TEST_UNCALIBRATED
    assert(!distance_calibrated);
    WheelDistance_Update(60000, 70, 70);
    Near(wheel_a_estimated_distance_cm, 0.0f);
    Near(wheel_b_estimated_distance_cm, 0.0f);
    Near(vehicle_estimated_travel_cm, 0.0f);
#else
    assert(distance_calibrated);
    /* 10 cm diameter, 60 wheel RPM, 1 second = one circumference. */
    WheelDistance_Update(1000, 70, 70);
    Near(wheel_a_estimated_distance_cm, circumference);
    Near(vehicle_estimated_travel_cm, circumference);
    WheelDistance_Update(2000, 0, 0);
    Near(vehicle_estimated_travel_cm, circumference);
    WheelDistance_Update(3000, -70, -70);
    Near(vehicle_estimated_displacement_cm, 0.0f);
    Near(vehicle_estimated_travel_cm, 2.0f * circumference);
    /* A wheel pivot moves the axle center half the wheel distance. */
    WheelDistance_Reset(0);
    WheelDistance_Update(1000, 70, 0);
    Near(vehicle_estimated_travel_cm, circumference * 0.5f);
    WheelDistance_Reset(0);
    WheelDistance_Update(1000, 70, -70);
    Near(vehicle_estimated_travel_cm, 0.0f);
    /* Multiple updates must agree with one update. */
    WheelDistance_Reset(0);
    for (uint32_t t = 10; t <= 1000; t += 10)
        WheelDistance_Update(t, 70, 70);
    Near(vehicle_estimated_travel_cm, circumference);
    WheelDistance_Reset(UINT32_MAX - 499U);
    WheelDistance_Update(500, 70, 70);
    Near(vehicle_estimated_travel_cm, circumference);
    WheelDistance_Reset(0);
    WheelDistance_Update(1000, 200, 200);
    Near(vehicle_estimated_travel_cm, circumference * (100.0f / 70.0f));
#endif
    (void)circumference;
    puts("wheel distance tests passed");
    return 0;
}
