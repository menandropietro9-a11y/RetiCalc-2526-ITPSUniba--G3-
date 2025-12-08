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
#include <arpa/inet.h>  // Definizioni per le operazioni Internet (es. htons)
#define closesocket close // Alias per uniformare la chiusura del socket
#endif

#define BUFFERSIZE 512              // Dimensione del buffer
#define PROTOPORT 5193              // Porta UDP predefinita

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

int main(int argc, char *argv[]){
    int port = PROTOPORT; // Porta predefinita
    // Se fornito un argomento da linea di comando, usalo come porta
    if (argc > 1) port = atoi(argv[1]);

#if defined WIN32
    // Inizializzazione di Winsock su Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) { ErrorHandler("Errore in WSAStartup"); return -1; }
#endif

    int server_fd;
    // 1. Creazione del socket UDP
    // Usa SOCK_DGRAM per i datagrammi (protocollo UDP)
    if ((server_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) { 
        ErrorHandler("Creazione Socket fallita."); ClearWinSock(); return -1;
    }
    

    // 2. Preparazione dell'indirizzo e della porta di ascolto (Binding)
    struct sockaddr_in sad; // Struttura per l'indirizzo del server
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    // Ascolta su tutte le interfacce disponibili
    sad.sin_addr.s_addr = htonl(INADDR_ANY); 
    // Imposta la porta, convertendo in Network Byte Order
    sad.sin_port = htons(port); 

    // Associa l'indirizzo e la porta al socket
    if (bind(server_fd, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
        ErrorHandler("Bind failed."); closesocket(server_fd); ClearWinSock(); return -1;
    }

    printf("Server UDP in ascolto sulla porta %d...\n", port);
    
    struct sockaddr_in client_addr; // Struttura per memorizzare l'indirizzo del client mittente
    int client_addr_len = sizeof(client_addr);
    
    // Loop principale: il server UDP è sempre in attesa di datagrammi
    while(1){
        char command_buffer[1];
        int bytes_received;
        
        // 3. Ricezione del comando (recvfrom)
        // La chiamata è bloccante e attende il primo datagramma.
        // `recvfrom` salva il dato nel buffer e l'indirizzo del mittente in `client_addr`.
        bytes_received = recvfrom(server_fd, command_buffer, 1, 0, (struct sockaddr*)&client_addr, &client_addr_len);

        if (bytes_received > 0) {
            char command = toupper(command_buffer[0]);
            const char *response_str = "TERMINE PROCESSO CLIENT";
            int operation_required = 0;

            // 4. Server determina l'operazione in base al comando
            if (command == 'A') { response_str = "ADDIZIONE"; operation_required = 1; }
            else if (command == 'S') { response_str = "SOTTRAZIONE"; operation_required = 1; }
            else if (command == 'M') { response_str = "MOLTIPLICAZIONE"; operation_required = 1; }
            else if (command == 'D') { response_str = "DIVISIONE"; operation_required = 1; }
            
            // 5. Invia la stringa operazione al client (sendto)
            // Usa l'indirizzo del mittente (`client_addr`) salvato da `recvfrom`.
            sendto(server_fd, response_str, (int)strlen(response_str), 0, (struct sockaddr*)&client_addr, client_addr_len);

            // 6. Se operazione richiesta, riceve i due interi e invia il risultato
            if (operation_required) {
                int numeri_net[2];
                
                // Riceve i due interi successivi (8 byte) dal *medesimo* client.
                // La chiamata è bloccante e aspetta il datagramma con i numeri.
                if (recvfrom(server_fd, (char*)numeri_net, sizeof(numeri_net), 0, (struct sockaddr*)&client_addr, &client_addr_len) == sizeof(numeri_net)) {
                    // Conversione da Network Byte Order a Host Byte Order
                    int n1 = ntohl(numeri_net[0]);
                    int n2 = ntohl(numeri_net[1]);
                    int risultato = 0;

                    // Esecuzione dell'operazione
                    switch(command) {
                        case 'A': risultato = n1 + n2; break;
                        case 'S': risultato = n1 - n2; break;
                        case 'M': risultato = n1 * n2; break;
                        case 'D': 
                            if (n2 != 0) { risultato = n1 / n2; }
                            // Gestione implicita della divisione per zero (risultato = 0)
                            break;
                    }

                    // Conversione del risultato in Network Byte Order
                    int risultato_net = htonl(risultato); 
                    // Invia il risultato (4 byte) al client (sendto)
                    sendto(server_fd, (char*)&risultato_net, sizeof(risultato_net), 0, (struct sockaddr*)&client_addr, client_addr_len);
                } else {
                    ErrorHandler("Errore nella ricezione dei numeri.");
                }
            }
        } else if (bytes_received < 0) {
            ErrorHandler("Errore in recvfrom comando.");
        }
        // Il server UDP non ha bisogno di chiudere la connessione e torna in attesa.
    } 

    // Chiusura del socket principale e pulizia (non raggiunta nel ciclo infinito)
    closesocket(server_fd);
    ClearWinSock();
    return 0;
}