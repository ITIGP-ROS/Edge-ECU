# STM32F401CC Custom Bootloader — Technical Reference

> **Component** — Second-stage-free, single-image custom bootloader for the STM32F401CCU6 node of the vehicle gateway project.
> **Location** — `GP/ECU/STM32/Bootloader/`
> **Target** — STM32F401CCU6, Cortex-M4F @ 84 MHz, 256 KB Flash, 64 KB SRAM
> **Toolchain** — PlatformIO, `framework = cmsis`, no ST HAL, no CubeMX
> **Footprint** — 13,100 B `.text` + 188 B `.data` = **13,288 B Flash**, 15,104 B RAM
> **Host** — ESP32 gateway over UART1 @ 115200 8N1

---

**Scope of this document.** This is the bootloader only. The FreeRTOS/TinyML application that the bootloader launches is documented separately in [`../Application/APPLICATION.md`](../Application/APPLICATION.md); how the two halves cooperate is documented in [`../STM32_OVERVIEW.md`](../STM32_OVERVIEW.md). Where a statement here depends on application behaviour it is called out explicitly and cross-referenced rather than restated.

Everything below was derived by reading the source. Where the code disagrees with a comment in the code, the code wins and the discrepancy is flagged.

---

## Table of Contents

**Part I — Orientation**

- [1. Purpose and Responsibilities](#1-purpose-and-responsibilities)
- [2. System Context](#2-system-context)
- [3. Design Constraints](#3-design-constraints)
- [4. Source Tree Map](#4-source-tree-map)

**Part II — Platform**

- [5. Target Microcontroller](#5-target-microcontroller)
- [6. Clock Tree and Bring-Up](#6-clock-tree-and-bring-up)
- [7. Flash Partition Map](#7-flash-partition-map)
- [8. RAM Map and the Shared Boot Flag](#8-ram-map-and-the-shared-boot-flag)
- [9. Linker Script Walkthrough](#9-linker-script-walkthrough)
- [10. Build System](#10-build-system)
- [11. Footprint Budget](#11-footprint-budget)

**Part III — Boot Sequence**

- [12. Reset to main()](#12-reset-to-main)
- [13. CheckForAppToRun — The Boot Decision](#13-checkforapptorun--the-boot-decision)
- [14. Application Vector-Table Validation](#14-application-vector-table-validation)
- [15. The Jump to Application](#15-the-jump-to-application)
- [16. Peripheral Bring-Up When Staying Resident](#16-peripheral-bring-up-when-staying-resident)
- [17. The One-Second LED Signature](#17-the-one-second-led-signature)

**Part IV — Host Protocol**

- [18. Protocol Overview](#18-protocol-overview)
- [19. Wire Format](#19-wire-format)
- [20. The Command Dispatch Loop](#20-the-command-dispatch-loop)
- [21. 0xAA — Enter-Bootloader Probe](#21-0xaa--enter-bootloader-probe)
- [22. 0x10 — Get Version](#22-0x10--get-version)
- [23. 0x12 — Jump to Application](#23-0x12--jump-to-application)
- [24. 0x13 — Erase Flash](#24-0x13--erase-flash)
- [25. 0x14 — Memory Write](#25-0x14--memory-write)
- [26. CRC32 Verification](#26-crc32-verification)
- [27. ACK, NACK and Reply Framing](#27-ack-nack-and-reply-framing)
- [28. The Timeout Model](#28-the-timeout-model)
- [29. End-to-End Message Sequences](#29-end-to-end-message-sequences)

**Part V — Firmware Update Engine**

- [30. Update Engine Architecture](#30-update-engine-architecture)
- [31. The Chunk Accumulator](#31-the-chunk-accumulator)
- [32. Two Ways a Transfer Ends](#32-two-ways-a-transfer-ends)
- [33. Decrypt-and-Flash](#33-decrypt-and-flash)
- [34. State Machine Reference](#34-state-machine-reference)
- [35. Engine Failure Modes](#35-engine-failure-modes)

**Part VI — Cryptography**

- [36. Threat Model](#36-threat-model)
- [37. AES-128-GCM Construction](#37-aes-128-gcm-construction)
- [38. Key and Nonce Derivation](#38-key-and-nonce-derivation)
- [39. The AAD Choice](#39-the-aad-choice)
- [40. mbedTLS Configuration](#40-mbedtls-configuration)
- [41. Crypto Memory Cost](#41-crypto-memory-cost)
- [42. The Self-Test Harness](#42-the-self-test-harness)
- [43. Security Assessment](#43-security-assessment)

**Part VII — Driver Layer**

- [44. Layering Model](#44-layering-model)
- [45. RCC Driver](#45-rcc-driver)
- [46. GPIO Driver](#46-gpio-driver)
- [47. UART Driver (MCAL)](#47-uart-driver-mcal)
- [48. HSerial (HAL)](#48-hserial-hal)
- [49. FLASH Driver](#49-flash-driver)
- [50. CRC Driver](#50-crc-driver)
- [51. SysTick Driver](#51-systick-driver)
- [52. LED Driver](#52-led-driver)
- [53. STD_Types and Return Codes](#53-std_types-and-return-codes)

**Part VIII — Operations**

- [54. Flashing the Bootloader](#54-flashing-the-bootloader)
- [55. Bench Procedures](#55-bench-procedures)
- [56. Failure Modes and Recovery](#56-failure-modes-and-recovery)
- [57. Known Gaps](#57-known-gaps)

**Appendices**

- [Appendix A — Command Byte Reference](#appendix-a--command-byte-reference)
- [Appendix B — Register Cheat Sheet](#appendix-b--register-cheat-sheet)
- [Appendix C — Public API Index](#appendix-c--public-api-index)
- [Appendix D — Glossary](#appendix-d--glossary)

---

# Part I — Orientation

## 1. Purpose and Responsibilities

The bootloader is the first code the STM32F401CC executes after every reset. It owns Flash sectors 0 and 1 and exists to do exactly four things:

1. **Decide** whether this boot should launch the application or stay resident.
2. **Validate** that an application image is actually present and structurally sane before jumping to it.
3. **Serve** a small command protocol over UART1 so the ESP32 gateway can erase and rewrite the application region.
4. **Decrypt** the incoming firmware image, chunk by chunk, and program it into Flash.

It deliberately does *not* do a number of things that bootloaders often do, and the absences are as important as the presences:

| Not done | Consequence |
|---|---|
| No image signature check | Authenticity rests entirely on the AES-GCM tag; see [§43](#43-security-assessment) |
| No version record | Version bookkeeping lives on the ESP32 in LittleFS; [§22](#22-0x10--get-version) |
| No golden image / rollback | A failed update leaves the node with no runnable application |
| No CRC over the finished image | Integrity is per-2 KB-chunk only, via the GCM tag |
| No read-out protection setup | RDP level is whatever the factory or a prior operator left it at |
| No interrupt use whatsoever | Everything is polled; see [§28](#28-the-timeout-model) |

```
                    ┌──────────────────────────────────────┐
                    │      What the bootloader owns        │
                    ├──────────────────────────────────────┤
                    │  Flash sectors 0-1   (32 KB)         │
                    │  UART1 (PA9/PA10) while resident     │
                    │  RAM word at 0x2000FFF8 (shared)     │
                    │  PA0 / PA1 LEDs                      │
                    │  CRC peripheral                      │
                    │  SysTick (polled, no exception)      │
                    └──────────────────────────────────────┘
                                     │
                                     │ hands over
                                     ▼
                    ┌──────────────────────────────────────┐
                    │     What the application owns        │
                    ├──────────────────────────────────────┤
                    │  Flash sectors 2-5   (224 KB)        │
                    │  Everything else                     │
                    └──────────────────────────────────────┘
```

## 2. System Context

The STM32 is a leaf node. It never talks to the outside world directly — the ESP32 gateway is the only device on the other end of UART1, and the gateway is the only path by which an update can arrive.

```
   Cloud / MQTT broker
            │
            │  MQTT over Wi-Fi
            ▼
   ┌────────────────────────┐
   │      ESP32 Gateway     │   Zephyr; holds the encrypted image in
   │  (ECU/ESP32, Zephyr)   │   LittleFS at /update/firmware.bin and
   │                        │   the version at /update/version.bin
   └───────┬────────────────┘
           │  UART2 on the ESP32 side
           │  UART1 on the STM32 side
           │  115200 8N1, no flow control
           ▼
   ┌────────────────────────┐
   │   STM32F401CC          │
   │  ┌──────────────────┐  │
   │  │  Bootloader      │  │  ← this document
   │  │  sectors 0-1     │  │
   │  ├──────────────────┤  │
   │  │  Application     │  │  ← ../Application/APPLICATION.md
   │  │  sectors 2-5     │  │
   │  └──────────────────┘  │
   └────────────────────────┘
           │
           │  (application only)
           ▼
      MPU6050, LM35, 2× HC-SR04
```

Two consequences fall out of this topology and shape the whole design:

**The link is shared and mode-switched.** UART1 carries application telemetry frames in one mode and bootloader commands in the other. There is no multiplexing — whichever firmware is running owns the line completely. That is why the enter-bootloader probe ([§21](#21-0xaa--enter-bootloader-probe)) has to be answered by *both* firmwares with the same byte sequence.

**The gateway is the system of record.** The bootloader keeps no persistent state at all: no version, no update counter, no boot-attempt counter. Everything it would want to remember lives across the link. This is why `Bootloader_GetVersion` returns a hardcoded zero.

## 3. Design Constraints

| Constraint | Value | Where it comes from |
|---|---|---|
| Flash budget | 32 KB (sectors 0-1) | Application linker script starts at `0x08008000` |
| RAM budget | 64 KB minus 8 reserved bytes | Shared boot-flag region at top of SRAM |
| No RTOS | — | Bootloader is a single `while(1)` polling loop |
| No interrupts | — | Vector table is the startup default; nothing is unmasked |
| Chunk buffer | 2048 B ciphertext + 2032 B plaintext | Host chunking is fixed at 2 KB |
| Link speed | 115200 baud | Matches the gateway's UART2 configuration |
| Max RX packet | 140 B (`BL_HOST_BUFFER_RX_LENGTH`) | Largest write packet is 74 B, so 140 is comfortable |

The 32 KB Flash ceiling is the binding constraint. AES-GCM via mbedTLS costs roughly 9 KB of the 13.3 KB used; everything else — five drivers, the protocol handler and the update engine — fits in the remaining 4 KB.

## 4. Source Tree Map

```
Bootloader/
├── platformio.ini                 build configuration
├── STM32F401CCUX_FLASH.ld         linker script (RAM carve-out for boot flag)
├── README.md                      st-flash cheat sheet
│
├── lib/
│   ├── STD_Types.h                uint8_t..float64_t, STD_ReturnType
│   └── BIT_Math.h                 bit macros (unused by current code)
│
├── include/
│   ├── app/
│   │   ├── bootloader.h           command codes, buffer sizes, magic values
│   │   ├── firmware_flashing.h    OTA chunk sizes and state enum
│   │   └── aes_gcm.h              decrypt API + self-test
│   ├── interface/                 public driver APIs
│   │   ├── Core/systick.h
│   │   ├── HAL/led.h
│   │   ├── HAL/hserial.h
│   │   └── MCAL/{rcc,gpio,uart,flash,crc}.h
│   ├── private/                   register structs, never included by app code
│   │   ├── Core/systick_priv.h
│   │   └── MCAL/{rcc,gpio,uart,flash,crc}_priv.h
│   ├── configuration/
│   │   ├── Core/systick_conf.h    clock-source and max-tick selection
│   │   └── HAL/led_cfg.h          LED name enum
│   └── mbedtls/                   vendored mbedTLS headers + trimmed config.h
│
├── src/
│   ├── main.c                     boot decision + resident loop
│   ├── app/
│   │   ├── bootloader.c           protocol handler (336 lines)
│   │   ├── firmware_flashing.c    OTA chunk accumulator (107 lines)
│   │   └── aes_gcm.c              GCM wrapper + self-test (162 lines)
│   ├── Core/systick.c
│   ├── HAL/interface/{led,hserial}.c
│   ├── HAL/config/led_cfg.c
│   ├── MCAL/interface/{rcc,gpio,uart,flash,crc}.c
│   └── mbedtls/                   aes.c, gcm.c, cipher.c, cipher_wrap.c,
│                                  constant_time.c, platform.c, platform_util.c
│
└── tests/
    ├── encrypt.main.c             alternative main() that decrypts two
    │                              known chunks and blinks on success
    ├── chunk0.h                   2032 B ciphertext + tag, chunk index 0
    └── chunk1.h                   1476 B ciphertext + tag, chunk index 1
```

The layering is conventional AUTOSAR-flavoured: `MCAL` touches registers, `HAL` composes MCAL into device abstractions, `app` composes HAL into behaviour. The one deviation is that `bootloader.c` reaches directly into `private/MCAL/flash_priv.h` — see [§49](#49-flash-driver).

---

# Part II — Platform

## 5. Target Microcontroller

| Property | Value |
|---|---|
| Part | STM32F401CCU6 |
| Core | ARM Cortex-M4F, ARMv7E-M |
| FPU | FPv4-SP-D16 (single precision) — **not used by the bootloader** |
| Max clock | 84 MHz |
| Flash | 256 KB, 6 sectors, `0x08000000`–`0x0803FFFF` |
| SRAM | 64 KB, `0x20000000`–`0x2000FFFF` |
| Package | UFQFPN48 |
| CRC unit | 32-bit, fixed polynomial `0x04C11DB7` |
| Debug | SWD via ST-Link |

The bootloader builds without hardware-float flags (`platformio.ini` sets no `-mfpu`), so the FPU is left off. Nothing in the bootloader path uses floating point.

Flash sector geometry matters and is not uniform:

| Sector | Base | Size | Erase granularity |
|---|---|---|---|
| 0 | `0x08000000` | 16 KB | whole sector |
| 1 | `0x08004000` | 16 KB | whole sector |
| 2 | `0x08008000` | 16 KB | whole sector |
| 3 | `0x0800C000` | 16 KB | whole sector |
| 4 | `0x08010000` | 64 KB | whole sector |
| 5 | `0x08020000` | 128 KB | whole sector |

Because sector 5 is 128 KB, erasing it dominates the erase command's latency — see [§24](#24-0x13--erase-flash).

## 6. Clock Tree and Bring-Up

The bootloader runs at the part's maximum, 84 MHz, sourced from an external 25 MHz crystal through the PLL.

```
  HSE  25 MHz  (external crystal)
    │
    ├─► PLLM = 25   ──►  VCO input   = 25 MHz / 25   = 1 MHz
    │                    (RM0368 wants 1–2 MHz here; 1 MHz is legal)
    ├─► PLLN = 336  ──►  VCO output  = 1 MHz × 336   = 336 MHz
    ├─► PLLP = /4   ──►  PLLCLK      = 336 / 4       = 84 MHz  ──► SYSCLK
    └─► PLLQ = 7    ──►  PLL48CLK    = 336 / 7       = 48 MHz  (unused)

  SYSCLK 84 MHz
    ├─ AHB  prescaler /1  ──► HCLK  = 84 MHz   (CPU, Flash, CRC, GPIO)
    ├─ APB1 prescaler /2  ──► PCLK1 = 42 MHz   (USART2 — unused here)
    └─ APB2 prescaler /1  ──► PCLK2 = 84 MHz   (USART1)

  Flash latency: 2 wait states (required above 60 MHz at 3.3 V)
```

The configuration is declared statically at the top of [`src/main.c`](src/main.c):

```c
RCC_CFG_t rcc_pll = {
    .sysClkSource = RCC_CLOCK_SOURCE_PLL,
    .pllClkSource = RCC_CLOCK_SOURCE_HSE,
    .pllConfig.pll_cfg_max_t = { .pllMax = RCC_PLL_MAX }
};
```

`RCC_PLL_MAX` (`0xFF`) is a sentinel meaning "give me the fastest legal settings for whichever source is selected". `RCC_SetPLLMaxClock()` branches on the current `PLLCFGR.PLLSRC` bit and picks the M/N/P/Q quadruple accordingly. Because `RCC_SetPLLClockSource(HSE)` runs before `RCC_ConfigurePLL()`, the HSE branch is the one taken.

### 6.1 The bring-up order, and why it looks odd

`RCC_ConfigureClock()` performs an unusual dance for the PLL case:

```
 1. Enable HSE                       RCC_SetClock(HSE, ENABLE)
 2. Wait for HSERDY                  RCC_WaitForClockReady(HSE, 500)
 3. Switch SYSCLK to HSE             RCC_SetSystemClock(HSE)          ◄── temporary
 4. Wait for SWS == HSE
 5. Disable the PLL                  RCC_SetClock(PLL, DISABLE)
 6. Select HSE as PLL source         RCC_SetPLLClockSource(HSE)
 7. Set Flash latency to 2 WS        FLASH_SetLatency(2WS)            ◄── before speed-up
 8. Program PLLM/N/P/Q               RCC_ConfigurePLL(...)
 9. Enable the PLL                   RCC_SetClock(PLL, ENABLE)
10. Wait for PLLRDY
11. AHB /1, APB1 /2, APB2 /1
12. Switch SYSCLK to PLL             RCC_SetSystemClock(PLL)
13. Wait for SWS == PLL
```

Step 3 is the part that surprises people. The MCU boots on HSI at 16 MHz; the code moves SYSCLK onto HSE *before* touching the PLL. This is not strictly required — you may reconfigure the PLL while running from HSI — but it is harmless and it guarantees that step 5 (disabling the PLL) can never pull the rug out from under the running core, regardless of what state a previous firmware left the PLL in. After a warm reset from the application, the PLL may well already be on and selected; the HSE detour makes step 5 safe in that case.

Step 7 (latency before speed-up) is mandatory. Programming 2 wait states while still at 25 MHz is safe — extra wait states never break a slow bus — whereas raising the clock first would cause Flash read errors before the latency write landed.

Step 11 sets **APB1 = /2** deliberately: 84 MHz would exceed the 42 MHz APB1 maximum. APB2 stays at /1 because its maximum is 84 MHz, which is exactly what USART1 needs to hit 115200 with minimal error.

### 6.2 Timeouts are loop counts, not milliseconds

Every `RCC_WaitFor*` call takes a "timeout" that is a bare decrementing loop counter:

```c
while ((RCC->CR.BITS.HSERDY == 0) && (tickStart < timeout)) {
    tickStart++;
}
```

`500` iterations at 25 MHz is on the order of a few dozen microseconds. HSE startup for a typical crystal is 1–2 ms. **The HSE-ready wait will time out on a cold start with a slow crystal**, and — critically — the return value is assigned into `ret` and then checked, so the whole chain aborts and `RCC_ConfigureClock` returns `STD_TIMEOUT`.

In `main()` the return value is checked only to gate the peripheral-clock enable:

```c
ret = RCC_ConfigureClock(&rcc_pll);
if(ret == STD_SUCCESS){
    ret = RCC_ControlPeripheral(RCC_GPIOA | RCC_GPIOB | RCC_GPIOC, RCC_PERIPHERAL_ENABLE);
}
ret = SYSTICK_Init(SYSTICK_CLOCK_SOURCE_PLL_MAX);   // runs regardless
```

So on a timeout the MCU keeps running at whatever clock it landed on while SysTick and UART are configured *as if* it were at 84 MHz. The symptom would be a wrong baud rate and wrong delays, not a hang. This has not been observed on the bench, which suggests the crystal starts fast enough in practice, but the margin is unquantified. Logged in [§57](#57-known-gaps).

## 7. Flash Partition Map

```
0x08000000 ┌──────────────────────────────────────────────┐  ▲
           │  Sector 0 — 16 KB                            │  │
           │    .isr_vector  (bootloader vector table)    │  │
           │    .text        (bootloader code)            │  │  BOOTLOADER
           │    .rodata      (AES tables? no — see §41)   │  │  32 KB total
0x08004000 ├──────────────────────────────────────────────┤  │  13,288 B used
           │  Sector 1 — 16 KB                            │  │  40.5 % full
           │    (bootloader spills here; currently mostly │  │
           │     empty — image is only 13 KB)             │  ▼
0x08008000 ├══════════════════════════════════════════════┤  ▲   ◄── FLASH_APP_START
           │  Sector 2 — 16 KB                            │  │
           │    application .isr_vector at +0x0000        │  │
           │      word 0 = initial MSP                    │  │
           │      word 1 = Reset_Handler                  │  │
           │    application .text ...                     │  │
0x0800C000 ├──────────────────────────────────────────────┤  │  APPLICATION
           │  Sector 3 — 16 KB                            │  │  224 KB total
0x08010000 ├──────────────────────────────────────────────┤  │  193,580 B used
           │  Sector 4 — 64 KB                            │  │  84.4 % full
0x08020000 ├──────────────────────────────────────────────┤  │
           │  Sector 5 — 128 KB                           │  │
0x0803FFFF └──────────────────────────────────────────────┘  ▼
```

Three constants encode this split, and all three must agree or the node bricks:

| Constant | File | Value |
|---|---|---|
| `APP_FLASH_BASE` | `include/app/bootloader.h` and again in `src/main.c` | `0x08008000` |
| `APP_FLASH_END` | same, twice | `0x08040000` |
| `FLASH_APP_START` | `include/app/firmware_flashing.h` | `0x08008000` |
| `ORIGIN(FLASH)` | `../Application/STM32F401CCFX_FLASH_Sector2.ld` | `0x08008000` |

Note the duplication: `SRAM_BASE`, `SRAM_END`, `APP_FLASH_BASE` and `APP_FLASH_END` are `#define`d **both** in `bootloader.h` and again at the top of `main.c`. They currently agree. There is no compile-time check that they continue to. See [§57](#57-known-gaps).

`APP_FLASH_END` is `0x08040000`, which is one past the last valid Flash byte (`0x0803FFFF`). The validation in [§14](#14-application-vector-table-validation) uses `>` rather than `>=` against it, so an entry point of exactly `0x08040000` would be accepted — an off-by-one that cannot actually occur because a linked image never places its reset handler outside its own region.

## 8. RAM Map and the Shared Boot Flag

The single most important design decision in this bootloader is how the application asks to be *re-entered into the bootloader*. It is done with one 32-bit word of SRAM that neither firmware's startup code will zero.

```
0x20000000 ┌──────────────────────────────────────────────┐  ▲
           │  .data                                       │  │
           │  .bss                                        │  │
           │    cipher_buffer     2048 B                  │  │
           │    plaintext_buffer  2032 B                  │  │  RAM
           │    mbedTLS AES FT0..FT3, RT0..RT3  8192 B    │  │  64 KB - 8
           │    rxBuffer 140 B, txBuffer[3][100] 300 B    │  │
           │  heap        512 B  (_Min_Heap_Size 0x200)   │  │
           │  stack      1024 B  (_Min_Stack_Size 0x400)  │  │
0x2000FFF7 ├──────────────────────────────────────────────┤  ▼
0x2000FFF8 │  RAM_BOOT — 8 bytes, section .boot_flag      │  ◄── NOLOAD, never zeroed
0x2000FFFF └──────────────────────────────────────────────┘
```

The mechanism:

```
 Application wants to enter the bootloader
        │
        │  *(volatile uint32_t *)0x2000FFF8 = 0xDEADBEEF;
        │  SCB->AIRCR = (0x05FA << 16) | (1 << 2);      // SYSRESETREQ
        ▼
 ═══ system reset ═══   (SRAM contents survive; only a POR clears them)
        │
        ▼
 Bootloader Reset_Handler
        │  copies .data, zeroes .bss   ← neither touches 0x2000FFF8,
        │                                 because RAM_BOOT is a separate
        │                                 MEMORY region outside .bss
        ▼
 CheckForAppToRun()
        │  flag = *BOOTLOADER_FLAG_ADDR        → 0xDEADBEEF
        │  *BOOTLOADER_FLAG_ADDR = 0           ← cleared immediately
        │  flag == BOOTLOADER_APP_MAGIC → return, stay resident
        ▼
 Bootloader resident loop
```

Two properties make this work:

**The region is outside `.bss`.** The linker script declares `RAM` as `LENGTH = 64K - 8` and a separate `RAM_BOOT` region for the last 8 bytes. `.bss` is placed `>RAM`, so the startup loop that zeroes from `_sbss` to `_ebss` provably cannot reach `0x2000FFF8`. The `.boot_flag (NOLOAD)` output section is placed `>RAM_BOOT` purely so that the address is reserved and visible in the map file; no symbol is actually assigned to it in the current code (both firmwares hardcode the address).

**The flag is cleared on read, not on use.** `CheckForAppToRun()` writes zero to the flag *before* it branches. That makes the request one-shot: if the bootloader is later reset by any means — power cycle, NRST, watchdog, or its own `Bootloader_JumpToApp` — the flag reads zero and the next boot goes straight to the application. Without this, a single "enter bootloader" request would trap the node in the bootloader forever.

The identical 8-byte carve-out appears in the application's linker script, which is what guarantees the application's own `.bss` also stops short of the flag.

> **Reset-source caveat.** A power-on reset clears SRAM to an indeterminate pattern, which is overwhelmingly unlikely to be `0xDEADBEEF` but is not architecturally guaranteed to differ. In practice STM32F4 SRAM powers up to a stable, mostly-zero pattern. The risk is accepted, not mitigated.

## 9. Linker Script Walkthrough

[`STM32F401CCUX_FLASH.ld`](STM32F401CCUX_FLASH.ld), 171 lines. The parts that differ from the stock ST template:

```ld
MEMORY
{
  RAM       (xrw) : ORIGIN = 0x20000000, LENGTH = 64K - 8   /* 0x20000000 – 0x2000FFF7 */
  RAM_BOOT  (xrw) : ORIGIN = 0x2000FFF8, LENGTH = 8         /* 0x2000FFF8 – 0x2000FFFF */
  FLASH     (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
}
```

`FLASH` is declared as the *whole* 256 KB, not just the 32 KB the bootloader is supposed to occupy. Nothing stops the linker from placing bootloader code at `0x08008000` and silently overlapping the application. At 13 KB there is a 19 KB margin, so it has never happened — but it is unenforced. Constraining `LENGTH` to `32K` would turn a silent overlap into a link error. Logged in [§57](#57-known-gaps).

```ld
_estack = ORIGIN(RAM) + LENGTH(RAM);
```

Because `LENGTH(RAM)` is `64K - 8`, `_estack` lands at `0x2000FFF8` — the first byte of the boot-flag region, which is correct: the stack grows *downwards* from `_estack`, so the flag itself is never in the stack's path.

```ld
  .boot_flag (NOLOAD) :
  {
    KEEP(*(.boot_flag))
  } >RAM_BOOT
```

`NOLOAD` means no initialiser bytes are emitted into the image; `KEEP` prevents `--gc-sections` from discarding the (currently empty) section.

```ld
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }
```

This is inherited from the ST template and is misleading. It does not prevent libc from being linked — as [§41](#41-crypto-memory-cost) shows, `calloc` and `free` *are* present in the final image, pulled from newlib-nano under a different archive name. The discard rule matches archives literally named `libc.a`; PlatformIO links `libc_nano.a`. Harmless, but it is not doing what it appears to do.

Heap and stack minimums:

```ld
_Min_Heap_Size  = 0x200;   /*  512 B */
_Min_Stack_Size = 0x400;   /* 1024 B */
```

The heap is genuinely used — mbedTLS allocates its AES context there. 512 bytes is enough for exactly one context (`sizeof(mbedtls_aes_context)` ≈ 288 B) plus allocator overhead. See [§41](#41-crypto-memory-cost).

## 10. Build System

[`platformio.ini`](platformio.ini) is 18 lines:

```ini
[env:genericSTM32F401CC]
platform = ststm32
board = genericSTM32F401CC
framework = cmsis
upload_protocol = stlink
debug_tool = stlink
build_type = debug
board_build.ldscript = STM32F401CCUX_FLASH.ld
```

Notable points:

| Setting | Effect |
|---|---|
| `framework = cmsis` | Only CMSIS core headers and the ST startup/system files. No HAL, no LL. |
| `build_type = debug` | `-Og -g` — the image is **not** size-optimised |
| `board_build.ldscript` | Overrides the board's default script with the boot-flag variant |
| no `build_flags` | No `-DSTM32F401xC`, no FPU flags, no include-path overrides |

`build_type = debug` is worth pausing on. The bootloader is 13,288 B at `-Og`. Switching to `build_type = release` (`-Os`) would very likely bring it under 11 KB. Since the ceiling is 32 KB there is no pressure to do so, and keeping symbols and unoptimised control flow makes single-stepping the protocol handler far easier. This is a deliberate trade, not an oversight — but it is worth re-checking if mbedTLS ever grows.

The absence of `build_flags` means include paths come from PlatformIO's defaults plus the implicit `include/` and `src/` roots. Every source file uses paths relative to `include/`, e.g. `#include "interface/MCAL/rcc.h"`, and the `lib/` headers are reached with explicit `../../lib/STD_Types.h` relative paths. That relative-path style is brittle — moving a header one directory deep breaks several files — but it removes the need for any `-I` configuration.

Build and flash:

```bash
pio run -d Bootloader
```

```bash
pio run -d Bootloader -t upload
```

## 11. Footprint Budget

Measured on the committed build (`arm-none-eabi-size`):

```
   text     data      bss      dec      hex
  13100      188    14916    28204     6e2c
```

| Region | Bytes | Of budget | Notes |
|---|---|---|---|
| Flash (`text` + `data`) | 13,288 | 40.5 % of 32 KB | comfortable |
| RAM (`data` + `bss`) | 15,104 | 23.0 % of 64 KB | plus 512 B heap + 1 KB stack |

Where the RAM goes — the four largest `.bss` objects account for 12,272 bytes of the 14,916:

| Symbol | Size | Source |
|---|---|---|
| `cipher_buffer` | 2,048 B | `firmware_flashing.c` |
| `plaintext_buffer` | 2,032 B | `firmware_flashing.c` |
| `FT0`…`FT3` | 4 × 1,024 B | mbedTLS AES forward tables, built at runtime |
| `RT0`…`RT3` | 4 × 1,024 B | mbedTLS AES reverse tables, built at runtime |

The 8 KB of AES tables are the interesting entry. mbedTLS generates them lazily on first `mbedtls_aes_setkey_*` call unless `MBEDTLS_AES_ROM_TABLES` is defined, in which case they become `const` and move to Flash. Given that the bootloader has 19 KB of Flash headroom and only 49 KB of RAM headroom, moving them would be a net improvement in both directions of the argument — Flash is the abundant resource here. `MBEDTLS_AES_FEWER_TABLES` would instead shrink them to 2 KB at a modest speed cost. Neither is currently set. See [§57](#57-known-gaps).

Where the Flash goes — the two largest text symbols are `mbedtls_internal_aes_encrypt` (1,076 B) and `mbedtls_internal_aes_decrypt` (1,076 B). Rough attribution of the 13.1 KB:

```
  mbedTLS (aes.c, gcm.c, cipher.c, cipher_wrap.c, constant_time.c) ~ 9.0 KB
  RCC driver (rcc.c, 555 lines, heavy switch statements)           ~ 1.4 KB
  Protocol handler (bootloader.c)                                  ~ 1.0 KB
  GPIO + UART + HSerial + SysTick + LED + CRC + FLASH              ~ 1.2 KB
  Startup, system_stm32f4xx, newlib-nano bits (calloc/free)        ~ 0.5 KB
```

The proportions are the headline: **two thirds of this bootloader is a crypto library.** Every other decision — no interrupts, no RTOS, polled UART, one command at a time — is downstream of keeping the remaining third small enough that the whole thing fits.

---

# Part III — Boot Sequence

## 12. Reset to main()

The bootloader uses the stock CMSIS startup file supplied by the PlatformIO `ststm32` platform. Nothing about it is customised, which means the pre-`main` sequence is entirely conventional:

```
  Power-on / NRST / SYSRESETREQ / IWDG
        │
        ▼
  Core fetches from 0x08000000
        word 0 → MSP  = _estack  = 0x2000FFF8
        word 1 → PC   = Reset_Handler
        │
        ▼
  Reset_Handler (startup_stm32f401xc.s)
        ├─ SystemInit()          — sets CP10/CP11 if FPU enabled (it is not here);
        │                          leaves the clock on HSI 16 MHz
        ├─ copy .data from _sidata (Flash) to _sdata.._edata (RAM)
        ├─ zero .bss from _sbss to _ebss     ← stops at 0x2000FFF7
        ├─ __libc_init_array()   — runs .init_array (empty in C)
        └─ main()
```

Two observations that matter later:

**VTOR is untouched.** `SystemInit()` in the ST system file may write `SCB->VTOR` if `USER_VECT_TAB_ADDRESS` is defined; by default it is not, so VTOR keeps its reset value of `0x00000000`, which aliases to `0x08000000` on this part. The bootloader's vector table is therefore live. The bootloader never changes VTOR — even when jumping to the application. Relocating VTOR to `0x08008000` is the *application's* first act in its own `main()`. That division of labour is documented in [§15](#15-the-jump-to-application) and is a genuine coupling between the two images.

**No interrupt is ever enabled.** The bootloader unmasks nothing in the NVIC and never sets `TICKINT` in SysTick. `USART1_ITHandler`, `USART2_ITHandler` and `USART6_ITHandler` exist in `hserial.c` and are fully implemented, but nothing in the bootloader's call graph registers them with the vector table or enables the corresponding NVIC lines. They are dead code carried over from the shared driver stack. Consequence: the bootloader cannot be interrupted, so there are no reentrancy concerns anywhere in this document.

## 13. CheckForAppToRun — The Boot Decision

The very first statement of `main()` is the boot decision. Nothing has been initialised at this point — no clock configuration, no GPIO, no UART — and that is intentional: the less state the bootloader creates before jumping, the less it has to tear down.

```c
int main(){
    CheckForAppToRun();          // may never return

    volatile STD_ReturnType ret = STD_SUCCESS;
    ret = RCC_ConfigureClock(&rcc_pll);
    ...
}
```

The function itself, from [`src/main.c:41`](src/main.c):

```c
void CheckForAppToRun(){
    uint32_t flag = *BOOTLOADER_FLAG_ADDR;      /* 0x2000FFF8 */

    *BOOTLOADER_FLAG_ADDR = BOOT_FLAG_CLEAR;    /* one-shot: clear on read */

    if(flag == BOOTLOADER_APP_MAGIC){           /* 0xDEADBEEF */
        return;                                 /* stay resident */
    }

    if(!App_IsValid()){
        return;                                 /* nothing to jump to */
    }

    uint32_t appSP       = *((volatile uint32_t *)(APP_FLASH_BASE));
    uint32_t MainAppAddr = *((volatile uint32_t *)(APP_FLASH_BASE + 4U));
    mainAppPtr ResetHandler_Address = (mainAppPtr)MainAppAddr;

    __asm volatile ("MSR msp, %0" : : "r" (appSP) : );

    ResetHandler_Address();
}
```

As a decision table:

| Boot flag at `0x2000FFF8` | `App_IsValid()` | Outcome |
|---|---|---|
| `0xDEADBEEF` | *not evaluated* | Stay in bootloader |
| anything else | `1` | Jump to application |
| anything else | `0` | Stay in bootloader |

And as a flow:

```
                    ┌─────────────────────────┐
                    │  read flag @ 0x2000FFF8 │
                    └───────────┬─────────────┘
                                ▼
                    ┌─────────────────────────┐
                    │  write 0 to the flag    │   ← unconditional, always
                    └───────────┬─────────────┘
                                ▼
                        flag == 0xDEADBEEF ?
                          │              │
                        yes│              │no
                          ▼              ▼
                  ┌────────────┐   ┌──────────────────┐
                  │   return   │   │  App_IsValid() ? │
                  │  (resident)│   └────┬────────┬────┘
                  └────────────┘      no│        │yes
                                        ▼        ▼
                              ┌────────────┐  ┌──────────────────┐
                              │   return   │  │  MSR msp, appSP  │
                              │  (resident)│  │  call Reset_H    │
                              └────────────┘  └──────────────────┘
                                                       │
                                                       ▼
                                              never returns
```

### 13.1 A subtlety worth internalising

There is no "boot attempt counter" and no timeout. If the application's vector table is structurally valid but the application itself immediately faults — say, a hard fault in the first line of its `main()` — the sequence is:

```
  reset → bootloader → App_IsValid() passes → jump → app hard-faults
        → HardFault_Handler (default: infinite loop)
        → node is dead until NRST or power cycle
```

The IWDG is not started by the bootloader, so nothing recovers automatically. And because the application starts the IWDG only *after* creating all its tasks, a fault before that point is not covered by the watchdog either. The recovery path is entirely manual: hold the ESP32's enter-bootloader probe against a power cycle, or attach ST-Link.

This is a real availability gap and is recorded in [§57](#57-known-gaps).

## 14. Application Vector-Table Validation

`App_IsValid()` is the only thing standing between "sector 2 contains garbage" and "the core executes garbage". It is deliberately cheap — three integer comparisons, no Flash scan, no CRC.

```c
uint8_t App_IsValid(void){
    uint32_t sp  = *((volatile uint32_t *)(APP_FLASH_BASE));      /* word 0 */
    uint32_t pc  = *((volatile uint32_t *)(APP_FLASH_BASE + 4U)); /* word 1 */

    if(sp < SRAM_BASE || sp > SRAM_END)          return 0U;   /* test 1 */
    if(pc < APP_FLASH_BASE || pc > APP_FLASH_END) return 0U;   /* test 2 */
    if((pc & 1U) == 0U)                           return 0U;   /* test 3 */

    return 1U;
}
```

| # | Test | Rationale | What it catches |
|---|---|---|---|
| 1 | `0x20000000 ≤ sp ≤ 0x20010000` | The initial MSP must point into SRAM | Erased Flash (`0xFFFFFFFF`), all-zero Flash, a half-written image |
| 2 | `0x08008000 ≤ pc ≤ 0x08040000` | The reset handler must live in the app region | An image linked for the wrong base address |
| 3 | `pc & 1 == 1` | Cortex-M is Thumb-only; every function pointer has bit 0 set | A word that happens to be in range but is data, not code |

Test 3 is the strongest of the three for its cost. Roughly half of all random 32-bit values fail it outright, and a linked ARM image *always* passes it. Combined with test 2, a random word has about a `(0x38000 / 0x100000000) × 0.5` ≈ 1 in 60 million chance of slipping through.

Erased Flash reads as `0xFFFFFFFF`, which fails test 1 immediately (`0xFFFFFFFF > 0x20010000`). This is the case that matters in practice: after `0x13` (erase flash) and before a successful `0x14` (write), `App_IsValid()` returns 0 and the bootloader will refuse to jump. That refusal is what makes the erase/write sequence safe to interrupt.

### 14.1 What it does *not* catch

- **A truncated image.** If the host sends the first 40 KB of a 190 KB image and then dies, words 0 and 1 are correct and `App_IsValid()` passes. The bootloader will happily jump into a half-flashed application.
- **A corrupted image with a valid header.** Nothing checksums the body.
- **A stale image.** No version, no build ID, no comparison against what the gateway thinks is flashed.

The GCM tag on each 2 KB chunk ([§37](#37-aes-128-gcm-construction)) does catch corruption *of chunks that were actually received*, and a failed tag aborts the transfer — but it cannot detect chunks that never arrived. The truncation gap is real and is the most consequential item in [§57](#57-known-gaps).

## 15. The Jump to Application

Three instructions' worth of work, with a lot riding on them:

```c
uint32_t appSP       = *((volatile uint32_t *)(APP_FLASH_BASE));
uint32_t MainAppAddr = *((volatile uint32_t *)(APP_FLASH_BASE + 4U));
mainAppPtr ResetHandler_Address = (mainAppPtr)MainAppAddr;

__asm volatile ("MSR msp, %0" : : "r" (appSP) : );

ResetHandler_Address();
```

```
   Before                              After
   ──────                              ─────
   MSP = 0x2000FFF8 (bootloader)       MSP = app's _estack (also 0x2000FFF8)
   PC  = inside CheckForAppToRun       PC  = app Reset_Handler | 1
   VTOR= 0x00000000 → 0x08000000       VTOR= unchanged (still bootloader's!)
   .data/.bss = bootloader's           .data/.bss = about to be re-initialised
                                             by the app's own Reset_Handler
```

**What is deliberately *not* done, and why it is safe here:**

| Typical jump-to-app step | Done? | Why it is safe to omit |
|---|---|---|
| `__disable_irq()` | No | No interrupt is ever enabled by the bootloader |
| De-init every peripheral | No | `CheckForAppToRun()` runs *before* any peripheral is touched |
| Reset RCC to defaults | No | Same — the clock is still at HSI 16 MHz |
| Clear pending NVIC IRQs | No | Nothing was ever pended |
| Set `SCB->VTOR` | No | The application sets it itself, first line of its `main()` |
| Set PSP / switch to MSP | No | Never left MSP |

The ordering discipline — put `CheckForAppToRun()` first, before any `RCC_*` or `GPIO_*` call — is what buys all of these omissions. It is not an accident and it should not be relaxed. If someone later moves an initialisation call above `CheckForAppToRun()`, the jump becomes unsafe in a way that manifests as intermittent, hard-to-attribute faults inside the application.

**The VTOR handoff is a contract between the two images.** The bootloader relies on this line in the application's `main()`:

```c
*((volatile uint32_t *)0xE000ED08U) = 0x08008000U;
```

Until that executes, every exception the application takes would be dispatched through the *bootloader's* vector table. In practice the window is a handful of instructions and no interrupt is enabled yet on either side, so the window is not exploitable — but the coupling is real, undocumented in the code, and would break silently if someone removed the VTOR line while "cleaning up" the application. See [`../STM32_OVERVIEW.md`](../STM32_OVERVIEW.md) for the full list of cross-image contracts.

**Stack coincidence.** Both linker scripts compute `_estack = ORIGIN(RAM) + LENGTH(RAM)` with the same `64K - 8`, so the application's initial MSP is the same value the bootloader was already using. The `MSR msp` is therefore a no-op *today*. It is still correct to do it — the application is free to change its own `_estack`, and the whole point of reading word 0 is to honour whatever it chose.

## 16. Peripheral Bring-Up When Staying Resident

If `CheckForAppToRun()` returns, the bootloader initialises itself for service:

```c
volatile STD_ReturnType ret = STD_SUCCESS;
ret = RCC_ConfigureClock(&rcc_pll);                       /* 84 MHz, §6 */
if(ret == STD_SUCCESS){
    ret = RCC_ControlPeripheral(RCC_GPIOA | RCC_GPIOB | RCC_GPIOC,
                                RCC_PERIPHERAL_ENABLE);
}

ret = SYSTICK_Init(SYSTICK_CLOCK_SOURCE_PLL_MAX);         /* §51 */
ret = LED_Init();                                          /* §52 */
ret = BL_Init(SYSTICK_CLOCK_SOURCE_PLL_MAX);               /* UART1 + CRC */
ret = LED_SetState(LED_0, LED_HIGH);
SYSTICK_DelayMS(1000);
ret = LED_SetState(LED_0, LED_LOW);

while(1){
    ret = BL_FetchHostCommand();
}
```

Order dependencies:

```
  RCC_ConfigureClock       ─── must precede everything (sets 84 MHz + latency)
        │
        ├─► RCC_ControlPeripheral(GPIOA|GPIOB|GPIOC)
        │        │
        │        └─► LED_Init         needs GPIOA clock (PA0, PA1)
        │        └─► BL_Init → HSerial_Init → UART_Init → GPIO_Init(PA9/PA10)
        │                                                needs GPIOA clock
        │
        └─► SYSTICK_Init          needs the final clock rate to compute ticks/ms
                 │
                 └─► BL_Init(clockSource)  passes the same enum down to
                     UART_Init so it can compute BRR from 84 MHz
```

The `RCC_GPIOA | RCC_GPIOB | RCC_GPIOC` bitwise-OR works because `RCC_Peripheral_t` encodes the bus in bits [31:30] and the enable-bit position in the low bits — OR-ing three AHB1 peripherals yields a value whose bus field is still AHB1 and whose low bits are the union of the three enable bits. This only works *within one bus*; OR-ing an AHB1 and an APB1 peripheral would corrupt the bus field. The header comments this ("Can send multiple parameters only for the same bus"). GPIOB and GPIOC are enabled but never used by the bootloader.

`ret` is `volatile` and every assignment overwrites the previous one. Nothing checks it after the first `if`. This is debugger-oriented code: with `build_type = debug`, `ret` is guaranteed to live in memory and can be watched while single-stepping. As error handling it is inert — a failed `LED_Init()` or `BL_Init()` proceeds silently into the command loop.

## 17. The One-Second LED Signature

```c
ret = LED_SetState(LED_0, LED_HIGH);
SYSTICK_DelayMS(1000);
ret = LED_SetState(LED_0, LED_LOW);
```

PA0 is driven high for one second, then low, then the command loop starts. This is the bootloader's only unsolicited output and it is genuinely useful on the bench:

| Observed on PA0 after reset | Interpretation |
|---|---|
| One clean 1 s pulse, then dark | Bootloader is resident and waiting for commands |
| No pulse at all | Either the app was launched (normal), or the bootloader died before `LED_Init` |
| Pulse much shorter/longer than 1 s | `SYSTICK_Init` computed the wrong tick rate → the clock is not at 84 MHz → HSE probably failed to start ([§6.2](#62-timeouts-are-loop-counts-not-milliseconds)) |

That third row makes the LED an accidental clock-integrity check. A pulse of roughly 5.25 s instead of 1 s would indicate the core is running at 16 MHz HSI while SysTick believes it is at 84 MHz — exactly the HSE-timeout failure mode.

`LED_1` (PA1) is configured by `LED_Init()` but never driven. In the test build ([§42](#42-the-self-test-harness)) `LED_0` blinks at 2.5 Hz forever on crypto self-test success.
---

# Part IV — Host Protocol

## 18. Protocol Overview

The bootloader speaks a strictly half-duplex, host-driven, one-command-at-a-time protocol on UART1. The STM32 never initiates; every byte it sends is a direct response to something the ESP32 sent.

```
   ESP32 (host)                              STM32 (bootloader)
        │                                            │
        │  ── command packet ──────────────────────► │
        │                                            │  verify CRC32
        │  ◄──────────────────────────── ACK / NACK ─│
        │                                            │  execute
        │  ◄────────────────── 0xEE + reply bytes ── │
        │                                            │
        │  (next command)                            │
```

Design properties:

| Property | Value |
|---|---|
| Framing (host → STM32) | Leading length byte |
| Framing (STM32 → host) | Leading start byte `0xEE` |
| Integrity (host → STM32) | Trailing CRC32, little-endian, over the whole packet minus the CRC |
| Integrity (STM32 → host) | **None** |
| Flow control | None (no RTS/CTS, no XON/XOFF) |
| Retransmission | None on either side |
| Concurrency | None — strictly one command in flight |
| Session state | Only the OTA engine's state ([§34](#34-state-machine-reference)) |

The asymmetry in integrity is deliberate and reasonable: a corrupted *command* could erase the wrong sector or write garbage into Flash, so it must be checked; a corrupted *reply* only misleads the gateway's logging, and the gateway's next command will reveal the true state.

## 19. Wire Format

### 19.1 Host → STM32 command packet

```
 ┌───────┬────────┬─────────────────────────────┬───────────────────────┐
 │  LEN  │  CMD   │        parameters           │        CRC32          │
 │ 1 byte│ 1 byte │       LEN-5 bytes           │       4 bytes, LE     │
 └───────┴────────┴─────────────────────────────┴───────────────────────┘
   ▲                                                        ▲
   │                                                        │
   └─ number of bytes that FOLLOW this one                  └─ computed over
      (i.e. total packet size = LEN + 1)                       bytes [0 .. LEN-4]
                                                               — INCLUDING the
                                                                 LEN byte itself
```

The CRC coverage is the detail most likely to trip up a reimplementation. From `Bootloader_GetVersion`:

```c
uint16_t packetLen  = rxBuffer[0] + 1;                              /* total size */
uint32_t receivedCRC = *((uint32_t *)((rxBuffer + packetLen) - CRC_TYPE_SIZE_BYTE));
if(CRC_VERIFICATION_PASSED ==
       Bootloader_CRCVerify((uint8_t *)&rxBuffer[0], packetLen - 4, receivedCRC)) { ... }
```

`packetLen - 4` bytes starting at `rxBuffer[0]` — so the length byte is inside the CRC. The gateway matches this exactly; in `Send_CMD_Get_Version` it computes `calculateCRC32(packet, 2)` over `{LEN, CMD}` for a 6-byte packet where `LEN = 5`, and `5 + 1 - 4 = 2`. ✓

The CRC is stored little-endian because it is written and read through a `uint32_t *` cast on both ends, and both ends are little-endian machines. There is no explicit byte-order code anywhere, which is fine here but is an unstated portability assumption.

### 19.2 STM32 → host reply

Two shapes:

```
  ACK / NACK  (bare, no start byte)
  ┌──────┐
  │ 0xAA │   BL_SEND_ACK   — CRC verified
  └──────┘
  ┌──────┐
  │ 0x00 │   BL_SEND_NACK  — CRC mismatch
  └──────┘

  Data reply (always prefixed)
  ┌──────┬──────────────────────┐
  │ 0xEE │  N payload bytes     │
  └──────┴──────────────────────┘
    ▲
    └─ BL_REPLAY_START_BYTE
```

From `Bootloader_SendDataToHost`:

```c
static void Bootloader_SendDataToHost(uint8_t *Host_Buffer, uint32_t Data_Len){
    txBuffer[0] = BL_REPLAY_START_BYTE;              /* 0xEE */
    memcpy(&txBuffer[1], Host_Buffer, Data_Len);
    hserialConfig.txBuffer->buffer.length = Data_Len + 1;
    HSerial_SendBuffer(&hserialConfig, BL_UART_DELAY * 1000UL);
}
```

`txBuffer` is `uint8_t txBuffer[3]`. With the start byte occupying slot 0, **a reply payload may be at most 2 bytes.** The longest reply the protocol actually sends is the 2-byte version from `0x10`, so this is exactly saturated — no headroom at all. Adding a 3-byte reply to any command would overflow `txBuffer` into whatever `.bss` object the linker placed next. Recorded in [§57](#57-known-gaps).

The gateway's receiver mirrors this: `ReceiveReplayFromBootloader` first spins until it sees `0xEE` (`WaitForStartByte`), then reads exactly `packetLength` bytes.

### 19.3 The one exception: the enter-bootloader probe

The `0xAA` probe does not follow the length-prefixed format at all. It is a fixed 2-byte sequence with no CRC. See [§21](#21-0xaa--enter-bootloader-probe).

## 20. The Command Dispatch Loop

`BL_FetchHostCommand()` is called in a tight `while(1)` from `main()`. Each invocation is one complete command exchange, or one timeout.

```c
STD_ReturnType BL_FetchHostCommand(void){
    STD_ReturnType status = STD_SUCCESS;
    uint8_t Data_Length = 0;

    hserial_rxBuffer.buffer.data = rxBuffer;          /* rewind the descriptor */
    memset(rxBuffer, 0, BL_HOST_BUFFER_RX_LENGTH);

    /* --- Phase 1: read exactly one byte (the length / probe byte) --- */
    hserialConfig.rxBuffer->buffer.length = 1;
    status = HSerial_ReceiveBuffer(&hserialConfig, BL_UART_DELAY);

    if(rxBuffer[0] == 0 ||
       (rxBuffer[0] > BL_HOST_BUFFER_RX_LENGTH - 1 && rxBuffer[0] != ENTER_BOOTLOADER_CMD)){
        return STD_ERROR;                             /* implausible length */
    }

    if(status == STD_SUCCESS){
        if(ENTER_BOOTLOADER_CMD == rxBuffer[0]){      /* 0xAA — special case */
            ... see §21 ...
        }

        /* --- Phase 2: read the rest of the packet --- */
        Data_Length = rxBuffer[0];
        hserial_rxBuffer.buffer.data = rxBuffer + 1;   /* append after the length */
        hserialConfig.rxBuffer->buffer.length = Data_Length;
        status = HSerial_ReceiveBuffer(&hserialConfig, BL_UART_DELAY * 100000UL);

        if(status != STD_SUCCESS){
            status = STD_NACK;
        }
        else{
            switch(rxBuffer[1]){                       /* --- Phase 3: dispatch --- */
                case BL_GET_VERSION:    status = Bootloader_GetVersion();  status = STD_CMD_FINISHED; break;
                case BL_JUMP_TO_ADDR_CMD: Bootloader_JumpToApp();          status = STD_CMD_FINISHED; break;
                case BL_FLASH_ERASE_CMD: status = Bootloader_EraseFlash(); status = STD_CMD_FINISHED; break;
                case BL_MEM_WRITE_CMD:   Bootloader_MemoryWrite();         status = STD_CMD_FINISHED; break;
            }
        }
    }
    return status;
}
```

```
   ┌────────────────────────────────────────────────────────────┐
   │  memset rxBuffer[140] = 0;  descriptor → &rxBuffer[0]      │
   └──────────────────────────┬─────────────────────────────────┘
                              ▼
              ┌──────────────────────────────────┐
              │  RX 1 byte, timeout 1000 spins   │
              └──────────────┬───────────────────┘
                             ▼
                 rxBuffer[0] == 0  ────────────► return STD_ERROR
                 rxBuffer[0] > 139 && != 0xAA ─► return STD_ERROR
                             │
                             ▼
                 rxBuffer[0] == 0xAA ? ──yes──► [§21] probe path, return
                             │no
                             ▼
              ┌──────────────────────────────────────┐
              │  RX rxBuffer[0] bytes into &rx[1]    │
              │  timeout 100,000,000 spins           │
              └──────────────┬───────────────────────┘
                             ▼
                   status != SUCCESS ──────────► return STD_NACK  (silent!)
                             │
                             ▼
                    switch (rxBuffer[1])
                     ├─ 0x10 → Bootloader_GetVersion()
                     ├─ 0x12 → Bootloader_JumpToApp()
                     ├─ 0x13 → Bootloader_EraseFlash()
                     ├─ 0x14 → Bootloader_MemoryWrite()
                     └─ else → (nothing; status stays STD_SUCCESS)
```

### 20.1 Points worth noting

**The buffer descriptor is rewound every call.** `hserial_rxBuffer.buffer.data` is mutated to `rxBuffer + 1` in phase 2 and would stay there; resetting it at the top of every call is what makes the loop repeatable. This is easy to miss and would produce a "works once, then drifts" bug if removed.

**The plausibility filter is the only defence against a desync.** If the receiver falls out of frame — say the gateway resets mid-packet — the next byte read as a "length" is arbitrary. Values of `0` or `> 139` (except `0xAA`) are rejected immediately, which resynchronises within one byte. Values in `1..139` are accepted and the bootloader will block waiting for that many bytes, then fail CRC and NACK. Either way it recovers, but the second path can stall for the phase-2 timeout.

**A NACK from phase 2 is silent.** When `HSerial_ReceiveBuffer` times out mid-packet, `status` is set to `STD_NACK` and returned — but **nothing is transmitted**. The gateway is left waiting for its own 1000 ms ACK timeout. The return code goes nowhere: `main()` assigns it to `ret` and discards it.

**Unknown commands are silently ignored.** The `switch` has no `default`. Command `0x11` (`BL_GET_PROTECTION_LEVEL`, defined in the header but never implemented) falls through with no reply at all.

**`status` is overwritten immediately after each handler.** `status = Bootloader_GetVersion(); status = STD_CMD_FINISHED;` — the handler's return value is computed and thrown away on the next line. Every path reports `STD_CMD_FINISHED` regardless of what actually happened. Since nothing consumes the value, this is cosmetic, but it means the dispatch loop has no notion of success or failure.

## 21. 0xAA — Enter-Bootloader Probe

This is the handshake that lets the gateway ask "who am I talking to?" without knowing whether the application or the bootloader is currently running.

```c
if(ENTER_BOOTLOADER_CMD == rxBuffer[0]){        /* 0xAA */
    uint8_t data = WE_ARE_IN_BOOTLOADER;        /* 0xFB */

    /* Dummy read — consume the second byte of the probe */
    hserial_rxBuffer.buffer.data = rxBuffer + 1;
    hserialConfig.rxBuffer->buffer.length = 1;
    status = HSerial_ReceiveBuffer(&hserialConfig, BL_UART_DELAY*1000);

    Bootloader_SendDataToHost(&data, 1);        /* → 0xEE 0xFB */
    return status;
}
```

The gateway sends `{0xAA, 0xEB}` and interprets the reply:

```
   Case A — the APPLICATION is running
   ───────────────────────────────────
     ESP32                      STM32 app (Thread5_BootloaderRx, priority 4)
       │  0xAA 0xEB  ─────────────► state machine matches
       │  ◄───────────  0xEE 0xAA   ACK via UART_Transmit_Polling
       │                            *(0x2000FFF8) = 0xDEADBEEF
       │                            SCB->AIRCR = SYSRESETREQ
       │                            ═══ reset ═══
       │                            bootloader stays resident (flag set)
       │
       │  gateway sees 0xAA = BOOTLOADER_ACKNOWLEDGE
       │  → prints "Entering Bootloader..."


   Case B — the BOOTLOADER is already running
   ──────────────────────────────────────────
     ESP32                      STM32 bootloader
       │  0xAA 0xEB  ─────────────► rxBuffer[0]==0xAA, dummy-read 0xEB
       │  ◄───────────  0xEE 0xFB   WE_ARE_IN_BOOTLOADER
       │
       │  gateway sees 0xFB = BOOTLOADER_ALREADY_IN
       │  → prints "We are already in Bootloader"
```

The two firmwares answer with **different bytes on purpose**: `0xAA` means "I was the application and I am now resetting into the bootloader", `0xFB` means "I was already the bootloader; no reset happened". The gateway distinguishes them because the first case implies a reset delay before the node is ready for the next command, and the second does not.

Note that the second byte of the probe (`0xEB`) is read and discarded by the bootloader — it does not check it. The application *does* check it. So sending `{0xAA, 0x00}` to a resident bootloader still elicits `0xEE 0xFB`, while the same sequence to a running application would be ignored. Minor asymmetry, no practical impact.

### 21.1 Why `0xAA` cannot be a length byte

`0xAA` = 170, which is greater than `BL_HOST_BUFFER_RX_LENGTH - 1` = 139. The plausibility filter would normally reject it — hence the explicit `&& rxBuffer[0] != ENTER_BOOTLOADER_CMD` carve-out in the filter condition. This is the only reason the probe byte can be `0xAA`: it is chosen to be a value the length filter would otherwise reject, so it can never be confused with a real packet length. Choosing `0x05` as the probe byte would have been ambiguous with a 5-byte packet.

The same value `0xAA` is also `BL_SEND_ACK`. On this half-duplex link that never causes confusion — a byte's meaning is fully determined by direction — but it is worth knowing when reading a logic-analyser capture.

## 22. 0x10 — Get Version

**Packet** (6 bytes): `05 10 <crc32 ×4>`
**Reply**: `AA` (ACK) then `EE 00 00`

```c
static STD_ReturnType Bootloader_GetVersion(){
    uint16_t packetLen   = rxBuffer[0] + 1;
    uint32_t receivedCRC = *((uint32_t *)((rxBuffer + packetLen) - CRC_TYPE_SIZE_BYTE));

    if(CRC_VERIFICATION_PASSED ==
           Bootloader_CRCVerify((uint8_t *)&rxBuffer[0], packetLen - 4, receivedCRC)){
        Bootloader_SendACK();

        /* Version is now managed by ESP32 LittleFS — report 0 from bootloader */
        uint16_t version = 0;

        Bootloader_SendDataToHost((uint8_t *)&version, 2);
        status = STD_CMD_FINISHED;
    }
    else{
        Bootloader_SendNACK();
    }
    return status;
}
```

The command is a **liveness probe, not a version query**. It always answers `0x0000`.

This is intentional, and the reasoning is worth spelling out because it looks like a stub. The STM32 has no non-volatile store other than its own Flash. Writing a version into Flash would mean either (a) burning a whole 16 KB sector for two bytes, or (b) reserving space inside the application image, which the bootloader would then have to know the layout of. Meanwhile the gateway already has a filesystem (LittleFS) and already knows what it flashed, because *it* is the one that received the image over MQTT. So the gateway keeps `/update/version.bin` and the STM32 keeps nothing.

The gateway's handler makes this explicit:

```c
/* The reply bytes are a liveness handshake only -- the STM32 has just a
 * bootloader, an app and non-persistent RAM, so it keeps no version of
 * its own and always answers 0 0. This gateway is the system of record. */
```

After receiving `EE 00 00`, the gateway reads its own `readVersion(&major, &minor)` and reports *that*. A `0.0` result there means "nothing has ever been flashed through this gateway" — which is not the same as "the STM32 has no application", since an ST-Link flash bypasses the gateway entirely.

**Consequence:** there is no way for anyone to interrogate the STM32 about what is actually running on it. If the gateway's LittleFS is wiped, the mapping is lost permanently.

## 23. 0x12 — Jump to Application

**Packet** (8 bytes): `07 12 <major> <minor> <crc32 ×4>`
**Reply**: `AA` (ACK) then `EE A0` (app valid, resetting) or `EE 00` (no app)

```c
static STD_ReturnType Bootloader_JumpToApp(){
    packetLength = rxBuffer[0] + 1;
    receivedCRC  = *((uint32_t *)((rxBuffer + packetLength) - CRC_TYPE_SIZE_BYTE));

    if(CRC_VERIFICATION_PASSED == Bootloader_CRCVerify(&rxBuffer[0], packetLength - 4, receivedCRC)){
        Bootloader_SendACK();

        if(App_IsValid()){
            appExists = 0xA0;
            Bootloader_SendDataToHost(&appExists, 1);      /* EE A0 */

            /* No magic word needed — CheckForAppToRun() defaults to jumping
             * to app when no BOOTLOADER_APP_MAGIC is found in RAM */
            *((volatile uint32_t *)0xE000ED0C) = (0x5FA << 16) | (1 << 2);   /* SYSRESETREQ */
        }
        else{
            appExists = 0;
            Bootloader_SendDataToHost(&appExists, 1);      /* EE 00 */
        }
    }
    else{
        Bootloader_SendNACK();
    }
}
```

### 23.1 It resets rather than jumping

Despite the name, this command does **not** call `CheckForAppToRun()`'s jump path. It performs a full system reset via `AIRCR.SYSRESETREQ` and lets the *next* boot make the decision.

```
   0x12 arrives
        │
        ├─ CRC ok? ──no──► NACK, done
        │  yes
        ├─ ACK (0xAA)
        │
        ├─ App_IsValid()? ──no──► reply EE 00, stay resident
        │  yes
        ├─ reply EE A0
        │
        └─ SCB->AIRCR = 0x05FA0004
                │
                ▼
           ═══ system reset ═══
                │
                ▼
           bootloader Reset_Handler
                │
                ▼
           CheckForAppToRun()
                │  flag reads 0 (it was cleared on the previous boot,
                │  and this command never set it)
                │  App_IsValid() passes
                ▼
           jump to application
```

Resetting is strictly better than jumping in-place here, because by this point the bootloader *has* configured the clock to 84 MHz, enabled three GPIO ports, configured UART1 and enabled the CRC peripheral. Jumping directly would hand the application a machine in an unexpected state. A reset returns everything to defaults and re-runs the clean early-jump path from [§15](#15-the-jump-to-application).

The comment in the code — "No magic word needed — `CheckForAppToRun()` defaults to jumping to app" — is the load-bearing observation. The flag was already cleared during *this* boot's `CheckForAppToRun()`, so it is guaranteed zero.

### 23.2 The reply is optimistic

`0xA0` is sent **before** the reset, and the gateway reads it as `BOOTLOADER_APP_IS_RUNNING`:

```c
if(packet[0] == BOOTLOADER_APP_IS_RUNNING){
    in_bootloader_mode = false;
    printk("[Bootloader] Firmware is now Running\n");
    PublishBootloaderStatus("Firmware is now Running");
    ota_secoc_send_running_status(OTA_SLOT_STM32);
}
```

What `0xA0` actually asserts is narrower: *"the vector table at 0x08008000 passes three structural checks and I am about to reset."* It does not assert that the application booted, initialised, or is producing telemetry. If the application faults immediately, the gateway has already published "Firmware is now Running" and already sent the SecOC RUNNING notification to the instrument cluster.

The observable ground truth is different and arrives later: telemetry frames resuming on UART1. The gateway does not currently wait for that before declaring success. This is a known and accepted simplification — the alternative (waiting for the first heartbeat, ~5 s) would complicate the gateway's state machine — but it means the "Firmware is now Running" message should be read as "handoff initiated", not "handoff confirmed".

### 23.3 The version bytes are ignored

The gateway populates `packet[2]` and `packet[3]` with the major/minor it read from LittleFS:

```c
readVersion(&major, &minor);
packet[0] = 7;
packet[1] = BOOTLOADER_CMD_JUMP_TO_APP;
packet[2] = major;
packet[3] = minor;
```

The bootloader reads neither. They are covered by the CRC, so they cannot be corrupted silently, but they have no effect. They appear to be a vestige of an earlier design in which the bootloader stored the version. Removing them would require a coordinated change on both sides (the packet length and CRC would shift), so they remain.

## 24. 0x13 — Erase Flash

**Packet** (6 bytes): `05 13 <crc32 ×4>`
**Reply**: `AA` (ACK) then `EE E1` (success) or `EE 00` (failure)

```c
static STD_ReturnType Bootloader_EraseFlash(){
    if(CRC_VERIFICATION_PASSED == Bootloader_CRCVerify(&rxBuffer[0], packetLength - 4, receivedCRC)){
        Bootloader_SendACK();

        status = FLASH_Erase(FLASH_SECTOR_NUMBER_2);
        status = FLASH_Erase(FLASH_SECTOR_NUMBER_3);
        status = FLASH_Erase(FLASH_SECTOR_NUMBER_4);
        status = FLASH_Erase(FLASH_SECTOR_NUMBER_5);

        if(STD_SUCCESS == status){
            eraseStatus = SUCCESSFUL_ERASE;      /* 0xE1 */
            Bootloader_SendDataToHost(&eraseStatus, 1);
        }
        else{
            eraseStatus = UNSUCCESSFUL_ERASE;    /* 0x00 */
            Bootloader_SendDataToHost(&eraseStatus, 1);
        }
    }
    else{
        Bootloader_SendNACK();
    }
}
```

Erases the four application sectors. Sectors 0 and 1 — the bootloader itself — are never touched, which is what makes this command safe to issue at any time.

### 24.1 Only the last erase result is checked

`status` is reassigned by each of the four calls. If sector 2 fails and sector 5 succeeds, `status == STD_SUCCESS` and the bootloader reports `0xE1`. The correct form is an accumulating check:

```c
status  = FLASH_Erase(FLASH_SECTOR_NUMBER_2);
if (status == STD_SUCCESS) status = FLASH_Erase(FLASH_SECTOR_NUMBER_3);
...
```

In practice `FLASH_Erase` fails only on a write-protection error or a busy controller, neither of which is transient in this context — so the four calls either all succeed or all fail. The bug is real but its window is narrow. Logged in [§57](#57-known-gaps).

### 24.2 Timing, and why the gateway waits 5 seconds

STM32F4 sector-erase times at 2.7–3.6 V, from the datasheet (typical / maximum):

| Sector | Size | Typical | Maximum |
|---|---|---|---|
| 2 | 16 KB | 250 ms | 400 ms |
| 3 | 16 KB | 250 ms | 400 ms |
| 4 | 64 KB | 550 ms | 700 ms |
| 5 | 128 KB | 1000 ms | 1600 ms |
| **Total** | **224 KB** | **≈ 2.05 s** | **≈ 3.10 s** |

`FLASH_Erase` spins on `SR.BSY` with **no timeout at all**:

```c
FLASH->CR |= (1 << 16);            /* STRT */
while(FLASH->SR & (1 << 16));      /* unbounded */
```

So the whole command blocks the bootloader for roughly two to three seconds with interrupts irrelevant (there are none) and the UART unattended. Any bytes the gateway sends during this window are lost — the STM32's UART has a 1-byte receive register and no DMA armed, so the second byte would overrun.

The gateway accounts for this by using a longer reply timeout for erase specifically:

```c
ReceiveReplayFromBootloader(1, 5000);   /* 5 s, versus 1000 ms elsewhere */
```

5 s against a 3.1 s worst case is a 1.6× margin. Adequate, but not generous — a part at the hot end of its temperature range, or a Flash cell near end-of-life, would eat into it.

### 24.3 The erase is not idempotent with respect to the OTA engine

`Bootloader_EraseFlash()` does not call `OTA_Init()`. If a previous transfer left the engine in `OTA_STATE_COMPLETE` or `OTA_STATE_ERROR`, erasing does not reset it, and the subsequent `0x14` packets will be rejected. See [§35](#35-engine-failure-modes) — this is the mechanism behind the "second OTA in one session always fails" defect.

## 25. 0x14 — Memory Write

This is the workhorse. One packet carries up to 64 bytes of ciphertext; 32 packets fill one 2 KB chunk; a 190 KB image is ~3,000 packets.

**Packet** (11 to 74 bytes):

```
 offset:   0     1      2       3        4      5      6       7 .. 7+N-1     ...
         ┌────┬──────┬───────┬────────┬──────┬──────┬───────┬───────────────┬─────────┐
         │LEN │ 0x14 │ chunk │ pkt in │ resv │ resv │ payld │   payload     │  CRC32  │
         │    │      │ index │ chunk  │  =0  │  =0  │  len  │   N bytes     │ 4B LE   │
         └────┴──────┴───────┴────────┴──────┴──────┴───────┴───────────────┴─────────┘
          N+10                0..31            1..64          ciphertext

   LEN = 6 + N + 4 = N + 10           total packet size = LEN + 1 = N + 11
   CRC32 covers bytes [0 .. 6+N]  (that is, 7+N bytes, including LEN)
```

**Reply**: `AA` (ACK) then `EE E2` (written) or `EE 00` (failed)

The handler:

```c
static STD_ReturnType Bootloader_MemoryWrite(void){
    if(CRC_VERIFICATION_PASSED == Bootloader_CRCVerify(&rxBuffer[0], packetLength - 4, receivedCRC)){
        Bootloader_SendACK();

        uint8_t chunk_idx    = rxBuffer[2];
        uint8_t pkt_in_chunk = rxBuffer[3];
        uint8_t payload_len  = rxBuffer[6];

        /* ---- End-of-transfer marker: FF FF ... len 0 ---- */
        if(chunk_idx == 0xFF && pkt_in_chunk == 0xFF && payload_len == 0){
            if(OTA_GetState() == OTA_STATE_IDLE){
                OTA_Init();              /* late init if the first packet was missed */
            }
            if(OTA_Finish()){
                Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_PASSED;   /* 0xE2 */
            } else {
                Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;   /* 0x00 */
            }
            Bootloader_SendDataToHost(&Flash_Payload_Write_Status, 1);
            return status;
        }

        /* ---- Normal data packet ---- */
        if(OTA_GetState() == OTA_STATE_IDLE){
            OTA_Init();
        }

        bool ok = OTA_ReceivePacket(&rxBuffer[7], payload_len);

        Flash_Payload_Write_Status = ok ? FLASH_PAYLOAD_WRITE_PASSED
                                        : FLASH_PAYLOAD_WRITE_FAILED;
        Bootloader_SendDataToHost(&Flash_Payload_Write_Status, 1);
    }
    else{
        Bootloader_SendNACK();
    }
}
```

### 25.1 The chunk and packet indices are decorative

`chunk_idx` and `pkt_in_chunk` are read out of the packet — and then used only to detect the `FF FF` end marker. The OTA engine maintains its **own** `ota.chunk_index`, incremented internally on every successful decrypt, and that internal counter is what feeds the nonce:

```c
/* firmware_flashing.c */
bool ok = AES_GCM_DecryptChunk(ota.chunk_index, cipher, cipher_len, tag, plaintext_buffer);
...
ota.chunk_index++;
```

This is a significant design property with two consequences:

**Positive:** the wire indices cannot desynchronise the crypto. Even if the host mislabels a packet, the decryption uses the receiver's own count.

**Negative:** there is no way to detect a *dropped* packet. If packet 17 of chunk 3 is lost, packets 18–31 shift down by 64 bytes, the accumulator fills 64 bytes short of 2048, and the next packet's 64 bytes complete the buffer with the wrong contents. The GCM tag then fails and the whole transfer aborts — so corruption is *detected*, but only after up to 2 KB of wasted transfer, and the diagnosis ("tag mismatch") does not point at the real cause ("packet loss").

Since every packet is individually ACKed and the gateway aborts on the first non-`0xE2` reply, packet loss would have to coincide with a lost reply to go unnoticed. The window is small but the indices sitting unused in the packet is a missed opportunity: a two-line check (`if (pkt_in_chunk != expected) → error`) would turn a confusing tag failure into a precise one.

### 25.2 The `0xFF 0xFF` end marker

The gateway sends it **only** when the last data packet was exactly 64 bytes:

```c
if(byteCounter == 64){
    /* Last packet was full 64B, STM32 needs explicit finish */
    packet[2] = 0xFF; packet[3] = 0xFF; packet[6] = 0;
    ...
}
else {
    printk("[Bootloader] Last packet was %uB, STM32 auto-detects end\n", byteCounter);
}
```

This mirrors the engine's auto-finish rule ([§32](#32-two-ways-a-transfer-ends)): a short packet is self-announcing, a full packet is not.

Note the marker's `LEN` is `10`, so the packet is `10 14 FF FF 00 00 00 <crc32>` — 11 bytes, CRC over the first 7.

### 25.3 A `0x00` reply aborts everything

The gateway breaks out of its send loop on the first failure:

```c
if(packet[0] == BOOTLOADER_WRITE_SUCCEEDED){ ... }
else{
    printk("[Bootloader] Write Failed\n");
    PublishBootloaderStatus("Write Failed");
    isFailed = 1;
    break;
}
```

There is no retry of the failed packet and no resume. The whole transfer is abandoned, and — because the erase already happened — the node is left with a partially written, unbootable application. Recovery requires restarting the entire OTA from `0x13`.

## 26. CRC32 Verification

Every length-prefixed command is CRC-checked. The implementation is unusual and the reason is worth understanding, because it is the single most likely place for a reimplementation to go wrong.

```c
static uint8_t Bootloader_CRCVerify(uint8_t *data, uint8_t dataLen, uint32_t receivedCRC){
    uint32_t calculatedCRC = 0;

    for(uint8_t i = 0; i < dataLen; i++){
        uint32_t crcBuf = (uint32_t)data[i];       /* zero-extend byte to 32 bits */
        CRC_Accumulate(&crcBuf, 1, &calculatedCRC);
    }
    CRC_Reset();

    return (calculatedCRC == receivedCRC) ? CRC_VERIFICATION_PASSED
                                          : CRC_VERIFICATION_FAILED;
}
```

### 26.1 What the STM32 CRC unit actually computes

The STM32F4 CRC peripheral is fixed-function:

| Parameter | Value | Configurable? |
|---|---|---|
| Polynomial | `0x04C11DB7` | No |
| Initial value | `0xFFFFFFFF` | No (on F4) |
| Input width | 32 bits per write to `DR` | No |
| Input reflection | None | No |
| Output reflection | None | No |
| Final XOR | None | No |

Because the register is 32 bits wide, feeding it one byte means feeding it that byte **followed by 24 zero bits**. That is not CRC-32/MPEG-2 over the byte stream — it is a different function entirely.

### 26.2 Why the ESP32's software CRC matches

The gateway computes the CRC in software, and its inner loop runs **32 iterations per byte**, not 8:

```c
/* ESP32 side, Application/src/FRAME.c mirrors the same algorithm */
crc ^= (uint32_t)byte_in;
for (bit = 0U; bit < 32U; bit++) {
    if ((crc & 0x80000000U) != 0U) crc = (crc << 1U) ^ 0x04C11DB7U;
    else                            crc = crc << 1U;
}
```

Thirty-two shift-and-conditional-XOR steps per byte is exactly what the hardware unit does when handed a zero-extended byte. The two implementations agree bit-for-bit *because* the software one was written to imitate the hardware one, not because either implements a standard.

The application's `FRAME.c` documents this explicitly and warns against "fixing" it:

> *"The inner loop runs 32 iterations to match the teammate's ESP32 `calculateCRC32()` reference exactly. This produces a result that is mathematically different from standard MPEG-2 CRC32... DO NOT CHANGE TO 8 ITERATIONS — both ends of the link must stay in sync."*

Also note: `crc ^= (uint32_t)byte_in` XORs the byte into the **low** 8 bits, whereas conventional MSB-first CRC-32 implementations XOR into the high byte. Again, this is imitating what the hardware does with a zero-extended write.

### 26.3 The reset ordering

`CRC_Reset()` is called **after** accumulation, not before:

```c
for(...) { CRC_Accumulate(...); }
CRC_Reset();
```

`CRC_Accumulate` does not reset — it appends to whatever state the unit holds. So the sequence works only because every call site leaves the unit reset for the next one. `CRC_Init()` (called once from `BL_Init`) enables the clock but does not reset, and the peripheral's `DR` powers up at `0xFFFFFFFF`, so the *first* verification also starts clean.

This is correct but fragile: any future code path that calls `CRC_Accumulate` without a matching `CRC_Reset()` would silently poison every subsequent verification. Resetting *before* accumulating — as `CRC_Calculate()` already does — would be self-contained. Logged in [§57](#57-known-gaps).

### 26.4 `dataLen` is a `uint8_t`

```c
static uint8_t Bootloader_CRCVerify(uint8_t *data, uint8_t dataLen, uint32_t recievedCRC);
```

The maximum `dataLen` is `packetLen - 4` = `rxBuffer[0] + 1 - 4` ≤ `139 + 1 - 4` = 136, which fits. But the length filter permits `rxBuffer[0]` up to 139 while `BL_HOST_BUFFER_RX_LENGTH` is 140, so the packet always fits the buffer too. The bound is tight rather than comfortable — raising `BL_HOST_BUFFER_RX_LENGTH` above 256 without changing this signature would truncate the CRC input and cause every long packet to fail verification.

## 27. ACK, NACK and Reply Framing

```c
static void Bootloader_SendACK(void){
    txBuffer[0] = BL_SEND_ACK;                       /* 0xAA */
    hserialConfig.txBuffer->buffer.length = 1;
    HSerial_SendBuffer(&hserialConfig, BL_UART_DELAY * 1000UL);
}

static void Bootloader_SendNACK(void){
    txBuffer[0] = BL_SEND_NACK;                      /* 0x00 */
    hserialConfig.txBuffer->buffer.length = 1;
    HSerial_SendBuffer(&hserialConfig, BL_UART_DELAY * 1000UL);
}
```

Both are bare bytes with no `0xEE` prefix. The gateway's `GetAcknowledgementFromBootloader()` reads exactly one byte and compares it to `0xAA` — it does not look for a start byte:

```c
uint8_t GetAcknowledgementFromBootloader(uint32_t timeoutMs){
    while(k_uptime_get_32() - start < timeoutMs){
        if(uart_poll_in(uart_2, &c) == 0){ return c; }
        k_yield();
    }
    return 0xFF;
}
```

Whereas `ReceiveReplayFromBootloader()` *does* first hunt for `0xEE`. So the two reply shapes are consumed by two different gateway functions, and the protocol is only unambiguous because the gateway knows, per command, which one to call next. There is no self-describing framing.

| Value | Name | Direction | Meaning |
|---|---|---|---|
| `0xAA` | `BL_SEND_ACK` | STM32 → host | CRC verified, command accepted |
| `0x00` | `BL_SEND_NACK` | STM32 → host | CRC mismatch |
| `0xEE` | `BL_REPLAY_START_BYTE` | STM32 → host | Start of a data reply |
| `0xFB` | `WE_ARE_IN_BOOTLOADER` | STM32 → host | Probe answer (payload of an `0xEE` reply) |
| `0xA0` | (`appExists`) | STM32 → host | App vector table valid, resetting |
| `0xE1` | `SUCCESSFUL_ERASE` | STM32 → host | Four sectors erased |
| `0xE2` | `FLASH_PAYLOAD_WRITE_PASSED` | STM32 → host | Packet accepted |

## 28. The Timeout Model

Every timeout in the bootloader is a bare decrementing loop counter, not a time. The unit is "iterations of a `while` that reads a status register".

```c
STD_ReturnType UART_ReceiveChar(const UART_Config_t* uartObj, uint8_t* data, uint32_t timeoutMS){
    uint32_t currentTime = 0;
    while((!(uartObj->UartInstance->SR & (1 << 5))) && currentTime < timeoutMS){
        currentTime++;
    }
    if(currentTime >= timeoutMS){ return STD_TIMEOUT; }
    *data = (uint8_t)(uartObj->UartInstance->DR & 0xFF);
    return STD_SUCCESS;
}
```

The parameter is *named* `timeoutMS` throughout the driver stack, which is actively misleading. At 84 MHz with `-Og`, this loop is roughly 5–8 cycles per iteration, so:

| Nominal value | Iterations | Approximate real time |
|---|---|---|
| `BL_UART_DELAY` = 1000 | 1,000 | **≈ 70–95 µs** |
| `BL_UART_DELAY * 1000` = 1,000,000 | 1,000,000 | ≈ 70–95 ms |
| `BL_UART_DELAY * 100000` = 100,000,000 | 100,000,000 | ≈ 7–9.5 s |

Now map that onto where each is used:

| Call site | Timeout arg | Real time | Bytes at 115200 |
|---|---|---|---|
| Phase-1 length byte read | `BL_UART_DELAY` (1,000) | ≈ 80 µs | **less than one byte time (86.8 µs)** |
| Probe dummy read | `BL_UART_DELAY * 1000` | ≈ 80 ms | ~920 bytes |
| Phase-2 packet body read | `BL_UART_DELAY * 100000` | ≈ 8 s | ~92,000 bytes |
| All transmits | `BL_UART_DELAY * 1000` | ≈ 80 ms | ~920 bytes |

### 28.1 The phase-1 timeout is shorter than one byte

This is the single most surprising number in the bootloader, and it is why the idle loop behaves the way it does.

One byte at 115200 8N1 takes 10 bit-times = 86.8 µs. The phase-1 read gives up after roughly 80 µs. So when the line is idle, `BL_FetchHostCommand()` returns `STD_ERROR` almost immediately, `main()` calls it again, and the bootloader spins in a tight poll-and-give-up loop — thousands of times per second.

That is not a bug in effect: the loop *is* the polling mechanism, and because `memset(rxBuffer, 0, 140)` runs at the top of every iteration, a stale `rxBuffer[0]` cannot be mistaken for a length. But it has consequences:

- **The bootloader burns 100 % CPU while idle.** Irrelevant on mains/vehicle power, but it rules out any low-power waiting.
- **The first byte of a command must arrive during the ~80 µs window,** or it is caught on a subsequent iteration. Since the loop restarts within microseconds and the UART's receive register latches the byte regardless, the byte is not lost — `SR.RXNE` stays set until read. The next iteration reads it. So arrival timing genuinely does not matter.
- **`memset` of 140 bytes thousands of times per second** is the dominant cost of the idle loop. Harmless, but it means the "80 µs" figure above is optimistic; the true loop period is closer to 80 µs plus the memset.

### 28.2 The phase-2 timeout is enormous

Eight seconds to receive at most 139 more bytes (≈ 12 ms of wire time) is a 600× margin. It exists because a mid-packet stall — the gateway pausing to read the next block from LittleFS, for instance — must not abort a transfer. In exchange, a genuinely dead link stalls the bootloader for 8 seconds per attempt.

### 28.3 `UART_SendChar` has two sequential timeouts

```c
while((!(SR & (1 << 7))) && currentTime < timeoutMS){ currentTime++; }   /* TXE */
if(currentTime >= timeoutMS) return STD_TIMEOUT;
DR = data;
currentTime = 0;
while(!(SR & (1 << 6)) && currentTime < timeoutMS){ currentTime++; }     /* TC  */
if(currentTime >= timeoutMS) return STD_TIMEOUT;
```

Waiting for **TC** (transmission complete), not just TXE, after every byte makes transmission fully blocking down to the last stop bit. This is what makes `Bootloader_JumpToApp` safe: the `0xEE 0xA0` reply is guaranteed to be fully on the wire before `AIRCR` is written. A TXE-only wait would leave the last byte in the shift register when the reset fired, and the gateway would never see it.

It also means transmission is slow by design — 86.8 µs of pure spinning per byte. For a 3-byte reply that is 260 µs, which nobody cares about.

## 29. End-to-End Message Sequences

### 29.1 Full OTA, application currently running

```
 ESP32 gateway                                    STM32
 ─────────────                                    ─────
                                              [application running,
                                               streaming telemetry]

 B1: enter bootloader
   AA EB                    ──────────────────►  Thread5 matches 0xAA,0xEB
                            ◄──────────────────  EE AA
                                                 *(0x2000FFF8)=0xDEADBEEF
                                                 SYSRESETREQ
                                                 ═══ reset ═══
                                                 flag set → stay resident
                                                 PA0 high 1 s, then loop
   "Entering Bootloader..."

 B3: erase flash
   05 13 <crc32>            ──────────────────►  CRC ok
                            ◄──────────────────  AA
                                                 erase sectors 2,3,4,5
                                                   (≈ 2.0–3.1 s, blocking)
                            ◄──────────────────  EE E1
   "Erased Successfully"

 B5: write firmware  (loop, ~3000 iterations)
   <LEN> 14 <ci> <pi> 00 00 <n> <n bytes> <crc>
                            ──────────────────►  CRC ok
                            ◄──────────────────  AA
                                                 OTA_ReceivePacket()
                                                   accumulate into cipher_buffer
                                                   if full → GCM decrypt + flash
                            ◄──────────────────  EE E2
   ... × 3000 ...
                                                 (progress published at
                                                  10 % steps over MQTT)

   [if the last packet was exactly 64 B]
   0A 14 FF FF 00 00 00 <crc>
                            ──────────────────►  OTA_Finish()
                            ◄──────────────────  AA
                            ◄──────────────────  EE E2
   "Firmware Written Successfully"

 B4: jump to application
   07 12 <maj> <min> <crc32>──────────────────►  CRC ok
                            ◄──────────────────  AA
                                                 App_IsValid() → 1
                            ◄──────────────────  EE A0
                                                 SYSRESETREQ
                                                 ═══ reset ═══
                                                 flag clear → jump to app
   "Firmware is now Running"
   → SecOC RUNNING(slot=1) onto CAN
                                              [application running again]
```

### 29.2 Enter bootloader when already resident

```
 ESP32                                            STM32 bootloader
   AA EB                    ──────────────────►  rxBuffer[0] = 0xAA
                                                 dummy-read 0xEB
                            ◄──────────────────  EE FB
   "We are already in Bootloader"
```

### 29.3 CRC failure

```
 ESP32                                            STM32
   05 13 DE AD BE EF        ──────────────────►  CRC mismatch
                            ◄──────────────────  00   (NACK)
   GetAcknowledgementFromBootloader() returns 0x00
   → "Received Not-acknowledgement"
   → no retry; the whole command is abandoned
```

### 29.4 Timeout — the silent failure

```
 ESP32                                            STM32
   05 13 <crc32>            ────────────X         (bytes lost — cable, noise,
                                                   or the STM32 was mid-erase)
   waits 1000 ms for ACK
   GetAcknowledgementFromBootloader() → 0xFF
   → "[Bootloader] ACK Timeout"
   → "Received Not-acknowledgement"

                                                  Meanwhile the STM32 is
                                                  blocked in phase 2 for up to
                                                  8 seconds waiting for the
                                                  rest of a packet that will
                                                  never arrive, then returns
                                                  STD_NACK and sends nothing.
```

This is the only state where the two ends can be out of step for a meaningful period. The gateway will happily issue its *next* command while the STM32 is still in its 8-second phase-2 wait; those bytes get consumed as packet body, fail CRC, and elicit a NACK that the gateway is no longer listening for. The link resynchronises within one or two commands but the intermediate log output is confusing.

---

# Part V — Firmware Update Engine

## 30. Update Engine Architecture

[`src/app/firmware_flashing.c`](src/app/firmware_flashing.c) is 107 lines and does one job: turn a stream of ≤64-byte ciphertext fragments into decrypted, Flash-programmed application image.

```
      0x14 packets
      (≤64 B ciphertext each)
             │
             ▼
   ┌──────────────────────────────────────────────────┐
   │  OTA_ReceivePacket(payload, len)                 │
   │                                                  │
   │   memcpy into cipher_buffer[cipher_count]        │
   │   cipher_count += len                            │
   │                                                  │
   │   if cipher_count >= 2048:                       │
   │       decrypt_and_flash(2048); cipher_count = 0  │
   │                                                  │
   │   if len < 64:            ← "last packet" rule   │
   │       decrypt_and_flash(cipher_count)            │
   │       state = COMPLETE                           │
   └──────────────────────┬───────────────────────────┘
                          ▼
   ┌──────────────────────────────────────────────────┐
   │  decrypt_and_flash(encrypted_len)                │
   │                                                  │
   │   cipher_len = encrypted_len - 16                │
   │   tag        = cipher_buffer + cipher_len        │
   │                                                  │
   │   AES_GCM_DecryptChunk(ota.chunk_index,          │
   │                        cipher_buffer, cipher_len,│
   │                        tag, plaintext_buffer)    │
   │       └─ fails → return false → state = ERROR    │
   │                                                  │
   │   FLASH_Write(ota.flash_addr, plaintext_buffer,  │
   │               (cipher_len + 3) / 4)              │
   │                                                  │
   │   ota.flash_addr  += cipher_len                  │
   │   ota.chunk_index += 1                           │
   └──────────────────────┬───────────────────────────┘
                          ▼
              Flash at 0x08008000 + offset
```

The engine's entire state is one file-scope struct:

```c
static struct {
    OTA_State_t state;         /* IDLE / ACCUMULATING / ERROR / COMPLETE */
    uint32_t    chunk_index;   /* feeds the GCM nonce                    */
    uint16_t    cipher_count;  /* bytes currently in cipher_buffer       */
    uint32_t    flash_addr;    /* next Flash write address               */
} ota;
```

plus two static buffers:

```c
static uint8_t cipher_buffer[OTA_CHUNK_ENCRYPTED_SIZE];    /* 2048 */
static uint8_t plaintext_buffer[OTA_CHUNK_PLAINTEXT_SIZE]; /* 2032 */
```

Sixteen bytes of the ciphertext buffer are the GCM tag, which is why the plaintext buffer is 16 bytes smaller. `2048 = 2032 + 16` is the arithmetic that has to hold across the host tooling, the engine, and the crypto layer.

## 31. The Chunk Accumulator

```c
bool OTA_ReceivePacket(const uint8_t *payload, uint8_t payload_len){
    if(ota.state == OTA_STATE_ERROR ||
       ota.state != OTA_STATE_ACCUMULATING ||
       payload_len == 0 ||
       payload_len > OTA_PACKET_SIZE){
        return false;
    }

    if(ota.cipher_count + payload_len > OTA_CHUNK_ENCRYPTED_SIZE){
        ota.state = OTA_STATE_ERROR;
        return false;
    }

    memcpy(cipher_buffer + ota.cipher_count, payload, payload_len);
    ota.cipher_count += payload_len;

    /* Condition 1: buffer full → decrypt and flush */
    if (ota.cipher_count >= OTA_CHUNK_ENCRYPTED_SIZE) {
        if (!decrypt_and_flash(OTA_CHUNK_ENCRYPTED_SIZE)) {
            ota.state = OTA_STATE_ERROR;
            return false;
        }
        ota.cipher_count = 0;
        memset(cipher_buffer, 0, sizeof(cipher_buffer));
    }

    /* Condition 2: short payload → last packet, auto-finish */
    if (payload_len < OTA_PACKET_SIZE) {
        if (ota.cipher_count > 0) {
            if (!decrypt_and_flash(ota.cipher_count)) {
                ota.state = OTA_STATE_ERROR;
                return false;
            }
        }
        ota.state = OTA_STATE_COMPLETE;
    }

    return true;
}
```

The four guards in the first `if` are worth separating:

| Guard | Catches |
|---|---|
| `state == ERROR` | Packets arriving after a tag failure — refuse rather than corrupt further |
| `state != ACCUMULATING` | Packets arriving in `IDLE` (impossible — caller inits first) or `COMPLETE` (see [§35](#35-engine-failure-modes)) |
| `payload_len == 0` | A malformed packet that is not the `FF FF` end marker |
| `payload_len > 64` | A length byte larger than the protocol allows |

The `state == ERROR` test is redundant with `state != ACCUMULATING` — the first is a strict subset of the second. Harmless.

The overflow check `cipher_count + payload_len > 2048` is arithmetic on a `uint16_t` plus a `uint8_t` promoted to `int`, so it cannot itself overflow. With `cipher_count ≤ 2047` and `payload_len ≤ 64` the maximum is 2111, well inside `int`. Correct.

### 31.1 The 2048-boundary invariant

The "buffer full" branch uses `>=` but then passes the constant `OTA_CHUNK_ENCRYPTED_SIZE` to `decrypt_and_flash`, not `ota.cipher_count`:

```c
if (ota.cipher_count >= OTA_CHUNK_ENCRYPTED_SIZE) {
    if (!decrypt_and_flash(OTA_CHUNK_ENCRYPTED_SIZE)) { ... }
```

That is only correct if `cipher_count` lands on **exactly** 2048 and never overshoots. It does, because 2048 is an exact multiple of 64 and the host sends full 64-byte packets until the final short one. Break that assumption — say by changing `OTA_PACKET_SIZE` to 48, which does not divide 2048 — and the accumulator would overshoot into the guard, trip the overflow check, and error out. So the constant `2048 % 64 == 0` is an unstated invariant of the design.

The `memset(cipher_buffer, 0, 2048)` after each flush is defensive hygiene; nothing reads stale bytes because `cipher_count` gates every access. It costs about 2048 cycles per chunk, roughly 0.02 % of the transfer time. Fine.

## 32. Two Ways a Transfer Ends

The engine accepts two mutually exclusive end signals, and the host picks based on the size of the last data packet.

```
   Image size mod 64 != 0                Image size mod 64 == 0
   ──────────────────────                ──────────────────────

   ... 64 B packet                       ... 64 B packet
   ... 64 B packet                       ... 64 B packet
       37 B packet   ◄── short!              64 B packet   ◄── full
          │                                      │
          │ payload_len < 64                     │ nothing distinguishes it
          ▼                                      ▼
   OTA_ReceivePacket:                     host sends the explicit marker:
     flush remainder                        0A 14 FF FF 00 00 00 <crc>
     state = COMPLETE                             │
                                                  ▼
                                          Bootloader_MemoryWrite:
                                            OTA_Finish()
                                              flush remainder (if any)
                                              state = COMPLETE
```

`OTA_Finish()` is nearly identical to condition 2 of `OTA_ReceivePacket`:

```c
bool OTA_Finish(void){
    if (ota.state == OTA_STATE_ERROR){ return false; }

    if (ota.cipher_count > 0) {
        if (!decrypt_and_flash(ota.cipher_count)) {
            ota.state = OTA_STATE_ERROR;
            return false;
        }
    }
    ota.state = OTA_STATE_COMPLETE;
    return true;
}
```

Note it is idempotent-ish: calling it when `cipher_count == 0` and state is already `COMPLETE` just re-sets `COMPLETE` and returns `true`. Calling it after an `ERROR` returns `false` without side effects.

### 32.1 Why not always send the explicit marker?

Sending it unconditionally would be simpler and would remove the "short packet" special case entirely. The current split exists because the auto-finish rule came first (it is the one built into the engine) and the explicit marker was added later, on the gateway side, to cover the exact-multiple case that auto-finish cannot see. The gateway's comment says as much:

```c
/* Last packet was full 64B, STM32 needs explicit finish */
```

The redundancy is harmless: if the host sent the marker after a short packet, `OTA_Finish()` would find `cipher_count == 0`, do nothing, and return `true`. The reply would still be `0xE2`. So making the marker unconditional would be a safe simplification.

## 33. Decrypt-and-Flash

```c
static bool decrypt_and_flash(uint16_t encrypted_len){
    if (encrypted_len < 16) {
        return false;                      /* can't contain a tag */
    }

    uint16_t cipher_len = encrypted_len - 16;
    uint8_t *cipher = cipher_buffer;
    uint8_t *tag    = cipher_buffer + cipher_len;

    bool ok = AES_GCM_DecryptChunk(ota.chunk_index, cipher, cipher_len,
                                   tag, plaintext_buffer);
    if (!ok) {
        return false;
    }

    uint32_t words = (cipher_len + 3) / 4;
    if (FLASH_Write(ota.flash_addr, (uint32_t *)plaintext_buffer, words) != STD_SUCCESS) {
        return false;
    }

    ota.flash_addr  += cipher_len;
    ota.chunk_index++;

    return true;
}
```

### 33.1 Buffer layout

```
   cipher_buffer (2048 B)
   ┌────────────────────────────────────────────────────┬──────────────┐
   │              ciphertext — 2032 B                   │   tag 16 B   │
   └────────────────────────────────────────────────────┴──────────────┘
    ▲                                                    ▲
    cipher                                               tag = cipher_buffer + 2032

                    │ AES-128-GCM decrypt + verify
                    ▼
   plaintext_buffer (2032 B)
   ┌────────────────────────────────────────────────────┐
   │              plaintext — 2032 B                    │
   └────────────────────────────────────────────────────┘
                    │ FLASH_Write, 508 words
                    ▼
   Flash at ota.flash_addr .. +2031
```

The tag is the **trailing** 16 bytes of every chunk, including the final short one. So the host's chunking tool must append the tag to each chunk before splitting into 64-byte packets — the tag is not carried out-of-band.

### 33.2 The word-rounding subtlety

`words = (cipher_len + 3) / 4` rounds *up*. For the standard 2032-byte chunk this is exactly 508 words with no rounding. For a final chunk whose plaintext length is not a multiple of 4 — say 1477 bytes — `words` becomes 370, meaning `FLASH_Write` programs 1480 bytes: the 1477 real bytes plus 3 bytes of whatever `plaintext_buffer` held at those offsets.

Those 3 bytes are stale data from the *previous* chunk (the buffer is never cleared between decrypts). They land in Flash immediately past the end of the image. Since nothing reads past the image end, this is harmless in practice — but it is a small information leak into Flash and it means the flashed image is not byte-identical to the source file past its last word boundary.

Meanwhile `ota.flash_addr += cipher_len` advances by the *unrounded* length. If a non-multiple-of-4 chunk were ever followed by another chunk, the next `FLASH_Write` would be called with a non-word-aligned address and would be rejected:

```c
if(address % 4 != 0){ return STD_ERROR; }
```

This can only happen on the last chunk (by construction), so it is not reachable today. But it means the engine silently assumes **every chunk except the last has a length divisible by 4** — which holds because 2032 is divisible by 4.

### 33.3 No erase, ever

`decrypt_and_flash` writes without erasing. STM32 Flash programming can only clear bits (1 → 0); writing to a location that is not `0xFFFFFFFF` produces the bitwise AND of the old and new values, silently. The engine relies entirely on the host having sent `0x13` first.

If a host skipped the erase, the result would be a corrupted image with **no error reported at any layer**: `FLASH_Write` checks only `SR` error flags, and programming over non-erased Flash does not set `PGSERR` on F4 (it does on some other families). The GCM tag would still verify — it is checked before the write. The failure would only surface as an application that crashes on boot.

The one-line fix would be a read-back verify after each `FLASH_Write`. Logged in [§57](#57-known-gaps).

## 34. State Machine Reference

```
                    OTA_Init()
                        │
   ┌──────┐             ▼             ┌──────────────┐
   │ IDLE │────────────────────────►  │ ACCUMULATING │
   └──────┘                           └──────┬───────┘
       ▲                                     │
       │                                     ├── packet, buffer not full
       │                                     │      └─► stay ACCUMULATING
       │                                     │
       │                                     ├── packet fills 2048
       │                                     │      ├─ tag ok  → stay ACCUMULATING
       │                                     │      └─ tag bad → ERROR
       │                                     │
       │                                     ├── packet < 64 B
       │                                     │      ├─ tag ok  → COMPLETE
       │                                     │      └─ tag bad → ERROR
       │                                     │
       │                                     └── OTA_Finish()
       │                                            ├─ tag ok  → COMPLETE
       │                                            └─ tag bad → ERROR
       │
       │                            ┌──────────┐         ┌───────┐
       │                            │ COMPLETE │         │ ERROR │
       │                            └──────────┘         └───────┘
       │                                 │                    │
       └─────────── ✗ no transition ─────┴────────────────────┘
                    (only a reset returns to IDLE)
```

| State | Value | `OTA_ReceivePacket` | `OTA_Finish` | Escape |
|---|---|---|---|---|
| `OTA_STATE_IDLE` | 0 | rejected | `true`, → COMPLETE | `OTA_Init()` |
| `OTA_STATE_ACCUMULATING` | 1 | accepted | accepted | — |
| `OTA_STATE_ERROR` | 2 | rejected | rejected | **reset only** |
| `OTA_STATE_COMPLETE` | 3 | rejected | `true` (no-op) | **reset only** |

The absence of any path back to `IDLE` other than a reset is the engine's most consequential limitation. `OTA_Init()` is the only writer of `IDLE → ACCUMULATING`, and the only caller is `Bootloader_MemoryWrite`, gated on `OTA_GetState() == OTA_STATE_IDLE`. See the next section.

## 35. Engine Failure Modes

### 35.1 A second OTA in the same session always fails

This is a genuine defect, reproducible, and worth stating plainly.

```
  Boot into bootloader
     ota.state = IDLE           (zero-initialised .bss)

  First OTA
     0x13 erase                 → does not touch ota.state
     0x14 packet #1             → state == IDLE, so OTA_Init(), state = ACCUMULATING
     ... transfer ...
     last packet / FF FF marker → state = COMPLETE

  Second OTA — without an intervening reset
     0x13 erase                 → does not touch ota.state (still COMPLETE)
     0x14 packet #1             → state != IDLE, so OTA_Init() is NOT called
                                → OTA_ReceivePacket sees state != ACCUMULATING
                                → returns false
                                → bootloader replies EE 00
     gateway: "Write Failed", aborts

  Result: the application region has been erased and nothing was written.
          The node is now unbootable and must be recovered.
```

The same trap applies after an `ERROR`: once a tag fails, every subsequent packet is rejected until a reset.

**Why it has not bitten hard yet:** the gateway's normal flow is `B1 (enter) → B3 (erase) → B5 (write) → B4 (jump)`, and `B4` resets the MCU. So a second OTA is normally preceded by a fresh `B1`, which also resets. The failure needs an operator to issue `B3`/`B5` twice without `B4` in between — which is exactly what happens when someone retries after a failed transfer.

**The fix is two lines.** Either call `OTA_Init()` unconditionally from `Bootloader_EraseFlash()`, or change the guard in `Bootloader_MemoryWrite` to re-init on the first packet of a chunk-0 sequence:

```c
if(OTA_GetState() != OTA_STATE_ACCUMULATING){
    OTA_Init();
}
```

Recorded in [§57](#57-known-gaps) as the highest-severity open item.

### 35.2 Tag failure aborts without cleanup

When `AES_GCM_DecryptChunk` returns false, `decrypt_and_flash` returns false, the caller sets `ERROR`, and the bootloader replies `0x00`. The gateway aborts. But:

- `ota.flash_addr` is left pointing partway through the image.
- The already-written chunks stay in Flash.
- `App_IsValid()` will *still pass* if chunk 0 was written successfully, because words 0 and 1 came from chunk 0.

So after a mid-transfer tag failure, a subsequent `0x12` (jump) would report `0xA0` and boot a truncated application. Nothing in the current design prevents this. The gateway does not issue `0x12` after a failed `0x14` — but an operator sending `B4` manually would.

### 35.3 A dropped packet is indistinguishable from corruption

Covered in [§25.1](#251-the-chunk-and-packet-indices-are-decorative). The symptom is a tag failure on the chunk boundary *after* the drop, which is confusing to diagnose because the chunk that fails is not the chunk that lost data.

### 35.4 There is no total-length check

The engine has no idea how large the image is supposed to be. It writes whatever it receives and stops when told to. A transfer that stops early — cable pulled, gateway crash — produces a truncated image with a valid header. The `App_IsValid()` gap from [§14.1](#141-what-it-does-not-catch) is what turns that into a boot into garbage.

The cheapest mitigation would be for the host to prepend the total image length to chunk 0's plaintext, and for the engine to refuse `OTA_Finish()` until `flash_addr - FLASH_APP_START` matches. That is a coordinated change across the gateway's OTA handler and the chunking tool.
---

# Part VI — Cryptography

## 36. Threat Model

Before describing what the crypto does, it is worth stating what it is defending against — because the answer shapes whether the design is adequate.

**Assets**

| Asset | Value to an attacker |
|---|---|
| The application firmware image | Reverse-engineering the ML model, the frame protocol, and the OTA keys |
| The ability to run arbitrary code on the STM32 | Full control of a sensing node; UART access to the gateway |
| The AES-128 key | Ability to forge images accepted by every node built from this tree |

**Attacker positions**

| Position | Capability | Covered? |
|---|---|---|
| Passive listener on the UART1 wire | Reads the ciphertext stream | Yes — GCM confidentiality |
| Active injector on the UART1 wire | Sends arbitrary `0x14` packets | Yes — GCM tag rejects forged chunks |
| Replay of a previously captured image | Re-flash an old, possibly vulnerable version | **No** — no version/counter binding |
| Chunk reordering within one transfer | Swap chunk 5 and chunk 9 | Yes — the nonce includes the chunk index |
| Compromised gateway | Sends any correctly-encrypted image it possesses | Out of scope |
| Physical access with ST-Link | Read Flash, extract the key, write anything | **No** — RDP is not configured |
| Physical access with a debugger while running | Halt the core, read RAM including the key | **No** |

**Non-goals, explicitly**

- The bootloader does not authenticate the *sender*. Anyone holding the key can produce an accepted image.
- The bootloader does not prevent downgrade.
- The bootloader does not protect the key at rest.

With those bounds set, the construction below is a reasonable fit for the wire threats and a poor fit for the physical ones. [§43](#43-security-assessment) grades it.

## 37. AES-128-GCM Construction

[`src/app/aes_gcm.c`](src/app/aes_gcm.c) wraps mbedTLS's GCM implementation. The public surface is one function:

```c
bool AES_GCM_DecryptChunk(uint32_t chunk_index,
                          const uint8_t *ciphertext, size_t ciphertext_len,
                          const uint8_t *tag,
                          uint8_t *plaintext_out);
```

Implementation:

```c
bool AES_GCM_DecryptChunk(uint32_t chunk_index, const uint8_t *ciphertext,
                          size_t ciphertext_len, const uint8_t *tag,
                          uint8_t *plaintext_out){
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES,
                                 AES_GCM_KEY, AES_GCM_KEY_SIZE * 8);
    if(ret != 0){ mbedtls_gcm_free(&ctx); return false; }

    uint8_t nonce[AES_GCM_NONCE_SIZE];
    build_nonce(chunk_index, nonce);

    uint8_t aad[4];
    memcpy(aad, &nonce[8], 4);

    ret = mbedtls_gcm_auth_decrypt(&ctx, ciphertext_len,
                                   nonce, AES_GCM_NONCE_SIZE,
                                   aad, sizeof(aad),
                                   tag, AES_GCM_TAG_SIZE,
                                   ciphertext, plaintext_out);
    mbedtls_gcm_free(&ctx);
    return (ret == 0);
}
```

Parameters:

| Parameter | Value |
|---|---|
| Cipher | AES-128 (`AES_GCM_KEY_SIZE * 8` = 128 bits) |
| Mode | GCM (Galois/Counter Mode) |
| Nonce | 12 bytes (the GCM-native length — no GHASH derivation needed) |
| Tag | 16 bytes, full length |
| AAD | 4 bytes (the chunk index, big-endian) |
| Plaintext per call | 2032 bytes, or less for the final chunk |

`mbedtls_gcm_auth_decrypt` verifies the tag **before** returning, and returns `MBEDTLS_ERR_GCM_AUTH_FAILED` (a non-zero value) if it does not match. `plaintext_out` may have been written during the streaming decrypt even on failure, but the caller discards it — `decrypt_and_flash` returns false before reaching `FLASH_Write`. So a bad chunk never reaches Flash. That ordering is the single most important correctness property in the whole update path.

### 37.1 Context lifetime

`mbedtls_gcm_init` / `mbedtls_gcm_free` bracket every call. Each 2 KB chunk therefore pays for:

- one `mbedtls_gcm_setkey` → `mbedtls_cipher_setup` → `mbedtls_calloc` of an AES context
- one AES key schedule expansion (11 round keys for AES-128)
- one GHASH subkey computation (`HL`/`HH` tables, 2 × 16 × 8 bytes inside the GCM context)
- one `mbedtls_gcm_free` → `mbedtls_free`

That is meaningful overhead — roughly a few thousand cycles of setup against ~2032 bytes of decryption. At 84 MHz the whole chunk still decrypts in well under a millisecond, and the transfer is UART-bound (2048 bytes of ciphertext takes 178 ms of wire time at 115200), so the crypto is nowhere near the critical path. Hoisting the context out of the loop would save perhaps 0.1 % of the transfer time and would introduce lifetime complexity. Leaving it per-call is the right trade.

It does mean the heap is exercised once per chunk. With 512 bytes of heap and an allocation/free pair that always balances, fragmentation cannot accumulate.

## 38. Key and Nonce Derivation

```c
/* Host key: 0724356978ABCDEF4123456789ABDDEF */
static const uint8_t AES_GCM_KEY[AES_GCM_KEY_SIZE] = {
    0x07, 0x24, 0x35, 0x69, 0x78, 0xAB, 0xCD, 0xEF,
    0x41, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xDD, 0xEF
};

/* Base nonce: 000000000000000000000001 */
static const uint8_t AES_GCM_BASE_NONCE[AES_GCM_NONCE_SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};
```

Nonce construction:

```c
static void build_nonce(uint32_t chunk_index, uint8_t *nonce_out){
    memcpy(nonce_out, AES_GCM_BASE_NONCE, 8);       /* first 8 bytes: all zero */
    nonce_out[8]  = (chunk_index >> 24) & 0xFF;
    nonce_out[9]  = (chunk_index >> 16) & 0xFF;
    nonce_out[10] = (chunk_index >> 8)  & 0xFF;
    nonce_out[11] = (chunk_index)       & 0xFF;
}
```

```
   AES_GCM_BASE_NONCE
   ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
   │00│00│00│00│00│00│00│00│00│00│00│01│
   └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘
     0  1  2  3  4  5  6  7  8  9 10 11
     └──────── copied ────────┘  └── overwritten ──┘

   nonce for chunk N
   ┌──┬──┬──┬──┬──┬──┬──┬──┬────────────────────┐
   │00│00│00│00│00│00│00│00│  N as 4-byte BE    │
   └──┴──┴──┴──┴──┴──┴──┴──┴────────────────────┘

   chunk 0 → 00 00 00 00 00 00 00 00 00 00 00 00
   chunk 1 → 00 00 00 00 00 00 00 00 00 00 00 01
   chunk 2 → 00 00 00 00 00 00 00 00 00 00 00 02
   ...
```

Note that only the first **8** bytes of `AES_GCM_BASE_NONCE` are used; bytes 8–11 (`00 00 00 01`) are always overwritten. So the trailing `01` in the "base nonce" is decorative — it is never seen by the cipher. That also means `chunk 1`'s nonce happens to equal the literal base-nonce constant, which is a coincidence, not a design.

### 38.1 Nonce-reuse analysis

GCM's catastrophic failure mode is nonce reuse under the same key: it leaks the GHASH authentication subkey and allows forgery of arbitrary messages. So the question "can two different plaintexts ever be encrypted with the same nonce?" is the one that matters.

**Within a single image:** no. `ota.chunk_index` starts at 0 and strictly increments once per successful decrypt. Each chunk gets a distinct nonce.

**Across images:** **yes, always.** Every firmware image starts at chunk 0. So chunk 0 of version 1.0 and chunk 0 of version 1.1 are both encrypted with nonce `00…00` under the same fixed key.

The consequence of reusing a GCM nonce across two messages *M₁* and *M₂*:

1. The keystream is identical, so `C₁ ⊕ C₂ = M₁ ⊕ M₂`. An attacker who captures both ciphertexts learns the XOR of the two plaintexts. Since firmware images from the same build tree share large identical regions (the vector table layout, library code, the CubeAI runtime), the XOR is mostly zeros with structure in the differing regions — which is a strong plaintext-recovery position.
2. More seriously, the pair `(C₁, T₁)`, `(C₂, T₂)` with the same nonce permits recovery of the GHASH subkey `H` by solving a polynomial over GF(2¹²⁸) — the "forbidden attack" (Joux, 2006; Böck–Zauner–Devlin–Somorovsky 2016). With `H` recovered, an attacker can forge a valid tag for *any* ciphertext under that nonce. Combined with the XOR keystream from point 1, they can produce an arbitrary chunk 0 that the bootloader will accept.

This is not theoretical. It requires only that the attacker capture two different firmware images on the UART line — which any passive listener with a logic analyser gets for free across two OTA cycles.

**The mitigation is small.** Give each *image* a random or monotonic 8-byte prefix instead of the fixed zeros, and ship that prefix in the clear as part of the transfer (it is not secret; it only needs to be unique). The nonce becomes `image_id(8) || chunk_index(4)`. Both the chunking tool and `build_nonce` change; nothing else does.

This is the highest-severity security finding in this document and is recorded in [§43](#43-security-assessment) and [§57](#57-known-gaps).

### 38.2 The key is a compile-time constant in Flash

`AES_GCM_KEY` is `static const`, so it lands in `.rodata` in sector 0 or 1. It is:

- **Readable over SWD** unless Flash read-out protection is set to level 1 or 2. Nothing in this project sets RDP.
- **Identical on every node** built from this tree.
- **Committed to git**, in plaintext, with a comment giving the hex string.

Extracting it is a two-command operation with an ST-Link:

```bash
st-flash read bl_dump.bin 0x08000000 8000
```

then search the dump for the 16-byte pattern. There is no obfuscation and none would help much.

Mitigations, in increasing order of effort:

| Mitigation | Effort | Effect |
|---|---|---|
| Set RDP level 1 | One option-byte write | Blocks SWD Flash reads; reversible with a mass erase (which destroys the key anyway) |
| Set RDP level 2 | One option-byte write | Permanent; also permanently disables debug — a one-way door |
| Per-device key derived from the 96-bit unique ID | Moderate | Compromising one node no longer compromises the fleet |
| Key in an external secure element | High | Out of scope for this hardware |

RDP level 1 is the obvious first step and costs nothing at runtime.

## 39. The AAD Choice

```c
uint8_t aad[4];
memcpy(aad, &nonce[8], 4);      /* the chunk index, again */
```

The additional authenticated data is a **copy of the last four bytes of the nonce** — that is, the chunk index, which is already an input to the cipher via the nonce.

Cryptographically this adds nothing. GCM already binds the tag to the nonce; authenticating the same value a second time as AAD does not strengthen the binding. It is not harmful either — it costs one extra GHASH block per chunk, about 16 bytes' worth of processing.

What AAD *would* be useful for here is binding metadata that is not already in the nonce and that the receiver can independently verify:

| Candidate AAD | What it would buy |
|---|---|
| Total image length | Detects truncation ([§35.4](#354-there-is-no-total-length-check)) |
| Firmware version | Enables downgrade rejection |
| Target device ID | Prevents an image for node A being flashed to node B |
| Total chunk count | Detects a missing final chunk |

Any of these would turn AAD from decoration into a real control. The change is symmetric — the encryptor and `AES_GCM_DecryptChunk` must agree — but it does not affect the wire format, since AAD is not transmitted.

The `test_encrypt` function in the same file uses the identical AAD construction, which is how the round-trip self-test passes. Any change must update both.

## 40. mbedTLS Configuration

The vendored mbedTLS is trimmed to the bone. [`include/mbedtls/config.h`](include/mbedtls/config.h) is 17 lines:

```c
#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* Minimal config for AES-GCM only */
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY

/* Exclude everything else to save flash */
// No RSA, ECC, SHA, TLS, etc.

#include "mbedtls/check_config.h"
#endif
```

| Macro | Pulls in | Why |
|---|---|---|
| `MBEDTLS_HAVE_ASM` | Inline assembly paths where available | Modest speed-up |
| `MBEDTLS_CIPHER_C` | `cipher.c`, `cipher_wrap.c` | GCM's generic cipher abstraction layer |
| `MBEDTLS_AES_C` | `aes.c` | The block cipher |
| `MBEDTLS_GCM_C` | `gcm.c` | The mode |
| `MBEDTLS_PLATFORM_C` | `platform.c` | `mbedtls_calloc` / `mbedtls_free` indirection |
| `MBEDTLS_PLATFORM_MEMORY` | — | Makes the allocator swappable at runtime |

The config is selected via a compile-time define in `aes_gcm.c` itself:

```c
#define MBEDTLS_CONFIG_FILE "mbedtls/config.h"
#include "mbedtls/gcm.h"
```

Rather than a `-DMBEDTLS_CONFIG_FILE=...` build flag. That works because `aes_gcm.c` is the only translation unit in the bootloader that includes mbedTLS headers directly — the mbedTLS `.c` files themselves are compiled with the default config path, which resolves to the same vendored header because it is the only `mbedtls/config.h` on the include path. Fragile, but functional. A `build_flags` entry would be more robust.

`check_config.h` is included at the end, which is the standard mbedTLS mechanism for catching illegal combinations at compile time (e.g. `MBEDTLS_GCM_C` without a block cipher).

### 40.1 What is *not* enabled, and what that costs

| Excluded | Consequence |
|---|---|
| `MBEDTLS_AES_ROM_TABLES` | The 8 KB of AES tables are built at runtime into `.bss` instead of living in `.rodata`. See [§41](#41-crypto-memory-cost). |
| `MBEDTLS_AES_FEWER_TABLES` | Tables stay at 8 KB rather than 2 KB |
| `MBEDTLS_SELF_TEST` | `mbedtls_aes_self_test()` / `mbedtls_gcm_self_test()` unavailable — the project rolls its own ([§42](#42-the-self-test-harness)) |
| `MBEDTLS_AESNI_C` / hardware acceleration | N/A on Cortex-M4 |
| Everything else (RSA, ECC, SHA, TLS, X.509, RNG) | ~200 KB of Flash saved. This is the whole point of the trim. |

Notably absent is any hash or signature primitive. That is what forecloses image signing without adding several kilobytes back.

## 41. Crypto Memory Cost

### 41.1 Flash

Rough attribution from the symbol table of the committed build:

| Symbol / module | Bytes | Note |
|---|---|---|
| `mbedtls_internal_aes_encrypt` | 1,076 | Largest single function in the image |
| `mbedtls_internal_aes_decrypt` | 1,076 | |
| Remainder of `aes.c` (key schedule, table init, ECB/CBC wrappers) | ~2,400 | |
| `gcm.c` (GHASH, counter mode, tag) | ~2,200 | |
| `cipher.c` + `cipher_wrap.c` | ~1,800 | Generic abstraction; mostly overhead for a single-cipher build |
| `constant_time.c`, `platform.c`, `platform_util.c` | ~450 | |
| **Total** | **≈ 9,000** | **≈ 69 % of the 13,100 B `.text`** |

`cipher.c` / `cipher_wrap.c` is the entry most amenable to removal: it exists so that GCM can be parameterised over any block cipher, and this build only ever uses AES-128. Calling `mbedtls_aes_*` directly from a hand-written GCM would save perhaps 1.8 KB, at the cost of hand-writing GHASH — a poor trade given 19 KB of Flash headroom.

### 41.2 RAM

| Object | Bytes | Lifetime |
|---|---|---|
| `FT0`, `FT1`, `FT2`, `FT3` | 4 × 1,024 = 4,096 | Permanent, `.bss`, generated on first key schedule |
| `RT0`, `RT1`, `RT2`, `RT3` | 4 × 1,024 = 4,096 | Permanent, `.bss` |
| `mbedtls_gcm_context` (on the stack in `AES_GCM_DecryptChunk`) | ~400 | Per call |
| `mbedtls_aes_context` (heap, via `mbedtls_calloc`) | ~288 | Per call |
| **Total steady-state** | **8,192** | |

The 8 KB of tables are `mbedtls_aes_gen_tables()`'s output — forward and reverse S-box-derived lookup tables for the table-driven AES implementation. Defining `MBEDTLS_AES_ROM_TABLES` would make them `const` and move all 8 KB from RAM to Flash. Given the budgets:

```
   Current                        With MBEDTLS_AES_ROM_TABLES
   ───────                        ───────────────────────────
   Flash 13,288 / 32,768  (41 %)  Flash 21,480 / 32,768  (66 %)
   RAM   15,104 / 65,528  (23 %)  RAM    6,912 / 65,528  (11 %)
```

Both fit. Which is preferable depends on which resource is scarcer, and here neither is under pressure — so this is a "would be tidier" item rather than a required fix. `MBEDTLS_AES_FEWER_TABLES` is the middle option: 2 KB of tables, roughly 25 % slower AES, which is invisible against a UART-bound transfer.

### 41.3 The heap

`_Min_Heap_Size = 0x200` = 512 bytes. `mbedtls_calloc` (which resolves to newlib-nano's `calloc`, confirmed present in the symbol table at `0x08002ec8`) is called once per `mbedtls_gcm_setkey` to allocate the AES context.

```
   sizeof(mbedtls_aes_context) = 4 (nr) + 4 (rk offset) + 68 × 4 (buf)  ≈ 288 B
   newlib-nano malloc chunk overhead                                    ≈  8 B
                                                                        ─────
                                                                        ≈ 296 B
```

296 of 512 bytes — a 1.7× margin against a single simultaneous allocation, and the code never holds two at once. It is tight enough that adding any other heap user to the bootloader would need re-checking, and loose enough that it has never been a problem.

The `/DISCARD/ libc.a` rule in the linker script does *not* prevent this — newlib-nano's archive is `libc_nano.a`, which does not match the literal `libc.a` pattern. Verified by the presence of `T calloc` and `T free` in the final image.

## 42. The Self-Test Harness

Two testing mechanisms exist, both currently dormant.

### 42.1 `AES_GCM_RunTest()` — the round-trip test

Defined at the end of `aes_gcm.c`, alongside a `test_encrypt()` helper that mirrors the host's encryption exactly:

```c
bool AES_GCM_RunTest(void){
    uint8_t plaintext[2032];
    for(int i = 0; i < 2032; i++){ plaintext[i] = i & 0xFF; }

    uint8_t ciphertext[2032], tag[16], decrypted[2032];

    /* Test 1: full 2032-byte chunk round-trips */
    if(test_encrypt(0, plaintext, 2032, ciphertext, tag) != 0) return false;
    if(!test_decrypt(0, ciphertext, 2032, tag, decrypted))     return false;
    if(memcmp(plaintext, decrypted, 2032) != 0)                return false;

    /* Test 2: wrong chunk index must FAIL */
    if(test_decrypt(1, ciphertext, 2032, tag, decrypted))      return false;

    /* Test 3: corrupted tag must FAIL */
    uint8_t bad_tag[16];
    memcpy(bad_tag, tag, 16);
    bad_tag[0] ^= 0xFF;
    if(test_decrypt(0, ciphertext, bad_tag ... ))              return false;

    return true;
}
```

Three cases, and the two negative ones are the valuable half: they prove that the nonce actually binds the chunk index, and that the tag actually rejects tampering. A GCM integration that silently ignored the tag would pass test 1 and fail tests 2 and 3.

Cost: 6,096 bytes of **stack** (three 2032-byte arrays as locals) against `_Min_Stack_Size` of 1,024. **Calling `AES_GCM_RunTest()` from `main()` as written would blow the stack.** The arrays would need to be `static` first. This is presumably why the call is commented out in `tests/encrypt.main.c`:

```c
//bool aes_ok = AES_GCM_RunTest();
```

Recorded in [§57](#57-known-gaps).

### 42.2 `tests/encrypt.main.c` — the known-answer test

An alternative `main()` that decrypts two pre-captured chunks and blinks on success:

```c
#include "../tests/chunk0.h"      /* 2032 B ciphertext + 16 B tag, index 0 */
#include "../tests/chunk1.h"      /* 1476 B ciphertext + 16 B tag, index 1 */

int main(){
    CheckForAppToRun();
    static uint8_t decrypted0[2032];
    static uint8_t decrypted1[1476];
    ... clock, LED, BL_Init ...

    bool ok0 = AES_GCM_DecryptChunk(0, chunk0_cipher, 2032, chunk0_tag, decrypted0);
    bool ok1 = AES_GCM_DecryptChunk(1, chunk1_cipher, 1476, chunk1_tag, decrypted1);

    if (ok0 && ok1) {
        while(1) {                       /* 2.5 Hz blink = pass */
            LED_SetState(LED_0, LED_HIGH); SYSTICK_DelayMS(200);
            LED_SetState(LED_0, LED_LOW);  SYSTICK_DelayMS(200);
        }
    }
    return 0;                            /* silent = fail */
}
```

This is a genuine known-answer test: `chunk0.h` and `chunk1.h` hold real ciphertext produced by the host tooling, so a pass proves interoperability with the actual encryptor, not just self-consistency. The two buffers are correctly `static`, so the stack is fine.

The 1476-byte second chunk is significant — it exercises the **short final chunk** path, which is the one where the word-rounding in [§33.2](#332-the-word-rounding-subtlety) applies.

Note the stale `CheckForAppToRun()` in this file: it is the *older* implementation that reads a flag from `FLASH_SECTOR_1` rather than from RAM.

```c
/* tests/encrypt.main.c — OBSOLETE version */
FLASH_Read(FLASH_SECTOR_1, &flag, 1);
FLASH_Read(FLASH_SECTOR_2, &appExists, 1);
if(flag == BOOTLOADER_APP_MAGIC && 0xFFFFFFFF != appExists){ ... }
```

This predates the RAM-flag design ([§8](#8-ram-map-and-the-shared-boot-flag)) and is not compatible with the current application. The file is not in the build (`src/main.c` provides the real `main`), so it does not conflict — but anyone swapping it in for a crypto test would also silently swap in the old boot logic. Worth a comment at minimum.

**To run it:** rename `src/main.c` aside and copy `tests/encrypt.main.c` into `src/`. There is no build flag to select it.

## 43. Security Assessment

| # | Finding | Severity | Status |
|---|---|---|---|
| S1 | **Nonce reuse across images.** Every image starts at chunk 0 with a fixed base nonce, so chunk *N* of any two images shares a nonce under the same key. Enables keystream recovery and — via the forbidden attack — GHASH subkey recovery and arbitrary forgery. | **Critical** | Open |
| S2 | **No read-out protection.** RDP is left at level 0, so the AES key can be extracted over SWD with `st-flash read`. | **High** | Open |
| S3 | **Fleet-wide shared key.** One extraction compromises every node built from this tree. | **High** | Open |
| S4 | **No downgrade protection.** Nothing binds an image to a version, so a captured old image can be replayed indefinitely. | **Medium** | Open |
| S5 | **No image-level integrity.** Per-chunk tags do not detect truncation; `App_IsValid()` will happily launch a half-flashed image. | **Medium** | Open |
| S6 | **AAD carries no independent information.** It duplicates the nonce, so it authenticates nothing new. | **Low** | Open |
| S7 | **Key committed to version control** in plaintext with a comment naming it. | **Informational** | Open |

### 43.1 What is done well

It is worth being explicit that the parts that are right are right:

- **Authenticated encryption, not encryption alone.** GCM gives confidentiality *and* integrity in one pass. A design that used raw AES-CTR plus a CRC would be trivially forgeable; this one is not, absent S1.
- **Tag verified before Flash write.** `decrypt_and_flash` returns on `!ok` before touching Flash. A failed chunk cannot corrupt the image.
- **Full 16-byte tags.** No truncation. Forgery probability per attempt is 2⁻¹²⁸.
- **Chunk index in the nonce.** Reordering or replaying a chunk *within* a transfer fails the tag.
- **Constant-time comparison.** `mbedtls_gcm_auth_decrypt` uses `mbedtls_ct_memcmp` internally, so tag verification does not leak timing. `constant_time.c` is in the build for exactly this.
- **Failure is fail-closed.** Every crypto error path leads to `OTA_STATE_ERROR` and a `0x00` reply, not to a partial write.

### 43.2 Recommended remediation order

1. **Fix S1.** Add an 8-byte per-image nonce prefix, transmitted in the clear. This is the only finding that lets a purely passive wire attacker escalate to code execution. Changes: `build_nonce()`, the host chunker, and one new field in the transfer preamble.
2. **Set RDP level 1 (S2).** One option-byte write during manufacturing. Does not affect runtime behaviour and is reversible via mass erase.
3. **Bind the image length into AAD (S5, S6).** Kills two findings with one change and requires no wire-format change.
4. **Per-device key derivation (S3).** `HKDF(master_key, device_unique_id)` — but this needs a hash primitive, which means adding `MBEDTLS_SHA256_C` (~3 KB Flash). Affordable within the 19 KB headroom.
5. **Version binding (S4).** Requires the gateway to be the version authority anyway, which it already is.

---

# Part VII — Driver Layer

## 44. Layering Model

```
   ┌───────────────────────────────────────────────────────────┐
   │  app/                                                     │
   │    main.c              boot decision, resident loop       │
   │    bootloader.c        command protocol                   │
   │    firmware_flashing.c OTA chunk engine                   │
   │    aes_gcm.c           GCM wrapper                        │
   └────────────┬──────────────────────────────────────────────┘
                │
   ┌────────────▼──────────────────────────────────────────────┐
   │  HAL/  — device abstractions built from MCAL              │
   │    hserial.c   buffered serial over UART_*                │
   │    led.c       named LEDs over GPIO_*                     │
   └────────────┬──────────────────────────────────────────────┘
                │
   ┌────────────▼──────────────────────────────────────────────┐
   │  MCAL/ — register-level, one module per peripheral        │
   │    rcc.c  gpio.c  uart.c  flash.c  crc.c                  │
   └────────────┬──────────────────────────────────────────────┘
                │
   ┌────────────▼──────────────────────────────────────────────┐
   │  Core/                                                     │
   │    systick.c   Cortex-M core timer                        │
   └───────────────────────────────────────────────────────────┘
```

Header discipline:

| Directory | Contains | Who may include |
|---|---|---|
| `include/interface/` | Public APIs, enums, config structs | Anyone |
| `include/private/` | Register-map structs, base addresses | Only the owning `.c` |
| `include/configuration/` | Compile-time knobs | The owning module |

**One violation.** `bootloader.c` includes `"private/MCAL/flash_priv.h"`:

```c
#include "interface/MCAL/flash.h"
#include "private/MCAL/flash_priv.h"      /* ← reaches through the layer */
```

Inspection shows it does not actually use anything from `flash_priv.h` — no `FLASH->` access appears in `bootloader.c`. It is a leftover include. Removing it would restore the invariant.

## 45. RCC Driver

`src/MCAL/interface/rcc.c` — 555 lines, the largest driver. Covered functionally in [§6](#6-clock-tree-and-bring-up); this section is the API reference.

```c
STD_ReturnType RCC_ConfigureClock(const RCC_CFG_t *cfg);
STD_ReturnType RCC_SetSystemClock(RCC_ClockType_t clockType);
STD_ReturnType RCC_SetClock(RCC_ClockType_t clockType, RCC_Status_t status);
STD_ReturnType RCC_WaitForClockReady(RCC_ClockType_t clockType, uint32_t timeout);
STD_ReturnType RCC_WaitForSysClkReady(RCC_ClockType_t clockType, uint32_t timeout);
STD_ReturnType RCC_ConfigurePLL(const PLL_CFG_t *pllCfg);
STD_ReturnType RCC_SetPLLClockSource(RCC_ClockType_t source);
STD_ReturnType RCC_CheckPLLClockSource(RCC_ClockType_t *source);
STD_ReturnType RCC_SetPLLMaxClock(void);
STD_ReturnType RCC_ControlPeripheral(RCC_Peripheral_t p, RCC_Peripheral_Operation_t op);
STD_ReturnType RCC_SetAHBPrescaler(RCC_AHB_Prescaler_t prescaler);
STD_ReturnType RCC_SetAPBPrescaler(RCC_APB_Prescaler_t prescaler, RCC_BusType_t bus);
```

### 45.1 Peripheral ID encoding

`RCC_Peripheral_t` packs the bus selector into the top two bits and the `xxxENR` bit position into the rest:

```
   bit 31    30                                    0
   ┌────┬────┬──────────────────────────────────────┐
   │  bus ID │       enable-bit mask                │
   └─────────┴──────────────────────────────────────┘

   RCC_BUS_MASK   = 0xC0000000
   RCC_BUS_OFFSET = 30

   RCC_AHB1 = 0  →  0x00000000 | (1 << n)
   RCC_AHB2 = 1  →  0x40000000 | (1 << n)
   RCC_APB1 = 2  →  0x80000000 | (1 << n)
   RCC_APB2 = 3  →  0xC0000000 | (1 << n)
```

Examples:

| Constant | Value | Decodes to |
|---|---|---|
| `RCC_GPIOA` | `0x00000001` | AHB1ENR bit 0 |
| `RCC_CRC` | `0x00001000` | AHB1ENR bit 12 |
| `RCC_USART2` | `0x80020000` | APB1ENR bit 17 |
| `RCC_USART1` | `0xC0000010` | APB2ENR bit 4 |

`RCC_ControlPeripheral` extracts `busID = (peripheral & RCC_BUS_MASK) >> RCC_BUS_OFFSET` and switches on it, then applies `peripheral` (including the bus bits!) as a mask:

```c
case RCC_AHB1: RCC->AHB1ENR.REG |= peripheral; break;
```

For AHB1 that is fine because the bus field is `00`. For APB2 it would set bits 30 and 31 of `APB2ENR` — which are reserved and read-as-zero on this part, so the write is harmless but not clean. A masked write (`peripheral & RCC_PERIPH_MASK`) would be correct; `RCC_PERIPH_MASK` is even defined in the header for exactly this purpose and is never used.

The bootloader only ever enables AHB1 (GPIO) and APB2 (USART1) peripherals, and both work, so this has no observable effect.

### 45.2 `RCC_WaitPeripheralReady` is a no-op in disguise

```c
static STD_ReturnType RCC_WaitPeripheralReady(RCC_Peripheral_t peripheral, uint32_t timeout){
    while(((RCC->AHB1ENR.REG & peripheral) == 0) && tickStart < timeout){ tickStart++; }
    ...
}
```

It waits for the *enable bit it just set* to read back as set. On STM32F4 there is a documented delay of up to two peripheral clock cycles before a newly-enabled peripheral's registers are accessible, but the ENR bit itself reads back immediately. So this loop exits on the first iteration and provides none of the settling delay it appears to. The correct idiom is a dummy read-back of the ENR register (which introduces the required stall) — which, coincidentally, is exactly what the `&` in the loop condition does. So it works, by accident.

## 46. GPIO Driver

`src/MCAL/interface/gpio.c` — 218 lines. Conventional register-level GPIO.

```c
STD_ReturnType GPIO_Init(GPIO_t* gpio);
STD_ReturnType GPIO_DeInit(GPIO_t* gpio);
STD_ReturnType GPIO_SetMode(GPIO_t* gpio, GPIO_Mode_t mode);
STD_ReturnType GPIO_SetSpeed(GPIO_t* gpio, GPIO_Speed_t speed);
STD_ReturnType GPIO_SetPull(GPIO_t* gpio, GPIO_Pull_t pull);
STD_ReturnType GPIO_SetOutputType(GPIO_t* gpio, GPIO_OutputType_t outputType);
STD_ReturnType GPIO_SetAltFunction(GPIO_t* gpio, GPIO_AltFunc_t altFunc);
STD_ReturnType GPIO_WritePin(GPIO_t* gpio, GPIO_PinState_t state);
STD_ReturnType GPIO_ReadPin(GPIO_t* gpio, GPIO_PinState_t* state);
STD_ReturnType GPIO_TogglePin(GPIO_t* gpio);
```

Ports are indexed through a const table:

```c
GPIOx_t* const GPIO_PORTS[GPIO_PORT_MAX] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE};
```

`GPIO_PORT_MAX` is 5, so `GPIOH` (which exists on the F401 and carries the HSE oscillator pins) is not addressable. Not needed here.

`GPIO_Init` applies fields in an order that matters:

```
   1. MODER            ← via GPIO_SetMode
   2. if OUTPUT:  ODR = 0 first (GPIO_WritePin RESET), then OTYPER, then OSPEEDR
      if AF:      OTYPER, then OSPEEDR
   3. PUPDR
   4. if AF:      AFRL/AFRH
```

Driving the output low *before* configuring the output type avoids a glitch: if the pin were left in its reset state (input, floating) and then switched to push-pull output with `ODR` holding a stale 1, the pin would drive high for the handful of cycles until the intended value was written. Writing `ODR = 0` first makes the transition clean. Only the OUTPUT path does this; the AF path does not need it because the peripheral drives the pin.

Writes use `BSRR` rather than read-modify-write on `ODR`:

```c
if(state == GPIO_PIN_SET) GPIOx->BSRR.REG = pinMask;
else                      GPIOx->BSRR.REG = (pinMask << 16);
```

`BSRR` is atomic in hardware — no read-modify-write, so no interrupt-safety concern (moot here, since there are no interrupts).

`GPIO_TogglePin` is *not* atomic: it reads `ODR`, decides, then writes `BSRR`. Again moot without interrupts.

`GPIO_SetMode` and friends mutate the caller's struct as a side effect (`gpio->mode = mode;`). That makes the struct a live handle rather than a pure configuration record — `gpio_leds[]` in `led.c` relies on this, since `LED_SetState` passes the same struct back to `GPIO_WritePin`.

## 47. UART Driver (MCAL)

`src/MCAL/interface/uart.c` — 261 lines. Supports USART1, USART2 and USART6; the bootloader uses only USART1.

```c
STD_ReturnType UART_Init(const UART_Config_t* uartObj, SYSTICK_ClockSource_t clockSource);
STD_ReturnType UART_DeInit(const UART_Config_t* uartObj);
STD_ReturnType UART_SendChar(const UART_Config_t* uartObj, uint8_t data, uint32_t timeoutMS);
STD_ReturnType UART_SendBuffer(const UART_Config_t* uartObj, Buffer_t* buffer, uint32_t timeoutMS);
STD_ReturnType UART_ReceiveChar(const UART_Config_t* uartObj, uint8_t* data, uint32_t timeoutMS);
STD_ReturnType UART_ReceiveBuffer(const UART_Config_t* uartObj, Buffer_t* buffer, uint32_t timeoutMS);
```

### 47.1 Baud rate computation

```c
usartdiv = (clockFreq) / (16 * baudRate);
fraction = ((clockFreq % (16 * baudRate)) * 16 + (16 * baudRate)/2) / (16 * baudRate);
usartdiv = (usartdiv << 4) | fraction;
uartObj->UartInstance->BRR = usartdiv;
```

For USART1 at 84 MHz and 115200 baud:

```
   16 × 115200            = 1,843,200
   84,000,000 / 1,843,200 = 45         (mantissa)
   84,000,000 % 1,843,200 = 1,056,000
   (1,056,000 × 16 + 921,600) / 1,843,200
     = (16,896,000 + 921,600) / 1,843,200
     = 17,817,600 / 1,843,200
     = 9                               (fraction, rounded)

   BRR = (45 << 4) | 9 = 0x2D9 = 729

   Actual baud = 84,000,000 / (16 × 45.5625) = 115,274
   Error       = +0.06 %                            ✓ well within tolerance
```

The `+ (16*baudRate)/2` term is round-to-nearest rather than truncation, which is what keeps the error at 0.06 % instead of 0.5 %.

### 47.2 The clock frequency is passed in as an enum, not measured

```c
switch(clockSource){
    case SYSTICK_CLOCK_SOURCE_HSI:      clockFreq = 16000000; break;
    case SYSTICK_CLOCK_SOURCE_HSE:      clockFreq = 25000000; break;
    case SYSTICK_CLOCK_SOURCE_PLL_MAX:  clockFreq = 84000000; break;
}
if(uartNum == 1){ clockFreq /= 2; }   /* USART2 is on APB1 (42 MHz) */
```

This is an *assertion* about the clock, not a query of it. If `RCC_ConfigureClock` silently failed ([§6.2](#62-timeouts-are-loop-counts-not-milliseconds)) and the core is still on HSI at 16 MHz, `UART_Init` will still compute BRR for 84 MHz and the actual baud rate becomes 115,274 × (16/84) ≈ 21,957 — completely unusable. There is no cross-check.

Reading `RCC->CFGR.SWS` and the PLL dividers to derive the true frequency would make this self-consistent. It is a small amount of code and would remove an entire class of silent failure.

The `uartNum == 1` special case hardcodes that USART2 is the APB1 instance. USART1 and USART6 are on APB2. Correct for this part.

### 47.3 The RX pin is derived, not configured

```c
GPIO_t uart_rx_pin = {
    .port = uartObj->port,
    .pin  = uartObj->txPin + 1,          /* ← assumption */
    ...
};
```

The config struct has a `txPin` field but no `rxPin`; RX is assumed to be TX+1. That holds for the STM32F401's standard mappings:

| USART | TX | RX | TX+1 == RX? |
|---|---|---|---|
| USART1 | PA9 | PA10 | ✓ |
| USART2 | PA2 | PA3 | ✓ |
| USART6 | PC6 | PC7 | ✓ |

It does *not* hold for the alternate mappings (USART1 on PB6/PB7 is TX=PB6, RX=PB7 — which happens to also work; USART1 remap to PA15/PB3 does not). Fine for this board, brittle in general.

The RX pin is configured with `GPIO_PULLUP`, which is correct — an idle UART line is high, and a pull-up prevents a floating RX from generating spurious start bits if the gateway is disconnected.

Alternate function selection:

```c
if(uartNum == 0 || uartNum == 1) altFunc = GPIO_AF7_USART1_2;
else                             altFunc = GPIO_AF8_USART6;
```

### 47.4 Frame configuration

```c
CR1 &= ~(1 << 12);   /* M = 0 → 8 data bits */
CR1 &= ~(1 << 10);   /* PCE = 0 → no parity */
CR2 &= ~(0b11 << 12);/* STOP = 00 → 1 stop bit */
CR1 |= (1 << 3);     /* TE  — transmitter enable */
CR1 |= (1 << 2);     /* RE  — receiver enable */
CR1 |= (1 << 13);    /* UE  — USART enable */
```

Oversampling by 16 (`OVER8 = 0`) is the reset default and is never changed, which matches the BRR computation above.

`UE` is set **before** the GPIO pins are configured. That ordering is fine — the peripheral is enabled but the pins are still in their reset state (analog/floating) so nothing is driven until `GPIO_Init` runs.

### 47.5 `UART_SendBuffer` / `UART_ReceiveBuffer` use `uint8_t` loop counters

```c
for(uint8_t idx = 0; idx < buffer->length; idx++){ ... }
```

`Buffer_t::length` is `uint8_t`, so the loop cannot overflow. But it caps any single transfer at 255 bytes — fine for this protocol, where the largest reply is 3 bytes and the largest receive is 139.

The comment `/* index, don't mutate pointer */` on the send loop marks a fixed bug: an earlier version incremented `buffer->data` itself, which corrupted the descriptor for the next call.

## 48. HSerial (HAL)

`src/HAL/interface/hserial.c` — 191 lines. Thin buffered wrapper over the UART MCAL.

```c
STD_ReturnType HSerial_Init(HSerial_Config_t* cfg, SYSTICK_ClockSource_t clockSource);
STD_ReturnType HSerial_DeInit(HSerial_Config_t* cfg);
STD_ReturnType HSerial_SendBuffer(HSerial_Config_t* cfg, uint32_t timeoutMS);      /* blocking */
STD_ReturnType HSerial_ReceiveBuffer(HSerial_Config_t* cfg, uint32_t timeoutMS);   /* blocking */
STD_ReturnType HSerial_SendBufferIT(HSerial_Config_t* cfg);                        /* unused */
STD_ReturnType HSerial_ReceiveBufferIT(HSerial_Config_t* cfg);                     /* unused */
```

The bootloader uses only the two blocking functions, which are pure pass-throughs:

```c
STD_ReturnType HSerial_SendBuffer(HSerial_Config_t* hserialConfig, uint32_t timeoutMS){
    return UART_SendBuffer(hserialConfig->uartConfig,
                           (Buffer_t*)hserialConfig->txBuffer, timeoutMS);
}
```

The cast `(Buffer_t*)hserialConfig->txBuffer` works because `HSerial_Buffer_t` is a union whose first member is a struct with the same layout as `Buffer_t`:

```c
typedef struct{
    union{
        struct{ uint8_t* data; uint8_t length; uint8_t index; } buffer;
        struct{ uint8_t* src; uint8_t* dest; uint32_t length; } dmaBuffer;
    };
} HSerial_Buffer_t;
```

Type-punning through a union's common initial sequence. Legal in C11 for structs sharing a common initial sequence within a union, though `Buffer_t` is not itself a member of the union — so this is strictly implementation-defined, not standard-guaranteed. It works on GCC/ARM. Not worth changing; worth knowing.

### 48.1 The interrupt path is dead code

`hserial.c` implements a complete IRQ-driven TX/RX machine:

```c
volatile uint8_t txBuffer[3][100] = {0};          /* 300 B, per-instance staging */
volatile uint8_t* requestedTxBuffer[3] = {NULL};
volatile uint32_t requestedTxLength[3] = {0};
...
void USART_Handler(uint8_t uartNum, UART_Instance_t uartInstance){ ... }
void USART1_ITHandler(void){ USART_Handler(0, UART1); }
void USART2_ITHandler(void){ USART_Handler(1, UART2); }
void USART6_ITHandler(void){ USART_Handler(2, UART6); }
```

None of it runs. The handler names end in `_ITHandler`, not `_IRQHandler`, so the linker does not bind them to the vector table entries; the NVIC lines are never enabled; and `UART_SetTXIE`/`UART_SetRXIE` (called by the handler) are not even declared in `uart.h` — they exist only as references, which means this file would not compile if those functions were not defined somewhere. They are not.

**This implies `hserial.c`'s interrupt half does not currently compile as part of the bootloader build**, or the symbols resolve from elsewhere. Given the image builds and links, the most likely explanation is that `--gc-sections` removes the unreferenced `USART_Handler` before the undefined references matter — but that only works if the compiler emits them into their own sections and nothing references them. Worth verifying if the build is ever touched.

There is also a name collision worth flagging: `hserial.c` declares `volatile uint8_t txBuffer[3][100]` at file scope, while `bootloader.c` declares `uint8_t txBuffer[3]` at file scope. **Both are non-static, both are named `txBuffer`, and both are in the same link.** This is a tentative-definition collision that C's common-symbol handling resolves by merging them into a single 300-byte object — meaning `bootloader.c`'s `txBuffer[0..2]` and `hserial.c`'s `txBuffer[0][0..2]` are *the same memory*.

With `-fcommon` (the GCC default before version 10) this links silently. With `-fno-common` (default from GCC 10 onward) it is a multiple-definition error. The PlatformIO toolchain here is GCC 10.2 (`toolchain-gccarmnoneeabi@1.70201.0`), which defaults to `-fno-common` — yet the build succeeds, which means the types differ enough that they are being treated as distinct symbols, or one is being garbage-collected. Either way this is an accident waiting to be tripped over. Marking both `static` is the correct fix and costs nothing. Recorded in [§57](#57-known-gaps).

## 49. FLASH Driver

`src/MCAL/interface/flash.c` — 147 lines.

```c
STD_ReturnType FLASH_Read(uint32_t address, uint32_t* data, uint32_t length);
STD_ReturnType FLASH_Write(uint32_t address, uint32_t* data, uint32_t length);
STD_ReturnType FLASH_Erase(FLASH_Sector_Number_t sectorNumber);
STD_ReturnType FLASH_SetLatency(FLASH_Latency_t latency);
```

`length` is in **32-bit words**, not bytes, for both read and write. Getting this wrong by a factor of four is the most likely misuse.

### 49.1 Write sequence

```c
FLASH->SR |= FLASH_ERROR_FLAGS;          /* clear sticky errors (write-1-to-clear) */
if(FLASH->SR & (1 << 16)) return STD_BUSY;   /* BSY */

FLASH->KEYR = 0x45670123;                /* unlock key 1 */
FLASH->KEYR = 0xCDEF89AB;                /* unlock key 2 */

FLASH->CR &= ~(0b11 << 8);               /* clear PSIZE */
FLASH->CR |=  (0b10 << 8);               /* PSIZE = 32-bit */
FLASH->CR |=  (1 << 0);                  /* PG */

for(each word){
    *((volatile uint32_t*)addr) = data[i];
    while(FLASH->SR & (1 << 16));        /* wait BSY */
    if(FLASH->SR & FLASH_ERROR_FLAGS){ ...cleanup...; return STD_ERROR; }
}

FLASH->CR &= ~(1 << 0);                  /* clear PG */
FLASH->CR |=  (1 << 31);                 /* LOCK */
```

`FLASH_ERROR_FLAGS` is `(1<<4)|(1<<5)|(1<<6)|(1<<7)|(1<<8)` = WRPERR, PGAERR, PGPERR, PGSERR, RDERR.

`PSIZE = 0b10` selects 32-bit programming, which requires **VDD ≥ 2.7 V**. At 3.3 V this is satisfied. Below 2.7 V the part would need PSIZE = 8 or 16 bits and the driver has no provision for that.

Validation before any register touch:

```c
if(data == NULL || length == 0)                          return STD_ERROR;
if(address < FLASH_SECTOR_0 || (address + length*4) > FLASH_END) return STD_ERROR;
if(address % 4 != 0)                                      return STD_ERROR;
```

`FLASH_END` is `0x0803FFFF`, the last *valid byte*. The check `address + length*4 > FLASH_END` therefore permits a write whose last byte is at `FLASH_END + 1` — an off-by-one that allows one word past the end. Unreachable in practice (the OTA engine never gets near the top of Flash) but wrong. `>=` against `0x08040000` would be correct.

Notice there is **no protection against writing into sectors 0–1** — the bootloader's own code. `FLASH_Write(0x08000000, ...)` would be accepted. The only thing preventing self-destruction is that `ota.flash_addr` starts at `FLASH_APP_START` and only increases.

### 49.2 The unbounded BSY waits

```c
while(FLASH->SR & (1 << 16));
```

appears three times in `FLASH_Write` and once in `FLASH_Erase`, with no timeout in any of them. If the Flash controller wedged — a supply brownout mid-erase is the realistic cause — the bootloader would hang forever with no watchdog to recover it. Adding a bounded counter would turn an unrecoverable hang into a reportable error.

### 49.3 Erase

```c
STD_ReturnType FLASH_Erase(FLASH_Sector_Number_t sectorNumber){
    if(sectorNumber < FLASH_SECTOR_NUMBER_0 ||
       (sectorNumber > FLASH_SECTOR_NUMBER_5 && sectorNumber != FLASH_MASS_ERASE)){
        return STD_ERROR;
    }
    ...
    if(sectorNumber == FLASH_MASS_ERASE){
        FLASH->CR |= (1 << 15);           /* MER */
    } else {
        FLASH->CR |= (1 << 1);            /* SER */
        FLASH->CR &= ~(0xF << 3);         /* clear SNB */
        FLASH->CR |= (sectorNumber << 3); /* SNB */
    }
    FLASH->CR |= (1 << 16);               /* STRT */
    while(FLASH->SR & (1 << 16));
    ...
}
```

The range check `sectorNumber < FLASH_SECTOR_NUMBER_0` compares an unsigned enum against 0 — always false. Harmless; the upper bound does the real work.

`FLASH_MASS_ERASE` (`0xFF`) is accepted and would erase **everything including the bootloader**, leaving a brick recoverable only by ST-Link. Nothing in the bootloader calls it. It is a loaded gun sitting in the API.

## 50. CRC Driver

`src/MCAL/interface/crc.c` — 41 lines, the smallest driver.

```c
STD_ReturnType CRC_Init(void);       /* enables the AHB1 clock */
STD_ReturnType CRC_DeInit(void);     /* disables it */
STD_ReturnType CRC_Accumulate(const uint32_t* buffer, uint32_t length, uint32_t* crcResult);
STD_ReturnType CRC_Calculate(const uint32_t* buffer, uint32_t length, uint32_t* crcResult);
STD_ReturnType CRC_Reset(void);      /* CR.RESET = 1 */
```

```c
STD_ReturnType CRC_Accumulate(const uint32_t* buffer, uint32_t length, uint32_t* crcResult){
    for (index = 0U; index < length; index++){
        CRC->DR = buffer[index];
    }
    *crcResult = CRC->DR;
    return STD_SUCCESS;
}
```

Writing to `DR` feeds a word in; reading from `DR` returns the accumulated CRC. That dual-purpose register is the whole peripheral.

`CRC_Calculate` is the safe wrapper (`CRC_Reset()` then `CRC_Accumulate()`) and is **never called**. `Bootloader_CRCVerify` uses `CRC_Accumulate` directly with a trailing reset, which is the fragile pattern described in [§26.3](#263-the-reset-ordering).

`CRC32_POLYNOMIAL 0x04C11DB7` is defined in the header for documentation only — the hardware polynomial is fixed and cannot be written on the F4.

## 51. SysTick Driver

`src/Core/systick.c` — 175 lines. Used only for `SYSTICK_DelayMS(1000)` in `main()`.

```c
STD_ReturnType SYSTICK_Init(SYSTICK_ClockSource_t clockSource);
STD_ReturnType SYSTICK_Stop(void);
STD_ReturnType SYSTICK_Enable(void);
STD_ReturnType SYSTICK_Disable(void);
STD_ReturnType SYSTICK_DelayMS(uint16_t delayMillieSec);
STD_ReturnType SYSTICK_DelayUS(uint32_t delayMicroSec);
STD_ReturnType SYSTICK_SingleInterval(uint16_t ms, FuncPtr cb);     /* unused */
STD_ReturnType SYSTICK_PeriodicInterval(uint16_t ms, FuncPtr cb);   /* unused */
STD_ReturnType SYSTICK_GetRemainingTicks(uint32_t* remTicks);
STD_ReturnType SYSTICK_GetElapsedTicks(uint32_t* elapsedTicks);
```

### 51.1 The /8 prescaler

`configuration/Core/systick_conf.h`:

```c
#define SYSTICK_CLOCK_SOURCE      SYSTICK_CLOCK_SOURCE_PROCESSOR_DIV8
#define SYSTICK_MAX_TICKS         0xFFFFFFUL
```

`SYSTICK_Init` first sets `CSR.CLKSOURCE = 1` (processor clock), records the nominal rate in kHz, and then — because the config selects `/8` — divides the recorded rate and clears `CLKSOURCE` again:

```c
STK->CSR |= (1 << SYSTICK_CSR_CLKSOURCE_BIT_POS);
if(clockSource == SYSTICK_CLOCK_SOURCE_PLL_MAX){ sysClockSourceKHz = 84000; }
...
#elif (SYSTICK_CLOCK_SOURCE == SYSTICK_CLOCK_SOURCE_PROCESSOR_DIV8)
    sysClockSourceKHz /= 8;                                    /* 84000 → 10500 */
    STK->CSR &= ~(1 << SYSTICK_CSR_CLKSOURCE_BIT_POS);
#endif
```

So SysTick runs at 10.5 MHz. The 24-bit reload register (`SYSTICK_MAX_TICKS = 0xFFFFFF` = 16,777,215) then covers:

```
   16,777,215 / 10,500 kHz ≈ 1,598 ms   per reload
```

`SYSTICK_DelayMS(1000)` needs `1000 × 10,500 - 1` = 10,499,999 ticks, which fits in one reload. Good.

Without the /8 (i.e. at 84 MHz) a single reload would cover only 199 ms, and `SYSTICK_DelayMS(1000)` would take the multi-reload loop path. The /8 exists to make the common case a single load.

### 51.2 The delay primitive

```c
static STD_ReturnType delay(uint32_t delayTicks){
    STK->RVR = delayTicks;
    STK->CVR = 0;                       /* writing any value clears COUNTFLAG */
    STK->CSR |= (1 << SYSTICK_CSR_ENABLE_BIT_POS);
    while(!((STK->CSR >> SYSTICK_CSR_COUNTFLAG_BIT_POS) & 1U));
    return STD_SUCCESS;
}
```

`COUNTFLAG` (CSR bit 16) sets when the counter reaches zero and **clears on read of CSR**. The polling loop reads CSR each iteration, so it clears the flag as a side effect of testing it — which is exactly right for a one-shot: the loop exits on the first read that sees it set.

The commented-out `//ret = SYSTICK_Stop();` means the counter keeps running after the delay returns. Harmless (nothing else uses SysTick) but it means a subsequent `delay()` call starts from an arbitrary `CVR` — which is fine because writing `RVR` and `CVR` reloads it.

### 51.3 The multi-reload loop drops the remainder

```c
uint32_t fullDelay = (delayMillieSec * sysClockSourceKHz) - 1;
while(fullDelay > SYSTICK_MAX_TICKS){
    reqReloadVal = SYSTICK_MAX_TICKS;
    fullDelay -= SYSTICK_MAX_TICKS;
    ret = delay(reqReloadVal);
}
ret = delay(fullDelay);
```

Each iteration of `delay(SYSTICK_MAX_TICKS)` actually waits `MAX_TICKS + 1` ticks (the counter counts from RVR down through 0), so the accumulated delay overshoots by one tick per reload — about 95 ns each. Irrelevant.

`delayMillieSec` is `uint16_t` and `sysClockSourceKHz` is `uint32_t`, so the product promotes to `uint32_t`. Maximum: `65535 × 10500` = 688,117,500, which fits. No overflow.

`SYSTICK_DelayUS` computes `delayMicroSec * (sysClockSourceKHz / 1000)` = `us × 10`. The integer division `10500 / 1000 = 10` truncates, so microsecond delays run 5 % short (10 ticks/µs instead of 10.5). Not used by the bootloader.

## 52. LED Driver

`src/HAL/interface/led.c` (61 lines) plus `src/HAL/config/led_cfg.c` (8 lines).

```c
const LED_Config_t LEDS[LED_LEN] = {
    [LED_0] = { .port = GPIO_PORTA, .pin = GPIO_PIN_0, .activeState = LED_ACTIVE_HIGH, .isPP = 1 },
    [LED_1] = { .port = GPIO_PORTA, .pin = GPIO_PIN_1, .activeState = LED_ACTIVE_HIGH, .isPP = 1 },
};
```

`LED_Config_t` uses bitfields to fit each entry in two bytes:

```c
typedef struct LED_Config {
    uint8_t port        : 4;
    uint8_t pin         : 4;
    uint8_t activeState : 1;
    uint8_t isPP        : 1;
} LED_Config_t;
```

Four bits for `port` caps it at 16 ports; four for `pin` at 16 pins. Both adequate.

The polarity trick in `LED_SetState`:

```c
GPIO_PinState_t gpioState = state ^ LEDS[ledName].activeState;
```

XOR against the active-state flag. For `LED_ACTIVE_HIGH` (0) it is the identity; for `LED_ACTIVE_LOW` (1) it inverts. Branch-free and correct for both.

`LED_Toggle` has a bug: the bounds check sets `ret` but does not return, so an out-of-range `ledName` still indexes `gpio_leds[]`:

```c
STD_ReturnType LED_Toggle(uint8_t ledName){
    if(ledName >= LED_LEN){ ret = STD_ERROR; }   /* ← no return */
    ret = GPIO_TogglePin(&gpio_leds[ledName]);   /* ← out-of-bounds read */
    return ret;
}
```

`LED_Toggle` is never called in the bootloader build, so it is latent. `LED_SetState` and `LED_DeInit` both have the correct `else` structure.

## 53. STD_Types and Return Codes

`lib/STD_Types.h`:

```c
typedef unsigned char       uint8_t;
typedef unsigned short int  uint16_t;
typedef unsigned long int   uint32_t;
typedef signed char         int8_t;
typedef signed short int    int16_t;
typedef signed long int     int32_t;
typedef float               float32_t;
typedef double              float64_t;
```

These are hand-rolled rather than `#include <stdint.h>`. On ARM EABI `unsigned long int` is 32 bits, so `uint32_t` is correct — but it is a *different type* from `<stdint.h>`'s `uint32_t` (which is `unsigned int`), and both are in scope in `aes_gcm.c` and `firmware_flashing.c`, which include `<stdint.h>` via the mbedTLS headers.

C forbids redefining a typedef with a different type. That this compiles means the two headers are never both fully in scope in a translation unit that would notice — `firmware_flashing.h` includes `<stdint.h>` and does *not* include `STD_Types.h`, while `flash.h` includes `STD_Types.h` and not `<stdint.h>`. `firmware_flashing.c` includes both indirectly and does compile, which suggests the include guards happen to order things favourably. This is exactly the kind of arrangement that breaks when someone adds an `#include`.

The application solved this properly — its `STD_TYPES.h` defers to `<stdint.h>` when `__GNUC__` is defined and only defines the `sint*_t` aliases itself. Porting that approach here would remove the hazard.

`STD_ReturnType`:

```c
typedef enum {
    STD_ERROR = 0,
    STD_SUCCESS,        /* 1 */
    STD_TIMEOUT,        /* 2 */
    STD_BUSY,           /* 3 */
    STD_ACK,            /* 4 */
    STD_NACK,           /* 5 */
    STD_CMD_FINISHED    /* 6 */
} STD_ReturnType;
```

`STD_ERROR = 0` and `STD_SUCCESS = 1` is the inverse of the C convention (0 = success). Consistent within this codebase, but a source of confusion when reading alongside the application, which uses `*_OK = 0` for every driver.

---

# Part VIII — Operations

## 54. Flashing the Bootloader

The bootloader itself is only ever flashed over SWD — there is no way to update it over the air, by design (a self-updating bootloader that fails mid-update bricks the node).

```bash
pio run -d Bootloader -t upload
```

Or with `stlink-tools` directly, as the project README documents:

```bash
st-flash write Bootloader/.pio/build/genericSTM32F401CC/firmware.bin 0x08000000
```

Reading Flash back for inspection:

```bash
st-flash read app_dump.bin 0x08008000 10000
```

```bash
xxd app_dump.bin > flash.hex
```

Erasing everything (bootloader included — full recovery from a brick):

```bash
st-flash erase
```

### 54.1 Order matters when provisioning a blank part

```
   1. Flash the bootloader to 0x08000000
        → sectors 2-5 are erased (0xFF), App_IsValid() fails
        → the node boots, stays resident, PA0 pulses 1 s
   2. Either:
        a) Flash the application over SWD to 0x08008000, or
        b) Let the gateway push it: B1 → B3 → B5 → B4
```

Flashing the application first and the bootloader second also works, because `pio run -t upload` for the bootloader only programs sectors 0–1.

## 55. Bench Procedures

### 55.1 Confirm the bootloader is alive

```
   1. Reset the board with the gateway disconnected.
   2. Scope PA0.
   Expected: a single clean 1.00 s high pulse, then low.
   If the pulse is ~5.25 s → the core is at 16 MHz, HSE failed to start.
   If there is no pulse    → either the app was launched (check for
                             telemetry on PA9) or the bootloader faulted.
```

### 55.2 Confirm the protocol end to end, without the gateway

Any USB-UART adapter on PA9/PA10 at 115200 8N1 will do.

| Send (hex) | Expect | Meaning |
|---|---|---|
| `AA EB` | `EE FB` | Bootloader is resident |
| `05 10 <crc>` | `AA` then `EE 00 00` | Get-version handshake works |
| `05 13 <crc>` | `AA` then (≈2-3 s) `EE E1` | Erase works |
| `07 12 00 00 <crc>` | `AA` then `EE 00` | No app present (post-erase) |

The CRC values must be computed with the 32-iteration algorithm from [§26](#26-crc32-verification). A quick Python reference:

```python
def bl_crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(32):
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if crc & 0x80000000 else (crc << 1) & 0xFFFFFFFF
    return crc
```

For `05 10`: `bl_crc32(bytes([0x05,0x10]))`, appended little-endian.

### 55.3 Confirm the crypto against known vectors

Swap in the test main and watch PA0:

```bash
cd Bootloader && mv src/main.c src/main.c.bak && cp tests/encrypt.main.c src/main.c
```

```bash
pio run -d Bootloader -t upload
```

A 2.5 Hz blink means both `chunk0` (2032 B, index 0) and `chunk1` (1476 B, index 1) decrypted and their tags verified. No blink means at least one failed — which, given the vectors are fixed, points at a key mismatch or a broken mbedTLS build rather than at data corruption.

Restore afterwards:

```bash
cd Bootloader && mv src/main.c.bak src/main.c
```

### 55.4 Measure the erase time

```
   1. Put a scope on PA0 — but note the bootloader does not toggle it
      during erase, so instead watch the UART TX line (PA9).
   2. Send 05 13 <crc>.
   3. Measure from the trailing edge of the ACK byte (0xAA) to the
      leading edge of the reply (0xEE).
   Expected: 2.0–3.1 s. Anything above 4 s means the part is
   degrading and the gateway's 5 s timeout is at risk.
```

## 56. Failure Modes and Recovery

```
   Symptom: node is silent, no telemetry, no LED pulse
   ───────────────────────────────────────────────────
     ├─ Is PA0 pulsing 1 s after reset?
     │    yes → bootloader is resident and waiting.
     │          The app is missing or invalid. Push one.
     │    no  → continue
     │
     ├─ Does the gateway's AA EB get 0xEE 0xFB?
     │    yes → bootloader is alive but the LED path is broken
     │    no  → continue
     │
     ├─ Attach ST-Link. Does it connect?
     │    no  → check power, NRST, BOOT0 (must be low)
     │    yes → read 0x08000000; is it the bootloader?
     │             no  → reflash the bootloader
     │             yes → halt and read PC. If it is inside
     │                   HardFault_Handler, the app faulted
     │                   after a successful jump.
```

| Failure | Cause | Recovery |
|---|---|---|
| Stuck in bootloader forever | Boot flag never clears | Impossible by construction — the flag is cleared on read. If observed, RAM is failing. |
| Boots into a faulting app | Truncated image passed `App_IsValid()` | Power cycle, send `AA EB` early (the app's Thread 5 runs at priority 4 and may catch it), or ST-Link |
| Erase reports success but write fails | Second OTA in one session ([§35.1](#351-a-second-ota-in-the-same-session-always-fails)) | Send `AA EB` to force a reset, then retry the full sequence |
| Every write packet NACKs | CRC algorithm mismatch on the host | Verify with the Python reference in [§55.2](#552-confirm-the-protocol-end-to-end-without-the-gateway) |
| Tag failure mid-transfer | Packet loss, or a key/nonce mismatch | Reset and retry; if it fails at the same chunk every time, suspect the encryptor |
| Node hangs during erase | `FLASH_Erase` BSY wait with no timeout ([§49.2](#492-the-unbounded-bsy-waits)) | Power cycle. No watchdog is running. |
| Wrong baud rate | HSE failed to start; UART configured for 84 MHz ([§6.2](#62-timeouts-are-loop-counts-not-milliseconds)) | Check the crystal and load capacitors |

## 57. Known Gaps

Ranked by consequence.

### Critical

| # | Gap | Detail |
|---|---|---|
| G1 | **Second OTA in one bootloader session always fails** | `ota.state` never returns to `IDLE` without a reset, so `OTA_Init()` is skipped and every packet is rejected — after the erase has already destroyed the old image. [§35.1](#351-a-second-ota-in-the-same-session-always-fails) |
| G2 | **GCM nonce reused across images** | Chunk *N* of every image shares a nonce under a fixed key. Enables keystream recovery and tag forgery. [§38.1](#381-nonce-reuse-analysis) |
| G3 | **A truncated image boots** | `App_IsValid()` checks only words 0 and 1. Nothing verifies total length or whole-image integrity. [§14.1](#141-what-it-does-not-catch) |

### High

| # | Gap | Detail |
|---|---|---|
| G4 | **No read-out protection** | RDP level 0 leaves the AES key extractable over SWD. [§38.2](#382-the-key-is-a-compile-time-constant-in-flash) |
| G5 | **`FLASH_Erase` and `FLASH_Write` have unbounded BSY waits** | A wedged Flash controller hangs the bootloader with no watchdog. [§49.2](#492-the-unbounded-bsy-waits) |
| G6 | **No boot-attempt counter** | An app that faults immediately produces a dead node with no automatic recovery. [§13.1](#131-a-subtlety-worth-internalising) |
| G7 | **Erase result is not accumulated** | Only the last of four `FLASH_Erase` calls is checked. [§24.1](#241-only-the-last-erase-result-is-checked) |
| G8 | **`txBuffer` symbol collision** between `bootloader.c` and `hserial.c` | Two non-static file-scope arrays with the same name in one link. [§48.1](#481-the-interrupt-path-is-dead-code) |

### Medium

| # | Gap | Detail |
|---|---|---|
| G9 | **`txBuffer[3]` allows only a 2-byte reply payload** | Exactly saturated by the version reply. Any 3-byte reply overflows. [§19.2](#192-stm32--host-reply) |
| G10 | **No write verify** | A missed erase produces a silently corrupt image. [§33.3](#333-no-erase-ever) |
| G11 | **`AES_GCM_RunTest()` would overflow the stack** | 6,096 B of locals against a 1,024 B minimum stack. [§42.1](#421-aes_gcm_runtest--the-round-trip-test) |
| G12 | **UART baud assumes the clock succeeded** | No cross-check against `RCC->CFGR.SWS`. [§47.2](#472-the-clock-frequency-is-passed-in-as-an-enum-not-measured) |
| G13 | **Linker declares the full 256 KB as FLASH** | Bootloader growth past 32 KB would silently overlap the application. [§9](#9-linker-script-walkthrough) |
| G14 | **`CRC_Reset()` after accumulate, not before** | Correct only because every call site is disciplined. [§26.3](#263-the-reset-ordering) |
| G15 | **RCC ready-timeouts are loop counts** | 500 iterations ≈ 40 µs against an HSE startup of 1-2 ms. [§6.2](#62-timeouts-are-loop-counts-not-milliseconds) |

### Low / Informational

| # | Gap | Detail |
|---|---|---|
| G16 | **AAD duplicates the nonce** | Authenticates nothing new. [§39](#39-the-aad-choice) |
| G17 | **8 KB of AES tables in RAM** | `MBEDTLS_AES_ROM_TABLES` would move them to Flash. [§41.2](#412-ram) |
| G18 | **Address constants duplicated** between `bootloader.h` and `main.c` | No compile-time consistency check. [§7](#7-flash-partition-map) |
| G19 | **`FLASH_MASS_ERASE` is reachable through the API** | Would erase the bootloader. Never called. [§49.3](#493-erase) |
| G20 | **`LED_Toggle` misses a `return` on the bounds check** | Latent out-of-bounds read; never called. [§52](#52-led-driver) |
| G21 | **Chunk/packet indices in `0x14` are unused** | A free packet-loss check is being discarded. [§25.1](#251-the-chunk-and-packet-indices-are-decorative) |
| G22 | **`tests/encrypt.main.c` carries the obsolete Flash-flag boot logic** | Swapping it in silently reverts the boot mechanism. [§42.2](#422-testsencryptmainc--the-known-answer-test) |
| G23 | **Unknown command bytes get no reply** | `0x11` is defined but unimplemented; the switch has no `default`. [§20.1](#201-points-worth-noting) |
| G24 | **`STD_Types.h` shadows `<stdint.h>`** | Works by include-order luck. [§53](#53-std_types-and-return-codes) |
| G25 | **`build_type = debug`** | Ships an unoptimised image. Deliberate, but worth revisiting if mbedTLS grows. [§10](#10-build-system) |
| G26 | **`bootloader.c` includes `flash_priv.h`** unnecessarily | Layering violation with no purpose. [§44](#44-layering-model) |
| G27 | **`hserial.c` interrupt path is dead code** | ~90 lines and 300 B of RAM for a machine that never runs. [§48.1](#481-the-interrupt-path-is-dead-code) |

---

# Appendices

## Appendix A — Command Byte Reference

### Host → STM32

| Byte | Name | Packet | CRC over | Reply |
|---|---|---|---|---|
| `0xAA` | `ENTER_BOOTLOADER_CMD` | `AA EB` (2 B, no CRC) | — | `EE FB` |
| `0x10` | `BL_GET_VERSION` | `05 10 <crc32>` (6 B) | bytes 0-1 | `AA` + `EE 00 00` |
| `0x11` | `BL_GET_PROTECTION_LEVEL` | — | — | **not implemented, no reply** |
| `0x12` | `BL_JUMP_TO_ADDR_CMD` | `07 12 <maj> <min> <crc32>` (8 B) | bytes 0-3 | `AA` + `EE A0` / `EE 00` |
| `0x13` | `BL_FLASH_ERASE_CMD` | `05 13 <crc32>` (6 B) | bytes 0-1 | `AA` + `EE E1` / `EE 00` |
| `0x14` | `BL_MEM_WRITE_CMD` | `<N+10> 14 <ci> <pi> 00 00 <N> <N bytes> <crc32>` | bytes 0..6+N | `AA` + `EE E2` / `EE 00` |
| `0x14` | (end marker) | `0A 14 FF FF 00 00 00 <crc32>` (11 B) | bytes 0-6 | `AA` + `EE E2` / `EE 00` |

### STM32 → Host

| Byte | Name | Context |
|---|---|---|
| `0xAA` | `BL_SEND_ACK` | Bare, immediately after a CRC pass |
| `0x00` | `BL_SEND_NACK` | Bare, on CRC failure |
| `0xEE` | `BL_REPLAY_START_BYTE` | Prefix of every data reply |
| `0xFB` | `WE_ARE_IN_BOOTLOADER` | Payload of the probe reply |
| `0xA0` | (`appExists`, true) | Payload of `0x12` — app valid, resetting |
| `0x00` | (`appExists`, false) | Payload of `0x12` — no valid app |
| `0xE1` | `SUCCESSFUL_ERASE` | Payload of `0x13` |
| `0x00` | `UNSUCCESSFUL_ERASE` | Payload of `0x13` |
| `0xE2` | `FLASH_PAYLOAD_WRITE_PASSED` | Payload of `0x14` |
| `0x00` | `FLASH_PAYLOAD_WRITE_FAILED` | Payload of `0x14` |

### Magic values

| Constant | Value | Meaning |
|---|---|---|
| `BOOTLOADER_APP_MAGIC` | `0xDEADBEEF` | Written to `0x2000FFF8` to request bootloader residency |
| `BOOT_FLAG_CLEAR` | `0x00000000` | Written back on every boot |
| `BOOTLOADER_FLAG_ADDR` | `0x2000FFF8` | The shared word |
| `SYSRESETREQ` write | `0x05FA0004` → `0xE000ED0C` | `SCB->AIRCR` software reset |

## Appendix B — Register Cheat Sheet

Registers the bootloader touches, with the bit positions used.

### FLASH (`0x40023C00`)

| Register | Offset | Bits used | Purpose |
|---|---|---|---|
| `ACR` | `0x00` | `[2:0]` LATENCY | Wait states — set to 2 |
| `KEYR` | `0x04` | 32-bit | Unlock: write `0x45670123` then `0xCDEF89AB` |
| `SR` | `0x0C` | `16` BSY, `[8:4]` errors | Busy + WRPERR/PGAERR/PGPERR/PGSERR/RDERR |
| `CR` | `0x10` | `0` PG, `1` SER, `[6:3]` SNB, `[9:8]` PSIZE, `15` MER, `16` STRT, `31` LOCK | Program/erase control |

### CRC (`0x40023000`)

| Register | Offset | Purpose |
|---|---|---|
| `DR` | `0x00` | Write to feed a word; read to get the CRC |
| `IDR` | `0x04` | 8-bit scratch (unused) |
| `CR` | `0x08` | Bit 0 = RESET |

### USART1 (`0x40011000`)

| Register | Offset | Bits used |
|---|---|---|
| `SR` | `0x00` | `5` RXNE, `6` TC, `7` TXE |
| `DR` | `0x04` | `[8:0]` data |
| `BRR` | `0x08` | `[15:4]` mantissa, `[3:0]` fraction — `0x2D9` for 115200 @ 84 MHz |
| `CR1` | `0x0C` | `2` RE, `3` TE, `9` PS, `10` PCE, `12` M, `13` UE |
| `CR2` | `0x10` | `[13:12]` STOP |

### SysTick (`0xE000E010`)

| Register | Offset | Bits used |
|---|---|---|
| `CSR` | `0x00` | `0` ENABLE, `1` TICKINT, `2` CLKSOURCE, `16` COUNTFLAG |
| `RVR` | `0x04` | `[23:0]` reload |
| `CVR` | `0x08` | `[23:0]` current — any write clears it and COUNTFLAG |

### SCB

| Register | Address | Use |
|---|---|---|
| `AIRCR` | `0xE000ED0C` | `0x05FA0004` → system reset |
| `VTOR` | `0xE000ED08` | **Not written by the bootloader** — the application sets it |

### GPIOA (`0x40020000`)

| Register | Offset | Pins configured |
|---|---|---|
| `MODER` | `0x00` | PA0, PA1 output; PA9, PA10 alternate |
| `OTYPER` | `0x04` | push-pull for all four |
| `OSPEEDR` | `0x08` | medium (LEDs), high (UART) |
| `PUPDR` | `0x0C` | none (LEDs, TX), pull-up (RX) |
| `BSRR` | `0x18` | atomic set/reset |
| `AFRH` | `0x24` | AF7 for PA9, PA10 |

## Appendix C — Public API Index

| Function | File | Section |
|---|---|---|
| `App_IsValid` | `src/main.c` | [§14](#14-application-vector-table-validation) |
| `CheckForAppToRun` | `src/main.c` | [§13](#13-checkforapptorun--the-boot-decision) |
| `BL_Init` | `src/app/bootloader.c` | [§16](#16-peripheral-bring-up-when-staying-resident) |
| `BL_FetchHostCommand` | `src/app/bootloader.c` | [§20](#20-the-command-dispatch-loop) |
| `OTA_Init` | `src/app/firmware_flashing.c` | [§34](#34-state-machine-reference) |
| `OTA_ReceivePacket` | `src/app/firmware_flashing.c` | [§31](#31-the-chunk-accumulator) |
| `OTA_Finish` | `src/app/firmware_flashing.c` | [§32](#32-two-ways-a-transfer-ends) |
| `OTA_GetState` | `src/app/firmware_flashing.c` | [§34](#34-state-machine-reference) |
| `AES_GCM_DecryptChunk` | `src/app/aes_gcm.c` | [§37](#37-aes-128-gcm-construction) |
| `AES_GCM_RunTest` | `src/app/aes_gcm.c` | [§42.1](#421-aes_gcm_runtest--the-round-trip-test) |
| `RCC_ConfigureClock` | `src/MCAL/interface/rcc.c` | [§6](#6-clock-tree-and-bring-up) |
| `RCC_ControlPeripheral` | `src/MCAL/interface/rcc.c` | [§45.1](#451-peripheral-id-encoding) |
| `GPIO_Init` | `src/MCAL/interface/gpio.c` | [§46](#46-gpio-driver) |
| `UART_Init` | `src/MCAL/interface/uart.c` | [§47](#47-uart-driver-mcal) |
| `HSerial_SendBuffer` | `src/HAL/interface/hserial.c` | [§48](#48-hserial-hal) |
| `HSerial_ReceiveBuffer` | `src/HAL/interface/hserial.c` | [§48](#48-hserial-hal) |
| `FLASH_Write` | `src/MCAL/interface/flash.c` | [§49.1](#491-write-sequence) |
| `FLASH_Erase` | `src/MCAL/interface/flash.c` | [§49.3](#493-erase) |
| `CRC_Accumulate` | `src/MCAL/interface/crc.c` | [§50](#50-crc-driver) |
| `SYSTICK_DelayMS` | `src/Core/systick.c` | [§51](#51-systick-driver) |
| `LED_SetState` | `src/HAL/interface/led.c` | [§52](#52-led-driver) |

## Appendix D — Glossary

| Term | Meaning |
|---|---|
| **AAD** | Additional Authenticated Data — input to GCM that is authenticated but not encrypted |
| **AIRCR** | Application Interrupt and Reset Control Register; `SYSRESETREQ` lives here |
| **BSY** | Flash status busy flag; set while a program or erase is in progress |
| **Chunk** | 2048 bytes on the wire = 2032 B ciphertext + 16 B GCM tag |
| **GCM** | Galois/Counter Mode — authenticated encryption combining CTR mode with GHASH |
| **GHASH** | The universal hash inside GCM that produces the authentication tag |
| **HSE** | High-Speed External oscillator — the 25 MHz crystal |
| **HSI** | High-Speed Internal RC oscillator — 16 MHz, the reset default |
| **MCAL** | Microcontroller Abstraction Layer — register-level drivers |
| **MSP** | Main Stack Pointer |
| **Nonce** | Number used once — the 12-byte IV for GCM |
| **Packet** | Up to 64 bytes of ciphertext inside one `0x14` command |
| **PSIZE** | Flash programming parallelism — 8/16/32/64 bits, voltage-dependent |
| **RDP** | Read-Out Protection — Flash option-byte setting that blocks debug reads |
| **SNB** | Sector Number field in `FLASH_CR` |
| **SWD** | Serial Wire Debug |
| **SYSRESETREQ** | The bit in `AIRCR` that requests a system reset |
| **Tag** | The 16-byte GCM authenticator appended to each chunk |
| **TC** | Transmission Complete — UART flag, set when the shift register empties |
| **TXE** | Transmit data register Empty — UART flag |
| **VTOR** | Vector Table Offset Register |

---

*For the application, see [`../Application/APPLICATION.md`](../Application/APPLICATION.md). For how the two cooperate, see [`../STM32_OVERVIEW.md`](../STM32_OVERVIEW.md).*
