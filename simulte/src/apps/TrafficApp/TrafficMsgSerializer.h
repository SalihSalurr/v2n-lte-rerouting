#ifndef __TRAFFICMSGSERIALIZER_H
#define __TRAFFICMSGSERIALIZER_H

#include "common/LteCommon.h"
#include "inet/common/packet/serializer/FieldsChunkSerializer.h"

class SIMULTE_API VehicleReportMsgSerializer : public inet::FieldsChunkSerializer
{
  protected:
    virtual void serialize(inet::MemoryOutputStream& stream, const inet::Ptr<const inet::Chunk>& chunk) const override;
    virtual const inet::Ptr<inet::Chunk> deserialize(inet::MemoryInputStream& stream) const override;
  public:
    VehicleReportMsgSerializer() : FieldsChunkSerializer() {}
};

class SIMULTE_API ServerRouteMsgSerializer : public inet::FieldsChunkSerializer
{
  protected:
    virtual void serialize(inet::MemoryOutputStream& stream, const inet::Ptr<const inet::Chunk>& chunk) const override;
    virtual const inet::Ptr<inet::Chunk> deserialize(inet::MemoryInputStream& stream) const override;
  public:
    ServerRouteMsgSerializer() : FieldsChunkSerializer() {}
};

#endif
