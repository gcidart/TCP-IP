#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// passes automated checks run by `make check_lab5`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
constexpr EthernetAddress ETHERNET_ARP = {0, 0, 0, 0, 0, 0};


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
    if(_eth_ip_addr_map.count(next_hop_ip))
    {
        EthernetFrame efrm ;
        efrm.header().src = _ethernet_address;
        efrm.header().type = EthernetHeader::TYPE_IPv4;
        efrm.header().dst = _eth_ip_addr_map[next_hop_ip];
        efrm.payload() = dgram.serialize();
        _frames_out.push(efrm);
    }
    else
    {
        _ip_addr_list_dgram_map[next_hop_ip].push_back(dgram);
        if(_ip_addr_arp_req_time.count(next_hop_ip) && (_curr_time - _ip_addr_arp_req_time[next_hop_ip] <5000))
            return;
        send_arp(ETHERNET_ARP, next_hop_ip, ARPMessage::OPCODE_REQUEST);
        _ip_addr_arp_req_time[next_hop_ip] = _curr_time;
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst!=_ethernet_address && frame.header().dst!=ETHERNET_BROADCAST)
        return {};
    Buffer payload_single = frame.payload().concatenate();
    if(frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(payload_single) == ParseResult::NoError) {
                return dgram;
        }else {
                return {};
        }
    }
    else {
        ARPMessage arpmsg ;
        arpmsg.parse(payload_single);
        _eth_ip_addr_map[arpmsg.sender_ip_address] = arpmsg.sender_ethernet_address;
        _ip_addr_arp_entry_time[arpmsg.sender_ip_address] = _curr_time;
        _time_arp_entry.push(make_pair(_curr_time, arpmsg.sender_ip_address));
        if(arpmsg.opcode==ARPMessage::OPCODE_REQUEST && arpmsg.target_ip_address==_ip_address.ipv4_numeric())
        {
            send_arp(arpmsg.sender_ethernet_address, arpmsg.sender_ip_address, ARPMessage::OPCODE_REPLY);
        }
        while(_ip_addr_list_dgram_map[arpmsg.sender_ip_address].size() >0)
        {
            InternetDatagram dgram = _ip_addr_list_dgram_map[arpmsg.sender_ip_address].front();
            EthernetFrame efrm ;
            efrm.header().src = _ethernet_address;
            efrm.header().type = EthernetHeader::TYPE_IPv4;
            efrm.header().dst = _eth_ip_addr_map[arpmsg.sender_ip_address];
            efrm.payload() = dgram.serialize();
            _frames_out.push(efrm);
            _ip_addr_list_dgram_map[arpmsg.sender_ip_address].pop_front();
        }
    }
            
        
        
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    _curr_time+=ms_since_last_tick;
    while(!_time_arp_entry.empty() && (_curr_time - _time_arp_entry.front().first > 30000))
    {
        uint32_t next_hop_ip = _time_arp_entry.front().second;
        if(_ip_addr_arp_entry_time[next_hop_ip] == _time_arp_entry.front().first)
            _eth_ip_addr_map.erase(next_hop_ip);
        _time_arp_entry.pop();
    }
}

void NetworkInterface::send_arp(EthernetAddress target_address, uint32_t next_hop_ip, uint16_t arp_opcode) {
    EthernetFrame efrm ;
    efrm.header().src = _ethernet_address;
    ARPMessage arpmsg;
    arpmsg.opcode = arp_opcode;
    arpmsg.sender_ethernet_address = _ethernet_address;
    arpmsg.sender_ip_address = _ip_address.ipv4_numeric();
    arpmsg.target_ethernet_address = target_address;
    arpmsg.target_ip_address = next_hop_ip;
    efrm.header().type = EthernetHeader::TYPE_ARP;
    if(arp_opcode==ARPMessage::OPCODE_REPLY)
        efrm.header().dst = target_address;
    else
        efrm.header().dst = ETHERNET_BROADCAST;
    efrm.payload() = arpmsg.serialize();
    _frames_out.push(efrm);

}

