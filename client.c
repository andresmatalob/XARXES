/**
 *  Autor: Andrés Bernárdez Matalobos
 *  Asignatura: XARXES.
 *  Práctica 1 
 * 
 * 
 **/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/select.h>
#include <poll.h>
#include <arpa/inet.h>
#include <stdbool.h>

//client information
char id_equip[6];
char adress_MAC[12];
char NMS_id[16];
char NMS_UDP_port[6];
char random_number[7] = {'0','0','0','0','0','0','\0'};
int actual_state; // estado actual del cliente es un int

//sever information
char id_server[6];
char adress_MAC_server[12];
unsigned long ip_server;

//tipos de PDU en el registro
#define NO_RESPONSE 0X08
#define REGISTER_REQ 0X00
#define REGISTER_ACK 0X02
#define REGISTER_NACK 0X04
#define REGISTER_REJ 0X06
#define ERROR 0X0F


//estados del registro
#define DISCONNECTED 0XA0
#define WAIT_REG_RESPONSE 0XA2
#define WAIT_DB_CHECK 0XA4
#define REGISTERED 0XA6
#define SEND_ALIVE 0XA8

//tamaño de los paquetes
#define MAX_PACKAGE_LENGTH 100
#define MAX_FILE_PATH_LENGTH 100
#define MAX_COMMANDS_LENGTH 100

//paquete
//package_to_send[78];
char* package_to_check;

//variables de control de programa (read configuration)
bool debug = false;
char config_file[MAX_FILE_PATH_LENGTH];
char network_file[MAX_FILE_PATH_LENGTH];
//variables para openupdsocketc
char* ip_client = "127.0.0.1";
char* package;

//variables locales sockect
struct sockaddr_in	addr_server,addr_client;
int sock_udp,sock_tcp;
socklen_t laddr_server;

struct pdu_upd{
    unsigned char type;
    char id_equip_pdu[7];
    char address_MAC_pdu[13];
    char num_aleatorio_pdu[7];
    char data_pdu[50];
};
//variables client_registered
#define T 1
#define P 2
#define Q 3
#define U 2
#define N 6
#define O 2
int tries_counter_registered = 0;
int package_counter_registered= 0;
int package_counter_alive= 0;


void client_registered();
int timing_registered();

//lee el fichero de configuracion y guarda los datos en las variables globales
void read_configuration (char *config_file) {
    FILE *file; //puntero al fichero
    char line[MAX_FILE_PATH_LENGTH]; //linea del fichero
    char *token; //token de la linea
    int i = 0; //contador de tokens
    bool flag = false; //flag para saber si el token es el que queremos


    file = fopen(config_file, "r"); //abrimos el fichero en modo lectura

    if (file == NULL) { //comprobamos que se ha abierto correctamente
        printf("Error opening the configuration file.\n"); //si no se ha abierto correctamente, salimos del programa
        exit(1);
    }

    while (fgets(line, MAX_FILE_PATH_LENGTH, file) != NULL) { //mientras haya elementos leemos el fichero linea a linea
        token = strtok(line, " "); //separamos la linea en tokens

        while (token != NULL) { //mientras haya tokens

            if (flag) { //si el token es el que queremos
                if (i == 0) { //si es el primer token, lo guardamos en la variable id_equip
                    strcpy((char *) id_equip, token);}
                if (i == 1) {
                    strcpy((char *) adress_MAC, token);}
                if (i == 2) {
                    strcpy((char *) NMS_id, token);}
                if (i == 3) {
                    strcpy((char *) NMS_UDP_port, token);
                }
                i++; //incrementamos el contador de tokens
            }
            token = strtok(NULL, " "); //leemos el siguiente token
            flag = !flag; //cambiamos el valor del flag

        }

    }
}

//lee los parametros de entrada y los guarda en las variables globales
void read_parameters (int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 < argc) {
                i++;
                strcpy(config_file, argv[i]);
            }else{
                printf("Error: No configuration file specified.\n");
                exit(0);
            }
        } else if (strcmp(argv[i], "-d") == 0){
            debug = true;
            printf("Debug mode activated.\n");
        } else if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 < argc) {
                i++;
                strcpy(network_file, argv[i]);
            } else {
                printf("Error: No configuration file specified.\n");
                exit(0);
            }
        } else {
            printf("Error: Invalid parameter.\n");
            exit(0);
        }
    }
}
char* create_package (int type, char* data){
    char* result;
    result = (char*) malloc(78);

    result[0] = type;
    for (int i = 1; i < 8; i++){
        if(i<7) {
            result[i] = id_equip[i - 1];
        }else{
            result[i] = '\0';
        }
    }
    for (int i = 8; i < 21; i++){
        if(i<20){
            result[i] = adress_MAC[i-8];
        }else{
            result[i] = '\0';
        }
    }
    for (int i = 21; i < 28; i++){
        if(i<27){
            result[i] = random_number[i-21];
        }else{
            result[i] = '\0';
        }
    }
    for (int i = 28; i < 78; i++){
        result[i] = data[i-28];
    }
    return result;
}
void print_package (char* package_to_check){

    printf("Tipo de paquete: %x\n", package_to_check[0]);


    printf("ID del equipo: ");
    for (int i = 1; i < 7; i++){
        printf("%c", package_to_check[i]);
    }
    printf("\n");

    printf("Adress Mac: ");
    for (int i = 7; i < 20; i++){
        printf("%c", package_to_check[i]);
    }

    printf("Random number: ");
    for (int i = 20; i < 27; i++){
        printf("%c", package_to_check[i]);
    }
    printf("\n");

    printf("Datos: ");
    for (int i = 28; i < 78 && package_to_check[i] != 0; i++){
        printf("%c", package_to_check[i]);
    }
    printf("\n");

}

void open_udp_socket(){
    sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_udp < 0) //si despues de crearlo te ha devuelto menor que cero es porque no lo ha podido crear
    {
        printf("Can´t open socket!!!\n");
        exit(-1);
    }

    memset(&addr_client, 0, sizeof (struct sockaddr_in)); // guarda memoria para structs
    addr_client.sin_family = AF_INET;
    addr_client.sin_addr.s_addr = inet_addr(ip_client);
    addr_client.sin_port = htons(0);

    /* binding (ligar) */
    if(bind(sock_udp, (struct sockaddr *)&addr_client, sizeof(struct sockaddr_in)) < 0)
    {
        fprintf(stderr, " Can´t do socket's binding !!!\n");
        exit(-2);
    }

    memset(&addr_server, 0, sizeof(addr_server));

    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(atoi(NMS_UDP_port));
    addr_server.sin_addr.s_addr = atoi(NMS_id);
    printf("succesfull socket!!!\n");

}
void send_udp_package(char* package_to_send){
    if (sendto(sock_udp, package_to_send, 78, 0, (struct sockaddr *) &addr_server, sizeof( addr_server)) < 0){
        printf("Error sending package\n");
    }
    else{
        printf("Package sent\n");

    }

}

void print_debug(char* message){ //funcion para imprimir mensajes de debug
    if (debug){
        //print_time();
        printf("%s\n", message);
    }
}

void receive_udp_package(float waiting_time) {
    struct timeval tv;
    package = malloc(78);
    tv.tv_sec = waiting_time;
    tv.tv_usec = 0;
    setsockopt(sock_udp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    if (recvfrom(sock_udp, package, 78, 0,(struct sockaddr *) &addr_server, &laddr_server) < 0) {
        printf("No se han recibido paquetes en el tiempo\n");
        package[0]=NO_RESPONSE;
    } else {
        if (package[0] == REGISTER_ACK) {
            printf("Se ha recibido un ACK\n");
        } else if (package[0] == REGISTER_NACK) {
            printf("Se ha recibido un NACK\n");
        } else if (package[0] == REGISTER_REJ) {
            printf("Se ha recibido un REJ\n");
        } else if (package[0] == REGISTER_REQ) {
            printf("Se ha recibido un REQ\n");
        } else {
            printf("Se ha recibido un paquete no reconocido\n");
        }
    }
}

void check_package(char* package_to_check){
    for (int i = 1; i < 7; i++) {
        if (package_to_check[i] != id_equip[i - 1]) {
            printf("Not the expected id\n");
            exit(0);
        }
    }
    for (int i = 7; i < 20; i++) {
        if (package_to_check[i] != adress_MAC[i - 7]) {
            printf("Not the expected MAC\n");
            exit(0);
        }
    }
    for (int i = 20; i < 27; i++){
        if(package_to_check[i] != random_number[i-20]){
            printf("Not the expected number\n");
            exit(0);
        }
    }
    if(addr_server.sin_addr.s_addr != ip_server){
        printf("Not the expected ip\n");
        exit(0);
    }
}
void package_management_register(){
    receive_udp_package(timing_registered());
    if(package[0] == NO_RESPONSE){

        if(package_counter_registered > N){
            if(tries_counter_registered > O){
                printf("Se han superado el numero de intentos permitidos: %i, se finaliza el programa\n", O);
                exit(0);
            }else{
                tries_counter_registered++;
                package_counter_registered = 0;
                client_registered();
            }

        } else {
            package_counter_registered++;
            client_registered();
        }

    }
    else if (package[0] == REGISTER_ACK) {
        actual_state = REGISTERED;
        printf("Actual state: REGISTERED\n");

        //guardar el id_server, adress_MAC_server, ip_server
        for (int i = 1; i < 7; i++) {
            id_server[i - 1] = package[i];
        }
        for (int i = 7; i < 13; i++) {
            adress_MAC_server[i - 7] = package[i];
        }
        for (int i = 13; i < 20; i++) {
            random_number[i - 13] = package[i];
        }
        ip_server = addr_server.sin_addr.s_addr;


    }


    else if (package[0] == REGISTER_NACK){
        printf("Se ha recibido un paquete NACK\n");
        for (int i = 28; i < 78 && package_to_check[i] != 0; i++){
            printf("%c", package_to_check[i]);
        }
        printf("\n");
        tries_counter_registered++;
        package_counter_registered = 0;
        client_registered();

    } else if (package[0] == REGISTER_REJ){
        actual_state = DISCONNECTED;
        printf("Actual state: %i\n", actual_state);


        printf("Se ha recibido un paquete REJ, se finaliza el programa\n");
        for (int i = 28; i < 78 && package_to_check[i] != 0; i++){
            printf("%c", package_to_check[i]);
        }
        printf("\n");
        exit(0);
    } else{
        printf("Se ha recibido un paquete no reconocido, se finaliza el programa\n");
        exit(0);
    }

}


int timing_registered() {
    if (package_counter_registered < P) {
        return T;
    } else if (package_counter_registered < Q) {
        return T *((package_counter_registered - P) + 1);
    } else {
        return T * Q;
    }
}

void client_registered(){
    send_udp_package(create_package(REGISTER_REQ, "Hola"));
    actual_state = WAIT_REG_RESPONSE;
    package_management_register();
}

void change_state(int state) {
    if (actual_state != state) {
        actual_state = state;
        printf("Actual state: %i\n", actual_state);
    }
}

int main(int argc, char *argv[]) {
    strcpy(config_file, "client.cfg"); //default configuration file
    read_parameters(argc, argv);
    read_configuration(config_file); //read the configuration file

    create_package(REGISTER_REQ, "Hola");
    print_package(create_package(REGISTER_REQ,  "Hola"));
    open_udp_socket();
    send_udp_package(create_package(REGISTER_REQ, "Hola"));
    print_debug("Hemos enviado el paquete");
    receive_udp_package(2);
    print_package(package);
}


