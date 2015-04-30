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
  
  After that you are prompted with entering the username and the client's port number. Thanks to this number clients can be connected using the P2P model.
