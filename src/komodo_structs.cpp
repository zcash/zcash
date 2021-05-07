#include <komodo_structs.h>

extern std::mutex komodo_mutex;


/***
 * komodo_state
 */

bool komodo_state::add_event(const std::string& symbol, const uint32_t height, std::shared_ptr<komodo::event> in)
{
    if (ASSETCHAINS_SYMBOL[0] != 0)
    {
        std::lock_guard<std::mutex> lock(komodo_mutex);
        events.push_back( in );
        return true;
    }
    return false;
}

namespace komodo {

/***
 * Helps serialize integrals
 */
template<class T>
class serializable
{
public:
    serializable(const T& in) : value(in) {}
    T value;
};

std::ostream& operator<<(std::ostream& os, serializable<int32_t> in)
{
    os  << (uint8_t) (in.value & 0x000000ff)
        << (uint8_t) ((in.value & 0x0000ff00) >> 8)
        << (uint8_t) ((in.value & 0x00ff0000) >> 16)
        << (uint8_t) ((in.value & 0xff000000) >> 24);
    return os;
}

std::ostream& operator<<(std::ostream& os, serializable<uint16_t> in)
{
    os  << (uint8_t)  (in.value & 0x00ff)
        << (uint8_t) ((in.value & 0xff00) >> 8);
    return os;
}

std::ostream& operator<<(std::ostream& os, serializable<uint256> in)
{
    in.value.Serialize(os);
    return os;
}

std::ostream& operator<<(std::ostream& os, serializable<uint64_t> in)
{
    os  << (uint8_t)  (in.value & 0x00000000000000ff)
        << (uint8_t) ((in.value & 0x000000000000ff00) >> 8)
        << (uint8_t) ((in.value & 0x0000000000ff0000) >> 16)
        << (uint8_t) ((in.value & 0x00000000ff000000) >> 24)
        << (uint8_t) ((in.value & 0x000000ff00000000) >> 32)
        << (uint8_t) ((in.value & 0x0000ff0000000000) >> 40)
        << (uint8_t) ((in.value & 0x00ff000000000000) >> 48)
        << (uint8_t) ((in.value & 0xff00000000000000) >> 56);
    return os;
}

/***
 * Read a chunk of memory
 */

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

std::size_t mem_readn(void* dest, size_t num_bytes, uint8_t *filedata, long &fpos, long datalen)
{
    if (fpos + num_bytes <= datalen)
    {
        memcpy(dest, &filedata[fpos], num_bytes );
        fpos += num_bytes;
        return num_bytes;
    }
    throw parse_error("Invalid size: " + std::to_string(num_bytes) );
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

/****
 * Read a size that is less than the array length
 */
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


/****
 * This serializes the 5 byte header of an event
 * @param os the stream
 * @param in the event
 * @returns the stream
 */
std::ostream& operator<<(std::ostream& os, const event& in)
{
    switch (in.type)
    {
        case(EVENT_PUBKEYS):
            os << "P";
            break;
        case(EVENT_NOTARIZED):
        {
            event_notarized* tmp = dynamic_cast<event_notarized*>(const_cast<event*>(&in));
            if (tmp->MoMdepth == 0)
                os << "N";
            else
                os << "M";
            break;
        }
        case(EVENT_U):
            os << "U";
            break;
        case(EVENT_KMDHEIGHT):
        {
            event_kmdheight* tmp = dynamic_cast<event_kmdheight*>(const_cast<event*>(&in));
            if (tmp->timestamp == 0)
                os << "K";
            else
                os << "T";
            break;
        }
        case(EVENT_OPRETURN):
            os << "R";
            break;
        case(EVENT_PRICEFEED):
            os << "V";
            break;
    }
    os << serializable<int32_t>(in.height);
    return os;
}

/***
 * ctor from data stream
 * @param data the data stream
 * @param pos the starting position (will advance)
 * @param data_len full length of data
 */
event_pubkeys::event_pubkeys(uint8_t* data, long& pos, long data_len, int32_t height) : event(EVENT_PUBKEYS, height)
{
    num = data[pos++];
    if (num > 64)
        throw parse_error("Illegal number of keys: " + std::to_string(num));
    mem_nread(pubkeys, num, data, pos, data_len);
}

std::ostream& operator<<(std::ostream& os, const event_pubkeys& in)
{
    const event& e = dynamic_cast<const event&>(in);
    os << e;
    os << in.num;
    for(uint8_t i = 0; i < in.num-1; ++i)
        os << in.pubkeys[i];
    return os;
}

event_notarized::event_notarized(uint8_t *data, long &pos, long data_len, int32_t height, bool includeMoM) : event(EVENT_NOTARIZED, height)
{
    mem_read(this->notarizedheight, data, pos, data_len);
    mem_read(this->blockhash, data, pos, data_len);
    mem_read(this->desttxid, data, pos, data_len);
    if (includeMoM)
    {
        mem_read(this->MoM, data, pos, data_len);
        mem_read(this->MoMdepth, data, pos, data_len);
    }
}

std::ostream& operator<<(std::ostream& os, const event_notarized& in)
{
    const event& e = dynamic_cast<const event&>(in);
    os << e;
    os << serializable<int32_t>(in.notarizedheight);
    os << serializable<uint256>(in.blockhash);
    os << serializable<uint256>(in.desttxid);
    if (in.MoMdepth > 0)
    {
        os << serializable<uint256>(in.MoM);
        os << serializable<int32_t>(in.MoMdepth);
    }
    return os;
}

event_u::event_u(uint8_t *data, long &pos, long data_len, int32_t height) : event(EVENT_U, height)
{
    mem_read(this->n, data, pos, data_len);
    mem_read(this->nid, data, pos, data_len);
    mem_read(this->mask, data, pos, data_len);
    mem_read(this->hash, data, pos, data_len);
}

std::ostream& operator<<(std::ostream& os, const event_u& in)
{
    const event& e = dynamic_cast<const event&>(in);
    os << e;
    os << in.n << in.nid;
    os.write((const char*)in.mask, 8);
    os.write((const char*)in.hash, 32);
    return os;
}

event_kmdheight::event_kmdheight(uint8_t* data, long &pos, long data_len, int32_t height, bool includeTimestamp) : event(EVENT_KMDHEIGHT, height)
{
    mem_read(this->kheight, data, pos, data_len);
    if (includeTimestamp)
        mem_read(this->timestamp, data, pos, data_len);
}

std::ostream& operator<<(std::ostream& os, const event_kmdheight& in)
{
    const event& e = dynamic_cast<const event&>(in);
    os << e << serializable<int32_t>(in.kheight);
    if (in.timestamp > 0)
        os << serializable<int32_t>(in.timestamp);
    return os;
}

event_opreturn::event_opreturn(uint8_t *data, long &pos, long data_len, int32_t height) : event(EVENT_OPRETURN, height)
{
    mem_read(this->txid, data, pos, data_len);
    mem_read(this->value, data, pos, data_len);
    mem_read(this->vout, data, pos, data_len);
    mem_read(this->oplen, data, pos, data_len);
    this->opret = new uint8_t[this->oplen];
    mem_readn(this->opret, this->oplen, data, pos, data_len);
}

event_opreturn::~event_opreturn()
{
    if (opret != nullptr)
        delete[] opret;
}

std::ostream& operator<<(std::ostream& os, const event_opreturn& in)
{
    const event& e = dynamic_cast<const event&>(in);
    os << e 
        << serializable<uint256>(in.txid)
        << serializable<uint64_t>(in.value)
        << serializable<uint16_t>(in.vout)
        << serializable<uint16_t>(in.oplen);
    os.write( (const char*)in.opret, in.oplen);
    return os;
}

event_pricefeed::event_pricefeed(uint8_t *data, long &pos, long data_len, int32_t height) : event(EVENT_PRICEFEED, height)
{
    mem_read(this->num, data, pos, data_len);
    // we're only interested if there are 35 prices. 
    // If there is any other amount, advance the pointer, but don't save it
    if (this->num == 35)
        mem_nread(this->prices, this->num, data, pos, data_len);
    else
        pos += num * sizeof(uint32_t);
}

std::ostream& operator<<(std::ostream& os, const event_pricefeed& in)
{
    const event& e = dynamic_cast<const event&>(in);
    os << e << (uint8_t)in.num;
    os.write((const char*)in.prices, in.num * sizeof(uint32_t));
    return os;
}

} // namespace komodo