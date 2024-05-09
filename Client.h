#include <netdb.h>
#include "MQTT.h"

using namespace std;

#define SERV_TCP_PORT  1883
#define SERV_HOST_ADDR "128.4.40.86"
#define MAX_SIZE_MSG 50              



class Client{
    protected:
        std::string Topic;
        std::string UserName;
        std::string Password;
        std::string ID; 
        int sockfd;
        struct sockaddr_in serv_addr;

    public:

        ~Client(){
            DISCONNECT();
        }
        Client(std::string id, std::string username, std::string password){

            UserName = username;
            ID = id;
            Password = password;

            int portno, n;
            struct sockaddr_in serv_addr;
            struct hostent *server;

            portno = SERV_TCP_PORT;
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) 
                perror("Error opening client socket.");
            server = gethostbyname("localhost");
            if (server == NULL) {
                fprintf(stderr,"ERROR, no such host\n");
                exit(0);
            }   
            bzero((char *) &serv_addr, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
                
            serv_addr.sin_port = htons(portno);

            if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
                perror("Connection error.");
        }

        void send(int Msg_len, uint8_t *buff){

            int w = write(sockfd,buff,Msg_len);
            if (w < 0) 
                perror("Error on the client to send\n");
        }
        void recive(uint8_t *buff){        
            int r = read(sockfd,buff,MAX_SIZE_MSG);
            if (r < 0) 
                perror("Error on the client to read\n");
        }

        void CONNECTION(){
            cout << "Client connecting..."<< endl;
            CONNECT_msg msg(ID, UserName, Password);       
            uint8_t buff[MAX_SIZE_MSG];

            msg.package(&buff[0]);
            send(msg.getLength(),&buff[0]);

            recive(&buff[0]);
            PacketType TYPE = static_cast<PacketType>(buff[0]>>4);

            if(TYPE != CONNACK){
                DISCONNECT();
            }
            cout << "Successful connection."<< endl;
        }
        void SUSCRIPTION(vector<std::string> topicsuscribe, uint16_t pi, vector<int> qos){
            cout << "Client subscribing..."<< endl;
            SUBSCRIBE_msg msg(pi, topicsuscribe, qos);

            uint8_t buff_s[MAX_SIZE_MSG];
            msg.package(&buff_s[0]);
            send(msg.getLength(), &buff_s[0]);

            int i = 0;
            while(i < 5){
                uint8_t buff[MAX_SIZE_MSG];
                recive(&buff[0]);
                PacketType TYPE = static_cast<PacketType>(buff[0]>>4);

                switch (TYPE){
                    case PUBLISH: {
                        PUBLISH_msg msg_pub;
                        msg_pub.unpackage(&buff[0]);
                        cout << "   Recive a PUBLISH: "<< endl;
                        cout << "   Topic: " << msg_pub.getTopicName() << " AppMsg: " << msg_pub.getApplicationMessage() << endl;
                        break;}
                    case SUBACK: {
                        cout << "   Recive un SUBACK"<< endl;
                        break;}
                    default: {
                        cout << "   Recive a wrong menssage"<< endl;
                        DISCONNECT();
                        i=5;
                        break;}
                }
                i++;
            }
        }
        void DISCONNECT(){
            cout << "Client disconnecting..."<< endl;
            DISCONNECT_msg msg;
            uint8_t buff[MAX_SIZE_MSG];
            msg.package(&buff[0]);
            send(msg.getLength(), &buff[0]);
        }
        void PUBLISHING(std::string topicname, std::string appmessage, uint16_t pi){
            cout << "Client publishing..."<< endl;
            PUBLISH_msg msg(topicname, appmessage, pi, 0, 0, 1);
            uint8_t buff[MAX_SIZE_MSG];
            msg.package(&buff[0]);
            int Msg_len = msg.getLength();
            send(Msg_len, &buff[0]);
        }
        void PING(){
            cout << "Ping request of client..."<< endl;
            PINGREQ_msg msg;
            uint8_t buff[2];
            msg.package(&buff[0]);
            int Msg_len = msg.getLength();
            send(Msg_len, &buff[0]);

            recive(&buff[0]);
            PacketType TYPE = static_cast<PacketType>(buff[0]>>4);  
            if(TYPE == PINGRESP){
                cout << "Recive ping response" << endl;
            }
        }
        void UNSUSCRIBE(vector<std::string> topicunsuscribe, uint16_t pi){
            cout << "Client disubscribing..."<< endl;
            UNSUBSCRIBE_msg msg(pi, topicunsuscribe);
            uint8_t buff[MAX_SIZE_MSG];
            msg.package(&buff[0]);

            send(msg.getLength(), &buff[0]);

            /*Espera la respuesta UNSUBACK*/  
            recive(&buff[0]);
            PacketType TYPE = static_cast<PacketType>(buff[0]>>4);  

            if(TYPE == UNSUBACK){
                cout << "Successful disubscription" << endl;
            }else{
                cout << "Error to disubscribing" <<endl;
            }
        }
};