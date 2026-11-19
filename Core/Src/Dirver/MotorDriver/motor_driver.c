#include "motor_driver.h"
#include "main.h"
#include "tim.h"
#include "wheel_distance.h"

static int motor_a_speed;
static int motor_b_speed;

void MotorA(int speed)
{
    WheelDistance_Update(HAL_GetTick(), motor_a_speed, motor_b_speed);
    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;
    motor_a_speed = speed;

    if (speed > 0)
    {
        HAL_GPIO_WritePin(MOTOR_A_IN1_GPIO_PORT, MOTOR_A_IN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_A_IN2_GPIO_PORT, MOTOR_A_IN2_PIN, GPIO_PIN_RESET);
    }
    else if (speed < 0)
    {
        HAL_GPIO_WritePin(MOTOR_A_IN1_GPIO_PORT, MOTOR_A_IN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_A_IN2_GPIO_PORT, MOTOR_A_IN2_PIN, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(MOTOR_A_IN1_GPIO_PORT, MOTOR_A_IN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_A_IN2_GPIO_PORT, MOTOR_A_IN2_PIN, GPIO_PIN_RESET);
    }

    __HAL_TIM_SET_COMPARE(&htim2, MOTOR_A_PWM_CHANNEL,
                          speed < 0 ? -speed : speed);
}

void MotorB(int speed)
{
    WheelDistance_Update(HAL_GetTick(), motor_a_speed, motor_b_speed);
    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;
    motor_b_speed = speed;

    if (speed > 0)
    {
        HAL_GPIO_WritePin(MOTOR_B_IN1_GPIO_PORT, MOTOR_B_IN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_B_IN2_GPIO_PORT, MOTOR_B_IN2_PIN, GPIO_PIN_RESET);
    }
    else if (speed < 0)
    {
        HAL_GPIO_WritePin(MOTOR_B_IN1_GPIO_PORT, MOTOR_B_IN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_B_IN2_GPIO_PORT, MOTOR_B_IN2_PIN, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(MOTOR_B_IN1_GPIO_PORT, MOTOR_B_IN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_B_IN2_GPIO_PORT, MOTOR_B_IN2_PIN, GPIO_PIN_RESET);
    }

    __HAL_TIM_SET_COMPARE(&htim2, MOTOR_B_PWM_CHANNEL,
                          speed < 0 ? -speed : speed);
}

void MotorDriver_Init(void)
{
    HAL_TIM_PWM_Start(&htim2, MOTOR_A_PWM_CHANNEL);
    HAL_TIM_PWM_Start(&htim2, MOTOR_B_PWM_CHANNEL);
    WheelDistance_Reset(HAL_GetTick());
    MotorA(0);
    MotorB(0);
}

void MotorDriver_UpdateDistance(void)
{
    WheelDistance_Update(HAL_GetTick(), motor_a_speed, motor_b_speed);
}
