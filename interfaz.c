#include <gtk/gtk.h>
#include <string.h>
#include <stdbool.h>
#include "conexion.h"

GtkWidget *caja_mensajes; // Variable global para el historial del chat
GtkAdjustment *adj_chat; // Variable global para el control del scroll
Mensaje emisor; // Variable global que tendra la infromacion local
Mensaje receptor; // Variable global que tendra la infromacion de la otra computadora
char buffer[TAM_MAX]; // Buffer para formatear texto para la interfaz grafica
char *name_user = "Enrique"; // Nombre de usuario local
char *name_guest = "Mario";  // Nombre del contacto 
char *estados[] = {"Activo", "Ocupado", "Desconectado"}; // Estados de conexion
int puerto_actual = 3000; // Puerto local (pruebas)
bool user = true; // Variable para controlar el mensaje a mostrar en la interfaz grafica

// Función auxiliar para el scroll (se ejecuta cuando la interfaz está libre)
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

    // --- AUTO-SCROLL SEGURO ---
    // Usamos g_idle_add para esperar a que el widget se dibuje y luego bajar
    g_idle_add(scroll_al_final, NULL);
}

static void on_button_clicked(GtkWidget *widget, gpointer data) {
    GtkEntry *entry = GTK_ENTRY(data);
    const char *texto = gtk_entry_get_text(entry);
    
    if (strlen(texto) > 0) {
        añadir_burbuja(texto, user); // Agrega el mensaje a la interfaz gráfica
        gtk_entry_set_text(entry, "Escriba mensaje..."); // Limpia el campo de entrada
        strcpy(emisor.peticion, texto); // Guadamos el mensaje en la estructura global para enviarlo
        user = !user;
    }
}

void cargar_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gtk_css_provider_load_from_path(provider, "./styles/v-boton.css", NULL);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

int main(int argc, char *argv[]) {
    GtkWidget *window, *box, *entry, *scrolled_window, *hbox, *button, *header_box, *info_contacto;

    gtk_init(&argc, &argv);
    cargar_css();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "MSN P2P - 2008");
    gtk_window_set_default_size(GTK_WINDOW(window), 450, 650);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), box);

    // Header
    header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(header_box, "header-messenger");
    gtk_box_pack_start(GTK_BOX(box), header_box, FALSE, FALSE, 0);
    info_contacto = gtk_label_new(NULL);
    // // Usamos snprintf para mezclar las etiquetas de estilo con tus variables
    snprintf(buffer, sizeof(buffer), 
        "<span font='11' weight='bold' foreground='#003399'>%s</span>\n"
        "<span font='9' foreground='#666'>Puerto: %d - %s</span>", 
        name_guest, puerto_actual, estados[2]);

    // Ahora le pasamos el resultado a la etiqueta
    gtk_label_set_markup(GTK_LABEL(info_contacto), buffer);
    gtk_box_pack_start(GTK_BOX(header_box), info_contacto, FALSE, FALSE, 15);

    // Scroll y Caja de Mensajes
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(box), scrolled_window, TRUE, TRUE, 0);

    // IMPORTANTE: Obtenemos el ajuste vertical de la ScrolledWindow directamente aquí
    adj_chat = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));

    caja_mensajes = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(caja_mensajes, GTK_ALIGN_END); // Inicia abajo
    gtk_container_add(GTK_CONTAINER(scrolled_window), caja_mensajes);

    // Área de entrada
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    // En lugar de gtk_widget_set_margin_all(hbox, 10);
    gtk_widget_set_margin_start(hbox, 10);
    gtk_widget_set_margin_end(hbox, 10);
    gtk_widget_set_margin_top(hbox, 10);
    gtk_widget_set_margin_bottom(hbox, 10);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    g_signal_connect(entry, "activate", G_CALLBACK(on_button_clicked), entry);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_widget_set_size_request(entry, 250, 50);

    button = gtk_button_new_with_label("Enviar");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), entry);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_widget_set_size_request(button, 100, 50);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}