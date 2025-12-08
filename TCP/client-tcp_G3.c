#include <stdio.h>    // Per funzioni standard di I/O (printf, scanf, fprintf)
#include <stdlib.h>   // Per funzioni di utilità generale (es. exit)
#include <string.h>   // Per manipolazione di stringhe (memset, strcmp)
#include <ctype.h>    // Per manipolazione di caratteri (non strettamente usato qui ma utile in generale)

// Disabilita l'avviso di deprecazione per le funzioni Winsock non sicure (come gethostbyname)
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
#include <arpa/inet.h>  // Definizioni per le operazioni Internet (es. htons)
#include <netdb.h>      // Definizioni per la risoluzione dei nomi (es. gethostbyname)
#define closesocket close // Alias per uniformare la chiusura del socket
#endif

#define BUFFERSIZE 512              // Dimensione del buffer per la comunicazione
#define PROTOPORT 5193              // Porta TCP predefinita del server
#define DEFAULT_SERVER_NAME "localhost" // Nome del server predefinito (non usato nell'input)

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

// Funzione principale del client
int main(int argc, char *argv[]){
    char server_input[BUFFERSIZE];  // Buffer per leggere il nome del server
    char *server_name;              // Puntatore al nome del server
    int port = PROTOPORT;           // Porta del server (usa il valore predefinito)

    // 2. Richiesta nome server all'utente
    printf("Inserisci il nome del server (es. 'localhost'): ");
    if (scanf("%s", server_input) != 1) {
        printf("Input non valido.\n"); return -1;
    }
    server_name = server_input; // Imposta il nome del server letto

#if defined WIN32
    // Inizializzazione di Winsock su Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) { ErrorHandler("WSAStartup fallito."); return -1; }
#endif
    
    // 3. Risoluzione del nome e preparazione della connessione
    struct hostent *host;
    // gethostbyname risolve il nome del server (es. "localhost" o "www.example.com") nel suo indirizzo IP.
    if ((host = gethostbyname(server_name)) == NULL) { 
        ErrorHandler("Risoluzione nome host fallita."); 
        ClearWinSock(); 
        return -1; 
    }

    struct sockaddr_in sad; // Struttura per l'indirizzo del socket (server address)
    memset(&sad, 0, sizeof(sad)); // Inizializza la struttura a zero
    sad.sin_family = AF_INET; // Specifica la famiglia di indirizzi (Internet)
    // Copia il primo indirizzo IP risolto nella struttura sad.
    sad.sin_addr.s_addr = *(u_long*)host->h_addr_list[0]; 
    // Imposta la porta, convertendo in Network Byte Order (Big-Endian)
    sad.sin_port = htons(port); 

    // Creazione del socket client
    int clientSocket;
    // socket(famiglia, tipo, protocollo): crea un socket TCP (PF_INET, SOCK_STREAM, IPPROTO_TCP)
    if ((clientSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) { 
        ErrorHandler("Creazione socket client fallita."); 
        ClearWinSock(); 
        return -1; 
    }

    // PUNTO DELL'ERRORE: La chiamata 'connect'
    // Tenta di stabilire una connessione TCP con il server specificato in sad.
    if (connect(clientSocket, (struct sockaddr *)&sad, sizeof(sad)) < 0) { 
        ErrorHandler("Connessione fallita. Controlla che il server sia attivo."); 
        closesocket(clientSocket); 
        ClearWinSock(); 
        return -1;
    }
    printf("Connessione al server %s sulla porta %d riuscita.\n", server_name, port);
    
    char buffer[BUFFERSIZE]; // Buffer per la ricezione e l'invio di dati
    int bytes_received;      // Numero di byte ricevuti

    // 5. Ricezione e stampa del messaggio iniziale di benvenuto dal server
    // recv riceve dati dal socket (bloccante)
    bytes_received = recv(clientSocket, buffer, BUFFERSIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0'; // Terminatore di stringa
        printf("Server: %s\n", buffer);
    } else {
        ErrorHandler("Ricezione conferma connessione fallita (o server disconnesso)."); 
        closesocket(clientSocket); 
        ClearWinSock(); 
        return -1;
    }

    // 6. Lettura e invio del comando (carattere singolo)
    char command;
    printf("Inserisci l'operazione (A/S/M/D o altro per terminare): ");
    // " %c" ignora spazi bianchi e newline lasciati da input precedenti
    if (scanf(" %c", &command) != 1) { command = 'X'; } // Se l'input fallisce, usa 'X' per terminare

    // Invia il comando di un singolo carattere al server
    if (send(clientSocket, &command, 1, 0) != 1) { 
        ErrorHandler("Invio comando fallito."); 
        closesocket(clientSocket); 
        ClearWinSock(); 
        return -1; 
    }

    // 8. Ricezione della stringa che conferma l'operazione o la terminazione
    bytes_received = recv(clientSocket, buffer, BUFFERSIZE - 1, 0);
    if (bytes_received <= 0) { 
        ErrorHandler("Ricezione stringa operazione fallita."); 
        closesocket(clientSocket); 
        ClearWinSock(); 
        return -1; 
    }
    buffer[bytes_received] = '\0';
    printf("Server risponde: %s\n", buffer);

    // 8. Se l'operazione è aritmetica, prosegue con l'invio dei numeri
    if (strcmp(buffer, "ADDIZIONE") == 0 || strcmp(buffer, "SOTTRAZIONE") == 0 || 
        strcmp(buffer, "MOLTIPLICAZIONE") == 0 || strcmp(buffer, "DIVISIONE") == 0) 
    {
        int n1, n2;
        printf("Inserisci due interi: ");
        // Legge i due operandi
        if (scanf(" %d %d", &n1, &n2) != 2) { 
            printf("Input interi non valido. Terminazione.\n");
        } else {
            int numeri_net[2];
            // Conversione dei due interi da Host Byte Order a Network Byte Order
            numeri_net[0] = htonl(n1); 
            numeri_net[1] = htonl(n2);
            
            // Invio dei due interi (4 byte * 2 = 8 byte)
            if (send(clientSocket, (char*)numeri_net, sizeof(numeri_net), 0) == sizeof(numeri_net)) {
                
                // 10. Ricezione e stampa del risultato
                int risultato_net;
                // Attende la ricezione del risultato (4 byte)
                if (recv(clientSocket, (char*)&risultato_net, sizeof(risultato_net), 0) == sizeof(risultato_net)) {
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

    // Chiusura connessione e pulizia
    closesocket(clientSocket); // Chiude il socket
    ClearWinSock();            // Pulisce le risorse Winsock (se su Windows)

    return 0;
}