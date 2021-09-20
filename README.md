# Redes Proyecto 2
## Objetivos
- Aplicar los conocimientos de TCP para mantener sincronizados los estados internos de distintos clientes de una aplicación

- Implementar un juego de cartas llamativo para el usuario

## Desarrollo
Implementación de un protocolo TCP para la realización de un juego de cartas (99)


## Prerequisitos
- Instalar Protobuf (Ubuntu)
```
$ sudo apt install -y protobuf-compiler
```
- La version a instalar debe ser la 3.6.1
```
$ protoc --version  # Ensure compiler version is 3+
```

## Ejecucion 

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

## Grupo de trabajo
- Ricardo Valenzuela 
- Diego Solorzano
- Sara Zavala
