# Redes Proyecto 2 👨🏽‍💻
## Objetivos 🎯
- Aplicar los conocimientos de TCP para mantener sincronizados los estados internos de distintos clientes de una aplicación

- Implementar un juego de cartas llamativo para el usuario

## Desarrollo 🧑🏽‍💻
Implementación de un protocolo TCP para la realización de un juego de cartas (99)


## Prerequisitos 📑
- Verificar version de gcc, esta debe ser la 9.3.0
```
gcc --version
```
- Instalar Protobuf (Ubuntu)
```
$ sudo apt install -y protobuf-compiler
```
- La version a instalar debe ser la 3.6.1
```
$ protoc --version  # Ensure compiler version is 3+
```

## Ejecucion 🏎️

1. Realizar make
```
$ make
```
2. Ejecutar el servidor
```
$ ./server <puertodelservidor>
```
3. Ejecutar juego 
```
$ ./card_game <server_ip> <server_port>
```

## Grupo de trabajo 📓
- Ricardo Valenzuela 
- Diego Solorzano
- Sara Zavala

## Reglas del juego
1. Unirse a una sala:

Lo primero que se hara es unirse a una sala luego de haber elegido
su nombre de usuario. Mientras espera que mas jugadores entren a la
sala usted podra: Mandar mensajes a todos los usuarios en la sala y
ver los usuarios que estan en la sala. El juego comenzara al haber
cuatro usuarios en una sala.

2. Espera de turno:

Mientras un usuario esta en la sala podra realizar las acciones 
mencionadas anteriormente, mas poder ver sus cartas actuales y su
vida restante. Ademas en caso de tener 3 cartas de un mismo valor
podra optar por ganar inmediatamente la partida, esto tambien se
puede lograr con la ayuda de los Comodines, pero OJO si intentan
ganar de esta manera y no tienen el trio perderan automaticamente
esa ronda y se comenzara una nueva.

3. Durante su turno:

Durante su turno se le desplegara el contador actual de la partida
teniendo esto en cuenta debera elegir una de sus 3 cartas para
realizar cambios a este contador. A continuacion se lista los cambios
que las diferentes cartas hacen al contador:

- A - 9: Suman al contador su valor, en el caso del As se suma 1
- 10: Suma o resta al contador 10, esto depende del jugador
- J: Resta al contador 10
- Q: Equivale a sumar 0 al contador
- K: Automaticamente iguala el contador a 99
- Joker: Automaticamente iguala el contador a 0

Condiciones de derrota y victoria

Cada jugador comienza con 3 vidas. El objetivo principal del juego
es evitar que el contador se pase de 99. Si esto sucede el jugador
que se paso pierde una vida y comienza una nueva ronda. Si el 
jugador ya no cuenta con vidas queda expulsado del juego, el ultimo
jugador con vidas es el Ganador!

