all: server card_game

server: server.cpp protocol.pb.cc
	g++ -o server server.cpp protocol.pb.cc -lpthread -lprotobuf

card_game: card_game.cpp protocol.pb.cc
	g++ -o card_game card_game.cpp protocol.pb.cc -lpthread -lprotobuf

protocol.pb.cc: protocol.proto
	protoc -I=. --cpp_out=. protocol.proto