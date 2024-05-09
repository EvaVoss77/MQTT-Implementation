#include <mutex>
#include <condition_variable>
#include <list>
#include "MQTT.h"

using namespace std;

#define SERV_TCP_PORT  1883
#define SERV_HOST_ADDR "128.4.40.86"
#define MAX_SIZE_MSG 1024               

/*Client struct which contain the information client*/
struct Client_t
{
    struct sockaddr_in cli_addr;
    int cs;
    
    list<struct TopicVal *> TopicList;
    
    std::string Client_ID;
    std::string UserName;
    std::string Password;
};

/*MQTT topic*/
struct TopicVal{
    std::string TopicName;
    std::string Value;
    list<struct Client_t *> ClientList;

    bool retain;
};

/*Class server MQTT*/
class Broker{

    protected:
        struct sockaddr_in serv_addr;
        int ss;
        mutex mtx;
        condition_variable cv;

        list<struct TopicVal *> TopicList;
        list<struct Client_t *> ClientsList;
        vector<thread> ThreadsList;

    public:

        Broker(){
            cout << "Initializing the server..." <<endl;
            ss = socket(AF_INET, SOCK_STREAM, 0);
            if(ss < 0){
                perror("Error initializing ss.");
            }

            bzero((char *) &serv_addr, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = INADDR_ANY;
            serv_addr.sin_port = htons(SERV_TCP_PORT);
            
            int val = 1;
            /*Send the initial options to the ss socket*/
            if(setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)))
                perror("Error loading options in ss.");
            if (bind(ss, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
                perror("Error in the bind.");
        }

        ~Broker(){
            for( auto &t : ThreadsList)
                t.join();
        }

        void AddClient(int cs, sockaddr_in cli_addr){
            /*memory allocation*/
            Client_t *client = new Client_t;
            client->cli_addr = cli_addr;
            client->cs = cs;
            ClientsList.emplace_back(client);
            /*I create a thread for each client that connects*/
            ThreadsList.emplace_back([&, client]() {ServeClient(client);});
        }

        void DeleteClient(Client_t *c){
            /*Remove it from subscribed topics*/
            for(auto t : c->TopicList){
                t->ClientList.remove(c);
            }
            /*Remove from customer vector*/
            ClientsList.remove(c);
        }

       void AddTopic(std::string TopicName){
            TopicVal *t = new TopicVal;
            t->TopicName = TopicName;
            TopicList.push_back(t);
        }
        

        void Management(){
            listen(ss, 5);
            cout << "Server listening..." << endl;
        
            while( true ) {
                
                sockaddr_in cli_addr;
                socklen_t clilen;
                clilen = sizeof(cli_addr);

                int cs = accept(this->ss, (struct sockaddr *) &cli_addr, &clilen);
                if(cs < 0){
                    perror("Error in accept().");
                    close(cs);
                }else{
                    /*Add the accepted client to the list and start a thread to serve it*/
                    AddClient(cs, cli_addr);
                }
            }
        }

        void ServeClient(Client_t *c){
            bool cond = true;
            while(cond){
                cout << endl;
                /*Recive the message and save it in BUFF*/
                uint8_t BUFF[MAX_SIZE_MSG];
                Message msg;
                int n = msg.readFromInto(c->cs, BUFF, MAX_SIZE_MSG);
                if(n < 0)
                    perror("Error al leer");
                    
                /*Get message type*/
                uint8_t type = BUFF[0]>>4;
                PacketType TYPE = static_cast<PacketType>(type);
                uint8_t FLAGS = BUFF[0] & 0xf;

                switch(TYPE){
                    case CONNECT: {
                            cout<<"CONNECT MESAGE"<<endl;
                            CONNECT_msg msg_connect;
                            msg_connect.unpackage(&BUFF[0]);

                            /*Get client information*/
                            unique_lock<mutex> lck{mtx};
                            c->Client_ID = msg_connect.getClient_ID();
                            c->UserName = msg_connect.getUserName();
                            c->Password = msg_connect.getPassword();
                            
                            cv.notify_all();

                            /*Responde con un CONNACK*/
                            CONNACK_msg msg_connack(0,0);
                            msg_connack.package(&BUFF[0]);
                            int w = write(c->cs, BUFF, msg_connack.getLength());
                            if(w < 0)
                                perror("Error al escribir CONNACK");
                            break;
                        }
                    case PUBLISH: {
                            cout<<"PUBLISH MESAGE"<<endl;
                            PUBLISH_msg msg_publish;
                            msg_publish.unpackage(&BUFF[0]);

                            std::string AppMsg = msg_publish.getApplicationMessage();
                            std::string TopNam = msg_publish.getTopicName();
                            bool retain = FLAGS & 1;
                            
                            /*It sends PUBACK to the client that published in case of QoS level 1 -- incomplete functionality */
                            /*It sends PUREC to the client that I publish in case of QoS level 2 -- incomplete functionality  */
                            
                            /*Update of the topic in subscribed clients*/
                            unique_lock<mutex> lck{mtx};
                            int cont = 0;
                            for(auto t : TopicList){
                                if(t->TopicName == TopNam){
                                     t->Value = AppMsg;
                                     /*Publish to all clients subscribed to that topic*/
                                     for(auto cnt : t->ClientList){
                                        PUBLISH_msg msg_pub(TopNam, AppMsg, 0, 0, 0, 1);
                                        msg_pub.package(&BUFF[0]);
                                        int w = write(cnt->cs, BUFF, msg_pub.getLength());
                                        if(w < 0)
                                            perror("Error to read PUBLISH");
                                     }
                                     printf("   Actualized topic: %s\n", t->TopicName.c_str());
                                     printf("   Actualized value: %s\n", t->Value.c_str());
                                }else{cont++;}
                            }
                            if(cont == TopicList.size()){
                                /*in case of the case does no exist it create it*/
                                TopicVal *t = new TopicVal;
                                t->TopicName = TopNam;
                                t->Value = AppMsg;
                                t->retain = retain;
                                TopicList.push_back(t);
                                printf("    Topic created in publishing: %s\n", t->TopicName.c_str());
                            }
                            cv.notify_all();

                        break;
                    }
                    case SUBSCRIBE: {
                            cout<<"SUBSCRIBE"<<endl;
                            SUBSCRIBE_msg msg_subscribe;
                            msg_subscribe.unpackage(&BUFF[0]);
                            /*get information of the subscription*/
                            vector<std::string> name;
                            msg_subscribe.getTopicSubscribe(&name);

            
                            for(auto &n : name){
                                int cond_create = 0;
                                /*It subscribe the client in the existing topics*/
                                unique_lock<mutex> lck{mtx};
                                for(auto t : TopicList){
                                    if(t->TopicName == n){
                                        c->TopicList.push_back(t);
                                        t->ClientList.push_back(c);         
                                        printf("    The client %d subscribe at topic %s\n", c->cs,t->TopicName.c_str());           
                                        /*It public the last message actualization on client subscripted.*/
                                        if(t->retain){
                                            PUBLISH_msg msg_pub(t->TopicName, t->Value, 0, 0, 0, 1);
                                            msg_pub.package(&BUFF[0]);
                                            int w = write(c->cs, BUFF, msg_pub.getLength());
                                            if(w < 0)
                                                perror("Error to read PUBLISH");
                                            cond_create = 1;
                                            printf("    Actualized topic: %s\n", t->TopicName.c_str());
                                            printf("    Actualized value: %s\n", t->Value.c_str());
                                        }
                                    }
                                }
                                /*in case of the topic does no exist it create it and it subscribe the client*/
                                if(cond_create == 0){
                                    TopicVal *t = new TopicVal;
                                    t->TopicName = n;
                                    t->ClientList.push_back(c);
                                    c->TopicList.push_back(t);     
                                    TopicList.push_back(t);
                                    printf("    Topic created in subscription: %s\n", n.c_str());
                                }
                            }
                            cv.notify_all();

                            /*It respond a SUBACK*/
                            vector<int> retcod;
                            for(int i=0; i<name.size(); i++){ //-- If I made the complete protocol I would have to verify what to send here
                                retcod.push_back(0);
                            }                
                            SUBACK_msg msg_suback(msg_subscribe.getPacketIdentifier(), retcod);
                            msg_suback.package(&BUFF[0]);
                            int w = write(c->cs, BUFF, msg_suback.getLength());
                            if(w < 0)
                                perror("Error to read SUBACK");

                            
                        break;
                    }

                    case UNSUBSCRIBE: {
                            cout<<"UNSUBSCRIBE"<<endl;
                            UNSUBSCRIBE_msg msg_unsubscribe;
                            msg_unsubscribe.unpackage(&BUFF[0]);

                            /*Unsubscribe the client from the requested topics*/
                            vector<std::string> name;
                            msg_unsubscribe.getTopicUnsubscribe(&name);

                            unique_lock<mutex> lck{mtx};
                            for(auto &n : name){
                                 for(auto t : TopicList){
                                    if(t->TopicName == n){
                                        try{
                                            c->TopicList.remove(t);
                                            t->ClientList.remove(c);
                                        }catch(...){
                                            cout << "   The client is not subscribed to the topic "<< n <<endl;
                                        }
                                    }
                                }
                            }
                            /*It resond a UNSUBACK*/
                            UNSUBACK_msg msg_unsuback(msg_unsubscribe.getPacketIdentifier());
                            msg_unsuback.package(&BUFF[0]);
                            int w = write(c->cs, BUFF, msg_unsuback.getLength());
                            if(w < 0)
                                perror("Error to read SUBACK");
                        break;
                    }
                    case DISCONNECT: {
                            cout<<"DISCONNECT MESAGE"<<endl;
                            /*It delete of the list clients*/
                            DeleteClient(c);
                            /*condition to finish the thread*/
                            cond = false;
                        break;
                    }
                    default: 
                            //cout<<"DEFOULT"<<endl;
                        break;
                }
            }
        }
};