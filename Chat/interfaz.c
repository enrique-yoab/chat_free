#include <gtk/gtk.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include "header.h"

#define TAM_MAX 1024

// Variables de control exclusivas para los hilos de la GUI
volatile int gui_activo = 1;
volatile int gui_escribe = 0;

// Buffers para la sincronización de la GUI
Mensaje msg_para_enviar;
Mensaje msg_recibido_pipe;

// Datos de la conexion del amigo
int puerto_amigo = 4001;
char dir_amigo[] = "127.0.0.1";
int puerto_actual = 4000; 

GtkWidget *caja_mensajes; 
GtkAdjustment *adj_chat; 
char buffer[TAM_MAX]; 

Tuberia *gui_pipe_ref;

static gboolean scroll_al_final(gpointer data) {
    double max = gtk_adjustment_get_upper(adj_chat);
    double page = gtk_adjustment_get_page_size(adj_chat);
    gtk_adjustment_set_value(adj_chat, max - page);
    return FALSE; 
}

static void añadir_burbuja(const char *texto, gboolean es_mio) {
    GtkWidget *burbuja = gtk_label_new(texto);
    gtk_label_set_line_wrap(GTK_LABEL(burbuja), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(burbuja), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(burbuja), 40);
    
    GtkWidget *alineacion = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start(burbuja, 15);
    gtk_widget_set_margin_end(burbuja, 15);
    gtk_widget_set_margin_top(burbuja, 5);
    gtk_widget_set_margin_bottom(burbuja, 5);

    if (es_mio) {
        gtk_widget_set_halign(alineacion, GTK_ALIGN_END);
        gtk_widget_set_name(burbuja, "mi-mensaje");
    } else {
        gtk_widget_set_halign(alineacion, GTK_ALIGN_START);
        gtk_widget_set_name(burbuja, "su-mensaje");
    }

    gtk_container_add(GTK_CONTAINER(alineacion), burbuja);
    gtk_box_pack_start(GTK_BOX(caja_mensajes), alineacion, FALSE, FALSE, 0);
    gtk_widget_show_all(alineacion);

    g_idle_add(scroll_al_final, NULL);
}

// Función auxiliar que ejecuta GTK de forma segura para pintar la burbuja remota
static gboolean mostrar_mensaje_recibido(gpointer data) {
    añadir_burbuja(msg_recibido_pipe.texto, FALSE);
    return FALSE; // Se ejecuta una sola vez
}

// === HILO 1 DE LA GUI: Lector dedicado del Pipe A ===
void *hilo_lector_gui(void *arg) {
    Tuberia *local = (Tuberia *)arg;
    printf("[GUI-HILO-LECTOR] Hilo de lectura de pipe iniciado...\n");

    while (gui_activo) {
        Mensaje temporal;
        // Se queda bloqueado aquí de forma segura sin congelar la ventana de GTK
        int bytes = read(local->A[0], &temporal, sizeof(Mensaje));

        if (bytes > 0 && gui_activo) {
            if (temporal.tipo == RECEPCION) {
                printf("Recibi mensaje de la conexion...\n");
                // Copiamos al buffer interno de la GUI
                memcpy(&msg_recibido_pipe, &temporal, sizeof(Mensaje));
                // Le pedimos a GTK que pinte de forma segura desde su hilo principal
                g_idle_add(mostrar_mensaje_recibido, NULL);
            }
        } else {
            break;
        }
    }
    printf("[GUI-HILO-LECTOR] Saliendo del hilo lector de la interfaz.\n");
    pthread_exit(NULL);
}

// === HILO 2 DE LA GUI: Escritor dedicado en el Pipe B ===
void *hilo_escritor_gui(void *arg) {
    Tuberia *local = (Tuberia *)arg;
    printf("[GUI-HILO-ESCRITOR] Hilo de escritura de pipe iniciado...\n");

    while (gui_activo) {
        if (gui_escribe == 1) {
            write(local->B[1], &msg_para_enviar, sizeof(Mensaje));
            gui_escribe = 0; // Apagamos la bandera tras escribir
        }
        usleep(10000); // Evita consumo innecesario de CPU (10ms)
    }
    printf("[GUI-HILO-ESCRITOR] Saliendo del hilo escritor de la interfaz.\n");
    pthread_exit(NULL);
}

static void boton_pulsado(GtkWidget *widget, gpointer data) {
    GtkEntry *entry = GTK_ENTRY(data);
    const char *texto = gtk_entry_get_text(entry);
    
    if (strlen(texto) > 0) {
        añadir_burbuja(texto, TRUE); 
        
        // Colocamos los datos en el buffer de envío de la GUI
        msg_para_enviar.id_emisor = getpid();
        msg_para_enviar.tipo = ENVIO;
        strncpy(msg_para_enviar.texto, texto, sizeof(msg_para_enviar.texto) - 1);
        strncpy(msg_para_enviar.ip_destino, dir_amigo, sizeof(msg_para_enviar.ip_destino) - 1);
        msg_para_enviar.puerto_destino = puerto_amigo;
        
        // Activamos la bandera para que el hilo escritor de la GUI despache el mensaje
        gui_escribe = 1;
        
        gtk_entry_set_text(entry, ""); 
    }
}

static void ventana_destruida(GtkWidget *widget, gpointer data) {
    printf("[GUI] Cerrando ventana. Enviando señal de apagado...\n");
    
    msg_para_enviar.id_emisor = -1; 
    msg_para_enviar.tipo = SALIDA;
    strncpy(msg_para_enviar.texto, "Cierre de interfaz.", sizeof(msg_para_enviar.texto) - 1);

    write(gui_pipe_ref->B[1], &msg_para_enviar, sizeof(Mensaje));
    
    gui_escribe = 0; // <-- Asegura que el escritor de la GUI no intente escribir duplicado
    gui_activo = 0;  // <-- Apaga los hilos de la interfaz de inmediato
    gtk_main_quit();
}

void cargar_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gtk_css_provider_load_from_path(provider, "v-boton.css", NULL);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

void interfaz_grafica(int argc, char *argv[], Tuberia *padre) {
    GtkWidget *window, *box, *entry, *scrolled_window, *hbox, *button, *header_box, *info_contacto;
    pthread_t gui_lector, gui_escritor;

    gui_pipe_ref = padre;

    gtk_init(&argc, &argv);
    cargar_css();

    // --- LEVANTAR LOS HILOS DE LA INTERFAZ para comunicarse con el back-end ---
    pthread_create(&gui_lector, NULL, hilo_lector_gui, (void *)gui_pipe_ref);
    pthread_create(&gui_escritor, NULL, hilo_escritor_gui, (void *)gui_pipe_ref);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "MSN P2P - INTERFAZ");
    gtk_window_set_default_size(GTK_WINDOW(window), 450, 650);
    
    g_signal_connect(window, "destroy", G_CALLBACK(ventana_destruida), NULL);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), box);

    header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(header_box, "header-messenger"); 
    gtk_box_pack_start(GTK_BOX(box), header_box, FALSE, FALSE, 0);
    info_contacto = gtk_label_new(NULL);
    
    snprintf(buffer, sizeof(buffer), 
        "<span font='11' weight='bold' foreground='#003399'>MESSENGER P2P</span>\n"
        "<span font='9' foreground='#666'>Escucha: %d - Conectado</span>", puerto_actual);

    gtk_label_set_markup(GTK_LABEL(info_contacto), buffer);
    gtk_box_pack_start(GTK_BOX(header_box), info_contacto, FALSE, FALSE, 15);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(box), scrolled_window, TRUE, TRUE, 0);
    adj_chat = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));

    caja_mensajes = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(caja_mensajes, GTK_ALIGN_END); 
    gtk_container_add(GTK_CONTAINER(scrolled_window), caja_mensajes);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(hbox, 10);
    gtk_widget_set_margin_end(hbox, 10);
    gtk_widget_set_margin_top(hbox, 10);
    gtk_widget_set_margin_bottom(hbox, 10);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    g_signal_connect(entry, "activate", G_CALLBACK(boton_pulsado), entry);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    button = gtk_button_new_with_label("Enviar");
    g_signal_connect(button, "clicked", G_CALLBACK(boton_pulsado), entry);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    // Sincronizar hilos al cerrar la aplicación
    pthread_join(gui_lector, NULL);
    pthread_join(gui_escritor, NULL);
}