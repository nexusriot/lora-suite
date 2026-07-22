#pragma once

// Cap LoRa868 as wired to the Cardputer-Adv EXT / Cap-Bus 14-pin header.
// Source: docs.m5stack.com/en/cap/Cap_LoRa868
namespace ls {
namespace pins {

// SX1262 radio (SPI). SCK/MOSI/MISO are shared with the microSD slot.
constexpr int LORA_NSS  = 5;
constexpr int LORA_SCK  = 40;   // shared bus
constexpr int LORA_MOSI = 14;   // shared bus
constexpr int LORA_MISO = 39;   // shared bus
constexpr int LORA_BUSY = 6;
constexpr int LORA_DIO1 = 4;    // IRQ
constexpr int LORA_RST  = 3;

// ATGM336H GPS on UART1. Pin names are the module's; the host must read NMEA
// on the line the GPS transmits. If parsing fails, swap RX/TX below.
constexpr int GPS_MODULE_RX = 13;  // data into the GPS  (host TX)
constexpr int GPS_MODULE_TX = 15;  // data out of the GPS (host RX)
constexpr int GPS_BAUD      = 115200;

// Cardputer-Adv built-in microSD chip-select on the shared SPI bus.
constexpr int SD_CS = 12;

} // namespace pins
} // namespace ls
