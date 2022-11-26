#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <math.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define MAXX 80
#define PORT1 8080 // LIGHTBULB
#define PORT2 8081 // TV
#define PORT3 8082 // VACUUM
#define HOME "127.0.0.1"
#define SA struct sockaddr_in
int debug = 1;
int connect_to(char ip[30], int port, int *sockfd);
void disconnect_from(int option, int turnoff);
char *write_to(int sockfd, char message[100]);
char *response_handler(char response[100]);

// LIGHTBULB FUNCTIONS //
void on_lightbulb_color_button_clicked(GtkButton *b);
void show_lightbulb_frame();
void hide_lightbulb_frame();
const char *choose_color(int option);
void change_lightbulb_color(const char *color);
int color_char_to_int(const char *color);
void on_lightbulb_brightness_changed(GtkVolumeButton *volumeButton);
void change_lightbulb_label_brightness(const char *value);
void on_lightbulb_connect_clicked(GtkButton *b);
void show_tv_frame();
// LIGHTBULB FUNCTIONS //

// TV FUNCTIONS
void on_tv_connect_clicked(GtkButton *button);
void change_tv_label_brightness(const char *value);
void hide_tv_frame();
void on_tv_brightness_changed(GtkVolumeButton *volumeButton);
void on_tv_shutdown_clicked(GtkButton *button);
void change_tv_label_volume(const char *value);
void on_tv_channel_changed(GtkSpinButton *channelSpinner);
// TV FUNCTIONS

// VACUUM FUNCTIONS
void on_vacuum_connect_clicked(GtkButton *b);
void show_vacuum_frame();
void hide_vacuum_frame();
void change_vacuum_label_power(const char *value);
void change_vacuum_label_speed(const char *value);
void on_vacuum_shutdown_clicked(GtkButton *b);
void on_vacuum_select_speed_clicked(GtkRadioButton *button);
void set_vacuum_option(int option);

// VACUUM FUNCTIONS

void rearrange_labels();
void change_error_zone(const char *content);

void turnoff(char *p);
int sockfd0,
    sockfd1, sockfd2;
short connectedLightbulb, connectedTV, connectedVacuum;
struct lightbulbValues
{
    int brightness;
    enum Colors
    {
        WHITE = 1,
        RED = 2,
        ORANGE = 3,
        YELLOW = 4,
        GREEN = 5,
        BLUE = 6,
        INDIGO = 7,
        VIOLET = 8
    } currentColor;
    int sockfd;
    short connected;
} lightbulb;
struct tvValues
{
    int volume;
    int channel;    // 0-1000
    int brightness; // 0-100
    int sockfd;
    short connected;
} tv;
struct vacuumValues
{
    int speed; // 0-10
    int power; // 0-100
    int sockfd;
    short connected;

} vacuum;

// Make them global

GtkWidget *window;
GtkWidget *fixed;
GtkWidget *lightbulb_connect;
GtkWidget *lightbulb_connection_status;
GtkBuilder *builder;

int main(int argc, char *argv[])
{

    gtk_init(&argc, &argv); // init Gtk

    //---------------------------------------------------------------------
    // establish contact with xml code used to adjust widget settings
    //---------------------------------------------------------------------

    builder = gtk_builder_new_from_file("project.glade");

    window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_builder_connect_signals(builder, NULL);

    fixed = GTK_WIDGET(gtk_builder_get_object(builder, "fixed1"));
    lightbulb_connect = GTK_WIDGET(gtk_builder_get_object(builder, "lightbulb_connect"));
    lightbulb_connection_status = GTK_WIDGET(gtk_builder_get_object(builder, "lightbulb_connection_status"));

    gtk_widget_show(window);
    rearrange_labels();
    gtk_main();

    return EXIT_SUCCESS;
}

void on_lightbulb_connect_clicked(GtkButton *b)
{
    gchar *connectionXmlColor;
    if (lightbulb.connected == 0)
    {
        if (connect_to(HOME, PORT1, &lightbulb.sockfd) == 0)
        {
            show_lightbulb_frame();
            char values[100];
            sprintf(values, "Brightness: %d, Color: %d", lightbulb.brightness, lightbulb.currentColor);

            if (debug)
                change_error_zone(values);
            else
                change_error_zone("");
            // CONNECTION STATUS CHANGE
            gtk_label_set_text(GTK_LABEL(lightbulb_connection_status), (const gchar *)"Connected");
            gtk_button_set_label(GTK_BUTTON(lightbulb_connect), "Disconnect");
            connectionXmlColor = g_strdup_printf("<span foreground='green'>%s</span>", "Connected");
            gtk_label_set_markup(GTK_LABEL(lightbulb_connection_status), connectionXmlColor);
            free(connectionXmlColor);
            // CURRENT COLOR CHANGE
            change_lightbulb_color(choose_color(lightbulb.currentColor));
            // CURRENT BRIGHTNESS CHANGE
            sprintf(values, "%d", lightbulb.brightness);
            GtkWidget *brightnessSlider = GTK_WIDGET(gtk_builder_get_object(builder, "lightbulb_brightness_slider"));
            gtk_scale_button_set_value(GTK_SCALE_BUTTON(brightnessSlider), (gdouble)lightbulb.brightness);

            change_lightbulb_label_brightness(values);
            // BACKEND
            lightbulb.connected = 1;
            // SHOW
        }
        else
        {
            change_error_zone("Error: Lightbulb probably not on.");
        }
    }
    else
    {
        disconnect_from(1, 0);
    }
}

void on_lightbulb_color_button_clicked(GtkButton *b)
{
    printf("\non_lightbulb_color_button_clicked\n");
    char toBeSent[100];
    GtkWidget *const children = gtk_bin_get_child(GTK_BIN(b));
    const gchar *colorLabel = gtk_label_get_label(GTK_LABEL(children));
    sprintf(toBeSent, "setcolor %d", color_char_to_int(colorLabel));
    char *p = write_to(lightbulb.sockfd, toBeSent);
    // printf("\np in on_lightbulb_color_button_clicked: %s\n", p);
    if (strstr(p, "success") != NULL)
    {
        change_lightbulb_color(colorLabel);
        lightbulb.currentColor = color_char_to_int((const char *)colorLabel);
    }
    if (debug)
        change_error_zone(p);
    free(p);
}
int connect_to(char ip[30], int port, int *sockfd)
{
    struct sockaddr_in servaddr;

    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd == -1)
    {
        printf("socket 1 creation failed...\n");
        exit(0);
    }
    else
        printf("Socket 1 successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port);

    printf("connecting to %s:%d...\n", ip, port);
    // connect the client socket to server socket
    if (connect(*sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("connection with the server 1 failed...\n");
        return 1;
    }
    else
    {
        printf("connected to the server %s:%d..\n", ip, port);
        printf("retrieving info...\n");
        char config[MAXX];
        bzero(config, MAXX);
        read(*sockfd, config, sizeof(config));
        char *p = strtok(config, ":");
        if (strncmp(p, "lightbulb", 9) == 0)
        {
            lightbulb.connected = 1;
            printf("---LIGHTBULB---\n");
            p = strtok(NULL, ":");
            lightbulb.brightness = atoi(p);
            printf("brightness (int) : %d\n", lightbulb.brightness);
            p = strtok(NULL, ":");
            lightbulb.currentColor = atoi(p);
            printf("color(int):%d \n", lightbulb.currentColor);
        }
        else if (strncmp(p, "tv", 2) == 0)
        {
            tv.connected = 1;
            printf("---TV---\n");
            p = strtok(NULL, ":");
            tv.brightness = atoi(p);
            printf("brightness (int) : %d\n", tv.brightness);
            p = strtok(NULL, ":");
            tv.volume = atoi(p);
            printf("volume(int):%d\n", tv.volume);
            p = strtok(NULL, ":");
            tv.channel = atoi(p);
            printf("channel(int):%d\n", tv.channel);
        }
        else if (strncmp(p, "vacuum", 6) == 0)
        {
            vacuum.connected = 1;
            printf("---VACUUM---\n");
            p = strtok(NULL, ":");
            vacuum.power = atoi(p);
            printf("power (int) : %d\n", vacuum.power);
            p = strtok(NULL, ":");
            vacuum.speed = atoi(p);
            printf("speed (int):%d", vacuum.speed);
        }
        else
        {
            printf("ERROR: Unknown info??");
        }
        printf("------------\n");
        return 0;
    }
}

void disconnect_from(int option, int turnoff)
{
    // 1 - lightbulb
    // 2 - TV
    // 3 - vacuum
    gchar *connectionXmlColor;
    switch (option)
    {
    case 1:
        lightbulb.connected = 0;
        if (turnoff == 0)
        {
            write_to(lightbulb.sockfd, "exit");
            if (debug)
                change_error_zone("Lightbulb disconnected.");
        }
        else
        {
            write_to(lightbulb.sockfd, "turnoff");
            if (debug)
                change_error_zone("Lightbulb turned off.");
        }
        close(lightbulb.sockfd);
        // hide_lightbulb_frame();
        gtk_button_set_label(GTK_BUTTON(lightbulb_connect), "Connect");
        gtk_label_set_text(GTK_LABEL(lightbulb_connection_status), (const gchar *)"Disconnected");
        // gtk_label_set_text(GTK_LABEL(errorzone), (const gchar *)"");
        if (debug)
            change_error_zone("");
        connectionXmlColor = g_strdup_printf("<span foreground='white'>%s</span>", "Disconnected");
        lightbulb.connected = 0;
        hide_lightbulb_frame();
        gtk_label_set_markup(GTK_LABEL(lightbulb_connection_status), connectionXmlColor);
        free(connectionXmlColor);

        break;
    case 2:
        tv.connected = 0;
        if (turnoff == 0)
        {
            write_to(tv.sockfd, "exit");
            if (debug)
                change_error_zone("TV disconnected.");
        }
        else
        {
            write_to(tv.sockfd, "turnoff");
            if (debug)
                change_error_zone("TV turned off.");
        }
        close(tv.sockfd);
        GtkWidget *tv_connect = GTK_WIDGET(gtk_builder_get_object(builder, "tv_connect"));
        GtkWidget *tv_connection_status = GTK_WIDGET(gtk_builder_get_object(builder, "tv_connection_status"));

        gtk_button_set_label(GTK_BUTTON(tv_connect), "Connect");
        gtk_label_set_text(GTK_LABEL(tv_connection_status), (const gchar *)"Disconnected");
        // gtk_label_set_text(GTK_LABEL(errorzone), (const gchar *)"");
        connectionXmlColor = g_strdup_printf("<span foreground='white'>%s</span>", "Disconnected");
        lightbulb.connected = 0;
        hide_tv_frame();
        gtk_label_set_markup(GTK_LABEL(tv_connection_status), connectionXmlColor);
        free(connectionXmlColor);
        break;
    case 3:
        vacuum.connected = 0;
        if (turnoff == 0)
        {
            write_to(vacuum.sockfd, "exit");
            if (debug)
                change_error_zone("Vacuum disconnected.");
        }
        else
        {
            write_to(vacuum.sockfd, "turnoff");
            if (debug)
                change_error_zone("Vacuum turned off.");
        }
        close(vacuum.sockfd);
        GtkWidget *vacuum_connect = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_connect"));
        GtkWidget *vacuum_connection_status = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_connection_status"));

        gtk_button_set_label(GTK_BUTTON(vacuum_connect), "Connect");
        gtk_label_set_text(GTK_LABEL(vacuum_connection_status), (const gchar *)"Disconnected");
        // gtk_label_set_text(GTK_LABEL(errorzone), (const gchar *)"");
        connectionXmlColor = g_strdup_printf("<span foreground='white'>%s</span>", "Disconnected");
        vacuum.connected = 0;
        hide_vacuum_frame();
        gtk_label_set_markup(GTK_LABEL(vacuum_connection_status), connectionXmlColor);
        free(connectionXmlColor);
        break;
    default:
        printf("WARNING - wrong disconnect option\n");
        return;
    }
    printf("Disconnected.\n");
}

char *write_to(int sockfd, char message[100])
{
    char response[100];
    int n;

    if (write(sockfd, message, strlen(message) + 1))
        perror("write: ");
    bzero(response, sizeof(response));
    if (read(sockfd, response, sizeof(response)) == -1)
        perror("read: ");
    char *p = response_handler(response);
    if ((strncmp(response, "exit", 4)) == 0)
    {
        printf("Client Exit...\n");
    }
    return p;
}

char *response_handler(char response[100])
{
    // printf("lightbulb said : |%s|, strlen(response): %lu\n", response, strlen(response));
    // printf("RESPONSE: %s\n", response);

    // printf("length: %lu\n", strlen(response));
    // char printer[100];

    // if (strstr(response, "lightbulb") != NULL) // LIGHTBULB
    //     // IF LIGHTBULB
    //     if (strstr(response, "success") != NULL)
    //     { // LIGHTBULB,SUCCESS

    //         if (strstr(response, "light+") != NULL || strstr(response, "light-") != NULL ||
    //             strstr(response, "setBrightness") != NULL)
    //         { // LIGHTBULB,SUCCES,LIGHT+/-/setBrightness

    //             char *p = strtok(response, "@");
    //             p = strtok(NULL, "\0");
    //             strcpy(printer, "Light+/-/setBrightness, light=");
    //             strcat(printer, p);
    //             lightbulb.brightness = atoi(p);
    //         }
    //         else if (strstr(response, "setColor") != NULL)
    //         { // LIGHTBULB,SUCCESS,setColor
    //             char *p = strtok(response, "@");
    //             p = strtok(NULL, "\0");
    //             strcpy(printer, "setColor to ");
    //             strcat(printer, p);
    //             lightbulb.currentColor = atoi(p);
    //         }
    //         else
    //             strcpy(printer, response);
    //     }
    //     else
    //     { // LIGHTBULB,FAILED
    //         if (strstr(response, "failed") != NULL)
    //         {

    //             char *p = strtok(response, "@");
    //             p = strtok(NULL, "\0");
    //             strcpy(printer, p);
    //         }
    //         else
    //         {
    //             strcpy(printer, "Neither success or fail found in server response?\t response: ");
    //             strcat(printer, response);
    //         }
    //     }
    // else if (0 == 1)
    // {
    // } // NEXT DEVICE
    //   // printf("printer: |%s|\n", printer);

    char *p = strdup(response);
    return p;
}

void show_lightbulb_frame()
{
    GtkWidget *lightbulb_frame = GTK_WIDGET(gtk_builder_get_object(builder, "lightbulb_frame"));
    gtk_widget_show(lightbulb_frame);
}
void hide_lightbulb_frame()
{

    GtkWidget *lightbulb_frame = GTK_WIDGET(gtk_builder_get_object(builder, "lightbulb_frame"));
    gtk_widget_hide(lightbulb_frame);
}

void change_lightbulb_color(const char *color)
{
    printf("\nChanging lightbulb color to %s \n ", color);

    GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(builder, "lightbulb_currentcolor"));
    gchar *currentXmlColor = g_strdup_printf("<span foreground='%s'>%s</span>", color, color);
    gtk_label_set_markup(GTK_LABEL(widget), currentXmlColor);
    free(currentXmlColor);
}

void change_error_zone(const char *content)
{
    GtkWidget *errorzone = GTK_WIDGET(gtk_builder_get_object(builder, "label1"));

    gtk_label_set_text(GTK_LABEL(errorzone), (const gchar *)content);
}
const char *choose_color(int option)
{
    switch (option)
    {
    case 1:
        return "WHITE";
        break;
    case 2:
        return "RED";
        break;
    case 3:
        return "ORANGE";
        break;
    case 4:
        return "YELLOW";
        break;
    case 5:
        return "GREEN";
        break;
    case 6:
        return "BLUE";
        break;
    case 7:
        return "INDIGO";
        break;
    case 8:
        return "VIOLET";
        break;
    case 9:
        return "GRAY";
        break;
    default:
        return "ERROR";
    }
}
int color_char_to_int(const char *color)
{
    if (strstr(color, "WHITE"))
        return 1;
    if (strstr(color, "RED"))
        return 2;
    if (strstr(color, "ORANGE"))
        return 3;
    if (strstr(color, "YELLOW"))
        return 4;
    if (strstr(color, "GREEN"))
        return 5;
    if (strstr(color, "BLUE"))
        return 6;
    if (strstr(color, "INDIGO"))
        return 7;
    if (strstr(color, "VIOLET"))
        return 8;
    if (strstr(color, "GRAY"))
        return 9;
    return -1;
}

void on_lightbulb_brightness_changed(GtkVolumeButton *volumeButton)
{

    gdouble val = gtk_scale_button_get_value(GTK_SCALE_BUTTON(volumeButton));

    gchar *str = g_strdup_printf("Brightness changed to %.f", val);
    //////////////////////////////////////////////
    char toBeSent[20];
    sprintf(toBeSent, "setbrightness %d", (int)val);
    char *p = write_to(lightbulb.sockfd, toBeSent);
    if (strstr(p, "success"))
    {
        g_free(str);
        str = g_strdup_printf("%d", (int)val);
        change_lightbulb_label_brightness(str);
        lightbulb.brightness = (int)val;
    }
    if (debug)
        change_error_zone(p);
    free(p);
    g_free(str);
}

void change_lightbulb_label_brightness(const char *value)
{
    GtkWidget *brightnessLabel = GTK_WIDGET(gtk_builder_get_object(builder, "lightbulb_brightness"));
    gtk_label_set_text(GTK_LABEL(brightnessLabel), (const gchar *)value);
}
void turnoff(char *p) // p = device
{

    if (strncmp(p, "lightbulb", 9) == 0)
    {
        printf("\nTurnoff lightbulb received.\n");
        disconnect_from(1, 1);
    }
    else if (strncmp(p, "tv", 2) == 0)
    {
        printf("\nTurnoff tv received.\n");
        disconnect_from(2, 1);
    }
    else if (strncmp(p, "vacuum", 6) == 0)
    {
        printf("\nTurnoff vacuum received.\n");
        disconnect_from(3, 1);
    }
}

void on_lightbulb_shutdown_clicked(GtkButton *b)
{
    printf("TURNOFF");
    turnoff("lightbulb");
    if (debug)
        change_error_zone("Lightbulb turned off.");
}

void rearrange_labels()
{
    int tvoffset = 0, vacuumoffset = 0;
    if (lightbulb.connected)
    {
        tvoffset++;
        vacuumoffset++;
    }
    if (tv.connected)
    {
        vacuumoffset++;
    }
    // 335 pixels moved

    // TV OFFSETTING    // not-connected-y - 125 | increment by 335
    GtkWidget *tvWordLabel = GTK_WIDGET(gtk_builder_get_object(builder, "tv_word_status"));
    GtkWidget *tvConnectionLabel = GTK_WIDGET(gtk_builder_get_object(builder, "tv_connection_status"));
    GtkWidget *tvConnectionButton = GTK_WIDGET(gtk_builder_get_object(builder, "tv_connect"));
    // gtk_label_set_yalign(GTK_LABEL(tvWordLabel), (gfloat)(125 + tvoffset * 335));
    // gtk_label_set_yalign(GTK_LABEL(tvConnectionLabel), (gfloat)((double)(125 + tvoffset * 335)));

    printf("REMADE OFFSETS");
    // gtk_button_set_yalign(GTK_BUTTON(tvConnectionButton), (gfloat)(125 + tvoffset * 335));
    // gtk_button_set_alignment(GTK_BUTTON(tvConnectionButton), (gfloat)295 * 1.01, (gfloat)((double)(125 + tvoffset * 335)));
}

void show_tv_frame()
{
    GtkWidget *tv_frame = GTK_WIDGET(gtk_builder_get_object(builder, "tv_frame"));
    gtk_widget_show(tv_frame);
}

void on_tv_connect_clicked(GtkButton *b)
{
    gchar *connectionXmlColor;
    GtkWidget *tv_connect = GTK_WIDGET(gtk_builder_get_object(builder, "tv_connect"));
    GtkWidget *tv_connection_status = GTK_WIDGET(gtk_builder_get_object(builder, "tv_connection_status"));

    if (tv.connected == 0)
    {
        if (connect_to(HOME, PORT2, &tv.sockfd) == 0)
        {
            show_tv_frame();
            char values[100];
            sprintf(values, "Brightness: %d, Volume: %d, Channel: %d", tv.brightness, tv.volume, tv.channel);
            if (debug)
                change_error_zone(values);
            else
                change_error_zone("");
            // CONNECTION STATUS CHANGE
            gtk_label_set_text(GTK_LABEL(tv_connection_status), (const gchar *)"Connected");
            gtk_button_set_label(GTK_BUTTON(tv_connect), "Disconnect");
            connectionXmlColor = g_strdup_printf("<span foreground='green'>%s</span>", "Connected");
            gtk_label_set_markup(GTK_LABEL(tv_connection_status), connectionXmlColor);
            free(connectionXmlColor);
            // CURRENT BRIGHTNESS CHANGE
            sprintf(values, "%d", tv.brightness);
            GtkWidget *brightnessSlider = GTK_WIDGET(gtk_builder_get_object(builder, "tv_brightness_slider"));
            gtk_scale_button_set_value(GTK_SCALE_BUTTON(brightnessSlider), (gdouble)tv.brightness);
            change_tv_label_brightness(values);
            // CURRENT VOLUME CHANGE
            sprintf(values, "%d", tv.volume);
            GtkWidget *volumeSlider = GTK_WIDGET(gtk_builder_get_object(builder, "tv_volume_slider"));
            gtk_scale_button_set_value(GTK_SCALE_BUTTON(volumeSlider), (gdouble)tv.volume);
            change_tv_label_volume(values);
            // CURRENT CHANNEL CHANGE
            GtkSpinButton *channelSpinner = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "tv_channel"));
            gtk_spin_button_set_value(channelSpinner, tv.channel);
            // BACKEND
            tv.connected = 1;
            // SHOW
        }
        else
        {

            change_error_zone("Error: TV probably not on.");
        }
    }
    else
    {
        disconnect_from(2, 0);
    }
}

void change_tv_label_brightness(const char *value)
{
    GtkWidget *brightnessLabel = GTK_WIDGET(gtk_builder_get_object(builder, "tv_brightness"));
    gtk_label_set_text(GTK_LABEL(brightnessLabel), (const gchar *)value);
}

void hide_tv_frame()
{
    GtkWidget *tv_frame = GTK_WIDGET(gtk_builder_get_object(builder, "tv_frame"));
    gtk_widget_hide(tv_frame);
}

void on_tv_brightness_changed(GtkVolumeButton *volumeButton)
{
    gdouble val = gtk_scale_button_get_value(GTK_SCALE_BUTTON(volumeButton));

    gchar *str = g_strdup_printf("Brightness changed to %.f", val);
    //////////////////////////////////////////////
    char toBeSent[20];
    sprintf(toBeSent, "setbrightness %d", (int)val);
    char *p = write_to(tv.sockfd, toBeSent);
    if (strstr(p, "success"))
    {
        g_free(str);
        str = g_strdup_printf("%d", (int)val);
        change_tv_label_brightness(str);
        tv.brightness = (int)val;
    }
    if (debug)
        change_error_zone(p);
    free(p);
    g_free(str);
}

void on_tv_volume_changed(GtkVolumeButton *volumeButton)
{
    gdouble val = gtk_scale_button_get_value(GTK_SCALE_BUTTON(volumeButton));

    //////////////////////////////////////////////
    char toBeSent[20];
    sprintf(toBeSent, "setvolume %d", (int)val);
    char *p = write_to(tv.sockfd, toBeSent);
    if (strstr(p, "success"))
    {

        gchar *str = g_strdup_printf("%d", (int)val);
        change_tv_label_volume(str);
        g_free(str);
        tv.volume = (int)val;
    }
    if (debug)
        change_error_zone(p);
    free(p);
}

void change_tv_label_volume(const char *value)
{
    GtkWidget *volumeLabel = GTK_WIDGET(gtk_builder_get_object(builder, "tv_volume"));
    gtk_label_set_text(GTK_LABEL(volumeLabel), (const gchar *)value);
}

void on_tv_channel_changed(GtkSpinButton *channelSpinner)
{

    int channel = gtk_spin_button_get_value_as_int(channelSpinner);
    char toBeSent[20];
    sprintf(toBeSent, "setchannel %d", channel);
    char *p = write_to(tv.sockfd, toBeSent);
    if (strstr(p, "success"))
    {

        gchar *str = g_strdup_printf("%d", channel);
        g_free(str);
        tv.channel = channel;
    }
    if (debug)
        change_error_zone(p);
    free(p);
}

void on_tv_shutdown_clicked(GtkButton *button)

{
    printf("TURNOFF TV");
    turnoff("tv");
    if (debug)
        change_error_zone("TV turned off.");
}

void on_vacuum_connect_clicked(GtkButton *b)
{
    GtkWidget *vacuum_connect = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_connect"));
    GtkWidget *vacuum_connection_status = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_connection_status"));

    gchar *connectionXmlColor;
    if (vacuum.connected == 0)
    {
        if (connect_to(HOME, PORT3, &vacuum.sockfd) == 0)
        {
            show_vacuum_frame();
            char values[100];
            sprintf(values, "Power: %d, Speed: %d", vacuum.power, vacuum.speed);
            if (debug)
                change_error_zone(values);
            else
                change_error_zone("");
            // CONNECTION STATUS CHANGE
            gtk_label_set_text(GTK_LABEL(vacuum_connection_status), (const gchar *)"Connected");
            gtk_button_set_label(GTK_BUTTON(vacuum_connect), "Disconnect");
            connectionXmlColor = g_strdup_printf("<span foreground='green'>%s</span>", "Connected");
            gtk_label_set_markup(GTK_LABEL(vacuum_connection_status), connectionXmlColor);
            free(connectionXmlColor);
            // CURRENT BRIGHTNESS CHANGE
            sprintf(values, "%d", vacuum.power);
            GtkWidget *powerSlider = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_power_slider"));
            gtk_scale_button_set_value(GTK_SCALE_BUTTON(powerSlider), (gdouble)vacuum.power);
            change_vacuum_label_power(values);

            // CURRENT SPEED CHANGE
            set_vacuum_option(vacuum.speed);

            // BACKEND
            vacuum.connected = 1;
            // SHOW
        }
        else
        {
            change_error_zone("Error: Vacuum probably not on.");
        }
    }
    else
    {
        disconnect_from(3, 0);
    }
}

void show_vacuum_frame()
{
    GtkWidget *vacuum_frame = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_frame"));
    gtk_widget_show(vacuum_frame);
}

void hide_vacuum_frame()
{
    GtkWidget *vacuum_frame = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_frame"));
    gtk_widget_hide(vacuum_frame);
}
void change_vacuum_label_power(const char *value)
{
    GtkWidget *vacuum_power = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_power"));
    gtk_label_set_text(GTK_LABEL(vacuum_power), (const gchar *)value);
}

void on_vacuum_shutdown_clicked(GtkButton *b)
{
    printf("TURNOFF VACUUM");
    turnoff("vacuum");
    if (debug)
        change_error_zone("Vacuum turned off.");
}
void on_vacuum_power_changed(GtkVolumeButton *powerButton)
{
    gdouble val = gtk_scale_button_get_value(GTK_SCALE_BUTTON(powerButton));

    //////////////////////////////////////////////
    char toBeSent[20];
    sprintf(toBeSent, "setpower %d", (int)val);
    char *p = write_to(vacuum.sockfd, toBeSent);
    if (strstr(p, "success"))
    {

        gchar *str = g_strdup_printf("%d", (int)val);
        change_vacuum_label_power(str);
        g_free(str);
        tv.volume = (int)val;
    }
    if (debug)
        change_error_zone(p);
    free(p);
}

void set_vacuum_option(int option)
{
    printf("HELLO");
    switch (option)
    {
    case 0:
        printf("\n\n\n OPTION 0");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_option0"))), (gboolean)1);
        break;
    case 1:
        printf("\n\n\n OPTION 1");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_option1"))), (gboolean)1);
        break;
    case 2:
        printf("\n\n\n OPTION 2");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_option2"))), (gboolean)1);
        break;
    default:
        printf("Wrong set_vacuum_option, option: %d", option);
    }
}

void on_vacuum_select_speed_clicked(GtkRadioButton *button)
{
    printf("\non_vacuum_select_speed_clicked\n");
    char toBeSent[100];
    int option = -1;

    GtkWidget *rbutton0 = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_option0"));

    GtkWidget *rbutton1 = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_option1"));

    GtkWidget *rbutton2 = GTK_WIDGET(gtk_builder_get_object(builder, "vacuum_option2"));

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rbutton0)) == TRUE)
        option = 0;
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rbutton1)) == TRUE)
        option = 1;
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rbutton2)) == TRUE)
        option = 2;

    sprintf(toBeSent, "setspeed %d", option);
    char *p = write_to(vacuum.sockfd, toBeSent);
    if (strstr(p, "success"))
    {
        vacuum.speed = option;
    }
    if (debug)
        change_error_zone(p);
    free(p);
}