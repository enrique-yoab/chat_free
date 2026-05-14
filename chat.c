#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include "conexion.h"

Tuberia global;

int main() {
    // 1. Inicializamos las tuberías antes de los forks
    pipe(global.A); 
    pipe(global.B); 
    pipe(global.C); 
    pipe(global.D);
    global.flag = false;
    // 2. Creamos el proceso INTERCEPTOR (seccion critica por eso es primero)
    pid_t pid_interceptor = fork();
    if (pid_interceptor == 0) {
        // Estamos dentro del proceso Intermediario
        global.identificador = 1; // identificador de proceso entre tuberias
        process1(&global);
        exit(0); // Importante: salir para no seguir haciendo forks
    }

    // 3. Creamos el proceso HIJO (Conexión)
    pid_t pid_hijo = fork();
    if (pid_hijo == 0) {
        // Estamos dentro del proceso Hijo
        global.identificador = 2; // identificador de proceso entre tuberias
        process2(&global);
        exit(0);
    }

    // 4. El proceso original se queda como el PADRE (Interfaz)
    global.identificador = 0; // identificador de proceso entre tuberias
    process0(&global);

    // El Padre ya salió de process0 porque recibió la confirmación de SALIDA
    waitpid(pid_interceptor, NULL, 0); // Espera que el Interceptor muera
    waitpid(pid_hijo, NULL, 0);        // Espera que el Hijo muera

    printf("Sistema P2P Messenger: Todos los procesos finalizados correctamente.\n");
    return 0;
}