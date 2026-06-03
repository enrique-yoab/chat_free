#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/time.h>
#include "header.h"

// Variable que controla el ciclo de vida de los hilos de la conexion P2P
volatile int encendido = 1;
// Variable que controla el ciclo de vida de los hilos de los pipes
volatile int activo = 1;
// Variable que controla la accion de escribir por la Red (socket)
volatile int escribe = 0;
// Variable que controla la accion de escribir para la UI (pipes)
volatile int regresa = 0;

int PUERTO = 4000;              // El puerto que usamos nosotros
char DIRECTION[] = "127.0.0.1"; // No se cambia la ip de nuestra compu

// Mensaje que se recibe por la Interfaz.
Mensaje interfaz;
// Mensaje que se recibe por el Socket.
Mensaje externo;

// Funcion que se encarga de crear los 4 hilos de la conexion P2P
void conexion(Tuberia *local)
{
    // Se crean 4 variables tipo hilo
    pthread_t servidor, escritor, pipe_lector, pipe_escritor;

    printf("[HIJO] Inicializando los 4 hilos (Sockets y Pipes)...\n");
    // Creamos el hilo servidor (P2P)
    pthread_create(&servidor, NULL, hilo_lector_p2p, NULL);
    // Creamos el hilo cliente (P2P)
    pthread_create(&escritor, NULL, hilo_escritor_p2p, NULL);
    // Creamos el hilo lector (pipe B)
    pthread_create(&pipe_lector, NULL, hilo_lector_pipe, (void *)local);
    // Creamos el hilo escritor (pipe A)
    pthread_create(&pipe_escritor, NULL, hilo_escritor_pipe, (void *)local);

    // Esperamos a los 4 hilos creados para finalizar correctamente en orden
    pthread_join(servidor, NULL);
    pthread_join(escritor, NULL);
    pthread_join(pipe_lector, NULL);
    pthread_join(pipe_escritor, NULL);

    printf("[HIJO] Todos los hilos destruidos de forma segura. Saliendo.\n");
}

// Hilo que se encarga de escuchar mensajes por el socket y nuestro PUERTO
void *hilo_lector_p2p(void *arg)
{
    // Descriptores de archivo: server_fd para la escucha y new_socket para la conexion aceptada
    int server_fd, new_socket;

    // Estructura de red que almacenara los datos de direccionamiento IP y Puerto del servidor
    struct sockaddr_in address;

    // Variable de configuracion para habilitar opciones en el nivel de socket
    int opt = 1;

    // Estructura local donde se desempaquetara el mensaje binario recibido por la red
    Mensaje msg_recibido;

    // Intentamos crear un socket de flujo (TCP) utilizando el protocolo de internet IPv4
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        // Si la creacion del socket falla, terminamos el hilo de forma segura retornando NULL
        pthread_exit(NULL);
    }

    // Permite reutilizar el puerto inmediatamente si la aplicacion se reinicia, evitando el error "Address already in use"
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Indicamos que utilizaremos la familia de direcciones IPv4
    address.sin_family = AF_INET;

    // Vinculamos el servidor a cualquier interfaz de red disponible en el equipo (0.0.0.0)
    address.sin_addr.s_addr = INADDR_ANY;

    // Asignamos el puerto de escucha (4000) convirtiendolo al formato de red (Big Endian) con htons
    address.sin_port = htons(PUERTO);

    // Asociamos la configuracion de la IP y el PUERTO al descriptor de archivo del socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        // Si el puerto ya esta ocupado o falla el enlace, cerramos el descriptor y salimos del hilo
        close(server_fd);
        pthread_exit(NULL);
    }

    // Configuramos el socket en modo pasivo, permitiendo una cola de hasta 5 conexiones en espera
    if (listen(server_fd, 5) < 0)
    {
        // Si el sistema operativo no puede asignar la cola, liberamos el recurso y salimos
        close(server_fd);
        pthread_exit(NULL);
    }

    // Ciclo de vida del hilo servidor (P2P); se ejecutara activamente mientras la bandera global sea verdadera
    while (encendido)
    {
        // Obtenemos el tamaño de la estructura de direccionamiento para la llamada accept
        int tam_addr = sizeof(address);

        // Bloquea el hilo aqui hasta que un nodo remoto intente conectarse, generando un nuevo socket especifico
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&tam_addr);

        // Si la conexion se establecio correctamente y el descriptor es valido
        if (new_socket >= 0)
        {
            // Limpiamos la memoria del buffer local de recepcion para evitar basura de datos anteriores
            memset(&msg_recibido, 0, sizeof(Mensaje));

            // Leemos el flujo binario proveniente del socket remoto y lo mapeamos directo en la estructura Mensaje
            read(new_socket, &msg_recibido, sizeof(Mensaje));

            // Si el id_emisor es -1, significa que el nodo remoto o la interfaz local envio un evento de cierre
            if (msg_recibido.id_emisor == -1)
            {
                // Desplegamos en consola el evento global de finalizacion del programa
                printf("[GLOBAL] Evento de cierre... Cerrando hilos...\n");

                // Apagamos las banderas de control del backend para romper el ciclo de los demas hilos
                encendido = 0;
                activo = 0;

                // Cerramos de forma segura las conexiones
                close(new_socket);

                // Rompemos el ciclo infinito de escucha activa
                break;
            }
            // Si el mensaje es ordinario (contiene texto de un chat activo)
            else
            {
                // Informamos que se recibio con exito un paquete de datos en el puerto local configurado
                printf("Hilo recibio mensaje en %d...\n", PUERTO);

                // Forzamos el tipo de mensaje a RECEPCION para que la GUI sepa que debe pintarlo a la izquierda
                msg_recibido.tipo = RECEPCION;

                // Copiamos el contenido de forma segura a la variable global compartida 'externo'
                memcpy(&externo, &msg_recibido, sizeof(Mensaje));

                // Activamos la bandera volatil para indicarle al hilo escritor del pipe que hay datos listos
                regresa = 1;
            }
            // Cerramos el socket de la sesion actual para liberar el descriptor una vez procesado el mensaje
            close(new_socket);
        }
    }
    // Al salir del ciclo principal, cerramos definitivamente el socket maestro del servidor
    close(server_fd);

    // Terminamos la ejecucion del hilo de forma limpia ante el entorno de pthreads
    pthread_exit(NULL);
}

// Hilo que se encarga de escribir a un PUERTO e IP enviado por la interfaz
void *hilo_escritor_p2p(void *arg)
{
    // Ciclo de vida del hilo cliente (P2P); se ejecutara activamente mientras la bandera global sea verdadera
    while (encendido)
    {
        // Si la bandera cambia es que recibio un mensaje de la interfaz y debe enviarlo al p2p amigo
        if (escribe == 1)
        {
            // Se desactiva la bandera ya que solo debe realizarse una vez.
            escribe = 0;
            // Envia el mensaje que ya se cargo en la estructura de Mensaje interfaz;
            enviar_msj(interfaz.ip_destino, interfaz.puerto_destino, interfaz);
            // Imprime en pantalla que envio un mensaje (Depuracion)
            printf("Hilo envio mensaje a %d...\n", interfaz.puerto_destino);
        }
        // Al ser un evento volatil se debe dormir considerablemente para evitar el consumo excesivo del CPU
        usleep(1000);
    }
    // Sale del hilo para terminar la ejecucion.
    pthread_exit(NULL);
}

// Hilo que se encarga de interceptar el mensaje que envia la interfaz grafica por el pipe B
void *hilo_lector_pipe(void *arg)
{
    // El hilo tiene como entrada la estructura de Tuberia que tiene los pipes a utilizar
    Tuberia *local = (Tuberia *)arg;
    // Variable para almacenar el mensaje y despues copiarlo a la variable volatil interfaz
    Mensaje msg_pipe;

    // Ciclo de vida del hilo lector (pipe B); Se ejecuta activamente mientras la bandera global sea verdadera.
    while (activo)
    {
        // Al ser un evento bloqueante se queda esperando hasta recibir algo de la interfaz
        int bytes = read(local->B[0], &msg_pipe, sizeof(Mensaje));
        // Si el mensaje si contiene algo y sigue activo la bandera global evalua el mensaje recibido.
        if (bytes > 0 && activo)
        {
            // Si el id del mensaje es -1 significa que es un eveno de cierre y debe avisar a la conexion P2P
            if (msg_pipe.id_emisor == -1)
            {
                // Inyecta el mensaje a si mismo para que lo procese
                enviar_msj(DIRECTION, PUERTO, msg_pipe);
                // Rompemos el ciclo ya que se inyecto el mensaje de cierre y la conexion cierra los demas hilos
                break;
            }
            else // Si el id no es -1 es del usuario y debemos activar al cliente de la conexion P2P
            {
                // Muestra un mensaje en la interfaz de que se recibio un mensaje del usuario
                printf("Recibi Mensaje de la GUI...\n");
                // Copiamos el mensaje recibido a la variable global interfaz
                memcpy(&interfaz, &msg_pipe, sizeof(Mensaje));
                // Acivamos la bandera de escritura para el P2P (cliente)
                escribe = 1;
            }
        }
    }
    pthread_exit(NULL);
}

// Hilo que se encarga de escribir hacia la interfaz grafica por el pipe A
void *hilo_escritor_pipe(void *arg)
{
    // El hilo tiene como entrada la estructura de Tuberia que tiene los pipes a utilizar
    Tuberia *local = (Tuberia *)arg;
    // Ciclo de vida del hilo escritor (pipe A); Se ejecuta activamente mientras la bandera global sea verdadera.
    while (activo)
    {
        // Si la bandera cambio significa que la conexion P2P (servidor) recibio un mensaje por lo que debemos enviarlo a la interfaz por el pipe
        if (regresa == 1)
        {
            // Desactivamos la bandera ya que solo debe realizarse una vez
            regresa = 0;
            // Escribe a la tuberia unicamente (pipe A)
            write(local->A[1], &externo, sizeof(Mensaje));
        }
        // Al ser un evento volatil se debe dormir considerablemente para evitar el consumo excesivo del CPU
        usleep(1000);
    }
    pthread_exit(NULL);
}

// Funcion para enviar el Mensaje a una IP y PUERTO especifico (Cliente TCP).
void enviar_msj(char *ip_destino, int puerto_destino, Mensaje msg)
{
    // Descriptor de archivo para el socket local que iniciará la conexión.
    int sock = 0;
    
    // Estructura de red que almacenará la dirección IP y el Puerto del nodo remoto (destino).
    struct sockaddr_in serv_addr;

    // Intentamos crear un socket de flujo activo utilizando el protocolo TCP/IP bajo IPv4.
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        // Si el sistema operativo no puede asignar el socket, salimos de la función inmediatamente.
        return;

    // Convertimos el puerto destino de formato de host a formato de red (Big Endian) usando htons.
    serv_addr.sin_port = htons(puerto_destino);
    
    // Especificamos que la dirección de destino pertenece a la familia IPv4.
    serv_addr.sin_family = AF_INET;

    // Convertimos la IP de texto (ej. "127.0.0.1") a su representación binaria de red.
    if (inet_pton(AF_INET, ip_destino, &serv_addr.sin_addr) <= 0)
    {
        // Si la IP de destino es inválida o el formato es incorrecto, cerramos el socket y salimos.
        close(sock);
        return;
    }

    // Intentamos establecer una conexión activa de tres vías (Three-way handshake) con el nodo remoto.
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0)
    {
        // Si la conexión es exitosa, enviamos la estructura completa 'Mensaje' de manera directa por el flujo del socket.
        send(sock, &msg, sizeof(Mensaje), 0);
    }

    // Cerramos el socket una vez terminada la transferencia (o si falló la conexión) para liberar el descriptor.
    close(sock);
}

// Función para obtener el tiempo en milisegundos
long long get_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}

// Función para transformar ms a HH:MM:SS:ms
void formatear_tiempo_str(long long tiempo_ms, char *buffer_salida)
{
    time_t segundos = tiempo_ms / 1000;
    int milisegundos = tiempo_ms % 1000;
    struct tm *tm_info = localtime(&segundos);

    sprintf(buffer_salida, "%02d:%02d:%02d:%03d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, milisegundos);
}