/*****************************************************************************
 * heartbeat.h — Heartbeat Payload Definition
 *
 * STM32F401CC bare‑metal / FreeRTOS data collection project
 *
 * The heartbeat frame is sent every 1 second via UART (FRAME_TYPE_HEARTBEAT).
 * It carries runtime statistics that allow the ESP32 gateway (or a PC monitor)
 * to observe system health without a debugger.
 *
 * TOTAL PAYLOAD SIZE: 24 bytes
 * FRAME OVERHEAD:      7 bytes (LEN + TYPE + ECU_ID + CRC32)
 * TOTAL ON WIRE:      31 bytes (well within the 255‑byte max)
 *
 * ═══════════════════════════════════════════════════════════════════════
 * WIRE ORDER — ALL MULTI‑BYTE FIELDS ARE BIG‑ENDIAN
 * ═══════════════════════════════════════════════════════════════════════
 *
 * Offset | Size | Field            | Description
 * -------|------|------------------|-----------------------------------------
 *   0    |  4   | uptime_ms        | Milliseconds since boot
 *   4    |  2   | cpu_t1_x100      | Sensor task CPU usage ×100 (e.g. 1234 = 12.34%)
 *   6    |  2   | cpu_t2_x100      | TinyML task CPU usage ×100
 *   8    |  2   | cpu_t3_x100      | UART TX task CPU usage ×100
 *  10    |  2   | cpu_idle_x100    | Idle task CPU usage ×100 (headroom)
 *  12    |  2   | stack_t1_free    | Sensor task stack high‑water mark (words free)
 *  14    |  2   | stack_t2_free    | TinyML task stack high‑water mark
 *  16    |  2   | stack_t3_free    | UART TX task stack high‑water mark
 *  18    |  2   | inf_wcet_us      | Worst‑case inference execution time (µs)
 *  20    |  2   | rb_max_fill      | Ring buffer max fill level (sample count)
 *  22    |  2   | (reserved)       | Reserved — set to 0x0000
 *
 * ═══════════════════════════════════════════════════════════════════════
 * FIELD NOTES
 * ═══════════════════════════════════════════════════════════════════════
 *
 *   uptime_ms:
 *     Derived from xTaskGetTickCount() * portTICK_PERIOD_MS.
 *     On FreeRTOS with 1 ms tick this is simply xTaskGetTickCount().
 *
 *   cpu_*_x100:
 *     CPU utilisation percentages multiplied by 100 to avoid floating point
 *     on the wire. The ESP32 side divides by 100.0 for display.
 *     Example: 12.34% → 1234.
 *
 *   stack_*_free:
 *     Raw return value of uxTaskGetStackHighWaterMark(), in words.
 *     The ESP32 converts to bytes if needed (×4 for Cortex‑M4).
 *
 *   inf_wcet_us:
 *     Longest inference time measured in the TinyML task, in microseconds.
 *
 *   rb_max_fill:
 *     Maximum number of MPU6050 samples ever simultaneously present in
 *     the ring buffer since boot.  A value approaching RING_BUFFER_CAPACITY
 *     indicates the consumer loop is falling behind.
 *
 *   reserved:
 *     Padding to 24 bytes. Set to 0x0000 when packing.
 *****************************************************************************/

#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include "STD_TYPES.h"   /* uint32_t, uint16_t etc. */

/* ══════════════════════════════════════════════════════════════════
 *  Payload structure — exactly 24 bytes, packed
 * ══════════════════════════════════════════════════════════════════ */
typedef struct {
    uint32_t uptime_ms;             /* 4 bytes, offset 0 */
    uint16_t cpu_t1_x100;           /* 2 bytes, offset 4 */
    uint16_t cpu_t2_x100;           /* 2 bytes, offset 6 */
    uint16_t cpu_t3_x100;           /* 2 bytes, offset 8 */
    uint16_t cpu_t6_x100;           /* 2 bytes, offset 10 */
    uint16_t cpu_t7_x100;           /* 2 bytes, offset 12 */
    uint16_t cpu_idle_x100;         /* 2 bytes, offset 14 */
    uint16_t stack_t1_free;         /* 2 bytes, offset 16 */
    uint16_t stack_t2_free;         /* 2 bytes, offset 18 */
    uint16_t stack_t3_free;         /* 2 bytes, offset 20 */
    uint16_t stack_t6_free;         /* 2 bytes, offset 22 */
    uint16_t stack_t7_free;         /* 2 bytes, offset 24 */
    uint16_t inf_wcet_us;           /* 2 bytes, offset 26 */
    uint16_t rb_max_fill;           /* 2 bytes, offset 28 */
} __attribute__((packed)) Heartbeat_Payload_t;

/* Total payload size in bytes */
#define HEARTBEAT_PAYLOAD_SIZE  30U

#endif /* HEARTBEAT_H */
