#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address),_cache(std::map<uint32_t,EthernetAddress>()) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    
    EthernetFrame frame;
    EthernetHeader& hdr=frame.header();
    
    hdr.type=EthernetHeader::TYPE_IPv4;
    hdr.src=_ethernet_address;
    frame.payload()=dgram.serialize();

    if(_cache.find(next_hop_ip)!=_cache.end()){
        hdr.dst=_cache[next_hop_ip];
    }else{
        if(_delay_cache.find(next_hop_ip)==_delay_cache.end()){
            _delay_cache[next_hop_ip]=std::list<EthernetFrame>();
        }
        auto& lst=_delay_cache[next_hop_ip];
        lst.push_front(frame);


        //准备发送一个arp request
        _make_arp_request(next_hop_ip);

    }

    _frames_out.push(frame);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    const EthernetHeader& hdr=frame.header();
    if(hdr.dst!=_ethernet_address&&hdr.dst!=ETHERNET_BROADCAST){
         return {};
    }
    const BufferList &payload=frame.payload();
    if(hdr.type==EthernetHeader::TYPE_ARP){
        InternetDatagram dgram;
        if(ParseResult::NoError==dgram.parse(payload)){
            return dgram;
        }
    }else if(hdr.type==EthernetHeader::TYPE_ARP){
        ARPMessage argm;
       
         if(ParseResult::NoError== argm.parse(payload)){
            _cache[argm.sender_ip_address]=argm.sender_ethernet_address;
            _cache_to_live[argm.sender_ip_address]=_ms_since_last_tick;


            if(argm.opcode==ARPMessage::OPCODE_REQUEST){
                EthernetFrame frame;
                EthernetHeader& hdr=frame.header();
                hdr.src=_ethernet_address;
                hdr.dst=argm.sender_ethernet_address;

                argm.opcode=ARPMessage::OPCODE_REPLY;

                argm.target_ethernet_address=argm.sender_ethernet_address;
                argm.target_ip_address=argm.sender_ip_address;
                
                argm.sender_ethernet_address=_ethernet_address;
                argm.sender_ip_address=_ip_address.ipv4_numeric();


                frame.payload()=argm.serialize();

                _frames_out.push(frame);
            }else{
                uint32_t target_ip=argm.sender_ip_address;
                auto& target_mac_addr=argm.sender_ethernet_address;

                if(_delay_cache.find(target_ip)!=_delay_cache.end()){
                    auto &lst=_delay_cache[target_ip];
                    for(EthernetFrame &frame :lst){
                        frame.header().dst=target_mac_addr;
                        _frames_out.push(frame);
                    }
                    _delay_cache.erase(target_ip);
                }
            }
            
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    
    _ms_since_last_tick=ms_since_last_tick;
   
   for(auto it=_cache_to_live.begin();it!=_cache_to_live.end();it++){
        size_t ts=it->second;
        if(ms_since_last_tick>ts+30*1000){
            _cache.erase(it->first);
            _cache_to_live.erase(it);
        }
   }
 }
