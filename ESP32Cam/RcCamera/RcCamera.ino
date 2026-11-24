#include <WiFi.h>
#include <esp_camera.h>

#include "camera_settings.h"
#include "camera_web.h"

namespace {

esp_err_t startCamera() {
    const bool hasPsram = psramFound();
    camera_config_t config{};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

    // AI Thinker ESP32-CAM (OV2640) pinout.
    config.pin_pwdn = 32;
    config.pin_reset = -1;
    config.pin_xclk = 0;
    config.pin_sccb_sda = 26;
    config.pin_sccb_scl = 27;
    config.pin_d0 = 5;
    config.pin_d1 = 18;
    config.pin_d2 = 19;
    config.pin_d3 = 21;
    config.pin_d4 = 36;
    config.pin_d5 = 39;
    config.pin_d6 = 34;
    config.pin_d7 = 35;
    config.pin_vsync = 25;
    config.pin_href = 23;
    config.pin_pclk = 22;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = kFrameSize;
    config.jpeg_quality = kJpegQuality;
    config.fb_count = hasPsram ? 2 : 1;
    config.fb_location = hasPsram ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
    config.grab_mode = hasPsram ? CAMERA_GRAB_LATEST : CAMERA_GRAB_WHEN_EMPTY;

    const esp_err_t result = esp_camera_init(&config);
    if (result == ESP_OK) {
        sensor_t *sensor = esp_camera_sensor_get();
        sensor->set_vflip(sensor, kFlipVertical);
        sensor->set_hmirror(sensor, kMirrorHorizontal);
        Serial.printf("Camera ready (PSRAM: %s)\n", hasPsram ? "yes" : "no");
    }
    return result;
}

bool startAccessPoint() {
    const IPAddress address(192, 168, 4, 1);
    const IPAddress subnet(255, 255, 255, 0);
    return WiFi.mode(WIFI_AP) && WiFi.softAPConfig(address, address, subnet) &&
           WiFi.softAP(kAccessPointSsid, kAccessPointPassword);
}

}  // namespace

void setup() {
    Serial.begin(115200);

    const esp_err_t cameraResult = startCamera();
    if (cameraResult != ESP_OK) {
        Serial.printf("Camera init failed: %s (0x%x)\n", esp_err_to_name(cameraResult),
                      static_cast<unsigned>(cameraResult));
        return;
    }

    if (!startAccessPoint()) {
        Serial.println("Wi-Fi AP startup failed");
        WiFi.mode(WIFI_OFF);
        esp_camera_deinit();
        return;
    }

    const esp_err_t serverResult = startCameraWebServer();
    if (serverResult != ESP_OK) {
        Serial.printf("Camera web server startup failed: %s\n", esp_err_to_name(serverResult));
        WiFi.mode(WIFI_OFF);
        esp_camera_deinit();
        return;
    }

    Serial.printf("Wi-Fi: %s\n", kAccessPointSsid);
    Serial.print("FPV: http://");
    Serial.println(WiFi.softAPIP());
}

void loop() {
    delay(1000);
}
