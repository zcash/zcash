#include <komodo_structs.h>

extern std::mutex komodo_mutex;


template<class T>
std::size_t mem_read(T& dest, uint8_t *filedata, long &fpos, long datalen)
{
    if (fpos + sizeof(T) <= datalen)
    {
        memcpy( &dest, &filedata[fpos], sizeof(T) );
        fpos += sizeof(T);
        return sizeof(T);
    }
    throw parse_error("Invalid size: " + std::to_string(sizeof(T)) );
}

template<class T, std::size_t N>
std::size_t mem_read(T(&dest)[N], uint8_t *filedata, long& fpos, long datalen)
{
    std::size_t sz = sizeof(T) * N;
    if (fpos + sz <= datalen)
    {
        memcpy( &dest, &filedata[fpos], sz );
        fpos += sz;
        return sz;
    }
    throw parse_error("Invalid size: "  + std::to_string( sz ) );
}

template<class T, std::size_t N>
std::size_t mem_nread(T(&dest)[N], size_t num_elements, uint8_t *filedata, long& fpos, long datalen)
{
    std::size_t sz = sizeof(T) * num_elements;
    if (fpos + sz <= datalen)
    {
        memcpy( &dest, &filedata[fpos], sz );
        fpos += sz;
        return sz;
    }
    throw parse_error("Invalid size: " + std::to_string(sz));
}

bool komodo_state::add_event(const std::string& symbol, const uint32_t height, std::shared_ptr<event> in)
{
    if (ASSETCHAINS_SYMBOL[0] != 0)
    {
        std::lock_guard<std::mutex> lock(komodo_mutex);
        events.push_back( in );
        return true;
    }
    return false;
}

/***
 * ctor from data stream
 * @param data the data stream
 * @param pos the starting position (will advance)
 * @param data_len full length of data
 */
komodo_event_pubkeys::komodo_event_pubkeys(uint8_t* data, long& pos, long data_len, int32_t height) : event(KOMODO_EVENT_PUBKEYS, height)
{
    num = data[pos++];
    if (num > 64)
        throw parse_error("Illegal number of keys: " + std::to_string(num));
    mem_nread(pubkeys, num, data, pos, data_len);
}

std::ostream& operator<<(std::ostream& os, const komodo_event_pubkeys& in)
{
    os << in.num;
    for(uint8_t i = 0; i < in.num; ++i)
        os << in.pubkeys[i];
    return os;
}
