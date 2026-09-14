// USB-only hardware reference capture. See ../README.md for protocol and limits.
#include <Arduino.h>
#include "esp_camera.h"
#include "esp_app_desc.h"
#include "cJSON.h"

static sensor_t *sensor;
static esp_err_t camera_error;
static const int discard_count = 8;

static void send_json(cJSON *j) {
  char *text = cJSON_PrintUnformatted(j);
  Serial.println(text);
  cJSON_free(text);
  cJSON_Delete(j);
}
static void error(const char *message) {
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "type", "error");
  cJSON_AddStringToObject(j, "message", message);
  send_json(j);
}
static void info() {
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "type", "info");
  cJSON_AddNumberToObject(j, "protocol", 1);
  cJSON_AddNumberToObject(j, "camera_error", camera_error);
  cJSON_AddStringToObject(j, "idf", esp_get_idf_version());
  char sha[65];
  const uint8_t *digest = esp_app_get_description()->app_elf_sha256;
  for (int i = 0; i < 32; ++i) snprintf(sha + 2*i, 3, "%02x", digest[i]);
  cJSON_AddStringToObject(j, "app_elf_sha256", sha);
  if (sensor) {
    cJSON_AddNumberToObject(j, "sensor_pid", sensor->id.PID);
    cJSON_AddNumberToObject(j, "sensor_ver", sensor->id.VER);
  }
  cJSON_AddNumberToObject(j, "configured_xclk_hz", 20000000);
  cJSON_AddStringToObject(j, "pixel_format", "JPEG");
  cJSON_AddNumberToObject(j, "jpeg_quality", 10);
  cJSON_AddNumberToObject(j, "frame_buffers", 1);
  send_json(j);
}
static void capture(int aec, int gain, int count) {
  if (!sensor || camera_error != ESP_OK) { error("camera initialization failed"); return; }
  if (sensor->id.PID != OV2640_PID) { error("manual protocol requires OV2640"); return; }
  if (aec < 0 || aec > 1200 || gain < 0 || gain > 30 || count < 1 || count > 300) {
    error("settings outside supported bounds"); return;
  }
  // Check each setter; the status cache alone does not prove successful I2C writes.
  if (sensor->set_exposure_ctrl(sensor, 0) || sensor->set_aec2(sensor, 0) ||
      sensor->set_gain_ctrl(sensor, 0) || sensor->set_whitebal(sensor, 0) ||
      sensor->set_awb_gain(sensor, 0) || sensor->set_aec_value(sensor, aec) ||
      sensor->set_agc_gain(sensor, gain)) { error("sensor setting failed"); return; }
  for (int i = 0; i < discard_count; ++i) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { error("discard capture failed"); return; }
    esp_camera_fb_return(fb);
  }
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "type", "settings");
  cJSON_AddNumberToObject(j, "requested_aec", aec);
  cJSON_AddNumberToObject(j, "requested_gain_index", gain);
  cJSON_AddNumberToObject(j, "discarded_frames", discard_count);
  cJSON *cache = cJSON_AddObjectToObject(j, "driver_status_cache");
#define STATUS(field) cJSON_AddNumberToObject(cache, #field, sensor->status.field)
  STATUS(quality); STATUS(brightness); STATUS(contrast); STATUS(saturation);
  STATUS(sharpness); STATUS(denoise); STATUS(special_effect); STATUS(wb_mode);
  STATUS(awb); STATUS(awb_gain); STATUS(aec); STATUS(aec2); STATUS(ae_level);
  STATUS(aec_value); STATUS(agc); STATUS(agc_gain); STATUS(gainceiling);
  STATUS(bpc); STATUS(wpc); STATUS(raw_gma); STATUS(lenc); STATUS(hmirror);
  STATUS(vflip); STATUS(dcw); STATUS(colorbar);
#undef STATUS
  // OV2640 get_reg uses the high byte as bank and low byte as address.
  // Snapshot timing/exposure/control registers, not DSP indirect table ports.
  const int addresses[] = {0x100,0x104,0x10a,0x10b,0x10c,0x10d,0x10e,0x10f,
    0x110,0x111,0x112,0x113,0x114,0x115,0x117,0x118,0x119,0x11a,
    0x12a,0x12b,0x12d,0x12e,0x145,0x146,0x147,0x15a,
    0x050,0x051,0x052,0x053,0x054,0x055,0x057,0x05a,0x05b,0x05c,
    0x086,0x087,0x0c0,0x0c1,0x0c2,0x0c3,0x0c7,0x0d3,0x0da,0x0e0};
  cJSON *regs = cJSON_AddObjectToObject(j, "registers_bank_address_hex");
  bool read_failed = false;
  for (unsigned i = 0; i < sizeof(addresses)/sizeof(addresses[0]); ++i) {
    char key[5]; snprintf(key, sizeof(key), "%03x", addresses[i]);
    int value = sensor->get_reg(sensor, addresses[i], 0xff);
    if (value < 0) read_failed = true;
    cJSON_AddNumberToObject(regs, key, value);
  }
  send_json(j);
  if (read_failed) { error("register read failed"); return; }
  for (int i = 0; i < count; ++i) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { error("capture failed"); return; }
    j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "type", "frame");
    cJSON_AddNumberToObject(j, "sequence", i);
    cJSON_AddNumberToObject(j, "bytes", fb->len);
    cJSON_AddNumberToObject(j, "width", fb->width);
    cJSON_AddNumberToObject(j, "height", fb->height);
    cJSON_AddNumberToObject(j, "dma_timestamp_us", (double)fb->timestamp.tv_sec * 1000000 + fb->timestamp.tv_usec);
    send_json(j);
    size_t sent = 0;
    while (sent < fb->len && Serial) {
      size_t n = Serial.write(fb->buf + sent, fb->len - sent);
      if (!n) break;
      sent += n;
    }
    bool complete = sent == fb->len;
    esp_camera_fb_return(fb);
    if (!complete) return;
  }
  Serial.println("{\"type\":\"done\"}");
}
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1000);
  camera_config_t c = {};
  c.pin_pwdn = -1; c.pin_reset = -1; c.pin_xclk = 10;
  c.pin_sccb_sda = 40; c.pin_sccb_scl = 39;
  c.pin_d7 = 48; c.pin_d6 = 11; c.pin_d5 = 12; c.pin_d4 = 14;
  c.pin_d3 = 16; c.pin_d2 = 18; c.pin_d1 = 17; c.pin_d0 = 15;
  c.pin_vsync = 38; c.pin_href = 47; c.pin_pclk = 13;
  c.xclk_freq_hz = 20000000; c.ledc_timer = LEDC_TIMER_0; c.ledc_channel = LEDC_CHANNEL_0;
  c.pixel_format = PIXFORMAT_JPEG; c.frame_size = FRAMESIZE_SVGA;
  c.jpeg_quality = 10; c.fb_count = 1; c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  camera_error = esp_camera_init(&c);
  if (camera_error == ESP_OK) sensor = esp_camera_sensor_get();
}
void loop() {
  if (!Serial.available()) { delay(10); return; }
  String command = Serial.readStringUntil('\n'); command.trim();
  if (command == "INFO") { info(); return; }
  int aec, gain, count; char extra;
  if (sscanf(command.c_str(), "CAP %d %d %d %c", &aec, &gain, &count, &extra) == 3) {
    capture(aec, gain, count);
  } else error("expected INFO or CAP aec gain_index count");
}
