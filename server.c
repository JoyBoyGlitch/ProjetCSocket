#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>

// Pour les threads
#include <pthread.h>
#include <semaphore.h>
 
#define CMD_QUIT "quit"
#define CMD_STOP "stop"
#define BUFFER_LEN 10
#define CLIENTS_NB 2
#define EMPTY_VALUE -1

struct clientInfo{
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
void* assyncWaitForClient(void* ci); 
int main(void) {
    int fdsocket = createSocketServer();
    // Structure contenant l'adresse du client
    struct sockaddr_in clientAdresse;
    unsigned int addrLen = sizeof(clientAdresse);
    clientInfo clientTab[CLIENTS_NB];
    initClientTab(clientTab);
    int socket = 0;
    while ((socket = accept(fdsocket, (struct sockaddr *) &clientAdresse, &addrLen)) != -1) {
        clientInfo *ci = getNextFreeClient(clientTab);
        //gerer le cas ou clientTab return NULL
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
    sendClient(ci, "Welcome on the coolest chat server ;)\n");
    char buffer[BUFFER_LEN+1]="";
    int len = 0;
    sendClient(ci, "Blaze ? ");
    recv(ci->socket,ci->pseudo, BUFFER_LEN, SOCK_NONBLOCK);
    ci->pseudo[strlen(ci->pseudo)-2]='\0';
    int lenPrompt = snprintf(0,0,"%s : \t",ci->pseudo);
    char prompt[lenPrompt];
    sprintf(prompt,"%s : \t",ci->pseudo);
    //printf("%s",prompt);
    //printf("%s",ci->pseudo);
    while(1){
        sendClient(ci, prompt);
        len = recv(ci->socket, buffer, BUFFER_LEN, SOCK_NONBLOCK);
        if(len == BUFFER_LEN && buffer[len-1] == '\n'){
            printf("%s",buffer);
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
            printf("%s",buffer);
            sendClient(ci, buffer);
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
        printf("%s",buffer);
        sendClient(ci, "\n");
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
