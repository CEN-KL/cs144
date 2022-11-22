#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// ByteStream::ByteStream(const size_t capacity) { DUMMY_CODE(capacity); }
ByteStream::ByteStream(const size_t capacity) 
    : _capacity(capacity), _buffer(""), _tot_bytes_written(0), _tot_bytes_read(0), _end_write(false) {}

size_t ByteStream::write(const string &data) {
    // DUMMY_CODE(data);
    // return {};
    size_t data_len = data.length();
    size_t remain_len = remaining_capacity();
    if (data_len <= remain_len) {
        _buffer += data;
        _tot_bytes_written += data_len;
        return data_len;
    } else {
        _buffer += data.substr(0, remain_len);
        _tot_bytes_written += remain_len;
        return remain_len;
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    // DUMMY_CODE(len);
    // return {};
    size_t peek_len = min(_buffer.length(), len);
    return _buffer.substr(0, peek_len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
// void ByteStream::pop_output(const size_t len) { DUMMY_CODE(len); }
void ByteStream::pop_output(const size_t len) {
    size_t buf_len = _buffer.length();
    if (len > buf_len) {
        set_error();
        return;
    }
    _buffer = _buffer.substr(len);
    _tot_bytes_read += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    // DUMMY_CODE(len);
    // return {};
    std::string res = "";
    size_t buf_len = _buffer.length();
    if (len > buf_len) {
        set_error();
        return res;
    }
    _tot_bytes_read += len;
    res += _buffer.substr(0, len);
    _buffer = _buffer.substr(len);
    return res;
}

void ByteStream::end_input() { _end_write = true; }

bool ByteStream::input_ended() const { return _end_write; }

size_t ByteStream::buffer_size() const { return _buffer.length(); }

bool ByteStream::buffer_empty() const { return _buffer.length() == 0; }

bool ByteStream::eof() const { return _buffer.length() == 0 && _end_write; }

size_t ByteStream::bytes_written() const { return _tot_bytes_written; }

size_t ByteStream::bytes_read() const { return _tot_bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.length(); }
