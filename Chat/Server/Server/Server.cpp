#include <iostream>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <thread>
#include <vector>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <fstream>
#include<string>
#include <algorithm>
#include <map> // contain map header file
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT	5019
#define DEFAULT_BUFLEN 512

std::vector<SOCKET> clientSockets;
std::mutex mtx; // a mutex for reading and writing files
std::map<SOCKET, std::string> clientUsernames; // map client identifiers and sockets
std::map<std::string, std::vector<std::string>> groupMembers;// group member mapping

void sendMsg(SOCKET socket, const std::string& msg) {
    send(socket, msg.c_str(), msg.length(), 0);
}

void updateGroup() {
    while (1) {
        groupMembers.clear();
        std::ifstream infile("groupChat.txt");
        std::string line;
        while (std::getline(infile, line)) {
            // use stringstream to split the contents of each line
            std::istringstream iss(line);
            std::string groupName;
            if (!(iss >> groupName)) { // read the group name, excluding colons
                break;
            }
            groupName.erase(std::remove(groupName.begin(), groupName.end(), ':'), groupName.end()); // delete colons
            std::string username;
            std::vector<std::string> groupUsers;
            while (iss >> username) { // read group member
                groupUsers.push_back(username);
            }
            // store group name and member list in groupMembers
            groupMembers[groupName] = groupUsers;
        }
        infile.close();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "update group\n";
    }

}

void broadcastMessage(const std::string& message, SOCKET senderSocket) {
    for (SOCKET s : clientSockets) {
        if (s != senderSocket) {
            sendMsg(s, "Public message: " + message);
        }
    }
}

void processClient(SOCKET clientSocket, sockaddr_in clientAddr) {
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);

    std::cout << "Client connected: " << clientIP << ":" << ntohs(clientAddr.sin_port) << "\n";

    // the connection is accepted for data exchange or other processing
    std::string username;
    char recvbuf[DEFAULT_BUFLEN];
    int bytesReceived;

    do {
        bytesReceived = recv(clientSocket, recvbuf, DEFAULT_BUFLEN, 0);

        if (bytesReceived > 0) {
            recvbuf[bytesReceived] = '\0';
            std::cout << "Received message: " << recvbuf << std::endl;
            std::string message = recvbuf;

            if (message.substr(0, 8) == "REGISTER") {
                std::string registerInfo = message.substr(9); // get user name and password
                size_t spaceIndex = registerInfo.find(' ');
                std::string username = registerInfo.substr(0, spaceIndex);
                std::string password = registerInfo.substr(spaceIndex + 1);

                // check whether the user name has been registered
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    std::ifstream inFile("registeredUsers.txt");
                    std::string line;
                    bool isRegistered = false;
                    while (std::getline(inFile, line)) {
                        size_t pos = line.find(' ');
                        std::string user = line.substr(0, pos);
                        if (user == username) {
                            isRegistered = true;
                            break;
                        }
                    }
                    inFile.close();

                    if (isRegistered) {
                        sendMsg(clientSocket, "Username already exists.Please try again.");
                    }
                    else {
                        // user registration
                        std::ofstream outFile("registeredUsers.txt", std::ios::app);
                        outFile << username << " " << password << "\n";
                        outFile.close();
                        sendMsg(clientSocket, "Registration successful.");
                    }
                }
            }
            else if (message.substr(0, 5) == "LOGIN") {
                std::string loginInfo = message.substr(6); // get user name and password
                size_t spaceIndex = loginInfo.find(' ');
                username = loginInfo.substr(0, spaceIndex);
                std::string username = loginInfo.substr(0, spaceIndex);
                std::string password = loginInfo.substr(spaceIndex + 1);

                // check whether the user name and password match
                std::lock_guard<std::mutex> lock(mtx);
                std::ifstream inFile("registeredUsers.txt");
                std::string line;
                bool isAuthenticated = false;
                while (std::getline(inFile, line)) {
                    size_t pos = line.find(' ');
                    std::string storedUsername = line.substr(0, pos);
                    std::string storedPassword = line.substr(pos + 1);
                    if (storedUsername == username && storedPassword == password) {
                        isAuthenticated = true;
                        break;
                    }
                }
                inFile.close();

                if (isAuthenticated) {
                    // if login succeeds, the message is sent to the client
                    sendMsg(clientSocket, "LOGIN_SUCCESS");
                    clientUsernames[clientSocket] = username; // associate the user name with the client socket
                }
                else {
                    // if login failed, also send a failure message
                    sendMsg(clientSocket, "LOGIN_FAILURE");
                }
            }
            else if (message.substr(0, 7) == "PRIVATE") {
                size_t spacePos = message.find(' ');
                size_t spacePos2 = message.find(' ', spacePos + 1);
                std::string receiverUsername = message.substr(spacePos + 1, spacePos2 - spacePos - 1);
                std::string privateMessage = message.substr(spacePos2 + 1);
                if (message.length() > 9 + receiverUsername.length()) {
                    // finds the corresponding socket based on the recipient user name
                    bool findReceiver = false;
                    for (const auto& pair : clientUsernames) {
                        if (pair.second == receiverUsername) {
                            // find the receiver socket and send the private chat message
                            sendMsg(pair.first, "PRIVATE MESSAGE from " + clientUsernames[clientSocket] + ": " + privateMessage);
                            findReceiver = true;
                        }
                    }
                    if (!findReceiver) {
                        sendMsg(clientSocket, "Can't find the receiver now.");
                    }
                }
                else {
                    std::cerr << "Error: Insufficient data in message" << std::endl;
                    sendMsg(clientSocket, " ");
                }
            }
            else if (message.substr(0, 5) == "GROUP") {
                // find the location of the first space
                size_t firstSpaceIndex = message.find(' ');
                if (firstSpaceIndex != std::string::npos) {
                    // find the location of the second space
                    size_t secondSpaceIndex = message.find(' ', firstSpaceIndex + 1);
                    if (secondSpaceIndex != std::string::npos) {
                        // find the location of the third space
                        size_t thirdSpaceIndex = message.find(' ', secondSpaceIndex + 1);
                        if (thirdSpaceIndex != std::string::npos) {
                            // extract the group chat name, user name, and message content
                            std::string username = message.substr(firstSpaceIndex + 1, secondSpaceIndex - firstSpaceIndex - 1); // intercept the group name
                            std::string groupName = message.substr(secondSpaceIndex + 1, thirdSpaceIndex - secondSpaceIndex - 1); // intercept the user name
                            std::string groupMessage = message.substr(thirdSpaceIndex + 1); // intercept the message content

                            // check whether the client is in the specified group
                            auto it = groupMembers.find(groupName);
                            if (it != groupMembers.end()) {
                                const std::vector<std::string>& groupUsers = it->second;
                                bool isInGroup = false;
                                for (const std::string& user : groupUsers) {
                                    if (user == username) {
                                        isInGroup = true;
                                        break;
                                    }
                                }

                                if (!isInGroup) {
                                    sendMsg(clientSocket, "You are not in Group " + groupName + ".");
                                }
                                else {
                                    // send a message to each member of the group
                                    for (const std::string& user : groupUsers) {
                                        // finds the corresponding socket based on the user name and sends a message to it
                                        for (const auto& pair : clientUsernames) {
                                            if (pair.second == user) {
                                                // find the receiver socket and send the group message
                                                sendMsg(pair.first, "Group Message from " + clientUsernames[clientSocket] + " (Group " + groupName + "): " + groupMessage);
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            else {
                                sendMsg(clientSocket, "Group " + groupName + " does not exist.");
                            }
                        }
                        else {
                            sendMsg(clientSocket, "Invalid message format now.");
                        }
                    }
                    else {
                        sendMsg(clientSocket, "Invalid message format now.");
                    }
                }
                else {
                    sendMsg(clientSocket, "Invalid message format now.");
                }
            }
            else if (message.substr(0, 6) == "CREATE") {
                if (message.length() > 7) {
                    std::string groupInfo = message.substr(7);  // get the group name and user name
                    std::cout << "Debug: Group Info: " << groupInfo << std::endl;
                    size_t colonIndex = groupInfo.find(':');
                    if (colonIndex != std::string::npos) {
                        std::string groupName = groupInfo.substr(0, colonIndex);
                        std::string members = groupInfo.substr(colonIndex + 2);  // skip the part after ":"
                        std::cout << "Debug: Group Name: " << groupName << std::endl;
                        std::cout << "Debug: Members: " << members << std::endl;

                        // check whether the group name already exists
                        {
                            std::lock_guard<std::mutex> lock(mtx);
                            std::ifstream inFile("groupChat.txt");
                            if (!inFile.is_open()) {
                                std::cerr << "Error: Unable to open file groupChat.txt" << std::endl;
                                sendMsg(clientSocket, "Server error. Please try again later.");
                                return;
                            }
                            std::string line;
                            bool isGroupExists = false;
                            while (std::getline(inFile, line)) {
                                size_t pos = line.find(':');
                                std::string existingGroupName = line.substr(0, pos);
                                if (existingGroupName == groupName) {
                                    isGroupExists = true;
                                    break;
                                }
                            }
                            inFile.close();

                            if (isGroupExists) {
                                sendMsg(clientSocket, "Group name already exists. Please try again.");
                            }
                            else {
                                // create a group chat
                                std::ofstream outFile("groupChat.txt", std::ios::app);
                                if (!outFile.is_open()) {
                                    std::cerr << "Error: Unable to open file groupChat.txt for writing" << std::endl;
                                    sendMsg(clientSocket, "Server error. Please try again later.");
                                    return;
                                }
                                outFile << groupName << ": " << members << "\n";
                                outFile.close();
                                // read group message from a text file
                                std::ifstream infile("groupChat.txt");
                                std::string line;
                                while (std::getline(infile, line)) {
                                    // Use stringstream to split the contents of each line
                                    std::istringstream iss(line);
                                    std::string groupName;
                                    if (!(iss >> groupName)) { // read the group name, excluding colons
                                        break;
                                    }
                                    groupName.erase(std::remove(groupName.begin(), groupName.end(), ':'), groupName.end()); // delete colon
                                    std::string username;
                                    std::vector<std::string> groupUsers;
                                    while (iss >> username) { // read group member
                                        groupUsers.push_back(username);
                                    }
                                    // store the group name and member list in groupMembers
                                    groupMembers[groupName] = groupUsers;
                                }
                                infile.close();
                                sendMsg(clientSocket, "Group chat created successfully.");
                            }
                        }
                    }
                    else {
                        std::cerr << "Error: Invalid group info format" << std::endl;
                        sendMsg(clientSocket, "Invalid group info format. Please try again.");
                    }
                }
                else {
                    std::cerr << "Error: Insufficient data in message" << std::endl;
                    sendMsg(clientSocket, " ");
                }
            }
            else if (message.substr(0, 8) == "DISSOLVE") {

                std::string dissolveInfo = message.substr(9);  // get the group name and user name
                size_t spaceIndex = dissolveInfo.find(' ');
                std::string username = dissolveInfo.substr(0, spaceIndex);
                std::string groupName = dissolveInfo.substr(spaceIndex + 1);
                if (message.length() > 10 + username.length()) {
                    // check that the group name exists and that the user is in the group
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        std::ifstream inFile("groupChat.txt");
                        std::ofstream tempFile("tempGroupChat.txt");
                        std::string line;
                        bool groupFound = false;
                        bool userInGroup = false;

                        while (std::getline(inFile, line)) {
                            size_t pos = line.find(':');
                            std::string existingGroupName = line.substr(0, pos);

                            if (existingGroupName == groupName) {
                                groupFound = true;
                                std::string members = line.substr(pos + 2);  // skip the part after ":"
                                if (members.find(username) != std::string::npos) {
                                    userInGroup = true;
                                    continue; // not writing to tempFile is equivalent to deleting
                                }
                            }
                            tempFile << line << "\n";
                        }

                        inFile.close();
                        tempFile.close();

                        if (groupFound && userInGroup) {
                            // delete groupChat.txt and rename to tempGroupChat.txt to groupChat.txt
                            std::remove("groupChat.txt");
                            std::rename("tempGroupChat.txt", "groupChat.txt");
                            sendMsg(clientSocket, "Group dissolved successfully.");
                        }
                        else if (!groupFound) {
                            std::remove("tempGroupChat.txt");
                            sendMsg(clientSocket, "Group not found. Please try again.");
                        }
                        else if (groupFound && !userInGroup) {
                            std::remove("tempGroupChat.txt");
                            sendMsg(clientSocket, "You are not a member of this group.");
                        }
                    }
                }
                else {
                    std::cerr << "Error: Insufficient data in message" << std::endl;
                    sendMsg(clientSocket, " ");
                }
            }
            else if (message.substr(0, 6) == "DELETE") {
                std::cout << "delete";
                std::ifstream inFile("registeredUsers.txt");
                std::ofstream tempFile("temp_user_info.txt");
                std::string line;
                bool userFound = false;

                while (std::getline(inFile, line)) {
                    size_t pos = line.find(' ');
                    std::string existingUsername = line.substr(0, pos);

                    if (existingUsername == username) {
                        userFound = true;
                        continue; //don't write into temp_user_info.txt, as deleted
                    }
                    tempFile << line << "\n";
                }

                inFile.close();
                tempFile.close();

                if (userFound) {
                    // delete user_info.txt and rename temp_user_info.txt as user_info.txt
                    std::remove("registeredUsers.txt");
                    std::rename("temp_user_info.txt", "registeredUsers.txt");
                    sendMsg(clientSocket, "Delete successful");
                }
                else {
                    std::remove("temp_user_info.txt");
                    sendMsg(clientSocket, "User doesn't exist");
                }
            }
            else {
                std::string replyMessage = username + " : ";
                replyMessage += recvbuf;
                broadcastMessage(replyMessage, clientSocket);
            }
        }
        else if (bytesReceived == 0) {
            std::cout << "Client disconnected.\n";
        }
        else {
            std::cerr << "Recv failed with error: " << WSAGetLastError() << std::endl;
        }
    } while (bytesReceived > 0);

    closesocket(clientSocket);
    std::cout << "Client disconnected.\n";
}

int main() {
    WSADATA wsaData;

    //creat groupChat.txt
    std::ofstream outfile("groupChat.txt");
    outfile.close();

    // read group information from a text file
    std::ifstream infile("groupChat.txt");
    std::string line;
    while (std::getline(infile, line)) {
        // use stringstream to split the contents of each line
        std::istringstream iss(line);
        std::string groupName;
        if (!(iss >> groupName)) { // read the group name, excluding colons
            break;
        }
        groupName.erase(std::remove(groupName.begin(), groupName.end(), ':'), groupName.end()); // delete colon
        std::string username;
        std::vector<std::string> groupUsers;
        while (iss >> username) { // read group member
            groupUsers.push_back(username);
        }
        // store the group name and member list in groupMembers
        groupMembers[groupName] = groupUsers;
    }
    infile.close();

    // create a thread to update the group chat list
    std::thread updateGroupThread(updateGroup);
    updateGroupThread.detach();

    if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR) {
        std::cerr << "WSAStartup failed with error " << WSAGetLastError() << "\n";
        WSACleanup();
        return -1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(DEFAULT_PORT);

    if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port " << DEFAULT_PORT << "...\n";



    while (true) {
        SOCKET clientSocket;
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed: " << WSAGetLastError() << "\n";
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }

        clientSockets.push_back(clientSocket);

        std::thread clientThread(processClient, clientSocket, clientAddr);
        clientThread.detach(); // separate threads so that the host thread does not have to wait for the processing thread to complete
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}