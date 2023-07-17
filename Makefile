# Makefile para compilar o servidor e o cliente

# Comando "all"
all: server client

# Compilação do servidor
server: server.o utils.o
	gcc -Wall -o server server.o utils.o

# Compilação do cliente
client: client.o utils.o
	gcc -Wall -o client client.o utils.o

# Compilação dos objetos
server.o: server.c
	gcc -Wall -c -o server.o server.c

utils.o: utils.c
	gcc -Wall -c -o utils.o utils.c

client.o: client.c
	gcc -Wall -c -o client.o client.c

# Limpar arquivos objeto e executáveis
clean:
	rm -f server client server.o utils.o client.o
