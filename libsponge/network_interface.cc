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
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
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

    if(_cache.count(next_hop_ip)&& _ms_passed <=_cache[next_hop_ip].expired_ts){
        hdr.dst=_cache[next_hop_ip].mac;
        _frames_out.push(frame);
    }else{
        if(_delay_cache.count(next_hop_ip)==0){
            _delay_cache[next_hop_ip]=std::list<EthernetFrame>();
        }
        auto& lst=_delay_cache[next_hop_ip];
        lst.emplace_back(frame);
        //准备发送一个arp request
        _make_arp_request(next_hop_ip);

    }

}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    const EthernetHeader& hdr=frame.header();
    if(hdr.dst!=_ethernet_address&&hdr.dst!=ETHERNET_BROADCAST){
         return nullopt;
    }
    const BufferList &payload=frame.payload();
    if(hdr.type==EthernetHeader::TYPE_IPv4){    //payload是ipv4数据包
        InternetDatagram dgram;
        if(ParseResult::NoError==dgram.parse(payload)){//成功解析 ipv4数据包
            return dgram;
        }
    }else if(hdr.type==EthernetHeader::TYPE_ARP){
        ARPMessage argm;
       
         if(ParseResult::NoError== argm.parse(payload)){//成功解析 arp数据包

            _cache[argm.sender_ip_address].mac=argm.sender_ethernet_address;
            _cache[argm.sender_ip_address].expired_ts= _ms_passed +30*1000;


            if(argm.opcode==ARPMessage::OPCODE_REQUEST){//arp request

                if (argm.target_ip_address==_ip_address.ipv4_numeric()){//判断是否是对我的ip地址的arp req
                    EthernetFrame frame_out;
                    EthernetHeader& hdr_out=frame_out.header();


                    hdr_out.src=_ethernet_address;
                    hdr_out.dst=argm.sender_ethernet_address;
                    hdr_out.type=EthernetHeader::TYPE_ARP;

                    argm.opcode=ARPMessage::OPCODE_REPLY;//回复 arp reply

                    argm.target_ethernet_address=argm.sender_ethernet_address;
                    argm.target_ip_address=argm.sender_ip_address;

                    argm.sender_ethernet_address=_ethernet_address;
                    argm.sender_ip_address=_ip_address.ipv4_numeric();


                    frame_out.payload()=argm.serialize();

                    _frames_out.push(frame_out);
                }

            }else{//arp reply
                uint32_t target_ip=argm.sender_ip_address;
                auto& target_mac_addr=argm.sender_ethernet_address;

                if(_delay_cache.find(target_ip)!=_delay_cache.end()){
                    auto &lst=_delay_cache[target_ip];
                    for(EthernetFrame &frm :lst){
                        frm.header().dst=target_mac_addr;
                        _frames_out.push(frm);
                    }
                    _delay_cache.erase(target_ip);
                    _last_arp_timestamps.erase(target_ip);
                }
            }
            
        }
    }
    return nullopt;
}
 
//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _ms_passed +=ms_since_last_tick;

    for(auto& x:_last_arp_timestamps){
        _make_arp_request(x.first);
    }
 }


 //5s以内如果发送了arp请求，直接返回，否则
 void NetworkInterface::_make_arp_request(uint32_t ip){

       if(_last_arp_timestamps.count(ip)){
          size_t ts=_last_arp_timestamps[ip];
          if(_ms_passed <ts+5*1000){//5s以内 发送过对ip的arp request
              return;
          }
       }
       //记录本次发生的时间
       _last_arp_timestamps[ip]= _ms_passed;

       ARPMessage m;

       m.opcode= ARPMessage::OPCODE_REQUEST;

       m.sender_ethernet_address=_ethernet_address;
       m.sender_ip_address=_ip_address.ipv4_numeric();

        m.target_ethernet_address={0, 0, 0, 0, 0, 0};
        m.target_ip_address=ip;

        EthernetFrame frame;
        EthernetHeader& hdr=frame.header();
    
        hdr.type=EthernetHeader::TYPE_ARP;
        hdr.src=_ethernet_address;
        hdr.dst=ETHERNET_BROADCAST;
        
        frame.payload()=m.serialize();

        _frames_out.push(frame);

    }