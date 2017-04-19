/*
 Simple udp server
 http://www.binarytides.com/programming-udp-sockets-c-linux/
 */

#include "useful.h"

int main(int argc, char *argv[]) {
	struct sockaddr_in si_me; //server struct addr
	struct sockaddr_in si_other; //client struct addr
	int s; //socket
	int slen = sizeof(si_other);
	char *buf = (char*) malloc(sizeof(char) * BUFLEN); //recu
	char *message = (char*) malloc(sizeof(char) * FRAGLEN); //envoyé
	int i; //filling the temporary little buffer
	int c; //useful to get the char in the file
	int pid_fils = -1;
	int compare = 1; // il y a une difference
	FILE * f_in;
	int id_frag = 0, lastACK = 0, id_lastfrag =  0, oldACK=0, nb_ACK=0, firstprint_SS=0, firstprint_CA=0, currentACK= 0, ACKcounter=0,previouslySent=0;;
	int read = FRAGLEN, sent = 0, recv = 0, len =0;
	char * ENTREE = (char*) malloc(sizeof(char) * FRAGLEN); //what server is sending
	int INITIAL_CWND = 0;

	struct timeval end, start, beginread;
	float RTT_msec = 0, delta =0;
	char *res = (char*) malloc(sizeof(char) * FRAGLEN + 7); //what's finally sent (id_frag + data)

	int rwnd = 0; //receiver window
	int cwnd = 0; //congestion window
	int flightSize = 0; //data that has been sent, but not yet acknowledged
	int ssthresh = 2147483647;
	int PORT=0, PORT2=0;

	if (argc != 2){
		printf(ANSI_COLOR_RED "./server1-explorers server_port" ANSI_COLOR_RESET"\n");
		exit(1);
	}else{
		PORT = atoi(argv[1]);
		if (PORT> 1023 && PORT<9999){
			PORT2 = PORT;
			printf("port: %d\n",PORT);
		}else{
			printf(ANSI_COLOR_RED"Error: 1023 <server_port < 9999! " ANSI_COLOR_RESET"\n");
			exit(1);
		}
	}

	//
	if (create_server(&s, &si_me, PORT) != 0) {
		die("Error on creating first server\n");
	}
	printf("I am %d \n", getpid());
	//we clean up the buffer
	delete(buf, sizeof(buf));

	//keep listening for data
	while (1) {
		//delete(buf, strlen(buf));
		receive_message(s, buf, (struct sockaddr *) &si_other, &slen);
		printf("Received by %d : %s\n", getpid(), buf);
		if (compare != 0) { //acceptation de connexion
			compare = strcmp("SYN", buf);
			if (compare == 0) {
				PORT2++;

				sprintf(message, "SYN-ACK%d", PORT2);

				send_message(s, message, 15, (struct sockaddr *) &si_other,
						slen);
				pid_fils = fork();

				if (pid_fils == 0) {
					printf("FILS %d de port %d\n", getpid(), PORT2);

					//On recrée un nouveau socket
					if (create_server(&s, &si_me, PORT2) != 0) {
						die("Error on creating a son server\n");
					}
				} else {
					printf("PERE %d de port %d\n", getpid(), PORT);
					compare = 1;
					continue;
				}
			} else {
				compare = 1;
			}
		} else { //une fois la connexion acceptée
			ENTREE = buf;
			
			if (access(ENTREE, 0) == 0) {
				printf(ANSI_COLOR_BLUE"\nEnvoi du fichier %s par %d...\n", ENTREE, getpid());

				if ((f_in = fopen(ENTREE, "rb")) == NULL) {	//b because nbinary
					die("Probleme ouverture fichier\n");
				}
				
				fseek(f_in,0,SEEK_END);
				len = ftell(f_in);
				id_lastfrag = (int) (len/(FRAGLEN-6)) +1;
				printf("Taille: %d\n",len);
				printf("Nb de fragments: %d" ANSI_COLOR_RESET"\n\n",id_lastfrag);
				fseek(f_in,0,SEEK_SET);
				
				gettimeofday(&beginread, NULL);
				
				//At the beginning of slowstart
				INITIAL_CWND=1000;
				cwnd = INITIAL_CWND; //sending only INITIAL_CWND fragment
				RTT_msec = INITIAL_RTT_MSEC;//waits INITIAL_RTT_MSEC for timeout
				while (1) {
					
					//slowstart
					//if (ssthresh > cwnd){
						//first display of slowstart
						if (firstprint_SS ==0){
							printf(ANSI_COLOR_YELLOW "ssthresh %d > cwnd %d "ANSI_COLOR_RESET"\n",ssthresh,cwnd);
							printf(ANSI_COLOR_YELLOW "Slowstart mode..."ANSI_COLOR_RESET"\n");
							firstprint_SS = -1;
							firstprint_CA = 0;
						}
						
						//sending cwnd fragments
						while (flightSize < cwnd){
							//sending normally
							if (lastACK == 0 || lastACK == id_frag){
								id_frag++;
							}
							//resending the lost fragment
							else{
								id_frag = lastACK+1;
								//printf(ANSI_COLOR_GREEN"SENT: %d" ANSI_COLOR_RESET"\n",id_frag);
							}
							
							//resetting the last ACK received as 0 because sending data mode...
							lastACK = 0;
							
							fseek(f_in,(id_frag-1)*(FRAGLEN-6),SEEK_SET);
							//printf("\nid_frag: %d\n",id_frag);
							//printf("curseur: %d\n",ftell(f_in));
							/*if ((ftell(f_in)%(FRAGLEN-6))>0){
								printf(ANSI_COLOR_RED"Probleme de curseur! %dè fragment au %d" ANSI_COLOR_RESET"\n",id_frag,ftell(f_in));
								exit(1);
							}*/
							
							read = fread(message, 1, FRAGLEN-6, f_in);
							//printf("read: %d\n",read);
							
							//classic fragment of 1018 or last one
							if (read >= 1) {
								
								//preparing the fragment with id_frag number
								sprintf(res, "%0.6d%s\n", id_frag, message);//the 6 first digits are for the id_frag
								
								//sending fragments while the threshold isn't reached
								sent = send_message(s, res, read+6,(struct sockaddr *) &si_other, slen);
								//printf("sent: id_%d\n",id_frag);
								
								//useful for RTT
								gettimeofday(&start, NULL);
								
								
								flightSize++;
								
							}
							else{
								//printf("EOF reached\n");
								cwnd = (int)(flightSize/2)+1;
							}
							
						}
						
						if (flightSize>= cwnd){
							
							//reading the receiving buffer while there are the ACK
							do{
								
								
								recv = rcv_msg_timeout(s, buf,(struct sockaddr *) &si_other, &slen,RTT_msec);
								currentACK = atoi(index(buf, 'K') + 1);
								//printf("prec: %d  suiv: %d\n",lastACK,atoi(index(buf, 'K') + 1));
								
								if (lastACK < currentACK && currentACK > oldACK)
									lastACK = currentACK;
												
							}while(lastACK < id_lastfrag && recv >0);
							
							gettimeofday(&end, NULL);
							
							
							//if there's no new ACK
							if (lastACK ==0)
								lastACK= oldACK;
							
							//getting the nb of new ACK
							nb_ACK = lastACK-oldACK;
							
							/*printf("\n\n%d fragments sent\n",flightSize);
							printf("%d fragments ACK-ed\n",nb_ACK);
							printf("lastsent: %d\n",id_frag);
							printf("lastACK: %d\n",lastACK);
							*/
							
							//if it's ok
							if (lastACK >= id_frag)
								cwnd+=lastACK-oldACK;
							//if it's not ok
							else{
								ssthresh = (int) (flightSize/2);
								cwnd = INITIAL_CWND;
							}
							
							
							//if there are new ACK received
							if ((nb_ACK >0 || lastACK ==0) && (delta=((((end.tv_sec - start.tv_sec) * 1000.0f+ (end.tv_usec - start.tv_usec) / 1000.0f)/ 1000.0f))*1000/(nb_ACK)) < 500 && delta>0){
								RTT_msec = delta;
							}
							
							
							//RTT display
							//printf(ANSI_COLOR_RED "RTT: %lf msec" ANSI_COLOR_RESET "\n",RTT_msec );
							
							//ready to send
							flightSize = 0;		
							oldACK=lastACK;
							
							
						}
					/*}
				
				
				
				
				
					
					//congestion avoidance
					else{
						//first display of congestion avoidance
						if (firstprint_CA ==0){
							printf(ANSI_COLOR_YELLOW "ssthresh %d < cwnd %d " ANSI_COLOR_RESET"\n",ssthresh,cwnd);
							printf(ANSI_COLOR_YELLOW "Congestion avoidance..." ANSI_COLOR_RESET "\n");
							firstprint_CA = -1;
							firstprint_SS = 0;
						}
						while (flightSize < cwnd){
							
							
							//sending normally
							if (lastACK == 0 || lastACK == id_frag){
								id_frag++;
							}else{
								id_frag = lastACK+1;
							}
							
							//resetting the last ACK received as 0 because sending data...
							lastACK = 0;
							
							fseek(f_in,(id_frag-1)*(FRAGLEN-6),SEEK_SET);
							//printf("id_frag: %d\n",id_frag);
							//printf("curseur: %d\n",ftell(f_in));
							read = fread(message, 1, FRAGLEN-6, f_in);
							//printf("read: %d\n",read);
							
							//classic fragment of 1018 or last one
							if (read >= 1) {
								
								//preparing the fragment with id_frag number
								sprintf(res, "%0.6d%s\n", id_frag, message);//the 6 first digits are for the id_frag
								
								//sending fragments while the threshold isn't reached
								sent = send_message(s, res, read+6,(struct sockaddr *) &si_other, slen);
								//printf("sent: id_%d\n",id_frag);
								flightSize++;
								
							}
							else{
								//printf("EOF reached\n");
								flightSize = cwnd +1;
							}
							
						}
						
						if (flightSize>= cwnd){
							//reading the buffer while there are the ACK
							do{
								
								recv = rcv_msg_timeout(s, buf,(struct sockaddr *) &si_other, &slen,500);
								currentACK = atoi(index(buf, 'K') + 1);
								//printf("prec: %d  suiv: %d\n",lastACK,atoi(index(buf, 'K') + 1));
									
								//resending the missing fragment (fast retransmit)
								if (lastACK == currentACK ){
									ACKcounter++;
																		
									if (ACKcounter == MAX_ACK_DUPLICATED && currentACK!=previouslySent){
										//printf(ANSI_COLOR_GREEN"ACK %d dupliqué %d fois!"ANSI_COLOR_RESET"\n", lastACK,ACKcounter);
										//printf(ANSI_COLOR_GREEN"Resending %d..." ANSI_COLOR_RESET"\n", lastACK+1);
										
										
										//resending process
										id_frag = lastACK+1;
										fseek(f_in,(id_frag-1)*(FRAGLEN-6),SEEK_SET);
										read = fread(message, 1, FRAGLEN-6, f_in);
										if (read >= 1) {
											//preparing the fragment with id_frag number
											sprintf(res, "%0.6d%s\n", id_frag, message);//the 6 first digits are for the id_frag
											
											//sending fragments while the threshold isn't reached
											sent = send_message(s, res, read+6,(struct sockaddr *) &si_other, slen);
											//printf("sent: id_%d\n",id_frag);
											flightSize++;
											
										}
										else{
											//printf("EOF reached\n");
										}
										
										//printf(ANSI_COLOR_GREEN"Resending %d done!" ANSI_COLOR_RESET"\n",id_frag);
										previouslySent = id_frag;
										ACKcounter=0;
									}
								}
								
								if (lastACK < currentACK && currentACK > oldACK)
									lastACK = currentACK;
											
							}while(lastACK < id_lastfrag && recv >0);
							gettimeofday(&end, NULL);
							
							
							
							//if there's no new ACK
							if (lastACK ==0){
								printf("Rien d'acquitté\n");
								lastACK= oldACK;
							}
							
							//getting the nb of new ACK
							nb_ACK = lastACK-oldACK;
							
							//printf("\n\n%d fragments sent\n",flightSize);
							//printf("%d fragments ACK-ed\n",nb_ACK);
							//printf("lastsent: %d\n",id_frag);
							//printf("lastACK: %d\n",lastACK);
							
							
							//if it's ok
							if (lastACK >= id_frag)
								cwnd++;
							//if it's not ok
							else{
								ssthresh = (int) (flightSize/2);
								cwnd =ssthresh;
							}
							
							
							//if there are new ACK received
							if ((nb_ACK >0 || lastACK ==0) && (j=((((end.tv_sec - start.tv_sec) * 1000.0f+ (end.tv_usec - start.tv_usec) / 1000.0f)/ 1000.0f))*1000/(nb_ACK)) < 500){
								RTT_msec = ((((end.tv_sec - start.tv_sec) * 1000.0f+ (end.tv_usec - start.tv_usec) / 1000.0f)/ 1000.0f))*1000/(nb_ACK);
								//RTT display
								printf(ANSI_COLOR_RED     "RTT: %lf msec"     ANSI_COLOR_RESET "\n",RTT_msec );
							}
							
							//if there are no new ACK received (TIMEOUT)
							else{
								RTT_msec = INITIAL_RTT_MSEC/10;
								//printf(ANSI_COLOR_RED     "RTT reset (TIMEOUT)"     ANSI_COLOR_RESET "\n\n");
							}
							
							
							//ready to send
							flightSize = 0;
							oldACK=lastACK;
							
							
						}
						
					}*/
					
						
							
					//if EOF
					if (lastACK == id_lastfrag){
						//printf(ANSI_COLOR_GREEN"BREAK!!!!!!" ANSI_COLOR_RESET"\n");
						break;
					}
					
						
						
						
				//end of while		
				}
				
				//sending the final fragment with END
				send_message(s, "FIN",4, (struct sockaddr *) &si_other, slen);
				delta = ((end.tv_sec - beginread.tv_sec) * 1000.0f+ (end.tv_usec - beginread.tv_usec) / 1000.0f)/ 1000.0f;
						
				//printf("\nvery last ACK: %d\n", lastACK);
				printf(ANSI_COLOR_BLUE"\nFichier de taille %f Ko envoyé\n",(float)len/1000);			
				printf("Durée d'envoi: %fsec\n",delta);		
				printf("Débit moyen: %f Ko/s\n",len/(1024*delta));
				printf("Terminé!" ANSI_COLOR_RESET"\n\n");
				
				//receiving the very last(s) ACK
				do{
					recv = rcv_msg_timeout(s, buf,(struct sockaddr *) &si_other, &slen,RTT_msec);
				}while(recv >0);
						
						
				fclose(f_in);
				delete(buf, strlen(buf));
				
				
			} else {
				if (strcmp(" ", ENTREE) != 0 && strcmp("", ENTREE) != 0) {
					send_message(s, "FIN", 4, (struct sockaddr *) &si_other,slen);
					printf("Le fichier '%s' n'existe pas.\n", ENTREE);
					delete(buf, strlen(buf));

				} else {
					printf("Pourquoi envoyer du vide?\n");
				}
			}
			
			
			kill(getpid(),SIGINT);
		}
	}
	close(s);

}

