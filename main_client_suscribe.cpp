#include "Client.h"

int main(int argc, char *argv[]){

    if (argc != 3) {
       printf("Incorrect entry of arguments.\n");
       exit(0);
    }
    std::string ClientID = argv[1];
    vector<std::string> topicsuscribe;
    topicsuscribe.push_back(argv[2]);
    vector<int> qos{0};

    Client client(ClientID, "Eba", "contraseña");
    client.CONNECTION();
    client.SUSCRIPTION(topicsuscribe, 1, qos);
    client.UNSUSCRIBE({"Temp", "Hum", "Radiacion"}, 0);
    client.DISCONNECT();

    return 0;
}