#include "stream_reassembler.hh"
#include <algorithm>


// passes automated checks run by `make check_lab1`.



StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity),
                                     last_assembled{-1}, total_bytes_rcvd{0}, intervals{}{

}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t sz = data.size();
    size_t index_end = index + sz - 1;
    size_t bsz; // Used to store the size of substring to be extracted from data
    //Handle the case where EOF is encountered but nothing from data is to be copied
    if(sz==0 || (last_assembled!=-1 && index_end<=static_cast<size_t>(last_assembled)))
    {
        if(!eof_seen)
            eof_seen = eof; 
        if(eof_seen && unassembled_bytes()==0)
        {
            _output.end_input();
        }
        return;
    }
    size_t index_start = max(index, static_cast<size_t>(last_assembled+1));    
    //Insert first interval
    if(intervals.empty())
    {
        //More data can be copied into Bytestream if some bytes have already been read
        if(index_start < (_capacity + stream_out().bytes_read()))  
        {
            struct interval new_interval;
            new_interval.start = index_start;
            new_interval.end = min(index_end, _capacity+stream_out().bytes_read()-1);
            bsz = new_interval.end - new_interval.start + 1;
            new_interval.buffer = data.substr(index_start-index, bsz);
            total_bytes_rcvd += bsz;
            if(index_start==static_cast<size_t>(last_assembled+1))
            {
                _output.write(new_interval.buffer);
                last_assembled = static_cast<int>(index+bsz)-1;
            }
            else
                intervals.insert(intervals.end(), new_interval);

        }
        if(!eof_seen)
            eof_seen = eof; 
        if(eof_seen && unassembled_bytes()==0)
        {
            _output.end_input();
        }
        return;
    }


    auto it = intervals.begin();
    auto last_assembled_it = intervals.end();
    //Iterate through intervals which are less than the new intervals
    while(it!=intervals.end() && it->end < index_start)
    {
        it++;
    }
    //If new interval is part of an existing interval
    if(it!=intervals.end() && it->start<=index_start && index_end<=it->end)
        it++;
    //If new interval does not overlap with existing interval
    else if((it!=intervals.end() && index_end < it->start) || it==intervals.end())
    {
        auto prev = it;
        if(it!=intervals.begin())
            prev--;

        if( it!=intervals.begin() && index==(prev)->end+1)
        {
            it = prev;
            it->end = min(index_end, _capacity + stream_out().bytes_read()-1);
            bsz = it->end - index_start + 1;
            it->buffer+= data.substr(index_start - index, bsz); 
        }
        else
        {
            struct interval new_interval;
            new_interval.start = index_start;
            new_interval.end = min(index_end, _capacity+stream_out().bytes_read());
            bsz = new_interval.end - new_interval.start + 1;
            new_interval.buffer = data.substr(index_start-index, bsz);
            it = intervals.insert(it, new_interval);
        }
        total_bytes_rcvd += bsz;
        auto next = it;
        if(it!=intervals.end())
            next++;
        if(it!=intervals.end() && next!=intervals.end() && (index_end+1) == next->start)
        {   
            it->end = next->end;
            it->buffer+=next->buffer;
            if(it->start<=static_cast<size_t>(last_assembled+1) && static_cast<size_t>(last_assembled+1)<=it->end)
            {
                last_assembled_it = it;
            }
            it = intervals.erase(next);
        }
        else
        {
            if(it->start<=static_cast<size_t>(last_assembled+1) && static_cast<size_t>(last_assembled+1)<=it->end)
            {
                last_assembled_it = it;
            }
            it++;
        }
    }    
    //If new interval overlaps with existing interval(s)
    else
    {
        size_t seen_till_now;
        auto it_sec = it;
        auto prev = it;
        if(it!=intervals.begin())
            prev--;
        auto next = it;
        //Example: Existing interval [5,7]; New interval [3,8]
        if(it!=intervals.end() && index_start < it->start)
        {   
            //Example: Existing interval [0,2), [5,7]; New interval [3,8]
            if(it!=intervals.begin() && index_start==(prev)->end+1)
            {
                next = it;
                it = prev;
                it->end = next->end;
                bsz = (next->start-1) - index_start+1;
                it->buffer = it->buffer + data.substr(index_start-index, bsz) + next->buffer;
                total_bytes_rcvd+= bsz;
            }
            //Example: Existing interval [0,1], [5,7]; New interval (3,8]
            else
            {
                next = it;
                struct interval new_interval;
                new_interval.start = index_start;
                new_interval.end = next->end;
                bsz = (next->start-1) - new_interval.start + 1;
                new_interval.buffer = data.substr(index_start-index, bsz) + next->buffer;
                total_bytes_rcvd += bsz;
                it = intervals.insert(it, new_interval);
            }
            seen_till_now = next->end;
            it_sec = intervals.erase(next);
        }
        //Example: Existing interval [5,7]; New interval [6,8]
        else
        {
            seen_till_now = next->end;
            it_sec++;
        }
        while(it_sec!=intervals.end() && it_sec->start <= index_end )
        {
            if(it_sec->start > 0)
            {
                it->end = it_sec->end;
                bsz = (it_sec->start-1) - (seen_till_now+1) + 1;
                it->buffer += data.substr(seen_till_now+1-index, bsz) + it_sec->buffer;
                total_bytes_rcvd += bsz;
            }
            seen_till_now = it_sec->end;
            it_sec = intervals.erase(it_sec);
        }
        if(seen_till_now < index_end)
        {
            bsz = index_end - (seen_till_now+1) + 1;
            it->buffer += data.substr(seen_till_now+1-index_start, bsz);
            total_bytes_rcvd += bsz;
            it->end = index_end;
            seen_till_now = index_end;
        }
        if(it_sec!=intervals.end() && seen_till_now+1 == it_sec->start)
        {
            it->buffer += it_sec->buffer;
            it->end = it_sec->end;
            it_sec = intervals.erase(it_sec);
        }
        else
        {
            it->end = seen_till_now;
        }
        if(it->start<=static_cast<size_t>(last_assembled+1) && static_cast<size_t>(last_assembled+1)<=it->end)
            last_assembled_it = it;
    }
            
        
            

    if(!eof_seen)
        eof_seen = eof; 
    if(last_assembled_it!=intervals.end())
    {
        _output.write(move(last_assembled_it->buffer));
        last_assembled = static_cast<int>(last_assembled_it->end);
        intervals.erase(last_assembled_it);
    }
    if(eof_seen && unassembled_bytes()==0)
    {
        _output.end_input();
    }
            
        
}


size_t StreamReassembler::unassembled_bytes() const { return total_bytes_rcvd - (last_assembled + 1); }

bool StreamReassembler::empty() const { return (unassembled_bytes()==0); }
