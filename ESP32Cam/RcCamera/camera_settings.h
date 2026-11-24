#pragma once

#include <esp_camera.h>

constexpr char kAccessPointSsid[] = "RC-CAM";
constexpr char kAccessPointPassword[] = "RCcar2026!";
static_assert(sizeof(kAccessPointPassword) >= 9 && sizeof(kAccessPointPassword) <= 64,
              "Use a Wi-Fi password of 8 to 63 characters");

constexpr framesize_t kFrameSize = FRAMESIZE_QVGA;
constexpr int kJpegQuality = 12;  // Lower values give better quality and larger frames.
constexpr unsigned long kFrameDelayMs = 30;
constexpr bool kFlipVertical = false;
constexpr bool kMirrorHorizontal = false;
