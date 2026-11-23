# RC 자동차 펌웨어

현재 메인 루프는 **블루투스 수동 조종과 명령 기반 자율 주행 FSM**을 실행합니다.
HC-SR04 3개는 전방 PB0/PB1, 좌측 PC4/PC5, 우측 PC6/PC7에 배정했습니다.
ZS-040은 USART1 PA9/PA10을 사용합니다. 실제 배선과 앱 설정은 현장에서 확인해야 합니다.
부팅 시 수동 정지 상태로 시작합니다.

- [HC-SR04 배선과 측정](Core/Src/Dirver/Ultrasonic/README.md)
- [ZS-040 배선과 앱 명령 설정](Core/Src/Dirver/Bluetooth/README.md)
- `A`는 수동에서 자율주행으로 전환하고 새 센서 측정값을 받은 뒤 자동으로 출발합니다. `S`는 정지하고, 자율주행에서 `F`로 재개합니다. `M`은 정지 후 수동으로 전환합니다.
- 수동 모드의 방향·속도 조작과 500ms 명령 타임아웃은 이전과 같습니다. 자율 모드에선 `F` 이외의 방향 명령과 속도 명령을 무시합니다. 자율 주행 중에는 명령 반복 전송이 필요 없습니다.
- 새로 출발할 때 전방이 50cm 이상이고 양옆이 12cm보다 멀거나 반사파가 없으면 전진합니다. 한쪽 벽이 30cm 안에 있고 반대쪽에 여유가 있으면 두 바퀴의 PWM을 다르게 줘 벽에서 멀어집니다.
- 주행 중 전방 45cm 이하 또는 어느 옆이 12cm 이하이면 멈춘 뒤 다시 측정합니다. 공간이 더 넓은 쪽에 30cm 이상이 확인되면 최대 180ms 제자리 회전합니다. 회전이 막히고 전방과 양옆의 거리가 확인되면 최대 120ms 후진한 다음 정지하고 다시 측정합니다. 뒤쪽 센서가 없어 후진은 한 번의 회피 시도에서 1회로 제한합니다.
- 기본 직진·회전·후진 PWM은 80%입니다. 기본 코너링은 안쪽 71%, 바깥쪽 90%입니다. `CAR_AUTO_PWM`과 좌우 배선 설정인 `CAR_MOTOR_A_IS_RIGHT`는 `car_control.h`, 거리 기준과 회전 시간은 `car_control.c`에서 조정합니다. 기본 자율 조향은 MoonCar 수동 조향에 맞춰 A를 오른쪽, B를 왼쪽으로 사용합니다.
- MoonCar 화면에서 전방·좌측·우측 센서의 거리와 응답 상태, 모터 A/B PWM, 전진·코너링·회전·후진 상태를 확인할 수 있습니다.

아래 거리 보정/목표 거리 주행 설명은 기존 라이브러리용입니다. **현재 CarApp에서는
거리 주행 요청을 실행하지 않습니다.** 해당 기능을 사용하려면 모터 제어 소유권을
전환하는 통합 작업이 필요합니다.

# 바퀴 이동 거리(cm) 추정

바퀴 지름은 양쪽 모두 **6.2 cm**로 설정되어 있습니다(둘레 약 19.478 cm).
현재 기본 RPM은 **12V 전원, L298N으로 가정한 드라이버, PWM 70** 조건의
추정값을 사용합니다. 사용자 표기는 L289N이며, 실제 부품 식별은 확인되지 않았습니다.

- GEA067 기준: 9V에서 출력축 약 200 RPM.
- 드라이버 전압 강하: **2V로 가정** (`MOTOR_DRIVER_DROP_V`). 실측값이 아닙니다.
- PWM 100 추정 RPM: `200 × (12 - 2) / 9 ≈ 222.22`.
- PWM 70 추정 RPM: `222.22 × 0.70 ≈ 155.56`.
- 지름 6.2 cm에서 추정 속도: 약 **50.50 cm/s**.
- 목표 50 cm: 약 **991ms**, 목표 100 cm: 약 **1981ms** 구동.

따라서 보정 없이도 목표 거리 테스트를 시작할 수 있습니다. 전압/PWM과 RPM을
선형으로 가정한 값이며, 실제 주행 속도나 12V 공식 사양을 뜻하지 않습니다.
드라이버 전압 강하는 전류에 따라 달라지고, 모터에 걸리는 PWM 펄스 전압이
정격 범위를 넘을 수 있으므로 공급 12V를 모터의 정격 전압으로 취급하면 안 됩니다.

설정은 `Core/Src/Dirver/MotorDriver/wheel_distance.h`의 `MOTOR_SUPPLY_VOLTAGE_V`,
`MOTOR_DRIVER_DROP_V`에서 조정합니다. GEA067의 기어 출력축 RPM을 사용하므로
감속비 48로 다시 나누지 않습니다. 실측 거리 보정을 적용하면 추정 RPM을 대체합니다.
리셋하면 이 기본 추정값으로 돌아갑니다.

출처: [GEA067 판매처 사양](https://www.antoniadis.com.cy/en/educational-products/design-and-technology/motors-and-accesories/gear-boxes/dc-gear-motor-gearbox-6-9v-148-single-shaft),
[L298 ST 데이터시트](https://www.st.com/resource/en/datasheet/l298.pdf).
2V는 데이터시트의 고정값이 아니라 이 코드에서 선택한 근삿값입니다.

실제 이동 거리에 맞추려면 아래 시험 주행 보정을 사용합니다.
모터 드라이버 모델명만으로 실제 바퀴 회전수나 이동 거리를 알 수는 없습니다.

## 엔코더 없이 실제 거리로 보정

펌웨어를 다운로드한 뒤 평평한 바닥에서 디버거 변수로 진행합니다.

1. 시작 위치를 표시하고 `drive_calibration_request = 1`로 설정합니다.
   양쪽 모터가 PWM 70으로 2초간 전진하고 자동 정지합니다.
2. `drive_status = 7`이면 완전히 정지한 위치까지 실제 이동 거리(cm)를
   줄자로 측정합니다. 차량의 같은 지점을 기준으로 측정합니다.
3. 측정값을 `drive_calibration_measured_cm`에 입력한 다음
   `drive_calibration_apply_request = 1`로 설정합니다.
   예를 들어 40 cm 이동했다면 `40.0`을 입력합니다.
4. 성공하면 `drive_status = 0`, `distance_calibrated = 1`이 됩니다.
   이제 `drive_target_cm = 50.0`, `drive_start_request = 1`로 50 cm 주행을 시험합니다.

시험 중 정지는 `drive_stop_request = 1`입니다. 중단한 시험은 보정에 사용하지 않습니다.
입력 거리가 잘못되어 상태 4가 되면 올바른 값으로 수정하고 적용 요청을 다시 줍니다.
주행 중 들어온 보정/목표 주행 시작·적용 요청은 무시합니다.

실제 구동 시간은 `drive_calibration_elapsed_ms`에 기록됩니다.
보정은 `속도 = 실측 거리 / 구동 시간`을 사용하며, 그 속도를 지름으로 환산한
유효 RPM을 `wheel_a_rpm_at_70`, `wheel_b_rpm_at_70`에 저장합니다.
이 값은 좌우 바퀴가 직진 시 같은 거리를 이동했다고 가정한 추정값입니다.
독립적으로 측정한 모터 RPM이 아닙니다.

보정값은 RAM에만 저장되므로 리셋·전원 재인가 시 다시 보정해야 합니다.
유지하려면 확인한 두 RPM 값을 `wheel_distance.h`의 `WHEEL_A_RPM_AT_70`,
`WHEEL_B_RPM_AT_70` 기본값에 입력해 다시 빌드합니다.
배터리·노면·적재 조건이 바뀌면 재보정합니다. 가속과 정지 뒤 관성 때문에
시험 거리와 크게 다른 목표에서는 오차가 커질 수 있습니다.

## RPM을 알고 있을 때 직접 설정

`Core/Src/Dirver/MotorDriver/wheel_distance.h`의 다음 값을 실측값으로 설정한 후 빌드합니다.

- `WHEEL_A_DIAMETER_CM`, `WHEEL_B_DIAMETER_CM`: 타이어를 포함한 바퀴 바깥 지름(cm).
- `WHEEL_A_RPM_AT_70`, `WHEEL_B_RPM_AT_70`: PWM 70으로 구동했을 때 **감속기 출력 바퀴**의 분당 회전수. 일정 시간 동안 회전수를 세어 `RPM = 회전수 × 60 / 측정시간(초)`로 구합니다.

프로그램이 지름을 자동으로 측정하지는 않습니다. 줄자로 지름을 재거나,
바깥 둘레를 측정해 `지름 = 둘레 / π`로 구해 입력해야 합니다.
RPM을 0으로 설정하면 해당 바퀴의 거리를 누적하지 않습니다. 기본값은 위 계산식의 추정 RPM입니다.

계산식은 `거리(cm) = π × 지름(cm) × RPM × 시간(ms) / 60000`입니다.
다른 PWM에서는 `현재 PWM / 70`에 비례하는 RPM을 가정합니다.
실제 RPM은 부하, 배터리, 정지 마찰 등에 따라 달라지므로 이것은 추정값입니다.
가속·감속, 멈춘 뒤 관성 이동, 바퀴 미끄러짐도 측정하지 못합니다.
실제 회전수를 측정하려면 엔코더와 배선·바퀴 1회전당 카운트 정보가 필요합니다.

디버거의 Watch 또는 Live Expressions에 다음 변수를 추가합니다.

| 변수 | 의미 |
| --- | --- |
| `distance_calibrated` | 두 바퀴의 지름과 RPM이 모두 유효하면 1 (기본 추정값 포함), 아니면 0 |
| `distance_rpm_from_measurement` | 이번 실행에서 실측 거리 보정을 적용했으면 1, 기본값 사용 중이면 0 |
| `wheel_a_estimated_distance_cm` | A 바퀴의 부호 있는 누적 이동 거리 |
| `wheel_b_estimated_distance_cm` | B 바퀴의 부호 있는 누적 이동 거리 |
| `vehicle_estimated_displacement_cm` | 차체 중심의 전진/후진 부호를 반영한 누적 거리 |
| `vehicle_estimated_travel_cm` | 후진도 양수로 더한 차체 중심의 총 이동 거리 |

차체 값은 A/B가 같은 차축의 좌우 구동 바퀴이고, 두 모터의 양수 명령이
모두 차량 전진 방향이라는 조건에서 매 구간 양쪽 거리의 평균으로 계산합니다.
직접 모터 배선 방향을 확인해야 합니다. 곡선 경로를 따라 누적한 값이며
시작점과 현재 위치 사이의 직선 거리가 아닙니다. 두 바퀴가 모두 보정되어야
차체 값을 누적합니다. 한쪽 바퀴만 돌면 중심 이동 거리는 그 바퀴 거리의 절반,
서로 반대로 같은 거리만큼 돌면 중심 이동 거리는 0입니다.

목표 거리 테스트에서는 A/B를 함께 구동하며, 주행 중 거리 값을 갱신합니다.
`WheelDistance_Reset(HAL_GetTick())`을 호출하면 누적값을 초기화합니다.
화면이나 UART 출력은 구성되어 있지 않아 디버거로 확인합니다.

예: 지름 10 cm, PWM 70에서 60 RPM인 바퀴를 1초 돌리면 약 31.416 cm입니다.

## 목표 거리만큼 주행 테스트

기본 설정(지름 6.2 cm, 추정 RPM 155.56)으로 빌드·다운로드하면 바로 테스트할 수 있습니다.
실제 거리와 오차가 크면 위의 시험 주행 보정을 적용합니다.
부팅 후에는 정지 상태로 기다립니다. UART가 구성되어 있지 않으므로
디버거의 변수 수정 기능으로 입력합니다. 디버거가 실행 중 쓰기를 지원하지 않으면
정지 상태에서 일시 중단한 뒤 값을 수정하고 실행을 재개합니다.

1. Watch에 `drive_target_cm`, `drive_start_request`, `drive_stop_request`,
   `drive_status`, `drive_planned_duration_ms`, `vehicle_estimated_travel_cm`을 추가합니다.
2. `drive_target_cm`에 원하는 양수 거리(cm)를 입력합니다. 예: `50.0`.
3. `drive_start_request`를 `1`로 바꿉니다. 요청은 자동으로 `0`으로 돌아갑니다.
4. 두 모터가 PWM 70으로 함께 전진하고 계산상 목표에 도달하면 정지합니다.
   완료 시 `drive_status`가 `2`가 됩니다. 반복하려면 거리 입력 후 시작 요청을 다시 `1`로 설정합니다.
5. 도중에 멈추려면 `drive_stop_request`를 `1`로 설정합니다.

| `drive_status` | 의미 |
| --- | --- |
| 0 | 대기 |
| 1 | 주행 중 |
| 2 | 완료 |
| 3 | 사용자 정지 |
| 4 | 잘못된 거리: 0 이하, 비정상 수치 또는 표현 가능한 주행 시간 초과 |
| 5 | 바퀴 지름 또는 RPM 미설정 |
| 6 | 2초 보정 주행 중 |
| 7 | 보정 주행 완료, 실측 거리 입력 대기 |

시작할 때 누적 거리를 초기화하고 목표를 `drive_active_target_cm`에 고정합니다.
주행 중 목표 값을 바꾸거나 시작을 다시 요청해도 진행 중인 목표는 바뀌지 않습니다.
계산된 시간(ms)은 `drive_planned_duration_ms`로 확인합니다.
`시간 = 목표 거리 / 두 바퀴 평균 선속도`이며, 1ms 단위로 올림합니다.
예를 들어 양쪽 지름 10 cm, RPM 60에서 50 cm의 계산 시간은 1592ms입니다.
이동 거리 또는 계산 시간에 도달하면 모터 PWM을 0으로 만듭니다.

엔코더 없이 계산한 추정 이동량에 따라 정지하므로 실제 cm 정확도는
RPM 보정과 노면·관성에 영향을 받습니다. 양쪽 PWM이 같아도 바퀴 선속도가
다르면 휘어질 수 있으며, 현재 기능은 조향 보정을 하지 않습니다.
실제 차량 주행과 정지 거리는 하드웨어에서 확인해야 합니다.

## 계산 검증 (호스트 GCC)

프로젝트 디렉터리에서 실행합니다.

```sh
gcc -std=c11 -Wall -Wextra -Werror -ICore/Inc -ICore/Src/Dirver/MotorDriver \
  -DWHEEL_A_DIAMETER_CM=10.0f -DWHEEL_B_DIAMETER_CM=10.0f \
  -DWHEEL_A_RPM_AT_70=60.0f -DWHEEL_B_RPM_AT_70=60.0f \
  Core/Src/Dirver/MotorDriver/wheel_distance.c tests/test_wheel_distance.c -lm -o /tmp/test_wheel_distance
/tmp/test_wheel_distance
gcc -std=c11 -Wall -Wextra -Werror -ICore/Inc -ICore/Src/Dirver/MotorDriver -DTEST_UNCALIBRATED \
  Core/Src/Dirver/MotorDriver/wheel_distance.c tests/test_wheel_distance.c -lm -o /tmp/test_wheel_distance_unset
/tmp/test_wheel_distance_unset
gcc -std=c11 -Wall -Wextra -Werror -ICore/Inc -ICore/Src/Dirver/MotorDriver \
  -DWHEEL_A_DIAMETER_CM=10.0f -DWHEEL_B_DIAMETER_CM=10.0f \
  -DWHEEL_A_RPM_AT_70=60.0f -DWHEEL_B_RPM_AT_70=60.0f \
  Core/Src/Dirver/MotorDriver/wheel_distance.c Core/Src/Dirver/MotorDriver/distance_drive.c tests/test_distance_drive.c \
  -lm -o /tmp/test_distance_drive
/tmp/test_distance_drive
gcc -std=c11 -Wall -Wextra -Werror -ICore/Inc -ICore/Src/Dirver/MotorDriver -DTEST_UNCALIBRATED \
  Core/Src/Dirver/MotorDriver/wheel_distance.c Core/Src/Dirver/MotorDriver/distance_drive.c tests/test_distance_drive.c \
  -lm -o /tmp/test_distance_drive_unset
/tmp/test_distance_drive_unset
gcc -std=c11 -Wall -Wextra -Werror -ICore/Inc -ICore/Src/Dirver/MotorDriver \
  Core/Src/Dirver/MotorDriver/wheel_distance.c Core/Src/Dirver/MotorDriver/distance_drive.c tests/test_motor_estimate.c \
  -lm -o /tmp/test_motor_estimate
/tmp/test_motor_estimate
```

## 장치별 소스 폴더

`Core/Src/Dirver/` 아래에 `Ultrasonic`, `Bluetooth`, `MotorDriver`로 정리했습니다.
HC-SR04와 ZS-040 드라이버를 구현했으며, 모터 관련 소스와 헤더는 `MotorDriver`에 있습니다.
자세한 구조는 [Dirver/README.md](Core/Src/Dirver/README.md)를 참고하세요.

## 조종 로직 검증

```sh
gcc -std=c11 -Wall -Wextra -Werror -ICore/Src/Control \
  Core/Src/Control/car_control.c tests/test_car_control.c -o /tmp/test_car_control
/tmp/test_car_control
gcc -std=c11 -Wall -Wextra -Werror -Itests/fakes \
  -ICore/Src/Dirver/Ultrasonic -ICore/Src/Dirver/Bluetooth \
  Core/Src/Dirver/Ultrasonic/hc_sr04.c Core/Src/Dirver/Bluetooth/zs040.c \
  tests/test_device_drivers.c -lm -o /tmp/test_device_drivers
/tmp/test_device_drivers
```

`cmake --preset Debug && cmake --build --preset Debug`로 펌웨어를 빌드합니다.
호스트 테스트는 수동 명령과 타임아웃, 장애물 감지와 좌우 회전, 회전 후 전진, 센서 에지와 UART 오류 처리를 검증합니다. 실제 UART 파형,
센서 에지 타이밍, 배선, 주행 및 정지 거리는 하드웨어에서 확인해야 합니다.
