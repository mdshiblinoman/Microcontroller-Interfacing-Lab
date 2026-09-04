// List of Register Declarations
#define RCC_BASE (0x40023800)
#define GPIOB_BASE (0x40020400)
#define GPIOA_BASE (0x40020000)
#define I2C1_BASE (0x40005400)
#define TIM2_BASE (0x40000000)

#define RCC_AHB1ENR (*(volatile unsigned int *)(RCC_BASE + 0x30))
#define RCC_APB1ENR (*(volatile unsigned int *)(RCC_BASE + 0x40))
#define RCC_CFGR (*(volatile unsigned int *)(RCC_BASE + 08))

#define GPIOA_MODER (*(volatile unsigned int *)(GPIOA_BASE + 0x00))
#define GPIOA_OTYPER (*(volatile unsigned int *)(GPIOA_BASE + 0x04))
#define GPIOA_ODR (*(volatile unsigned int *)(GPIOA_BASE + 0x14))

#define GPIOB_MODER (*(volatile unsigned int *)(GPIOB_BASE + 0x00))
#define GPIOB_OTYPER (*(volatile unsigned int *)(GPIOB_BASE + 0x04))
#define GPIOB_PUPDR (*(volatile unsigned int *)(GPIOB_BASE + 0x0C))
#define GPIOB_AFRH (*(volatile unsigned int *)(GPIOB_BASE + 0x24))

#define I2C1_CR1 (*(volatile unsigned int *)(I2C1_BASE + 0x00))
#define I2C1_CR2 (*(volatile unsigned int *)(I2C1_BASE + 0x04))
#define I2C1_CCR (*(volatile unsigned int *)(I2C1_BASE + 0x1C))
#define I2C1_TRISE (*(volatile unsigned int *)(I2C1_BASE + 0x20))
#define I2C1_SR1 (*(volatile unsigned int *)(I2C1_BASE + 0x14))
#define I2C1_SR2 (*(volatile unsigned int *)(I2C1_BASE + 0x18))
#define I2C1_DR (*(volatile unsigned int *)(I2C1_BASE + 0x10))
#define I2C1_OAR1 (*(volatile unsigned int *)(I2C1_BASE + 0x08))

#define TIM2_PSC (*(volatile unsigned int *)(TIM2_BASE + 0x28))
#define TIM2_ARR (*(volatile unsigned int *)(TIM2_BASE + 0x2C))
#define TIM2_CNT (*(volatile unsigned int *)(TIM2_BASE + 0x24))
#define TIM2_CR1 (*(volatile unsigned int *)(TIM2_BASE + 0x00))
#define TIM2_SR (*(volatile unsigned int *)(TIM2_BASE + 0x10))

// List of Function Declarations
void I2C1_Init(void);
void I2C1_Read(int n, char *str);
void Delay(unsigned int ms);

int main(void)
{
    char str[6];
    I2C1_Init();
    while (1)
    {
        I2C1_Read(6, str);
    }
}

void I2C1_Init(void)
{

    /*Enable clock of GPIOB through AHB1ENR*/
    RCC_AHB1ENR |= (1 << 1);

    /*Set PB8 & PB9 mode to alternate function thru GPIOB_MODER*/
    GPIOB_MODER |= (1 << 19);
    GPIOB_MODER &= ~(1 << 18);
    GPIOB_MODER |= (1 << 17);
    GPIOB_MODER &= ~(1 << 16);

    /*Set PB8 & PB9 output type to open-drain*/
    GPIOB_OTYPER |= (1 << 8);
    GPIOB_OTYPER |= (1 << 9);

    /*Set PB8 & PB9 to output pull-up*/
    GPIOB_PUPDR &= ~(1 << 19);
    GPIOB_PUPDR |= (1 << 18);
    GPIOB_PUPDR &= ~(1 << 17);
    GPIOB_PUPDR |= (1 << 16);

    /*Set PB8 & PB9 alternate function type to I2C (AF4)*/
    GPIOB_AFRH &= ~(0xFF << 0);
    GPIOB_AFRH |= (1 << 2);
    GPIOB_AFRH |= (1 << 6);

    /*Enable clock of I2C1*/
    RCC_APB1ENR |= (1 << 21);

    /*Enter I2C peripheral reset mode*/
    I2C1_CR1 |= (1 << 15);

    /*Come out of reset mode*/
    I2C1_CR1 &= ~(1 << 15);

    /*Set I2C clock frequency (default 16 MHz)*/
    I2C1_CR2 = 16;

    /*Set slave address*/
    I2C1_OAR1 = (0x12 << 1);

    /*Enable the address*/
    I2C1_OAR1 |= (1 << 14);

    /*Now finally enable the I2C1*/
    I2C1_CR1 |= (1 << 0);
}

void I2C1_Read(int n, char *str)
{

    /*Enable acknowledge for slave*/
    I2C1_CR1 |= (1 << 10);

    /* Wait for address match */
    while (!(I2C1_SR1 & (1 << 1)))
    {
    }

    /* Clear ADDR flag */
    (void)I2C1_SR2;

    for (int i = 0; i < n; i++)
    {

        /* Wait until data is received */
        while (!(I2C1_SR1 & (1 << 6)))
        {
        }

        /* Read received data */
        str[i] = I2C1_DR;
    }

    /*Disable acknowledge for slave*/
    I2C1_CR1 &= ~(1 << 10);
}