// Radio.cpp  (Hazer-TX)
// Fire-and-forget ESP-NOW sends to the selected receivers; status replies from each
// receiver provide heartbeat, version, RSSI, value echo and flags.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "Config.h"
#include "HazerProtocol.h"
#include "FixtureTable.h"
#include "Radio.h"

static const char* errMsg = nullptr;
static uint8_t     seq = 0;
static uint8_t     targets = 0x01;
static uint8_t     peerOk = 0;          // bit i = RECEIVERS[i] registered as a peer

static volatile bool     haveStatus[RECEIVER_COUNT];
static volatile uint32_t lastStatusMs[RECEIVER_COUNT];
static volatile int8_t   localRssi[RECEIVER_COUNT];
static HzStatus          lastStatus[RECEIVER_COUNT];

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != (int)sizeof(HzStatus)) return;
  const HzStatus* s = (const HzStatus*)data;
  if (s->magic != HZ_MAGIC || s->version != HZ_PROTOCOL_VERSION || s->cmd != HZ_CMD_STATUS) return;
  for (int i = 0; i < RECEIVER_COUNT; i++) {
    if (memcmp(info->src_addr, RECEIVERS[i].mac, 6) != 0) continue;
    memcpy(&lastStatus[i], s, sizeof(HzStatus));
    localRssi[i]    = info->rx_ctrl->rssi;
    lastStatusMs[i] = millis();
    haveStatus[i]   = true;
    return;
  }
}

void radioBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  esp_wifi_set_channel(RADIO_CHANNEL, WIFI_SECOND_CHAN_NONE);
  uint8_t own[6];
  esp_wifi_get_mac(WIFI_IF_STA, own);      // WiFi.macAddress() can still read zeros this early
  Serial.printf("[RADIO] this remote's MAC: %02X:%02X:%02X:%02X:%02X:%02X  channel %d\n",
                own[0], own[1], own[2], own[3], own[4], own[5], RADIO_CHANNEL);

  if (esp_now_init() != ESP_OK) { errMsg = "ESP-NOW init failed"; Serial.println("[RADIO] ESP-NOW init failed"); return; }
  esp_now_register_recv_cb(onRecv);

  for (int i = 0; i < RECEIVER_COUNT; i++) {
    bool macSet = false;
    for (int b = 0; b < 6; b++) if (RECEIVERS[i].mac[b] != 0) macSet = true;
    if (!macSet) { Serial.printf("[RADIO] %s MAC not set\n", RECEIVERS[i].name); continue; }

    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, RECEIVERS[i].mac, 6);
    pi.channel = RADIO_CHANNEL;
    pi.encrypt = false;
    if (esp_now_add_peer(&pi) == ESP_OK) peerOk |= (1 << i);
    else Serial.printf("[RADIO] add peer %s failed\n", RECEIVERS[i].name);
  }
  if (!peerOk) errMsg = "Set RX MACs in Config.h";
}

const char* radioError() { return errMsg; }

static void sendPacket(uint8_t mask, uint8_t cmd, const uint8_t* values, uint8_t count, uint16_t arg) {
  if (errMsg) return;
  HzPacket p = {};
  p.magic   = HZ_MAGIC;
  p.version = HZ_PROTOCOL_VERSION;
  p.cmd     = cmd;
  p.seq     = seq++;
  p.arg     = arg;
  p.count   = count > HZ_MAX_VALUES ? HZ_MAX_VALUES : count;
  for (uint8_t i = 0; i < p.count; i++) p.values[i] = values[i];
  for (int i = 0; i < RECEIVER_COUNT; i++)
    if ((mask & peerOk) & (1 << i)) esp_now_send(RECEIVERS[i].mac, (const uint8_t*)&p, sizeof(p));
}

void radioSetTargets(uint8_t mask) {
  if (mask == targets) return;
  uint8_t dropped = targets & ~mask;
  if (dropped) sendPacket(dropped, HZ_CMD_SAFE, nullptr, 0, 0);   // stop it now, not after fail-safe
  targets = mask;
}

uint8_t radioTargets() { return targets; }

void radioSendData(const uint8_t* values, uint8_t count) {
  sendPacket(targets, HZ_CMD_DATA, values, count, 0);
}

void radioSendCommand(uint8_t cmd, uint16_t arg) {
  sendPacket(targets, cmd, nullptr, 0, arg);
}

void radioSendCommandTo(int rx, uint8_t cmd, uint16_t arg) {
  sendPacket(1 << rx, cmd, nullptr, 0, arg);
}

RadioStatus radioGetStatus(int rx) {
  RadioStatus r = {};
  uint32_t now = millis();
  r.ageMs  = haveStatus[rx] ? (now - lastStatusMs[rx]) : 0xFFFFFFFFUL;
  r.linkUp = haveStatus[rx] && r.ageMs < LINK_TIMEOUT_MS;
  r.rssi   = localRssi[rx];
  if (haveStatus[rx]) {
    r.rxRssi  = lastStatus[rx].rxRssi;
    r.fwMajor = lastStatus[rx].fwMajor;
    r.fwMinor = lastStatus[rx].fwMinor;
    r.flags   = lastStatus[rx].flags;
    r.base    = lastStatus[rx].baseAddr;
    for (int i = 0; i < 3; i++) r.echo[i] = lastStatus[rx].echo[i];
  }
  return r;
}
