all: mastermind_server mastermind_client

mastermind_server: mastermind_server.c
    gcc mastermind_server.c -o mastermind_server

mastermind_client: mastermind_client.c
    gcc mastermind_client.c -o mastermind_client