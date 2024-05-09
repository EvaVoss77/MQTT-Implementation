#include "Client.h"

int main(int argc, char *argv[]){

    if (argc != 4) {
       printf("Incorrect entry of arguments.\n");
       exit(0);
    }
    std::string ClientID = argv[1];
    std::string topicpublish = argv[2];
    std::string appmess = argv[3];

    Client client(ClientID, "Eva", "1245678");
    client.CONNECTION();
    client.PUBLISHING(topicpublish, appmess, 0);
    client.DISCONNECT();

    return 0;
}