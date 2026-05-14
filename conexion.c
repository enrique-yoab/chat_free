/*
* Proyecto: Chat P2P simple en C
* Archivo: conexion.c
* Descripción: Implementa la logica de conexion P2P, envio y recepcion de mensajes, y medicion de tiempos.
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
char  *ip_emisora = "127.0.0.1";

Mensaje local;    // Mensaje que esta computadora va a enviar
Mensaje externo;  // Mensaje recibido (del peer o de la GUI)

// -----------------------------------------------
// Levanta la conexion P2P y se queda en el loop
// de envío esperando que inyectar_mensaje active
// la bandera 'enviar'.
// -----------------------------------------------
void conexion_p2p(int port2, int id, char *ip, char *name_user)
{
    // setbuf(stdout, NULL); // salida inmediata sin buffer
    PUERTO2        = port2;
    ip_receptora = ip;
    local.id_computadora = (id != 0) ? id : 500;
    strncpy(local.name_user, name_user, TAM_MAX - 1);
    memset(local.peticion, 0, sizeof(local.peticion));
    

    pthread_t hilo_server;
    pthread_create(&hilo_server, NULL, hilo_servidor, NULL);

    // Hilo cliente de envio: espera a que 'enviar' se active, luego envia el mensaje local al peer.
    while (1)
    {
        if (enviar == 1)
        {
            enviar = 0;

            if (local.llegada.horas || local.llegada.minutos || local.llegada.segundos || local.llegada.milisegundos)
            {
                capturar_tiempo(&local.envio);
                long long t_resp = convertion_ms(&local.envio) - convertion_ms(&local.llegada);
                convertir_tiempo(t_resp, &local.respuesta);
            }

            printf("[C%02d] Enviando \"%s\" → puerto %d\n",
                   local.id_computadora, local.peticion, PUERTO2);
            enviar_msj(PUERTO2, ip_receptora, local);
        }

        if (!encendido) break; // salir solo después de procesar envíos pendientes
        usleep(5000);
    }
    pthread_join(hilo_server, NULL);
}

// -----------------------------------------------
// Hilo servidor: acepta conexiones entrantes.
// Distingue comandos internos (id=0) de mensajes
// reales del peer (id>0).
// -----------------------------------------------
void *hilo_servidor(void *arg)
{
    (void)arg;
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

    printf("[C%02d] Escuchando en puerto %d...\n", local.id_computadora, PUERTO1);

    while (encendido)
    {
        int addrlen = sizeof(address);
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0) break;

        memset(&externo, 0, sizeof(Mensaje));
        read(new_socket, &externo, sizeof(Mensaje));
        close(new_socket);

        // ── Comando interno enviado por inyectar_mensaje() ──
        if (externo.id_computadora == 0)
        {
            if (externo.tipo == ENVIO)
            {
                // *** FIX PRINCIPAL: copiar el texto al mensaje local ***
                strncpy(local.peticion, externo.peticion, TAM_MAX - 1);
                local.peticion[TAM_MAX - 1] = '\0';
                enviar = 1; // activa el loop principal
            }
            else if (externo.tipo == SALIDA)
            {
                printf("[C%02d] Cerrando nodo...\n", local.id_computadora);
                encendido = 0;
            }
        }
        // ── Mensaje real del peer ──
        else
        {
            capturar_tiempo(&local.llegada);
            char *hora = convertir_hora(&local.llegada);
            printf("\n[C%02d] Mensaje: [%s], ID: C%02d %s: %s\n",
                   local.id_computadora,
                   externo.name_user,
                   externo.id_computadora,
                   hora,
                   externo.peticion);
            free(hora);
        }
    }

    close(server_fd);
    return NULL;
}

// -----------------------------------------------
// Envía un Mensaje por TCP al puerto e IP dados.
// -----------------------------------------------
void enviar_msj(int puerto_destino, char *ip, Mensaje datos)
{
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(puerto_destino);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[ERROR] connect");
        close(sock);
        return;
    }

    send(sock, &datos, sizeof(Mensaje), 0);
    close(sock);
}

// -----------------------------------------------
// Llamada desde el proceso padre (GUI/menú).
// Empaqueta el mensaje como comando interno (id=0)
// y lo inyecta al hilo_escucha del hijo via socket.
// -----------------------------------------------
void inyectar_mensaje(Mensaje *datos)
{
    Mensaje evento     = *datos;
    evento.id_computadora = 0;     // marca como comando interno
    evento.tipo           = ENVIO;
    enviar_msj(PUERTO1, ip_emisora, evento);
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