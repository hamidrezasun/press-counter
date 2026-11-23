#include "NfcManager.h"

NfcManager::NfcManager() : _rc522(NFC_SS_PIN, -1), _nfc(&_rc522) {}

bool NfcManager::begin() {
  SPI.begin();
  _rc522.PCD_Init();
  _nfc.begin();
  const byte v = _rc522.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.printf("[NFC] RC522 version register 0x%02X\n", v);
  return v != 0x00 && v != 0xFF;                 // 0x00/0xFF = no chip answering on SPI
}

bool NfcManager::readText(char* out, size_t outSize) {
  NfcTag tag = _nfc.read();
  if (!tag.hasNdefMessage()) return false;

  NdefMessage message = tag.getNdefMessage();
  for (int i = 0; i < message.getRecordCount(); i++) {
    NdefRecord rec = message.getRecord(i);
    // well-known Text record: TNF 0x01, type "T"
    if (rec.getTnf() != 0x01 || rec.getTypeLength() != 1 || rec.getType()[0] != 'T') continue;

    const byte* payload = rec.getPayload();
    const int len = rec.getPayloadLength();
    if (len < 2) continue;
    const int textStart = 1 + (payload[0] & 0x3F);          // status byte + language code
    if (len <= textStart) continue;

    size_t n = (size_t)(len - textStart);
    if (n >= outSize) n = outSize - 1;
    memcpy(out, payload + textStart, n);
    out[n] = '\0';
    return n > 0;
  }
  return false;
}

bool NfcManager::writeText(const char* text) {
  for (int attempt = 1; attempt <= 5; attempt++) {
    NdefMessage message;
    message.addTextRecord(text);
    if (_nfc.write(message)) return true;
    Serial.printf("[NFC] write attempt %d failed\n", attempt);
    _rc522.PCD_Init();
    delay(50);
  }
  return false;
}

void NfcManager::setPendingWrite(const char* text) {
  strlcpy(_pendingText, text, sizeof(_pendingText));
  _pendingWrite = true;
  _pendingSince = millis();
  Serial.printf("[NFC] write queued: %s\n", _pendingText);
}

void NfcManager::handle(ControlManager& ctrl, InfluxDB& influx, PressCounter& press) {
  const unsigned long now = millis();
  if (now - _lastPoll < NFC_POLL_MS) return;
  _lastPoll = now;

  if (_pendingWrite && now - _pendingSince > PENDING_WRITE_TIMEOUT_MS) {
    _pendingWrite = false;                       // never overwrite an operator's card long after the web request
    Serial.println("[NFC] pending write expired");
    ctrl.playWarningSound();
  }
  if (now < _cooldownUntil) return;
  if (!_nfc.tagPresent()) return;

  // ── write path ──────────────────────────────────────────────────────────────
  if (_pendingWrite) {
    _pendingWrite = false;
    if (writeText(_pendingText)) {
      strlcpy(_lastTag, _pendingText, sizeof(_lastTag));
      ctrl.playSuccessSound();                   // writing a card must NOT switch the machine
    } else {
      ctrl.playErrorSound();
    }
    _cooldownUntil = millis() + TAG_COOLDOWN_MS;
    return;
  }

  // ── read path ───────────────────────────────────────────────────────────────
  char text[MAX_TAG_DATA_LENGTH];
  _cooldownUntil = millis() + TAG_COOLDOWN_MS;
  if (!readText(text, sizeof(text))) {
    ctrl.playErrorSound();                       // blank / unreadable / not NDEF text
    return;
  }
  strlcpy(_lastTag, text, sizeof(_lastTag));
  Serial.printf("[NFC] tag: %s\n", text);

  if (strncmp(text, "personnel:", 10) == 0) {
    ctrl.toggleRelay("personnel tag");
    press.reset();
    ctrl.playSuccessSound();
  } else if (strncmp(text, "die:", 4) == 0) {
    ctrl.playSuccessSound();                     // die card only identifies the tool
  } else {
    ctrl.playWarningSound();                     // unknown format: log it, change nothing
  }

  // log the event with the relay state AFTER handling (analyzer starts/ends sessions from this)
  String esc = text;
  esc.replace("\\", "\\\\");
  esc.replace("\"", "\\\"");
  char fields[MAX_TAG_DATA_LENGTH + 48];
  snprintf(fields, sizeof(fields), "tag_data=\"%s\",relay_state=%s", esc.c_str(), ctrl.relayOn() ? "true" : "false");
  influx.sendData("nfc_event", fields);
}
