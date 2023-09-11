// Declaration header files 
#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//*********************Declaration of Global Variable >> START **********************//

// Declaring the Commands 
#define Findfile_cmd "findfile"
#define SGetFile_cmd "sgetfiles"
#define DGetFile_cmd "dgetfiles"
#define GetFile_cmd "getfiles"
#define GetTar_gz_cmd "gettargz"
#define QUIT "quit"

// Server will be start on Port number 8080 
#define PORT 8080
#define SERVER_IP_ADDR "127.0.0.1" // Assigned IP address of mirror server to connect the clients
// Maximum length of the IP addresses used in the server
#define IP_LENGTH 16
#define PORT_LENGTH 6
// Set success message to display when client is connected successfully
#define CONN_SUCCESS "success"
#define BUFFER_SIZE 1024
#define TAR_FILE_NAME "temp.tar.gz"
#define MAX_FILES 6
#define MAX_FILENAME_LEN 50



//*********************Declaration of Global Variable >> END **********************//


//*********************Declaration of Function >> START **********************//


// Function to check validation on commands where commands meets all the condions and correct or not
int IsValidCmd(char* buffer);
// Function to read the file name and tokenized file string
void tokenize_FileName(char* buffer, char filenames[MAX_FILES][MAX_FILENAME_LEN], int* IsZip, int* num_files);
// Function to send commands to server
void send_cmd_toServer(int client_fd, char* buffer);
// Function to receive file stream and handle the data
int receive_files(int socket_fd);

//*********************Declaration of Function >> END **********************//

// Implemetation of main function
int main(int argc, char const *argv[]) {
    int client_fd, n;
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE] = {0};

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP Client - Socket Creation Error\n");
        exit(EXIT_FAILURE);
    }

    memset(&address, '0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP_ADDR, &address.sin_addr) <= 0) {
        perror("TCP Client -Invalid Address");
        exit(EXIT_FAILURE);
    }

    printf("Connecting to the server.......\n");
    if (connect(client_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("TCP Client - Connection Error");
        exit(EXIT_FAILURE);
    }

    if (recv(client_fd, buffer, BUFFER_SIZE, 0) == -1) {
        perror("TCP Client - Error occured while receiving connection status from server");
    }

    if (strcmp(buffer, CONN_SUCCESS) == 0) {
        printf("Succesfully Connected to server....\n");
    } else {
       // to redirect to mirror server if main server reached at its maximum limit
        close(client_fd);
        client_fd = 0;
        if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Mirror Socket Creation Error\n");
            exit(EXIT_FAILURE);
        }
        char mirror_ip[IP_LENGTH], mirror_port[PORT_LENGTH];
        char *ip, *port;
        char buffer_copy[BUFFER_SIZE];
        strcpy(buffer_copy, buffer);
        ip = strtok(buffer_copy, ":");
        port = strtok(NULL, ":");
        strncpy(mirror_ip, ip, sizeof(mirror_ip));
        strncpy(mirror_port, port, sizeof(mirror_port));
        memset(&address, '0', sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(atoi(mirror_port));
        if (inet_pton(AF_INET, mirror_ip, &address.sin_addr) <= 0) {
            perror("TCP Client - Invalid Address");
            exit(EXIT_FAILURE);
        }

        printf("Connecting to the mirror server.....\n");
        if (connect(client_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("TCP Client - Mirror Connection Error");
            exit(EXIT_FAILURE);
        }

        memset(buffer, 0, sizeof(buffer));

        if (recv(client_fd, buffer, BUFFER_SIZE, 0) == -1) {
            perror("Oops ! Error occured while receiving connection status from mirror server");
        }

        if (strcmp(buffer, CONN_SUCCESS) == 0) {
            printf("Connected to mirror server successfully.\n");
        } else {
            perror("Could not connect to main server or mirror server");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        printf("Enter command: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strlen(buffer)-1] = '\0';

        if (!IsValidCmd(buffer)) {
            printf("Invalid Command: %s\n", buffer);
            continue;
        }

        if (strcmp(buffer, QUIT) == 0) {
            send_cmd_toServer(client_fd, buffer);
            break;
        } else if (strncmp(buffer, Findfile_cmd, strlen(Findfile_cmd)) == 0) {
            send_cmd_toServer(client_fd, buffer);
            memset(buffer, 0, BUFFER_SIZE);
            n = read(client_fd, buffer, BUFFER_SIZE - 1);
            if (n < 0) {
                perror("TCP Client - Read Error");
                exit(EXIT_FAILURE);
            }
            if (n == 0) {
                printf("Server disconnected.\n");
                break;
            }
            buffer[n] = '\0';
            printf("File details: \n%s\n", buffer);
        } else if (strncmp(buffer, SGetFile_cmd, strlen(SGetFile_cmd)) == 0) {
            char unzip_status[BUFFER_SIZE] = "";
            sscanf(buffer, "%*s %*d %*d %s", unzip_status);
            send_cmd_toServer(client_fd, buffer);
            int receive_status = receive_files(client_fd);
            int unzip = strncmp(unzip_status, "-u", strlen("-u")) == 0 ? 1 : 0;
            if (unzip && !receive_status) {
                printf("Unzipping the tar file: %s\n", TAR_FILE_NAME);
                char system_call[BUFFER_SIZE] = "tar -xzvf";
                strcat(system_call, TAR_FILE_NAME);
                system(system_call);
            }
        } else if (strncmp(buffer, DGetFile_cmd, strlen(DGetFile_cmd)) == 0) {
            char unzip_status[BUFFER_SIZE] = "";
            char min_date[BUFFER_SIZE];
            char max_date[BUFFER_SIZE];
            sscanf(buffer, "%*s %s %s %s", min_date, max_date, unzip_status);
            send_cmd_toServer(client_fd, buffer);
            int receive_status = receive_files(client_fd);
            int unzip = strncmp(unzip_status, "-u", strlen("-u")) == 0 ? 1 : 0;
            if (unzip && !receive_status) {
                printf("Unzipping the tar file : %s\n", TAR_FILE_NAME);
                char system_call[BUFFER_SIZE] = "tar -xzvf";
                strcat(system_call, TAR_FILE_NAME);
                system(system_call);
            }
        } else if (strncmp(buffer, GetFile_cmd, strlen(GetFile_cmd)) == 0) {
            char filenames[MAX_FILES][MAX_FILENAME_LEN];
            int num_files, IsZip;
            send_cmd_toServer(client_fd, buffer);
            tokenize_FileName(buffer, filenames, &IsZip,&num_files);
            int receive_status = receive_files(client_fd);
            if (IsZip && !receive_status) {
                printf("Unzipping the tar file :%s\n", TAR_FILE_NAME);
                char system_call[BUFFER_SIZE] = "tar -xzvf";
                strcat(system_call, TAR_FILE_NAME);
                system(system_call);
            }
        } else if (strncmp(buffer, GetTar_gz_cmd, strlen(GetTar_gz_cmd)) == 0) {
            char extensions[MAX_FILES][MAX_FILENAME_LEN];
            int num_extensions, IsZip;
            send_cmd_toServer(client_fd, buffer);
            tokenize_FileName(buffer, extensions, &num_extensions, &IsZip);
            int receive_status = receive_files(client_fd);
            if (IsZip && !receive_status) {
                printf("Unzipping the tar file : %s\n", TAR_FILE_NAME);
                char system_call[BUFFER_SIZE] = "tar -xzvf";
                strcat(system_call, TAR_FILE_NAME);
                system(system_call);
            }
        } else {
          // send command to server
            send_cmd_toServer(client_fd, buffer);
            
            memset(buffer, 0, BUFFER_SIZE);
            n = read(client_fd, buffer, BUFFER_SIZE - 1);
            if (n < 0) {
                perror("TCP Client - Read Error");
                exit(EXIT_FAILURE);
            }
            if (n == 0) {
                printf("Server disconnected.\n");
                break;
            }
            buffer[n] = '\0';
            printf("Response from server : \n%s\n", buffer);
        }
    }

    close(client_fd);
    return 0;
}

//*********************Implementation Of Function >> START **********************//

// Function to receive file stream and handle the data
int receive_files(int socket_fd) {

    FILE* fp = fopen(TAR_FILE_NAME, "wb");
    if (!fp) {
        perror("Oops! error occured while creating tar file.");
        return 1;
    }

    char size_buffer[BUFFER_SIZE];
    if (recv(socket_fd, size_buffer, BUFFER_SIZE, 0) == -1) {
        perror("Oops! error occured while receiving size of tar file server");
        fclose(fp);
        return 1;
    }
    long tar_Size = atol(size_buffer);
    printf("File size : %ld \n", tar_Size);

    char buffer[BUFFER_SIZE];
    sprintf(size_buffer, "%ld", tar_Size);

    if (send(socket_fd, size_buffer, strlen(size_buffer), 0) != strlen(size_buffer)) {
        perror("Error occured while sending acknowledgment to the server");
        fclose(fp);
        return 1;
    }
    // handle the file size with 0 length
    if (strcmp(size_buffer, "0") == 0) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = read(socket_fd, buffer, BUFFER_SIZE - 1);
        if (n < 0) {
            perror("TCP Client - Read Error");
            exit(EXIT_FAILURE);
        }
        if (n == 0) {
            printf("Disconnected with server.\n");
        }
        buffer[n] = '\0';
        printf("Response from server: \n%s\n", buffer);
        return 1;
    }

    long bytes_received = 0;
    size_t n;
    while (bytes_received < tar_Size && (n = recv(socket_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        if (fwrite(buffer, sizeof(char), n, fp) != n) {
            perror("Something went wrong! error occured while writing in to tar file");
            break;
        }
        bytes_received += n;
    }
    printf("Successful! File received server\n");
    fclose(fp);
    return 0;
}

// Function to read the file name and tokenized file string
void tokenize_FileName(char* buffer, char filenames[MAX_FILES][MAX_FILENAME_LEN], int* IsZip, int* num_files) {
    char buffer_copy[BUFFER_SIZE];
    strcpy(buffer_copy, buffer);
    char* token;
    char delim[] = " ";
    int i = 0;
    *IsZip = 0;
    token = strtok(buffer_copy, delim);
    token = strtok(NULL, delim);
    i++;
    while (token != NULL && i <= MAX_FILES + 1) {
        if (strcmp(token, "-u") == 0) {
            *IsZip = 1; // unzip file
        }
        else {
            strncpy(filenames[i], token, MAX_FILENAME_LEN);
            i++;
        }
        token = strtok(NULL, delim);
    }

    *num_files = i;
}

// Function to send commands to server
void send_cmd_toServer(int client_fd, char* buffer) {
    // Send command to server
    if (send(client_fd, buffer, strlen(buffer), 0) != strlen(buffer)) {
        perror("TCP Client - Send Error");
        exit(EXIT_FAILURE);
    }
}

// Function to check validation on commands where commands meets all the condions and correct or not
int IsValidCmd(char* buffer) {
    int buffer_length = strlen(buffer);
    char buffer_copy[BUFFER_SIZE];
    strcpy(buffer_copy, buffer);
    char* valid_commands[] = { Findfile_cmd, SGetFile_cmd, DGetFile_cmd, GetFile_cmd, GetTar_gz_cmd, QUIT };
    int total_cmds = sizeof(valid_commands) / sizeof(valid_commands[0]);
    char* token;
    int arg_count = 0;
    token = strtok(buffer_copy, " ");
    if (strcmp(token, QUIT) == 0) {
        while (token != NULL) {
            arg_count++;
            token = strtok(NULL, " ");
        }
        if (arg_count == 1) { // check valid exit command
            return 1;
        }
        else {
            printf("Enter commands %s is invalid as it contains extra arguments. Please enter again.\n", QUIT);
            return 0;
        }
    }
    else if (strncmp(token, Findfile_cmd, strlen(Findfile_cmd)) == 0) {
        while (token != NULL) {
            arg_count++;
            token = strtok(NULL, " ");
        }
        if (arg_count == 2) { // check valid findfile command
            return 1;
        }
        else if (arg_count < 2) {
            printf("Invalid command: %s\n", Findfile_cmd);
            return 0;
        }
        else {
            printf("Enter commands %s is invalid as it contains extra arguments. Please enter again.\n", Findfile_cmd);
            return 0;
        }
    }
    else if (strncmp(token, SGetFile_cmd, strlen(SGetFile_cmd)) == 0) {
        int min_value, max_value;
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) {
            printf("Invalid command %s ! Min and Max file size not provided.\n", SGetFile_cmd);
            return 0;
        }
        for (int i = 0; i < strlen(token); i++) {
            if (token[i] < '0' || token[i] > '9') {
                printf("Passed argument %s is not an integer.\n", token);
                return 0;
            }
        }
        min_value = atoi(token);
        token = strtok(NULL, " ");
        if (token == NULL) {
            printf("Invalid argument! Min size is not provided.\n");
            return 0;
        }
        for (int i = 0; i < strlen(token); i++) {
            if (token[i] < '0' || token[i] > '9') {
                printf("Passed argument %s is not an integer.\n", token);
                return 0;
            }
        }
        max_value = atoi(token);
        if (max_value < min_value) {
            printf("Invalid ! Min value should be smaller than max value.\n");
            return 0;
        }
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) {
            if (token != NULL && strtok(NULL, " ") != NULL) {
                printf("Invalid ! Extra arguments found after the command: %s\n", SGetFile_cmd);
                return 0;
            }
            return 1;
        }
        printf("Extra arguments found after the command: %s\n", SGetFile_cmd);
        return 0;
    }
    else if (strncmp(token, DGetFile_cmd, strlen(DGetFile_cmd)) == 0) {
        char min_date[BUFFER_SIZE];
        char max_date[BUFFER_SIZE];
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) {
            // Invalid - No min or max dates provided
            printf("Invalid dgetfile command ! Min and max dates are not provided \n");
            return 0;
        }
        strcpy(min_date, token);
        token = strtok(NULL, " ");
        if (token == NULL) {
            // Invalid - No max date provided
            printf("Max date is not provided.\n");
            return 0;
        }
        strcpy(max_date, token);
        int IsValidDates = 0;
        struct tm tm1 = { 0 }, tm2 = { 0 };
        time_t time1, time2;
        if (strptime(min_date, "%Y-%m-%d", &tm1) == NULL) {
            printf("Failed to parse date string: %s\n", min_date);
            IsValidDates = 0;
        }
        time1 = mktime(&tm1);

        if (strptime(max_date, "%Y-%m-%d", &tm2) == NULL) {
            printf("Failed to parse date string: %s\n", max_date);
            IsValidDates = 0;
        }
        time2 = mktime(&tm2);

        // Compare dates
        if (difftime(time1, time2) <= 0) {
            IsValidDates = 1;
        }
        else {
            IsValidDates = 0;
        }
        if (!IsValidDates) {
            printf("Enter dates are not valid\n");
            return 0;
        }
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) {
            if (token != NULL && strtok(NULL, " ") != NULL) {
                // Invalid - Extra arguments after -u
                printf("Extra arguments found after the command: %s\n", DGetFile_cmd);
                return 0;
            }
            return 1;
        }
        printf("Invalid command ! dgetfile");
        return 0;
    }
    else if (strncmp(token, GetFile_cmd, strlen(GetFile_cmd)) == 0) {
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) {
            printf("Invalid command! file name is not provided\n");
            return 0;
        }
        while (token != NULL && strcmp(token, "-u") != 0) {
            arg_count++;
            token = strtok(NULL, " ");
            if (token == NULL) {
                if (arg_count <= MAX_FILES) {
                    return 1;
                }
                else {
                    printf("Invalid getfile command as it contains extra arguments\n");
                    return 0;
                }
            }
            else if (strcmp(token, "-u") == 0) {
                if (arg_count <= MAX_FILES && strtok(NULL, " ") == NULL) {
                    return 1;
                }
                else {
                    printf("Invalid getfile command as it contains extra arguments\n");
                    return 0;
                }
            }
        }
    }
    else if (strncmp(token, GetTar_gz_cmd, strlen(GetTar_gz_cmd)) == 0) {
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) {
            printf("Invalid gettargz command as it does not contains extension for a file\n");
            return 0;
        }
        while (token != NULL && strcmp(token, "-u") != 0) {
            arg_count++;
            token = strtok(NULL, " ");
            if (token == NULL) {
                if (arg_count <= MAX_FILES) {
                    return 1;
                }
                else {

                    printf("Invalid gettargz command as it contains extra arguments\n");
                    return 0;
                }
            }
            else if (strcmp(token, "-u") == 0) {
                if (arg_count <= MAX_FILES && strtok(NULL, " ") == NULL) {
                    return 1;
                }
                else {
                    printf("Invalid gettargz command as it contains extra arguments\n");
                        return 0;
                }
            }
        }
    }
    // return 0 as command is not valid
    return 0;
}

//*********************Implementation Of Function >> END **********************//