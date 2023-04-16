# -------------------------------------------------------------------------------
# author:   Andres Bernardez Matalobos
# python_version: 3.10
# -------------------------------------------------------------------------------
#!/usr/bin/env python3

import sys
import datetime
import socket
import time
import select
import struct
import threading
import random

# Register packages
REGISTER_REQ = 0X00
REGISTER_ACK = 0X02
REGISTER_NACK = 0X04
REGISTER_REJ = 0X06
ERROR = 0X0F
NO_RESPONSE = 0X08

# Alive packages
ALIVE_INF = 0X10
ALIVE_ACK = 0X12
ALIVE_NACK = 0X14
ALIVE_REJ = 0X16

# Client's states
DISCONNECTED = 0XA0
WAIT_DB_CHECK = 0XA4
REGISTERED = 0XA6
SEND_ALIVE = 0XA8



from datetime import datetime

configuration_file = "server.cfg"
acceptedclients_file = "equips.dat"
machine_data = []
debug_mode = False
IP = "127.0.0.1"
clients_timeout = []

#variables de tiempo
r = 2
j = 2
s = 3

def read_parameters():
    global debug_mode, server_data
    global configuration_file
    global acceptedclients_file

    for parameter in range(1, len(sys.argv)):
        if sys.argv[parameter] == "-c":
            if len(sys.argv) < parameter + 2:
                print_debug("Parameter error, possible parameters:\n\t-c:\tAllows you to specify the file where configuration is stored, followed by the route of the configuration file.\n\t-d:\tActivates the debug mode.\n");
                sys.exit()
            else:
                configuration_file = sys.argv[parameter + 1]
        elif sys.argv[parameter - 1] == "-c":
            pass
        elif sys.argv[parameter] == "-d":
            debug_mode = True
        elif sys.argv[parameter] == "-u":
            if len(sys.argv) < parameter + 2:
                print_debug("Parameter error, possible parameters:\n\t-c:\tAllows you to specify the file where configuration is stored, followed by the route of the configuration file.\n\t-d:\tActivates the debug mode.\n\t-u:\tAllows you to specify the file where authorized machines data is stored, followed by the route of the authorized machines file.");
                sys.exit()
            else:
                acceptedclients_file = sys.argv[parameter + 1]
        elif sys.argv[parameter - 1] == "-u":
            pass
        else:
            print_debug("Parameter error, possible parameters:\n\t-c:\tAllows you to specify the file where configuration is stored, followed by the route of the configuration file.\n\t-d:\tActivates the debug mode.\n\t-u:\tAllows you to specify the file where authorized machines data is stored, followed by the route of the authorized machines file.")
            sys.exit()
read_parameters()

def print_debug(message_debug):
    if debug_mode == True:
        print (str(datetime.now().time())[:8] + ": " + "DEBUG " + "=> " + message_debug)
def print_message(message):
    if debug_mode == True:
        print (str(datetime.now().time())[:8] + ": " + "MSG. " + "=> " + message)

def set_parameters(file):
    global configuration_file
    with open(file) as f:
        server_data = f.readlines()
    server_data = [x.strip() for x in server_data]
    for line in range(len(server_data)):
        if server_data[line] == "":
            server_data.pop(line)
    for line in range(len(server_data)):
        server_data[line] = server_data[line].split(" ")


    name, MAC, udp_port, tcp_port = server_data[0][1], server_data[1][1], server_data[2][1], server_data[3][1]


    return name, MAC, udp_port, tcp_port



def initialize_machine_data(file):
    global machine_data
    global acceptedclients_file
    with open(acceptedclients_file) as f:
        machine_data = f.readlines()
    machine_data = [x.strip() for x in machine_data]

    # Transform each line of machine_data
    for i in range(len(machine_data)):
        if machine_data[i] == "":
            continue
        machine_data[i] = machine_data[i].split(" ")
        machine_data[i].insert(0, DISCONNECTED)
        machine_data[i][1] = machine_data[i][1].ljust(7, "\0")
        machine_data[i][2] += "\0"
        machine_data[i] += ["000000\0"]
        machine_data[i].append(0)
        machine_data[i].append(0) #tiempo en el que se ha recibido el ultimo paquete
        machine_data[i].append(0) #numero de paqutes alives perdidos

def read_commands():
    command = input("")
    if command.startswith("quit"):
        print_debug("End of request")
        print_debug("Control alives timer canceled")
        sock_udp.close()
        sys.exit()
    elif command == "list":
        print_debug("Showing the state of the authorized clients")
        print("STATE\t\tNAME\t\tMAC\t\tRANDOM_NUMBER\t\tIP\n")
        for client in machine_data:
            if client[0] == "ALIVE":
                print(f"{client[0]}\t\t{client[1]}\t\t{client[2]}\t{client[3]}\t\t\t{client[4]}\n")
            else:
                print(f"{client[0]}\t{client[1]}\t\t{client[2]}\t{client[3]}\t\t\t{client[4]}\n")
    else:
        print_debug("Invalid command")


def create_package(package_type, random_number, data):
    if package_type == REGISTER_ACK:
        package = struct.pack('B',package_type) + bytes(name + '\0' + MAC + "\0" + random_number + data + "\0", 'utf-8') + struct.pack('78B',*([0]* 78))
    elif package_type == REGISTER_NACK:
        package = struct.pack('B',package_type) + bytes("\0\0\0\0\0\0\0" + "000000000000" + "\0" + "000000" + "\0" + data + "\0", 'utf-8') + struct.pack('78B',*([0]* 78))
    elif package_type == REGISTER_REJ:
        package = struct.pack('B',package_type) + bytes("\0\0\0\0\0\0\0" + "000000000000" + "\0" + "000000" + "\0" + data + "\0", 'utf-8') + struct.pack('78B',*([0]* 78))
    elif package_type == ALIVE_ACK:
        package = struct.pack('B',package_type) + bytes(name + "\0" + MAC + "\0" + random_number + data + "\0", 'utf-8')  + struct.pack('78B',*([0]* 78))
    elif package_type == ALIVE_NACK:
        package = struct.pack('B',package_type) + bytes("\0\0\0\0\0\0\0" + "000000000000" + "\0" + "000000" + "\0" + data + "\0", 'utf-8') + struct.pack('78B',*([0]* 78))
    elif package_type == ALIVE_REJ:
        package = struct.pack('B',package_type) + bytes("\0\0\0\0\0\0\0" + "000000000000" + "\0" + "000000" + "\0" + data + "\0", 'utf-8') + struct.pack('78B',*([0]* 78))
    return package

### FUNCION NUMERO ALEATORIO
def generate_random():
    random_number = str(random.randint(0,9))
    for x in range(0,5):
        random_number = random_number + str(random.randint(0,9))
    return random_number

def get_clock_seconds():
    return int(str(datetime.now().time())[0:2])*3600 + int(str(datetime.now().time())[3:5])*60 + int(str(datetime.now().time())[6:8])
def manage_package (package, addr):
    global machine_data
    if package[0] == REGISTER_REQ:
        print_debug("Receive: REGISTER_REQ")
        for machine in range(len(machine_data)):
            if package[1:8] == bytes(machine_data[machine][1], 'utf-8'):
                if package[8:21] == bytes(machine_data[machine][2], 'utf-8'):
                    if package[21:28] == bytes(machine_data[machine][3], 'utf-8'):
                        if machine_data[machine][0] == DISCONNECTED:
                            machine_data[machine][0] = REGISTERED
                            machine_data[machine][4] = addr[0] # IP
                            machine_data[machine][3] = generate_random()+'\0'
                            machine_data[machine][5] = get_clock_seconds()
                            machine_data[machine][6] = 0 # aunque ya esta a 0 por defecto
                            print_debug("Registered Accepted")
                            print_debug("REGISTERED")
                            package = create_package(REGISTER_ACK, machine_data[machine][3], tcp_port)
                            sock_udp.sendto(package, addr)

                            print_debug(f"REGISTER_ACK sent to {addr[0]}")
                            return

                        elif machine_data[machine][0] == REGISTERED or machine_data[machine][0] == SEND_ALIVE:
                            package = create_package(REGISTER_ACK, machine_data[machine][3], tcp_port)
                            sock_udp.sendto(package, addr)
                            print_debug(f"REGISTER_ACK sent to {addr[0]}")
                            return
                        else:
                            package = create_package(REGISTER_REJ, "000000\0", "device unauthorized")
                            sock_udp.sendto(package, addr)
                            print_debug(f"REGISTER_REJ sent to {addr[0]}")
                            return
                    else:
                        package = create_package(REGISTER_NACK, "000000\0", "Wrong random number")
                        sock_udp.sendto(package, addr)
                        print_debug(f"REGISTER_NACK sent to {addr[0]}")
                        return
                else:
                    package = create_package(REGISTER_NACK, package[9:21], "Wrong IP")
                    sock_udp.sendto(package, addr)
                    print_debug(f"REGISTER_NACK sent to {addr[0]}")
                    return

        package = create_package(REGISTER_REJ, "000000\0", "device unauthorized")
        sock_udp.sendto(package, addr)
        print_debug(f"REGISTER_REJ sent to {addr[0]}")
        return

    elif package[0] == ALIVE_INF:
        print_debug("ALIVE_INF received")
        for machine in range(len(machine_data)):
            if package[1:8] == bytes(machine_data[machine][1], 'utf-8'):
                if package[8:21] == bytes(machine_data[machine][2], 'utf-8'):
                    if package[21:28] == bytes(machine_data[machine][3], 'utf-8'):
                        if machine_data[machine][0] == REGISTERED or machine_data[machine][0] == SEND_ALIVE :
                            if machine_data[machine][4] == addr[0]:
                                if machine_data[machine][0] == REGISTERED:
                                    print_message ("Client " + str(machine + 1) + ": State changed from REGISTERED to ALIVE")
                                    machine_data[machine][0] = SEND_ALIVE
                                machine_data[machine][5] = get_clock_seconds()
                                machine_data[machine][6] = 0
                                package = create_package(ALIVE_ACK, machine_data[machine][3], "")
                                sock_udp.sendto(package, addr)
                                print_debug(f"ALIVE_ACK sent to {addr[0]}")

                                return
                        else:
                            package = create_package(ALIVE_REJ, "000000\0", "device not registered")
                            sock_udp.sendto(package, addr)
                            print_debug(f"ALIVE_REJ sent to {addr[0]}")
                            return
                    else:
                        package = create_package(ALIVE_NACK, "000000\0","wrong random number")
                        sock_udp.sendto(package, addr)
                        print_debug(f"ALIVE_NACK sent to {addr[0]}")
                        return
                else:
                    package = create_package(ALIVE_REJ, "000000\0", "device unauthorized, wrong MAC ")
                    sock_udp.sendto(package, addr)
                    print_debug(f"ALIVE_REJ sent to {addr[0]}")
                    return
        print_debug("ALIVE_INF from an unauthorized client")
        print_debug("Send an ALIVE_REJ")

        package = create_package(ALIVE_REJ, machine_data[machine][3],"received from an unauthorized client, wrong id" )
        sock_udp.sendto(package, addr)
        return


#temporización primer ALIVE
#Perdida de ALIVES
def timout_alives():
    global machine_data
    while (1):
        time.sleep(0.1)
        for x in range (len(machine_data)):
            if machine_data[x][0] == REGISTERED:
                time.sleep(0.1)
                if get_clock_seconds() - machine_data[x][5] >= r:
                    if machine_data [x][6] >= j:
                        print_message("Device " + str(x + 1) + ": State changed from ALIVE to DISCONNECTED (Don't receive 3 ALIVE consecutives")
                        machine_data[x][0] = DISCONNECTED
                        machine_data[x][4] = ""
                        machine_data[x][5] = 0

                    else:
                        machine_data[x][6] = machine_data[x][6] + 1
                        machine_data[x][5] = get_clock_seconds()

            elif machine_data[x][0] == SEND_ALIVE:
                time.sleep(0.1)
                if get_clock_seconds() - machine_data[x][5] >= r:
                    if machine_data [x][6] >= s:
                        print_message("Device " + str(x + 1) + ": State changed from ALIVE to DISCONNECTED (Don't receive 3 ALIVE consecutives")
                        machine_data[x][0] = DISCONNECTED
                        machine_data[x][4] = ""
                        machine_data[x][5] = 0

                    else:
                        machine_data[x][6] = machine_data[x][6] + 1
                        machine_data[x][5] = get_clock_seconds()


if __name__ == '__main__':

    name, MAC, udp_port, tcp_port = set_parameters(configuration_file)
    read_parameters()
    set_parameters(configuration_file)
    initialize_machine_data(acceptedclients_file)
    sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock_udp.bind((IP, int(udp_port)))
    alives_timeout_thread = threading.Thread(target=timout_alives).start()

    while (1):
        readable, writable, exceptional = select.select([sock_udp, sys.stdin], [], [])
        for descriptor in readable:
            if descriptor is sock_udp:
                message, addr = sock_udp.recvfrom(78)
                manage_package(message, addr)
            elif descriptor is sys.stdin:
                read_commands()
