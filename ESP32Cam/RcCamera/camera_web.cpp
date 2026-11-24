#include "camera_web.h"

#include <Arduino.h>
#include <esp_http_server.h>

#include <cstdio>

#include "camera_settings.h"

namespace {

constexpr char kStreamType[] = "multipart/x-mixed-replace; boundary=rc-cam-frame";
constexpr char kFrameBoundary[] = "\r\n--rc-cam-frame\r\n";

constexpr char kPage[] = R"HTML(<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>RC-CAM</title>
  <style>
    body { margin: 0; background: #101419; color: #f1f4f6; font: 1rem system-ui; text-align: center; }
    img {
      display: block;
      width: 100%;
      height: calc(100vh - 4rem);
      height: calc(100dvh - 4rem);
      object-fit: contain;
    }
    button { margin: .5rem; padding: .65rem 1rem; font: inherit; }
  </style>
</head>
<body>
  <img id="video" alt="RC카 전방 영상">
  <button id="reconnect" type="button">영상 다시 연결</button>
  <p id="error" role="status" hidden>영상에 연결할 수 없습니다.</p>
  <script>
    const video = document.querySelector('#video');
    const error = document.querySelector('#error');
    const stream = new URL('/stream', location.href);
    stream.port = '81';

    function connect() {
      error.hidden = true;
      stream.searchParams.set('t', Date.now());
      video.src = stream.href;
    }

    video.addEventListener('error', () => { error.hidden = false; });
    document.querySelector('#reconnect').addEventListener('click', connect);
    connect();
  </script>
</body>
</html>)HTML";

esp_err_t servePage(httpd_req_t *request) {
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, kPage);
}

esp_err_t sendFrame(httpd_req_t *request, const camera_fb_t &frame) {
    char header[80];
    std::snprintf(header, sizeof(header), "Content-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n",
                  frame.len);

    esp_err_t result = httpd_resp_sendstr_chunk(request, kFrameBoundary);
    if (result == ESP_OK) {
        result = httpd_resp_sendstr_chunk(request, header);
    }
    if (result == ESP_OK) {
        result = httpd_resp_send_chunk(request, reinterpret_cast<const char *>(frame.buf), frame.len);
    }
    return result;
}

esp_err_t serveStream(httpd_req_t *request) {
    httpd_resp_set_type(request, kStreamType);
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");

    for (;;) {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame == nullptr) {
            Serial.println("Camera frame capture failed");
            return ESP_FAIL;
        }

        const esp_err_t result = sendFrame(request, *frame);
        esp_camera_fb_return(frame);
        if (result != ESP_OK) {
            return result;
        }

        delay(kFrameDelayMs);
    }
}

esp_err_t startServer(httpd_handle_t &server, const httpd_config_t &config,
                      const char *path, esp_err_t (*handler)(httpd_req_t *)) {
    const esp_err_t startResult = httpd_start(&server, &config);
    if (startResult != ESP_OK) {
        return startResult;
    }

    httpd_uri_t route{};
    route.uri = path;
    route.method = HTTP_GET;
    route.handler = handler;
    const esp_err_t registerResult = httpd_register_uri_handler(server, &route);
    if (registerResult != ESP_OK) {
        httpd_stop(server);
    }
    return registerResult;
}

}  // namespace

esp_err_t startCameraWebServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t pageServer = nullptr;
    const esp_err_t pageResult = startServer(pageServer, config, "/", servePage);
    if (pageResult != ESP_OK) {
        return pageResult;
    }

    // Keep the page reachable while the stream handler is sending frames.
    config.server_port = 81;
    config.ctrl_port += 1;
    config.send_wait_timeout = 2;
    httpd_handle_t streamServer = nullptr;
    const esp_err_t streamResult = startServer(streamServer, config, "/stream", serveStream);
    if (streamResult != ESP_OK) {
        httpd_stop(pageServer);
    }
    return streamResult;
}
