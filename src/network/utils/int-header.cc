#include "int-header.h"

namespace ns3
{

const uint64_t IntHop::lineRateValues[8] =
    {25000000000lu, 50000000000lu, 100000000000lu, 200000000000lu, 400000000000lu, 0, 0, 0};
uint32_t IntHop::multi = 1;

IntHeader::Mode IntHeader::mode = NONE;
int IntHeader::pint_bytes = 2;

IntHeader::IntHeader()
{
    IntHeader_t.nhop = 0;
    for (uint32_t i = 0; i < maxHop; i++)
        IntHeader_t.hop[i] = {0};
}

uint32_t
IntHeader::GetStaticSize()
{
    if (mode == NORMAL)
    {
        return sizeof(IntHeader_t.hop) + sizeof(IntHeader_t.nhop);
    }
    else if (mode == TS)
    {
        return sizeof(ts);
    }
    else if (mode == PINT)
    {
        return sizeof(pint);
    }
    else
    {
        return 0;
    }
}

void
IntHeader::PushHop(uint64_t time, uint64_t bytes, uint32_t qlen, uint64_t rate)
{
    // only do this in INT mode
    if (mode == NORMAL)
    {
        uint32_t idx = IntHeader_t.nhop % maxHop;
        IntHeader_t.hop[idx].Set(time, bytes, qlen, rate);
        IntHeader_t.nhop++;
    }
}

void
IntHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    if (mode == NORMAL)
    {
        for (uint32_t j = 0; j < maxHop; j++)
        {
            i.WriteU32(IntHeader_t.hop[j].buf[0]);
            i.WriteU32(IntHeader_t.hop[j].buf[1]);
        }
        i.WriteU16(IntHeader_t.nhop);
    }
    else if (mode == TS)
    {
        i.WriteU64(ts);
    }
    else if (mode == PINT)
    {
        if (pint_bytes == 1)
            i.WriteU8(pint.IntHeader_u.power_lo8);
        else if (pint_bytes == 2)
            i.WriteU16(pint.power);
    }
}

uint32_t
IntHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    if (mode == NORMAL)
    {
        for (uint32_t j = 0; j < maxHop; j++)
        {
            IntHeader_t.hop[j].buf[0] = i.ReadU32();
            IntHeader_t.hop[j].buf[1] = i.ReadU32();
        }
        IntHeader_t.nhop = i.ReadU16();
    }
    else if (mode == TS)
    {
        ts = i.ReadU64();
    }
    else if (mode == PINT)
    {
        if (pint_bytes == 1)
            pint.IntHeader_u.power_lo8 = i.ReadU8();
        else if (pint_bytes == 2)
            pint.power = i.ReadU16();
    }
    return GetStaticSize();
}

uint64_t
IntHeader::GetTs(void)
{
    if (mode == TS)
        return ts;
    return 0;
}

uint16_t
IntHeader::GetPower(void)
{
    if (mode == PINT)
        return pint_bytes == 1 ? pint.IntHeader_u.power_lo8 : pint.power;
    return 0;
}

void
IntHeader::SetPower(uint16_t power)
{
    if (mode == PINT)
    {
        if (pint_bytes == 1)
            pint.IntHeader_u.power_lo8 = power;
        else
            pint.power = power;
    }
}

} // namespace ns3
