#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define PSEUDO_MAX_LENGTH 32
#define MAX_MESSAGE_LENGTH 256

// Liste de mots interdits pour le filtrage de contenu
const char *banned_words[] = {"badword1", "badword2", "badword3"};
int num_banned_words = sizeof(banned_words) / sizeof(banned_words[0]);

// Structure pour stocker les informations de chaque client
typedef struct {
    int socket_fd;
    struct sockaddr_in address;
    char pseudonym[PSEUDO_MAX_LENGTH];
} ClientData;

// Liste des clients connectés
ClientData *clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Fonction de filtrage pour masquer les mots interdits dans un message
void filter_message(char *message) {
    for (int i = 0; i < num_banned_words; ++i) {
        char *pos;
        while ((pos = strcasestr(message, banned_words[i])) != NULL) {
            memset(pos, '*', strlen(banned_words[i]));  // Masque le mot avec des '*'
        }
    }
}

// Diffuse un message à tous les autres clients connectés
void broadcast_message(char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; ++i) {
        if (clients[i]->socket_fd != sender_socket) {
            if (send(clients[i]->socket_fd, message, strlen(message), 0) < 0) {
                perror("Erreur d'envoi du message");
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Envoie une notification de connexion ou de déconnexion
void notify_connection_status(const char *pseudonym, int is_connected) {
    char message[128];
    snprintf(message, sizeof(message), "*** %s s'est %s ***\n", pseudonym, is_connected ? "connecté(e)" : "déconnecté(e)");
    broadcast_message(message, -1);
}

// Fonction qui gère chaque client connecté dans un thread séparé
void *handle_client(void *arg) {
    ClientData *client_data = (ClientData *)arg;
    int client_socket = client_data->socket_fd;
    char buffer[MAX_MESSAGE_LENGTH + 1];
    int bytes_received;

    // Demande au client de saisir un pseudonyme
    if ((bytes_received = recv(client_socket, client_data->pseudonym, PSEUDO_MAX_LENGTH, 0)) <= 0) {
        close(client_socket);
        free(client_data);
        pthread_exit(NULL);
    }
    client_data->pseudonym[bytes_received] = '\0';

    // Notifie les autres clients que ce client s'est connecté
    notify_connection_status(client_data->pseudonym, 1);

    // Boucle pour recevoir et diffuser les messages
    while ((bytes_received = recv(client_socket, buffer, MAX_MESSAGE_LENGTH, 0)) > 0) {
        buffer[bytes_received] = '\0';

        // Filtre les mots interdits dans le message
        filter_message(buffer);

        // Formate et diffuse le message avec le pseudonyme
        char formatted_message[1064];
        snprintf(formatted_message, sizeof(formatted_message), "%s: %s", client_data->pseudonym, buffer);
        printf("%s\n", formatted_message);  // Affiche le message côté serveur
        broadcast_message(formatted_message, client_socket);
    }

    // Notifie les autres clients que ce client s'est déconnecté
    notify_connection_status(client_data->pseudonym, 0);

    // Supprime le client de la liste
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; ++i) {
        if (clients[i] == client_data) {
            clients[i] = clients[client_count - 1];
            clients[client_count - 1] = NULL;
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(client_socket);
    free(client_data);
    printf("Client %s déconnecté.\n", client_data->pseudonym);
    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t tid;

    // Création du socket du serveur
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Échec de création du socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Lier le socket à l'adresse et au port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Échec de la liaison");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Écoute des connexions entrantes
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Échec de l'écoute");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Le serveur écoute sur le port %d...\n", PORT);

    // Accepter et gérer les connexions clients
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) >= 0) {
        printf("Nouvelle connexion client.\n");

        // Allocation et initialisation des données client
        ClientData *client_data = (ClientData *)malloc(sizeof(ClientData));
        client_data->socket_fd = client_socket;
        client_data->address = client_addr;

        // Ajouter le client à la liste des clients connectés
        pthread_mutex_lock(&clients_mutex);
        clients[client_count++] = client_data;
        pthread_mutex_unlock(&clients_mutex);

        // Créer un thread pour gérer le client
        if (pthread_create(&tid, NULL, handle_client, (void *)client_data) != 0) {
            perror("Échec de création du thread");
            free(client_data);
            continue;
        }

        // Détacher le thread pour libérer automatiquement les ressources
        pthread_detach(tid);
    }

    if (client_socket < 0) {
        perror("Échec de l'acceptation");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    close(server_socket);
    return 0;
}
