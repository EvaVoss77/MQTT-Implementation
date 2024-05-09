#ifndef MQTT_H_
#define MQTT_H_ 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

using namespace std;

/*MQTT message types*/
typedef enum : uint8_t{
    Reserved1,
    CONNECT,
    CONNACK,
    PUBLISH,
    PUBACK,
    PUBREC,
    PUBREL, 
    PUBCOMP,
    SUBSCRIBE,
    SUBACK,
    UNSUBSCRIBE, 
    UNSUBACK,
    PINGREQ,
    PINGRESP,
    DISCONNECT,
    Reserved2
}PacketType;

/*Class Message which contains the common elements of each message type*/
class Message{
    protected:
        int len;                             // message length
        int VHeader_Pyload_len;              // variable header + payload length

        /*Fixed Header*/
        PacketType MQTT_ControlPacket_type;  // message type 
        uint8_t  Flags_MQTT;                 // MQTT flags
        char RemainingLength[4];             // the Remaining Length is the number of bytes remaining within the current packet, including data in the 258 variable header and the payload.

    public:

        int encodeLength(uint32_t len, uint8_t *buff);  // variable length encoding scheme
        uint32_t decodeLen(uint8_t *p, int *codeLen);   //  variable length decoding scheme

        int encodeString(std::string string, uint8_t *buffer, int idx); // string encoding
        std::string decodeString(uint8_t *buff, int *plen);             // string decoding

        int readFromInto(int socket, uint8_t *b, int maxSz);

        int getLength();

        /*Function to put on a buffer each byte of the messager*/
        int package(uint8_t *buffer);                   
        /*Function to extract on a buffer each byte of the messager*/
        int unpackage(uint8_t *buffer);
};

/*Subclass CONNECT*/
class CONNECT_msg: public Message {
    protected:
        /*VariableHeader*/
        uint8_t ProtocolNameBytes[6];
        uint8_t ProtocolLevelByte;
        uint8_t connect_flags;               // 1 byte
        uint16_t Keep_Alive;                 // 2 byte

        /*Payload*/                         
        std::string Client_ID;              // Client_ID_len bytes
        uint8_t  *Will_Topic;               // if the WillFlag=1
        uint8_t  *Will_Msg;                 // if the WillFlag=1
        std::string UserName;              // if UserNameFlag=1
        std::string Password;              // if PasswordFlag=1

        static const uint8_t HasUser = 1 << 7; 
        static const uint8_t HasPassword = 1 << 6; 

    public:
        CONNECT_msg(std::string clientid, std::string username, std::string password);

        CONNECT_msg();

        int set_VariableHeader(uint8_t *buffer);
        int get_VariableHeader(uint8_t *buffer);

        int set_Payload(uint8_t *buffer);
        int get_Payload(uint8_t *buffer);
        
        int package(uint8_t *buffer);
        int unpackage(uint8_t *buffer);

        std::string getClient_ID();              
        std::string getUserName();              
        std::string getPassword();             
};

/*Subclass CONNACK*/
class CONNACK_msg : public Message {
    protected:
        bool ConnectAcknowledgeFlags;   //1 byte
        uint8_t ConnectReturnCode;      //1 byte

    public:

        CONNACK_msg(bool caf, uint8_t crc);

        int set_VariableHeader(uint8_t *buffer);
        int get_VariableHeader(uint8_t *buffer);

        int package(uint8_t *buffer);
        int unpackage(uint8_t *buffer);
};

/*Subclass PUBLISH*/
class PUBLISH_msg: public Message{

    protected:
        std::string TopicName;
        uint16_t PacketIdentifier;
        std::string ApplicationMessage;

        static const uint8_t HasDUP = 1 << 3; /*0: primera vez que se intenta enviar*/
        static const uint8_t HasQoS_Level = (1 << 2) + (1 << 1); 
        static const uint8_t HasRETAIN = 1; /*Si el indicador RETAIN se establece en 1, en un paquete PUBLISH enviado por un cliente a un servidor, el servidor DEBE almacenar el mensaje de aplicación y su QoS, para que pueda entregarse a futuros suscriptores cuyas suscripciones coincidan con su nombre de tema*/

    public:
        PUBLISH_msg();

        PUBLISH_msg(std::string topicname, std::string appmessage, uint16_t pi,
                        bool dup, int qos, bool retain);

        int set_VariableHeader(uint8_t *buffer);
        int get_VariableHeader(uint8_t *buffer);
        
        int set_Payload(uint8_t *buffer);
        int get_Payload(uint8_t *buffer);

        int package(uint8_t *buffer);
        int unpackage(uint8_t *buffer);

        std::string getTopicName();
        uint16_t getPacketIdentifier();
        std::string getApplicationMessage();

};

/*Subclass PUBREC*/
class PUBREC_msg: public Message{

};

/*Subclass SUBSCRIBE*/
class SUBSCRIBE_msg: public Message{

    protected:
        uint16_t PacketIdentifier;
        vector<std::string> SubscribedTopic;
        vector<int> QoS;

    public: 
        SUBSCRIBE_msg(uint16_t pi, vector<std::string> subscribedtopic, vector<int> qos);
        SUBSCRIBE_msg();

        int set_VariableHeader(uint8_t *buffer);
        int get_VariableHeader(uint8_t *buffer);

        int set_Payload(uint8_t *buffer);
        int get_Payload(uint8_t *buffer);

        int package(uint8_t *buffer);
        int unpackage(uint8_t *buffer);

        uint16_t getPacketIdentifier();
        void getTopicSubscribe(vector<std::string> *topics);
        void getQoS(vector<int> *qos);

};

/*Subclass SUBACK*/
class SUBACK_msg: public Message{
    protected:
        uint16_t PacketIdentifier;
        vector<int> ReturnCodes;

    public:
        SUBACK_msg(uint16_t pi, vector<int> retcod);

        int set_VariableHeader(uint8_t *buffer);
        int get_VariableHeader(uint8_t *buffer);

        int set_Payload(uint8_t *buffer);
        int get_Payload(uint8_t *buffer);
        
        int package(uint8_t *buffer);
        int unpackage(uint8_t *buffer);
};

/*Subclass UNSUBSCRIBE*/
class UNSUBSCRIBE_msg: public Message{
    protected:
        uint16_t PacketIdentifier;
        vector<std::string> UnsubscribedTopic;

    public:
        UNSUBSCRIBE_msg(uint16_t pi, vector<std::string> unsubstop);
        UNSUBSCRIBE_msg();
        
        int set_VariableHeader(uint8_t *buffer);
        int get_VariableHeader(uint8_t *buffer);

        int set_Payload(uint8_t *buffer);
        int get_Payload(uint8_t *buffer);

        int package(uint8_t *buffer);
        int unpackage(uint8_t *buffer);

        uint16_t getPacketIdentifier();
        void getTopicUnsubscribe(vector<std::string> *topics);
};

/*Subclass UNSUBACK*/
class UNSUBACK_msg: public Message{
    protected:
        uint16_t PacketIdentifier;

    public:
        UNSUBACK_msg(uint16_t pi);

        int set_VariableHeader(uint8_t *buffer);
        int get_VariableHeader(uint8_t *buffer);
        
        int package(uint8_t *buffer);
        int unpackage(uint8_t *buffer);
};

/*Subclass DISCONNECT*/
class DISCONNECT_msg: public Message {
    public:
        DISCONNECT_msg();
        int package(uint8_t *buffer);
        int unpackage(uint8_t *buffer);
};

/*Subclass PINGREQ*/
class PINGREQ_msg: public Message {
    public:
        PINGREQ_msg();
        int package(uint8_t *buffer);
        int unpackage(uint8_t *buffer);
};

/*Subclass PINGRESP*/
class PINGRESP_msg: public Message {
    public:
        PINGRESP_msg();
        int package(uint8_t *buffer);
        int unpackage(uint8_t *buffer);
};


#endif /*MQTT_H_*/