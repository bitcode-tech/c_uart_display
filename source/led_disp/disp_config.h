/* 7-segment display digit arrangement direction: 1 = lower-numbered display positions are on the right; 0 = lower-numbered display positions are on the left */
#define LOW_DIG_NUM_ON_RIGHT 0

/* Whether the UART uses interrupt mode: 1 = interrupt mode; 0 = non-interrupt mode */
#define	UART_MODE_INTERRUPT	1

/* UART FIFO buffer size. The default is 8. This definition has no effect in normal mode. */
/* The value must be a power of 2, such as 2, 4, 8, 16, 32, ...                  */
#define	UART_FIFO_SIZE		(8)
