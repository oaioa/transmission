#include "useful.h"
//ONLY FOR ONE CLIENT2!!!


int main(int argc, char *argv[]) {
	struct sockaddr_in si_me; //server struct addr
	struct sockaddr_in si_other; //client struct addr
	int s; //socket
	int slen = sizeof(si_other);
	char *buf = (char*) malloc(sizeof(char) * BUFLEN); //recu
	char *message = (char*) malloc(sizeof(char) * FRAGLEN); //envoyÃ©
	int i, j; //filling the temporary little buffer
	int beginning = 2;
	int pid_fils = -1;
	int compare = 1; // il y a une difference
	FILE * f_in;
	int id_frag = 0, lastACK = 0, id_lastfrag = 0, oldACK = 0, nb_ACK = 0, firstprint_SS = 0, currentACK = 0;
	int read = FRAGLEN, sent = 0, recv = 0, len = 0;
	char * ENTREE = (char*) malloc(sizeof(char) * FRAGLEN); //what server is sending
	char* data;

	struct timeval end, start, beginread;
	double delta = 0;
	void *res = (void*) malloc(sizeof(char) * FRAGLEN + 7); //what's finally sent (id_frag + data)
	
	int cwnd = 0; //congestion window
	int flightSize = 0; //data that has been sent, but not yet acknowledged
	int PORT = 0;
	int INITIAL_CWND = 0;

	if (argc < 2) {
		printf(
		ANSI_COLOR_RED "./server1-explorers server_port" ANSI_COLOR_RESET"\n");
		exit(1);
	} else {
		PORT = atoi(argv[1]);
		if (PORT > 1023 && PORT < 9999) {
			printf(ANSI_COLOR_RED"Port: %d "ANSI_COLOR_RESET "\n", PORT);
		} else {
			printf(ANSI_COLOR_RED"Error: 1023 < server_port < 9999! " ANSI_COLOR_RESET"\n");
			exit(1);
		}
	}

	//
	if (create_server(&s, &si_me, PORT) != 0) {
		die("Error on creating first server\n");
	}

	//we clean up the buffer
	delete(buf, sizeof(buf));

	//keep listening for data
	while (1) {
		id_frag = 0, lastACK = 0, id_lastfrag = 0, oldACK = 0, nb_ACK = 0, firstprint_SS = 0, currentACK = 0, sent = 0, recv = 0, len = 0;
		
		receive_message(s, buf, (struct sockaddr *) &si_other, &slen);
		printf(ANSI_COLOR_YELLOW "Recu sur le port %d : '%s'" ANSI_COLOR_RESET "\n", PORT, buf);
		
		//accepting connexion process... 
		if (beginning == 2){//received syn
			
			//printf(ANSI_COLOR_YELLOW "Beginning!" ANSI_COLOR_RESET "\n");

			if (strcmp(buf,"SYN") ==0){//sending syn-ack
				
				sprintf(message, "SYN-ACK%d", PORT);
				send_message(s, message, 15, (struct sockaddr *) &si_other, slen);
				beginning =1;
					
			} else{
				printf(ANSI_COLOR_RED"Not a SYN." ANSI_COLOR_RESET"\n");
				beginning = 2;
			}
			
		} else if (beginning == 1){//waiting for ack
			if (strcmp(buf,"ACK") ==0){//receiving ack
				beginning =0;
					
			} else{
				printf(ANSI_COLOR_RED"Not a ACK." ANSI_COLOR_RESET"\n");
				beginning = 1;
			}
			
		} else if (beginning == 0){//connexion accepted
			
			ENTREE = buf;
			//opening file 
			if ((f_in = fopen(ENTREE, "rb")) == NULL) {	//b because binary
				printf(
				ANSI_COLOR_RED "Fichier %s inexistant." ANSI_COLOR_RESET "\n",ENTREE);
				send_message(s, "FIN", 4, (struct sockaddr *) &si_other, slen);

			} else {
				printf(ANSI_COLOR_BLUE"\nEnvoi du fichier %s...\n",ENTREE);

				fseek(f_in, 0, SEEK_END);
				len = ftell(f_in);
				id_lastfrag = (int) (len / (FRAGLEN - 6)) + 1;
				printf("Taille: %d\n", len);
				printf("Nb de fragments: %d" ANSI_COLOR_RESET"\n\n",id_lastfrag);
				fseek(f_in, 0, SEEK_SET);

				//printf("getting the time...\n");
				gettimeofday(&beginread, NULL);
				
				//At the beginning of sending mode
				if (len <= 25600) {	//0.25Mo
					INITIAL_CWND = id_lastfrag;
				} else if (len <= 512000) {	//0.5Mo
					INITIAL_CWND = 100;
				} else if (len <= 15360000){ // 0.5Mo<length<15Mo
					INITIAL_CWND = 250;
				} else if (len <= 61440000) { // 15Mo<length<60Mo
					INITIAL_CWND = 350;
				} else if (len <= 307200000) { // 15Mo<length<300Mo
					INITIAL_CWND = 450;	
				} else if (len <= 614400000) { // 300Mo<length<600Mo
					INITIAL_CWND = 550;		
				} else{ // 600Mo<length<1Go
					INITIAL_CWND = 650;		
				}
				//printf("initial: %d\n",INITIAL_CWND);
				cwnd = INITIAL_CWND; //sending first INITIAL_CWND
				
				//copy completely the file inside a very big buffer
				data = (char*) malloc(sizeof(char) * len);
				fread(data, 1, len, f_in);
				fclose(f_in);

				while (1) {

					//first display
					if (firstprint_SS == 0) {
						printf(ANSI_COLOR_YELLOW "Envoi du fichier en cours..."ANSI_COLOR_RESET"\n");
						firstprint_SS = -1;
					}

					//sending cwnd-1 fragments
					while (flightSize < cwnd) {

						//sending normally
						if (lastACK == 0 || lastACK >= id_frag) {
							id_frag++;
						}
						//resending the lost fragment
						else {
							id_frag = lastACK + 1;
							//printf(ANSI_COLOR_GREEN"SENT: %d" ANSI_COLOR_RESET"\n",id_frag);
						}

						//resetting the last ACK received as 0 because sending data mode...
						lastACK = 0;

						//getting the fragment to send
						read = seek(message, data, id_frag, id_lastfrag, len);

						//classic fragment of FRAGLEN-6 or last one
						if (read >= 1 && id_frag <= id_lastfrag) {
							//printf("seeking %d OK\n",id_frag);

							//preparing the fragment with id_frag number
							sprintf(res, "%0.6d", id_frag);	//the 6 first digits are for the id_frag
							memcpy(res + 6, message, FRAGLEN - 6);

							//sending fragments while the threshold isn't reached
							sent = send_message(s, res, read + 6,(struct sockaddr *) &si_other, slen);

							flightSize++;

							

						} else {
							//printf("EOF reached!\n");
							id_frag--;
							break;
						}
					}
					//reading the receiving buffer while there are the ACK
					do {
						recv = rcv_msg_timeout(s, buf,(struct sockaddr *) &si_other, &slen, 500); //adaptive reception
						currentACK = getACK(buf, 6);
						//printf("prec: %d  suiv: %d\n",lastACK,atoi(index(buf, 'K') + 1));

						if (lastACK < currentACK && currentACK > oldACK)
							lastACK = currentACK;

					} while (lastACK < id_lastfrag && recv > 0);

					//if it's ok
					if (lastACK >= id_frag) {
						cwnd += nb_ACK;
					}
					//if it's not ok
					else {
						cwnd = INITIAL_CWND;
					}
					
					//if there's no new ACK
					if (lastACK == 0)
						lastACK = oldACK;
					
					//getting the nb of new ACK
					nb_ACK = lastACK - oldACK;

					if (argc == 2) {
						printf("\n");
						printf("%d fragments sent\n", flightSize);
						printf("%d fragments ACK-ed\n", nb_ACK);
						printf("lastsent: %d\n", id_frag);
						printf("lastACK: %d\n", lastACK);
					}
					
					//ready to send
					flightSize = 0;
					oldACK = lastACK;

					//if EOF
					if (lastACK == id_lastfrag) {
						//printf(ANSI_COLOR_GREEN"BREAK!!!!!!" ANSI_COLOR_RESET"\n");
						break;
					}

					//end of while
				}
				
				
				//receiving the very last(s) ACK
				//resending the final fragments (many many times to make sure the client ends)
				i=0;
				do{
					recv = rcv_msg_timeout(s, buf,	(struct sockaddr *) &si_other, &slen, 10000);
					send_message(s, "FIN", 4, (struct sockaddr *) &si_other, slen);
					i++;
				}while(i<INITIAL_CWND && recv >0);
				
				
				gettimeofday(&end, NULL);
				delta = ((end.tv_sec - beginread.tv_sec) * 1000.0f+ (end.tv_usec - beginread.tv_usec) / 1000.0f)/ 1000.0f;
				
				//printf("\nvery last ACK: %d\n", lastACK);
				printf(ANSI_COLOR_BLUE"\nFichier de taille " ANSI_COLOR_RED" %f" ANSI_COLOR_BLUE" Ko envoyÃ©\n",(float) len / 1000);
				printf("Duree d'envoi: " ANSI_COLOR_RED"%fsec " ANSI_COLOR_BLUE"\n",delta);
				printf("Debit moyen: "ANSI_COLOR_RED "%f Ko/s" ANSI_COLOR_BLUE"\n",	len / (1024 * delta));
				printf("Envoi terminé!" ANSI_COLOR_RESET"\n\n");
				
				
				beginning = 2;
				
			}
			
		}
	
	}
	free(res);
	close(s);

}
