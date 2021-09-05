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


struct Client
{
    int socketFd;
    std::string username;
    char ipAddr[INET_ADDRSTRLEN];
};

// Lista de clientes
std::unordered_map<std::string, Client *> clients;

// Funcion que manda errores
void SendErrorResponse(int socketFd, std::string errorMsg)
{
    std::string msgSerialized;
    protocol::ErrorResponse *errorMessage = new protocol::ErrorResponse();
    errorMessage->set_errormessage(errorMsg);

    errorMessage->SerializeToString(&msgSerialized);
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
            // TODO


            // Log del server
            std::cout << "Servidor:  usuario agregado exitosamente con id: "
                      << socketFd
                      << std::endl;

            // Guardar informacion de nuevo cliente
            thisClient.username = myInfo.username();
            thisClient.socketFd = socketFd;
            strcpy(thisClient.ipAddr, newClientParams->ipAddr);
            clients[thisClient.username] = &thisClient;
        }
        else
        {
            SendErrorResponse(socketFd, "Opcion indicada no existe.");
        }
    }
    // Cuando el cliente se desconecta se elimina de la lista y se cierra su socket
    clients.erase(thisClient.username);
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
