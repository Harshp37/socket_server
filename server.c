// Declaration header files 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>

//*********************Declaration of Global Variable >> START **********************//

// Server will be start on Port number 8080 
#define PORT 8080
#define Mirror_PortNumber 8081 // Port number 8081 is assigned for the mirror server
#define Mirror_IPAddress "127.0.0.1" // Assigned IP address of mirror server to connect the clients
// Maximum length of the IP addresses and port number
#define IPLen 16
#define PORT_LENGTH 6
#define SuccessMsg "success" // Set success message to display when client is connected successfully

// Declaring the Commands 
#define Findfile_cmd "findfile"
#define SGetFile_cmd "sgetfiles"
#define DGetFile_cmd "dgetfiles"
#define GetFile_cmd "getfiles"
#define GetTar_gz_cmd "gettargz"
#define Quit_cmd "quit"

#define MAX_CLIENTS 4 // max number of client can be connected 
#define BUFFER_SIZE 1024
#define Tar_Fname "servertemp.tar.gz"// Temporary tar.gz file created by the server
#define MAX_FILES 6
#define MAX_FILENAME_LEN 50

//*********************Declaration of Global Variable >> END **********************//

//*********************Declaration of Function >> START **********************//

// Function to check if file exists in directory(s)
int IsFileExists(const char* dir_name, const char* filename, char* tar_file);

// Function to Find the file in the server from /home/harsh
char* findfile(char* filename);

// Function to send tar file
void send_tarfile_Stream(int socket_fd);

/// Function to excute getfiles command to get the files
char* getfiles(int socket_fd, char files[MAX_FILES][MAX_FILENAME_LEN], int num_files);

// Function to add the matching files in the link list
void Find_and_matchFiles(const char* dir_path, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions, FILE* temp_list);

// Function to find the tar file and send 
char* gettargz(int socket_fd, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions);

// Function to read the arguments from the command 
void tokenize_FileName(char* buffer, char filenames[MAX_FILES][MAX_FILENAME_LEN], int* num_files);

//*********************Declaration of Function >> END **********************//

// Implementation of main function
int main(int argc, char const *argv[]) {
    
    int server_fd, client_fd;
    // intialize the counters
    int ClientCounter = 0, cnt = 0;
    
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int optval = 1;

    pid_t childpid;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("TCP Server - Socket Error");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("TCP Server - setsockopt Error");
        exit(EXIT_FAILURE);
    }

    memset(&address, '0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Binding the address to the opened socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("TCP Server - Bind Error");
        exit(EXIT_FAILURE);
    }

    // Waiting for clients
    if (listen(server_fd, MAX_CLIENTS - 1) < 0) {
        perror("TCP Server - Listen Error");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
         client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);

        // Handle the clients by server and redirect to the mirror server 

        //Check the codition if max client is more than 4 
        if ((ClientCounter >= MAX_CLIENTS && ClientCounter < 2*MAX_CLIENTS) || (ClientCounter >= 2*MAX_CLIENTS && ClientCounter % 2 != 0)) {
            // code to handle client by mirror server
            char mirrorPortNumber[PORT_LENGTH];
            sprintf(mirrorPortNumber, "%d", Mirror_PortNumber);

            char mirror_address[IPLen + PORT_LENGTH + 1] = Mirror_IPAddress;
            strcat(mirror_address, ":");
            strcat(mirror_address, mirrorPortNumber);

            printf("Please wait..Redirecting to mirror server....\n");
            // Send the mirror server connection details like address, port to client to create socket
            if (send(client_fd, mirror_address, strlen(mirror_address), 0) < 0) {
                perror("TCP Server - Mirror Address Send failed");
                exit(EXIT_FAILURE);
            }
            ClientCounter++;
        } else {
            // code to handle clients by the main server

            if (client_fd < 0) {
                perror("TCP Server - Accept Error");
                continue;
            }

            // Acknowledging client socket connection successful
            if (send(client_fd, SuccessMsg, strlen(SuccessMsg), 0) < 0) {
                perror("TCP Server - Connection Acknowledgement Send failed");
                exit(EXIT_FAILURE);
            }

            printf("Client connected successfully !\n");
            
            // creating child process to handle the clients using fork
            childpid = fork();
            if (childpid < 0) {
                perror("Error occured while forking process");
                exit(EXIT_FAILURE);
            }
            if (childpid == 0) {
                close(server_fd);
                int exit_status = Execute_ClientCmds(client_fd);
                if (exit_status == 0) {
                   exit(EXIT_SUCCESS); // client disconnected successfully
                } else {
                   exit(EXIT_FAILURE);// client disconnected abnormally
                }
            } else {
                // Parent process
                ClientCounter++;
                cnt++;
                printf("Total connected Clients to the server: %d\n", cnt);
                close(client_fd);
                while (waitpid(-1, NULL, WNOHANG) > 0);
            }
        }
    }

    close(server_fd);
    return 0;
}

//*********************Implementation Of Function >> START **********************//

// Function to read the arguments from the command 
void tokenize_FileName(char* buffer, char filenames[MAX_FILES][MAX_FILENAME_LEN], int* num_files) {
    char* token;
    char delim[] = " ";
    int i = 0;

    token = strtok(buffer, delim);
    token = strtok(NULL, delim);
    while (token != NULL && i < MAX_FILES) {
        if (strcmp(token, "-u") == 0) {
            printf("");
        }
        else {
            strncpy(filenames[i], token, MAX_FILENAME_LEN);
            i++;
        }
        token = strtok(NULL, delim);
    }

    *num_files = i;
}

// Function to check if file exists in directory(s)
int IsFileExists(const char* dir_name, const char* filename, char* tar_file) {
    int found = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat file_info;
    char path[PATH_MAX];

    // Open the directory 
    if ((dir = opendir(dir_name)) == NULL) {
        perror("opendir");
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(path, PATH_MAX, "%s/%s", dir_name, entry->d_name);

        // Fetching the file information using stat
        if (lstat(path, &file_info) < 0) {
            perror("lstat");
            continue;
        }

        // Serach file in directory tree
        if (S_ISDIR(file_info.st_mode)) {
            IsFileExists(path, filename, tar_file);
        }
        else if (S_ISREG(file_info.st_mode)) {
            if (strcmp(entry->d_name, filename) == 0) {
                strncat(tar_file, " ", BUFFER_SIZE - strlen(tar_file) - 1);
                strncat(tar_file, path, BUFFER_SIZE - strlen(tar_file) - 1);
                printf("File Path: %s\n", path);
                found = 1;
            }
        }
    }

    closedir(dir);
    return found;
}

// Function to Find the file in the server from /home/harsh
char* findfile(char* filename) {
    char BufferStr[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -name %s -printf '%%f|%%s|%%T@\\n' | head -n 1", filename);
    FILE* fp = popen(command, "r");
    char path[BUFFER_SIZE];
    if (fgets(path, BUFFER_SIZE, fp) != NULL) {
        printf("Successful.... Given File Found.\n");
        path[strcspn(path, "\n")] = 0;
        // Extract the filename, size, and date from the path string
        char* filename_ptr = strtok(path, "|");
        char* size_ptr = strtok(NULL, "|");
        char* date_ptr = strtok(NULL, "|");

        int size = atoi(size_ptr);
        time_t date = atoi(date_ptr);

        char print_filename[BUFFER_SIZE];
        strcpy(print_filename, "File Name: ");
        strcat(print_filename, filename_ptr);
        strcat(print_filename, "\n");

        char print_size[BUFFER_SIZE];
        strcpy(print_size, "Size of File: ");
        strcat(print_size, size_ptr);
        strcat(print_size, "\n");

        char print_created[BUFFER_SIZE];
        strcpy(print_created, "Timestemp Of File Creation: ");
        strcat(print_created, ctime(&date));
        strcat(print_created, "\n");

        strcpy(BufferStr, print_filename);
        strcat(BufferStr, print_size);
        strcat(BufferStr, print_created);
    }
    else {
        printf("Sorry ,Given file not found!\n");
        strcpy(BufferStr, "Sorry ,Given file not found!\n");
    }
    pclose(fp);
    char* ptr_client_str = BufferStr;
    return ptr_client_str;
}

/// Function to excute getfiles command to get the files
char* getfiles(int socket_fd, char files[MAX_FILES][MAX_FILENAME_LEN], int num_files) {
    // Get the current directory path
    char* dir_path = getenv("HOME");
    if (dir_path == NULL) {
        fprintf(stderr, "Error occured while getting home directory path\n");
        return NULL;
    }

    // command to create tar file
    char tar_cmd[BUFFER_SIZE] = "tar -czvf ";
    strcat(tar_cmd, Tar_Fname);

    int foundFlag = 0;
    for (int i = 0; i < num_files; i++) {
        printf("Searching Filename: %s .......\n", files[i]);
        char file_path[BUFFER_SIZE];
        snprintf(file_path, BUFFER_SIZE, "%s/%s", dir_path, files[i]);

        const char* homedir = getenv("HOME");
        if (homedir == NULL) {
            printf("home directory not found !\n");
            return 0;
        }

        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", homedir, files[i]);
        foundFlag += IsFileExists(homedir, files[i], tar_cmd);
    }

    if (foundFlag) {
        printf("File(s) found\n");
        system(tar_cmd);

        FILE* tar_file = fopen(Tar_Fname, "r");
        if (tar_file == NULL) {
            fprintf(stderr, "Error while opening tar file!\n");
            return NULL;
        }
        fclose(tar_file);
        send_tarfile_Stream(socket_fd);
    }
    else {
        printf("file(s) not found!\n");
        if (send(socket_fd, "0", strlen("0"), 0) != strlen("0")) {
            perror("Oopps! Error occured while sending the size of tar file to connected client");
            return NULL;
        }
        return "file(s) not found.";
    }

    return NULL;
}

// Function to add the matching files in the link list
void Find_and_matchFiles(const char* dir_path, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions, FILE* temp_list) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        printf("Error occured while opening the directory %s\n", dir_path);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char* name = entry->d_name;
            for (int i = 0; i < num_extensions; i++) {
                char* extension = extensions[i];
                int len_ext = strlen(extension);
                int len_name = strlen(name);
                if (len_name >= len_ext && strcmp(name + len_name - len_ext, extension) == 0) {
                    // Add the files in the link list which matches
                    fprintf(temp_list, "%s/%s\n", dir_path, name);
                    break;
                }
            }
        }
        else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char subdir_path[BUFFER_SIZE];
            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", dir_path, entry->d_name);
            Find_and_matchFiles(subdir_path, extensions, num_extensions, temp_list);
        }
    }

    closedir(dir);
}

// Function to find the tar file and send 
char* gettargz(int socket_fd, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions) {
    int foundFlag = 0;
    FILE* temp_list = tmpfile();
    if (!temp_list) {
        printf("Error while creating the temporary file\n");
        return NULL;
    }

    Find_and_matchFiles(getenv("HOME"), extensions, num_extensions, temp_list);
    rewind(temp_list);
    char filename[BUFFER_SIZE];

    while (fgets(filename, sizeof(filename), temp_list) != NULL) {
        // Remove the newline character at the end of the filename
        filename[strcspn(filename, "\n")] = 0;
        foundFlag++;
    }

    if (foundFlag) {
        rewind(temp_list);
        char command[BUFFER_SIZE] = "tar -czvf ";
        strcat(command, Tar_Fname);
        char filename[BUFFER_SIZE];
        while (fgets(filename, sizeof(filename), temp_list) != NULL) {
            filename[strcspn(filename, "\n")] = 0;
            strcat(command, " ");
            strcat(command, filename);
        }
        int result = system(command);
        send_tarfile_Stream(socket_fd);
        fclose(temp_list);
    }
    else {
        printf("Error : file not found.\n");
        if (send(socket_fd, "0", strlen("0"), 0) != strlen("0")) {
            perror("Error sending tar file size to client");
            return NULL;
        }
        fclose(temp_list);
        return "Error : file not found.";
    }
    return NULL;
}

// Function to send tar file
void send_tarfile_Stream(int socket_fd) {
    int flag = 0;
    // Open the tar file in binary read mode
    FILE* fp = fopen(Tar_Fname, "rb");
    if (!fp) {
        perror("Oops! Error in opening tar file");
        return;
    }

    // Send the tar file size to the client
    fseek(fp, 0, SEEK_END);
    long tar_file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char size_buffer[BUFFER_SIZE];
    sprintf(size_buffer, "%ld", tar_file_size);
    if (send(socket_fd, size_buffer, strlen(size_buffer), 0) != strlen(size_buffer)) {
        perror("Error occured while sending tar file size to the connected client");
        fclose(fp);
        return;
    }

    // Get Acknowledgment from the client
    if (recv(socket_fd, size_buffer, BUFFER_SIZE, 0) == -1) {
        perror("Error occured while sending tar file size to client");
        fclose(fp);
        return;
    }
    printf("Acknowledgment recieved from client.\n");
    printf("File of size %ld sent.\n", tar_file_size);

    // Sending the tar file contents to the client
    char buffer[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0) {
        if (send(socket_fd, buffer, n, 0) != n) {
            perror("Error occured while sending tar file contents to client");
            flag = 1;
            break;
        }
    }
    if (flag) {
        printf("Error occured while sending tar file to client\n");
    }
    else {
        printf("File sent successfully\n");
    }
    fclose(fp);
}

// Execute commands entered by the client 
int Execute_ClientCmds(int socket_fd) {
    char* result;
    char buffer[BUFFER_SIZE];
    int n, fd;
    while (1) {
        result = NULL;
        bzero(buffer, BUFFER_SIZE);
        n = read(socket_fd, buffer, BUFFER_SIZE - 1);
        if (n < 0) {
            perror("TCP Server - Read Error");
            return 1;
        }
        if (n == 0) {
            break;
        }

        buffer[n] = '\0';
        printf("Given Command is %s\n", buffer);
        printf("Executing the given command...\n");

        if (strncmp(buffer, Findfile_cmd, strlen(Findfile_cmd)) == 0) {
            char filename[BUFFER_SIZE];
            sscanf(buffer, "%*s %s", filename);
            printf("Filename: %s\n", filename);
            result = findfile(filename);
        }
        else if (strncmp(buffer, SGetFile_cmd, strlen(SGetFile_cmd)) == 0) {
            int min_value, max_value;
            sscanf(buffer, "%*s %d %d", &min_value, &max_value);
            // excute sgetfiles command to get the files
            char command[BUFFER_SIZE];
            sprintf(command, "find ~/ -type f -size +%d -size -%d -print0 | tar -czvf %s --null -T -", min_value, max_value, Tar_Fname);
            FILE* fp = popen(command, "r");
            send_tarfile_Stream(socket_fd);

            result = NULL;
            continue;
        }
        else if (strncmp(buffer, DGetFile_cmd, strlen(DGetFile_cmd)) == 0) {
            char min_date[BUFFER_SIZE];
            char max_date[BUFFER_SIZE];
            sscanf(buffer, "%*s %s %s", min_date, max_date);
            //excute dgetfiles command to get the files
            char command[BUFFER_SIZE];
            sprintf(command, "find ~/ -type f -newermt \"%s\" ! -newermt \"%s\" -print0 | tar -czvf %s --null -T -", min_date, max_date, Tar_Fname);
            FILE* fp = popen(command, "r");
            send_tarfile_Stream(socket_fd);

            result = NULL;
            continue;
        }
        else if (strncmp(buffer, GetFile_cmd, strlen(GetFile_cmd)) == 0) {
            char filenames[MAX_FILES][MAX_FILENAME_LEN];
            int num_files;
            tokenize_FileName(buffer, filenames, &num_files);
            result = getfiles(socket_fd, filenames, num_files);
            if (result == NULL) {
                continue;
            }
        }
        else if (strncmp(buffer, GetTar_gz_cmd, strlen(GetTar_gz_cmd)) == 0) {
            char extensions[MAX_FILES][MAX_FILENAME_LEN];
            int num_extensions;
            tokenize_FileName(buffer, extensions, &num_extensions);
            result = gettargz(socket_fd, extensions, num_extensions);
            if (result == NULL) {
                continue;
            }
        }
        else if (strcmp(buffer, Quit_cmd) == 0) {
            printf("disconnecting from the server.\n");
            break;
        }
        else {
            result = buffer;
        }
        if (send(socket_fd, result, strlen(result), 0) != strlen(result)) {
            perror("TCP Server - Send Error");
            close(socket_fd);
            return 1;
        }
        printf("Response from server to client: %s\n", result);
    }
    close(socket_fd);
    return 0;
}

//*********************Implementation Of Function >> END **********************//
