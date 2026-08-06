# STM32F401CC Application — Technical Reference

> **Component** — Road-surface classification node: FreeRTOS + on-device TinyML + multi-sensor telemetry.
> **Location** — `GP/ECU/STM32/Application/`
> **Target** — STM32F401CCU6, Cortex-M4F @ 84 MHz, 256 KB Flash, 64 KB SRAM
> **RTOS** — FreeRTOS, static allocation only (`configSUPPORT_DYNAMIC_ALLOCATION = 0`)
> **ML runtime** — STM32Cube.AI (X-CUBE-AI) 2.2.0, int8 quantised
> **Toolchain** — PlatformIO, `framework = cmsis`, custom register-level HAL — no ST HAL
> **Footprint** — 193,580 B Flash (84.4 % of 224 KB), 31,084 B RAM (47.4 % of 64 KB)
> **Link** — UART1 @ 115200 to the ESP32 gateway

---

**Scope of this document.** This covers the application image only — the firmware that lives in Flash sectors 2–5 and runs after the bootloader hands over. The bootloader is documented in [`../Bootloader/BOOTLOADER.md`](../Bootloader/BOOTLOADER.md); the contract between the two is in [`../STM32_OVERVIEW.md`](../STM32_OVERVIEW.md).

Everything here was derived by reading the source at commit `30d6ff0`. Where a comment in the code contradicts the code, the code wins and the discrepancy is called out — there are several, and they are the kind that mislead a reader badly.

> **Build mode.** `main.c` line 21 reads `#define REPLAY_MODE 0` — the build reads the live MPU6050 over I2C1 (PB6/PB7). Setting it to `1` switches Thread 1 to injecting 200 pre-recorded samples from `replay_data.h` in a loop, for pipeline validation without hardware. See [§43](#43-replay-mode).

---

## Table of Contents

**Part I — Orientation**

- [1. What This Node Does](#1-what-this-node-does)
- [2. System Context](#2-system-context)
- [3. Design Principles](#3-design-principles)
- [4. Capability Matrix](#4-capability-matrix)
- [5. Source Tree Map](#5-source-tree-map)

**Part II — Platform**

- [6. Target Microcontroller](#6-target-microcontroller)
- [7. Clock Tree](#7-clock-tree)
- [8. Pin Assignment](#8-pin-assignment)
- [9. Peripheral Allocation](#9-peripheral-allocation)
- [10. Flash Layout and VTOR Relocation](#10-flash-layout-and-vtor-relocation)
- [11. SRAM Budget](#11-sram-budget)
- [12. Linker Script](#12-linker-script)
- [13. Build System](#13-build-system)
- [14. Footprint Analysis](#14-footprint-analysis)

**Part III — RTOS Architecture**

- [15. FreeRTOS Configuration](#15-freertos-configuration)
- [16. Static Allocation Policy](#16-static-allocation-policy)
- [17. Task Inventory](#17-task-inventory)
- [18. Priority Design](#18-priority-design)
- [19. Synchronisation Map](#19-synchronisation-map)
- [20. Runtime Statistics via DWT](#20-runtime-statistics-via-dwt)
- [21. Stack Sizing](#21-stack-sizing)
- [22. Interrupt Priority Map](#22-interrupt-priority-map)

**Part IV — Tasks**

- [23. Boot Sequence](#23-boot-sequence)
- [24. Thread 1 — Sensor Producer](#24-thread-1--sensor-producer)
- [25. Thread 2 — TinyML Inference](#25-thread-2--tinyml-inference)
- [26. Thread 3 — UART Transmit](#26-thread-3--uart-transmit)
- [27. Thread 4 — Heartbeat](#27-thread-4--heartbeat)
- [28. Thread 5 — Bootloader Receive](#28-thread-5--bootloader-receive)
- [29. Thread 6 — Temperature](#29-thread-6--temperature)
- [30. Thread 7 — Ultrasonic](#30-thread-7--ultrasonic)
- [31. Logger Task](#31-logger-task)
- [32. Idle Hook and the Watchdog Supervisor](#32-idle-hook-and-the-watchdog-supervisor)

**Part V — The TinyML Pipeline**

- [33. Pipeline Overview](#33-pipeline-overview)
- [34. Stage 1 — Acquisition](#34-stage-1--acquisition)
- [35. Stage 2 — Ring Buffer](#35-stage-2--ring-buffer)
- [36. Stage 3 — Scaling](#36-stage-3--scaling)
- [37. Stage 4 — Feature Extraction](#37-stage-4--feature-extraction)
- [38. Stage 5 — Z-Score Normalisation](#38-stage-5--z-score-normalisation)
- [39. Stage 6 — Quantisation](#39-stage-6--quantisation)
- [40. Stage 7 — Inference](#40-stage-7--inference)
- [41. Stage 8 — Temporal Voting](#41-stage-8--temporal-voting)
- [42. Windowing and Stride](#42-windowing-and-stride)
- [43. Replay Mode](#43-replay-mode)
- [44. Model Architecture](#44-model-architecture)
- [45. Numerical Parity with the Training Pipeline](#45-numerical-parity-with-the-training-pipeline)
- [46. WCET Measurement](#46-wcet-measurement)

**Part VI — Wire Protocol**

- [47. FRAME Wire Format](#47-frame-wire-format)
- [48. Frame Type Catalogue](#48-frame-type-catalogue)
- [49. Classification Payload](#49-classification-payload)
- [50. Temperature Payload](#50-temperature-payload)
- [51. Heartbeat Payload](#51-heartbeat-payload)
- [52. Log Payload](#52-log-payload)
- [53. Ultrasonic Payload](#53-ultrasonic-payload)
- [54. The CRC32 Quirk](#54-the-crc32-quirk)
- [55. The GPIO Sync Line](#55-the-gpio-sync-line)
- [56. Inbound Bootloader Command](#56-inbound-bootloader-command)

**Part VII — Driver Layer**

- [57. Driver Layering](#57-driver-layering)
- [58. RCC Driver](#58-rcc-driver)
- [59. GPIO Driver](#59-gpio-driver)
- [60. NVIC Driver](#60-nvic-driver)
- [61. DWT Driver](#61-dwt-driver)
- [62. TIM Driver](#62-tim-driver)
- [63. ADC Driver](#63-adc-driver)
- [64. I2C Driver and Service](#64-i2c-driver-and-service)
- [65. DMA Driver](#65-dma-driver)
- [66. UART Driver and Service](#66-uart-driver-and-service)
- [67. FLASH Driver](#67-flash-driver)
- [68. IWDG Driver](#68-iwdg-driver)
- [69. MPU6050 Driver](#69-mpu6050-driver)
- [70. HC-SR04 Driver](#70-hc-sr04-driver)
- [71. Ring Buffer Module](#71-ring-buffer-module)
- [72. Buffer_t Descriptor](#72-buffer_t-descriptor)

**Part VIII — Diagnostics**

- [73. Logging Architecture](#73-logging-architecture)
- [74. Log Code Taxonomy](#74-log-code-taxonomy)
- [75. Debounce and Deduplication](#75-debounce-and-deduplication)
- [76. Debug Breadcrumbs](#76-debug-breadcrumbs)
- [77. Heartbeat as Telemetry](#77-heartbeat-as-telemetry)

**Part IX — Operations**

- [78. Build and Flash](#78-build-and-flash)
- [79. Bench Procedures](#79-bench-procedures)
- [80. Failure Modes](#80-failure-modes)
- [81. Troubleshooting Decision Trees](#81-troubleshooting-decision-trees)
- [82. Known Gaps](#82-known-gaps)

**Appendices**

- [Appendix A — Pin and Peripheral Table](#appendix-a--pin-and-peripheral-table)
- [Appendix B — Interrupt Vector Reference](#appendix-b--interrupt-vector-reference)
- [Appendix C — API Index](#appendix-c--api-index)
- [Appendix D — File Map](#appendix-d--file-map)
- [Appendix E — Glossary](#appendix-e--glossary)

---

# Part I — Orientation

## 1. What This Node Does

The node samples a 6-axis IMU at 100 Hz, runs a quantised convolutional neural network over a sliding 0.5-second window, and reports whether the road surface under the vehicle is **smooth** or **rough** at roughly 4 Hz. Alongside that it reads an ambient temperature sensor, measures two ultrasonic distances, and publishes a runtime health heartbeat — all over a single 115200-baud UART to the ESP32 gateway.

Everything runs concurrently under FreeRTOS with eight tasks, zero dynamic memory allocation, and a thread-aware watchdog that will reset the MCU if any of the four monitored tasks stops making progress.

```
   ┌──────────────────────────────────────────────────────────────────┐
   │                         What runs here                           │
   ├──────────────────────────────────────────────────────────────────┤
   │  100 Hz   MPU6050 6-axis IMU sampling (I2C + DMA)                │
   │    4 Hz   CNN inference → smooth / rough + confidence            │
   │  0.5 Hz   LM35 ambient temperature (ADC polling)                 │
   │    4 Hz   2× HC-SR04 distance (TIM3 input capture)               │
   │  0.2 Hz   Runtime heartbeat: CPU %, stack HWM, WCET, RB fill     │
   │  ≤0.25 Hz Structured event log (debounced, deduplicated)         │
   │  async    Bootloader-entry command listener                      │
   └──────────────────────────────────────────────────────────────────┘
```

The classification output is the reason the node exists; everything else is either sensor fusion input for the wider vehicle system or observability for the engineers building it.

## 2. System Context

```
                        Cloud / MQTT broker
                                 │
                                 ▼
                   ┌───────────────────────────┐
                   │      ESP32 Gateway        │
                   │      (Zephyr 4.3.99)      │
                   │  Wi-Fi · MQTT · CAN · OTA │
                   └──┬──────────────────┬─────┘
                      │                  │
        UART2 @115200 │                  │ CAN @ 500 kbit
                      │                  │
                      ▼                  ▼
      ┌───────────────────────┐   ┌──────────────────────┐
      │  STM32F401CC          │   │  Instrument Cluster  │
      │  ┌─────────────────┐  │   │  (Jetson)            │
      │  │  Bootloader     │  │   └──────────────────────┘
      │  │  sectors 0-1    │  │
      │  ├─────────────────┤  │
      │  │  Application    │  │  ◄── this document
      │  │  sectors 2-5    │  │
      │  └─────────────────┘  │
      └───┬───┬───┬───┬───────┘
          │   │   │   │
     I2C1 │   │   │   │ TIM3 IC
          ▼   │   │   ▼
      MPU6050 │   │   HC-SR04 ×2
              │   │   (PA6, PA7 echo;
        ADC1  │   │    PA4, PA5 trigger)
              ▼   │
            LM35  │
                  ▼
              PC13 LED
```

The UART is the node's only link to anything. It carries:

| Direction | Content |
|---|---|
| STM32 → ESP32 | Classification, temperature, ultrasonic, heartbeat and log frames — all CRC32-protected, framed by a GPIO sync pulse |
| ESP32 → STM32 | Exactly one thing: the two-byte `0xAA 0xEB` enter-bootloader command |

There is no request/response, no polling, no acknowledgement of telemetry. The node talks; the gateway listens. The single inbound command is the exception, and it is handled by a dedicated top-priority task ([§28](#28-thread-5--bootloader-receive)).

## 3. Design Principles

Five constraints shaped nearly every decision in this codebase.

### 3.1 No dynamic allocation, anywhere

```c
#define configSUPPORT_STATIC_ALLOCATION   1
#define configSUPPORT_DYNAMIC_ALLOCATION  0
/* No configTOTAL_HEAP_SIZE — no heap exists */
```

Every task, queue and semaphore is created with a `...Static` variant and backed by a file-scope array. The FreeRTOS heap manager (`heap_4.c` and friends) is excluded from the build entirely — not merely unused, but absent. `pvPortMalloc` does not exist to be called.

Consequences that show up throughout:
- Every buffer's size is a compile-time constant, so the RAM ceiling is a link-time fact, not a runtime hope.
- `vApplicationGetIdleTaskMemory()` must be implemented by the application ([§16](#16-static-allocation-policy)).
- There is no `vTaskDelete`; `INCLUDE_vTaskDelete` is 0. Tasks run forever.

### 3.2 No ST HAL

Every peripheral driver in `src/` is written against the reference manual, using hand-written register-map structs in `include/*_REGS.h`. CMSIS supplies only the core headers (`core_cm4.h`, the CMSIS-DSP library) and the startup file.

This is a deliberate cost: roughly 6,500 lines of driver code that ST would have supplied. What it buys is that nothing is opaque — when the I2C DMA path misbehaves, the fix is in a file in this repo, not behind a `HAL_I2C_Master_Receive_DMA` whose state machine you have to reverse-engineer.

### 3.3 MISRA-C:2012 discipline in the ML path

The pipeline modules (`scale.c`, `features.c`, `quantize.c`, `vote.c`, `inference.c`, `RING_BUFFER.c`) carry explicit MISRA compliance claims and follow the associated style:

- Every enumerator has an explicit value (Rule 8.12).
- Every loop bound is statically known (Rule 14.4).
- No recursion.
- Explicit boolean conversions rather than implicit truthiness.
- `U` and `f` suffixes on every literal.

The driver layer follows the same style but with the rule references left as inline comments (`/* U-04: explicit value */`, `/* D-07 */`, `/* I2C-I01 */`), which read as the residue of a review checklist.

### 3.4 Bit-exact parity with the Python training pipeline

The C feature extractor is not "equivalent to" the Python one — it is a transliteration, function by function, with the Python line quoted in a comment above each block:

```c
/* Python equivalent:
 *   amag = np.sqrt(ax**2 + ay**2 + az**2)                         */
```

Population variance (`/N`, matching NumPy's default) rather than sample variance. Linear-interpolation percentiles matching `np.percentile`'s default. `nan_to_num(nan=0, posinf=10, neginf=-10)` reproduced exactly. This discipline is what makes an offline accuracy figure meaningful on-target.

### 3.5 Fail-loud, then fail-safe

The IWDG is not fed by a timer. It is fed by the idle hook, and only when all four monitored tasks have signalled liveness since the last feed ([§32](#32-idle-hook-and-the-watchdog-supervisor)). A task that deadlocks or starves therefore resets the MCU within one watchdog period. This is strictly stronger than a periodic `IWDG_Refresh()`, which would keep petting the dog while the application was dead.

## 4. Capability Matrix

| Capability | Rate | Path | Frame type |
|---|---|---|---|
| Road classification | 4 Hz raw, gated by a 9-vote window | MPU6050 → ring buffer → CNN → vote | `0x01` |
| Ambient temperature | 0.5 Hz | LM35 → ADC1 CH1 (polled) | `0x02` |
| Runtime heartbeat | 0.2 Hz | `vTaskGetInfo` + DWT + stack HWM | `0x03` |
| Structured event log | ≤ 0.25 Hz (hard rate limit) | Any task/ISR → logger queue | `0x04` |
| Obstacle distance ×2 | 4 Hz | HC-SR04 → TIM3 input capture → queue | `0x05` |
| OTA entry | on demand | UART1 RX ISR → Thread 5 → RAM flag + reset | — |
| Watchdog supervision | continuous | 4 alive flags + idle hook | — |

## 5. Source Tree Map

```
Application/
├── platformio.ini                    build configuration (-O2, hard float, CubeAI)
├── link_cubeai.py                    pre-link script: appends the CubeAI runtime .a
├── STM32F401CCFX_FLASH_Sector2.ld    linker script — FLASH origin 0x08008000
├── README.md                         earlier reference doc (partly stale — see §82)
│
├── include/                          all headers
│   ├── STD_TYPES.h  STD_BUFFER.h     shared types and the Buffer_t descriptor
│   ├── FreeRTOSConfig.h              kernel configuration
│   │
│   ├── FRAME.h  frame_request.h      wire protocol
│   ├── heartbeat.h  log_payload.h    payload layouts
│   ├── logger.h                      log codes and macros
│   │
│   ├── norm_params.h  stat_norm.h    ML constants and scaler parameters
│   ├── scale.h  features.h           pipeline stages
│   ├── quantize.h  inference.h  vote.h
│   ├── replay_data.h                 200 pre-recorded IMU samples
│   │
│   ├── RING_BUFFER.h                 SPSC lock-free ring buffer
│   ├── MPU6050.h  HCSR04.h           device drivers
│   │
│   ├── {RCC,GPIO,NVIC,DWT,TIM,ADC,I2C,DMA,UART,FLASH,IWDG,SYSTICK}_INTERFACE.h
│   ├── {RCC,GPIO,NVIC,DWT,TIM,ADC,I2C,DMA,UART,FLASH,IWDG,SYSTICK}_REGS.h
│   └── {I2C,UART}_SERVICE.h          service layers above the raw drivers
│
├── src/                              implementations, one per header
│   ├── main.c                        1,192 lines — tasks, ISR callbacks, init
│   ├── FRAME.c  logger.c
│   ├── scale.c  features.c  quantize.c  inference.c  vote.c  stat_norm.c
│   ├── RING_BUFFER.c
│   ├── MPU6050.c  HCSR04.c
│   ├── RCC.c  GPIO.c  NVIC.c  DWT.c  TIM.c  ADC.c  I2C.c  DMA.c  UART.c
│   ├── FLASH.c  IWGD.c            (note the filename typo: IWGD, not IWDG)
│   └── I2C_SERVICE.c  UART_SERVICE.c
│
├── models/                           model artefacts
│   ├── model_data.h                  16,288-byte .tflite blob (unused at runtime)
│   ├── stat_norm.{c,h}               StandardScaler mean/std, 50 features
│   └── norm_params.h                 duplicate of include/norm_params.h
│
├── lib/
│   ├── FreeRTOS/                     vendored kernel
│   ├── CubeAI/
│   │   ├── Inc/                      X-CUBE-AI headers
│   │   ├── Lib/NetworkRuntime1020_CM4_GCC.a
│   │   └── network/                  generated network.c / network_data.c
│   └── CMSIS_DSP_wrapper/            10-line shim
│
├── tools/                            host-side verification utilities
│   ├── gen_test_vectors.py  tflite_reference.py  mcu_compare.py
│   ├── capture_frame.py  verify_frame.py  capture_dump.py
│   ├── mcu_verify.{c,h}  verify.c  test_vote.c  uart_debug.{c,h}
│   └── collect_data_app.{c,h}        earlier bare-metal data-collection firmware
│
├── data/                             CSV recordings + replay-header generators
│   ├── rough/csv_to_replay_header.py
│   └── smooth/{csv_to_replay_header.py, stage5_pyref.py}
│
└── test/                             captured UART logs from bring-up
```

Two duplications worth flagging up front: `norm_params.h` and `stat_norm.{c,h}` exist in **both** `include/` and `models/`. The build's include order (`-Imodels` comes after the implicit `include/`) determines which wins. They are currently byte-identical, so nothing breaks — but a divergence would be silent and would change inference results. See [§82](#82-known-gaps).

---

# Part II — Platform

## 6. Target Microcontroller

| Property | Value |
|---|---|
| Part | STM32F401CCU6 |
| Core | ARM Cortex-M4F, ARMv7E-M, Thumb-2 |
| FPU | FPv4-SP-D16 — single precision, hardware, **used** |
| DSP extensions | SIMD (`SMLAD`, `QADD`…), single-cycle MAC — used by CMSIS-DSP |
| Max clock | 84 MHz |
| Flash | 256 KB; application occupies 224 KB (sectors 2–5) |
| SRAM | 64 KB single bank; 8 bytes reserved for the boot flag |
| DMA | 2 controllers × 8 streams × 8 channels |
| Timers | TIM1, TIM2–TIM5, TIM9–TIM11 |
| ADC | 1 × 12-bit SAR, up to 16 external channels |
| Comms | 3 × USART, 3 × I2C, 4 × SPI, USB OTG FS |
| Watchdogs | IWDG (LSI-clocked, independent), WWDG |
| Debug | SWD, plus DWT/ITM trace |

The FPU is the reason this project is viable. Feature extraction computes 50 statistics over a 50×6 float window, including six 64-point real FFTs — roughly 40,000 floating-point operations per inference at 4 Hz. Without hardware float that would be soft-float library calls at 20–100 cycles each; with it, most are single-cycle `VMUL`/`VADD`.

The DSP extensions matter for CMSIS-DSP's `arm_rfft_fast_f32`, which uses them in its radix-4 butterflies.

## 7. Clock Tree

The application configures 84 MHz from the **internal** HSI oscillator — unlike the bootloader, which uses the external crystal.

```
  HSI  16 MHz  (internal RC, ±1 % at 25 °C, ±4 % over temperature)
    │
    ├─► PLLM = 8    ──►  VCO input  = 16 / 8    = 2 MHz
    ├─► PLLN = 168  ──►  VCO output = 2 × 168   = 336 MHz
    ├─► PLLP = /4   ──►  PLLCLK     = 336 / 4   = 84 MHz  ──► SYSCLK
    └─► PLLQ = 7    ──►  PLL48CLK   = 336 / 7   = 48 MHz  (unused — no USB)

  SYSCLK 84 MHz
    ├─ AHB  /1  ──► HCLK  = 84 MHz   CPU, DMA1, DMA2, GPIO, DWT, Flash
    ├─ APB1 /2  ──► PCLK1 = 42 MHz   I2C1, TIM2, TIM3
    │                ↳ timer clock  = 2 × PCLK1 = 84 MHz  (APB1 ≠ /1)
    └─ APB2 /1  ──► PCLK2 = 84 MHz   USART1, ADC1
                     ↳ ADC clock    = PCLK2 / 4 = 21 MHz  (ADC_CCR.ADCPRE = /4)

  LSI  ≈ 32 kHz (17–47 kHz over process and temperature)  ──► IWDG
```

Set up by a single call in `main()`:

```c
RCC_INIT_84MHz_HSI();
```

### 7.1 HSI versus HSE — a deliberate divergence from the bootloader

| | Bootloader | Application |
|---|---|---|
| PLL source | HSE 25 MHz crystal | HSI 16 MHz internal RC |
| PLLM / PLLN / PLLP | 25 / 336 / 4 | 8 / 168 / 4 |
| SYSCLK | 84 MHz | 84 MHz |
| Accuracy | crystal, ±20 ppm typical | RC, ±1 % at 25 °C, up to ±4 % across −40…85 °C |

Both land on 84 MHz, so the handover is seamless. But the accuracy difference is real and has a consequence: **the UART baud rate is derived from HSI.** At ±1 % the 115200 link is fine (UART tolerates roughly ±2 % total between the two ends). At the ±4 % extremes of the HSI specification, combined with the ESP32's own tolerance, the link would start dropping frames.

Nothing in the current design compensates. There is no HSI trimming against LSE, no baud-rate auto-detection. In a bench environment at room temperature this is invisible; in a vehicle cabin at −20 °C or +70 °C it is a latent risk. Recorded in [§82](#82-known-gaps).

Why HSI at all, when the board has a crystal the bootloader already uses? Most likely because the application was developed on a board where the crystal was not populated, and HSI removes a hardware dependency. It is worth revisiting.

### 7.2 Timer clock doubling

APB1 is prescaled to /2, giving PCLK1 = 42 MHz. But the STM32 timer clock multiplexer doubles the APB clock whenever the APB prescaler is not 1:

```
   APB1 prescaler == 1   →   TIMxCLK = PCLK1
   APB1 prescaler  > 1   →   TIMxCLK = 2 × PCLK1
```

So TIM2 and TIM3 both see 84 MHz, not 42 MHz. Every prescaler value in the codebase assumes this:

| Timer | PSC | ARR | Derivation |
|---|---|---|---|
| TIM2 | 8399 | 99 | 84 MHz / 8400 = 10 kHz; 10 kHz / 100 = **100 Hz** |
| TIM3 | 83 | 65535 | 84 MHz / 84 = **1 MHz** → 1 µs per tick; 65.535 ms full-scale |

Getting the doubling wrong would halve both rates — the IMU would sample at 50 Hz and the ultrasonic timer would count in 2 µs steps, doubling every reported distance.

## 8. Pin Assignment

| Pin | Peripheral | Function | Mode | Speed | Pull |
|---|---|---|---|---|---|
| PA1 | ADC1_IN1 | LM35 analog input | Analog | Low | None |
| PA4 | GPIO out | HC-SR04 #1 trigger | Push-pull | Very high | None |
| PA5 | GPIO out | HC-SR04 #2 trigger | Push-pull | Very high | None |
| PA6 | TIM3_CH1 (AF2) | HC-SR04 #1 echo | Alternate | Very high | Pull-down |
| PA7 | TIM3_CH2 (AF2) | HC-SR04 #2 echo | Alternate | Very high | Pull-down |
| PA8 | GPIO out | Frame sync strobe | Push-pull | Very high | None |
| PA9 | USART1_TX (AF7) | Telemetry out (DMA) | Alternate | Very high | None |
| PA10 | USART1_RX (AF7) | Bootloader command in (IRQ) | Alternate | Very high | Pull-up |
| PB6 | I2C1_SCL (AF4) | MPU6050 clock | Alternate | High | Pull-up |
| PB7 | I2C1_SDA (AF4) | MPU6050 data | Alternate | High | Pull-up |
| PC13 | GPIO out | Activity LED | Push-pull | Low | None |

```
                 STM32F401CCU6 (UFQFPN48)
        ┌──────────────────────────────────────────┐
        │                                          │
   PA1 ─┤ ADC1_IN1        ◄── LM35 Vout            │
   PA4 ─┤ GPIO out        ──► HC-SR04 #1 TRIG      │
   PA5 ─┤ GPIO out        ──► HC-SR04 #2 TRIG      │
   PA6 ─┤ TIM3_CH1  AF2   ◄── HC-SR04 #1 ECHO      │
   PA7 ─┤ TIM3_CH2  AF2   ◄── HC-SR04 #2 ECHO      │
   PA8 ─┤ GPIO out        ──► frame sync (to ESP32)│
   PA9 ─┤ USART1_TX AF7   ──► ESP32 RX             │
  PA10 ─┤ USART1_RX AF7   ◄── ESP32 TX             │
   PB6 ─┤ I2C1_SCL  AF4   ◄─► MPU6050 SCL          │
   PB7 ─┤ I2C1_SDA  AF4   ◄─► MPU6050 SDA          │
  PC13 ─┤ GPIO out        ──► LED (active low on   │
        │                     most Blackpill boards)│
        └──────────────────────────────────────────┘
```

### 8.1 Pull configuration rationale

**PA10 (UART RX) has a pull-up.** An idle UART line rests high. Without a pull-up, a disconnected gateway leaves PA10 floating, and induced noise generates spurious start bits — which would wake Thread 5 on every glitch and pollute the RX ring buffer. The pull-up guarantees a clean idle.

**PA6 / PA7 (echo) have pull-downs.** The HC-SR04 echo line is driven low when idle, but if a sensor is unplugged the pin floats. A pull-down makes an absent sensor read as "no echo" rather than as random capture events. This is what turns a disconnected sensor into a clean 50 ms timeout in Thread 7 rather than into garbage distances.

**PB6 / PB7 (I2C) have internal pull-ups and are open-drain.** The internal pull-ups on STM32 are weak (roughly 40 kΩ), which is marginal for 400 kHz Fast-mode I2C — the standard recommendation is 2.2–4.7 kΩ external. If the board has external pull-ups, the internal ones are harmless in parallel. If it does not, the rise time at 400 kHz will be marginal and the bus will be sensitive to capacitance. Worth checking on hardware.

**PA9 (UART TX) has no pull.** Correct — the peripheral drives it push-pull in both directions.

### 8.2 PA8, the frame sync strobe

PA8 is not a peripheral pin. It is a plain GPIO that Thread 3 raises immediately before starting a DMA transmission and lowers when the transmission completes:

```c
UART_SVC_TransmitDMA(UART1_ID, &buf);
GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_SET);
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_RESET);
```

Its purpose is documented in `FRAME.c`:

> *"An external GPIO line is toggled to mark frame start; this saves one byte per frame and avoids an additional state machine on the ESP32."*

See [§55](#55-the-gpio-sync-line) for what it actually achieves versus what it was intended to achieve.

## 9. Peripheral Allocation

```
   AHB1
   ┌──────────────────────────────────────────────────────────────┐
   │  DMA1                                                        │
   │    Stream 0, Channel 1  ──►  I2C1_RX   (MPU6050 burst read)  │
   │                                                              │
   │  DMA2                                                        │
   │    Stream 7, Channel 4  ──►  USART1_TX (telemetry frames)    │
   │                                                              │
   │  GPIOA, GPIOB, GPIOC   — clocks enabled in main()            │
   │  CRC                   — NOT enabled; CRC32 is software      │
   └──────────────────────────────────────────────────────────────┘

   APB1  (PCLK1 = 42 MHz, timer clock 84 MHz)
   ┌──────────────────────────────────────────────────────────────┐
   │  TIM2   100 Hz update event  ──►  wakes Thread 1             │
   │  TIM3   input capture CH1/CH2 ─►  HC-SR04 echo timing        │
   │  I2C1   400 kHz Fast mode, DMA RX  ──►  MPU6050              │
   └──────────────────────────────────────────────────────────────┘

   APB2  (PCLK2 = 84 MHz)
   ┌──────────────────────────────────────────────────────────────┐
   │  USART1  115200 8N1, DMA TX / IRQ RX  ──►  ESP32 gateway     │
   │  ADC1    12-bit, CH1, software-triggered polling ──► LM35    │
   └──────────────────────────────────────────────────────────────┘

   Core
   ┌──────────────────────────────────────────────────────────────┐
   │  SysTick  1 kHz  ──►  FreeRTOS tick                          │
   │  DWT      free-running cycle counter  ──►  runtime stats      │
   │  IWDG     3000 ms nominal, LSI-clocked                       │
   └──────────────────────────────────────────────────────────────┘
```

### 9.1 DMA stream selection is not arbitrary

The STM32F4 DMA request mapping is fixed in silicon — each peripheral request line is hard-wired to specific (stream, channel) pairs. From RM0368 tables 27 and 28:

| Request | Valid mappings | Chosen |
|---|---|---|
| I2C1_RX | DMA1 S0 C1, DMA1 S5 C1 | **DMA1 S0 C1** |
| USART1_TX | DMA2 S7 C4 | **DMA2 S7 C4** (the only option) |

USART1_TX has exactly one mapping, so there was no choice. I2C1_RX had two; stream 0 was picked, leaving stream 5 free.

### 9.2 The CRC peripheral is not used

The bootloader uses the hardware CRC unit. The application computes the identical CRC in software (`Frame_CRC32` in `FRAME.c`), and `PERIPH_CRC` is never enabled.

Why the asymmetry? The application's CRC input is the frame it just built in RAM — feeding 30-odd bytes through the hardware unit one word at a time would cost about as much as the software loop, and the software version keeps `FRAME.c` free of any peripheral dependency, which is what lets `tools/verify_frame.py` and `tools/mcu_verify.c` reuse the same algorithm off-target. That said, the software loop runs 32 iterations per byte ([§54](#54-the-crc32-quirk)) — for a 37-byte heartbeat frame that is 1,184 iterations, perhaps 5,000 cycles, at 0.2 Hz. Negligible either way.

## 10. Flash Layout and VTOR Relocation

```
0x08000000 ┌────────────────────────────────────────────────┐
           │  Sectors 0-1 — 32 KB — BOOTLOADER              │
           │  (not touched by the application)              │
0x08008000 ├════════════════════════════════════════════════┤ ◄── application base
           │  .isr_vector                                   │
           │    +0x00  initial MSP  = _estack = 0x2000FFF8  │
           │    +0x04  Reset_Handler                        │
           │    +0x08  NMI, HardFault, ... , IRQ handlers   │
           ├────────────────────────────────────────────────┤
           │  .text     ≈ 189.7 KB                          │
           │    application code                            │
           │    CMSIS-DSP  (incl. ~109 KB of FFT tables)    │
           │    CubeAI runtime                              │
           │    FreeRTOS kernel                             │
           ├────────────────────────────────────────────────┤
           │  .rodata                                       │
           │    s_network_weights_array_u64   5,864 B       │
           │    stat_mean[50], stat_std[50]     400 B       │
           │    replay_samples[200]           2,800 B       │
           ├────────────────────────────────────────────────┤
           │  .data initialisers (LMA)          3,924 B     │
           ├────────────────────────────────────────────────┤
           │  free — 35,796 B                               │
0x0803FFFF └────────────────────────────────────────────────┘
```

### 10.1 VTOR relocation is the first executable statement

```c
int main(void){
    /* SCB->VTOR is at 0xE000ED08. Write the app base address so all interrupts
       are dispatched through our vector table, not the bootloader's at 0x08000000 */
    *((volatile uint32_t *)0xE000ED08U) = 0x08008000U;

    RCC_INIT_84MHz_HSI();
    ...
}
```

This is mandatory and its absence would be catastrophic in a way that is hard to diagnose. The bootloader jumps to the application's `Reset_Handler` **without** changing VTOR ([`../Bootloader/BOOTLOADER.md`](../Bootloader/BOOTLOADER.md) §15). So on entry to `main()`, VTOR still points at `0x08000000` — the bootloader's table.

If the write were removed:

```
   TIM2 fires
       │
       ▼
   Core reads vector at VTOR + 0x0B0
       = 0x08000000 + 0x0B0
       = the BOOTLOADER's TIM2 entry
       = Default_Handler (an infinite loop)
       │
       ▼
   Node hangs on the first timer tick, ~10 ms after the scheduler starts.
```

The symptom would be "the board boots and immediately goes dead", with a debugger showing the PC inside the bootloader's Flash — which reads as a wild jump rather than as a vector-table problem.

VTOR is written *before* `RCC_INIT_84MHz_HSI()` and before any NVIC line is enabled, so the window in which the wrong table is live contains no possible interrupt.

### 10.2 The alignment requirement

`SCB->VTOR` requires the table to be aligned to the next power of two greater than or equal to `4 × (number of vectors)`. The STM32F401 has 85 IRQs plus 16 core exceptions = 101 entries = 404 bytes, so the alignment requirement is 512 bytes.

`0x08008000` is 32 KB-aligned. Satisfied with enormous margin — and it has to be, because it is also a Flash sector boundary.

## 11. SRAM Budget

Measured: `.data` 3,924 B + `.bss` 27,160 B = **31,084 B** of the 65,528 available (47.4 %).

```
0x20000000 ┌────────────────────────────────────────────────────────┐
           │ .data — 3,924 B  (initialised globals, copied at boot)  │
           ├────────────────────────────────────────────────────────┤
           │ .bss — 27,160 B                                        │
           │                                                        │
           │   Task stacks                            10,880 B      │
           │     Thread 2 "ML"        1280 words       5,120 B      │
           │     Logger                256 words       1,024 B      │
           │     Thread 4 "HB"         256 words       1,024 B      │
           │     Thread 5 "BL_RX"      256 words       1,024 B      │
           │     Thread 6 "TEMP"       192 words         768 B      │
           │     Idle                  128 words         512 B      │
           │     Thread 3 "TX"         128 words         512 B      │
           │     Thread 7 "ULTRA"      128 words         512 B      │
           │     Thread 1 "Sensor"      96 words         384 B      │
           │                                                        │
           │   ring_buffer_instance                    1,800 B      │
           │   UART service rings (3 × 512 B)          1,536 B      │
           │   CubeAI activations arena                1,464 B      │
           │   Thread 2 pipeline buffers .bss          2,350 B      │
           │     scaled[50][6] float                   1,200 B      │
           │     flat_window[50][6] int16                600 B      │
           │     ts_q[300] int8                          300 B      │
           │     features[50] float                      200 B      │
           │     stat_q[50] int8                          50 B      │
           │   features.c FFT statics                    644 B      │
           │   Logger dedup/debounce arrays            1,024 B      │
           │   s_tx_frame                                255 B      │
           │   g_frame_queue buffer (6 × 35)             210 B      │
           │   Logger queue buffer (16 × 10)             160 B      │
           │   FreeRTOS TCBs (9 tasks)                  ~800 B      │
           │   FreeRTOS queue/semaphore structs         ~400 B      │
           │   MPU6050 statics, misc globals            ~200 B      │
           │   remainder (kernel lists, CubeAI ctx)   ~5,400 B      │
           ├────────────────────────────────────────────────────────┤
           │ heap   1,024 B  (_Min_Heap_Size — unused, no malloc)   │
           │ stack  2,048 B  (_Min_Stack_Size — ISR/main stack)     │
           ├────────────────────────────────────────────────────────┤
           │ free — ≈ 31,400 B                                      │
0x2000FFF7 ├────────────────────────────────────────────────────────┤
0x2000FFF8 │ RAM_BOOT — 8 B — the shared boot flag                  │
0x2000FFFF └────────────────────────────────────────────────────────┘
```

Two observations:

**Thread 2's stack is 47 % of all stack memory.** 5,120 bytes against the 5,760 held by the other eight tasks combined. Justified — see [§21](#21-stack-sizing) for the measurement.

**The UART service allocates for three instances but uses one.** `static UART_SVC_Instance_t svc[3];` gives every one of USART1/2/6 a 256-byte TX ring and a 256-byte RX ring. Only UART1 is initialised. That is 1,024 bytes of permanently untouched `.bss` — 3.3 % of the total RAM footprint, recoverable by making the array size a configuration constant.

**There is 31 KB of headroom.** Nothing is tight. Thread 2's stack could double without concern.

## 12. Linker Script

[`STM32F401CCFX_FLASH_Sector2.ld`](STM32F401CCFX_FLASH_Sector2.ld), 165 lines. The header comment carries the memory map:

```
 * Flash layout (STM32F401CC, 256 KB total):
 *   Sector 0 : 0x08000000  16 KB  -> Bootloader
 *   Sector 1 : 0x08004000  16 KB  -> Bootloader
 *   Sector 2 : 0x08008000  16 KB  -> App starts HERE  <-- ORIGIN
 *   Sector 3 : 0x0800C000  16 KB  -> App
 *   Sector 4 : 0x08010000  64 KB  -> App
 *   Sector 5 : 0x08020000 128 KB  -> App
 *   Available to app       224 KB
```

```ld
MEMORY
{
  RAM       (xrw) : ORIGIN = 0x20000000, LENGTH = 64K - 8   /* 0x20000000 – 0x2000FFF7 */
  RAM_BOOT  (xrw) : ORIGIN = 0x2000FFF8, LENGTH = 8         /* 0x2000FFF8 – 0x2000FFFF */
  FLASH     (rx)  : ORIGIN = 0x08008000, LENGTH = 224K      /* Sectors 2-5 */
}
```

Unlike the bootloader's script — which declares the whole 256 KB and relies on the image staying small — this one is correctly bounded. An application that grew past 224 KB would fail to link rather than silently overwrite the bootloader. Given the image is at 84.4 % of that budget, this constraint is doing real work.

```ld
_estack = ORIGIN(RAM) + LENGTH(RAM);      /* = 0x2000FFF8 */
_Min_Heap_Size  = 0x400;   /* 1 KB  */
_Min_Stack_Size = 0x800;   /* 2 KB  */
```

`_estack` lands exactly on the first byte of the boot-flag region. The stack grows downward from there, so the flag is never in its path — the same arrangement as the bootloader, and it has to be identical for the handover to work.

The `_Min_Heap_Size` of 1 KB is dead space: with `configSUPPORT_DYNAMIC_ALLOCATION = 0` and no `printf`, nothing calls `malloc`. It could be reclaimed.

`_Min_Stack_Size` of 2 KB is the **ISR and pre-scheduler stack** — the MSP. Every task has its own stack; the MSP is used by `main()` before `vTaskStartScheduler()` and by every interrupt handler thereafter. 2 KB is generous for the ISRs in this design (the deepest is `on_hcsr04_capture`, which calls `TIM_IC_GetCapture` and `xQueueSendFromISR` — perhaps 200 bytes).

```ld
  .boot_flag (NOLOAD) :
  {
    KEEP(*(.boot_flag))
  } >RAM_BOOT
```

Reserves the shared word without emitting initialiser bytes. `.bss` is placed `>RAM` and therefore provably stops at `0x2000FFF7`, which is what guarantees the application's startup code cannot clobber a flag the bootloader is about to read.

```ld
  /DISCARD/ :
  {
    libc.a(*)
    libm.a(*)
    libgcc.a(*)
  }
```

Inherited from the ST template. As in the bootloader, this does not do what it appears to — PlatformIO links `libc_nano.a` and `libgcc.a` under paths that this literal pattern does not match, and the image demonstrably contains libgcc's soft-float helpers and `math.h` functions (`sqrtf`, `floorf`). Harmless, but misleading.

## 13. Build System

[`platformio.ini`](platformio.ini):

```ini
[platformio]
default_envs = genericSTM32F401CC

[env:genericSTM32F401CC]
platform = ststm32
board = genericSTM32F401CC
framework = cmsis
upload_protocol = stlink
debug_tool = stlink
build_type = debug

board_build.ldscript = STM32F401CCFX_FLASH_Sector2.ld
board_build.fpu = fpv4-sp-d16
board_build.float-abi = hard

build_src_filter =
    +<*>
    +<../lib/CubeAI/network/>
    +<../lib/CMSIS_DSP_wrapper/>

build_flags =
    -O2
    -DSTM32F401xC
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -ffast-math
    -Wall
    -Wextra
    -Ilib/CubeAI/Inc
    -Ilib/CubeAI/network
    -Imodels
    -DARM_MATH_CM4
    -Ilib/FreeRTOS/include
    -Ilib/FreeRTOS/portable/GCC/ARM_CM4F
    -I/home/ehab/.platformio/packages/framework-cmsis/CMSIS/DSP/Include
    -I/home/ehab/.platformio/packages/framework-cmsis/CMSIS/Core/Include

build_unflags = -Os -O0 -mfloat-abi=soft -msoft-float

debug_build_flags = -Og -ggdb3 -g3

extra_scripts = pre:link_cubeai.py

check_tool = cppcheck
check_flags = cppcheck: --enable=all
```

### 13.1 `build_type = debug` with `-O2`

These look contradictory. `build_type = debug` normally implies `-Og`, but `build_flags` explicitly adds `-O2` and `build_unflags` strips `-Os` and `-O0`. Since `build_flags` are appended after the build-type defaults, `-O2` wins.

The net effect is an `-O2` build that also carries `-g` debug info. Debug info costs nothing in Flash (it lives in non-loadable ELF sections — note `firmware.elf` is 1.2 MB while `firmware.bin` is 193 KB), so this is the right combination: full optimisation with full symbols.

`-O2` rather than `-Os` is a deliberate choice for the ML path. The inference and feature-extraction inner loops benefit substantially from `-O2`'s loop unrolling and inlining, and the Flash budget has 35 KB of headroom.

### 13.2 `-ffast-math` and its implications

`-ffast-math` enables:

| Sub-flag | Effect |
|---|---|
| `-fno-math-errno` | Math functions do not set `errno` — allows `sqrtf` to compile to a single `VSQRT.F32` |
| `-funsafe-math-optimizations` | Permits reassociation, e.g. `(a+b)+c → a+(b+c)` |
| `-ffinite-math-only` | **Assumes no NaN or Inf ever occurs** |
| `-fno-signed-zeros` | Ignores the distinction between `+0.0` and `-0.0` |
| `-fno-trapping-math` | Assumes FP operations do not trap |
| `-fassociative-math`, `-freciprocal-math` | Allows `a/b → a * (1/b)` |

`-ffinite-math-only` is the problematic one, and it interacts directly with a piece of code in `features.c`:

```c
static void feat_sanitize(float32_t *v, uint32_t n){
    for (uint32_t i = 0U; i < n; ++i){
        if (isnan(v[i]))       { v[i] = 0.0f; }
        else if (isinf(v[i]))  { v[i] = (v[i] > 0.0f) ? 10.0f : -10.0f; }
    }
}
```

Under `-ffinite-math-only`, the compiler is entitled to assume `isnan(x)` is always false and `isinf(x)` is always false, and to delete the entire function body as dead code.

**Whether it actually does so depends on the GCC version and how `isnan`/`isinf` expand.** Newer GCC keeps `__builtin_isnan` live even under `-ffinite-math-only` in many cases; older versions fold it to `0`. This is not a theoretical concern: `feat_sanitize` is the guard that keeps a division-by-near-zero in the variance-ratio features from propagating an `Inf` into the quantiser and thence into the model.

**This should be verified on the actual toolchain.** The test is simple — feed a window that produces a zero-variance channel and check whether features 48/49 come out as `10.0f` or as `inf`. If the sanitiser has been optimised away, either drop `-ffast-math` for `features.c` specifically (`#pragma GCC optimize`) or replace the `isnan`/`isinf` calls with bit-pattern tests that the optimiser cannot reason about. Recorded in [§82](#82-known-gaps).

The upside of `-ffast-math` is real: `sqrtf` becomes one instruction instead of a library call, and it is used 100 times per inference (twice per window sample in the magnitude computation) plus inside every `feat_std` and `feat_rms`.

### 13.3 `link_cubeai.py`

```python
Import("env")
import os

lib_path = os.path.join(env.subst("$PROJECT_DIR"), "lib", "CubeAI", "Lib")
lib_file = os.path.join(lib_path, "NetworkRuntime1020_CM4_GCC.a")

env.Append(LINKFLAGS=["-mcpu=cortex-m4", "-mthumb",
                      "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"])

# Method: wrap with --start-group / --end-group to force re-scanning
env.Append(_LIBFLAGS=" -Wl,--start-group " + lib_file + " -Wl,--end-group")
```

The `--start-group` / `--end-group` wrapper exists because of a circular dependency: the generated `network.c` calls into `NetworkRuntime1020_CM4_GCC.a`, and the runtime library calls back into symbols defined by `network.c` (the layer forward functions and weight tables). A single linker pass over the archive would leave undefined references; the group forces the linker to re-scan until closure.

The explicit float-ABI flags on `LINKFLAGS` matter because the CubeAI archive is compiled hard-float. Linking a hard-float archive into a soft-float image produces `uses VFP register arguments, output does not` errors.

### 13.4 The `build_flags` include a machine-specific absolute path

```
-I/home/ehab/.platformio/packages/framework-cmsis/CMSIS/DSP/Include
-I/home/ehab/.platformio/packages/framework-cmsis/CMSIS/Core/Include
```

These will not resolve on any other machine or CI runner. The portable form is `$PROJECT_PACKAGES_DIR/framework-cmsis/...`, which PlatformIO expands. Recorded in [§82](#82-known-gaps).

### 13.5 `build_src_filter`

```
+<*>
+<../lib/CubeAI/network/>
+<../lib/CMSIS_DSP_wrapper/>
```

By default PlatformIO compiles `src/` only and treats `lib/` subdirectories as separate library builds. Explicitly pulling `lib/CubeAI/network/` into the main compilation unit set means `network.c` and `network_data.c` get the same `build_flags` — critically including `-mfloat-abi=hard`, without which they would be compiled soft-float and fail to link against the runtime archive.

`lib/CMSIS_DSP_wrapper/cmsis_dsp_wrapper.c` is a 10-line shim that exists solely to force CMSIS-DSP source into the build.

## 14. Footprint Analysis

```
   text     data      bss      dec      hex
 189656     3924    27160   220740    35e44
```

| Region | Bytes | Budget | Utilisation |
|---|---|---|---|
| Flash (`text` + `data`) | 193,580 | 229,376 | **84.4 %** |
| RAM (`data` + `bss`) | 31,084 | 65,528 | 47.4 % |

Flash at 84 % is the tighter of the two, and the reason is startling.

### 14.1 Half the Flash is FFT tables that are never used

The largest `.rodata` objects, from the symbol table:

| Symbol | Bytes |
|---|---|
| `twiddleCoef_4096` | 32,768 |
| `twiddleCoef_rfft_4096` | 16,384 |
| `twiddleCoef_2048` | 16,384 |
| `twiddleCoef_rfft_2048` | 8,192 |
| `twiddleCoef_1024` | 8,192 |
| `armBitRevIndexTable4096` | 8,064 |
| `armBitRevIndexTable2048` | 7,616 |
| `s_network_weights_array_u64` | 5,864 |
| `twiddleCoef_rfft_1024` | 4,096 |
| `twiddleCoef_512` | 4,096 |
| `armBitRevIndexTable1024` | 3,600 |
| `replay_samples` | 2,800 |
| `twiddleCoef_rfft_512` | 2,048 |
| `twiddleCoef_256` | 2,048 |
| **Twiddle + bit-reverse subtotal** | **≈ 113,488** |

The application performs exactly one kind of FFT: a **64-point** real FFT, six times per inference, in `feat_hfe_channel`. It needs `twiddleCoef_rfft_64` and `armBitRevIndexTable64` — a few hundred bytes.

The cause is `arm_rfft_fast_init_f32()`:

```c
(void)arm_rfft_fast_init_f32(&s_fft_inst, (uint16_t)FFT_N);
```

This is the *generic* initialiser. Internally it is a switch over every supported length:

```c
switch (fftLen) {
    case 4096: ... arm_cfft_sR_f32_len2048 ... break;
    case 2048: ... break;
    case 1024: ... break;
    case 512:  ... break;
    case 256:  ... break;
    case 128:  ... break;
    case 64:   ... break;   /* ← the only reachable case here */
    case 32:   ... break;
}
```

Because `FFT_N` is not a compile-time constant from the linker's point of view (it is passed as a runtime argument), **every branch is live**, and each branch references a different table. The linker cannot garbage-collect any of them.

**The fix is one line.** CMSIS-DSP provides length-specific initialisers:

```c
(void)arm_rfft_fast_init_64_f32(&s_fft_inst);
```

This references only the 64-point tables. The expected saving is on the order of **110 KB**, which would take Flash utilisation from 84.4 % down to roughly **36 %**.

That is the single highest-value change available in this codebase. It is recorded as the top item in [§82](#82-known-gaps).

> **Caveat.** `arm_rfft_fast_init_64_f32` was added in CMSIS-DSP 1.10. The PlatformIO `framework-cmsis` package version needs checking. If it is absent, an equivalent effect is achievable by initialising the `arm_rfft_fast_instance_f32` struct by hand from the 64-point tables — about six lines, all documented in `arm_rfft_fast_init_f32`'s source.

### 14.2 Where the rest goes

Largest `.text` symbols:

| Symbol | Bytes | Origin |
|---|---|---|
| `_lite_kernel_nl_softmax_is8os8` | 2,728 | CubeAI runtime |
| `forward_lite_conv2d_sssa8_ch` | 2,620 | CubeAI runtime |
| `_lite_kernel_nl_softmax_iu8ou8` | 2,000 | CubeAI runtime (unused variant) |
| `forward_concat` | 1,694 | CubeAI runtime |

Rough attribution of the 189.7 KB:

```
   CMSIS-DSP tables (unused)              ≈ 113 KB   ◄── §14.1
   CubeAI runtime + generated network     ≈  22 KB
   CMSIS-DSP FFT/statistics code          ≈  12 KB
   Application code (main, tasks, pipeline) ≈ 12 KB
   Peripheral drivers                     ≈  14 KB
   FreeRTOS kernel                        ≈   8 KB
   newlib-nano, libgcc soft helpers       ≈   5 KB
   .rodata: weights, scaler, replay data  ≈   9 KB
```

Remove the dead tables and the picture inverts entirely: the CubeAI runtime becomes the dominant consumer, at about a quarter of a 77 KB image.

### 14.3 RAM headroom

47.4 % used, 31 KB free. Nothing is under pressure. The largest single objects are `s_thread2_stack` (5,120 B) and `ring_buffer_instance` (1,800 B), both of which could grow substantially without concern.
---

# Part III — RTOS Architecture

## 15. FreeRTOS Configuration

[`include/FreeRTOSConfig.h`](include/FreeRTOSConfig.h), 141 lines. The settings that matter, grouped by consequence.

### 15.1 Scheduler

| Setting | Value | Consequence |
|---|---|---|
| `configUSE_PREEMPTION` | 1 | A higher-priority task that becomes ready preempts immediately |
| `configUSE_TIME_SLICING` | **0** | Equal-priority ready tasks are **not** rotated on the tick |
| `configUSE_PORT_OPTIMISED_TASK_SELECTION` | 1 | Next-task selection uses `CLZ`, O(1) instead of O(n) |
| `configUSE_TICKLESS_IDLE` | 0 | The tick keeps running when idle |
| `configCPU_CLOCK_HZ` | 84,000,000 | Must match the actual SYSCLK — used to derive the SysTick reload |
| `configTICK_RATE_HZ` | 1000 | 1 ms granularity |
| `configMAX_PRIORITIES` | 5 | Priorities 0…4 |
| `configMINIMAL_STACK_SIZE` | 128 words | Idle task stack |
| `configIDLE_SHOULD_YIELD` | 1 | Idle yields to any ready priority-0 task |

**`configUSE_TIME_SLICING = 0` deserves scrutiny.** The header's own comment justifies it as:

```c
#define configUSE_TIME_SLICING    0   /* distinct priorities */
```

That comment is **stale**. Four tasks share priority 1: Thread 3 (TX), Thread 4 (Heartbeat), Thread 6 (Temperature) and Thread 7 (Ultrasonic).

With time slicing off, equal-priority tasks switch only when the running one blocks or explicitly yields. Is that safe here?

| Task | Blocks on | Blocks unconditionally? |
|---|---|---|
| Thread 3 | `xQueueReceive(g_frame_queue, …, portMAX_DELAY)` then `ulTaskNotifyTake(…, portMAX_DELAY)` | Yes |
| Thread 4 | `vTaskDelayUntil(…, 1000 ms)` | Yes |
| Thread 6 | `vTaskDelayUntil(…, 2000 ms)` then `ADC_Read` (bounded spin) | Yes |
| Thread 7 | `vTaskDelayUntil(…, 250 ms)` then `xQueueReceive(…, 50 ms)` | Yes |

Every one of them blocks on every loop iteration, so no task can monopolise its priority level. It works — but by property of the task bodies, not by configuration. Any future priority-1 task with a compute loop that does not block would starve the other three indefinitely, and the failure would look like "the heartbeat stopped" rather than like a scheduling problem.

Turning time slicing on would cost one context switch per tick in the worst case and would remove the hazard. Recorded in [§82](#82-known-gaps).

**`configIDLE_SHOULD_YIELD = 1`** has a similarly stale comment:

```c
#define configIDLE_SHOULD_YIELD   1   /* no priority-0 tasks */
```

There *is* a priority-0 task — the Logger. So the setting is not inert; it makes the idle task yield to the Logger whenever the Logger becomes ready. That is the desired behaviour, so the setting is right and only the comment is wrong. But the comment matters, because the idle hook is where the watchdog is fed ([§32](#32-idle-hook-and-the-watchdog-supervisor)) — anyone reasoning about how often the idle task runs needs to know the Logger competes with it.

### 15.2 Memory

```c
#define configSUPPORT_STATIC_ALLOCATION    1
#define configSUPPORT_DYNAMIC_ALLOCATION   0
/* No configTOTAL_HEAP_SIZE — no heap exists */
```

Covered in [§16](#16-static-allocation-policy).

### 15.3 Synchronisation primitives

| Setting | Value | Note |
|---|---|---|
| `configUSE_MUTEXES` | 0 | No mutexes — and therefore no priority inheritance |
| `configUSE_RECURSIVE_MUTEXES` | 0 | |
| `configUSE_COUNTING_SEMAPHORES` | 0 | Binary semaphores only |
| `configUSE_TASK_NOTIFICATIONS` | 1 | The primary IPC mechanism here |
| `configUSE_QUEUE_SETS` | 0 | |
| `configQUEUE_REGISTRY_SIZE` | 0 | No debugger queue names |
| `configUSE_TIMERS` | 0 | No software timers — hardware TIM2/TIM3 instead |

**No mutexes means no priority inversion protection.** That is safe here only because there is no shared resource guarded by a lock. The one genuinely shared resource — UART1 — is protected by a *design* invariant instead: exactly one task (Thread 3) ever transmits.

Except that is not quite true. Thread 5 also transmits, using `UART_Transmit_Polling`, when it ACKs the bootloader command ([§28](#28-thread-5--bootloader-receive)). Since Thread 5 immediately resets the MCU afterwards, the collision window is real but its consequence is not — a garbled telemetry frame moments before a reset. Worth knowing; not worth a mutex.

**Task notifications instead of semaphores** is the right call on Cortex-M: a notification is a 32-bit word in the TCB, and `ulTaskNotifyTake` is roughly 3× faster than `xSemaphoreTake` because there is no queue structure to traverse. Four of the six wake paths use notifications.

### 15.4 Debug and safety

| Setting | Value | Cost | Benefit |
|---|---|---|---|
| `configCHECK_FOR_STACK_OVERFLOW` | **2** | ~20 cycles per context switch | Method 2: writes a 20-byte pattern at stack creation and verifies it on every switch |
| `configUSE_TRACE_FACILITY` | 1 | ~12 B per TCB | Enables `vTaskGetInfo` — required by the heartbeat |
| `configGENERATE_RUN_TIME_STATS` | 1 | 2 × 32-bit reads per switch | Per-task CPU accounting |
| `configUSE_STATS_FORMATTING_FUNCTIONS` | 0 | — | No `vTaskList` string formatting (saves ~1 KB) |

Stack-overflow checking method 2 is the thorough one. Method 1 only checks whether the stack pointer is out of range at switch time; method 2 additionally verifies that the last 20 bytes of the stack still hold their `0xA5` fill pattern. A task that overflows *and returns* before the next context switch would evade method 1 but not method 2.

```c
void vApplicationStackOverflowHook(TaskHandle_t task, char *name){
    (void)task;
    (void)name;
    for (;;) {}
}
```

The hook is an infinite loop. With no debugger attached, the node hangs — and because the IWDG is fed from the idle hook, which never runs from inside this loop, the watchdog will reset within ~2 s. So the practical behaviour is a reset loop, which is at least observable from the gateway (telemetry stops and restarts). Capturing the task name into a `.noinit` RAM location before hanging would make the cause diagnosable across the reset. Recorded in [§82](#82-known-gaps).

### 15.5 `configASSERT`

```c
#define configASSERT(x)                                              \
    do {                                                             \
        if ((x) == 0) {                                              \
            taskDISABLE_INTERRUPTS();                                \
            for (;;) { __asm volatile ("bkpt #0"); }                 \
        }                                                            \
    } while (0)
```

`bkpt #0` halts into the debugger if one is attached. If none is, the behaviour on Cortex-M is to escalate to a HardFault — and since interrupts are disabled first, that HardFault cannot be preempted and the node is hard-hung with **no watchdog recovery** (the IWDG needs the idle hook, which needs interrupts).

This is the one path in the design that produces an unrecoverable hang. It fires on `configASSERT(g_frame_queue != NULL)` and the equivalent checks in `main()`, all of which are static-allocation results that cannot realistically fail. So it is unreachable in practice. Still, disabling interrupts before hanging is what turns a recoverable fault into an unrecoverable one.

### 15.6 Interrupt priority configuration

```c
#define configPRIO_BITS                               4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY       15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5

#define configKERNEL_INTERRUPT_PRIORITY               (15 << 4)   /* 0xF0 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY          ( 5 << 4)   /* 0x50 */
```

The STM32F4 implements 4 priority bits, in the **upper** nibble of the 8-bit priority field — hence the `<< (8 - configPRIO_BITS)`.

```
   Priority value   Meaning
   ──────────────   ───────────────────────────────────────────────
        0  ▲        Highest hardware priority.
        1  │        MUST NOT call any FreeRTOS *FromISR API.
        2  │        These ISRs are never masked by taskENTER_CRITICAL.
        3  │
        4  ▼
   ─────────────────────────────────────────────────────────────────
        5  ▲        configMAX_SYSCALL_INTERRUPT_PRIORITY
        6  │        May call xQueueSendFromISR, xSemaphoreGiveFromISR,
        7  │        vTaskNotifyGiveFromISR, portYIELD_FROM_ISR.
        8  │        Masked by critical sections.
       ... │
       14  ▼
   ─────────────────────────────────────────────────────────────────
       15            Kernel: SysTick and PendSV. Lowest, always.
```

Every application ISR must sit in 5…14 or the kernel's internal assertions (in a debug build) will catch it. [§22](#22-interrupt-priority-map) audits the actual assignments.

### 15.7 `INCLUDE_*` selection

```c
#define INCLUDE_vTaskPrioritySet            0
#define INCLUDE_uxTaskPriorityGet           0
#define INCLUDE_vTaskDelete                 0
#define INCLUDE_vTaskSuspend                0
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      0
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTimerPendFunctionCall      0
#define INCLUDE_eTaskGetState               0
#define INCLUDE_xTaskGetIdleTaskHandle      1
```

The four enabled ones each have exactly one call site:

| Macro | Used by |
|---|---|
| `INCLUDE_vTaskDelayUntil` | Threads 4, 6, 7 — periodic wake without drift |
| `INCLUDE_vTaskDelay` | The Logger's 4-second rate limit |
| `INCLUDE_xTaskGetCurrentTaskHandle` | Thread 5 registering itself with the UART RX ISR |
| `INCLUDE_uxTaskGetStackHighWaterMark` | Thread 4 building the heartbeat |
| `INCLUDE_xTaskGetIdleTaskHandle` | Thread 4 reading the idle task's runtime counter |

`INCLUDE_vTaskSuspend = 0` is notable because it changes the meaning of `portMAX_DELAY`: with suspension disabled, `portMAX_DELAY` is treated as a very long finite timeout rather than as "block forever". In practice `portMAX_DELAY` is `0xFFFFFFFF` ticks ≈ 49.7 days, so the distinction never materialises.

## 16. Static Allocation Policy

With `configSUPPORT_DYNAMIC_ALLOCATION = 0`, FreeRTOS requires the application to supply memory for the idle task:

```c
void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack, uint32_t *size){
    static StaticTask_t idle_tcb;
    static StackType_t  idle_stack[configMINIMAL_STACK_SIZE];
    *tcb   = &idle_tcb;
    *stack = idle_stack;
    *size  = configMINIMAL_STACK_SIZE;
}
```

`vApplicationGetTimerTaskMemory` is **not** required, because `configUSE_TIMERS = 0`.

Every FreeRTOS object in the system, with its backing storage:

| Object | Storage | Bytes |
|---|---|---|
| `s_tim_sem` | `s_tim_sem_storage` (`StaticSemaphore_t`) | ~80 |
| `s_dma_sem` | `s_dma_sem_storage` | ~80 |
| `g_frame_queue` | `s_frame_queue_storage` + `s_frame_queue_buffer[6 × 35]` | ~80 + 210 |
| `g_ultrasonic_queue` | `s_ultrasonic_queue_storage` + `s_ultrasonic_queue_buffer[4]` | ~80 + 16 |
| `s_logger_queue` | `s_logger_queue_storage` + `s_logger_queue_buffer[16 × 10]` | ~80 + 160 |
| Thread 1 | `s_thread1_tcb` + `s_thread1_stack[96]` | ~88 + 384 |
| Thread 2 | `s_thread2_tcb` + `s_thread2_stack[1280]` | ~88 + 5,120 |
| Thread 3 | `s_thread3_tcb` + `s_thread3_stack[256]`¹ | ~88 + 1,024 |
| Thread 4 | `s_thread4_tcb` + `s_thread4_stack[256]` | ~88 + 1,024 |
| Thread 5 | `s_thread5_tcb` + `s_thread5_stack[256]` | ~88 + 1,024 |
| Thread 6 | `s_thread6_tcb` + `s_thread6_stack[192]` | ~88 + 768 |
| Thread 7 | `s_thread7_tcb` + `s_thread7_stack[128]` | ~88 + 512 |
| Logger | `s_logger_task_tcb` + `s_logger_task_stack[256]` | ~88 + 1,024 |
| Idle | `idle_tcb` + `idle_stack[128]` | ~88 + 512 |

¹ **A discrepancy.** `s_thread3_stack` is declared as `StackType_t s_thread3_stack[256]` — 256 words, 1,024 bytes — but the task is created with `THREAD3_STACK_WORDS`, which is `#define`d as `128U`:

```c
#define THREAD3_STACK_WORDS   128U
static StackType_t  s_thread3_stack[256];       /* ← 256, not THREAD3_STACK_WORDS */
...
s_thread3_handle = xTaskCreateStatic(
    Thread3_UartTx, "TX",
    THREAD3_STACK_WORDS, NULL, 1,               /* ← creates with 128 */
    s_thread3_stack, &s_thread3_tcb);
```

The task gets a 128-word stack; the other 128 words of the array are allocated but unused and unmonitored. Not a bug — the task's stack is fully inside its array, so there is no overflow risk — but 512 bytes are wasted and the two numbers should agree. The same pattern appears for Thread 4 (`s_thread4_stack[256]` with `THREAD4_STACK_WORDS = 256`, which does match).

### 16.1 What static allocation buys and costs

**Buys:**
- Link-time certainty. If it links, the RAM fits.
- No fragmentation, no allocation failure path, no `pvPortMalloc` in any interrupt-adjacent code.
- Every object's address is fixed, which makes debugger watch expressions stable across builds.

**Costs:**
- Every size is a compile-time guess that must be validated empirically ([§21](#21-stack-sizing)).
- Unused capacity is permanently unavailable — the UART service's 1 KB for two uninitialised instances ([§11](#11-sram-budget)) is the clearest example.

## 17. Task Inventory

| # | Name | Prio | Stack (words / bytes) | Wake source | Period | IWDG? |
|---|---|---|---|---|---|---|
| 5 | `BL_RX` | **4** | 256 / 1,024 | UART1 RX ISR notification | async | No |
| 1 | `Sensor` | 3 | 96 / 384 | `s_tim_sem` from TIM2 ISR | 10 ms | **Yes** |
| 2 | `ML` | 2 | 1,280 / 5,120 | Task notification from Thread 1 | on demand | **Yes** |
| 3 | `TX` | 1 | 128 / 512 | `g_frame_queue` | on demand | **Yes** |
| 4 | `HB` | 1 | 256 / 1,024 | `vTaskDelayUntil` | 1,000 ms | **Yes** |
| 6 | `TEMP` | 1 | 192 / 768 | `vTaskDelayUntil` | 2,000 ms | No |
| 7 | `ULTRA` | 1 | 128 / 512 | `vTaskDelayUntil` | 250 ms | No |
| — | `Logger` | 0 | 256 / 1,024 | `s_logger_queue` | ≤ 0.25 Hz | No |
| — | `IDLE` | 0 | 128 / 512 | — | — | feeds |

### 17.1 The full interaction map

```
   ┌─────────────────────────────── HARDWARE ──────────────────────────────┐
   │  TIM2 IRQ    DMA1_S0 IRQ    TIM3 IRQ      USART1 IRQ    DMA2_S7 IRQ  │
   │  (100 Hz)    (I2C RX done)  (echo edge)   (RX byte)     (TX done)    │
   └─────┬────────────┬──────────────┬──────────────┬──────────────┬───────┘
         │            │              │              │              │
    s_tim_sem    s_dma_sem   g_ultrasonic_queue  notify         notify
         │            │              │              │              │
         ▼            ▼              ▼              ▼              ▼
   ┌──────────────────────┐   ┌────────────┐  ┌──────────┐  ┌──────────────┐
   │  Thread 1  "Sensor"  │   │ Thread 7   │  │ Thread 5 │  │  Thread 3    │
   │  prio 3, 10 ms       │   │ "ULTRA"    │  │ "BL_RX"  │  │  "TX"        │
   │                      │   │ prio 1     │  │ prio 4   │  │  prio 1      │
   │  MPU6050_TriggerRead │   │ 250 ms     │  │          │  │              │
   │  RingBuffer_Push     │   │            │  │ 0xAA/0xEB│  │ Frame_Build  │
   │  notify Thread 2     │   │ trigger ×2 │  │ → ACK    │  │ + CRC32      │
   │  IWDG alive          │   │ wait 50 ms │  │ → magic  │  │ PA8 high     │
   └──────────┬───────────┘   │ IWDG: no   │  │ → reset  │  │ TransmitDMA  │
              │               └─────┬──────┘  └──────────┘  │ wait notify  │
        notify│                     │                        │ PA8 low     │
              ▼                     │                        │ IWDG alive  │
   ┌──────────────────────┐         │                        └──────▲──────┘
   │  Thread 2  "ML"      │         │                               │
   │  prio 2              │         │                               │
   │                      │         │                               │
   │  PeekWindow(50)      │         │                               │
   │  Scale → Features    │         │                               │
   │  → Quantize          │         │                               │
   │  → Inference         │         │                               │
   │  → Vote              │         │                               │
   │  Advance(25)         │         │                               │
   │  IWDG alive          │         │                               │
   └──────────┬───────────┘         │                               │
              │                     │                               │
              └──────────┬──────────┴───────────┬───────────────────┘
                         │                      │
                         ▼                      │
              ┌─────────────────────┐           │
              │   g_frame_queue     │───────────┘
              │   depth 6 × 35 B    │
              └──────────▲──────────┘
                         │
              ┌──────────┴──────────┬──────────────────┐
              │                     │                  │
     ┌────────────────┐   ┌──────────────────┐  ┌──────────────┐
     │  Thread 4 "HB" │   │ Thread 6 "TEMP"  │  │ Logger       │
     │  prio 1, 1 s   │   │ prio 1, 2 s      │  │ prio 0       │
     │  frame every 5s│   │ ADC_Read (poll)  │  │ ≤1 per 4 s   │
     │  IWDG alive    │   │ IWDG: no         │  │ IWDG: no     │
     └────────────────┘   └──────────────────┘  └──────▲───────┘
                                                        │
                                        s_logger_queue (16 × 10 B)
                                                        │
                                    ┌───────────────────┴──────────────┐
                                    │  LOG_* from any task or ISR      │
                                    └──────────────────────────────────┘

   ┌──────────────────────────────────────────────────────────────────────┐
   │  IDLE (prio 0)  →  vApplicationIdleHook()  →  IWDG_SupervisorFeed()  │
   │      feeds the watchdog only if T1 && T2 && T3 && T4 all alive       │
   └──────────────────────────────────────────────────────────────────────┘
```

## 18. Priority Design

```
   Priority 4  ┌────────────────────────────────────────────────────┐
   (highest)   │  Thread 5 — Bootloader RX                          │
               │  Must preempt everything to catch the OTA trigger  │
               └────────────────────────────────────────────────────┘
   Priority 3  ┌────────────────────────────────────────────────────┐
               │  Thread 1 — Sensor                                 │
               │  Hard 10 ms deadline; a missed tick is a lost      │
               │  sample and a hole in the inference window         │
               └────────────────────────────────────────────────────┘
   Priority 2  ┌────────────────────────────────────────────────────┐
               │  Thread 2 — TinyML                                 │
               │  Must drain the ring buffer faster than Thread 1   │
               │  fills it, or samples are dropped                  │
               └────────────────────────────────────────────────────┘
   Priority 1  ┌────────────────────────────────────────────────────┐
               │  Thread 3 (TX) · Thread 4 (HB)                     │
               │  Thread 6 (TEMP) · Thread 7 (ULTRA)                │
               │  Soft deadlines; all block on every iteration      │
               └────────────────────────────────────────────────────┘
   Priority 0  ┌────────────────────────────────────────────────────┐
   (lowest)    │  Logger · Idle                                     │
               │  Logging must never delay real-time work.          │
               │  The idle hook feeds the watchdog.                 │
               └────────────────────────────────────────────────────┘
```

### 18.1 Why Thread 5 outranks the sensor

The OTA trigger is a two-byte sequence with no retransmission. If Thread 5 were below Thread 1, the sequence would still be received — the UART ISR pushes into a 256-byte ring buffer regardless of task scheduling — so the bytes cannot be lost. What priority 4 actually buys is **latency**: the gateway waits 1,000 ms for the ACK, and at priority 4 the round trip is microseconds rather than milliseconds.

More importantly, priority 4 means Thread 5 can reset the MCU *promptly* even if the rest of the system is misbehaving. If Thread 2 were stuck in a long inference and the operator needed to force the node into the bootloader to recover it, a lower-priority Thread 5 would have to wait. At priority 4 it does not.

The cost is nil: Thread 5 blocks on `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` and consumes no CPU until a byte arrives.

### 18.2 Why the sensor outranks the ML

Thread 1's deadline is hard. TIM2 gives `s_tim_sem` every 10 ms; if Thread 1 has not consumed the previous give, the binary semaphore saturates and the tick is silently lost. Since the semaphore is binary (not counting — `configUSE_COUNTING_SEMAPHORES = 0`), there is no queue of pending ticks. A single 10 ms delay costs exactly one sample.

Thread 2's deadline is soft: it has a whole ring buffer (128 samples = 1.28 s at 100 Hz) of slack before anything is lost. Preempting it is free.

### 18.3 The producer/consumer rate argument

```
   Producer:  Thread 1, 100 samples/s
   Consumer:  Thread 2, drains WINDOW_STRIDE = 25 samples per inference

   For stability:   inference_rate × 25 ≥ 100 samples/s
                    inference_rate ≥ 4 Hz
                    inference_period ≤ 250 ms

   Measured inference latency (g_stats.inf_wcet_us): the CNN forward pass alone.
   Full pipeline per window: scale + features + quantise + inference.
   Feature extraction (6 × 64-point FFT + 50 statistics) dominates.
```

Thread 2's `while (RingBuffer_Count() >= WINDOW_SIZE)` loop is what makes this self-correcting. If Thread 2 falls behind — say a burst of preemption from Thread 1 — the ring buffer accumulates, and on the next wake Thread 2 processes *multiple* windows back-to-back until the count drops below 50. It catches up rather than falling permanently behind.

The failure mode is only reached if the *steady-state* processing rate drops below 4 Hz, at which point `rb_max_fill` in the heartbeat climbs toward 128 and `LOG_CODE_RING_BUFFER_DROP` starts firing. Both are visible in telemetry, which is why they are in the heartbeat.

### 18.4 The priority-1 cluster

Four tasks, no time slicing, all blocking. Analysed in [§15.1](#151-scheduler). The ordering among them when several become ready simultaneously is FIFO by readiness, which is unspecified but irrelevant — none has a deadline tighter than 250 ms and all four together consume a fraction of a percent of CPU.

### 18.5 Logger at priority 0

Deliberately the same priority as idle. `configIDLE_SHOULD_YIELD = 1` means idle yields to it, so the Logger does get scheduled — but only when nothing else is ready. Combined with the 4-second rate limit ([§31](#31-logger-task)), the Logger's total CPU consumption is unmeasurable.

The trade: a log entry may sit in `s_logger_queue` for an unbounded time if the system is fully loaded. Since the queue holds 16 entries and the debounce logic ([§75](#75-debounce-and-deduplication)) prevents flooding, that is acceptable.

## 19. Synchronisation Map

| Primitive | Instance | Producer | Consumer | Purpose |
|---|---|---|---|---|
| Binary semaphore | `s_tim_sem` | TIM2 ISR | Thread 1 | 100 Hz tick |
| Binary semaphore | `s_dma_sem` | I2C DMA TC ISR → `on_mpu_read_done` | Thread 1 | IMU read complete |
| Task notification | Thread 2's | Thread 1 (`xTaskNotifyGive`) | Thread 2 (`ulTaskNotifyTake`) | New sample available |
| Task notification | Thread 3's | DMA2_S7 TC ISR → `on_uart_tx_done` | Thread 3 | UART DMA complete |
| Task notification | Thread 5's | USART1 RXNE ISR → `uart_svc_callback` | Thread 5 | Byte received |
| Queue (6 × 35 B) | `g_frame_queue` | Threads 2, 4, 6, 7, Logger | Thread 3 | Frames to transmit |
| Queue (4 × 4 B) | `g_ultrasonic_queue` | TIM3 IC ISR → `on_hcsr04_capture` | Thread 7 | Measured distances |
| Queue (16 × 10 B) | `s_logger_queue` | Any task or ISR | Logger | Log entries |
| Lock-free SPSC | `ring_buffer_instance` | Thread 1 | Thread 2 | IMU samples |
| Critical section | `taskENTER_CRITICAL` | Thread 1 | — | Atomic snapshot of `s_latest_sample` |

### 19.1 The one critical section

```c
if (xSemaphoreTake(s_dma_sem, pdMS_TO_TICKS(DMA_TIMEOUT_MS)) == pdTRUE){
    taskENTER_CRITICAL();
    snapshot = (MPU6050_RawData_t)s_latest_sample;
    taskEXIT_CRITICAL();
    ...
}
```

`s_latest_sample` is a 14-byte `volatile MPU6050_RawData_t` written by the DMA completion callback (`on_mpu_read_done`) and read here. A 14-byte struct copy is not atomic — it compiles to several `LDRH`/`STRH` pairs — so without the critical section a new DMA completion landing mid-copy would produce a torn sample: three axes from reading *n* and three from reading *n+1*.

`taskENTER_CRITICAL()` on this port raises `BASEPRI` to `configMAX_SYSCALL_INTERRUPT_PRIORITY` (5), which masks every ISR at priority 5 or above — including the DMA1_S0 ISR at priority 5. So the protection is real.

**But there is a subtle point.** The DMA callback has already given `s_dma_sem` before Thread 1 wakes. The next write to `s_latest_sample` can only come from the *next* `MPU6050_TriggerRead`, which Thread 1 itself initiates on the next tick. So the race window is: Thread 1 takes the semaphore, gets preempted for a full 10 ms tick before the copy, and — no, it cannot, because Thread 1 is the only thing that triggers a read. The critical section guards against a race that the design already precludes.

It costs perhaps 20 cycles and removes the need to reason about the above. Keeping it is correct; the reasoning above is why removing it would also be correct, and why nobody should rely on that.

This code path is in the `#else` branch of `REPLAY_MODE`, i.e. it is the one compiled in the committed build (`REPLAY_MODE 0`).

### 19.2 Binary semaphores saturate

Both `s_tim_sem` and `s_dma_sem` are binary. `xSemaphoreGiveFromISR` on an already-given binary semaphore returns `pdFALSE` and the give is lost.

For `s_tim_sem` that is the intended overrun-detection behaviour: if Thread 1 misses a tick, the next give is discarded rather than queued, so the pipeline stays synchronous with real time instead of trying to catch up on a backlog of stale ticks.

There is no counter for how often this happens. A counting semaphore, or a simple `s_missed_ticks++` in the ISR when the give fails, would make sample loss observable. Currently the only evidence is a gap in the timestamps at the gateway. Recorded in [§82](#82-known-gaps).

### 19.3 `g_frame_queue` is the system's single choke point

Five producers, one consumer, depth 6.

```
   Steady-state offered load:
     Thread 7 (ultrasonic)     4.0 frames/s
     Thread 2 (classification) ≤ 4.0 frames/s   (gated by the 9-vote window)
     Thread 6 (temperature)    0.5 frames/s
     Thread 4 (heartbeat)      0.2 frames/s
     Logger                    ≤ 0.25 frames/s
                               ──────────────
                               ≈ 9 frames/s peak

   Service rate:
     Largest frame = heartbeat = 30 B payload + 7 B overhead = 37 B
     37 B at 115200 8N1 = 37 × 86.8 µs = 3.2 ms
     → Thread 3 can serve ~310 frames/s
```

A 34× margin. The queue depth of 6 is there to absorb bursts (all five producers becoming ready in the same tick), not to buffer a rate mismatch.

Every producer uses a **zero timeout**:

```c
if (xQueueSend(g_frame_queue, &req, 0) != pdTRUE) { ... }
```

so a full queue drops the frame rather than blocking the producer. That is the right choice for Thread 2 — blocking the ML task to wait for UART would be far worse than losing one classification — and every drop is logged:

| Producer | On failure |
|---|---|
| Thread 2 | `g_t2_queue_drops++` and `LOG_ERROR(LOG_CODE_QUEUE_FULL, g_t2_queue_drops)` |
| Thread 4 | `LOG_WARN(LOG_CODE_QUEUE_FULL, 1U)` |
| Thread 6 | `LOG_WARN(LOG_CODE_QUEUE_FULL, 2U)` |
| Thread 7 | `LOG_WARN(LOG_CODE_QUEUE_FULL, 3U)` |
| Logger | `s_forward_drop_count++` (silent — cannot log a log failure) |

The `aux_data` field distinguishes which producer dropped, which is exactly the kind of detail that makes a log useful rather than merely present.

## 20. Runtime Statistics via DWT

FreeRTOS's runtime statistics need a counter that is at least 10× faster than the tick. The Cortex-M4's DWT cycle counter is the cheapest possible source: a free-running 32-bit counter at CPU clock, read with a single 32-bit load.

```c
void vConfigureTimerForRunTimeStats(void){
    DWT_Init();
}

uint32_t ulGetRunTimeCounterValue(void){
    return DWT_GetCycles();
}
```

wired in by:

```c
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()  vConfigureTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()          ulGetRunTimeCounterValue()
```

FreeRTOS calls `portGET_RUN_TIME_COUNTER_VALUE()` on every context switch and accumulates the delta into the outgoing task's `ulRunTimeCounter`.

| Property | Value |
|---|---|
| Resolution | 1 CPU cycle ≈ 11.9 ns at 84 MHz |
| Read cost | ~3 cycles (one `LDR`) |
| Wrap period | 2³² / 84 MHz ≈ **51.1 seconds** |
| Overhead per switch | 2 reads + 1 subtract + 1 add ≈ 10 cycles |

### 20.1 The 51-second wrap is handled by differencing

Thread 4 never uses absolute counter values. It samples each task's counter every second and works with the delta:

```c
uint32_t dt_t1    = t1_now    - prev_t1_cycles;
uint32_t dt_total = total_now - prev_total;

g_stats.cpu_t1_x100 = (uint16_t)(((uint64_t)dt_t1 * 10000ULL) / dt_total);
```

Unsigned subtraction is correct across a wrap as long as the true elapsed interval is under 2³² cycles. At a 1-second sampling period, the interval is 84 M cycles — 51× under the limit. Safe.

The `uint64_t` cast on the multiplication is necessary: `dt_t1 × 10000` for a task consuming 84 M cycles/s would be 8.4 × 10¹¹, far past `uint32_t`.

### 20.2 The x100 fixed-point convention

CPU percentages are transmitted as `percent × 100`:

```
   1234  →  12.34 %
   9987  →  99.87 %
   0     →   0.00 %
```

Two decimal places without floating point on the wire. Values are clamped:

```c
if (g_stats.cpu_t1_x100 > 10000U) g_stats.cpu_t1_x100 = 10000U;
```

The clamp exists because `ulRunTimeCounter` accumulation and `ulGetRunTimeCounterValue()` are not sampled atomically — a context switch between reading a task's counter and reading the total can produce a ratio slightly above 1.0. Clamping turns a nonsensical 101.3 % into a plausible 100.00 %.

### 20.3 What is *not* measured

`vTaskGetInfo` is called for Threads 1, 2, 3, 6, 7 and the idle task. **Threads 4 and 5 and the Logger are not measured.** So the six reported figures do not sum to 100 %:

```
   cpu_t1 + cpu_t2 + cpu_t3 + cpu_t6 + cpu_t7 + cpu_idle
     + (Thread 4, unmeasured)
     + (Thread 5, unmeasured — effectively 0, it never runs)
     + (Logger, unmeasured — effectively 0)
     + (ISR time, never attributed to any task)
     = 100 %
```

The gap is dominated by **interrupt time**, which FreeRTOS's runtime stats fundamentally cannot attribute: cycles spent in an ISR are charged to whichever task happened to be running. At 100 Hz TIM2 + 100 Hz DMA + 8 Hz TIM3 IC + one DMA TX per frame, ISR time is small but non-zero.

Thread 4 not measuring itself is a genuine omission — it holds its own handle implicitly (it is the running task) and could call `vTaskGetInfo(xTaskGetCurrentTaskHandle(), …)`.

## 21. Stack Sizing

FreeRTOS fills each stack with `0xA5` at creation. `uxTaskGetStackHighWaterMark()` scans upward from the stack base and returns the number of **words** that still hold the fill pattern — the minimum free headroom ever observed.

Thread 4 samples five of them every second and reports them in the heartbeat:

```c
g_stats.stack_t1_free = (uint16_t)uxTaskGetStackHighWaterMark(s_thread1_handle);
g_stats.stack_t2_free = (uint16_t)uxTaskGetStackHighWaterMark(s_thread2_handle);
g_stats.stack_t3_free = (uint16_t)uxTaskGetStackHighWaterMark(s_thread3_handle);
g_stats.stack_t6_free = (uint16_t)uxTaskGetStackHighWaterMark(s_thread6_handle);
g_stats.stack_t7_free = (uint16_t)uxTaskGetStackHighWaterMark(s_thread7_handle);
```

Threads 4 and 5 and the Logger are again unmonitored.

### 21.1 Static analysis of Thread 2's stack

Thread 2 has the largest allocation, and it is worth working through why.

```c
static void Thread2_TinyML(void *arg){
    MPU6050_RawData_t window[WINDOW_SIZE];               /* 50 × 14 =  700 B  ON STACK */

    static int16_t   flat_window[WINDOW_SIZE][N_FEATURES]; /* 600 B  → .bss */
    static float32_t scaled     [WINDOW_SIZE][N_FEATURES]; /* 1200 B → .bss */
    static float32_t features   [N_STAT_FEATURES];         /* 200 B  → .bss */
    static int8_t    ts_q       [WINDOW_SIZE * N_FEATURES];/* 300 B  → .bss */
    static int8_t    stat_q     [N_STAT_FEATURES];         /* 50 B   → .bss */

    Vote_t vote;                                          /* 11 B ON STACK */
    ...
}
```

The `static` keyword on five of the six buffers is load-bearing. Without it, `scaled` alone (1,200 B) plus `flat_window` (600 B) would add 1,800 bytes to the frame. The comment in the earlier README gets this right.

But the deepest point is not in `Thread2_TinyML` — it is inside `Features_Extract`:

```c
void Features_Extract(const float32_t scaled[WINDOW_SIZE][N_FEATURES],
                            float32_t features_out[N_STAT_FEATURES]){
    float32_t col[N_FEATURES][WINDOW_SIZE];   /* 6 × 50 × 4 = 1,200 B  ON STACK */
    float32_t amag[WINDOW_SIZE];              /*      50 × 4 =   200 B  ON STACK */
    float32_t gmag[WINDOW_SIZE];              /*      50 × 4 =   200 B  ON STACK */
    ...
}
```

and one level deeper, inside `feat_iqr`:

```c
static float32_t feat_iqr(const float32_t *x, uint32_t n){
    float32_t buf[WINDOW_SIZE];               /*      50 × 4 =   200 B  ON STACK */
    ...
}
```

So the worst-case chain:

```
   Thread2_TinyML                        700 B (window) + 11 B (vote) + locals
     └─► Features_Extract              1,600 B (col + amag + gmag) + locals
           └─► feat_iqr                  200 B (buf) + locals
                 └─► feat_insertion_sort   ~32 B
                 └─► feat_percentile_sorted ~32 B
   ─────────────────────────────────────────────
   Data subtotal                       ≈ 2,575 B
   + AAPCS frame overhead, spilled registers,
     FP register saves across calls     ≈   400 B
   ─────────────────────────────────────────────
   Estimated peak                      ≈ 2,975 B
```

Against 5,120 bytes allocated, that is a **42 % headroom**. The allocation is generous but not wasteful — and the heartbeat's `stack_t2_free` field provides the empirical check. A healthy reading would be around 530 words (2,120 B) free.

`arm_rfft_fast_f32` is called from `feat_hfe_channel`, whose FFT buffers are `static` (644 B in `.bss`), so the FFT itself adds only CMSIS-DSP's own frame — a few hundred bytes at most, and it is called from `Features_Extract`, not from inside `feat_iqr`, so the two deep paths do not compose.

### 21.2 The other tasks

| Task | Allocated | Deepest data on stack | Assessment |
|---|---|---|---|
| Thread 1 | 384 B | `MPU6050_RawData_t snapshot` (14 B) + `sample` (14 B) | Very tight but adequate — no deep calls |
| Thread 3 | 512 B | `FrameRequest_t req` (35 B) + `Buffer_t buf` (10 B) + `Frame_Build` frame | Comfortable |
| Thread 4 | 1,024 B | `FrameRequest_t req` (35 B) + `TaskStatus_t info` (~40 B) | Comfortable |
| Thread 5 | 1,024 B | `Buffer_t` (10 B) + `rx_byte` | Massively over-allocated |
| Thread 6 | 768 B | `FrameRequest_t req` (35 B) | Over-allocated |
| Thread 7 | 512 B | `FrameRequest_t req` (35 B) + two `HCSR04_Config_t` | Comfortable |
| Logger | 1,024 B | `Log_Payload_t` (10 B) + `FrameRequest_t` (35 B) | Over-allocated |

Thread 5 at 1,024 bytes is the clearest over-allocation — its entire frame is under 50 bytes. The earlier README explains the size as "FLASH unlock locals", which was true of an earlier design in which Thread 5 erased a sector before resetting. The current implementation does no such thing ([§28](#28-thread-5--bootloader-receive)), so the justification is stale and roughly 900 bytes could be reclaimed.

Thread 1 at 384 bytes is the one worth watching. In live mode it calls `MPU6050_TriggerRead` → `I2C_SVC_ReadBurst_DMA` → `I2C_MasterTransmit_IT`, which is three frames deep. The heartbeat's `stack_t1_free` is the check; a reading below ~20 words would warrant an increase.

## 22. Interrupt Priority Map

Every ISR that calls a FreeRTOS `*FromISR` API must have a numeric priority of **5 or higher** (`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`). Auditing the actual assignments:

| IRQ | Priority | Set at | Calls FromISR API? | Compliant? |
|---|---|---|---|---|
| `TIM2` | 5 | `main.c`: `NVIC_SetPriority(TIM2, 5U)` | `xSemaphoreGiveFromISR` | ✓ (5 ≥ 5) |
| `TIM3` | 5 | `main.c`: `NVIC_SetPriority(TIM3, 5U)` | `xQueueSendFromISR` | ✓ |
| `USART1` | 6 | `UART_SVC_Init(UART1_ID, 6U, …)` | `vTaskNotifyGiveFromISR` | ✓ |
| `DMA2_Stream7` | 7 | `UART_SVC_Init` → `priority + 1U` | `vTaskNotifyGiveFromISR` (via `on_uart_tx_done`) | ✓ |
| `I2C1_EV` | 6 | `I2C_SVC_Init(…, 6U, 6U, …)` | none directly | ✓ |
| `I2C1_ER` | 6 | same | none directly | ✓ |
| `DMA1_Stream0` | **5** | `I2C_SVC_Init`: `computed_dma_prio = i2c_ev_priority - 1` | `xSemaphoreGiveFromISR` (via `on_mpu_read_done`) | ✓ (exactly at the boundary) |
| `SysTick` | 15 | `configKERNEL_INTERRUPT_PRIORITY` | kernel | ✓ |
| `PendSV` | 15 | `configKERNEL_INTERRUPT_PRIORITY` | kernel | ✓ |

```
   0 ─┐
   1  │  UNUSED — would be unable to call FreeRTOS APIs
   2  │
   3  │
   4 ─┘
   ────────────────────────  configMAX_SYSCALL_INTERRUPT_PRIORITY = 5
   5 ─┐  TIM2 (100 Hz IMU tick)
      │  TIM3 (HC-SR04 input capture)
      │  DMA1_Stream0 (I2C RX complete)
   6  │  USART1 (RX byte)
      │  I2C1_EV, I2C1_ER
   7  │  DMA2_Stream7 (UART TX complete)
   8  │
  ... │
  14 ─┘
   ────────────────────────
  15    SysTick, PendSV  (kernel — always lowest)
```

### 22.1 DMA1_Stream0 sits exactly on the boundary

```c
/* I2C_SERVICE.c */
uint32_t computed_dma_prio = (i2c_ev_priority < 15U) ? (i2c_ev_priority + 1U) : 15U;
(void)dma_priority;   /* intentionally unused — computed from ev priority */
NVIC_SetPriority(i2c_svc_dma_irqn(dma_id, dma_stream), computed_dma_prio);
```

Reading the code: the DMA IRQ is set to `i2c_ev_priority + 1` = **7**, not 5. The comment says the passed `dma_priority` parameter is deliberately ignored in favour of a rule tying the DMA priority to the I2C event priority.

So `main()` passes `6U` for the DMA priority and it is discarded; the actual value is 7. Numerically lower priority than the I2C event IRQ, which is correct ordering — the DMA completion should not preempt the I2C state machine mid-transfer.

All compliant. Worth noting because the parameter's presence in the API signature invites the belief that it does something.

### 22.2 The 100 Hz + 100 Hz coincidence

TIM2 (100 Hz) and DMA1_Stream0 (also 100 Hz, one per IMU read) both fire at priority 5. When they coincide, neither can preempt the other — the second waits for the first to return. Each ISR is short (`xSemaphoreGiveFromISR` plus `portYIELD_FROM_ISR`, perhaps 60 cycles), so the added latency is under a microsecond against a 10 ms period.

### 22.3 The `IRQn_t` enum encoding

`NVIC_INTERFACE.h` uses an unusual packed encoding rather than plain IRQ numbers:

```c
typedef enum {
    WWDG_IRQ            = 0x00,
    ...
    TIM2                = 0xE01C,
    TIM3                = 0xE81D,
    USART1              = 0x2925,
    DMA1_Stream0_IRQ    = 0x580B,
    DMA2_Stream7        = 0x3246,
    ...
} IRQn_t;
```

The low byte is the IRQ number (`TIM2` → `0x1C` = 28 ✓, `USART1` → `0x25` = 37 ✓) and the high byte encodes the register index and bit position that `NVIC_EnableIRQ` needs. This lets the NVIC driver avoid a division:

```
   register index = IRQn / 32
   bit position   = IRQn % 32
```

both of which are precomputed into the enum value. A clever micro-optimisation for code that runs a handful of times at boot.

The naming is inconsistent — some entries carry an `_IRQ` suffix (`DMA1_Stream0_IRQ`) and some do not (`DMA2_Stream7`, `TIM2`). `UART_SERVICE.c`'s `uart_svc_dma_irqn()` has to know which is which:

```c
case DMA_STREAM_6: return DMA1_Stream6_IRQ;
case DMA_STREAM_7: return DMA1_Stream7;      /* ← no suffix */
```

A cosmetic wart that has caused at least one confusing lookup.

Note also that several ARM core exception numbers collide in this encoding: `I2C1_ER = 0x0120` has low byte `0x20` = 32, and `EXTI15_10 = 0x4128` has low byte `0x28` = 40. Both correct. But `OTG_FS = 0x1A43` gives low byte `0x43` = 67, which the comment annotates as "451" — the comment is tracking the raw enum value, not the IRQ number. Confusing but harmless.

---

# Part IV — Tasks

## 23. Boot Sequence

`main()` runs on the MSP with interrupts enabled but no NVIC line unmasked, from the bootloader's jump ([`../Bootloader/BOOTLOADER.md`](../Bootloader/BOOTLOADER.md) §15) to `vTaskStartScheduler()`.

```
 ┌─────────────────────────────────────────────────────────────────────┐
 │  1. SCB->VTOR = 0x08008000                                          │  ◄── §10.1
 │     MUST be first. Until this runs, every exception vectors into    │
 │     the bootloader's table.                                        │
 ├─────────────────────────────────────────────────────────────────────┤
 │  2. RCC_INIT_84MHz_HSI()                                            │  ◄── §7
 │     HSI → PLL(8,168,4) → 84 MHz. AHB /1, APB1 /2, APB2 /1.         │
 ├─────────────────────────────────────────────────────────────────────┤
 │  3. RCC_EN_CLK_PERIPHERAL × 8                                       │
 │     GPIOB, GPIOC, I2C1, DMA1, GPIOA, USART1, DMA2, ADC1            │
 │     RCC_LSI_Enable()                     ← for the IWDG            │
 ├─────────────────────────────────────────────────────────────────────┤
 │  4. GPIO configuration                                              │  ◄── §8
 │     config_gpio_i2c1()   PB6/PB7  AF4, open-drain, pull-up         │
 │     config_gpio_pa8_sync() PA8    output, driven LOW               │
 │     config_gpio_uart1()  PA9/PA10 AF7                              │
 │     PA1 analog (ADC)                                                │
 ├─────────────────────────────────────────────────────────────────────┤
 │  5. ADC_Init(ch 1, 12-bit, 480 cycles)                              │
 ├─────────────────────────────────────────────────────────────────────┤
 │  6. app_i2c_init()   I2C_Init(fast, DMA) + I2C_SVC_Init             │
 │     app_uart_init()  UART_Init(115200, DMA TX, IRQ RX) + SVC        │
 │     config_led_pc13()                                               │
 ├─────────────────────────────────────────────────────────────────────┤
 │  7. MPU6050_Init(I2C1, 0x68, timeout 5,000,000)                     │  ◄── §69
 │     Blocking. ~250 ms of spin delays plus a DMA WHO_AM_I read.     │
 │     MUST follow I2C_SVC_Init or the DMA read hangs.                │
 │     RingBuffer_Init()                                               │
 ├─────────────────────────────────────────────────────────────────────┤
 │  8. TIM2 100 Hz configured, NVIC priority 5, enabled — NOT started  │
 ├─────────────────────────────────────────────────────────────────────┤
 │  9. FreeRTOS objects                                                │
 │     s_tim_sem, s_dma_sem  (binary, static)                          │
 │     g_frame_queue         (6 × 35 B, static)                        │
 │     configASSERT(g_frame_queue != NULL)                             │
 ├─────────────────────────────────────────────────────────────────────┤
 │ 10. Logger_Init()      ← MUST follow g_frame_queue creation        │  ◄── §31
 │     LOG_INFO(LOG_CODE_BOOT, 0)                                      │
 ├─────────────────────────────────────────────────────────────────────┤
 │ 11. Task creation, in this order:                                   │
 │       Thread 2 "ML"     prio 2                                      │
 │       Thread 3 "TX"     prio 1                                      │
 │       UART_SVC_RegisterTxDoneCb(UART1, on_uart_tx_done)             │
 │       Thread 4 "HB"     prio 1                                      │
 │       Thread 5 "BL_RX"  prio 4                                      │
 │       Thread 6 "TEMP"   prio 1                                      │
 ├─────────────────────────────────────────────────────────────────────┤
 │ 12. Ultrasonic subsystem                                            │
 │       g_ultrasonic_queue (4 × 4 B)                                  │
 │       HCSR04_Init × 2   (PA4, PA5 trigger)                          │
 │       PA6/PA7 → AF2, pull-down                                      │
 │       TIM3 PSC 83, ARR 65535 → 1 µs tick                            │
 │       TIM_IC_Init CH1 (ctx 0), CH2 (ctx 1), rising edge             │
 │       NVIC TIM3 priority 5, enabled                                 │
 │       TIM_Start(TIM3)          ← TIM3 runs from here                │
 │       Thread 7 "ULTRA"  prio 1                                      │
 ├─────────────────────────────────────────────────────────────────────┤
 │ 13. Thread 1 "Sensor"   prio 3     ← created LAST                   │
 ├─────────────────────────────────────────────────────────────────────┤
 │ 14. IWDG_Init(3000)                                                 │  ◄── §68
 │     IWDG_Thread{1,2,3,4}_Alive = 1     ← pre-seeded                 │
 │     IWDG_Start()                        ← irreversible              │
 ├─────────────────────────────────────────────────────────────────────┤
 │ 15. TIM_Start(TIM2)     ← the 100 Hz tick begins                    │
 ├─────────────────────────────────────────────────────────────────────┤
 │ 16. vTaskStartScheduler()   — never returns                         │
 └─────────────────────────────────────────────────────────────────────┘
```

### 23.1 The ordering constraints that actually matter

Most of the sequence is arbitrary. Six orderings are not:

| Must happen before | Because |
|---|---|
| `SCB->VTOR` before anything that could interrupt | Otherwise exceptions vector into the bootloader |
| `RCC_INIT_84MHz_HSI` before all peripheral init | Baud rates, timer prescalers and ADC prescaler all assume 84 MHz |
| `RCC_EN_CLK_PERIPHERAL` before touching that peripheral's registers | Register writes to a clock-gated peripheral are discarded silently |
| `I2C_SVC_Init` before `MPU6050_Init` | `MPU6050_Init` does a DMA WHO_AM_I read; without the service layer's DMA and NVIC setup it spins until timeout |
| `g_frame_queue` creation before `Logger_Init` | The Logger task's body dereferences `g_frame_queue` |
| `IWDG_Thread*_Alive = 1` before `IWDG_Start` | Otherwise the first idle-hook feed sees clear flags and withholds |

The last one is subtle and worth expanding. `IWDG_SupervisorFeed()` feeds only when all four flags are set. At the moment `vTaskStartScheduler()` runs, no task has executed yet, so none has called `IWDG_Thread_SetAlive`. If the idle task ran before Threads 1–4 got their first chance, the feed would be withheld — and with a 3-second nominal timeout there is plenty of margin, but pre-seeding the flags removes the question entirely.

### 23.2 `TIM_Start(TIM2)` is deliberately last

TIM2's ISR gives `s_tim_sem`. That semaphore is created in step 9, so starting the timer earlier would be *safe* — but starting it after every task exists means the first tick finds a fully-constructed system. It also means the 100 Hz cadence starts at a well-defined moment rather than during a 250 ms MPU6050 initialisation.

TIM3, by contrast, **is** started before the scheduler (step 12). That is fine: TIM3's ISR only fires on an echo edge, and no trigger pulse has been sent yet, so no capture can occur.

### 23.3 The boot takes about 300 ms

Dominated by `MPU6050_Init`:

```c
MPU6050_SpinDelay(2100000U);   /* ≈ 100 ms — VDD ramp                    */
/* write PWR_MGMT_1 = 0x80 (DEVICE_RESET) */
MPU6050_SpinDelay(2100000U);   /* ≈ 100 ms — post-reset settle           */
/* write PWR_MGMT_1 = 0x01 (wake)         */
MPU6050_SpinDelay(1050000U);   /* ≈  50 ms — gyro zero-rate settling     */
/* WHO_AM_I DMA read, then 4 config writes */
```

250 ms of calibrated spin delays. These run before the scheduler, on the MSP, with interrupts enabled — so a stray interrupt would lengthen them, but nothing is unmasked yet except the ones `I2C_SVC_Init` enabled.

`MPU6050_SpinDelay` is calibrated at "~4 cycles per iteration" at 84 MHz. With `-O2`, `while (count > 0U) { count--; }` on a `volatile uint32_t` compiles to load/subtract/store/compare/branch — closer to 5–6 cycles. So the real delays are 20–50 % longer than labelled. Harmless (longer is safer for a settling delay) but the calibration comment is optimistic.

## 24. Thread 1 — Sensor Producer

**Name** `"Sensor"` · **Priority** 3 · **Stack** 96 words (384 B) · **Period** 10 ms · **Watchdog** monitored

Thread 1 is the clock master. Everything downstream is paced by it.

### 24.1 Replay mode — what the committed build actually does

```c
#define REPLAY_MODE   1   /* 1 = inject CSV, 0 = live IMU */
```

```c
#if REPLAY_MODE
    uint32_t idx = 0U;

    for (;;)
    {
        xSemaphoreTake(s_tim_sem, portMAX_DELAY);

        if (idx >= REPLAY_NUM_SAMPLES)
        {
            idx = 0U;
            g_replay_done = 1U;
        }

        MPU6050_RawData_t sample = replay_samples[idx];

        if (RingBuffer_Push(&sample) == RING_BUFFER_OK)
        {
            xTaskNotifyGive(s_thread2_handle);
            g_replay_samples_pushed++;
        }
        idx++;

        GPIO_TogglePin(GPIO_PORTC, GPIO_PIN13);
        IWDG_Thread_SetAlive(&IWDG_Thread1_Alive);
    }
#endif
```

```
   TIM2 ISR (100 Hz)
        │  xSemaphoreGiveFromISR(s_tim_sem)
        ▼
   xSemaphoreTake(s_tim_sem, portMAX_DELAY)
        │
        ▼
   idx >= 200 ?  ── yes ──►  idx = 0; g_replay_done = 1
        │ no
        ▼
   sample = replay_samples[idx]        ← from .rodata, no I2C at all
        │
        ▼
   RingBuffer_Push(&sample)
        │  ok → xTaskNotifyGive(Thread 2); g_replay_samples_pushed++
        │  full → silently ignored  ◄── note: NO log in replay mode
        ▼
   idx++
   GPIO_TogglePin(PC13)              ← LED at 50 Hz
   IWDG_Thread_SetAlive(&IWDG_Thread1_Alive)
```

The replay data is 200 samples of `rough` road, recorded at 100 Hz, from `rough_031_20260426_013229.csv`. The array is 2,800 bytes of `.rodata`. At 100 Hz the loop repeats every **2 seconds**.

Consequences **when `REPLAY_MODE` is set back to 1** (the committed build is `0`):

- **The MPU6050 is initialised but never read.** `MPU6050_Init()` still runs in `main()` — including the WHO_AM_I check — but `MPU6050_TriggerRead` is never called.
- **`s_dma_sem` is created and never given.** The `on_mpu_read_done` callback is compiled but unreachable.
- **The classification output is a constant.** With 200 samples of one class looping forever, the model produces the same answer every window. The telemetry looks alive but carries no information.
- **`RingBuffer_Push` failure is not logged.** The live branch calls `LOG_ERROR(LOG_CODE_RING_BUFFER_DROP, 1U)`; the replay branch just skips the notify. So a ring-buffer overflow in replay mode is invisible except through `rb_max_fill` in the heartbeat.

`REPLAY_MODE` is now 0, so none of the above applies to the shipping build. Note there is still no runtime switch, no build flag, and no log message announcing which mode is active — a `LOG_INFO(LOG_CODE_BOOT, REPLAY_MODE)` at startup would make it unmistakable in telemetry. Recorded in [§82](#82-known-gaps).

### 24.2 Live mode

```c
#else
    MPU6050_RawData_t snapshot;

    for (;;){
        xSemaphoreTake(s_tim_sem, portMAX_DELAY);

        if (MPU6050_TriggerRead(on_mpu_read_done, NULL) == MPU6050_OK){
            if (xSemaphoreTake(s_dma_sem, pdMS_TO_TICKS(DMA_TIMEOUT_MS)) == pdTRUE){
                taskENTER_CRITICAL();
                snapshot = (MPU6050_RawData_t)s_latest_sample;
                taskEXIT_CRITICAL();

                if (RingBuffer_Push(&snapshot) == RING_BUFFER_OK){
                    xTaskNotifyGive(s_thread2_handle);
                }
                else{
                    LOG_ERROR(LOG_CODE_RING_BUFFER_DROP, 1U);
                }

                IWDG_Thread_SetAlive(&IWDG_Thread1_Alive);
                GPIO_TogglePin(GPIO_PORTC, GPIO_PIN13);
            }
            else{
                LOG_WARN(LOG_CODE_MPU6050_TIMEOUT, 0U);
            }
        }
        else{
            LOG_ERROR(LOG_CODE_MPU6050_TIMEOUT, 1U);
        }
    }
#endif
```

```
   ┌─────────────────────────────────────────────────────────────┐
   │  BLOCK on s_tim_sem (portMAX_DELAY)                         │
   └──────────────────────────┬──────────────────────────────────┘
                              ▼
   ┌─────────────────────────────────────────────────────────────┐
   │  MPU6050_TriggerRead(on_mpu_read_done, NULL)                │
   │    → I2C_SVC_ReadBurst_DMA(0x68, reg 0x3B, buf, 14)         │
   │    → IRQ TX phase sends the register address                │
   │    → repeated START, DMA1_S0 pulls 14 bytes                 │
   └────────┬──────────────────────────────────┬─────────────────┘
       OK   │                          not OK  │
            ▼                                  ▼
   ┌──────────────────────────┐    LOG_ERROR(MPU6050_TIMEOUT, 1)
   │  BLOCK on s_dma_sem      │    (BUSY / not initialised / SVC reject)
   │  timeout 5 ms            │
   └────┬──────────────┬──────┘
   got  │      timeout │
        ▼              ▼
   critical section    LOG_WARN(MPU6050_TIMEOUT, 0)
   snapshot = s_latest_sample     (DMA did not complete in 5 ms)
        │
        ▼
   RingBuffer_Push(&snapshot)
        ├─ OK   → xTaskNotifyGive(Thread 2)
        └─ FULL → LOG_ERROR(RING_BUFFER_DROP, 1)
        │
        ▼
   IWDG_Thread_SetAlive(&IWDG_Thread1_Alive)
   GPIO_TogglePin(PC13)
```

**The 5 ms DMA timeout.** `DMA_TIMEOUT_MS` is 5, half the 10 ms tick period. An I2C burst of 14 bytes at 400 kHz takes:

```
   14 bytes × 9 bits (8 data + 1 ACK) = 126 bit-times
   plus START, address, repeated START, address, STOP ≈ 40 bit-times
   ≈ 166 bit-times at 400 kHz = 415 µs
```

415 µs against a 5 ms budget — a 12× margin. The timeout exists to catch a **stuck bus**: if the MPU6050 holds SDA low (the classic I2C wedge, caused by a reset mid-transfer), the DMA never completes and without the timeout Thread 1 would block forever, taking the watchdog with it.

Note the distinction between the two log calls:

| Log | Severity | Aux | Meaning |
|---|---|---|---|
| `LOG_CODE_MPU6050_TIMEOUT` | WARN | 0 | DMA did not complete in 5 ms — bus may be stuck |
| `LOG_CODE_MPU6050_TIMEOUT` | ERROR | 1 | `TriggerRead` was rejected outright — driver busy or uninitialised |

Same code, different severity and aux. The logger's debounce logic ([§75](#75-debounce-and-deduplication)) keys on the code, so these two contend for the same slot — a WARN followed by an ERROR would be treated as a state change and emitted, which is the desired behaviour.

**There is no recovery from a stuck bus.** The timeout logs and moves on, and the next tick calls `MPU6050_TriggerRead` again — which will return `MPU6050_ERROR_BUSY` forever, because `mpu_busy` was set and only the (never-arriving) DMA callback clears it. So a single bus wedge permanently disables sampling until reset.

The IWDG catches this indirectly: Thread 1 keeps running and keeps setting its alive flag, but Thread 2 stops being notified, stops setting *its* flag, and the watchdog fires within ~2 s. A reset is a crude but effective recovery. A proper I2C bus-recovery sequence (nine SCL pulses to clock the slave out of its stuck state) would be better and is a known omission in the driver — `MPU6050.c` even documents it in a comment.

### 24.3 The PC13 LED

`GPIO_TogglePin(GPIO_PORTC, GPIO_PIN13)` runs once per 10 ms tick, so PC13 toggles at 100 Hz and the LED blinks at **50 Hz** — visually a steady half-brightness glow.

Thread 4 *also* toggles PC13, once per second. The two writers are not synchronised and both use the non-atomic read-`ODR`-then-write-`BSRR` sequence in `GPIO_TogglePin`. A collision would produce one wrong toggle. Since the LED is a coarse liveness indicator, nobody notices — but it is a genuine unguarded shared resource, and the only one in the design.

The practical reading of PC13:

| Appearance | Meaning |
|---|---|
| Steady dim glow | Thread 1 running at 100 Hz — normal |
| Slow 1 Hz blink | Thread 1 has stopped; only Thread 4 is toggling |
| Static on or off | Both stopped — system hung or resetting |

## 25. Thread 2 — TinyML Inference

**Name** `"ML"` · **Priority** 2 · **Stack** 1,280 words (5,120 B) · **Wake** notification from Thread 1 · **Watchdog** monitored

The compute core. Everything in [Part V](#part-v--the-tinyml-pipeline) happens inside this task.

```c
static void Thread2_TinyML(void *arg)
{
    MPU6050_RawData_t window[WINDOW_SIZE];                  /* stack */

    static int16_t   flat_window[WINDOW_SIZE][N_FEATURES];  /* .bss  */
    static float32_t scaled     [WINDOW_SIZE][N_FEATURES];
    static float32_t features   [N_STAT_FEATURES];
    static int8_t    ts_q       [WINDOW_SIZE * N_FEATURES];
    static int8_t    stat_q     [N_STAT_FEATURES];

    Vote_t vote;
    Vote_Init(&vote);
    Features_Init();        /* one-time 64-point RFFT instance init */
    Inference_Init();       /* one-time CubeAI network create        */

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        g_t2_wake_count++;

        while (RingBuffer_Count() >= WINDOW_SIZE)
        {
            /* ... the pipeline ... */
            RingBuffer_Advance(WINDOW_STRIDE);
        }
        IWDG_Thread_SetAlive(&IWDG_Thread2_Alive);
    }
}
```

### 25.1 The drain loop

```
   ulTaskNotifyTake(pdTRUE, portMAX_DELAY)    ← clears the notification on take
        │
        ▼
   g_t2_wake_count++
        │
        ▼
   ┌──────────────────────────────────────────────────────────┐
   │  while (RingBuffer_Count() >= 50)                        │
   │      ┌────────────────────────────────────────────────┐  │
   │      │ PeekWindow(window, 50)   — tail NOT advanced   │  │
   │      │ flatten → flat_window[50][6]                   │  │
   │      │ Scale_RawWindow    → scaled[50][6]  float      │  │
   │      │ Features_Extract   → features[50]   float      │  │
   │      │ Quantize_TS        → ts_q[300]      int8       │  │
   │      │ Quantize_NormalizeStat (in place)              │  │
   │      │ Quantize_Stat      → stat_q[50]     int8       │  │
   │      │ t0 = DWT_GetCycles()                           │  │
   │      │ Inference_Run(ts_q, stat_q, &result)           │  │
   │      │ us = (DWT_GetCycles() - t0) / 84               │  │
   │      │ if (us > inf_wcet_us) inf_wcet_us = us         │  │
   │      │ Vote_Push(&vote, result.label)                 │  │
   │      │ if (Vote_Ready) → Vote_Decide → queue frame    │  │
   │      │ RingBuffer_Advance(25)      — tail += 25       │  │
   │      └────────────────────────────────────────────────┘  │
   └──────────────────────────────────────────────────────────┘
        │
        ▼
   IWDG_Thread_SetAlive(&IWDG_Thread2_Alive)
```

The `while` rather than `if` is what makes the pipeline self-correcting. Thread 1 notifies on **every** sample, so Thread 2 wakes 100 times per second — but 96 of those wakes find fewer than 50 fresh samples and fall straight through to the alive flag. Only every 25th wake does real work.

```
   Ring buffer count over time (steady state)
   ─────────────────────────────────────────

   50 ┤                    ╭─╮                    ╭─╮
      │                 ╭──╯ │                 ╭──╯ │
   40 ┤              ╭──╯    │              ╭──╯    │
      │           ╭──╯       │           ╭──╯       │
   30 ┤        ╭──╯          │        ╭──╯          │
      │     ╭──╯             │     ╭──╯             │
   25 ┼─────╯                ╰─────╯                ╰──
      │
      └──────────────────────────────────────────────────► time
       ▲                     ▲                     ▲
       Advance(25)           Advance(25)           Advance(25)
       every 250 ms          every 250 ms

   Count oscillates between 25 and 50: 25 samples of permanent
   backlog (the 50 % window overlap) plus 25 accumulating.
```

`rb_max_fill` in the heartbeat should therefore read close to **50** in a healthy system. A reading approaching 128 means Thread 2 is not keeping up.

### 25.2 One-time initialisation inside the task body

```c
Vote_Init(&vote);
Features_Init();
Inference_Init();
```

These run once, before the `for(;;)`, on Thread 2's own stack and in Thread 2's context. That is the right place:

- `Features_Init()` builds the CMSIS-DSP RFFT instance. Doing it in `main()` would work but would put a `arm_rfft_fast_instance_f32` in a scope that outlives its use.
- `Inference_Init()` calls `ai_network_create_and_init`, binding the 1,464-byte activations arena. It must happen before the first `Inference_Run` and after `.bss` is zeroed. Task entry satisfies both.
- `Vote_Init` zeroes the 11-byte vote struct, which lives on Thread 2's stack.

None of the three return values is checked. `Inference_Init()` returns a CubeAI error code:

```c
uint32_t Inference_Init(void){
    ai_error err = ai_network_create_and_init(&s_network, act_addr, NULL);
    if (err.type != AI_ERROR_NONE){
        s_initialised = 0U;
        return (uint32_t)err.code;
    }
    s_initialised = 1U;
    return 0U;
}
```

If it fails, `s_initialised` stays 0 and every subsequent `Inference_Run` returns `{ label = SMOOTH, confidence = 0 }` without touching the network. The node would then classify everything as "smooth" with zero confidence, forever, and nothing would log it. A `LOG_FATAL(LOG_CODE_INFERENCE_FAIL, err)` on a non-zero return would make this loud. `LOG_CODE_INFERENCE_FAIL` (`0x20`) is defined in `logger.h` and **never used anywhere**. Recorded in [§82](#82-known-gaps).

### 25.3 Diagnostic counters

```c
volatile uint32_t g_t2_wake_count       = 0U;   /* notifications received */
volatile uint32_t g_t2_inference_count  = 0U;   /* windows processed      */
volatile uint32_t g_t2_queue_drops      = 0U;   /* frames lost to a full queue */
```

`volatile` and file-scope, so a debugger can watch them live. In a healthy system:

```
   g_t2_wake_count      ≈ 100 × uptime_seconds
   g_t2_inference_count ≈   4 × uptime_seconds
   ratio                ≈  25 : 1              ← equals WINDOW_STRIDE
   g_t2_queue_drops     =   0
```

A ratio other than 25:1 is the fastest diagnostic available for a pipeline problem, and none of these counters is transmitted. Adding `g_t2_inference_count` to the heartbeat would make the inference rate observable remotely.

### 25.4 Replay-mode instrumentation

```c
#if REPLAY_MODE
    g_replay_last_label = (uint32_t)result.label;
    g_replay_last_conf  = (uint32_t)result.confidence;
    if (result.label == INFERENCE_LABEL_SMOOTH) { g_replay_vote_smooth++; }
    else                                        { g_replay_vote_rough++; }
#endif
```

Six extra globals for offline validation. With `replay_data.h` holding 200 samples of *rough* road, a correct model should give `g_replay_vote_rough` ≫ `g_replay_vote_smooth`. That single ratio is the on-target accuracy check, readable from a debugger with no host tooling.

`REPLAY_EXPECTED_CLASS_STR` is `"rough"` in the header, defined but never referenced in code — it is documentation for whoever reads the watch window.

## 26. Thread 3 — UART Transmit

**Name** `"TX"` · **Priority** 1 · **Stack** 128 words (512 B) · **Wake** `g_frame_queue` · **Watchdog** monitored

The single writer to UART1. Every telemetry byte the node emits passes through here.

```c
static void Thread3_UartTx(void *arg)
{
    FrameRequest_t req;
    uint16_t frame_len;
    Buffer_t buf;

    for (;;)
    {
        if (xQueueReceive(g_frame_queue, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (Frame_Build(req.type, req.ecu_id, req.payload, req.length,
                        s_tx_frame, sizeof(s_tx_frame), &frame_len) != FRAME_OK) {
            LOG_ERROR(LOG_CODE_FRAME_BUILD_FAIL, req.type);
            continue;
        }

        buf.data   = s_tx_frame;
        buf.size   = sizeof(s_tx_frame);
        buf.length = frame_len;
        buf.index  = 0U;
        UART_SVC_TransmitDMA(UART1_ID, &buf);

        GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_SET);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_RESET);
        IWDG_Thread_SetAlive(&IWDG_Thread3_Alive);
    }
}
```

```
   BLOCK on g_frame_queue (portMAX_DELAY)
        │
        ▼
   Frame_Build(type, ecu, payload, len → s_tx_frame)
        │  builds [LEN][TYPE][ECU][payload][CRC32 BE]
        ├─ error → LOG_ERROR(FRAME_BUILD_FAIL, type); continue
        ▼
   UART_SVC_TransmitDMA(UART1_ID, &buf)
        │  DMA2_S7 reads directly from s_tx_frame
        ▼
   PA8 HIGH                                  ◄── §55
        │
        ▼
   BLOCK on task notification (portMAX_DELAY)
        │  DMA2_S7 TC ISR → uart_svc_dma_tx_complete
        │                 → on_uart_tx_done
        │                 → vTaskNotifyGiveFromISR(Thread 3)
        ▼
   PA8 LOW
   IWDG_Thread_SetAlive(&IWDG_Thread3_Alive)
```

### 26.1 The sync pulse is raised *after* the DMA starts

```c
UART_SVC_TransmitDMA(UART1_ID, &buf);           /* DMA armed and running */
GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_SET);   /* THEN raise PA8 */
```

By the time PA8 goes high, the DMA controller has already pushed the first byte into `USART1->DR` and the UART has begun shifting it out. The gap is a few hundred nanoseconds of GPIO and function-call overhead — but the first bit of the first byte is already on the wire.

For the pulse to serve as a *frame start* marker it would need to be raised **before** the transmit call. As written it is closer to a "transmission in progress" indicator. See [§55](#55-the-gpio-sync-line) for the full analysis and what the gateway actually does with it.

### 26.2 `s_tx_frame` and DMA lifetime

```c
static uint8_t s_tx_frame[FRAME_OVERHEAD_BYTES + FRAME_MAX_PAYLOAD];   /* 7 + 248 = 255 */
```

A single file-scope buffer. `UART_SVC_TransmitDMA` points the DMA controller directly at it — there is no copy:

```c
DMA_UpdateMemoryAddress(inst->dma_id, inst->dma_stream, (uint32_t)buf->data);
```

That imposes a hard rule: **`s_tx_frame` must not be modified until the DMA completes.** Thread 3 enforces it by blocking on the completion notification before looping back to `Frame_Build`. Since Thread 3 is the only writer, and it always blocks, the rule holds.

If a second task ever transmitted, or if Thread 3 were changed to use a timeout on the notification, this would become a use-after-free-shaped bug: the DMA would be reading a buffer being overwritten, producing frames with a valid header and a corrupt tail that still passes CRC (because the CRC was computed over the *old* contents… no — the CRC would be from the old frame over new data, so it would fail). Detectable, but confusing.

### 26.3 The unbounded wait

```c
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

If the DMA completion interrupt never fires — DMA misconfiguration, a stream error that takes the error path but whose callback is somehow not invoked, an NVIC line disabled — Thread 3 blocks forever. It then stops setting `IWDG_Thread3_Alive`, and the watchdog resets the node within ~2 s.

That is an acceptable recovery, and it is the *intended* one: Thread 3 is watchdog-monitored precisely so that a stuck transmit becomes a reset rather than a silent stall. A finite timeout with a `LOG_ERROR` would give better diagnostics, at the cost of needing to decide what to do with the half-transmitted frame.

Note that `uart_svc_dma_tx_complete` invokes the user callback on **both** completion and error:

```c
else if (event == DMA_EVENT_TRANSFER_ERROR) {
    svc[id].error     = 1U;
    svc[id].tx_active = 0U;
    UART_DisableTxDMA(id);
    if (svc[id].tx_done_cb != NULL) { svc[id].tx_done_cb(svc[id].tx_done_ctx); }
}
```

So a DMA error also wakes Thread 3, which then lowers PA8 and carries on as if the frame had been sent. The `svc[id].error` flag is set but **nothing ever reads it** — `UART_SVC_IsError` exists and is never called. A transmit error is therefore completely silent. Recorded in [§82](#82-known-gaps).

### 26.4 `Frame_Build` failure

`FRAME_ERR_PAYLOAD_TOO_BIG` requires `len > 248`; `FrameRequest_t::payload` is 32 bytes, so it is unreachable. `FRAME_ERR_BUF_TOO_SMALL` requires `out_cap < len + 7`; `sizeof(s_tx_frame)` is 255 and `len ≤ 32`, so it is unreachable. `FRAME_ERR_NULL_PTR` requires a NULL `payload` with `len > 0`; `req.payload` is an array member and can never be NULL.

So `LOG_CODE_FRAME_BUILD_FAIL` is dead in the current configuration. It is correct defensive coding — the check costs three comparisons — and it would come alive if `FRAME_REQ_MAX_PAYLOAD` were ever raised past 248.
## 27. Thread 4 — Heartbeat

**Name** `"HB"` · **Priority** 1 · **Stack** 256 words (1,024 B) · **Period** 1,000 ms · **Frame every 5 s** · **Watchdog** monitored

Thread 4 is the node's self-report. Every second it samples runtime counters; every fifth second it averages them and pushes a 30-byte heartbeat frame.

```
   ┌────────────────────────────────────────────────────────────┐
   │  vTaskDelayUntil(&prev_wake, 1000 ms)                      │
   └──────────────────────────┬─────────────────────────────────┘
                              ▼
   ┌────────────────────────────────────────────────────────────┐
   │  IWDG_Thread_SetAlive(&IWDG_Thread4_Alive)   ← early       │
   ├────────────────────────────────────────────────────────────┤
   │  vTaskGetInfo × 6:  T1, T2, T3, T6, T7, IDLE               │
   │  ulGetRunTimeCounterValue()  → total_now                   │
   ├────────────────────────────────────────────────────────────┤
   │  first_run?                                                │
   │    yes → snapshot baselines, zero all cpu_*, first_run = 0 │
   │    no  → delta math, x100 scaling, clamp to 10000,         │
   │          accumulate into acc_*_x100, tick_count++          │
   ├────────────────────────────────────────────────────────────┤
   │  uptime_ms, stack HWM × 5, rb_max_fill                     │
   ├────────────────────────────────────────────────────────────┤
   │  tick_count >= 5 ?                                         │
   │    yes → average, pack 30 bytes, xQueueSend, reset accum   │
   ├────────────────────────────────────────────────────────────┤
   │  IWDG_Thread_SetAlive(&IWDG_Thread4_Alive)   ← again       │
   │  GPIO_TogglePin(PC13)                                      │
   └────────────────────────────────────────────────────────────┘
```

### 27.1 `vTaskDelayUntil` versus `vTaskDelay`

```c
TickType_t prev_wake = xTaskGetTickCount();
for (;;) {
    vTaskDelayUntil(&prev_wake, pdMS_TO_TICKS(1000));
    ...
}
```

`vTaskDelayUntil` computes the wake time as `prev_wake + period` and updates `prev_wake` to that value — so execution time inside the loop does not accumulate as drift. `vTaskDelay(1000)` would mean "sleep 1000 ms *after* finishing the work", and the period would become `1000 + work_time`, drifting by the work duration every cycle.

For a task whose whole purpose is reporting `uptime_ms` and per-second CPU deltas, drift-free periodicity is not optional. Threads 6 and 7 use the same pattern for the same reason.

### 27.2 The two-level averaging

```
   Every 1 s:  sample counters, compute a 1-second CPU %, accumulate
   Every 5 s:  divide the accumulator by 5, emit, reset
```

```c
acc_t1_x100 += g_stats.cpu_t1_x100;
...
tick_count++;

if (tick_count >= HB_SEND_INTERVAL) {          /* 5 */
    uint16_t avg_t1_x100 = (uint16_t)(acc_t1_x100 / HB_SEND_INTERVAL);
    ...
}
```

Why average rather than sample instantaneously? A 1-second CPU figure for Thread 2 in this workload is dominated by *where in the 250 ms inference cycle the sample lands*. Four inferences per second, each taking a few milliseconds — the per-second figure is genuinely stable, but the per-*measurement* figure would not be if the period were shorter. The 5-second average smooths the residual.

The peaks are tracked separately and are **not** averaged:

```c
if (g_stats.inf_wcet_us > peak_wcet_us) { peak_wcet_us = g_stats.inf_wcet_us; }
if (g_stats.rb_max_fill > peak_rb_fill) { peak_rb_fill = g_stats.rb_max_fill; }
```

Averaging a worst case would defeat the purpose. `peak_wcet_us` and `peak_rb_fill` report the maximum over the 5-second window and are reset after each send — so each heartbeat carries a fresh window's worst case.

**But note:** `g_stats.inf_wcet_us` itself is a *running maximum since boot*, never reset:

```c
/* Thread 2 */
if (us > g_stats.inf_wcet_us) { g_stats.inf_wcet_us = (uint16_t)us; }
```

So `peak_wcet_us` tracks the max of a monotonically non-decreasing value, which means it equals `g_stats.inf_wcet_us` at send time. The 5-second windowing has no effect on WCET — it reports the all-time maximum every time. Whether that is desirable is a judgement call: an all-time WCET is the safety-relevant number; a windowed WCET would show whether the system is degrading. Currently only the former is available, and the code reads as though it intended the latter. Recorded in [§82](#82-known-gaps).

`rb_max_fill` has the same structure (`RingBuffer_GetMaxFill()` is also monotonic since `RingBuffer_Init`), so `peak_rb_fill` is likewise an all-time maximum.

### 27.3 `first_run` and the baseline

```c
if (first_run) {
    prev_t1_cycles = t1_now;   /* ... etc ... */
    prev_total     = total_now;
    first_run      = 0U;
    g_stats.cpu_t1_x100 = 0U;  /* ... all zeroed ... */
}
```

The first iteration cannot compute a delta — there is no previous sample. It records baselines and reports 0 %. So the first heartbeat after boot (at t ≈ 5 s) averages four real measurements and one zero, understating CPU by 20 %. A minor and self-correcting artefact.

Note `tick_count++` is inside the `else` branch, so the first iteration does not count toward the five. The first heartbeat therefore goes out at t ≈ 6 s, not 5 s.

### 27.4 The `s_idle_handle` acquisition

```c
static TaskHandle_t s_idle_handle = NULL;
...
if (s_idle_handle == NULL) {
    s_idle_handle = xTaskGetIdleTaskHandle();
}
```

This sits **outside** the `for(;;)` loop, so it runs exactly once at task entry. The `if` is therefore redundant — `s_idle_handle` is guaranteed NULL at that point because it is a zero-initialised `static`. Harmless.

`xTaskGetIdleTaskHandle()` requires `INCLUDE_xTaskGetIdleTaskHandle = 1`, which is set.

### 27.5 The `static` locals

Nearly every local in Thread 4 is `static`:

```c
static uint32_t prev_t1_cycles = 0U;
static uint32_t acc_t1_x100    = 0U;
static uint8_t  tick_count     = 0U;
static uint32_t start_tick     = 0U;
static uint8_t  first_run      = 1U;
```

For a task function that never returns, `static` versus automatic makes no functional difference to lifetime — but it moves them from the 1,024-byte stack into `.bss`. With about 60 bytes of accumulators that is not why it was done; more likely it is to make them visible in a debugger watch window by name, which automatics inside an optimised function often are not.

One consequence: `start_tick` is assigned at task entry:

```c
static uint32_t start_tick = 0U;
start_tick = xTaskGetTickCount();
```

The `static` initialiser to 0 runs at load time, and the assignment runs at task entry. Both happen; the assignment wins. If the task could ever restart (it cannot — no `vTaskDelete`), the `static` would retain the old value until reassigned. Fine as written.

### 27.6 `uptime_ms` is task-relative

```c
g_stats.uptime_ms = (uint32_t)((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS);
```

`start_tick` is captured when *Thread 4* first runs, not at reset. Thread 4 is created in step 11 of `main()`, but tasks do not run until `vTaskStartScheduler()`. So `uptime_ms` excludes:

- The ~300 ms of `main()` initialisation (dominated by `MPU6050_Init`).
- Any scheduler latency before Thread 4's first slice.

The offset is a few hundred milliseconds and constant, so it does not affect uptime *differences* — only the absolute value, which is understated. Using `xTaskGetTickCount()` directly (which counts from `vTaskStartScheduler`) would be marginally more accurate and simpler.

At 1 ms ticks, `uint32_t` wraps at 2³² ms ≈ **49.7 days**.

### 27.7 The payload packing

Thirty bytes, all multi-byte fields big-endian:

```c
req.payload[0]  = (uint8_t)(g_stats.uptime_ms >> 24);
req.payload[1]  = (uint8_t)(g_stats.uptime_ms >> 16);
req.payload[2]  = (uint8_t)(g_stats.uptime_ms >>  8);
req.payload[3]  = (uint8_t)(g_stats.uptime_ms);

req.payload[4]  = (uint8_t)(avg_t1_x100 >> 8);
req.payload[5]  = (uint8_t)(avg_t1_x100);
/* ... six CPU figures, five stack figures, WCET, RB fill ... */

req.type   = FRAME_TYPE_HEARTBEAT;
req.ecu_id = ECU_ID_STM32_NODE1;
req.length = HEARTBEAT_PAYLOAD_SIZE;      /* 30 */
```

Full layout in [§51](#51-heartbeat-payload).

Note that the packing is done **by hand from `g_stats` and the averages**, not by `memcpy`ing the `Heartbeat_Payload_t` struct. That is deliberate and correct: the struct is `__attribute__((packed))` but its field order and the host's byte order would still make a raw copy little-endian. Manual big-endian packing is the only portable option.

The struct `Heartbeat_Payload_t` is therefore used only as a *convenient accumulator* (`g_stats`), never as a wire format. That is worth knowing, because the struct's own doc comment describes it as the wire layout — and describes it as **24 bytes with a different field set**, which is stale by two fields and six bytes. See [§51](#51-heartbeat-payload).

## 28. Thread 5 — Bootloader Receive

**Name** `"BL_RX"` · **Priority** 4 (highest) · **Stack** 256 words (1,024 B) · **Wake** UART RX ISR notification · **Watchdog** not monitored

The only inbound command path. Thread 5 exists to recognise two bytes and reboot the node into the bootloader.

```c
static void Thread5_BootloaderRx(void *arg)
{
    UART_SVC_SetRxNotifyTask(UART1_ID, xTaskGetCurrentTaskHandle());

    uint8_t  rx_byte = 0U;
    uint8_t  state   = 0U;   /* 0 = waiting for 0xAA, 1 = waiting for 0xEB */
    Buffer_t rx_buf;
    uint16_t avail = 0U;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (UART_SVC_RxAvailable(UART1_ID, &avail) == UART_SVC_OK && avail > 0U)
        {
            rx_buf.data = &rx_byte;  rx_buf.size = 1U;
            rx_buf.length = 1U;      rx_buf.index = 0U;

            if (UART_SVC_Receive(UART1_ID, &rx_buf) != UART_SVC_OK) break;

            if (state == 0U) {
                if (rx_byte == 0xAAU) state = 1U;
            }
            else {
                if (rx_byte == 0xEBU) {
                    static uint8_t ack_packet[2] = {0xEEU, 0xAAU};
                    Buffer_t tx_buf = { ack_packet, 2U, 2U, 0U };
                    (void)UART_Transmit_Polling(UART1_ID, &tx_buf, 10000UL);

                    *((volatile uint32_t *)0x2000FFF8UL) = 0xDEADBEEF;
                    *((volatile uint32_t *)0xE000ED0CU) = (0x05FAUL << 16U) | (1UL << 2U);
                    for (;;) {}
                }
                else {
                    state = (rx_byte == 0xAAU) ? 1U : 0U;
                }
            }
        }
    }
}
```

### 28.1 The state machine

```
                    ┌─────────────────────────────┐
                    │  State 0 — waiting for 0xAA │
                    └──────────┬──────────────────┘
                               │
             byte == 0xAA ─────┤───── byte != 0xAA ──► stay in State 0
                               ▼
                    ┌─────────────────────────────┐
                    │  State 1 — waiting for 0xEB │
                    └──────────┬──────────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
   byte == 0xEB          byte == 0xAA           anything else
        │                      │                      │
        ▼                      ▼                      ▼
  ┌──────────────┐     stay in State 1          back to State 0
  │ ENTER BOOTLO │     (a new first byte)
  │              │
  │ 1. TX EE AA  │  UART_Transmit_Polling — blocks until TC
  │ 2. RAM flag  │  *(0x2000FFF8) = 0xDEADBEEF
  │ 3. SYSRESET  │  SCB->AIRCR = 0x05FA0004
  │ 4. for(;;)   │  unreachable
  └──────────────┘
```

The `state = (rx_byte == 0xAAU) ? 1U : 0U;` on mismatch is the detail that makes the matcher robust. Consider the input `AA AA EB`:

```
   0xAA  state 0 → 1
   0xAA  state 1, not 0xEB, but IS 0xAA → stay in state 1
   0xEB  state 1, is 0xEB → trigger
```

A naive `state = 0` on mismatch would consume the second `0xAA` as a failed match, land in state 0, see `0xEB`, and miss the command entirely. The re-check costs one comparison and eliminates a whole class of missed triggers.

### 28.2 It does *not* erase Flash

The earlier `README.md` in this directory describes Thread 5 as:

> *2. Erases **Sector 1** (wipes the bootloader's second part to signal it should receive a new image)*

**That is not what the current code does, and it would be actively harmful if it did** — sector 1 is bootloader code, and erasing it would destroy roughly half the bootloader image.

The current mechanism is the RAM boot flag, described fully in [`../Bootloader/BOOTLOADER.md`](../Bootloader/BOOTLOADER.md) §8:

```c
*((volatile uint32_t *)0x2000FFF8UL) = 0xDEADBEEF;
```

No Flash operation of any kind. `FLASH_INTERFACE.h` is still `#include`d by `main.c` and `FLASH.c` is still compiled and linked — several kilobytes of driver for a subsystem the application no longer uses. The commit history (`fd7adfa Bootloader & Application switching via RAM`) marks the transition.

This is the single most misleading piece of stale documentation in the project, because a reader who trusts it would conclude that entering the bootloader is destructive.

### 28.3 The ACK uses polling, deliberately

```c
(void)UART_Transmit_Polling(UART1_ID, &tx_buf, 10000UL);
```

with the comment:

```c
/* 1. ACK back to ESP32 using UART Polling to bypass DMA conflicts */
```

Three reasons this is right:

1. **Thread 3 may hold the DMA.** `UART_SVC_TransmitDMA` returns `UART_SVC_ERROR_BUSY` if `tx_active` is set. Thread 5 preempted Thread 3 at priority 4, possibly mid-frame. Polling bypasses the service layer's busy flag entirely and writes `USART1->DR` directly.
2. **The reset must not outrun the ACK.** `UART_Transmit_Polling` waits for the **TC** flag — transmission complete, shift register empty — after the last byte. Only then does it return. So when `SCB->AIRCR` is written, both bytes are fully on the wire. A DMA transmit would have returned immediately with the bytes still in flight.
3. **No task switch is possible in between.** Thread 5 is the highest-priority task and the sequence contains no blocking call, so nothing can preempt between the ACK and the reset.

The interleaving hazard is real but benign: if Thread 3 had an active DMA transfer, Thread 5's polled writes to `USART1->DR` collide with the DMA's writes, producing a garbled frame. Moments later the MCU resets. The gateway sees a corrupt frame (which fails CRC and is discarded) followed by the ACK. Acceptable.

### 28.4 The drain loop

```c
while (UART_SVC_RxAvailable(UART1_ID, &avail) == UART_SVC_OK && avail > 0U)
```

Rather than reading one byte per notification, Thread 5 drains the entire RX ring buffer on each wake. This matters because the UART ISR notifies on **every** received byte:

```c
case UART_EVENT_RXNE:
    UART_ReadDR(id, &data);
    rx_push(&inst->rx, (uint8_t)(data & 0xFFU));
    if (inst->rx_notify_task != NULL) {
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(inst->rx_notify_task, &woken);
        portYIELD_FROM_ISR(woken);
    }
    break;
```

Task notifications are a counting mechanism but `ulTaskNotifyTake(pdTRUE, …)` **clears** the count on take rather than decrementing it. So two bytes arriving before Thread 5 runs produce one wake, not two. Draining the whole buffer is what makes that safe.

### 28.5 Registration order

```c
UART_SVC_SetRxNotifyTask(UART1_ID, xTaskGetCurrentTaskHandle());
```

is the first statement of the task body, not something done in `main()`. That means:

```
   main(): app_uart_init() → UART_SVC_Init → UART_EnableRxIRQ + NVIC enable
              │
              │  RX ISR is now live, but rx_notify_task is NULL
              │  → bytes are buffered, no notification
              ▼
   vTaskStartScheduler()
              │
              ▼
   Thread 5 (priority 4) runs first among ready tasks
              │  UART_SVC_SetRxNotifyTask(UART1_ID, self)
              ▼
   from here on, every byte notifies Thread 5
```

There is a window between `UART_SVC_Init` and Thread 5's first instruction in which bytes are received into the ring buffer but generate no notification. Because Thread 5 is the highest-priority task, it is the *first* task to run after the scheduler starts, so the window is at most the ~300 ms of `main()` initialisation after `app_uart_init()`.

Bytes arriving in that window are not lost — they sit in the 256-byte RX ring. Thread 5's first `ulTaskNotifyTake` would block, though, because no notification was generated. So an enter-bootloader command sent during boot would be buffered and then ignored until a *subsequent* byte arrived to generate a notification.

The gateway sends `AA EB` and waits 1,000 ms; if the node was mid-boot, the two bytes buffer, no notification fires, and the gateway times out. Its next attempt sends two more bytes, the first of which generates a notification, and Thread 5 then drains all four — matching `AA EB` from the first attempt. So the second attempt always works. Self-healing, but the first attempt after a reset can fail.

Draining the ring buffer once at task entry, before the first `ulTaskNotifyTake`, would close this. Recorded in [§82](#82-known-gaps).

### 28.6 Not watchdog-monitored

Thread 5 has no `IWDG_Thread_SetAlive` call, and correctly so: it blocks indefinitely by design. A liveness flag would have to be set on a path that may legitimately never execute.

The trade is that a wedged Thread 5 — say, `UART_SVC_Receive` returning an error every time — would go undetected, and the node would become un-enterable remotely. The only recovery would be a power cycle with the gateway retrying, or ST-Link.

## 29. Thread 6 — Temperature

**Name** `"TEMP"` · **Priority** 1 · **Stack** 192 words (768 B) · **Period** 2,000 ms · **Watchdog** not monitored

```c
static void Thread6_Temperature(void *arg)
{
    TickType_t prev_wake = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&prev_wake, pdMS_TO_TICKS(2000U));

        uint16_t adc_raw = 0U;
        if (ADC_Read(&adc_raw) != ADC_OK)
        {
            LOG_WARN(LOG_CODE_ADC_TIMEOUT, 0U);
            continue;
        }
        else
        {
            LOG_INFO(LOG_CODE_ADC_TIMEOUT, 0U);
        }

        uint16_t temp_x10 = ADC_LM35_ToTenthsCelsius(adc_raw, 3300U);
        uint32_t ts = (uint32_t)(xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS);

        FrameRequest_t req;
        req.type       = FRAME_TYPE_TEMPERATURE;
        req.ecu_id     = ECU_ID_STM32_NODE1;
        req.length     = 6U;
        req.payload[0] = (uint8_t)(temp_x10 >> 8);
        req.payload[1] = (uint8_t)(temp_x10);
        req.payload[2] = (uint8_t)(ts >> 24);
        req.payload[3] = (uint8_t)(ts >> 16);
        req.payload[4] = (uint8_t)(ts >>  8);
        req.payload[5] = (uint8_t)(ts);

        if (xQueueSend(g_frame_queue, &req, 0) != pdTRUE)
        {
            LOG_WARN(LOG_CODE_QUEUE_FULL, 2U);
        }
    }
}
```

### 29.1 The LM35 conversion

```c
uint16_t ADC_LM35_ToTenthsCelsius(uint16_t raw, uint16_t vref_mv){
    uint32_t vout_mv = ((uint32_t)raw * (uint32_t)vref_mv) / 4095U;
    return (uint16_t)(vout_mv & 0xFFFFU);
}
```

```
   LM35 output:  10 mV per °C, linear, 0 °C = 0 V

   V_out (mV)  = raw × 3300 / 4095
   T (°C)      = V_out / 10
   T (tenths)  = T × 10 = V_out

   → tenths of °C == millivolts, exactly.
```

The identity `T_x10 == V_out_mV` is the whole trick, and it is why the function body is a single expression. Worked example:

```
   raw = 292
   V_out = 292 × 3300 / 4095 = 235 mV
   T     = 23.5 °C
   T_x10 = 235                            ✓
```

The comment inside the function is confused about this — it says "Temp_x10 = Vout_mV / 10 * 10 = Vout_mV" and then talks itself through the algebra twice, arriving at the right answer. `ADC_INTERFACE.h`'s doc comment states it wrongly:

```c
 * Temp_x10 = Vout_mV / 10          → gives tenths of °C
```

That would give whole °C, not tenths. The example immediately below (`raw=620 → Vout≈499mV → ~49.9°C → returns 499`) is correct and contradicts the formula above it. The **code is right**; two of the three comments describing it are wrong.

### 29.2 The 480-cycle sample time

```c
ADC_Config_t adc_cfg = {
    .channel     = 1U,
    .resolution  = ADC_RES_12BIT,
    .sample_time = ADC_SAMPLETIME_480,
};
```

The ADC sampling capacitor must charge through the source impedance. The LM35's output impedance is roughly 0.1 Ω on paper — but only when driving a low-impedance load; in a typical wiring arrangement with a series resistor or a long lead, the effective source impedance is far higher.

```
   ADC clock = PCLK2 / 4 = 21 MHz
   Sample time = 480 cycles / 21 MHz = 22.9 µs
   Conversion  =  12 cycles / 21 MHz =  0.57 µs
   Total       ≈ 23.4 µs per reading
```

23 µs of charging time supports a source impedance up to about 50 kΩ at 12 bits. The slowest setting is the right default for an analog sensor whose drive characteristics are not precisely known — and at 0.5 Hz the cost is irrelevant.

`ADC_Read` spins for up to 100,000 iterations waiting for `EOC`:

```c
uint32_t timeout = 100000U;
while (ADC1->SR.BITS.EOC == 0U) {
    if (timeout == 0U) { return ADC_ERROR_TIMEOUT; }
    timeout--;
}
```

At ~5 cycles per iteration that is about 6 ms — 250× the expected 23 µs. It exists to catch a completely dead ADC, not to bound normal latency. Note this spin happens at priority 1 with interrupts enabled, so it does not block anything important.

### 29.3 The INFO-on-success pattern

```c
if (ADC_Read(&adc_raw) != ADC_OK) { LOG_WARN(LOG_CODE_ADC_TIMEOUT, 0U); continue; }
else                              { LOG_INFO(LOG_CODE_ADC_TIMEOUT, 0U); }
```

Logging *success* with the same code as failure, distinguished only by severity, looks wrong at first read. It is deliberate and it exploits the logger's state machine ([§75](#75-debounce-and-deduplication)).

The logger tracks `s_last_severity[code]` and suppresses any log whose severity matches the last one emitted for that code. So:

```
   t=0    ADC works    → LOG_INFO(ADC_TIMEOUT)   → last_severity = INFO, emitted
   t=2    ADC works    → LOG_INFO(ADC_TIMEOUT)   → same severity, SUPPRESSED
   t=4    ADC works    → suppressed
   ...
   t=100  ADC fails    → LOG_WARN(ADC_TIMEOUT)   → severity changed, EMITTED
   t=102  ADC fails    → suppressed
   ...
   t=200  ADC recovers → LOG_INFO(ADC_TIMEOUT)   → severity changed, EMITTED  ◄── "recovered"
```

The result is **edge-triggered logging**: the gateway receives exactly one message when the sensor breaks and exactly one when it recovers, with nothing in between. That is dramatically more useful than either "log every failure" (a flood) or "log only failures" (no recovery signal).

The same pattern appears in Thread 7 for both ultrasonic sensors. It is the single cleverest thing in the diagnostics design.

The cost is that the log *code* name (`ADC_TIMEOUT`) no longer describes the event — an INFO with that code means "ADC is fine". A name like `LOG_CODE_ADC_STATE` would read better.

### 29.4 The timestamp

```c
uint32_t ts = (uint32_t)(xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS);
```

`portTICK_PERIOD_MS` is `1000 / configTICK_RATE_HZ` = 1, so the multiply is a no-op and `ts` is the raw tick count in milliseconds since `vTaskStartScheduler()`.

This is a **different time base** from the classification frame's timestamp, which uses raw ticks without the multiply:

```c
/* Thread 2 */
uint32_t ts = (uint32_t)xTaskGetTickCount();
```

Numerically identical at a 1 kHz tick, so the two agree today. If `configTICK_RATE_HZ` ever changed, they would diverge silently. Recorded in [§82](#82-known-gaps).

## 30. Thread 7 — Ultrasonic

**Name** `"ULTRA"` · **Priority** 1 · **Stack** 128 words (512 B) · **Period** 250 ms · **Watchdog** not monitored

Two HC-SR04 sensors, fired sequentially, both distances packed into one frame.

```
   ┌────────────────────────────────────────────────────────────┐
   │  vTaskDelayUntil(&prev_wake, 250 ms)                       │
   └──────────────────────────┬─────────────────────────────────┘
                              ▼
   ┌────────────────────────────────────────────────────────────┐
   │  xQueueReset(g_ultrasonic_queue)                           │
   │  HCSR04_Trigger(&u1)          — 10 µs pulse on PA4         │
   │  xQueueReceive(…, &msg1, 50 ms)                            │
   │     got → if (msg1 >> 16 == 0) dist1 = msg1 & 0xFFFF       │
   │           if (dist1 > 400) dist1 = 400                     │
   │           LOG_INFO(ULTRASONIC1_TIMEOUT, 1)                 │
   │     timeout → LOG_WARN(ULTRASONIC1_TIMEOUT, 1)             │
   │               dist1 = 0xFFFF                               │
   ├────────────────────────────────────────────────────────────┤
   │  xQueueReset(g_ultrasonic_queue)                           │
   │  HCSR04_Trigger(&u2)          — 10 µs pulse on PA5         │
   │  xQueueReceive(…, &msg2, 50 ms)                            │
   │     … same, checking (msg2 >> 16 == 1) …                   │
   ├────────────────────────────────────────────────────────────┤
   │  pack 8 bytes: dist1 BE, dist2 BE, timestamp BE            │
   │  xQueueSend(g_frame_queue, &req, 0)                        │
   └────────────────────────────────────────────────────────────┘
```

### 30.1 The capture ISR

```c
static void on_hcsr04_capture(void *ctx)
{
    uint32_t id = (uint32_t)ctx;              /* 0 for CH1, 1 for CH2 */
    TIM_Channel_t ch = (id == 0U) ? TIM_CH_1 : TIM_CH_2;
    uint32_t val = 0U;
    TIM_IC_GetCapture(TIM_ID_3, ch, &val);

    volatile TIM_REGS_t *tim3_regs = (volatile TIM_REGS_t *)0x40000400UL;
    uint32_t cc_shift = (id == 0U) ? 0U : 4U;

    if ((tim3_regs->CCER.ALL & (1U << (cc_shift + 1U))) == 0U)
    {
        /* CCxP == 0 → we were armed for RISING; this is the rising edge */
        capture_val1[id] = val;
        tim3_regs->CCER.ALL |= (1U << (cc_shift + 1U));      /* arm FALLING */
    }
    else
    {
        /* CCxP == 1 → this is the falling edge */
        capture_val2[id] = val;
        tim3_regs->CCER.ALL &= ~(1U << (cc_shift + 1U));     /* re-arm RISING */

        uint32_t diff;
        if (capture_val2[id] >= capture_val1[id])
            diff = capture_val2[id] - capture_val1[id];
        else
            diff = (0xFFFFU - capture_val1[id]) + capture_val2[id] + 1U;

        uint32_t dist_cm = diff / 58U;
        uint32_t msg = (id << 16U) | (dist_cm & 0xFFFFU);

        BaseType_t woken = pdFALSE;
        xQueueSendFromISR(g_ultrasonic_queue, &msg, &woken);
        portYIELD_FROM_ISR(woken);
    }
}
```

```
   HC-SR04 protocol
   ────────────────

   TRIG (PA4/PA5)
        ┌──┐
        │10│ µs
   ─────┘  └────────────────────────────────────────────

   Sensor emits 8 × 40 kHz bursts, then:

   ECHO (PA6/PA7)
                    ┌──────────────────┐
                    │   HIGH for the   │
   ─────────────────┘   round trip     └────────────────
                    ▲                  ▲
                capture_val1       capture_val2
                (rising, CCxP=0)   (falling, CCxP=1)

   TIM3 counts at 1 MHz → 1 tick = 1 µs

   distance_cm = diff_µs / 58
     (sound at 343 m/s = 29.15 µs/cm one way; ×2 for round trip = 58.3)
```

**The polarity toggle is the mechanism.** TIM3 input capture on the F4 cannot capture both edges into separate registers on one channel, so the ISR flips `CCER.CCxP` after each edge: capture rising → switch to falling → capture falling → switch back to rising. Two interrupts per measurement.

**Direct register access inside the ISR.** `on_hcsr04_capture` bypasses the TIM driver to toggle `CCER`:

```c
volatile TIM_REGS_t *tim3_regs = (volatile TIM_REGS_t *)0x40000400UL;
```

with a hardcoded base address, because `TIM_INTERFACE.h` exposes no polarity-change function. `main.c` even has to `#undef TIM2/TIM3/TIM4/TIM5` after including `TIM_REGS.h`, because those macros collide with the `IRQn_t` enumerators of the same names:

```c
#include "TIM_REGS.h"
#undef TIM2
#undef TIM3
#undef TIM4
#undef TIM5
```

A `TIM_IC_SetPolarity(id, channel, polarity)` in the driver would remove both the hardcoded address and the `#undef` dance.

**Rollover handling.** TIM3's ARR is 65535, so the counter wraps every 65.536 ms. If the rising edge is captured just before a wrap and the falling edge just after, `capture_val2 < capture_val1` and the naive subtraction underflows. The `else` branch handles it:

```c
diff = (0xFFFFU - capture_val1[id]) + capture_val2[id] + 1U;
```

An echo pulse can be at most ~38 ms (the HC-SR04's own timeout), so at most one wrap can occur — the arithmetic is correct for that case and cannot be correct for two, which cannot happen.

### 30.2 The message encoding

```c
uint32_t msg = (id << 16U) | (dist_cm & 0xFFFFU);
```

```
   bit 31          16 15            0
   ┌─────────────────┬───────────────┐
   │   sensor id     │  distance cm  │
   │   0 or 1        │   0..65535    │
   └─────────────────┴───────────────┘
```

The sensor id lets Thread 7 verify that the message it received belongs to the sensor it just triggered — necessary because both channels share one queue.

### 30.3 A real bug: a mismatched id reports 400 cm

```c
uint16_t dist1 = 0xFFFFU;
if (xQueueReceive(g_ultrasonic_queue, &msg1, pdMS_TO_TICKS(50U)) == pdTRUE)
{
    if ((msg1 >> 16U) == 0U) {
        dist1 = (uint16_t)(msg1 & 0xFFFFU);
    }
    if (dist1 > 400U) {
        dist1 = 400U;                    /* ◄── applies to the sentinel too */
    }
    LOG_INFO(LOG_CODE_ULTRASONIC1_TIMEOUT, 1U);
}
else
{
    LOG_WARN(LOG_CODE_ULTRASONIC1_TIMEOUT, 1U);
    dist1 = 0xFFFFU;
}
```

Trace the path where a message arrives but carries the **wrong sensor id** — for instance a late echo from sensor 2 landing in the queue while Thread 7 is waiting for sensor 1:

```
   dist1 = 0xFFFF                      (initial sentinel)
   xQueueReceive succeeds
   (msg1 >> 16) == 1, not 0            → dist1 NOT updated, stays 0xFFFF
   if (dist1 > 400)                    → 0xFFFF > 400 is TRUE
       dist1 = 400                     ◄── the sentinel is clamped to 400 cm
   LOG_INFO(ULTRASONIC1_TIMEOUT, 1)    ◄── and reported as HEALTHY
```

The frame goes out claiming a confident 4.00 m reading from a sensor that produced nothing. The gateway has no way to tell this from a genuine 4 m measurement.

The fix is to move the clamp inside the id check:

```c
if ((msg1 >> 16U) == 0U) {
    dist1 = (uint16_t)(msg1 & 0xFFFFU);
    if (dist1 > 400U) { dist1 = 400U; }
    LOG_INFO(LOG_CODE_ULTRASONIC1_TIMEOUT, 1U);
} else {
    LOG_WARN(LOG_CODE_ULTRASONIC1_TIMEOUT, 1U);   /* wrong sensor answered */
}
```

**How likely is the mismatch?** `xQueueReset(g_ultrasonic_queue)` is called immediately before each trigger, which discards stale messages. The window is: reset → trigger sensor 1 → sensor 2's echo (from a previous cycle, or a reflection) completes and posts. Given the 250 ms period and 38 ms maximum echo time, a genuinely late sensor-2 echo cannot survive a full cycle. The realistic trigger is **crosstalk** — sensor 1's burst reflecting into sensor 2's receiver, causing an unexpected CH2 capture. HC-SR04s are notorious for this when mounted close together.

So the bug is reachable in exactly the situation where accurate readings matter most. Recorded in [§82](#82-known-gaps).

### 30.4 Sequential triggering avoids crosstalk — mostly

Firing both sensors simultaneously would guarantee cross-detection. Firing them sequentially, each with a 50 ms wait, means sensor 1's burst has 50 ms to decay before sensor 2 fires. Sound travels 17 m in 50 ms, so any echo path longer than that has attenuated below the detection threshold.

The cost is that both readings are taken 50–100 ms apart rather than simultaneously, and the frame timestamp reflects neither — it is taken after both:

```c
uint32_t ts = (uint32_t)(xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS);
```

At vehicle speeds that skew matters: 50 ms at 50 km/h is 70 cm of travel. The two distances in one frame are not contemporaneous, and nothing in the frame says so.

### 30.5 The 10 µs trigger pulse

```c
void HCSR04_Trigger(const HCSR04_Config_t *cfg){
    GPIO_WritePin(cfg->trigger_port, cfg->trigger_pin, GPIO_PIN_SET);

    /* Very basic blocking delay for ~10us at 84MHz */
    for (volatile uint32_t i = 0; i < 1000U; i++) { }

    GPIO_WritePin(cfg->trigger_port, cfg->trigger_pin, GPIO_PIN_RESET);
}
```

A `volatile` loop of 1,000 iterations. At `-O2` on Cortex-M4, a volatile loop counter forces load/increment/store/compare/branch — roughly 5–6 cycles per iteration:

```
   1000 × 5.5 cycles ≈ 5,500 cycles at 84 MHz ≈ 65 µs
```

The comment claims ~10 µs; the actual pulse is closer to **65 µs**. The HC-SR04 datasheet specifies a *minimum* of 10 µs and does not specify a maximum, so a longer pulse works — the sensor triggers on the falling edge. No functional problem, but the comment is wrong by 6×.

More importantly, this is a **busy-wait at priority 1 with interrupts enabled**, executed twice per 250 ms cycle = 130 µs of pure spinning per cycle, or 0.05 % CPU. Negligible.

`DWT_DelayUs(10)` is available in this codebase and would be both accurate and self-documenting. Recorded in [§82](#82-known-gaps).

### 30.6 The 400 cm cap

The HC-SR04's specified range is 2–400 cm. Readings above 400 are either noise or an echo from a distant surface arriving after the sensor's internal timeout. Clamping rather than rejecting means the gateway sees a saturated value rather than a gap — which is the right choice for a distance sensor, where "at least 4 m" is more useful than "unknown".

The `0xFFFF` sentinel means genuinely no echo. So the value space is:

| Value | Meaning |
|---|---|
| 2–399 | Measured distance in cm |
| 400 | At or beyond maximum range (**or** the mismatched-id bug, [§30.3](#303-a-real-bug-a-mismatched-id-reports-400-cm)) |
| `0xFFFF` | No echo within 50 ms — sensor disconnected or failed |

## 31. Logger Task

**Name** `"Logger"` · **Priority** 0 · **Stack** 256 words (1,024 B) · **Wake** `s_logger_queue` · **Watchdog** not monitored

```c
static void Logger_Task(void *arg)
{
    Log_Payload_t  log_entry;
    FrameRequest_t req;

    for (;;)
    {
        if (xQueueReceive(s_logger_queue, &log_entry, portMAX_DELAY) == pdTRUE)
        {
            req.type   = FRAME_TYPE_LOG;
            req.ecu_id = ECU_ID_STM32_NODE1;
            req.length = LOG_PAYLOAD_SIZE;      /* 10 */

            req.payload[0] = log_entry.code;
            req.payload[1] = log_entry.severity;
            req.payload[2] = (uint8_t)(log_entry.timestamp_ms >> 24);
            /* ... big-endian timestamp and aux_data ... */

            if (xQueueSend(g_frame_queue, &req, 0U) != pdTRUE) {
                s_forward_drop_count++;
            } else {
                s_last_severity[log_entry.code] = log_entry.severity;
            }

            s_is_pending[log_entry.code] = 0U;

            /* Enforce strict rate limit: 1 log every 4 seconds maximum */
            vTaskDelay(pdMS_TO_TICKS(4000));
        }
    }
}
```

### 31.1 The 4-second rate limit

The `vTaskDelay(4000)` at the bottom of the loop is the hardest constraint in the diagnostics design. **The node emits at most one log frame every four seconds, system-wide.**

```
   t=0.000  entry A dequeued, forwarded, s_is_pending[A] cleared
   t=0.000  vTaskDelay(4000) begins
   t=0.001  entries B, C, D queued by other tasks — they sit in s_logger_queue
   t=4.000  Logger wakes, dequeues B, forwards
   t=8.000  dequeues C
   t=12.000 dequeues D
```

With a 16-entry queue, a burst of 16 events takes **64 seconds** to drain. During that time `s_is_pending[code]` stays set for each queued code, so repeats are dropped rather than accumulating — the queue cannot grow past 16 distinct codes, and there are only 16 codes defined ([§74](#74-log-code-taxonomy)). So the queue can hold exactly one of each code at most, which is a neat closure.

Why so aggressive? Log frames compete with telemetry for the same UART and the same 6-deep `g_frame_queue`. A fault that fires at 100 Hz — say `RING_BUFFER_DROP` on every sample — would, without the limit, saturate the queue and starve the classification frames. The rate limit guarantees telemetry bandwidth.

The cost is latency: a fault that occurs while three other events are queued is reported up to 12 seconds late, with a `timestamp_ms` that correctly reflects when it *happened* rather than when it was sent. The timestamp is captured in `Logger_Log`, not in the task:

```c
entry.timestamp_ms = (uint32_t)xTaskGetTickCount();
```

So the gateway can reconstruct the true ordering and timing even though delivery is delayed. That is the detail that makes the rate limit acceptable.

### 31.2 `s_is_pending` clearing order

```c
if (xQueueSend(g_frame_queue, &req, 0U) != pdTRUE) { s_forward_drop_count++; }
else { s_last_severity[log_entry.code] = log_entry.severity; }

s_is_pending[log_entry.code] = 0U;
```

`s_last_severity` is updated **only on successful forward**. If `g_frame_queue` was full, the severity is not recorded as emitted — so the next occurrence of the same code and severity will be treated as a state change and retried. That is correct: a dropped log should not suppress the next one.

`s_is_pending` is cleared unconditionally, which reopens the code for queueing regardless. Also correct.

### 31.3 What the Logger does not do

- **It is not watchdog-monitored.** A wedged Logger silently stops all logging. Since logging is diagnostic rather than functional, that is defensible — but the loss would be invisible.
- **It cannot log its own failures.** `s_drop_count` (logger queue full) and `s_forward_drop_count` (frame queue full) are `volatile` file-scope counters readable only by a debugger. Neither is transmitted.
- **It has no severity filter.** Every accepted entry is forwarded regardless of severity. With a 4-second budget, a flood of INFOs can crowd out an ERROR — though the debounce logic makes a flood of INFOs nearly impossible.

## 32. Idle Hook and the Watchdog Supervisor

```c
void vApplicationIdleHook(void){
    IWDG_SupervisorFeed();
}
```

Two lines that carry the node's entire failure-recovery strategy.

```c
IWDG_Status_t IWDG_SupervisorFeed(void)
{
    if ((IWDG_Thread1_Alive != 0U) &&
        (IWDG_Thread2_Alive != 0U) &&
        (IWDG_Thread3_Alive != 0U) &&
        (IWDG_Thread4_Alive != 0U))
    {
        IWDG_Thread1_Alive = 0U;
        IWDG_Thread2_Alive = 0U;
        IWDG_Thread3_Alive = 0U;
        IWDG_Thread4_Alive = 0U;
        IWDG_RELOAD();
        return IWDG_OK;
    }
    return IWDG_THREAD_HANG;
}
```

```
   ┌──────────────────────────────────────────────────────────────────┐
   │                    THE SUPERVISOR CONTRACT                       │
   ├──────────────────────────────────────────────────────────────────┤
   │                                                                  │
   │   Thread 1 ──► IWDG_Thread1_Alive = 1   (every 10 ms)            │
   │   Thread 2 ──► IWDG_Thread2_Alive = 1   (every wake, 100 Hz)     │
   │   Thread 3 ──► IWDG_Thread3_Alive = 1   (every frame sent)       │
   │   Thread 4 ──► IWDG_Thread4_Alive = 1   (every 1 s, twice)       │
   │                          │                                       │
   │                          ▼                                       │
   │   IDLE task ──► vApplicationIdleHook() ──► IWDG_SupervisorFeed() │
   │                          │                                       │
   │            all four set? ├── yes ──► clear all four, IWDG->KR    │
   │                          │                        = 0xAAAA       │
   │                          └── no  ──► DO NOT FEED                 │
   │                                        │                         │
   │                                        ▼                         │
   │                              IWDG expires → MCU reset            │
   └──────────────────────────────────────────────────────────────────┘
```

### 32.1 Why the idle hook and not a timer

The distinction is the whole point, and `IWDG_INTERFACE.h` states it explicitly:

```c
 *   // ❌ WRONG — do NOT call from a timer ISR:
 *   // void TIM2_IRQHandler(void) { IWDG_Refresh(); }
 *   // ISR fires even when main logic is deadlocked → watchdog never triggers.
```

A timer ISR keeps running when every task is deadlocked. Feeding from there produces a watchdog that only detects a total clock or core failure — nearly useless. Feeding from the idle hook requires that:

1. The scheduler is running.
2. No task is spinning at any priority (or the idle task never gets a slice).
3. All four monitored tasks have completed a full loop iteration.

All three must hold, continuously, or the node resets.

### 32.2 The timing budget

```
   IWDG_Init(3000U)  with the default 32 kHz LSI assumption

   IWDG_CalculateTimeout tries prescalers smallest-first:
     /4   → reload = 3000 × 32000 / (4 × 1000)  = 24000  > 4095  ✗
     /8   → reload = 12000                      > 4095  ✗
     /16  → reload =  6000                      > 4095  ✗
     /32  → reload =  3000                      ≤ 4095  ✓

   Selected: prescaler /32, reload 3000

   Actual timeout = 32 × 3000 / f_LSI
     at f_LSI = 32 kHz (nominal)  →  3.00 s
     at f_LSI = 47 kHz (max)      →  2.04 s   ◄── the number that matters
     at f_LSI = 17 kHz (min)      →  5.65 s
```

The LSI is an uncalibrated RC oscillator with a specified range of 17–47 kHz. **Design against 2.04 s**, not 3.00 s.

Against that budget:

| Monitored task | Sets its flag | Margin at 2.04 s |
|---|---|---|
| Thread 1 | every 10 ms | 204× |
| Thread 2 | every wake (~10 ms) | 204× |
| Thread 3 | every frame sent | see below |
| Thread 4 | every 1,000 ms | 2.04× |

**Thread 3 is the tight one, and it is coupled to Thread 7.** Thread 3 only sets its flag after successfully transmitting a frame. Frames arrive from:

```
   Thread 7 (ultrasonic)     every 250 ms   ◄── the pacemaker
   Thread 2 (classification) every 250 ms, gated by the 9-vote window
   Thread 6 (temperature)    every 2,000 ms
   Thread 4 (heartbeat)      every 5,000 ms
```

Thread 7's 250 ms cadence is what guarantees Thread 3 runs at least 8 times per watchdog period. If Thread 7 stopped — and Thread 7 is **not** watchdog-monitored — the frame rate would fall to Thread 2's 4 Hz, still fine. If Thread 2 *also* stopped producing (which would trip its own flag first), the fallback is Thread 6 at 0.5 Hz = one frame per 2 s, which is right at the 2.04 s worst-case boundary.

So there is a scenario — Threads 2 and 7 both silent, Thread 6 the only producer — in which Thread 3's liveness margin is essentially zero. It is a contrived scenario (Thread 2 stopping would trip its own flag), but the coupling is real: **Thread 3's watchdog liveness depends on other tasks producing work.**

A cleaner design would have Thread 3 use a bounded `xQueueReceive` timeout and set its flag on timeout as well as on transmission:

```c
if (xQueueReceive(g_frame_queue, &req, pdMS_TO_TICKS(500)) != pdTRUE) {
    IWDG_Thread_SetAlive(&IWDG_Thread3_Alive);   /* alive, just idle */
    continue;
}
```

Recorded in [§82](#82-known-gaps).

Thread 4's 2.04× margin is the other one to watch. It is adequate but not generous — a 1-second period against a 2.04-second worst-case timeout leaves room for exactly one missed iteration.

### 32.3 Which tasks are *not* covered

| Task | Monitored? | Consequence of a silent hang |
|---|---|---|
| Thread 5 (BL_RX) | No | Node becomes un-enterable remotely; telemetry continues |
| Thread 6 (TEMP) | No | Temperature frames stop; nothing else affected |
| Thread 7 (ULTRA) | No | Distance frames stop; Thread 3's liveness margin shrinks |
| Logger | No | All logging stops silently |

The four unmonitored tasks are all "nice to have" rather than core. That is a defensible line — but Thread 7's role in Thread 3's liveness ([§32.2](#322-the-timing-budget)) means it is more load-bearing than its unmonitored status suggests.

`IWDG_INTERFACE.h` still documents only three flags:

```c
 * Ownership:
 *   IWDG_Thread1_Alive — set by Thread 1 after each I2C DMA sensor read
 *   IWDG_Thread2_Alive — set by Thread 2 after each inference completes
 *   IWDG_Thread3_Alive — set by Thread 3 after each UART frame is transmitted
```

A fourth (`IWDG_Thread4_Alive`) was added to both the header's `extern` block and the supervisor's condition, but not to this comment. Minor, but it is the kind of drift that makes a reader distrust the rest.

### 32.4 Failure preserves evidence

```c
else
{
    status = IWDG_THREAD_HANG;      /* flags NOT cleared */
}
```

On a failed feed the flags are left as they are. After the reset, `.bss` is zeroed, so the evidence is gone — the comment in the header says the flags are preserved "for post-reset diagnosis", which is only true if a debugger is attached and halts before the reset completes.

Preserving them across the reset would require placing them in a `.noinit` section like the boot flag. That, plus recording *which* flag was clear, would turn "the node reset" into "the node reset because Thread 3 stalled" — a large diagnostic improvement for about ten lines. Recorded in [§82](#82-known-gaps).

### 32.5 The IWDG survives software resets

```c
 * @note Once started, the watchdog CANNOT be stopped except by a power cycle.
 *       NVIC_SystemReset() does NOT stop the IWDG — it survives software reset.
```

This is architecturally true on STM32F4 and has a direct consequence for the OTA path. When Thread 5 triggers `SYSRESETREQ` to enter the bootloader:

```
   Application running, IWDG active with a 2.04 s worst-case timeout
        │
        ▼
   Thread 5: *(0x2000FFF8) = 0xDEADBEEF; SCB->AIRCR = SYSRESETREQ
        │
        ▼
   ═══ system reset ═══   IWDG keeps counting; the reset does not stop it
        │
        ▼
   Bootloader starts, stays resident
        │
        │  ◄── the bootloader NEVER feeds the IWDG
        ▼
   ~2 seconds later: IWDG expires → another reset
        │
        ▼
   Bootloader starts again — but the boot flag was cleared on the first
   boot, so this time it JUMPS TO THE APPLICATION.
```

**This would make the node impossible to hold in the bootloader.** It does not happen, and the reason is worth pinning down precisely: the IWDG is only *started* by `IWDG_Start()`, and once started, only a power-on reset (not `SYSRESETREQ`) clears the `IWDG_KR` enable state on this family.

The empirical evidence is that OTA works — operators do hold the node in the bootloader for the ~30 seconds an update takes. So either:

1. The reset path in question is a power cycle rather than `SYSRESETREQ` in practice, or
2. `SYSRESETREQ` on this part *does* reset the IWDG enable state, contrary to the header's claim.

RM0368 §21.3 states the IWDG "once started, cannot be stopped except by a reset" — and the STM32F4 reference manual's reset section lists the IWDG as being reset by a system reset. The header's note appears to be describing the *watchdog reset* case (an IWDG-triggered reset does not disable the IWDG) rather than the software-reset case.

**This should be verified on hardware**, because if the header is right, every OTA longer than ~2 seconds would be interrupted by a spurious reset — and OTAs demonstrably work. The test is one line: enter the bootloader, wait 10 seconds, send `AA EB` again and see whether the reply is `0xEE 0xFB` (still resident) or `0xEE 0xAA` (the application answered, meaning a reset occurred). Recorded in [§82](#82-known-gaps).
---

# Part V — The TinyML Pipeline

## 33. Pipeline Overview

Eight stages, all inside Thread 2, running once per 25 new samples (4 Hz).

```
   MPU6050 @ 100 Hz  (or replay_data.h — see §43)
        │  14 bytes per sample: ax ay az temp gx gy gz, big-endian int16
        ▼
  ┌───────────────────────────────────────────────────────────┐
  │ 1. RING BUFFER — 128 × 14 B, SPSC lock-free               │  §35
  │    Producer: Thread 1     Consumer: Thread 2              │
  └────────────────────────┬──────────────────────────────────┘
                           │ PeekWindow(50)  — tail not advanced
                           ▼
  ┌───────────────────────────────────────────────────────────┐
  │ 2. FLATTEN — struct-of-7 → int16 flat_window[50][6]       │
  │    Drops temp_raw. Reorders nothing.                      │
  └────────────────────────┬──────────────────────────────────┘
                           ▼
  ┌───────────────────────────────────────────────────────────┐
  │ 3. SCALE — int16 → float32 physical units                 │  §36
  │    accel × (1/8192)   → g                                 │
  │    gyro  × (1/65.5)   → °/s                               │
  │    → scaled[50][6] float32                                │
  └───────┬───────────────────────────────────────┬───────────┘
          │                                       │
          ▼                                       ▼
  ┌────────────────────────────────┐   ┌──────────────────────────┐
  │ 4. FEATURE EXTRACTION           │   │ 6b. QUANTISE TIME-SERIES │  §39
  │    50 float32 statistics        │   │     scaled → int8 [300]  │
  │    incl. 6 × 64-point RFFT      │   │     s=0.189672  z=-6     │
  │    → features[50]               │   └────────────┬─────────────┘
  └────────────────┬────────────────┘                │
                   ▼                                 │
  ┌────────────────────────────────┐                 │
  │ 5. Z-SCORE NORMALISE (in place) │  §38            │
  │    (x - mean[i]) / std[i]       │                 │
  └────────────────┬────────────────┘                 │
                   ▼                                 │
  ┌────────────────────────────────┐                 │
  │ 6a. QUANTISE STATISTICS         │  §39            │
  │     → stat_q[50] int8           │                 │
  │     s=0.057328  z=-53           │                 │
  └────────────────┬────────────────┘                 │
                   │                                  │
                   └───────────┬──────────────────────┘
                               ▼
  ┌───────────────────────────────────────────────────────────┐
  │ 7. CUBEAI INFERENCE                                        │  §40
  │    inputs[0] = stat_q  50 int8                             │
  │    inputs[1] = ts_q   300 int8                             │
  │    outputs[0] = 2 int8 softmax                             │
  │    argmax → label;  margin → confidence 0..100             │
  └────────────────────────┬──────────────────────────────────┘
                           ▼
  ┌───────────────────────────────────────────────────────────┐
  │ 8. TEMPORAL VOTING — 9-deep circular history               │  §41
  │    majority; ties favour SMOOTH                            │
  └────────────────────────┬──────────────────────────────────┘
                           ▼
                  FrameRequest_t (type 0x01) → g_frame_queue
                           │
                           ▼
                  RingBuffer_Advance(25)   ← 50 % overlap
```

Per-window data volumes:

| Stage | Representation | Bytes |
|---|---|---|
| Ring buffer window | 50 × `MPU6050_RawData_t` | 700 |
| Flattened | 50 × 6 × `int16` | 600 |
| Scaled | 50 × 6 × `float32` | 1,200 |
| Features | 50 × `float32` | 200 |
| Quantised TS | 300 × `int8` | 300 |
| Quantised stat | 50 × `int8` | 50 |
| Model output | 2 × `int8` | 2 |
| Frame payload | 6 bytes | 6 |

A 700-byte window becomes a 6-byte frame — a compression ratio of about 117:1, which is the point of doing inference at the edge rather than streaming raw IMU data to the cloud.

## 34. Stage 1 — Acquisition

Covered operationally in [§24](#24-thread-1--sensor-producer). What matters for the pipeline is the sample format.

```c
typedef struct {
    sint16_t accel_x;   /* raw counts, ±32768 at ±4g full scale       */
    sint16_t accel_y;
    sint16_t accel_z;
    sint16_t temp_raw;  /* on-die temperature — read but never used   */
    sint16_t gyro_x;    /* raw counts, ±32768 at ±500 °/s full scale  */
    sint16_t gyro_y;
    sint16_t gyro_z;
} MPU6050_RawData_t;    /* 14 bytes, no padding (all int16)           */
```

`temp_raw` is carried through the ring buffer and then dropped at the flatten step. It costs 2 bytes per sample × 128 slots = 256 bytes of RAM to transport data nothing consumes. Keeping it means the struct matches the sensor's 14-byte burst exactly, which is worth more than 256 bytes.

### 34.1 The axis remap

`MPU6050_ParseRaw` does not pass the raw axes straight through:

```c
sint16_t raw_ax = (sint16_t)(((uint16_t)raw[0U]  << 8U) | (uint16_t)raw[1U]);
sint16_t raw_ay = (sint16_t)(((uint16_t)raw[2U]  << 8U) | (uint16_t)raw[3U]);
sint16_t raw_az = (sint16_t)(((uint16_t)raw[4U]  << 8U) | (uint16_t)raw[5U]);
sint16_t raw_gx = (sint16_t)(((uint16_t)raw[8U]  << 8U) | (uint16_t)raw[9U]);
sint16_t raw_gy = (sint16_t)(((uint16_t)raw[10U] << 8U) | (uint16_t)raw[11U]);
sint16_t raw_gz = (sint16_t)(((uint16_t)raw[12U] << 8U) | (uint16_t)raw[13U]);

out->accel_x =  raw_az;
out->accel_y =  raw_ay;
out->accel_z =  raw_ax;

out->gyro_x  =  raw_gz;
out->gyro_y  =  raw_gy;
out->gyro_z  =  raw_gx;

out->temp_raw = (sint16_t)(((uint16_t)raw[6U] << 8U) | (uint16_t)raw[7U]);
```

```
   Sensor frame          Logical frame
   ────────────          ─────────────
   raw_ax  ──────────►   accel_z     (board +X points UP)
   raw_ay  ──────────►   accel_y
   raw_az  ──────────►   accel_x

   raw_gx  ──────────►   gyro_z
   raw_gy  ──────────►   gyro_y
   raw_gz  ──────────►   gyro_x
```

An X↔Z swap on both accelerometer and gyroscope, with Y unchanged. The determinant of that permutation is −1, so it is a **reflection, not a rotation** — the logical frame is left-handed if the sensor frame was right-handed. For a model trained on the same remapped data that is harmless (the model never sees a cross product); it would matter if anyone tried to integrate the gyro into an attitude estimate.

**The comment block above this code contradicts itself.** It contains two different remap descriptions:

```c
 *  Remap so logical +Z = UP (away from road):
 *    logical_X =  raw_X
 *    logical_Y =  raw_Z
 *    logical_Z = -raw_Y   (negate so +Z = up = +1g at rest)
```

then, immediately below:

```c
 *  logical_X =  raw_Z
 *  logical_Y =  raw_Y
 *  logical_Z =  raw_X   (+X points up = +Z up = +1g)
```

The **second** block matches the code. The first is stale, and it describes a *different* mounting orientation with a sign inversion that the code does not perform. Anyone reading the first block would conclude that `accel_z` is negated; it is not.

The sanity check is simple: at rest, `accel_z` should read approximately **+8192** (one g at ±4g full scale). If it reads −8192, the board is mounted upside down relative to what the model expects. If it reads near zero while `accel_x` reads +8192, the remap is not being applied at all.

### 34.2 Sensor configuration and its effect on the data

| Register | Value | Effect |
|---|---|---|
| `PWR_MGMT_1` (0x6B) | `0x80` then `0x01` | Device reset, then wake with CLKSEL = 1 (PLL, X-gyro reference) |
| `ACCEL_CONFIG` (0x1C) | `0x08` | AFS_SEL = 1 → ±4 g → **8192 LSB/g** |
| `GYRO_CONFIG` (0x1B) | `0x08` | FS_SEL = 1 → ±500 °/s → **65.5 LSB/(°/s)** |
| `CONFIG` (0x1A) | `0x03` | DLPF_CFG = 3 → **44 Hz** bandwidth, 4.9 ms group delay |
| `SMPLRT_DIV` (0x19) | `0x09` | Internal rate 1 kHz ÷ (1+9) = **100 Hz** |

Two of these deserve comment.

**The 44 Hz DLPF against a 100 Hz sample rate** gives a Nyquist frequency of 50 Hz with the filter cutoff at 44 Hz — only 6 Hz of transition band. Content between 44 and 50 Hz is attenuated but present; content above 50 Hz aliases. The MPU6050's DLPF is a single-pole filter with a gentle roll-off, so aliasing of road vibration above 50 Hz into the 0–50 Hz band is real.

Whether that matters depends on the model. Since the FFT-based HFE features look at bins 16–32 of a 64-point transform — which at a 100 Hz sample rate is 25–50 Hz — the aliased content lands squarely in the band the model uses most. If the training data was collected with the identical configuration (it was — the CSV metadata in `replay_data.h` records `accel_range = 4g`, `gyro_range = 500dps`, `sample_rate_hz = 100`), the aliasing is *consistent* between training and inference and the model has learned to work with it. That is the saving grace, and it is a strong argument against ever changing the DLPF setting without retraining.

**The 4.9 ms group delay** means every sample is a filtered view of the acceleration 4.9 ms earlier. Constant, so it shifts the whole time series uniformly and no feature is affected.

**`CLKSEL = 1` versus the comment.** The code writes `0x01` to `PWR_MGMT_1`:

```c
cfg_buf[1U] = 0x01U;   /* SLEEP=0, CLKSEL=0 */
```

`0x01` sets CLKSEL = **1** (PLL with X-axis gyroscope reference), not 0 (internal 8 MHz oscillator). The comment is wrong; the code is right — InvenSense explicitly recommends CLKSEL 1–3 over 0 for stability, and the datasheet notes the internal oscillator has poorer temperature drift.

## 35. Stage 2 — Ring Buffer

The decoupling layer between the 100 Hz producer and the 4 Hz consumer. Full API in [§71](#71-ring-buffer-module); this section covers its role in the pipeline.

```
   RING_BUFFER_CAPACITY = 128      (power of two — masking, not modulo)
   sizeof(MPU6050_RawData_t) = 14
   Storage = 128 × 14 = 1,792 B + head + tail = 1,800 B

   At 100 Hz: 128 samples = 1.28 seconds of absorbed jitter
```

```
   head ──────────────────────────────► written ONLY by RingBuffer_Push (Thread 1)
   tail ──────────────────────────────► written ONLY by Pop / Advance  (Thread 2)

   ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬─── ... ───┬───┐
   │ 0 │ 1 │ 2 │...│   │   │   │   │   │   │           │127│
   └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴─── ... ───┴───┘
             ▲                       ▲
            tail                    head
             └────── count = head - tail ──────┘
                     (unsigned subtraction — wraps correctly)
```

### 35.1 Peek-then-advance, not pop

Thread 2 uses a two-call pattern that a plain FIFO cannot express:

```c
RingBuffer_PeekWindow(window, WINDOW_SIZE);   /* copy 50, tail unchanged */
/* ... process ... */
RingBuffer_Advance(WINDOW_STRIDE);            /* tail += 25 */
```

Peek copies 50 samples without consuming; Advance consumes only 25. The 25 that were peeked but not advanced are read **again** in the next window — which is exactly the 50 % overlap the model was trained with ([§42](#42-windowing-and-stride)).

A `Pop`-based design would need Thread 2 to maintain its own 25-sample carry-over buffer. Peek/Advance pushes that bookkeeping into the ring buffer, where the indices already exist.

### 35.2 The memory barriers

```c
/* Push, after writing the slot */
__asm volatile ("dmb" ::: "memory");
ring_buffer_instance.head = local_head + 1U;
```

```c
/* Advance, before publishing tail */
__asm volatile ("dmb" ::: "memory");
ring_buffer_instance.tail = local_tail + n;
```

Two barriers, each guarding a publish:

| Barrier | Prevents |
|---|---|
| Push: after data, before `head++` | The consumer seeing an incremented head while the slot data is still in a store buffer |
| Advance: after reads, before `tail += n` | The producer seeing freed slots and overwriting them while the consumer's reads are still in flight |

`"memory"` is the compiler barrier (stops GCC reordering across it); `dmb` is the CPU barrier (stops the store buffer reordering). **Both are needed** — the module's header says so explicitly, and it is right. On Cortex-M4 the store buffer is shallow and stores to Device/Strongly-Ordered memory are not reordered, but SRAM is Normal memory and *is* subject to reordering.

`PeekWindow` correctly uses **no** barrier:

```c
/* ---- 6) No DMB and no tail update ----
 *   PeekWindow is a pure read. There is no publishing event,
 *   so no memory barrier is required. */
```

Pure reads with no subsequent publish need no ordering guarantee. This is the kind of detail that is usually got wrong in the conservative direction (barrier everywhere); getting it right saves a barrier on the hot path.

### 35.3 Drop, not overwrite

```c
if ((local_head - local_tail) >= RING_BUFFER_CAPACITY)
{
    /* Buffer full — DROP the sample, do not overwrite oldest. */
    if (rb_drop_count < 0xFFFFFFFFUL) { rb_drop_count++; }
    return RING_BUFFER_FULL;
}
```

The alternative — overwriting the oldest — would corrupt a window that Thread 2 is mid-way through peeking. Dropping the newest keeps every window that has been handed out internally consistent.

The trade is a hole in the time series rather than a corrupted one. For a model that reads statistical features over a window, a missing sample shifts the window by one and slightly perturbs every feature; a *torn* sample would inject an impulse that the HFE features would read as high-frequency road content. Dropping is clearly better.

Overflow can only happen if Thread 2 falls more than 1.28 seconds behind — at which point `rb_max_fill` in the heartbeat is already reading 128 and `LOG_CODE_RING_BUFFER_DROP` has fired.

## 36. Stage 3 — Scaling

```c
void Scale_RawWindow(const int16_t   raw_in    [WINDOW_SIZE][N_FEATURES],
                           float32_t scaled_out[WINDOW_SIZE][N_FEATURES])
{
    const float32_t inv_accel_scale = 1.0f / ACCEL_SCALE;  /* 1 / 8192.0f */
    const float32_t inv_gyro_scale  = 1.0f / GYRO_SCALE;   /* 1 / 65.5f   */

    for (uint32_t t = 0U; t < (uint32_t)WINDOW_SIZE; ++t)
    {
        scaled_out[t][0] = ((float32_t)raw_in[t][0]) * inv_accel_scale;
        scaled_out[t][1] = ((float32_t)raw_in[t][1]) * inv_accel_scale;
        scaled_out[t][2] = ((float32_t)raw_in[t][2]) * inv_accel_scale;
        scaled_out[t][3] = ((float32_t)raw_in[t][3]) * inv_gyro_scale;
        scaled_out[t][4] = ((float32_t)raw_in[t][4]) * inv_gyro_scale;
        scaled_out[t][5] = ((float32_t)raw_in[t][5]) * inv_gyro_scale;
    }
}
```

Raw counts become physical units:

```
   accel_raw / 8192.0  →  g       (range ±4 g)
   gyro_raw  / 65.5    →  °/s     (range ±500 °/s)
```

### 36.1 Reciprocal multiplication

```c
const float32_t inv_accel_scale = 1.0f / ACCEL_SCALE;
```

computed once outside the loop, then multiplied 150 times. On Cortex-M4F:

| Instruction | Latency |
|---|---|
| `VMUL.F32` | 1 cycle |
| `VDIV.F32` | **14 cycles** |

300 divisions replaced by 300 multiplications plus 2 divisions saves roughly 3,900 cycles per window — about 46 µs at 84 MHz, or 0.02 % of the 250 ms budget.

The saving is negligible in this context. What makes it worth doing anyway is that it is free: the compiler would do it under `-ffast-math`'s `-freciprocal-math` regardless, and writing it explicitly documents the intent and makes the code independent of that flag.

**A numerical caveat.** `1.0f / 8192.0f` is exact — 8192 is a power of two, so the reciprocal is `2^-13` with no rounding. `1.0f / 65.5f` is **not** exact; it rounds to the nearest float32. So `x * (1/65.5f)` and `x / 65.5f` can differ by one ULP.

Does that break parity with Python? The training pipeline uses `raw / 65.5` in NumPy float64, then casts to float32. The difference is at most 1 ULP of float32 (about 6 × 10⁻⁸ relative), which propagates through the statistics and is utterly swamped by the int8 quantisation step ([§39](#39-stage-6--quantisation)) whose resolution is 0.19 in scaled units. So the answer is no — but it is the right question to have asked, and the reason the answer is no is quantisation, not exactness.

### 36.2 The channel ordering is a contract

```
   index 0 1 2 = accel x y z
   index 3 4 5 = gyro  x y z
```

This ordering appears in five places and must agree in all of them:

1. `Thread2_TinyML`'s flatten loop
2. `Scale_RawWindow`'s per-channel divisor choice
3. `Features_Extract`'s de-interleave (`col[0..5]`)
4. `Quantize_TS`'s row-major packing
5. The trained model's `ts_input` tensor layout

There is no compile-time check tying these together. A reordering in one place would produce a model fed with gyro data where it expects accelerometer data — which would not crash, would not warn, and would simply classify badly. Recorded in [§82](#82-known-gaps).

## 37. Stage 4 — Feature Extraction

`Features_Extract()` produces the 50-element statistical vector. It is the most compute-intensive stage and the most carefully written module in the codebase.

### 37.1 The actual feature layout

```c
for (ch = 0U; ch < (uint32_t)N_FEATURES; ++ch)
{
    const uint32_t base = 6U * ch;

    features_out[base + 0U] = feat_std(col[ch], WINDOW_SIZE);
    features_out[base + 1U] = feat_mad(col[ch], WINDOW_SIZE);
    features_out[base + 2U] = feat_p2p(col[ch], WINDOW_SIZE);
    features_out[base + 3U] = feat_hfe_channel(scaled, ch);
    features_out[base + 4U] = feat_iqr(col[ch], WINDOW_SIZE);
    features_out[base + 5U] = feat_rms(col[ch], WINDOW_SIZE);
}
```

`base = 6 * ch` means the vector is grouped **by channel**, with six statistics per channel:

| Index | Content |
|---|---|
| 0–5 | ax: std, MAD, P2P, HFE, IQR, RMS |
| 6–11 | ay: std, MAD, P2P, HFE, IQR, RMS |
| 12–17 | az: std, MAD, P2P, HFE, IQR, RMS |
| 18–23 | gx: std, MAD, P2P, HFE, IQR, RMS |
| 24–29 | gy: std, MAD, P2P, HFE, IQR, RMS |
| 30–35 | gz: std, MAD, P2P, HFE, IQR, RMS |
| 36–39 | amag: std, MAD, P2P, RMS |
| 40–43 | gmag: std, MAD, P2P, RMS |
| 44 | corrcoef(amag, gmag) |
| 45 | corrcoef(az, gz) |
| 46 | skewness(az) |
| 47 | skewness(gz) |
| 48 | var(amag[25:]) / var(amag[:25]) |
| 49 | var(gmag[25:]) / var(gmag[:25]) |

> **The earlier `README.md` in this directory documents this layout wrongly.** It shows `[0..5] std(ch)` for all six channels, `[6..11] MAD(ch)` for all six, and so on — grouped by *statistic* rather than by *channel*. That is the transpose of what the code does.
>
> This matters enormously: `stat_mean[]` and `stat_std[]` in `stat_norm.c` are indexed positionally, so a reader who trusted the README and regenerated the scaler in statistic-major order would produce a normaliser that scrambles every feature. The header comment in `features.h` has it **right** (`[0..35] : 6 features x 6 channels`), and so does the code.

### 37.2 The eight statistics

**Standard deviation** — population, `/N`, matching NumPy's default `ddof=0`:

```c
static float32_t feat_var(const float32_t *x, uint32_t n){
    const float32_t m = feat_mean(x, n);
    float32_t s = 0.0f;
    for (uint32_t i = 0U; i < n; ++i){
        const float32_t d = x[i] - m;
        s += d * d;
    }
    return s / (float32_t)n;          /* NOT n-1 */
}
static float32_t feat_std(const float32_t *x, uint32_t n){ return sqrtf(feat_var(x, n)); }
```

Using `n-1` (sample variance) instead would produce values 1.01× larger at n=50 — a 1 % systematic offset on 14 of the 50 features, which the z-score step would partly absorb and the model would partly mis-handle. The choice is not stylistic.

**MAD** — despite the name, this is `mean(|diff(x)|)`, the mean absolute *first difference*, not the mean absolute deviation from the mean:

```c
static float32_t feat_mad(const float32_t *x, uint32_t n){
    if (n < 2U) { return 0.0f; }
    float32_t s = 0.0f;
    for (uint32_t i = 1U; i < n; ++i){
        s += fabsf(x[i] - x[i - 1U]);
    }
    return s / (float32_t)(n - 1U);
}
```

`np.mean(np.abs(np.diff(x)))`. It measures sample-to-sample roughness — a direct proxy for surface texture, and arguably the most physically meaningful feature in the set. The naming collision with the statistical term "median absolute deviation" is unfortunate; the comment above the function disambiguates it.

**Peak-to-peak** — `max - min`, one pass.

**RMS** — `sqrt(mean(x²))`. Note this is not centred, so it includes the DC component. For `accel_z`, which sits at ~1 g at rest, RMS is dominated by gravity and is nearly constant — which makes it a poor discriminator for that channel but a fine one for the others.

**IQR** — 75th minus 25th percentile, with NumPy's linear-interpolation convention:

```c
static float32_t feat_percentile_sorted(const float32_t *sorted_x, uint32_t n, float32_t p){
    const float32_t idx  = (p * 0.01f) * (float32_t)(n - 1U);
    const float32_t flo  = floorf(idx);
    const uint32_t  lo   = (uint32_t)flo;
    uint32_t        hi   = lo + 1U;
    if (hi >= n) { hi = n - 1U; }
    const float32_t frac = idx - flo;
    return sorted_x[lo] + (frac * (sorted_x[hi] - sorted_x[lo]));
}
```

For n = 50 and p = 25: `idx = 0.25 × 49 = 12.25`, so `lo = 12`, `hi = 13`, `frac = 0.25`, and the result is `sorted[12] + 0.25 × (sorted[13] - sorted[12])`. That is exactly `np.percentile(x, 25)` with the default `interpolation='linear'`.

Sorting uses insertion sort on a stack-local copy:

```c
static void feat_insertion_sort(float32_t *a, uint32_t n){
    for (uint32_t i = 1U; i < n; ++i){
        const float32_t key = a[i];
        uint32_t j = i;
        while ((j > 0U) && (a[j - 1U] > key)) { a[j] = a[j - 1U]; --j; }
        a[j] = key;
    }
}
```

O(n²) = 2,500 comparisons worst case, six times per window = 15,000 comparisons. At roughly 4 cycles each that is 60,000 cycles ≈ 715 µs — **the single most expensive operation in the pipeline**, more than the CNN inference. Against a 250 ms budget it is 0.3 %, so it does not matter; but if the window ever grew, this is the first thing that would need replacing (heapsort, or a selection algorithm that finds only the two order statistics needed).

Insertion sort was chosen for good reasons: no recursion (MISRA), no allocation, deterministic timing, and it is fastest of all algorithms for nearly-sorted input — which IMU windows often are.

**Skewness** — third standardised moment, population form:

```c
const float32_t s = sqrtf(var_acc / (float32_t)n);
if (s < EPS_STAB) { return 0.0f; }              /* 1e-10 guard */
const float32_t inv_s = 1.0f / s;
float32_t acc = 0.0f;
for (uint32_t i = 0U; i < n; ++i){
    const float32_t z = (x[i] - m) * inv_s;
    acc += z * z * z;
}
return acc / (float32_t)n;
```

Applied only to `az` and `gz` — the vertical axes, where asymmetry between upward and downward excursions distinguishes potholes (sharp negative) from speed bumps (sharp positive).

**Correlation** — Pearson, with a zero-variance guard:

```c
if ((std_x < EPS_STAB) || (std_y < EPS_STAB)) { return 0.0f; }
return cov / (std_x * std_y);
```

`corrcoef(amag, gmag)` captures whether linear and rotational disturbance move together — a signature of a genuine road event versus sensor noise.

**Variance ratio** — the only feature that looks at *within-window* time structure:

```c
const float32_t var_a_lo = feat_var(&amag[0],           HALF_WINDOW);   /* [0:25]  */
const float32_t var_a_hi = feat_var(&amag[HALF_WINDOW], HALF_WINDOW);   /* [25:50] */
features_out[48] = (var_a_hi + EPS_STAB) / (var_a_lo + EPS_STAB);
```

A ratio near 1 means stationary; far from 1 means the surface changed mid-window. The `+1e-10` on both numerator and denominator is what prevents a division by zero on a perfectly still window — and it matches the Python `(np.var(x[half:]) + 1e-10) / (np.var(x[:half]) + 1e-10)` exactly.

### 37.3 HFE — the FFT feature

```c
static float32_t feat_hfe_channel(const float32_t scaled[WINDOW_SIZE][N_FEATURES],
                                  uint32_t ch)
{
    static float32_t fft_in [FFT_N]     __attribute__((aligned(4)));   /* 64 */
    static float32_t fft_out[FFT_N]     __attribute__((aligned(4)));   /* 64 */
    static float32_t mag_sq [FFT_NBINS];                               /* 33 */

    /* Zero-pad 50 → 64 */
    for (i = 0U; i < WINDOW_SIZE; ++i) { fft_in[i] = scaled[i][ch]; }
    for (i = WINDOW_SIZE; i < FFT_N; ++i) { fft_in[i] = 0.0f; }

    arm_rfft_fast_f32(&s_fft_inst, fft_in, fft_out, 0U);

    mag_sq[0]  = fft_out[0] * fft_out[0];          /* DC      */
    mag_sq[32] = fft_out[1] * fft_out[1];          /* Nyquist */
    for (i = 1U; i < 32U; ++i){
        const float32_t re = fft_out[2U * i];
        const float32_t im = fft_out[(2U * i) + 1U];
        mag_sq[i] = (re * re) + (im * im);
    }

    float32_t total   = EPS_STAB;
    float32_t hfe_sum = 0.0f;
    for (i = 0U; i < FFT_NBINS; ++i)              { total   += mag_sq[i]; }
    for (i = FFT_HF_SPLIT; i < FFT_NBINS; ++i)    { hfe_sum += mag_sq[i]; }

    return hfe_sum / total;
}
```

**HFE is a ratio, not an energy.** The return is `hfe_sum / total` — the *fraction* of spectral energy above bin 16. The earlier README describes it as "sum of |F[k]|² for k in [16..32]", which is `hfe_sum` alone. The ratio form is scale-invariant: a rough road at low speed and the same road at high speed produce different absolute energies but similar ratios.

**The CMSIS-DSP packed output format** is the part most likely to be got wrong:

```
   arm_rfft_fast_f32 output for a 64-point real FFT:

   fft_out[ 0] = Re(X[0])   — DC
   fft_out[ 1] = Re(X[32])  — Nyquist, PACKED into the imaginary slot of DC
   fft_out[ 2] = Re(X[1])
   fft_out[ 3] = Im(X[1])
   fft_out[ 4] = Re(X[2])
   fft_out[ 5] = Im(X[2])
   ...
   fft_out[62] = Re(X[31])
   fft_out[63] = Im(X[31])
```

Both `X[0]` and `X[32]` are purely real for a real input, so CMSIS packs the Nyquist bin's real part where DC's imaginary part would be. The code handles both special cases explicitly and loops over 1…31 for the general case, reconstructing 33 magnitude-squared values — matching `np.fft.rfft`'s `N/2 + 1` output length exactly.

**Frequency mapping:**

```
   Sample rate 100 Hz, FFT length 64 (zero-padded from 50)
   Bin spacing = 100 / 64 = 1.5625 Hz

   bin  0 →  0.00 Hz    (DC)
   bin 16 → 25.00 Hz    ◄── FFT_HF_SPLIT
   bin 32 → 50.00 Hz    (Nyquist)

   HFE = energy in 25–50 Hz  /  energy in 0–50 Hz
```

So HFE measures the fraction of vibration energy in the upper half of the band. Smooth asphalt concentrates energy at low frequency (vehicle body modes, 1–5 Hz); rough gravel excites the 25–50 Hz range through tyre-surface interaction. This is the most physically motivated feature in the set.

**Zero-padding 50 → 64** is required because CMSIS-DSP's radix-4 FFT needs a power-of-two length. It does not add information; it interpolates the spectrum onto a finer grid. Since both numerator and denominator are computed from the same padded spectrum, the ratio is unaffected by the padding's spectral leakage — another benefit of the ratio form.

**No window function is applied.** A rectangular window (which zero-padding effectively imposes) has −13 dB sidelobes, so a strong low-frequency component leaks into the high bins and inflates HFE. A Hann window would reduce that to −31 dB. The Python pipeline does the same thing, so the leakage is consistent between training and inference — and consistency beats correctness here.

### 37.4 The sanitiser

```c
static void feat_sanitize(float32_t *v, uint32_t n){
    for (uint32_t i = 0U; i < n; ++i){
        if (isnan(v[i]))      { v[i] = 0.0f; }
        else if (isinf(v[i])) { v[i] = (v[i] > 0.0f) ? 10.0f : -10.0f; }
    }
}
```

Reproduces `np.nan_to_num(f, nan=0, posinf=10, neginf=-10)`.

Where could a NaN or Inf come from, given the `EPS_STAB` guards?

| Source | Guarded? |
|---|---|
| `feat_corrcoef` zero variance | Yes — returns 0.0f |
| `feat_skewness` zero std | Yes — returns 0.0f |
| Variance ratio, zero denominator | Yes — `+1e-10` on both terms |
| HFE, zero total energy | Yes — `total` starts at `EPS_STAB` |
| **Variance ratio overflow** | **No** — `(large + 1e-10) / (1e-10 + 1e-10)` can reach ~1e10, finite but enormous |
| **`sqrtf` of a negative** | Cannot occur — variance is a sum of squares |

So the sanitiser is genuinely a backstop rather than a load-bearing guard. Its most likely real activation is the variance ratio producing a very large but finite value, which it does **not** catch — only Inf is clamped, not 1e10.

**And it may be optimised away entirely under `-ffast-math`.** See [§13.2](#132--ffast-math-and-its-implications). This is the interaction that most deserves a bench check.

### 37.5 Cost breakdown

Estimated cycles per window at 84 MHz:

| Operation | Count | Est. cycles | Total |
|---|---|---|---|
| `feat_iqr` insertion sorts | 6 | ~10,000 | 60,000 |
| `arm_rfft_fast_f32` (64-point) | 6 | ~3,500 | 21,000 |
| `feat_std`/`feat_var` (2 passes each) | 14 | ~500 | 7,000 |
| `feat_rms` | 8 | ~350 | 2,800 |
| `feat_mad` | 8 | ~300 | 2,400 |
| `feat_corrcoef` | 2 | ~800 | 1,600 |
| `feat_skewness` | 2 | ~700 | 1,400 |
| `feat_p2p` | 8 | ~200 | 1,600 |
| De-interleave + magnitudes | 1 | ~2,500 | 2,500 |
| **Total** | | | **≈ 100,000 cycles ≈ 1.2 ms** |

Against a 250 ms budget: **0.5 % CPU**. Feature extraction is roughly ten times more expensive than the CNN inference itself, and both are negligible. The heartbeat's `cpu_t2_x100` should read in the low hundreds (a few percent).

## 38. Stage 5 — Z-Score Normalisation

```c
void Quantize_NormalizeStat(float32_t features[N_STAT_FEATURES])
{
    for (uint32_t i = 0U; i < (uint32_t)N_STAT_FEATURES; ++i)
    {
        float32_t s = stat_std[i];
        if (s < STD_EPS) { s = 1.0f; }         /* match Python's behaviour */
        features[i] = (features[i] - stat_mean[i]) / s;
    }
}
```

`stat_mean[]` and `stat_std[]` come from `stat_norm.c` — 50 pairs of constants exported from the training pipeline's `StandardScaler`:

```c
const float stat_mean[N_STAT] = {
    0.030802f, 0.031645f, 0.140445f, 0.011278f, 0.040550f,
    0.222039f, 0.047390f, 0.060847f, 0.207018f, 0.470574f,
    ...
};

const float stat_std[N_STAT] = {
    0.015919f, 0.017091f, 0.083069f, 0.021489f, 0.020065f,
    0.046206f, 0.023887f, 0.032218f, 0.114498f, 0.272826f,
    ...
};
```

### 38.1 Why normalise before quantising

The raw features have wildly different scales:

| Feature | Typical magnitude |
|---|---|
| `std(ax)` in g | ~0.03 |
| `P2P(gz)` in °/s | ~8.5 |
| `HFE(ax)` (a ratio) | 0…1 |
| `var ratio` | 0.1…10 |

A single int8 quantisation scale across that range would waste almost all of the 256 levels. Z-scoring maps every feature to roughly N(0,1), after which one scale (`STAT_SCALE = 0.057328`) covers ±7 standard deviations across the full int8 range. That is why the two steps are adjacent and ordered this way.

### 38.2 The zero-std guard

```c
if (s < STD_EPS) { s = 1.0f; }
```

with the comment `/* CHANGE: match Python's behavior */`. `sklearn.preprocessing.StandardScaler` replaces any zero-variance feature's scale with 1.0 rather than dividing by zero. Reproducing that exactly matters: a feature that was constant across the training set would otherwise produce `(x - mean) / 0 = ±Inf` here and `(x - mean) / 1 = 0` there.

Inspecting `stat_std[]`, the smallest value is `0.001429f` at index 15 — `HFE(az)`, which makes sense (a ratio bounded in [0,1] with low variance across the training set). Well above `1e-10`, so the guard never fires. It is there for the case where the scaler is regenerated from a smaller or less varied dataset.

### 38.3 In-place, and the ordering that requires

```c
Quantize_TS(scaled, ts_q);              /* uses `scaled`,  NOT `features` */
Quantize_NormalizeStat(features);       /* modifies `features` in place   */
Quantize_Stat(features, stat_q);        /* uses the modified `features`   */
```

The actual call order in Thread 2 is:

```c
Scale_RawWindow(flat_window, scaled);
Features_Extract(scaled, features);
Quantize_TS(scaled, ts_q);
Quantize_NormalizeStat(features);
Quantize_Stat(features, stat_q);
```

`Quantize_TS` operates on `scaled`, which normalisation never touches, so its position is free. The two `features` operations must be in the order shown — normalise, then quantise. The `quantize.h` header documents the required sequence:

```c
 *     Features_Extract(scaled, feats);
 *     Quantize_NormalizeStat(feats);     // in-place z-score
 *     Quantize_Stat(feats, q_stat);      // float -> int8
 *     Quantize_TS  (scaled, q_ts);       // float -> int8
```

Because normalisation is destructive, the un-normalised feature values are unavailable after this point — which is why the replay-mode diagnostics capture the *label*, not the features. Anyone wanting to compare on-target features against a Python reference must break here with a debugger, before the call.

## 39. Stage 6 — Quantisation

Standard TFLite affine quantisation:

```
   q = clamp( round(f / scale) + zero_point, -128, +127 )
```

```c
static inline int8_t quantize_one(float32_t f, float32_t inv_scale, int32_t zero_point)
{
    const float32_t scaled = f * inv_scale;
    const float32_t rnd    = nearbyintf(scaled);

    int32_t q = (int32_t)rnd + zero_point;

    if (q < (int32_t)INT8_MIN) { q = (int32_t)INT8_MIN; }
    if (q > (int32_t)INT8_MAX) { q = (int32_t)INT8_MAX; }

    return (int8_t)q;
}
```

### 39.1 The two scales

| Tensor | Scale | Zero point | Represents |
|---|---|---|---|
| Time series (`ts_q`) | `0.189672` | `−6` | Scaled IMU values in g and °/s |
| Statistics (`stat_q`) | `0.057328` | `−53` | Z-scored features |

Both come from the TFLite converter's calibration pass and are recorded at the top of `models/model_data.h`:

```
// In 'serving_default_ts_input:0': [1, 50, 6] s=0.189672 z=-6
// In 'serving_default_stat_input:0': [1, 50] s=0.057328 z=-53
// Out 'StatefulPartitionedCall_1:0': [1, 2] s=0.003906 z=-128
```

and duplicated in `norm_params.h` as `TS_SCALE`, `TS_ZP`, `STAT_SCALE`, `STAT_ZP`.

Representable ranges:

```
   Time series:
     q = -128  →  f = (-128 - (-6)) × 0.189672 = -23.14
     q = +127  →  f = (+127 - (-6)) × 0.189672 = +25.23
     → covers roughly ±23 in scaled units

     Accelerometer at ±4 g full scale reaches ±4.0 — comfortably inside.
     Gyroscope at ±500 °/s full scale reaches ±500 — WAY outside.

   Statistics:
     q = -128  →  f = (-128 - (-53)) × 0.057328 = -4.30
     q = +127  →  f = (+127 - (-53)) × 0.057328 = +10.32
     → covers roughly -4.3 to +10.3 standard deviations
```

**The gyroscope range is the interesting one.** A ±500 °/s reading scales to ±500, which quantises to a saturated ±127. That looks alarming until you consider what the calibration set contained: a vehicle on a road produces yaw/pitch/roll rates of perhaps ±30 °/s in normal driving, and the TFLite converter chose 0.189672 based on the *observed* range in the calibration data, not the sensor's full scale. So the quantiser is tuned for realistic driving, and saturation would only occur during a spin or a rollover — conditions where "rough" is the correct answer anyway.

The asymmetric statistics range (−4.3 to +10.3 σ) reflects that many of the 50 features are non-negative and right-skewed — variances, ratios, peak-to-peak values. The converter placed the zero point to give more headroom above the mean than below.

### 39.2 `nearbyintf` versus manual rounding

```c
const float32_t rnd = nearbyintf(scaled);   /* CHANGE: was manual +/- 0.5 */
```

`nearbyintf` uses the current rounding mode, which on Cortex-M4 defaults to **round-to-nearest, ties-to-even** — the IEEE 754 default, and exactly what TFLite's reference kernels use.

The replaced `(int)(x + 0.5f)` idiom rounds ties *away from zero* and, worse, rounds negative numbers wrongly (`(int)(-2.5 + 0.5) = -2`, but round-to-nearest gives −2 and ties-to-even gives −2; whereas `(int)(-1.5 + 0.5) = -1` versus the correct −2). The `CHANGE:` comment marks a real fix.

`nearbyintf` compiles to `VRINTN.F32` on Cortex-M4F — a single instruction, no library call.

### 39.3 Clamping before the cast

```c
int32_t q = (int32_t)rnd + zero_point;
if (q < INT8_MIN) { q = INT8_MIN; }
if (q > INT8_MAX) { q = INT8_MAX; }
return (int8_t)q;
```

The header comment explains:

```c
 * Clamping is performed in float BEFORE the cast — this keeps the
 * conversion well-defined per C11 6.3.1.4 (out-of-range float-to-int
 * is otherwise undefined behaviour).
```

Strictly the clamp happens in `int32_t`, not float — but the substance is right: the `float → int32_t` conversion is safe because `rnd` is bounded by the physical range of the inputs, and the `int32_t → int8_t` conversion is safe because of the clamp. Casting an out-of-range float directly to `int8_t` would be UB, and on ARM would saturate silently in a way that happens to be correct — but relying on that is not portable.

### 39.4 The time-series layout

```c
for (uint32_t t = 0U; t < WINDOW_SIZE; ++t)
{
    const uint32_t base = t * (uint32_t)N_FEATURES;
    out_int8[base + 0U] = quantize_one(scaled[t][0], inv_scale, zp);
    ...
    out_int8[base + 5U] = quantize_one(scaled[t][5], inv_scale, zp);
}
```

Row-major, time-major: `out[t * 6 + ch]`.

```
   index:  0   1   2   3   4   5   6   7   8   9  10  11  ...
          ax  ay  az  gx  gy  gz  ax  ay  az  gx  gy  gz  ...
          └──── t = 0 ────┘  └──── t = 1 ────┘

   Total 300 int8 = 50 timesteps × 6 channels
```

This must match the model's `ts_input` tensor, declared in `network.h` as:

```c
#define AI_NETWORK_IN_2_HEIGHT   (50)
#define AI_NETWORK_IN_2_CHANNEL  (6)
#define AI_NETWORK_IN_2_SIZE     (300)
```

A `(height=50, channel=6)` tensor in CubeAI's channel-last convention is exactly `[t][ch]` row-major. ✓

`inference.c` carries a compile-time check:

```c
#if defined(AI_NETWORK_IN_1_SIZE) && defined(AI_NETWORK_IN_2_SIZE)
#  if (AI_NETWORK_IN_1_SIZE != N_STAT_FEATURES)
#    error "CubeAI input[0] size mismatch — expected N_STAT_FEATURES (50)."
#  endif
#  if (AI_NETWORK_IN_2_SIZE != (WINDOW_SIZE * N_FEATURES))
#    error "CubeAI input[1] size mismatch — expected WINDOW_SIZE*N_FEATURES."
#  endif
#endif
```

Sizes are checked; **layout is not and cannot be**. A transposed `[ch][t]` packing would have the same 300-byte size and would pass.

## 40. Stage 7 — Inference

```c
void Inference_Run(const int8_t ts_i8[300], const int8_t stat_i8[50],
                   Inference_Result_t *result)
{
    if ((s_initialised == 0U) || (result == NULL)) {
        if (result != NULL) { result->label = INFERENCE_LABEL_SMOOTH; result->confidence = 0U; }
        return;
    }

    result->label      = INFERENCE_LABEL_SMOOTH;    /* defined fail state */
    result->confidence = 0U;

    ai_buffer *inputs  = ai_network_inputs_get(s_network, NULL);
    ai_buffer *outputs = ai_network_outputs_get(s_network, NULL);

    if ((inputs == NULL) || (outputs == NULL) ||
        (inputs[0].data == NULL) || (inputs[1].data == NULL) ||
        (outputs[0].data == NULL)) { return; }

    (void)memcpy(inputs[0].data, stat_i8, (size_t)N_STAT_FEATURES);
    (void)memcpy(inputs[1].data, ts_i8,   (size_t)(WINDOW_SIZE * N_FEATURES));

    ai_i32 batch = ai_network_run(s_network, inputs, outputs);
    if (batch != (ai_i32)1) { return; }

    const int8_t *out = (const int8_t *)outputs[0].data;

    uint8_t label;  int16_t winner, loser;
    if (out[1] > out[0]) { label = INFERENCE_LABEL_ROUGH;  winner = out[1]; loser = out[0]; }
    else                 { label = INFERENCE_LABEL_SMOOTH; winner = out[0]; loser = out[1]; }

    int32_t margin = (int32_t)winner - (int32_t)loser;
    if (margin < 0)   { margin = 0; }
    if (margin > 255) { margin = 255; }

    result->label      = label;
    result->confidence = (uint8_t)((margin * 100) / 255);
}
```

### 40.1 Input ordering is fixed by the graph

```c
inputs[0] = stat features  (50 bytes)
inputs[1] = time series   (300 bytes)
```

CubeAI orders the input array by the order the tensors appear in the TFLite graph's `subgraph.inputs` list, which is determined by the Keras model's input ordering at conversion time. The names in `model_data.h` confirm it:

```
// In 'serving_default_ts_input:0'  ...
// In 'serving_default_stat_input:0' ...
```

Alphabetical by TensorFlow's signature convention puts `stat_input` before `ts_input`. Matching the code. ✓

If someone regenerated the model with the Keras inputs in the other order, `inputs[0]` and `inputs[1]` would swap, `memcpy` would write 50 bytes into a 300-byte tensor and 300 bytes into a 50-byte one — **a 250-byte buffer overrun into the activations arena.** The size assertions in `inference.c` would not catch it because both sizes are still 50 and 300, just assigned to the other index.

An extra check on `inputs[0].size` at runtime would close this:

```c
if (AI_BUFFER_SIZE(&inputs[0]) != N_STAT_FEATURES) { return; }
```

Recorded in [§82](#82-known-gaps).

### 40.2 Argmax in int8 space

```c
if (out[1] > out[0]) { label = ROUGH; } else { label = SMOOTH; }
```

Dequantisation is a monotonic affine map (`f = (q - zp) × scale` with `scale > 0`), so comparing quantised values gives the same ordering as comparing dequantised ones. Skipping the dequantisation saves two multiplies and is exactly correct.

Ties (`out[0] == out[1]`) fall to the `else` branch → SMOOTH. Consistent with the voting layer's tie-breaking ([§41](#41-stage-8--temporal-voting)).

### 40.3 Confidence from the margin

```c
int32_t margin = winner - loser;             /* 0..255 in int8 space */
result->confidence = (uint8_t)((margin * 100) / 255);
```

```
   out = [+40, -30]  →  margin = 70   →  confidence = 27
   out = [-128, +127] → margin = 255  →  confidence = 100
   out = [+5, +4]    →  margin = 1    →  confidence = 0
```

This is **not** a probability. The output tensor is a softmax with `s = 0.003906 = 1/256` and `z = −128`, so dequantising gives:

```
   p = (q - (-128)) × (1/256) = (q + 128) / 256
```

A true probability would be `p_winner` directly — for `out = [+40, -30]`, `p_smooth = 168/256 = 0.656`. The margin-based figure gives 27 instead. The two are related but not equal, and the margin form is more conservative near the decision boundary.

Since the value is used only as a human-readable indicator in the classification frame — nothing downstream thresholds on it — the choice is defensible. Naming it `confidence` invites the reader to interpret it as a probability, which it is not.

Multiplying by 100 before dividing by 255 preserves resolution: `(70 × 100) / 255 = 27` rather than `(70/255) × 100 = 0` in integer arithmetic.

### 40.4 The activations arena

```c
static ai_u8 s_activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE] __attribute__((aligned(8)));
```

`AI_NETWORK_DATA_ACTIVATIONS_SIZE` is **1,464 bytes**. That is the total scratch space CubeAI needs for all intermediate tensors — the tool computes it by liveness analysis and overlaps buffers whose lifetimes do not intersect.

The 8-byte alignment is a CubeAI ABI requirement (some kernels use 64-bit loads).

For a network with a 50×8 conv output, a 40-element concat, and three dense layers, 1,464 bytes is plausible and tight — which is exactly what the liveness overlapping achieves.

### 40.5 Failure is silent and biased

Every early-return path leaves `{ label = SMOOTH, confidence = 0 }`. So a failed inference is indistinguishable from a genuine low-confidence "smooth" classification.

```
   Uninitialised network  → SMOOTH, 0
   NULL tensor pointer    → SMOOTH, 0
   batch != 1             → SMOOTH, 0
   Genuine tie            → SMOOTH, 0
```

No log, no counter, no distinguishing marker. `LOG_CODE_INFERENCE_FAIL` exists in `logger.h` and is never used. Since the failure modes are all "impossible" (they require the CubeAI runtime to misbehave), this has never mattered — but if it ever did, the symptom would be a node that confidently reports smooth road forever.

## 41. Stage 8 — Temporal Voting

```c
Vote_Push(&vote, result.label);
if (Vote_Ready(&vote))
{
    result.label = Vote_Decide(&vote);
    /* ... build and queue the frame ... */
}
```

A 9-deep circular history, majority vote, ties favouring SMOOTH.

```c
typedef struct {
    uint8_t history[VOTE_WINDOW];   /* 9 */
    uint8_t head;                   /* next write position */
    uint8_t count;                  /* saturates at 9 */
} Vote_t;                           /* 11 bytes */
```

```
   Push sequence: R R S R R R S R R
   ┌───┬───┬───┬───┬───┬───┬───┬───┬───┐
   │ R │ R │ S │ R │ R │ R │ S │ R │ R │
   └───┴───┴───┴───┴───┴───┴───┴───┴───┘
                                       ▲ head wraps to 0

   Tally:  SMOOTH = 2,  ROUGH = 7   →  decide ROUGH
```

### 41.1 The warm-up gate

```c
uint8_t Vote_Ready(const Vote_t *v){
    return (v->count >= (uint8_t)VOTE_WINDOW) ? 1U : 0U;
}
```

Until nine labels have accumulated, `Vote_Ready` returns 0 and **no classification frame is sent at all**. At 4 Hz that is 2.25 seconds of silence after boot (or after a `Vote_Init`).

The header's usage example suggests a different policy:

```c
 *         if (Vote_Ready(&g_vote)) {
 *             uint8_t smoothed = Vote_Decide(&g_vote);
 *             emit(smoothed);
 *         } else {
 *             emit(raw_label);   // fallback during warm-up
 *         }
```

Thread 2 does **not** implement the fallback — it simply skips the frame. So the documented "emit raw during warm-up" behaviour is not what happens. Emitting nothing is arguably the better choice (an unsmoothed label is exactly what the voting layer exists to suppress), but the divergence between the documented pattern and the implementation is worth knowing.

A practical consequence: the gateway sees no classification frames for the first ~2.3 seconds after every STM32 reset. Since a reset is what follows every OTA, that gap is visible in the field.

### 41.2 Tie-breaking

```c
uint8_t best_lbl   = 0U;
int16_t best_count = counts[0];
for (uint32_t c = 1U; c < N_CLASSES; ++c)
{
    if (counts[c] > best_count) { best_count = counts[c]; best_lbl = (uint8_t)c; }
}
```

Strict `>` means the first (lowest-indexed) class with the maximum wins. With `N_CLASSES = 2` and `SMOOTH = 0`, ties go to SMOOTH.

The header explains the domain reasoning:

```c
 * Tie-breaking: favours the lower-indexed class. ... Rationale:
 * false positives for "rough" are operationally worse than false negatives
 * in a road monitor.
```

With `VOTE_WINDOW = 9` (odd) a genuine 2-class tie is impossible once the buffer is full — 9 votes cannot split 4.5/4.5. The tie-break only matters if `Vote_Decide` is called before the buffer fills, which `Vote_Ready` prevents, or if `VOTE_WINDOW` were ever changed to an even number. Choosing an odd window is what makes ties unreachable; the tie-break is belt-and-braces.

### 41.3 What the window costs and buys

```
   Raw inference rate: 4 Hz  (one per 250 ms)
   Window depth:       9
   Full-window latency: 9 × 250 ms = 2.25 s

   A step change from smooth to rough takes 5 consecutive ROUGH labels
   to flip the majority (5 of 9), i.e. 5 × 250 ms = 1.25 s of latency.
```

```
   Raw:      S S S S S R R R R R R R R R R R R R
   Voted:    S S S S S S S S S R R R R R R R R R
                               ▲
                               5 rough votes → majority flips
                               1.25 s after the surface changed
```

Buys: a single spurious inference cannot change the reported class. With a model at, say, 90 % per-window accuracy, the 5-of-9 majority pushes effective accuracy above 99 %.

Costs: 1.25 seconds of latency on a genuine transition. At 50 km/h that is 17 metres of road classified wrongly at every surface change.

Whether 9 is the right number is a domain question — it trades transition latency against noise immunity. The value is centralised in `norm_params.h` (`VOTE_WINDOW 9`), duplicated in `models/model_data.h` (`#define VOTE_WINDOW 9`), and the model's comment header recommends it:

```
// 64-point FFT, temporal voting k=9 recommended
```

So it came from the training-side evaluation, not from a guess.

### 41.4 Out-of-range labels are ignored

```c
void Vote_Push(Vote_t *v, uint8_t label){
    if (label >= (uint8_t)N_CLASSES) { return; }    /* head and count both stay */
    ...
}
```

A label of 2 or higher advances neither the head nor the count. So a buggy upstream cannot pollute the history *or* prematurely satisfy `Vote_Ready`. Given that `Inference_Run` only ever produces 0 or 1, this is unreachable defensive code — cheap and correct.

## 42. Windowing and Stride

```c
#define WINDOW_SIZE     50U    /* norm_params.h */
#define WINDOW_STRIDE   25U    /* main.c        */
```

```
   Sample index:  0    25   50   75   100  125  150
                  │    │    │    │    │    │    │
   Window 0:      ├─────────┤                          samples   0..49
   Window 1:           ├─────────┤                     samples  25..74
   Window 2:                ├─────────┤                samples  50..99
   Window 3:                     ├─────────┤           samples  75..124
                       ◄───►
                       stride 25 = 50 % overlap
```

| Quantity | Value | Derivation |
|---|---|---|
| Window duration | 500 ms | 50 samples ÷ 100 Hz |
| Stride duration | 250 ms | 25 samples ÷ 100 Hz |
| Overlap | 50 % | (50 − 25) ÷ 50 |
| Inference rate | 4 Hz | 100 Hz ÷ 25 |
| Voted output rate | 4 Hz | one vote per inference |
| Total latency, sample to voted frame | ≈ 2.5–2.75 s | 0.5 s window + 2.25 s vote fill |

### 42.1 Why 50 % overlap

Without overlap (stride 50), the inference rate halves to 2 Hz and a road event landing at a window boundary is split across two windows, diluting its signature in both.

With 50 % overlap, every event appears fully contained in at least one window:

```
   Event of duration ≤ 250 ms, arriving anywhere:

   Window N:    ├──────────┤
   Window N+1:       ├──────────┤
                  ▲▲▲▲
                  Any 250 ms interval is fully inside at least one window
```

Higher overlap (stride 12, 75 %) would double the inference rate and the CPU cost for diminishing returns. Stride 25 is the natural choice for a 50-sample window.

### 42.2 The stride and the ring buffer

`RingBuffer_Advance(25)` after each window leaves 25 samples in the buffer as permanent carry-over. That is what makes the ring buffer's steady-state occupancy oscillate between 25 and 50 ([§25.1](#251-the-drain-loop)) — the 25 carried plus up to 25 newly arrived.

The buffer's 128-sample capacity therefore provides `128 − 50 = 78` samples of headroom, or **780 ms**, before an overflow. That is the real jitter budget, not the nominal 1.28 s.

### 42.3 `WINDOW_STRIDE` lives in the wrong file

`WINDOW_SIZE`, `N_FEATURES`, `VOTE_WINDOW` and every other pipeline constant is in `norm_params.h`. `WINDOW_STRIDE` is defined in `main.c`:

```c
/* main.c */
#define WINDOW_STRIDE   25U   /* 50% overlap → 4 Hz inference rate */
```

It is as much a model hyperparameter as the others — the training pipeline used the same stride to generate its windows — and it belongs alongside them. Minor, but it means someone regenerating `norm_params.h` from the training script would not see the stride and might not realise it needs to match.

## 43. Replay Mode

```c
/* ===== Build Configuration ============================================== */
#define REPLAY_MODE   1   /* 1 = inject CSV, 0 = live IMU */
```

**The committed build has this set to 0** — Thread 1 reads the live IMU. Set it to `1` to replay a fixed array instead ([§24.1](#241-replay-mode--what-the-committed-build-actually-does)).

### 43.1 The replay data

```c
/*
 * replay_data.h — auto-generated, do not edit by hand.
 * Generated:    2026-05-04 16:38:45
 * Source CSV:   rough_031_20260426_013229.csv
 * Samples:      200
 *
 * CSV metadata:
 *   class = rough
 *   sample_rate_hz = 100
 *   accel_range = 4g
 *   gyro_range = 500dps
 *   accel_scale = 8192.0
 *   gyro_scale = 65.5
 *   total_samples = 1500
 *   missed_frames = 0
 *   crc_errors = 0
 */

#define REPLAY_NUM_SAMPLES           200U
#define REPLAY_EXPECTED_CLASS_STR    "rough"

static const MPU6050_RawData_t replay_samples[REPLAY_NUM_SAMPLES] = { ... };
```

200 samples = 2 seconds of recorded rough road, looping forever. 2,800 bytes of `.rodata`.

The embedded metadata is the file's best feature: it records the exact sensor configuration used during capture, so a reader can verify that the replay data and the live path agree on scaling. They do — `accel_scale = 8192.0` and `gyro_scale = 65.5` match `norm_params.h`.

`missed_frames = 0` and `crc_errors = 0` mean the capture was clean, which matters because a capture with dropped samples would embed a time discontinuity that the variance-ratio features would read as a surface change.

The generator is `data/rough/csv_to_replay_header.py`.

### 43.2 What replay mode is for

```
   ┌──────────────────────────────────────────────────────────────┐
   │  Python training pipeline                                     │
   │    → tflite_reference.py runs the model on the same CSV       │
   │    → emits expected label + confidence per window             │
   └───────────────────────┬──────────────────────────────────────┘
                           │  same 200 samples
                           ▼
   ┌──────────────────────────────────────────────────────────────┐
   │  STM32 with REPLAY_MODE = 1                                   │
   │    → g_replay_last_label, g_replay_last_conf                  │
   │    → g_replay_vote_smooth, g_replay_vote_rough                │
   └───────────────────────┬──────────────────────────────────────┘
                           │
                           ▼
                    compare — any divergence means the C pipeline
                    disagrees with the Python one somewhere in
                    scale → features → normalise → quantise → infer
```

This is the mechanism by which the numerical-parity claim of [§45](#45-numerical-parity-with-the-training-pipeline) is actually *tested* rather than merely asserted. Without it, a subtle divergence — a wrong variance denominator, a transposed feature index — would only show up as slightly degraded field accuracy, which is nearly impossible to attribute.

Expected result with `rough` data: `g_replay_vote_rough` should dominate `g_replay_vote_smooth` by a wide margin. Reading them in a debugger is a 30-second validation.

`tools/mcu_compare.py` (548 lines) and `tools/gen_test_vectors.py` (457 lines) automate the comparison; `tools/tflite_reference.py` (450 lines) is the Python-side reference runner.

### 43.3 Switching to live mode

One edit:

```c
#define REPLAY_MODE   0
```

Then verify:

| Check | Expected |
|---|---|
| `dbg_init_stage` (MPU6050.c) | `99` — all init stages passed |
| `dbg_whoami_val` | `0x68` |
| PC13 | steady dim glow (Thread 1 toggling at 100 Hz) |
| `accel_z` at rest | ≈ +8192 |
| `g_t2_inference_count / g_t2_wake_count` | ≈ 1 : 25 |
| `rb_max_fill` in the heartbeat | ≈ 50 |

The `dbg_init_stage` codes are documented in `MPU6050.c` ([§76](#76-debug-breadcrumbs)) and are the fastest way to localise a sensor bring-up failure.

### 43.4 The mode is invisible in telemetry

Nothing in any frame indicates whether the node is replaying or reading a live sensor. The heartbeat, the classification frames and the logs are byte-identical between the two modes.

Given that shipping in replay mode would produce a node that confidently reports a constant classification forever, this is the most consequential silent-failure mode in the design. A one-line fix:

```c
LOG_INFO(LOG_CODE_BOOT, (uint32_t)REPLAY_MODE);
```

`main()` already calls `LOG_INFO(LOG_CODE_BOOT, 0U)` — passing `REPLAY_MODE` instead of `0` costs nothing and makes the mode visible in the very first log frame after every boot. Recorded in [§82](#82-known-gaps).

## 44. Model Architecture

Reconstructed from `lib/CubeAI/network/network.c`'s tensor and layer declarations.

```
   ┌─────────────────────────┐        ┌──────────────────────────┐
   │  ts_input   [50, 6] i8  │        │  stat_input  [50] i8     │
   └────────────┬────────────┘        └────────────┬─────────────┘
                │                                  │
                ▼                                  ▼
   ┌─────────────────────────┐        ┌──────────────────────────┐
   │ conv2d_3                │        │ gemm_0                   │
   │   in  6 ch × 50         │        │   50 → 48                │
   │   kernel 5, 6→8         │        │   weights 50×48 = 2400   │
   │   out 8 ch × 50         │        │   bias 48                │
   │   weights [6,5,1,8]=240 │        └────────────┬─────────────┘
   │   bias 8                │                     │
   └────────────┬────────────┘                     ▼
                │                       ┌──────────────────────────┐
                ▼                       │ gemm_1                   │
   ┌─────────────────────────┐          │   48 → 32                │
   │ pool_6                  │          │   weights 48×32 = 1536   │
   │   out 8 ch × 10         │          │   bias 32                │
   │   (pool factor 5)       │          └────────────┬─────────────┘
   └────────────┬────────────┘                       │
                ▼                                    │
   ┌─────────────────────────┐                       │
   │ conv2d_9                │                       │
   │   kernel 3, 8→8         │                       │
   │   out 8 ch × 10         │                       │
   │   weights [8,3,1,8]=192 │                       │
   │   bias 8                │                       │
   └────────────┬────────────┘                       │
                ▼                                    │
   ┌─────────────────────────┐                       │
   │ pool_11                 │                       │
   │   out 8 ch × 1          │                       │
   │   (global pool)         │                       │
   └────────────┬────────────┘                       │
                │  8                                 │  32
                └──────────────┬─────────────────────┘
                               ▼
                  ┌──────────────────────────┐
                  │ concat_12                │
                  │   8 + 32 = 40            │
                  └────────────┬─────────────┘
                               ▼
                  ┌──────────────────────────┐
                  │ gemm_13                  │
                  │   40 → 24                │
                  │   weights 40×24 = 960    │
                  │   bias 24                │
                  └────────────┬─────────────┘
                               ▼
                  ┌──────────────────────────┐
                  │ gemm_14                  │
                  │   24 → 2                 │
                  │   weights 24×2 = 48      │
                  │   bias 2                 │
                  └────────────┬─────────────┘
                               ▼
                  ┌──────────────────────────┐
                  │ nl_15  — softmax          │
                  │   out [2] i8              │
                  │   s = 0.003906, z = -128  │
                  └────────────┬─────────────┘
                               ▼
                        [smooth, rough]
```

### 44.1 Layer-by-layer detail

| Layer | Type | Input | Output | Weights | Bias |
|---|---|---|---|---|---|
| `conv2d_3` | 1-D conv, kernel 5 | 6 × 50 | 8 × 50 | `[6,5,1,8]` = 240 | 8 |
| `pool_6` | pooling | 8 × 50 | 8 × 10 | — | — |
| `conv2d_9` | 1-D conv, kernel 3 | 8 × 10 | 8 × 10 | `[8,3,1,8]` = 192 | 8 |
| `pool_11` | global pooling | 8 × 10 | 8 × 1 | — | — |
| `gemm_0` | dense | 50 | 48 | 50 × 48 = 2,400 | 48 |
| `gemm_1` | dense | 48 | 32 | 48 × 32 = 1,536 | 32 |
| `concat_12` | concatenate | 8 + 32 | 40 | — | — |
| `gemm_13` | dense | 40 | 24 | 40 × 24 = 960 | 24 |
| `gemm_14` | dense | 24 | 2 | 24 × 2 = 48 | 2 |
| `nl_15` | softmax | 2 | 2 | — | — |
| **Total** | | | | **5,376 int8** | **122 int32** |

`AI_NETWORK_DATA_WEIGHTS_SIZE` is 5,864 bytes, which accounts for the 5,376 int8 weights plus the int32 biases and per-channel quantisation parameters.

### 44.2 The two-branch design

This is a **dual-input hybrid**: a small CNN over the raw time series in parallel with an MLP over the hand-crafted statistics, fused by concatenation before the classifier head.

```
   Time-series branch:  6 × 50  →  conv(5) →  pool  →  conv(3) →  pool  →  8
   Statistics branch:      50   →  dense48 →  dense32                    →  32
                                                                    concat → 40
                                                          dense24 → dense2 → softmax
```

Why both? Each captures what the other cannot:

| Branch | Captures | Misses |
|---|---|---|
| CNN over raw samples | Local temporal patterns — the shape of an impact, the decay of an oscillation | Global spectral content (a 5-tap kernel cannot see 25 Hz structure over 50 samples) |
| MLP over 50 statistics | Spectral energy distribution (HFE), distributional shape (skewness, IQR), cross-axis coupling (correlations) | Any time-ordering information — the features are permutation-invariant |

The statistics branch has **8× the parameters** of the CNN branch (3,936 versus 440), which suggests the hand-crafted features carry most of the discriminative power and the CNN adds a smaller refinement. That is consistent with the physics: road roughness is fundamentally a spectral property, and HFE measures it directly.

### 44.3 Scratch buffers

CubeAI allocates per-layer scratch inside the 1,464-byte arena:

| Layer | Scratch |
|---|---|
| `conv2d_3` | 712 B |
| `conv2d_9` | 592 B |
| `gemm_0` | 290 × 2 = 580 B |
| `gemm_1` | 208 × 2 = 416 B |
| `gemm_13` | 160 × 2 = 320 B |
| `gemm_14` | 34 × 2 = 68 B |
| `nl_15` | 256 × 4 = 1,024 B |

The sum (3,720 B) far exceeds the 1,464-byte arena — because CubeAI overlaps buffers whose lifetimes do not intersect. `nl_15`'s 1,024-byte scratch is live only during the final softmax, by which point `conv2d_3`'s 712 bytes are long dead and their memory has been reused.

### 44.4 `models/model_data.h` is not used at runtime

```c
// Road Classifier v13 INT8, 16288 bytes
alignas(8) const unsigned char g_model_data[] = { 0x1c, 0x00, ... };
```

A 16,288-byte `.tflite` FlatBuffer. **Nothing references `g_model_data`.** The runtime uses CubeAI's generated `network.c` plus `s_network_weights_array_u64` (5,864 B), not the TFLite blob.

The file is retained because its comment header is the authoritative record of the quantisation parameters:

```
// In 'serving_default_ts_input:0': [1, 50, 6] s=0.189672 z=-6
// In 'serving_default_stat_input:0': [1, 50] s=0.057328 z=-53
// Out 'StatefulPartitionedCall_1:0': [1, 2] s=0.003906 z=-128
```

Whether 16 KB of Flash is actually consumed depends on whether the linker garbage-collects it. With `-ffunction-sections -fdata-sections` and `--gc-sections` it would be dropped; PlatformIO's default for `framework = cmsis` does enable these. The `.rodata` figures in [§14.2](#142-where-the-rest-goes) do not show a 16 KB unexplained block, which suggests it **is** being collected. Worth confirming with a map file if Flash ever gets tight.

## 45. Numerical Parity with the Training Pipeline

The C pipeline is a transliteration of the Python one. Each place where an implementation choice could diverge, and how it was resolved:

| Aspect | Python / NumPy | C | Match |
|---|---|---|---|
| Variance denominator | `np.var` → `/N` | `s / (float32_t)n` | ✓ |
| Std | `np.std` → `sqrt(var)` | `sqrtf(feat_var(...))` | ✓ |
| MAD | `np.mean(np.abs(np.diff(x)))` | `sum(|x[i]-x[i-1]|) / (n-1)` | ✓ |
| Percentile | `np.percentile` linear interp | `feat_percentile_sorted` | ✓ |
| Skewness | `mean(((x-m)/s)**3)`, 0 if `s<1e-10` | identical, `EPS_STAB = 1e-10` | ✓ |
| Correlation | `np.corrcoef[0,1]`, 0 if either std `<1e-10` | identical | ✓ |
| Variance ratio | `(var(hi)+1e-10)/(var(lo)+1e-10)` | identical | ✓ |
| NaN/Inf | `nan_to_num(nan=0, posinf=10, neginf=-10)` | `feat_sanitize` | ✓ (but see [§13.2](#132--ffast-math-and-its-implications)) |
| FFT | `np.fft.rfft`, 33 bins | `arm_rfft_fast_f32`, unpacked to 33 | ✓ |
| HFE | `sum(bins[16:]) / (sum(bins)+1e-10)` | identical | ✓ |
| Z-score | `StandardScaler`, `s=1` if zero-variance | identical | ✓ |
| Quantisation rounding | TFLite round-half-to-even | `nearbyintf` | ✓ |
| Precision | float64 → float32 at the end | float32 throughout | **≈** |

### 45.1 The one genuine divergence

NumPy computes in **float64** and the results are cast to float32 only when fed to the model. The C pipeline is float32 from `Scale_RawWindow` onward.

Where does that matter most? The accumulation loops. `feat_mean` over 50 elements accumulates in float32:

```c
float32_t s = 0.0f;
for (uint32_t i = 0U; i < n; ++i) { s += x[i]; }
return s / (float32_t)n;
```

With 50 terms of similar magnitude, the relative error of naive float32 summation is bounded by roughly `n × ε` = 50 × 1.19e-7 ≈ 6e-6. Against a quantisation step of `STAT_SCALE = 0.057` on z-scored values whose spread is ~1.0, that is 4 orders of magnitude below the quantiser's resolution.

**Conclusion: the divergence is real and irrelevant.** A feature would have to sit within 6e-6 of a quantisation boundary for the float32/float64 difference to change its int8 value. With 50 features and 4 Hz inference, that happens perhaps once every few hours and changes one feature by one LSB.

Kahan summation would eliminate it and is not worth the cycles.

### 45.2 The verification tooling

| Tool | Purpose |
|---|---|
| `tools/gen_test_vectors.py` | Generates `test_vectors.h` — input windows with expected intermediate values at every stage |
| `tools/tflite_reference.py` | Runs the `.tflite` model in Python for ground truth |
| `tools/mcu_verify.c` / `.h` | On-target harness that runs the vectors and compares |
| `tools/mcu_compare.py` | Host-side diff of on-target output against Python |
| `tools/verify.c` | Standalone verification entry point |
| `data/smooth/stage5_pyref.py` | Python reference for the normalisation stage specifically |

`tools/test_vectors.h` is 576 lines of committed vectors — a real regression suite for the numerical path.

This tooling is the reason the parity claims above can be trusted. It is also the answer to "how would you know if someone broke the feature extractor": run the vectors.

## 46. WCET Measurement

```c
uint32_t t0 = DWT_GetCycles();
Inference_Run(ts_q, stat_q, &result);
uint32_t us = (DWT_GetCycles() - t0) / 84U;   /* 84 MHz clock */
if (us > g_stats.inf_wcet_us) {
    g_stats.inf_wcet_us = (uint16_t)us;
}
```

Measures **only the CNN forward pass** — not the feature extraction that precedes it, which is roughly ten times more expensive ([§37.5](#375-cost-breakdown)).

That is a meaningful limitation. The reported "WCET" is the cheapest part of the pipeline. Bracketing the whole window-processing block instead would give a figure that actually bounds Thread 2's per-window cost:

```c
uint32_t t0 = DWT_GetCycles();
Scale_RawWindow(...);
Features_Extract(...);
Quantize_TS(...);
Quantize_NormalizeStat(...);
Quantize_Stat(...);
Inference_Run(...);
uint32_t us = (DWT_GetCycles() - t0) / 84U;
```

Recorded in [§82](#82-known-gaps).

### 46.1 Wrap safety

`DWT_GetCycles() - t0` is unsigned modular subtraction, correct across a 32-bit wrap as long as the true interval is under 2³² cycles ≈ 51 s. An inference takes microseconds. Safe.

### 46.2 The `/ 84U` and `uint16_t` truncation

Dividing by 84 gives microseconds, discarding sub-microsecond precision. Storing in `uint16_t` caps the reportable value at **65,535 µs = 65.5 ms**. An inference that took longer would wrap and report a small number — but 65 ms is 26× the whole 250 ms budget's inference share, so this is unreachable.

The hardcoded `84U` duplicates `DWT_CPU_CLOCK_HZ / 1000000UL` from `DWT.c`. `DWT_CyclesToUs()` exists and does exactly this:

```c
uint32_t DWT_CyclesToUs(uint32_t cycles){ return cycles / DWT_CYCLES_PER_US; }
```

Using it would keep the clock constant in one place. If the project ever moved to 96 or 100 MHz, `DWT.c` would be updated and `main.c` would silently keep dividing by 84.

### 46.3 It is a since-boot maximum, never reset

Discussed in [§27.2](#272-the-two-level-averaging). `g_stats.inf_wcet_us` only ever increases, so the heartbeat's `peak_wcet_us` field reports the all-time maximum on every transmission rather than the 5-second window maximum the surrounding code implies.
---

# Part VI — Wire Protocol

## 47. FRAME Wire Format

Every byte the node transmits belongs to a frame. The format is defined in [`src/FRAME.c`](src/FRAME.c) and built by `Frame_Build()`.

```
 offset:  0      1      2      3 ... 3+N-1        3+N ......... 6+N
        ┌──────┬──────┬───────┬──────────────┬─────────────────────┐
        │ LEN  │ TYPE │ ECU_ID│   PAYLOAD    │   CRC32 big-endian  │
        │ 1 B  │ 1 B  │  1 B  │   N bytes    │       4 bytes       │
        └──────┴──────┴───────┴──────────────┴─────────────────────┘
           ▲                   └──────────────┬──────────────┘
           │                                  │
           │       LEN = N + 6                │
           │       (TYPE + ECU + payload + CRC)
           │                                  │
           │       total on wire = N + 7      │
           │
           └── NOT covered by the CRC

                  CRC32 input range:
        ┌──────┬──────┬───────┬──────────────┐
        │      │ TYPE │ ECU_ID│   PAYLOAD    │   ← N + 2 bytes, from offset 1
        └──────┴──────┴───────┴──────────────┘
```

```c
Frame_Status_t Frame_Build(uint8_t type, uint8_t ecu_id,
                           const uint8_t *payload, uint8_t len,
                           uint8_t *out_buf, uint16_t out_cap, uint16_t *out_len)
{
    if ((out_buf == NULL) || (out_len == NULL))      return FRAME_ERR_NULL_PTR;
    if ((payload == NULL) && (len > 0U))             return FRAME_ERR_NULL_PTR;
    if (len > FRAME_MAX_PAYLOAD)                     return FRAME_ERR_PAYLOAD_TOO_BIG;

    total_len = (uint16_t)len + (uint16_t)FRAME_OVERHEAD_BYTES;   /* len + 7 */
    if (out_cap < total_len)                         return FRAME_ERR_BUF_TOO_SMALL;

    len_byte = (uint8_t)(len + 6U);

    out_buf[0U] = len_byte;
    out_buf[1U] = type;
    out_buf[2U] = ecu_id;
    for (i = 0U; i < len; i++) { out_buf[3U + i] = payload[i]; }

    crc = Frame_CRC32(&out_buf[1U], (uint16_t)(len + 2U));
    frame_pack_u32_be(crc, &out_buf[3U + len]);

    *out_len = total_len;
    return FRAME_OK;
}
```

### 47.1 Constants

| Constant | Value | Meaning |
|---|---|---|
| `FRAME_HEADER_BYTES` | 3 | LEN + TYPE + ECU_ID |
| `FRAME_CRC_BYTES` | 4 | |
| `FRAME_OVERHEAD_BYTES` | 7 | header + CRC |
| `FRAME_MAX_PAYLOAD` | 248 | keeps total ≤ 255 |
| `FRAME_MAX_TOTAL` | 255 | LEN fits in one byte |
| `ECU_ID_STM32_NODE1` | `0x01` | this node |
| `ECU_ID_ESP32_GATEWAY` | `0x02` | the gateway |
| `FRAME_INBOUND_SYNC_BYTE` | `0xAA` | for the ESP32→STM32 direction |
| `FRAME_CRC32_POLY` | `0x04C11DB7` | |
| `FRAME_CRC32_INIT` | `0xFFFFFFFF` | |
| `FRAME_CRC32_XOROUT` | `0xFFFFFFFF` | **defined but never applied** — see [§54](#54-the-crc32-quirk) |

### 47.2 There is no SYNC byte outbound

The `FRAME.c` header comment explains:

```
 * WHY NO SYNC BYTE IN OUTBOUND DIRECTION?
 *   An external GPIO line is toggled to mark frame start; this saves one
 *   byte per frame and avoids an additional state machine on the ESP32.
```

Saving one byte out of 37 is a 2.7 % bandwidth improvement at 4 % of the link's capacity — not a meaningful saving. The real argument is the second clause: a sync byte requires the receiver to handle the case where the sync value appears inside a payload, which means either byte-stuffing or a length-plus-CRC resynchronisation scheme.

The design instead relies on:

1. **LEN as an implicit frame start.** The receiver reads one byte, treats it as a length, and reads that many more.
2. **CRC32 as the resynchronisation check.** A frame that fails CRC means the receiver is out of frame; it discards one byte and retries.
3. **The PA8 GPIO strobe** as an out-of-band hint.

Whether (3) is actually used is the subject of [§55](#55-the-gpio-sync-line).

### 47.3 Why LEN is excluded from the CRC

```
 * WHY IS THE CRC OVER TYPE+ECU+PAYLOAD ONLY?
 *   The LEN byte is re-derivable and non-critical; including it would
 *   add no error detection for the data that matters.
```

Re-derivable is the key word: if the receiver has TYPE, ECU_ID, the payload and a matching CRC, it knows the payload length implicitly (from `LEN`, which it used to read that many bytes). A corrupted LEN causes a wrong-length read, which produces a CRC failure anyway — just via a different mechanism.

Note this differs from the **bootloader's** protocol, where the CRC *does* cover the length byte ([`../Bootloader/BOOTLOADER.md`](../Bootloader/BOOTLOADER.md) §19.1). Two protocols on the same wire with two different CRC coverage rules. Both ends implement both correctly, but it is a trap for anyone writing a unified parser.

### 47.4 Frame sizes in practice

| Type | Payload | Total on wire | Time at 115200 |
|---|---|---|---|
| Classification `0x01` | 6 | 13 B | 1.13 ms |
| Temperature `0x02` | 6 | 13 B | 1.13 ms |
| Heartbeat `0x03` | 30 | 37 B | 3.21 ms |
| Log `0x04` | 10 | 17 B | 1.48 ms |
| Ultrasonic `0x05` | 8 | 15 B | 1.30 ms |

Steady-state link utilisation:

```
   Ultrasonic   4.0 /s × 15 B =  60 B/s
   Classification 4.0 /s × 13 B =  52 B/s
   Temperature  0.5 /s × 13 B = 6.5 B/s
   Heartbeat    0.2 /s × 37 B = 7.4 B/s
   Log         0.25 /s × 17 B = 4.3 B/s
                                ───────
                                ≈ 130 B/s

   Link capacity at 115200 8N1 = 11,520 B/s
   Utilisation ≈ 1.1 %
```

The link is 99 % idle. The 115200 rate was chosen for compatibility, not throughput — and the headroom means bandwidth will never be the constraint even if every rate were raised tenfold.

## 48. Frame Type Catalogue

```c
/* STM32 → ESP32 */
#define FRAME_TYPE_CLASSIFICATION   0x01U
#define FRAME_TYPE_TEMPERATURE      0x02U
#define FRAME_TYPE_HEARTBEAT        0x03U
#define FRAME_TYPE_LOG              0x04U
#define FRAME_TYPE_ULTRASONIC       0x05U
#define FRAME_TYPE_BL_ACK           0xFFU

/* ESP32 → STM32 (received with 0xAA SYNC) */
#define FRAME_TYPE_BL_ENTER         0xFEU
#define FRAME_TYPE_BL_DATA          0xFDU
#define FRAME_TYPE_BL_VERIFY        0xFCU
```

| Code | Name | Direction | Payload | Producer | Implemented? |
|---|---|---|---|---|---|
| `0x01` | CLASSIFICATION | out | 6 B | Thread 2 | ✓ |
| `0x02` | TEMPERATURE | out | 6 B | Thread 6 | ✓ |
| `0x03` | HEARTBEAT | out | 30 B | Thread 4 | ✓ |
| `0x04` | LOG | out | 10 B | Logger | ✓ |
| `0x05` | ULTRASONIC | out | 8 B | Thread 7 | ✓ |
| `0xFF` | BL_ACK | out | — | — | **no** |
| `0xFE` | BL_ENTER | in | — | — | **no** |
| `0xFD` | BL_DATA | in | — | — | **no** |
| `0xFC` | BL_VERIFY | in | — | — | **no** |

The four bootloader-related types are declared and never used. They belong to an abandoned design in which the OTA protocol would have been carried inside FRAME envelopes. The actual mechanism is far simpler — a raw two-byte sequence and a reset ([§56](#56-inbound-bootloader-command)) — and the constants are vestigial.

`FRAME_INBOUND_SYNC_BYTE` (`0xAA`) is similarly declared for "the receiver state machine" that does not exist. Thread 5 matches `0xAA` directly against a raw byte, not as a frame delimiter.

## 49. Classification Payload

**Type `0x01`, 6 bytes, total frame 13 bytes.**

```
 offset   size   field           encoding
 ──────   ────   ─────────────   ─────────────────────────────────
   0       1     label           0 = SMOOTH, 1 = ROUGH
   1       1     confidence      0..100
   2..5    4     timestamp_ms    uint32 big-endian, FreeRTOS ticks
```

```c
FrameRequest_t req;
req.type   = FRAME_TYPE_CLASSIFICATION;
req.ecu_id = ECU_ID_STM32_NODE1;
req.length = 6U;

uint32_t ts = (uint32_t)xTaskGetTickCount();
req.payload[0] = result.label;
req.payload[1] = result.confidence;
req.payload[2] = (uint8_t)(ts >> 24);
req.payload[3] = (uint8_t)(ts >> 16);
req.payload[4] = (uint8_t)(ts >>  8);
req.payload[5] = (uint8_t)(ts);
```

Example — ROUGH at 42 % confidence, 12.345 s after scheduler start:

```
   payload:  01 2A 00 00 30 39
             │  │  └────┬────┘
             │  │       └── 0x00003039 = 12345 ms
             │  └────────── 0x2A = 42
             └───────────── ROUGH

   full frame:
   0C 01 01 01 2A 00 00 30 39 <crc32 BE>
   │  │  │  └──── payload ───┘
   │  │  └─ ECU_ID_STM32_NODE1
   │  └──── FRAME_TYPE_CLASSIFICATION
   └─────── LEN = 6 + 6 = 12
```

`label` is the **voted** label from `Vote_Decide`, not the raw inference output. `confidence` is the raw inference confidence from the *most recent* window, which is not necessarily the window whose label won the vote. So the pairing is slightly inconsistent — a frame can report ROUGH (because 5 of the last 9 windows said rough) with a confidence of 12 (because the most recent window was nearly a tie). This is not wrong so much as under-specified; averaging the confidence over the winning votes would be more coherent.

## 50. Temperature Payload

**Type `0x02`, 6 bytes, total frame 13 bytes.**

```
 offset   size   field            encoding
 ──────   ────   ──────────────   ───────────────────────────────
   0..1    2     temperature_x10  uint16 big-endian, tenths of °C
   2..5    4     timestamp_ms     uint32 big-endian
```

Example — 23.5 °C at 60.000 s:

```
   payload:  00 EB 00 00 EA 60
             └─┬─┘ └────┬────┘
               │        └── 0x0000EA60 = 60000 ms
               └─────────── 0x00EB = 235 = 23.5 °C
```

`temperature_x10` is `uint16`, so **negative temperatures are unrepresentable**. The LM35 in its basic single-supply configuration cannot measure below 0 °C anyway (its output would need to go negative), so this is consistent with the hardware — but a vehicle in winter would read a saturated 0.0 °C rather than an error.

Range: 0 to 6553.5 °C. The practical ceiling is set by the ADC: at `vref_mv = 3300` and 12 bits, the maximum reading is 3300 tenths = 330.0 °C, well beyond the LM35's 150 °C limit.

## 51. Heartbeat Payload

**Type `0x03`, 30 bytes, total frame 37 bytes.**

```
 offset   size   field              encoding
 ──────   ────   ────────────────   ──────────────────────────────────
   0..3    4     uptime_ms          uint32 BE, since Thread 4 started
   4..5    2     cpu_t1_x100        uint16 BE, Sensor  CPU % × 100
   6..7    2     cpu_t2_x100        uint16 BE, TinyML  CPU % × 100
   8..9    2     cpu_t3_x100        uint16 BE, UART TX CPU % × 100
  10..11   2     cpu_t6_x100        uint16 BE, Temp    CPU % × 100
  12..13   2     cpu_t7_x100        uint16 BE, Ultra   CPU % × 100
  14..15   2     cpu_idle_x100      uint16 BE, Idle    CPU % × 100
  16..17   2     stack_t1_free      uint16 BE, words free (HWM)
  18..19   2     stack_t2_free      uint16 BE
  20..21   2     stack_t3_free      uint16 BE
  22..23   2     stack_t6_free      uint16 BE
  24..25   2     stack_t7_free      uint16 BE
  26..27   2     inf_wcet_us        uint16 BE, µs (all-time max)
  28..29   2     rb_max_fill        uint16 BE, samples (all-time max)
```

All six CPU figures are **5-second averages**; the two peaks are all-time maxima ([§27.2](#272-the-two-level-averaging)).

### 51.1 The header comment is stale

`heartbeat.h` opens with a detailed wire-format table that says:

```
 * TOTAL PAYLOAD SIZE: 24 bytes
 * TOTAL ON WIRE:      31 bytes
 *
 * Offset | Size | Field
 *   0    |  4   | uptime_ms
 *   4    |  2   | cpu_t1_x100
 *   6    |  2   | cpu_t2_x100
 *   8    |  2   | cpu_t3_x100
 *  10    |  2   | cpu_idle_x100        ◄── wrong offset
 *  12    |  2   | stack_t1_free        ◄── wrong offset
 *  ...
 *  22    |  2   | (reserved)           ◄── does not exist
```

and states the heartbeat is sent "every 1 second".

**None of that is current.** The struct immediately below it declares 13 `uint16_t` fields plus a `uint32_t` = **30 bytes**, `HEARTBEAT_PAYLOAD_SIZE` is `30U`, the send interval is 5 seconds, and there is no reserved field.

What changed: `cpu_t6_x100`, `cpu_t7_x100`, `stack_t6_free` and `stack_t7_free` were added when Threads 6 and 7 were introduced (commit `37fd6c3 Modified heartbeat with temperature & ultrasonic tasks`), the reserved field was consumed, and the comment was not updated.

The **struct is authoritative**, and so is the packing code in Thread 4. A gateway-side parser written from the comment would misread every field from offset 10 onward.

### 51.2 The struct is not the wire format

```c
typedef struct {
    uint32_t uptime_ms;
    uint16_t cpu_t1_x100;
    ...
} __attribute__((packed)) Heartbeat_Payload_t;
```

`__attribute__((packed))` removes padding, so `sizeof(Heartbeat_Payload_t)` is exactly 30. But the struct's fields are stored **little-endian** in memory (ARM), while the wire format is big-endian. So a `memcpy(payload, &g_stats, 30)` would produce a byte-reversed frame.

Thread 4 packs by hand precisely to avoid that. The struct is used only as `g_stats`, a convenient accumulator. Nothing ever transmits it directly, and nothing should.

### 51.3 Interpreting the fields

| Field | Healthy value | Concerning |
|---|---|---|
| `cpu_t1_x100` | < 100 (1 %) | > 500 |
| `cpu_t2_x100` | 100–500 (1–5 %) | > 3000 |
| `cpu_t3_x100` | < 50 | > 500 |
| `cpu_idle_x100` | > 9000 (90 %) | < 5000 |
| `stack_t1_free` | > 20 words | < 10 words |
| `stack_t2_free` | ≈ 530 words | < 100 words |
| `inf_wcet_us` | a few hundred µs | > 50,000 |
| `rb_max_fill` | ≈ 50 | > 100 |

`rb_max_fill` is the single most informative number. It should sit at 50 ([§25.1](#251-the-drain-loop)); a value climbing toward 128 is the earliest warning that Thread 2 is falling behind, and it appears well before samples are actually dropped.

`cpu_idle_x100` below 5000 would indicate something is consuming CPU that should not be — but remember the six figures do not sum to 100 % ([§20.3](#203-what-is-not-measured)), so idle is the residual after five measured tasks *and* everything unmeasured.

## 52. Log Payload

**Type `0x04`, 10 bytes, total frame 17 bytes.**

```
 offset   size   field           encoding
 ──────   ────   ─────────────   ────────────────────────────
   0       1     code            Log_Code_t
   1       1     severity        0=INFO 1=WARN 2=ERROR 3=FATAL
   2..5    4     timestamp_ms    uint32 BE, tick at LOG time
   6..9    4     aux_data        uint32 BE, caller context
```

```c
req.payload[0] = log_entry.code;
req.payload[1] = log_entry.severity;
req.payload[2] = (uint8_t)(log_entry.timestamp_ms >> 24);
/* ... */
req.payload[9] = (uint8_t)(log_entry.aux_data);
```

Example — `LOG_WARN(LOG_CODE_ULTRASONIC1_TIMEOUT, 1)` at 45.678 s:

```
   payload:  15 01 00 00 B2 6E 00 00 00 01
             │  │  └────┬────┘ └────┬────┘
             │  │       │           └── aux = 1 (sensor 1)
             │  │       └────────────── 0x0000B26E = 45678 ms
             │  └────────────────────── WARN
             └───────────────────────── LOG_CODE_ULTRASONIC1_TIMEOUT
```

**`timestamp_ms` is captured at `Logger_Log()` time, not at transmission time.** Because of the 4-second rate limit ([§31.1](#311-the-4-second-rate-limit)), a log can be delivered up to a minute after it occurred — but its timestamp is always correct. The gateway can therefore reconstruct true ordering and timing from a delayed, out-of-order-looking stream.

`aux_data` is deliberately free-form. Its meaning is per-code:

| Code | `aux_data` |
|---|---|
| `LOG_CODE_MPU6050_TIMEOUT` | 0 = DMA timeout, 1 = trigger rejected |
| `LOG_CODE_RING_BUFFER_DROP` | 1 = from Thread 1, 2 = from Thread 2's peek |
| `LOG_CODE_QUEUE_FULL` | 1 = Thread 4, 2 = Thread 6, 3 = Thread 7, else = `g_t2_queue_drops` |
| `LOG_CODE_ULTRASONIC1_TIMEOUT` | 1 |
| `LOG_CODE_ULTRASONIC2_TIMEOUT` | 2 |
| `LOG_CODE_FRAME_BUILD_FAIL` | the frame type that failed |
| `LOG_CODE_ADC_TIMEOUT` | 0 |
| `LOG_CODE_BOOT` | 0 |

## 53. Ultrasonic Payload

**Type `0x05`, 8 bytes, total frame 15 bytes.**

```
 offset   size   field           encoding
 ──────   ────   ─────────────   ───────────────────────────────────
   0..1    2     distance_1_cm   uint16 BE, 0..400 or 0xFFFF
   2..3    2     distance_2_cm   uint16 BE, 0..400 or 0xFFFF
   4..7    4     timestamp_ms    uint32 BE
```

Example — 123 cm and no echo, at 90.000 s:

```
   payload:  00 7B FF FF 00 01 5F 90
             └─┬─┘ └─┬─┘ └────┬────┘
               │     │        └── 0x00015F90 = 90000 ms
               │     └─────────── 0xFFFF = no echo
               └───────────────── 0x007B = 123 cm
```

Value semantics ([§30.6](#306-the-400-cm-cap)):

| Value | Meaning |
|---|---|
| 2–399 | Measured distance in cm |
| 400 | At or beyond max range — **or** the mismatched-id bug ([§30.3](#303-a-real-bug-a-mismatched-id-reports-400-cm)) |
| `0xFFFF` | No echo within 50 ms |

The two distances are measured 50–100 ms apart and the single timestamp reflects neither ([§30.4](#304-sequential-triggering-avoids-crosstalk--mostly)).

## 54. The CRC32 Quirk

```c
uint32_t Frame_CRC32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = FRAME_CRC32_INIT;                /* 0xFFFFFFFF */

    if ((data == NULL) && (len > 0U)) { return FRAME_CRC32_INIT; }

    for (i = 0U; i < len; i++)
    {
        uint8_t byte_in = data[i];
        crc ^= (uint32_t)byte_in;                   /* XOR into the LOW byte */

        for (bit = 0U; bit < 32U; bit++)            /* 32 iterations, not 8  */
        {
            if ((crc & 0x80000000U) != 0U) { crc = (crc << 1U) ^ FRAME_CRC32_POLY; }
            else                            { crc = crc << 1U; }
        }
    }

    return crc;                                     /* NO final XOR */
}
```

Three deviations from any standard CRC-32, all deliberate:

| Aspect | Standard CRC-32/MPEG-2 | This implementation |
|---|---|---|
| Iterations per byte | 8 | **32** |
| Byte XOR position | high byte (`crc ^= b << 24`) | **low byte** (`crc ^= b`) |
| Final XOR | none for MPEG-2 | none — `FRAME_CRC32_XOROUT` is defined but unused |

The file carries an explicit warning:

```
 * WHY 32 ITERATIONS PER BYTE (NOT 8)?
 *   The inner loop runs 32 iterations to match the teammate's ESP32
 *   calculateCRC32() reference exactly. This produces a result that is
 *   mathematically different from standard MPEG-2 CRC32 (it effectively
 *   processes each byte followed by 24 implicit zero bits). DO NOT CHANGE
 *   TO 8 ITERATIONS — both ends of the link must stay in sync.
```

### 54.1 Where the algorithm came from

The 32-iteration, low-byte-XOR form is exactly what the **STM32 hardware CRC peripheral** computes when fed a byte zero-extended into its 32-bit data register:

```c
/* Bootloader — hardware path */
uint32_t crcBuf = (uint32_t)data[i];   /* zero-extend byte to 32 bits */
CRC_Accumulate(&crcBuf, 1, &calculatedCRC);
```

The hardware unit processes 32 bits per write with polynomial `0x04C11DB7`, init `0xFFFFFFFF`, no reflection, no final XOR. Feeding it `0x000000AB` is identical to processing the byte `0xAB` followed by 24 zero bits.

So the lineage is:

```
   STM32 hardware CRC unit (bootloader, fixed function)
            │
            │  someone wrote a software equivalent for the ESP32
            ▼
   ESP32 calculateCRC32()  — 32 iterations to match the hardware
            │
            │  the STM32 application then had to match the ESP32
            ▼
   Frame_CRC32()  — same 32 iterations
```

The application does **not** use the hardware unit ([§9.2](#92-the-crc-peripheral-is-not-used)), so it reimplements in software what the bootloader gets for free.

### 54.2 Error-detection properties

Processing each byte followed by 24 zero bits does not weaken the CRC — appending zeros to a message is a well-defined CRC operation, and the polynomial's Hamming distance properties are preserved. The effective message is `b₀ 0 0 0 b₁ 0 0 0 …`, which is 4× longer than the real message.

For a 32-byte frame the effective message length is 128 bytes = 1,024 bits. CRC-32 with `0x04C11DB7` guarantees detection of:

- All single-bit errors
- All double-bit errors up to 2^32-ish message lengths
- All odd numbers of bit errors
- All burst errors up to 32 bits
- Longer bursts with probability 1 − 2⁻³²

So it is a genuine CRC-32, just over a padded message. The detection strength is intact.

### 54.3 The cost

32 iterations per byte instead of 8 is a 4× cost. For the largest frame:

```
   Heartbeat: 32 bytes of CRC input × 32 iterations = 1,024 iterations
   At ~5 cycles each ≈ 5,120 cycles ≈ 61 µs at 84 MHz
   At 0.2 Hz → 0.001 % CPU
```

Irrelevant. A table-driven implementation would be 8× faster and need 1 KB of Flash; not worth it.

### 54.4 The unused `XOROUT`

```c
#define FRAME_CRC32_XOROUT    0xFFFFFFFFU
```

Defined in `FRAME.h`, referenced nowhere. It documents an intent that the code does not implement — `Frame_CRC32` returns `crc` directly with the comment `/* No final XOR to match ESP32 receiver */`. Harmless, but a reader implementing a third endpoint from the header alone would apply it and get every CRC wrong.

## 55. The GPIO Sync Line

PA8 is raised around each DMA transmission:

```c
UART_SVC_TransmitDMA(UART1_ID, &buf);
GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_SET);
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_RESET);
```

```
   UART1_TX (PA9)
        ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
   ─────┤  │  │  │  │  │  │  │  │  │  │  │  │  ├────────
        └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘
        ▲                                        ▲
        first bit                                last stop bit

   PA8 sync
         ┌──────────────────────────────────────┐
   ──────┘                                      └───────
         ▲                                      ▲
         raised a few hundred ns AFTER          lowered after the
         the first byte hit the DR              DMA TC interrupt
```

### 55.1 What it actually marks

Not the frame start. The sequence is:

1. `UART_SVC_TransmitDMA` configures and starts DMA2 Stream 7.
2. The DMA controller immediately transfers byte 0 to `USART1->DR`.
3. USART1 begins shifting out the start bit.
4. Thread 3 returns from `UART_SVC_TransmitDMA` and calls `GPIO_WritePin`.

Steps 2–3 happen in hardware while step 4 is still executing C. So PA8 rises **after** transmission has begun — by roughly 200–500 ns, which at 115200 baud is about 3–6 % of one bit time.

In practice the rising edge is still within the first bit, so a receiver sampling PA8 would see it go high essentially coincident with the start bit. It works as a frame marker despite the ordering, by a margin of a few hundred nanoseconds.

Moving the `GPIO_WritePin` above the `UART_SVC_TransmitDMA` call would make it unambiguous and costs nothing.

### 55.2 The falling edge is the more useful one

PA8 falls after the DMA transfer-complete interrupt, which fires when the last byte has been written to `DR` — **not** when it has finished shifting out. There is up to one byte time (86.8 µs) of transmission remaining after PA8 goes low.

So:

| Edge | Marks | Accuracy |
|---|---|---|
| Rising | Frame start | ~300 ns late |
| Falling | Frame end | ~87 µs early |

Neither is exact, and the falling edge is off by three orders of magnitude more than the rising one.

### 55.3 Does the gateway use it?

The ESP32 gateway's UART2 receive path parses frames purely from the byte stream — reading a length byte, then that many bytes, then verifying the CRC. **There is no GPIO input configured on the gateway side for PA8.**

So the sync line is, at present, **write-only**. It exists in the STM32's design and is not consumed.

That does not make it useless — it is genuinely valuable on the bench. On a two-channel logic analyser with PA9 on one channel and PA8 on the other, PA8 gives instant visual frame boundaries without decoding UART. Measuring the pulse width gives the frame duration, from which the frame size follows.

But it should be understood as a **debug aid**, not a protocol element. The `FRAME.c` comment presenting it as the reason there is no SYNC byte ([§47.2](#472-there-is-no-sync-byte-outbound)) overstates its role: the receiver works entirely without it.

### 55.4 It doubles as a Thread 3 activity indicator

```
   PA8 duty cycle ≈ (frames/s × frame_time) / 1 s
                  ≈ (9 × 1.5 ms) / 1000 ms
                  ≈ 1.4 %
```

A scope on PA8 showing no activity means Thread 3 is not running — which, given Thread 3 is watchdog-monitored, means a reset is imminent. A PA8 stuck **high** means Thread 3 is blocked in `ulTaskNotifyTake` waiting for a DMA completion that never came ([§26.3](#263-the-unbounded-wait)) — a diagnosis available at a glance.

## 56. Inbound Bootloader Command

The only thing the node ever receives.

```
   ESP32                                    STM32 (application running)
     │                                              │
     │  0xAA 0xEB                                   │
     ├─────────────────────────────────────────────►│  USART1 RXNE ISR ×2
     │                                              │  → rx_push into the 256 B ring
     │                                              │  → vTaskNotifyGiveFromISR(Thread 5)
     │                                              │
     │                                              │  Thread 5 (priority 4) preempts
     │                                              │  drains the ring, matches AA then EB
     │                                              │
     │  0xEE 0xAA                                   │  UART_Transmit_Polling — waits for TC
     │◄─────────────────────────────────────────────┤
     │                                              │  *(0x2000FFF8) = 0xDEADBEEF
     │                                              │  SCB->AIRCR = 0x05FA0004
     │                                              │  ═══ reset ═══
     │                                              │
     │  (node is now in the bootloader)             │
```

### 56.1 The reply is `0xEE 0xAA`

```c
static uint8_t ack_packet[2] = {0xEEU, 0xAAU};
```

`0xEE` is the bootloader protocol's `BL_REPLAY_START_BYTE`, and `0xAA` is `BOOTLOADER_ACKNOWLEDGE`. So the application **impersonates the bootloader's reply format** — it answers a bootloader-protocol probe with a bootloader-protocol reply.

That is what lets the gateway use one code path regardless of which firmware is running:

```c
/* ESP32, Send_CMD_Enter_Bootloader */
ReceiveReplayFromBootloader(1, 1000);      /* waits for 0xEE, then reads 1 byte */

if(packet[0] == BOOTLOADER_ACKNOWLEDGE){         /* 0xAA — the APP answered */
    in_bootloader_mode = true;
    printk("[Bootloader] Entering Bootloader...\n");
}
else if(packet[0] == BOOTLOADER_ALREADY_IN){     /* 0xFB — the BOOTLOADER answered */
    in_bootloader_mode = true;
    printk("[Bootloader] We are already in Bootloader\n");
}
```

| Who answered | Reply | Gateway's interpretation |
|---|---|---|
| Application (Thread 5) | `0xEE 0xAA` | "It was running the app; it is now resetting into the bootloader" |
| Bootloader | `0xEE 0xFB` | "It was already in the bootloader; no reset occurred" |

The distinction matters because the first case implies a ~50 ms reset delay before the node is ready for the next command.

### 56.2 Why not a FRAME

`FRAME_TYPE_BL_ENTER` (`0xFE`) and `FRAME_INBOUND_SYNC_BYTE` (`0xAA`) exist for a design in which the inbound command would have been a full CRC-protected frame. It was not built, and the raw two-byte form is better for this purpose:

- **The bootloader must understand the same command.** The bootloader has no FRAME parser and cannot grow one — it is a 32 KB image already two-thirds full of mbedTLS.
- **A two-byte sequence needs no length or CRC.** The state machine's re-check on mismatch ([§28.1](#281-the-state-machine)) provides all the robustness required for a two-byte pattern.
- **Symmetry across the two firmwares is the whole point.** The gateway sends the same two bytes and gets a compatible reply either way.

### 56.3 The false-trigger question

`0xAA 0xEB` could in principle appear in noise. What would happen?

```
   Spurious 0xAA on the line → Thread 5 → state 1
   Spurious 0xEB next        → Thread 5 → ENTER BOOTLOADER → reset
```

Two specific bytes in sequence. The probability of random noise producing exactly this is 2⁻¹⁶ per byte pair, and the RX line has a pull-up ([§8.1](#81-pull-configuration-rationale)) that keeps it idle-high when disconnected — so there is no ongoing stream of random bytes to sample from.

The real risk is not noise but **a desynchronised gateway** transmitting mid-stream data that happens to contain `AA EB`. Since the gateway only ever sends these two bytes, that cannot occur.

There is no CRC, no timeout between the two bytes, and no confirmation step. A `0xAA` received at t=0 and a `0xEB` at t=10 minutes would still trigger. Adding a short inter-byte timeout (say 100 ms) would tighten this at the cost of a timer.

---

# Part VII — Driver Layer

## 57. Driver Layering

```
   ┌────────────────────────────────────────────────────────────┐
   │  Application                                                │
   │    main.c   tasks, ISR callbacks, initialisation            │
   │    FRAME.c  logger.c                                        │
   │    ML pipeline: scale, features, quantize, inference, vote  │
   └───────────────────────┬────────────────────────────────────┘
                           │
   ┌───────────────────────▼────────────────────────────────────┐
   │  Device drivers                                             │
   │    MPU6050.c    IMU over I2C_SERVICE                        │
   │    HCSR04.c     trigger pulse over GPIO                     │
   │    RING_BUFFER.c  SPSC queue for samples                    │
   └───────────────────────┬────────────────────────────────────┘
                           │
   ┌───────────────────────▼────────────────────────────────────┐
   │  Service layers  (buffering, DMA orchestration, callbacks)  │
   │    I2C_SERVICE.c   two-phase IRQ-TX → DMA-RX                │
   │    UART_SERVICE.c  TX/RX rings, DMA TX, RX notification     │
   └───────────────────────┬────────────────────────────────────┘
                           │
   ┌───────────────────────▼────────────────────────────────────┐
   │  Peripheral drivers  (register level, one per peripheral)   │
   │    RCC  GPIO  NVIC  DWT  TIM  ADC  I2C  DMA  UART  FLASH    │
   │    IWDG                                                     │
   └───────────────────────┬────────────────────────────────────┘
                           │
   ┌───────────────────────▼────────────────────────────────────┐
   │  Register maps  *_REGS.h                                    │
   │    Hand-written bitfield structs, no CMSIS device header    │
   └────────────────────────────────────────────────────────────┘
```

Two conventions run through the whole layer:

**Out-parameters for queries.** Every boolean or value query returns a status code and writes the answer through a pointer:

```c
TIM_Error_t  TIM_IsRunning(TIM_Id_t id, uint8_t *state);
UART_Error_t UART_IsTxEmpty(UART_Id_t uart, uint8_t *state);
I2C_Error_t  I2C_IsBusy(I2C_Id_t id, uint8_t *state);
NVIC_ERROR_t NVIC_GetPriority(IRQn_t IRQn, uint8_t *priority);
```

The rationale is spelled out in the headers, e.g. `TIM_INTERFACE.h`:

```
 * T-I03: Out-param pattern on all boolean queries — 0U return is ambiguous
 *        between "condition false" and "invalid ID".
```

Verbose at call sites, but unambiguous.

**Explicit enumerator values.** Every enum assigns explicit values with a MISRA rule citation:

```c
typedef enum {
    UART_OK                  = 0x00U,  /* U-04/U-05: explicit value — MISRA Rule 8.12 */
    UART_ERROR_INVALID_PARAM = 0x01U,
    ...
} UART_Error_t;
```

`RCC_INTERFACE.h` documents why this is not merely stylistic:

```
 * All values explicit — no implicit increment.
 * Implicit increment was the root cause of PLLM_16 resolving
 * to 13 instead of 16, causing VCO_INPUT = 1230769 Hz
 * instead of 1000000 Hz and failing the range check.
```

A real bug that the convention now prevents.

## 58. RCC Driver

`src/RCC.c` (545 lines), `include/RCC_INTERFACE.h`, `include/RCC_REGS.h`.

```c
RCC_Error_t RCC_SET_SYSCLK(RCC_SYSCLK_TYPE_t clk_type);
RCC_Error_t RCC_CTL_CLK(RCC_SYSCLK_TYPE_t clk_type, RCC_CLK_CTRL_t state);
RCC_Error_t RCC_PLL_CFG(PLL_CFG_t *PLL_CFG);
RCC_Error_t RCC_PLL_Enable(void);
RCC_Error_t RCC_EN_CLK_PERIPHERAL(RCC_Peripheral_t peri_clk);
RCC_Error_t RCC_DisablePeripheralClock(RCC_Peripheral_t peri_clk);
RCC_Error_t get_sys_clk(RCC_SYSCLK_TYPE_t *clk_src);
RCC_Error_t GET_AHB_FREQ(uint32_t *freq);
RCC_Error_t GET_PCLK_FREQ(RCC_Peripheral_t peri, uint32_t *freq);
RCC_Error_t RCC_GET_PLL_FREQ(uint32_t *freq);
RCC_Error_t RCC_GET_SYSCLK_FREQ(uint32_t *freq);
RCC_Error_t RCC_INIT_84MHz_HSI(void);
RCC_Error_t RCC_LSI_Enable(void);
```

`main()` uses exactly three: `RCC_INIT_84MHz_HSI`, `RCC_EN_CLK_PERIPHERAL` and `RCC_LSI_Enable`.

### 58.1 Peripheral ID encoding

```
   bits [6:5] = bus index      bits [4:0] = ENR bit position

   0x00 range → AHB1
   0x20 range → AHB2
   0x40 range → APB1
   0x60 range → APB2
```

| Constant | Value | Decodes to |
|---|---|---|
| `PERIPH_GPIOA` | `0x00` | AHB1ENR bit 0 |
| `PERIPH_DMA1` | `0x15` | AHB1ENR bit 21 |
| `PERIPH_DMA2` | `0x16` | AHB1ENR bit 22 |
| `PERIPH_TIM2` | `0x40` | APB1ENR bit 0 |
| `PERIPH_I2C1` | `0x55` | APB1ENR bit 21 |
| `PERIPH_USART1` | `0x64` | APB2ENR bit 4 |
| `PERIPH_ADC1` | `0x68` | APB2ENR bit 8 |

This is a **different encoding** from the bootloader's, which uses the top two bits for the bus and a full bitmask for the peripheral ([`../Bootloader/BOOTLOADER.md`](../Bootloader/BOOTLOADER.md) §45.1). The bootloader's form allows OR-ing multiple peripherals on one bus into a single call; this one does not. Two implementations of the same idea in one project, neither wrong.

### 58.2 The unimplemented prescaler API

```c
/* Reserved for future RCC_SetAPBPrescaler function — not yet implemented.
 * Values are the 3-bit PPRE field encodings from RM0368 §6.3.3, not
 * the actual divisor values. Do not use as divisors directly. */
typedef enum {
    APB_PRESCALER_1  = 0x0U,
    APB_PRESCALER_2  = 0x4U,
    ...
} RCC_APB_PRESCALER_t;
```

The enums exist; the functions do not. Prescalers are set inside `RCC_INIT_84MHz_HSI` with direct register writes. The warning that these are field encodings rather than divisors is exactly the kind of note that prevents a future misuse.

## 59. GPIO Driver

`src/GPIO.c` (523 lines), `include/GPIO_INTERFACE.h`.

```c
GPIO_Error_t GPIO_INIT(GPIO_CONFIG_t *GPIO_Config);
GPIO_Error_t GPIO_DeInit(GPIO_PORT_t Port, GPIO_PIN_t Pin);
GPIO_Error_t GPIO_ReadPin(GPIO_PORT_t Port, GPIO_PIN_t Pin, uint8_t *State);
GPIO_Error_t GPIO_WritePin(GPIO_PORT_t Port, GPIO_PIN_t Pin, GPIO_PinState_t State);
GPIO_Error_t GPIO_TogglePin(GPIO_PORT_t Port, GPIO_PIN_t Pin);
GPIO_Error_t GPIO_LockPin(GPIO_PORT_t Port, GPIO_PIN_t Pin);
GPIO_Error_t GPIO_UnlockPin(GPIO_PORT_t Port, GPIO_PIN_t Pin);
GPIO_Error_t GPIO_ReadPinFast(GPIO_PORT_t Port, GPIO_PIN_t Pin, uint8_t *state);
```

Unlike the bootloader's GPIO driver, which takes a `GPIO_t*` handle, this one takes (port, pin) pairs for every operation. Stateless and simpler.

### 59.1 The alternate-function table

```c
typedef enum {
    AF_SYSTEM    = 0U,
    AF_TIM_1_2   = 1U,
    AF_TIM_3_5   = 2U,
    AF_TIM_9_11  = 3U,
    AF_I2C_1_3   = 4U,
    AF_SPI_1_4   = 5U,
    AF_SPI_3     = 6U,
    AF_USART_1_2 = 7U,
    AF_USART_6   = 8U,
    AF_I2C_2_3   = 9U,
    AF_OTG_FS    = 10U,
    AF_SDIO      = 12U,  /* G-09: SDIO is AF12, not sequential 11 */
    AF_EVENTOUT  = 15U   /* G-09: EVENTOUT is AF15, not sequential 12 */
} GPIO_AlternateFunction_t;
```

The `G-09` comments record a fixed bug: implicit incrementing had assigned `AF_SDIO = 11` and `AF_EVENTOUT = 12`, both wrong. AF11 is `ETH` (not present on the F401) and AF13/AF14 are reserved. The explicit values match RM0368's AF mapping table.

The three AF values this project uses (`AF_I2C_1_3 = 4`, `AF_TIM_3_5 = 2`, `AF_USART_1_2 = 7`) were all correct even under implicit incrementing, so the bug never manifested here — it was caught by review, not by failure.

### 59.2 `GPIO_ERROR_PORT_NOT_ENABLED` is not implemented

```c
/* G-11: Reserved — clock enablement check not implemented in GPIO driver;
   caller must enable port clock via RCC before calling GPIO_INIT */
GPIO_ERROR_PORT_NOT_ENABLED = 0x08U,
```

Configuring a GPIO whose port clock is gated silently does nothing — the register writes are discarded. This is one of the most common bring-up mistakes on STM32, and the driver explicitly declines to catch it.

The check would be one read of `RCC->AHB1ENR`. Given how cheap it is and how confusing the failure is (a pin that simply does not respond, with no error anywhere), implementing it would be worthwhile. `main()` gets the ordering right, so it has never bitten here.

## 60. NVIC Driver

`src/NVIC.c` (478 lines), `include/NVIC_INTERFACE.h`.

```c
NVIC_ERROR_t NVIC_EnableIRQ(IRQn_t IRQn);
NVIC_ERROR_t NVIC_DisableIRQ(IRQn_t IRQn);
NVIC_ERROR_t NVIC_SetPendingIRQ(IRQn_t IRQn);
NVIC_ERROR_t NVIC_GetPendingIRQ(IRQn_t IRQn, uint8_t *pending);
NVIC_ERROR_t NVIC_ClearPendingIRQ(IRQn_t IRQn);
NVIC_ERROR_t NVIC_GetActive(IRQn_t IRQn, uint8_t *active);
NVIC_ERROR_t NVIC_SetPriority(IRQn_t IRQn, uint32_t priority);
NVIC_ERROR_t NVIC_GetPriority(IRQn_t IRQn, uint8_t *priority);
NVIC_ERROR_t NVIC_SetPriorityGrouping(uint32_t priority_grouping);
void         NVIC_SystemReset(void) __attribute__((noreturn));
```

The packed `IRQn_t` encoding is described in [§22.3](#223-the-irqn_t-enum-encoding).

`NVIC_SystemReset` is declared `noreturn` — correct and useful, since it lets the compiler elide unreachable code after the call. It is **not used** by the application; Thread 5 writes `AIRCR` directly:

```c
*((volatile uint32_t *)0xE000ED0CU) = (0x05FAUL << 16U) | (1UL << 2U);
```

Presumably because the direct write is self-contained and does not require the header. Using the driver function would be cleaner and would get the `noreturn` benefit.

`NVIC_SetPriority` takes a `uint32_t` priority and is called with raw values 5, 6, 7. It must shift them into the upper 4 bits (`priority << 4`) to match the F4's implemented priority bits — and the resulting behaviour ([§22](#22-interrupt-priority-map)) confirms it does.

## 61. DWT Driver

`src/DWT.c` (137 lines), `include/DWT_INTERFACE.h` (175 lines).

```c
DWT_Status_t DWT_Init(void);
uint32_t     DWT_GetCycles(void);
uint32_t     DWT_ElapsedSince(uint32_t start_cycles);
uint32_t     DWT_CyclesToUs(uint32_t cycles);
uint32_t     DWT_CyclesToNs(uint32_t cycles);
void         DWT_DelayUs(uint32_t us);
```

### 61.1 Initialisation with verification

```c
DWT_Status_t DWT_Init(void)
{
    DWT_DEMCR |= DWT_DEMCR_TRCENA;      /* 1) master trace enable  */
    DWT_CYCCNT = 0U;                     /* 2) known start state    */
    DWT_CTRL  |= DWT_CTRL_CYCCNTENA;     /* 3) enable the counter   */

    uint32_t a = DWT_CYCCNT;             /* 4) verify it runs       */
    uint32_t b = DWT_CYCCNT;

    if (a == b) { return DWT_ERR_NOT_PRESENT; }
    return DWT_OK;
}
```

Step 4 is the good part. The ARMv7-M architecture permits an implementation to omit the cycle counter (`DWT_CTRL.NOCYCCNT` indicates this). Rather than decoding that bit, the driver reads the counter twice — at 84 MHz the counter advances every 11.9 ns, so two consecutive `LDR` instructions (each ≥ 2 cycles) are guaranteed to see different values if the counter is running.

Cheaper than decoding `NOCYCCNT` and it also catches the case where the counter exists but is locked by a debug authority.

**The return value is discarded.** `vConfigureTimerForRunTimeStats` calls `DWT_Init()` and ignores it:

```c
void vConfigureTimerForRunTimeStats(void){ DWT_Init(); }
```

If the counter were unavailable, `ulGetRunTimeCounterValue()` would return a constant, every `dt_total` would be zero, and the `if (dt_total > 0U)` guard in Thread 4 would skip the CPU calculation entirely — leaving all six CPU figures at their previous values forever. Degraded but not fatal, and silent.

### 61.2 Exact nanosecond conversion

```c
#define DWT_NS_NUMERATOR      250UL
#define DWT_NS_DENOMINATOR    21UL

uint32_t DWT_CyclesToNs(uint32_t cycles){
    return (cycles * DWT_NS_NUMERATOR) / DWT_NS_DENOMINATOR;
}
```

`1000 / 84 = 250 / 21` after dividing both by 4 — an exact reduction that avoids 64-bit arithmetic. The header documents the overflow bound precisely:

```
 *   cycles * 250 must fit in uint32_t
 *   => cycles_max = 0xFFFFFFFF / 250 = 17,179,869
 *   => ~204 ms at 84 MHz before this function overflows.
```

Neither `DWT_CyclesToNs` nor `DWT_CyclesToUs` is called anywhere. Thread 2 divides by a hardcoded 84 ([§46.2](#462-the--84u-and-uint16_t-truncation)).

### 61.3 `DWT_DelayUs`

```c
void DWT_DelayUs(uint32_t us){
    if (us == 0U) { return; }
    uint32_t start = DWT_CYCCNT;
    uint32_t target_cycles = us * DWT_CYCLES_PER_US;
    while ((DWT_CYCCNT - start) < target_cycles) { }
}
```

Cycle-accurate to about ±1 cycle, and correct across a counter wrap because the subtraction is modular.

Also unused — `HCSR04_Trigger` rolls its own inaccurate volatile loop ([§30.5](#305-the-10-µs-trigger-pulse)) and `MPU6050_SpinDelay` does the same. Both would be better served by this function. Three delay implementations in one codebase, one of them correct and unused.

## 62. TIM Driver

`src/TIM.c` (1,216 lines), `include/TIM_INTERFACE.h` (527 lines).

```c
TIM_Error_t TIM_Init(const TIM_Config_t *cfg);
TIM_Error_t TIM_IC_Init(const TIM_IC_Config_t *cfg);
TIM_Error_t TIM_IC_GetCapture(TIM_Id_t id, TIM_Channel_t channel, uint32_t *val);
TIM_Error_t TIM_DeInit(TIM_Id_t id);
TIM_Error_t TIM_Start(TIM_Id_t id);
TIM_Error_t TIM_Stop(TIM_Id_t id);
TIM_Error_t TIM_Reset(TIM_Id_t id);
TIM_Error_t TIM_GetCounter(TIM_Id_t id, uint32_t *count);
TIM_Error_t TIM_SetCounter(TIM_Id_t id, uint32_t value);
TIM_Error_t TIM_SetPrescaler(TIM_Id_t id, uint32_t psc);
TIM_Error_t TIM_SetPeriod(TIM_Id_t id, uint32_t arr);
TIM_Error_t TIM_IsRunning(TIM_Id_t id, uint8_t *state);
TIM_Error_t TIM_GetUpdateFlag(TIM_Id_t id, uint8_t *state);
TIM_Error_t TIM_ClearUpdateFlag(TIM_Id_t id);
TIM_Error_t TIM_EnableUpdateIRQ(TIM_Id_t id);
TIM_Error_t TIM_DisableUpdateIRQ(TIM_Id_t id);
TIM_Error_t TIM_RegisterCallback(TIM_Id_t id, TIM_Callback_t cb, void *ctx);
```

The header is unusually well documented — 527 lines for 17 functions, with numbered design notes (`T-I01` through `T-I08`) that read as the output of a design review. Four of them are worth reproducing because they encode real hardware traps.

### 62.1 T-I01 — PSC and ARR are raw register values

```
 * T-I01: PSC and ARR store (value), not (value+1).
 *        Hardware adds 1 internally: actual period = (PSC+1)(ARR+1) / TIM_CLK.
 *        TIM_PSC_100HZ = 8399U means PSC register = 8399, actual divisor = 8400.
 *        Do not subtract 1 before writing — TIM_Init writes the value directly.
```

```
   100 Hz derivation:
     TIM_CLK = 84 MHz
     PSC = 8399  →  tick = 84,000,000 / 8400 = 10,000 Hz
     ARR = 99    →  period = 10,000 / 100    = 100 Hz  ✓
```

### 62.2 T-I02 — EGR.UG after writing PSC/ARR

```
 * T-I02: EGR.UG must be fired after writing PSC and ARR, before CEN.
 *        Without it, the first period uses the reset value of the shadow
 *        registers, not the configured PSC/ARR. This causes a one-shot
 *        timing error on the very first tick only — hard to catch in testing.
```

PSC and ARR are shadowed. Writing them loads the preload registers; the shadow registers update only on the next update event. Without a software-forced update (`EGR.UG = 1`), the first period runs with the reset values (PSC = 0, ARR = 0xFFFF) — at 84 MHz that is a 780 µs first tick instead of 10 ms.

A one-shot 9.2 ms error at startup. Precisely the kind of bug that shows up as "the first sample is weird" and gets dismissed.

### 62.3 T-I04 — SR.UIF is rc_w0

```
 * T-I04: SR.UIF is rc_w0: hardware sets it, software clears by writing 0.
 *          TIM2->SR.ALL &= ~(1U << 0U);    ← correct
 *          TIM2->SR.ALL = 0U;               ← also acceptable
 *        NOT: TIM2->SR.ALL |= (1U << 0U);  ← this is a no-op, UIF stays set,
 *                                             ISR re-enters immediately.
```

`rc_w0` (read-clear, write-0) is the opposite of the more common `rc_w1`. Writing 1 to a `rc_w0` bit has no effect. Getting this backwards produces an ISR that re-enters immediately on return — the interrupt storm that locks the core with no obvious cause.

### 62.4 T-I05 — TIM_Init does not touch the NVIC

```
 * T-I05: TIM_Init does NOT enable the NVIC line. Caller must call
 *        NVIC_EnableIRQ(TIM2) after TIM_Init.
```

Which `main()` duly does:

```c
TIM_Init(&tim_cfg);
NVIC_SetPriority(TIM2, 5U);
NVIC_EnableIRQ(TIM2);
```

The separation keeps the TIM driver independent of the NVIC driver.

### 62.5 The input-capture gap

`TIM_IC_Config_t` has a `polarity` field, but there is **no function to change polarity after init**. The HC-SR04 ISR needs to flip it on every edge, so it reaches into the registers directly with a hardcoded base address ([§30.1](#301-the-capture-isr)):

```c
volatile TIM_REGS_t *tim3_regs = (volatile TIM_REGS_t *)0x40000400UL;
tim3_regs->CCER.ALL |= (1U << (cc_shift + 1U));
```

A `TIM_IC_SetPolarity(id, channel, polarity)` would close the gap and remove both the magic address and the `#undef TIM2/TIM3/TIM4/TIM5` workaround in `main.c`. Recorded in [§82](#82-known-gaps).

## 63. ADC Driver

`src/ADC.c` (127 lines) — the smallest peripheral driver.

```c
ADC_Error_t ADC_Init(const ADC_Config_t *cfg);
ADC_Error_t ADC_Read(uint16_t *raw_out);
uint16_t    ADC_LM35_ToTenthsCelsius(uint16_t raw, uint16_t vref_mv);
```

Single-channel, single-conversion, software-triggered, polled. No DMA, no interrupts, no scan mode.

```c
ADC_CCR->BITS.ADCPRE = 0x1U;       /* PCLK2 / 4 = 21 MHz  (max 36 MHz) */
ADC1->CR2.BITS.ADON  = 0U;         /* disable before configuring       */
ADC1->CR1.BITS.RES   = resolution;
ADC1->CR2.BITS.CONT  = 0U;         /* single conversion                */
ADC1->CR2.BITS.EXTEN = 0U;         /* no external trigger              */
ADC1->CR2.BITS.ALIGN = 0U;         /* right-aligned                    */
adc_set_sample_time(cfg->channel, cfg->sample_time);
ADC1->SQR1.BITS.L    = 0U;         /* sequence length 1                */
ADC1->SQR3.BITS.SQ1  = cfg->channel;
ADC1->CR2.BITS.ADON  = 1U;         /* power on                         */
for (volatile uint32_t d = 0U; d < 1000U; d++) { __asm("nop"); }   /* t_STAB */
```

### 63.1 The sample-time register split

```c
static void adc_set_sample_time(uint8_t ch, ADC_SampleTime_t smp)
{
    if (ch <= 9U) {
        uint32_t shift = (uint32_t)ch * 3U;
        ADC1->SMPR2.ALL = (ADC1->SMPR2.ALL & ~(0x7UL << shift)) | (smp << shift);
    } else if (ch <= 18U) {
        uint32_t shift = ((uint32_t)ch - 10U) * 3U;
        ADC1->SMPR1.ALL = (ADC1->SMPR1.ALL & ~(0x7UL << shift)) | (smp << shift);
    }
}
```

Channels 0–9 in `SMPR2`, channels 10–18 in `SMPR1`, three bits each. The apparent inversion (higher channels in the lower-numbered register) is how ST defined it.

Note a channel of 19 or above silently does nothing — `ADC_Init` validates `cfg->channel > 18U` before calling, so it is unreachable.

### 63.2 The stabilisation delay

```c
for (volatile uint32_t d = 0U; d < 1000U; d++) { __asm("nop"); }
```

The F401 datasheet specifies `t_STAB` = 3 µs after setting `ADON`. This loop is 1,000 iterations of load/nop/increment/store/compare/branch — roughly 6 cycles each = 6,000 cycles = **71 µs**, 24× the requirement.

Massively over-specified, but it runs once at boot and costs nothing. `DWT_DelayUs(10)` would be clearer — except `DWT_Init()` has not run at that point in `main()`, so it would not work. That is a genuine ordering constraint and probably why the spin loop is there.

### 63.3 Clearing EOC before starting

```c
if (ADC1->SR.BITS.OVR) { ADC1->SR.BITS.OVR = 0U; }
ADC1->SR.BITS.EOC = 0U;
ADC1->CR2.BITS.SWSTART = 1U;
```

Clearing `EOC` before triggering prevents the poll loop from immediately seeing a stale flag from the previous conversion and returning the previous value. Reading `DR` also clears `EOC` in hardware, so under normal operation the flag would already be clear — this is belt-and-braces against a path where `ADC_Read` returned early on timeout without reading `DR`.

The `OVR` clear matters more: overrun is sticky, and once set, conversions stop being written to `DR`. Clearing it each time makes the driver self-healing after a missed read.

## 64. I2C Driver and Service

`src/I2C.c` (1,232 lines) and `src/I2C_SERVICE.c` (492 lines). The most complex driver pair in the project.

### 64.1 The two-phase read

An I2C register read is inherently two transactions:

```
   ┌─────────────────────────────────────────────────────────────────┐
   │ Phase 1 — IRQ-driven TX                                          │
   │   START → addr(W) → reg_addr → (no STOP)                        │
   │   Driven by I2C1_EV interrupts through the I2C driver's          │
   │   internal state machine.                                        │
   └───────────────────────────┬─────────────────────────────────────┘
                               │  I2C_EVENT_TX_COMPLETE
                               ▼
   ┌─────────────────────────────────────────────────────────────────┐
   │ Phase 2 — DMA RX                                                 │
   │   repeated START → addr(R) → DMA1_S0 pulls N bytes → NACK+STOP  │
   │   Configured in the TX_COMPLETE callback, then started.          │
   └───────────────────────────┬─────────────────────────────────────┘
                               │  DMA1_S0 transfer complete
                               ▼
                    user callback (ISR context)
```

The transition happens inside `i2c_svc_ev_callback`:

```c
static void i2c_svc_ev_callback(I2C_Event_t event, void *ctx)
{
    if (event == I2C_EVENT_TX_COMPLETE)
    {
        if (svc[id].rx_buf_pending == NULL) {
            svc[id].state = I2C_SVC_STATE_IDLE;      /* write-only, done */
            return;
        }

        svc[id].state = I2C_SVC_STATE_RX_DMA;

        DMA_Config_t dma_cfg;
        dma_cfg.direction          = DMA_DIR_PERIPH_TO_MEM;
        dma_cfg.peripheral_address = I2C_GetDRAddress(id);
        dma_cfg.memory0_address    = (uint32_t)svc[id].rx_buf_pending;
        dma_cfg.length             = svc[id].rx_len_pending;
        dma_cfg.complete_callback  = i2c_svc_dma_rx_complete;
        dma_cfg.error_callback     = i2c_svc_dma_rx_complete;
        /* ... */

        if (DMA_Init(&dma_cfg) != DMA_OK) { svc[id].state = I2C_SVC_STATE_ERROR; return; }

        I2C_EnableDMALast(id);          /* CR2.LAST — NACK + STOP after last byte */
        /* ... repeated START, address ... */
        DMA_Start(svc[id].dma_id, svc[id].dma_stream);
    }
}
```

### 64.2 `CR2.LAST` is the critical bit

```c
I2C_EnableDMALast(id);  /* set CR2.LAST — NACK + STOP after last DMA byte */
```

In an I2C master read, the master must NACK the final byte to tell the slave to stop driving, then issue STOP. With DMA, the CPU is not in the loop and cannot count bytes.

`CR2.LAST` delegates this to the hardware: when the DMA's transfer-complete signal asserts, the I2C peripheral automatically generates NACK and STOP. Without it, the master would ACK the last byte, the slave would send another, and the bus would desynchronise.

This is one of the two classic STM32F4 I2C-DMA traps.

### 64.3 The single-byte DMA workaround

The other trap, documented at length in `MPU6050.c`:

```c
/* FIX (Bug 5 — single-byte DMA):
 *  Buffer is 2 bytes, NOT 1. STM32F4 I2C cannot reliably use
 *  DMA for single-byte receives — the ADDR handler generates
 *  STOP before DMA can latch the transfer (RM0368 §27.3.3).
 *  Reading 2 bytes takes the multi-byte ADDR path, letting
 *  DMA and CR2.LAST handle NACK+STOP correctly. The second
 *  byte (register 0x76) is harmless and discarded.
 */
static uint8_t whoami_buf[2U] = {0U, 0U};
```

RM0368 §27.3.3 specifies three different sequences for master reception depending on the byte count: N = 1, N = 2, and N > 2. The N = 1 case requires the NACK to be set *before* clearing `ADDR` and the STOP *immediately* after — a window too tight for DMA to be armed in.

Reading two bytes takes the N = 2 path, which DMA handles correctly. The second byte (register 0x76) is read and thrown away.

This is exactly the kind of hardware erratum that costs days to diagnose and is invisible in the code without the comment. The comment is the artefact that makes it maintainable.

### 64.4 Service-layer state machine

```c
typedef enum {
    I2C_SVC_STATE_IDLE   = 0x00U,
    I2C_SVC_STATE_TX     = 0x01U,   /* IRQ TX phase active */
    I2C_SVC_STATE_RX_DMA = 0x02U,   /* DMA RX phase active */
    I2C_SVC_STATE_ERROR  = 0x03U
} I2C_SVC_State_t;
```

```
                   I2C_SVC_ReadBurst_DMA
                            │
   ┌──────┐                 ▼               ┌────────┐
   │ IDLE │────────────────────────────────►│   TX   │
   └──────┘                                 └───┬────┘
      ▲                                         │ TX_COMPLETE
      │                                         ▼
      │                                    ┌──────────┐
      │◄───────────────────────────────────│  RX_DMA  │
      │        DMA transfer complete       └────┬─────┘
      │                                         │ DMA error / NACK
      │                                         ▼
      │                                    ┌─────────┐
      └────────────────────────────────────│  ERROR  │
                     (via re-init)          └─────────┘
```

Unlike the bootloader's OTA engine ([`../Bootloader/BOOTLOADER.md`](../Bootloader/BOOTLOADER.md) §35.1), this state machine *does* return to `IDLE` on both success and failure paths, so a NACK does not permanently wedge the driver.

But `MPU6050.c`'s own `mpu_busy` flag does not have that property:

```c
static volatile uint8_t mpu_busy = 0U;
...
mpu_busy = 1U;
I2C_SVC_ReadBurst_DMA(..., mpu_dma_done, NULL);
```

`mpu_busy` is cleared only in `mpu_dma_done`, which runs only on DMA completion or error. If the transfer never completes at all — a genuinely stuck bus where the TX phase never finishes — neither callback fires and `mpu_busy` stays set forever. Every subsequent `MPU6050_TriggerRead` returns `MPU6050_ERROR_BUSY`.

That is the mechanism behind the "no recovery from a stuck bus" observation in [§24.2](#242-live-mode).

### 64.5 Blocking writes at init

```c
I2C_SVC_Error_t I2C_SVC_WriteReg(I2C_Id_t id, I2C_DevAddr7_t dev_addr,
                                  const uint8_t *tx_buf, uint16_t tx_len,
                                  uint32_t timeout);
```

Used only by `MPU6050_Init` for the five configuration writes. It starts an IRQ-driven transmit and then spins on `I2C_GetXferStatus()` with a decrementing counter.

Blocking is acceptable because it happens before the scheduler starts. The 5,000,000 timeout passed by `main()` is a loop count, roughly 300 ms at 84 MHz — comfortable for a two-byte write that takes ~60 µs.

## 65. DMA Driver

`src/DMA.c` (685 lines), `include/DMA_INTERFACE.h` (194 lines).

```c
DMA_Error_t DMA_Init(const DMA_Config_t *cfg);
DMA_Error_t DMA_Start(DMA_Id_t dma, DMA_StreamId_t stream);
DMA_Error_t DMA_Stop(DMA_Id_t dma, DMA_StreamId_t stream);
DMA_Error_t DMA_GetRemaining(DMA_Id_t dma, DMA_StreamId_t stream, uint16_t *remaining);
DMA_Error_t DMA_UpdateMemoryAddress(DMA_Id_t dma, DMA_StreamId_t stream, uint32_t new_address);
DMA_Error_t DMA_GetTransferStatus(DMA_Id_t dma, DMA_StreamId_t stream, DMA_TransferStatus_t *status);
DMA_Error_t DMA_SetCallback(DMA_Id_t dma, DMA_StreamId_t stream,
                            DMA_Callback_t complete_cb, DMA_Callback_t error_cb, void *ctx);
```

Plus all sixteen IRQ handler prototypes, declared in the header so the linker can bind them to the vector table:

```c
void DMA1_Stream0_IRQHandler(void);
...
void DMA2_Stream7_IRQHandler(void);
```

The comment explains why they are in the header:

```
/* D-17: prototypes added — IRQ handlers must be declared in the header so the
   linker can verify the definition matches the NVIC vector table symbol. */
```

Strictly the linker binds by symbol name regardless; what the declarations buy is a compiler warning if a definition's signature drifts (`void (*)(void)` versus anything else).

### 65.1 Full configuration surface

```c
typedef struct {
    DMA_Id_t         dma_id;
    DMA_StreamId_t   stream_id;
    uint32_t         peripheral_address;
    uint32_t         memory0_address;
    uint32_t         memory1_address;    /* double-buffer mode only */
    uint16_t         length;
    DMA_Direction_t  direction;
    DMA_DataWidth_t  periph_width;
    DMA_DataWidth_t  mem_width;
    DMA_Increment_t  periph_inc;
    DMA_Increment_t  mem_inc;
    DMA_Mode_t       mode;
    DMA_Priority_t   priority;
    DMA_Channel_t    channel;
    DMA_Callback_t   half_callback;
    DMA_Callback_t   complete_callback;
    DMA_Callback_t   error_callback;
    DMA_FlowController_t flow_controller;
    void            *user_context;
} DMA_Config_t;
```

Both users configure similarly:

| Field | I2C1 RX | USART1 TX |
|---|---|---|
| `direction` | `PERIPH_TO_MEM` | `MEM_TO_PERIPH` |
| `periph_width` / `mem_width` | BYTE / BYTE | BYTE / BYTE |
| `periph_inc` / `mem_inc` | DISABLE / ENABLE | DISABLE / ENABLE |
| `mode` | NORMAL | NORMAL |
| `priority` | HIGH | HIGH |
| `flow_controller` | DMA | DMA |
| `half_callback` | NULL | NULL |

`DMA_MODE_CIRCULAR` and `DMA_MODE_DOUBLE_BUFFER` are supported by the driver and unused.

### 65.2 Re-init per transfer

`UART_SVC_TransmitDMA` calls `DMA_Init` on **every** frame:

```c
/* Re-init DMA with correct length — DMA_Start only reloads NDTR from stored config.
   We must update stored config length before calling DMA_Start. */
DMA_Config_t dma_cfg;
/* ... 18 fields ... */
dma_cfg.length = buf->length;
if (DMA_Init(&dma_cfg) != DMA_OK) { ... }
```

The comment explains the constraint: `DMA_Start` reloads `NDTR` from the driver's stored config, so changing the transfer length requires re-initialising.

That means ~40 register writes per frame instead of the 2 that would be needed (`M0AR` and `NDTR`). At 9 frames/s and perhaps 200 cycles per re-init, that is 1,800 cycles/s — 0.002 % CPU. Wasteful in principle, invisible in practice.

`DMA_UpdateMemoryAddress` exists and is called immediately before the re-init:

```c
DMA_UpdateMemoryAddress(inst->dma_id, inst->dma_stream, (uint32_t)buf->data);
/* ... then DMA_Init overwrites it anyway ... */
```

Redundant — the subsequent `DMA_Init` sets `memory0_address` from the same value. Harmless.

A `DMA_SetLength(dma, stream, len)` would make the redundancy unnecessary.

## 66. UART Driver and Service

`src/UART.c` (708 lines) and `src/UART_SERVICE.c` (547 lines).

### 66.1 Driver API

```c
UART_Error_t UART_Init(const UART_Config_t *cfg);
UART_Error_t UART_WriteDR(UART_Id_t uart, uint16_t data);
UART_Error_t UART_ReadDR(UART_Id_t uart, uint16_t *data);
uint32_t     UART_GetDRAddress(UART_Id_t uart);
UART_Error_t UART_Transmit_Polling(UART_Id_t uart_id, const Buffer_t *buf, uint32_t timeout);
UART_Error_t UART_Receive_Polling(UART_Id_t uart_id, Buffer_t *buf, uint32_t timeout);
UART_Error_t UART_EnableTxIRQ / DisableTxIRQ / EnableTxCompleteIRQ / DisableTxCompleteIRQ
UART_Error_t UART_EnableRxIRQ / DisableRxIRQ / EnableIdleIRQ / DisableIdleIRQ
UART_Error_t UART_EnableTxDMA / DisableTxDMA / EnableRxDMA / DisableRxDMA
UART_Error_t UART_RegisterCallback(UART_Id_t uart, UART_Callback_t cb, void *context);
UART_Error_t UART_ClearErrors(UART_Id_t uart_id);
UART_Error_t UART_FlushRx(UART_Id_t uart_id);
UART_Error_t UART_IsError(UART_Id_t uart_id, uint8_t *state);
/* LIN mode: UART_EnableLINMode, UART_SendBreak, UART_EnableBreakIRQ, ... */
```

`UART_GetDRAddress` exists specifically so the service layer can hand the DMA controller a peripheral address without including the register header.

The LIN-mode block (five functions) is fully implemented and entirely unused — LIN is not part of this system.

### 66.2 Service-layer ring buffers

```c
#define UART_SVC_TX_BUF_SIZE  256U
#define UART_SVC_RX_BUF_SIZE  256U
#define UART_SVC_TX_MASK  (UART_SVC_TX_BUF_SIZE - 1U)
#define UART_SVC_RX_MASK  (UART_SVC_RX_BUF_SIZE - 1U)
```

Power-of-two sizes so wrapping is a mask, not a modulo.

```c
static inline void rx_push(UART_SVC_RxRing_t *r, uint8_t byte)
{
    uint16_t next = (r->head + 1U) & UART_SVC_RX_MASK;
    if (next != r->tail) {          /* drop rather than overwrite */
        r->buf[r->head] = byte;
        r->head = next;
    }
}
```

Head-chases-tail with a one-slot gap, so a full buffer drops the newest byte rather than overwriting the oldest. Same policy as the sample ring buffer ([§35.3](#353-drop-not-overwrite)) and for the same reason.

`static UART_SVC_Instance_t svc[3];` allocates all three instances regardless of use — 1,536 bytes of rings, of which 1,024 belong to UART2 and UART6 and are never touched ([§11](#11-sram-budget)).

### 66.3 The event callback

```c
static void uart_svc_callback(UART_Event_t event, void *ctx)
{
    UART_Id_t id = (UART_Id_t)(uint32_t)ctx;
    UART_SVC_Instance_t *inst = &svc[(uint8_t)id];

    switch (event)
    {
        case UART_EVENT_TXE:
            if (!tx_empty(&inst->tx)) { UART_WriteDR(id, tx_pop(&inst->tx)); }
            else { UART_DisableTxIRQ(id); UART_EnableTxCompleteIRQ(id); }
            break;

        case UART_EVENT_TC:
            UART_DisableTxCompleteIRQ(id);
            inst->tx_active = 0U;
            break;

        case UART_EVENT_RXNE:
        {
            uint16_t data = 0U;
            UART_ReadDR(id, &data);
            rx_push(&inst->rx, (uint8_t)(data & 0xFFU));
            if (inst->rx_notify_task != NULL) {
                BaseType_t woken = pdFALSE;
                vTaskNotifyGiveFromISR(inst->rx_notify_task, &woken);
                portYIELD_FROM_ISR(woken);
            }
            break;
        }

        case UART_EVENT_ERROR:
            inst->error = 1U;
            UART_ClearErrors(id);
            break;
        ...
    }
}
```

The TXE→TC handoff is the standard idiom: disable TXE when the buffer drains, enable TC to learn when the shift register empties, then mark idle. The application never uses the IRQ TX path (it uses DMA), so this code is exercised only if someone calls `UART_SVC_Transmit`.

The context pointer trick — `(void *)(uint32_t)id` — packs an enum into a pointer to avoid a per-instance context struct. It is well-defined for values that fit in a pointer, which 0/1/2 do.

### 66.4 The TX-mode split

| Path | Mechanism | Used by |
|---|---|---|
| `UART_SVC_Transmit` | Ring buffer + TXE IRQ | **nobody** |
| `UART_SVC_TransmitDMA` | Direct DMA from caller's buffer | Thread 3 |
| `UART_Transmit_Polling` | Blocking, TXE + TC spin | Thread 5 (bootloader ACK) |

Three transmit paths, all functional, used by two callers. The polling path is deliberately kept for Thread 5's reset-critical ACK ([§28.3](#283-the-ack-uses-polling-deliberately)); the IRQ ring-buffer path is dead code that costs 512 bytes of RAM per instance.

### 66.5 The silent error flag

```c
case UART_EVENT_ERROR:
    inst->error = 1U;
    UART_ClearErrors(id);
    break;
```

`inst->error` is set on framing, noise, overrun or parity errors. `UART_SVC_IsError()` exists to read it. **Nothing calls it.**

So every receive error on the link is recorded into a variable no one inspects. Given that the only inbound traffic is the two-byte bootloader command, and that a corrupted command simply fails to match the state machine, the practical impact is nil — but the flag is a free signal being discarded. Polling it from Thread 4 and folding it into the heartbeat would cost two bytes of payload.

## 67. FLASH Driver

`src/FLASH.c` (745 lines), `include/FLASH_INTERFACE.h` (200 lines).

```c
FLASH_Status_t FLASH_Unlock(void);
FLASH_Status_t FLASH_Lock(void);
FLASH_Status_t FLASH_EraseSector(FLASH_Sector_t Sector, FLASH_VoltageRange_t VoltageRange);
FLASH_Status_t FLASH_MassErase(void);
FLASH_Status_t FLASH_ProgramByte / HalfWord / Word / DoubleWord(...);
FLASH_Status_t FLASH_ProgramBuffer(uint32_t Address, const uint8_t *pData,
                                    uint32_t Length, FLASH_ProgramSize_t ProgramSize);
FLASH_Status_t FLASH_WaitForLastOperation(uint32_t Timeout);
FLASH_Sector_t FLASH_GetSector(uint32_t Address);
void           FLASH_ClearErrorFlags(void);
FLASH_Status_t FLASH_GetLastError(void);
uint8_t        FLASH_IsLocked(void);
uint8_t        FLASH_IsBusy(void);
```

**None of it is called.** `main.c` includes `FLASH_INTERFACE.h` and never uses a single function.

The driver dates from the design in which Thread 5 erased a sector before resetting into the bootloader ([§28.2](#282-it-does-not-erase-flash)). The RAM-flag mechanism replaced it, and the driver was left behind.

745 lines and roughly 3–4 KB of Flash for a subsystem the application does not use. Whether it is actually linked depends on `--gc-sections`; since nothing references it, it should be collected. The `#include` in `main.c` does not create a reference — only a call would.

Removing the include and the source file from the build would be a clean tidy-up. It is also worth keeping in mind that this driver is a genuine liability if anyone ever calls it: `FLASH_MassErase()` would erase the bootloader.

## 68. IWDG Driver

`src/IWGD.c` (606 lines — note the filename typo), `include/IWDG_INTERFACE.h` (606 lines).

```c
IWDG_Status_t    IWDG_Init(uint32_t timeout_ms);
IWDG_Status_t    IWDG_InitWithConfig(const IWDG_Config_t* config);
IWDG_Status_t    IWDG_Start(void);
IWDG_Status_t    IWDG_Refresh(void);
IWDG_State_t     IWDG_GetState(void);
uint8_t          IWDG_IsRunning(void);
uint32_t         IWDG_GetActualTimeout(uint32_t lsi_freq);
IWDG_Prescaler_t IWDG_GetPrescaler(void);
uint16_t         IWDG_GetReloadValue(void);
IWDG_Status_t    IWDG_CalculateTimeout(uint32_t timeout_ms, uint32_t lsi_freq,
                                        IWDG_Prescaler_t* prescaler, uint16_t* reload);
uint8_t          IWDG_IsTimeoutValid(uint32_t timeout_ms, uint32_t lsi_freq);

/* Thread-aware supervisor */
void          IWDG_Thread_SetAlive(volatile uint8_t *flag);
IWDG_Status_t IWDG_SupervisorFeed(void);
```

The supervisor half is covered in [§32](#32-idle-hook-and-the-watchdog-supervisor).

### 68.1 Prescaler selection

```c
for (pr = IWDG_PRESCALER_DIV_4; pr <= IWDG_PRESCALER_DIV_256; pr++)
{
    uint32_t prescaler_div = IWDG_GetPrescalerValue(pr);
    uint64_t reload_calc = ((uint64_t)timeout_ms * (uint64_t)local_lsi_freq)
                           / ((uint64_t)prescaler_div * 1000ULL);
    if ((reload_calc >= 1ULL) && (reload_calc <= IWDG_RLR_MAX))
    {
        *prescaler = pr;
        *reload    = (uint16_t)reload_calc;
        status     = IWDG_OK;
        break;
    }
}
```

Smallest prescaler first, because a smaller prescaler with a larger reload gives finer resolution. For `IWDG_Init(3000)`:

```
   /4   → 24000  > 4095  reject
   /8   → 12000  > 4095  reject
   /16  →  6000  > 4095  reject
   /32  →  3000  ≤ 4095  ACCEPT

   → PR = /32, RLR = 3000
   → nominal 3.00 s, worst case 2.04 s at 47 kHz LSI
```

`uint64_t` for the intermediate is necessary: `32768 × 47000` overflows `uint32_t`.

### 68.2 The hardware sequence

```c
IWDG_ENABLE_WRITE();                    /* KR = 0x5555 — unlock PR and RLR */
if (iwdg_state == IWDG_STATE_RUNNING) { IWDG_WaitForUpdate(); }
IWDG_SET_PRESCALER(prescaler);
IWDG_SET_RELOAD(reload);
if (iwdg_state == IWDG_STATE_RUNNING) { IWDG_WaitForUpdate(); }
IWDG_RELOAD();                          /* KR = 0xAAAA — load the new RLR */
```

The `IWDG_WaitForUpdate()` calls poll `SR.PVU` and `SR.RVU` — flags that indicate the hardware is still transferring a written value into the (LSI-clocked) IWDG domain. Because the IWDG runs on LSI (~32 kHz) while the CPU runs at 84 MHz, a write takes up to ~5 LSI cycles ≈ 150 µs to take effect.

The `if (iwdg_state == IWDG_STATE_RUNNING)` guard skips the wait before the watchdog is started, with the comment:

```c
/* Only wait if IWDG is already running — otherwise PVU/RVU are stale */
```

Correct: before `KR = 0xCCCC` the LSI domain is not clocked and the flags never clear, so the wait would spin until its bounded timeout.

### 68.3 The API is far larger than the use

`main()` calls three functions: `IWDG_Init(3000U)`, `IWDG_Start()`, and (via the tasks and idle hook) `IWDG_Thread_SetAlive` / `IWDG_SupervisorFeed`.

The other nine are unused. `IWDG_GetActualTimeout(lsi_freq)` would be genuinely valuable in the heartbeat — it computes the real timeout from the programmed prescaler and reload, so reporting it would make the watchdog margin visible rather than assumed.

### 68.4 The filename

`src/IWGD.c` — `IWGD`, not `IWDG`. A transposition that the build does not care about and that makes the file hard to find with tab-completion.
## 69. MPU6050 Driver

`src/MPU6050.c` (522 lines), `include/MPU6050.h` (119 lines). Built strictly on `I2C_SERVICE.h` — it never touches `I2C.c` directly.

```c
MPU6050_Error_t MPU6050_Init(I2C_Id_t i2c_id, I2C_DevAddr7_t addr, uint32_t timeout);
MPU6050_Error_t MPU6050_TriggerRead(MPU6050_ReadDone_t callback, void *ctx);
```

Two functions. The whole driver is one blocking initialiser and one non-blocking read trigger.

### 69.1 The initialisation sequence

```
   1. Validate the device address (must be 0x68 or 0x69)
   2. Latch i2c_id and addr into file-scope statics
   3. SpinDelay ~100 ms                    ← VDD ramp, datasheet §7.4
   4. Write PWR_MGMT_1 = 0x80              ← DEVICE_RESET
   5. SpinDelay ~100 ms                    ← post-reset settle
   6. Write PWR_MGMT_1 = 0x01              ← wake, CLKSEL = PLL/X-gyro
   7. SpinDelay ~50 ms                     ← gyro ZRO settling, datasheet §6.1
   8. DMA-read WHO_AM_I (2 bytes), poll for completion
   9. Verify whoami_buf[0] == 0x68
  10. Write ACCEL_CONFIG = 0x08            ← ±4 g
  11. Write GYRO_CONFIG  = 0x08            ← ±500 °/s
  12. Write CONFIG       = 0x03            ← DLPF 44 Hz
  13. Write SMPLRT_DIV   = 0x09            ← 100 Hz
  14. mpu_initialized = 1;  dbg_init_stage = 99
```

Roughly 250 ms of spin delays, all before the scheduler starts.

**Step 4 — the device reset — is the important one**, and the comment explains why:

```c
 *     After a failed I2C session, the sensor may be stuck in
 *     an unknown internal state — a device reset is the only
 *     guaranteed recovery path without cycling power.
```

An MPU6050 that was mid-transfer when the STM32 reset can be left holding SDA low. Writing `DEVICE_RESET` clears every internal register and unwedges it — provided the bus is healthy enough for that one write to land. If it is not, `dbg_init_stage` stops at 10 and `MPU6050_Init` returns `MPU6050_ERROR_I2C`.

### 69.2 The return value is discarded

```c
/* main.c */
MPU6050_Init(I2C_ID_1, (I2C_DevAddr7_t)MPU6050_ADDR_LOW, 5000000UL);
RingBuffer_Init();
```

No check. If the sensor is absent, misaddressed, or the bus is stuck, `main()` proceeds to start the scheduler with `mpu_initialized == 0`. Then in live mode every `MPU6050_TriggerRead` returns `MPU6050_ERROR_INIT`, Thread 1 logs `LOG_ERROR(LOG_CODE_MPU6050_TIMEOUT, 1)` on every tick, and the debounce logic emits exactly one log frame about it.

So the failure *is* reported — once, four seconds after boot, as a single log frame. That is better than nothing but far weaker than halting or entering a distinctive fault mode. And in replay mode ([§43](#43-replay-mode)) the failure is completely invisible, because `MPU6050_TriggerRead` is never called.

Compare with the adjacent checks in `main()`:

```c
if (I2C_Init(&i2c_cfg) != I2C_OK) { while (1); }
if (Logger_Init() != 0U) { for (;;) {} }
if (IWDG_Init(3000U) != IWDG_OK) { for (;;) {} }
```

Three initialisers halt on failure; the sensor initialiser does not. Recorded in [§82](#82-known-gaps).

### 69.3 The busy guard and its dead end

```c
static volatile uint8_t mpu_busy = 0U;

MPU6050_Error_t MPU6050_TriggerRead(MPU6050_ReadDone_t callback, void *ctx)
{
    if (callback == NULL)      return MPU6050_ERROR_PARAM;
    if (mpu_initialized != 1U) return MPU6050_ERROR_INIT;
    if (mpu_busy != 0U)        return MPU6050_ERROR_BUSY;

    mpu_user_cb  = callback;
    mpu_user_ctx = ctx;
    mpu_busy     = 1U;

    I2C_SVC_Error_t svc_err = I2C_SVC_ReadBurst_DMA(
        mpu_i2c_id, mpu_dev_addr, MPU6050_REG_ACCEL_XOUT_H,
        mpu_rx_raw, MPU6050_BURST_LEN, mpu_dma_done, NULL);

    if (svc_err != I2C_SVC_OK) { mpu_busy = 0U; return MPU6050_ERROR_I2C; }
    return MPU6050_OK;
}
```

`mpu_busy` is cleared in exactly two places: the early-return path above, and the DMA callback:

```c
static void mpu_dma_done(I2C_SVC_Error_t result, void *ctx)
{
    mpu_busy = 0U;              /* cleared BEFORE the user callback — mandatory */
    if (result == I2C_SVC_OK) {
        MPU6050_ParseRaw(mpu_rx_raw, &mpu_parsed);
        if (mpu_user_cb != NULL) { mpu_user_cb(&mpu_parsed, mpu_user_ctx); }
    } else {
        if (mpu_user_cb != NULL) { mpu_user_cb(NULL, mpu_user_ctx); }
    }
}
```

Clearing `mpu_busy` before invoking the user callback is deliberate and documented — it lets the callback immediately re-trigger a read without hitting the guard.

**But if the transfer never completes at all**, neither callback fires and `mpu_busy` stays 1 forever. That is the dead end described in [§64.4](#644-service-layer-state-machine). There is no timeout in the driver and no way for Thread 1 to reset it. The only recovery is the watchdog.

### 69.4 The error callback passes NULL

```c
if (mpu_user_cb != NULL) { mpu_user_cb(NULL, mpu_user_ctx); }
```

On an I2C error, the callback receives `data = NULL`. Thread 1's callback does not check:

```c
static void on_mpu_read_done(const MPU6050_RawData_t *data, void *ctx){
    (void)ctx;
    s_latest_sample = *data;              /* ◄── NULL dereference on error */
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_dma_sem, &woken);
    portYIELD_FROM_ISR(woken);
}
```

`*data` with `data == NULL` dereferences address 0. On Cortex-M that reads the vector table (address 0 aliases to Flash at `0x08000000` — the *bootloader's* initial stack pointer and reset vector), so it does not fault. It silently copies 14 bytes of bootloader vector table into `s_latest_sample`, gives the semaphore, and Thread 1 pushes that garbage into the ring buffer as a sample.

The consequence: an I2C error produces a **corrupt sample that looks valid**, rather than a detected error. The values would be extreme (`0x2000`, `0xFFF8`, and whatever the next vectors are), which would show up as a wild outlier in the feature vector and probably classify as ROUGH.

The fix is one line:

```c
static void on_mpu_read_done(const MPU6050_RawData_t *data, void *ctx){
    (void)ctx;
    if (data == NULL) { return; }        /* let Thread 1's 5 ms timeout fire */
    ...
}
```

This is in the `#if !REPLAY_MODE` path, which the committed build does execute. Recorded in [§82](#82-known-gaps).

### 69.5 Static buffers and DMA lifetime

```c
static uint8_t mpu_rx_raw[MPU6050_BURST_LEN];   /* 14 bytes */
static MPU6050_RawData_t mpu_parsed;
static uint8_t cfg_buf[2U];
static volatile uint8_t whoami_buf[2U];
```

Every buffer handed to the DMA controller is file-scope static, with the reasoning spelled out:

```c
/* MUST be static (file-scope SRAM) so it remains valid from
 * DMA_Start until DMA_TC fires. NEVER use a stack buffer —
 * the stack frame will be gone before DMA completes.          */
```

`cfg_buf` is static even though `I2C_SVC_WriteReg` is blocking:

```c
 *  FIX (Bug 4): Moved from stack to file-scope static.
 *  Even though I2C_SVC_WriteReg is blocking, this eliminates
 *  any latent corruption risk if the service layer has an
 *  internal async path or yields during transmission.
```

Defensive, and the `FIX (Bug 4)` tag says it was found the hard way.

## 70. HC-SR04 Driver

`src/HCSR04.c` (37 lines), `include/HCSR04.h` (19 lines). The smallest module in the project.

```c
void HCSR04_Init(const HCSR04_Config_t *cfg);
void HCSR04_Trigger(const HCSR04_Config_t *cfg);
```

```c
typedef struct {
    GPIO_PORT_t trigger_port;
    GPIO_PIN_t  trigger_pin;
} HCSR04_Config_t;
```

The driver owns **only the trigger pin**. Echo capture lives entirely in `main.c` — the TIM3 configuration, the input-capture callback, the polarity toggling and the distance arithmetic ([§30.1](#301-the-capture-isr)).

That split is the module's main weakness. A caller cannot use this "driver" without also writing the TIM3 half themselves, and the two halves are coupled by the sensor-id convention in the queue message. A complete driver would own both and expose `HCSR04_GetDistance(sensor, &cm)`.

The 10 µs pulse and its actual ~65 µs duration are covered in [§30.5](#305-the-10-µs-trigger-pulse).

`HCSR04_Init` is called twice from `main()` and then the same configs are rebuilt as locals inside Thread 7:

```c
/* main() */
HCSR04_Config_t u1 = { .trigger_port = GPIO_PORTA, .trigger_pin = GPIO_PIN4 };
HCSR04_Init(&u1);

/* Thread7_Ultrasonic — declared again */
HCSR04_Config_t u1 = { .trigger_port = GPIO_PORTA, .trigger_pin = GPIO_PIN4 };
```

Duplicated literals in two places. They agree; nothing enforces that they continue to.

## 71. Ring Buffer Module

`src/RING_BUFFER.c` (589 lines), `include/RING_BUFFER.h` (210 lines). Behaviour in the pipeline is covered in [§35](#35-stage-2--ring-buffer); this is the API and the correctness argument.

```c
RingBuffer_Error_t RingBuffer_Init(void);
RingBuffer_Error_t RingBuffer_Push(const MPU6050_RawData_t *sample);      /* producer */
RingBuffer_Error_t RingBuffer_Pop(MPU6050_RawData_t *sample);             /* consumer */
RingBuffer_Error_t RingBuffer_PeekWindow(MPU6050_RawData_t *dest, uint32_t n);
RingBuffer_Error_t RingBuffer_Advance(uint32_t n);
uint32_t RingBuffer_Count(void);
uint8_t  RingBuffer_IsEmpty(void);
uint8_t  RingBuffer_IsFull(void);
uint32_t RingBuffer_GetPushCount(void);
uint32_t RingBuffer_GetPopCount(void);
uint32_t RingBuffer_GetDropCount(void);
uint32_t RingBuffer_GetMaxFill(void);
```

### 71.1 The stated invariants

The header opens with numbered constraints that the implementation then references:

```
 * RB-C01: HEAD is written ONLY by the producer (ISR).
 *         TAIL is written ONLY by the consumer (main).
 *         If any other code writes head or tail, the lock-free design
 *         breaks silently — data corruption with no visible error.
 *
 * RB-C08: No interrupt disabling. No critical sections. The SPSC
 *         invariant plus DMB barriers are the synchronisation mechanism.
```

| ID | Constraint |
|---|---|
| RB-C01 | Single writer per index |
| RB-C02 | Full ⟺ `(head − tail) ≥ CAPACITY` |
| RB-C03 | Empty ⟺ `head == tail` |
| RB-C04 | Count by unsigned subtraction — correct across wrap |
| RB-C05 | Push order: check → write → DMB → publish head |
| RB-C06 | Pop order: check → read → DMB → publish tail |
| RB-C07 | `dmb` + `"memory"` clobber — both required |
| RB-C08 | No interrupt disabling anywhere |
| RB-C09 | PeekWindow is a pure read — no barrier, no publish |
| RB-C10 | Advance publishes tail — barrier required |

This is the most rigorously specified module in the codebase, and the rigour is warranted: a lock-free structure that is subtly wrong fails intermittently under load, which is the worst possible failure mode.

### 71.2 Unsigned subtraction across the wrap

```c
uint32_t RingBuffer_Count(void){
    return (ring_buffer_instance.head - ring_buffer_instance.tail);
}
```

`head` and `tail` are free-running `uint32_t` counters, never masked. Only the *array index* is masked:

```c
idx = local_head & RING_BUFFER_MASK;
```

That means `head` wraps at 2³² (after 2³² pushes ≈ 497 days at 100 Hz), and the subtraction remains correct through the wrap:

```
   head = 0x00000000  (just wrapped)
   tail = 0xFFFFFFFF
   head - tail = 0x00000001 = 1        ✓ correct
```

The alternative — masking both indices — needs a separate full/empty discriminator (the classic "one slot wasted" or "extra flag" problem). Free-running counters avoid it entirely.

### 71.3 Field-by-field copy, not `memcpy`

```c
ring_buffer_instance.buf[idx].accel_x  = sample->accel_x;
ring_buffer_instance.buf[idx].accel_y  = sample->accel_y;
/* ... seven fields ... */
```

with the rationale:

```c
/*   No memset — explicit field-by-field loop.
 *   memset treats the struct as a raw byte array and is
 *   not MISRA-clean for structured types.                */
```

MISRA-C:2012 Rule 21.15/21.16 discourages `memcpy`/`memcmp` on structured types because padding bytes are indeterminate. `MPU6050_RawData_t` is seven `int16_t` with no padding, so `memcpy` would in fact be safe and faster — but the rule is followed uniformly rather than case-by-case, which is the right discipline for a MISRA claim.

Cost: 7 halfword loads + 7 halfword stores versus a 14-byte `memcpy` (3 word ops + 1 halfword). Perhaps 10 extra cycles per sample, 1,000 cycles/s. Irrelevant.

### 71.4 The max-fill tracker

```c
fill = (local_head + 1U) - local_tail;
if (fill > rb_max_fill) { rb_max_fill = fill; }
```

with a careful comment about the race:

```c
/*   This is a consistent snapshot: the current fill is at least
 *   (new head - old tail). The consumer may have advanced tail
 *   since we read it, but that would only make the real fill smaller,
 *   so we might record a slightly larger max — acceptable. */
```

Correct reasoning. `rb_max_fill` is written only by the producer, so there is no write race; the read of `tail` may be stale, which biases the estimate *upward*. For a high-water-mark diagnostic, over-reporting is the safe direction.

### 71.5 Saturating counters

```c
if (rb_push_count < 0xFFFFFFFFUL) { rb_push_count++; }
```

Saturate rather than wrap, so a large value unambiguously means "a lot" rather than "a lot, modulo 2³²". At 100 Hz, `rb_push_count` would saturate after 497 days.

`RingBuffer_Advance` saturates with an overflow check:

```c
if ((rb_pop_count + n) >= rb_pop_count) { rb_pop_count += n; }
else                                     { rb_pop_count = 0xFFFFFFFFUL; }
```

The condition `(a + n) >= a` for unsigned arithmetic is true unless the addition wrapped. Correct, though a compiler may warn that it looks like a tautology.

### 71.6 Debug breadcrumbs

```c
static volatile uint8_t dbg_rb_stage = 0U;
```

| Value | Meaning |
|---|---|
| `0x01` | `Init` entered |
| `0x63` | `Init` success (99 decimal) |
| `0xA0` / `0xA1` / `0xA2` | Push entered / full-dropped / complete |
| `0xB0` / `0xB1` / `0xB2` | Pop entered / empty / complete |
| `0xC0` / `0xC1` / `0xC2` / `0xC3` | Peek entered / bad param / insufficient / complete |
| `0xD0` / `0xD1` / `0xD2` / `0xD3` | Advance entered / bad param / insufficient / complete |

`volatile` so a debugger reads the true current value. Halting the core and reading `dbg_rb_stage` tells you exactly where in the ring buffer the last operation was. Costs one byte of RAM and one store per call.

### 71.7 `RingBuffer_Pop` is unused

Thread 2 uses `PeekWindow` + `Advance` exclusively. `Pop` is fully implemented, barrier-correct, and never called. It is the natural API for a future consumer that wants one sample at a time, and it costs nothing to keep.

## 72. Buffer_t Descriptor

`include/STD_BUFFER.h` (108 lines). A four-field descriptor that replaces the `(uint8_t *buf, uint16_t len)` pair across the driver APIs.

```c
typedef struct {
    uint8_t  *data;    /* pointer to the raw byte array — must not be NULL */
    uint16_t  size;    /* total capacity of data[] in bytes                */
    uint16_t  length;  /* number of valid bytes currently in data[]        */
    uint16_t  index;   /* current read or write cursor                     */
} Buffer_t;
```

```
   data ──► ┌────────────────────────────────────────────────┐
            │████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
            └────────────────────────────────────────────────┘
             ▲       ▲       ▲                              ▲
             0     index   length                          size
                     │        │                              │
                     │        │                              └─ capacity
                     │        └─ valid data ends here
                     └─ read/write cursor
```

### 72.1 Macros

```c
#define BUFFER_INIT(arr, cap)   { (arr), (uint16_t)(cap), 0U, 0U }

#define BUFFER_IS_EMPTY(b)      ((b)->length == 0U)
#define BUFFER_IS_FULL(b)       ((b)->length >= (b)->size)
#define BUFFER_READ_DONE(b)     ((b)->index >= (b)->length)
#define BUFFER_REMAINING(b)     ((uint16_t)((b)->length - (b)->index))
#define BUFFER_FREE(b)          ((uint16_t)((b)->size - (b)->length))

#define BUFFER_RESET(b)         do { (b)->index = 0U; (b)->length = 0U; } while (0)
#define BUFFER_REWIND(b)        do { (b)->index = 0U; } while (0)
#define BUFFER_ADVANCE(b, n)    do { (b)->index = (uint16_t)((b)->index + (n)); } while (0)

#define BUFFER_IS_VALID(b) \
    (((b) != NULL) && ((b)->data != NULL) && ((b)->size > 0U) && \
     ((b)->length <= (b)->size) && ((b)->index <= (b)->length))
```

`BUFFER_IS_VALID` checks all four fields for mutual consistency and is used at the entry of every driver function that takes a buffer:

```c
if (!BUFFER_IS_VALID(buf)) return UART_SVC_ERROR_PARAM;
```

That single check replaces what would otherwise be three or four separate NULL and range tests, and it catches a class of caller error — a `length` larger than `size`, an `index` past `length` — that the raw pointer-plus-length convention cannot express at all.

### 72.2 The const variant is identical

```c
#define CONST_BUFFER_IS_VALID(b) \
    (((b) != NULL) && ((b)->data != NULL) && ((b)->size > 0U) && \
     ((b)->length <= (b)->size) && ((b)->index <= (b)->length))
```

Byte-for-byte the same expression as `BUFFER_IS_VALID`. Since macros are untyped, both work on `const Buffer_t *` and non-const alike — the separate name documents intent and nothing more. `CONST_BUFFER_IS_VALID` is never used.

### 72.3 Name collision with the bootloader

The bootloader has its own `Buffer_t` in `include/interface/MCAL/uart.h`:

```c
/* Bootloader */
typedef struct{
    uint8_t* data;
    uint8_t  length;      /* ◄── uint8_t, not uint16_t */
    uint8_t  index;
} Buffer_t;               /* ◄── no size field */
```

Three fields, `uint8_t` lengths, no capacity. Same name, different layout, different semantics. The two never link together, so there is no conflict — but anyone moving code between the two projects will be caught by it.

---

# Part VIII — Diagnostics

## 73. Logging Architecture

```
   ┌─────────────────────────────────────────────────────────────────┐
   │  Any task or ISR                                                 │
   │    LOG_INFO(code, aux) / LOG_WARN / LOG_ERROR / LOG_FATAL        │
   └────────────────────────────┬────────────────────────────────────┘
                                ▼
   ┌─────────────────────────────────────────────────────────────────┐
   │  Logger_Log(code, severity, aux_data)                            │
   │    1. severity == last emitted for this code?  → drop            │
   │    2. new candidate severity?                  → require 2×      │
   │    3. already pending in the queue?            → drop            │
   │    4. build Log_Payload_t with a timestamp                       │
   │    5. mark pending, then xQueueSend or xQueueSendFromISR         │
   └────────────────────────────┬────────────────────────────────────┘
                                ▼
                  s_logger_queue  (16 × 10 B)
                                │
                                ▼
   ┌─────────────────────────────────────────────────────────────────┐
   │  Logger_Task  (priority 0)                                       │
   │    xQueueReceive(portMAX_DELAY)                                  │
   │    pack 10 bytes big-endian into a FrameRequest_t                │
   │    xQueueSend(g_frame_queue, 0)                                  │
   │    on success: s_last_severity[code] = severity                  │
   │    clear s_is_pending[code]                                      │
   │    vTaskDelay(4000)                    ← hard rate limit         │
   └────────────────────────────┬────────────────────────────────────┘
                                ▼
                       g_frame_queue → Thread 3 → UART
```

### 73.1 ISR safety

```c
if (xPortIsInsideInterrupt() != pdFALSE)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    status = xQueueSendFromISR(s_logger_queue, &entry, &xHigherPriorityTaskWoken);
    if (status != pdTRUE) { s_is_pending[code] = 0U; s_drop_count++; }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
else
{
    status = xQueueSend(s_logger_queue, &entry, 0U);
    if (status != pdTRUE) { s_is_pending[code] = 0U; s_drop_count++; }
}
```

`xPortIsInsideInterrupt()` reads the `IPSR` register: non-zero means an exception is active. So a single `LOG_*` macro works correctly from either context without the caller knowing which it is in.

Both paths use a **zero timeout**, so logging never blocks — essential in an ISR and desirable in a task.

`portYIELD_FROM_ISR(woken)` is called unconditionally in the ISR path, including when the send failed. Harmless: `woken` is `pdFALSE` in that case and the macro is a no-op.

### 73.2 The 1 KB of tracking arrays

```c
static volatile uint8_t s_is_pending[256]         = {0};
static volatile uint8_t s_last_severity[256];
static volatile uint8_t s_candidate_severity[256];
static volatile uint8_t s_candidate_count[256];
```

Four arrays indexed by the full `uint8_t` code space — 1,024 bytes of `.bss` for the 16 codes actually defined ([§74](#74-log-code-taxonomy)).

Direct indexing means no search and no bounds check: `s_is_pending[code]` is O(1) for any `uint8_t`. A 16-entry lookup would use 64 bytes and need a linear scan or a switch. At 3.3 % of total RAM the array approach is affordable and it removes an entire class of bug (a code added to the enum but not to the table).

`Logger_Init` initialises three of the four:

```c
for(int i = 0; i < 256; i++) {
    s_last_severity[i]      = 0xFF;   /* 0xFF = never logged */
    s_candidate_severity[i] = 0xFF;
    s_candidate_count[i]    = 0;
}
```

`s_is_pending` relies on `.bss` zeroing, which is correct but inconsistent with the explicit loop for the other three.

`0xFF` as "never logged" works because valid severities are 0–3.

## 74. Log Code Taxonomy

```c
typedef enum {
    /* 0x10..0x1F — Hardware */
    LOG_CODE_MPU6050_TIMEOUT      = 0x10,
    LOG_CODE_I2C_BUS_STUCK        = 0x11,
    LOG_CODE_RING_BUFFER_DROP     = 0x12,
    LOG_CODE_DMA_ERROR            = 0x13,
    LOG_CODE_ADC_TIMEOUT          = 0x14,
    LOG_CODE_ULTRASONIC1_TIMEOUT  = 0x15,
    LOG_CODE_ULTRASONIC2_TIMEOUT  = 0x16,

    /* 0x20..0x2F — Software */
    LOG_CODE_INFERENCE_FAIL       = 0x20,
    LOG_CODE_QUEUE_FULL           = 0x21,
    LOG_CODE_FRAME_BUILD_FAIL     = 0x22,
    LOG_CODE_LOGGER_DROP          = 0x23,

    /* 0x30..0x3F — Status */
    LOG_CODE_BOOT                 = 0x30,
    LOG_CODE_THREAD_STARTED       = 0x31,
    LOG_CODE_RESET_DETECTED       = 0x32,

    /* 0x40..0x4F — Warnings */
    LOG_CODE_LOW_CONFIDENCE       = 0x40,
    LOG_CODE_STACK_HIGH_WATER     = 0x41
} Log_Code_t;
```

Sixteen codes in four ranges of sixteen. The range encoding means a receiver can classify an unknown code by its high nibble.

| Code | Used? | Emitted by | Severities used |
|---|---|---|---|
| `0x10` MPU6050_TIMEOUT | ✓ | Thread 1 (live mode only) | WARN (aux 0), ERROR (aux 1) |
| `0x11` I2C_BUS_STUCK | ✗ | — | — |
| `0x12` RING_BUFFER_DROP | ✓ | Thread 1, Thread 2 | ERROR (aux 1, aux 2) |
| `0x13` DMA_ERROR | ✗ | — | — |
| `0x14` ADC_TIMEOUT | ✓ | Thread 6 | INFO + WARN (edge-triggered) |
| `0x15` ULTRASONIC1_TIMEOUT | ✓ | Thread 7 | INFO + WARN |
| `0x16` ULTRASONIC2_TIMEOUT | ✓ | Thread 7 | INFO + WARN |
| `0x20` INFERENCE_FAIL | ✗ | — | — |
| `0x21` QUEUE_FULL | ✓ | Threads 2, 4, 6, 7 | ERROR / WARN (aux distinguishes) |
| `0x22` FRAME_BUILD_FAIL | ✓ | Thread 3 | ERROR (unreachable, [§26.4](#264-frame_build-failure)) |
| `0x23` LOGGER_DROP | ✗ | — | — |
| `0x30` BOOT | ✓ | `main()` | INFO |
| `0x31` THREAD_STARTED | ✗ | — | — |
| `0x32` RESET_DETECTED | ✗ | — | — |
| `0x40` LOW_CONFIDENCE | ✗ | — | — |
| `0x41` STACK_HIGH_WATER | ✗ | — | — |

**Seven of sixteen codes are used.** The nine unused ones represent diagnostics that were designed and not wired up. Three are notably worth having:

- `LOG_CODE_INFERENCE_FAIL` — would make a failed `Inference_Init` visible ([§25.2](#252-one-time-initialisation-inside-the-task-body))
- `LOG_CODE_LOGGER_DROP` — would surface `s_drop_count` and `s_forward_drop_count`, currently debugger-only
- `LOG_CODE_RESET_DETECTED` — reading `RCC->CSR` reset flags at boot and reporting whether the last reset was IWDG, software or power-on would be the single most useful addition to the diagnostics ([§32.4](#324-failure-preserves-evidence))

### 74.1 Severity semantics

```c
#define LOG_SEV_INFO    0U
#define LOG_SEV_WARN    1U
#define LOG_SEV_ERROR   2U
#define LOG_SEV_FATAL   3U
```

In this codebase the convention is:

| Severity | Meaning here |
|---|---|
| INFO | A monitored subsystem is **healthy** — used for the edge-triggered recovery signal |
| WARN | Recoverable: a timeout, a dropped frame |
| ERROR | Data loss occurred: a dropped sample, a full queue |
| FATAL | **never used** |

The INFO-means-healthy convention is unusual and is what makes the edge-triggered pattern work ([§29.3](#293-the-info-on-success-pattern)). It is worth internalising before reading a log capture: an INFO with code `0x15` does not mean "ultrasonic sensor 1 timed out, informationally" — it means "ultrasonic sensor 1 is working again".

## 75. Debounce and Deduplication

Three independent filters sit between a `LOG_*` call and the queue.

```c
void Logger_Log(uint8_t code, uint8_t severity, uint32_t aux_data)
{
    /* ---- Filter 1: state-change detection ---- */
    if (s_last_severity[code] == severity)
    {
        s_candidate_severity[code] = 0xFF;
        s_candidate_count[code]    = 0;
        return;                                    /* nothing changed */
    }

    /* ---- Filter 2: debounce (2 consecutive readings) ---- */
    if (s_candidate_severity[code] != severity)
    {
        s_candidate_severity[code] = severity;
        s_candidate_count[code]    = 1;

        if (s_last_severity[code] == 0xFF) {
            s_candidate_count[code] = 2;           /* first ever — accept now */
        } else {
            return;                                /* wait for confirmation */
        }
    }
    else
    {
        s_candidate_count[code]++;                 /* same candidate again */
    }

    if (s_candidate_count[code] < 2) { return; }

    /* ---- Filter 3: deduplication ---- */
    if (s_is_pending[code] != 0U) { return; }

    /* ... build and enqueue ... */
}
```

### 75.1 Filter 1 — state change only

```
   s_last_severity[code] == severity  →  drop
```

Only *transitions* are logged. A sensor that fails on every one of 1,000 consecutive attempts produces exactly one WARN.

This is the single most important filter. Without it, Thread 7's per-cycle `LOG_INFO` would generate 4 entries per second forever, saturating the 4-second rate limit and crowding out everything else.

### 75.2 Filter 2 — two-reading debounce

```
   First occurrence of a NEW severity for this code:
     record it as a candidate, count = 1, and RETURN without logging.

   Second consecutive occurrence of the SAME candidate:
     count = 2 → proceed.

   Exception: if this code has never been logged (last == 0xFF),
     force count = 2 and accept immediately.
```

```
   Reading:   OK   OK   FAIL  OK   OK   FAIL FAIL FAIL
   Severity:  I    I    W     I    I    W    W    W
   ─────────────────────────────────────────────────────
   last:      I    I    I     I    I    I    W    W
   candidate: -    -    W     I    -    W    W    -
   count:     -    -    1     1    -    1    2    -
   emitted:   -    -    no    no   -    no   YES  no
                          ▲
                          single-cycle glitch — suppressed
```

A one-off glitch never reaches the queue. Two consecutive readings of the same new state are required.

The `last_severity == 0xFF` exception means the **first** log for any code is always emitted immediately, without debouncing. That is right: at boot there is no prior state to compare against, and delaying the first report of a broken sensor by one cycle serves no purpose.

Note the `else` branch increments `s_candidate_count[code]` without bound. After 200 consecutive identical readings the count is 200 — but since filter 1 catches the case where the state has already been *emitted*, the count only grows while a candidate is pending, which is at most one cycle. It cannot overflow in practice.

### 75.3 Filter 3 — pending deduplication

```c
if (s_is_pending[code] != 0U) { return; }
```

`s_is_pending[code]` is set when an entry is queued and cleared when the Logger task dequeues it. Because of the 4-second rate limit, that window can be tens of seconds — during which any further logs for the same code are dropped.

This is what bounds `s_logger_queue` occupancy: **at most one entry per code**, so at most 16 entries with 16 defined codes. The queue is sized at exactly 16. That is not a coincidence.

### 75.4 The combined effect

```
   Raw event rate:      up to 100 Hz (Thread 1's per-tick logs)
        │
        │  Filter 1: state changes only
        ▼
   Transition rate:     a few per minute in normal operation
        │
        │  Filter 2: two-reading debounce
        ▼
   Confirmed rate:      glitches removed
        │
        │  Filter 3: one pending entry per code
        ▼
   Queued rate:         ≤ 16 entries outstanding
        │
        │  Logger task: vTaskDelay(4000)
        ▼
   Transmitted rate:    ≤ 0.25 Hz
```

Four orders of magnitude of reduction, with the property that **no state change is ever lost** — it is delayed, but the timestamp preserves when it happened.

### 75.5 What the filters cost

**A rapidly oscillating sensor is under-reported.** A connection that fails and recovers every cycle produces alternating WARN/INFO candidates that never reach count 2, so **nothing is ever logged**. The sensor is visibly broken and the log is silent.

That is a genuine blind spot. A counter of suppressed transitions, reported periodically, would close it.

**Recovery can be reported before the failure.** If a WARN is queued (pending) and the sensor recovers before the Logger drains it, the INFO is dropped by filter 3. Then `s_last_severity` becomes WARN when the entry is finally sent — and the next INFO is treated as a state change and emitted. So the pair does arrive, just with a large gap. Correct, but the ordering can look strange in a capture.

## 76. Debug Breadcrumbs

Three modules maintain `volatile` file-scope variables whose only purpose is to be read from a halted debugger.

### 76.1 MPU6050 initialisation stages

```c
static volatile uint8_t dbg_init_stage = 0U;
static volatile uint8_t dbg_whoami_val = 0U;
static volatile uint8_t dbg_svc_err    = 0U;
```

| `dbg_init_stage` | Meaning |
|---|---|
| 0 | Not entered |
| 1 | About to write DEVICE_RESET |
| 2 | About to write the wake command |
| 3 | About to read WHO_AM_I |
| 4 | About to write ACCEL_CONFIG |
| 5 | About to write GYRO_CONFIG |
| 6 | About to write CONFIG (DLPF) |
| 7 | About to write SMPLRT_DIV |
| **10** | DEVICE_RESET write rejected by the service layer |
| **11** | Wake write rejected |
| **20** | WHO_AM_I DMA read rejected |
| **21** | WHO_AM_I DMA callback never fired (timeout) |
| **22** | WHO_AM_I DMA completed with an I2C error |
| **23** | WHO_AM_I value mismatch |
| **30** | ACCEL_CONFIG write failed |
| **40** | GYRO_CONFIG write failed |
| **50** | CONFIG write failed |
| **60** | SMPLRT_DIV write failed |
| **99** | **Success** |

`dbg_whoami_val` records what was actually read, with the diagnosis table in the source:

```c
/* Common dbg_whoami_val values:
 *   0x00 = sensor not responding (SDA stuck low)
 *   0xFF = SDA stuck high (no pull-down / no device)
 *   0x68 = correct MPU6050
 *   0x69 = MPU6050 with AD0=VCC (address mismatch)
 *   0x71 = MPU9250 (not MPU6050)
 *   0x73 = MPU6500 variant                                 */
```

This turns "the sensor does not work" into a specific diagnosis in one debugger read. It is the highest-value diagnostic in the project and it costs three bytes.

The stage-21 comment is equally practical:

```c
/* DMA callback never fired. Most common causes:
 *   - I2C_SVC_Init() not called before MPU6050_Init()
 *   - Wrong DMA stream/channel for I2C1 RX
 *   - NVIC IRQ for DMA stream not enabled
 *   - ITBUFEN left enabled — RXNE ISR stealing DMA bytes
 *   - timeout value too small for CPU clock speed          */
```

### 76.2 Ring buffer stages

`dbg_rb_stage`, documented in [§71.6](#716-debug-breadcrumbs).

### 76.3 Thread 2 counters

```c
volatile uint32_t g_t2_wake_count;
volatile uint32_t g_t2_inference_count;
volatile uint32_t g_t2_queue_drops;
```

Non-static and `volatile`, so they are watchable by name.

### 76.4 Replay counters

```c
volatile uint32_t g_replay_samples_pushed;
volatile uint32_t g_replay_done;
volatile uint32_t g_replay_last_label;
volatile uint32_t g_replay_last_conf;
volatile uint32_t g_replay_vote_smooth;
volatile uint32_t g_replay_vote_rough;
```

The on-target model validation described in [§43.2](#432-what-replay-mode-is-for).

### 76.5 Declared and unused

```c
volatile uint32_t g_sensor_cpu_cycles      = 0U;
volatile uint32_t g_sensor_stack_remaining = 0U;
volatile uint32_t g_total_runtime          = 0U;
volatile uint32_t g_sensor_cpu_percent_x100 = 0U;
volatile uint32_t g_sensor_stack_used       = 0U;
volatile uint32_t g_sensor_stack_percent    = 0U;
volatile uint32_t g_uptime_ms               = 0U;
```

Seven globals in `main.c` under the comment `/* Thread 1 runtime stats (computed in idle hook) */`. **Nothing writes them** — the idle hook only calls `IWDG_SupervisorFeed()`. They are the residue of an earlier design in which the idle hook computed per-task statistics, superseded by Thread 4's `vTaskGetInfo` approach.

28 bytes of `.bss` and a misleading comment.

## 77. Heartbeat as Telemetry

The heartbeat is the primary observability surface. Every five seconds it answers "is this node healthy?" with thirteen numbers.

```
   ┌──────────────────────────────────────────────────────────────────┐
   │  WHAT THE HEARTBEAT TELLS YOU                                    │
   ├──────────────────────────────────────────────────────────────────┤
   │                                                                  │
   │  uptime_ms            Has it reset?  A drop to near zero means   │
   │                       a watchdog fire, an OTA, or a power event. │
   │                                                                  │
   │  cpu_idle_x100        Headroom. Below 5000 (50 %) means          │
   │                       something unexpected is consuming CPU.     │
   │                                                                  │
   │  cpu_t2_x100          The ML task's cost. A step change means    │
   │                       the model or the pipeline changed.         │
   │                                                                  │
   │  stack_t*_free        Stack headroom in words. Monotonically     │
   │                       non-increasing — a value that drops over   │
   │                       days indicates a rarely-taken deep path.   │
   │                                                                  │
   │  inf_wcet_us          All-time worst inference latency.          │
   │                                                                  │
   │  rb_max_fill          THE key number. Should sit at ~50.         │
   │                       Climbing toward 128 = Thread 2 is losing.  │
   └──────────────────────────────────────────────────────────────────┘
```

### 77.1 What is missing

| Not reported | Why it would matter |
|---|---|
| `REPLAY_MODE` | The node could be shipping replayed data ([§43.4](#434-the-mode-is-invisible-in-telemetry)) |
| Reset cause (`RCC->CSR`) | Distinguishes watchdog fire from power cycle from OTA |
| `RingBuffer_GetDropCount()` | Samples actually lost, versus `rb_max_fill`'s early warning |
| `g_t2_inference_count` | Would make the inference rate directly observable |
| `s_drop_count` / `s_forward_drop_count` | Logs that never made it out |
| UART `svc[].error` | Link-layer errors ([§66.5](#665-the-silent-error-flag)) |
| Thread 4, 5, Logger CPU and stack | Three unmeasured tasks |
| `IWDG_GetActualTimeout()` | The real watchdog margin |

The payload has room: `FRAME_REQ_MAX_PAYLOAD` is 32 and the heartbeat uses 30. Two spare bytes — enough for a reset-cause byte and a flags byte, which would be the two highest-value additions. Beyond that, `FRAME_REQ_MAX_PAYLOAD` would need raising (the queue would grow by 6 entries × the increase).

### 77.2 Reading a healthy heartbeat

```
   0C ... no — a heartbeat frame is:

   24 03 01 <30 payload bytes> <crc32>
   │  │  │
   │  │  └─ ECU_ID_STM32_NODE1
   │  └──── FRAME_TYPE_HEARTBEAT
   └─────── LEN = 30 + 6 = 36 = 0x24

   Decoded payload from a healthy node at ~2 minutes uptime:

   uptime_ms      = 0x0001D4C0 = 120,000 ms
   cpu_t1_x100    = 0x0032 =  0.50 %
   cpu_t2_x100    = 0x012C =  3.00 %
   cpu_t3_x100    = 0x000A =  0.10 %
   cpu_t6_x100    = 0x0002 =  0.02 %
   cpu_t7_x100    = 0x0008 =  0.08 %
   cpu_idle_x100  = 0x2634 = 98.12 %
   stack_t1_free  = 0x0032 = 50 words  (200 B of 384)
   stack_t2_free  = 0x0212 = 530 words (2,120 B of 5,120)
   stack_t3_free  = 0x0050 = 80 words  (320 B of 512)
   stack_t6_free  = 0x0090 = 144 words (576 B of 768)
   stack_t7_free  = 0x0058 = 88 words  (352 B of 512)
   inf_wcet_us    = 0x00C8 = 200 µs
   rb_max_fill    = 0x0032 = 50 samples
```

The two numbers to check first are `cpu_idle_x100` (should be > 9000) and `rb_max_fill` (should be ≈ 50). Together they answer "is the pipeline keeping up?".

---

# Part IX — Operations

## 78. Build and Flash

### 78.1 Build

```bash
pio run -d Application
```

Expected output tail:

```
RAM:   [====      ]  47.4% (used 31084 bytes from 65528 bytes)
Flash: [========  ]  84.4% (used 193580 bytes from 229376 bytes)
```

Artefacts land in `Application/.pio/build/genericSTM32F401CC/`:

| File | Size | Use |
|---|---|---|
| `firmware.bin` | 193,600 B | Raw image for `st-flash` or OTA |
| `firmware.elf` | 1,239,372 B | Symbols and debug info for GDB |

### 78.2 Flash over SWD

```bash
pio run -d Application -t upload
```

or directly:

```bash
st-flash write Application/.pio/build/genericSTM32F401CC/firmware.bin 0x08008000
```

**The address matters.** Flashing to `0x08000000` would overwrite the bootloader and leave an image whose vector table is in the wrong place. The result is a board that does not boot and cannot be entered over UART.

### 78.3 Flash over the air

The full OTA path is documented in [`../STM32_OVERVIEW.md`](../STM32_OVERVIEW.md). Summary:

```
   1. Encrypt and chunk firmware.bin with the host tooling (AES-128-GCM,
      2,032-byte plaintext chunks + 16-byte tag)
   2. Publish to the gateway over MQTT; it stores the image in LittleFS
   3. Gateway command B1  → enter bootloader
   4. Gateway command B3  → erase sectors 2-5
   5. Gateway command B5  → stream the image
   6. Gateway command B4  → jump to the application
```

### 78.4 Static analysis

```bash
pio check -d Application
```

Configured for cppcheck with `--enable=all`. Given the MISRA discipline in the pipeline modules, this is worth running before any change to them.

### 78.5 Debug

```bash
pio debug -d Application
```

`debug_build_flags = -Og -ggdb3 -g3` replaces `-O2` with `-Og` for a debug session, which makes single-stepping the pipeline tractable. Note this changes code generation — a bug that only appears at `-O2` will not reproduce under `pio debug`.

## 79. Bench Procedures

### 79.1 Confirm it is alive

```
   1. Power on with the gateway connected.
   2. Watch PC13.
      Steady dim glow  → Thread 1 running at 100 Hz         ✓
      Slow 1 Hz blink  → Thread 1 stopped, Thread 4 alive   ✗
      Static           → nothing running, or reset looping  ✗
   3. Scope PA8. Expect ~9 pulses/s of 1-3 ms each.
   4. Scope PA9 (UART TX). Expect bursts matching PA8.
```

### 79.2 Decode telemetry without the gateway

Any USB-UART adapter on PA9 at 115200 8N1.

```bash
python3 Application/tools/capture_frame.py /dev/ttyUSB0
```

and to verify framing and CRC:

```bash
python3 Application/tools/verify_frame.py capture.bin
```

Manual decode of one frame:

```
   Bytes:  0C 01 01 01 2A 00 00 30 39 <4 CRC bytes>
           │  │  │  └──── payload ───┘
           │  │  └─ ECU 0x01 = STM32
           │  └──── type 0x01 = classification
           └─────── LEN 0x0C = 12 → payload = 12 - 6 = 6 bytes
                                  → total frame = 6 + 7 = 13 bytes
```

CRC reference implementation ([§54](#54-the-crc32-quirk)):

```python
def frame_crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(32):
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if crc & 0x80000000 else (crc << 1) & 0xFFFFFFFF
    return crc
```

Computed over `frame[1 : 3 + payload_len]` and compared against the trailing four bytes, big-endian.

### 79.3 Verify the sensor bring-up

With a debugger attached, read three symbols:

| Symbol | Healthy value |
|---|---|
| `dbg_init_stage` | `99` |
| `dbg_whoami_val` | `0x68` |
| `dbg_svc_err` | `0` |

If `dbg_init_stage` is anything else, the table in [§76.1](#761-mpu6050-initialisation-stages) localises the failure exactly.

### 79.4 Verify the pipeline is keeping up

Read three counters:

```
   g_t2_wake_count      / uptime_s   should be ≈ 100
   g_t2_inference_count / uptime_s   should be ≈ 4
   ratio                             should be ≈ 25 : 1
   g_t2_queue_drops                  should be 0
   RingBuffer_GetMaxFill()           should be ≈ 50
   RingBuffer_GetDropCount()         should be 0
```

Or read `rb_max_fill` from the heartbeat, which is the same number without a debugger.

### 79.5 Validate the model on target

With `REPLAY_MODE = 1`, let it run for 30 seconds and read:

```
   g_replay_vote_rough   should greatly exceed
   g_replay_vote_smooth
```

because `replay_data.h` contains rough-road data (`REPLAY_EXPECTED_CLASS_STR = "rough"`).

For a full numerical comparison against the Python reference:

```bash
python3 Application/tools/tflite_reference.py Application/data/rough/rough_031_20260426_013229.csv
```

```bash
python3 Application/tools/mcu_compare.py --mcu Application/tools/mcu_dump.txt --ref reference_out.json
```

### 79.6 Measure the actual sample rate

```
   1. Scope PC13.
   2. Measure the toggle frequency.
   Expected: 100 Hz toggle → 50 Hz LED blink.
   A frequency of 50 Hz toggle would mean TIM2 is running at half rate,
   which means the timer-clock doubling assumption is wrong (§7.2).
```

### 79.7 Exercise the bootloader entry

```
   1. Send 0xAA 0xEB on PA10 at 115200.
   2. Expect 0xEE 0xAA back within ~1 ms.
   3. The node resets; telemetry stops.
   4. Send 0xAA 0xEB again.
   5. Expect 0xEE 0xFB — the bootloader is now resident.
```

Step 5 answering `0xEE 0xAA` instead would mean the application is running again, which means the boot flag did not persist across the reset — a serious finding.

## 80. Failure Modes

| Symptom | Likely cause | Confirm by | Fix |
|---|---|---|---|
| No telemetry at all | Flashed to `0x08000000`, or VTOR line removed | Read `0x08008000` over SWD | Reflash to `0x08008000` |
| Telemetry stops and restarts every few seconds | Watchdog reset loop | `uptime_ms` resets in the heartbeat | Find which of Threads 1-4 stopped |
| Classification never changes | `REPLAY_MODE = 1` | Read the source | Set to 0 |
| Classification always SMOOTH, confidence 0 | `Inference_Init` failed | Debugger: `s_initialised` in `inference.c` | Check the activations arena and CubeAI init |
| No classification frames for 2.3 s after boot | Vote warm-up | Expected behaviour ([§41.1](#411-the-warm-up-gate)) | — |
| `rb_max_fill` climbing toward 128 | Thread 2 falling behind | Heartbeat | Profile `Features_Extract` |
| Temperature always 0 | ADC read failing, or LM35 disconnected | `LOG_WARN(ADC_TIMEOUT)` frame | Check PA1 wiring |
| Both distances `0xFFFF` | Sensors unplugged or TIM3 not running | Scope PA4/PA5 for the trigger pulse | Check `TIM_Start(TIM_ID_3)` ran |
| One distance always exactly 400 | The mismatched-id bug ([§30.3](#303-a-real-bug-a-mismatched-id-reports-400-cm)) | Sensor id in the queue message | Move the clamp inside the id check |
| Node hangs, no watchdog reset | `configASSERT` fired (interrupts disabled) | Debugger: PC inside `configASSERT` | Find the failing assertion |
| Stack overflow hook hit | A task exceeded its stack | Debugger: `vApplicationStackOverflowHook` | Increase that task's stack |
| Sensor init fails at stage 21 | `I2C_SVC_Init` ordering, or DMA misconfigured | `dbg_init_stage` | Check `main()`'s init order |
| Sensor init fails at stage 23 | Wrong device, or AD0 pulled high | `dbg_whoami_val` | 0x69 = address mismatch; 0x71 = MPU9250 |
| Logs stop appearing | Logger task blocked, or rate limit saturated | Debugger: `s_drop_count` | Check `g_frame_queue` depth |
| Frames corrupt on the gateway | Baud mismatch from HSI drift ([§7.1](#71-hsi-versus-hse--a-deliberate-divergence-from-the-bootloader)) | Scope the bit time on PA9 | Consider switching to HSE |

## 81. Troubleshooting Decision Trees

### 81.1 No telemetry

```
   No frames arriving at the gateway
        │
        ├─ Is PC13 glowing dimly?
        │    no ──► Is PC13 blinking at 1 Hz?
        │    │        yes ──► Thread 1 is dead. Thread 4 still runs.
        │    │        │        → check MPU6050 init (dbg_init_stage)
        │    │        │        → check TIM2 is started
        │    │        no  ──► Nothing is running.
        │    │                 → attach SWD; read PC
        │    │                 → if in vApplicationStackOverflowHook: stack overflow
        │    │                 → if in configASSERT loop: an assertion failed
        │    │                 → if in the bootloader: VTOR or flash address is wrong
        │    yes
        ▼
   ├─ Is PA8 pulsing?
   │    no ──► Thread 3 is not transmitting.
   │    │       → is g_frame_queue empty? (no producers running)
   │    │       → is Thread 3 blocked in ulTaskNotifyTake? (PA8 stuck HIGH)
   │    yes
   ▼
   ├─ Is PA9 toggling?
   │    no ──► UART or DMA misconfigured.
   │    │       → check DMA2 Stream 7 Channel 4
   │    │       → check USART1 clock enabled
   │    yes
   ▼
   └─ Frames are being sent. The problem is downstream:
        → wrong baud rate on the gateway
        → wiring (PA9 → ESP32 RX, and a common ground)
        → gateway is in bootloader mode (stm32_bootloader_mode_active)
```

### 81.2 Watchdog reset loop

```
   uptime_ms keeps resetting to near zero
        │
        │  One of Threads 1-4 is not setting its alive flag.
        │  Halt the core BEFORE the reset and read the four flags:
        ▼
   IWDG_Thread1_Alive == 0 ?
        └─► Thread 1 stalled.
              → live mode: MPU6050_TriggerRead returning BUSY forever
                (mpu_busy stuck — a wedged I2C bus, §69.3)
              → is TIM2 still firing? scope PC13
   IWDG_Thread2_Alive == 0 ?
        └─► Thread 2 stalled.
              → Inference_Run hung inside the CubeAI runtime
              → RingBuffer_PeekWindow failing repeatedly
   IWDG_Thread3_Alive == 0 ?
        └─► Thread 3 stalled.
              → blocked in ulTaskNotifyTake: DMA TC never fired (PA8 stuck high)
              → OR no frames are being produced at all (§32.2)
   IWDG_Thread4_Alive == 0 ?
        └─► Thread 4 stalled.
              → vTaskGetInfo on an invalid handle
              → a task handle was never assigned
   All four set, still resetting ?
        └─► The idle task never runs — something is spinning at priority > 0.
              → check for a task that stopped blocking
```

### 81.3 Classification looks wrong

```
   The reported class does not match the actual surface
        │
        ├─ Is REPLAY_MODE 1?
        │    yes ──► You are seeing replayed data. Set it to 0.
        │    no
        ▼
   ├─ Is dbg_whoami_val 0x68 and dbg_init_stage 99?
   │    no ──► The sensor is not initialised. Fix that first.
   │    yes
   ▼
   ├─ At rest, does accel_z read ≈ +8192?
   │    reads ≈ -8192 ──► board mounted upside down
   │    reads ≈ 0, accel_x ≈ +8192 ──► the axis remap is not applied
   │    yes
   ▼
   ├─ Is rb_max_fill ≈ 50 and drop count 0?
   │    no ──► samples are being lost; the window has holes
   │    yes
   ▼
   ├─ Do stat_mean[] / stat_std[] match the trained scaler?
   │    → compare include/stat_norm.c against models/stat_norm.c
   │      (they are duplicated, §5)
   │    → regenerate from the training pipeline if unsure
   ▼
   └─ Run the replay validation (§79.5). If g_replay_vote_rough does not
      dominate on rough data, the C pipeline diverges from Python.
      → use tools/mcu_compare.py to find which stage
```

### 81.4 A sensor stopped reporting

```
   Temperature or distance frames stopped
        │
        ├─ Are OTHER frame types still arriving?
        │    no ──► the whole node is down; see §81.1
        │    yes
        ▼
   ├─ Did a log frame arrive for that sensor?
   │    LOG_WARN(0x14) ──► ADC read timing out
   │    LOG_WARN(0x15) ──► ultrasonic 1 no echo
   │    LOG_WARN(0x16) ──► ultrasonic 2 no echo
   │    nothing ──► the task itself has stalled (neither is
   │                watchdog-monitored, §32.3)
   ▼
   └─ Attach SWD and check the task state:
        → is Thread 6 / Thread 7 in the blocked list?
        → is its wake time in the past? (vTaskDelayUntil drift)
```

## 82. Known Gaps

Ranked by consequence. Cross-referenced to the section that explains each.

### Critical

| # | Gap | Detail |
|---|---|---|
| A1 | ~~**`REPLAY_MODE = 1` in the committed build**~~ — **fixed**, now `0` (live IMU). Residual: nothing in the telemetry reveals which mode is active | [§43](#43-replay-mode) |
| A2 | **~110 KB of unused CMSIS-DSP FFT tables** — `arm_rfft_fast_init_f32()` keeps every length's tables alive; only 64-point is used. Flash would drop from 84.4 % to ~36 % | [§14.1](#141-half-the-flash-is-fft-tables-that-are-never-used) |
| A3 | ~~**`on_mpu_read_done` dereferences NULL on an I2C error**~~ — **fixed**; the callback now flags the failure and Thread 1 drops the tick instead of pushing a stale or bogus sample | [§69.4](#694-the-error-callback-passes-null) |

### High

| # | Gap | Detail |
|---|---|---|
| A4 | **Ultrasonic mismatched-id reports exactly 400 cm** and logs INFO (healthy) — the `> 400` clamp is applied to the `0xFFFF` sentinel | [§30.3](#303-a-real-bug-a-mismatched-id-reports-400-cm) |
| A5 | ~~**`MPU6050_Init` return value discarded**~~ — **fixed**; bring-up is retried 3× and the result stored in `g_mpu_init_status`, then logged once via `LOG_CODE_MPU6050_TIMEOUT` after `Logger_Init()` | [§69.2](#692-the-return-value-is-discarded) |
| A6 | **`-ffast-math` may delete `feat_sanitize`** — `-ffinite-math-only` licenses the compiler to assume `isnan`/`isinf` are always false | [§13.2](#132--ffast-math-and-its-implications) |
| A7 | **Thread 3's watchdog liveness depends on other tasks producing frames** — it only sets its flag after a successful transmit | [§32.2](#322-the-timing-budget) |
| A8 | **`heartbeat.h`'s wire-format comment is stale by 6 bytes and 4 fields** — a gateway parser written from it misreads everything past offset 10 | [§51.1](#511-the-header-comment-is-stale) |
| A9 | **The `README.md` feature-layout table is transposed** — it shows statistic-major, the code is channel-major | [§37.1](#371-the-actual-feature-layout) |
| A10 | **The `README.md` claims Thread 5 erases Flash sector 1** — it does not, and doing so would destroy half the bootloader | [§28.2](#282-it-does-not-erase-flash) |

### Medium

| # | Gap | Detail |
|---|---|---|
| A11 | **`configUSE_TIME_SLICING = 0` with four tasks at priority 1** — safe only because all four block every iteration | [§15.1](#151-scheduler) |
| A12 | **No I2C bus-recovery** — a wedged bus leaves `mpu_busy` set forever; only the watchdog recovers | [§69.3](#693-the-busy-guard-and-its-dead-end) |
| A13 | **Thread 5's RX notification window** — bytes arriving before its first instruction buffer without notifying, so the first enter-bootloader attempt after a reset can fail | [§28.5](#285-registration-order) |
| A14 | **`inf_wcet_us` measures only the CNN**, not the 10×-more-expensive feature extraction | [§46](#46-wcet-measurement) |
| A15 | **`inf_wcet_us` is never reset**, so the "5-second peak" is really an all-time maximum | [§27.2](#272-the-two-level-averaging) |
| A16 | **`vApplicationStackOverflowHook` discards the task name** — the reset cause is unrecoverable | [§15.4](#154-debug-and-safety) |
| A17 | **`configASSERT` disables interrupts before hanging**, converting a recoverable fault into an unrecoverable one | [§15.5](#155-configassert) |
| A18 | **`s_thread3_stack[256]` vs `THREAD3_STACK_WORDS = 128`** — 512 bytes allocated and unused | [§16](#16-static-allocation-policy) |
| A19 | **Watchdog flags are not preserved across a reset** — which task stalled is unknowable post-hoc | [§32.4](#324-failure-preserves-evidence) |
| A20 | **The IWDG-survives-`SYSRESETREQ` claim is unverified** and, if true, would break OTA | [§32.5](#325-the-iwdg-survives-software-resets) |
| A21 | **`norm_params.h` and `stat_norm.*` are duplicated** in `include/` and `models/` — a divergence would silently change inference | [§5](#5-source-tree-map) |
| A22 | **Machine-specific absolute include paths** in `build_flags` — the project does not build elsewhere | [§13.4](#134-the-build_flags-include-a-machine-specific-absolute-path) |
| A23 | **CubeAI input index assumption is unchecked at runtime** — a regenerated model with swapped inputs causes a 250-byte overrun | [§40.1](#401-input-ordering-is-fixed-by-the-graph) |
| A24 | **HSI-derived baud rate** — ±4 % over temperature against UART's ~±2 % budget | [§7.1](#71-hsi-versus-hse--a-deliberate-divergence-from-the-bootloader) |

### Low

| # | Gap | Detail |
|---|---|---|
| A25 | **UART service allocates 3 instances, uses 1** — 1,024 bytes of dead `.bss` | [§11](#11-sram-budget) |
| A26 | **`FLASH.c` is 745 lines of unused driver** and `FLASH_MassErase` would erase the bootloader | [§67](#67-flash-driver) |
| A27 | **`svc[].error` is set and never read** — every UART receive error is silent | [§66.5](#665-the-silent-error-flag) |
| A28 | **`LOG_CODE_INFERENCE_FAIL` and 8 other codes are defined and never emitted** | [§74](#74-log-code-taxonomy) |
| A29 | **Seven `g_sensor_*` globals are declared, commented as idle-hook outputs, and never written** | [§76.5](#765-declared-and-unused) |
| A30 | **`HCSR04_Trigger` pulse is ~65 µs, not the documented 10 µs**, and uses a volatile loop rather than `DWT_DelayUs` | [§30.5](#305-the-10-µs-trigger-pulse) |
| A31 | **No `TIM_IC_SetPolarity`** — the ISR pokes `CCER` at a hardcoded address, forcing the `#undef TIM2/3/4/5` workaround | [§62.5](#625-the-input-capture-gap) |
| A32 | **Two timestamp conventions** — Thread 2 uses raw ticks, Threads 6 and 7 multiply by `portTICK_PERIOD_MS` | [§29.4](#294-the-timestamp) |
| A33 | **`WINDOW_STRIDE` lives in `main.c`**, not with the other model hyperparameters | [§42.3](#423-window_stride-lives-in-the-wrong-file) |
| A34 | **Channel ordering is a five-way unchecked contract** across flatten, scale, features, quantise and the model | [§36.2](#362-the-channel-ordering-is-a-contract) |
| A35 | **`GPIO_ERROR_PORT_NOT_ENABLED` is declared and never checked** — a gated port silently ignores configuration | [§59.2](#592-gpio_error_port_not_enabled-is-not-implemented) |
| A36 | **Rapidly oscillating faults are never logged** — the debounce requires two consecutive identical readings | [§75.5](#755-what-the-filters-cost) |
| A37 | **Both distances in one frame are 50-100 ms apart** with a single timestamp | [§30.4](#304-sequential-triggering-avoids-crosstalk--mostly) |
| A38 | **PC13 has two unsynchronised writers** (Threads 1 and 4) using a non-atomic toggle | [§24.3](#243-the-pc13-led) |
| A39 | **PA8 is raised after the DMA starts**, so it trails the first bit by ~300 ns | [§26.1](#261-the-sync-pulse-is-raised-after-the-dma-starts) |
| A40 | **`src/IWGD.c`** — filename typo for `IWDG` | [§68.4](#684-the-filename) |
| A41 | **`IWDG_INTERFACE.h` documents only 3 alive flags**; there are 4 | [§32.3](#323-which-tasks-are-not-covered) |
| A42 | **`ADC_INTERFACE.h`'s LM35 formula comment is wrong** (contradicted by its own worked example) | [§29.1](#291-the-lm35-conversion) |
| A43 | **`MPU6050.c`'s axis-remap comment contradicts itself** — two different remaps described, one stale | [§34.1](#341-the-axis-remap) |
| A44 | **`FRAME_CRC32_XOROUT` is defined and never applied** | [§54.4](#544-the-unused-xorout) |
| A45 | **Four bootloader FRAME types are declared and unimplemented** | [§48](#48-frame-type-catalogue) |
| A46 | **Confidence is a margin, not a probability**, despite the name | [§40.3](#403-confidence-from-the-margin) |
| A47 | **`vote.h`'s documented warm-up fallback is not implemented** — Thread 2 emits nothing instead of the raw label | [§41.1](#411-the-warm-up-gate) |
| A48 | **Threads 4, 5 and the Logger have no CPU or stack telemetry** | [§20.3](#203-what-is-not-measured) |
| A49 | **`models/model_data.h` holds a 16 KB unused `.tflite` blob** whose collection depends on `--gc-sections` | [§44.4](#444-modelsmodel_datah-is-not-used-at-runtime) |
| A50 | **No reset-cause reporting** — `RCC->CSR` flags would distinguish watchdog from power-on from software reset | [§77.1](#771-what-is-missing) |

---

# Appendices

## Appendix A — Pin and Peripheral Table

| Pin | AF | Peripheral | Direction | Mode | Speed | Pull | Used by |
|---|---|---|---|---|---|---|---|
| PA1 | — | ADC1_IN1 | in | Analog | Low | None | Thread 6 |
| PA4 | — | GPIO | out | PP | Very high | None | Thread 7 (trigger 1) |
| PA5 | — | GPIO | out | PP | Very high | None | Thread 7 (trigger 2) |
| PA6 | AF2 | TIM3_CH1 | in | Alternate | Very high | Pull-down | `on_hcsr04_capture` |
| PA7 | AF2 | TIM3_CH2 | in | Alternate | Very high | Pull-down | `on_hcsr04_capture` |
| PA8 | — | GPIO | out | PP | Very high | None | Thread 3 (sync) |
| PA9 | AF7 | USART1_TX | out | Alternate | Very high | None | Thread 3 (DMA) |
| PA10 | AF7 | USART1_RX | in | Alternate | Very high | Pull-up | Thread 5 (IRQ) |
| PB6 | AF4 | I2C1_SCL | bidir | Alternate OD | High | Pull-up | MPU6050 |
| PB7 | AF4 | I2C1_SDA | bidir | Alternate OD | High | Pull-up | MPU6050 |
| PC13 | — | GPIO | out | PP | Low | None | Threads 1, 4 |

### Peripheral configuration summary

| Peripheral | Configuration |
|---|---|
| TIM2 | PSC 8399, ARR 99, up-count, ARPE on, UIE on → 100 Hz |
| TIM3 | PSC 83, ARR 65535, up-count, IC on CH1/CH2 → 1 µs resolution |
| I2C1 | Fast mode 400 kHz, duty 2, 7-bit, DMA, analog filter on, digital filter 0 |
| USART1 | 115200, 8N1, no parity, oversample 16, DMA TX / IRQ RX |
| ADC1 | 12-bit, CH1, 480-cycle sample, single conversion, software trigger, PCLK2/4 = 21 MHz |
| DMA1 S0 C1 | I2C1 RX, periph→mem, byte, mem-inc, normal, high priority |
| DMA2 S7 C4 | USART1 TX, mem→periph, byte, mem-inc, normal, high priority |
| IWDG | PR /32, RLR 3000 → 3.00 s nominal, 2.04 s at 47 kHz LSI |
| DWT | CYCCNT enabled, free-running at 84 MHz |
| SysTick | 1 kHz, FreeRTOS tick, priority 15 |

## Appendix B — Interrupt Vector Reference

| Vector | IRQ # | Priority | Handler | Calls FromISR | Purpose |
|---|---|---|---|---|---|
| SysTick | −1 | 15 | `xPortSysTickHandler` | kernel | FreeRTOS tick |
| PendSV | −2 | 15 | `xPortPendSVHandler` | kernel | Context switch |
| SVCall | −5 | 15 | `vPortSVCHandler` | kernel | First task start |
| TIM2 | 28 | 5 | `TIM2_IRQHandler` → `on_tim2_update` | `xSemaphoreGiveFromISR` | 100 Hz tick |
| TIM3 | 29 | 5 | `TIM3_IRQHandler` → `on_hcsr04_capture` | `xQueueSendFromISR` | Echo capture |
| I2C1_EV | 31 | 6 | I2C driver | — | I2C state machine |
| I2C1_ER | 32 | 6 | I2C driver | — | I2C errors |
| USART1 | 37 | 6 | `uart_svc_callback` | `vTaskNotifyGiveFromISR` | RX byte |
| DMA1_Stream0 | 11 | 7 | `i2c_svc_dma_rx_complete` → `mpu_dma_done` → `on_mpu_read_done` | `xSemaphoreGiveFromISR` | IMU read done |
| DMA2_Stream7 | 70 | 7 | `uart_svc_dma_tx_complete` → `on_uart_tx_done` | `vTaskNotifyGiveFromISR` | Frame sent |

All application ISRs sit at priority ≥ 5 (`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`), so all may legally call `*FromISR` APIs. Verified in [§22](#22-interrupt-priority-map).

### Callback chains

```
   TIM2 update
     → TIM2_IRQHandler          (TIM.c)
       → on_tim2_update          (main.c)
         → xSemaphoreGiveFromISR(s_tim_sem)
           → Thread 1 wakes

   I2C1 RX DMA complete
     → DMA1_Stream0_IRQHandler   (DMA.c)
       → i2c_svc_dma_rx_complete (I2C_SERVICE.c)
         → mpu_dma_done           (MPU6050.c)
           → MPU6050_ParseRaw     (axis remap)
           → on_mpu_read_done     (main.c)
             → xSemaphoreGiveFromISR(s_dma_sem)
               → Thread 1 resumes

   USART1 RXNE
     → USART1_IRQHandler         (UART.c)
       → uart_svc_callback        (UART_SERVICE.c)
         → rx_push into the ring
         → vTaskNotifyGiveFromISR(Thread 5)

   USART1 TX DMA complete
     → DMA2_Stream7_IRQHandler   (DMA.c)
       → uart_svc_dma_tx_complete (UART_SERVICE.c)
         → UART_DisableTxDMA
         → on_uart_tx_done        (main.c)
           → vTaskNotifyGiveFromISR(Thread 3)

   TIM3 capture (CH1 or CH2)
     → TIM3_IRQHandler           (TIM.c)
       → on_hcsr04_capture        (main.c)
         → TIM_IC_GetCapture
         → toggle CCER.CCxP
         → on falling edge: xQueueSendFromISR(g_ultrasonic_queue)
```

## Appendix C — API Index

### Application layer

| Function | File | Section |
|---|---|---|
| `Frame_Build` | `FRAME.c` | [§47](#47-frame-wire-format) |
| `Frame_CRC32` | `FRAME.c` | [§54](#54-the-crc32-quirk) |
| `Logger_Init` | `logger.c` | [§31](#31-logger-task) |
| `Logger_Log` | `logger.c` | [§75](#75-debounce-and-deduplication) |

### ML pipeline

| Function | File | Section |
|---|---|---|
| `Scale_RawWindow` | `scale.c` | [§36](#36-stage-3--scaling) |
| `Features_Init` | `features.c` | [§37](#37-stage-4--feature-extraction) |
| `Features_Extract` | `features.c` | [§37](#37-stage-4--feature-extraction) |
| `Quantize_NormalizeStat` | `quantize.c` | [§38](#38-stage-5--z-score-normalisation) |
| `Quantize_TS` | `quantize.c` | [§39](#39-stage-6--quantisation) |
| `Quantize_Stat` | `quantize.c` | [§39](#39-stage-6--quantisation) |
| `Inference_Init` | `inference.c` | [§40](#40-stage-7--inference) |
| `Inference_Run` | `inference.c` | [§40](#40-stage-7--inference) |
| `Vote_Init` / `Push` / `Ready` / `Decide` | `vote.c` | [§41](#41-stage-8--temporal-voting) |

### Devices

| Function | File | Section |
|---|---|---|
| `MPU6050_Init` | `MPU6050.c` | [§69](#69-mpu6050-driver) |
| `MPU6050_TriggerRead` | `MPU6050.c` | [§69.3](#693-the-busy-guard-and-its-dead-end) |
| `HCSR04_Init` / `HCSR04_Trigger` | `HCSR04.c` | [§70](#70-hc-sr04-driver) |
| `RingBuffer_*` | `RING_BUFFER.c` | [§71](#71-ring-buffer-module) |

### Services

| Function | File | Section |
|---|---|---|
| `I2C_SVC_Init` / `WriteReg` / `ReadBurst_DMA` | `I2C_SERVICE.c` | [§64](#64-i2c-driver-and-service) |
| `UART_SVC_Init` / `TransmitDMA` / `Receive` | `UART_SERVICE.c` | [§66](#66-uart-driver-and-service) |
| `UART_SVC_SetRxNotifyTask` | `UART_SERVICE.c` | [§28.5](#285-registration-order) |

### Peripherals

| Function | File | Section |
|---|---|---|
| `RCC_INIT_84MHz_HSI` / `RCC_EN_CLK_PERIPHERAL` | `RCC.c` | [§58](#58-rcc-driver) |
| `GPIO_INIT` / `GPIO_WritePin` / `GPIO_TogglePin` | `GPIO.c` | [§59](#59-gpio-driver) |
| `NVIC_SetPriority` / `NVIC_EnableIRQ` | `NVIC.c` | [§60](#60-nvic-driver) |
| `DWT_Init` / `DWT_GetCycles` | `DWT.c` | [§61](#61-dwt-driver) |
| `TIM_Init` / `TIM_IC_Init` / `TIM_Start` | `TIM.c` | [§62](#62-tim-driver) |
| `ADC_Init` / `ADC_Read` / `ADC_LM35_ToTenthsCelsius` | `ADC.c` | [§63](#63-adc-driver) |
| `DMA_Init` / `DMA_Start` | `DMA.c` | [§65](#65-dma-driver) |
| `UART_Init` / `UART_Transmit_Polling` | `UART.c` | [§66](#66-uart-driver-and-service) |
| `IWDG_Init` / `IWDG_Start` / `IWDG_SupervisorFeed` | `IWGD.c` | [§68](#68-iwdg-driver) |

## Appendix D — File Map

| File | Lines | Role |
|---|---|---|
| `src/main.c` | 1,192 | Tasks, ISR callbacks, initialisation |
| `src/TIM.c` | 1,216 | Timer driver, incl. input capture |
| `src/I2C.c` | 1,232 | I2C register driver |
| `src/FLASH.c` | 745 | **Unused** Flash driver |
| `src/UART.c` | 708 | USART register driver |
| `src/DMA.c` | 685 | DMA controller driver |
| `src/IWGD.c` | 606 | IWDG + thread supervisor |
| `src/RING_BUFFER.c` | 589 | SPSC lock-free ring buffer |
| `src/UART_SERVICE.c` | 547 | UART ring buffers + DMA TX |
| `src/RCC.c` | 545 | Clock configuration |
| `src/GPIO.c` | 523 | GPIO driver |
| `src/MPU6050.c` | 522 | IMU driver |
| `src/features.c` | 495 | 50-feature extractor |
| `src/I2C_SERVICE.c` | 492 | Two-phase I2C read |
| `src/NVIC.c` | 478 | Interrupt controller |
| `src/logger.c` | 217 | Logger task + filters |
| `src/FRAME.c` | 162 | Wire protocol |
| `src/inference.c` | 162 | CubeAI wrapper |
| `src/DWT.c` | 137 | Cycle counter |
| `src/vote.c` | 130 | Temporal voting |
| `src/ADC.c` | 127 | ADC polling driver |
| `src/quantize.c` | 99 | float32 → int8 |
| `src/scale.c` | 41 | int16 → float32 |
| `src/HCSR04.c` | 37 | Trigger pulse |
| `src/stat_norm.c` | 33 | Scaler constants |
| `include/IWDG_INTERFACE.h` | 606 | Watchdog API + extensive docs |
| `include/TIM_INTERFACE.h` | 527 | Timer API + design notes |
| `include/I2C_INTERFACE.h` | 449 | I2C API |
| `include/UART_INTERFACE.h` | 279 | UART API |
| `include/RCC_INTERFACE.h` | 245 | Clock API |
| `include/replay_data.h` | 233 | 200 recorded IMU samples |
| `include/RING_BUFFER.h` | 210 | Ring buffer API + invariants |
| `include/FLASH_INTERFACE.h` | 200 | Flash API (unused) |
| `include/DMA_INTERFACE.h` | 194 | DMA API |
| `include/DWT_INTERFACE.h` | 175 | Cycle counter API |
| `include/GPIO_INTERFACE.h` | 144 | GPIO API |
| `include/FreeRTOSConfig.h` | 141 | Kernel configuration |
| `include/I2C_SERVICE.h` | 129 | I2C service API |
| `include/MPU6050.h` | 119 | IMU API + register map |
| `include/vote.h` | 113 | Voting API |
| `include/STD_BUFFER.h` | 108 | `Buffer_t` + macros |
| `include/logger.h` | 92 | Log codes and macros |
| `include/heartbeat.h` | 89 | Heartbeat payload (comment stale) |
| `include/NVIC_INTERFACE.h` | 89 | Packed `IRQn_t` |
| `include/FRAME.h` | 84 | Frame constants |
| `include/ADC_INTERFACE.h` | 84 | ADC API |
| `include/inference.h` | 76 | Inference API |
| `include/quantize.h` | 77 | Quantisation API |
| `include/features.h` | 71 | Feature extractor API |
| `include/frame_request.h` | 39 | `FrameRequest_t` |
| `include/scale.h` | 42 | Scaling API |
| `include/log_payload.h` | 21 | Log payload struct |
| `include/HCSR04.h` | 19 | Ultrasonic API |
| `include/stat_norm.h` | 17 | Scaler declarations |
| `include/norm_params.h` | 14 | **All pipeline constants** |
| `models/model_data.h` | 1,373 | Unused `.tflite` blob + quantisation record |
| `lib/CubeAI/network/network.c` | 1,131 | Generated network graph |
| `tools/mcu_verify.c` | 773 | On-target verification harness |
| `tools/collect_data_app.c` | 603 | Earlier bare-metal collector |
| `tools/mcu_compare.py` | 548 | Host-side comparison |
| `tools/test_vectors.h` | 576 | Committed regression vectors |
| `tools/gen_test_vectors.py` | 457 | Vector generator |
| `tools/tflite_reference.py` | 450 | Python reference runner |

## Appendix E — Glossary

| Term | Meaning |
|---|---|
| **AAPCS** | ARM Architecture Procedure Call Standard — the calling convention |
| **AF** | Alternate Function — a GPIO pin driven by a peripheral |
| **ARPE** | Auto-Reload Preload Enable — buffers ARR writes until the next update event |
| **BSRR** | Bit Set/Reset Register — atomic GPIO writes |
| **CCER** | Capture/Compare Enable Register — holds the input-capture polarity bits |
| **CubeAI** | STM32Cube.AI (X-CUBE-AI) — ST's neural-network code generator |
| **DLPF** | Digital Low-Pass Filter — the MPU6050's on-chip anti-alias filter |
| **DMB** | Data Memory Barrier — orders memory accesses |
| **DWT** | Data Watchpoint and Trace — the Cortex-M debug unit hosting `CYCCNT` |
| **EOC** | End Of Conversion — ADC completion flag |
| **HFE** | High-Frequency Energy — the fraction of spectral energy above 25 Hz |
| **HSI** | High-Speed Internal oscillator — 16 MHz RC |
| **HWM** | High-Water Mark — minimum-ever free stack |
| **IQR** | Inter-Quartile Range — 75th minus 25th percentile |
| **IWDG** | Independent Watchdog — LSI-clocked, cannot be stopped once started |
| **LSI** | Low-Speed Internal oscillator — ~32 kHz, drives the IWDG |
| **MAD** | Here: mean absolute first difference, `mean(|diff(x)|)` |
| **MSP** | Main Stack Pointer — used by `main()` and every ISR |
| **NDTR** | Number of Data To Transfer Register — the DMA transfer count |
| **P2P** | Peak-to-peak, `max − min` |
| **PSC** | Prescaler register |
| **RFFT** | Real Fast Fourier Transform |
| **SPSC** | Single-Producer Single-Consumer |
| **TC** | Transmission Complete — UART flag, set when the shift register empties |
| **TXE** | Transmit data register Empty — UART flag |
| **UIF** | Update Interrupt Flag — timer overflow flag, `rc_w0` |
| **VTOR** | Vector Table Offset Register |
| **WCET** | Worst-Case Execution Time |
| **ZRO** | Zero-Rate Output — the gyroscope's bias at rest |

---

For the bootloader, see [`../Bootloader/BOOTLOADER.md`](../Bootloader/BOOTLOADER.md). For how the two cooperate, see [`../STM32_OVERVIEW.md`](../STM32_OVERVIEW.md).*
