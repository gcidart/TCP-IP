#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>
// Implementation of a flow-controlled in-memory byte stream.

// Passes automated checks run by `make check_lab0`.
// Changed from Linked List of Char nodes to Linked List of String nodes to resolve Lab1 timeout issue

ByteStream::ByteStream(const size_t capacity) {
    _capacity = capacity; 
    if(capacity==0)
        return;
    ll = make_unique<Node>("");
    write_ptr = &ll;
    read_ptr  = &ll;
    num_pop = 0, num_write = 0;
}

size_t ByteStream::write(const string &data) {
    size_t sz = data.size();
    size_t write_size = min(sz, remaining_capacity());
    if(remaining_capacity()>0)
    {
        (*write_ptr)->next = make_unique<Node>(data.substr(0, write_size));
        num_write+= write_size;
        write_ptr = &((*write_ptr)->next);
        return write_size;
    }
    return 0;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string peek_data = "";
    size_t read_num = 0;
    size_t read_len = min(len, buffer_size());
    std::unique_ptr<Node>* oread_ptr = (read_ptr);
    while((!buffer_empty() || (oread_ptr!=write_ptr)) && read_num < read_len)
    {
        oread_ptr = &((*oread_ptr)->next);
        string oread_string = (*oread_ptr)->val;
        size_t oread_size = oread_string.size();
        if(read_num + oread_size <= read_len)
        {
            peek_data+= oread_string;
            read_num+=oread_size;
        }
        else
        {
            size_t read_size =  (read_len - read_num);
            peek_data+= oread_string.substr(0, read_size);
            read_num+= read_size;
        }
    }
    return peek_data;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t read_num = 0;
    size_t read_len = min(len, buffer_size());
    while((!buffer_empty() ||(read_ptr!=write_ptr)) && read_num < read_len)
    {
        std::unique_ptr<Node>* read_ptr_cpy = read_ptr;
        read_ptr = &((*read_ptr)->next);
        string read_string = (*read_ptr)->val;
        size_t read_size = read_string.size();
        if(read_num + read_size <= read_len)
        {
            read_num+=read_size;
            num_pop+=read_size;
        }
        else
        {
            read_size =  (read_len - read_num);
            (*read_ptr)->val = read_string.substr(read_size, string::npos);
            read_ptr = read_ptr_cpy;
            read_num+= read_size;
            num_pop+= read_size;
        }
    }
}

void ByteStream::end_input() {_end_input = true;}

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return num_write - num_pop; }

bool ByteStream::buffer_empty() const { return buffer_size()==0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return num_write; }

size_t ByteStream::bytes_read() const { return num_pop ; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
