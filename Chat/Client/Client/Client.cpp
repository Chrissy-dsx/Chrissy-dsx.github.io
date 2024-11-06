#define _CRT_SECURE_NO_WARNINGS 1
#include <iostream>
#include <string>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <thread>
#include<string>
#include <cstdlib> // contain system function header file

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT 5019
#define DEFAULT_BUFLEN 512
std::string username;
std::atomic<bool> shouldExit(false); // shared flag variable
void receiveMessages(SOCKET socket) {
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    while (true) {
        int bytesReceived = recv(socket, recvbuf, recvbuflen, 0);
        if (bytesReceived > 0) {
            recvbuf[bytesReceived] = '\0';
            std::cout << recvbuf << std::endl;
        }
        else if (bytesReceived == 0) {
            std::cout << "Connection closed by server." << std::endl;
            break;
        }
        else {
            std::cerr << "Recv failed: " << WSAGetLastError() << std::endl;
            break;
        }
        if (shouldExit) {
            break;
        }
    }

}

bool validLogin(SOCKET socket) {
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    while (true) {
        int bytesReceived = recv(socket, recvbuf, recvbuflen, 0);
        if (bytesReceived > 0) {
            recvbuf[bytesReceived] = '\0';
            if (strcmp(recvbuf, "LOGIN_SUCCESS") == 0) {
                std::cout << "Login successful!" << std::endl;
                return true;
            }
            else if (strcmp(recvbuf, "LOGIN_FAILURE") == 0) {
                std::cout << "Login failed. Please try again." << std::endl;
                return false;
            }

            else {
                std::cerr << recvbuf << std::endl;
                return false;
            }

        }
        else if (bytesReceived == 0) {
            std::cout << "Connection closed by server." << std::endl;
            break;
        }
        else {
            std::cerr << "Recv failed: " << WSAGetLastError() << std::endl;
            break;
        }
    }
}

void login(SOCKET socket) {
    std::string password;

    // prompt the user for an account number and password
    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter password: ";
    std::cin >> password;

    // send a login request to the server
    std::string message = "LOGIN " + username + " " + password;
    send(socket, message.c_str(), message.length(), 0);

}


bool registerUser(SOCKET socket) {
    std::string username, password, confirmPassword;

    while (true) {
        // get user name
        std::cout << "Enter username: ";
        std::cin >> username;
        if (username == "exit")
            break;

        // get password
        std::cout << "Enter password: ";
        std::cin >> password;
        if (password == "exit")
            break;

        // confirm password
        std::cout << "Confirm password: ";
        std::cin >> confirmPassword;
        if (confirmPassword == "exit")
            break;

        // check whether the passwords are consistent
        if (password == confirmPassword) {
            break; // if same password, out of the loop
        }
        else {
            std::cout << "Passwords do not match. Please try again.\n";
            break;
        }
    }

    // send a registration request to the server
    std::string message = "REGISTER " + username + " " + password;
    send(socket, message.c_str(), message.length(), 0);

    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    while (true) {
        int bytesReceived = recv(socket, recvbuf, recvbuflen, 0);
        if (bytesReceived > 0) {
            recvbuf[bytesReceived] = '\0';
            if (strcmp(recvbuf, "Registration successful.") == 0) {
                std::cout << "Registration successful!" << std::endl;
                return true;
            }
            else if (strcmp(recvbuf, "Username already exists.Please try again.") == 0) {
                std::cout << "Username already exists.Please try again." << std::endl;
                return false;
            }

            else {
                std::cerr << recvbuf << std::endl;
                return false;
            }

        }
        else if (bytesReceived == 0) {
            std::cout << "Connection closed by server." << std::endl;
            break;
        }
        else {
            std::cerr << "Recv failed: " << WSAGetLastError() << std::endl;
            break;
        }
    }
    return false;
}

void sendMessage(SOCKET socket) {
    // send message in loop
    while (true) {
        std::string message;
        std::getline(std::cin, message);

        // check for exit
        if (message == "exit")
            break;

        // send a message
        send(socket, message.c_str(), message.length(), 0);


    }
}
void sendPublicMessage(SOCKET socket) {
    while (true) {
        // get user input
        std::string message;
        std::cout << "Send public message (or type 'exit' to quit):";
        std::getline(std::cin, message);

        // check for exit
        if (message == "exit")
            break;

        // send message
        send(socket, message.c_str(), message.length(), 0);
    }
}

void sendPrivateMessage(SOCKET socket) {
    // send message in loop
    while (true) {
        // get user input
        std::string message;
        std::getline(std::cin, message);

        // check for exit
        if (message == "exit")
            break;
        message = "PRIVATE " + message;
        // send message
        send(socket, message.c_str(), message.length(), 0);
        std::cout << "Send private message, input receiver name + private information, input exit to get back:";
    }
}

void sendGroupMessage(SOCKET socket) {
    // send message in loop
    while (true) {
        // get user input
        std::string message;
        std::getline(std::cin, message);

        // check for exit
        if (message == "exit")
            break;
        message = "GROUP " + username + " " + message;
        // send message
        send(socket, message.c_str(), message.length(), 0);
        std::cout << "Send group message, input exit if you want to go back, otherwise input group name + message:";
    }
}

void createGroup(SOCKET socket) {
    while (true) {
        // get user input
        std::string message;
        std::getline(std::cin, message);

        // check for exit
        if (message == "exit")
            break;
        message = "CREATE " + message;
        // send message
        send(socket, message.c_str(), message.length(), 0);
        std::cout << "Please input the group name: member1 member2 ...(separated by spaces) (or type 'exit' to quit): ";
    }
}

void dissolveGroup(SOCKET socket) {
    while (true) {
        // get user input
        std::string message;
        std::getline(std::cin, message);

        // check for exit
        if (message == "exit")
            break;
        message = "DISSOLVE " + username + " " + message;
        // send message
        send(socket, message.c_str(), message.length(), 0);
        std::cout << "Here you can dissolve the group, input the group name (or type 'exit' to quit):";
    }
}

void deleteUser(SOCKET socket) {
    // send a message
    std::string message = "DELETE " + username;
    send(socket, message.c_str(), message.length(), 0);
}

int main() {
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;

    // initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // create Socket
    ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ConnectSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // set server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DEFAULT_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr); // replace it with the server IP address

    // connect to the server
    if (connect(ConnectSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    bool isLoggedIn = false; // user login status
    while (1) {
        std::cout << "Select an option: \n";
        std::cout << "1. Login\n";
        std::cout << "2. Register\n";
        std::cout << "3. Exit\n";

        int option;
        std::cin >> option;
        if (option == 1) {
            login(ConnectSocket);
        }
        else if (option == 2) {
            registerUser(ConnectSocket);
            continue;
        }
        else if (option == 3) {
            closesocket(ConnectSocket);
            WSACleanup();
            return 0;
        }
        else {
            std::cout << "Invalid option.\n";
        }
        if (validLogin(ConnectSocket)) {
            break;
        }
    }
    // create a thread to receive messages
    std::thread receiveThread(receiveMessages, ConnectSocket);
    // choose
    while (1) {
        std::cout << "Select the option:" << std::endl;
        std::cout << "1. Public chat" << std::endl;
        std::cout << "2. Private chat" << std::endl;
        std::cout << "3. Group chat" << std::endl;
        std::cout << "4. Create a group chat" << std::endl;
        std::cout << "5. Dissolve a group chat" << std::endl;
        std::cout << "6. Delete the current user" << std::endl;
        std::cout << "7. Exit" << std::endl;
        int option2;
        std::cin >> option2;

        if (option2 == 1) {
            // public chat
            sendPublicMessage(ConnectSocket);
            continue;
        }
        if (option2 == 2) {
            // private chat
            sendPrivateMessage(ConnectSocket);
            continue;
        }
        if (option2 == 3) {
            // group chat
            sendGroupMessage(ConnectSocket);
            continue;
        }
        else if (option2 == 4) {
            createGroup(ConnectSocket);
        }
        else if (option2 == 5) {
            dissolveGroup(ConnectSocket);
        }
        else if (option2 == 6) {
            deleteUser(ConnectSocket);
            std::cout << "Logging out and rebooting...\n";
            shouldExit = true; // set exit flag
            isLoggedIn = false; // reset login status
            closesocket(ConnectSocket);
            WSACleanup();
            exit(0);
        }
        else if (option2 == 7) {
            std::cout << "Logging out and rebooting...\n";
            shouldExit = true; // set exit flag
            isLoggedIn = false; // reset login status
            closesocket(ConnectSocket);
            WSACleanup();
            exit(0);
        }
        else {
            std::cout << "Invalid operator.\n";
        }
    }

    /// close Socket
    closesocket(ConnectSocket);
    WSACleanup();

    // wait for the receiving thread to end  
    receiveThread.join();

    return 0;
}