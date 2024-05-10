Partial implementation of the M2M MQTT communications protocol in version 3.1.1.

In `MQTT.h ` classes are defined to generate each type of message following the protocol requirements in each case. Cases where Quality of Service (QoS) is different from 0 are not considered.

In `Broker.h` a server class is implemented containing a list with information of the connected clients and a list with information of each MQTT topic. The `Management()` method handles the connection of different subscribing clients by generating a thread for each subscribed client. The `ServeClient()` method attends to each client according to the type of message it sends.

In  `Client.h ` a client class is defined containing attributes for identification and methods with which it interacts with the server.

The programs  `main_client_publish.cpp` and `main_client_subscribe.cpp` demonstrate the protocol's operation with these classes. `main_client_publish.cpp` runs a program in which a client publishes on a topic, while `main_client_subscribe.cpp` runs a program in which a client subscribes to a specific topic.
