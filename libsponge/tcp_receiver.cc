#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // DUMMY_CODE(seg);
    const TCPHeader header = seg.header();
    if (!_has_syn && !header.syn) {
        return;
    }
    uint32_t len = seg.length_in_sequence_space();
    if (header.fin) {
        _eof = true;
        _end_abs_index = unwrap(header.seqno, _isn, 0) + len;
    }
    if (header.syn) {
        _has_syn = true;
        _isn = header.seqno;   //  is it ok ?
        _ack = _isn + len;
        _reassembler.push_substring(seg.payload().copy(), 0, header.fin);
    }  else {
        uint64_t idx = unwrap(header.seqno, _isn, 0);
        // uint64_t ack_idx = unwrap(_ack, _isn, 0);
        _reassembler.push_substring(seg.payload().copy(), idx - 1 , header.fin);
        _ack = wrap(_reassembler.get_unass_index() + 1, _isn);
        if (_eof && unwrap(_ack, _isn, 0) + 1 == _end_abs_index) {
            _ack = _ack + 1;
        }
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (_has_syn) {
        return _ack;
    } else {
        return {};
    }
 }

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
