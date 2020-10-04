#include "router.hh"

#include <iostream>

using namespace std;

// passes automated checks run by `make check_lab6`.

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`


//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    int idx = 31;
    if(_root ==nullptr)
    {
        _root = make_unique<Node>();
    }
    std::unique_ptr<Node>* node_ptr = &_root;

    for(int i = 1; i<=prefix_length; i++)
    {
        if((route_prefix & (1<<idx))!=0)
        {
            if((*node_ptr)->right==nullptr)
                (*node_ptr)->right = make_unique<Node>();
            node_ptr = &((*node_ptr)->right);
        }
        else
        {
            if((*node_ptr)->left==nullptr)
                (*node_ptr)->left = make_unique<Node>();
            node_ptr = &((*node_ptr)->left);
        }
        idx--;
    }
    (*node_ptr)->match = true;
    (*node_ptr)->next_hop = next_hop;
    (*node_ptr)->interface_num = interface_num;

}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    uint32_t dgram_dst = dgram.header().dst;
    bool match_found = false;
    optional<Address> next_hop = {};
    size_t interface_num = 0;
    std::unique_ptr<Node>* node_ptr = &_root;
    if((*node_ptr)->match)
    {
        match_found = true;
        next_hop = (*node_ptr)->next_hop; 
        interface_num = (*node_ptr)->interface_num;
    }
    for(int i=31; i>=0; i--)
    {
        if((dgram_dst&(1<<i))!=0)
        {
            node_ptr = &((*node_ptr)->right);
        }
        else
        {
            node_ptr = &((*node_ptr)->left);
        }
        if((*node_ptr)==nullptr)
            break;
        if((*node_ptr)->match)
        {
            match_found = true;
            next_hop = (*node_ptr)->next_hop; 
            interface_num = (*node_ptr)->interface_num;
        }
    }
    if(match_found && dgram.header().ttl>1)
    {
        dgram.header().ttl--;
        if(next_hop.has_value())
        {
            interface(interface_num).send_datagram(dgram, next_hop.value());
        }
        else
        {
            interface(interface_num).send_datagram(dgram, Address::from_ipv4_numeric(dgram_dst));
        }
    }
            
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
