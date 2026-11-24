# RC카 ESP32-CAM

AI Thinker ESP32-CAM의 OV2640 카메라를 사용합니다. ESP32-CAM이 `RC-CAM` Wi-Fi를 열고 MoonCar Android 앱과 브라우저에 영상을 송출합니다. [MoonCar Android 앱](../MobileController/README.md)에서는 영상을 보면서 ZS-040 블루투스로 수동·자율주행을 제어합니다.

## 업로드

1. Arduino IDE 환경설정의 **추가 보드 매니저 URLs**에 `https://espressif.github.io/arduino-esp32/package_esp32_index.json`을 추가합니다. 보드 매니저에서 Espressif Systems의 **esp32** 4.0.0을 설치합니다. [Espressif 설치 문서](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
2. [RcCamera.ino](RcCamera/RcCamera.ino)를 열고 **AI Thinker ESP32-CAM** 보드를 선택합니다. ESP32-CAM-MB를 USB 데이터 케이블로 PC에 연결한 뒤 COM 포트를 선택하고 업로드합니다.
3. 업로드 후 시리얼 모니터를 `115200`으로 열고 보드를 리셋합니다. `Camera ready`, `Wi-Fi: RC-CAM`, `FPV: http://192.168.4.1`이 순서대로 표시됩니다.

업로드가 `Connecting…`에서 멈추고 MB 보드에 `IO0`와 `RST` 버튼이 있다면, `IO0`를 누른 상태에서 `RST`를 눌렀다 뗀 뒤 업로드합니다. 업로드 후에는 `IO0`에서 손을 뗀 상태로 리셋해 스케치를 실행합니다. [ESP32 부트 모드](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html)

Arduino CLI에서 빌드할 때는 저장소 루트에서 `arduino-cli compile --fqbn esp32:esp32:esp32cam ESP32Cam/RcCamera`를 실행합니다.

## 영상과 조종 확인

1. 카메라를 차량에 고정하고 ESP32-CAM-MB의 USB에 5V 보조배터리를 연결합니다.
2. 휴대폰을 `RC-CAM` Wi-Fi에 연결합니다. 시연용 기본 비밀번호는 `RCcar2026!`입니다. 인터넷 연결이 없다는 메시지가 표시되면 연결을 유지합니다. MoonCar 앱에서 **FPV 켜기**를 누르고 중앙 영상이 갱신되는지 확인합니다.
3. 앱에서 **블루투스 연결**을 누르고 차량에 연결합니다. 영상과 함께 수동 저속으로 전진·후진·회전을 확인한 뒤 **자율주행**, **정지**, **주행 재개**를 확인합니다.

카메라만 확인하려면 브라우저에서 [FPV 화면](http://192.168.4.1)을 엽니다. 외부 뷰어에서는 [MJPEG 주소](http://192.168.4.1:81/stream)를 사용합니다. 스트림은 한 번에 한 곳에서 엽니다.

## 설정

[camera_settings.h](RcCamera/camera_settings.h)에서 Wi-Fi 이름과 암호, JPEG 품질, 프레임 전송 후 대기 시간, 상하·좌우 방향을 조정할 수 있습니다. 기본 해상도는 QVGA(320×240)입니다. 영상 지연이 크면 `kJpegQuality`를 높여 JPEG 크기를 줄이거나 `kFrameDelayMs`를 조정합니다. 변경 후 보드에 다시 업로드합니다.
