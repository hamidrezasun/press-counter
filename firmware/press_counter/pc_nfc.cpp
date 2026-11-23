#include "pc_nfc.h"
#include <MFRC522.h>
#include "pc_ndef.h"
#include "pc_errors.h"

namespace {
MFRC522 g_rc(PIN_RC_SS, PIN_RC_RST);
nfc::TagCallback g_cb = nullptr;
bool     g_present = false;
uint32_t g_lastPoll = 0, g_lastHealth = 0, g_cooldownUntil = 0;
char     g_last[MAX_TAG_TEXT + 2] = "";
bool     g_writePending = false;
char     g_writeText[MAX_TAG_TEXT + 2];
uint32_t g_writeSince = 0;

// keys tried on MIFARE Classic: NFC Forum NDEF key, factory default, MAD key A
const uint8_t KEYS[3][6] = {{0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}};

bool authBlock(byte block, MFRC522::MIFARE_Key& key, int& keyIdx) {
  // try the key that worked last time first
  for (int n = 0; n < 3; n++) {
    const int k = (keyIdx + n) % 3;
    memcpy(key.keyByte, KEYS[k], 6);
    if (g_rc.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(g_rc.uid)) == MFRC522::STATUS_OK) { keyIdx = k; return true; }
    // a failed authentication leaves the card in a state where it must be re-selected
    byte atqa[2]; byte sz = sizeof(atqa);
    g_rc.PICC_WakeupA(atqa, &sz);
    g_rc.PICC_ReadCardSerial();
  }
  return false;
}

// Reads the NDEF TLV area into buf (max 48 bytes).  Returns the byte count or 0.
size_t readTag(uint8_t* buf, MFRC522::PICC_Type type) {
  byte tmp[18]; byte sz;
  if (type == MFRC522::PICC_TYPE_MIFARE_UL) {                   // NTAG / Ultralight: user memory starts at page 4
    size_t n = 0;
    for (byte page = 4; page <= 12 && n + 16 <= 48; page += 4) {
      sz = sizeof(tmp);
      if (g_rc.MIFARE_Read(page, tmp, &sz) != MFRC522::STATUS_OK) break;
      memcpy(buf + n, tmp, 16);
      n += 16;
    }
    return n;
  }
  if (type == MFRC522::PICC_TYPE_MIFARE_1K || type == MFRC522::PICC_TYPE_MIFARE_4K || type == MFRC522::PICC_TYPE_MIFARE_MINI) {
    MFRC522::MIFARE_Key key;
    int keyIdx = 0;
    if (!authBlock(4, key, keyIdx)) return 0;
    size_t n = 0;
    for (byte block = 4; block <= 6; block++) {                  // sector 1 data blocks
      sz = sizeof(tmp);
      if (g_rc.MIFARE_Read(block, tmp, &sz) != MFRC522::STATUS_OK) break;
      memcpy(buf + n, tmp, 16);
      n += 16;
    }
    return n;
  }
  return 0;
}

bool writeTag(const char* text, MFRC522::PICC_Type type) {
  uint8_t data[48];
  memset(data, 0, sizeof(data));
  const size_t n = pc::ndefBuildText(text, data, sizeof(data), 4);
  if (n == 0) return false;
  if (type == MFRC522::PICC_TYPE_MIFARE_UL) {
    for (size_t i = 0; i < n; i += 4) {
      if (g_rc.MIFARE_Ultralight_Write((byte)(4 + i / 4), data + i, 4) != MFRC522::STATUS_OK) return false;
    }
    return true;
  }
  if (type == MFRC522::PICC_TYPE_MIFARE_1K || type == MFRC522::PICC_TYPE_MIFARE_4K || type == MFRC522::PICC_TYPE_MIFARE_MINI) {
    MFRC522::MIFARE_Key key;
    int keyIdx = 0;
    if (!authBlock(4, key, keyIdx)) return false;
    for (byte b = 0; b < 3; b++) {
      if (g_rc.MIFARE_Write((byte)(4 + b), data + b * 16, 16) != MFRC522::STATUS_OK) return false;
    }
    return true;
  }
  return false;
}
}  // namespace

namespace nfc {

char kindOf(const char* text) {
  if (strncmp(text, "personnel:", 10) == 0) return pc::NFC_PERSONNEL;
  if (strncmp(text, "die:", 4) == 0) return pc::NFC_DIE;
  return pc::NFC_UNKNOWN;
}

bool begin() {
  pinMode(PIN_RC_SS, OUTPUT);
  digitalWrite(PIN_RC_SS, HIGH);
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_RC_SS);
  g_rc.PCD_Init();
  g_rc.PCD_SetAntennaGain(g_rc.RxGain_max);
  const byte v = g_rc.PCD_ReadRegister(MFRC522::VersionReg);
  g_present = (v != 0x00 && v != 0xFF);
  err::set(pc::ERR_RFID, !g_present);
  g_lastHealth = millis();
  return g_present;
}

void onTag(TagCallback cb) { g_cb = cb; }
bool present() { return g_present; }
const char* lastTag() { return g_last; }
bool writePending() { return g_writePending; }
void cancelWrite() { g_writePending = false; }
uint32_t writeRemainingMs() {
  if (!g_writePending) return 0;
  const uint32_t el = millis() - g_writeSince;
  return el >= TAG_WRITE_TIMEOUT_MS ? 0 : TAG_WRITE_TIMEOUT_MS - el;
}

void queueWrite(const char* text) {
  pc::sanitize(g_writeText, sizeof(g_writeText), text);
  if (strlen(g_writeText) == 0) return;
  g_writePending = true;
  g_writeSince = millis();
}

void poll() {
  const uint32_t now = millis();

  if (g_writePending && now - g_writeSince > TAG_WRITE_TIMEOUT_MS) {       // never overwrite a card long after the request
    g_writePending = false;
    err::latch(pc::ERR_TAG_WRITE, 1, 10000);                                  // timed out
  }

  // reader health: version register reads 0x00 / 0xFF when nothing answers; try a re-init once it is gone
  if (now - g_lastHealth >= NFC_HEALTH_MS) {
    g_lastHealth = now;
    byte v = g_rc.PCD_ReadRegister(MFRC522::VersionReg);
    if (v == 0x00 || v == 0xFF) { g_rc.PCD_Init(); v = g_rc.PCD_ReadRegister(MFRC522::VersionReg); }
    g_present = (v != 0x00 && v != 0xFF);
    err::set(pc::ERR_RFID, !g_present);
  }
  if (!g_present) return;
  if (now - g_lastPoll < NFC_POLL_MS || now < g_cooldownUntil) return;
  g_lastPoll = now;

  if (!g_rc.PICC_IsNewCardPresent() || !g_rc.PICC_ReadCardSerial()) return;
  const MFRC522::PICC_Type type = g_rc.PICC_GetType(g_rc.uid.sak);
  g_cooldownUntil = millis() + TAG_COOLDOWN_MS;

  if (g_writePending) {                                                    // write path: a written card must NOT switch the machine
    g_writePending = false;
    const bool ok = writeTag(g_writeText, type);
    g_rc.PICC_HaltA();
    g_rc.PCD_StopCrypto1();
    if (ok) { pc::copyStr(g_last, g_writeText, sizeof(g_last)); if (g_cb) g_cb(pc::NFC_WRITTEN, g_writeText); }
    else err::latch(pc::ERR_TAG_WRITE, 2, 10000);
    return;
  }

  uint8_t buf[48];
  memset(buf, 0, sizeof(buf));
  const size_t n = readTag(buf, type);
  g_rc.PICC_HaltA();
  g_rc.PCD_StopCrypto1();
  char text[MAX_TAG_TEXT + 2];
  if (n == 0 || pc::ndefParseText(buf, n, text, sizeof(text)) == 0) {
    if (g_cb) g_cb(pc::NFC_UNKNOWN, "");                                  // unreadable / blank card
    return;
  }
  pc::copyStr(g_last, text, sizeof(g_last));
  if (g_cb) g_cb(kindOf(text), text);
}

}  // namespace nfc
