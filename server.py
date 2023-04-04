#!/usr/bin/env python3

import sys


configuration_file = "server.cfg"
acceptedclients_file = "equips.dat"


machine_data = []

debug_mode = False



def read_parameters():
    global debug_mode, server_data
    global configuration_file
    global acceptedclients_file

    for parameter in range(1, len(sys.argv)):
        if sys.argv[parameter] == "-c":
            if len(sys.argv) < parameter + 2:
                print(
                    "Parametro erroneo, posibles parametross:\n\t-c:\tEspecifica el nombre del archivo de donde se leerán los datos necesarios para la comunicación entre cliente y servidor\n\t-d:\tActiva el debug mode.\n");
                sys.exit()
            else:
                configuration_file = sys.argv[parameter + 1]
        elif sys.argv[parameter - 1] == "-c":
            pass
        elif sys.argv[parameter] == "-d":
            debug_mode = True
        elif sys.argv[parameter] == "-u":
            if len(sys.argv) < parameter + 2:
                print(
                    "Parametro erroneo, posibles parametross:\n\t-c:\tEspecifica el nombre del archivo de donde se leerán los datos necesarios para la comunicación entre cliente y servidor\n\t-d:\tActiva el debug mode.\n");

                sys.exit()
            else:
                acceptedclients_file = sys.argv[parameter + 1]
        elif sys.argv[parameter - 1] == "-u":
            pass
        else:
            sys.exit()
read_parameters()

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
    print("Datos del servidor extraidos del archivo ")
    for line in server_data:
        print(line)

    name, MAC, udp_port, tcp_port = server_data[0][1], server_data[1][1], server_data[2][1], server_data[3][1]

    print("\nInformación del servidor:")
    print("Nombre: ", name)
    print("MAC: ", MAC)
    print("Puerto UDP: ", udp_port)
    print("Puerto TCP: ", tcp_port)
    return name, MAC, udp_port, tcp_port

def initialize_machine_data(file):
    global machine_data
    global acceptedclients_file
    with open(acceptedclients_file) as f:
        machine_data = f.readlines()
    machine_data = [x.strip() for x in machine_data]
    for line in range(len(machine_data)):
        if machine_data[line] == "":
            machine_data.pop(line)
    for line in range(len(machine_data)):
        machine_data[line] = machine_data[line].split(" ")
    for i in range (len(machine_data)):
        machine_data[i].insert(0, "DISCONNECTED")
        while len(machine_data[i][1]) < 7:
            machine_data[i][1] = "\0" + machine_data[i][1]
        machine_data[i][2] = machine_data[i][2] + "\0"
        machine_data[i].append("000000\0")
        machine_data[i].append("")
    initialize_machine_data(acceptedclients_file)



    print("Datos de las máquinas extraidos del archivo ")
    for line in machine_data:
        print(line)

    return machine_data


if __name__ == '__main__':
    set_parameters(configuration_file)
    initialize_machine_data(acceptedclients_file)