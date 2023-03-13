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
char id_server[7];
char adress_MAC_server[13];
unsigned long ip_server;


//tipos de PDU en el registro
#define NO_RESPONSE 0X08
#define REGISTER_REQ 0X00
#define REGISTER_ACK 0X02
#define REGISTER_NACK 0X04
#define REGISTER_REJ 0X06
#define ERROR 0X0F

//tipos de PDU en el alive
#define ALIVE_INF 0X10
#define ALIVE_ACK 0X12
#define ALIVE_NACK 0X14
#define ALIVE_REJ 0X16


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
int sock_udp;
socklen_t laddr_server;


//variables client_register
#define T 1
#define P 2
#define Q 3
#define U 2
#define N 6
#define O 2
int tries_counter_registered = 0;
int package_counter_registered= 0;
int package_counter_alive= 0;

//variables para el alive
#define S 3
#define R 2
char port_udp_alive[50];
int lost_alives = 0;


void client_register();
int timing_registered();

void change_state(int state);

void keep_in_touch();
void command_line();

void package_management_alive();

//1. FASE REGISTRO
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
            //printf("Debug mode activated.\n");
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
    result = (char*) malloc(78*sizeof(char));

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


    //printf("ID del equipo: ");
    for (int i = 1; i < 8; i++){
        printf("%c", package_to_check[i]);
    }
    //printf("\n");

    //printf("Adress Mac: ");
    for (int i = 8; i < 21; i++){
        printf("%c", package_to_check[i]);
    }

    //printf("\n");

    //printf("Random number: ");
    for (int i = 21; i < 28; i++){
        printf("%c", package_to_check[i]);
    }
    //printf("\n");

    //printf("Datos: ");
    for (int i = 28; i < 78 && package_to_check[i] != 0; i++){
        printf("%c", package_to_check[i]);
    }
    //printf("\n");

}

// 2.FASE RESPUESTA
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
    //printf("succesfull socket!!!\n");

}
void send_udp_package(char* package_to_send){
    if (sendto(sock_udp, package_to_send, 78, 0, (struct sockaddr *) &addr_server, sizeof( addr_server)) < 0){
        printf("Error sending package\n");
    }
    else{
        //printf("Package sent\n");

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
        printf("Se ha recibido un paquete %x\n",  package[0]);
        if (package[0] == REGISTER_ACK) {
            printf("Se ha recibido un REGISTER_ACK\n");
        } else if (package[0] == REGISTER_NACK) {
            printf("Se ha recibido un REGISTER_NACK\n");
        } else if (package[0] == REGISTER_REJ) {
            printf("Se ha recibido un REGISTER_REJ\n");
        } else if (package[0] == REGISTER_REQ) {
            printf("Se ha recibido un REGISTER_REQ\n");
        } else if (package[0] == ALIVE_ACK) {
            printf("Se ha recibido un ALIVE_ACK\n");
        } else if (package[0] == ALIVE_NACK) {
            printf("Se ha recibido un ALIVE_NACK\n");
        } else if (package[0] == ALIVE_REJ) {
            printf("Se ha recibido un ALIVE_REJ\n");
        } else if (package[0] == ALIVE_INF) {
            printf("Se ha recibido un ALIVE_INF\n");
        } else {
            printf("Se ha recibido un paquete no reconocido\n");
            print_package(package);
        }
    }
}

void check_package(char* package_to_check){
    for (int i = 1; i < 8; i++) {
        if (package_to_check[i] != id_equip[i - 1]) {
            printf("Not the expected id\n");
            exit(0);
        }
    }
    for (int i = 8; i < 21; i++) {
        if (package_to_check[i] != adress_MAC[i - 8]) {
            printf("Not the expected MAC\n");
            exit(0);
        }
    }
    for (int i = 21; i < 27; i++){
        if(package_to_check[i] != random_number[i-21]){
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
        change_state(WAIT_REG_RESPONSE);
        printf("Actual state: WAIT_REG_RESPONSE\n");

        if(package_counter_registered >= N){
            sleep(U);
            tries_counter_registered++;
            if(tries_counter_registered >= O){
                printf("Se han superado el numero de intentos permitidos: %i, se finaliza el programa\n", O);
                exit(0);
            }else{
                printf("intento nº %i\n", tries_counter_registered);
                package_counter_registered = 0;
                client_register();

            }

        } else {
            package_counter_registered++;
            client_register();
        }
    }

    else if (package[0] == REGISTER_ACK) {
        change_state(REGISTERED);
        printf("Actual state: REGISTERED\n");

        //guardar el id_server, adress_MAC_server, ip_server
        for (int i = 1; i < 8; i++) {
            id_server[i - 1] = package[i];
        }
        for (int i = 8; i < 21; i++) {
            adress_MAC_server[i - 8] = package[i];
        }
        for (int i = 21; i < 28; i++){
            random_number[i-21] = package[i];
        }

       //printf id_server value and adress_MAC_server value
        printf("id_server: %s\n", id_server);
        printf("adress_MAC_server: %s\n", adress_MAC_server);
        printf("random_number: %s\n", random_number);
        ip_server = addr_server.sin_addr.s_addr;
    }

    else if (package[0] == REGISTER_NACK){
        printf("Se ha recibido un paquete NACK\n");

        for (int i = 28; i < 78 && package[i] != 0; i++){
            printf("%c", package[i]);
        }
        printf("\n");
        tries_counter_registered++;
        package_counter_registered = 0;
        client_register();

    } else if (package[0] == REGISTER_REJ){
        change_state(actual_state);
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

void change_state(int state) {
    if (actual_state != state) {
        actual_state = state;
        printf("Actual state: %i\n", actual_state);
    }

}


int timing_registered() {
    if (package_counter_registered < P) {
        printf("timing en los primeros p paquetes = %i\n", T);
        return T;
    } else if (package_counter_registered - P < Q) {
        printf("timing en los siguientes q paquetes = %i\n", T *((package_counter_registered - P) + 1));
        return T *((package_counter_registered - P) + 1);
    } else {
        printf("timing en los siguientes paquetes = %i\n", T * Q);
        return T * Q;
    }
}

void client_register(){
    send_udp_package(create_package(REGISTER_REQ, "Hola"));
    actual_state = WAIT_REG_RESPONSE;
    package_management_register();
}

//2.MANTENER COMUNICACIÓN PERIÓDICA CON EL SERVIDOR

void alive_try(){
    while (actual_state == REGISTERED) {
        package_counter_alive = 0;
        send_udp_package(create_package(ALIVE_INF, "Hola"));

        receive_udp_package(R);
        //si no se recibe respuesta del servidor de la recepción del paquete ALIVE_INF a R paquetes, se finaliza el programa pasara a estado DISCONNECTED y se iniciara el proceso de registro
        //comporbamos que el numero de alives perdidos no sea mayor que S, si es mayor se finaliza el programa
        if(package[0] == NO_RESPONSE){
            printf("No se ha recibido respuesta del servidor\n");
            if (lost_alives > R){
                change_state(DISCONNECTED);
                alive_try();
            }else{
                change_state(REGISTERED);
                lost_alives++;
            }
        }
        else if(package[0] == ALIVE_ACK){
            printf("Se ha recibido un paquete ACK\n");
            change_state(SEND_ALIVE);
            keep_in_touch();
        }
        else if(package[0] == ALIVE_NACK){
            printf("Se ha recibido un paquete NACK\n");
            if (lost_alives > S){
                change_state(DISCONNECTED);
                alive_try();
            }else{
                change_state(REGISTERED);
                lost_alives++;
            }
        }
        else if(package[0] == ALIVE_REJ){
            printf("Se ha recibido un paquete REJ\n");
            change_state(DISCONNECTED);
            alive_try();
        }
        else{
            printf("Se ha recibido un paquete no reconocido\n");
        }
    }

}

void keep_in_touch() {
    while (actual_state == SEND_ALIVE) {
        printf("paquete recibido %x\n", package[0]);
        command_line();
        printf("voy a enviar un paquete alive\n");
        send_udp_package(create_package(ALIVE_INF, "Hola"));
        printf("he enviado un paquete alive\n");
        print_package(package);
        package_management_alive();

    }
}

void package_management_alive() {
    receive_udp_package(R);
    if (package[0] == NO_RESPONSE){
        printf("alives: %i\n", lost_alives);
        printf("aqii");
        if (lost_alives > S){
            change_state(DISCONNECTED);
            printf("Actual: %c\n", package[0]);
            client_register();
        } else {
            change_state(REGISTERED);
            printf("Actual state: %i\n", actual_state);
            lost_alives++;
        }
    } else if (package[0] == ALIVE_ACK){
        printf("Se ha recibido un paquete ACK\n");
        sleep(U);
        change_state(SEND_ALIVE);
        printf("Actual state: %i\n", actual_state);
        keep_in_touch();

    } else if (package[0] == ALIVE_REJ){
        printf("Se ha recibido un paquete REJ\n");
        change_state(DISCONNECTED);
        client_register();
    } else{
        if (lost_alives > S){
            change_state(DISCONNECTED);
            alive_try();
        } else {
            change_state(REGISTERED);
            lost_alives++;
        }
    }
}

//3. LEER COMANDOS DE LA LÍNEA DE COMANDOS
void command_line() {
    printf("entrando en command_line\n");
    fd_set readfds;
    char command [100];
    struct timeval tv;
    int err;
    FD_ZERO(&readfds); // borra los conjuntos de descriptores de fichero
    FD_SET(0, &readfds); // añade el descriptor de fichero 0 (stdin) al conjunto
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    err = select(1, &readfds, NULL, NULL, &tv);
    if (err > 0) {
        printf("Se ha recibido algo\n");
        scanf("%s", command);
        if (strcmp(command, "quit") == 0){
            printf("Se ha recibido el comando quit\n");
            close(sock_udp);
            exit(0);

        }
        if (strcmp(command, "send-cfg") == 0){
            printf("Se ha recibido el comando send-cfg\n");;
        }
        if (strcmp(command, "get-cfg") == 0){
            printf("Se ha recibido el comando get-cfg\n");
        }
    } else if (err == -1 ){
        printf("Error en el select\n");
    }
}


int main(int argc, char *argv[]) {
    strcpy(config_file, "client.cfg"); //default configuration file
    read_parameters(argc, argv);
    read_configuration(config_file); //read the configuration file
    //create_package(REGISTER_REJ, "Hola");//
    //print_package(create_package(ERROR, "Hola"));
    open_udp_socket();
    client_register();
    alive_try();

    printf("\n");
    //print_debug("Hemos enviado el paquete");
    //receive_udp_package(2)  ;
    //print_package(package);
}
