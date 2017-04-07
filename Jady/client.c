/*
   Simple udp client
http://www.binarytides.com/programming-udp-sockets-c-linux/
*/
#include "useful.h"
#include <time.h>
#include <sys/time.h>
//#define SORTIE "c.jpg" //where i'm stocking what i get

int main(void)
{
//	
    struct sockaddr_in si_other;
    int s;
    int i;
    int slen=sizeof(si_other);
    int compare = 1 ; // il ya une difference
    int first = 0;
    
    char *buf = (char*) malloc(sizeof(char) * FRAGLEN); //temporary buffer to receive ONE fragment
    char *message = (char*) malloc(sizeof(char) * BUFLEN); 
    char *buf_file=(char*)malloc(sizeof(char) * 1000000000); 
    
    FILE * f_out;
    int file_i=0;
    int id_frag = 0;
	int sent, recv=0;
	
	char *SORTIE = (char*) malloc(sizeof(char) * FRAGLEN);
	
	struct timeval end, start;
	float delta;

    if (connect_to_port (&s, PORT, &si_other) !=0){
		die("Premiere connexion échouée\n");
	}

    while(1)
    {
        if(compare ==1){ //processus d'acceptation de connexion	
            compare = string_compare(buf,strlen(buf)); //comparer le debut avec SYN-ACK
            if(compare==0){
                sent = send_message(s,"ACK",3,  (struct sockaddr *)&si_other, slen);
                int PORT2 = atoi(index(buf,'_')+1);
                
                if (connect_to_port (&s, PORT2, &si_other) <0){
					die("Connexion au serveur-fils échouée\n");
				}
            }
            else{//envoie du SYN
                sent = send_message(s,"SYN",3,  (struct sockaddr *)&si_other, slen);
                compare =1;
            }
            
            
			if (first == 0){ //attente de SYN-ACK_NUMERO_DE_PORT
				recv = receive_message(s, buf,NULL,0);
				first = -1;
			}
        }
        
        else{//une fois la connexion acceptée
            insert_a_message(&message);
            sent = send_message(s,message,strlen(message),  (struct sockaddr *)&si_other, slen);
            delete(buf,recv);
            recv = receive_message(s, buf,NULL,0);
            //printf("recu: %s\n",buf);
            
            if(strcmp("OK",buf)==0){
				SORTIE = "recv";
                printf("\nMode réception de fichier à mettre dans %s\n", SORTIE);
                gettimeofday(&start, NULL);
                
                while(1){
					delete(buf,sizeof(buf));
                    recv = receive_message(s, buf,NULL,0);
                    //printf("\nrecu: %d\n",recv);
                    id_frag ++;//useful for ACK
                    if(strcmp(buf,"end")==0){
                        break;
                    }
                    
                    //filling the buffer
                    memcpy(buf_file+file_i,buf,recv);
                    file_i += recv;
                    
                    
                    //sending ACK
                    delete(message,sent);
                    sprintf(message,"ACK_%d",id_frag);
                    //sleep(5); //TEST OF TIMEOUT
                    sent = send_message(s,message,20, (struct sockaddr *)&si_other, slen);
                    //printf("Envoyé: %d\n",sent);
                }
                
                gettimeofday(&end, NULL);
                delta = ((end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f) / 1000.0f;
                printf("Fichier complet reçu en %f sec\n", delta);
                if ((f_out = fopen(SORTIE,"wb"))==NULL){
                    die("Erreur: Impossible de lire le fichier de sortie (client)\n");
                }
   
                if ((i =(fwrite(buf_file,1,file_i,f_out))) == 0){
                    return(EXIT_FAILURE);
                }
                else{
                    printf("Nombre d'octets écrits: %d \n\n",i);
                }
                fclose(f_out);	
                
                
                i = 0;
                id_frag = 0;
                delete(buf_file,sizeof(buf_file));
                delete(buf,sizeof(buf));
                file_i = 0;
            }	

        }


    }

    close(s);
    return 0;
}
