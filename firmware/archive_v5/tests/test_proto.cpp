// Host-side unit tests of the shared protocol code:   g++ -std=c++11 -Wall -I../shared test_proto.cpp -o test_proto && ./test_proto
#include <stdio.h>
#include <string.h>
#include "pc_proto.h"
#include "pc_ndef.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void testCrc() {
  const char* s = "123456789";
  CHECK(pc::crc32((const uint8_t*)s, 9) == 0xCBF43926u);           // standard CRC-32 check value
  uint32_t c = pc::crc32Update(0, (const uint8_t*)s, 4);
  c = pc::crc32Update(c, (const uint8_t*)s + 4, 5);
  CHECK(c == 0xCBF43926u);
}

static void testFrames() {
  char out[256];
  const size_t n = pc::frameEncode(out, sizeof(out), "P,1,7,42,350,253,1,1760000000.123,0");
  CHECK(n > 0 && out[0] == '$' && out[n - 1] == '\n');
  pc::LineParser lp;
  bool got = false;
  for (size_t i = 0; i < n; i++) got = lp.feed(out[i]);
  CHECK(got);
  CHECK(strcmp(lp.payload(), "P,1,7,42,350,253,1,1760000000.123,0") == 0);

  // corrupted checksum / noise before the frame
  pc::LineParser lp2;
  out[n - 3] ^= 1;
  bool ok = false;
  for (const char* p = "garbage"; *p; p++) lp2.feed(*p);
  for (size_t i = 0; i < n; i++) ok = lp2.feed(out[i]) || ok;
  CHECK(!ok && lp2.badFrames() == 1);

  char pay[] = "N,1,2,P,1,0.000,0,personnel:123";
  char* f[12];
  const int k = pc::splitFields(pay, f, 12);
  CHECK(k == 8 && strcmp(f[0], "N") == 0 && strcmp(f[7], "personnel:123") == 0);
  char empty[] = "S,,,x";
  CHECK(pc::splitFields(empty, f, 12) == 4 && f[1][0] == '\0');
}

static void testTime() {
  char b[24];
  pc::formatTime(b, sizeof(b), 1760000000u, 7);
  CHECK(strcmp(b, "1760000000.007") == 0);
  uint32_t s; uint16_t ms;
  CHECK(pc::parseTime("1760000000.123", s, ms) && s == 1760000000u && ms == 123);
  CHECK(pc::parseTime("5.5", s, ms) && s == 5 && ms == 500);
  int32_t v;
  CHECK(pc::parseInt("-123", v) && v == -123);
  CHECK(!pc::parseInt("12x", v));
}

static void testSanitize() {
  char o[16];
  pc::sanitize(o, sizeof(o), "a,b*c$d\n");
  CHECK(strcmp(o, "a_b_c_d_") == 0);
  pc::sanitize(o, 4, "abcdef");
  CHECK(strcmp(o, "abc") == 0);
}

static void testNdef() {
  uint8_t buf[64];
  const size_t n = pc::ndefBuildText("personnel:12345", buf, sizeof(buf), 16);
  CHECK(n == 32 && buf[0] == 0x03 && buf[2] == 0xD1);
  char text[40];
  CHECK(pc::ndefParseText(buf, n, text, sizeof(text)) == 15 && strcmp(text, "personnel:12345") == 0);
  // as written by the NDEF Arduino library for "die:7"
  const uint8_t lib[] = {0x03, 0x0C, 0xD1, 0x01, 0x08, 0x54, 0x02, 'e', 'n', 'd', 'i', 'e', ':', '7', 0xFE};
  CHECK(pc::ndefParseText(lib, sizeof(lib), text, sizeof(text)) == 5 && strcmp(text, "die:7") == 0);
  // blank card, garbage and truncated data must not crash
  const uint8_t blank[16] = {0};
  CHECK(pc::ndefParseText(blank, sizeof(blank), text, sizeof(text)) == 0);
  const uint8_t cut[] = {0x03, 0x20, 0xD1, 0x01};
  CHECK(pc::ndefParseText(cut, sizeof(cut), text, sizeof(text)) == 0);
  CHECK(pc::ndefBuildText("", buf, sizeof(buf)) == 0);
  CHECK(pc::ndefBuildText("0123456789012345678901234567890123456789", buf, 20) == 0);
}

static void testErrors() {
  CHECK(!strcmp(pc::errorText(pc::ERR_NONE), "OK"));
  CHECK(strcmp(pc::errorText(pc::ERR_LAST), "UNKNOWN") != 0);          // every code up to ERR_LAST has a text
  CHECK(!strcmp(pc::errorText(pc::ERR_LAST + 1), "UNKNOWN"));
  CHECK(pc::errorSeverity(pc::ERR_OVERTEMP) == 2 && pc::errorSeverity(pc::ERR_SERVER) == 1 && pc::errorSeverity(pc::ERR_NONE) == 0);
  CHECK(pc::ERR_LAST < 16);                                              // the error mask on the link is 16 bit wide
}

static void testEventLines() {                                         // the exact frame shapes the gateway parses (docs/PROTOCOL.md)
  char out[128];
  const size_t n = pc::frameEncode(out, sizeof(out), "P,7,42,350,253,1,1760000000.123");
  pc::LineParser lp;
  bool got = false;
  for (size_t i = 0; i < n; i++) got = lp.feed(out[i]);
  CHECK(got);
  char* f[10];
  const int k = pc::splitFields(lp.payload(), f, 10);
  CHECK(k == 7 && f[0][0] == 'P' && pc::toInt(f[1]) == 7 && pc::toInt(f[2]) == 42 && pc::toInt(f[3]) == 350);
  uint32_t sec; uint16_t ms;
  CHECK(pc::parseTime(f[6], sec, ms) && sec == 1760000000u && ms == 123);
}

int main() {
  testCrc(); testFrames(); testTime(); testSanitize(); testNdef(); testErrors(); testEventLines();
  if (failures) { printf("%d check(s) failed\n", failures); return 1; }
  printf("all checks passed\n");
  return 0;
}
