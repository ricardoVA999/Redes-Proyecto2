#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ifaddrs.h>
#include "protocol.pb.h"
#include <queue>
#include <chrono>
#include <thread>


int connected, waitingForServerResponse, waitingForStart, lives, myTurn, roomCounter;
std::string newCard;
std::vector<std::string> myCards;

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void *listenToMessages(void *args)
{
	while (1)
	{
		char bufferMsg[8192];
		int *sockmsg = (int *)args;
		protocol::ServerMessage serverMsg;

		int bytesReceived = recv(*sockmsg, bufferMsg, 8192, 0);

		serverMsg.ParseFromString(bufferMsg);

		if (serverMsg.option() == 2){
			printf("________________________________________________________\n");
			std::cout << "Error: "
			  << serverMsg.error().errormessage()
			  << std::endl;
			printf("________________________________________________________\n");
		}
		else if (serverMsg.option() == 3){
			printf("________________________________________________________\n");
			std::cout << "Notificacion:\n"
			  << serverMsg.noti().notimessage()
			  << std::endl;
			printf("________________________________________________________\n");
		}
		else if (serverMsg.option() == 4){
			printf("___________________El juego ha comenzado___________________\n");
			newCard = "";
			myCards.clear();
			myTurn = 0;
			printf("Sus primeras cartas son:\n");
			waitingForStart = 0;
			std::string cards = serverMsg.start().cards();
			std::string delimiter = ",";

			size_t pos = 0;
			std::string token;
			while ((pos = cards.find(delimiter)) != std::string::npos) {
				token = cards.substr(0, pos);
				myCards.insert(myCards.end(),token);
				cards.erase(0, pos + delimiter.length());
			}
			for (auto it = myCards.begin(); it != myCards.end(); ++it)
        		std::cout << *it <<", ";
				printf("\n");
			
			printf("Esperando turno... Recarge menu - 0\n");
		}
		else if (serverMsg.option() == 5){
			printf("___________________Es su turno___________________\n");
			myTurn = 1;
			protocol::NewTurn income = serverMsg.turn();
			newCard = income.newcard();
			roomCounter = income.roomcounter();
			std::cout << "Contador acutal de la partida: "<<roomCounter<<std::endl;
			printf("Que accion desea realizar... Recarge menu - 0\n");
			
		}
		else if (serverMsg.option() == 6){
			printf("___________________HAS GANADO LA PARTIDA___________________\n");
			connected = 0;
		}
		else if (serverMsg.option() == 7){
			printf("___________________HAS PERDIDO LA PARTIDA___________________\n");
			connected = 0;
		}

		waitingForServerResponse = 0;

		if (connected == 0)
		{
			pthread_exit(0);
		}
	}
}

int get_client_option()
{
	// Client options
	int client_opt;
	std::cin >> client_opt;

	while (std::cin.fail())
	{
		std::cout << "Por favor, ingrese una opcion valida: " << std::endl;
		std::cin.clear();
		std::cin.ignore(256, '\n');
		std::cin >> client_opt;
	}

	return client_opt;
}

int main(int argc, char *argv[])
{
	lives = 3;
	newCard = "";
    // Estructura de la coneccion
	int sockfd, numbytes;
	char buf[8192];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 3)
	{
		fprintf(stderr, "Forma de uso: <server_ip> <server_port>\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// Conectarse a la opcion que este disponible
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}
	// Indicar fallo al conectarse
	if (p == NULL)
	{
		fprintf(stderr, "failed to connect\n");
		return 2;
	}

	//Completar la coneccion
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			  s, sizeof s);
	printf("Conectado con %s\n", s);
	freeaddrinfo(servinfo);

    std::string uname;
    printf("Ingrese su nombre de usuario:\n");
    std::cin >> uname;

    // Escribir el mensaje de registro
	char buffer[8192];
	std::string message_serialized;

    protocol::ClientConnect *connect = new protocol::ClientConnect();
    connect->set_username(uname);
    connect->set_ip(s);

	protocol::ClientMessage firstMessage;
    firstMessage.set_option(1);
    firstMessage.set_allocated_connect(connect);
    firstMessage.SerializeToString(&message_serialized);
    strcpy(buffer, message_serialized.c_str());
	send(sockfd, buffer, message_serialized.size() + 1, 0);

	recv(sockfd, buffer, 8192, 0);

	protocol::ServerMessage serverMessage;
	serverMessage.ParseFromString(buffer);
	if(serverMessage.option()==2){
		std::cout << "Error: "
			  << serverMessage.error().errormessage()
			  << std::endl;
			return 0;
	}

	connected = 1;
	waitingForStart = 1;
	printf("Rooms del servidor:\n");
	std::cout << serverMessage.rooms().rooms()<< std::endl;
	//Elegir Room
	printf("A que room se desea unir:\n");
	int myroom;
	myroom = get_client_option();
	 if (myroom == 1 || myroom == 2)
	 {
		 if (serverMessage.rooms().roomsjoin().find(std::to_string(myroom)) != std::string::npos)
		 {
			protocol::JoinRoom *join = new protocol::JoinRoom();
			join->set_room(myroom);
		
			protocol::ClientMessage roomMessage;
			roomMessage.set_option(2);
			roomMessage.set_allocated_roomjoin(join);
			roomMessage.SerializeToString(&message_serialized);
			strcpy(buffer, message_serialized.c_str());
			send(sockfd, buffer, message_serialized.size() + 1, 0);
			roomCounter = 0;
			myTurn = 0;
		 }
		 else
		 {
			 printf("Dicha room esta llena\n");
			 return 0;
		 }
	 }
	 else {
		 printf("Dicha room no existe\n");
		 return 0;
	 }

	pthread_t thread_id;
	pthread_attr_t attrs;
	pthread_attr_init(&attrs);
	pthread_create(&thread_id, &attrs, listenToMessages, (void *)&sockfd);

	while (connected == 1)
	{
		while (waitingForServerResponse == 1){}
		printf("Opciones dentro del room:\n");
		if (waitingForStart == 1){
			printf("Esperado a que se unan mas jugadores...\n");
			printf("	1. Mandar mensaje\n");
			printf("	2. Ver usuarios en la sala\n");
			printf("	0. Recargar Menu\n");
			int client_opt;
			client_opt = get_client_option();
			if (client_opt == 1)
			{
				printf("Mensaje que desea mandar a la sala:\n");
				std::cin.ignore();
				std::string msg;
				std::getline(std::cin, msg);

				protocol::RoomMessage *roomMsg = new protocol::RoomMessage();
				roomMsg->set_message(msg);
			
				protocol::ClientMessage roomMessage;
				roomMessage.set_option(3);
				roomMessage.set_allocated_msgroom(roomMsg);
				roomMessage.SerializeToString(&message_serialized);
				strcpy(buffer, message_serialized.c_str());
				send(sockfd, buffer, message_serialized.size() + 1, 0);
				printf("Mensaje enviado\n");
			}
			else if (client_opt == 2) {
				protocol::ClientMessage listMsg;
				listMsg.set_option(4);
				listMsg.SerializeToString(&message_serialized);
				strcpy(buffer, message_serialized.c_str());
				send(sockfd, buffer, message_serialized.size() + 1, 0);
				waitingForServerResponse = 1;
			}
			if (client_opt == 0){}
			else
			{
				printf("Dicha opcion no existe.");
			}
		}
		else
		{
			if (myTurn == 0)
			{
				printf("	1. Mandar mensaje\n");
				printf("	2. Ver cartas\n");
				printf("	3. Ganar con 3 iguales\n");
				printf("	4. Ver usuarios en la sala\n");
				printf("	5. Ver cantidad de vidas\n");
				int client_opt;
				client_opt = get_client_option();
				if (client_opt == 1)
				{
					printf("Mensaje que desea mandar a la sala:\n");
					std::cin.ignore();
					std::string msg;
					std::getline(std::cin, msg);

					protocol::RoomMessage *roomMsg = new protocol::RoomMessage();
					roomMsg->set_message(msg);
				
					protocol::ClientMessage roomMessage;
					roomMessage.set_option(3);
					roomMessage.set_allocated_msgroom(roomMsg);
					roomMessage.SerializeToString(&message_serialized);
					strcpy(buffer, message_serialized.c_str());
					send(sockfd, buffer, message_serialized.size() + 1, 0);
					printf("Mensaje enviado\n");
				}
				else if (client_opt == 2){
					printf("Sus cartas actuales son:\n");
					for (auto it = myCards.begin(); it != myCards.end(); ++it)
						std::cout << *it << ", ";
						printf("\n");

				}
				else if (client_opt == 3){
					std::string toCheck = "";
					std::string delimiter = "-";
					std::string temp = "";
					int isGood = 1;
					for (auto it = myCards.begin(); it != myCards.end(); ++it)
					{
						std::string actual = *it;
						std::string token = actual.substr(0, actual.find(delimiter));

						if (temp == "" && token!="Joker"){
							temp = token;
						}

						if (token == "Joker" || token == temp){
							if (token!="Joker"){
								temp = token;
							}
						}
						else{
							isGood = 0;
						}
					}
					if (isGood == 1){
						printf("___________________HAS GANADO LA PARTIDA___________________\n");
						// Send win
						protocol::ClientMessage lostMsg;
						lostMsg.set_option(7);
						lostMsg.SerializeToString(&message_serialized);
						strcpy(buffer, message_serialized.c_str());
						send(sockfd, buffer, message_serialized.size() + 1, 0);
						return 0;
					}else{
						printf("MENTIROSO\n");
						printf("Has perdido esta ronda\n");
						lives = lives - 1;

						protocol::ClientMessage lostMsg;
						lostMsg.set_option(6);
						lostMsg.SerializeToString(&message_serialized);
						strcpy(buffer, message_serialized.c_str());
						send(sockfd, buffer, message_serialized.size() + 1, 0);

						if (lives > 0 )
						{
							myTurn = 0;
							printf("Te quedan %d vidas\n", lives);
							printf("Esperando que inicie nueva ronda.\n");
						}
						else
						{
							printf("No te quedan vidas has perdido el juego!\n");
							std::chrono::seconds dura(2);
							std::this_thread::sleep_for( dura );
							return 0;
						}
					}
				}
				else if (client_opt == 4) {
					protocol::ClientMessage listMsg;
					listMsg.set_option(4);
					listMsg.SerializeToString(&message_serialized);
					strcpy(buffer, message_serialized.c_str());
					send(sockfd, buffer, message_serialized.size() + 1, 0);
					waitingForServerResponse = 1;
				}
				else if (client_opt == 5){
					printf("- Actualmente cuenta con %d vidas\n", lives);

				}
				else if (client_opt == 0){}
				else
				{
					printf("Dicha opcion no existe.\n");
				}
			}
			else
			{
				int indices = 1;
				int extra = 0;
				printf("El contador actual de la partida es: %d			Sus vidas:%d\n", roomCounter, lives);
				printf("Sus cartas actualmente son:\n");
				for (std::string i: myCards){
					std::cout << indices <<"."<<i << '\n';
					indices = indices + 1;
				}
				printf("Que carta desea colocar:\n");
				int client_opt;
				client_opt = get_client_option();
				if (client_opt<4 && client_opt>0){
					std::string toSend = myCards[client_opt-1];
					std::string delimiter = "-";
					std::string token = toSend.substr(0, toSend.find(delimiter));
					if (token == "A"){
						roomCounter = roomCounter+1;
					}
					else if (token == "10"){
						printf("Deseas:\n	1. Sumar 10\n	2. Restar 10\n");
						int action;
						action = get_client_option();
						if (action == 1){
							roomCounter = roomCounter + 10;
						}
						else if (action == 2){
							roomCounter = roomCounter - 10;
							extra = 1;
							if (roomCounter < 0){
								roomCounter = 0;
							}
						}
						else{
							printf("Esa opcion no existe\n");
						}
					}
					else if (token == "J"){
						roomCounter = roomCounter - 10;
						if (roomCounter < 0){
							roomCounter = 0;
						}
					}
					else if (token == "Q"){}
					else if (token == "K"){
						roomCounter = 99;
					}
					else if (token == "Joker"){
						roomCounter = 0;
					}
					else{
						int toSum = std::stoi(token);
						roomCounter = roomCounter+toSum;
					}

					if (roomCounter < 100){
						myTurn = 0;
						myCards.erase(myCards.begin() + client_opt - 1);
						printf("Tu nueva carta es %s\n", newCard.c_str());
						myCards.insert(myCards.end(),newCard);
						//Send Card
						protocol::SendCard *card  = new protocol::SendCard();
						card->set_card(toSend);
						card->set_extra(extra);

						protocol::ClientMessage cardMessage;
						cardMessage.set_option(5);
						cardMessage.set_allocated_card(card);
						cardMessage.SerializeToString(&message_serialized);
						strcpy(buffer, message_serialized.c_str());
						send(sockfd, buffer, message_serialized.size() + 1, 0);
						printf("El contador ahora es %d\n", roomCounter);
					}
					else{
						printf("Has perdido esta ronda\n");
						lives = lives - 1;

						protocol::ClientMessage lostMsg;
						lostMsg.set_option(6);
						lostMsg.SerializeToString(&message_serialized);
						strcpy(buffer, message_serialized.c_str());
						send(sockfd, buffer, message_serialized.size() + 1, 0);

						if (lives > 0 )
						{
							myTurn = 0;
							printf("Te quedan %d vidas\n", lives);
							printf("Esperando que inicie nueva ronda.\n");
						}
						else
						{
							printf("No te quedan vidas has perdido el juego!\n");
							std::chrono::seconds dura(2);
            				std::this_thread::sleep_for( dura );
							return 0;
						}
					}
				}
				else{
					printf("Dicha opcion no existe\n");
				}
			}
		}
	}

	// cerrar conexion
	pthread_cancel(thread_id);
	close(sockfd);

	return 0;
}