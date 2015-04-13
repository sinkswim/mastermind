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
#define COMBLEN 4 // lunghezza combinazione
#define BUSY 1
#define FREE 0
#define TURNOAVV 0
#define MYTURNO 1

int socketS;
int socketC;

struct sockaddr_in serveraddr;
struct sockaddr_in avvaddr; // indirizzo avversario
struct sockaddr_in myaddr;  
struct timeval timer;
int addrlen ;
int n_giuste = 0 ; // num. corretti e al posto giusto
int n_sbagliate = 0 ; // num. corretti ma al posto sbagliato

//CLIENT DI RIFERIMENTO
char myUsername[32];
unsigned long myIndirizzoIP;
 short int myPortaUDP;
int myComb ;
int myTentativo ;

//AVVERSARIO
char usernameAvv[32];
unsigned long indirizzoIPAvv;
 short int portaUDPAvv;
int tentativoAvv  ; // memorizzo la combinazione che propone l'avversario 	

int turno=1; //avversario 0, cliente di riferimento 1
int n_utenti_connessi;
char comandi[6][30] = {    
	"!help",
	"!who",
	"!quit",
	"!connect",
	"!disconnect",
	"!combinazione"
};

//VARIE
char shell;    
fd_set master;
fd_set read_fds;

int fdmax;
int length;
char help[]=("Sono disponibili i seguenti comandi:\n * !help --> mostra l'elenco dei comandi disponibili\n * !who --> mostra l'elenco dei client connessi al server\n * !connect nome_client --> avvia una partita con l'utente nome_client\n * !disconnect --> disconnette il client dall'attuale partita intrapresa con un altro peer\n * !quit --> disconnette il client dal server\n * !combinazione comb -> permette al client di fare un tentativo con la combinazione comb\n ");

int step=0; // N.B. aspetto sfidante o scrivo da tastiera


//FUNZIONI TCP e UDP
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

int sendUDPNumber(int socket , int * buffer ) {
		int n ;
		n = sendto(socketC, (void*)buffer , sizeof(int) , 0 , (struct sockaddr*) &avvaddr,sizeof(avvaddr));
		if ( n < sizeof(int) ){ 
		printf("Errore durante il comando sendUDPNumber() \n");
		exit(1);
		}
	else return n ;
}

int sendUDP(int socket , char * buffer , int length) {
		int n ;
		n = sendto(socketC, (void*)buffer , length , 0 , (struct sockaddr*) &avvaddr,sizeof(avvaddr));
		if ( n < length){ 
		printf("Errore durante il comando sendUDP \n");
		exit(1);
		}
	else return n ;
}

int receiveUDPNumber( int socket , int * buffer) {
		int n ;
		n = recvfrom(socketC, (void*)buffer ,sizeof(int) , 0 , (struct sockaddr*) &avvaddr,(socklen_t*)&addrlen);
		if ( n < sizeof(int) ){ 
		printf("Errore durante il comando recvfrom() \n");
		exit(1);
		}
	else return n ;
}

int receiveUDP( int socket , char * buffer , int length ) {
		int n ;
		n = recvfrom(socketC, (void*)buffer ,length , 0 , (struct sockaddr*) &avvaddr,(socklen_t*)&addrlen);
		if ( n < length ){ 
		printf("Errore durante il comando recvfrom() \n");
		exit(1);
		}
	else return n ;
}

//DEFINIZIONE FUNZIONI
void inserisci_combinazione(int * combinazione) {
	int ret ;
	fflush(stdin);
	printf("digita la tua combinazione:");
	ret = scanf("%d",combinazione);
	if ( *(combinazione) > 9999 || *(combinazione) < 0000 || ret == 0 ) {
        printf("combinazione non valida \n" ) ;
        scanf("%*[^\n]") ;
        inserisci_combinazione(combinazione) ;
        return ;
    }
	if ( *combinazione == 0 ) printf("0000");
	else if(*combinazione < 10 ) printf("000%d",*combinazione);
	else if (*combinazione < 100) printf("00%d",*combinazione);
	else if(*combinazione < 1000) printf("0%d",*combinazione);
	else printf("%d",*combinazione);
	printf("\n");
}

void disconnect(int end){
	char cmd;
	if(turno==TURNOAVV && end==1){
		printf("non e' il tuo turno\n");
		return;	
	}
	switch (end) {
		case 0: { // finita per combinazione azzeccata
			cmd='k';
			break;
			}
		case 1: { // abbandono
			cmd='d'; // vedi main
			printf("Ti sei arreso. HAI PERSO\n");
			sendUDP(socketC,&cmd,sizeof(char));
			break;
		}
		case 2: { // timeout
			cmd='t'; // vedi main			
			printf("TIMEOUT!! HAI PERSO\n");
			sendUDP(socketC,&cmd,sizeof(char));
			break;
		}
	}
	printf("Disconnesso da %s \n",usernameAvv);
	sendTCP(socketS,&cmd,sizeof(char));
	shell='>'; // shell comandi
	memset(&avvaddr,0,sizeof(avvaddr)); // riazzero la struttura avvaddr
	return ;
}


void controlla_comb_avv(int tentativo ) { // controllo la comb che ho ricevuto dall'avversario
	n_giuste = 0 ;
	n_sbagliate = 0 ;
	int ret;
	int temp1[4] ;
	int temp2[4] ;
	int resto1=tentativo ;
	int resto2=myComb ;
	int i ;
	int j ;
	temp1[0] = resto1/1000 ;
	resto1 = resto1%1000 ;
	temp1[1] = resto1/100 ;
	resto1 = resto1%100 ;
	temp1[2] = resto1/10 ;
	resto1 = resto1%10 ;
	temp1[3] = resto1 ;
	temp2[0] = resto2/1000 ;
	resto2 = resto2%1000 ;
	temp2[1] = resto2/100 ;
	resto2 = resto2%100 ;
	temp2[2] = resto2/10 ;
	resto2 = resto2%10 ;
	temp2[3] = resto2 ;
	if ( tentativo == myComb ) {
			int k = 4 ;
			printf("HAI PERSO!!\n");
			sendUDPNumber( socketC , &k ) ;
			disconnect(0);
			return ;
		}
	for ( i = 0 ; i < 4 ; i++ ) {
		if ( temp1[i] == temp2[i] )n_giuste++ ;
		else for ( j = 0 ; j < 4 ; j++) {
			if ( temp1[i] == temp2[j] ){
					 n_sbagliate++ ;
					break;
					}
			}
		}		 
	printf("%s,dice ",usernameAvv) ;	 	
	if ( tentativo == 0 ) printf("0000");
	else if(tentativo < 10 ) printf("000%d",tentativo);
	else if (tentativo < 100) printf("00%d",tentativo);
	else if(tentativo < 1000) printf("0%d",tentativo);
	else printf("%d",tentativo);	
	printf("   Il suo tentativo è sbagliato\n") ;
	ret = sendto(socketC,(void *)&n_giuste,sizeof(int),0,(struct sockaddr*)&avvaddr, (socklen_t)sizeof(avvaddr));
	if ( ret < sizeof(int)) {
		printf("errore nella sendto()");
		exit(1);
		}
		printf("é il tuo turno\n") ;
}	

void invia_risposta2() {
	sendUDPNumber(socketC,&n_sbagliate);
	printf("Ho inviato le cifre\n");
	turno = MYTURNO; // è il mio turno 
	printf("é il mio turno \n");
	step = 0 ;
	}

void combinazione () {
	char cmd = 'h' ;
	if(shell=='>') {
		printf("comando valido solo in partita!\n");
		fflush(stdin);
		return;
	}
	if(turno == TURNOAVV) {
		printf("comando valido solo durante il proprio turno!\n");
		fflush(stdin);
		return;
	}
	inserisci_combinazione ( &myTentativo ) ;
	sendUDP(socketC,&cmd,sizeof(char));
	
	sendUDPNumber(socketC,&myTentativo);
	step = 9 ;
	 turno = TURNOAVV ;// non è più il mio turno
	printf(" è il turno di %s \n", usernameAvv) ;
}

void ricevo_risposte1 () { // ricevo il numero delle cifre giuste  al posto giusto
	step = 10 ;
	char cmd ='z'; // dico di inviarmi la seconda parte delle risposte , vedi invia_risposte2()
	int k ;
	receiveUDPNumber(socketC, &k) ;
	if ( k == COMBLEN ){
		printf("HAI VINTO!!\n") ;
		turno = MYTURNO ; // è il mio turno
		step = 0 ;
		disconnect(0);
		return ; 
		}
	 printf("%s dice : %d al posto giusto",usernameAvv,k) ;
	sendUDP(socketC , &cmd , sizeof(char));
}
	
void ricevo_risposte2() { // ricevo il numero di cifre giuste al posto sbagliato 
	step = 0 ;
	int k ;
	receiveUDPNumber(socketC , &k ) ;
	printf("   %d cifre giuste al posto sbagliato\n",k) ;
	turno = TURNOAVV ; //non è il mio turno 
}

int cmdtoint (char* cmd) { // "traduzione" del comando in intero
	int i;
	for(i=0;i<6;i++) {
		if(strcmp(cmd,comandi[i])==0) return i;
	}
	return -1;
}

void avvio_del_gioco() {
	printf("avvio del gioco\n");
	turno = MYTURNO ; // sta a me perchè io ho lanciato la richiesta 
	step=1;	
	return;	
	}

void ricevo_porta(){
	int ret=recv(socketS,(void*)&portaUDPAvv,sizeof(portaUDPAvv),0); // ricevo numero porta avversario
	if (ret < sizeof(portaUDPAvv) ) {
		printf("Errore durante il comando recv\n");
		exit(1);
	}
	step=2;
}

void ricevo_ip(){
	int ret=recv(socketS,(void*)&indirizzoIPAvv,sizeof(indirizzoIPAvv),0); // ricevo indirizzo ip dell'avversario
	if (ret < sizeof(indirizzoIPAvv) ) {
		printf("Errore durante il comando recv\n");
		exit(1);
	}

	memset(&avvaddr,0,sizeof(avvaddr));
	avvaddr.sin_family=AF_INET; // ora ho tutto epr costruire la struttura avvaddr
	avvaddr.sin_port=portaUDPAvv;
	avvaddr.sin_addr.s_addr=indirizzoIPAvv;
	portaUDPAvv=ntohs(portaUDPAvv); // conversione all'host byte order
	printf("Numero porta avversario %d\n",portaUDPAvv);
	printf("%s ha accettato la partita\n",usernameAvv);
	printf("Partita avviata con %s\n",usernameAvv);
	inserisci_combinazione (&myComb ) ; 
	printf("è il tuo turno:\n");
	turno = MYTURNO ; // è il mio turno
	step=0;
	shell = '#';
	return;
}


//FUNZIONI CHE IMPLEMENTANO I COMANDI

void who(){
	char cmd='w';
	sendTCP(socketS,(void *)&cmd,sizeof(char));
	step=3;	
	return;
}

void who0(){
	receiveTCPNumber(socketS,&n_utenti_connessi);
    printf("%d utenti connessi:\n",n_utenti_connessi);
	step=4;
	return;
}

void who1(){	
	receiveTCPNumber(socketS,&length);
	step=5;
}

void who2(){
	char username[32];
	receiveTCP(socketS,username,length);
	username[length]='\0';
	printf("%s ",username);
	step=6;

}

void who3(){
	int stato;	
	receiveTCPNumber(socketS,&stato);
	if(stato == FREE) printf("libero\n");
	else printf("occupato\n");
	n_utenti_connessi--;
	if(n_utenti_connessi==0) step=0;
	else step=4; // cicla finchè n_connessi non è 0 cioè torna ad eseguire la who1	
	return;
}

void connect1() {
	int length;
	char cmd='c';
	if(strcmp(myUsername,usernameAvv)==0) {
		printf("Stai scegliendo te stesso! \n");
		return;
	}
	sendTCP(socketS,&cmd,sizeof(cmd));
	length=strlen(usernameAvv);
	sendTCPNumber(socketS,length);
	sendTCP(socketS,usernameAvv,length);
	printf("in attesa di risposta\n"); // attendo
	receiveTCP(socketS,&cmd,sizeof(char));
	switch(cmd) {
		case 'i': {
			printf("Impossibile connettersi a %s: utente inesistente. \n",usernameAvv);
			break;
		}
		case 'a': {
			avvio_del_gioco();
			break;
		}
		case 'b': {
			printf("Impossibile connettersi: l'utente e' occupato.\n");
			break;
		}
		case 'r': {
			printf("Impossibile connettersi a %s: l'utente ha rifiutato la partita.\n",usernameAvv);
			break;
		}
	}
	return;
}

void quit(){
	char cmd;
	cmd='q'; 
	if(shell=='#') { // se siamo nella scell partita
		disconnect(1); // ti arrendi
	if(turno==TURNOAVV) return;	}
	sendTCP(socketS,&cmd,sizeof(char));
	close(socketC);
	close(socketS);
	printf("Client disconnesso correttamente\n");
	exit(0);
}


//ALTRE FUNZIONI UTILI

void leggi_comando_in_input() {
	char cmd[100];
	fflush(stdin); //per eliminare dal buffer eventuali altri caratteri
	scanf("%s",cmd);
	switch(cmdtoint(cmd)) {
		case 0: {
			printf("%s",help);
			break;
		}
		case 1: { who(); break;}
		case 2: { quit(); break;}
		case 3: { scanf("%s",usernameAvv);
			if(shell=='>') connect1(); 
			else printf("sei gia' in una partita\n");	
			break;}
		case 4: { if(shell=='#') disconnect(1); else printf("Non sei in una partita\n"); break;}
		case 5: { if(shell=='#') combinazione(); else{ printf("Non sei in una partita\n");/*fflush(stdin);*/}; break;}
		default: printf("Il comando non esiste\n");
	}
	return;
}

void connetti_al_server(char* addr, int port) {
	int ret ;
	memset(&serveraddr,0,sizeof(serveraddr));
	ret=inet_pton(AF_INET, addr, &serveraddr.sin_addr.s_addr); // converte i caratteri dati dal buffer addr in indirizzo IP	 e li copia nel serveraddr
	if(ret==0) {
		printf("indirizzo non valido\n");
		exit(1);
	}

	if(port<1024||port>65535) {
		printf("Numero di porta inserito non valido!\n");
		exit(1);
	}
	serveraddr.sin_port=htons(port);
	socketS=socket(AF_INET,SOCK_STREAM,0);
	if(socketS==1) {
		printf("Errore durante la socket()\n");
		exit(1);
	 }
	 serveraddr.sin_family=AF_INET;
	 ret=connect(socketS,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
	 if(ret==-1) {
		printf("Errore durante la connect(). Probabilmente il server non e' raggiungibile.\n");
		exit(1);
	}
	return;
}

void gestisci0(){		// ricevo lunghezza nome sfidante					
	int ret = receiveTCPNumber(socketS,&length) ;
	if(ret==0) { //il server si e' disconnesso
		printf("Il server ha chiuso la connessione.\n");
		fflush(stdout);
		exit(1);
	}
	memset(usernameAvv,0,32);
	step=7;
}	


void gestisci1(){ // ricevo username sfidante e rispondo
	char ris;
	char cmd;
	step=0;
	receiveTCP(socketS , usernameAvv , length) ;
	usernameAvv[length]='\0';
	printf("%s ti ha chiesto di giocare!\n",usernameAvv);
	do {
		scanf("%*[^\n]") ;
		scanf("%c",&ris); //
		printf("Accettare la richiesta? (y|n): ");
		fflush(stdin);
		scanf("%c",&ris);
	}
	while(ris!='y'&& ris!='n');
	if(ris=='y') {
		turno=TURNOAVV; // turno dell'avversario poi il primo è quello che ha fatto la richiesta
		printf("Richiesta di gioco accettata\n");
		printf("E' il turno di %s\n",usernameAvv);
		cmd='a';
		shell = '#';
		sendTCP(socketS , &cmd , sizeof(char));
		inserisci_combinazione(&myComb) ; 
	}
	else {
		printf("Richiesta di gioco rifiutata\n");
		cmd='r';
		sendTCP(socketS , &cmd , sizeof(char)) ;  
	}
	return;
}



/////////////////INIZIO DEL MAIN

int main(int argc,char* argv[]){
	int ret ; 
	int i;
	char cmd;
	 int temp;
	short int port_tmp;


	struct timeval *time;
	
	if(argc!=3) {
		printf("Errore nel passaggio dei parametri\n");
		return -1;
	}
	
	//il client si connette al server
	connetti_al_server(argv[1],atoi(argv[2]));
	printf("Connessione al server %s (porta %s) effettuata con successo\n",argv[1],argv[2]);
	//chiede i dati al client (porta UDP e username)
	printf("Inserisci il tuo nome: ");
	scanf("%s",myUsername);

	length=strlen(myUsername);
	sendTCPNumber(socketS , length);	//invio lunghezza username al server
	sendTCP(socketS , myUsername , length);		//invio username al server
	do {
		printf("inserisci la porta UDP di ascolto: ");
		scanf("%*[^\n]") ;
		ret=scanf("%d",&temp);
		if(temp<1024||temp>65535 || ret == 0) {printf("Numero della porta errato\n"); temp=1;}
		else {myPortaUDP=( short int)temp; temp=0;}	
	}
	while(temp);
	port_tmp=htons(myPortaUDP);
	ret=send(socketS,(void*)&port_tmp,sizeof(port_tmp),0);	//non ha definito TCPsend for short int
	if ( ret < sizeof(port_tmp)) {
		printf("errore nella send() \n");
		exit(1) ;
		}
	receiveTCP(socketS , &cmd , sizeof(char)) ;
	if(cmd=='e') {
		printf("Username gia' in uso\n");
		exit(1);
	}
	socketC=socket(AF_INET,SOCK_DGRAM,0);
	if(socketC==-1) {
		printf("Errore durante la socket()\n");
		exit(1);
	}
	
	memset(&myaddr,0,sizeof(myaddr));
	myaddr.sin_family=AF_INET;
	myaddr.sin_port=htons(myPortaUDP);
	myaddr.sin_addr.s_addr=htonl(INADDR_ANY); // inaddr_any mette in ascolto su qualsiasi interfaccia di rete
	
	ret=bind(socketC,(struct sockaddr*)&myaddr,sizeof(myaddr));
	if(ret==-1){
		printf("Errore durante la bind(): forse la porta che hai scelto e' occupata\n");
		quit();
	}

	FD_ZERO(&master); // azzera il set
	FD_SET(socketS,&master); // metti ad 1 il descrittore relativo al socket del server
	FD_SET(socketC,&master); // metti ad 1 il socket relativo a quello che uso per comunicare con l'avversario
	FD_SET(0,&master);
	fdmax=(socketS>socketC)?socketS:socketC; // scelgo l'estremo superiore
	shell='>'; // shell comando
	printf("%s",help);
	while(1) {		//infinite loop
		read_fds=master; // copio il master così da non "sporcarlo"
		timer.tv_sec=60;
		timer.tv_usec=0;		
		time=&timer;
		if(shell=='>') time=NULL; // siamo in modalità comandi,quindi il timer non viene settato
		if(turno==TURNOAVV) time=NULL; // sta giocando l'avversario quindi il timer non viene settato
//		if(shell=='#') printf("entro in select\n");		
		if(step==0) { printf("%c",shell); fflush(stdout);}
		ret=select(fdmax+1,&read_fds,NULL,NULL,time); 
//		if(shell=='#') printf("esco dalla select\n");
		if(ret==-1) {
			printf("Errore durante la select()\n");
			exit(1);
		}
		if(ret==0) disconnect(2); // timeout, la select torna 0 se scade il timeout
		else for(i=0;i<=fdmax;i++) {
			if(FD_ISSET(i,&read_fds)) { //descrittore pronto
				if(i==0) { //descrittore pronto e' stdin 
					//leggo comando input
					leggi_comando_in_input();
				}
				if(i==socketS) { //descrittore pronto e' server
					if(step==1) ricevo_porta();
					else if(step==2) ricevo_ip();					
					else if(step==3) who0();
					else if(step==4) who1();
					else if(step==5) who2();
					else if(step==6) who3();
					else if(step==7) gestisci1();					
					else gestisci0(); // ricevo richiesta di sfida			
				}
				if(i==socketC) {
					if(step==8) {
						step=0;	
						addrlen=sizeof(avvaddr);					
						receiveUDPNumber(socketC,&tentativoAvv);
						controlla_comb_avv (tentativoAvv) ; 
					}
					else if( step == 9 ) 
						ricevo_risposte1() ;
					else if ( step == 10 ) 	
						ricevo_risposte2() ;
					else {
						receiveUDP(socketC , &cmd , sizeof(char) ) ;
						switch(cmd) {
							case 'h': {
								step=8;
								break;
							}
							case 'z' : {
								invia_risposta2();
								break;
								}
							case 'd': { //disconnessione dell'avversario
								printf("%s si e' arreso. HAI VINTO!\n",usernameAvv);
								shell='>';
								memset(&avvaddr,0,sizeof(avvaddr));
								break;
							}					
							case 't': { //disconnessione dell'avversario
								printf("timeout per %s. HAI VINTO!\nDisconnesso da %s\n",usernameAvv,usernameAvv);	
								shell='>';
								memset(&avvaddr,0,sizeof(avvaddr));
								break;
							}
						}
					}
				}		
			}
		}
	}	
	return 0;
}
