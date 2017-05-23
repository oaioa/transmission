#include <stdio.h> //printf
#include <string.h> //memset
#include <stdlib.h> //exit(0);
#include <arpa/inet.h>
#include <math.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SERVER "127.0.0.2"

//Max length of ONE fragment from server: 1500
//Ethernet frame bytes (fixed) = 18
//IP header (min) = 20
//UDP header (fixed) = 8
//Max. allowed payload for no fragmentation = 1500
#define FRAGLEN 1500
#define BUFLEN 10000000  //Max length of server buffer: 10Mo
#define INITIAL_RTT_MICROSEC 100000

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
//generating a file of 1MBytes: dd if=/dev/zero of=a  bs=1M  count=1
//printf in red: printf(ANSI_COLOR_RED     "This text is RED!"     ANSI_COLOR_RESET "\n");


void die(char *s)
{
    printf("Adios...\n");
    perror(s);
    exit(1);
}

int delete(char buf[], int taille){
    int i = 0;	
    for(i = 0 ; i<taille ; i++){
        buf[i]='\0';
    }
    return 1;
}

//send the message
int send_message(int socket,char* message, size_t message_len,struct sockaddr *si_other,int si_other_len){
	int sent = -1;
    if ((sent = sendto(socket, message, message_len , 0 , (struct sockaddr *) si_other,si_other_len))==-1)
    {
        die("sendto()");
    }
    return sent;
}

//receive the message
int receive_message(int socket, char* buf,struct sockaddr *si_other, int * si_other_len){
    int recv = -1;
    struct timeval tv;

	//set recvfrom as a blocking call
	tv.tv_sec = 0;  // 0 secs Timeout
	tv.tv_usec = 0; // 0 µsec
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,(char*)&tv,sizeof(struct timeval )); 
	
    
    if ((recv = recvfrom(socket, buf, BUFLEN, 0, (struct sockaddr *) si_other, si_other_len)) == -1)
    {
		perror("Error");
    }
    return recv;
}

int string_compare(char * s1, int size){ //size = strlen(buf);
	if (size <7){
		return -1;
	}else{
		int i = 0;
		char s2 [7] = "SYN-ACK";
		int same_letter = 0; //on suppose que la premiere lettre est ok
		do{
			if(s1[i] != s2[i])
				same_letter = -1;
			i++;
		}while(same_letter == 0 && i <7);
		
		return same_letter;
	}
}

int create_server(int *s, struct sockaddr_in* si_me, int port){

	int res = -4;
	
    //create a UDP socket
    if ((*s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }
    res++; //-3
	
    // zero out the structure
    memset((char *) si_me, 0, sizeof(*si_me));
    res++; //-2
	
	//parameters
    (*si_me).sin_family = AF_INET;
    (*si_me).sin_port = htons(port);
    (*si_me).sin_addr.s_addr = htonl(INADDR_ANY);
    res++; //-1
	
	//enable the reusing (avoids bind problem)
	int on = 1;
	setsockopt(*s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	
    //bind socket to port
    if( bind(*s , (struct sockaddr*)si_me, sizeof(*si_me) ) == -1)
    {
        die("bind");
    }
	
	res++; //0
	return res;
	
}

int connect_to_port (int *s, int port, struct sockaddr_in* si_other){
	int res = -3;
	
	
	//create socket 
	if ( (*s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		die("socket");
	}
	res++;//-2
	
	//parameters
	memset((char *) si_other, 0, sizeof(*si_other));
	(*si_other).sin_family = AF_INET;
	(*si_other).sin_port = htons(port);
	res++; //-1
	
	//connection
	if (inet_aton(SERVER, &((*si_other).sin_addr)) == 0) 
	{
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}	
	res++;//0

	return res;

}

int insert_a_message(char ** message){
	printf("Entrez votre message : ");
	fgets((*message), BUFLEN, stdin);
	strtok((*message), "\n"); //n'integre pas le retour chariot dans le message envoyé sauf si message vide
				
	return 0;
}


//receive the message with a timeout
int rcv_msg_timeout(int socket, char* buf,struct sockaddr *si_other, int * si_other_len, int RTT_microsec){
    int recv = -1;
    struct timeval tv;
    
    //C'EST ICI QUE CA BUG SA MERE AU BOUT D'ENVIRONS 13000FRAGMENTS
    if (RTT_microsec <= 0){
		printf("Erreur de RTT! BYE\n");
		exit(1);
	}else{
		//printf(ANSI_COLOR_RED "RTT OK: %d µsec" ANSI_COLOR_RESET"\n",RTT_microsec);
	}
	
    tv.tv_sec = 0;// 0 sec Timeout
    
    //depends on value of RTT
    if (RTT_microsec <=INITIAL_RTT_MICROSEC){ //if RTT small enough
		//printf(ANSI_COLOR_RED "TIMEOUT OK" ANSI_COLOR_RESET"\n");
		tv.tv_usec = 3* RTT_microsec;
	}else{
		//printf(ANSI_COLOR_RED "TIMEOUT TROP GRAND!\n");
		tv.tv_usec = INITIAL_RTT_MICROSEC;
		//printf("On change en %lu microsec"ANSI_COLOR_RESET"\n",(long int)tv.tv_usec);
	}
	
	
	if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,(char*)&tv,sizeof(struct timeval ))<0){
		
		printf(ANSI_COLOR_RED);
		printf("Le RTT ne convient pas: %lu sec & %lu %sec\n",(long int) tv.tv_sec, (long int)tv.tv_usec);
		perror("Erreur de timeout: ");
		printf(ANSI_COLOR_RESET);
		kill(getpid(),SIGINT);
		exit(1);
	}
	
	if ((recv = recvfrom(socket, buf, BUFLEN, 0, (struct sockaddr *) si_other, si_other_len)) < 0){
		//printf(ANSI_COLOR_RED "TIMEOUT REACHED: %lu sec & %lu µsec" ANSI_COLOR_RESET"\n",(long int) tv.tv_sec, (long int)tv.tv_usec);
	
    }else{
		//printf("bien recu: %s de taille %d\n", buf,recv);
	}
	
	
    return recv;
    
}


//count the digits of a given number
int count_digits(int i){
   
	if (i < 9)
		return 1;
	else if (i < 99)
		return 2;
	else if (i < 999)
		return 3;
	else if (i < 9999)
		return 4;
	else if (i < 99999)
		return 5;
	else if (i < 999999)
		return 6;
	else if (i < 9999999)
		return 7;
	else if (i < 99999999)
		return 8;
	else if (i < 999999999)
		return 9;
	else if (i < 9999999999)
		return 10;
}

int max(int a, int b){
	if (a>b)
		return a;
	else
		return b;
}


int min(int a, int b){
	if (a<b)
		return a;
	else
		return b;
}

void displayGraph(int* tab, int len){
	int i = 0;
	
	for (i = 0 ; i < len ; i++){
		
		if (tab[i] ==0){
			//break;
		}else{
			printf("%d",tab[i]);
			if (i < len-1 && tab[i+1] !=0){
				printf(", ");
			}
		}
	}
	printf("\n");
}

void displayAbsiTime(double* tab, int len){
	int i = 0;
	
	for (i = 0 ; i < len ; i++){
		
		if (tab[i] ==0){
			//break;
		}else{
			printf("%f",tab[i]);
			if (i < len-1 && tab[i+1] !=0){
				printf(", ");
			}
		}
	}
	printf("\n");
}

//export into a [graphName] file
void outGraph(FILE* f, int* tab, int len, char* graphName){
	int i = 0;
	
	//if the graph already exists
	if (access(graphName, 0) == 0) {
		//printf(ANSI_COLOR_BLUE"Suppression de l'ancien %s...\n",graphName);
		if (remove(graphName) != 0){
			//printf("Impossible de supprimer %s!\n",graphName);
		}else{				
			//printf("Suppression de %s OK\n",graphName);
		}
	}else{
		//printf("Création du fichier %s...\n",graphName);
	}
	
	if ((f= fopen(graphName,"a")) == NULL) {	//a because appending
		die("Probleme ouverture fichier de graphe\n");
	}else{
		//printf("Ecriture du fichier %s\n",graphName);
		for (i = 0 ; i < len ; i++){ 
			if (tab[i] ==0){
				break;
			}else{
				fprintf(f,"%d",tab[i]);
				if (i < len-1 && tab[i+1] !=0){
					fprintf(f,",");
				}
			}
		}
		//printf("Ecriture terminée!\n");	
	}
	
	fprintf(f,"\n");
	printf("Fichier " ANSI_COLOR_RED"%s" ANSI_COLOR_BLUE" créé.\n", graphName);
}

//export into a [graphName] file for abscisse
void outGraphTime(FILE* f, double* tab, int len, char* graphName){
	int i = 0;
	
	//if the graph already exists
	if (access(graphName, 0) == 0) {
		//printf(ANSI_COLOR_BLUE"Suppression de l'ancien %s...\n",graphName);
		if (remove(graphName) != 0){
			//printf("Impossible de supprimer %s!\n",graphName);
		}else{				
			//printf("Suppression de %s OK\n",graphName);
		}
	}else{
		//printf("Création du fichier %s...\n",graphName);
	}
	
	if ((f= fopen(graphName,"a")) == NULL) {	//a because appending
		die("Probleme ouverture fichier de graphe (abscisse)\n");
	}else{
		//printf("Ecriture du fichier %s\n",graphName);
		for (i = 0 ; i < len ; i++){ 
			if (tab[i] ==0){
				break;
			}else{
				fprintf(f,"%f",tab[i]);
				if (i < len-1 && tab[i+1] !=0){
					fprintf(f,",");
				}
			}
		}
		//printf("Ecriture terminée!\n");	
	}
	
	fprintf(f,"\n");
	printf("Fichier " ANSI_COLOR_RED"%s" ANSI_COLOR_BLUE" créé.\n", graphName);
}


int seek(void* dest,void* src,int id_frag, int id_lastfrag, int filesize){
	
	int counter = 0;
	void* visitor = src;
	int res = 0;
	
	//not the last one
	if (id_frag < id_lastfrag){
		memcpy(dest,visitor + (FRAGLEN-6)*(id_frag-1),FRAGLEN-6);
		res = FRAGLEN-6;
	
	//the last fragment
	}else if (id_frag == id_lastfrag){
		//count the bits inside the last fragment
		counter = filesize%(FRAGLEN-6);
		
		//copy the last fragment 
		memcpy(dest,visitor + (FRAGLEN-6)*(id_frag-1),counter);
		res = counter;
	
	//outside the data	
	}else if (id_frag > id_lastfrag){
		res = -1;
	}
	return res;			
}


//get the 6digits number from ACK[NUMBER]
int getACK(char* s, int n){
	int i=0, res =0, temp = 0;
	
	for (i = 3 ; i<=n+2 ; i++){
		temp = s[i] - '0';
		res = res + temp*pow(10,n-i+2);
	}
	
	return res;
	
}

//get the 2nd number after the coma
int digitAfterComa(double tmpTime){
	int n = 0;
	int tmp =  floor(tmpTime*100);
	n = (int) (tmp %100);
	
	return n;
	
}


