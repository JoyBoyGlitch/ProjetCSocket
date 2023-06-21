#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

// Pour les threads
#include <pthread.h>
#include <semaphore.h>

#define CMD_QUIT "quit"
#define CMD_STOP "stop"
#define BUFFER_LEN 10
#define CLIENTS_NB 2
#define EMPTY_VALUE -1

struct clientInfo {
    char pseudo[BUFFER_LEN];
    int port;
    char ip[INET_ADDRSTRLEN];
    int socket;
    struct sockaddr_in *clientAdresse;
    unsigned int addrLen;
    int fdSocket;
} typedef clientInfo;

void getClientInfo(clientInfo *ci, struct sockaddr_in *ca);
int createSocketServer();
int manageClient(clientInfo *ci);
void sendClient(clientInfo *ci, char *msg);
void initClientTab(clientInfo ci[]);
clientInfo * getNextFreeClient(clientInfo ci[]);
void initClientInfo(clientInfo* ci);
void sig_handler(int sig); // Gestionnaire de signaux
void sendAllClients(clientInfo ci[], char *msg);
void sendToAllExcept(char *msg, clientInfo *ci);
int end = 0; // Indicateur de fin de programme (interrupt)
int fdsocket; //utilisé dans le main
clientInfo clientTab[CLIENTS_NB];
void* assyncWaitForClient(void* ci);
int main(void) {
    puts("Utilisez le signal SIGINT pour interrompre l'exécution !");
        // Enregistrement du gestionnaire de signaux        
        if(signal(SIGINT, sig_handler) == SIG_ERR || signal(SIGUSR1, sig_handler) == SIG_ERR){
                puts("Erreur à l'enregistrement du gestionnaire de signaux !");
        }
    fdsocket = createSocketServer();
    // Structure contenant l'adresse du client
    struct sockaddr_in clientAdresse;
    unsigned int addrLen = sizeof(clientAdresse);
   
    initClientTab(clientTab);
    int socket = 0;
    while ((socket = accept(fdsocket, (struct sockaddr *) &clientAdresse, &addrLen)) != -1) {
        if (end == 1) {
            break; //Sortir du while en cas d'interrupt
        }
        clientInfo *ci = getNextFreeClient(clientTab);
        // Gérer le cas où clientTab retourne NULL
        if(ci == NULL){
            send(socket,"On est complet mec \n",sizeof("On est complet mec \n"),MSG_DONTWAIT);
            continue;
        }
        ci->clientAdresse = &clientAdresse;
        ci->socket = socket;
        ci->fdSocket = fdsocket;

        pthread_t threadClient;
        if(pthread_create(&threadClient, NULL, assyncWaitForClient, ci) != 0){
            printf("Erreur à la creation du thread client");
        }
        
    }
    
    return EXIT_SUCCESS;
}

 
int createSocketServer(){
    int fdsocket;
    if ((fdsocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Echéc de la création: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(fdsocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) != 0) {
        printf("Echéc de paramètrage: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in adresse;
    adresse.sin_family = AF_INET;
    // Ecoute sur toutes les adresses (INADDR_ANY <=> 0.0.0.0)
    adresse.sin_addr.s_addr = INADDR_ANY;
    // Conversion du port en valeur réseaux (Host TO Network Short)
    adresse.sin_port = htons(1234);
    if (bind(fdsocket, (struct sockaddr *) &adresse, sizeof(adresse)) != 0) {
        printf("Echéc d'attachement: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (listen(fdsocket, 2) != 0) {
        printf("Echéc de la mise en écoute: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return fdsocket;
}
void getClientInfo(clientInfo *ci, struct sockaddr_in *ca){
    // Convertion de l'IP en texte
    inet_ntop(AF_INET, &(ca->sin_addr), ci->ip, sizeof(ci->ip));
    ci->port = ca->sin_port;
    printf("Connexion de %s:%i\n", ci->ip, ci->port);
}
 
int manageClient(clientInfo *ci){
    //signal(SIGINT, sig_handler); // Définition du gestionnaire de signaux pour SIGINT
    sendClient(ci, "Welcome on the coolest chat server ;)\n");
    char buffer[BUFFER_LEN+1]="";
    int len = 0;
    sendClient(ci, "Ton pseudo ? ");
    recv(ci->socket,ci->pseudo, BUFFER_LEN, SOCK_NONBLOCK);
    ci->pseudo[strlen(ci->pseudo)-2]='\0';
    int lenPrompt = snprintf(0,0,"%s : \t",ci->pseudo);
    char prompt[lenPrompt];
    //sprintf(prompt,"%s : \t",ci->pseudo);
    //printf("%s",prompt);
    //printf("%s",ci->pseudo);*

    int bool = 0;
    while(1){

        if(bool == 0){ //tout premier envoi
           
           bool = 1;
        }
      
        len = recv(ci->socket, buffer, BUFFER_LEN, SOCK_NONBLOCK);
        if (end == 1) {
            break;
        }

        // Si le buffer est de taille BUFFER_LEN et que le dernier caractère est retour a la ligne
        if(len == BUFFER_LEN && buffer[len-1] == '\n'){
            sendToAllExcept(ci->pseudo,ci);
            sendToAllExcept(" : ",ci);
            printf("%s",buffer);
            sendToAllExcept(strncat(buffer,"\0\n",2),ci);
            sendClient(ci, buffer);
            
            continue;
        }

        if(buffer[len-1] == '\n'){
        // fin du message, nettoyage du \n si besoin
            buffer[len-2] = '\0';
        }else {
            buffer[len] = '\0';
        }

        if(len == BUFFER_LEN){
            
            sendToAllExcept(ci->pseudo,ci);
            sendToAllExcept(" : ",ci);
            printf("%s","Moi : ")
            printf("%s",buffer);
            sendToAllExcept(strcat(buffer,"\0\n"),ci);
            sendToAllExcept("\n",ci);
    
            bool = 1;

            continue;
        }  
        if(strlen(buffer) == 0){
            // message vide
            
            continue;
        }
        // Vérification d'un commande
        if(strncmp(CMD_QUIT, buffer, strlen(CMD_QUIT)) == 0){
            sendClient(ci, "Adios\n");
            close(ci->socket);
            initClientInfo(ci);
            return 0;
        }
        if(strncmp(CMD_STOP, buffer, strlen(CMD_STOP)) == 0){
            sendClient(ci, "Server shuting down\n");
            close(ci->socket);
            initClientInfo(ci);
            return 1;
        }
        
        sendClient(ci, buffer);
        sendToAllExcept(buffer,ci);
        printf("%s",buffer);
        sendToAllExcept( "\n",ci);
        printf("\n");
    }
}
 
void sendClient(clientInfo *ci, char *msg){
    send(ci->socket, msg, strlen(msg),MSG_DONTWAIT);
}
void initClientTab(clientInfo ci[]){
    for(int i = 0; i<CLIENTS_NB;i++){
        initClientInfo(&ci[i]);
    }
}
clientInfo * getNextFreeClient(clientInfo ci[]){ //**ci aka *ci[] (la structure ci en tableau)
    for(int i = 0; i<CLIENTS_NB;i++){
       if (ci[i].port == EMPTY_VALUE){
            return &ci[i];
       }
    }
    return NULL;
}
void initClientInfo(clientInfo* ci){
    ci->pseudo[0]='\0';
    //strcpy(ci->pseudo,""); //le resultats est le meme que la 155 
    ci->port = EMPTY_VALUE;
    ci->ip[0] ='\0';
    ci->socket = EMPTY_VALUE;
    ci->fdSocket = EMPTY_VALUE;
    ci->addrLen = sizeof(ci->clientAdresse);
}

void* assyncWaitForClient(void* arg){
        clientInfo* ci = (clientInfo*) arg;
        getClientInfo(ci, ci->clientAdresse);
        if(manageClient(ci) != 0){
            close(ci->fdSocket);
            return NULL;
        }
}
// Gestionnaire de signaux
void sig_handler(int sig) {
    printf("\nSIGINT attrapé, on stop le programme %i\n", getpid());
    for (int i=0; i<CLIENTS_NB; i++){
        close (clientTab[i].socket);
    }
    close(fdsocket);
    end = 1;
}

void sendToAll(char *msg){
    for(int i = 0; i<CLIENTS_NB;i++){
        if(clientTab[i].port != EMPTY_VALUE){
            sendClient(&clientTab[i], msg);
        }
    }
}

void sendToAllExcept(char *msg, clientInfo *ci){
    for(int i = 0; i<CLIENTS_NB;i++){
        if(clientTab[i].port != EMPTY_VALUE && clientTab[i].port != ci->port){
            sendClient(&clientTab[i], msg);
        }
    }
}