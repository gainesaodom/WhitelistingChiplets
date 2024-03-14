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
#define MAX_TIMESTAMP_SIZE 20 // Maximum size of the timestamp buffer
#define MAX_TAG_SIZE 20 // Maximum size of the tag buffer
#define CSV_FILENAME "data.csv" // CSV file name
#define PC_IP_ADDRESS "192.168.1.100" // IP address of the PC
#define PORT 12345 // Port number to use for the socket connection

void write_to_csv(const char *filename, const char *timestamp, const char *tag, char byte0, char byte1) {
    FILE *fp = fopen(filename, "a");
    if (fp == NULL) {
        printf("Error - Unable to open CSV file for writing.\n");
        return;
    }
    fprintf(fp, "%s,%s,%c,%c\n", timestamp, tag, byte0, byte1);
    fclose(fp);
}

int main() {
    int uart0_filestream = serialOpen("/dev/serial0", 9600);
    if (uart0_filestream == -1) {
        printf("Error - Unable to open UART.\n");
        return 1;
    }

    char rx_buffer[MAX_BUFFER_SIZE];
    char timestamp_buffer[MAX_TIMESTAMP_SIZE];
    char tag_buffer[MAX_TAG_SIZE];
    int index = 0;
    int timestamp_index = 0;
    int tag_index = 0;
    int first_byte_received = 0;
    time_t last_csv_write_time = time(NULL);

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
    dest_addr.sin_addr.s_addr = inet_addr(PC_IP_ADDRESS);
    dest_addr.sin_port = htons(PORT);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1) {
        perror("Error - Unable to connect to server");
        return 1;
    }

    while (1) {
        if (serialDataAvail(uart0_filestream)) {
            char received_char = serialGetchar(uart0_filestream);
            rx_buffer[index++] = received_char;

            if (!first_byte_received && received_char == 1) {
                first_byte_received = 1;
            } else if (first_byte_received && received_char == 1) {
                // Second byte received, check if both bytes are 1
                if (rx_buffer[index - 2] == 1) {
                    // Both bytes are 1, transmit rx_buffer back across UART
                    for (int i = 0; i < index; ++i) {
                        serialPutchar(uart0_filestream, rx_buffer[i]);
                    }
                } else {
                    // First two bytes are not both 1, store timestamp and tag
                    rx_buffer[index] = '\0'; // Null-terminate the received message
                    printf("Received message: %s\n", rx_buffer);
                    
                    // Get current timestamp
                    time_t current_time = time(NULL);
                    struct tm *tm_info = localtime(&current_time);
                    strftime(timestamp_buffer, MAX_TIMESTAMP_SIZE, "%Y-%m-%d %H:%M:%S", tm_info);
                    
                    // Store tag in the tag array
                    tag_buffer[tag_index++] = 'A'; // Example tag for tag
                    tag_buffer[tag_index++] = ':'; // Example separator
                    // Code to get tag goes here
                    // Example: strcpy(&tag_buffer[tag_index], "ExampleTag");
                    
                    // Write to CSV if ten minutes have passed since the last write
                    if (current_time - last_csv_write_time >= 600) {
                        write_to_csv(CSV_FILENAME, timestamp_buffer, tag_buffer, rx_buffer[0], rx_buffer[1]);
                        last_csv_write_time = current_time;
                        
                        // Send CSV file to PC
                        FILE *csv_file = fopen(CSV_FILENAME, "r");
                        if (csv_file == NULL) {
                            printf("Error - Unable to open CSV file for reading.\n");
                        } else {
                            char csv_data[MAX_BUFFER_SIZE];
                            while (fgets(csv_data, MAX_BUFFER_SIZE, csv_file) != NULL) {
                                if (send(sockfd, csv_data, strlen(csv_data), 0) == -1) {
                                    perror("Error - Unable to send data to server");
                                    fclose(csv_file);
                                    close(sockfd);
                                    return 1;
                                }
                            }
                            fclose(csv_file);
                        }
                    }
                }
                
                // Reset the receive buffer
                index = 0;
                first_byte_received = 0;
            }
        }
    }

    serialClose(uart0_filestream);
    close(sockfd);
    return 0;
}