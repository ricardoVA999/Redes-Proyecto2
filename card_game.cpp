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

int connected, waitingForServerResponse, waitingForInput;

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
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



/* 	firstMessage->set_sender(uname);
	firstMessage->set_flag(chat::Payload_PayloadFlag_register_);
	firstMessage->set_ip(s);

	firstMessage->SerializeToString(&message_serialized);

	strcpy(buffer, message_serialized.c_str());
	send(sockfd, buffer, message_serialized.size() + 1, 0); */
}