/* ================================================================
 * I2C1 MASTER - STM32F446RE
 * Bare-metal, register-level, NO HEADER FILES INCLUDED.
 * Board 1 (MASTER)
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
 *   2) Master's OWN button pressed & held:
 *        - Master LED turns OFF immediately (local, no I2C needed).
 *        - A "PRESSED" byte is sent to the Slave over I2C (WRITE).
 *        - The Slave, on receiving it, blinks its LED
 *          BLINK_COUNT_SLAVE times (1 second per blink) then goes
 *          steady OFF - this is the Slave's visible confirmation
 *          that it actually received the byte.
 *   3) Master's OWN button released:
 *        - Master LED turns back ON immediately (local).
 *        - A "RELEASED" byte is sent to the Slave over I2C (WRITE).
 *        - The Slave, on receiving it, turns its LED straight ON
 *          (no blink) - confirmation of the release message.
 *   4) Slave's button pressed & held (detected remotely):
 *        - Since I2C only lets the MASTER start a transaction, the
 *          Slave cannot push this to us on its own. Instead, this
 *          Master continuously polls the Slave's button state with
 *          an I2C READ transaction, and detects the press by
 *          noticing the polled value change.
 *        - On detecting the Slave was just pressed: this Master's
 *          own LED blinks BLINK_COUNT_MASTER times then goes
 *          steady OFF - our confirmation that we received the
 *          Slave's button state correctly.
 *   5) Slave's button released (detected remotely):
 *        - This Master's LED turns straight ON (no blink).
 *
 * WHY THIS DESIGN (single master, both directions of data flow)
 * ----------------------------------------------------------------
 * True I2C only allows ONE device (the master) to generate START
 * conditions. A slave can NEVER push data on its own. To still get
 * data flowing in both directions on a single-master bus, the
 * standard technique - used here - is:
 *   - MASTER -> SLAVE data:  a normal I2C WRITE.
 *   - SLAVE  -> MASTER data: the MASTER periodically issues an I2C
 *     READ, and the SLAVE answers with its current status byte.
 * This is not "fake" slave-to-master communication - the DATA
 * really originates at the Slave (its button pin) and is carried
 * to the Master over the bus; only the bus TRANSACTION is always
 * master-initiated, which is a requirement of the I2C protocol
 * itself, not a limitation of this code.
 *
 * WHY THERE IS NO INFINITE LOOP
 * ----------------------------------------------------------------
 * Every single `while` that waits on a hardware flag is bounded by
 * a decrementing `timeout` counter (I2C_TIMEOUT iterations). If the
 * flag never arrives, the function gives up, records WHY in
 * `i2c_last_error`, and returns 0 (failure) instead of hanging.
 * The main loop never calls anything that can block forever - the
 * only "long" operation is BlinkLED(), which is a fixed, finite
 * number of iterations (bounded by design, not by a flag), so it
 * always completes and returns control to the loop.
 *
 * i2c_last_error VALUES (inspect with the debugger if needed)
 * ----------------------------------------------------------------
 *   0 = last transaction OK
 *   1 = START timeout (SB never set)   -> bus not idle/high. Most
 *       common cause: missing external pull-up resistors on
 *       SCL/SDA, or a wiring/ground fault.
 *   2 = Address NACK (AF flag)         -> bus is fine, but nothing
 *       acknowledged the address. Slave board not running, or
 *       SLAVE_ADDR mismatch between the two files.
 *   3 = ADDR wait timeout, no AF       -> rare; slave stretching
 *       the clock abnormally long.
 *   4 = TXE timeout (write data phase stalled)
 *   5 = BTF timeout (write data phase stalled)
 *   6 = RXNE timeout (read data phase stalled) - this is EXPECTED
 *       to happen occasionally if the Slave is busy inside its own
 *       5-second blink routine when we try to poll it; the next
 *       poll ~20ms later will simply succeed once the Slave is
 *       free again. This is not an error you need to fix.
 *
 * TIMING
 * ----------------------------------------------------------------
 * delay_ms() uses the ARM Cortex-M4 core SysTick timer (present on
 * every Cortex-M chip, not an ST vendor peripheral, so using it
 * does not violate the "no header files" requirement) clocked from
 * the default 16 MHz HSI, giving accurate millisecond delays -
 * BLINK_COUNT_MASTER blinks of 500ms ON + 500ms OFF = exactly
 * 1 second per blink, as required.
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
 * While the Slave is inside its 5-second blink routine it cannot
 * service the bus, so any Master WRITE/READ attempted during that
 * exact window will time out and be silently skipped (no crash, no
 * hang, no retry queue). Given the button is a manual, human-speed
 * input and polling happens every ~20ms, this is not noticeable in
 * normal use, but it is a deliberate simplification worth knowing
 * about if you extend this design.
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
#define I2C_CR1_START   (1U << 8)
#define I2C_CR1_STOP    (1U << 9)
#define I2C_CR1_ACK     (1U << 10)
#define I2C_CR1_SWRST   (1U << 15)

#define I2C_SR1_SB      (1U << 0)
#define I2C_SR1_ADDR    (1U << 1)
#define I2C_SR1_BTF     (1U << 2)
#define I2C_SR1_RXNE    (1U << 6)
#define I2C_SR1_TXE     (1U << 7)
#define I2C_SR1_AF      (1U << 10)

#define SYSTICK_CTRL_ENABLE     (1U << 0)
#define SYSTICK_CTRL_CLKSOURCE  (1U << 2)
#define SYSTICK_CTRL_COUNTFLAG  (1U << 16)

/* ---------------- User-configurable parameters ---------------- */

#define SLAVE_ADDR            0x30U      /* 7-bit I2C address of the slave board */
#define I2C_TIMEOUT           100000U    /* bounded loop iterations before giving up */
#define SYSCLK_HZ             16000000U  /* default HSI, no PLL configured */

/* Y: number of 1-second blinks THIS board's LED performs when it
   detects (via polling) that the Slave's button was pressed. */
#define BLINK_COUNT_MASTER    5U

/* ---------------- Protocol byte values ---------------- */
/* Same encoding used in both directions: this Master writes these
   values to tell the Slave about ITS OWN button, and the Slave
   returns the identical encoding when read, to report its button. */
#define CMD_RELEASED   0x00U
#define CMD_PRESSED    0x01U

volatile unsigned char i2c_last_error = 0;  /* see header comment for meaning */

static void GPIO_Init(void);
static void I2C1_Init(void);
static void I2C1_Recover(void);
static unsigned char I2C1_MasterWrite(unsigned char cmd);
static unsigned char I2C1_MasterRead(unsigned char *out);
static unsigned char Button_Pressed(void);
static void LED_On(void);
static void LED_Off(void);
static void BlinkLED(unsigned char times);
static void delay_ms(unsigned int ms);

int main(void)
{
    GPIO_Init();
    I2C1_Init();

    LED_On();   /* Required boot state */

    unsigned char last_own_button    = 0;
    unsigned char last_remote_button = 0;

    while (1)
    {
        /* ---- 1) Service our OWN button -> our OWN LED + notify Slave ---- */
        unsigned char button_now = Button_Pressed();

        if (button_now && !last_own_button)
        {
            LED_Off();                                  /* local, instant */
            if (!I2C1_MasterWrite(CMD_PRESSED) && i2c_last_error == 1)
            {
                I2C1_Recover();
            }
        }
        else if (!button_now && last_own_button)
        {
            LED_On();                                   /* local, instant */
            if (!I2C1_MasterWrite(CMD_RELEASED) && i2c_last_error == 1)
            {
                I2C1_Recover();
            }
        }
        last_own_button = button_now;

        /* ---- 2) Poll the Slave's button state -> react on our LED ---- */
        unsigned char remote_state;
        if (I2C1_MasterRead(&remote_state))
        {
            if (remote_state == CMD_PRESSED && last_remote_button == 0)
            {
                BlinkLED(BLINK_COUNT_MASTER);
                LED_Off();
                last_remote_button = 1;
            }
            else if (remote_state == CMD_RELEASED && last_remote_button == 1)
            {
                LED_On();
                last_remote_button = 0;
            }
        }
        else if (i2c_last_error == 1)
        {
            I2C1_Recover();
        }
        /* Any other read failure (e.g. error 6 while Slave is blinking)
           is expected occasionally and is simply retried next loop. */

        delay_ms(20);   /* loop pace / simple button debounce */
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

/* ================= I2C1 master init/recovery ================= */

static void I2C1_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    I2C1->CR2   = 16;    /* APB1 = 16 MHz (default HSI) */
    I2C1->CCR   = 80;    /* 100 kHz Standard mode: 16MHz / (2 * 100kHz) */
    I2C1->TRISE = 17;    /* (16MHz * 1000ns / 1e9) + 1 */

    I2C1->CR1 |= I2C_CR1_PE;    /* enable peripheral FIRST ... */
    I2C1->CR1 |= I2C_CR1_ACK;   /* ... THEN enable ACK (order matters) */
}

static void I2C1_Recover(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
    delay_ms(2);
    I2C1_Init();
}

/* ================= I2C1 master transactions ================= */

/* Sends one command byte to the slave (Master -> Slave direction).
   Returns 1 on success, 0 on failure (see i2c_last_error). */
static unsigned char I2C1_MasterWrite(unsigned char cmd)
{
    unsigned int timeout;
    i2c_last_error = 0;

    timeout = I2C_TIMEOUT;
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB))
    {
        if (--timeout == 0) { i2c_last_error = 1; return 0; }
    }

    I2C1->DR = (unsigned char)((SLAVE_ADDR << 1) | 0U);   /* write direction */
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & (I2C_SR1_ADDR | I2C_SR1_AF)))
    {
        if (--timeout == 0) { i2c_last_error = 3; return 0; }
    }
    if (I2C1->SR1 & I2C_SR1_AF)
    {
        I2C1->SR1 &= ~I2C_SR1_AF;
        i2c_last_error = 2;
        return 0;
    }
    (void)I2C1->SR1;
    (void)I2C1->SR2;   /* clears ADDR */

    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_TXE))
    {
        if (--timeout == 0) { i2c_last_error = 4; return 0; }
    }
    I2C1->DR = cmd;

    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_BTF))
    {
        if (--timeout == 0) { i2c_last_error = 5; return 0; }
    }

    I2C1->CR1 |= I2C_CR1_STOP;
    return 1;
}

/* Reads one status byte from the slave (Slave -> Master direction).
   Implements the STM32 reference-manual single-byte master receive
   sequence (clear ACK, clear ADDR, set STOP, THEN wait for the byte)
   to avoid generating a spurious extra clock. Returns 1 on success. */
static unsigned char I2C1_MasterRead(unsigned char *out)
{
    unsigned int timeout;
    i2c_last_error = 0;

    timeout = I2C_TIMEOUT;
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB))
    {
        if (--timeout == 0) { i2c_last_error = 1; return 0; }
    }

    I2C1->DR = (unsigned char)((SLAVE_ADDR << 1) | 1U);   /* read direction */
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & (I2C_SR1_ADDR | I2C_SR1_AF)))
    {
        if (--timeout == 0) { i2c_last_error = 3; return 0; }
    }
    if (I2C1->SR1 & I2C_SR1_AF)
    {
        I2C1->SR1 &= ~I2C_SR1_AF;
        i2c_last_error = 2;
        return 0;
    }

    I2C1->CR1 &= ~I2C_CR1_ACK;   /* NACK the (only) byte we're about to get */
    (void)I2C1->SR1;
    (void)I2C1->SR2;             /* clears ADDR */
    I2C1->CR1 |= I2C_CR1_STOP;

    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_RXNE))
    {
        if (--timeout == 0)
        {
            i2c_last_error = 6;
            I2C1->CR1 |= I2C_CR1_ACK;   /* restore for next attempt */
            return 0;
        }
    }

    *out = (unsigned char)I2C1->DR;
    I2C1->CR1 |= I2C_CR1_ACK;    /* restore ACK for the next reception */
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
