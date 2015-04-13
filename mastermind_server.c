//Robert Margelli, matricola 478683
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/time.h>

// DEFINIZIONI
#define MAXPENDING 10
#define BUSY 1
#define FREE 0


struct utente {
	char username[32];
	char usernameAvv[32];	
    short int portaUDP;
	int socket;
	unsigned long indirizzoIP;
	int stato;	//BUSY or FREE
	int step;	//da 0 a 10
	int length;
	struct utente *avversario;
	struct utente *puntatore; // prossimo elemento nella lista degli utenti
};

struct utente* nuovo=0; // lista utenti nuovi,formata al massimo da 1 elemento
struct utente* utenti_connessi=0;
struct utente* provvisorio=0;
int n_utenti_connessi=0;
struct utente *client=0; // attuale client

struct sockaddr_in serveraddr;
struct sockaddr_in clientaddr;
char comando;
int length;
int listener;
fd_set master;
fd_set read_fds;
int fdmax;





///////////////////FUNZIONI PER LA GESTIONE DELLA LISTA
void rimozione_elemento_da_lista(struct utente* elem) { // rimuovi dalla lista degli utenti connessi
	struct utente* temp=utenti_connessi;
	if(n_utenti_connessi==0)
		return;
	if(temp==elem) {	//e' il primo nella lista
		utenti_connessi=utenti_connessi->puntatore;
		free(temp);
		n_utenti_connessi--;
		return;
	}
	while(temp->puntatore!=elem && temp->puntatore!=NULL)	//scorre la lista
		temp=temp->puntatore;
	if(temp->puntatore==NULL)	//non lo ha trovato
		return;
	n_utenti_connessi--;
	temp->puntatore=temp->puntatore->puntatore;   
	return;
}

void rimuovi_da_provvisorio(struct utente* elem) {
	struct utente* temp=provvisorio;
	if(temp==elem) {	//e' il primo nella lista
		provvisorio=provvisorio->puntatore;
	return;
	}
	while(temp->puntatore!=elem && temp->puntatore!=NULL)	//scorre la lista
		temp=temp->puntatore;
	if(temp->puntatore==NULL)
		return;
	temp->puntatore=temp->puntatore->puntatore;
	return;
}


struct utente* trova_conn_da_socket(int sd) {
	struct utente* temp=utenti_connessi;
	while(temp!=NULL) {
		if(temp->socket==sd)
			return temp;
		temp=temp->puntatore;
	}
	return NULL;
}

struct utente* trova_provv_da_socket(int sd) {
	struct utente* temp=provvisorio;
	while(temp!=NULL) {
		if(temp->socket==sd)
			return temp;
		temp=temp->puntatore;
	}
	return NULL;
}


struct utente* trova_da_username(char* name) { 
	struct utente* temp=utenti_connessi;
	while(temp!=NULL) {
		if(strcmp(temp->username,name)==0)
			return temp;
		temp=temp->puntatore;
	}
	return NULL;
}

int esiste_conn(char* name) { // RITORNA 1 SE ESISTE IL NOME (o O) TRA GLI UTENTI CONNESSI
	struct utente* temp=utenti_connessi;
	while(temp!=NULL) {
		if(strcmp(temp->username,name)==0) 
			return 1;
		temp=temp->puntatore;
	}
	return 0;
}

//FUNZIONI DI COMUNICAZIONE TCP

int sendTCPNumber ( int socket,int j) {
	int n ;
	n = send(socket , (void*) &j ,sizeof(int) , 0 ) ;
	if(n < sizeof(int)) {	
		printf("Errore durante il comando send()\n");
		exit(1);
	}
	else return n ;
}

int sendTCP ( int socket,char* buffer,int length) {
	int n ;
	n = send(socket , (void*) buffer ,length, 0 ) ;
	if ( n < length ){
		printf("Errore durante il comando send()\n");
		exit(1);
		}
	else return n ; // ritorna numero di bytes(caratteri) inviati
}

int receiveTCPNumber ( int socket , int * number ) {
	int n ;
	n = recv(socket , (void*) number, sizeof(int) , 0 ) ;
	if ( n < sizeof(int) ){ 
		printf("Errore durante il comando recv() \n");
		exit(1);
		}
	else return n ;
}

int receiveTCP ( int socket , char * buffer , int length ) {
	int n ;
	n = recv(socket , (void*) buffer, length , 0 ) ;
	if ( n < length ){
		printf("Errore durante il comando recv()\n");
		exit(1);
		}
	else return n ;
}

	


//FUNZIONI CHE IMPLEMENTANO I COMANDI DISPONIBILI NEL SERVER

void who() {
	int length;
	struct utente* temp=utenti_connessi;
	//dice al client quanti utenti sono connessi
	 sendTCPNumber(client->socket, n_utenti_connessi ) ;
	while(temp) {
		length=strlen(temp->username);
		sendTCPNumber(client->socket,length);	//lunghezza
		sendTCP(client->socket,temp->username,length);		//username
		sendTCPNumber(client->socket,temp->stato);		//stato utente
		temp=temp->puntatore;
	}
}

void disconnect(char cmd) { // disconnesso dalla partita aggiorno strutture dati client attuale e avversario
	printf("%s si e' disconnesso dalla partita \n",client->username);
	printf("%s e' libero\n", client->username);
	if(cmd!='k') {	// si è disconnesso (avversario)
		printf("%s si e' disconnesso dalla partita \n",client->avversario->username);
		printf("%s e' libero\n", client->avversario->username);
		client->avversario->stato=FREE;
		client->avversario->avversario=NULL;
			
	}
	client->stato=FREE; 
	client->avversario=NULL;
}

void connect1() { // ricevo la lunghezza del nome dell'avversario
	receiveTCPNumber(client->socket,&client->length);
	client->step=5;
}
void connect2() { // ricevo nome avversario e aggiorno strutture dei 2 avversari(li faccio puntare a vicenda e li metto BUSY)
	char cmd;
	//lunghezza username al quale si vuole connettere
	client->step=0;	
	receiveTCP(client->socket,client->usernameAvv,client->length) ;
	client->usernameAvv[client->length]='\0';
	//dice il client che sta rispondendo al comando !connect
	if(!esiste_conn(client->usernameAvv)) {
		printf("Utente %s non trovato\n",client->usernameAvv);		
		cmd='i'; // comando di utente inesistente
		sendTCP(client->socket,&cmd,sizeof(char));
		return;
	}
	////INVIO AL DESTINATARIO DELLA RICHIESTA INFORMAZIONI SUL CLIENT CHE VUOLE GIOCARE CON LUI
	client->avversario=trova_da_username(client->usernameAvv);
	if(client->avversario->stato== BUSY) {	
		printf("Utente %s occupato\n",client->usernameAvv);		
		cmd='b'; // comando di utente occupato
		sendTCP(client->socket,&cmd,sizeof(char));
		return;
	}
	//comando scelto per inoltrare la richiesta al client
	client->stato= BUSY;
	client->avversario->stato= BUSY;	
	client->avversario->avversario=client;
	length=strlen(client->username);
	sendTCPNumber(client->avversario->socket,length);	//lunghezza username
	sendTCP(client->avversario->socket,client->username,length);	//username
}

void connect3(char cmd) { // gestione risposta destinatario della richiesta di sfida
	
	  short int porta;
	switch(cmd) { //switch che gestisce la scelta del client interrogato
		case 'a': { //accetta
			sendTCP(client->avversario->socket,&cmd,sizeof(char));
			//invio della porta di client
			porta=htons(client->portaUDP);
			int ret=send(client->avversario->socket,(void *)&porta,sizeof(porta),0);// faccio una send normale perchè è un short int e non ho definito una funzione per questo tipo
			if ( ret < sizeof(porta)){
				printf("Errore durante il comando send()");
				exit(1);
				}
			 ret=send(client->avversario->socket,(void *)&client->indirizzoIP,sizeof(client->indirizzoIP),0); // faccio una send normale perchè è un unsignend long e non ho definito una funzione per questo tipo
			if ( ret < sizeof(client->indirizzoIP)){
				printf("Errore durante il comando send()");
				exit(1);
				}
			printf("%s si e' connesso a %s\n",client->username,client->avversario->username);
			break;
		}
		case 'r':	{ //rifiutato
			sendTCP(client->avversario->socket,&cmd,sizeof(char));
			client->stato=FREE;
			client->avversario->stato=FREE;
			client->avversario->avversario=NULL;
			client->avversario=NULL;						
			break;
		}
		//non vi e' caso default
	}
}

void quit() {
	
	printf("Il client %s si e' disconnesso dal server\n",client->username);
	close(client->socket);
	FD_CLR(client->socket,&master); // metto a 0 il bit riguardante il client così non viene controllato se è pronto
	rimozione_elemento_da_lista(client); // rimuovo da lista utenti connessi
}

void inizializza1(){
	receiveTCPNumber(client->socket,&length); // ricevo lunghezza nome
	client->length=length;
	client->step=2;
}

void inizializza2() {	//ricevo il nome
	
	receiveTCP(client->socket,client->username,length);
	client->username[client->length]='\0';//gestione stringa: carattere di fine string
	client->step=3;
}

void inizializza3(){	//qui si rimuove definitivamente un utente dalla lista provvisoria e si mette in testa ai connessi
	char cmd='@';	
	int ret ;
	ret = recv(client->socket,(void *)&client->portaUDP,sizeof(client->portaUDP),0); // PORTA UDP è short int 
	if ( ret < sizeof(client->portaUDP)) {
		printf("Errore durante il comando recv() \n");
		exit(1);
		}
	client->portaUDP=ntohs(client->portaUDP);
	rimuovi_da_provvisorio(client); // levo dalla lista provvisoria
	if(esiste_conn(client->username)) { // vale 1 se esiste gia un utente con quel username
		cmd = 'e'; //esiste_conns
		sendTCP(client->socket,&cmd,sizeof(cmd));		
		printf("Il client ha usato un username non valido!\n");
		close(client->socket);
		FD_CLR(client->socket,&master); // metto a 0 il bit riguardante il client così non viene controllato se è pronto
		free(client);// libero la memoria
		client=NULL;
		return;
	}		
	client->puntatore=utenti_connessi; // lo metto in testa alla lista 
	utenti_connessi=client; // aggiorno puntatore testa della lista connessi
	n_utenti_connessi++;
	sendTCP(client->socket,&cmd,sizeof(cmd));
	client->step=0;
	printf("%s si e' connesso\n",client->username);
	printf("%s e' libero\n",client->username);
}


/////////////////INIZIO DEL MAIN
int main(int argc, char* argv[]) {
	int i; 
	int addrlen; //lunghezza dell'indirizzo
	int des_conn_sock;  //descrittore del connected socket
	int yes=1;
	int ret ; // variabile per i ritorni
	if(argc!=3) { //ret sul numero dei parametri
		printf("Errore durante il passaggio dei parametri\n");
		exit(-1);
		}
	 ret=atoi(argv[2]);
	if(ret<1024||ret>65535) {
		printf("Errore nel numero di porta scelto\n");
		exit(1);
	}
	memset(&serveraddr,0,sizeof(serveraddr));
	serveraddr.sin_family=AF_INET; // protocolli internet IPv4
	serveraddr.sin_port=htons(ret); // converte nel network order format
	ret=inet_pton(AF_INET,argv[1],&serveraddr.sin_addr.s_addr); // da presentazione a formato numerico
	if(ret==0) {
		printf("indirizzo non valido!\n");
		exit(1);
	}
	printf("Indirizzo: %s (Porta: %s)\n",argv[1],argv[2]);
	listener=socket(AF_INET,SOCK_STREAM,0); // socket TCP
	if(listener==-1){
		printf("errore nella creazione del socket (server)\n");
		exit(1);
	}
	if(setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))==-1) {
		printf("Errore durante la setsockopt()\n");
		exit(1);
	}
	ret=bind(listener,(struct sockaddr *)&serveraddr,sizeof(serveraddr)); // cast da sockaddr_in ->sockaddr
	if(ret==-1){
		printf("errore durante la bind(): forse un server e' gia' attivo\n");
		exit(1);
	}
	ret=listen(listener,MAXPENDING);
	if(ret==-1){
		printf("errore listen()\n");
	exit(1);
	}
	FD_ZERO(&master); // inizializzo il set
	FD_ZERO(&read_fds); // inizializzo il set
	FD_SET(listener,&master); // metto ad 1 il bit relativo a listener,cioè al server
	fdmax=listener; // quello di indice maggiore, per ora listener e' l'unico
	for(;;){		//infinite loop
		read_fds=master; // copia dei descrittori da controllare perchè viene modificato il set di input(nel senso che la select mi mette a 1 i descrittori pronti e a 0 quelli non pronti)
		ret=select(fdmax+1,&read_fds,NULL,NULL,NULL);
		if(ret==-1) {
			printf("errore durante la select()\n");
			exit(1);
		}
		for(i=0;i<=fdmax;i++) {
			if(FD_ISSET(i,&read_fds)) { // controlla se un descrittore è pronto,cioè settato
				if(i==listener) { // se è il listening socket
					addrlen=sizeof(clientaddr);
					des_conn_sock=accept(listener,(struct sockaddr *)&clientaddr,(socklen_t *)&addrlen);
					if(des_conn_sock==-1)  {
						printf("Errore durante la accept()\n");
						exit(1);
					}
					printf("Connessione stabilita con il client\n");
					FD_SET(des_conn_sock,&master); // setto il descrittore(cioè il socket appena connesso) nel master
					if(des_conn_sock>fdmax) fdmax=des_conn_sock; // scelgo l'estremo superiore
					nuovo=malloc(sizeof(struct utente)); // metto in testa alla lista "nuovo" l'utente appena connesso
					nuovo->indirizzoIP=clientaddr.sin_addr.s_addr;
					nuovo->socket=des_conn_sock;
					nuovo->stato=FREE;
					nuovo->avversario=NULL;
					nuovo->step=1;
					nuovo->puntatore=provvisorio;
					provvisorio=nuovo; // inserisco in testa alla lista "provvisorio"
					nuovo=NULL;		
				}	//chiusura caso i==listener
				else {
					client=trova_provv_da_socket(i); 
					if(client!=NULL) { 		// ne ha trovato uno
											
						if(client->step==1) 
							inizializza1();
						else if(client->step==2)
							inizializza2();
						else if(client->step==3)
							inizializza3(); // è qui che lo levo dalla lista provvisoria perchè si è registrato completamente 
					}
					else {
						client=trova_conn_da_socket(i); // passo il socket e vado a cercare nella lista utenti_connessi
						if(client == NULL) {
							printf("il client non esiste\n");
							exit(1);
						
						}
						if(client->step!=0) {
							switch(client->step) {
								case 4: {
									connect1(); // vedi commento connect1()
									break;
								}
								case 5: {
									connect2(); // vedi commento connect2()
									break;
								}				
							}
						}						
						else	{ // caso step == 0
							ret=recv(i,(void *)&comando,sizeof(comando),0);
							if(ret==0) {
								quit(); // disconnessione per inattività
							}							
						       else  if ( ret < sizeof(comando)) {
								printf("Errore durante il comando recv()\n");
								exit(1);
								}
							switch(comando) {
								case 'r':
								case 'a': {
									connect3(comando); //vedi commento connect3()
									break;
								}
								case 'w': {
									who();
									break;
								}
								case 'k': case 't':
								case 'd': {
									disconnect(comando);
									break;
								}
								case 'c': { // !connect
									client->step=4;
									break;
								}
								case 'q': {
									quit();
									break;
								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}



