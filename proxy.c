#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "./simpleSocketAPI.h"

#define SERVADDR "127.0.0.1"        // Définition de l'adresse IP d'écoute
#define SERVPORT "0"                // Définition du port d'écoute, si 0 port choisi dynamiquement
#define LISTENLEN 1                 // Taille de la file des demandes de connexion
#define MAXBUFFERLEN 1024           // Taille du tampon pour les échanges de données
#define MAXHOSTLEN 64               // Taille d'un nom de machine
#define MAXPORTLEN 64               // Taille d'un numéro de port

int main() {
    int ecode;                       // Code retour des fonctions
    char serverAddr[MAXHOSTLEN];     // Adresse du serveur
    char serverPort[MAXPORTLEN];     // Port du server
    int descSockRDV;                 // Descripteur de socket de rendez-vous
    int descSockCOM;                 // Descripteur de socket de communication
    struct addrinfo hints;           // Contrôle la fonction getaddrinfo
    struct addrinfo *res;            // Contient le résultat de la fonction getaddrinfo
    struct sockaddr_storage myinfo;  // Informations sur la connexion de RDV
    struct sockaddr_storage from;    // Informations sur le client connecté
    socklen_t len;                   // Variable utilisée pour stocker les
                                     // longueurs des structures de socket
    char buffer[MAXBUFFERLEN];       // Tampon de communication entre le client et le serveur

    // Initialisation de la socket de RDV IPv4/TCP
    descSockRDV = socket(AF_INET, SOCK_STREAM, 0);
    if (descSockRDV == -1) {
        perror("Erreur création socket RDV\n");
        exit(2);
    }

    // Publication de la socket au niveau du système
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;      // mode serveur, nous allons utiliser la fonction bind
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_family = AF_INET;        // seules les adresses IPv4 seront présentées

    ecode = getaddrinfo(SERVADDR, SERVPORT, &hints, &res);
    if (ecode) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ecode));
        exit(1);
    }

    ecode = bind(descSockRDV, res->ai_addr, res->ai_addrlen);
    if (ecode == -1) {
        perror("Erreur liaison de la socket de RDV");
        exit(3);
    }
    freeaddrinfo(res);

    len = sizeof(struct sockaddr_storage);
    ecode = getsockname(descSockRDV, (struct sockaddr *) &myinfo, &len);
    if (ecode == -1) {
        perror("SERVEUR: getsockname");
        exit(4);
    }
    ecode = getnameinfo((struct sockaddr*)&myinfo, sizeof(myinfo), serverAddr, MAXHOSTLEN,
                        serverPort, MAXPORTLEN, NI_NUMERICHOST | NI_NUMERICSERV);
    if (ecode != 0) {
        fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(ecode));
        exit(4);
    }
    printf("L'adresse d'ecoute est: %s\n", serverAddr);
    printf("Le port d'ecoute est: %s\n", serverPort);

    printf("Je suis en attente d'une connexion, veuillez ouvrir un deuxième terminal \n");
    printf("et vous connectez avec ftp -d %s %s\n", serverAddr, serverPort);

    ecode = listen(descSockRDV, LISTENLEN);
    if (ecode == -1) {
        perror("Erreur initialisation buffer d'écoute");
        exit(5);
    }

    len = sizeof(struct sockaddr_storage);
    descSockCOM = accept(descSockRDV, (struct sockaddr *) &from, &len);
    if (descSockCOM == -1) {
        perror("Erreur accept\n");
        exit(6);
    }

    // Connexion acceptée
    printf("Connexion acceptée avec succès depuis un client.\n");

    char clientAddr[MAXHOSTLEN], clientPort[MAXPORTLEN];
    ecode = getnameinfo((struct sockaddr*)&from, len, clientAddr, MAXHOSTLEN, clientPort, MAXPORTLEN,
                        NI_NUMERICHOST | NI_NUMERICSERV);
    if (ecode == 0) {
        printf("Client connecté : %s:%s\n", clientAddr, clientPort);
    } else {
        printf("Impossible de récupérer les informations sur le client.\n");
    }

    /*****
     * Testez de mettre 220 devant BLABLABLA ...
     * ***/
    strcpy(buffer, "220 Proxy FTP en attente de commandes\n");
    write(descSockCOM, buffer, strlen(buffer));

    /*******

     * A vous de continuer !

     * *****/

    char clientCommand[MAXBUFFERLEN];
    char serverResponse[MAXBUFFERLEN];
    int serverSock;
    char *userCommand, *serverName, *login;

    ssize_t receivedBytes = read(descSockCOM, clientCommand, MAXBUFFERLEN - 1);
    if (receivedBytes < 0) {
        perror("Erreur lors de la lecture de la commande du client");
        close(descSockCOM);
        close(descSockRDV);
        exit(7);
    }
    clientCommand[receivedBytes] = '\0';
    printf("Commande reçue du client : %s\n", clientCommand);

    userCommand = strtok(clientCommand, " ");
    if (strcmp(userCommand, "USER") != 0) {
        fprintf(stderr, "Commande inattendue: %s\n", userCommand);
        close(descSockCOM);
        close(descSockRDV);
        exit(8);
    }

    login = strtok(NULL, "@");
    serverName = strtok(NULL, "\r\n");
    if (login == NULL || serverName == NULL) {
        fprintf(stderr, "Format de commande USER incorrect\n");
        close(descSockCOM);
        close(descSockRDV);
        exit(9);
    }

    if (connect2Server(serverName, "21", &serverSock) < 0) {
        fprintf(stderr, "Impossible de se connecter au serveur FTP %s\n", serverName);
        close(descSockCOM);
        close(descSockRDV);
        exit(10);
    }

    printf("Connexion au serveur FTP %s réussie.\n", serverName);

    char tmpBuffer[MAXBUFFERLEN];
    memset(tmpBuffer, 0, sizeof(tmpBuffer));

    // Envoi de la commande USER au serveur FTP
    snprintf(tmpBuffer, sizeof(tmpBuffer), "USER %s\r\n", login);
    if (write(serverSock, tmpBuffer, strlen(tmpBuffer)) < 0) {
        perror("Erreur write vers serverSock pour USER");
        close(serverSock);
        close(descSockCOM);
        close(descSockRDV);
        exit(11);
    }

    // Lecture de la réponse du serveur FTP
    memset(serverResponse, 0, MAXBUFFERLEN);
    ssize_t receivedBytesUser = read(serverSock, serverResponse, MAXBUFFERLEN - 1);
    if (receivedBytesUser < 0) {
        perror("Erreur lors de la lecture de la réponse du serveur FTP après USER");
        close(serverSock);
        close(descSockCOM);
        close(descSockRDV);
        exit(11);
    }
    serverResponse[receivedBytesUser] = '\0';

    // Envoi de la réponse du serveur FTP au client
    if (write(descSockCOM, serverResponse, strlen(serverResponse)) < 0) {
        perror("Erreur write vers descSockCOM pour la réponse USER");
        close(serverSock);
        close(descSockCOM);
        close(descSockRDV);
        exit(11);
    }

    // Boucle principale pour gérer les commandes et les réponses
    while (true) {
        memset(buffer, 0, MAXBUFFERLEN);

        // Lecture de la commande du client
        ssize_t clientBytes = read(descSockCOM, buffer, MAXBUFFERLEN - 1);
        if (clientBytes <= 0) {
            printf("Client a fermé la connexion ou erreur de lecture\n");
            break;
        }
        buffer[clientBytes] = '\0';
        printf("[CLIENT -> PROXY] %s", buffer);

        // Vérification et ajout de la terminaison CRLF si nécessaire
        if (clientBytes >= 2 && buffer[clientBytes - 2] != '\r' && buffer[clientBytes - 1] == '\n') {
            strncat(buffer, "\r", MAXBUFFERLEN - strlen(buffer) - 1);
        } else if (clientBytes >= 1 && buffer[clientBytes - 1] != '\n') {
            strncat(buffer, "\r\n", MAXBUFFERLEN - strlen(buffer) - 1);
        }

        // Gestion des commandes spécifiques
        if (strncmp(buffer, "USER", 4) == 0) {
            printf("[PROXY] Commande USER reçue.\n");
            if (write(serverSock, buffer, strlen(buffer)) < 0) break;
            memset(serverResponse, 0, MAXBUFFERLEN);
            ssize_t serverBytes = read(serverSock, serverResponse, MAXBUFFERLEN - 1);
            if (serverBytes <= 0) break;
            serverResponse[serverBytes] = '\0';
            if (write(descSockCOM, serverResponse, strlen(serverResponse)) < 0) break;
            printf("[SERVER -> PROXY] %s", serverResponse);
            continue;
        }

        if (strncmp(buffer, "PASS", 4) == 0) {
            printf("[PROXY] Commande PASS reçue.\n");
            if (write(serverSock, buffer, strlen(buffer)) < 0) break;
            memset(serverResponse, 0, MAXBUFFERLEN);
            ssize_t serverBytes = read(serverSock, serverResponse, MAXBUFFERLEN - 1);
            if (serverBytes <= 0) break;
            serverResponse[serverBytes] = '\0';
            if (write(descSockCOM, serverResponse, strlen(serverResponse)) < 0) break;
            printf("[SERVER -> PROXY] %s", serverResponse);
            continue;
        }

        if (strncmp(buffer, "SYST", 4) == 0) {
            printf("[PROXY] Commande SYST reçue.\n");
            if (write(serverSock, buffer, strlen(buffer)) < 0) break;
            memset(serverResponse, 0, MAXBUFFERLEN);
            ssize_t serverBytes = read(serverSock, serverResponse, MAXBUFFERLEN - 1);
            if (serverBytes <= 0) break;
            serverResponse[serverBytes] = '\0';
            if (write(descSockCOM, serverResponse, strlen(serverResponse)) < 0) break;
            printf("[SERVER -> PROXY] %s", serverResponse);
            continue;
        }

        if (strncmp(buffer, "QUIT", 4) == 0) {
            printf("[PROXY] Commande QUIT reçue. Fermeture de la connexion.\n");
            if (write(serverSock, buffer, strlen(buffer)) < 0) break;
            memset(serverResponse, 0, MAXBUFFERLEN);
            ssize_t serverBytes = read(serverSock, serverResponse, MAXBUFFERLEN - 1);
            if (serverBytes <= 0) break;
            serverResponse[serverBytes] = '\0';
            if (write(descSockCOM, serverResponse, strlen(serverResponse)) < 0) break;
            printf("[SERVER -> PROXY] %s", serverResponse);
            break;
        }

        // Commandes génériques (transmettre directement au serveur)
        printf("[PROXY] Commande générique reçue : %s", buffer);
        if (write(serverSock, buffer, strlen(buffer)) < 0) break;
        memset(serverResponse, 0, MAXBUFFERLEN);
        ssize_t serverBytes = read(serverSock, serverResponse, MAXBUFFERLEN - 1);
        if (serverBytes <= 0) break;
        serverResponse[serverBytes] = '\0';
        if (write(descSockCOM, serverResponse, strlen(serverResponse)) < 0) break;
        printf("[SERVER -> PROXY] %s", serverResponse);
    }

    // Fermeture des sockets
    close(serverSock);
    close(descSockCOM);
    close(descSockRDV);
    return 0;
}
