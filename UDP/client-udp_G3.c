#include <stdio.h>    // Per funzioni standard di I/O
#include <stdlib.h>   // Per funzioni di utilità generale
#include <string.h>   // Per manipolazione di stringhe (es. memset)
#include <ctype.h>    // Per manipolazione di caratteri

// Disabilita l'avviso di deprecazione per le funzioni Winsock non sicure (come gethostbyname)
#define _WINSOCK_DEPRECATED_NO_WARNINGS 

// Blocco condizionale per la piattaforma Windows (WIN32)
#if defined WIN32 || defined _WIN32
#include <winsock2.h>   // Contiene definizioni per le funzioni Winsock
#include <ws2tcpip.h>   // Contiene definizioni aggiuntive per le API di rete
#pragma comment(lib, "ws2_32.lib") // Collega la libreria ws2_32.lib
#else
// Blocco per sistemi Unix-like (Linux, macOS, ecc.)
#include <unistd.h>     // Per funzioni POSIX (es. close)
#include <sys/socket.h> // Definizioni per le API dei socket
#include <arpa/inet.h>  // Definizioni per le operazioni Internet (es. htons)
#include <netdb.h>      // Definizioni per la risoluzione dei nomi (es. gethostbyname)
#define closesocket close // Alias per uniformare la chiusura del socket
#endif

#define BUFFERSIZE 512              // Dimensione del buffer per la comunicazione
#define PROTOPORT 5193              // Porta UDP predefinita del server
#define DEFAULT_SERVER_NAME "localhost" // Nome del server predefinito

// Funzione per la gestione degli errori e la stampa di un messaggio
void ErrorHandler (const char *errorMessage){
// Stampa l'errore specifico a seconda della piattaforma
#if defined WIN32
    fprintf(stderr, "Errore Winsock %d: %s\n", WSAGetLastError(), errorMessage);
#else
    fprintf(stderr, "Errore: %s\n", errorMessage);
#endif
}

// Funzione per la pulizia delle risorse Winsock (necessaria solo su Windows)
void ClearWinSock (){
#if defined WIN32
    WSACleanup();
#endif
}

int main(int argc, char *argv[]){
    char server_input[BUFFERSIZE];  // Buffer per leggere il nome del server
    char *server_name;              // Puntatore al nome del server
    int port = PROTOPORT;           // Porta del server

    // Richiesta nome server all'utente
    printf("Inserisci il nome del server (es. 'localhost'): "); 
    if (scanf("%s", server_input) != 1) { printf("Input non valido.\n"); return -1; }
    server_name = server_input; // Imposta il nome del server letto

#if defined WIN32
    // Inizializzazione di Winsock su Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) { ErrorHandler("WSAStartup fallito."); return -1; }
#endif
    
    // Risoluzione del nome DNS
    struct hostent *host;
    // gethostbyname risolve il nome del server nel suo indirizzo IP.
    if ((host = gethostbyname(server_name)) == NULL) { 
        ErrorHandler("Risoluzione nome host fallita."); ClearWinSock(); return -1; 
    }

    // Setup indirizzo server (sad = Server Address)
    struct sockaddr_in sad;
    memset(&sad, 0, sizeof(sad)); // Inizializza la struttura a zero
    sad.sin_family = AF_INET; // Famiglia di indirizzi (Internet)
    // Copia il primo indirizzo IP risolto
    sad.sin_addr.s_addr = *(u_long*)host->h_addr_list[0];
    // Imposta la porta, convertendo in Network Byte Order
    sad.sin_port = htons(port);
    int sad_len = sizeof(sad); // Lunghezza della struttura dell'indirizzo

    // Creazione Socket UDP
    int clientSocket;
    // socket(famiglia, tipo, protocollo): crea un socket UDP (SOCK_DGRAM)
    if ((clientSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) { 
        ErrorHandler("Creazione socket client fallita."); ClearWinSock(); return -1; 
    }
    

    printf("Client UDP pronto per la comunicazione con %s:%d.\n", server_name, port);

    // 1. Lettura e invio comando (carattere singolo)
    char command;
    printf("Inserisci l'operazione (A/S/M/D o altro per terminare): ");
    // L'uso di " %c" ignora gli spazi bianchi lasciati da input precedenti
    if (scanf(" %c", &command) != 1) { command = 'X'; } 

    // Invio del comando (sendto)
    // Invia 1 byte di dati (il comando) all'indirizzo specificato in `sad`.
    if (sendto(clientSocket, &command, 1, 0, (struct sockaddr *)&sad, sad_len) != 1) { 
        ErrorHandler("Invio comando fallito."); closesocket(clientSocket); ClearWinSock(); return -1;
    }

    // 2. Ricezione stringa operazione/terminazione
    char buffer[BUFFERSIZE];
    int bytes_received;
    
    // Ricezione della risposta dal server (recvfrom)
    // Riceve un datagramma e memorizza l'indirizzo del mittente in `sad` (anche se qui è già preimpostato, è la pratica standard per UDP).
    bytes_received = recvfrom(clientSocket, buffer, BUFFERSIZE - 1, 0, (struct sockaddr *)&sad, &sad_len);
    if (bytes_received <= 0) { ErrorHandler("Ricezione stringa operazione fallita."); closesocket(clientSocket); ClearWinSock(); return -1; }
    buffer[bytes_received] = '\0';
    printf("Server risponde: %s\n", buffer);

    // 3. Se l'operazione è aritmetica, chiede e invia i due interi
    if (strcmp(buffer, "ADDIZIONE") == 0 || strcmp(buffer, "SOTTRAZIONE") == 0 || 
        strcmp(buffer, "MOLTIPLICAZIONE") == 0 || strcmp(buffer, "DIVISIONE") == 0) 
    {
        int n1, n2;
        printf("Inserisci due interi: ");
        if (scanf("%d %d", &n1, &n2) != 2) {
            printf("Input interi non valido. Terminazione.\n");
        } else {
            int numeri_net[2];
            // Conversione dei due interi da Host Byte Order a Network Byte Order
            numeri_net[0] = htonl(n1); 
            numeri_net[1] = htonl(n2);
            
            // Invio dei due interi (8 byte) (sendto)
            if (sendto(clientSocket, (char*)numeri_net, sizeof(numeri_net), 0, (struct sockaddr *)&sad, sad_len) == sizeof(numeri_net)) {
                
                // 4. Ricezione e stampa del risultato
                int risultato_net;
                // Riceve il risultato (4 byte) dal server (recvfrom)
                if (recvfrom(clientSocket, (char*)&risultato_net, sizeof(risultato_net), 0, (struct sockaddr *)&sad, &sad_len) == sizeof(risultato_net)) {
                    // Conversione del risultato da Network Byte Order a Host Byte Order
                    int risultato = ntohl(risultato_net); 
                    printf("\nRISULTATO RICEVUTO: %d\n", risultato);
                } else {
                    ErrorHandler("Ricezione risultato fallita.");
                }
            } else {
                ErrorHandler("Invio numeri fallito.");
            }
        }
    } 

    // Chiusura socket e pulizia
    closesocket(clientSocket); // Chiude il socket
    ClearWinSock();            // Pulisce le risorse Winsock (se su Windows)

    return 0;

}
