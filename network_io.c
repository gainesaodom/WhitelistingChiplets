
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wiringSerial.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_BUFFER_SIZE 256 // Maximum size of the receive buffer
#define ECID_SOURCE_UART_NETWORK 0x00
#define ECID_SOURCE_DESTINATION_NETWORK 0x01
#define ECID_DESTINATION_NETWORK_START 0x10
#define ECID_DESTINATION_NETWORK_END 0x11
#define DESTINATION_IP_ADDRESS "192.168.1.100" // IP address of the destination Raspberry Pi
#define PORT 12345 // Port number to use for the socket connection
#define CSV_FILENAME "bad_attempts.csv" // CSV file name for logging bad attempts
#define CSV_SAVE_INTERVAL 300 // Interval to save the CSV file (in seconds)

// Define a structure to represent a source-destination pair
typedef struct {
    char source_ecid;
    char destination_ecid;
} SourceDestinationPair;

// Define the list of allowed source-destination pairs (Whitelist)
SourceDestinationPair allowed_pairs[] = {
    {ECID_SOURCE_UART_NETWORK, ECID_DESTINATION_NETWORK_START},
    {ECID_SOURCE_UART_NETWORK, ECID_DESTINATION_NETWORK_END}
};

// Define the size of the allowed pairs array
#define ALLOWED_PAIRS_COUNT (sizeof(allowed_pairs) / sizeof(allowed_pairs[0]))

// Define a structure to represent a bad attempt
typedef struct {
    char source_ecid;
    char destination_ecid;
    time_t timestamp;
} BadAttempt;

// Define the array to store bad attempts
BadAttempt bad_attempts[MAX_BUFFER_SIZE];
int bad_attempts_count = 0;

// Define a function to check if a pair is allowed
int is_pair_allowed(char source_ecid, char destination_ecid) {
    for (int i = 0; i < ALLOWED_PAIRS_COUNT; ++i) {
        if (allowed_pairs[i].source_ecid == source_ecid && allowed_pairs[i].destination_ecid == destination_ecid) {
            return 1; // Pair is allowed
        }
    }
    return 0; // Pair is not allowed
}


// Define a function to save bad attempts to a CSV file
void save_bad_attempts_to_csv() {
    FILE *fp = fopen(CSV_FILENAME, "w");
    if (fp == NULL) {
        perror("Error - Unable to open CSV file for writing");
        return;
    }

    fprintf(fp, "Source ECID,Destination ECID,Timestamp\n");
    for (int i = 0; i < bad_attempts_count; ++i) {
        fprintf(fp, "%02X,%02X,%ld\n", bad_attempts[i].source_ecid, bad_attempts[i].destination_ecid, bad_attempts[i].timestamp);
    }
    
    fclose(fp);
    bad_attempts_count = 0; // Reset the bad attempts count after saving
}

// Define a function to log bad attempts to a CSV file
void log_bad_attempt(char source_ecid, char destination_ecid, time_t timestamp) {
    if (bad_attempts_count < MAX_BUFFER_SIZE) {
        bad_attempts[bad_attempts_count].source_ecid = source_ecid;
        bad_attempts[bad_attempts_count].destination_ecid = destination_ecid;
        bad_attempts[bad_attempts_count].timestamp = timestamp;
        bad_attempts_count++;
    }
    if (bad_attempts_count >= MAX_BUFFER_SIZE) {
        save_bad_attempts_to_csv();    
    }
}


int main() {
    int uart0_filestream = serialOpen("/dev/serial0", 9600);
    if (uart0_filestream == -1) {
        printf("Error - Unable to open UART.\n");
        return 1;
    }

    char rx_buffer[MAX_BUFFER_SIZE];
    int index = 0;
    int first_byte_received = 0;
    time_t last_csv_save_time = time(NULL);

    // Create socket
    int sockfd;
    struct sockaddr_in dest_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error - Unable to create socket");
        return 1;
    }

    // Configure server address
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(DESTINATION_IP_ADDRESS);
    dest_addr.sin_port = htons(PORT);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1) {
        perror("Error - Unable to connect to server");
        return 1;
    }

    // Initialize logger-related variables
    int bad_attempt_flag = 0;
    bad_attempts_count = 0;

    while (1) {
        bad_attempt_flag = 0;
        if (serialDataAvail(uart0_filestream)) {
            char received_char = serialGetchar(uart0_filestream);
            rx_buffer[index++] = received_char;

            if (!first_byte_received && received_char == ECID_SOURCE_UART_NETWORK) {
                first_byte_received = 1;

            } else if (first_byte_received && received_char == ECID_SOURCE_UART_NETWORK) {
                // Second byte received, check if the pair is allowed
                if (is_pair_allowed(ECID_SOURCE_UART_NETWORK, rx_buffer[index - 2])) {
                    // Transmit over Ethernet
                    if (send(sockfd, rx_buffer, index, 0) == -1) {
                        perror("Error - Unable to send data over Ethernet");
                    }
                } 
                else {
                    bad_attempt_flag = 1;
                    bad_attempts_count++;
                }

                // Reset the receive buffer
                index = 0;
                first_byte_received = 0;
            }
        }

        if (bad_attempt_flag == 1) log_bad_attempt(rx_buffer[0], rx_buffer[1], time(NULL));
        if (time(NULL) - last_csv_save_time >= CSV_SAVE_INTERVAL) save_bad_attempts_to_csv();
    }

    serialClose(uart0_filestream);
    close(sockfd);
    return 0;
}
