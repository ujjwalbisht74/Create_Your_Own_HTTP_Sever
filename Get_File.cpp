#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>

constexpr int BUFFER_SIZE = 512;
constexpr int SERVER_PORT = 4221;

std::string directory;

int createServerSocket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

void setSocketOptions(int server_fd) {
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        exit(EXIT_FAILURE);
    }
}

void bindServerSocket(int server_fd) {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port " << SERVER_PORT << std::endl;
        exit(EXIT_FAILURE);
    }
}

int acceptClientConnection(int server_fd) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    std::cout << "Waiting for a client to connect...\n";
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
    if (client_fd < 0) {
        std::cerr << "Failed to connect with client\n";
        exit(EXIT_FAILURE);
    }
    return client_fd;
}

std::string receiveData(int client_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_received < 0) {
        std::cerr << "Failed to receive data from client\n";
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    return std::string(buffer, bytes_received);
}

void sendData(int client_fd, const std::string& data) {
    int bytes_sent = send(client_fd, data.c_str(), data.size(), 0);
    if (bytes_sent < 0) {
        std::cerr << "Failed to send data to client\n";
    } else {
        std::cout << "Sent " << bytes_sent << " bytes to client." << std::endl;
    }
}

std::string extractPath(const std::string& request) {
    int i = request.find_first_of('/');
    std::string path = request.substr(i);
    int j = path.find_first_of(' ');
    return path.substr(0, j);
}

void handleRequest(int client_fd) {
    std::string request = receiveData(client_fd);
    std::string path = extractPath(request);

    std::string OK = "HTTP/1.1 200 OK";
    std::string NOT_FOUND = "HTTP/1.1 404 Not Found\r\n\r\n";
    std::string OCTET_STREAM = "Content-Type: application/octet-stream";
    std::string CONTENT_LENGTH = "Content-Length: ";
    std::string files_prefix = "/files/";

    int bytes_sent;

    if (path.compare(0, files_prefix.size(), files_prefix) == 0) {
        std::string file_path = directory + path.substr(files_prefix.size());

        std::ifstream file(file_path, std::ios::binary);
        if (file) {
            std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            std::string payload = OK + "\r\n" + OCTET_STREAM + "\r\n" + CONTENT_LENGTH +
                                  std::to_string(contents.size()) + "\r\n\r\n" + contents;

            bytes_sent = send(client_fd, payload.c_str(), payload.size(), 0);
            file.close();
        } else {
            bytes_sent = send(client_fd, NOT_FOUND.c_str(), NOT_FOUND.size(), 0);
        }
    } else {
        bytes_sent = send(client_fd, NOT_FOUND.c_str(), NOT_FOUND.size(), 0);
    }

    if (bytes_sent < 0) {
        std::cerr << "Failed to send data to client\n";
    } else {
        std::cout << "Sent " << bytes_sent << " bytes to client." << std::endl;
    }

    close(client_fd);
}

void handleClient(int client_fd) {
    handleRequest(client_fd);
    close(client_fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3 || std::string(argv[1]) != "--directory") {
        std::cerr << "Usage: " << argv[0] << " --directory <directory>\n";
        return EXIT_FAILURE;
    }
    
    directory = argv[2];

    int server_fd = createServerSocket();
    setSocketOptions(server_fd);
    bindServerSocket(server_fd);

    if (listen(server_fd, 5) != 0) {
        std::cerr << "listen failed\n";
        exit(EXIT_FAILURE);
    }

    while (true) {
        int client_fd = acceptClientConnection(server_fd);

        // Create a new thread to handle the client connection
        std::thread client_thread(handleClient, client_fd);
        
        // Detach the thread so that it can run independently
        client_thread.detach();
    }

    close(server_fd);
    return 0;
}
