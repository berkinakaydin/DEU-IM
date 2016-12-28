#include<stdio.h>
#include<string.h>    // for strlen
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h> // for inet_addr
#include<unistd.h>    // for write
#include<pthread.h>   // for threading, link with lpthread
#include "split.h"

#define MAX_CLIENT_NUMBER 100

void *connection_handler(void *);

struct User searchUser(char searchName[10]);


struct User {
    char userName[10];
    int  clientSocketNo;
};

struct Group {
    char        groupName[20];
    struct User person[10];
    int         groupSize;
};

unsigned int clientNumber    = 0;
unsigned int userArrayIndex  = 0;
unsigned int groupArrayIndex = 0;
struct User  users[MAX_CLIENT_NUMBER];
struct Group group[20];

//FOR GETUSERS COMMAND
void getUsers(int socketNumber) {
    char *userString = malloc(100);
    if (users[0].userName != NULL) {
        strcpy(userString, users[0].userName);
        strcat(userString, ",");
    } else {
        return;
    }

    for (int i = 1; i < userArrayIndex; ++i) {
        strcat(userString, users[i].userName);
        strcat(userString, ",");
    }
    write(socketNumber, userString, strlen(userString) + 1);
}

//AFTER LOGIN COMMAND, ADD TO THE ARRAY
void addUserToArray(char userName[10], int socketNumber) {
    printf("Client logged in as %s\n", userName);
    strcpy(users[userArrayIndex].userName, userName);
    users[userArrayIndex].clientSocketNo = socketNumber;
    userArrayIndex++;
}

//LOGIN COMMAND
void loginUser(char userName[10], int socketNumber) {
    char *message = "login successful";
    write(socketNumber, message, strlen(message) + 1);
    addUserToArray(userName, socketNumber);
}

//SEND MESSAGE IF USER FOUND
void sendMessage(int clientSocketNumber, char *message) {
    write(clientSocketNumber, message, strlen(message) + 1);
}

void sendMessageToGroup(struct Group group, char *message) {
    for (int i = 0; i < group.groupSize; ++i) {
        write(group.person[i].clientSocketNo, message, strlen(message) + 1);
    }
}

void addGroupToArray(char groupName[20], char *persons) {
    char *parsedInput[10];
    int  numberOfPerson = 0;

    strcpy(group[groupArrayIndex].groupName, groupName);
    numberOfPerson = parsing(parsedInput, persons, ",");

    for (int i = 0; i < numberOfPerson; ++i) {
        group[groupArrayIndex].person[i] = searchUser(parsedInput[i]);
    }

    group[groupArrayIndex].groupSize = numberOfPerson;
    groupArrayIndex++;
}

void createGroup(char *groupName, char *persons, int socketNumber) {
    char *message = "Group crated successfully";
    write(socketNumber, message, strlen(message) + 1);
    addGroupToArray(groupName, persons);
}

//SEARCH USER FROM ARRAY
struct User searchUser(char searchName[10]) {
    for (int i = 0; i < userArrayIndex; ++i) {
        if (strcmp(users[i].userName, searchName) == 0) {
            return users[i];
        }
    }
}

struct Group searchGroup(char groupName[10]) {
    for (int i = 0; i < groupArrayIndex; ++i) {
        if (strcmp(group[i].groupName, groupName) == 0) {
            return group[i];
        }
    }
}

int checkLogin(char userName[10]) {
    if (strcmp(searchUser(userName).userName, "") == 0) {
        return 0;
    } else
        return -1;
}

int checkMultiGroup(char groupName[20]) {
    if (strcmp(searchGroup(groupName).groupName, "") == 0) {
        return 0;
    } else
        return -1;
}
    
void *connection_handler(void *socket_desc) {
    //Get the socket descriptor
    char receivedMessage[2000];  //client's message

    int readControl;
    int parsedItemNumber = 0;
    int sock             = *((int *) socket_desc);

    while ((readControl = recv(sock, receivedMessage, 2000, 0)) > 0) {
        char *parsedCommand[50]; //parsedClientMessage

        parsedItemNumber = parsing(parsedCommand, receivedMessage, " ");

        if (strcmp(parsedCommand[0], "login") == 0 && parsedCommand[1] != NULL) {
            int isLogged = checkLogin(parsedCommand[1]);
            if (isLogged == 0)
                loginUser(parsedCommand[1], sock);
            else {
                sendMessage(sock, "You can login only once");
            }

        } else if (strcmp(parsedCommand[0], "getusers") == 0) {
            getUsers(sock);

        } else if (strcmp(parsedCommand[0], "alias") == 0 && parsedCommand[1] != NULL && parsedCommand[2] != NULL) {
            int isCreatedBefore = checkMultiGroup(parsedCommand[1]);
            if (isCreatedBefore == 0) {
                createGroup(parsedCommand[1], parsedCommand[2], sock);
            } else {
                sendMessage(sock, "You can't create same group");
            }


        } else if (strcmp(parsedCommand[0], "exit") == 0) {
            close(sock);
            return 0;
        } else {
            if (parsedCommand[0] != NULL) {
                struct User  find      = searchUser(parsedCommand[0]);
                struct Group findGroup = searchGroup(parsedCommand[0]);

                if (strcmp(find.userName, "") != 0) {
                    char *message = malloc(2000);
                    if (parsedCommand[1] != NULL) {
                        strcpy(message, parsedCommand[1]);
                        strcat(message, " ");
                    }
                    for (int i = 2; i < parsedItemNumber; ++i) {
                        strcat(message, parsedCommand[i]);
                        strcat(message, " ");
                    }
                    sendMessage(find.clientSocketNo, message);
                } else if (strcmp(findGroup.groupName, "") != 0) {

                    char *message = malloc(2000);
                    if (parsedCommand[1] != NULL) {
                        strcpy(message, parsedCommand[1]);
                        strcat(message, " ");
                    }
                    for (int i = 2; i < parsedItemNumber; ++i) {
                        strcat(message, parsedCommand[i]);
                        strcat(message, " ");
                    }

                    sendMessageToGroup(findGroup, message);
                } else {
                    perror("Your input was wrong");
                }
            }
        }
    }

    if (readControl == 0) {
        puts("Client disconnected");
        clientNumber--;
        fflush(stdout);
    } else if (readControl == -1) {
        perror("recv failed");
    }

    //Free the socket pointer
    free(socket_desc);
    return 0;
}

int main(int argc, char *argv[]) {

    int                socket_desc, new_socket, c, *new_sock;
    struct sockaddr_in server, client;

    //Create Socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        puts("Could not create socket");
        return 1;
    }

    server.sin_family      = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port        = htons(8888);

    if (bind(socket_desc, (struct sockaddr *) &server, sizeof(server)) < 0) {
        puts("Binding failed");
        return 1;
    }

    listen(socket_desc, 3);

    puts("Server started");
    c = sizeof(struct sockaddr_in);

    while ((new_socket = accept(socket_desc, (struct sockaddr *) &client, (socklen_t *) &c)) &&
           clientNumber < MAX_CLIENT_NUMBER) {

        pthread_t sniffer_thread[MAX_CLIENT_NUMBER];
        new_sock = malloc(1);
        *new_sock = new_socket;

        if (pthread_create(&sniffer_thread[clientNumber], NULL, connection_handler,
                           (void *) new_sock) < 0) {
            perror("Could not create thread");
            return 1;
        } else {
            clientNumber++;
        }

        puts("Client connected");
    }


    if (new_socket < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}