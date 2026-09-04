#define RCC_BASE (0x40023800)
#define GPIOA_BASE (0x40020000)
#define UART1_BASE (0x40011000)

#define RCC_AHB1ENR (*(volatile unsigned int *)(RCC_BASE + 0x30))
#define RCC_APB2ENR (*(volatile unsigned int *)(RCC_BASE + 0x44))

#define GPIOA_MODER (*(volatile unsigned int *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRH (*(volatile unsigned int *)(GPIOA_BASE + 0x24))

#define UART1_SR (*(volatile unsigned int *)(UART1_BASE + 0x00))
#define UART1_BRR (*(volatile unsigned int *)(UART1_BASE + 0x08))
#define UART1_CR1 (*(volatile unsigned int *)(UART1_BASE + 0x0C))
#define UART1_DR (*(volatile unsigned int *)(UART1_BASE + 0x04))

#define SYSCLK 16000000U
#define BAUDRATE 9600U

void uart1_tx_init(void);
void uart1_receive(int n, char *str);

int main(void)
{
    uart1_tx_init();
    char str[6];
    while (1)
    {
        uart1_receive(6, str);
        (void)str;
    }
}

void uart1_tx_init()
{
    /* Enable clock of port A*/
    RCC_AHB1ENR |= (1 << 0);

    /*PA10 to alternate function mode 10 at bit 20 and 21*/
    GPIOA_MODER &= ~(3 << 20); // Clear bits 20:21
    GPIOA_MODER |= (2 << 20);  // Set to Alternate Function mode (10)

    /*AF7 (0111)*/
    GPIOA_AFRH &= ~(0xF << 8); // clear bit 8-11
    GPIOA_AFRH |= (7 << 8);

    /*Clock of USART1*/
    RCC_APB2ENR |= (1 << 4);

    /*Configure BaudRate*/
    UART1_BRR = (SYSCLK + (BAUDRATE / 2)) / BAUDRATE;

    /* Enable RE*/
    UART1_CR1 = (1 << 2);

    /*Enable the UART1*/
    UART1_CR1 |= (1 << 13);
}

void uart1_receive(int n, char *str)
{
    for (int i = 0; i < n; i++)
    {
        /* Rx data register is empty*/
        while (!(UART1_SR & (1 << 5)))
        {
        }
        str[i] = UART1_DR;
    }
}