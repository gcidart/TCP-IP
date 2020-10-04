#include "wrapping_integers.hh"


// passes "ctest -R wrap" testcases


using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t max32 = 1;
    max32 = (max32<<32) ;
    n+= isn.raw_value();
    n%=max32;
    return WrappingInt32{static_cast<uint32_t>(n)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    WrappingInt32 wrapped_checkpoint = wrap(checkpoint, isn);
    uint64_t max32 = 1;
    max32 = (max32<<32);
    uint64_t option1, option2;
    if(n.raw_value() < wrapped_checkpoint.raw_value())
    {
        option1 =  static_cast<uint64_t>(static_cast<uint32_t>(max32) - wrapped_checkpoint.raw_value() + n.raw_value());
        option2 =  static_cast<uint64_t>(wrapped_checkpoint.raw_value() - n.raw_value());
    }
    else
    {
        option1 =  static_cast<uint64_t>(n.raw_value() - wrapped_checkpoint.raw_value());
        option2 =  static_cast<uint64_t>(static_cast<uint32_t>(max32) - n.raw_value() + wrapped_checkpoint.raw_value());

    }
    if((checkpoint >= option2) && (option2 < option1))
        return (checkpoint-option2);
    else
        return (checkpoint+option1);

        
    
}
