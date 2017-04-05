/*
   Simple udp server
http://www.binarytides.com/programming-udp-sockets-c-linux/
*/

#include "useful.h"


int main(void)
{
//
    struct sockaddr_in si_me;//server struct addr
    struct sockaddr_in si_other;//client struct addr
    int s; //socket 
    int slen = sizeof(si_other);
    char *buf = (char*) malloc(sizeof(char) * BUFLEN); //recu
    char *message = (char*) malloc(sizeof(char)* FRAGLEN); //envoyé
    int i; //filling the temporary little buffer
    int c; //useful to get the char in the file
    int PORT2 = PORT;
    int pid_fils = -1;
    int compare = 1 ; // il y a une difference
    FILE * f_in;
    int counter_fragment;
    int read = FRAGLEN,sent=0,recv=0;
    char * ENTREE = (char*) malloc(sizeof(char)* FRAGLEN); //what server is sending

    int rwnd = 1; //receiver window
    int cwnd = 10; //congestion window
    int flightSize = 0; //data that has been sent, but not yet acknowledged

    int j = 0;
//
    if (create_server(&s, &si_me, PORT) != 0){
        die("Error on creating first server\n");
    }
    printf("I am %d \n",getpid());
    //we clean up the buffer  
    delete(buf,sizeof(buf));

    //keep listening for data
    while(1){
        delete(buf,strlen(buf));
        receive_message(s, buf,(struct sockaddr *) &si_other, &slen);
        //printf("Received by %d : %s\n",getpid(),buf);
        if(compare !=0){//acceptation de connexion
            compare = strcmp("SYN",buf);
            if(compare==0){
                PORT2 ++;

                sprintf(message, "SYN-ACK_%d",PORT2);

                send_message(s,message,15,  (struct sockaddr *)&si_other, slen);                
                pid_fils = fork();

                if(pid_fils==0){
                    printf("FILS %d de port %d\n",getpid(),PORT2);

                    //On recrée un nouveau socket	
                    if (create_server(&s, &si_me, PORT2) != 0){
                        die("Error on creating a son server\n");
                    }
                }else{
                    printf("PERE %d de port %d\n",getpid(),PORT);
                    compare = 1;
                    continue;					
                }                
            }
            else{
                compare = 1;				
            }
        }
        else{ //une fois la connexion acceptée
            ENTREE = "../explorers/src/a";
            if (access(ENTREE,0) == 0){
                send_message(s,"OK",2,(struct sockaddr *)&si_other, slen);

                printf("\nEnvoi du fichier %s par %d\n",ENTREE,getpid());

                if ((f_in = fopen(ENTREE,"rb")) == NULL){//b because nbinary
                    die("Probleme ouverture fichier\n");
                }

                while (1){
                    if(flightSize<cwnd){
                        read = fread(message,1,FRAGLEN,f_in);
                        //printf("\nRead: %d\n",read);
                        if (read > 1){
                            //usleep(1000000);
                            sent = send_message(s,message,read,(struct sockaddr *)&si_other, slen);
                            //printf("Sent: %d\n",sent);
                            flightSize ++ ;
                        }
                        else{
                            break;
                        }

                    }
                    else{
                        for(j=0;j<flightSize;j++){
                            recv = receive_message(s, buf,(struct sockaddr *) &si_other, &slen);
                            //printf("Recv: %s de taille %d\n",buf, recv);
                        }
                        flightSize = 0;
                    }
                }
                fseek(f_in, 0L, SEEK_END);
                printf("Fichier de taille %d transmis avec succès\n\n",ftell(f_in));
                fclose(f_in);
                send_message(s,"end",3,  (struct sockaddr *)&si_other, slen);
            }
            else{
                printf("Le fichier %s n'existe pas.\n", ENTREE);
                send_message(s,"NOK",3,(struct sockaddr *)&si_other, slen);
            }
        }
    }
    close(s);
    return 0;
}
