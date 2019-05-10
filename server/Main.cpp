//Example RedRelay server
#include "RedRelayServer.hpp"

#define PORT 6121

rs::RedRelayServer& Server = *new rs::RedRelayServer;

/*void LaunchUDP(){
    Server.LaunchUDP();
}*/

int main(){
    Server.SetPeersLimit(1000);
    Server.SetChannelsLimit(250);
    Server.SetChannelsPerPeerLimit(4);
    while (true){ //In case of failure, retry every 5s
        Server.Start(PORT);
        sf::sleep(sf::seconds(5));
    }
    return 0;
}
