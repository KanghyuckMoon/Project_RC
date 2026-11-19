# HC-SR04 전방 벽 감지

기본 배선은 **미확인 가정**입니다. 실제 배선에 맞춰 `hc_sr04.h`의 `HCSR04_TRIG_*`, `HCSR04_ECHO_*` 매크로를 수정하세요.
ECHO 핀 번호를 바꿀 때 IRQn과 IRQ_HANDLER도 해당 EXTI 벡터에 맞춰 변경합니다.

| HC-SR04 | STM32F411 |
| --- | --- |
| VCC | 5V |
| GND | STM32/모터 드라이버와 공통 GND |
| TRIG | PB0 |
| ECHO | 전압 분배 후 PB1 |

ECHO는 5V 신호입니다. 예: ECHO → 2.2kΩ → PB1 → 3.3kΩ → GND로
약 3.0V로 낮춥니다. TRIG는 STM32 3.3V TTL 출력이며, 모듈 변종에서
인식이 안 되면 3.3V→5V 논리 변환기를 사용하세요.

- TIM5를 1MHz 자유 실행 카운터로 사용하고 EXTI1 양쪽 에지로 ECHO 폭을 측정합니다.
- 12µs 트리거, 70ms 측정 주기, 30ms 응답 제한, `폭(µs) / 58`로 cm 환산.
- 2~400cm 이외, 응답 없음, ECHO 고정 HIGH, 250ms 초과 오래된 값은 무효입니다.
- `HCSR04_Read(&distance_cm)`은 유효 여부와 거리를 반환합니다.
- 응답을 기다리며 메인 루프를 막지 않습니다. 트리거를 만드는 12µs만 대기합니다.
- 이 드라이버는 추후 자율 주행 FSM에 연동할 예정입니다.

초음파 센서 하나가 바라보는 전방 범위만 감지합니다. 사각지대, 옆/뒤 장애물,
흡음 재질 감지는 보장하지 않습니다. 실제 정지 거리에 맞춰 임계값과 속도를 조정하세요.

근거: [ELECFreaks HC-SR04 데이터시트](https://content.arduino.cc/assets/HCSR04.pdf).
