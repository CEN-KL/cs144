#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _unass_bytes(0)
    , _unass_index(0)
    , _buffer(capacity, '\0')
    , _tag(capacity ,false)
    , _eof(false)
    , _output(capacity)
    , _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
//! 注意，此处的eof参数指的是data字符串的最后一个字符是否是整个字节流的末尾
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // DUMMY_CODE(data, index, eof);
    if (eof) {
        _eof = true;
    }
    size_t len_data = data.length();

    // if (len_data == 0 && _eof && _unass_bytes == 0) {
    //     _output.end_input();
    //     return;
    // }
    
    if (index > _unass_index + _capacity) {
        return;
    }
    if (index >= _unass_index) {
        size_t offset = index - _unass_index;
        size_t len_handle = min(len_data, _capacity - _output.buffer_size() - offset);
        // 需要添加判断，如果有字符被丢弃了，则说明一定读不到eof了，故将_eof设置为false
        if (len_data > len_handle) {
            _eof = false;
        }
        for (size_t i = 0; i < len_handle; i++) {
            if (!_tag[i + offset]) {
                _buffer[i + offset] = data[i];
                _tag[i + offset] = true;
                _unass_bytes += 1;
            }
        }
    } else if (index + len_data > _unass_index) {
        size_t offset = _unass_index - index;
        size_t len_handle = min(len_data - offset, _capacity - _output.buffer_size());
        // 需要添加判断，如果有字符被丢弃了，则说明一定读不到eof了，故将_eof设置为false
        if (len_handle < len_data - offset) {
            _eof = false;
        }
        for (size_t i = 0; i < len_handle; i++) {
            if (!_tag[i]) {
                _buffer[i] = data[i + offset];
                _tag[i] = true;
                _unass_bytes += 1;
            }
        }
    }

    string res = "";
    while (_tag.front()) {
        res += _buffer.front();
        _buffer.pop_front();
        _tag.pop_front();
        _buffer.push_back('\0');
        _tag.push_back(false);
    }
    if (res.length() > 0) {
        _output.write(res);
        _unass_bytes -= res.length();
        _unass_index += res.length();
    }

    if (_eof && _unass_bytes == 0) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unass_bytes; }

bool StreamReassembler::empty() const { return _unass_bytes == 0; }
