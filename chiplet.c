#include <stdio.h>
#include <string.h>
#include <wiringSerial.h>

#define TX_BUFFER_LENGTH 256

int main() {
    int uart0_filestream = serialOpen("/dev/serial0", 9600);
    if (uart0_filestream == -1) {
        printf("Error - Unable to open UART.\n");
        return 1;
    }

    char tx_buffer[TX_BUFFER_LENGTH] = "98 Big Bad Hardware Snatchers Beating Down Your Door";
    serialPrintf(uart0_filestream, "%s", tx_buffer);
    serialClose(uart0_filestream);
    return 0;