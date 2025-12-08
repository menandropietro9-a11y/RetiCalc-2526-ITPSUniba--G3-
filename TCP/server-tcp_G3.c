#include <stdio.h>    // Per funzioni standard di I/O
#include <stdlib.h>   // Per funzioni di utilità (es. atoi)
#include <string.h>   // Per manipolazione di stringhe (es. memset, strlen)
#include <ctype.h>    // Per manipolazione di caratteri (es. toupper)

// Disabilita l'avviso di deprecazione per le funzioni Winsock
#define _WINSOCK_DEPRECATED_NO_WARNINGS 

// Blocco condizionale per la piattaforma Windows (WIN32)
#if defined WIN32
#include <winsock2.h>   // Contiene definizioni per le funzioni Winsock
#include <ws2tcpip.h>   // Contiene definizioni aggiuntive per le API di rete
#pragma comment(lib, "ws2_32.lib") // Collega la libreria ws2_32.lib
#else
// Blocco per sistemi Unix-like (Linux, macOS, ecc.)
#include <unistd.h>     // Per funzioni POSIX (es. close)
#include <sys/socket.h> // Definizioni per le API dei socket
#include <arpa/inet.h>  // Definizioni per le operazioni Internet (es. htons, inet_ntoa)
#define closesocket close // Alias per uniformare la chiusura del socket
#endif

#define BUFFERSIZE 512              // Dimensione del buffer per la comunicazione
#define PROTOPORT 5193              // Porta TCP predefinita
#define QLEN 6                      // Lunghezza massima della coda di connessioni pendenti per 'listen'

// Funzione per la gestione degli errori e la stampa di un messaggio
void ErrorHandler (const char *errorMessage){
#if defined WIN32
    // Su Windows, stampa anche il codice di errore specifico Winsock
    fprintf(stderr, "Errore Winsock %d: %s\n", WSAGetLastError(), errorMessage);
#else
    // Su Unix-like, stampa solo il messaggio di errore
    fprintf(stderr, "Errore: %s\n", errorMessage);
#endif
}

// Funzione per la pulizia delle risorse Winsock (necessaria solo su Windows)
void ClearWinSock (){
#if defined WIN32
    // Termina l'uso della DLL Winsock
    WSACleanup();
#endif
}

// Funzione principale del server
int main(int argc, char *argv[]){
    int port = PROTOPORT; // Porta predefinita
    // Se fornito un argomento da linea di comando, usalo come porta
    if (argc > 1) port = atoi(argv[1]);

#if defined WIN32
    // Inizializzazione di Winsock su Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        ErrorHandler("Errore in WSAStartup"); return -1;
    }
#endif

    // 1. Creazione del socket del server
    int server_fd;
    // socket(famiglia, tipo, protocollo): crea un socket TCP
    if ((server_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) { 
        ErrorHandler("Creazione Socket fallita."); ClearWinSock(); return -1;
    }
    

    // 2. Preparazione dell'indirizzo e della porta di ascolto (Binding)
    struct sockaddr_in sad; // Struttura per l'indirizzo del socket (server address)
    memset(&sad, 0, sizeof(sad)); // Inizializza la struttura a zero
    sad.sin_family = AF_INET; // Specifica la famiglia di indirizzi (Internet)
    // Imposta l'indirizzo IP: INADDR_ANY significa ascoltare su tutte le interfacce disponibili
    sad.sin_addr.s_addr = htonl(INADDR_ANY); 
    // Imposta la porta, convertendo in Network Byte Order
    sad.sin_port = htons(port); 

    // Associa l'indirizzo e la porta al socket
    if (bind(server_fd, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
        ErrorHandler("Bind failed."); closesocket(server_fd); ClearWinSock(); return -1;
    }

    // 3. Metti il socket in modalità ascolto (Listen)
    // Imposta il socket per accettare connessioni e definisce la coda massima (QLEN)
    if (listen(server_fd, QLEN) < 0) {
        ErrorHandler("Listen Failed!"); closesocket(server_fd); ClearWinSock(); return -1;
    }

    printf("Server TCP in ascolto sulla porta %d...\n", port);
    
    struct sockaddr_in cad; // Struttura per l'indirizzo del client (Client Address)
    int clientSocket;       // Socket dedicato alla comunicazione con il singolo client
    int clientLen = sizeof(cad);
    
    // Loop principale: il server accetta connessioni per sempre
    while(1){
        // 4. Accettazione della connessione (Accept)
        // La chiamata è bloccante e attende che un client si connetta.
        // Restituisce un nuovo socket (clientSocket) per la comunicazione.
        if ((clientSocket = accept(server_fd, (struct sockaddr*)&cad, &clientLen)) < 0) {
            ErrorHandler("Accept failed"); continue; // Se fallisce, prova ad accettare di nuovo
        }
        // Stampa l'indirizzo IP del client connesso
        printf("Connessione accettata dall'indirizzo %s\n", inet_ntoa(cad.sin_addr));
        
        // 5. Server invia messaggio di conferma "connessione avvenuta"
        const char *welcome_msg = "connessione avvenuta";
        if (send(clientSocket, welcome_msg, strlen(welcome_msg), 0) < 0) {
            ErrorHandler("Invio welcome fallito."); closesocket(clientSocket); continue;
        }
        
        char command_buffer[1];
        int bytes_received;
        
        // 6. Server riceve il comando (singolo carattere)
        // Tenta di ricevere 1 byte di dati
        bytes_received = recv(clientSocket, command_buffer, 1, 0);

        if (bytes_received > 0) {
            char command = toupper(command_buffer[0]); // Converte il comando in maiuscolo
            const char *response_str = "TERMINE PROCESSO CLIENT";
            int operation_required = 0;

            // 7. Server determina il comando ricevuto e prepara la risposta
            if (command == 'A') { response_str = "ADDIZIONE"; operation_required = 1; }
            else if (command == 'S') { response_str = "SOTTRAZIONE"; operation_required = 1; }
            else if (command == 'M') { response_str = "MOLTIPLICAZIONE"; operation_required = 1; }
            else if (command == 'D') { response_str = "DIVISIONE"; operation_required = 1; }
            
            // Invia la stringa che conferma l'operazione da eseguire o la terminazione
            send(clientSocket, response_str, (int)strlen(response_str), 0);

            // 8. Se è stata richiesta un'operazione aritmetica (A, S, M, D)
            if (operation_required) {
                int numeri_net[2];
                // Riceve i due interi (8 byte totali) inviati dal client
                if (recv(clientSocket, (char*)numeri_net, sizeof(numeri_net), 0) == sizeof(numeri_net)) {
                    // Conversione da Network Byte Order (Big-Endian) a Host Byte Order
                    int n1 = ntohl(numeri_net[0]);
                    int n2 = ntohl(numeri_net[1]);
                    int risultato = 0;

                    // Esecuzione dell'operazione richiesta
                    switch(command) {
                        case 'A': risultato = n1 + n2; break;
                        case 'S': risultato = n1 - n2; break;
                        case 'M': risultato = n1 * n2; break;
                        case 'D': 
                            if (n2 != 0) { risultato = n1 / n2; }
                            // Se n2 è 0, il risultato rimane 0 (inizializzato)
                            break;
                    }

                    // Conversione del risultato in Network Byte Order prima dell'invio
                    int risultato_net = htonl(risultato); 
                    // Invia il risultato (4 byte) al client
                    send(clientSocket, (char*)&risultato_net, sizeof(risultato_net), 0);
                } else {
                    ErrorHandler("Errore nella ricezione dei numeri.");
                }
            }
        } else if (bytes_received < 0) {
            ErrorHandler("Errore in recv comando.");
        }
        
        // 9. Chiude il socket dedicato alla comunicazione col client corrente
        closesocket(clientSocket);
    } 

    // Chiusura socket principale (questa parte non è raggiunta a causa del 'while(1)')
    closesocket(server_fd);
    ClearWinSock();
    return 0;
}