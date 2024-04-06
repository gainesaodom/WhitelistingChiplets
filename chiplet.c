#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringSerial.h>

#define UART_DEVICE "/dev/serial0"
#define ECID_RECEIVER_UART_NETWORK 0x00
#define ECID_RECEIVER_DESTINATION_NETWORK_START 0x10
#define ECID_RECEIVER_DESTINATION_NETWORK_END 0x11
#define TRANSMISSION_DELAY_SECONDS 30
#define TRANSMISSION_REPEAT_COUNT 10

int main() {
    int uart_filestream = serialOpen(UART_DEVICE, 9600);
    if (uart_filestream == -1) {
        printf("Error - Unable to open UART.\n");
        return 1;
    }

    // Create the message buffer
    char message[3] = {0x01, 0x00, 0xAA}; // ECID 0x01, destination ECID, data (e.g., 0xAA)

    // Repeat the transmission process
    for (int i = 0; i < TRANSMISSION_REPEAT_COUNT; ++i) {
        // Attempt to send message to ECID 0x00
        message[1] = ECID_RECEIVER_UART_NETWORK;
        serialPuts(uart_filestream, message);
        printf("Transmitted message to ECID 0x00\n");
        sleep(TRANSMISSION_DELAY_SECONDS);

        // Attempt to send message to ECID 0x10
        message[1] = ECID_RECEIVER_DESTINATION_NETWORK_START;
        serialPuts(uart_filestream, message);
        printf("Transmitted message to ECID 0x10\n");
        sleep(TRANSMISSION_DELAY_SECONDS);

        // Attempt to send message to ECID 0x11
        message[1] = ECID_RECEIVER_DESTINATION_NETWORK_END;
        serialPuts(uart_filestream, message);
        printf("Transmitted message to ECID 0x11\n");
        sleep(TRANSMISSION_DELAY_SECONDS);
    }

    serialClose(uart_filestream);
    return 0;
}
