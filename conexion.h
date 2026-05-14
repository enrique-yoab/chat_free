#ifndef CONEXION_H
#define CONEXION_H

// Ester archivo conecta a todos los demas codigos.
// Declara estructuras, y funciones necesarias para la comunicacion entre procesos y con el peer.

#include <stdbool.h>

#define TAM_MAX 1024    // Cantidad maxima de caracteres en el mensaje.

// ----- Tipo de mensaje -----
typedef enum {
    ENVIO,      // Enviar mensaje al peer conectado.
    RECEPCION,  // Mensaje recibido del peer conectado.
    SALIDA      // Apagado global.
} TipoMsj;

// ----- Tiempo físico para determinar el tiempo entre maquinas -----
typedef struct {
    int horas;
    int minutos;
    int segundos;
    int milisegundos;
} tiempo;

// ----- Mensaje que realmente se transmite -----
typedef struct {
    TipoMsj tipo; // 
    int id_computadora;    // 0 = comando interno, > 0 = id del peer
    char name_user[TAM_MAX];
    char peticion[TAM_MAX];
    tiempo envio;
    tiempo llegada;
    tiempo respuesta;
} Mensaje;

// ------ Mensaje que se transmite entre los procesos diferente al mensaje principal ------
typedef struct {
    int identificador; // 0 = Padre, 1 = Intermediario, 2 = Hijo
    Mensaje intermedio; // Mensaje que se transmite entre el p2p y la GUI.
    bool flag; // Bandera para el intermediario
    int A[2]; // Tuberia -> Padre a Interceptor
    int B[2]; // Tuberia -> Interceptor a hijo
    int C[2]; // Tuberia -> Hijo a Interceptor
    int D[2]; // Tuberia -> Inerceptor a Padre
} Tuberia;   // Tuberia[2] -> [lectura, escritura]

// ----- Variables exportadas (necesarias en el proceso padre) -----
extern int PUERTO1;
extern char *ip_emisora;

// ----- Declaraciones -----
void conexion_p2p(int port2, int id, char *ip, char *name_user);
void enviar_msj(int puerto_destino, char *ip, Mensaje datos);
void inyectar_mensaje(Mensaje *datos);
char *convertir_hora(tiempo *t);
long long convertion_ms(tiempo *t);
void convertir_tiempo(long long ms, tiempo *t);
void capturar_tiempo(tiempo *t);
void process0(Tuberia *padre);
void process1(Tuberia *inter);
void process2(Tuberia *hijo);
void *hijo_lector(void *arg);
void *hijo_escritor(void *arg);
void avisar(Tuberia interceptada);

#endif