#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <unordered_map>
#include "protocol.pb.h"

int randomfunc(int j)
{
    return rand() % j;
}

struct Client
{
    int socketFd;
    std::string username;
    char ipAddr[INET_ADDRSTRLEN];
    int room;
};

struct Room
{
    int roomNo;
    std::vector<std::string> roomDeck;
    std::vector<std::string> users;
    int counter;
};


// Lista de clientes
std::unordered_map<std::string, Client *> clients;
Room RoomOne;
Room RoomTwo;

std::vector<Room> allRooms {RoomOne, RoomTwo};

// Lista de cartas:
std::vector<std::string> deck {
    "A-D","2-D","3-D","4-D","5-D","6-D","7-D","8-D","9-D","10-D","J-D","Q-D","K-D",
    "A-P","2-P","3-P","4-P","5-P","6-P","7-P","8-P","9-P","10-P","J-P","Q-P","K-P",
    "A-C","2-C","3-C","4-C","5-C","6-C","7-C","8-C","9-C","10-C","J-C","Q-C","K-C",
    "A-E","2-E","3-E","4-E","5-E","6-E","7-E","8-E","9-E","10-E","J-E","Q-E","K-E",
    "Joker-Negro","Joker-Rojo"
};


// Funcion que manda errores
void SendErrorResponse(int socketFd, std::string errorMsg)
{
    std::string msgSerialized;
    protocol::ErrorResponse *errorMessage = new protocol::ErrorResponse();
    errorMessage->set_errormessage(errorMsg);
    protocol::ServerMessage serverMessage;
    serverMessage.set_option(2);
    serverMessage.set_allocated_error(errorMessage);
    serverMessage.SerializeToString(&msgSerialized);
    char buffer[msgSerialized.size() + 1];
    strcpy(buffer, msgSerialized.c_str());
    send(socketFd, buffer, sizeof buffer, 0);
}

void *ThreadWork(void *params)
{
    // Cliente actual
    struct Client thisClient;
    struct Client *newClientParams = (struct Client *)params;
    int socketFd = newClientParams->socketFd;
    char buffer[8192];

    // Estructura de los mensajes
    std::string msgSerialized;
    protocol::ClientMessage messageReceived;

    while(1)
    {
        // Si no se recive response del cliente este cerro sesion
        if (recv(socketFd, buffer, 8192, 0) < 1)
        {
            if (recv(socketFd, buffer, 8192, 0) == 0)
            {
                // cliente cerro conexion
                std::cout << "Servidor: el cliente "
                          << thisClient.username
                          << " ha cerrado su sesión."
                          << std::endl;
            }
            break;
        }
        // Parsear mensaje recibido de cliente
        messageReceived.ParseFromString(buffer);
        // Manejo de servicios
        // Registro de clientes
        if (messageReceived.option() == 1)
        {
            protocol::ClientConnect myInfo = messageReceived.connect();

            std::cout << "Servidor: se recibió informacion de: "
                << myInfo.username()
                << std::endl;

            // Revisar si nombre de usuario ya existe, si ya existe, se devuelver un error.
            if (clients.count(myInfo.username()) > 0)
            {
                std::cout << "Servidor: el nombre de usuario ya existe." << std::endl;
                SendErrorResponse(socketFd, "El nombre de usuario ya existe.");
                break;
            }

            // Send available rooms
            std::string all_rooms = "";
            std::string room_availabe = "";

            for (auto &it : allRooms)
            {
                all_rooms = all_rooms + "-Id: " + std::to_string(it.roomNo) + " Espacios " + std::to_string(it.users.size()) + "/4\n";
                if (it.users.size() < 4)
                {
                    room_availabe = room_availabe+std::to_string(it.roomNo);
                }
            }

            // Escribir el mensaje de rooms
            char buffer[8192];
            std::string message_serialized;

            protocol::RoomsToJoin *rooms = new protocol::RoomsToJoin();
            rooms->set_rooms(all_rooms);
            rooms->set_roomsjoin(room_availabe);

            protocol::ServerMessage response;
            response.set_option(1);
            response.set_allocated_rooms(rooms);
            response.SerializeToString(&message_serialized);
            strcpy(buffer, message_serialized.c_str());
            send(socketFd, buffer, message_serialized.size() + 1, 0);


            // Log del server
            std::cout << "Servidor:  usuario agregado exitosamente con id: "
                      << socketFd
                      << std::endl;

            // Guardar informacion de nuevo cliente
            thisClient.username = myInfo.username();
            thisClient.socketFd = socketFd;
            thisClient.room = 0;
            strcpy(thisClient.ipAddr, newClientParams->ipAddr);
            clients[thisClient.username] = &thisClient;
        }
        if (messageReceived.option() == 2)
        {
            protocol::JoinRoom JoinRoom = messageReceived.roomjoin();
            thisClient.room = JoinRoom.room();
            allRooms[JoinRoom.room()-1].users.insert(allRooms[JoinRoom.room()-1].users.end(), thisClient.username);
            printf("Usuarios en room %d:\n", JoinRoom.room());
            for (auto it = allRooms[JoinRoom.room()-1].users.begin(); it != allRooms[JoinRoom.room()-1].users.end(); ++it)
                std::cout << "-"<< *it ;
            std::cout << "\n";

            // Notificar que se unio un compa a todos de la room
            char buffer[8192];
            std::string message_serialized;
            protocol::Notification *noti = new protocol::Notification();
            noti->set_notimessage("El usuario "+thisClient.username+" Se ha conectado a la sala. "+ std::to_string(allRooms[JoinRoom.room()-1].users.size()) + "/4");

            protocol::ServerMessage response;
            response.set_option(3);
            response.set_allocated_noti(noti);
            response.SerializeToString(&message_serialized);
            strcpy(buffer, message_serialized.c_str());

            for (auto it = allRooms[JoinRoom.room()-1].users.begin(); it != allRooms[JoinRoom.room()-1].users.end(); ++it)
                if (*it != thisClient.username){
                    send(clients[*it]->socketFd, buffer, message_serialized.size() + 1, 0);
                }

            if (allRooms[JoinRoom.room()-1].users.size() == 4){
                //Todo Start Game
            }
        }
        if (messageReceived.option() == 3)
        {
            // Mandar el mensaje a todos los participantes menos a si mismo
            char buffer[8192];
            std::string message_serialized;
            
            protocol::Notification *noti = new protocol::Notification();
            noti->set_notimessage("El usuario "+thisClient.username+" Ha enviado un mensaje: "+ messageReceived.msgroom().message());

            protocol::ServerMessage response;
            response.set_option(3);
            response.set_allocated_noti(noti);
            response.SerializeToString(&message_serialized);
            strcpy(buffer, message_serialized.c_str());

            for (auto it = allRooms[thisClient.room-1].users.begin(); it != allRooms[thisClient.room-1].users.end(); ++it)
                if (*it != thisClient.username){
                    send(clients[*it]->socketFd, buffer, message_serialized.size() + 1, 0);
                }
        }
        if (messageReceived.option() == 4)
        {
            std::string list_to_sent = "";
            
            for (auto item = allRooms[thisClient.room-1].users.begin(); item != allRooms[thisClient.room-1].users.end(); ++item)
            {
                list_to_sent = list_to_sent + "Username: " + *item + "\n";
            }

            // Mandar lista de usuarios en mi room
            char buffer[8192];
            std::string message_serialized;
            
            protocol::Notification *noti = new protocol::Notification();
            noti->set_notimessage(list_to_sent);

            protocol::ServerMessage response;
            response.set_option(3);
            response.set_allocated_noti(noti);
            response.SerializeToString(&message_serialized);
            strcpy(buffer, message_serialized.c_str());
            send(socketFd, buffer, message_serialized.size() + 1, 0);
        }
    }
    // Cuando el cliente se desconecta se elimina de la lista y se cierra su socket
    clients.erase(thisClient.username);
    for (auto &it : allRooms) // access by reference to avoid copying
    {  
        auto itr = std::find(it.users.begin(), it.users.end(), thisClient.username);
        if (itr != it.users.end()) it.users.erase(itr);
    }
    close(socketFd);
    std::string thisUser = thisClient.username;
    if (thisUser.empty())
        thisUser = "No cliente";
    // Log del server
    std::cout << "Servidor: socket de " << thisUser
              << " cerrado."
              << std::endl;
    pthread_exit(0);
    return 0;
}

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    srand(unsigned(time(0)));
    //Cuando no se indica el puerto del server
    if (argc != 2)
    {
        fprintf(stderr, "Uso: server <puertodelservidor>\n");
        return 1;
    }

    // Manejo general del server y creacion del socket
    long port = strtol(argv[1], NULL, 10);

    sockaddr_in server, incoming_conn;
    socklen_t new_conn_size;
    int socket_fd, new_conn_fd;
    char incoming_conn_addr[INET_ADDRSTRLEN];

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(server.sin_zero, 0, sizeof server.sin_zero);

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Servidor: error creando socket.\n");
        return 1;
    }

    if (bind(socket_fd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        close(socket_fd);
        fprintf(stderr, "Servidor: error bindeando IP:Puerto a socket.\n");
        return 2;
    }

    // escuchar conexiones
    if (listen(socket_fd, 5) == -1)
    {
        close(socket_fd);
        fprintf(stderr, "Servidor: error en listen().\n");
        return 3;
    }
    printf("Servidor: escuchando en puerto %ld\n", port);

    // Las dos rooms del server
    int roomNo = 1;

    for (auto &it : allRooms) // access by reference to avoid copying
    {  
        std::random_shuffle(deck.begin(), deck.end(), randomfunc);
        it.roomNo = roomNo;
        it.roomDeck = deck;
        it.counter = 0;
        roomNo = roomNo+1;
    }

    while (1)
    {
        new_conn_size = sizeof incoming_conn;
        new_conn_fd = accept(socket_fd, (struct sockaddr *)&incoming_conn, &new_conn_size);
        if (new_conn_fd == -1)
        {
            perror("error en accept()");
            continue;
        }

        // Aceptar nuevo cliente
        struct Client newClient;
        newClient.socketFd = new_conn_fd;
        inet_ntop(AF_INET, &(incoming_conn.sin_addr), newClient.ipAddr, INET_ADDRSTRLEN);

        // Thread para le nuevo cliente
        pthread_t thread_id;
        pthread_attr_t attrs;
        pthread_attr_init(&attrs);
        pthread_create(&thread_id, &attrs, ThreadWork, (void *)&newClient);
    }

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
