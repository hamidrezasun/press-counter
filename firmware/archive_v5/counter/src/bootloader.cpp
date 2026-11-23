#include "bootloader.h"
#include "link.h"

namespace {
const uint16_t MAGIC_ROM = 0xB007;               // BKP_DR1: jump to the system-memory bootloader after the next reset
bool g_wdt = false;

void bkpUnlock() {
  RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
  PWR->CR |= PWR_CR_DBP;                          // backup domain write access
}
}  // namespace

namespace bootloader {

void earlyInit() {
  g_wdt = (RCC->CSR & RCC_CSR_IWDGRSTF) != 0;
  RCC->CSR |= RCC_CSR_RMVF;                       // clear the flags for the next reset

  bkpUnlock();
  if (BKP->DR1 == MAGIC_ROM) {
    BKP->DR1 = 0;
    // leave the clock tree in its reset state, mask everything, then start the ROM code like after a reset
    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}
    RCC->CFGR = 0;
    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_PLLON | RCC_CR_CSSON);
    RCC->CIR = 0x009F0000;
    for (int i = 0; i < 8; i++) { NVIC->ICER[i] = 0xFFFFFFFFUL; NVIC->ICPR[i] = 0xFFFFFFFFUL; }
    const uint32_t sp = *(volatile uint32_t*)0x1FFFF000UL;
    const uint32_t entry = *(volatile uint32_t*)0x1FFFF004UL;
    __set_MSP(sp);
    ((void (*)(void))entry)();
    while (true) {}
  }
}

void enterRom() {
  bkpUnlock();
  BKP->DR1 = MAGIC_ROM;
  LinkSerial.flush();
  NVIC_SystemReset();
  while (true) {}
}

bool wasWatchdogReset() { return g_wdt; }

}  // namespace bootloader
