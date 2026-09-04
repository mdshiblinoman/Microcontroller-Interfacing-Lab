/* ================================================================
 * I2C1 SLAVE - STM32F446RE
 * Bare-metal, register-level, NO HEADER FILES INCLUDED.
 * Board 2 (SLAVE)
 * ================================================================
 *
 * WHAT THIS BOARD DOES
 * ---------------------
 * Pins:
 *   PA5  = onboard LED (LD2)
 *   PC13 = onboard user button (B1), active LOW
 *   PB8  = I2C1_SCL
 *   PB9  = I2C1_SDA
 *
 * Required behaviour:
 *   1) At boot: LED is ON.
 *   2) When this Slave receives a "PRESSED" byte from the Master
 *      (sent when the Master's own button is pressed):
 *        - LED blinks BLINK_COUNT_SLAVE times (1 second per blink),
 *          then goes steady OFF. This blink IS the visible proof
 *          that the byte was received correctly.
 *   3) When this Slave receives a "RELEASED" byte from the Master:
 *        - LED turns straight ON (no blink).
 *   4) This Slave's OWN button pressed & held:
 *        - LED turns OFF immediately (local, no I2C needed).
 *        - The current button state is exposed so the Master can
 *          pick it up next time it polls us (see below).
 *   5) This Slave's OWN button released:
 *        - LED turns back ON immediately (local).
 *
 * WHY THIS DESIGN (single master, both directions of data flow)
 * ----------------------------------------------------------------
 * A slave can never generate a START condition - only the Master
 * can begin a transaction. So this Slave does not "send" its button
 * state proactively; instead it keeps its latest button reading in
 * `own_button_state`, and answers with that byte whenever the
 * Master performs an I2C READ. The Master polls continuously, so
 * from a user's point of view the data still flows Slave -> Master,
 * it is simply carried on a Master-initiated transaction, which is
 * how the I2C protocol itself works (not a limitation of this code).
 *
 * WHY THERE IS NO INFINITE LOOP (this is the key fix from before)
 * ----------------------------------------------------------------
 * The main loop NEVER blocks waiting for the bus. Instead of the
 * classic `while (!(SR1 & ADDR)) { }` that blocks forever while
 * idle, this version does a single, non-blocking `if (SR1 & ADDR)`
 * check every pass through the loop. If the Master hasn't addressed
 * us yet, we simply skip the I2C section this iteration and go on
 * to service the local button - so the button stays responsive at
 * all times, and there is no line of code that can wait forever.
 * Every remaining `while` inside the I2C handling (TXE, RXNE, AF,
 * STOPF) is additionally bounded by a decrementing `timeout`
 * counter, so even a partial/interrupted transaction cannot hang.
 * The only "long" operation is BlinkLED(), a fixed, finite number
 * of iterations - not a wait on a flag - so it always completes.
 *
 * i2c_slave_last_error VALUES (inspect with the debugger if needed)
 * ----------------------------------------------------------------
 *   0 = last transaction OK
 *   1 = RXNE timeout   (address matched as receiver, but no data
 *       byte ever arrived)
 *   2 = STOPF timeout  (data received, but no STOP arrived)
 *   3 = TXE/AF timeout (address matched as transmitter, but we
 *       could not hand off our status byte, or the Master never
 *       NACKed to end the read)
 * A non-zero value here after a real transaction stall (not just
 * "idle, nothing happened yet") triggers an automatic peripheral
 * re-init so the Slave always recovers on its own.
 *
 * TIMING
 * ----------------------------------------------------------------
 * delay_ms() uses the ARM Cortex-M4 core SysTick timer (present on
 * every Cortex-M chip, not an ST vendor peripheral) clocked from the
 * default 16 MHz HSI, giving accurate millisecond delays -
 * BLINK_COUNT_SLAVE blinks of 500ms ON + 500ms OFF = exactly 1
 * second per blink, as required.
 *
 * WIRING
 * ----------------------------------------------------------------
 *   - Tie both boards' GND together.
 *   - PB8-PB8 (SCL), PB9-PB9 (SDA) between boards.
 *   - External 4.7k pull-up resistors from SCL and SDA to 3.3V.
 *     (Nucleo-F446RE has no onboard pull-ups on PB8/PB9.)
 *
 * KNOWN LIMITATION (documented, not a bug)
 * ----------------------------------------------------------------
 * While this Slave is inside its 5-second BlinkLED() it cannot
 * service the bus. Any Master request arriving during that exact
 * window will simply time out on the Master's side and be retried
 * on its next poll (~20ms later) - see i2c_master.c for how this is
 * handled gracefully there.
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
    volatile unsigned int CR1;
    volatile unsigned int CR2;
    volatile unsigned int OAR1;
    volatile unsigned int OAR2;
    volatile unsigned int DR;
    volatile unsigned int SR1;
    volatile unsigned int SR2;
    volatile unsigned int CCR;
    volatile unsigned int TRISE;
    volatile unsigned int FLTR;
} I2C_TypeDef;

typedef struct {
    volatile unsigned int CTRL;
    volatile unsigned int LOAD;
    volatile unsigned int VAL;
    volatile unsigned int CALIB;
} SysTick_TypeDef;

#define RCC_BASE      0x40023800U
#define GPIOA_BASE    0x40020000U
#define GPIOB_BASE    0x40020400U
#define GPIOC_BASE    0x40020800U
#define I2C1_BASE     0x40005400U
#define SYSTICK_BASE  0xE000E010U

#define RCC      ((RCC_TypeDef     *)RCC_BASE)
#define GPIOA    ((GPIO_TypeDef    *)GPIOA_BASE)
#define GPIOB    ((GPIO_TypeDef    *)GPIOB_BASE)
#define GPIOC    ((GPIO_TypeDef    *)GPIOC_BASE)
#define I2C1     ((I2C_TypeDef     *)I2C1_BASE)
#define SYSTICK  ((SysTick_TypeDef *)SYSTICK_BASE)

/* ---------------- Bit definitions used ---------------- */

#define RCC_AHB1ENR_GPIOAEN   (1U << 0)
#define RCC_AHB1ENR_GPIOBEN   (1U << 1)
#define RCC_AHB1ENR_GPIOCEN   (1U << 2)
#define RCC_APB1ENR_I2C1EN    (1U << 21)

#define I2C_CR1_PE      (1U << 0)
#define I2C_CR1_ACK     (1U << 10)
#define I2C_CR1_SWRST   (1U << 15)

#define I2C_SR1_ADDR    (1U << 1)
#define I2C_SR1_STOPF   (1U << 4)
#define I2C_SR1_RXNE    (1U << 6)
#define I2C_SR1_TXE     (1U << 7)
#define I2C_SR1_AF      (1U << 10)

#define I2C_SR2_TRA     (1U << 2)   /* 1 = we are transmitter (Master READ) */

#define SYSTICK_CTRL_ENABLE     (1U << 0)
#define SYSTICK_CTRL_CLKSOURCE  (1U << 2)
#define SYSTICK_CTRL_COUNTFLAG  (1U << 16)

/* ---------------- User-configurable parameters ---------------- */

#define SLAVE_ADDR            0x30U      /* must match i2c_master.c */
#define I2C_TIMEOUT           100000U    /* bounded loop iterations before giving up */
#define SYSCLK_HZ             16000000U  /* default HSI, no PLL configured */

/* X: number of 1-second blinks THIS board's LED performs when it
   receives a "PRESSED" byte from the Master. */
#define BLINK_COUNT_SLAVE     5U

/* ---------------- Protocol byte values ---------------- */
#define CMD_RELEASED   0x00U
#define CMD_PRESSED    0x01U

volatile unsigned char i2c_slave_last_error = 0;  /* see header comment */

static void GPIO_Init(void);
static void I2C1_Slave_Init(void);
static unsigned char Button_Pressed(void);
static void LED_On(void);
static void LED_Off(void);
static void BlinkLED(unsigned char times);
static void delay_ms(unsigned int ms);

int main(void)
{
    GPIO_Init();
    I2C1_Slave_Init();

    LED_On();   /* Required boot state */

    unsigned char last_own_button  = 0;
    unsigned char own_button_state = 0;   /* reported to Master on READ */

    while (1)
    {
        /* ---- 1) Service our OWN button -> our OWN LED (local, instant) ---- */
        unsigned char button_now = Button_Pressed();

        if (button_now && !last_own_button)
        {
            LED_Off();
        }
        else if (!button_now && last_own_button)
        {
            LED_On();
        }
        last_own_button   = button_now;
        own_button_state  = button_now;   /* always kept current for polling */

        /* ---- 2) Non-blocking check: has the Master addressed us? ---- */
        if (I2C1->SR1 & I2C_SR1_ADDR)
        {
            (void)I2C1->SR1;
            unsigned int sr2 = I2C1->SR2;   /* reading SR2 clears ADDR; bit2 = TRA */

            if (sr2 & I2C_SR2_TRA)
            {
                /* --- Master is READING us: send our button status --- */
                unsigned char ok = 1;
                unsigned int timeout = I2C_TIMEOUT;

                while (!(I2C1->SR1 & I2C_SR1_TXE))
                {
                    if (--timeout == 0) { ok = 0; break; }
                }

                if (ok)
                {
                    I2C1->DR = own_button_state;

                    /* Master reads exactly 1 byte then NACKs -> AF sets.
                       Per RM0390, STOPF is NOT set in this case. */
                    timeout = I2C_TIMEOUT;
                    while (!(I2C1->SR1 & I2C_SR1_AF))
                    {
                        if (--timeout == 0) { ok = 0; break; }
                    }
                    if (I2C1->SR1 & I2C_SR1_AF)
                    {
                        I2C1->SR1 &= ~I2C_SR1_AF;
                    }
                }

                i2c_slave_last_error = ok ? 0 : 3;
                if (!ok) I2C1_Slave_Init();   /* self-recover, stay non-blocking */
            }
            else
            {
                /* --- Master is WRITING to us: receive a command byte --- */
                unsigned char ok = 1;
                unsigned char cmd = 0;
                unsigned int timeout = I2C_TIMEOUT;

                while (!(I2C1->SR1 & I2C_SR1_RXNE))
                {
                    if (--timeout == 0) { ok = 0; break; }
                }

                if (ok)
                {
                    cmd = (unsigned char)I2C1->DR;

                    timeout = I2C_TIMEOUT;
                    while (!(I2C1->SR1 & I2C_SR1_STOPF))
                    {
                        if (--timeout == 0) { ok = 0; break; }
                    }
                    if (ok)
                    {
                        (void)I2C1->SR1;
                        I2C1->CR1 |= I2C_CR1_PE;   /* clears STOPF */
                    }
                }

                if (ok)
                {
                    i2c_slave_last_error = 0;
                    if (cmd == CMD_PRESSED)
                    {
                        BlinkLED(BLINK_COUNT_SLAVE);
                        LED_Off();
                    }
                    else if (cmd == CMD_RELEASED)
                    {
                        LED_On();
                    }
                }
                else
                {
                    i2c_slave_last_error = 1;
                    I2C1_Slave_Init();   /* self-recover, stay non-blocking */
                }
            }
        }

        delay_ms(10);   /* loop pace / simple button debounce */
    }
}

/* ================= GPIO / clock setup ================= */

static void GPIO_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

    /* PA5 -> output (LED) */
    GPIOA->MODER   &= ~(3U << (5 * 2));
    GPIOA->MODER   |=  (1U << (5 * 2));
    GPIOA->OTYPER  &= ~(1U << 5);
    GPIOA->OSPEEDR |=  (3U << (5 * 2));

    /* PC13 -> input (User Button B1) */
    GPIOC->MODER &= ~(3U << (13 * 2));
    GPIOC->PUPDR &= ~(3U << (13 * 2));

    /* PB8 (SCL), PB9 (SDA) -> AF4, Open-Drain, Pull-up */
    GPIOB->MODER &= ~((3U << (8 * 2)) | (3U << (9 * 2)));
    GPIOB->MODER |=  ((2U << (8 * 2)) | (2U << (9 * 2)));

    GPIOB->OTYPER |= (1U << 8) | (1U << 9);

    GPIOB->OSPEEDR |= (3U << (8 * 2)) | (3U << (9 * 2));

    GPIOB->PUPDR &= ~((3U << (8 * 2)) | (3U << (9 * 2)));
    GPIOB->PUPDR |=  ((1U << (8 * 2)) | (1U << (9 * 2)));

    GPIOB->AFR[1] &= ~((0xFU << ((8 - 8) * 4)) | (0xFU << ((9 - 8) * 4)));
    GPIOB->AFR[1] |=  ((4U   << ((8 - 8) * 4)) | (4U   << ((9 - 8) * 4)));
}

/* ================= I2C1 slave init ================= */

static void I2C1_Slave_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    I2C1->CR2 = 16;   /* APB1 = 16 MHz (default HSI) */

    I2C1->OAR1  = 0;
    I2C1->OAR1 |= (1U << 14);          /* must be kept at 1 by software */
    I2C1->OAR1 |= (SLAVE_ADDR << 1);   /* 7-bit own address */

    I2C1->CCR   = 80;
    I2C1->TRISE = 17;

    I2C1->CR1 |= I2C_CR1_PE;    /* enable peripheral FIRST ... */
    I2C1->CR1 |= I2C_CR1_ACK;   /* ... THEN enable ACK (order matters) */
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

/* Blinks the LED `times` times, 500ms ON + 500ms OFF each = exactly
   1 second per blink. Bounded loop - always finishes and returns. */
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

/* Millisecond-accurate blocking delay using the Cortex-M4 core
   SysTick timer (not a vendor peripheral - no header needed). */
static void delay_ms(unsigned int ms)
{
    SYSTICK->LOAD = (SYSCLK_HZ / 1000U) - 1U;
    SYSTICK->VAL  = 0U;
    SYSTICK->CTRL = SYSTICK_CTRL_ENABLE | SYSTICK_CTRL_CLKSOURCE;

    unsigned int i;
    for (i = 0; i < ms; i++)
    {
        while (!(SYSTICK->CTRL & SYSTICK_CTRL_COUNTFLAG)) { }
    }

    SYSTICK->CTRL = 0U;
}
