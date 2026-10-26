// Radio.cpp  (Hazer-TX)
// Fire-and-forget ESP-NOW sends to the active fixture's receiver; status replies
// from the receiver provide heartbeat, version, RSSI, value echo and flags.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "Config.h"
#include "HazerProtocol.h"
#include "FixtureTable.h"
#include "Radio.h"

static const uint8_t* rxMac = FIXTURES[ACTIVE_FIXTURE].mac;
static const char*    errMsg = nullptr;
static uint8_t        seq = 0;

static volatile bool     haveStatus = false;
static volatile uint32_t lastStatusMs = 0;
static volatile int8_t   localRssi = -127;
static HzStatus          lastStatus;

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != (int)sizeof(HzStatus)) return;
  const HzStatus* s = (const HzStatus*)data;
  if (s->magic != HZ_MAGIC || s->version != HZ_PROTOCOL_VERSION || s->cmd != HZ_CMD_STATUS) return;
  if (memcmp(info->src_addr, rxMac, 6) != 0) return;
  memcpy(&lastStatus, s, sizeof(HzStatus));
  localRssi    = info->rx_ctrl->rssi;
  lastStatusMs = millis();
  haveStatus   = true;
}

void radioBegin() {
  bool macSet = false;
  for (int i = 0; i < 6; i++) if (rxMac[i] != 0) macSet = true;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  esp_wifi_set_channel(RADIO_CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.printf("[RADIO] this remote's MAC: %s  channel %d\n", WiFi.macAddress().c_str(), RADIO_CHANNEL);

  if (!macSet) { errMsg = "Set RX MAC in Config.h"; Serial.println("[RADIO] RX_MAC_BYTES not set"); return; }
  if (esp_now_init() != ESP_OK) { errMsg = "ESP-NOW init failed"; Serial.println("[RADIO] ESP-NOW init failed"); return; }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t pi = {};
  memcpy(pi.peer_addr, rxMac, 6);
  pi.channel = RADIO_CHANNEL;
  pi.encrypt = false;
  if (esp_now_add_peer(&pi) != ESP_OK) { errMsg = "Add peer failed"; Serial.println("[RADIO] add peer failed"); }
}

const char* radioError() { return errMsg; }

static void sendPacket(uint8_t cmd, const uint8_t* values, uint8_t count, uint16_t arg) {
  if (errMsg) return;
  HzPacket p = {};
  p.magic   = HZ_MAGIC;
  p.version = HZ_PROTOCOL_VERSION;
  p.cmd     = cmd;
  p.seq     = seq++;
  p.arg     = arg;
  p.count   = count > HZ_MAX_VALUES ? HZ_MAX_VALUES : count;
  for (uint8_t i = 0; i < p.count; i++) p.values[i] = values[i];
  esp_now_send(rxMac, (const uint8_t*)&p, sizeof(p));
}

void radioSendData(const uint8_t* values, uint8_t count) {
  sendPacket(HZ_CMD_DATA, values, count, 0);
}

void radioSendCommand(uint8_t cmd, uint16_t arg) {
  sendPacket(cmd, nullptr, 0, arg);
}

RadioStatus radioGetStatus() {
  RadioStatus r = {};
  uint32_t now = millis();
  r.ageMs  = haveStatus ? (now - lastStatusMs) : 0xFFFFFFFFUL;
  r.linkUp = haveStatus && r.ageMs < LINK_TIMEOUT_MS;
  r.rssi   = localRssi;
  if (haveStatus) {
    r.rxRssi  = lastStatus.rxRssi;
    r.fwMajor = lastStatus.fwMajor;
    r.fwMinor = lastStatus.fwMinor;
    r.flags   = lastStatus.flags;
    r.base    = lastStatus.baseAddr;
    for (int i = 0; i < 3; i++) r.echo[i] = lastStatus.echo[i];
  }
  return r;
}
