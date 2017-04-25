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
	int i,j; //filling the temporary little buffer
	int c; //useful to get the char in the file
	int pid_fils = -1;
	int compare = 1; // il y a une difference
	FILE * f_in;
	int id_frag = 0, lastACK = 0, id_lastfrag =  0, oldACK=0, nb_ACK=0, firstprint_SS=0,currentACK=0;
	int read = FRAGLEN, sent = 0, recv = 0, len =0;
	char * ENTREE = (char*) malloc(sizeof(char) * FRAGLEN); //what server is sending
	char* data; 
	
	struct timeval end, start, beginread;
	float RTT_msec = 0, delta =0;
	void *res = (void*) malloc(sizeof(char) * FRAGLEN + 7); //what's finally sent (id_frag + data)
	
	int rwnd = 0; //receiver window
	int cwnd = 0; //congestion window
	int flightSize = 0; //data that has been sent, but not yet acknowledged
	int ssthresh = 2147483647;
	int PORT=0, PORT2=0;
	
	//useful for graphs
	int *graph = NULL;
	int loopCounter = 0;
	int *graphACK = NULL;
	int loopCounterACK = 0;
	
	if (argc < 2){
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
		
		receive_message(s, buf, (struct sockaddr *) &si_other, &slen);
		printf("Received by %d : %s\n", getpid(), buf);


		//accepting connexion process... 
		if (compare != 0) { 
			compare = strcmp("SYN", buf);
			if (compare == 0) {
				PORT2++;
				pid_fils = fork();
					
				if (pid_fils == 0) {
					//printf("FILS %d de port %d\n", getpid(), PORT2);

					//Creating a new socket
					if (create_server(&s, &si_me, PORT2) != 0){
						die("Echec de la creation du serveur-fils.\n");
					}
				
				} else {
				
					//printf("PERE %d de port %d\n", getpid(), PORT);
					sprintf(message, "SYN-ACK%d", PORT2);
					sleep(1);				
					send_message(s, message, 15, (struct sockaddr *) &si_other,slen);
					compare = 1;
					continue;
				
				}
			} else {
				compare = 1;
			}
			
			
		//connexion accepted (child process)
		} else { 
			ENTREE = buf;			
			
			if (access(ENTREE, 0) == 0) {
				printf(ANSI_COLOR_BLUE"\nEnvoi du fichier %s par %d...\n", ENTREE, getpid());

				if ((f_in = fopen(ENTREE, "rb")) == NULL) {	//b because binary
					die("Probleme ouverture fichier\n");
				}
				
				fseek(f_in,0,SEEK_END);
				len = ftell(f_in);
				id_lastfrag = (int) (len/(FRAGLEN-6)) +1;
				printf("Taille: %d\n",len);
				printf("Nb de fragments: %d" ANSI_COLOR_RESET"\n\n",id_lastfrag);
				fseek(f_in,0,SEEK_SET);
				
				//printf("getting the time...\n");
				gettimeofday(&beginread, NULL);
				//printf("getting the time OK\n");
				
				
				//At the beginning of sending mode
				cwnd = INITIAL_CWND; //sending first INITIAL_CWND
				RTT_msec = INITIAL_RTT_MSEC;//waits INITIAL_RTT_MSEC for timeout
				
				//printf("Init graphs...\n");				
				graph = (int*) malloc(sizeof(int) * 100000*id_lastfrag);
				graphACK = (int*) malloc(sizeof(int) * 100000*id_lastfrag);
				//printf("Init OK!\n");
				
				//copy the file inside a very big buffer
				data = (char*) malloc(sizeof(char) * len);
				fread(data,1,len,f_in);
				fclose(f_in);
				
				
				while (1) {
						
						//first display
						if (firstprint_SS ==0){
							printf(ANSI_COLOR_YELLOW "Sending data..."ANSI_COLOR_RESET"\n");
							firstprint_SS = -1;
						}
						
						//sending cwnd-1 fragments
						while (flightSize < cwnd){
							//printf("\nwhile begins\n");
							
							//sending normally
							if (lastACK == 0 || lastACK >= id_frag){
								//printf("normal\n");
								id_frag++;
							}
							//resending the lost fragment
							else{
								//printf("resend\n");
								id_frag = lastACK+1;
								//printf(ANSI_COLOR_GREEN"SENT: %d" ANSI_COLOR_RESET"\n",id_frag);
							}
							
							//resetting the last ACK received as 0 because sending data mode...
							lastACK = 0;
							
							//printf("seeking %d\n...",id_frag);
							//getting the fragment to send
							read = seek(message,data,id_frag,id_lastfrag,len);
							
							//classic fragment of 1018 or last one
							if (read >= 1 && id_frag<=id_lastfrag) {
								//printf("seeking %d OK\n",id_frag);
							
								//preparing the fragment with id_frag number
								sprintf(res, "%0.6d", id_frag);//the 6 first digits are for the id_frag
								memcpy(res+6,message,FRAGLEN-6);
								
								//sending fragments while the threshold isn't reached
								sent = send_message(s, res, read+6,(struct sockaddr *) &si_other, slen);
								//printf("sent: id_%d of %d\n",id_frag,sent);
								flightSize++;
								
								//useful for graph
								graph[loopCounter] = id_frag;
								//printf("graph[%d] = %d\n",loopCounter,graph[loopCounter]);
								loopCounter++;
								
								//useful for RTT
								//printf("getting time...\n");
								gettimeofday(&start, NULL);
								//printf("getting time ok!\n");
							}
							else {
								//printf("EOF reached!\n");
								id_frag--;
								flightSize =cwnd;
								break;
							}
							
							//printf("end of while\n");
						}
						
							
						//reading the receiving buffer while there are the ACK
						do{
							recv = rcv_msg_timeout(s, buf,(struct sockaddr *) &si_other, &slen,RTT_msec);
							currentACK = atoi(index(buf, 'K') + 1);
							//printf("prec: %d  suiv: %d\n",lastACK,atoi(index(buf, 'K') + 1));
							
							if (lastACK < currentACK && currentACK > oldACK)
								lastACK = currentACK;
								
							graphACK[loopCounterACK] = currentACK;
							//printf("graphACK[%d] = %d\n",loopCounterACK,graph[loopCounterACK]);
							loopCounterACK++;
								
											
						}while(lastACK < id_lastfrag && recv >0);
						
						gettimeofday(&end, NULL);
						
						
						//if there's no new ACK
						if (lastACK ==0)
							lastACK= oldACK;
						
						//getting the nb of new ACK
						nb_ACK = lastACK-oldACK;
						
						if (argc == 2){
							printf("\n");
							printf("%d fragments sent\n",flightSize);
							printf("%d fragments ACK-ed\n",nb_ACK);
							printf("lastsent: %d\n",id_frag);
							printf("lastACK: %d\n",lastACK);
							
						}
						
						
						//if it's ok
						if (lastACK >= id_frag)
							cwnd+=nb_ACK;
						//if it's not ok
						else{
							cwnd=INITIAL_CWND;
						}
						
						
						//if there are new ACK received
						if ((nb_ACK >0 || lastACK ==0) && (delta=((((end.tv_sec - start.tv_sec) * 1000.0f+ (end.tv_usec - start.tv_usec) / 1000.0f)/ 1000.0f))*1000/(nb_ACK)) < 500 && delta>0){
							RTT_msec = delta;
						}
						
						
						//RTT display (decrease the throughput x2 !!!!)
						//printf(ANSI_COLOR_RED "RTT: %lf msec" ANSI_COLOR_RESET "\n",RTT_msec );
						
						//ready to send
						flightSize = 0;		
						oldACK=lastACK;
						
						//printf(ANSI_COLOR_RED "Sending again..." ANSI_COLOR_RESET "\n");
						
			
						
							
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
				printf(ANSI_COLOR_BLUE"\nFichier de taille " ANSI_COLOR_RED" %f" ANSI_COLOR_BLUE" Ko envoyé\n",(float)len/1000);			
				printf("Dernier RTT: " ANSI_COLOR_RED"%lf" ANSI_COLOR_BLUE" msec\n",RTT_msec );
				printf("Durée d'envoi: " ANSI_COLOR_RED"%fsec " ANSI_COLOR_BLUE"\n",delta);		
				printf("Débit moyen: "ANSI_COLOR_RED "%f Ko/s" ANSI_COLOR_BLUE"\n",len/(1024*delta));
				
				//receiving the very last(s) ACK
				do{
					recv = rcv_msg_timeout(s, buf,(struct sockaddr *) &si_other, &slen,1);
					graphACK[loopCounterACK] = lastACK;
					//printf("graphACK[%d] = %d\n",loopCounterACK,graph[loopCounterACK]);
					loopCounterACK++;
								
				}while(recv >0);
						
				delete(buf, strlen(buf));
				
				//displayGraph(graph,sizeof(int) * 100000 *id_lastfrag);
				//displayGraph(graphACK,sizeof(int) *100000* id_lastfrag);
				
				outGraph(f_in, graph,sizeof(int) * 100000 *id_lastfrag,"graph.txt");
				fclose(f_in);
				outGraph(f_in, graphACK,sizeof(int) *100000* id_lastfrag,"graphACK.txt");
				fclose(f_in);
				
				printf("Terminé!" ANSI_COLOR_RESET"\n\n");
				
				
			} else {
				if (strcmp(" ", ENTREE) != 0 && strcmp("", ENTREE) != 0) {
					send_message(s, "FIN", 4, (struct sockaddr *) &si_other,slen);
					printf(ANSI_COLOR_RED"Le fichier '%s' n'existe pas." ANSI_COLOR_RESET"\n", ENTREE);
					delete(buf, strlen(buf));

				} else {
					printf("Pourquoi envoyer du vide?\n");
				}
			}
			
			//cleaning up every malloc for graphs
			free(graph);
			free(graphACK);
			free(res);
			
			//killing the socket and the process reserved to the current gone client
			close(s);
			kill(getpid(),SIGINT);
			
		}
	}
	close(s);

}

