#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _rto{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // 检查SYN是否已经发送
    if (!_syn_sent) {
        // cout << "syn to be sent" << endl;
        _syn_sent = true;
        TCPSegment seg;
        seg.header().syn = true;
        send_segment(seg);
    }
    // SYN已发送但为得到确认, 则不会继续发送
    if (!_outstanding.empty() && _outstanding.front().header().syn) {
        return;
    }
    // _stream 已空但输入尚未结束
    if (!stream_in().buffer_size() && !stream_in().input_ended()) {
        return;
    }
    // FIN已经发送
    if (_fin_sent) {
        return;
    }

    if (_window_size) {
        // 窗口大小不为0
        while (_window_free_size) {
            TCPSegment seg;
            // 确定payload的长度
            size_t payload_len = TCPConfig::MAX_PAYLOAD_SIZE;
            payload_len = min(payload_len, _window_free_size);
            payload_len = min(payload_len, stream_in().buffer_size());
            seg.payload() = Buffer{stream_in().read(payload_len)};
            if (stream_in().eof() && _window_free_size > payload_len) {
                // 如果输入结束且窗口大小大于payload的长度（因为fin本身占一个字节，需要额外1字节的空间），则发送fin
                seg.header().fin = true;
                _fin_sent = true;
            }
            send_segment(seg);
            if (stream_in().buffer_empty()) {
                break;
            }
        }
    } else if (_window_free_size == 0) {
        // 窗口大小为0（则剩余的空间也应该是0），则应按照窗口大小为1处理， 发送1字节大小的数据
        TCPSegment seg;
        if (stream_in().eof()) {
            // 已经结束输入， 发送1字节的fin
            seg.header().fin = true;
            _fin_sent = true;
            send_segment(seg);
        } else if (!stream_in().buffer_empty()) {
            // 输入没有结束，从输入流中读取一字节数据
            seg.payload() = Buffer{stream_in().read(1)};
            send_segment(seg);
        }
    }

/* 旧版 (没有通过测试)
    while (_next_seqno < _right) {
        // 检查 ByteStream 里还有多少字节
        size_t buf_size = stream_in().buffer_size();
        if (stream_in().input_ended() && buf_size == 0) {
            return;
        }
        // 比较目前能够发送的字节数是否达到 max payload size 
        size_t payload_len = TCPConfig::MAX_PAYLOAD_SIZE;
        size_t remain_len = _right - _next_seqno;
        payload_len = min(remain_len, payload_len);

        // 处理SYN和FIN
        TCPSegment seg;
        bool is_syn = false, is_fin = false;
        // SYN
        if (_next_seqno == 0) {
            is_syn = true;
            payload_len -= 1;
        }
        // FIN
        if (stream_in().input_ended() && buf_size <= payload_len - 1) {
            is_fin = true;
            payload_len -= 1;
        }
        
        seg.header().syn = is_syn;
        seg.header().fin = is_fin;
        payload_len = min(payload_len, buf_size);
        seg.payload() = Buffer{stream_in().read(payload_len)};
        seg.header().seqno = wrap(_next_seqno, _isn);
        _outstanding[_next_seqno] = seg;

        // 发送
        segments_out().push(seg);
        _next_seqno += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();
        // 如果timer停止，则启动
        if (!_timer_is_running) {
            start_timer();
        }
    }
*/
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    // // 如果 window_size 是0，按1处理
    // uint16_t w_size = window_size;
    // if (w_size == 0) {
    //     w_size = 1;
    // }
    uint64_t ackno_abs = unwrap(ackno, _isn, _next_seqno);
    if (!is_ack_valid(ackno_abs)) {
        return;
    }
    _window_size = window_size;
    _window_free_size = window_size;
    // 处理outstanding segments
    while (!_outstanding.empty()) {
        TCPSegment seg = _outstanding.front();
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= ackno_abs) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _outstanding.pop();
            _consecutive_retransmissions = 0;
            _rto = _initial_retransmission_timeout;
            start_timer();
        } else {
            break;
        }
    }

    // 处理window_size
    if (!_outstanding.empty()) {
        // _window_free_size = ackno_abs + window_size;
        // _window_free_size -= unwrap(_outstanding.front().header().seqno, _isn, _next_seqno) + _bytes_in_flight;
        size_t delta_between_ack_and_outstanding = unwrap(_outstanding.front().header().seqno, _isn, _next_seqno) - ackno_abs;
        _window_free_size = window_size - _bytes_in_flight - delta_between_ack_and_outstanding;
    }

    // 如果没有正在发送但未确认的数据，则停止timer
    if (!_bytes_in_flight) {
        stop_timer();
    }

    // 继续填充窗口
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if (!_timer_is_running) {
        return;
    }
    _time_elapsed += ms_since_last_tick;
    // 处理超时的情况
    if (_time_elapsed >= _rto) {
        retransmit();
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    segments_out().push(seg);
}

void TCPSender::start_timer() {
    _timer_is_running = true;
    _time_elapsed = 0;
}

void TCPSender::stop_timer() {
    _timer_is_running = false;
}

void TCPSender::retransmit() {
    segments_out().push(_outstanding.front());
    if (_window_size || _outstanding.front().header().syn) {
        _consecutive_retransmissions += 1;
        _rto *= 2;
    }
    start_timer();
}

void TCPSender::send_segment(TCPSegment& seg) {
    // cout << "sending segment..." << endl;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _window_free_size -= seg.length_in_sequence_space();
    segments_out().push(seg);
    _outstanding.push(seg);
    // cout << "outstanding size: " << segments_out().size() << endl;
    if (!_timer_is_running) {
        start_timer();
    }
}

bool TCPSender::is_ack_valid(uint64_t ack) {
    if (_outstanding.empty()) {
        // 如果没有outstanding segs，那么收到的ack只有<= 期望的下一个序号才是有效的
        if (ack > _next_seqno)
            return false;
        else 
            return true;
    }
    uint64_t lower = unwrap(_outstanding.front().header().seqno, _isn, _next_seqno);
    if (ack < lower || ack > _next_seqno) {
        return false;
    }
    return true;
}