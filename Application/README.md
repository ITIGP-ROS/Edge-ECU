# Road Surface Classifier — STM32F401CC + FreeRTOS + TinyML
## Complete Technical Reference

> **Project**: Embedded AI Road Surface Classification System  
> **Target**: STM32F401CC (Cortex-M4 @ 84 MHz, 256 KB Flash, 64 KB SRAM)  
> **RTOS**: FreeRTOS (static allocation only)  
> **Build**: PlatformIO + STM32CubeAI  
> **Framework**: Custom CMSIS bare-metal HAL (no STM32 HAL)

---

## Table of Contents

1. [Project Overview](#1-project-overview)
   - 1.1 [Purpose and Goals](#11-purpose-and-goals)
   - 1.2 [Key Capabilities](#12-key-capabilities)
   - 1.3 [Architecture Summary](#13-architecture-summary)

2. [Hardware Platform](#2-hardware-platform)
   - 2.1 [Microcontroller Specifications](#21-microcontroller-specifications)
   - 2.2 [Clock Configuration](#22-clock-configuration)
   - 2.3 [Pin Assignment Table](#23-pin-assignment-table)
   - 2.4 [Peripheral Overview](#24-peripheral-overview)
   - 2.5 [MPU6050 IMU Sensor](#25-mpu6050-imu-sensor)
   - 2.6 [LM35 Temperature Sensor](#26-lm35-temperature-sensor)
   - 2.7 [HC-SR04 Ultrasonic Sensors](#27-hc-sr04-ultrasonic-sensors)

3. [Memory Architecture](#3-memory-architecture)
   - 3.1 [Flash Layout (Bootloader + Application)](#31-flash-layout-bootloader--application)
   - 3.2 [SRAM Allocation](#32-sram-allocation)
   - 3.3 [Linker Script Analysis](#33-linker-script-analysis)

4. [FreeRTOS Configuration](#4-freertos-configuration)
   - 4.1 [Scheduler Configuration](#41-scheduler-configuration)
   - 4.2 [Static Allocation Policy](#42-static-allocation-policy)
   - 4.3 [Task Priority Map](#43-task-priority-map)
   - 4.4 [Synchronisation Primitives](#44-synchronisation-primitives)
   - 4.5 [Runtime Statistics (DWT)](#45-runtime-statistics-dwt)

5. [Task Architecture](#5-task-architecture)
   - 5.1 [Overall Task Graph](#51-overall-task-graph)
   - 5.2 [Thread 1 — Sensor Producer](#52-thread-1--sensor-producer)
   - 5.3 [Thread 2 — TinyML Inference](#53-thread-2--tinyml-inference)
   - 5.4 [Thread 3 — UART TX Serialiser](#54-thread-3--uart-tx-serialiser)
   - 5.5 [Thread 4 — Heartbeat Monitor](#55-thread-4--heartbeat-monitor)
   - 5.6 [Thread 5 — Bootloader RX](#56-thread-5--bootloader-rx)
   - 5.7 [Thread 6 — Temperature Sensor](#57-thread-6--temperature-sensor)
   - 5.8 [Thread 7 — Ultrasonic Sensors](#58-thread-7--ultrasonic-sensors)
   - 5.9 [Logger Task](#59-logger-task)
   - 5.10 [Stack Size Summary](#510-stack-size-summary)

6. [TinyML Inference Pipeline](#6-tinyml-inference-pipeline)
   - 6.1 [End-to-End Pipeline Diagram](#61-end-to-end-pipeline-diagram)
   - 6.2 [Stage 1 — Raw IMU Sampling (Thread 1)](#62-stage-1--raw-imu-sampling-thread-1)
   - 6.3 [Stage 2 — Ring Buffer (SPSC)](#63-stage-2--ring-buffer-spsc)
   - 6.4 [Stage 3 — Scaling to Physical Units](#64-stage-3--scaling-to-physical-units)
   - 6.5 [Stage 4 — Feature Extraction (50 Features)](#65-stage-4--feature-extraction-50-features)
   - 6.6 [Stage 5 — Quantization (int8)](#66-stage-5--quantization-int8)
   - 6.7 [Stage 6 — CubeAI Inference](#67-stage-6--cubeai-inference)
   - 6.8 [Stage 7 — Temporal Majority Voting](#68-stage-7--temporal-majority-voting)
   - 6.9 [Windowing and Stride Logic](#69-windowing-and-stride-logic)
   - 6.10 [WCET Measurement](#610-wcet-measurement)

7. [Model Architecture and Training Pipeline](#7-model-architecture-and-training-pipeline)
   - 7.1 [Model Files](#71-model-files)
   - 7.2 [Network Topology](#72-network-topology)
   - 7.3 [Quantization Parameters](#73-quantization-parameters)
   - 7.4 [Statistical Normalization (Z-Score)](#74-statistical-normalization-z-score)
   - 7.5 [CubeAI Integration](#75-cubeai-integration)
   - 7.6 [Replay Mode for Offline Validation](#76-replay-mode-for-offline-validation)

8. [Communication Protocol (FRAME)](#8-communication-protocol-frame)
   - 8.1 [Wire Format](#81-wire-format)
   - 8.2 [Frame Types Reference](#82-frame-types-reference)
   - 8.3 [Classification Frame Payload](#83-classification-frame-payload)
   - 8.4 [Temperature Frame Payload](#84-temperature-frame-payload)
   - 8.5 [Heartbeat Frame Payload](#85-heartbeat-frame-payload)
   - 8.6 [Ultrasonic Frame Payload](#86-ultrasonic-frame-payload)
   - 8.7 [Log Frame Payload](#87-log-frame-payload)
   - 8.8 [Bootloader Frames (ESP32 → STM32)](#88-bootloader-frames-esp32--stm32)
   - 8.9 [CRC32 Algorithm](#89-crc32-algorithm)
   - 8.10 [GPIO Sync Signal](#810-gpio-sync-signal)

9. [UART Stack](#9-uart-stack)
   - 9.1 [Driver Layer (UART_INTERFACE)](#91-driver-layer-uart_interface)
   - 9.2 [Service Layer (UART_SERVICE)](#92-service-layer-uart_service)
   - 9.3 [DMA TX Path](#93-dma-tx-path)
   - 9.4 [IRQ RX Path](#94-irq-rx-path)
   - 9.5 [TX Done Callback Chain](#95-tx-done-callback-chain)

10. [Ring Buffer (SPSC Lock-Free)](#10-ring-buffer-spsc-lock-free)
    - 10.1 [Design Invariants](#101-design-invariants)
    - 10.2 [Memory Barrier Strategy](#102-memory-barrier-strategy)
    - 10.3 [Capacity and Overflow Policy](#103-capacity-and-overflow-policy)
    - 10.4 [API Reference](#104-api-reference)

11. [IWDG Thread Supervisor](#11-iwdg-thread-supervisor)
    - 11.1 [Architecture](#111-architecture)
    - 11.2 [Thread-Alive Protocol](#112-thread-alive-protocol)
    - 11.3 [Supervisor Feed Logic](#113-supervisor-feed-logic)
    - 11.4 [Configuration and Timeout](#114-configuration-and-timeout)
    - 11.5 [API Reference](#115-api-reference)

12. [OTA Bootloader Entry Mechanism](#12-ota-bootloader-entry-mechanism)
    - 12.1 [Protocol Sequence](#121-protocol-sequence)
    - 12.2 [Flash Erase and Reset Flow](#122-flash-erase-and-reset-flow)
    - 12.3 [Thread 5 State Machine](#123-thread-5-state-machine)

13. [Logging System](#13-logging-system)
    - 13.1 [Architecture](#131-architecture)
    - 13.2 [Log Code Taxonomy](#132-log-code-taxonomy)
    - 13.3 [ISR Safety](#133-isr-safety)
    - 13.4 [API Reference](#134-api-reference)

14. [Peripheral Drivers — API Reference](#14-peripheral-drivers--api-reference)
    - 14.1 [RCC — Reset and Clock Control](#141-rcc--reset-and-clock-control)
    - 14.2 [GPIO Driver](#142-gpio-driver)
    - 14.3 [I2C Driver and Service](#143-i2c-driver-and-service)
    - 14.4 [DMA Driver](#144-dma-driver)
    - 14.5 [TIM Driver (TIM2 and TIM3)](#145-tim-driver-tim2-and-tim3)
    - 14.6 [ADC Driver (LM35)](#146-adc-driver-lm35)
    - 14.7 [FLASH Driver](#147-flash-driver)
    - 14.8 [NVIC Driver](#148-nvic-driver)
    - 14.9 [DWT Driver](#149-dwt-driver)
    - 14.10 [SYSTICK (disabled)](#1410-systick-disabled)

15. [Build System](#15-build-system)
    - 15.1 [PlatformIO Configuration](#151-platformio-configuration)
    - 15.2 [CubeAI Library Linking](#152-cubeai-library-linking)
    - 15.3 [CMSIS-DSP Integration](#153-cmsis-dsp-integration)
    - 15.4 [Build Flags and Optimisation](#154-build-flags-and-optimisation)
    - 15.5 [FPU Configuration (FPv4-SP-D16)](#155-fpu-configuration-fpv4-sp-d16)
    - 15.6 [GPIO Patch](#156-gpio-patch)

16. [Interrupt Priority Map](#16-interrupt-priority-map)

17. [Data Flow and Timing Analysis](#17-data-flow-and-timing-analysis)
    - 17.1 [End-to-End Latency Budget](#171-end-to-end-latency-budget)
    - 17.2 [CPU Utilisation Breakdown](#172-cpu-utilisation-breakdown)
    - 17.3 [Ring Buffer Occupancy Analysis](#173-ring-buffer-occupancy-analysis)

18. [Testing and Validation](#18-testing-and-validation)
    - 18.1 [Test Output Files](#181-test-output-files)
    - 18.2 [Replay Mode Validation](#182-replay-mode-validation)

19. [Known Issues and Design Notes](#19-known-issues-and-design-notes)

20. [Glossary](#20-glossary)

---

## 1. Project Overview

### 1.1 Purpose and Goals

This project implements a **real-time road surface classification system** embedded on an STM32F401CC microcontroller. The system acquires 6-axis IMU (Inertial Measurement Unit) data from an MPU6050 sensor at 100 Hz, processes it through a full TinyML pipeline running on-device, classifies the road surface as either **smooth** or **rough**, and transmits structured telemetry frames over UART to an upstream ESP32 gateway.

Beyond road classification, the system simultaneously monitors ambient temperature (LM35 ADC), two obstacle distances (dual HC-SR04 ultrasonic sensors), streams structured diagnostic heartbeats, and provides a complete UART-triggered OTA bootloader entry mechanism — all running concurrently under FreeRTOS with zero dynamic memory allocation.

**Design Principles:**

- **No dynamic allocation**: every FreeRTOS object (task, queue, semaphore) uses static storage declared at compile time. `configSUPPORT_DYNAMIC_ALLOCATION = 0` is enforced.
- **MISRA-C:2012 compliance** across the ML pipeline and ring buffer modules.
- **Custom HAL**: no STM32 HAL library dependency. All peripheral drivers are register-level, built on top of CMSIS only.
- **Safety-first watchdog**: an IWDG supervisor withholds the hardware watchdog feed if any monitored thread fails to check in.

### 1.2 Key Capabilities

| Capability | Detail |
|---|---|
| Road classification | Smooth / Rough, 4 Hz output rate |
| IMU sampling | MPU6050 at 100 Hz via I2C + DMA |
| Temperature monitoring | LM35 on ADC1 CH1 (PA1), every 2 s |
| Distance measurement | Dual HC-SR04 via TIM3 input capture, every 250 ms |
| UART telemetry | 115200 baud, DMA TX, IRQ RX, CRC32-protected frames |
| Heartbeat | CPU usage, stack HWM, inference WCET — every 5 s |
| Logging | Structured event log (code + severity + aux + timestamp) |
| OTA entry | UART command (0xAA 0xEB) → erase + reset into bootloader |
| Watchdog | IWDG 3000 ms with per-thread alive-flag supervision |
| Replay mode | Offline CSV injection for pipeline validation |

### 1.3 Architecture Summary

The system is structured around a **7-stage data pipeline** driven by a 100 Hz hardware timer, with each stage isolated in its own FreeRTOS task and connected via lock-free ring buffers or lightweight FreeRTOS queues:

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │                    HARDWARE INTERRUPT LAYER                         │
  │  TIM2 (100Hz)   TIM3 IC (HC-SR04)   UART1 RX IRQ   DMA ISRs       │
  └────────┬─────────────────┬───────────────┬───────────────┬──────────┘
           │                 │               │               │
  ┌────────▼──────┐  ┌───────▼──────┐  ┌────▼────┐  ┌──────▼──────────┐
  │  Thread 1     │  │  Thread 7    │  │Thread 5 │  │  Thread 3       │
  │  Sensor       │  │  Ultrasonic  │  │ BL RX   │  │  UART TX (DMA)  │
  │  Producer     │  │  (250ms)     │  │         │  │                 │
  └────────┬──────┘  └───────┬──────┘  └─────────┘  └──────▲──────────┘
           │                 │                               │
           │ RingBuffer_Push │ g_frame_queue                 │
           │                 └───────────────────────────────┘
  ┌────────▼──────┐                                          │
  │  Thread 2     │  ─────────────── g_frame_queue ──────────┘
  │  TinyML       │
  │  Inference    │
  └───────────────┘
  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
  │  Thread 4    │  │  Thread 6    │  │  Logger Task │
  │  Heartbeat   │  │  LM35 Temp  │  │  (priority 0) │
  │  (5s window) │  │  (2s)        │  │              │
  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘
         │                 │                  │
         └─────────────────┴──────────────────┘
                     g_frame_queue
```

---

## 2. Hardware Platform

### 2.1 Microcontroller Specifications

| Feature | Value |
|---|---|
| Part number | STM32F401CCU6 |
| Core | ARM Cortex-M4F |
| Clock speed (configured) | 84 MHz |
| Flash | 256 KB (6 sectors) |
| SRAM | 64 KB |
| FPU | FPv4-SP-D16 (single-precision) |
| Package | UFQFPN48 |
| Operating voltage | 3.3 V (VDDA used for ADC reference) |

The Cortex-M4F core has a single-precision hardware FPU (`-mfpu=fpv4-sp-d16 -mfloat-abi=hard`) which is critical for the feature extraction pipeline — all 50 statistical features are computed in float32.

### 2.2 Clock Configuration

The system runs at 84 MHz via the internal HSI oscillator (16 MHz) through the PLL. This is configured by `RCC_INIT_84MHz_HSI()`.

```
HSI (16 MHz)
    │
    ▼  PLL_M = 8, PLL_N = 168, PLL_P = 4
   PLL ──────────────────────────────────► SYSCLK = 84 MHz
    │
    ├─── AHB  /1  ──► HCLK    = 84 MHz  (CPU, DMA, DWT)
    ├─── APB1 /2  ──► PCLK1   = 42 MHz  (I2C1, TIM2, TIM3, TIM4, TIM5)
    └─── APB2 /1  ──► PCLK2   = 84 MHz  (USART1, ADC1)

TIM2 clock = 2 × PCLK1 = 84 MHz  (APB1 prescaler != 1)
TIM3 clock = 2 × PCLK1 = 84 MHz
ADC clock  = PCLK2 / 4 = 21 MHz  (ADC_CCR.ADCPRE = /4)
LSI        = ~32 kHz               (IWDG source)
```

**Timer prescalers for 100 Hz TIM2:**

The TIM2 prescaler and ARR values are chosen to produce exactly 100 Hz interrupt rate from the 84 MHz timer clock:

```
TIM2_PSC × TIM2_ARR = 84,000,000 / 100 = 840,000
```

Both `TIM_PSC_100HZ` and `TIM_ARR_100HZ` are defined in `TIM_INTERFACE.h`.

**TIM3 prescaler for HC-SR04 (1 µs resolution):**

```
Prescaler = 83   →   Timer clock = 84 MHz / (83 + 1) = 1 MHz = 1 tick per µs
ARR = 65535       →   Max measurable pulse = 65.535 ms ≈ 11.3 m distance
```

### 2.3 Pin Assignment Table

| Pin | Peripheral | Function | Direction |
|---|---|---|---|
| PA1 | ADC1 CH1 | LM35 analog temperature input | Input (analog) |
| PA4 | GPIO Output | HC-SR04 Sensor 1 Trigger | Output PP |
| PA5 | GPIO Output | HC-SR04 Sensor 2 Trigger | Output PP |
| PA6 | TIM3 CH1 | HC-SR04 Sensor 1 Echo (Input Capture) | AF2 |
| PA7 | TIM3 CH2 | HC-SR04 Sensor 2 Echo (Input Capture) | AF2 |
| PA8 | GPIO Output | UART TX frame sync signal | Output PP (fast) |
| PA9 | USART1 TX | UART TX (DMA, 115200 baud) | AF7 PP |
| PA10 | USART1 RX | UART RX (IRQ, 115200 baud) | AF7 (PU) |
| PB6 | I2C1 SCL | MPU6050 clock line | AF4 OD (PU) |
| PB7 | I2C1 SDA | MPU6050 data line | AF4 OD (PU) |
| PC13 | GPIO Output | Activity LED (toggled by Thread 1/4) | Output PP (slow) |

### 2.4 Peripheral Overview

```
STM32F401CC Peripheral Block
═══════════════════════════════════════════════════════════════

  AHB1 Bus:
  ┌─────────────────────────────────────────────────────────┐
  │  DMA1  ─── Stream 0, Ch 1  ──────────► I2C1 RX         │
  │  DMA2  ─── Stream 7, Ch 4  ──────────► USART1 TX       │
  └─────────────────────────────────────────────────────────┘

  APB1 Bus:
  ┌─────────────────────────────────────────────────────────┐
  │  TIM2  (UP, 100 Hz)  ─────────────────► Thread 1 sem   │
  │  TIM3  (IC CH1/CH2)  ─────────────────► HC-SR04 capture│
  │  I2C1  (Fast, DMA)   ─────────────────► MPU6050        │
  └─────────────────────────────────────────────────────────┘

  APB2 Bus:
  ┌─────────────────────────────────────────────────────────┐
  │  USART1 (115200, DMA TX / IRQ RX)                       │
  │  ADC1   (12-bit, CH1, polling)  ───────► LM35          │
  └─────────────────────────────────────────────────────────┘
```

### 2.5 MPU6050 IMU Sensor

The MPU6050 is a 6-axis MEMS IMU (3-axis accelerometer + 3-axis gyroscope) connected on I2C1 (Fast mode, 400 kHz) with DMA-driven burst reads.

**Register Configuration:**

| Register | Address | Value | Meaning |
|---|---|---|---|
| SMPLRT_DIV | 0x19 | 9 | Sample rate = 1000 / (1 + 9) = 100 Hz |
| CONFIG | 0x1A | DLPF enabled | Low-pass filter to remove >50 Hz noise |
| GYRO_CONFIG | 0x1B | ±500 °/s range | Scale factor = 65.5 LSB/°/s |
| ACCEL_CONFIG | 0x1C | ±4g range | Scale factor = 8192 LSB/g |
| PWR_MGMT_1 | 0x6B | 0x00 | Wake from sleep, internal oscillator |

**Burst Read Protocol:**

Each read transfers 14 bytes starting at register `0x3B` (`ACCEL_XOUT_H`):

```
Byte offset:  0  1  2  3  4  5  6  7  8  9  10 11 12 13
              ├──ACCEL_X──┤  ├──ACCEL_Y──┤  ├──ACCEL_Z──┤  ├──TEMP─────┤  ├──GYRO_X───┤  ├──GYRO_Y───┤  ├──GYRO_Z───┤
               Hi   Lo      Hi   Lo      Hi   Lo      Hi   Lo      Hi   Lo      Hi   Lo      Hi   Lo
```

All values are big-endian on the wire; the driver assembles them into `sint16_t` fields inside `MPU6050_RawData_t`.

**I2C Address:** `0x68` (AD0 pin = GND). WHO_AM_I register (`0x75`) must read `0x68` on init.

**DMA Read Flow:**

```
Thread1 calls MPU6050_TriggerRead(cb, ctx)
        │
        ▼
  I2C_SVC_ReadDMA(0x68, reg=0x3B, buf=s_dma_buf, len=14)
        │
        ▼
  DMA1 Stream0 pulls 14 bytes from I2C1 DR into static buffer
        │
        ▼  (DMA TC interrupt fires)
  on_mpu_read_done() ISR callback
        │
        ├─ parse raw bytes → MPU6050_RawData_t
        └─ xSemaphoreGiveFromISR(s_dma_sem) → wakes Thread1
```

### 2.6 LM35 Temperature Sensor

The LM35 is a precision analog temperature sensor with a linear output of `10 mV/°C`. It is connected to PA1, read by ADC1 CH1.

**Conversion Formula:**

```
ADC raw (12-bit):   0 … 4095
V_out (mV)       = (raw × 3300) / 4095
Temperature (°C) = V_out_mV / 10
Temperature_x10  = V_out_mV    (integer, e.g. 235 = 23.5°C)
```

**ADC Configuration:**

| Parameter | Value | Reason |
|---|---|---|
| Resolution | 12-bit | Maximum precision |
| Sample time | 480 cycles | LM35 has ~10 kΩ output impedance — needs slow sampling |
| Mode | Single conversion, polling | Low data rate (0.5 Hz) makes polling acceptable |
| Clock | 21 MHz (APB2/4) | Within 36 MHz ADC max clock |

### 2.7 HC-SR04 Ultrasonic Sensors

Two HC-SR04 sensors provide obstacle/surface distance measurements. The sensors use the following protocol:

```
Trigger pulse:
  ┌──┐
  │10│  (10 µs HIGH pulse from GPIO)
──┘  └──────────────────────────────

Echo response (time proportional to distance):
           ┌──────────────────┐
           │    Echo HIGH     │
──────────┘                   └─────
           ↑                  ↑
     Rising edge          Falling edge
     (TIM3 IC capture)    (TIM3 IC capture)

Distance (cm) = (falling_time - rising_time) / 58
```

**TIM3 Input Capture Configuration:**

Each sensor echo pin is mapped to a TIM3 input capture channel:

| Sensor | GPIO | TIM3 Channel | Polarity Switch |
|---|---|---|---|
| Sensor 1 | PA6 | CH1 | Rising → Falling → Rising |
| Sensor 2 | PA7 | CH2 | Rising → Falling → Rising |

The `on_hcsr04_capture()` ISR checks the `CCER.CCxP` bit to know if it received a rising or falling edge, toggles the polarity, and on the falling edge computes the pulse width and sends the result to `g_ultrasonic_queue`.

**32-bit timestamp rollover protection:**

```c
if (capture_val2[id] >= capture_val1[id])
    diff = capture_val2[id] - capture_val1[id];
else
    diff = (0xFFFFU - capture_val1[id]) + capture_val2[id] + 1U;
```

---

## 3. Memory Architecture

### 3.1 Flash Layout (Bootloader + Application)

The STM32F401CC has 256 KB of Flash divided into 6 sectors of varying sizes. This project is designed to run **as a second-stage application** with an external bootloader occupying Sectors 0–1:

```
Flash Memory Map (256 KB total):
┌──────────────────────────────────────────────────────────────┐
│  Address       │  Sector  │  Size   │  Contents             │
├──────────────────────────────────────────────────────────────┤
│  0x08000000    │    0     │  16 KB  │  Bootloader (Part 1)  │
│  0x08004000    │    1     │  16 KB  │  Bootloader (Part 2)  │
│  ──────────────┼──────────┼─────────┼───────────────────────│
│  0x08008000    │    2     │  16 KB  │  Application  ◄─ ENTRY│
│  0x0800C000    │    3     │  16 KB  │  Application          │
│  0x08010000    │    4     │  64 KB  │  Application          │
│  0x08020000    │    5     │ 128 KB  │  Application          │
│                │          │ ──────  │                       │
│                │          │ 224 KB  │  Total app Flash      │
└──────────────────────────────────────────────────────────────┘
```

The vector table is relocated to `0x08008000` via `SCB->VTOR` (line exists in `main.c` but is commented out for current testing). When running as an application after OTA, this line must be enabled.

**Bootloader Entry Trigger (Thread 5):**

When Thread 5 receives the `0xAA 0xEB` command sequence:
1. Sends ACK `0xEE 0xAA` back via DMA
2. Erases **Sector 1** (wipes the bootloader's second part to signal it should receive a new image)
3. Triggers a system reset via `SCB->AIRCR` — the bootloader (in Sector 0) takes over and awaits the new image

### 3.2 SRAM Allocation

The STM32F401CC has 64 KB of SRAM at `0x20000000–0x2000FFFF`. With static-only FreeRTOS allocation, every byte is accounted for at link time:

```
SRAM Allocation (64 KB):
┌──────────────────────────────────────────────────────────────┐
│  Region                    │  Size (approx)  │  Notes        │
├──────────────────────────────────────────────────────────────┤
│  .bss / .data              │  ~20 KB         │  Globals, ZI  │
│  Ring Buffer (IMU)         │   1.8 KB        │  128 × 14 B   │
│  FreeRTOS kernel           │  ~2 KB          │  Scheduler    │
│  Thread 1 stack            │    384 B        │   96 words    │
│  Thread 2 stack            │   5.12 KB       │  1280 words   │
│  Thread 3 stack            │    512 B        │  128 words    │
│  Thread 4 stack            │   1.024 KB      │  256 words    │
│  Thread 5 stack            │   1.024 KB      │  256 words    │
│  Thread 6 stack            │    768 B        │  192 words    │
│  Thread 7 stack            │    512 B        │  128 words    │
│  Logger stack              │   1.024 KB      │  256 words    │
│  Idle task stack           │    512 B        │  128 words    │
│  Frame queue buffer        │    210 B        │  6 × 35 B     │
│  Ultrasonic queue buffer   │     16 B        │  4 × uint32   │
│  Logger queue buffer       │     80 B        │  8 × 10 B     │
│  ML inference arena        │  ~20 KB         │  CubeAI acts  │
│  UART svc TX/RX ring       │   1.536 KB      │  256×2×3 inst │
│  ISR + main stack          │  ~2 KB          │  Linker min   │
│  Heap                      │   1 KB          │  _Min_Heap    │
└──────────────────────────────────────────────────────────────┘

  Largest single consumer: Thread 2 TinyML stack (5.12 KB)
  Second largest: CubeAI activations arena (~20 KB, in .bss)
```

### 3.3 Linker Script Analysis

The custom linker script `STM32F401CCFX_FLASH_Sector2.ld` defines:

```
MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 64K
  FLASH (rx)  : ORIGIN = 0x08008000, LENGTH = 224K
}
```

Critical sections:
- `.isr_vector` placed first in FLASH — ensures the vector table is at `0x08008000` (the VTOR value)
- `.text` follows immediately — code
- `.rodata` — model weights, string literals, const arrays
- `.data` — initialised globals (copied from FLASH to RAM at boot by `Reset_Handler`)
- `.bss` — zero-initialised BSS (CubeAI activations arena lives here, 8-byte aligned)

---

## 4. FreeRTOS Configuration

### 4.1 Scheduler Configuration

The project uses a preemptive priority-based FreeRTOS scheduler with the following key parameters:

| Parameter | Value | Reason |
|---|---|---|
| `configCPU_CLOCK_HZ` | 84,000,000 | Cortex-M4 at 84 MHz |
| `configTICK_RATE_HZ` | 1000 | 1 ms tick granularity |
| `configMAX_PRIORITIES` | 5 | 0 (idle) to 4 (max used by Thread 5) |
| `configUSE_PREEMPTION` | 1 | Preemptive scheduling |
| `configUSE_TIME_SLICING` | 0 | All tasks have distinct priorities — no round-robin needed |
| `configUSE_PORT_OPTIMISED_TASK_SELECTION` | 1 | Uses CLZ instruction (hardware-accelerated priority lookup) |
| `configUSE_TICKLESS_IDLE` | 0 | Watchdog supervisor requires regular idle hook calls |

### 4.2 Static Allocation Policy

```c
#define configSUPPORT_STATIC_ALLOCATION   1
#define configSUPPORT_DYNAMIC_ALLOCATION  0
/* No configTOTAL_HEAP_SIZE — heap does not exist */
```

Because `SUPPORT_DYNAMIC_ALLOCATION = 0`, the FreeRTOS heap manager is excluded from the build entirely. Every `xTaskCreate`, `xQueueCreate`, and `xSemaphoreCreate` call **must** use the `...Static` variants. The linker script allocates 1 KB for the C library heap (`_Min_Heap_Size`), which is used only for `printf`-style formatting if called (not used in this project).

`vApplicationGetIdleTaskMemory()` is implemented in `main.c` to provide static storage for the idle task — this is mandatory when static allocation is used.

### 4.3 Task Priority Map

```
Priority 4 (highest used):  Thread 5 — Bootloader RX
                            Must preempt everything to catch OTA trigger

Priority 3:                 Thread 1 — Sensor Producer
                            Must respond to 100 Hz TIM2 sem in <10 ms

Priority 2:                 Thread 2 — TinyML Inference
                            High — must drain ring buffer before overflow

Priority 1:                 Thread 3 — UART TX
                            Thread 4 — Heartbeat
                            Thread 6 — Temperature
                            Thread 7 — Ultrasonic

Priority 0 (lowest):        Logger Task
                            Idle Task
                            (IWDG supervisor runs in idle hook)
```

**Why Thread 5 is highest priority:**

The bootloader entry command could arrive at any time. If Thread 5 is delayed and the host retransmits, the duplicated byte sequence could be missed or partially consumed. Running at priority 4 ensures Thread 5 processes the RX ring buffer before any other task resumes.

### 4.4 Synchronisation Primitives

| Primitive | Instance | Producer | Consumer | Purpose |
|---|---|---|---|---|
| Binary Semaphore | `s_tim_sem` | TIM2 ISR | Thread 1 | 100 Hz tick delivery |
| Binary Semaphore | `s_dma_sem` | DMA ISR (MPU read done) | Thread 1 | I2C DMA completion |
| Task Notification | Thread 2 | Thread 1 (`xTaskNotifyGive`) | Thread 2 (`ulTaskNotifyTake`) | "New sample in ring buffer" |
| Task Notification | Thread 3 | DMA TC ISR (`vTaskNotifyGiveFromISR`) | Thread 3 (`ulTaskNotifyTake`) | "UART DMA TX complete" |
| Task Notification | Thread 5 | UART RX ISR | Thread 5 | "Byte received" |
| Queue | `g_frame_queue` (depth 6) | T2, T4, T6, T7, Logger | Thread 3 | Frame transmission pipeline |
| Queue | `g_ultrasonic_queue` (depth 4) | TIM3 IC ISR | Thread 7 | HC-SR04 distances from ISR |
| Queue | `s_logger_queue` (depth 8) | Any task/ISR | Logger task | Structured log entries |

All semaphores use **static allocation** via `xSemaphoreCreateBinaryStatic`.

### 4.5 Runtime Statistics (DWT)

FreeRTOS runtime statistics are enabled via:
```c
#define configGENERATE_RUN_TIME_STATS    1
#define configUSE_TRACE_FACILITY         1
```

The run-time counter is driven by the **DWT (Data Watchpoint and Trace) cycle counter**:

```c
void vConfigureTimerForRunTimeStats(void) {
    DWT_Init();   // enables DWT_CYCCNT
}
uint32_t ulGetRunTimeCounterValue(void) {
    return DWT_GetCycles();   // reads DWT_CYCCNT register
}
```

This gives a **84 MHz resolution** counter for per-task CPU accounting. Thread 4 computes per-task CPU percentages as:

```
cpu_t_x100 = (delta_task_cycles × 10000) / delta_total_cycles
```

The x100 multiplier allows 2 decimal places (e.g. 1234 = 12.34%) without floating point on the wire.

---

## 5. Task Architecture

### 5.1 Overall Task Graph

```
                   ┌────────────────────────────────────────────┐
                   │            HARDWARE EVENTS                 │
                   │  TIM2_IRQ  DMA_ISR  UART_IRQ  TIM3_IC_ISR │
                   └─────┬────────┬────────┬─────────┬──────────┘
                         │        │        │         │
              Semaphore  │  Semaphore      │   Queue │
              (tim_sem)  │  (dma_sem)      │  (ultra)│ Notification
                         │        │        │         │   (byte rx)
     ┌───────────────────▼────────▼──┐    │  ┌──────▼──────────────┐
     │       Thread 1 (Prio 3)       │    │  │   Thread 5 (Prio 4) │
     │       "Sensor"                │    │  │   "BL_RX"           │
     │  - Takes tim_sem (100Hz)      │    │  │  - State machine    │
     │  - Triggers DMA read          │    │  │  - ACK+Erase+Reset  │
     │  - Waits dma_sem (5ms TO)     │    │  └─────────────────────┘
     │  - Copies MPU6050 sample      │    │
     │  - RingBuffer_Push()          │    │  ┌──────────────────────┐
     │  - xTaskNotifyGive(T2)        │    │  │   Thread 7 (Prio 1) │
     │  - IWDG alive flag            │    └──►  "ULTRA"            │
     └───────────────────────┬───────┘       │  - Trigger sensors  │
                             │                │  - Wait queue (50ms)│
                     (ring   │                │  - Pack FRAME       │
                      buffer)│                └──────┬──────────────┘
                             │                       │
     ┌───────────────────────▼───────┐               │
     │       Thread 2 (Prio 2)       │               │
     │       "ML"                    │   ┌───────────▼──────────┐
     │  - ulTaskNotifyTake           │   │  Thread 6 (Prio 1)  │
     │  - PeekWindow(50 samples)     │   │  "TEMP"              │
     │  - Scale → Features →         │   │  - Delay 2000ms     │
     │    Quantize → Inference       │   │  - ADC read LM35    │
     │  - Vote_Push / Vote_Decide    │   │  - Pack FRAME        │
     │  - Pack FrameRequest          │   └───────────┬──────────┘
     │  - xQueueSend(frame_queue)    │               │
     │  - IWDG alive flag            │               │
     └───────────────────────────────┘               │
                                                      │
     ┌─────────────────────────────────────────────◄──┘
     │               g_frame_queue  (depth 6)
     │
     │  Thread 4 (Prio 1) "HB"          Logger Task (Prio 0)
     │  ─────────────────────────        ─────────────────────
     │  Every 1 s: gather stats          Drains s_logger_queue
     │  Every 5 s: push heartbeat frame  Packs LOG frames
     │                                   xQueueSend(frame_queue)
     │
     ▼
     ┌──────────────────────────────────┐
     │     Thread 3 (Prio 1) "TX"       │
     │  - xQueueReceive(frame_queue)    │
     │  - Frame_Build() + CRC32         │
     │  - UART_SVC_TransmitDMA()        │
     │  - GPIO PA8 HIGH (sync pulse)    │
     │  - ulTaskNotifyTake (DMA done)   │
     │  - GPIO PA8 LOW                  │
     │  - IWDG alive flag               │
     └──────────────────────────────────┘
```

### 5.2 Thread 1 — Sensor Producer

**Name:** `"Sensor"` | **Priority:** 3 | **Stack:** 96 words (384 bytes)

Thread 1 is the **clock master** of the pipeline. It drives the entire inference chain at exactly 100 Hz.

**Live Mode (`REPLAY_MODE = 0`) Flow:**

```
BLOCK on s_tim_sem (binary sem from TIM2 ISR)
    │
    ▼
MPU6050_TriggerRead(on_mpu_read_done, NULL)
    │  starts non-blocking I2C+DMA burst read
    ▼
BLOCK on s_dma_sem (max 5 ms timeout)
    │
    ├── timeout? → LOG_WARN(MPU6050_TIMEOUT) → next tick
    │
    ▼
taskENTER_CRITICAL()
  snapshot = s_latest_sample   (copy from ISR-written volatile)
taskEXIT_CRITICAL()
    │
    ▼
RingBuffer_Push(&snapshot)
    │
    ├── RING_BUFFER_FULL? → LOG_ERROR(RING_BUFFER_DROP)
    │
    ▼
xTaskNotifyGive(s_thread2_handle)
    │
    ▼
IWDG_Thread_SetAlive(&IWDG_Thread1_Alive)
GPIO_TogglePin(PC13)   ← LED blinks at 50 Hz
```

**Replay Mode (`REPLAY_MODE = 1`) Flow:**

```
BLOCK on s_tim_sem
    │
    ▼
if idx >= REPLAY_NUM_SAMPLES → reset idx, set g_replay_done
    │
    ▼
sample = replay_samples[idx++]
    │
    ▼
RingBuffer_Push(&sample)
xTaskNotifyGive(Thread2)
IWDG_Thread_SetAlive()
```

The replay samples are pre-recorded IMU readings stored in `replay_data.h` as a C array. This allows offline validation of the entire pipeline without physical hardware. See [Section 7.6](#76-replay-mode-for-offline-validation).

**Critical section rationale:** The `s_latest_sample` is a global `volatile MPU6050_RawData_t` written by the DMA ISR callback (`on_mpu_read_done`). Since it's a 14-byte struct (not atomic), Thread 1 uses a critical section to take a consistent snapshot before pushing to the ring buffer. In replay mode, there is no ISR — the sample is read directly from the const array, so no critical section is needed.

### 5.3 Thread 2 — TinyML Inference

**Name:** `"ML"` | **Priority:** 2 | **Stack:** 1280 words (5120 bytes)

Thread 2 is the most compute-intensive task. Its stack is the largest in the system (5120 bytes) due to the multiple float32 working arrays required by the feature extraction engine.

**Static working buffers (in Task stack):**

```c
MPU6050_RawData_t window[WINDOW_SIZE];            // 50 × 14 = 700 bytes
static int16_t   flat_window[WINDOW_SIZE][N_FEATURES]; // 50 × 6 × 2 = 600 bytes
static float32_t scaled     [WINDOW_SIZE][N_FEATURES]; // 50 × 6 × 4 = 1200 bytes
static float32_t features   [N_STAT_FEATURES];         // 50 × 4    = 200 bytes
static int8_t    ts_q       [WINDOW_SIZE * N_FEATURES]; // 50 × 6  = 300 bytes
static int8_t    stat_q     [N_STAT_FEATURES];          // 50 × 1  = 50 bytes
```

Note: `static` in the function body means these buffers are in `.bss`, not on the stack. This is important — without `static`, the 1200-byte `scaled` array alone would exhaust Thread 2's stack.

**Processing Loop:**

```
BLOCK on task notification (from Thread 1)
    │
    ▼
g_t2_wake_count++

WHILE RingBuffer_Count() >= WINDOW_SIZE:
    │
    ├── RingBuffer_PeekWindow(window, 50)
    │
    ├── Flatten to flat_window[50][6]   (int16_t)
    │
    ├── Scale_RawWindow() → scaled[50][6]  (float32)
    │        accel / 8192.0f, gyro / 65.5f
    │
    ├── Features_Extract(scaled, features)  → 50 float32
    │
    ├── Quantize_TS(scaled, ts_q)           → 300 int8
    │
    ├── Quantize_NormalizeStat(features)    → in-place z-score
    │
    ├── Quantize_Stat(features, stat_q)     → 50 int8
    │
    ├── DWT_GetCycles() → t0
    ├── Inference_Run(ts_q, stat_q, &result)
    ├── us = (DWT_GetCycles() - t0) / 84   ← WCET measurement
    │
    ├── g_t2_inference_count++
    │
    ├── Vote_Push(&vote, result.label)
    │
    ├── if Vote_Ready(&vote):
    │       result.label = Vote_Decide(&vote)
    │       build FrameRequest_t (CLASSIFICATION)
    │       xQueueSend(g_frame_queue, &req, 0) → non-blocking
    │
    └── RingBuffer_Advance(WINDOW_STRIDE)   ← advance by 25
                                             (50% overlap)
│
IWDG_Thread_SetAlive(&IWDG_Thread2_Alive)
```

### 5.4 Thread 3 — UART TX Serialiser

**Name:** `"TX"` | **Priority:** 1 | **Stack:** 128 words (512 bytes)

Thread 3 is the **single writer** to the UART. All frame transmission is serialised through this task and the `g_frame_queue`. This eliminates any risk of interleaved transmission from multiple producers.

```
BLOCK on g_frame_queue (portMAX_DELAY)
    │
    ▼
Frame_Build(req.type, req.ecu_id, req.payload, req.length,
            s_tx_frame, sizeof(s_tx_frame), &frame_len)
    │
    ├── FRAME_OK? proceed
    └── error? LOG_ERROR(FRAME_BUILD_FAIL)

GPIO_WritePin(PA8, HIGH)   ← sync pulse starts

UART_SVC_TransmitDMA(UART1_ID, &buf)   ← fire-and-forget DMA

ulTaskNotifyTake(pdTRUE, portMAX_DELAY)   ← wait for DMA TC ISR
    │
    ▼
GPIO_WritePin(PA8, LOW)   ← sync pulse ends

IWDG_Thread_SetAlive(&IWDG_Thread3_Alive)
```

The PA8 GPIO sync pulse allows the receiving ESP32 to determine the exact start of a frame even without a software SYNC byte in the outbound frame format.

**TX Frame buffer:** `s_tx_frame[FRAME_OVERHEAD_BYTES + FRAME_MAX_PAYLOAD]` = 7 + 248 = 255 bytes. This is a static global, not on the stack. The DMA reads directly from this buffer during transmission.

### 5.5 Thread 4 — Heartbeat Monitor

**Name:** `"HB"` | **Priority:** 1 | **Stack:** 256 words (1024 bytes)

Thread 4 wakes every 1 second, collects per-task runtime counters from `vTaskGetInfo()`, and accumulates a 5-second rolling average for CPU utilisation. Every 5 seconds it pushes a `FRAME_TYPE_HEARTBEAT` frame to `g_frame_queue`.

**CPU % calculation (per 1-second slice):**

```c
cpu_t_x100 = (uint16_t)(
    ((uint64_t)(t_now - t_prev) * 10000ULL) / (total_now - total_prev)
);
```

`uint64_t` is used for the multiplication to prevent overflow. The `x100` suffix means the value represents `percent × 100` (e.g. `1234` = `12.34%`).

**5-second average:**

Each of the 5 per-second measurements is accumulated in `acc_tN_x100`. On the 5th tick:
- Divide by 5 to get the average
- Build the heartbeat frame
- Reset accumulators and `tick_count`

Additionally, the task tracks peak values over the 5-second window:
- `peak_wcet_us` — max inference latency observed in Thread 2
- `peak_rb_fill` — maximum ring buffer occupancy (saturation indicator)

### 5.6 Thread 5 — Bootloader RX

**Name:** `"BL_RX"` | **Priority:** 4 (highest) | **Stack:** 256 words (1024 bytes)

Thread 5 monitors the UART receive stream for the OTA bootloader entry command. It registers itself with the UART service layer so the RX ISR delivers a task notification on every received byte.

**State Machine:**

```
State 0: Waiting for 0xAA
    │
    ├── receive 0xAA → go to State 1
    └── receive other → stay in State 0

State 1: Waiting for 0xEB
    │
    ├── receive 0xEB → BOOTLOADER ENTRY
    │       1. Send ACK: {0xEE, 0xAA}
    │       2. vTaskDelay(2ms) — let last byte shift out
    │       3. FLASH_Unlock()
    │       4. FLASH_EraseSector(SECTOR_1)
    │       5. FLASH_Lock()
    │       6. SCB->AIRCR = (0x05FA << 16) | (1 << 2)  ← SYSRESETREQ
    │       7. for(;;) {}  ← unreachable
    │
    ├── receive 0xAA → stay in State 1 (new first byte)
    └── receive other → go to State 0
```

The 2 ms delay after ACK is critical: if the MCU resets before the ACK frame has fully shifted out of the UART TX shift register, the ESP32 will not see the confirmation.

### 5.7 Thread 6 — Temperature Sensor

**Name:** `"TEMP"` | **Priority:** 1 | **Stack:** 192 words (768 bytes)

Thread 6 reads the LM35 every 2 seconds using ADC1 polling and publishes a `FRAME_TYPE_TEMPERATURE` frame.

```
vTaskDelayUntil(&prev_wake, 2000ms)
    │
    ▼
ADC_Read(&adc_raw)   ← blocking single-conversion (< 100 µs)
    │
    ▼
temp_x10 = ADC_LM35_ToTenthsCelsius(adc_raw, 3300U)
    │
    ▼
Pack FrameRequest_t:
  payload[0..1] = temp_x10 (uint16 BE)
  payload[2..5] = timestamp_ms (uint32 BE)
    │
    ▼
xQueueSend(g_frame_queue, &req, 0)
```

### 5.8 Thread 7 — Ultrasonic Sensors

**Name:** `"ULTRA"` | **Priority:** 1 | **Stack:** 128 words (512 bytes)

Thread 7 fires both HC-SR04 sensors sequentially every 250 ms and packs both distances into a single `FRAME_TYPE_ULTRASONIC` frame.

```
vTaskDelayUntil(&prev_wake, 250ms)
    │
    ▼
xQueueReset(g_ultrasonic_queue)
HCSR04_Trigger(&u1)   ← 10µs pulse on PA4
    │
    ▼
xQueueReceive(g_ultrasonic_queue, &msg1, 50ms timeout)
  msg1 = (sensor_id << 16) | distance_cm
  dist1 = msg1 & 0xFFFF
    │
    ▼
xQueueReset(g_ultrasonic_queue)
HCSR04_Trigger(&u2)   ← 10µs pulse on PA5
    │
    ▼
xQueueReceive(g_ultrasonic_queue, &msg2, 50ms timeout)
  dist2 = msg2 & 0xFFFF
    │
    ▼
Pack FrameRequest_t:
  payload[0..1] = dist1 (uint16 BE, 0xFFFF if timeout)
  payload[2..3] = dist2 (uint16 BE, 0xFFFF if timeout)
  payload[4..7] = timestamp_ms (uint32 BE)
    │
    ▼
xQueueSend(g_frame_queue, &req, 0)
```

If either sensor times out (e.g. no echo within 50 ms — target beyond ~8.5 m), the distance is set to `0xFFFF` as a sentinel.

### 5.9 Logger Task

**Name:** `"Logger"` | **Priority:** 0 | **Stack:** 256 words (1024 bytes)

The logger task runs at the lowest possible priority so that log emission never preempts any real-time task. It drains `s_logger_queue` and forwards each entry as a `FRAME_TYPE_LOG` frame.

The `Logger_Log()` function is **ISR-safe**: it uses `xPortIsInsideInterrupt()` to detect calling context and chooses `xQueueSendFromISR` or `xQueueSend` accordingly.

### 5.10 Stack Size Summary

| Task | Priority | Stack Words | Stack Bytes | Notes |
|---|---|---|---|---|
| Thread 1 Sensor | 3 | 96 | 384 | Minimal — just sampling |
| Thread 2 TinyML | 2 | 1280 | 5120 | Large — CMSIS-DSP FFT |
| Thread 3 UART TX | 1 | 128 | 512 | |
| Thread 4 Heartbeat | 1 | 256 | 1024 | `TaskStatus_t` locals |
| Thread 5 BL RX | 4 | 256 | 1024 | FLASH unlock locals |
| Thread 6 Temperature | 1 | 192 | 768 | |
| Thread 7 Ultrasonic | 1 | 128 | 512 | |
| Logger | 0 | 256 | 1024 | |
| Idle | 0 | 128 | 512 | `configMINIMAL_STACK_SIZE` |
| **Total** | | **2720** | **10,880 B** | ~10.6 KB |

---

## 6. TinyML Inference Pipeline

### 6.1 End-to-End Pipeline Diagram

```
MPU6050 (I2C+DMA, 100Hz)
         │
         │  14-byte burst every 10ms
         ▼
  ┌─────────────────────┐
  │   RING BUFFER       │  128 samples × 14 bytes = 1,792 B
  │   (SPSC lock-free)  │  Head (ISR) / Tail (Thread2)
  └──────────┬──────────┘
             │  PeekWindow (no advance yet)
             ▼
  ┌─────────────────────┐
  │  Scale_RawWindow()  │  int16 → float32
  │  accel / 8192.0f    │  gyro  / 65.5f
  │  [50][6] float32    │  Physical units: g, °/s
  └──────────┬──────────┘
             │
             ▼
  ┌─────────────────────────────────┐
  │  Features_Extract()             │
  │  50 float32 statistical         │
  │  features from the 50×6 window  │
  │                                 │
  │  Per channel (6):               │
  │    std, MAD, P2P, HFE, IQR, RMS │  36 features
  │  Magnitude (accel, gyro):       │
  │    std, MAD, P2P, RMS           │   8 features
  │  Cross-channel:                 │
  │    corr(amag,gmag), corr(az,gz) │   2 features
  │  Skewness (az, gz):             │               │   2 features
  │  Variance ratio (amag, gmag):   │   2 features
  └──────────┬──────────────────────┘
             │  50 float32
             ├─────────────────────────────────────┐
             │                                     │
             ▼                                     ▼
  ┌──────────────────────┐          ┌──────────────────────────┐
  │ Quantize_NormalizeStat│         │  Quantize_TS()           │
  │  in-place z-score     │         │  float32 [50][6]         │
  │  (x - mean) / std     │         │  → int8  [300]           │
  └──────────┬────────────┘         │  scale=0.189672          │
             │                      │  zp=-6                   │
             ▼                      └──────────┬───────────────┘
  ┌──────────────────────┐                     │
  │  Quantize_Stat()     │                     │
  │  float32 → int8      │                     │
  │  scale=0.057328      │                     │
  │  zp=-53              │                     │
  └──────────┬───────────┘                     │
             │ stat_q[50]                       │ ts_q[300]
             │                                  │
             └──────────────┬───────────────────┘
                            │
                            ▼
               ┌────────────────────────────┐
               │  Inference_Run()           │
               │  STM32CubeAI runtime       │
               │                            │
               │  Input[0]: stat_q  50 int8 │
               │  Input[1]: ts_q   300 int8 │
               │  Output[0]: logits  2 int8 │
               │                            │
               │  argmax → label (0 or 1)   │
               │  margin → confidence 0-100 │
               └──────────┬─────────────────┘
                          │ Inference_Result_t
                          │ {label, confidence}
                          ▼
               ┌──────────────────────┐
               │  Vote_Push()         │  Circular buffer of 9
               │  (VOTE_WINDOW = 9)   │  most recent labels
               └──────────┬───────────┘
                          │ if Vote_Ready():
                          ▼
               ┌──────────────────────┐
               │  Vote_Decide()       │  Majority vote (smooth wins ties)
               └──────────┬───────────┘
                          │ smoothed_label
                          ▼
               ┌──────────────────────────────┐
               │  FrameRequest_t packing      │
               │  type=CLASSIFICATION         │
               │  xQueueSend(g_frame_queue)   │
               └──────────────────────────────┘

   RingBuffer_Advance(WINDOW_STRIDE=25)  ← 50% overlap
```

### 6.2 Stage 1 — Raw IMU Sampling (Thread 1)

Each sample is a 7-field struct:

```c
typedef struct {
    sint16_t accel_x;   // raw ADC count, ±32768 at ±4g
    sint16_t accel_y;
    sint16_t accel_z;   // ~8192 at rest (1g)
    sint16_t temp_raw;  // ignored by CNN
    sint16_t gyro_x;    // raw ADC count, ±32768 at ±500°/s
    sint16_t gyro_y;
    sint16_t gyro_z;
} MPU6050_RawData_t;    // 14 bytes
```

### 6.3 Stage 2 — Ring Buffer (SPSC)

Detailed in [Section 10](#10-ring-buffer-spsc-lock-free). Key parameters:
- Capacity: 128 samples (power of 2 for mask-based indexing)
- At 100 Hz: holds 1.28 seconds of data
- No overflow: full → drop (oldest data preserved)

### 6.4 Stage 3 — Scaling to Physical Units

`Scale_RawWindow()` converts the raw `int16_t` window to physical-unit `float32_t`:

```c
scaled[t][0] = (float32_t)raw[t][0] / ACCEL_SCALE; // g  (÷ 8192.0f)
scaled[t][1] = (float32_t)raw[t][1] / ACCEL_SCALE;
scaled[t][2] = (float32_t)raw[t][2] / ACCEL_SCALE;
scaled[t][3] = (float32_t)raw[t][3] / GYRO_SCALE;  // °/s (÷ 65.5f)
scaled[t][4] = (float32_t)raw[t][4] / GYRO_SCALE;
scaled[t][5] = (float32_t)raw[t][5] / GYRO_SCALE;
```

These scale factors correspond to the MPU6050 configuration: `±4g` range and `±500 °/s` range. The training Python pipeline uses identical divisors, ensuring numerical consistency.

### 6.5 Stage 4 — Feature Extraction (50 Features)

`Features_Extract()` computes a 50-dimensional statistical feature vector. The implementation is MISRA-C:2012 compliant and uses CMSIS-DSP for the FFT.

**Feature layout:**

```
Index    Feature           Applied to
────────────────────────────────────────────────────────
[0..5]   std(ch)           ax, ay, az, gx, gy, gz
[6..11]  MAD(ch)           ax, ay, az, gx, gy, gz
[12..17] P2P(ch)           ax, ay, az, gx, gy, gz      (peak-to-peak)
[18..23] HFE(ch)           ax, ay, az, gx, gy, gz      (high-freq energy via FFT)
[24..29] IQR(ch)           ax, ay, az, gx, gy, gz      (inter-quartile range)
[30..35] RMS(ch)           ax, ay, az, gx, gy, gz
[36..39] std/MAD/P2P/RMS   amag = sqrt(ax²+ay²+az²)
[40..43] std/MAD/P2P/RMS   gmag = sqrt(gx²+gy²+gz²)
[44]     corrcoef(amag, gmag)
[45]     corrcoef(az, gz)
[46]     skewness(az)
[47]     skewness(gz)
[48]     var(amag[25:]) / var(amag[:25])
[49]     var(gmag[25:]) / var(gmag[:25])
```

**HFE (High-Frequency Energy) calculation:**

```
For each channel ch:
  1. Extract channel time-series s[0..49]
  2. Zero-pad to FFT_N=64 (copies 50 values, fills 14 zeros)
  3. arm_rfft_fast_f32() → complex spectrum F[0..33]
  4. HFE = sum of |F[k]|² for k in [FFT_HF_SPLIT..32]
             where FFT_HF_SPLIT = 16 (bins 16-32 = high freq)
```

**Stability and NaN handling:**

All features are sanitized before output:
- NaN → 0.0f
- +Inf → +10.0f
- -Inf → -10.0f

This matches `numpy.nan_to_num(nan=0, posinf=10, neginf=-10)` in the Python training pipeline.

**IQR sort:** Uses an insertion sort (O(n²) but small n=50, no malloc, deterministic). The 25th and 75th percentile are then computed via linear interpolation.

**Static module state:**
- `s_fft_inst`: `arm_rfft_fast_instance_f32` — 64-point real FFT instance (initialised once in `Features_Init()`)
- `s_fft_ready`: guard flag to prevent use before init

### 6.6 Stage 5 — Quantization (int8)

Two separate quantization paths exist because the model has two separate inputs:

**Time-series quantization (`Quantize_TS`):**

```
q = clamp(round(f / TS_SCALE) + TS_ZP, -128, +127)
  where TS_SCALE = 0.189672, TS_ZP = -6
```

Output layout: row-major `[WINDOW_SIZE × N_FEATURES]` = `[300]` int8 values.

**Statistical feature quantization (`Quantize_Stat`):**

First, `Quantize_NormalizeStat()` performs in-place z-score normalization:
```
features[i] = (features[i] - stat_mean[i]) / stat_std[i]
```

Where `stat_mean[]` and `stat_std[]` come from `stat_norm.h` / `stat_norm.c` — these are the Python `StandardScaler` parameters from the training pipeline.

Then `Quantize_Stat()`:
```
q = clamp(round(f / STAT_SCALE) + STAT_ZP, -128, +127)
  where STAT_SCALE = 0.057328, STAT_ZP = -53
```

### 6.7 Stage 6 — CubeAI Inference

`Inference_Run()` wraps the STM32CubeAI-generated network runtime:

```c
// Copy inputs into CubeAI-managed tensor buffers
memcpy(inputs[0].data, stat_i8, N_STAT_FEATURES);      // 50 int8
memcpy(inputs[1].data, ts_i8,   WINDOW_SIZE*N_FEATURES); // 300 int8

// Forward pass
ai_i32 batch = ai_network_run(s_network, inputs, outputs);

// Parse output: 2 int8 softmax scores [smooth, rough]
const int8_t *out = (const int8_t *)outputs[0].data;
// label = argmax(out[0], out[1])
// confidence = (margin * 100) / 255,  margin = winner - loser
```

The activations arena is a static `uint8_t s_activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE]` aligned to 8 bytes. The size is generated by CubeAI based on the network topology.

**Confidence metric:** The int8 margin (winner score minus loser score, range 0–255) is linearly mapped to 0–100 without floating point:
```c
confidence = (margin * 100) / 255
```

### 6.8 Stage 7 — Temporal Majority Voting

`Vote_t` maintains a circular buffer of the last `VOTE_WINDOW = 9` classification labels.

**State struct:**
```c
typedef struct {
    uint8_t history[VOTE_WINDOW];  // circular buffer
    uint8_t head;                  // next write position
    uint8_t count;                 // 0..VOTE_WINDOW
} Vote_t;
```

**Tie-breaking:** Lower-indexed class wins. With `N_CLASSES = 2`, a 4–4 tie resolves to SMOOTH (label 0). This is intentional: false positives for "rough" are operationally worse for a road monitor.

**Warm-up period:** For the first 8 inferences, `Vote_Ready()` returns 0 and the raw (unsmoothed) label is used.

### 6.9 Windowing and Stride Logic

```
Ring Buffer contents at any given moment (128 max, 50+ needed):

 oldest ─────────────────────────────────────────────────── newest
 [  ][  ][  ][  ][  ][  ][  ][  ][  ] ... [  ][  ][  ][  ][  ][  ]
 ↑                                                              ↑
tail                                                           head

 PeekWindow(window, 50):  reads samples at [tail..tail+49]
                           DOES NOT advance tail

 After inference:
 RingBuffer_Advance(25):  advances tail by 25
                           → 50% overlap between consecutive windows

Timeline (@ 100 Hz):
  t=0ms   : first 50 samples collected
  t=500ms : inference on samples [0..49]
  t=250ms : inference on samples [25..74]   ← 50% overlap
  t=500ms : inference on samples [50..99]
  ...
  Inference rate = 100 Hz / 25 stride = 4 Hz
```

### 6.10 WCET Measurement

Thread 2 measures the worst-case execution time of each inference call using the DWT cycle counter:

```c
uint32_t t0 = DWT_GetCycles();
Inference_Run(ts_q, stat_q, &result);
uint32_t us = (DWT_GetCycles() - t0) / 84U;   // 84 MHz → µs

if (us > g_stats.inf_wcet_us) {
    g_stats.inf_wcet_us = (uint16_t)us;        // update global max
}
```

This WCET value is transmitted in every heartbeat frame (offset 26–27), allowing the upstream system to monitor inference timing degradation.

---

## 7. Model Architecture and Training Pipeline

### 7.1 Model Files

| File | Size | Description |
|---|---|---|
| `models/best_model.keras` | ~125 KB | Keras source model (training artifact) |
| `models/model_road_float32.tflite` | ~28 KB | TFLite float model |
| `models/model_road_int8.tflite` | ~16 KB | TFLite INT8 quantised model (deployed) |
| `models/model_data.h` | ~98 KB | CubeAI-generated C weight array |
| `models/norm_params.h` | — | Mirror of `include/norm_params.h` |
| `models/stat_norm.c/.h` | — | Z-score parameters (mean, std) |
| `models/stat_mean.npy` | — | Training-set feature means |
| `models/stat_std.npy` | — | Training-set feature standard deviations |

### 7.2 Network Topology

The deployed network is a **dual-input INT8 quantised CNN** generated by STM32CubeAI from the TFLite INT8 model. The two inputs represent complementary views of the same window:

```
Input 0: stat_i8   [50 × 1 int8]    ─────┐
  Statistical features (time-aggregated)  │
                                          ├──► [Fusion Layers] ──► Output [2 int8]
Input 1: ts_i8  [50 × 6 int8]       ─────┘                         [smooth, rough]
  Quantised time-series (temporal shape)
```

The exact topology (Conv layers, Dense layers, etc.) is determined by the `best_model.keras` training artifact and is embedded in the CubeAI-generated `network.h` / `network_data.h` headers. The `model_data.h` contains the quantised weight tables as C arrays.

### 7.3 Quantization Parameters

All quantization parameters are in `include/norm_params.h`:

```c
#define ACCEL_SCALE     8192.0f    // Physical scaling divisor
#define GYRO_SCALE      65.5f      // Physical scaling divisor
#define WINDOW_SIZE     50         // IMU samples per window
#define N_FEATURES      6          // ax, ay, az, gx, gy, gz
#define N_CLASSES       2          // smooth, rough
#define N_STAT_FEATURES 50         // Feature vector size
#define FFT_N           64         // FFT input size (padded)
#define FFT_HF_SPLIT    16         // High-freq bin start
#define VOTE_WINDOW     9          // Temporal smoothing window

// TFLite INT8 quantization parameters (from training):
#define TS_SCALE        0.189672f  // Time-series input scale
#define TS_ZP           -6         // Time-series zero-point
#define STAT_SCALE      0.057328f  // Statistical input scale
#define STAT_ZP         -53        // Statistical zero-point
```

### 7.4 Statistical Normalization (Z-Score)

The statistical features must be z-score normalized before quantization. The normalization parameters are stored in `stat_norm.c`:

```c
// For each of the 50 statistical features:
features[i] = (features[i] - stat_mean[i]) / stat_std[i]
```

`stat_mean[]` and `stat_std[]` are `float32_t` arrays of length 50, computed from the training dataset using Python's `sklearn.preprocessing.StandardScaler`. They are embedded in `stat_norm.c` as C initializers.

### 7.5 CubeAI Integration

STM32CubeAI converts the `.tflite` model into:
- `lib/CubeAI/Inc/` — API headers (`ai_datatypes_defines.h`, etc.)
- `lib/CubeAI/network/` — Generated `network.h`, `network_data.h`, `network_data_params.h`
- `lib/CubeAI/Lib/NetworkRuntime1020_CM4_GCC.a` — Precompiled runtime library

The runtime library is linked via the `link_cubeai.py` SCons extra script using `--start-group / --end-group` linker flags to resolve circular references:

```python
env.Append(
    _LIBFLAGS=" -Wl,--start-group " + lib_file + " -Wl,--end-group"
)
```

The `Inference_Init()` function binds the static activations arena:
```c
const ai_handle act_addr[] = { s_activations };
ai_error err = ai_network_create_and_init(&s_network, act_addr, NULL);
```

**Compile-time sanity checks:**

```c
#if (AI_NETWORK_IN_1_SIZE != N_STAT_FEATURES)
#  error "CubeAI input[0] size mismatch"
#endif
#if (AI_NETWORK_IN_2_SIZE != (WINDOW_SIZE * N_FEATURES))
#  error "CubeAI input[1] size mismatch"
#endif
```

These `#error` directives catch mismatches between the deployed model and the C constants at compile time.

### 7.6 Replay Mode for Offline Validation

The `REPLAY_MODE` preprocessor macro in `main.c` switches between live IMU and pre-recorded data:

```c
#define REPLAY_MODE   1   /* 1 = inject CSV, 0 = live IMU */
```

In replay mode:
- Thread 1 reads from `replay_data.h` (C array of `MPU6050_RawData_t`)
- The array contains pre-recorded samples from a known road type
- `g_replay_done` is set when the array is exhausted (wraps around)
- Validation counters track vote distribution:
  ```c
  volatile uint32_t g_replay_vote_smooth;
  volatile uint32_t g_replay_vote_rough;
  ```

This mode allows verifying the full pipeline (ring buffer → features → quantize → CubeAI → voting) against ground-truth CSV recordings without connecting physical hardware.

---

## 8. Communication Protocol (FRAME)

### 8.1 Wire Format

All outbound frames (STM32 → ESP32) use the following binary format. There is no SYNC byte in the outbound direction — the GPIO PA8 pulse serves as a frame delimiter.

```
Byte offset    Field        Size    Description
─────────────────────────────────────────────────────────────────
0              LEN          1       Total frame length - 1
                                    = payload_len + 6
                                    (TYPE + ECU_ID + payload + CRC32)
1              TYPE         1       Frame type identifier
2              ECU_ID       1       Source ECU identifier
3 .. (LEN-4)   PAYLOAD      N       Application data (0..248 bytes)
(LEN-3)        CRC[31:24]   1       CRC32 MSB (big-endian)
(LEN-2)        CRC[23:16]   1
(LEN-1)        CRC[15:8]    1
(LEN)          CRC[7:0]     1       CRC32 LSB
─────────────────────────────────────────────────────────────────
Total wire bytes = LEN + 1 = payload_len + 7
Max frame size  = 255 bytes (payload ≤ 248 bytes)
```

**CRC32 coverage:** The CRC is computed over `[TYPE, ECU_ID, PAYLOAD]` — starting at `out_buf[1]` for `payload_len + 2` bytes. The LEN byte is excluded.

**ECU identifiers:**

| ID | Constant | Description |
|---|---|---|
| 0x01 | `ECU_ID_STM32_NODE1` | This device (STM32F401CC) |
| 0x02 | `ECU_ID_ESP32_GATEWAY` | Upstream ESP32 |

### 8.2 Frame Types Reference

| Type Code | Constant | Direction | Description |
|---|---|---|---|
| 0x01 | `FRAME_TYPE_CLASSIFICATION` | STM32 → ESP32 | Road surface result |
| 0x02 | `FRAME_TYPE_TEMPERATURE` | STM32 → ESP32 | LM35 reading |
| 0x03 | `FRAME_TYPE_HEARTBEAT` | STM32 → ESP32 | Runtime diagnostics |
| 0x04 | `FRAME_TYPE_LOG` | STM32 → ESP32 | Structured event log |
| 0x05 | `FRAME_TYPE_ULTRASONIC` | STM32 → ESP32 | Dual HC-SR04 distances |
| 0xFE | `FRAME_TYPE_BL_ENTER` | ESP32 → STM32 | Bootloader entry trigger |
| 0xFD | `FRAME_TYPE_BL_DATA` | ESP32 → STM32 | Binary image data |
| 0xFC | `FRAME_TYPE_BL_VERIFY` | ESP32 → STM32 | Image verification |
| 0xFF | `FRAME_TYPE_BL_ACK` | STM32 → ESP32 | Bootloader acknowledgement |

### 8.3 Classification Frame Payload

**Total payload: 6 bytes**

```
Offset  Size  Field           Encoding
──────────────────────────────────────────────────────────
0       1     label           0x00 = SMOOTH, 0x01 = ROUGH
1       1     confidence      0..100 (percent)
2       4     timestamp_ms    uint32 big-endian (FreeRTOS tick)
──────────────────────────────────────────────────────────
Total wire frame: 6 + 7 = 13 bytes
```

### 8.4 Temperature Frame Payload

**Total payload: 6 bytes**

```
Offset  Size  Field           Encoding
──────────────────────────────────────────────────────────
0       2     temperature_x10 uint16 big-endian (e.g. 235 = 23.5°C)
2       4     timestamp_ms    uint32 big-endian
──────────────────────────────────────────────────────────
Total wire frame: 6 + 7 = 13 bytes
```

### 8.5 Heartbeat Frame Payload

**Total payload: 30 bytes** (`HEARTBEAT_PAYLOAD_SIZE = 30`)

```
Offset  Size  Field           Encoding / Description
──────────────────────────────────────────────────────────────────
0       4     uptime_ms       uint32 BE — ms since boot
4       2     cpu_t1_x100     uint16 BE — Sensor task CPU ×100 (5s avg)
6       2     cpu_t2_x100     uint16 BE — TinyML task CPU ×100 (5s avg)
8       2     cpu_t3_x100     uint16 BE — UART TX task CPU ×100 (5s avg)
10      2     cpu_t6_x100     uint16 BE — Temp task CPU ×100 (5s avg)
12      2     cpu_t7_x100     uint16 BE — Ultrasonic task CPU ×100 (5s avg)
14      2     cpu_idle_x100   uint16 BE — Idle CPU ×100 (5s avg)
16      2     stack_t1_free   uint16 BE — Thread 1 stack HWM (words free)
18      2     stack_t2_free   uint16 BE — Thread 2 stack HWM
20      2     stack_t3_free   uint16 BE — Thread 3 stack HWM
22      2     stack_t6_free   uint16 BE — Thread 6 stack HWM
24      2     stack_t7_free   uint16 BE — Thread 7 stack HWM
26      2     inf_wcet_us     uint16 BE — Peak inference time (µs, 5s window)
28      2     rb_max_fill     uint16 BE — Peak ring buffer fill (sample count)
──────────────────────────────────────────────────────────────────
Total wire frame: 30 + 7 = 37 bytes
```

**CPU % decoding (receiver side):**
```python
cpu_t1_percent = cpu_t1_x100 / 100.0   # e.g. 1234 → 12.34%
```

**Stack health decoding:**
```python
stack_t1_bytes_free = stack_t1_free * 4   # words → bytes on Cortex-M4
```

### 8.6 Ultrasonic Frame Payload

**Total payload: 8 bytes**

```
Offset  Size  Field           Encoding
──────────────────────────────────────────────────────────
0       2     dist1_cm        uint16 BE — Sensor 1 distance, 0xFFFF=timeout
2       2     dist2_cm        uint16 BE — Sensor 2 distance, 0xFFFF=timeout
4       4     timestamp_ms    uint32 BE
──────────────────────────────────────────────────────────
Total wire frame: 8 + 7 = 15 bytes
```

### 8.7 Log Frame Payload

**Total payload: 10 bytes** (`LOG_PAYLOAD_SIZE = 10`)

```
Offset  Size  Field           Encoding
──────────────────────────────────────────────────────────
0       1     code            Log_Code_t value (see Section 13.2)
1       1     severity        0=INFO, 1=WARN, 2=ERROR, 3=FATAL
2       4     timestamp_ms    uint32 BE — FreeRTOS tick at log time
6       4     aux_data        uint32 BE — caller context value
──────────────────────────────────────────────────────────
Total wire frame: 10 + 7 = 17 bytes
```

### 8.8 Bootloader Frames (ESP32 → STM32)

Inbound frames from the ESP32 use a SYNC byte prefix (`0xAA`) to allow the Thread 5 state machine to synchronise:

```
0xAA  LEN  TYPE  ECU_ID  PAYLOAD...  CRC32(4 bytes)
```

**Bootloader entry sequence (simplified):**

```
ESP32:  sends 0xAA 0xEB   (raw bytes, not full frame format)
STM32:  Thread 5 catches sequence, sends ACK {0xEE, 0xAA}
ESP32:  proceeds with BL_DATA frames
```

### 8.9 CRC32 Algorithm

The CRC32 is a custom variant that uses **32 inner iterations per byte** (not the standard 8). This is intentional to match the ESP32 side's `calculateCRC32()`:

```c
uint32_t Frame_CRC32(const uint8_t *data, uint16_t len) {
    uint32_t crc = 0xFFFFFFFF;   // INIT value
    for (i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (bit = 0; bit < 32; bit++) {   // ← 32, NOT 8
            if (crc & 0x80000000)
                crc = (crc << 1) ^ 0x04C11DB7;
            else
                crc <<= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;   // XOR-OUT value
}
```

> ⚠️ **Important:** Do NOT change to 8 iterations. Both ends of the link must use the same 32-iteration variant. The mathematical output differs from standard MPEG-2 CRC32.

### 8.10 GPIO Sync Signal

PA8 is toggled around each UART DMA transmission as a hardware frame delimiter:

```
PA8 timeline:
                ┌──────────────────────┐
                │   UART frame on wire │
────────────────┘                      └────────────────
↑ Frame_Build + DMA start               ↑ DMA TC ISR (Thread3 notification)

Thread 3 sets PA8 HIGH before DMA starts and LOW after the DMA TC ISR fires.
```

This allows the ESP32 GPIO interrupt to trigger at frame start without a software SYNC byte, saving 1 byte per frame and avoiding an additional state machine.

---

## 9. UART Stack

### 9.1 Driver Layer (UART_INTERFACE)

The UART driver configures USART1 via direct register writes (no HAL). Configuration:

```c
UART_Config_t uart_cfg = {
    .uart_id      = UART1_ID,
    .baudrate     = 115200U,
    .parity       = UART_PARITY_NONE,
    .stop_bits    = UART_STOP_1,
    .data_bits    = UART_DATA_8BIT,
    .oversampling = UART_OVER8_DISABLE,  // OVER8=0 → 16x oversampling
    .tx_mode      = UART_MODE_DMA,
    .rx_mode      = UART_MODE_IRQ
};
```

**Baud rate divisor** (84 MHz APB2, OVER8=0):
```
BRR = PCLK2 / (16 × baud) = 84,000,000 / (16 × 115200) = 45.57
BRR_INT = 45, BRR_FRAC = 9  →  actual baud ≈ 115,107 (error < 0.1%)
```

### 9.2 Service Layer (UART_SERVICE)

The service layer adds ring-buffered TX (for small IRQ-driven transfers), DMA TX (for large frame transmissions), and RX buffering.

**Per-instance context structure:**

```c
typedef struct {
    UART_SVC_TxRing_t  tx;           // 256-byte TX ring buffer
    UART_SVC_RxRing_t  rx;           // 256-byte RX ring buffer
    volatile uint8_t   tx_active;    // DMA/IRQ TX in progress
    volatile uint8_t   error;        // UART error flag
    uint8_t            initialized;
    UART_SVC_TxMode_t  tx_mode;      // IRQ or DMA
    DMA_Id_t           dma_id;       // DMA controller
    DMA_StreamId_t     dma_stream;
    DMA_Channel_t      dma_ch;
    UART_SVC_TxDoneCb_t tx_done_cb; // TX complete callback
    void               *tx_done_ctx;
    TaskHandle_t        rx_notify_task; // Task to notify on RX byte
} UART_SVC_Instance_t;

static UART_SVC_Instance_t svc[3];  // UART1, UART2, UART3
```

### 9.3 DMA TX Path

```
Thread 3 calls UART_SVC_TransmitDMA(UART1_ID, &buf)
    │
    ▼
Check svc[id].tx_active == 0   (not busy)
    │
    ▼
Configure DMA2 Stream7:
  - Source: buf->data (memory, increment)
  - Dest: USART1->DR (peripheral, fixed)
  - Length: buf->length
  - Direction: memory-to-peripheral
  - Channel: 4
    │
    ▼
Enable DMA2 Stream7 → hardware streams bytes to USART1 TX FIFO
    │
    ▼  (DMA TC interrupt fires when all bytes sent)
DMA TC ISR calls svc[id].tx_done_cb (= on_uart_tx_done)
    │
    ▼
on_uart_tx_done() calls vTaskNotifyGiveFromISR(s_thread3_handle)
    │
    ▼
Thread 3 wakes from ulTaskNotifyTake, clears PA8
```

### 9.4 IRQ RX Path

```
UART byte received → USART1 RXNE interrupt
    │
    ▼
UART ISR reads USART1->DR
    │
    ▼
rx_push() into svc[id].rx ring buffer
    │
    ▼
if rx_notify_task != NULL:
    vTaskNotifyGiveFromISR(rx_notify_task)
    │
    ▼
Thread 5 wakes, drains RX ring buffer
```

### 9.5 TX Done Callback Chain

```
UART_SVC_RegisterTxDoneCb(UART1_ID, on_uart_tx_done, NULL)
    │  stores callback in svc[UART1_ID].tx_done_cb
    │
    ▼
DMA TC ISR → svc.tx_done_cb() → on_uart_tx_done()
    │
    ▼
vTaskNotifyGiveFromISR(s_thread3_handle)
    portYIELD_FROM_ISR(woken)
    │
    ▼
Thread 3 resumes → GPIO PA8 LOW → IWDG alive → next frame
```

---

## 10. Ring Buffer (SPSC Lock-Free)

### 10.1 Design Invariants

The ring buffer is a **Single-Producer Single-Consumer (SPSC)** design. The key invariants are:

| Rule | Description |
|---|---|
| **RB-C01** | `head` is written ONLY by the producer (Thread 1 / DMA ISR callback). `tail` is written ONLY by the consumer (Thread 2). |
| **RB-C07** | A DMB (Data Memory Barrier) instruction is used after writing data and before advancing `head`. This prevents CPU and compiler reordering. |
| **RB-C08** | No interrupt disabling anywhere in the ring buffer. The SPSC invariant + DMB barriers are the sole synchronisation mechanism. |

Because of the SPSC invariant, no mutex is needed. Thread 1 (producer) only reads `tail` and only writes `head`. Thread 2 (consumer) only reads `head` and only writes `tail`. These accesses are inherently safe on a single Cortex-M4 core.

### 10.2 Memory Barrier Strategy

```c
// In RingBuffer_Push() after writing data:
__asm volatile ("dmb" ::: "memory");
// "memory" clobber = compiler barrier (prevents instruction reorder)
// "dmb" instruction = CPU barrier (prevents hardware store reordering)
```

Both are necessary:
- The compiler barrier prevents the compiler from reordering the `head` increment before the data write at the optimisation level `-O2`.
- The DMB instruction ensures the Cortex-M4 write buffer has flushed the data write before the head update becomes visible to Thread 2.

### 10.3 Capacity and Overflow Policy

```
Capacity:  128 samples (power of 2 → mask-based wrapping)
Per sample: 14 bytes (MPU6050_RawData_t)
Total RAM:  1,792 bytes

At 100 Hz input rate:
  Full buffer =  1.28 seconds of data
  Consumer drain rate (at 4 Hz inference, 25 sample advance) = 100 Hz
  → Consumer keeps up with producer: ring buffer stays ≈50 samples deep
```

**Overflow policy:** When full, `RingBuffer_Push()` **drops the new sample** and logs `LOG_ERROR(RING_BUFFER_DROP)`. The oldest data is preserved. This is the correct choice for road surface monitoring: a dropped sample is better than overwriting unread samples and corrupting the window.

### 10.4 API Reference

```c
// Initialize (call once before scheduler)
RingBuffer_Error_t RingBuffer_Init(void);

// Push one sample (ISR/Thread safe for producer)
RingBuffer_Error_t RingBuffer_Push(const MPU6050_RawData_t *sample);

// Returns number of valid samples in buffer
uint32_t RingBuffer_Count(void);

// Read N samples starting at tail WITHOUT advancing tail
RingBuffer_Error_t RingBuffer_PeekWindow(MPU6050_RawData_t *out, uint32_t n);

// Advance tail by N (consume N samples)
RingBuffer_Error_t RingBuffer_Advance(uint32_t n);

// Get maximum fill level since init (for heartbeat)
uint32_t RingBuffer_GetMaxFill(void);
```

---

## 11. IWDG Thread Supervisor

### 11.1 Architecture

The Independent Watchdog (IWDG) is driven by the LSI oscillator (~32 kHz) and runs completely independently of the CPU clock — it cannot be stopped by software once started. This project layers a **thread supervision scheme** on top of the hardware watchdog:

```
┌──────────────────────────────────────────────────────────────┐
│                   IWDG Supervisor Scheme                     │
│                                                              │
│  Thread 1 ──sets──► IWDG_Thread1_Alive                      │
│  Thread 2 ──sets──► IWDG_Thread2_Alive                      │
│  Thread 3 ──sets──► IWDG_Thread3_Alive           ┌─────┐    │
│  Thread 4 ──sets──► IWDG_Thread4_Alive           │ LSI │    │
│                                                  │ 32k │    │
│  vApplicationIdleHook()  (called every idle tick) └──┬──┘    │
│       │                                              │       │
│       ▼                                        ┌────▼────┐  │
│  IWDG_SupervisorFeed()                         │  IWDG   │  │
│       │                                        │ 3000ms  │  │
│  if ALL alive flags == 1:                      │ timeout │  │
│       clear all flags                          └────┬────┘  │
│       IWDG_Feed() → reload counter                 │       │
│  else:                                        reset if     │
│       do NOT feed → MCU resets at 3000ms ◄──── starved    │
└──────────────────────────────────────────────────────────────┘
```

### 11.2 Thread-Alive Protocol

Each monitored thread **sets its alive flag** at the end of each work cycle:

```c
// Each thread calls at least once per IWDG timeout window:
IWDG_Thread_SetAlive(&IWDG_Thread1_Alive);
```

The alive flag variables are `volatile uint8_t` defined in `IWGD.c`:

```c
volatile uint8_t IWDG_Thread1_Alive = 0U;
volatile uint8_t IWDG_Thread2_Alive = 0U;
volatile uint8_t IWDG_Thread3_Alive = 0U;
volatile uint8_t IWDG_Thread4_Alive = 0U;
```

They are externally declared in `IWDG_INTERFACE.h` so each thread can write to them.

The initial values are set to 1 (all alive) before `IWDG_Start()` to prevent an immediate reset on the first supervisor check.

### 11.3 Supervisor Feed Logic

`IWDG_SupervisorFeed()` runs from the idle hook (lowest priority context):

```c
void vApplicationIdleHook(void) {
    IWDG_SupervisorFeed();
}
```

Inside `IWDG_SupervisorFeed()`:

```
if (IWDG_Thread1_Alive && IWDG_Thread2_Alive &&
    IWDG_Thread3_Alive && IWDG_Thread4_Alive)
{
    IWDG_Thread1_Alive = 0U;
    IWDG_Thread2_Alive = 0U;
    IWDG_Thread3_Alive = 0U;
    IWDG_Thread4_Alive = 0U;
    IWDG_Feed();           // reload IWDG counter
}
// else: one or more threads hung → do NOT feed → MCU resets at timeout
```

The idle hook runs frequently as long as there is idle CPU time. If a high-priority thread hangs in an infinite loop, the idle hook may also stop running — but in that case the IWDG will still expire (its clock is independent).

### 11.4 Configuration and Timeout

```c
IWDG_Init(3000U);   // 3000 ms timeout
```

**Internal calculation:**

```
LSI frequency ≈ 32,000 Hz
Target timeout = 3000 ms = 3 s
Required tick count = timeout_ms × LSI / 1000 / prescaler

Optimal prescaler = 256 (IWDG_PRESCALER_DIV_256)
  → LSI_effective = 32,000 / 256 ≈ 125 Hz
  → Reload = 3000 × 125 / 1000 = 375
  → Actual timeout = 375 / 125 = 3.0 s
```

### 11.5 API Reference

```c
// Initialize with millisecond timeout (auto-selects prescaler + reload)
IWDG_Status_t IWDG_Init(uint32_t timeout_ms);

// Start the watchdog (cannot be stopped after this)
IWDG_Status_t IWDG_Start(void);

// Feed (reload) the watchdog counter
IWDG_Status_t IWDG_Feed(void);

// Thread sets its alive flag
void IWDG_Thread_SetAlive(volatile uint8_t *flag);

// Supervisor: check all flags, feed if all alive
IWDG_Status_t IWDG_SupervisorFeed(void);
```

---

## 12. OTA Bootloader Entry Mechanism

### 12.1 Protocol Sequence

The OTA bootloader entry is triggered by a two-byte magic sequence sent over UART from the ESP32:

```
ESP32                                        STM32
  │                                            │
  │──── 0xAA 0xEB ─────────────────────────►  │  Thread 5 detects
  │                                            │  magic sequence
  │  ◄── 0xEE 0xAA ───────────────────────────│  ACK response (DMA)
  │                                            │
  │                                            │  vTaskDelay(2ms)
  │                                            │
  │                                            │  FLASH_Unlock()
  │                                            │  FLASH_EraseSector(1)
  │                                            │  FLASH_Lock()
  │                                            │
  │                                            │  AIRCR.SYSRESETREQ = 1
  │                                            │  (MCU RESETS)
  │                                            │
  │                                    ┌───────▼────────┐
  │                                    │  Bootloader    │
  │◄───────────────────────────────────│  (Sector 0)    │
  │  Send BL_DATA frames (new image)   │  starts        │
                                       └────────────────┘
```

### 12.2 Flash Erase and Reset Flow

**Why erase Sector 1?**

Sector 1 (0x08004000) is the second part of the bootloader. Erasing it is used as a signal to the bootloader in Sector 0 that a firmware update is in progress. After reset, the Sector 0 bootloader checks Sector 1 — if erased, it enters image reception mode.

**System reset mechanism:**

```c
// SCB Application Interrupt and Reset Control Register
*((volatile uint32_t *)0xE000ED0CU) = (0x05FAUL << 16U) | (1UL << 2U);
//                                      VECTKEY                SYSRESETREQ
```

The `VECTKEY` value `0x05FA` is required to unlock the register write. `SYSRESETREQ` (bit 2) triggers a full system reset.

### 12.3 Thread 5 State Machine

```
                ┌───────────────────────────────┐
                │         State 0               │
                │     Waiting for 0xAA          │
                └──────────────┬────────────────┘
                               │
         ┌─────────────────────┼──────────────────────────┐
         │ byte == 0xAA        │                           │ byte != 0xAA
         ▼                     │                           │
  ┌──────────────┐             │                     stay in State 0
  │   State 1    │             │
  │ Waiting 0xEB │             │
  └──────────────┘             │
         │                     │
 ┌───────┼───────────┐         │
 │       │           │         │
 │ byte  │     byte  │   byte  │
 │==0xEB │    ==0xAA │  other  │
 ▼       │     ▼     │   ▼     │
ENTRY    │  stay in  │ State 0 │
         │  State 1  │         │
         └───────────┘─────────┘
```

---

## 13. Logging System

### 13.1 Architecture

The logger provides a structured, ISR-safe, non-blocking logging mechanism that serialises log events over UART without impacting real-time tasks.

```
Any Task/ISR
    │
    │ LOG_INFO / LOG_WARN / LOG_ERROR / LOG_FATAL macro
    │
    ▼
Logger_Log(code, severity, aux_data)
    │
    ├── xPortIsInsideInterrupt()?
    │     Yes → xQueueSendFromISR(s_logger_queue)
    │     No  → xQueueSend(s_logger_queue, &entry, 0)   ← non-blocking
    │
    ▼  (s_logger_queue, depth=8, type=Log_Payload_t, 10 bytes each)

Logger Task (Priority 0, lowest)
    │
    ▼ xQueueReceive(s_logger_queue, portMAX_DELAY)
    │
    ▼ Build FrameRequest_t (type=FRAME_TYPE_LOG, 10-byte payload)
    │
    ▼ xQueueSend(g_frame_queue, &req, 0)
    │
    ▼ Thread 3 → DMA → UART → ESP32
```

### 13.2 Log Code Taxonomy

```
Group 0x10..0x1F — Hardware errors:
  0x10  LOG_CODE_MPU6050_TIMEOUT    I2C read timed out
  0x11  LOG_CODE_I2C_BUS_STUCK      I2C bus lockup detected
  0x12  LOG_CODE_RING_BUFFER_DROP   IMU sample dropped (buffer full)
  0x13  LOG_CODE_DMA_ERROR          DMA transfer error

Group 0x20..0x2F — Software errors:
  0x20  LOG_CODE_INFERENCE_FAIL     CubeAI returned error
  0x21  LOG_CODE_QUEUE_FULL         g_frame_queue overflow
  0x22  LOG_CODE_FRAME_BUILD_FAIL   Frame serialisation error
  0x23  LOG_CODE_LOGGER_DROP        s_logger_queue overflow

Group 0x30..0x3F — Status events:
  0x30  LOG_CODE_BOOT               System started
  0x31  LOG_CODE_THREAD_STARTED     Task creation confirmed
  0x32  LOG_CODE_RESET_DETECTED     Reset source identified

Group 0x40..0x4F — Warnings:
  0x40  LOG_CODE_LOW_CONFIDENCE     Classifier confidence < threshold
  0x41  LOG_CODE_STACK_HIGH_WATER   Stack usage approaching limit
```

### 13.3 ISR Safety

The `xPortIsInsideInterrupt()` function reads the Cortex-M4 IPSR (Interrupt Program Status Register):

```
IPSR != 0  →  executing inside an interrupt  →  use *FromISR variant
IPSR == 0  →  task context                  →  use standard variant
```

The logger queue depth of 8 entries means up to 8 log events can be buffered before the logger task drains them. If the queue fills, `s_drop_count` is incremented silently (no recursion, no blocking).

### 13.4 API Reference

```c
// Initialise queue and task (call before vTaskStartScheduler)
uint8_t Logger_Init(void);

// Log entry — ISR-safe, non-blocking
void Logger_Log(uint8_t code, uint8_t severity, uint32_t aux_data);

// Convenience macros
#define LOG_INFO(code, aux)    Logger_Log(code, LOG_SEV_INFO,  aux)
#define LOG_WARN(code, aux)    Logger_Log(code, LOG_SEV_WARN,  aux)
#define LOG_ERROR(code, aux)   Logger_Log(code, LOG_SEV_ERROR, aux)
#define LOG_FATAL(code, aux)   Logger_Log(code, LOG_SEV_FATAL, aux)
```

---

## 14. Peripheral Drivers — API Reference

All peripheral drivers follow a consistent pattern:
- Register-level implementation (no STM32 HAL)
- Return status codes for all operations
- No dynamic allocation
- ISR-safe where applicable

### 14.1 RCC — Reset and Clock Control

```c
// Configure HSI PLL for 84 MHz operation
void RCC_INIT_84MHz_HSI(void);

// Enable peripheral clock
void RCC_EN_CLK_PERIPHERAL(PERIPH_t periph);

// Enable LSI oscillator (required for IWDG)
void RCC_LSI_Enable(void);
```

**Peripherals supported:**

```c
typedef enum {
    PERIPH_GPIOA, PERIPH_GPIOB, PERIPH_GPIOC,
    PERIPH_I2C1,
    PERIPH_DMA1, PERIPH_DMA2,
    PERIPH_USART1,
    PERIPH_ADC1,
    PERIPH_TIM2, PERIPH_TIM3, ...
} PERIPH_t;
```

### 14.2 GPIO Driver

```c
typedef struct {
    GPIO_PIN_t    Pin;       // GPIO_PIN0 .. GPIO_PIN15
    GPIO_PORT_t   Port;      // GPIO_PORTA .. GPIO_PORTC
    GPIO_Mode_t   Mode;      // INPUT, OUTPUT, ALTERNATE, ANALOG
    GPIO_Type_t   Type;      // PUSH_PULL, OPEN_DRAIN
    GPIO_Speed_t  Speed;     // LOW, MEDIUM, HIGH, VERY_HIGH
    GPIO_Pull_t   Pull;      // NO_PULL, PULL_UP, PULL_DOWN
    uint8_t       Alternate; // AF0..AF15 (AF_SYSTEM, AF_I2C_1_3, etc.)
} GPIO_CONFIG_t;

void GPIO_INIT(const GPIO_CONFIG_t *cfg);
void GPIO_WritePin(GPIO_PORT_t port, GPIO_PIN_t pin, GPIO_State_t state);
void GPIO_TogglePin(GPIO_PORT_t port, GPIO_PIN_t pin);
GPIO_State_t GPIO_ReadPin(GPIO_PORT_t port, GPIO_PIN_t pin);
```

### 14.3 I2C Driver and Service

**Driver layer (`I2C_INTERFACE.h`):**

```c
typedef struct {
    I2C_Id_t         id;                // I2C_ID_1
    I2C_Speed_t      speed;             // I2C_SPEED_FAST (400 kHz)
    I2C_DutyCycle_t  duty_cycle;        // I2C_DUTY_2 (T_low/T_high = 2)
    I2C_AddrMode_t   addressing_mode;   // I2C_ADDR_7BIT
    I2C_TransferMode_t transfer_mode;   // I2C_MODE_DMA
    // ... filter, dual address, etc.
} I2C_Config_t;

I2C_Status_t I2C_Init(const I2C_Config_t *cfg);
```

**Service layer (`I2C_SERVICE.h`):**

```c
// Init with IRQ priorities and DMA assignment
I2C_SVC_Status_t I2C_SVC_Init(
    I2C_Id_t id,
    uint8_t event_irq_priority,
    uint8_t error_irq_priority,
    DMA_Id_t dma_id, DMA_StreamId_t stream, DMA_Channel_t ch,
    uint8_t dma_irq_priority
);

// Non-blocking DMA read (fires callback when done)
I2C_SVC_Status_t I2C_SVC_ReadDMA(
    I2C_Id_t id,
    I2C_DevAddr7_t dev_addr,
    uint8_t reg_addr,
    uint8_t *buf,
    uint16_t len,
    I2C_SVC_DoneCallback_t callback,
    void *ctx
);
```

### 14.4 DMA Driver

```c
typedef struct {
    DMA_Id_t         dma_id;     // DMA_1 or DMA_2
    DMA_StreamId_t   stream;     // DMA_STREAM_0 .. DMA_STREAM_7
    DMA_Channel_t    channel;    // DMA_CHANNEL_0 .. DMA_CHANNEL_7
    DMA_Direction_t  direction;  // PERIPH_TO_MEMORY, MEMORY_TO_PERIPH
    DMA_Mode_t       mode;       // DMA_MODE_NORMAL, DMA_MODE_CIRCULAR
    uint32_t         src_addr;
    uint32_t         dst_addr;
    uint16_t         length;
    // ...
} DMA_Config_t;

DMA_Status_t DMA_Init(const DMA_Config_t *cfg);
DMA_Status_t DMA_Start(DMA_Id_t id, DMA_StreamId_t stream);
```

**DMA assignments:**

| DMA | Stream | Channel | Usage |
|---|---|---|---|
| DMA1 | Stream 0 | CH1 | I2C1 RX (MPU6050 burst read) |
| DMA2 | Stream 7 | CH4 | USART1 TX (frame transmission) |

### 14.5 TIM Driver (TIM2 and TIM3)

```c
typedef struct {
    TIM_Id_t      id;         // TIM_ID_2, TIM_ID_3
    uint16_t      prescaler;
    uint32_t      period;     // ARR value
    TIM_Mode_t    mode;       // TIM_MODE_UP
    TIM_ARPE_t    arpe;       // TIM_ARPE_ENABLE (buffered reload)
    TIM_Callback_t callback;  // ISR callback (or NULL)
    void         *ctx;
} TIM_Config_t;

TIM_Status_t TIM_Init(const TIM_Config_t *cfg);
TIM_Status_t TIM_Start(TIM_Id_t id);
TIM_Status_t TIM_Stop(TIM_Id_t id);

// Input Capture (for HC-SR04 on TIM3)
typedef struct {
    TIM_Id_t        id;
    TIM_Channel_t   channel;    // TIM_CH_1, TIM_CH_2
    TIM_IC_Polarity_t polarity; // TIM_IC_POLARITY_RISING
    TIM_IC_Callback_t callback; // fires on each capture
    void             *ctx;      // 0 for CH1, 1 for CH2
} TIM_IC_Config_t;

TIM_Status_t TIM_IC_Init(const TIM_IC_Config_t *cfg);
TIM_Status_t TIM_IC_GetCapture(TIM_Id_t id, TIM_Channel_t ch, uint32_t *val);
```

### 14.6 ADC Driver (LM35)

```c
typedef struct {
    uint8_t          channel;      // 0..18 (1 for PA1/LM35)
    ADC_Resolution_t resolution;   // ADC_RES_12BIT
    ADC_SampleTime_t sample_time;  // ADC_SAMPLETIME_480
} ADC_Config_t;

ADC_Error_t ADC_Init(const ADC_Config_t *cfg);
ADC_Error_t ADC_Read(uint16_t *raw_out);

// Convert raw to tenths of °C
uint16_t ADC_LM35_ToTenthsCelsius(uint16_t raw, uint16_t vref_mv);
```

**LM35 conversion implementation:**

```c
uint16_t ADC_LM35_ToTenthsCelsius(uint16_t raw, uint16_t vref_mv) {
    // Vout_mV = (raw * vref_mv) / 4095
    // Temp_x10 = Vout_mV / 10  [LM35: 10mV/°C]
    uint32_t vout_mv = ((uint32_t)raw * vref_mv) / 4095U;
    return (uint16_t)(vout_mv / 10U);
}
```

### 14.7 FLASH Driver

```c
FLASH_Status_t FLASH_Unlock(void);
FLASH_Status_t FLASH_Lock(void);
FLASH_Status_t FLASH_EraseSector(FLASH_Sector_t sector, FLASH_VoltageRange_t vr);
FLASH_Status_t FLASH_MassErase(void);
FLASH_Status_t FLASH_ProgramByte(uint32_t address, uint8_t data);
FLASH_Status_t FLASH_ProgramWord(uint32_t address, uint32_t data);
FLASH_Status_t FLASH_Verify(uint32_t address, const uint8_t *data, uint32_t len);
```

**Voltage range for 3.3V operation:** `FLASH_VOLTAGE_2_7V_TO_3_6V` allows up to 32-bit word programming.

### 14.8 NVIC Driver

```c
void NVIC_SetPriority(IRQn_Type irq, uint8_t priority);
void NVIC_EnableIRQ(IRQn_Type irq);
void NVIC_DisableIRQ(IRQn_Type irq);
void NVIC_ClearPendingIRQ(IRQn_Type irq);
```

**Cortex-M4 priority grouping:** The STM32F401 implements 4 bits of priority (0–15, lower = higher priority). FreeRTOS uses `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` to set the boundary between FreeRTOS-managed (≥ this value) and non-managed ISRs.

### 14.9 DWT Driver

The Data Watchpoint and Trace unit provides a 32-bit free-running cycle counter at CPU clock frequency:

```c
void     DWT_Init(void);           // Enable DWT_CYCCNT
uint32_t DWT_GetCycles(void);      // Read DWT_CYCCNT (84 MHz resolution)
void     DWT_DelayUs(uint32_t us); // Busy-wait using DWT
```

`DWT_Init()` sets `DWT_CTRL.CYCCNTENA = 1` and `CoreDebug->DEMCR |= DCB_DEMCR_TRCENA_Msk`.

**Usage for µs-precision timing:**
```c
uint32_t t0 = DWT_GetCycles();
// ... code to measure ...
uint32_t elapsed_us = (DWT_GetCycles() - t0) / 84;  // 84 cycles/µs
```

### 14.10 SYSTICK (disabled)

The file `src/SYSTICK.c.disabled` exists but is excluded from the build. FreeRTOS uses the SysTick timer for its tick interrupt (configured internally), so the application cannot also use SysTick independently.

---

## 15. Build System

### 15.1 PlatformIO Configuration

The project is built with PlatformIO using the STM32 platform:

```ini
[platformio]
default_envs = genericSTM32F401CC

[env:genericSTM32F401CC]
platform     = ststm32
board        = genericSTM32F401CC
framework    = cmsis
upload_protocol = stlink
debug_tool   = stlink
build_type   = debug
```

**Framework:** `cmsis` (not `arduino` or `stm32cube`) — gives access to CMSIS headers without the STM32 HAL overhead.

**Upload and debug:** ST-Link (SWD interface). The `debug_build_flags` are:

```ini
debug_build_flags = -Og -ggdb3 -g3
```

`-Og` is GCC's "debug-friendly" optimisation level — it enables optimisations that don't interfere with debugging while still providing reasonable performance for timing-critical code.

### 15.2 CubeAI Library Linking

CubeAI provides a precompiled static library:
```
lib/CubeAI/Lib/NetworkRuntime1020_CM4_GCC.a
```

This library is linked via the `link_cubeai.py` pre-build script because PlatformIO's normal `lib_deps` mechanism doesn't handle `.a` files with circular symbol references well:

```python
# link_cubeai.py
Import("env")
lib_file = os.path.join(env.subst("$PROJECT_DIR"), "lib", "CubeAI", "Lib",
                        "NetworkRuntime1020_CM4_GCC.a")
env.Append(
    LINKFLAGS=["-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16",
               "-mfloat-abi=hard"]
)
env.Append(
    _LIBFLAGS=" -Wl,--start-group " + lib_file + " -Wl,--end-group"
)
```

`--start-group / --end-group` forces the linker to re-scan the archive multiple times to resolve circular references between the CubeAI runtime and the application objects.

### 15.3 CMSIS-DSP Integration

CMSIS-DSP provides the ARM-optimised FFT used in `Features_Extract()`:

```ini
build_flags =
    -DARM_MATH_CM4
    -I/home/ehab/.platformio/packages/framework-cmsis/CMSIS/DSP/Include
    -I/home/ehab/.platformio/packages/framework-cmsis/CMSIS/Core/Include
```

The `arm_rfft_fast_f32()` function in `Features_Init()` uses the Cortex-M4 FPU for single-precision SIMD operations, giving approximately 4× speedup over a software-only implementation.

The CMSIS-DSP source filter in `build_src_filter`:
```ini
build_src_filter =
    +<*>
    +<../lib/CubeAI/network/>
    +<../lib/CMSIS_DSP_wrapper/>
```

### 15.4 Build Flags and Optimisation

```ini
build_flags =
    -O2                    # Release optimisation (size + speed balance)
    -DSTM32F401xC          # MCU family selector (affects CMSIS headers)
    -mfpu=fpv4-sp-d16      # Single-precision FPU
    -mfloat-abi=hard       # ABI: pass float args in FPU registers
    -ffast-math            # Allow non-IEEE math optimisations (OK for ML)
    -Wall -Wextra          # All warnings (code quality)

build_unflags = -Os -O0 -mfloat-abi=soft -msoft-float
```

**`-ffast-math` implications:** This allows the compiler to:
- Assume no NaN/Inf in intermediate results (the explicit `feat_sanitize()` call handles NaN at output)
- Use FMA (fused multiply-add) instructions
- Reorder floating-point operations (acceptable for statistics)

This flag can provide 15–30% speedup on the feature extraction pipeline on Cortex-M4.

### 15.5 FPU Configuration (FPv4-SP-D16)

The STM32F401CC's FPU is a **single-precision only** unit (`-SP-` in FPv4-SP-D16). Double-precision operations will be emulated in software — avoid them in time-critical paths.

**ABI:** `-mfloat-abi=hard` passes `float` function arguments directly in FPU registers (S0–S15), eliminating the general-purpose-register-to-FPU register transfer overhead. This is critical for the feature extraction functions which pass float arrays.

### 15.6 GPIO Patch

The `fix_gpio.patch` file contains a patch that adds the UART1 RX GPIO initialisation to `main.c`. In the original code, only the TX pin was configured. The patch adds:

```c
GPIO_CONFIG_t rx = {
    .Pin = GPIO_PIN10, .Port = GPIO_PORTA,
    .Mode = GPIO_MODE_ALTERNATE, .Type = GPIO_INPUT_FLOATING,
    .Speed = GPIO_SPEED_VERY_HIGH, .Pull = GPIO_NO_PULL,
    .Alternate = AF_USART_1_2
};
GPIO_INIT(&rx);
```

Note: The current `main.c` already has this fix applied (with `GPIO_OUTPUT_PUSH_PULL` and `GPIO_PULL_UP` instead of `GPIO_INPUT_FLOATING`). The patch represents an intermediate version.

---

## 16. Interrupt Priority Map

The Cortex-M4 on STM32F401 supports 16 priority levels (0 = highest, 15 = lowest). FreeRTOS reserves priorities above `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` for non-OS-aware ISRs.

```
Priority  IRQ                    Handler / Action
────────────────────────────────────────────────────────────────
 5        TIM2                   on_tim2_update() → xSemaphoreGiveFromISR
 5        TIM3                   on_hcsr04_capture() → xQueueSendFromISR
 6        DMA1 Stream0 (I2C RX)  on_mpu_read_done() → xSemaphoreGiveFromISR
 6        DMA2 Stream7 (UART TX) on_uart_tx_done() → vTaskNotifyGiveFromISR
 6        USART1                 UART RX ISR → rx_push() + vTaskNotifyGiveFromISR
 6        I2C1 Event             I2C service event handler
 6        I2C1 Error             I2C service error handler
────────────────────────────────────────────────────────────────
 FreeRTOS SysTick (internally configured by FreeRTOS port)
 FreeRTOS PendSV  (context switch, lowest priority = 15)
────────────────────────────────────────────────────────────────
 Notes:
 - All ISRs at priority 5-6 can use FreeRTOS FromISR functions
 - configMAX_SYSCALL_INTERRUPT_PRIORITY typically = 5 (or lower)
 - TIM2/TIM3 at priority 5 allows them to preempt DMA/UART ISRs
```

---

## 17. Data Flow and Timing Analysis

### 17.1 End-to-End Latency Budget

From the IMU measurement to the UART frame departure:

```
Event                              Time        Accumulated
──────────────────────────────────────────────────────────
TIM2 interrupt fires               t = 0
Thread 1 wakes (sem)               t ≈ 0.1 ms
MPU6050 DMA read starts            t ≈ 0.2 ms
I2C transfer (14 bytes @ 400kHz)   t ≈ 0.25ms  t ≈ 0.5 ms
DMA ISR + copy to ring buffer      t ≈ 0.05ms  t ≈ 0.55ms
Thread 1 completes, notifies T2    t ≈ 0.05ms  t ≈ 0.6 ms

Thread 2 accumulates 50 samples:
  50 samples × 10 ms each = 500 ms to fill first window
  After steady state: wakes every 250 ms (at 50% overlap)

Thread 2 processing (single window):
  Scale_RawWindow()               ≈ 0.1 ms
  Features_Extract()              ≈ 5–15 ms (FFT + stats)
  Quantize_TS + Quantize_Stat    ≈ 0.5 ms
  Inference_Run() (CubeAI INT8)  ≈ 5–30 ms (WCET tracked)
  Vote_Push + Vote_Decide        ≈ 0.05 ms
  xQueueSend                     ≈ 0.01 ms

Thread 3 frame build + DMA TX:
  Frame_Build (CRC32)            ≈ 0.1 ms
  DMA TX (13 bytes @ 115200)     ≈ 1.1 ms

Total latency (steady state, typical):
  From last sample in window → UART frame complete ≈ 15–50 ms
```

### 17.2 CPU Utilisation Breakdown

Typical expected CPU breakdown (heartbeat fields):

| Task | Expected CPU | Notes |
|---|---|---|
| Thread 1 Sensor | 1–3% | I2C+DMA + ring buffer push |
| Thread 2 TinyML | 10–30% | Dominated by CubeAI inference |
| Thread 3 UART TX | 0.5–2% | DMA does the heavy lifting |
| Thread 4 Heartbeat | < 0.5% | Periodic, lightweight |
| Thread 5 BL RX | < 0.1% | Mostly blocked |
| Thread 6 Temperature | < 0.1% | 0.5 Hz, blocking ADC |
| Thread 7 Ultrasonic | < 0.5% | 4 Hz, blocking queue |
| Logger | < 0.1% | Lowest priority |
| **Idle** | **65–88%** | Available for future features |

### 17.3 Ring Buffer Occupancy Analysis

```
Production rate: 100 Hz (Thread 1 pushes 1 sample every 10 ms)
Consumption rate: Thread 2 consumes 25 samples every 250 ms = 100 Hz

Steady-state occupancy:
  Production == Consumption → buffer depth stays constant at ~50
  (one window's worth)

Under load (TinyML inference spike > 250 ms):
  Production continues at 100 Hz
  Buffer grows: 1 sample per ms of overrun
  At 500ms overrun → buffer = 50 + 50 = 100 samples (near capacity)
  At 1280ms overrun → buffer overflows → samples dropped

Monitored by:
  g_stats.rb_max_fill → transmitted in heartbeat
  If approaching RING_BUFFER_CAPACITY (128), increase Thread 2 priority
  or optimise inference WCET
```

---

## 18. Testing and Validation

### 18.1 Test Output Files

The `test/` directory contains captured UART output from integration testing:

| File | Size | Description |
|---|---|---|
| `test/thread1_finished.txt` | 8.5 KB | Thread 1 sensor producer test log |
| `test/thread2_finished.txt` | 12.3 KB | Thread 2 TinyML inference test log |
| `test/uart_dma_irq.txt` | 10.9 KB | UART DMA TX + IRQ RX interaction test |
| `test/uart_test.txt` | 1.2 KB | Basic UART loopback test output |

These files contain raw UART captures decoded by the ESP32 gateway, showing classification results, heartbeat frames, and log events from full system runs.

### 18.2 Replay Mode Validation

To validate the pipeline without hardware:

1. Set `#define REPLAY_MODE 1` in `main.c`
2. Build and flash
3. Connect a serial monitor at 115200 baud
4. Observe the ESP32 parsing frames:
   - `g_replay_done` increments each time the dataset wraps
   - `g_replay_vote_smooth` and `g_replay_vote_rough` should match expected distribution for the recorded road type
   - Heartbeat frames show inference WCET (critical for timing validation)

**Expected replay validation results:**

For a recording made on a smooth road, `g_replay_vote_smooth` should be significantly higher than `g_replay_vote_rough` (>80% smooth votes). For a rough road recording, the inverse.

The `test/thread2_finished.txt` file contains example output from a complete replay run.

---

## 19. Known Issues and Design Notes

### 19.1 REPLAY_MODE Default

The file currently has `#define REPLAY_MODE 1` at the top of `main.c`. This means by default the firmware reads from `replay_data.h` instead of the live IMU. Before deploying to a vehicle, this must be changed to `0`.

### 19.2 VTOR Relocation (commented out)

```c
// *((volatile uint32_t *)0xE000ED08U) = 0x08008000U;
```

This line is commented out in `main.c`. When the application runs as a secondary application after the bootloader (jumping from 0x08000000 to 0x08008000), this line **must be uncommented** so that interrupt vectors are dispatched from the application's vector table at 0x08008000, not the bootloader's at 0x08000000.

Without this, all interrupt handlers (TIM2, DMA, UART, etc.) will jump to the bootloader's ISR stubs instead of the application handlers — causing immediate processor fault on the first timer interrupt.

### 19.3 GPIO Patch (PA10 Mode)

The `fix_gpio.patch` shows a discrepancy: the patch uses `GPIO_INPUT_FLOATING` for PA10 (UART RX), but the current `main.c` uses `GPIO_OUTPUT_PUSH_PULL`. For a UART RX pin in alternate function mode, the actual direction is controlled by the USART1 peripheral, not the GPIO output register. However, using `GPIO_MODE_ALTERNATE` is the correct setting, and the output type field is irrelevant for input-direction alternate-function pins on STM32F401.

### 19.4 FRAME_REQ_MAX_PAYLOAD Size

The `FrameRequest_t.payload[32]` array is sized to fit the heartbeat payload (30 bytes) with 2 bytes headroom. The ultrasonic frame payload is 8 bytes and the classification frame is 6 bytes — both well within 32 bytes. Any new frame type with payload > 32 bytes will require increasing `FRAME_REQ_MAX_PAYLOAD` and verifying the `g_frame_queue` memory budget.

### 19.5 Thread 4 IWDG Alive Frequency

Thread 4 wakes every 1000 ms and calls `IWDG_Thread_SetAlive()`. The IWDG timeout is 3000 ms. This leaves a comfortable 2000 ms margin. However, if Thread 4 is blocked (e.g., by a priority inversion with a higher-priority task holding a resource), the alive flag won't be set and the system will reset after 3 seconds — which is the intended failsafe behaviour.

### 19.6 HCSR04_Trigger Busy-Wait

The 10 µs trigger pulse is implemented with a blocking loop:
```c
for (volatile uint32_t i = 0; i < 1000U; i++) {}
```

At 84 MHz, 1000 iterations ≈ 12 µs. This busy-waits in Thread 7 context for 12 µs per sensor trigger. At 4 Hz (250 ms period, 2 sensors), the total busy-wait time is 24 µs/250 ms = 0.0096% CPU — negligible. If higher frequency triggering is ever needed, replace with a DWT-based precision delay.

### 19.7 Frame Queue Depth

`FRAME_QUEUE_DEPTH = 6` is sized as:
- 4 classification frames (maximum rate: 4 Hz, Thread 3 drains > 4 Hz)
- 1 heartbeat frame (every 5 seconds)
- 1 headroom

At maximum load (4 Hz classification + 2 Hz temperature + 4 Hz ultrasonic + 0.2 Hz heartbeat), Thread 3 must drain 10.2 frames/second. At 115200 baud, the maximum frame rate is approximately:
```
115200 baud / (8 bits × ~15 bytes avg frame) = ~960 frames/second
```
The queue will never overflow under normal conditions.

---

## 20. Glossary

| Term | Definition |
|---|---|
| **ADC** | Analog-to-Digital Converter. Converts analog voltage (e.g. LM35 output) to digital values. |
| **ARR** | Auto-Reload Register. TIM counter resets to zero when it reaches ARR, generating an update event. |
| **CubeAI** | STMicroelectronics' X-CUBE-AI tool that converts TFLite models to C code optimised for STM32. |
| **DMA** | Direct Memory Access. Hardware-driven data transfer between memory and peripherals without CPU involvement. |
| **DMB** | Data Memory Barrier. Cortex-M instruction that ensures all memory accesses before the barrier are visible to all observers before those after the barrier. |
| **DWT** | Data Watchpoint and Trace. Cortex-M debug/trace hardware that includes `CYCCNT`, a 32-bit cycle counter running at CPU clock speed. |
| **FPU** | Floating-Point Unit. Hardware accelerator for single-precision IEEE 754 arithmetic on Cortex-M4. |
| **FreeRTOS** | Real-Time Operating System kernel providing task scheduling, semaphores, queues, and other synchronisation primitives. |
| **HFE** | High-Frequency Energy. A spectral feature computed via FFT, representing energy in the upper frequency bins. |
| **HWM** | High Water Mark. FreeRTOS metric: minimum remaining stack space ever observed since task creation. |
| **I2C** | Inter-Integrated Circuit. Two-wire serial bus (SCL, SDA) used to communicate with the MPU6050. |
| **IQR** | Inter-Quartile Range. Q75 − Q25, a robust measure of signal spread. |
| **ISR** | Interrupt Service Routine. Function called by hardware when an interrupt fires. |
| **IWDG** | Independent Watchdog. Hardware timer driven by LSI oscillator; resets MCU if not periodically fed. |
| **LSI** | Low-Speed Internal oscillator. ~32 kHz RC oscillator used as the IWDG clock source. |
| **MAD** | Mean Absolute Deviation. |
| **MCU** | Microcontroller Unit. |
| **MEMS** | Micro-Electro-Mechanical System. Technology used in the MPU6050 accelerometer and gyroscope. |
| **MPU6050** | InvenSense 6-axis IMU (3-axis accelerometer + 3-axis gyroscope) connected over I2C. |
| **NVIC** | Nested Vectored Interrupt Controller. ARM peripheral managing interrupt priorities and enabling. |
| **OTA** | Over-the-Air update. Firmware update delivered via communication link (UART in this case). |
| **P2P** | Peak-to-Peak. Max(x) − Min(x). |
| **PLL** | Phase-Locked Loop. Frequency multiplier used to derive 84 MHz from 16 MHz HSI. |
| **PSC** | Prescaler. TIM register that divides the input clock before the counter. |
| **RCC** | Reset and Clock Control. STM32 peripheral managing clocks, resets, and bus enables. |
| **RTOS** | Real-Time Operating System. |
| **SCB** | System Control Block. Cortex-M register block including VTOR (vector table offset) and AIRCR (reset control). |
| **SPSC** | Single-Producer Single-Consumer. Lock-free ring buffer design pattern requiring exactly one writer and one reader. |
| **SWD** | Serial Wire Debug. 2-pin debug interface (SWDCLK, SWDIO) supported by ST-Link. |
| **TFLite** | TensorFlow Lite. Lightweight ML inference framework for embedded/mobile devices. |
| **TIM** | Timer/Counter. STM32 peripheral providing timing, PWM, input capture, and output compare. |
| **TinyML** | Machine learning inference on microcontrollers with severe resource constraints. |
| **UART** | Universal Asynchronous Receiver-Transmitter. Serial communication protocol used to link STM32 to ESP32. |
| **VTOR** | Vector Table Offset Register. SCB register specifying the base address of the interrupt vector table. |
| **WCET** | Worst-Case Execution Time. Maximum time a function has ever taken, used for real-time schedulability analysis. |
| **WHO_AM_I** | MPU6050 register (0x75) that returns a fixed ID byte (0x68) used to verify the sensor is connected correctly. |
| **ZP** | Zero-Point. TFLite INT8 quantization parameter: integer value corresponding to floating-point zero. |

---

*Documentation generated from source: `Application/` — STM32F401CC Road Surface Classifier with TinyML (FreeRTOS + CubeAI)*