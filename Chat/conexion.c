#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <pthread.h>
#include "header.h"

volatile int encendido = 1; 
volatile int activo    = 1;
volatile int escribe   = 0; 
volatile int regresa   = 0; 

int PUERTO = 4000; 
char DIRECTION[] = "127.0.0.1"; 

Mensaje interfaz;
Mensaje externo;

void conexion(Tuberia *local)
{
    pthread_t servidor, escritor, pipe_lector, pipe_escritor;

    printf("[HIJO] Inicializando los 4 hilos (Sockets y Pipes)...\n");

    if (pthread_create(&servidor, NULL, hilo_lector_p2p, (void *)local) != 0) { exit(1); }
    if (pthread_create(&escritor, NULL, hilo_escritor_p2p, NULL) != 0) { exit(1); }
    if (pthread_create(&pipe_lector, NULL, hilo_lector_pipe, (void *)local) != 0) { exit(1); }
    if (pthread_create(&pipe_escritor, NULL, hilo_escritor_pipe, (void *)local) != 0) { exit(1); }

    pthread_join(servidor, NULL);
    pthread_join(escritor, NULL);
    pthread_join(pipe_lector, NULL);
    pthread_join(pipe_escritor, NULL);

    printf("[HIJO] Todos los hilos destruidos de forma segura. Saliendo.\n");
}

void *hilo_lector_p2p(void *arg)
{
    Tuberia *local = (Tuberia *)arg;
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    Mensaje msg_recibido;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) pthread_exit(NULL);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PUERTO);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { close(server_fd); pthread_exit(NULL); }
    if (listen(server_fd, 5) < 0) { close(server_fd); pthread_exit(NULL); }

    while (encendido)
    {
        int tam_addr = sizeof(address);
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&tam_addr);

        if (new_socket >= 0)
        {
            memset(&msg_recibido, 0, sizeof(Mensaje));
            read(new_socket, &msg_recibido, sizeof(Mensaje));

            if (msg_recibido.id_emisor == -1)
            {
                printf("[GLOBAL] Evento de cierre... Cerrando hilos...\n");
                encendido = 0; activo = 0; 
                close(local->B[0]); close(local->A[1]);
                close(new_socket);
                break; 
            }
            else
            {
                printf("Hilo recibio mensaje en %d...\n", PUERTO);
                msg_recibido.tipo = RECEPCION;
                memcpy(&externo, &msg_recibido, sizeof(Mensaje));
                regresa = 1; 
            }
            close(new_socket);
        }
    }
    close(server_fd);
    pthread_exit(NULL);
}

void *hilo_escritor_p2p(void *arg)
{
    while (encendido)
    {
        if (escribe == 1)
        {
            escribe = 0; 
            enviar_msj(interfaz.ip_destino, interfaz.puerto_destino, interfaz);
            printf("Hilo envio mensaje a %d...\n", interfaz.puerto_destino);
        }
        usleep(10000); 
    }
    pthread_exit(NULL);
}

void *hilo_lector_pipe(void *arg)
{
    Tuberia *local = (Tuberia *)arg;
    Mensaje msg_pipe;

    while (activo)
    {
        int bytes = read(local->B[0], &msg_pipe, sizeof(Mensaje));
        if (bytes > 0 && activo)
        {
            if (msg_pipe.id_emisor == -1)
            {
                enviar_msj(DIRECTION, PUERTO, msg_pipe); 
                break; 
            }
            else
            {
                printf("Recibi Mensaje de la GUI...\n");
                memcpy(&interfaz, &msg_pipe, sizeof(Mensaje));
                escribe = 1; 
            }
        } else { break; }
    }
    close(local->B[0]);
    pthread_exit(NULL);
}

void *hilo_escritor_pipe(void *arg)
{
    Tuberia *local = (Tuberia *)arg;
    while (activo)
    {
        if(regresa == 1)
        {
            regresa = 0;
            write(local->A[1], &externo, sizeof(Mensaje));
        }
        usleep(10000); 
    }
    close(local->A[1]);
    pthread_exit(NULL);
}

void enviar_msj(char *ip_destino, int puerto_destino, Mensaje msg)
{
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return;
    serv_addr.sin_port = htons(puerto_destino);
    serv_addr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, ip_destino, &serv_addr.sin_addr) <= 0) { close(sock); return; }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0)
    {
        send(sock, &msg, sizeof(Mensaje), 0);
    }
    close(sock);
}