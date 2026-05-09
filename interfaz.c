#include <gtk/gtk.h>

// Esta es la función "Callback" que se ejecuta al presionar el botón
static void on_button_clicked(GtkWidget *widget, gpointer data) {
    g_print("¡Zumbido enviado! ⚡\n");
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *box;

    gtk_init(&argc, &argv); // Inicializa GTK

    // 1. Crear la ventana principal
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "MSN Messenger 2008");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 1024);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // 2. Crear una caja vertical para organizar todo
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), box);

    // 3. Crear un Botón
    button = gtk_button_new_with_label("Enviar Zumbido");
    // Conectamos el evento "clicked" con nuestra función
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), NULL);
    
    // Meter el botón a la caja
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);

    // Mostrar todo
    gtk_widget_show_all(window);

    gtk_main(); // El programa se queda aquí esperando eventos

    return 0;
}