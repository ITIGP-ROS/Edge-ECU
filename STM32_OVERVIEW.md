# STM32F401CC Node — System Overview

> **What this is** — how the two firmware images on the STM32F401CC fit together: what each owns, how control passes between them, and every contract they share.
>
> This is the map. The territory is in [`Bootloader/BOOTLOADER.md`](Bootloader/BOOTLOADER.md) and [`Application/APPLICATION.md`](Application/APPLICATION.md).

---

## Table of Contents

- [1. The Two Images](#1-the-two-images)
- [2. Memory Partitioning](#2-memory-partitioning)
- [3. The Boot Decision](#3-the-boot-decision)
- [4. The Handover Mechanism](#4-the-handover-mechanism)
- [5. UART1 — One Wire, Two Protocols](#5-uart1--one-wire-two-protocols)
- [6. The Full OTA Lifecycle](#6-the-full-ota-lifecycle)
- [7. Protocol Comparison](#7-protocol-comparison)
- [8. The Cryptography Split](#8-the-cryptography-split)
- [9. Shared Contracts](#9-shared-contracts)
- [10. Divergences Between the Two Images](#10-divergences-between-the-two-images)
- [11. Cross-Boundary Failure Modes](#11-cross-boundary-failure-modes)
- [12. Build and Provisioning Matrix](#12-build-and-provisioning-matrix)
- [13. Where to Go Next](#13-where-to-go-next)

---

## 1. The Two Images

One microcontroller, two independently linked, independently flashed firmware images.

```
   ┌────────────────────────────────────────────────────────────────────┐
   │                        STM32F401CCU6                                │
   │                  Cortex-M4F · 84 MHz · 256 KB Flash · 64 KB SRAM    │
   ├────────────────────────────────────────────────────────────────────┤
   │                                                                    │
   │   ┌──────────────────────────┐    ┌──────────────────────────────┐ │
   │   │      BOOTLOADER          │    │        APPLICATION           │ │
   │   │      sectors 0-1         │    │        sectors 2-5           │ │
   │   │      32 KB reserved      │    │        224 KB reserved       │ │
   │   │      13,288 B used (41%) │    │        193,580 B used (84%)  │ │
   │   ├──────────────────────────┤    ├──────────────────────────────┤ │
   │   │  bare metal, no RTOS     │    │  FreeRTOS, 9 tasks           │ │
   │   │  no interrupts at all    │    │  5 interrupt sources         │ │
   │   │  polled UART1            │    │  DMA TX / IRQ RX on UART1    │ │
   │   │  AES-128-GCM (mbedTLS)   │    │  TinyML via STM32Cube.AI     │ │
   │   │  HSE 25 MHz → PLL → 84   │    │  HSI 16 MHz → PLL → 84       │ │
   │   │  no watchdog             │    │  IWDG + task supervisor      │ │
   │   └──────────────────────────┘    └──────────────────────────────┘ │
   │                                                                    │
   │   Shared: 8 bytes of SRAM at 0x2000FFF8, UART1 on PA9/PA10         │
   └────────────────────────────────────────────────────────────────────┘
```

| | Bootloader | Application |
|---|---|---|
| **Purpose** | Decide, validate, receive, flash | Sense, classify, report |
| **Runs when** | Boot flag set, or no valid app | Boot flag clear and app valid |
| **Lifetime** | Seconds (an OTA) or forever (no app) | Indefinitely |
| **Source** | `Bootloader/` | `Application/` |
| **Reference** | [`BOOTLOADER.md`](Bootloader/BOOTLOADER.md) | [`APPLICATION.md`](Application/APPLICATION.md) |

Neither image knows anything about the other's internals. They communicate through exactly three things: a Flash address, a RAM word, and a two-byte UART sequence.

## 2. Memory Partitioning

### 2.1 Flash

```
0x08000000 ┌──────────────────────────────────────────────┐  ▲
           │  Sector 0 — 16 KB                            │  │  BOOTLOADER
           │    vector table, .text, .rodata              │  │  32 KB
0x08004000 ├──────────────────────────────────────────────┤  │  13,288 B used
           │  Sector 1 — 16 KB  (mostly empty)            │  ▼
0x08008000 ├══════════════════════════════════════════════┤  ▲  ◄── the boundary
           │  Sector 2 — 16 KB                            │  │
           │    word 0 = initial MSP                      │  │
           │    word 1 = Reset_Handler                    │  │  APPLICATION
0x0800C000 ├──────────────────────────────────────────────┤  │  224 KB
           │  Sector 3 — 16 KB                            │  │  193,580 B used
0x08010000 ├──────────────────────────────────────────────┤  │
           │  Sector 4 — 64 KB                            │  │
0x08020000 ├──────────────────────────────────────────────┤  │
           │  Sector 5 — 128 KB                           │  │
0x0803FFFF └──────────────────────────────────────────────┘  ▼
```

The boundary at `0x08008000` is hardcoded in **five** places and all five must agree:

| Location | Constant |
|---|---|
| `Bootloader/include/app/bootloader.h` | `APP_FLASH_BASE 0x08008000` |
| `Bootloader/src/main.c` | `APP_FLASH_BASE 0x08008000` (duplicated) |
| `Bootloader/include/app/firmware_flashing.h` | `FLASH_APP_START 0x08008000` |
| `Application/STM32F401CCFX_FLASH_Sector2.ld` | `ORIGIN(FLASH) = 0x08008000` |
| `Application/src/main.c` | `SCB->VTOR = 0x08008000` |

Nothing checks that they agree. See [§9](#9-shared-contracts).

**Asymmetric protection.** The application's linker script declares `LENGTH = 224K`, so an over-sized application fails to link. The bootloader's declares the full `256K`, so an over-sized bootloader would silently overlap the application. With 19 KB of bootloader headroom this has never mattered, but only one of the two boundaries is actually enforced.

### 2.2 SRAM

```
0x20000000 ┌──────────────────────────────────────────────┐  ▲
           │  .data / .bss / heap / stack                  │  │  RAM
           │  Whichever image is running owns all of this. │  │  64 K − 8
0x2000FFF7 ├──────────────────────────────────────────────┤  ▼
0x2000FFF8 │  RAM_BOOT — 8 bytes — the boot flag          │  ◄── shared, NOLOAD
0x2000FFFF └──────────────────────────────────────────────┘
```

Both linker scripts carve out the same 8 bytes:

```ld
RAM       (xrw) : ORIGIN = 0x20000000, LENGTH = 64K - 8
RAM_BOOT  (xrw) : ORIGIN = 0x2000FFF8, LENGTH = 8
```

```ld
  .boot_flag (NOLOAD) :
  {
    KEEP(*(.boot_flag))
  } >RAM_BOOT
```

Because `RAM_BOOT` is a separate `MEMORY` region and `.bss` is placed `>RAM`, neither image's startup zeroing loop can reach `0x2000FFF8`. That is what makes the word survive a reset.

`_estack = ORIGIN(RAM) + LENGTH(RAM)` = `0x2000FFF8` in both scripts, so the stack grows *downward* away from the flag.

## 3. The Boot Decision

Every reset lands in the bootloader. It makes one decision before doing anything else.

```
                      ═══ RESET ═══
                    (any cause)
                           │
                           ▼
              Bootloader Reset_Handler
                copy .data, zero .bss
                (neither touches 0x2000FFF8)
                           │
                           ▼
              main() → CheckForAppToRun()          ◄── FIRST statement
                           │
                  ┌────────┴────────┐
                  │ flag = *0x2000FFF8
                  │ *0x2000FFF8 = 0      ← cleared unconditionally, always
                  └────────┬────────┘
                           ▼
                 flag == 0xDEADBEEF ?
                    │              │
                 yes│              │no
                    ▼              ▼
          ┌──────────────┐   ┌───────────────────────┐
          │ stay resident│   │  App_IsValid() ?      │
          └──────────────┘   │   SP in SRAM?         │
                             │   PC in app Flash?    │
                             │   PC bit 0 set?       │
                             └───┬───────────────┬───┘
                               no│               │yes
                                 ▼               ▼
                       ┌──────────────┐   ┌──────────────────┐
                       │ stay resident│   │ MSR msp, app_SP  │
                       └──────────────┘   │ call Reset_H     │
                                          └──────────────────┘
                                                   │
                                                   ▼
                                          APPLICATION RUNNING
```

| Boot flag | `App_IsValid()` | Result |
|---|---|---|
| `0xDEADBEEF` | not evaluated | Bootloader stays resident |
| anything else | fails | Bootloader stays resident |
| anything else | passes | Application launches |

**Two properties make this work.**

**The flag is cleared on read, not on use.** Whatever the decision, the flag is zero afterwards. So a request to enter the bootloader is one-shot: the *next* reset, whatever causes it, goes back to the application. Without this, one enter-bootloader command would trap the node in the bootloader permanently.

**`CheckForAppToRun()` runs before any peripheral is touched.** No clock reconfiguration, no GPIO, no UART, no interrupt enabled. That is what lets the jump omit every teardown step a jump-to-application normally needs — there is nothing to tear down. If anyone ever moves an initialisation call above it, the jump becomes unsafe.

Details: [`BOOTLOADER.md` §13–15](Bootloader/BOOTLOADER.md#13-checkforapptorun--the-boot-decision).

## 4. The Handover Mechanism

Two transitions, one in each direction.

### 4.1 Bootloader → Application

```c
/* Bootloader/src/main.c */
uint32_t appSP       = *((volatile uint32_t *)(APP_FLASH_BASE));
uint32_t MainAppAddr = *((volatile uint32_t *)(APP_FLASH_BASE + 4U));
mainAppPtr ResetHandler_Address = (mainAppPtr)MainAppAddr;

__asm volatile ("MSR msp, %0" : : "r" (appSP) : );
ResetHandler_Address();
```

The bootloader reads the application's own vector table for both the stack pointer and the entry point, so the application controls both.

**VTOR is not changed by the bootloader.** The application does it itself, as the first executable statement of its `main()`:

```c
/* Application/src/main.c */
*((volatile uint32_t *)0xE000ED08U) = 0x08008000U;
```

Between the jump and that write, exceptions would vector through the bootloader's table. The window is a handful of instructions and no interrupt is enabled on either side, so it is not exploitable — but it is a real coupling. Remove the VTOR line while "tidying" the application and the node dies on the first TIM2 tick, with a debugger showing the PC inside bootloader Flash.

### 4.2 Application → Bootloader

```c
/* Application/src/main.c, Thread5_BootloaderRx */
static uint8_t ack_packet[2] = {0xEEU, 0xAAU};
(void)UART_Transmit_Polling(UART1_ID, &tx_buf, 10000UL);    /* waits for TC */

*((volatile uint32_t *)0x2000FFF8UL) = 0xDEADBEEF;
*((volatile uint32_t *)0xE000ED0CU) = (0x05FAUL << 16U) | (1UL << 2U);
for (;;) {}
```

```
   Application running
        │
        │  gateway sends 0xAA 0xEB
        ▼
   USART1 RXNE ISR ×2 → ring buffer → notify Thread 5 (priority 4)
        │
        ▼
   Thread 5 matches the sequence
        │
        ├─ 1. TX 0xEE 0xAA via POLLING, waiting for TC
        │       (polling, not DMA: Thread 3 may hold the DMA, and the
        │        TC wait guarantees both bytes are on the wire before reset)
        │
        ├─ 2. *(0x2000FFF8) = 0xDEADBEEF
        │
        └─ 3. SCB->AIRCR = 0x05FA0004      ← SYSRESETREQ
                 │
                 ▼
            ═══ reset ═══
                 │
                 ▼
        Bootloader sees the flag → stays resident
```

**No Flash is erased.** An earlier design had Thread 5 erase sector 1 to signal the bootloader; that was replaced by the RAM flag (commit `fd7adfa Bootloader & Application switching via RAM`). The `Application/README.md` still documents the erase behaviour and is wrong — erasing sector 1 would destroy half the bootloader.

Details: [`APPLICATION.md` §28](Application/APPLICATION.md#28-thread-5--bootloader-receive).

## 5. UART1 — One Wire, Two Protocols

```
   ESP32 gateway UART2  ◄──────────────► STM32 UART1 (PA9 TX / PA10 RX)
                        115200 8N1, no flow control
```

Whichever image is running owns the line completely. There is no multiplexing and no way to address one image while the other is active.

```
   ┌──────────────────────────────────────────────────────────────────┐
   │  APPLICATION MODE                                                 │
   ├──────────────────────────────────────────────────────────────────┤
   │  STM32 → ESP32   FRAME telemetry, ~9 frames/s, 1.1 % utilisation │
   │                  [LEN][TYPE][ECU][payload][CRC32 BE]             │
   │                  types 0x01 classification, 0x02 temperature,     │
   │                        0x03 heartbeat, 0x04 log, 0x05 ultrasonic  │
   │                  plus a PA8 GPIO strobe around each transmission  │
   │                                                                   │
   │  ESP32 → STM32   0xAA 0xEB only                                   │
   └──────────────────────────────────────────────────────────────────┘
                                  │
                       0xAA 0xEB  │  → reply 0xEE 0xAA → reset
                                  ▼
   ┌──────────────────────────────────────────────────────────────────┐
   │  BOOTLOADER MODE                                                  │
   ├──────────────────────────────────────────────────────────────────┤
   │  ESP32 → STM32   [LEN][CMD][params][CRC32 LE]                    │
   │                  0x10 version · 0x12 jump · 0x13 erase            │
   │                  0x14 write   · 0xAA probe                        │
   │                                                                   │
   │  STM32 → ESP32   0xAA ACK / 0x00 NACK, then 0xEE + payload        │
   │                  No CRC on replies.                               │
   └──────────────────────────────────────────────────────────────────┘
```

**The probe is answered by both images**, with different bytes so the gateway can tell them apart:

| Who answers `0xAA 0xEB` | Reply | Gateway reads it as |
|---|---|---|
| Application (Thread 5) | `0xEE 0xAA` | "was the app, now resetting into the bootloader" |
| Bootloader | `0xEE 0xFB` | "already the bootloader, no reset happened" |

That distinction matters because the first case implies a reset delay before the node is ready for the next command.

The gateway serialises the whole exchange behind a mutex (`uart2_mutex` in its `bootloader.c`) so its own log-receiver thread cannot interleave bytes between a command and its reply.

## 6. The Full OTA Lifecycle

```
  ┌───────────────────────────────────────────────────────────────────────┐
  │  HOST                                                                 │
  │    build firmware.bin (193,580 B)                                     │
  │    chunk into 2,032-byte plaintext blocks                             │
  │    AES-128-GCM encrypt each: nonce = 00×8 || chunk_index BE(4)        │
  │    append the 16-byte tag → 2,048-byte chunks                         │
  │    publish over MQTT                                                  │
  └────────────────────────────────┬──────────────────────────────────────┘
                                   ▼
  ┌───────────────────────────────────────────────────────────────────────┐
  │  ESP32 GATEWAY                                                        │
  │    receive over MQTT → store in LittleFS at /update/firmware.bin      │
  │    store the version at /update/version.bin                           │
  │    request cluster approval over CAN (SecOC DID 4 → DID 6)            │
  └────────────────────────────────┬──────────────────────────────────────┘
                                   ▼
   ESP32                                                     STM32
     │                                                          │
     │  B1  ── 0xAA 0xEB ──────────────────────────────────────►│ app: Thread 5
     │      ◄── 0xEE 0xAA ──────────────────────────────────────│ ACK (polling)
     │                                                          │ flag = 0xDEADBEEF
     │                                                          │ ═══ reset ═══
     │                                                          │ bootloader resident
     │                                                          │ PA0 pulses 1 s
     │
     │  B3  ── 05 13 <crc32> ──────────────────────────────────►│ CRC ok
     │      ◄── 0xAA ───────────────────────────────────────────│ ACK
     │                                             (2.0-3.1 s)  │ erase sectors 2-5
     │      ◄── 0xEE 0xE1 ──────────────────────────────────────│ success
     │
     │  B5  ── <LEN> 14 <ci> <pi> 00 00 <n> <n bytes> <crc32> ─►│ CRC ok
     │      ◄── 0xAA ───────────────────────────────────────────│ ACK
     │                                                          │ accumulate 64 B
     │                                                          │ at 2048 B:
     │                                                          │   GCM decrypt
     │                                                          │   verify tag
     │                                                          │   FLASH_Write
     │      ◄── 0xEE 0xE2 ──────────────────────────────────────│ written
     │      ... × ~3,000 packets, ~5 minutes ...                │
     │                                                          │
     │  [if the last packet was exactly 64 B]                   │
     │      ── 0A 14 FF FF 00 00 00 <crc32> ───────────────────►│ OTA_Finish()
     │      ◄── 0xAA, 0xEE 0xE2 ────────────────────────────────│
     │
     │  B4  ── 07 12 <maj> <min> <crc32> ──────────────────────►│ CRC ok
     │      ◄── 0xAA ───────────────────────────────────────────│ ACK
     │                                                          │ App_IsValid() → 1
     │      ◄── 0xEE 0xA0 ──────────────────────────────────────│ "app is running"
     │                                                          │ ═══ reset ═══
     │                                                          │ flag clear → jump
     │  publish "Firmware is now Running"                       │
     │  send SecOC RUNNING(slot = 1) onto CAN                   │ app running again
     │                                                          │ ~2.3 s of vote
     │                                                          │ warm-up, then
     │                                                          │ telemetry resumes
```

### 6.1 Timing

| Phase | Duration |
|---|---|
| B1 enter + reset | ~100 ms |
| B3 erase (224 KB) | 2.0–3.1 s |
| B5 transfer (193 KB) | ~5 min, UART-bound |
| B4 jump + reset | ~100 ms |
| Vote warm-up before the first classification frame | 2.25 s |

The transfer dominates and is entirely UART-bound: 193,580 bytes of plaintext become ~195,100 bytes of ciphertext plus tags, framed into ~3,050 packets averaging 74 bytes, plus a 1-byte ACK and a 2-byte reply each. Roughly 228 KB on the wire at 11.5 KB/s.

The AES-GCM decryption is not on the critical path — a 2,032-byte chunk decrypts in well under a millisecond against 178 ms of wire time for the same chunk.

### 6.2 The `0xA0` reply is optimistic

`0xEE 0xA0` asserts only *"the vector table at 0x08008000 passes three structural checks and I am about to reset"*. It does not assert that the application booted. The gateway nonetheless publishes "Firmware is now Running" and sends the SecOC RUNNING notification immediately.

The observable ground truth — telemetry resuming — arrives 2.5 seconds later and is not waited for.

### 6.3 Recovery from a failed transfer

If any packet returns `0xEE 0x00`, the gateway aborts. Because the erase already happened, the node is left with a partial image.

Retrying from B3 in the same bootloader session **fails**: the OTA engine's state is `COMPLETE` or `ERROR` and never returns to `IDLE` without a reset ([`BOOTLOADER.md` §35.1](Bootloader/BOOTLOADER.md#351-a-second-ota-in-the-same-session-always-fails)).

The working recovery is to send **B1 first** — which resets the MCU and re-initialises the engine — then B3, B5, B4.

## 7. Protocol Comparison

The two protocols on the same wire differ in ways that matter to anyone writing a parser.

| Aspect | Application FRAME | Bootloader command |
|---|---|---|
| Direction | STM32 → ESP32 | ESP32 → STM32 |
| Framing | Leading LEN byte | Leading LEN byte |
| `LEN` means | payload + 6 | bytes following this one |
| Total size | `LEN + 1` | `LEN + 1` |
| CRC coverage | TYPE + ECU + payload — **excludes LEN** | **includes LEN**, excludes the CRC |
| CRC byte order | **big-endian** | **little-endian** |
| CRC algorithm | identical: 32 iterations/byte, poly `0x04C11DB7`, init `0xFFFFFFFF`, no final XOR | identical |
| CRC computed by | software (`Frame_CRC32`) | STM32 hardware CRC unit |
| Reply | none — fire and forget | `0xAA`/`0x00`, then `0xEE` + payload |
| Reply integrity | n/a | **none** |
| Max payload | 248 B (32 B in practice) | 64 B |
| Out-of-band marker | PA8 GPIO strobe | none |

**Two traps for a unified parser:**

1. **CRC coverage differs.** The application excludes the length byte; the bootloader includes it.
2. **CRC byte order differs.** Big-endian outbound, little-endian inbound.

Both ends implement both correctly, but the asymmetry is undocumented anywhere except in the two reference documents.

### 7.1 The shared CRC algorithm

Both use a non-standard CRC-32 that processes **32 iterations per byte**, XORing each byte into the *low* byte of the accumulator:

```c
crc ^= (uint32_t)byte_in;
for (bit = 0U; bit < 32U; bit++) {
    crc = (crc & 0x80000000U) ? ((crc << 1U) ^ 0x04C11DB7U) : (crc << 1U);
}
```

This is exactly what the **STM32 hardware CRC peripheral** computes when fed a byte zero-extended into its 32-bit data register. The lineage:

```
   STM32 hardware CRC unit
        │  (used directly by the bootloader)
        ▼
   ESP32 calculateCRC32()  — software imitation, 32 iterations
        │
        ▼
   STM32 application Frame_CRC32()  — matches the ESP32
```

`FRAME.c` carries the warning:

> *"DO NOT CHANGE TO 8 ITERATIONS — both ends of the link must stay in sync."*

Reference implementation for any third endpoint:

```python
def crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(32):
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if crc & 0x80000000 else (crc << 1) & 0xFFFFFFFF
    return crc
```

## 8. The Cryptography Split

The two images make opposite choices, and both are right for their job.

| | Bootloader | Application |
|---|---|---|
| Primitive | AES-128-GCM (mbedTLS) | CRC-32 (software) |
| Protects | Firmware image confidentiality + authenticity | Telemetry frame integrity |
| Against | A wire attacker forging firmware | Bit errors on the wire |
| Flash cost | ~9,000 B (69 % of the image) | ~200 B |
| RAM cost | 8,192 B of AES tables + 4,080 B of buffers | 0 |

The bootloader spends two thirds of its Flash on crypto because the threat is real: anyone who can inject bytes on UART1 while the bootloader is resident could otherwise write arbitrary code into sectors 2–5. The GCM tag makes that infeasible.

The application spends almost nothing because the threat is different: a corrupted telemetry frame is a nuisance, not a compromise, and the gateway simply discards it.

### 8.1 The GCM construction

```
   Key   AES-128, compile-time constant in Bootloader/src/app/aes_gcm.c
   Nonce 12 B = 00 00 00 00 00 00 00 00 || chunk_index (4 B, big-endian)
   AAD    4 B = the chunk index again (adds nothing)
   Tag   16 B, full length, appended to each 2,048-byte chunk
```

The tag is verified **before** the Flash write, so a bad chunk never reaches Flash. That ordering is the single most important correctness property in the update path.

### 8.2 The one serious weakness

**Every image starts at chunk 0**, so chunk *N* of any two images shares a nonce under the same fixed key. GCM nonce reuse leaks the keystream (`C₁ ⊕ C₂ = M₁ ⊕ M₂`) and, via the forbidden attack, permits recovery of the GHASH subkey and forgery of arbitrary tags.

A passive listener capturing two OTA sessions has everything needed.

The fix is an 8-byte per-image nonce prefix, transmitted in the clear — it is not secret, only unique. Full analysis: [`BOOTLOADER.md` §38.1](Bootloader/BOOTLOADER.md#381-nonce-reuse-analysis) and [§43](Bootloader/BOOTLOADER.md#43-security-assessment).

Also open: Flash read-out protection is at level 0, so the key can be extracted with `st-flash read`.

## 9. Shared Contracts

Everything the two images must agree on. **None of these is checked at build time.**

| # | Contract | Bootloader side | Application side | Breaks if violated |
|---|---|---|---|---|
| C1 | Application base address | `APP_FLASH_BASE 0x08008000` (twice) | `ORIGIN(FLASH) = 0x08008000` | Bootloader validates and jumps to the wrong place |
| C2 | VTOR relocation | does **not** set VTOR | sets `SCB->VTOR = 0x08008000` first thing | Every interrupt vectors into the bootloader; node dies on the first tick |
| C3 | Boot flag address | `0x2000FFF8` | `0x2000FFF8` | Enter-bootloader silently fails |
| C4 | Boot flag magic | `0xDEADBEEF` | `0xDEADBEEF` | Same |
| C5 | RAM carve-out | `RAM = 64K - 8`, `RAM_BOOT` at the top | identical | `.bss` zeroing clobbers the flag |
| C6 | `_estack` | `ORIGIN(RAM) + LENGTH(RAM)` | identical | Stack could grow into the flag |
| C7 | Probe sequence | recognises `0xAA` then discards the next byte | recognises `0xAA` then `0xEB` | Gateway cannot switch modes |
| C8 | Probe reply | `0xEE 0xFB` | `0xEE 0xAA` | Gateway cannot tell which image answered |
| C9 | UART parameters | 115200 8N1 on PA9/PA10 | identical | No communication in one mode |
| C10 | CRC algorithm | hardware CRC unit | software, 32 iterations/byte | Every command NACKs |
| C11 | Vector-table shape | reads word 0 as SP, word 1 as PC | standard ARM layout | Validation rejects a good image |
| C12 | Chunk geometry | 2,032 B plaintext + 16 B tag | (host-side) | Tag verification fails |
| C13 | GCM key and nonce derivation | `AES_GCM_KEY`, `build_nonce` | (host-side) | Decryption fails |

C1 alone is duplicated across five files. A single shared header — or a linker-script `ASSERT` — would turn a silent brick into a build error.

### 9.1 The most fragile contract

**C2.** It is invisible: nothing in the bootloader mentions VTOR, and the application's line looks like boilerplate. Removing it produces a board that boots and immediately hangs, with a debugger showing the PC inside *bootloader* Flash — which reads as a wild jump, not as a vector-table problem.

The comment in `Application/src/main.c` is what documents it:

```c
/* --- Relocate vector table to Sector 2 (required when app runs from 0x08008000) --- */
/* SCB->VTOR is at 0xE000ED08. Write the app base address so all interrupts   */
/* are dispatched through our vector table, not the bootloader's at 0x08000000 */
*((volatile uint32_t *)0xE000ED08U) = 0x08008000U;
```

Do not delete it.

## 10. Divergences Between the Two Images

Places where the two make different choices for the same problem. Most are harmless; a few are worth knowing.

| Aspect | Bootloader | Application | Consequence |
|---|---|---|---|
| **PLL source** | HSE 25 MHz crystal | HSI 16 MHz internal RC | Both reach 84 MHz. HSI is ±1 % at 25 °C and up to ±4 % over temperature, which the UART baud rate inherits. |
| **`STD_ReturnType`** | `STD_ERROR = 0`, `STD_SUCCESS = 1` | every driver uses `*_OK = 0` | Inverted success conventions between the two trees. |
| **Integer types** | hand-rolled typedefs in `lib/STD_Types.h` | defers to `<stdint.h>` under GCC | The bootloader's version can collide with `<stdint.h>`; it works by include-order luck. |
| **`Buffer_t`** | 3 fields, `uint8_t` lengths, no capacity | 4 fields, `uint16_t`, with `size` | Same name, different layout. They never link together. |
| **RCC peripheral encoding** | bus in bits [31:30], full bitmask — allows OR-ing peripherals on one bus | bus in bits [6:5], bit position in [4:0] | Two implementations of the same idea. |
| **CRC** | hardware peripheral | software loop | Same result by construction ([§7.1](#71-the-shared-crc-algorithm)). |
| **Timeouts** | bare loop counters named `timeoutMS` | FreeRTOS ticks, real milliseconds | The bootloader's `BL_UART_DELAY = 1000` is ~80 µs, not 1 s. |
| **Watchdog** | none | IWDG + 4-flag task supervisor | The bootloader can hang unrecoverably (e.g. an unbounded Flash BSY wait). |
| **Interrupts** | none enabled, ever | 5 sources at priorities 5–7 | The bootloader has no reentrancy concerns anywhere. |
| **`build_type`** | `debug` (`-Og`) | `debug` + explicit `-O2` | The bootloader ships unoptimised; it has the headroom. |

## 11. Cross-Boundary Failure Modes

Failures that involve both images, or the handover between them.

```
   Node is silent — no telemetry
        │
        ├─ Is PA0 pulsing once for 1 s after reset?
        │    yes ──► the BOOTLOADER is resident.
        │             → the app is missing or invalid; push one
        │             → or the boot flag was set and never cleared
        │                (impossible by construction — suspect RAM)
        │    no
        ▼
   ├─ Does PC13 light up when you shake the board (rough)?
   │    yes ──► the APPLICATION (and ML) is running. The problem is downstream:
   │             baud rate, wiring, or the gateway.
   │    no
   ▼
   ├─ Does 0xAA 0xEB get a reply?
   │    0xEE 0xFB ──► bootloader alive, LED path broken
   │    0xEE 0xAA ──► application alive, LED path broken
   │    nothing   ──► neither is running
   ▼
   └─ Attach ST-Link. Read the PC.
        → inside 0x08000000-0x08008000: the bootloader is stuck
            (most likely an unbounded FLASH BSY wait)
        → inside 0x08008000+: the app faulted
            (HardFault_Handler, or configASSERT with IRQs disabled)
        → cannot connect: power, NRST, or BOOT0 not low
```

| Failure | Cause | Recovery |
|---|---|---|
| App boots and immediately hard-faults | Truncated image passed `App_IsValid()` — nothing verifies total length | Power cycle and send `0xAA 0xEB` early; Thread 5 is priority 4 and may catch it. Otherwise ST-Link. |
| Node dead after a failed OTA | Erase succeeded, transfer aborted | B1 (resets and re-inits the OTA engine), then B3, B5, B4 |
| Second OTA in one session always fails | The OTA engine never returns to `IDLE` | Always send B1 before retrying |
| App runs but no interrupts fire | VTOR line removed or the image flashed to `0x08000000` | Reflash to `0x08008000` |
| "Firmware is now Running" but no telemetry | `0xA0` is sent before the reset; it asserts nothing about the app booting | Wait 2.5 s for the vote warm-up; if still silent, the app faulted |
| Enter-bootloader ignored on the first try after a reset | Thread 5 registers its RX notification at task entry; bytes arriving earlier buffer without notifying | Send it twice; the second attempt always works |
| Baud errors at temperature extremes | The app derives baud from HSI (±4 %); the bootloader uses HSE (±20 ppm) | Consider moving the app to HSE |
| Bootloader hangs during erase | `FLASH_Erase` spins on BSY with no timeout, and there is no watchdog | Power cycle |

### 11.1 The truncation gap

The most consequential cross-boundary weakness. `App_IsValid()` checks three things: the stack pointer is in SRAM, the reset handler is in application Flash, and its low bit is set. It does **not** check total length or whole-image integrity.

```
   Host sends 40 KB of a 193 KB image, then dies
        │
        ▼
   Chunk 0 wrote a valid vector table at 0x08008000
        │
        ▼
   App_IsValid() passes
        │
        ▼
   The bootloader jumps into a 20 %-flashed application
```

The GCM tag catches corruption of chunks that *arrived*; it cannot detect chunks that never did.

The cheapest mitigation is to bind the total image length into the GCM AAD (which currently carries only a duplicate of the nonce, adding nothing) and refuse `OTA_Finish()` until `flash_addr - FLASH_APP_START` matches. That is a coordinated change across the host chunker and `firmware_flashing.c`, and it kills two findings at once.

## 12. Build and Provisioning Matrix

| Task | Command |
|---|---|
| Build the bootloader | `pio run -d Bootloader` |
| Flash the bootloader | `pio run -d Bootloader -t upload` |
| Build the application | `pio run -d Application` |
| Flash the application over SWD | `pio run -d Application -t upload` |
| Read Flash for inspection | `st-flash read dump.bin 0x08008000 10000` |
| Full erase (recovery from a brick) | `st-flash erase` |

Raw `st-flash` equivalents:

```bash
st-flash write Bootloader/.pio/build/genericSTM32F401CC/firmware.bin 0x08000000
```

```bash
st-flash write Application/.pio/build/genericSTM32F401CC/firmware.bin 0x08008000
```

**The addresses are not interchangeable.** Flashing the application to `0x08000000` overwrites the bootloader and leaves an image whose vector table is in the wrong place — a board that does not boot and cannot be entered over UART.

### 12.1 Provisioning a blank part

```
   1. Flash the bootloader to 0x08000000
        → sectors 2-5 read 0xFF, App_IsValid() fails
        → the node boots, stays resident, PA0 pulses for 1 s
   2. Either:
        a) flash the application over SWD to 0x08008000, or
        b) let the gateway push it: B1 → B3 → B5 → B4
```

Order does not matter — flashing the application first also works, because the bootloader upload only programs sectors 0–1.

### 12.2 Footprint at a glance

| | Flash used | Flash budget | RAM used | RAM budget |
|---|---|---|---|---|
| Bootloader | 13,288 B | 32,768 B (41 %) | 15,104 B | 65,528 B (23 %) |
| Application | 193,580 B | 229,376 B (84 %) | 31,084 B | 65,528 B (47 %) |

The application's 84 % is misleading: roughly **110 KB of it is CMSIS-DSP FFT twiddle tables for transform lengths the code never uses**. `arm_rfft_fast_init_f32()` keeps every length's tables alive; switching to `arm_rfft_fast_init_64_f32()` would take utilisation to about 36 %. See [`APPLICATION.md` §14.1](Application/APPLICATION.md#141-half-the-flash-is-fft-tables-that-are-never-used).

## 13. Where to Go Next

### By question

| Question | Where |
|---|---|
| How does the boot decision work? | [`BOOTLOADER.md` §13](Bootloader/BOOTLOADER.md#13-checkforapptorun--the-boot-decision) |
| What exactly does `App_IsValid()` check? | [`BOOTLOADER.md` §14](Bootloader/BOOTLOADER.md#14-application-vector-table-validation) |
| What are the bootloader command bytes? | [`BOOTLOADER.md` Appendix A](Bootloader/BOOTLOADER.md#appendix-a--command-byte-reference) |
| How does the firmware update engine work? | [`BOOTLOADER.md` Part V](Bootloader/BOOTLOADER.md#part-v--firmware-update-engine) |
| How secure is the OTA path? | [`BOOTLOADER.md` §43](Bootloader/BOOTLOADER.md#43-security-assessment) |
| What tasks run in the application? | [`APPLICATION.md` §17](Application/APPLICATION.md#17-task-inventory) |
| How does the ML pipeline work? | [`APPLICATION.md` Part V](Application/APPLICATION.md#part-v--the-tinyml-pipeline) |
| What is the model architecture? | [`APPLICATION.md` §44](Application/APPLICATION.md#44-model-architecture) |
| What does each telemetry frame contain? | [`APPLICATION.md` Part VI](Application/APPLICATION.md#part-vi--wire-protocol) |
| How does the watchdog supervisor work? | [`APPLICATION.md` §32](Application/APPLICATION.md#32-idle-hook-and-the-watchdog-supervisor) |
| Why is the node reporting constant classifications? | [`APPLICATION.md` §43](Application/APPLICATION.md#43-replay-mode) |
| What is known to be broken? | [`BOOTLOADER.md` §57](Bootloader/BOOTLOADER.md#57-known-gaps) · [`APPLICATION.md` §82](Application/APPLICATION.md#82-known-gaps) |

### The five things most worth acting on

| # | Item | Where |
|---|---|---|
| 1 | **`REPLAY_MODE = 1`** — the committed application replays recorded data instead of reading the IMU, and nothing in the telemetry says so | [`APPLICATION.md` §43](Application/APPLICATION.md#43-replay-mode) |
| 2 | **~110 KB of dead FFT tables** — one-line fix takes Flash from 84 % to ~36 % | [`APPLICATION.md` §14.1](Application/APPLICATION.md#141-half-the-flash-is-fft-tables-that-are-never-used) |
| 3 | **A second OTA in one bootloader session always fails**, after the erase has already destroyed the old image | [`BOOTLOADER.md` §35.1](Bootloader/BOOTLOADER.md#351-a-second-ota-in-the-same-session-always-fails) |
| 4 | **GCM nonce reuse across images** — a passive wire listener can escalate to forging firmware | [`BOOTLOADER.md` §38.1](Bootloader/BOOTLOADER.md#381-nonce-reuse-analysis) |
| 5 | **A truncated image boots** — nothing verifies total length or whole-image integrity | [§11.1](#111-the-truncation-gap) |

### Related documents

| Document | Covers |
|---|---|
| [`Bootloader/BOOTLOADER.md`](Bootloader/BOOTLOADER.md) | The bootloader in full — 57 sections + 4 appendices |
| [`Application/APPLICATION.md`](Application/APPLICATION.md) | The application in full — 82 sections + 5 appendices |
| `../ESP32/ESP32_GATEWAY.md` | The gateway: Wi-Fi, MQTT, CAN, SecOC, its own OTA |
| `Application/README.md` | An earlier application reference — **partly stale**; see [`APPLICATION.md` §82](Application/APPLICATION.md#82-known-gaps) items A8–A10 |
| `Bootloader/README.md` | `st-flash` command cheat sheet |

---

*Generated from source at commit `30d6ff0`.*
