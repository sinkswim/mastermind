# mastermind
Network based mastermind game (aka bulls and cows) for UNIX based systems.

##compiling
make...

##running
You need a terminal window/tab for the server and one for each client.

Running the server:

  ./mastermind_server 127.0.0.1 1234
  
Where you are passing the network address and port number of the server.
  
Running the client:
  ./mastermind_client 127.0.0.1 1234
  
  Where you are passing the network address and the port of the server you want to connect to.
  
