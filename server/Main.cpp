//Example RedRelay server
#include "RedRelayServer.hpp"
#include <fstream>
#include <csignal>

rs::RedRelayServer& Server = *new rs::RedRelayServer;
std::fstream config;
uint16_t Port = 6121;
bool PortSet = false,
     PingIntervalSet = false,
     LogEnabledSet = false,
     ConnectionsLimitSet = false,
     PeersLimitSet = false,
     ChannelsLimitSet = false,
     ChannelsPerPeerLimitSet = false;

bool LoadConfig(){
    config.open("redrelay.cfg", std::fstream::out | std::fstream::in);
    if (!config.is_open()){
        std::ofstream tmp("redrelay.cfg");
        if (!tmp.is_open()){
            Server.Log("Could not create config file", 4);
            return false;
        }
        tmp<<"#RedRelay Server configuration file\n\
\n\
#Server port\n\
Port = 6121\n\
\n\
#Keepalive ping interval (in seconds)\n\
#Set to 0 to disable\n\
PingInterval = 3\n\
\n\
#Logging\n\
LogEnabled = true\n\
\n\
#Limits unauthorised connections\n\
ConnectionsLimit = 16\n\
\n\
#Limits the peers count on the server\n\
PeersLimit = 128\n\
\n\
#Limits channels count (including hidden channels)\n\
ChannelsLimit = 32\n\
\n\
#Limits channels in which peer can be at once\n\
ChannelsPerPeerLimit = 4\n\
\n\
#WelcomeMessage = \"\"";
        tmp.close();
        config.open("redrelay.cfg", std::fstream::out | std::fstream::in);
    }
    return true;
}

void SetProp(const std::string& PropName, const std::string& PropVal){
    //std::cout<<"SetProp: "<<PropName<<" "<<PropVal<<std::endl;
    if (PropName == "Port"){
        Port = std::stoi(PropVal);
        PortSet = true;
    } else if (PropName == "PingInterval"){
        Server.SetPingInterval(std::stoi(PropVal));
        PingIntervalSet = true;
    } else if (PropName == "LogEnabled"){
        Server.SetLogEnabled(PropVal=="true");
        LogEnabledSet = true;
    } else if (PropName == "ConnectionsLimit"){
        Server.SetConnectionsLimit(std::stoi(PropVal));
        ConnectionsLimitSet = true;
    } else if (PropName == "PeersLimit"){
        Server.SetPeersLimit(std::stoi(PropVal));
        PeersLimitSet = true;
    } else if (PropName == "ChannelsLimit"){
        Server.SetChannelsLimit(std::stoi(PropVal));
        ChannelsLimitSet = true;
    } else if (PropName == "ChannelsPerPeerLimit"){
        Server.SetChannelsPerPeerLimit(std::stoi(PropVal));
        ChannelsPerPeerLimitSet = true;
    } else if (PropName == "WelcomeMessage") Server.SetWelcomeMessage(PropVal);
}

bool running = true;

void sig_handler(int sigmask){
    if (sigmask == SIGINT){
        signal(SIGINT, exit);
        running = false;
        Server.Stop();
    }
}

int main(){
    if (LoadConfig()){
        std::string tmp, PropName, PropVal;
        while (!config.eof()){
            PropName = "";
            PropVal = "";
            std::getline(config, tmp);
            for (uint32_t i=0; i<tmp.length(); ++i)
                if (tmp[i]=='='){
                    PropName = tmp.substr(0, i);
                    if (i<tmp.length()) PropVal = tmp.substr(i+1);
                }
            if (PropName!=""){

                //Get rid of spaces
                while (PropName[0]==' ') PropName = PropName.substr(1);
                while (PropName[PropName.length()-1]==' ') PropName = PropName.substr(0, PropName.length()-1);

                if (PropVal!=""){

                    while (PropVal[0]==' ') PropVal = PropVal.substr(1);
                    while (PropVal[PropVal.length()-1]==' ') PropVal = PropVal.substr(0, PropVal.length()-1);

                    if (PropVal[0]=='\"' || PropVal[0]=='\''){
                        while (PropVal[PropVal.length()-1]!='\"' && PropVal[PropVal.length()-1]!='\'' && !config.eof()){
                            config>>tmp;
                            PropVal+=tmp;
                        }
                        if (PropVal.length()>1) SetProp(PropName, PropVal.substr(1, PropVal.length()-2));
                    } else SetProp(PropName, PropVal);
                }
            }
        }
        config.clear();
        if (!PortSet) config<<"\nPort = 6121";
        if (!PingIntervalSet) config<<"\nPingInterval = 3";
        if (!LogEnabledSet) config<<"\nLogEnabled = true";
        if (!ConnectionsLimitSet) config<<"\nConnectionsLimit = 16";
        if (!PeersLimitSet) config<<"\nPeersLimit = 128";
        if (!ChannelsLimitSet) config<<"\nChannelsLimit = 32";
        if (!ChannelsPerPeerLimitSet) config<<"\nChannelsPerPeerLimit = 4";
        config.close();
    }

    signal(SIGINT, sig_handler);
    while (running){ //In case of failure, retry every 5s
        Server.Start(Port);
        if (running) sf::sleep(sf::seconds(5));
    }

    return 0;
}
