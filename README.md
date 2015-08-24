# mastermind
Terminal based network mastermind game (aka bulls and cows) for UNIX based systems.

##compiling
make

##running
You need a terminal window/tab for the server and one for each client.

Running the server:   ./mastermind_server 127.0.0.1 1234
  
Where you are passing the network address and port number of the server.
  
Running the client:   ./mastermind_client 127.0.0.1 1234
  
  Where you are passing the network address and the port of the server you want to connect to. This establishes a typical client-server connection.
  
  After that you are prompted with entering the username and the client's port number (thanks to this number, clients can connect using the P2P model). A list of all the commands and their description will be printed, at any given time these can be reprinted using the "!help" command. 
