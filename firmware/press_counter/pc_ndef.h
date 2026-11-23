// pc_ndef.h - minimal NDEF "Text" record handling for MIFARE Classic sector 1 / NTAG / Ultralight user memory.
// Plain C++11 (no Arduino / reader dependency) so it can be unit-tested on a PC (firmware/tests).
//
// Layout of the tag memory the firmware reads and writes (NFC Forum type-2 style TLV, same as the NDEF library
// the previous firmware used, so cards written by the old firmware stay readable):
//   03 <len> D1 01 <plen> 54 02 'e' 'n' <text...> FE
//   03 = NDEF message TLV, D1 = MB|ME|SR|TNF=well-known, 01 = type length, 54 = 'T' (text), 02 = UTF-8, 2 byte language "en"
#ifndef PC_NDEF_H
#define PC_NDEF_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace pc {

// Extracts the text of the first well-known Text record found in a TLV block.  Returns the text length (0 = none).
inline size_t ndefParseText(const uint8_t* buf, size_t len, char* out, size_t outSize) {
  if (outSize == 0) return 0;
  out[0] = '\0';
  size_t i = 0;
  while (i < len) {
    const uint8_t tag = buf[i++];
    if (tag == 0x00) continue;                       // NULL TLV (padding)
    if (tag == 0xFE) break;                          // terminator
    if (i >= len) break;
    size_t l = buf[i++];
    if (l == 0xFF) {                                 // 3 byte length
      if (i + 2 > len) break;
      l = ((size_t)buf[i] << 8) | buf[i + 1];
      i += 2;
    }
    if (tag != 0x03) { i += l; continue; }           // not an NDEF message TLV: skip
    if (i + l > len) l = len - i;                    // truncated read: parse what we have
    const uint8_t* m = buf + i;
    size_t p = 0;
    while (p + 3 <= l) {                             // walk the records of the message
      const uint8_t hdr = m[p++];
      const bool sr = hdr & 0x10, il = hdr & 0x08;
      const uint8_t tnf = hdr & 0x07;
      const uint8_t typeLen = m[p++];
      size_t payLen = 0;
      if (sr) {
        if (p >= l) break;
        payLen = m[p++];
      } else {
        if (p + 4 > l) break;
        payLen = ((size_t)m[p] << 24) | ((size_t)m[p + 1] << 16) | ((size_t)m[p + 2] << 8) | m[p + 3];
        p += 4;
      }
      size_t idLen = 0;
      if (il) { if (p >= l) break; idLen = m[p++]; }
      if (p + typeLen + idLen > l) break;
      const uint8_t* type = m + p;
      p += typeLen + idLen;
      if (p + payLen > l) payLen = l - p;
      if (tnf == 0x01 && typeLen == 1 && type[0] == 'T' && payLen >= 1) {
        const size_t langLen = m[p] & 0x3F;
        if (1 + langLen < payLen) {
          size_t n = payLen - 1 - langLen;
          if (n >= outSize) n = outSize - 1;
          memcpy(out, m + p + 1 + langLen, n);
          out[n] = '\0';
          return n;
        }
      }
      p += payLen;
      if (hdr & 0x40) break;                         // ME: last record
    }
    return 0;
  }
  return 0;
}

// Builds the TLV block for `text` into out (zero padded up to a multiple of `pad` bytes).  Returns the number of bytes
// written (including padding) or 0 if it does not fit.  The text must be 1..(255-7) characters.
inline size_t ndefBuildText(const char* text, uint8_t* out, size_t outSize, size_t pad = 4) {
  const size_t n = strlen(text);
  if (n == 0 || n > 200) return 0;
  const size_t payLen = 3 + n;                       // status + "en" + text
  const size_t recLen = 4 + payLen;                  // header, type length, payload length, type, payload
  const size_t total = 2 + recLen + 1;               // TLV tag + length + record + terminator
  const size_t padded = pad ? ((total + pad - 1) / pad) * pad : total;
  if (recLen > 254 || padded > outSize) return 0;
  size_t i = 0;
  out[i++] = 0x03;
  out[i++] = (uint8_t)recLen;
  out[i++] = 0xD1;
  out[i++] = 0x01;
  out[i++] = (uint8_t)payLen;
  out[i++] = 'T';
  out[i++] = 0x02;                                   // UTF-8, language code length 2
  out[i++] = 'e';
  out[i++] = 'n';
  memcpy(out + i, text, n);
  i += n;
  out[i++] = 0xFE;
  while (i < padded) out[i++] = 0x00;
  return padded;
}

}  // namespace pc

#endif  // PC_NDEF_H
