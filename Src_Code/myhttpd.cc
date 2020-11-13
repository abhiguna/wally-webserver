#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <time.h>
#include <string>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>

int QueueLength = 5;

pthread_mutex_t mutex;

// Parses the header
void processRequest( int socket);
void mainThread(int socket);
void processRequestThread(int socket);
void threadPoolBased(int socket);
void processBased(int socket);
void threadBased(int socket);
void poolSlave(int socket);


// Define a handler for zombie process
extern "C" void zombie_handler(int signal, siginfo_t *info,
                                void *ucontext) {
  int pid = info->si_pid;
  waitpid(pid, NULL, WNOHANG);
}


int main( int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "%s", "Please Specify Portnumber");
        exit(-1);
    }

    // Get portnumber
    char flag[1024];
    int port;
    if ((strcmp(argv[1], "-f") == 0) || 
        (strcmp(argv[1], "-t") == 0) ||
        (strcmp(argv[1], "-p") == 0)) {
        strcpy(flag, argv[1]);
        // std::cout << flag << std::endl;
        if (argc < 3) {
            port = 1025;
        }
        else {
            port = atoi(argv[2]);
        }
        // std::cout << port << std::endl;
    }
    else {
        port = atoi( argv[1]);
        // std::cout << port << std::endl;
    }
    
    // Set the IP address and port for this server
    struct sockaddr_in serverIPAddress; 
    memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
    serverIPAddress.sin_family = AF_INET;
    serverIPAddress.sin_addr.s_addr = INADDR_ANY;
    serverIPAddress.sin_port = htons((u_short) port);

    // Allocate a socket
    int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
    if ( masterSocket < 0) {
        perror("socket");
        exit( -1 );
    }

    // Set socket options to reuse port. Otherwise we will
    // have to wait about 2 minutes before reusing the sae port number
    int optval = 1; 
    int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
                (char *) &optval, sizeof( int ) );
    
    // Bind the socket to the IP address and port
    int error = bind( masterSocket,
                (struct sockaddr *)&serverIPAddress,
                sizeof(serverIPAddress) );
    if ( error ) {
        perror("bind");
        exit( -1 );
    }

    // Put socket in listening mode and set the 
    // size of the queue of unprocessed connections
    error = listen( masterSocket, QueueLength);
    if ( error ) {
        perror("listen");
        exit( -1 );
    }

        // Accept incoming connections
        if (strcmp(flag, "-f") == 0) {
            processBased(masterSocket);
        }
        else if (strcmp(flag, "-t") == 0) {
            threadBased(masterSocket);
        }
        else if (strcmp(flag, "-p") == 0) {
            threadPoolBased(masterSocket);
        }
        else {
            mainThread(masterSocket);
        }
        close( masterSocket );
    
    return 0;
}

// No Flags Present
void mainThread(int socket) {
    while ( 1 ) {
        // Accept incoming connections
        struct sockaddr_in clientIPAddress;
        int alen = sizeof( clientIPAddress );
        int slaveSocket = accept( socket,
                    (struct sockaddr *)&clientIPAddress,
                    (socklen_t*)&alen);

        if ( slaveSocket < 0 ) {
        perror( "accept" );
        exit( -1 );
        }
         // Process request.
            processRequest( slaveSocket );
            // Close socket
            close( slaveSocket );
    }

}

// Process Based
void processBased(int socket) {
    while ( 1 ) {
        // Accept incoming connections
        struct sockaddr_in clientIPAddress;
        int alen = sizeof( clientIPAddress );
        int slaveSocket = accept( socket,
                    (struct sockaddr *)&clientIPAddress,
                    (socklen_t*)&alen);
         if (slaveSocket >= 0) {
            pid_t ret = fork();
            struct sigaction sig;
            sig.sa_sigaction = zombie_handler;
            sigemptyset(&sig.sa_mask);
            sig.sa_flags = SA_SIGINFO | SA_RESTART;
            if (sigaction(SIGCHLD, &sig, NULL)) {
                perror("sigaction");
                _exit(1);
            }
            if (ret == 0) {
                processRequest( slaveSocket );
                close( slaveSocket );
                exit(EXIT_SUCCESS);
            }
            close(slaveSocket);
         }
    }
}

// Thread Based
void threadBased(int socket) {
    while ( 1 ) {
        // Accept incoming connections
        struct sockaddr_in clientIPAddress;
        int alen = sizeof( clientIPAddress );
        int slaveSocket = accept( socket,
                    (struct sockaddr *)&clientIPAddress,
                    (socklen_t*)&alen);

        if ( slaveSocket < 0 ) {
        perror( "accept" );
        exit( -1 );
        }
        pthread_t t1;
            pthread_attr_t attr;
            pthread_attr_init( &attr );
            pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
            pthread_create( &t1, &attr, (void * (*)(void *)) processRequestThread, 
			(void *) slaveSocket);
    }
}

// Thread Pool Based
void threadPoolBased(int socket) {
    pthread_t tid[5];
    for (int i = 0; i < 5; i++) {
        pthread_attr_t attr;
        pthread_attr_init( &attr );
        pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
        pthread_create( &tid[i], &attr, (void * (*)(void *)) poolSlave, 
			(void *) socket);
    }
    pthread_join(tid[0], NULL);
}

// Pool Helper
void poolSlave(int socket) {
    while ( 1 ) {
        // Create Mutex Lock
        pthread_mutex_init(&mutex, NULL);
        pthread_mutex_lock(&mutex);
        // Accept incoming connections
        struct sockaddr_in clientIPAddress;
        int alen = sizeof( clientIPAddress );
        int slaveSocket = accept( socket,
                    (struct sockaddr *)&clientIPAddress,
                    (socklen_t*)&alen);
        
        if ( slaveSocket < 0 ) {
        perror( "accept" );
        exit( -1 );
        }
        processRequest(slaveSocket);
        close(slaveSocket);
        pthread_mutex_unlock(&mutex);

    }

}

void processRequestThread(int socket) {
    processRequest(socket);
    close(socket);
}

void processRequest(int socket) {
    // docPath string
    std::cout << "Processing Request" << std::endl;
    const int MaxLength = 1024;
    std::string docPath = "";
    std::string lineBuffer = "";
    std::vector<std::string> headers;
    std::string filePath = "";
    std::string first_line;
    char crlf[] = "\r\n";
    int length = 0;
    int n;

    // Step 1: Read the HTTP header.
    int gotGet = 0;
    int firstLine = 0;

    unsigned char newChar;

    unsigned char oldChar;

    unsigned char third;

    unsigned char fourth;

    while((n = read(socket, &newChar, sizeof(newChar)))) {
        // std::cout << (newChar == '\n') << (oldChar == '\r') << 
        // (third == '\r') << (fourth == '\n') << std::endl;

        if (newChar == '\n' && oldChar == '\r') {
            lineBuffer.pop_back();
            headers.push_back(lineBuffer);
            lineBuffer = "";
            if (third == '\r') {
               break;  
            }
           
        }
        else {
            lineBuffer+=newChar;
            fourth = third;
            third = oldChar;
            oldChar = newChar;
            length++;
        }
    }
    length--;
    // lineBuffer.pop_back();
    // headers.push_back(lineBuffer);

    if (headers[0].find("GET") == 0) {
        docPath = headers[0].substr(headers[0].find(' ') + 1, headers[0].find_last_of(' ') - headers[0].find(' ') - 1);
        // std::cout << docPath << std::endl;
    }
    std::string authenticate;

    for (int i = 0; i < headers.size(); i++) {
        if (headers[i].find("Authorization") == 0) {
            authenticate = headers[i].substr(headers[i].find("Basic") + 6, headers[i].size() - headers[i].find("Basic") + 6);
        }
    }

    // std::cout << authenticate << std::endl;
    
    
    // length = 0;
    // while((n = read(socket, &newChar, sizeof(newChar)))) {
    //     if (newChar == '\n' && oldChar == '\r') {
    //         break;
    //     }
    //     else {
    //         authenticate+=newChar;
    //         oldChar = newChar;
    //         length++;
    //     }
    // }
    // length--;
    // authenticate.pop_back();
    // // std::cout << authenticate << std::endl;

    // std::string credentials;
    // length = 0;
    // while((n = read(socket, &newChar, sizeof(newChar)))) {
    //     if (newChar == '\n' && oldChar == '\r') {
    //         break;
    //     }
    //     else {
    //         credentials+=newChar;
    //         oldChar = newChar;
    //         length++;
    //     }
    // }
    // length--;
    // credentials.pop_back();
    // // std::cout << credentials << std::endl;

    // // newChar = 0;
    // // oldChar = 0;
    // while((n = read(socket, &newChar, sizeof(newChar)))) {
    //     // std::cout << newChar << std::endl;
    //     if (newChar == '\n' && oldChar == '\r' && 
    //     fourth == '\r' && third == '\n') {
    //         break;
    //     }
    //     else {
    //         fourth = third;
    //         third = oldChar;
    //         oldChar = newChar;
    //     }
    // }

    // std::cout << docPath << std::endl;
    std::string myCredentials = "cs252:password";
    int passwordMatch = 0;
    std::string myConvertedCredentials = "Y3MyNTI6cGFzc3dvcmQ=";
    // std::cout << myConvertedCredentials << std::endl;
    if (authenticate.empty() || (strcmp(myConvertedCredentials.c_str(), authenticate.c_str()) != 0)) {
        // std::cout << "invalidPassword" << std::endl;
        write(socket, "HTTP/1.1 401 Unauthorized", strlen("HTTP/1.1 401 Unauthorized"));
        write(socket, crlf, 2);
        write(socket, "WWW-Authenticate:", strlen("WWW-Authenticate:"));
        write(socket, " ", 1);
        write(socket, "Basic Realm=", strlen("Basic Realm="));
        write(socket, "\"server sundaram\"\n\n", strlen("\"server sundaram\"\n\n"));
        write(socket, crlf, 2);
        write(socket, crlf, 2);
        return;
    }

    
    // Step 3: Map the document path to the real file
    char cwd[256] = {0};

    getcwd(cwd, 256);

    if (cwd == NULL) {
        exit(1);
    }

    if (strncmp(docPath.c_str(), "/icons", 6) == 0) {
        std::cout << "here1: " << docPath << std::endl;
        filePath += "http-root-dir/";
        filePath += docPath;
    }
    else if (strncmp(docPath.c_str(), "/htdocs", 7) == 0) {
        std::cout << "here2 " << docPath << std::endl;
        filePath += "http-root-dir/";
        filePath += docPath;
    }
    else {
        std::cout << "here3 " << filePath << std::endl;
        filePath += "http-root-dir/htdocs";
        filePath += docPath;
    }
    if (strcmp(docPath.c_str(), "/") == 0) {
        std::cout << "here4 " << docPath << std::endl;
        filePath += cwd;
        filePath += "http-root-dir/htdocs/index.html";
    }

    // Step 4: Expand Filepath
    char actualPath[1024] = "";
    char *temp = realpath(filePath.c_str(), actualPath);

    std::string error_str = strcat(cwd, "/http-root-dir/");
    
    if (strlen(actualPath) < error_str.size()) {
        return;
    }

    // Step 5: Determine Content Type
    char *dot = strrchr(actualPath, '.');
    char contentType[1024] = "";

    

    if (dot && ((!strcmp(dot, ".html")) || (!strcmp(dot, ".html/")))) {
        // Case 1: .html file
        strcpy(contentType, "text/html");
    }
        if (dot && ((!strcmp(dot, ".gif")) || (!strcmp(dot, ".gif/")))) {
        // Case 2:  .gif file
        strcpy(contentType, "image/gif");
    }
    if (!dot) {
        // Empty
        strcpy(contentType, "text/plain");
    }
    // std::cout << contentType << std::endl;

    // Step 6: Open the file
    std::cout << actualPath << std::endl;
    FILE *file = fopen(actualPath, "r");

    // std::cout << file << std::endl;

    if (file == NULL) {
        // SEND 404: File not found error
        const char *notFound = "File not Found\n\n";
        write(socket, "HTTP/1.0", strlen("HTTP/1.0"));
        write(socket, " ", 1);
        write(socket, "404", 3);
        write(socket, " ", 1);
        write(socket, "File", 4);
        write(socket, " ", 1);
        write(socket, "Not", 3);
        write(socket, " ", 1);
        write(socket, "Found", 5);
        write(socket, " ", 1);
        write(socket, crlf, 2);
        write(socket, "Server:", 7);
        write(socket, " ", 1);
        write(socket, "Server Sundaram", strlen("Server Sundaram"));
        write(socket, crlf, 2);
        write(socket, "Content-type:", 13);
        write(socket, " ", 1);
        write(socket, contentType, strlen(contentType));
        write(socket, crlf, 2);
        write(socket, crlf, 2);
        write(socket, notFound, strlen(notFound)); 
    }
    else {
        // SEND Actual Reply
        write(socket, "HTTP/1.0", strlen("HTTP/1.0"));
        write(socket, " ", 1);
        write(socket, "200", 3);
        write(socket, " ", 1);
        write(socket, "Document", 8);
        write(socket, " ", 1);
        write(socket, "follows", 7);
        write(socket, crlf, 2);
        write(socket, "Server:", 7);
        write(socket, " ", 1);
        write(socket, "Server Sundaram", strlen("Server Sundaram"));
        write(socket, crlf, 2);
        write(socket, "Content-type:", 13);
        write(socket, " ", 1);
        write(socket, contentType, strlen(contentType));
        write(socket, crlf, 2);
        write(socket, crlf, 2);
        char temp_character;
        
        while (int count = read(fileno(file), &temp_character, sizeof(char))) {
            write(socket, &temp_character, sizeof(char));
            
        }
        write(socket, "\n\n", 2);

    }
}

