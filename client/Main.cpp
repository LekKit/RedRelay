//Example RedRelay client application

#include <iostream>
#include <cstdlib>
#include "RedRelayClient.hpp"

int main(int argc, char** argv){
    rc::RedRelayClient Client;
    std::cout<<Client.GetVersion()<<std::endl;
    Client.Connect(argc>1 ? argv[1] : "lekkit.hopto.org", argc>2 ? atoi(argv[2]) : 6121);
    while (true){
        Client.Update();
        for (const rc::Event&i : Client.Events)
            switch (i.Type){
                case rc::Event::Error:
                    std::cout<<i.ErrorMessage()<<std::endl;
                    break;
                case rc::Event::Connected:
                    std::cout<<"Connected to "<<Client.GetHostAddress()<<", self ID: "<<Client.SelfID()<<", welcome message: "<<i.WelcomeMessage()<<std::endl;
                    Client.RequestChannelsList();
                    Client.JoinChannel("");
                    break;
                case rc::Event::ConnectDenied:
                    std::cout<<"Connection denied: "<<i.DenyMessage();
                    break;
                case rc::Event::Established:
                    std::cout<<"UDP handshake completed"<<std::endl;
                    break;
                case rc::Event::Disconnected:
                    std::cout<<"Disconnected from the server"<<std::endl;
                    break;
                case rc::Event::ListReceived:
                    std::cout<<"Channels list ("<<i.ChannelsCount()<<" channels):"<<std::endl;
                    break;
                case rc::Event::ListEntry:
                    std::cout<<"\""<<i.ChannelName()<<"\" ("<<i.PeersCount()<<" peers)"<<std::endl;
                    break;
                case rc::Event::NameSet:
                    std::cout<<"Name set to \""<<Client.SelfName()<<"\""<<std::endl;
                    Client.JoinChannel("RedRelay Client example");
                    break;
                case rc::Event::NameDenied:
                    std::cout<<"Name set denied: "<<i.DenyMessage()<<std::endl;
                    break;
                case rc::Event::ChannelJoin:
                    std::cout<<"Joined channel \""<<i.ChannelName()<<"\", peers online: "<<Client.GetChannel(i.ChannelID()).GetPeerCount()<<std::endl;
                    if (Client.GetChannel(i.ChannelID()).GetPeerCount() != 0){
                        std::cout<<"Peers: ";
                        for (const rc::Peer&j : Client.GetChannel(i.ChannelID()).GetPeerList())
                            std::cout<<j.GetName()<<"("<<j.GetID()<<"); ";
                        std::cout<<std::endl;
                    }
                    break;
                case rc::Event::ChannelDenied:
                    std::cout<<"Channel join denied: "<<i.DenyMessage()<<std::endl;
                    break;
                case rc::Event::ChannelLeave:
                    std::cout<<"You left the channel \""<<i.ChannelName()<<"\""<<std::endl;
                    break;
                case rc::Event::PeerJoined:
                    std::cout<<"Peer \""<<i.PeerName()<<"\" joined the channel \""<<Client.GetChannel(i.ChannelID()).GetName()<<"\""<<std::endl;
                    break;
                case rc::Event::PeerLeft:
                    std::cout<<"Peer \""<<i.PeerName()<<"\" left the channel \""<<Client.GetChannel(i.ChannelID()).GetName()<<"\""<<std::endl;
                    break;
                case rc::Event::PeerChangedName:
                    std::cout<<"Peer \""<<i.PeerName()<<"\" changed name to \""<<Client.GetChannel(i.ChannelID()).GetPeer(i.PeerID()).GetName()<<"\""<<std::endl;
                    break;
                
            }
        Client.Events.clear();
        sf::sleep(sf::milliseconds(10));
    }
}
