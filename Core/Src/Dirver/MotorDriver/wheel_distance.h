#ifndef WHEEL_DISTANCE_H
#define WHEEL_DISTANCE_H

#include <stdint.h>

/* GEA067: seller lists approx. 200 output RPM at 9 V.
 * Driver assumed to be L298N (user label: L289N). The 2 V bridge drop is
 * an adjustable assumption, not a measured or guaranteed device voltage drop.
 * Linear voltage/PWM scaling is an initial estimate, not motor feedback.
 * Product: https://www.antoniadis.com.cy/en/educational-products/design-and-technology/motors-and-accesories/gear-boxes/dc-gear-motor-gearbox-6-9v-148-single-shaft
 * Driver: https://www.st.com/resource/en/datasheet/l298.pdf */
#ifndef MOTOR_SUPPLY_VOLTAGE_V
#define MOTOR_SUPPLY_VOLTAGE_V 12.0f
#endif
#ifndef MOTOR_DRIVER_DROP_V
#define MOTOR_DRIVER_DROP_V 2.0f
#endif
#define MOTOR_REFERENCE_VOLTAGE_V 9.0f
#define MOTOR_REFERENCE_OUTPUT_RPM 200.0f
#define MOTOR_ESTIMATED_RPM_AT_70 \
    (MOTOR_REFERENCE_OUTPUT_RPM * \
     ((MOTOR_SUPPLY_VOLTAGE_V - MOTOR_DRIVER_DROP_V) / MOTOR_REFERENCE_VOLTAGE_V) * 0.70f)

/* Override RPMs with measured values at PWM 70 if available.
 * Zero disables that wheel's distance calculation. */
#ifndef WHEEL_A_DIAMETER_CM
#define WHEEL_A_DIAMETER_CM 6.2f
#endif
#ifndef WHEEL_B_DIAMETER_CM
#define WHEEL_B_DIAMETER_CM 6.2f
#endif
#ifndef WHEEL_A_RPM_AT_70
#define WHEEL_A_RPM_AT_70 MOTOR_ESTIMATED_RPM_AT_70
#endif
#ifndef WHEEL_B_RPM_AT_70
#define WHEEL_B_RPM_AT_70 MOTOR_ESTIMATED_RPM_AT_70
#endif

/* Watch these variables in the debugger. Values are estimates without encoders.
 * A/B must be the two independently driven wheels of the same axle. */
extern volatile float wheel_a_estimated_distance_cm;
extern volatile float wheel_b_estimated_distance_cm;
extern volatile float vehicle_estimated_displacement_cm;
extern volatile float vehicle_estimated_travel_cm;
extern volatile uint8_t distance_calibrated;
/* 1 only after applying a measured-distance calibration this session.
 * distance_calibrated indicates usable parameters, including initial estimates. */
extern volatile uint8_t distance_rpm_from_measurement;
extern volatile float wheel_a_rpm_at_70;
extern volatile float wheel_b_rpm_at_70;
/* Effective wheel RPM inferred from a straight timed run, not sensor readings. */
int WheelDistance_CalibrateTravel(float measured_cm, uint32_t elapsed_ms);

void WheelDistance_Reset(uint32_t now_ms);
void WheelDistance_Update(uint32_t now_ms, int speed_a, int speed_b);
float WheelDistance_VehicleSpeedCmPerSecond(int speed);

#endif
