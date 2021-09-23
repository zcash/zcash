/******************************************************************************
 * Copyright © 2021 Komodo Core Deveelopers                                   *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/
#include "komodo_structs.h"
#include "mem_read.h"
#include <mutex>

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
        case(EVENT_REWIND):
            os << "B";
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

event_pubkeys::event_pubkeys(FILE* fp, int32_t height) : event(EVENT_PUBKEYS, height)
{
    num = fgetc(fp);
    if ( fread(pubkeys,33,num,fp) != num )
        throw parse_error("Illegal number of keys: " + std::to_string(num));
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

event_rewind::event_rewind(uint8_t *data, long &pos, long data_len, int32_t height) : event(EVENT_REWIND, height)
{
    // nothing to do
}
std::ostream& operator<<(std::ostream& os, const event_rewind& in)
{
    const event& e = dynamic_cast<const event&>(in);
    os << e;
    return os;
}

event_notarized::event_notarized(uint8_t *data, long &pos, long data_len, int32_t height, const char* _dest, bool includeMoM)
        : event(EVENT_NOTARIZED, height), MoMdepth(0)
{
    memcpy(this->dest, _dest, sizeof(this->dest)-1);
    this->dest[sizeof(this->dest)-1] = 0;
    MoM.SetNull();
    mem_read(this->notarizedheight, data, pos, data_len);
    mem_read(this->blockhash, data, pos, data_len);
    mem_read(this->desttxid, data, pos, data_len);
    if (includeMoM)
    {
        mem_read(this->MoM, data, pos, data_len);
        mem_read(this->MoMdepth, data, pos, data_len);
    }
}

event_notarized::event_notarized(FILE* fp, int32_t height, const char* _dest, bool includeMoM)
        : event(EVENT_NOTARIZED, height), MoMdepth(0)
{
    memcpy(this->dest, _dest, sizeof(this->dest)-1);
    this->dest[sizeof(this->dest)-1] = 0;
    MoM.SetNull();
    if ( fread(&notarizedheight,1,sizeof(notarizedheight),fp) != sizeof(notarizedheight) )
        throw parse_error("Invalid notarization height");
    if ( fread(&blockhash,1,sizeof(blockhash),fp) != sizeof(blockhash) )
        throw parse_error("Invalid block hash");
    if ( fread(&desttxid,1,sizeof(desttxid),fp) != sizeof(desttxid) )
        throw parse_error("Invalid Destination TXID");
    if ( includeMoM )
    {
        if ( fread(&MoM,1,sizeof(MoM),fp) != sizeof(MoM) )
            throw parse_error("Invalid MoM");
        if ( fread(&MoMdepth,1,sizeof(MoMdepth),fp) != sizeof(MoMdepth) )
            throw parse_error("Invalid MoMdepth");
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

event_u::event_u(FILE *fp, int32_t height) : event(EVENT_U, height)
{
    if (fread(&n, 1, sizeof(n), fp) != sizeof(n))
        throw parse_error("Unable to read n of event U from file");
    if (fread(&nid, 1, sizeof(nid), fp) != sizeof(n))
        throw parse_error("Unable to read nid of event U from file");
    if (fread(&mask, 1, sizeof(mask), fp) != sizeof(mask))
        throw parse_error("Unable to read mask of event U from file");
    if (fread(&hash, 1, sizeof(hash), fp) != sizeof(hash))
        throw parse_error("Unable to read hash of event U from file");
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

event_kmdheight::event_kmdheight(FILE *fp, int32_t height, bool includeTimestamp) : event(EVENT_KMDHEIGHT, height)
{
    if ( fread(&kheight,1,sizeof(kheight),fp) != sizeof(kheight) )
        throw parse_error("Unable to parse KMD height");
    if (includeTimestamp)
    {
        if ( fread( &timestamp, 1, sizeof(timestamp), fp) != sizeof(timestamp) )
            throw parse_error("Unable to parse timestamp of KMD height");
    }
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
    mem_read(this->vout, data, pos, data_len);
    mem_read(this->value, data, pos, data_len);
    uint16_t oplen;
    mem_read(oplen, data, pos, data_len);
    if (oplen <= data_len - pos)
    {
        this->opret = std::vector<uint8_t>( &data[pos], &data[pos] + oplen);
        pos += oplen;
    }
}

event_opreturn::event_opreturn(FILE* fp, int32_t height) : event(EVENT_OPRETURN, height)
{
    if ( fread(&txid,1,sizeof(txid),fp) != sizeof(txid) )
        throw parse_error("Unable to parse txid of opreturn record");
    if ( fread(&vout,1,sizeof(vout),fp) != sizeof(vout) )
        throw parse_error("Unable to parse vout of opreturn record");
    if ( fread(&value,1,sizeof(value),fp) != sizeof(value) )
        throw parse_error("Unable to parse value of opreturn record");
    uint16_t oplen;
    if ( fread(&oplen,1,sizeof(oplen),fp) != sizeof(oplen) )
        throw parse_error("Unable to parse length of opreturn record");
    this->opret.resize(oplen);
    if ( fread(this->opret.data(), 1, oplen, fp) != oplen)
        throw parse_error("Unable to parse binary data of opreturn");
}

std::ostream& operator<<(std::ostream& os, const event_opreturn& in)
{
    const event& e = dynamic_cast<const event&>(in);
    os << e 
        << serializable<uint256>(in.txid)
        << serializable<uint16_t>(in.vout)
        << serializable<uint64_t>(in.value)
        << serializable<uint16_t>(in.opret.size());
    os.write( (const char*)in.opret.data(), in.opret.size());
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

event_pricefeed::event_pricefeed(FILE* fp, int32_t height) : event(EVENT_PRICEFEED, height)
{
    num = fgetc(fp);
    if ( num * sizeof(uint32_t) <= sizeof(prices) && fread(prices,sizeof(uint32_t),num,fp) != num )
        throw parse_error("Unable to parse price feed");
}

std::ostream& operator<<(std::ostream& os, const event_pricefeed& in)
{
    const event& e = dynamic_cast<const event&>(in);
    os << e << (uint8_t)in.num;
    os.write((const char*)in.prices, in.num * sizeof(uint32_t));
    return os;
}

} // namespace komodo