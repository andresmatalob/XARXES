/**
 *  author: Andrés Bernárdez Matalobos
 *  XARXES
 *  Práctica 1 
 * 
 * 
 **/

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
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


//Registered types of PDU
#define NO_RESPONSE 0X08
#define REGISTER_REQ 0X00
#define REGISTER_ACK 0X02
#define REGISTER_NACK 0X04
#define REGISTER_REJ 0X06
#define ERROR 0X0F

//Alives types of PDU
#define ALIVE_INF 0X10
#define ALIVE_ACK 0X12
#define ALIVE_NACK 0X14
#define ALIVE_REJ 0X16


//Registered states
#define DISCONNECTED 0XA0
#define WAIT_REG_RESPONSE 0XA2
#define WAIT_DB_CHECK 0XA4
#define REGISTERED 0XA6
#define SEND_ALIVE 0XA8

//Package size
#define MAX_FILE_PATH_LENGTH 100

//variables de control de programa (read configuration)
bool debug = false;
char config_file[MAX_FILE_PATH_LENGTH];
char network_file[MAX_FILE_PATH_LENGTH];

//variables para openupdsocketc
char* ip_client = "127.0.0.1";
char* package;


//variables locales sockect
struct sockaddr_in	addr_server,addr_client,saved_addr_server;
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

void print_time();

//1. FASE REGISTRO
//lee el fichero de configuracion y guarda los datos en las variables globales
void read_configuration (char *config_file) {
    FILE *file; //puntero al fichero
    char line[MAX_FILE_PATH_LENGTH];
    char *token;
    int i = 0; //contador de tokens
    bool flag = false;


    file = fopen(config_file, "r");

    if (file == NULL) { //comprobamos que se ha abierto correctamente
        printf("Error opening the configuration file.\n");
        exit(1);
    }

    while (fgets(line, MAX_FILE_PATH_LENGTH, file) != NULL) {
        token = strtok(line, " ");

        while (token != NULL) {

            if (flag) {
                if (i == 0) {
                    strcpy((char *) id_equip, token);}
                if (i == 1) {
                    strcpy((char *) adress_MAC, token);}
                if (i == 2) {
                    strcpy((char *) NMS_id, token);}
                if (i == 3) {
                    strcpy((char *) NMS_UDP_port, token);
                }
                i++;
            }
            token = strtok(NULL, " ");
            flag = !flag;

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

    for (int i = 1; i < 8; i++){}

    for (int i = 8; i < 21; i++){}

    for (int i = 21; i < 28; i++){}

    for (int i = 28; i < 78 && package_to_check[i] != 0; i++){}
}

// 1.2.FASE RESPUESTA
void open_udp_socket(){
    sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_udp < 0) //if after socket creation, the socket is less than 0, it means that the socket has not been created
    {
        exit(-1);
    }

    memset(&addr_client, 0, sizeof (struct sockaddr_in)); // load memory with 0
    addr_client.sin_family = AF_INET;
    addr_client.sin_addr.s_addr = inet_addr(ip_client);
    addr_client.sin_port = htons(0);

    /* binding (ligar) */
    if(bind(sock_udp, (struct sockaddr *)&addr_client, sizeof(struct sockaddr_in)) < 0)
    {
        exit(-2);
    }

    memset(&addr_server, 0, sizeof(addr_server));

    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(atoi(NMS_UDP_port));
    addr_server.sin_addr.s_addr = atoi(NMS_id);

}

void print_time() {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    printf("%d:%d:%d: ", tm.tm_hour, tm.tm_min, tm.tm_sec);

}

void print_debug(char *message, int i) { //funcion para imprimir mensajes de debug
    debug = true;
    if (debug == true){
        print_time();
        printf("%s\n", message);
    }
}

void send_udp_package(char* package_to_send){
    if (sendto(sock_udp, package_to_send, 78, 0, (struct sockaddr *) &addr_server, sizeof( addr_server)) < 0){
        printf("Error sending package\n");
    }
    else{
        print_debug("MSG. => Package sent", 0);
    }
}

void receive_udp_package(float waiting_time) {
    struct timeval tv;
    package = malloc(78);
    tv.tv_sec = waiting_time;
    tv.tv_usec = 0;
    setsockopt(sock_udp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    if (recvfrom(sock_udp, package, 78, 0,(struct sockaddr *) &addr_server, &laddr_server) < 0) {
        package[0]=NO_RESPONSE;
    } else {
        if (package[0] == REGISTER_ACK) {
            print_debug("DEBUG => Received a REGISTER_ACK\n", 0);
        } else if (package[0] == REGISTER_NACK) {
            print_debug("DEBUG => Received a REGISTER_NACK\n", 0);
        } else if (package[0] == REGISTER_REJ) {
            print_debug("DEBUG => Received a REGISTER_REJ\n", 0);
        } else if (package[0] == REGISTER_REQ) {
            print_debug("DEBUG => Received a REGISTER_REQ\n", 0);
        } else if (package[0] == ALIVE_ACK) {
            print_debug("DEBUG => Received an ALIVE_ACK\n", 0);
        } else if (package[0] == ALIVE_NACK) {
            print_debug("DEBUG => Received an ALIVE_NACK\n", 0);
        } else if (package[0] == ALIVE_REJ) {
            print_debug("DEBUG => Received an ALIVE_REJ\n", 0);
        } else if (package[0] == ALIVE_INF) {
            print_debug("DEBUG => Received an ALIVE_INF\n", 0);
        } else {
            print_debug("DEBUG => Package unknowledge\n", 0);
            print_package(package);
        }
    }
}

bool check_package(char* package_to_check){
    for (int i = 1; i < 8; i++) {
        if (package_to_check[i] != id_server[i - 1]) {
            printf("Not the expected id\n");
            return false;
        }
    }
    for (int i = 8; i < 21; i++) {
        if (package_to_check[i] != adress_MAC_server[i - 8]) {
            printf("Not the expected MAC\n");

            return false;
        }
    }
    for (int i = 21; i < 27; i++){
        if(package_to_check[i] != random_number[i-21]){
            printf("Not the expected number\n");
            return false;
        }
    }
    if(strcmp(inet_ntoa(addr_server.sin_addr), inet_ntoa(saved_addr_server.sin_addr)) != 0){
        printf("Not the expected ip\n");
        return false;
    }
    return true;
}
void package_management_register(){
    receive_udp_package(timing_registered());
    if(package[0] == NO_RESPONSE){
        print_debug("DEBUG => Actual state: WAIT_REG_RESPONSE\n", 0);
        change_state(WAIT_REG_RESPONSE);

        if(package_counter_registered >= N){
            sleep(U);
            tries_counter_registered++;
            if(tries_counter_registered >= O){
                printf("Register failed whit server: localhost\n");
                printf("Attempts number allowed has been exceeded: %i, program ends\n", O);
                exit(0);
            }else{
                printf("Register try nº %i\n", tries_counter_registered);
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


        saved_addr_server = addr_server;
    }

    else if (package[0] == REGISTER_NACK){
        for (int i = 28; i < 78 && package[i] != 0; i++){
            printf("%c", package[i]);
        }
        printf("\n");
        tries_counter_registered++;
        package_counter_registered = 0;
        client_register();

    } else if (package[0] == REGISTER_REJ){
        change_state(actual_state);


        print_debug("DEBUG => Received REGISTER_REJ, program ends\n", 0);
        for (int i = 28; i < 78 && package[i] != 0; i++){
            printf("%c", package[i]);
        }
        printf("\n");
        exit(0);
    } else{
        print_debug("DEBUG => Received unknowledge package, program ends\n", 0);
        exit(0);
    }

}

void change_state(int state) {
    if (actual_state != state) {
        actual_state = state;
    }
}


int timing_registered() {
    if (package_counter_registered < P) {
        return T;
    } else if (package_counter_registered - P < Q) {
        return T *((package_counter_registered - P) + 1);
    } else {
        return T * Q;
    }
}

void client_register(){
    send_udp_package(create_package(REGISTER_REQ, ""));
    actual_state = WAIT_REG_RESPONSE;
    package_management_register();
}

//2.ALIVES

void alive_try(){
    print_debug("DEBUG => Process created to manage alives", 0);
    print_debug("DEBUG => Timer established\n", 0);
    while (actual_state == REGISTERED) { //while REGISTERED
        print_debug("MSG. => Actual state: REGISTERED\n", 0);
        package_counter_alive = 0;
        lost_alives = 0;
        send_udp_package(create_package(ALIVE_INF, ""));

        receive_udp_package(R);

        if(package[0] == NO_RESPONSE){
            if (lost_alives >= S){
                change_state(DISCONNECTED);
                print_debug("MSG. => Actual state: DISCONNECTED\n", 0);
                alive_try();
            }else{
                change_state(REGISTERED);
                print_debug("MSG. => Actual state: REGISTERED\n", 0);
                lost_alives++;
            }
        }
        else if(package[0] == ALIVE_ACK){
            if (check_package(package)== false){
                printf("Package with different PDU\n");
                if (lost_alives >=S){
                    print_debug("MSG. => Actual state: DISCONNECTED\n",0);
                    change_state(DISCONNECTED);
                    alive_try();
                }else{
                    change_state(REGISTERED);
                    print_debug("MSG. => Actual state: REGISTERED\n",0);
                    lost_alives++;
                }

            }else{
                change_state(SEND_ALIVE);
                print_debug("MSG. => Actual state: SEND_ALIVE\n", 0);
                keep_in_touch();
            }
        }
        else if(package[0] == ALIVE_NACK){
            if (lost_alives >= S){
                print_debug("MSG. => Actual state: DISCONNECTED\n",0);
                change_state(DISCONNECTED);
                alive_try();
            }else{
                change_state(REGISTERED);
                print_debug("MSG. => Actual state: REGISTERED\n",0);
                lost_alives++;
            }
        }
        else if(package[0] == ALIVE_REJ){
            print_debug("MSG. => Actual state: DISCONNECTED\n",0);
            change_state(DISCONNECTED);
            alive_try();
        } else if (check_package(package) == false) {
            printf("Package with different PDU\n");
            if (lost_alives >= S){
                print_debug("MSG. => Actual state: DISCONNECTED\n",0);
                change_state(DISCONNECTED);
                alive_try();
            }else{
                change_state(REGISTERED);
                print_debug("MSG. => Actual state: REGISTERED\n",0);
                lost_alives++;
            }
        }
        else{
            printf("Package unknowlegde\n");
        }
    }

}
//3.MANTENER COMUNICACIÓN PERIÓDICA CON EL SERVIDOR

void keep_in_touch() {
    while (actual_state == SEND_ALIVE) {
        command_line();
        send_udp_package(create_package(ALIVE_INF, ""));
        print_package(package);
        package_management_alive();

    }
}

void package_management_alive() {
    receive_udp_package(R);
    if (package[0] == NO_RESPONSE){
        if (lost_alives >= S-1){
            print_debug("INFO => Registered fail with server \n", 0);
            change_state(DISCONNECTED);
            client_register();

        } else {
            lost_alives++;
        }

    } else if (package[0] == ALIVE_ACK){
        if (check_package(package) == false){
            if (lost_alives >= S){
                change_state(DISCONNECTED);
                client_register();
            } else {
                change_state(REGISTERED);
                lost_alives++;
            }

        } else {
            lost_alives = 0;
            sleep(U);
            change_state(SEND_ALIVE);
            keep_in_touch();
        }

    } else if (package[0] == ALIVE_REJ){
        change_state(DISCONNECTED);
        client_register();
    }

    else{
        if (lost_alives >= S){
            change_state(DISCONNECTED);
            alive_try();
        } else {
            change_state(REGISTERED);
            lost_alives++;
        }
    }
}

//4. LEER COMANDOS DE LA LÍNEA DE COMANDOS
void command_line() {
    //printf("entrando en command_line\n");
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
        scanf("%s", command);
        if (strcmp(command, "quit") == 0){
            printf("Received command quit\n");
            close(sock_udp);
            exit(0);

        }
        if (strcmp(command, "send-cfg") == 0){
            printf("Received command send-cfg\n");;
        }
        if (strcmp(command, "get-cfg") == 0){
            printf("Received command get-cfg\n");
        }
    } else if (err == -1 ){
        printf("Error select\n");
    }
}


int main(int argc, char *argv[]) {
    strcpy(config_file, "client.cfg"); //default configuration file
    read_parameters(argc, argv);
    read_configuration(config_file); //read the configuration file
    open_udp_socket();
    client_register();
    alive_try();
    printf("\n");
}
