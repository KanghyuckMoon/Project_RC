# 장치별 코드 구조

- `Ultrasonic/hc_sr04.c/.h`: HC-SR04 3개 거리 측정 (TIM5, EXTI1, EXTI9_5)
- `Bluetooth/zs040.c/.h`: ZS-040 UART 수신 (USART1, PA9/PA10)
- `MotorDriver/motor_driver.c/.h`: PWM/방향 제어
- `MotorDriver/wheel_distance.c/.h`: 바퀴 이동 거리 추정과 보정
- `MotorDriver/distance_drive.c/.h`: 기존 목표 거리 주행 라이브러리
- `../Control/car_control.c/.h`: 수동 명령, 모드 전환, 벽 감지 정지 판단
- `../Control/car_app.c/.h`: 장치 초기화와 메인 루프 연결

각 모듈의 소스와 헤더를 같은 폴더에 둡니다. CMakeLists.txt에 소스/include를 등록했습니다.
초음파 GPIO·EXTI·TIM5, 모터 GPIO·TIM2, USART1 초기화는 CubeMX 생성 코드를 사용합니다.

`main.c`의 USER CODE 영역에서 `CarApp_Init/Update()`를 호출합니다.
현재 모터 출력은 CarApp이 소유합니다. 기존 `DistanceDrive_Update()` 기반 거리 주행은
메인 루프에서 호출하지 않으므로 해당 라이브러리의 디버거 요청은 모터에 적용되지 않습니다.
두 제어 루프가 동시에 모터를 조작하지 않도록 하세요.

CubeMX `.ioc`에 초음파 핀과 USART1의 PA9/PA10을 설정했습니다.
재생성 후 CMake의 사용자 소스와 USART1 IRQ의 USER CODE 보존을 확인하세요.

설정과 배선은 각 하위 폴더의 README를 참고하세요.

## 핀 매크로

초음파 핀은 CubeMX가 생성한 `main.h`의 `US_*`를 사용합니다.
블루투스와 모터 핀은 `zs040.h`, `motor_driver.h`에 있습니다.
모터 GPIO/PWM 초기화(`gpio.c`, `tim.c`)도 `motor_driver.h`의 매크로를 사용합니다.
PWM 핀/AF/채널은 TIM2에서 지원하는 조합으로 맞추세요.
CubeMX 재생성은 `gpio.c`/`tim.c`의 매크로 참조를 덮어쓸 수 있으므로 다시 확인해야 합니다.
