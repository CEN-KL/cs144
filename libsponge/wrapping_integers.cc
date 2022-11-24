#include "wrapping_integers.hh"
#include <climits>
#include <iostream>
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}




using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // DUMMY_CODE(n, isn);
    // return WrappingInt32{0};
    if (n == 0) {
        return WrappingInt32(isn.raw_value());
    }
    uint64_t mod = 1LL << 32;
    uint32_t isn_value = isn.raw_value();
    uint32_t idx = 1;
    if (n <= UINT_MAX - isn_value) {
        idx = isn_value + n;
    } else {
        n -= mod - isn_value;
        idx = n % mod;
    }
    return WrappingInt32(idx);
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
    // DUMMY_CODE(n, isn, checkpoint);
    // return {};
    // cout << "---- checkpoint: " << checkpoint << " -------" << endl;
    uint64_t mod = 1ul << 32;
    uint64_t base = 0;

    uint32_t n_value = n.raw_value();
    uint32_t isn_value = isn.raw_value();
    // cout << "n: " << n_value << endl;
    // cout << "isn: " << isn_value << endl;
    if (n_value > isn_value) {
        base = n_value - isn_value;
    } else {
        base = mod - isn_value + n_value;
    }
    
    uint64_t times = checkpoint / mod;
    uint64_t round = checkpoint % mod;
    if (base == round) {
        // cout << " == ok ==" << endl;
        return times * mod + base;
    }
    uint64_t v1 = 0, v2 = 0, d1 = 0, d2 = 0; 
    if (base < round) {
        v1 = times * mod + base;
        v2 = (times + 1ul) * mod + base;
        d1 = checkpoint >= v1 ? checkpoint - v1 : v1 - checkpoint;
        d2 = checkpoint >= v2 ? checkpoint - v2 : v2 - checkpoint;
    } else {
        v1 = times * mod + base;
        v2 = (times - 1ul) * mod + base;
        d1 = checkpoint >= v1 ? checkpoint - v1 : v1 - checkpoint;
        d2 = checkpoint >= v2 ? checkpoint - v2 : v2 - checkpoint;
    }
    if (d1 <= d2) {
        return v1;
    } else {
        return v2;
    }
    // cout << " ---------- end ---------- " << endl;
}
