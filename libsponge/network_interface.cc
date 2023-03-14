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
    if (_ip_to_mac.find(next_hop_ip) != _ip_to_mac.end()) {
        // the destination Ethernet address is already known
        EthernetAddress next_hop_mac = _ip_to_mac[next_hop_ip].first;
        EthernetFrame ef;
        ef.header().type = EthernetHeader::TYPE_IPv4;
        ef.header().dst = next_hop_mac;
        ef.header().src = _ethernet_address;
        ef.payload() = dgram.serialize();
        frames_out().push(ef);
    } else {
        BufferList bfl = dgram.serialize();
        _wating_queue.push({next_hop_ip, bfl});
        if (_time_since_last_broadcast > 5000) {
            braodcast_arp_request(next_hop_ip);
            _time_since_last_broadcast = 0;
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    EthernetHeader fh = frame.header();
    if (fh.dst != _ethernet_address && fh.dst != ETHERNET_BROADCAST) 
        return {};
    if (fh.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(Buffer(frame.payload().concatenate())) == ParseResult::NoError) {
            return dgram;
        }
    } else if (fh.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        if (arp_msg.parse(Buffer(frame.payload().concatenate())) == ParseResult::NoError) {
            _ip_to_mac[arp_msg.sender_ip_address] = {arp_msg.sender_ethernet_address, 0};
            if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == _ip_address.ipv4_numeric()) {
                reply_arp_broadcast(arp_msg.sender_ip_address, arp_msg.sender_ethernet_address);
            }
        }
        int len_of_waiting_queue = _wating_queue.size();
        for (; len_of_waiting_queue--; ) {
            auto tmp = _wating_queue.front();
            _wating_queue.pop();
            uint32_t ip = tmp.first;
            if (_ip_to_mac.find(ip) != _ip_to_mac.end()) {
                EthernetAddress next_hop_mac = _ip_to_mac[ip].first;
                EthernetFrame ef;
                ef.header().type = EthernetHeader::TYPE_IPv4;
                ef.header().dst = next_hop_mac;
                ef.header().src = _ethernet_address;
                ef.payload() = tmp.second;
                frames_out().push(ef);
            } else {
                _wating_queue.push(tmp);
            }
        }
    } 
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    _time_since_last_broadcast += ms_since_last_tick;
    // if (_time_since_last_broadcast > 5000 && !_wating_queue.empty()) {
    //     braodcast_arp_request(_wating_queue.front().first);
    // }
    for (auto it = _ip_to_mac.begin(); it != _ip_to_mac.end(); ) {
        if (it->second.second + ms_since_last_tick > 30000) {
            it = _ip_to_mac.erase(it);
        } else {
            it->second.second += ms_since_last_tick;
            ++it;
        }
    }
}

//! methods added by myself
void NetworkInterface::braodcast_arp_request(uint32_t target_ip) {
    EthernetFrame ef;
    ef.header().dst = ETHERNET_BROADCAST;
    ef.header().src = _ethernet_address;
    ef.header().type = EthernetHeader::TYPE_ARP;
    ARPMessage arp_msg;
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
    arp_msg.sender_ethernet_address = _ethernet_address;
    arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
    arp_msg.target_ip_address = target_ip;
    ef.payload() = arp_msg.serialize();
    frames_out().push(ef);
}

void NetworkInterface::reply_arp_broadcast(uint32_t dst_ip, EthernetAddress& dst_mac) {
    ARPMessage arp_msg;
    arp_msg.opcode = ARPMessage::OPCODE_REPLY;
    arp_msg.sender_ethernet_address = _ethernet_address;
    arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
    arp_msg.target_ethernet_address = dst_mac;
    arp_msg.target_ip_address = dst_ip;
    EthernetFrame ef;
    ef.header().dst = dst_mac;
    ef.header().src = _ethernet_address;
    ef.header().type = EthernetHeader::TYPE_ARP;
    ef.payload() = arp_msg.serialize();
    frames_out().push(ef);
}