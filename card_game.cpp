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

using namespace std;

// Variables globales de interes
int connected, waitingForServerResponse, waitingForStart, lives, myTurn, roomCounter;
string newCard;
vector<string> myCards;

// Obtencion de ip del usuario
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// Funcion que escucha las respuestas del servidor
void *listenToMessages(void *args)
{
	while (1)
	{
		char bufferMsg[8192];
		int *sockmsg = (int *)args;
		protocol::ServerMessage serverMsg;

		int bytesReceived = recv(*sockmsg, bufferMsg, 8192, 0);

		serverMsg.ParseFromString(bufferMsg);

		// En caso de recibir un mensaje de error
		if (serverMsg.option() == 2){
			printf("________________________________________________________\n");
			cout << "- Error: "
			  << serverMsg.error().errormessage()
			  << endl;
		}
		// En caso de recibir una notificacion
		else if (serverMsg.option() == 3){
			printf("________________________________________________________\n");
			cout << "Notificacion:\n"
			  <<serverMsg.noti().notimessage()
			  << endl;
		}
		// Al obtener la flag que se comenzo el juego
		else if (serverMsg.option() == 4){
			printf("___________________El juego ha comenzado___________________\n");
			newCard = "";
			myCards.clear();
			myTurn = 0;
			printf("Sus primeras cartas son:\n");
			waitingForStart = 0;
			string cards = serverMsg.start().cards();
			string delimiter = ",";

			size_t pos = 0;
			string token;
			while ((pos = cards.find(delimiter)) != string::npos) {
				token = cards.substr(0, pos);
				myCards.insert(myCards.end(),token);
				cards.erase(0, pos + delimiter.length());
			}
			for (auto it = myCards.begin(); it != myCards.end(); ++it)
        		cout << *it <<", ";
				printf("\n");
			
			printf("Esperando turno... Recarge menu - 0\n");
		}
		// En caso de obtener la flag de ser mi turno
		else if (serverMsg.option() == 5){
			printf("___________________Es su turno___________________\n");
			myTurn = 1;
			protocol::NewTurn income = serverMsg.turn();
			newCard = income.newcard();
			roomCounter = income.roomcounter();
			cout << "Contador acutal de la partida: "<<roomCounter<<endl;
			printf("Que accion desea realizar... Recarge menu - 0\n");
			
		}
		// En caso de ganar la partida
		else if (serverMsg.option() == 6){
			printf("___________________HAS GANADO LA PARTIDA___________________\n");
			connected = 0;
		}
		// En caso de perder la partida
		else if (serverMsg.option() == 7){
			printf("___________________HAS PERDIDO LA PARTIDA___________________\n");
			connected = 0;
		}

		waitingForServerResponse = 0;

		// Liberar el thread en caso de no estar conectado
		if (connected == 0)
		{
			pthread_exit(0);
		}
	}
}

// Funcion general para obtener una opcion ingresada por el cliente
int get_client_option()
{
	// Client options
	int client_opt;
	cin >> client_opt;

	while (cin.fail())
	{
		cout << "Por favor, ingrese una opcion valida: " << endl;
		cin.clear();
		cin.ignore(256, '\n');
		cin >> client_opt;
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
	printf("________________________________________________________\n");
	printf("Conectado con %s\n", s);
	freeaddrinfo(servinfo);


	//Instrucciones del juego
	printf("____________________________Bienvenido a 99____________________________\n");
	printf("Esta guia rapida te ayudara a comprender que opciones puedes realizar\n");
	printf("dentro del juego. Esto se explicara en tres etapas:\n");
	printf("	1. Unirse a una sala:\n");
	printf("	Lo primero que se hara es unirse a una sala luego de haber elegido\n");
	printf("	su nombre de usuario. Mientras espera que mas jugadores entren a la\n");
	printf("	sala usted podra: Mandar mensajes a todos los usuarios en la sala y\n");
	printf("	ver los usuarios que estan en la sala. El juego comenzara al haber\n");
	printf("	cuatro usuarios en una sala.\n\n");
	
	printf("	2. Espera de turno:\n");
	printf("	Mientras un usuario esta en la sala podra realizar las acciones \n");
	printf("	mencionadas anteriormente, mas poder ver sus cartas actuales y su\n");
	printf("	vida restante. Ademas en caso de tener 3 cartas de un mismo valor\n");
	printf("	podra optar por ganar inmediatamente la partida, esto tambien se\n");
	printf("	puede lograr con la ayuda de los Comodines, pero OJO si intentan\n");
	printf("	ganar de esta manera y no tienen el trio perderan automaticamente\n");
	printf("	esa ronda y se comenzara una nueva.\n\n");

	printf("	2. Durante su turno:\n");
	printf("	Durante su turno se le desplegara el contador actual de la partida\n");
	printf("	teniendo esto en cuenta debera elegir una de sus 3 cartas para\n");
	printf("	realizar cambios a este contador. A continuacion se lista los cambios\n");
	printf("	que las diferentes cartas hacen al contador:\n");
	printf("	- A - 9: Suman al contador su valor, en el caso del As se suma 1\n");
	printf("	- 10: Suma o resta al contador 10, esto depende del jugador\n");
	printf("	- J: Resta al contador 10\n");
	printf("	- Q: Equivale a sumar 0 al contador\n");
	printf("	- K: Automaticamente iguala el contador a 99\n");
	printf("	- Joker: Automaticamente iguala el contador a 0\n\n");
	
	printf("Condiciones de derrota y victoria\n");
	printf("	Cada jugador comienza con 3 vidas. El objetivo principal del juego\n");
	printf("	es evitar que el contador se pase de 99. Si esto sucede el jugador\n");
	printf("	que se paso pierde una vida y comienza una nueva ronda. Si el \n");
	printf("	jugador ya no cuenta con vidas queda expulsado del juego, el ultimo\n");
	printf("	jugador con vidas es el Ganador!\n");
	printf("Espero que se diviertan!\n");

	// Obtencion de nombre de usuario
    string uname;
    printf("- Ingrese su nombre de usuario:\n");
    cin >> uname;

    // Escribir el mensaje de registro
	char buffer[8192];
	string message_serialized;

    protocol::ClientConnect *connect = new protocol::ClientConnect();
    connect->set_username(uname);
    connect->set_ip(s);

	protocol::ClientMessage firstMessage;
    firstMessage.set_option(1);
    firstMessage.set_allocated_connect(connect);
    firstMessage.SerializeToString(&message_serialized);
    strcpy(buffer, message_serialized.c_str());
	send(sockfd, buffer, message_serialized.size() + 1, 0);

	// Recibir primera respuesta del servidor
	recv(sockfd, buffer, 8192, 0);

	protocol::ServerMessage serverMessage;
	serverMessage.ParseFromString(buffer);
	if(serverMessage.option()==2){
		cout << "- Error: "
			  << serverMessage.error().errormessage()
			  << endl;
			return 0;
	}

	// Definir que el usuario se ha conectado y esta esperando el inicio del juego
	connected = 1;
	waitingForStart = 1;
	printf("________________________________________________________\n");
	printf("Rooms del servidor:\n");
	cout << serverMessage.rooms().rooms()<< endl;

	//Elegir Room al que el usuario se conectara
	printf("A que room se desea unir:\n");
	int myroom;
	myroom = get_client_option();
	 if (myroom == 1 || myroom == 2)
	 {
		 if (serverMessage.rooms().roomsjoin().find(to_string(myroom)) != string::npos)
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
			 printf("________________________________________________________\n");
			 printf("Dicha room esta llena\n");
			 return 0;
		 }
	 }
	 else {
		 printf("________________________________________________________\n");
		 printf("Dicha room no existe\n");
		 return 0;
	 }

	// Despacho del thread que escucha 
	pthread_t thread_id;
	pthread_attr_t attrs;
	pthread_attr_init(&attrs);
	pthread_create(&thread_id, &attrs, listenToMessages, (void *)&sockfd);

	// Loopear por las opciones mientras este conectado
	while (connected == 1)
	{
		// Esperar respuesta del servidor antes de printear menus
		while (waitingForServerResponse == 1){}

		// Menu y opciones en caso de esperar que comience el juego
		if (waitingForStart == 1){
			printf("________________________________________________________\n");
			printf("Esperado a que se unan mas jugadores...\n");
			printf("Opciones mientras espera que se unan jugadores:\n");
			printf("	1. Mandar mensaje\n");
			printf("	2. Ver usuarios en la sala\n");
			printf("	0. Recargar Menu\n");
			int client_opt;
			client_opt = get_client_option();

			// Mandar un mensaje a todos los usuarios de mi room
			if (client_opt == 1)
			{
				printf("________________________________________________________\n");
				printf("- Mensaje que desea mandar a la sala:\n");
				cin.ignore();
				string msg;
				getline(cin, msg);

				protocol::RoomMessage *roomMsg = new protocol::RoomMessage();
				roomMsg->set_message(msg);
			
				protocol::ClientMessage roomMessage;
				roomMessage.set_option(3);
				roomMessage.set_allocated_msgroom(roomMsg);
				roomMessage.SerializeToString(&message_serialized);
				strcpy(buffer, message_serialized.c_str());
				send(sockfd, buffer, message_serialized.size() + 1, 0);
				printf("- Mensaje enviado correctamente\n");
			}
			// Obtener la lista de todos los conectados a mi room
			else if (client_opt == 2) {
				protocol::ClientMessage listMsg;
				listMsg.set_option(4);
				listMsg.SerializeToString(&message_serialized);
				strcpy(buffer, message_serialized.c_str());
				send(sockfd, buffer, message_serialized.size() + 1, 0);
				waitingForServerResponse = 1;
			}
			else if (client_opt == 0){}
			else
			{
				printf("- Dicha opcion no existe.");
			}
		}
		// Menu y opciones cuando el juego comenzo
		else
		{
			// Menu si no es mi turno
			if (myTurn == 0)
			{
				printf("________________________________________________________\n");
				printf("Opciones mientras espera su turno:\n");
				printf("	1. Mandar mensaje\n");
				printf("	2. Ver cartas\n");
				printf("	3. Ganar con 3 iguales\n");
				printf("	4. Ver usuarios en la sala\n");
				printf("	5. Ver cantidad de vidas\n");
				int client_opt;
				client_opt = get_client_option();

				// Mandar mensaje a usuarios del room
				if (client_opt == 1)
				{
					printf("________________________________________________________\n");
					printf("Mensaje que desea mandar a la sala:\n");
					cin.ignore();
					string msg;
					getline(cin, msg);

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
				// Ver mis cartas
				else if (client_opt == 2){
					printf("________________________________________________________\n");
					printf("Sus cartas actuales son:\n");
					for (auto it = myCards.begin(); it != myCards.end(); ++it)
						cout << *it << ", ";
						printf("\n");

				}
				// Intentar ganar con trio
				else if (client_opt == 3){
					string toCheck = "";
					string delimiter = "-";
					string temp = "";
					int isGood = 1;
					for (auto it = myCards.begin(); it != myCards.end(); ++it)
					{
						string actual = *it;
						string token = actual.substr(0, actual.find(delimiter));

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
					// Ganar la partida con trio
					if (isGood == 1){
						printf("___________________HAS GANADO LA PARTIDA___________________\n");
						// Send win
						protocol::ClientMessage lostMsg;
						lostMsg.set_option(7);
						lostMsg.SerializeToString(&message_serialized);
						strcpy(buffer, message_serialized.c_str());
						send(sockfd, buffer, message_serialized.size() + 1, 0);
						return 0;
					}
					// Perder la ronda por mentiroso
					else{
						printf("_______________________MENTIROSO!__________________________\n");
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
							chrono::seconds dura(2);
							this_thread::sleep_for( dura );
							return 0;
						}
					}
				}
				// Ver usuarios del room
				else if (client_opt == 4) {
					protocol::ClientMessage listMsg;
					listMsg.set_option(4);
					listMsg.SerializeToString(&message_serialized);
					strcpy(buffer, message_serialized.c_str());
					send(sockfd, buffer, message_serialized.size() + 1, 0);
					waitingForServerResponse = 1;
				}
				// Ver vidas actuales
				else if (client_opt == 5){
					printf("________________________________________________________\n");
					printf("- Actualmente cuenta con %d vidas\n", lives);
				}
				else if (client_opt == 0){}
				else
				{
					printf("________________________________________________________\n");
					printf("Dicha opcion no existe.\n");
				}
			}
			// Opciones cuando si es mi turno
			else
			{
				int indices = 1;
				int extra = 0;
				printf("________________________________________________________\n");
				printf("El contador actual de la partida es: %d			Sus vidas:%d\n", roomCounter, lives);
				printf("Sus cartas actualmente son:\n");
				for (string i: myCards){
					cout << indices <<"."<<i << '\n';
					indices = indices + 1;
				}
				printf("Que carta desea colocar:\n");
				int client_opt;
				client_opt = get_client_option();
				// Manejo del contador local
				if (client_opt<4 && client_opt>0){
					string toSend = myCards[client_opt-1];
					string delimiter = "-";
					string token = toSend.substr(0, toSend.find(delimiter));
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
						int toSum = stoi(token);
						roomCounter = roomCounter+toSum;
					}
					// Mandar al server mi carta si no me pase de 99
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
					// Perder ronda por que me pase de 99
					else{
						printf("________________________________________________________\n");
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
							printf("________________________________________________________\n");
							printf("No te quedan vidas has perdido el juego!\n");
							chrono::seconds dura(2);
            				this_thread::sleep_for( dura );
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