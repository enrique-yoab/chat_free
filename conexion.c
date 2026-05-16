/*
* Proyecto: Chat P2P simple en C
* Archivo: conexion.c
* Descripción: Implementa la logica de conexion P2P, envio, recepcion de mensajes, y medicion de tiempos.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>
#include "conexion.h"

// ------- Variables globales del nodo -------
int    PUERTO1      = 0;
int    PUERTO2      = 0;
int    encendido  = 1;
int    enviar     = 0;
char  *ip_receptora;
char  *ip_emisora = "127.0.0.1"; // Aqui va la ip local

Mensaje local;    // Mensaje que esta computadora va a enviar
Mensaje externo;  // Mensaje recibido (del peer o de la GUI)

// -----------------------------------------------
// Levanta la conexion P2P y se queda en el loop
// de envío esperando que inyectar_mensaje active
// la bandera 'enviar'.
// -----------------------------------------------
void conexion_p2p(int port1, int port2, int id, char *ip, char *name_user, Tuberia *hijo)
{
    PUERTO1 = port1;
    PUERTO2 = port2;
    ip_receptora = ip;
    local.id_computadora = id;
    strncpy(local.name_user, name_user, TAM_MAX - 1);
    memset(local.peticion, 0, sizeof(local.peticion));

    pthread_t hilo_server;
    // Le pasamos el struct 'hijo' al servidor para que tenga acceso al pipe C[1]
    pthread_create(&hilo_server, NULL, hilo_servidor, hijo);

    // Contenedor para leer lo que la interfaz mandó a través del interceptor
    Tuberia orden_desde_gui;

    printf("[HIJO] Escuchando órdenes de la GUI en pipe B...\n");
    
    while (encendido)
    {
        // El proceso se duerme aquí hasta que la interfaz escriba algo en el tubo
        ssize_t bytes = read(hijo->B[0], &orden_desde_gui, sizeof(Tuberia));
        
        if (bytes > 0)
        {
            // Si la GUI mandó un mensaje para enviar al peer
            if (orden_desde_gui.intermedio.tipo == ENVIO)
            {
                // Copiamos el texto de la tubería al mensaje que viajará por la red
                strncpy(local.peticion, orden_desde_gui.intermedio.peticion, TAM_MAX - 1);
                local.peticion[TAM_MAX - 1] = '\0';

                // Gestionamos marcas de tiempo (tu lógica intacta)
                if (local.llegada.horas || local.llegada.minutos || local.llegada.segundos || local.llegada.milisegundos)
                {
                    capturar_tiempo(&local.envio);
                    long long t_resp = convertion_ms(&local.envio) - convertion_ms(&local.llegada);
                    convertir_tiempo(t_resp, &local.respuesta);
                }

                printf("[C%02d] Enviando \"%s\" por Red → puerto %d\n", local.id_computadora, local.peticion, PUERTO2);
                
                enviar_msj(); // Envia por TCP real
            }
            // Si la GUI ordenó cerrar el programa
            else if (orden_desde_gui.intermedio.tipo == SALIDA)
            {
                printf("[HIJO] Orden de SALIDA recibida desde la GUI.\n");
                encendido = 0;

                // --- SOLUCIÓN: Despertar al accept() bloqueado ---
                int sock_despertador = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in addr_propia;
                addr_propia.sin_family = AF_INET;
                addr_propia.sin_port = htons(PUERTO1); // Nos conectamos a nuestro propio puerto
                inet_pton(AF_INET, "127.0.0.1", &addr_propia.sin_addr);
                
                // Al conectarnos a nosotros mismos, accept() sale del bloqueo,
                // ve que encendido == 0 y rompe el bucle while del servidor limpiamente.
                connect(sock_despertador, (struct sockaddr *)&addr_propia, sizeof(addr_propia));
                close(sock_despertador);
                
                break;
            }
        }
    }

    // Para despertar al accept() del servidor si se quedó colgado esperando conexión
    // e iniciar un cierre limpio, puedes hacer un connect ficticio aquí si es necesario, 
    // o simplemente cerrar el socket del servidor.
    pthread_join(hilo_server, NULL);
}

// -----------------------------------------------
// Hilo servidor: acepta conexiones entrantes.
// Distingue comandos internos (id=0) de mensajes
// reales del peer (id>0).
// -----------------------------------------------
void *hilo_servidor(void *arg)
{
    Tuberia *hijo_pipe = (Tuberia *)arg; // Recuperamos las tuberías del hijo
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(PUERTO1);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[ERROR] bind");
        return NULL;
    }
    if (listen(server_fd, 3) < 0) {
        perror("[ERROR] listen");
        return NULL;
    }

    printf("[C%02d] Red TCP escuchando en puerto %d...\n", local.id_computadora, PUERTO1);

    while (encendido)
    {
        int addrlen = sizeof(address);
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0 || !encendido) break;

        memset(&externo, 0, sizeof(Mensaje));
        ssize_t bytes = read(new_socket, &externo, sizeof(Mensaje));
        close(new_socket);

        if (bytes <= 0) continue;

        // Ya no existen comandos internos (id == 0) por socket. 
        // Todo lo que entra por aquí es un mensaje real de la red (Peer remoto).
        capturar_tiempo(&local.llegada);
        char *hora = convertir_hora(&local.llegada);
        printf("\n[C%02d] Mensaje recibido de Red: [%s]: %s\n", local.id_computadora, externo.name_user, externo.peticion);
        free(hora);

        // --- ENVIAR A LA INTERFAZ A TRAVÉS DE C ---
        if (hijo_pipe != NULL)
        {
            Tuberia paquete_hacia_gui = *hijo_pipe;
            paquete_hacia_gui.intermedio.tipo = RECEPCION;
            paquete_hacia_gui.intermedio.id_computadora = externo.id_computadora;
            strncpy(paquete_hacia_gui.intermedio.peticion, externo.peticion, TAM_MAX - 1);
            paquete_hacia_gui.intermedio.peticion[TAM_MAX - 1] = '\0';
            
            // Empujamos el mensaje hacia el Interceptor por el Pipe C
            write(hijo_pipe->C[1], &paquete_hacia_gui, sizeof(Tuberia));
        }
    }

    close(server_fd);
    return NULL;
}
// -----------------------------------------------
// Envía un Mensaje por TCP al puerto e IP amiga.
// -----------------------------------------------
void enviar_msj()
{
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(PUERTO2);
    inet_pton(AF_INET, ip_receptora, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[ERROR] connect");
        close(sock);
        return;
    }

    send(sock, &local, sizeof(Mensaje), 0);
    close(sock);
}

// ------- Utilidades de tiempo -------
char *convertir_hora(tiempo *t)
{
    char *buf = malloc(24);
    snprintf(buf, 24, "[%02d:%02d:%02d.%03d]",
             t->horas, t->minutos, t->segundos, t->milisegundos);
    return buf;
}

long long convertion_ms(tiempo *t)
{
    return (long long)t->horas   * 3600000LL +
           (long long)t->minutos *   60000LL +
           (long long)t->segundos *   1000LL +
           t->milisegundos;
}

void convertir_tiempo(long long ms, tiempo *t)
{
    t->milisegundos = ms % 1000;
    t->segundos     = (ms / 1000)    % 60;
    t->minutos      = (ms / 60000)   % 60;
    t->horas        = (ms / 3600000) % 24;
}

void capturar_tiempo(tiempo *t)
{
    struct timespec ts;
    struct tm *tm_info;

    clock_gettime(CLOCK_REALTIME, &ts);
    tm_info = localtime(&ts.tv_sec);

    t->milisegundos = ts.tv_nsec / 1000000;
    t->segundos     = tm_info->tm_sec;
    t->minutos      = tm_info->tm_min;
    t->horas        = tm_info->tm_hour;
}