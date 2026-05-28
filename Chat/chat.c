#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "header.h"

Tuberia principal;

int main(int argc, char *argv[])
{
    if (pipe(principal.B) < 0 || pipe(principal.A) < 0) {
        perror("Error al crear los pipes");
        return 1;
    }

    pid_t process = fork();
    if (process < 0) return 1;

    if (process == 0)
    {
        close(principal.A[0]); close(principal.B[1]); 
        conexion(&principal);
        exit(0);
    }
    else 
    {
        sleep(2);
        close(principal.A[1]); close(principal.B[0]);
        interfaz_grafica(argc, argv, &principal);
        close(principal.A[0]); close(principal.B[1]);
        wait(NULL);
    }
    return 0;
}