#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include "conexion.h"

// Variables globales internas del proceso (cada proceso tiene su propia copia tras el fork)
Tuberia emisora, receptora;

// Escribe en el canal de salida del proceso padre o hijo
void avisar(Tuberia interceptada)
{
    int canal = (interceptada.identificador == 0) ? interceptada.A[1] : interceptada.C[1];
    write(canal, &interceptada, sizeof(Tuberia));
}

// ─────────────────────────────────────────────────────────────────
//  HILO LECTOR  (reutilizado por los 3 procesos)
//
//  Cascada de SALIDA:
//    Padre  --> A - Interceptor --> B - Hijo
//    Hijo   --> C - Interceptor --> D - Padre
//  Cada proceso que recibe SALIDA la reenvía hacia adelante y sale.
// ─────────────────────────────────────────────────────────────────
void *hilo_lector(void *arg)
{
    Tuberia *proceso = (Tuberia *)arg;
    Tuberia recepcion;
    int pipe_lectura, pipe_escritura = -1;

    switch (proceso->identificador) {
        case 0: // PADRE: Lee de D
            pipe_lectura = proceso->D[0];
            break;
        case 1: // INTERCEPTOR: puente directo A-B  o  C-D
            pipe_lectura  = proceso->flag ? proceso->C[0] : proceso->A[0];
            pipe_escritura = proceso->flag ? proceso->D[1] : proceso->B[1];
            break;
        case 2: // HIJO: Lee de B
            pipe_lectura = proceso->B[0];
            break;
    }

    while (1)
    {
        ssize_t bytes = read(pipe_lectura, &recepcion, sizeof(Tuberia));
        if (bytes <= 0) break; // EOF o error: el escritor cerró el pipe

        // ── SALIDA: reenviar en cascada y salir ──────────────────
        if (recepcion.intermedio.tipo == SALIDA)
        {
            printf("[PROCESO %d%s] SALIDA recibida → cerrando hilo.\n",
                   proceso->identificador,
                   (proceso->identificador == 1) ? (proceso->flag ? " hilo-C" : " hilo-A") : "");

            // El interceptor reenvía SALIDA al siguiente proceso
            if (pipe_escritura != -1)
                write(pipe_escritura, &recepcion, sizeof(Tuberia));

            // El hijo reenvía SALIDA de vuelta por C para que llegue al padre
            if (proceso->identificador == 2)
                write(proceso->C[1], &recepcion, sizeof(Tuberia));

            break; // Salimos del bucle
        }

        // ── ENVIO ────────────────────────────────────────────────
        if (recepcion.intermedio.tipo == ENVIO)
        {
            emisora = recepcion;
            printf("[PROCESO %d] ENVIO recibido: \"%s\"\n",
                   proceso->identificador, recepcion.intermedio.peticion);

            // TEST: El hijo imprime el mensaje y lo devuelve como eco
            if (proceso->identificador == 2)
            {
                Tuberia eco = recepcion;
                eco.intermedio.tipo = RECEPCION;
                strcpy(eco.intermedio.peticion, "Eco desde Hijo: mensaje recibido OK");
                printf("[HIJO]       Enviando eco → \"%s\"\n", eco.intermedio.peticion);
                write(proceso->C[1], &eco, sizeof(Tuberia));
            }
        }
        // ── RECEPCION ────────────────────────────────────────────
        else
        {
            receptora = recepcion;
            printf("[PROCESO %d] RECEPCION recibida: \"%s\"\n",
                   proceso->identificador, recepcion.intermedio.peticion);

            // TEST: El padre confirma que llegó el eco e inicia el apagado
            if (proceso->identificador == 0)
            {
                printf("[PADRE]      Eco confirmado por el hijo...\n");
                // recepcion.intermedio.tipo = SALIDA;
                // write(proceso->A[1], &recepcion, sizeof(Tuberia));
                // // Tras escribir SALIDA en A, volvemos al read(D[0]) y
                // // esperamos que la cascada nos devuelva SALIDA por D.
            }
        }

        // El interceptor reenvía mensajes normales (ENVIO/RECEPCION) hacia adelante
        if (pipe_escritura != -1)
            write(pipe_escritura, &recepcion, sizeof(Tuberia));
    }

    return NULL;
}

// ─────────────────────────────────────────────────────────────────
//  PROCESO 0 — PADRE  (Interfaz / GUI)
//  Tuberias: escribe en A, lee en D
// ─────────────────────────────────────────────────────────────────
void process0(Tuberia *padre)
{
    close(padre->C[0]); close(padre->C[1]); // Cerramos C
    close(padre->B[0]); close(padre->B[1]); // Cerramos B
    close(padre->A[0]); // No lee en A
    close(padre->D[1]); // No escribe en D

    // Arrancamos el hilo que escucha D (respuestas del hijo)
    pthread_t t_lector_D;
    pthread_create(&t_lector_D, NULL, hilo_lector, padre);

    // ── TEST: enviar mensaje al hijo ─────────────────────────────
    sleep(1); // Damos tiempo a que los otros procesos estén listos
    Tuberia prueba = *padre;
    prueba.intermedio.tipo = ENVIO;
    strcpy(prueba.intermedio.peticion, "Hola desde Padre");
    printf("[PADRE]      Enviando ENVIO → \"%s\"\n", prueba.intermedio.peticion);
    write(padre->A[1], &prueba, sizeof(Tuberia));
    
    sleep(1); // Damos tiempo a que los otros procesos estén listos
    prueba.intermedio.tipo = SALIDA;
    strcpy(prueba.intermedio.peticion, "Buenas noches Hijo");
    printf("[PADRE]      Enviando SALIDA → \"%s\"\n", prueba.intermedio.peticion);
    write(padre->A[1], &prueba, sizeof(Tuberia));
    // ─────────────────────────────────────────────────────────────

    pthread_join(t_lector_D, NULL); // Espera hasta recibir SALIDA de vuelta

    close(padre->A[1]);
    close(padre->D[0]);
    printf("[PADRE]      Pipes cerrados. Proceso 0 finalizado.\n");
}

// ─────────────────────────────────────────────────────────────────
//  PROCESO 1 — INTERCEPTOR  (puente entre padre e hijo)
//  Lee en A, escribe en B  |  Lee en C, escribe en D
// ─────────────────────────────────────────────────────────────────
void process1(Tuberia *inter)
{
    close(inter->A[1]); // No escribe en A
    close(inter->B[0]); // No lee en B
    close(inter->C[1]); // No escribe en C
    close(inter->D[0]); // No lee en D

    Tuberia copia = *inter;
    copia.flag = true; // Segundo hilo: flujo C → D

    pthread_t t_lector_A, t_lector_C;
    pthread_create(&t_lector_A, NULL, hilo_lector, inter);  // Puente A → B
    pthread_create(&t_lector_C, NULL, hilo_lector, &copia); // Puente C → D

    pthread_join(t_lector_A, NULL);
    pthread_join(t_lector_C, NULL);

    close(inter->A[0]); close(inter->B[1]);
    close(inter->C[0]); close(inter->D[1]);
    printf("[INTERCEPTOR] Pipes cerrados. Proceso 1 finalizado.\n");
}

// ─────────────────────────────────────────────────────────────────
//  PROCESO 2 — HIJO  (conexión P2P / red)
//  Tuberias: lee en B, escribe en C
// ─────────────────────────────────────────────────────────────────
void process2(Tuberia *hijo)
{
    close(hijo->A[0]); close(hijo->A[1]);
    close(hijo->D[0]); close(hijo->D[1]);
    close(hijo->B[1]); // No escribe en B
    close(hijo->C[0]); // No lee en C

    pthread_t t_lector_B;
    pthread_create(&t_lector_B, NULL, hilo_lector, hijo);

    pthread_join(t_lector_B, NULL);

    close(hijo->B[0]);
    close(hijo->C[1]);
    printf("[HIJO]        Pipes cerrados. Proceso 2 finalizado.\n");
}