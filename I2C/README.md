# STM32F446RE I2C Master<->Slave Dual-Direction Demo (Bare-metal)

## Required behaviour

| Event                                  | Master LED         | Slave LED           |
|-----------------------------------------|--------------------|----------------------|
| Boot (both boards)                      | ON                 | ON                   |
| Master's own button PRESSED & HELD      | OFF (instant)      | Blinks `BLINK_COUNT_SLAVE` times (1s each), then steady OFF |
| Master's own button RELEASED            | ON (instant)       | ON (no blink, direct) |
| Slave's own button PRESSED & HELD       | Blinks `BLINK_COUNT_MASTER` times (1s each), then steady OFF | OFF (instant) |
| Slave's own button RELEASED             | ON (no blink, direct) | ON (instant)      |

`BLINK_COUNT_SLAVE` and `BLINK_COUNT_MASTER` are `#define`s at the
top of each file (both currently `5U`) - change either independently.

## How data flows both ways over a single-master I2C bus

I2C only allows the fixed *Master* to start a transaction - a slave
can never push data on its own. So:

- **Master -> Slave**: a normal I2C **WRITE**. Sent once, on each
  press/release edge of the Master's own button.
- **Slave -> Master**: the Master continuously performs an I2C
  **READ** (roughly every 20ms) asking "what's your button state?".
  The Slave always keeps its latest button reading ready and answers
  with it. The Master detects the Slave's press/release by noticing
  the polled value change between two reads.

This is the standard, correct way to get genuinely bidirectional
data on a bus where only one device may initiate - the data really
originates at each board's own button, it's just always carried on
a Master-initiated transaction.

## Why there is no infinite loop anymore

The previous version's Slave blocked forever on
`while (!(SR1 & ADDR)) {}` while idle. This version instead checks
`if (SR1 & ADDR)` once per loop pass (non-blocking) and immediately
falls through to service the local button if nothing arrived. Every
remaining wait on a hardware flag (TXE, RXNE, AF, STOPF, SB, ADDR) is
bounded by a decrementing `timeout` counter - if a step doesn't
complete in time, the function gives up, records why in
`i2c_last_error` / `i2c_slave_last_error`, and returns instead of
hanging. The only multi-iteration operation left, `BlinkLED()`, is a
fixed count of iterations (not a wait on a flag), so it always
finishes and returns control.

## Wiring

| Signal | Master pin | Slave pin |
|--------|-----------|-----------|
| SCL    | PB8       | PB8       |
| SDA    | PB9       | PB9       |
| GND    | GND       | GND       |

External **4.7kΩ pull-up resistors** from SCL to 3.3V and SDA to
3.3V are required - Nucleo-F446RE has none built in on PB8/PB9.

## Timing

Both files implement `delay_ms()` using the ARM Cortex-M4 core's
SysTick timer (a standard core peripheral present on every Cortex-M
chip, not an ST vendor peripheral - so this still needs no header
file) clocked from the default 16 MHz HSI. This gives accurate
millisecond delays, so each blink is exactly 500ms ON + 500ms OFF =
1 second, matching the "1 time = 1 second" requirement precisely.

## Error codes (inspect with the debugger if something looks off)

**Master `i2c_last_error`:**
| Value | Meaning |
|---|---|
| 0 | last transaction OK |
| 1 | START never happened - bus not idle. Check pull-ups/wiring. |
| 2 | Address NACKed - Slave not running, or `SLAVE_ADDR` mismatch |
| 3 | ADDR wait timed out without a NACK (rare) |
| 4 / 5 | write data phase stalled |
| 6 | read data phase stalled - **expected occasionally** if the Slave happens to be mid-blink when polled; self-resolves next poll |

**Slave `i2c_slave_last_error`:**
| Value | Meaning |
|---|---|
| 0 | last transaction OK |
| 1 | address matched as receiver, but no data byte arrived |
| 2 | data received, but no STOP arrived |
| 3 | address matched as transmitter, but handoff/AF stalled |

Any non-zero value from a genuine mid-transaction stall triggers an
automatic peripheral re-init, so both boards self-recover without
ever needing a manual reset.

## Files

- `i2c_master.c` - flash to Board 1.
- `i2c_slave.c` - flash to Board 2.
- Both compiled cleanly with `gcc -std=c99 -Wall -Wextra` (as a
  syntax/type sanity check; the register addresses are ordinary
  integer literals so this check is valid even though gcc's target
  isn't ARM).
- Self-contained: no CMSIS/vendor header is included anywhere -
  every register (including SysTick) is memory-mapped by hand at
  the top of each file.

## Known limitation (by design, not a bug)

While a board is inside its 5-second `BlinkLED()` it cannot service
the bus. Any request from the other side arriving in that exact
window will simply time out and be retried automatically on the next
poll (~20ms later for Master reads). Given button presses are
human-speed, this is not noticeable in normal testing.
