#include <stdint.h>

/*==========================================================
                    REGISTER DECLARATIONS
==========================================================*/

/*---------------- RCC ----------------*/
#define RCC_BASE        0x40023800

#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x40))


/*---------------- GPIOA ----------------*/
#define GPIOA_BASE      0x40020000

#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_OTYPER    (*(volatile uint32_t *)(GPIOA_BASE + 0x04))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14))


/*---------------- GPIOB ----------------*/
#define GPIOB_BASE      0x40020400

#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_OTYPER    (*(volatile uint32_t *)(GPIOB_BASE + 0x04))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0C))

#define GPIOB_AFRL      (*(volatile uint32_t *)(GPIOB_BASE + 0x20))
#define GPIOB_AFRH      (*(volatile uint32_t *)(GPIOB_BASE + 0x24))


/*==========================================================
                        I2C1
                    MASTER
==========================================================*/

#define I2C1_BASE       0x40005400

#define I2C1_CR1        (*(volatile uint32_t *)(I2C1_BASE + 0x00))
#define I2C1_CR2        (*(volatile uint32_t *)(I2C1_BASE + 0x04))
#define I2C1_OAR1       (*(volatile uint32_t *)(I2C1_BASE + 0x08))
#define I2C1_DR         (*(volatile uint32_t *)(I2C1_BASE + 0x10))
#define I2C1_SR1        (*(volatile uint32_t *)(I2C1_BASE + 0x14))
#define I2C1_SR2        (*(volatile uint32_t *)(I2C1_BASE + 0x18))
#define I2C1_CCR        (*(volatile uint32_t *)(I2C1_BASE + 0x1C))
#define I2C1_TRISE      (*(volatile uint32_t *)(I2C1_BASE + 0x20))


/*==========================================================
                        I2C2
                     SLAVE
==========================================================*/

#define I2C2_BASE       0x40005800

#define I2C2_CR1        (*(volatile uint32_t *)(I2C2_BASE + 0x00))
#define I2C2_CR2        (*(volatile uint32_t *)(I2C2_BASE + 0x04))
#define I2C2_OAR1       (*(volatile uint32_t *)(I2C2_BASE + 0x08))
#define I2C2_DR         (*(volatile uint32_t *)(I2C2_BASE + 0x10))
#define I2C2_SR1        (*(volatile uint32_t *)(I2C2_BASE + 0x14))
#define I2C2_SR2        (*(volatile uint32_t *)(I2C2_BASE + 0x18))


/*==========================================================
                        TIM2
==========================================================*/

#define TIM2_BASE       0x40000000

#define TIM2_PSC        (*(volatile uint32_t *)(TIM2_BASE + 0x28))
#define TIM2_ARR        (*(volatile uint32_t *)(TIM2_BASE + 0x2C))
#define TIM2_CR1        (*(volatile uint32_t *)(TIM2_BASE + 0x00))
#define TIM2_SR         (*(volatile uint32_t *)(TIM2_BASE + 0x10))


/*==========================================================
                    FUNCTION DECLARATIONS
==========================================================*/

void GPIO_I2C_Init(void);

void I2C1_Master_Init(void);
void I2C2_Slave_Init(void);

void I2C1_Master_Send(uint8_t saddr,
                      uint8_t n,
                      char *str);

void I2C2_Slave_Read(uint8_t n,
                     char *str);

void Delay(uint32_t ms);


/*==========================================================
                         MAIN
==========================================================*/

int main(void)
{
    char received[7];

    /* Initialize GPIO */
    GPIO_I2C_Init();

    /* Initialize I2C1 as MASTER */
    I2C1_Master_Init();

    /* Initialize I2C2 as SLAVE */
    I2C2_Slave_Init();


    while (1)
    {
        /*
         * Master sends 6 bytes
         */
        I2C1_Master_Send(0x12,
                         6,
                         "Shakil");


        /*
         * Slave receives 6 bytes
         */
        I2C2_Slave_Read(6,
                        received);


        /*
         * Null terminate the received string
         */
        received[6] = '\0';


        /*
         * Wait 1 second
         */
        Delay(1000);
    }
}


/*==========================================================
                    GPIO INITIALIZATION
==========================================================*/

void GPIO_I2C_Init(void)
{
    /*
     * Enable GPIOB clock
     */
    RCC_AHB1ENR |= (1 << 1);


    /*======================================================
                        I2C1
                  PB8 = SCL
                  PB9 = SDA
              Alternate Function 4
    ======================================================*/

    /*
     * PB8 -> Alternate Function
     *
     * PB8 MODER bits = 17:16
     * 10 = Alternate Function
     */

    GPIOB_MODER &= ~(3 << 16);
    GPIOB_MODER |=  (2 << 16);


    /*
     * PB9 -> Alternate Function
     */

    GPIOB_MODER &= ~(3 << 18);
    GPIOB_MODER |=  (2 << 18);


    /*
     * Open Drain
     */

    GPIOB_OTYPER |= (1 << 8);
    GPIOB_OTYPER |= (1 << 9);


    /*
     * Pull-up
     */

    GPIOB_PUPDR &= ~(3 << 16);
    GPIOB_PUPDR |=  (1 << 16);

    GPIOB_PUPDR &= ~(3 << 18);
    GPIOB_PUPDR |=  (1 << 18);


    /*
     * PB8/PB9 -> AF4
     */

    GPIOB_AFRH &= ~(0xFF << 0);

    GPIOB_AFRH |= (4 << 0);   // PB8 AF4
    GPIOB_AFRH |= (4 << 4);   // PB9 AF4


    /*======================================================
                        I2C2
                  PB10 = SCL
                  PB11 = SDA
              Alternate Function 4
    ======================================================*/

    /*
     * PB10 -> Alternate Function
     */

    GPIOB_MODER &= ~(3 << 20);
    GPIOB_MODER |=  (2 << 20);


    /*
     * PB11 -> Alternate Function
     */

    GPIOB_MODER &= ~(3 << 22);
    GPIOB_MODER |=  (2 << 22);


    /*
     * Open Drain
     */

    GPIOB_OTYPER |= (1 << 10);
    GPIOB_OTYPER |= (1 << 11);


    /*
     * Pull-up
     */

    GPIOB_PUPDR &= ~(3 << 20);
    GPIOB_PUPDR |=  (1 << 20);

    GPIOB_PUPDR &= ~(3 << 22);
    GPIOB_PUPDR |=  (1 << 22);


    /*
     * PB10/PB11 -> AF4
     */

    GPIOB_AFRH &= ~(0xFF << 8);

    GPIOB_AFRH |= (4 << 8);    // PB10 AF4
    GPIOB_AFRH |= (4 << 12);   // PB11 AF4
}


/*==========================================================
                  I2C1 MASTER INITIALIZATION
==========================================================*/

void I2C1_Master_Init(void)
{
    /*
     * Enable I2C1 clock
     *
     * APB1ENR bit 21 = I2C1EN
     */

    RCC_APB1ENR |= (1 << 21);


    /*
     * Reset I2C1
     */

    I2C1_CR1 |= (1 << 15);

    I2C1_CR1 &= ~(1 << 15);


    /*
     * APB1 clock = 16 MHz
     */

    I2C1_CR2 = 16;


    /*
     * Standard mode = 100 kHz
     *
     * CCR = Fpclk / (2 * Fscl)
     *
     * = 16 MHz / (2 * 100 kHz)
     * = 80
     */

    I2C1_CCR = 80;


    /*
     * Maximum rise time
     */

    I2C1_TRISE = 17;


    /*
     * Enable I2C1
     */

    I2C1_CR1 |= (1 << 0);
}


/*==========================================================
                  I2C2 SLAVE INITIALIZATION
==========================================================*/

void I2C2_Slave_Init(void)
{
    /*
     * Enable I2C2 clock
     *
     * APB1ENR bit 22 = I2C2EN
     */

    RCC_APB1ENR |= (1 << 22);


    /*
     * Reset I2C2
     */

    I2C2_CR1 |= (1 << 15);

    I2C2_CR1 &= ~(1 << 15);


    /*
     * APB1 clock frequency
     */

    I2C2_CR2 = 16;


    /*
     * Slave address = 0x12
     *
     * OAR1 stores address shifted left by 1
     */

    I2C2_OAR1 = (0x12 << 1);


    /*
     * Enable 10/7-bit address acknowledgement
     *
     * Bit 14 should be 1
     */

    I2C2_OAR1 |= (1 << 14);


    /*
     * Enable ACK
     */

    I2C2_CR1 |= (1 << 10);


    /*
     * Enable I2C2
     */

    I2C2_CR1 |= (1 << 0);
}


// I2C1 MASTER SEND
void I2C1_Master_Send(uint8_t saddr,
                      uint8_t n,
                      char *str)
{
    uint32_t timeout;

    // Wait until bus is free
    timeout = 100000;
    while (I2C1_SR2 & (1 << 1))
    {
        if (--timeout == 0)
            return;
    }

    // Generate START
    I2C1_CR1 |= (1 << 8);

    // Wait for SB flag
    timeout = 100000;
    while (!(I2C1_SR1 & (1 << 0)))
    {
        if (--timeout == 0)
            return;
    }


    // Send slave address + WRITE
    // 0x12 << 1 = 0x24

    I2C1_DR = (saddr << 1);


    // Wait for ADDR
    timeout = 100000;
    while (!(I2C1_SR1 & (1 << 1)))
    {
        if (--timeout == 0)
        {
            I2C1_CR1 |= (1 << 9);
            return;
        }
    }

    // Clear ADDR flag
    (void)I2C1_SR2;


    // Send data
    for (uint8_t i = 0; i < n; i++)
    {
        // Wait TXE
        timeout = 100000;
        while (!(I2C1_SR1 & (1 << 7)))
        {
            if (--timeout == 0)
            {
                I2C1_CR1 |= (1 << 9);
                return;
            }
        }

        // Send one byte
        I2C1_DR = *str++;
    }


    // Wait until transmission complete
    // BTF = Bit Transfer Finished
    timeout = 100000;
    while (!(I2C1_SR1 & (1 << 2)))
    {
        if (--timeout == 0)
        {
            I2C1_CR1 |= (1 << 9);
            return;
        }
    }

    // Generate STOP
    I2C1_CR1 |= (1 << 9);
}


// I2C2 SLAVE READ
void I2C2_Slave_Read(uint8_t n,
                     char *str)
{
    // Enable ACK
    I2C2_CR1 |= (1 << 10);


    // Wait until master sends address
    // ADDR flag
    while (!(I2C2_SR1 & (1 << 1)))
    {
    }


    // Clear ADDR
    (void)I2C2_SR2;

    // Receive data 
    for (uint8_t i = 0; i < n; i++)
    {
        // Wait RXNE
        while (!(I2C2_SR1 & (1 << 6)))
        {
        }

        // Read data
        str[i] = (char)I2C2_DR;
    }

    // Disable ACK
    I2C2_CR1 &= ~(1 << 10);
}


// DELAY
void Delay(uint32_t ms)
{
    // Enable TIM2 clock
    RCC_APB1ENR |= (1 << 0);

    // Timer clock = 16 MHz
    // 16,000,000 / 16,000
    // = 1000 Hz
    // 1 count = 1 ms

    TIM2_PSC = 16000 - 1;


    // Auto reload
    TIM2_ARR = ms;


    // Reset update flag
    TIM2_SR &= ~(1 << 0);


    // Enable timer
    TIM2_CR1 |= (1 << 0);

    // Wait for update event
    while (!(TIM2_SR & (1 << 0)))
    {
    }

    // Disable timer
    TIM2_CR1 &= ~(1 << 0);
}