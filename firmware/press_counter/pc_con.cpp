#include "pc_con.h"
#include <esp_rom_sys.h>
#include <soc/uart_reg.h>

namespace {
class RomPrint : public Print {
 public:
  size_t write(uint8_t c) override { ets_printf("%c", (char)c); return 1; }
  size_t write(const uint8_t* b, size_t n) override {
    for (size_t i = 0; i < n; i++) ets_printf("%c", (char)b[i]);
    return n;
  }
};
RomPrint g_out;
}  // namespace

namespace con {

void begin() {}

Print& out() { return g_out; }

// UART0 was set up by the ROM for the boot messages (115200 8N1), so only the RX FIFO is polled here.
int read() {
  if ((REG_READ(UART_STATUS_REG(0)) & UART_RXFIFO_CNT_M) == 0) return -1;
  return (int)(REG_READ(UART_FIFO_REG(0)) & 0xFF);
}

}  // namespace con
