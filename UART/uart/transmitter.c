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
void uart1_send(int n, char *str);

int main(void)
{
    uart1_tx_init();
    while (1)
    {
        uart1_send(6, "Shakil");
    }
}

void uart1_tx_init()
{
    /* Enable clock of port A*/
    RCC_AHB1ENR |= (1 << 0);

    /*PA9 to alternate function mode 10 at bit 18 and 19*/
    GPIOA_MODER &= ~(3 << 18); // Clear bits 18:19
    GPIOA_MODER |= (2 << 18);  // Set to Alternate Function mode (10)

    /*AF7 (0111)*/
    GPIOA_AFRH &= ~(0xF << 4); // clear bit 4-7
    GPIOA_AFRH |= (7 << 4);

    /*Clock of USART1*/
    RCC_APB2ENR |= (1 << 4);

    /*Configure BaudRate*/
    UART1_BRR = (SYSCLK + (BAUDRATE / 2)) / BAUDRATE;

    /* Enable TE*/
    UART1_CR1 = (1 << 3); // Directly assigned, intentionally cleared other bits

    /*Enable the UART1*/
    UART1_CR1 |= (1 << 13);
}

void uart1_send(int n, char *str)
{
    for (int i = 0; i < n; i++)
    {
        /* Tx data register is empty*/
        while (!(UART1_SR & (1 << 7)))
        {
        }

        UART1_DR = *str++;
    }
}