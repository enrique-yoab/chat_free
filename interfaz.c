#include <gtk/gtk.h>
#include <string.h>
#define PUERTO 3000
// GTK_ALIGN_START -> Arriba/Izquierda
// GTK_ALIGN_END   -> Abajo/Derecha

GtkTextBuffer *buffer_chat; // Buffer para que se almacene el chat sobre la interfaz
GtkWidget *caja_mensajes; // Este es la caja que almacena las demas cajas de cada mensaje.
char contenido[1024];
int conectado = 0;

static void añadir_burbuja(const char *texto, gboolean es_mio) {
    GtkWidget *burbuja = gtk_label_new(texto);

    // 1. Permitir que el texto salte de línea automáticamente
    gtk_label_set_line_wrap(GTK_LABEL(burbuja), TRUE);

    // 2. Definir cómo debe romper las palabras (por palabra, no por letra)
    gtk_label_set_line_wrap_mode(GTK_LABEL(burbuja), PANGO_WRAP_WORD_CHAR);

    // 3. Limitar el ancho máximo (opcional pero recomendado)
    // Esto evita que la burbuja toque los dos bordes de la ventana a la vez
    gtk_widget_set_size_request(burbuja, -1, -1); // Altura automática
    gtk_label_set_max_width_chars(GTK_LABEL(burbuja), 50); // Ancho máximo en caracteres
    
    GtkWidget *alineacion = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    // Estilo básico de la burbuja
    gtk_widget_set_margin_start(burbuja, 10);
    gtk_widget_set_margin_end(burbuja, 10);
    gtk_widget_set_margin_top(burbuja, 5);
    gtk_widget_set_margin_bottom(burbuja, 5);

    if (es_mio) {
        // Si lo envío yo, lo pego a la derecha (END)
        gtk_label_set_xalign(GTK_LABEL(burbuja), 1.0); // 0.0 es izquierda, 1.0 es derecha
        gtk_widget_set_halign(alineacion, GTK_ALIGN_END);
        gtk_widget_set_name(burbuja, "mi-mensaje"); // Para el CSS
    } else {
        // Si lo recibo, lo pego a la izquierda (START)
        gtk_label_set_xalign(GTK_LABEL(burbuja), 0.0); // 0.0 es izquierda, 1.0 es derecha
        gtk_widget_set_halign(alineacion, GTK_ALIGN_START);
        gtk_widget_set_name(burbuja, "su-mensaje"); // Para el CSS
    }

    gtk_container_add(GTK_CONTAINER(alineacion), burbuja);
    gtk_box_pack_start(GTK_BOX(caja_mensajes), alineacion, FALSE, FALSE, 0);

    // Importante: Mostrar el nuevo mensaje
    gtk_widget_show_all(alineacion);
}

static void on_button_clicked(GtkWidget *widget, gpointer data) {
    GtkEntry *entry = GTK_ENTRY(data);
    const char *texto = gtk_entry_get_text(entry);
    
    if (strlen(texto) > 0) {
        añadir_burbuja(texto, TRUE); // TRUE porque lo envío yo
        gtk_entry_set_text(entry, "");
    }
}


void cargar_css(void) {
    GtkCssProvider *provider;
    GdkDisplay *display;
    GdkScreen *screen;

    // 1. Creamos el proveedor de CSS
    provider = gtk_css_provider_new();
    display = gdk_display_get_default();
    screen = gdk_display_get_default_screen(display);

    // 2. Cargamos el archivo .css
    // Si hay un error en el CSS, se imprimirá en la terminal
    gtk_css_provider_load_from_path(provider, "./styles/v-boton.css", NULL);

    // 3. Aplicamos el CSS a la pantalla para que afecte a todos los widgets del programa
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(provider);
}

int main(int argc, char *argv[]) {
    GtkWidget *window; // Declaramos la ventana donde se dibujara todo
    GtkWidget *button; // Declaramos un boton
    GtkWidget *box; // Declaramos una caja
    GtkWidget *entry; // Declaramos el cuadro de texto

    gtk_init(&argc, &argv);
    cargar_css();

    // Crear la ventana
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Messenger 2008 Style");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 720);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // 1. Caja principal (Vertical)
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), box);

    // --- NUEVA SECCIÓN: ENCABEZADO DE CONTACTO ---
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(header_box, "header-messenger"); // Para el CSS
    gtk_box_pack_start(GTK_BOX(box), header_box, FALSE, FALSE, 5);

    // Etiqueta para el nombre y puerto
    // Aquí puedes usar marcado Pango (como HTML simple) para poner negritas
    GtkWidget *info_contacto = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(info_contacto), 
        "<span font='12' weight='bold'>Amigo P2P</span>\n<span font='11' foreground='#000000'>Puerto: 3000 - Conectado</span>");
    gtk_label_set_xalign(GTK_LABEL(info_contacto), 0.0); // Alinear texto a la izquierda
    gtk_widget_set_margin_start(info_contacto, 15);
    
    gtk_box_pack_start(GTK_BOX(header_box), info_contacto, TRUE, TRUE, 5);
    // ---------------------------------------------

    // Luego sigue tu código del scroll...
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(box), scrolled_window, TRUE, TRUE, 0);
    gtk_widget_set_size_request(scrolled_window, -1, 500);

    // 2. La caja que guardará las burbujas
    caja_mensajes = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(scrolled_window), caja_mensajes);

    // 2. Crear el TextView (el área de texto)
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE); // No queremos que el usuario borre el historial
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD); // Ajuste de línea automático
    
    // 3. Obtener el buffer del TextView y guardarlo en la global
    buffer_chat = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    // 2. NUEVA Sub-caja para el área de escritura (Horizontal)
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); // 5px de espacio entre input y botón
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 10); // Metemos la sub-caja en la caja principal

    // 3. Crear el cuadro de texto (Entry)
    entry = gtk_entry_new();
    g_signal_connect(entry, "activate", G_CALLBACK(on_button_clicked), entry);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Escribe un mensaje aquí...");
    gtk_widget_set_size_request(entry, 300, 70);

    // Lo metemos en la caja HORIZONTAL (hbox)
    // Usamos TRUE en expand para que el cuadro de texto ocupe el espacio sobrante
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    // 4. Crear el botón
    button = gtk_button_new_with_label("Enviar");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), entry);
    gtk_widget_set_size_request(button, 100, 70);

    // Lo metemos en la caja HORIZONTAL (hbox) al lado del entry
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}