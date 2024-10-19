#ifndef SPONGE_LIBSPONGE_ARP_MESSAGE_HH
#define SPONGE_LIBSPONGE_ARP_MESSAGE_HH

#include "ethernet_header.hh"
#include "ipv4_header.hh"

using EthernetAddress = std::array<uint8_t, 6>;

//! \brief [ARP](\ref rfc::rfc826) message
struct ARPMessage {
    static constexpr size_t LENGTH = 28;          //!< ARP message length in bytes
    static constexpr uint16_t TYPE_ETHERNET = 1;  //!< ARP type for Ethernet/Wi-Fi as link-layer protocol
    static constexpr uint16_t OPCODE_REQUEST = 1;
    static constexpr uint16_t OPCODE_REPLY = 2;

    //! \name ARPheader fields
    //!@{
    uint16_t hardware_type = TYPE_ETHERNET;              //!< Type of the link-layer protocol (generally Ethernet/Wi-Fi)
    uint16_t protocol_type = EthernetHeader::TYPE_IPv4;  //!< Type of the Internet-layer protocol (generally IPv4)
    uint8_t hardware_address_size = sizeof(EthernetHeader::src);//link地址的大小
    uint8_t protocol_address_size = sizeof(IPv4Header::src);   //ip地址的大小
    uint16_t opcode{};  //!< Request or reply

    EthernetAddress sender_ethernet_address{};//6字节, src mac
    uint32_t sender_ip_address{};

    EthernetAddress target_ethernet_address{};//6字节, dst mac
    uint32_t target_ip_address{};
    //!@}

    //! Parse the ARP message from a string
    ParseResult parse(const Buffer buffer);

    //! Return a string containing the ARP message in human-readable format
    // hardware_type[2]             :ETHERNET
    // protocol_type[2]             :IPV4
    // hardware_address_size[1]     : 6
    // protocol_address_size[1]     : 4
    // opcode[2]                    : Request or reply
    // src_mac[6]
    // src_ip[4]
    // dst_mac[6]
    // dst_ip[4]
    //! Serialize the ARP message to a string
    std::string serialize() const;

    std::string to_string() const;

    //! Is this type of ARP message supported by the parser?
    bool supported() const;
};

//! \struct ARPMessage
//! This struct can be used to parse an existing ARP message or to create a new one.

#endif  // SPONGE_LIBSPONGE_ETHERNET_HEADER_HH
