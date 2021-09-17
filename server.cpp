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
#include <chrono>
#include <thread>

using namespace std;

// Funcion de ayuda para obtener siempre una permutacion diferente de mi Deck
int randomfunc(int j)
{
    return rand() % j;
}

// Estructura de mi cliente
struct Client
{
    int socketFd;
    string username;
    char ipAddr[INET_ADDRSTRLEN];
    int room;
    int lives;
};

// Estructura de mi sala
struct Room
{
    int roomNo;
    vector<string> roomDeck;
    vector<string> users;
    int counter;
};


// Lista de clientes y rooms
unordered_map<string, Client *> clients;
Room RoomOne;
Room RoomTwo;

vector<Room> allRooms {RoomOne, RoomTwo};

// Lista de cartas:
vector<string> deck {
    "A-D","2-D","3-D","4-D","5-D","6-D","7-D","8-D","9-D","10-D","J-D","Q-D","K-D",
    "A-P","2-P","3-P","4-P","5-P","6-P","7-P","8-P","9-P","10-P","J-P","Q-P","K-P",
    "A-C","2-C","3-C","4-C","5-C","6-C","7-C","8-C","9-C","10-C","J-C","Q-C","K-C",
    "A-E","2-E","3-E","4-E","5-E","6-E","7-E","8-E","9-E","10-E","J-E","Q-E","K-E",
    "Joker-Negro","Joker-Rojo"
};


// Funcion que manda errores
void SendErrorResponse(int socketFd, string errorMsg)
{
    string msgSerialized;
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

// Funcion que maneja cada usuario con un thread diferente
void *ThreadWork(void *params)
{
    // Cliente actual
    struct Client thisClient;
    struct Client *newClientParams = (struct Client *)params;
    int socketFd = newClientParams->socketFd;
    char buffer[8192];

    // Estructura de los mensajes
    string msgSerialized;
    protocol::ClientMessage messageReceived;

    while(1)
    {
        // Si no se recive response del cliente este cerro sesion
        if (recv(socketFd, buffer, 8192, 0) < 1)
        {
            if (recv(socketFd, buffer, 8192, 0) == 0)
            {
                // cliente cerro conexion
                cout << "Servidor: el cliente "
                          << thisClient.username
                          << " ha cerrado su sesión."
                          << endl;
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

            cout << "Servidor: se recibió informacion de: "
                << myInfo.username()
                << endl;

            // Revisar si nombre de usuario ya existe, si ya existe, se devuelver un error.
            if (clients.count(myInfo.username()) > 0)
            {
                cout << "Servidor: el nombre de usuario ya existe." << endl;
                SendErrorResponse(socketFd, "El nombre de usuario ya existe.");
                break;
            }

            // Send available rooms
            string all_rooms = "";
            string room_availabe = "";

            for (auto &it : allRooms)
            {
                all_rooms = all_rooms + "-Id: " + to_string(it.roomNo) + " Espacios " + to_string(it.users.size()) + "/4\n";
                if (it.users.size() < 4)
                {
                    room_availabe = room_availabe+to_string(it.roomNo);
                }
            }

            // Escribir el mensaje de rooms
            char buffer[8192];
            string message_serialized;

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
            cout << "Servidor:  usuario agregado exitosamente con id: "
                      << socketFd
                      << endl;

            // Guardar informacion de nuevo cliente
            thisClient.username = myInfo.username();
            thisClient.socketFd = socketFd;
            thisClient.room = 0;
            thisClient.lives = 3;
            strcpy(thisClient.ipAddr, newClientParams->ipAddr);
            clients[thisClient.username] = &thisClient;
        }
        // Al unirse un usuario a una room
        else if (messageReceived.option() == 2)
        {
            // Actualizar datos
            protocol::JoinRoom JoinRoom = messageReceived.roomjoin();
            thisClient.room = JoinRoom.room();
            allRooms[JoinRoom.room()-1].users.insert(allRooms[JoinRoom.room()-1].users.end(), thisClient.username);
            printf("Usuarios en room %d:\n", JoinRoom.room());
            for (auto it = allRooms[JoinRoom.room()-1].users.begin(); it != allRooms[JoinRoom.room()-1].users.end(); ++it)
                cout << "-"<< *it ;
            cout << "\n";

            // Notificar que se unio un compa a todos de la room
            char buffer[8192];
            string message_serialized;
            protocol::Notification *noti = new protocol::Notification();
            noti->set_notimessage("El usuario "+thisClient.username+" Se ha conectado a la sala. "+ to_string(allRooms[JoinRoom.room()-1].users.size()) + "/4");

            protocol::ServerMessage response;
            response.set_option(3);
            response.set_allocated_noti(noti);
            response.SerializeToString(&message_serialized);
            strcpy(buffer, message_serialized.c_str());

            for (auto it = allRooms[JoinRoom.room()-1].users.begin(); it != allRooms[JoinRoom.room()-1].users.end(); ++it){
                if (*it != thisClient.username){
                    send(clients[*it]->socketFd, buffer, message_serialized.size() + 1, 0);
                }
            }

            // En caso que la room este llena comenzar el juego
            if (allRooms[JoinRoom.room()-1].users.size() == 4){
                for (auto it = allRooms[JoinRoom.room()-1].users.begin(); it != allRooms[JoinRoom.room()-1].users.end(); ++it){

                    string user_cards = "";
                    for (int i = 0; i < 3; ++i){
                        user_cards = user_cards + allRooms[JoinRoom.room()-1].roomDeck.front()+',';
                        allRooms[JoinRoom.room()-1].roomDeck.erase(allRooms[JoinRoom.room()-1].roomDeck.begin());
                    }
                    protocol::MatchStart *start = new protocol::MatchStart();
                    start->set_cards(user_cards);

                    protocol::ServerMessage response;
                    response.set_option(4);
                    response.set_allocated_start(start);
                    response.SerializeToString(&message_serialized);
                    strcpy(buffer, message_serialized.c_str());
                    send(clients[*it]->socketFd, buffer, message_serialized.size() + 1, 0);
                }

                chrono::seconds dura(2);
                this_thread::sleep_for( dura );
                // Mandar primer turno
                char buffer[8192];
                string message_serialized;
                protocol::NewTurn *turn = new protocol::NewTurn;

                string newCard = "";
                newCard = newCard +""+allRooms[JoinRoom.room()-1].roomDeck.front();
                allRooms[JoinRoom.room()-1].roomDeck.erase(allRooms[JoinRoom.room()-1].roomDeck.begin());
                turn->set_newcard(newCard);
                turn->set_roomcounter(allRooms[JoinRoom.room()-1].counter);

                protocol::ServerMessage response;
                response.set_option(5);
                response.set_allocated_turn(turn);
                response.SerializeToString(&message_serialized);
                strcpy(buffer, message_serialized.c_str());
                send(clients[allRooms[thisClient.room-1].users.front()]->socketFd, buffer, message_serialized.size() + 1, 0);
            }
        }
        // Envio de mensajes dentro del room
        else if (messageReceived.option() == 3)
        {
            // Mandar el mensaje a todos los participantes menos a si mismo
            char buffer[8192];
            string message_serialized;
            
            protocol::Notification *noti = new protocol::Notification();
            noti->set_notimessage("- El usuario "+thisClient.username+" Ha enviado un mensaje: "+ messageReceived.msgroom().message());

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
        // Mandar lista de usuarios conectados en el room
        else if (messageReceived.option() == 4)
        {
            string list_to_sent = "";
            
            for (auto item = allRooms[thisClient.room-1].users.begin(); item != allRooms[thisClient.room-1].users.end(); ++item)
            {
                list_to_sent = list_to_sent + "- Username: " + *item + "\n";
            }

            // Mandar lista de usuarios en mi room
            char buffer[8192];
            string message_serialized;
            
            protocol::Notification *noti = new protocol::Notification();
            noti->set_notimessage(list_to_sent);

            protocol::ServerMessage response;
            response.set_option(3);
            response.set_allocated_noti(noti);
            response.SerializeToString(&message_serialized);
            strcpy(buffer, message_serialized.c_str());
            send(socketFd, buffer, message_serialized.size() + 1, 0);
        }
        //Manejo general del juego dentro del room
        else if (messageReceived.option() == 5)
        {
            protocol::SendCard myCard =  messageReceived.card();
            string cardToOperate = myCard.card();
            string delimiter = "-";
            string token = cardToOperate.substr(0, cardToOperate.find(delimiter));

            // Hacer cambios en el contador de la room
            if (token == "A"){
                allRooms[thisClient.room-1].counter = allRooms[thisClient.room-1].counter+1;
            }
            else if (token == "10"){
                if (myCard.extra() == 0){
                    allRooms[thisClient.room-1].counter = allRooms[thisClient.room-1].counter + 10;
                }
                else if (myCard.extra() == 1){
                    allRooms[thisClient.room-1].counter = allRooms[thisClient.room-1].counter - 10;
                    if (allRooms[thisClient.room-1].counter < 0){
                        allRooms[thisClient.room-1].counter = 0;
                    }
                }
            }
            else if (token == "J"){
                allRooms[thisClient.room-1].counter = allRooms[thisClient.room-1].counter - 10;
                if (allRooms[thisClient.room-1].counter < 0){
                    allRooms[thisClient.room-1].counter = 0;
                }
            }
            else if (token == "Q"){}
            else if (token == "K"){
                allRooms[thisClient.room-1].counter = 99;
            }
            else if (token == "Joker"){
                allRooms[thisClient.room-1].counter = 0;
            }
            else{
                int toSum = stoi(token);
                allRooms[thisClient.room-1].counter = allRooms[thisClient.room-1].counter+toSum;
            }

            // Notificar a los usuarios del cambio
            char buffer[8192];
            string message_serialized;
            
            protocol::Notification *noti = new protocol::Notification();
            noti->set_notimessage("- El usuario "+thisClient.username+" ha usado la carta: "+ cardToOperate + " el contador a cambiado a "+ to_string(allRooms[thisClient.room-1].counter));

            protocol::ServerMessage response;
            response.set_option(3);
            response.set_allocated_noti(noti);
            response.SerializeToString(&message_serialized);
            strcpy(buffer, message_serialized.c_str());

            for (auto it = allRooms[thisClient.room-1].users.begin(); it != allRooms[thisClient.room-1].users.end(); ++it)
                if (*it != thisClient.username){
                    send(clients[*it]->socketFd, buffer, message_serialized.size() + 1, 0);
                }

            if (allRooms[thisClient.room-1].users.size()>1){
                //Pasar de turno
                chrono::seconds dura(2);
                this_thread::sleep_for( dura );

                // Mandar primer turno
                protocol::NewTurn *turn = new protocol::NewTurn;

                string newCard = "";
                newCard = newCard +""+allRooms[thisClient.room-1].roomDeck.front();
                allRooms[thisClient.room-1].roomDeck.erase(allRooms[thisClient.room-1].roomDeck.begin());
                turn->set_newcard(newCard);
                turn->set_roomcounter(allRooms[thisClient.room-1].counter);

                protocol::ServerMessage turnResponse;
                turnResponse.set_option(5);
                turnResponse.set_allocated_turn(turn);
                turnResponse.SerializeToString(&message_serialized);
                strcpy(buffer, message_serialized.c_str());

                auto it = find(allRooms[thisClient.room-1].users.begin(), allRooms[thisClient.room-1].users.end(), thisClient.username);
                int index = it - allRooms[thisClient.room-1].users.begin();

                if (index != allRooms[thisClient.room-1].users.size()-1)
                {
                    send(clients[allRooms[thisClient.room-1].users[index+1]]->socketFd, buffer, message_serialized.size() + 1, 0);
                }else{
                    send(clients[allRooms[thisClient.room-1].users.front()]->socketFd, buffer, message_serialized.size() + 1, 0);
                }
            }
        }
        // En caso de perder la ronda
        else if (messageReceived.option() == 6)
        {
            //Quitar vida del usuario
            thisClient.lives = thisClient.lives -1;
            int newTurn = 1;
            // Notificar a los demas que el usuario perdio la ronda
            char buffer[8192];
            string message_serialized;
            
            protocol::Notification *noti = new protocol::Notification();
            noti->set_notimessage("- El usuario "+thisClient.username+" ha perdido la ronda. Esperando que comienze la nueva ronda...");

            protocol::ServerMessage response;
            response.set_option(3);
            response.set_allocated_noti(noti);
            response.SerializeToString(&message_serialized);
            strcpy(buffer, message_serialized.c_str());

            for (auto it = allRooms[thisClient.room-1].users.begin(); it != allRooms[thisClient.room-1].users.end(); ++it)
                if (*it != thisClient.username){
                    send(clients[*it]->socketFd, buffer, message_serialized.size() + 1, 0);
                }
            
            // Resetear valores del room
            allRooms[thisClient.room-1].counter = 0;
            random_shuffle(deck.begin(), deck.end(), randomfunc);
            allRooms[thisClient.room-1].roomDeck = deck;

            // Verificar que aun pueda jugar
            if (thisClient.lives == 0)
            {
                // Notificar que el otro gano la partida
                if (allRooms[thisClient.room-1].users.size()<3)
                {
                    newTurn = 0;
                    chrono::seconds dura(2);
                    this_thread::sleep_for( dura );

                    protocol::ServerMessage response;
                    response.set_option(6);
                    response.SerializeToString(&message_serialized);
                    strcpy(buffer, message_serialized.c_str());

                    for (auto it = allRooms[thisClient.room-1].users.begin(); it != allRooms[thisClient.room-1].users.end(); ++it)
                        if (*it != thisClient.username){
                            send(clients[*it]->socketFd, buffer, message_serialized.size() + 1, 0);
                        }
                }
                // Notificar que perdi el juego
                else {
                    chrono::seconds dura(2);
                    this_thread::sleep_for( dura );

                    protocol::Notification *noti = new protocol::Notification();
                    noti->set_notimessage("- El usuario "+thisClient.username+" ha perdido el juego!! Esperando que comienze la nueva ronda...");

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
            }
            //En caso de continuar la partida mandar un nuevo turno
            if (newTurn = 1){
                chrono::seconds dura(2);
                this_thread::sleep_for( dura );

                // Comenzar nueva ronda
                for (auto it = allRooms[thisClient.room-1].users.begin(); it != allRooms[thisClient.room-1].users.end(); ++it){

                    string user_cards = "";
                    for (int i = 0; i < 3; ++i){
                        user_cards = user_cards + allRooms[thisClient.room-1].roomDeck.front()+',';
                        allRooms[thisClient.room-1].roomDeck.erase(allRooms[thisClient.room-1].roomDeck.begin());
                    }
                    protocol::MatchStart *start = new protocol::MatchStart();
                    start->set_cards(user_cards);

                    protocol::ServerMessage response;
                    response.set_option(4);
                    response.set_allocated_start(start);
                    response.SerializeToString(&message_serialized);
                    strcpy(buffer, message_serialized.c_str());
                    send(clients[*it]->socketFd, buffer, message_serialized.size() + 1, 0);
                }

                this_thread::sleep_for( dura );

                // Mandar primer turno
                protocol::NewTurn *turn = new protocol::NewTurn;

                string newCard = "";
                newCard = newCard +""+allRooms[thisClient.room-1].roomDeck.front();
                allRooms[thisClient.room-1].roomDeck.erase(allRooms[thisClient.room-1].roomDeck.begin());
                turn->set_newcard(newCard);
                turn->set_roomcounter(allRooms[thisClient.room-1].counter);

                protocol::ServerMessage newResponse;
                newResponse.set_option(5);
                newResponse.set_allocated_turn(turn);
                newResponse.SerializeToString(&message_serialized);
                strcpy(buffer, message_serialized.c_str());
                send(clients[allRooms[thisClient.room-1].users.front()]->socketFd, buffer, message_serialized.size() + 1, 0);
            }
        }
        // En caso de ganar la partida con un trio
        else if (messageReceived.option() == 7)
        {
            // Notificar a los demas que el usuario ganado la partida
            char buffer[8192];
            string message_serialized;
            
            protocol::Notification *noti = new protocol::Notification();
            noti->set_notimessage("- El usuario "+thisClient.username+" ha ganado la partida, con un trio!");

            protocol::ServerMessage response;
            response.set_option(3);
            response.set_allocated_noti(noti);
            response.SerializeToString(&message_serialized);
            strcpy(buffer, message_serialized.c_str());

            for (auto it = allRooms[thisClient.room-1].users.begin(); it != allRooms[thisClient.room-1].users.end(); ++it)
                if (*it != thisClient.username){
                    send(clients[*it]->socketFd, buffer, message_serialized.size() + 1, 0);
                }
            
            chrono::seconds dura(2);
            this_thread::sleep_for( dura );

            response.set_option(7);
            response.SerializeToString(&message_serialized);
            strcpy(buffer, message_serialized.c_str());

            for (auto it = allRooms[thisClient.room-1].users.begin(); it != allRooms[thisClient.room-1].users.end(); ++it)
                if (*it != thisClient.username){
                    send(clients[*it]->socketFd, buffer, message_serialized.size() + 1, 0);
                }
            
        }
    }
    // Cuando el cliente se desconecta se elimina de la lista y se cierra su socket
    clients.erase(thisClient.username);
    for (auto &it : allRooms) // access by reference to avoid copying
    {  
        auto itr = find(it.users.begin(), it.users.end(), thisClient.username);
        if (itr != it.users.end()) it.users.erase(itr);
    }
    close(socketFd);
    string thisUser = thisClient.username;
    if (thisUser.empty())
        thisUser = "No cliente";
    // Log del server
    cout << "Servidor: socket de " << thisUser
              << " cerrado."
              << endl;
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

    for (auto &it : allRooms)
    {  
        random_shuffle(deck.begin(), deck.end(), randomfunc);
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
