#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}
 
using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    // cout << "is ack? " << seg.header().ack << endl;
    // cout << "is syn? " << seg.header().syn << endl;
    // cout << seg.length_in_sequence_space() << endl;
    // 0. 记录此时的时间，便于计算time_since_last_segment_received
    _time_since_last_segment_received = 0;
    // 1. 收到RST，将发送端stream和接受端stream设置成error state并终止连接
    if (seg.header().rst) {
        inbound_stream().set_error();
        _sender.stream_in().set_error();
        close_connection();
        return;
    }

    // 2. 把segment交给 TCPReceiver 
    _receiver.segment_received(seg);

    // 5. handle "keep alive" segment
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0)
        && seg.header().seqno == _receiver.ackno().value() - 1) {
            _sender.send_empty_segment();
            send_segments();
            return;
        }

    // 3. 如果seg设置了ACK，告知 TCPSender
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        send_segments();
    }


    // 4. 如果收到的segment的payload不为空，则要确保至少发送了一个ACK
    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        size_t segs_sent = send_segments();
        if (segs_sent == 0) {
            // _sender没有发送segment
            _sender.send_empty_segment();
            TCPSegment ack_seg = _sender.segments_out().front();
            set_ackno_window(ack_seg);
            _sender.segments_out().pop();
            segments_out().push(ack_seg);
        }
    }

    // handle clean shutdown
    // if (check_inbound() && !_sender.stream_in().eof()) {
    if (check_inbound() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }
    if (check_inbound() && check_outbound()) {
        if (!_linger_after_streams_finish)
            close_connection();
    }
    
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t res = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
    return res;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    // 记录时间流逝
    _time_since_last_segment_received += ms_since_last_tick;
    // tell the TCPSender about the passage of time
    _sender.tick(ms_since_last_tick);
    // 处理TCPSender中超时重传的TCPSegment, 如果发生重传且重传次数超出限制，终止连接并发送一个RST
    if (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ackno_window(seg);
        if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
            _sender.stream_in().set_error();
            inbound_stream().set_error();
            seg.header().rst = true;
            close_connection();
        }
        segments_out().push(seg);
    }
    // 尽可能干净利落的结束连接(handle TIME_WAIT) 
    if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        if (!_linger_after_streams_finish) {
            close_connection();
        } else if (check_inbound() && check_outbound()) {
            _linger_after_streams_finish = false;
            close_connection();
        }
    }

}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
}

void TCPConnection::connect() {
    if (!_active)
        return;
    _sender.fill_window();
    send_segments();
}

// methods added by myself
void TCPConnection::send_rst() {
    _sender.send_empty_segment();
    TCPSegment rst_seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    set_ackno_window(rst_seg);
    rst_seg.header().rst = true;
    segments_out().push(rst_seg);
}

void TCPConnection::close_connection() {
    _active = false;
}

size_t TCPConnection::send_segments() {
    size_t tot = 0;
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ackno_window(seg);
        segments_out().push(seg);
        tot += 1;
    }
    return tot;
}

void TCPConnection::set_ackno_window(TCPSegment& seg) {
    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
    } 
    size_t w_size = _receiver.window_size();
    if (w_size > std::numeric_limits<uint16_t>::max())
        seg.header().win = std::numeric_limits<uint16_t>::max();
    else
        seg.header().win = _receiver.window_size();
}

bool TCPConnection::check_inbound() {
    return _receiver.unassembled_bytes() == 0 && inbound_stream().input_ended();
}

bool TCPConnection::check_outbound() {
    return _sender.bytes_in_flight() == 0 && _sender.stream_in().eof();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            inbound_stream().set_error();
            _sender.stream_in().set_error();
            send_rst();
            _active = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
