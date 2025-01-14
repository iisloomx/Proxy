/*
* miniprojet C fait par :
* Salim AL-NAAIMI
* Kaya Maure
* R3.05
*/

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
    char serverPort[MAXPORTLEN];     // Port du serveur
    int descSockRDV;                 // Descripteur de socket de rendez-vous
    int descSockCOM;                 // Descripteur de socket de communication
    struct addrinfo hints;           // Contrôle la fonction getaddrinfo
    struct addrinfo *res;            // Contient le résultat de la fonction getaddrinfo
    struct sockaddr_storage myinfo;  // Informations sur la connexion de RDV
    struct sockaddr_storage from;    // Informations sur le client connecté
    socklen_t len;                   // Variable utilisée pour stocker les longueurs des structures de socket
    char buffer[MAXBUFFERLEN];       // Tampon de communication entre le client et le serveur
    char us[50];                     // Contient l'utilisateur
    char serv[50];                   // Contient le serveur

    //Initialisation de la socket de RDV IPv4/TCP */
    descSockRDV = socket(AF_INET, SOCK_STREAM, 0);
    if (descSockRDV == -1) {
        perror("Erreur création socket RDV\n");
        exit(2);
    }

    
    //Publication de la socket au niveau du système */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;      // mode serveur
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_family = AF_INET;        // IPv4 uniquement

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
    ecode = getsockname(descSockRDV, (struct sockaddr *)&myinfo, &len);
    if (ecode == -1) {
        perror("Erreur getsockname");
        exit(4);
    }

    ecode = getnameinfo((struct sockaddr *)&myinfo, sizeof(myinfo),
                        serverAddr, MAXHOSTLEN, serverPort, MAXPORTLEN,
                        NI_NUMERICHOST | NI_NUMERICSERV);
    if (ecode != 0) {
        fprintf(stderr, "Erreur dans getnameinfo: %s\n", gai_strerror(ecode));
        exit(4);
    }

    printf("L'adresse d'écoute est : %s\n", serverAddr);
    printf("Le port d'écoute est : %s\n", serverPort);
    
    // Mise en écoute de la socket */
    ecode = listen(descSockRDV, LISTENLEN);
    if (ecode == -1) {
        perror("Erreur initialisation écoute");
        exit(5);
    }

    printf("En attente d'une connexion... utilisez ftp -z nossl -d %s %s\n", serverAddr, serverPort);

    //Acceptation d'une connexion entrante */
    len = sizeof(struct sockaddr_storage);
    descSockCOM = accept(descSockRDV, (struct sockaddr *)&from, &len);
    if (descSockCOM == -1) {
        perror("Erreur accept\n");
        exit(6);
    }

   

    strcpy(buffer, "220 Bienvenue sur le Proxy FTP\r\n");
    write(descSockCOM, buffer, strlen(buffer));

    /*******
     *
     * A vous de continuer !
     *
     * *****/

    // Lecture de la commande USER du client
    ecode = read(descSockCOM, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1) {
        perror("Problème de lecture commande USER\n");
        close(descSockCOM);
        close(descSockRDV);
        exit(7);
    }
    buffer[ecode] = '\0';
    printf("MESSAGE RECU DU CLIENT: \"%s\".\n", buffer);

    // Découpage de l'entrée [login]@[serveur]
    sscanf(buffer, "USER %[^@]@%s", us, serv);

    // Ajout de \r\n nécessaire pour la connexion au serveur
    // (dans le cas où on envoie la commande USER)
    // Cependant, on va plutôt construire la commande plus tard.
    // strcat(us, "\r\n");

    printf("Utilisateur: %s\n", us);
    printf("Serveur : %s\n", serv);

    // Connection au serveur sur le port 21
    int SockCOMServ;
    char *port = "21";
    printf("----Tentative de connection au serveur sur le port 21----\n");
    ecode = connect2Server(serv, port, &SockCOMServ);
    if (ecode == -1) {
        perror("Problème connection au serveur FTP\n");
        close(descSockCOM);
        close(descSockRDV);
        exit(8);
    } else {
        printf("Connexion établie\n");

        // Récupérer la bannière du serveur (ex: 220 ...)
        ecode = read(SockCOMServ, buffer, MAXBUFFERLEN - 1);
        if (ecode == -1) {
            perror("Problème de lecture bannière serveur\n");
            close(SockCOMServ);
            close(descSockCOM);
            close(descSockRDV);
            exit(9);
        }
        buffer[ecode] = '\0';
        printf("MESSAGE RECU DU SERVEUR: \"%s\".\n", buffer);

        // Transfert de la bannière au client
        write(descSockCOM, buffer, strlen(buffer));

        // Envoi de la commande USER <us>\r\n au serveur
        // (Le \r\n est indispensable en FTP)
        printf("----Tentative d'envoi de la commande USER au serveur----\n");
        char cmdSRV[60] = "USER ";
        strcat(cmdSRV, us);
        strcat(cmdSRV, "\r\n");
        write(SockCOMServ, cmdSRV, strlen(cmdSRV));

        // Écoute et récupère la demande password du serveur (Code: 331)
        printf("----Tentative de recuperation la socket demande password----\n");
        ecode = read(SockCOMServ, buffer, MAXBUFFERLEN - 1);
        if (ecode == -1) {
            perror("Probleme de lecture reponse 331\n");
            close(SockCOMServ);
            close(descSockCOM);
            close(descSockRDV);
            exit(10);
        }
        buffer[ecode] = '\0';
        printf("MESSAGE RECU DU SERVEUR: \"%s\".\n", buffer);

        // Transfert demande password au client
        printf("----Je transfère au client la demande----\n");
        write(descSockCOM, buffer, strlen(buffer));

        // Boucle de "ping pong" entre client et serveur
        // On écoute TOUTES les commandes du client, on les transfère au serveur
        // et on retransmet au client la réponse du serveur
        // (Cela inclut PASS, SYST, LIST, etc.)

        while (1) {
            memset(buffer, 0, MAXBUFFERLEN);
            ecode = read(descSockCOM, buffer, MAXBUFFERLEN - 1);
            if (ecode <= 0) {
                printf("----Fermeture de la connexion par le client ou erreur de lecture----\n");
                break;
            }
            buffer[ecode] = '\0';
            printf("MESSAGE RECU DU CLIENT: \"%s\".\n", buffer);

            // Envoi de cette commande au serveur
            printf("----Je transfère la commande du client au serveur----\n");
            write(SockCOMServ, buffer, strlen(buffer));

            // On lit la réponse du serveur
            memset(buffer, 0, MAXBUFFERLEN);
            ecode = read(SockCOMServ, buffer, MAXBUFFERLEN - 1);
            if (ecode <= 0) {
                printf("----Fermeture de la connexion par le serveur ou erreur----\n");
                break;
            }
            buffer[ecode] = '\0';
            printf("MESSAGE RECU DU SERVEUR: \"%s\".\n", buffer);

            // On retransmet cette réponse au client
            write(descSockCOM, buffer, strlen(buffer));

            // Si le client a envoyé "QUIT", on arrête la boucle
            if (strncmp(buffer, "221", 3) == 0) {
                // Le code 221 signale généralement la fermeture FTP
                printf("----Le serveur FTP a fermé la session (221)----\n");
                break;
            }
        }

        // Fermeture des connexions
        printf("----Je ferme les connexions----\n");
        close(SockCOMServ);
        close(descSockCOM);
        close(descSockRDV);
    }
    return 0;
}
