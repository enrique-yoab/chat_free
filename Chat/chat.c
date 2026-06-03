#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "header.h"

// Tuberia que se usa para comunicar los dos procesos
// pipe A y pipe B
Tuberia principal;

int main(int argc, char *argv[])
{
    // Creamos las dos tuberias a utilizar durante la ejecución
    if (pipe(principal.B) < 0 || pipe(principal.A) < 0)
    {
        perror("Error al crear los pipes");
        return 1;
    }
    // Creamos un fork para levantar la conexion p2p
    pid_t process = fork();
    // Si hubo algun error se cancela todo
    if (process < 0)
        return 1;
    // De lo contrario el proceso hijo se encarga de la conexion P2P
    if (process == 0)
    {
        // El hijo cierra los extremos no usados de los pipes
        close(principal.A[0]); close(principal.B[1]);
        // Llama la funcion que levanta la conexion P2P
        conexion(&principal);
        // Cierra los extremos usados para terminar exitosamente
        close(principal.A[1]); close(principal.B[0]);

        exit(0);
    }
    else // El proceso padre se encarga de la interfaz
    {
        // Se duerme 2 segundos para levantar el back-end
        sleep(2);
        // El padre cierra los extremos que no utiliza de los pipes
        close(principal.A[1]); close(principal.B[0]);
        // Llama la funcion que crea la interfaz grafica
        interfaz_grafica(argc, argv, &principal);
        // Cierra los extremos usados para terminar
        close(principal.A[0]); close(principal.B[1]);
        // Esperamos al proceso hijo para terminar exitosamente
        wait(NULL);
    }
    return 0;
}