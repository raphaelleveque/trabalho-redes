#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

const int PORT = 8080;
const int BUFFER_SIZE = 4096;

int createClientSocket() {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Erro ao criar o socket cliente." << std::endl;
        exit(1);
    }
    return clientSocket;
}

void connectToServer(int clientSocket) {
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &(serverAddress.sin_addr)) <= 0) {
        std::cerr << "Endereço inválido ou não suportado." << std::endl;
        exit(1);
    }

    if (connect(clientSocket, reinterpret_cast<const sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        std::cerr << "Erro ao conectar ao servidor." << std::endl;
        exit(1);
    }

    std::cout << "Conexão estabelecida com o servidor." << std::endl;
}

void sendMessage(int clientSocket, const std::string& message) {
    if (send(clientSocket, message.c_str(), message.size(), 0) < 0) {
        std::cerr << "Erro ao enviar a mensagem." << std::endl;
        exit(1);
    }
}

std::string receiveMessage(int clientSocket) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    if (recv(clientSocket, buffer, BUFFER_SIZE, 0) < 0) {
        std::cerr << "Erro ao receber a resposta." << std::endl;
        exit(1);
    }

    return std::string(buffer);
}

void closeConnection(int clientSocket) {
    close(clientSocket);
    std::cout << "Conexão encerrada." << std::endl;
}

int main() {
    int clientSocket = createClientSocket();
    connectToServer(clientSocket);

    std::string message;
    while (true) {
        std::cout << "Digite uma mensagem ('q' para sair): ";
        std::getline(std::cin, message);

        if (message == "q") {
            break;
        }

        sendMessage(clientSocket, message);

        std::string response = receiveMessage(clientSocket);
        std::cout << "Resposta do servidor: " << response << std::endl;
    }

    closeConnection(clientSocket);

    return 0;
}
