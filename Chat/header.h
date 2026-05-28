#ifndef HEADER_H
#define HEADER_H
#include <gtk/gtk.h>

typedef enum {
    ENVIO,
    RECEPCION,
    SALIDA
} TipoMensaje;

typedef struct {
    int id_emisor;          
    char texto[128];        
    char ip_destino[16];     
    int puerto_destino;      
    TipoMensaje tipo;        
} Mensaje;

typedef struct {
    int A[2]; // Interfaz <-- Backend (Backend escribe en A[1], Interfaz lee en A[0])
    int B[2]; // Interfaz --> Backend (Interfaz escribe en B[1], Backend lee en B[0])
} Tuberia;

// Prototipos del Backend (conexion.c)
void *hilo_lector_p2p(void *arg);
void *hilo_escritor_p2p(void *arg);
void *hilo_lector_pipe(void *arg);
void *hilo_escritor_pipe(void *arg);
void conexion(Tuberia *local);
void enviar_msj(char *ip_destino, int puerto_destino, Mensaje msg);

// Prototipos del Frontend / Interfaz (interfaz.c)
void *hilo_lector_gui(void *arg);
void *hilo_escritor_gui(void *arg);
void interfaz_grafica(int argc, char *argv[], Tuberia *padre);

#endif