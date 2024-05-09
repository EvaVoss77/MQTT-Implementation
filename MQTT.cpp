#include "MQTT.h"

/*Declaraciones de clase Message*/

int Message::encodeLength(uint32_t len, uint8_t *buff){
    int idx = 1;
    do {
        uint8_t b = len % 128;
        len = len / 128;
        if(len> 0)
            b |= 128;
        buff[idx++] = b;
    } while(len > 0);
    return idx-1;
}

uint32_t Message::decodeLen(uint8_t *p, int *codeLen){
    int multiplier = 1;
    uint32_t value = 0;
    uint8_t encodebite;
    int idx = 0;
    do{
        encodebite = p[idx];
        value += (encodebite & 127)*multiplier;
        multiplier *= 128;
        assert(idx < 4);
        idx+=1;
    }while (encodebite & 128);
    *codeLen = idx;

    return value;
}

int Message::encodeString(std::string string, uint8_t *buffer, int idx){
    uint16_t N = string.size();

    buffer[idx] = N >> 8;      // MSB: Most Significative Bites
    idx++;
    buffer[idx] = N & 0xf;      //LSB: Less Significative Bites
    idx++;

    for(int i=0; i<N; i++){
        buffer[idx] = (uint8_t) string[i];             
        idx++;
    }
    return idx;
}
std::string Message::decodeString(uint8_t *buff, int *plen){
    std::string ret;
    int idx = 0;
    int len = buff[idx++] << 8;
    len |= buff[idx++];
    for(int i = 0; i < len; i++)
        ret.push_back(buff[idx++]);
    *plen = idx;
    return ret;
}

int Message::readFromInto(int socket, uint8_t *b, int maxSz) {
    Message msg;
    int r = read(socket, b, 1);
    int i = 1;
    do{
        r = read(socket, b+i, 1);
        if(r < 0)
            perror("Error al leer");
        i++;
        assert(i < 5);
    }while((b[i] &  128) == 1);
    int cd;
    int rl = msg.decodeLen(b+1, &cd);
    r = read(socket, b+i, rl);
    return r+i+1;
}

int Message::getLength(){
    int len = VHeader_Pyload_len;
    int encLen = 0;
    do {
        len = len/128;
        encLen +=1;
    } while(len > 0);

    int FHeader_len = 1 + encLen;
    int Msg_len = VHeader_Pyload_len + FHeader_len;

    return Msg_len;
}

int Message::package(uint8_t *buffer){
    int index = 0;
    buffer[index] = this->MQTT_ControlPacket_type << 4;       //Aca tengo que correr los bits para que los dos campos entren en un único byte
    buffer[index++] += this->Flags_MQTT;
            
    int encL = encodeLength(this->VHeader_Pyload_len, buffer);     //Esto va a tirar error por pasarle un puntero como argumento 
    index += encL;
    return index;
}

int Message::unpackage(uint8_t *buffer){
    int index = 0;
    this->MQTT_ControlPacket_type = static_cast<PacketType>(buffer[index]>>4);   
    this->Flags_MQTT = buffer[index++] & 0xf;  

    int CodeLen = 0;
    this->VHeader_Pyload_len = decodeLen(&buffer[index],&CodeLen);
    index+=CodeLen;

    return index;
}

/*Declaraciones de la clase CONNECT_msg*/

CONNECT_msg::CONNECT_msg(std::string clientid, std::string username, std::string password){

    //FixedHeader
    this->MQTT_ControlPacket_type = CONNECT;
    this->Flags_MQTT = 0;

    //VariableHeader
    this->ProtocolNameBytes[0] = 0;
    this->ProtocolNameBytes[1] = 4;
    this->ProtocolNameBytes[2] = 'M';
    this->ProtocolNameBytes[3] = 'Q';
    this->ProtocolNameBytes[4] = 'T';
    this->ProtocolNameBytes[5] = 'T';
    ProtocolLevelByte = 4;

    this->connect_flags = 0;   

    this->Keep_Alive = 0xffff;            

    //Payload      
    this->Client_ID = clientid;        
    this->UserName = username;       
    this->Password = password;

    this->VHeader_Pyload_len = 10;
    this->VHeader_Pyload_len += 2+this->Client_ID.size();

    if(username.size()>0){
        this->connect_flags |= HasUser;
        this->VHeader_Pyload_len += 2+this->UserName.size();
    }
    if(password.size()>0){
        this->connect_flags |= HasPassword; 
        this->VHeader_Pyload_len += 2+this->Password.size();
    }

    this->len = Message::getLength();
}

CONNECT_msg::CONNECT_msg(){
    this->MQTT_ControlPacket_type = CONNECT;
}

int CONNECT_msg::set_VariableHeader(uint8_t *buffer){
    int index = 0;
    for(int i=0; i<6; i++){
        buffer[index++] = this->ProtocolNameBytes[i];
    }
        
    buffer[index++] = this->ProtocolLevelByte;      //It is not necessary define this atribute

    buffer[index++] = this->connect_flags;
    buffer[index++] = this->Keep_Alive >> 8;       //MSB
    buffer[index++] = this->Keep_Alive & 0xf ;     //LSB
    return index;
}

int CONNECT_msg::get_VariableHeader(uint8_t *buffer){
    int index = 0;
    for(int i=0; i<6; i++){
        this->ProtocolNameBytes[i] = buffer[index+i];
    }
    index+=6;

    this->ProtocolLevelByte = buffer[index++];     
    this->connect_flags = buffer[index++];

    this->Keep_Alive = buffer[index++] << 8;      //MSB
    this->Keep_Alive += buffer[index++];          //LSB

    return index;
}

int CONNECT_msg::set_Payload(uint8_t *buffer){
    int index = 0;
    index = Message::encodeString(Client_ID, buffer, index);
            
    //username
    if((this->connect_flags & 128) == 128){                    
        index = Message::encodeString(UserName, buffer, index);
    }
    //password
    if((this->connect_flags & 64) == 64){                   
        index = Message::encodeString(Password, buffer, index);
    }
    return index;
}

int CONNECT_msg::get_Payload(uint8_t *buffer){
    int index = 0;
    int l = 0;
    this->Client_ID = Message::decodeString(&buffer[index], &l);
    index += l; 

    //username
    if((this->connect_flags & 128) == 128){                   
        l = 0;
        this->UserName = Message::decodeString(&buffer[index], &l);
        index += l;
    }

    //password
    if((this->connect_flags & 64) == 64){         
        l = 0;               
        this->Password = Message::decodeString(&buffer[index], &l);
        index += l;
    }
    return index;
}

int CONNECT_msg::package(uint8_t *buffer){
    int index = 0;
    index += Message::package(&buffer[index]);
    index += set_VariableHeader(&buffer[index]);
    index += set_Payload(&buffer[index]);
    return index;
}

int CONNECT_msg::unpackage(uint8_t *buffer)
{
    int index = 0;
    index += Message::unpackage(&buffer[0]);
    index += get_VariableHeader(&buffer[index]);
    index += get_Payload(&buffer[index]);
    return index;
}

std::string CONNECT_msg::getClient_ID(){
    return this->Client_ID;
}             
std::string CONNECT_msg::getUserName(){
    return this->UserName;
}              
std::string CONNECT_msg::getPassword(){
    return this->Password;
} 
/*Funciones de CONNACK*/

CONNACK_msg::CONNACK_msg(bool caf, uint8_t crc){
            //FixedHeader
            this->MQTT_ControlPacket_type = CONNACK;
            this->Flags_MQTT = 0;

            //VariableHeader
            this->ConnectAcknowledgeFlags = caf;
            this->ConnectReturnCode= crc;

            this->VHeader_Pyload_len = 2;
            this->len = 4;
}

int CONNACK_msg::set_VariableHeader(uint8_t *buffer){
    int index = 0;
    buffer[index++] = ConnectAcknowledgeFlags;
    buffer[index++] = ConnectReturnCode;
    return index;
}
int CONNACK_msg::get_VariableHeader(uint8_t *buffer){
    int index = 0;
    ConnectAcknowledgeFlags = buffer[index++];
    ConnectReturnCode = buffer[index++];
    return index;
}

int CONNACK_msg::package(uint8_t *buffer){
    int index = 0;
    index += Message::package(&buffer[0]);
    index += set_VariableHeader(&buffer[index]);
    return index;
}

int CONNACK_msg::unpackage(uint8_t *buffer){
    int index = 0;
    index += Message::unpackage(&buffer[0]);
    index += get_VariableHeader(&buffer[index]);
    return index;
}

/*Funciones de PUBLISH*/

PUBLISH_msg::PUBLISH_msg(){
    this->MQTT_ControlPacket_type = PUBLISH;
}

PUBLISH_msg::PUBLISH_msg(std::string topicname, std::string appmessage, uint16_t pi,
                        bool dup, int qos, bool retain){

            //FixedHeader
            this->MQTT_ControlPacket_type = PUBLISH;
            if(qos == 0){dup = 0;}
            int flg = (dup << 3) + (qos << 1) + retain;
            this->Flags_MQTT = flg;

            //VariableHeader
            this->TopicName = topicname;
            this->VHeader_Pyload_len = 2 + TopicName.size();

            if((this->Flags_MQTT & HasQoS_Level) > 0){
                this->PacketIdentifier = pi;
                this->VHeader_Pyload_len += 2;
            }

            ApplicationMessage = appmessage;
            this->VHeader_Pyload_len += ApplicationMessage.size();
            this->len = Message::getLength();
        }

int PUBLISH_msg::set_VariableHeader(uint8_t *buffer){
    int index = 0;
    /*Cargo TopicName*/
    index = Message::encodeString(this->TopicName, buffer, index);
    
    /*Si QoS > 0 cargo PacketIdent*/
    if((this->Flags_MQTT & HasQoS_Level) > 0){
        buffer[index++] = this->PacketIdentifier >> 8;
        buffer[index++] = this->PacketIdentifier & 0xf;
    }
    return index;
}
int PUBLISH_msg::get_VariableHeader(uint8_t *buffer){
    int index = 0;
    int l = 0;
    this-> TopicName = Message::decodeString(&buffer[index], &l);
    index += l;
    if((this->Flags_MQTT & HasQoS_Level) > 0){
        this->PacketIdentifier = buffer[index++]<<8;
        this->PacketIdentifier += buffer[index++];
    }
    return index;
}

int PUBLISH_msg::set_Payload(uint8_t *buffer){
    int index = 0;
    int N = ApplicationMessage.size();
    for(int i=0; i<N; i++){
        buffer[index++] = ApplicationMessage[i];
    }
    return index;
}
int PUBLISH_msg::get_Payload(uint8_t *buffer){
    int index = 0;
    int N = this->VHeader_Pyload_len - 2 - this->TopicName.size();
    if((this->Flags_MQTT & HasQoS_Level) > 0){
        N-=2;
    }
    for(int i=0; i<N; i++)
        ApplicationMessage.push_back(buffer[index++]);   
    return index;           
}

int PUBLISH_msg::package(uint8_t *buffer){
    int index = 0;
    index += Message::package(&buffer[0]);
    index += set_VariableHeader(&buffer[index]);
    index += set_Payload(&buffer[index]);
    return index;
}
int PUBLISH_msg::unpackage(uint8_t *buffer){
    int index = 0;
    index += Message::unpackage(&buffer[0]);
    index += get_VariableHeader(&buffer[index]);
    index += get_Payload(&buffer[index]);
    return index;
}

std::string PUBLISH_msg::getTopicName(){
    return this->TopicName;
}
uint16_t PUBLISH_msg::getPacketIdentifier(){
    return this->PacketIdentifier;
}
std::string PUBLISH_msg::getApplicationMessage(){
    return this->ApplicationMessage;
}

/*Funciones de SUBSCRIBE*/
SUBSCRIBE_msg::SUBSCRIBE_msg(uint16_t pi, vector<std::string> subscribedtopic, vector<int> qos){
    this->MQTT_ControlPacket_type = SUBSCRIBE;
    this->Flags_MQTT = 0x2;
    /*VariableHeader*/
    this->PacketIdentifier = pi;
    this->VHeader_Pyload_len = 2;
    /*Payload*/
    int i = 0;
    for(auto &s : subscribedtopic){
        SubscribedTopic.push_back(s);
        QoS.push_back(qos.at(i++));
        this->VHeader_Pyload_len += 2 + s.size() + 1;
    }
    this->len = Message::getLength();
}

SUBSCRIBE_msg::SUBSCRIBE_msg(){
    this->MQTT_ControlPacket_type = SUBSCRIBE;
    this->Flags_MQTT = 0x2;
}

int SUBSCRIBE_msg::set_VariableHeader(uint8_t *buffer){
    int index = 0;
    buffer[index++] = PacketIdentifier >> 8;
    buffer[index++] = PacketIdentifier & 0xf; 
    return index;
}
int SUBSCRIBE_msg::get_VariableHeader(uint8_t *buffer){
    int index = 0;
    PacketIdentifier = buffer[index++] << 8;
    PacketIdentifier += buffer[index++];
    return index;
}

int SUBSCRIBE_msg::set_Payload(uint8_t *buffer){
    int index = 0;
    int i = 0;
    for(auto &s : SubscribedTopic){
        index = Message::encodeString(s, buffer, index);
        buffer[index++] = QoS[i++];
    }
    return index;
}
int SUBSCRIBE_msg::get_Payload(uint8_t *buffer){
    int index = 0;
    int i = 0;
    int PayloadLen = VHeader_Pyload_len - 2;
    while(PayloadLen > 0){
        int l = 0;
        std::string s = Message::decodeString(&buffer[index], &l);
        index += l;
        SubscribedTopic.push_back(s);
        QoS.push_back(buffer[index++]);
        PayloadLen = PayloadLen - 3 - s.size();
    }
    return index;
}
   
int SUBSCRIBE_msg::package(uint8_t *buffer){
    int index = 0;
    index += Message::package(&buffer[0]);
    index += set_VariableHeader(&buffer[index]);
    index += set_Payload(&buffer[index]);
    return index;
}

int SUBSCRIBE_msg::unpackage(uint8_t *buffer){
    int index = 0;
    index += Message::unpackage(&buffer[0]);
    index += get_VariableHeader(&buffer[index]);
    index += get_Payload(&buffer[index]);
    return index;
}

uint16_t SUBSCRIBE_msg::getPacketIdentifier(){
    return this->PacketIdentifier;
}
void SUBSCRIBE_msg::getTopicSubscribe(vector<std::string> *topics){
    for(auto &s : SubscribedTopic){
        topics->push_back(s);
    }
}
void SUBSCRIBE_msg::getQoS(vector<int> *qos){
    for(auto &q : QoS){
        qos->push_back(q);
    }
}

/*Funciones de SUBACK*/
SUBACK_msg::SUBACK_msg(uint16_t pi, vector<int> retcod){
    this->MQTT_ControlPacket_type = SUBACK;
    this->Flags_MQTT = 0;

    /*VariableHeader*/
    this->PacketIdentifier = pi;
    this->VHeader_Pyload_len = 2; 

    /*Payload*/
    int i = 0;
    for(auto &c : retcod){
        ReturnCodes.push_back(retcod.at(i++));
        VHeader_Pyload_len += 1;
    }
    this->len = Message::getLength();
}

int SUBACK_msg::set_VariableHeader(uint8_t *buffer){
    int index = 0;
    buffer[index++] = PacketIdentifier >> 8;
    buffer[index++] = PacketIdentifier & 0xf;
    return index;
}
int SUBACK_msg::get_VariableHeader(uint8_t *buffer){
    int index = 0;
    PacketIdentifier = buffer[index++] << 8;
    PacketIdentifier += buffer[index++];
    return index;
}

int SUBACK_msg::set_Payload(uint8_t *buffer){
    int index = 0;
    for(auto &c : ReturnCodes){
        buffer[index++] = (uint8_t) c;
    }
    return index;
}
int SUBACK_msg::get_Payload(uint8_t *buffer){
    int index = 0;
    int LenPayload = this->VHeader_Pyload_len - 2;
    for(int i = LenPayload; i>0; i--){
        ReturnCodes.push_back(buffer[index++]);
    }
    return index;
}
        
int SUBACK_msg::package(uint8_t *buffer){
    int index = 0;
    index += Message::package(&buffer[0]);
    index += set_VariableHeader(&buffer[index]);
    index += set_Payload(&buffer[index]);
    return index;
}
int SUBACK_msg::unpackage(uint8_t *buffer){
    int index = 0;
    index += Message::unpackage(&buffer[0]);
    index += get_VariableHeader(&buffer[index]);
    index += get_Payload(&buffer[index]);
    return index;
}

/*Funciones de UNSUBSCRIBE*/
UNSUBSCRIBE_msg::UNSUBSCRIBE_msg(uint16_t pi, vector<std::string> unsubstop){
    this->MQTT_ControlPacket_type = UNSUBSCRIBE;
    this->Flags_MQTT = 2;

    /*VariableHeader*/
    this->PacketIdentifier = pi;
    this->VHeader_Pyload_len = 2;
    /*Payload*/
    int i = 0;
    for(auto &s :unsubstop){
        UnsubscribedTopic.push_back(s);
        this->VHeader_Pyload_len += 2 + s.size();
    }
    this->len = Message::getLength();
}
UNSUBSCRIBE_msg::UNSUBSCRIBE_msg(){
    this->MQTT_ControlPacket_type = UNSUBSCRIBE;
    this->Flags_MQTT = 2;
    this->VHeader_Pyload_len = 2;
    this->len = Message::getLength();
}

int UNSUBSCRIBE_msg::set_VariableHeader(uint8_t *buffer){
    int index = 0;
    buffer[index++] = PacketIdentifier >> 8;
    buffer[index++] = PacketIdentifier & 0xf; 
    return index;
}
int UNSUBSCRIBE_msg::get_VariableHeader(uint8_t *buffer){
    int index = 0;
    PacketIdentifier = buffer[index++] << 8;
    PacketIdentifier += buffer[index++];
    return index;
}

int UNSUBSCRIBE_msg::set_Payload(uint8_t *buffer){
    int index = 0;
    for(auto &s : UnsubscribedTopic){
        index = Message::encodeString(s, buffer, index);
    }
    return index;
}
int UNSUBSCRIBE_msg::get_Payload(uint8_t *buffer){
    int index = 0;
    int i = 0;
    int PayloadLen = VHeader_Pyload_len - 2;
    while(PayloadLen > 0){
        int l = 0;
        std::string s = Message::decodeString(&buffer[index], &l);
        index += l;
        UnsubscribedTopic.push_back(s);
        PayloadLen = PayloadLen - 2 - s.size();
    }
    return index;
}

int UNSUBSCRIBE_msg::package(uint8_t *buffer){
    int index = 0;
    index += Message::package(&buffer[0]);
    index += set_VariableHeader(&buffer[index]);
    index += set_Payload(&buffer[index]);
    return index;
}
int UNSUBSCRIBE_msg::unpackage(uint8_t *buffer){
    int index = 0;
    index += Message::unpackage(&buffer[0]);
    index += get_VariableHeader(&buffer[index]);
    index += get_Payload(&buffer[index]);
    return index;
}

uint16_t UNSUBSCRIBE_msg::getPacketIdentifier(){
    return this->PacketIdentifier;
}
void UNSUBSCRIBE_msg::getTopicUnsubscribe(vector<std::string> *topics){
    for(auto &s : UnsubscribedTopic){
        topics->push_back(s);
    }
}

/*Funciones de SUBACK*/
UNSUBACK_msg::UNSUBACK_msg(uint16_t pi){
    this->MQTT_ControlPacket_type = UNSUBACK;
    this->Flags_MQTT = 0;

    /*VariableHeader*/
    this->PacketIdentifier = pi;
    this->VHeader_Pyload_len = 2;

    this->len = Message::getLength();
}

int UNSUBACK_msg::set_VariableHeader(uint8_t *buffer){
    int index = 0;
    buffer[index++] = PacketIdentifier >> 8;
    buffer[index++] = PacketIdentifier & 0xf;
    return index;
}
int UNSUBACK_msg::get_VariableHeader(uint8_t *buffer){
    int index = 0;
    PacketIdentifier = buffer[index++] << 8;
    PacketIdentifier += buffer[index++];
    return index;
}

int UNSUBACK_msg::package(uint8_t *buffer){
    int index = 0;
    index += Message::package(&buffer[0]);
    index += set_VariableHeader(&buffer[index]);
    return index;
}
int UNSUBACK_msg::unpackage(uint8_t *buffer){
    int index = 0;
    index += Message::unpackage(&buffer[0]);
    index += get_VariableHeader(&buffer[index]);
    return index;
}

/*Funciones de PINGREQ*/
PINGREQ_msg::PINGREQ_msg(){
    this->MQTT_ControlPacket_type = PINGREQ;
    this->Flags_MQTT = 0;
    this->VHeader_Pyload_len = 0;
    this->len = Message::getLength();
}

int PINGREQ_msg::package(uint8_t *buffer){
    int index = Message::package(buffer);
    return index;
}
int PINGREQ_msg::unpackage(uint8_t *buffer){
    int index = Message::unpackage(buffer);
    return index;
}

/*Funciones de PINGRESP*/
PINGRESP_msg::PINGRESP_msg(){
    this->MQTT_ControlPacket_type = PINGRESP;
    this->Flags_MQTT = 0;
    this->VHeader_Pyload_len = 0;
    this->len = Message::getLength();
}

int PINGRESP_msg::package(uint8_t *buffer){
    int index = Message::package(buffer);
    return index;
}
int PINGRESP_msg::unpackage(uint8_t *buffer){
    int index = Message::unpackage(buffer);
    return index;
}

/*Funciones de DISCONECT*/
DISCONNECT_msg::DISCONNECT_msg(){
    this->MQTT_ControlPacket_type = DISCONNECT;
    this->Flags_MQTT = 0;
    this->VHeader_Pyload_len = 0;
    this->len = Message::getLength();
}

int DISCONNECT_msg::package(uint8_t *buffer){
    int index = Message::package(buffer);
    return index;
}
int DISCONNECT_msg::unpackage(uint8_t *buffer){
    int index = Message::unpackage(buffer);
    return index;
}