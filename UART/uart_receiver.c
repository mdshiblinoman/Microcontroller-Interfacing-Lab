/* ================================================================
 * USART1 BOARD B ("RECEIVER") - STM32F446RE
 * Bare-metal, register-level, NO HEADER FILES INCLUDED.
 * ================================================================
 *
 * IMPORTANT NOTE ON NAMING
 * ----------------------------------------------------------------
 * This file is called the "Receiver" only to match the naming
 * used in the earlier I2C version of this lab. UART (unlike I2C)
 * is full-duplex: every board has its own independent TX wire and
 * RX wire, so BOTH boards transmit AND receive at the same time,
 * with no master/slave relationship at all. This file sends ITS
 * OWN button state to the other board, and simultaneously listens
 * for the other board's button state - exactly like uart_transmitter.c
 * does for the other board. The two files are functionally
 * symmetric; only the file name is kept from the earlier lab for
 * continuity.
 *
 * WHAT THIS BOARD DOES
 * ----------------------------------------------------------------
 * Pins:
 *   PA5  = onboard LED (LD2)
 *   PC13 = onboard user button (B1), active LOW
 *   PA9  = USART1_TX
 *   PA10 = USART1_RX
 *
 * Required behaviour:
 *   1) At boot: LED is ON.
 *   2) This board's OWN button pressed & held:
 *        - This board's LED turns OFF immediately (local).
 *        - A "PRESSED" byte (0x01) is transmitted over UART.
 *   3) This board's OWN button released:
 *        - This board's LED turns back ON immediately (local).
 *        - A "RELEASED" byte (0x00) is transmitted over UART.
 *   4) A "PRESSED" byte is received from the other board:
 *        - This board's LED blinks BLINK_COUNT times (1 second per
 *          blink), then goes steady OFF - visible confirmation the
 *          byte was received correctly.
 *   5) A "RELEASED" byte is received from the other board:
 *        - This board's LED turns straight ON (no blink).
 *
 * DESIGN - DELIBERATELY SIMPLE, ON PURPOSE
 * ----------------------------------------------------------------
 * The main loop does exactly two things, every pass, nothing else:
 *   1. Read the button pin ONCE. Compare to last time. If it just
 *      changed, act immediately (LED + one UART byte) and remember
 *      the new state.
 *   2. Check the UART receive flag ONCE (a plain `if`, not a
 *      `while`). If a byte is waiting, handle it; otherwise move on.
 * There is deliberately NO "confirm the press is held for N ms"
 * logic and NO "wait for release" loop. Nested waiting loops like
 * that make a bug in a lower-level function (e.g. delay_ms) look
 * like the outer check itself is "stuck", when really it's a
 * called function beneath it that never returned. Keeping the loop
 * this flat makes every line easy to reason about and guarantees
 * nothing here can hang.
 *
 * WHY THERE IS NO INFINITE LOOP - checked line by line
 * ----------------------------------------------------------------
 *   - The button check is a plain `if`, not a loop. Cannot hang.
 *   - The receive check is a plain `if` on SR & RXNE, not a
 *     `while`. It does not wait for a byte that isn't there - it
 *     just skips this pass and tries again next loop. Cannot hang.
 *   - UART_SendByte() waits on TXE and TC, but each wait is guarded
 *     by a decrementing `timeout` counter (UART_TIMEOUT iterations).
 *     If the hardware flag never arrives, the function gives up and
 *     returns 0 instead of looping forever.
 *   - BlinkLED() runs a fixed, known number of iterations - bounded
 *     by design, not by waiting on any flag.
 *   - delay_ms() is two `for` loops with fixed bounds (ms, then
 *     DELAY_LOOP_PER_MS). It contains no `while` and no flag wait
 *     at all - it is a pure, bounded busy-wait.
 * Every function callable from main() is therefore guaranteed to
 * return in bounded time.
 *
 * WHAT WENT WRONG IN THE EARLIER VERSION (read this if you're
 * comparing against a version you already have)
 * ----------------------------------------------------------------
 * An earlier revision's delay_ms() used the Cortex-M4 SysTick core
 * timer, polling its COUNTFLAG bit in a `while`. That hung on real
 * hardware in this lab setup and was never actually verified working
 * before being handed over - it only passed a syntax check on a
 * desktop compiler, which cannot catch a hardware timing bug. Since
 * delay_ms() was called every single pass of the main loop (at the
 * bottom, for pacing/debounce), that one hang froze the entire
 * program on its very first loop iteration - which is exactly why
 * NOTHING happened on either board even though the button-reading
 * code above it was completely correct. This version removes the
 * SysTick dependency entirely and replaces it with a plain
 * calibrated busy-wait loop - the same simple technique the very
 * first, confirmed-working version of this lab (the I2C button
 * debounce delay) already used successfully on this exact hardware.
 * It is less precise than a hardware timer, but it depends on
 * nothing but the CPU executing instructions, so it cannot get
 * stuck waiting on a peripheral flag that never arrives.
 *
 * CALIBRATING THE TIMING
 * ----------------------------------------------------------------
 * DELAY_LOOP_PER_MS below is an approximate constant, not a
 * cycle-accurate one. To check/tune it: press-and-hold this board's
 * button so the OTHER board blinks 5 times, and time it with a
 * stopwatch.
 *   - Noticeably MORE than 5 seconds -> DECREASE DELAY_LOOP_PER_MS.
 *   - Noticeably LESS than 5 seconds -> INCREASE DELAY_LOOP_PER_MS.
 *   - Example: if 5 blinks actually took 8 seconds, multiply
 *     DELAY_LOOP_PER_MS by (5.0/8.0) to correct it.
 * Change the value identically in BOTH files if you want blinks on
 * both boards to run at the same speed.
 *
 * uart_last_error VALUES (inspect with the debugger if needed)
 * ----------------------------------------------------------------
 *   0 = last transmit OK
 *   1 = TXE timeout - transmit register never emptied. Almost
 *       always means USART1 clock/GPIO AF setup is wrong, OR the
 *       two boards' BRR values don't match (mismatched baud rate
 *       can, in rare cases, still let SB-equivalent flags progress
 *       incorrectly - re-check BRR first).
 *   2 = TC timeout - byte never finished shifting out. Same causes
 *       as above.
 * A value of 0 after every button press confirms the send path is
 * healthy; if the OTHER board never reacts despite this, the issue
 * is on the receiving board or the wiring between TX and RX.
 *
 * BAUD RATE
 * ----------------------------------------------------------------
 * 9600 baud, both boards, assuming default 16 MHz HSI (no PLL):
 *     USARTDIV = 16,000,000 / (16 * 9600) = 104.1667
 *     Mantissa = 104, Fraction = round(0.1667 * 16) = 3
 *     BRR = (104 << 4) | 3 = 0x683
 * Both boards MUST use the identical BRR value.
 *
 * WIRING
 * ----------------------------------------------------------------
 *   Board A PA9  (TX) -----------> Board B PA10 (RX)
 *   Board A PA10 (RX) <----------- Board B PA9  (TX)
 *   Board A GND       <----------> Board B GND
 * No pull-up resistors needed - UART is push-pull, unlike I2C.
 * ================================================================ */

/* ---------------- Manual peripheral register map ---------------- */

typedef struct {
    volatile unsigned int MODER;
    volatile unsigned int OTYPER;
    volatile unsigned int OSPEEDR;
    volatile unsigned int PUPDR;
    volatile unsigned int IDR;
    volatile unsigned int ODR;
    volatile unsigned int BSRR;
    volatile unsigned int LCKR;
    volatile unsigned int AFR[2];
} GPIO_TypeDef;

typedef struct {
    volatile unsigned int CR;
    volatile unsigned int PLLCFGR;
    volatile unsigned int CFGR;
    volatile unsigned int CIR;
    volatile unsigned int AHB1RSTR;
    volatile unsigned int AHB2RSTR;
    volatile unsigned int AHB3RSTR;
    unsigned int RESERVED0;
    volatile unsigned int APB1RSTR;
    volatile unsigned int APB2RSTR;
    unsigned int RESERVED1[2];
    volatile unsigned int AHB1ENR;
    volatile unsigned int AHB2ENR;
    volatile unsigned int AHB3ENR;
    unsigned int RESERVED2;
    volatile unsigned int APB1ENR;
    volatile unsigned int APB2ENR;
} RCC_TypeDef;

typedef struct {
    volatile unsigned int SR;
    volatile unsigned int DR;
    volatile unsigned int BRR;
    volatile unsigned int CR1;
    volatile unsigned int CR2;
    volatile unsigned int CR3;
    volatile unsigned int GTPR;
} USART_TypeDef;

#define RCC_BASE      0x40023800U
#define GPIOA_BASE    0x40020000U
#define GPIOC_BASE    0x40020800U
#define USART1_BASE   0x40011000U

#define RCC     ((RCC_TypeDef   *)RCC_BASE)
#define GPIOA   ((GPIO_TypeDef  *)GPIOA_BASE)
#define GPIOC   ((GPIO_TypeDef  *)GPIOC_BASE)
#define USART1  ((USART_TypeDef *)USART1_BASE)

/* ---------------- Bit definitions used ---------------- */

#define RCC_AHB1ENR_GPIOAEN   (1U << 0)
#define RCC_AHB1ENR_GPIOCEN   (1U << 2)
#define RCC_APB2ENR_USART1EN  (1U << 4)

#define USART_CR1_RE   (1U << 2)
#define USART_CR1_TE   (1U << 3)
#define USART_CR1_UE   (1U << 13)

#define USART_SR_ORE   (1U << 3)
#define USART_SR_RXNE  (1U << 5)
#define USART_SR_TC    (1U << 6)
#define USART_SR_TXE   (1U << 7)

/* ---------------- User-configurable parameters ---------------- */

#define UART_BRR_VALUE       0x683U   /* 9600 baud @ 16 MHz PCLK2, see header */
#define UART_TIMEOUT         100000U  /* bounded loop iterations before giving up */
#define DELAY_LOOP_PER_MS    4000U    /* calibration constant - see header comment */

/* Number of 1-second blinks THIS board's LED performs when it
   receives a "PRESSED" byte from the other board. */
#define BLINK_COUNT      5U

/* ---------------- Protocol byte values ---------------- */
#define CMD_RELEASED   0x00U
#define CMD_PRESSED    0x01U

volatile unsigned char uart_last_error = 0;   /* see header comment */

static void GPIO_Init(void);
static void UART1_Init(void);
static unsigned char UART_SendByte(unsigned char data);
static unsigned char Button_Pressed(void);
static void LED_On(void);
static void LED_Off(void);
static void BlinkLED(unsigned char times);
static void delay_ms(unsigned int ms);

int main(void)
{
    GPIO_Init();
    UART1_Init();

    LED_On();   /* Required boot state */

    unsigned char last_own_button = 0;

    while (1)
    {
        /* ---- 1) Service our OWN button -> our OWN LED + transmit ---- */
        unsigned char button_now = Button_Pressed();

        if (button_now && !last_own_button)
        {
            LED_Off();                       /* local, instant */
            UART_SendByte(CMD_PRESSED);
        }
        else if (!button_now && last_own_button)
        {
            LED_On();                        /* local, instant */
            UART_SendByte(CMD_RELEASED);
        }
        last_own_button = button_now;

        /* ---- 2) Non-blocking check: did a byte arrive from the peer? ---- */
        unsigned int sr = USART1->SR;

        if (sr & USART_SR_RXNE)
        {
            unsigned char rx = (unsigned char)USART1->DR;   /* reading DR clears RXNE (and ORE) */

            if (rx == CMD_PRESSED)
            {
                BlinkLED(BLINK_COUNT);
                LED_Off();
            }
            else if (rx == CMD_RELEASED)
            {
                LED_On();
            }
        }
        else if (sr & USART_SR_ORE)
        {
            /* Overrun with no unread byte pending: clear by reading
               SR (already done above) then DR, per reference manual. */
            (void)USART1->DR;
        }

        delay_ms(10);   /* loop pace / simple button debounce */
    }
}

/* ================= GPIO / clock setup ================= */

static void GPIO_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;

    /* PA5 -> output (LED) */
    GPIOA->MODER   &= ~(3U << (5 * 2));
    GPIOA->MODER   |=  (1U << (5 * 2));
    GPIOA->OTYPER  &= ~(1U << 5);
    GPIOA->OSPEEDR |=  (3U << (5 * 2));

    /* PC13 -> input (User Button B1) */
    GPIOC->MODER &= ~(3U << (13 * 2));
    GPIOC->PUPDR &= ~(3U << (13 * 2));

    /* PA9 (TX), PA10 (RX) -> AF7 (USART1), push-pull, high speed */
    GPIOA->MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA->MODER |=  ((2U << (9 * 2)) | (2U << (10 * 2)));

    GPIOA->OTYPER &= ~((1U << 9) | (1U << 10));   /* push-pull (UART default) */

    GPIOA->OSPEEDR |= (3U << (9 * 2)) | (3U << (10 * 2));

    GPIOA->PUPDR &= ~((3U << (9 * 2)) | (3U << (10 * 2)));

    GPIOA->AFR[1] &= ~((0xFU << ((9  - 8) * 4)) | (0xFU << ((10 - 8) * 4)));
    GPIOA->AFR[1] |=  ((7U   << ((9  - 8) * 4)) | (7U   << ((10 - 8) * 4))); /* AF7 */
}

/* ================= USART1 init ================= */

static void UART1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    USART1->CR2 = 0;   /* 1 stop bit (default) */
    USART1->CR3 = 0;   /* no flow control, no DMA */
    USART1->BRR = UART_BRR_VALUE;

    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    /* 8 data bits, no parity (M=0, PCE=0 - reset defaults). */
}

/* ================= UART transmit ================= */

/* Sends one byte. Returns 1 on success, 0 on timeout (see
   uart_last_error). Both waits are bounded - cannot hang. */
static unsigned char UART_SendByte(unsigned char data)
{
    unsigned int timeout;

    timeout = UART_TIMEOUT;
    while (!(USART1->SR & USART_SR_TXE))
    {
        if (--timeout == 0) { uart_last_error = 1; return 0; }
    }
    USART1->DR = data;

    timeout = UART_TIMEOUT;
    while (!(USART1->SR & USART_SR_TC))
    {
        if (--timeout == 0) { uart_last_error = 2; return 0; }
    }

    uart_last_error = 0;
    return 1;
}

/* ================= Small helpers ================= */

static unsigned char Button_Pressed(void)
{
    return ((GPIOC->IDR & (1U << 13)) == 0U);   /* B1 is active LOW */
}

static void LED_On(void)
{
    GPIOA->BSRR = (1U << 5);
}

static void LED_Off(void)
{
    GPIOA->BSRR = (1U << (5 + 16));
}

/* Blinks the LED `times` times, 500ms ON + 500ms OFF each = ~1
   second per blink. Fixed iteration count - always finishes. */
static void BlinkLED(unsigned char times)
{
    unsigned char i;
    for (i = 0; i < times; i++)
    {
        LED_On();
        delay_ms(500);
        LED_Off();
        delay_ms(500);
    }
}

/* Calibrated busy-wait delay - see header comment for tuning.
   Two fixed-bound for-loops, no `while`, no flag wait - cannot
   hang under any circumstance. */
static void delay_ms(unsigned int ms)
{
    volatile unsigned int i, j;
    for (i = 0; i < ms; i++)
    {
        for (j = 0; j < DELAY_LOOP_PER_MS; j++) { }
    }
}
