#include "TrafficMsg_m.h"
#include "TrafficMsgSerializer.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include <cstring>

using namespace inet;

Register_Serializer(VehicleReportMsg, VehicleReportMsgSerializer);
Register_Serializer(ServerRouteMsg, ServerRouteMsgSerializer);

// ── Yardımcı: double ↔ uint64 ──────────────────────────────
static void writeDouble(inet::MemoryOutputStream& stream, double val)
{
    uint64_t bits;
    memcpy(&bits, &val, sizeof(double));
    stream.writeUint64Be(bits);
}

static double readDouble(inet::MemoryInputStream& stream)
{
    uint64_t bits = stream.readUint64Be();
    double val;
    memcpy(&val, &bits, sizeof(double));
    return val;
}

// ═════════════════════════════════════════════════════════════
//  VehicleReportMsg — 6 alan
//  edgeIndex(4) + destEdgeIndex(4) + speed(8) + timestamp(8)
//  + realTravelTime(8) + laneChangeCount(4) = 36 byte
// ═════════════════════════════════════════════════════════════
void VehicleReportMsgSerializer::serialize(inet::MemoryOutputStream& stream,
    const inet::Ptr<const inet::Chunk>& chunk) const
{
    const auto& msg = staticPtrCast<const VehicleReportMsg>(chunk);
    stream.writeUint32Be((uint32_t)msg->getEdgeIndex());
    stream.writeUint32Be((uint32_t)msg->getDestEdgeIndex());
    writeDouble(stream, msg->getSpeed());
    writeDouble(stream, msg->getTimestamp());
    writeDouble(stream, msg->getRealTravelTime());
    stream.writeUint32Be((uint32_t)msg->getLaneChangeCount());
}

const inet::Ptr<inet::Chunk> VehicleReportMsgSerializer::deserialize(inet::MemoryInputStream& stream) const
{
    auto msg = inet::makeShared<VehicleReportMsg>();
    msg->setEdgeIndex((int)stream.readUint32Be());
    msg->setDestEdgeIndex((int)stream.readUint32Be());
    msg->setSpeed(readDouble(stream));
    msg->setTimestamp(readDouble(stream));
    msg->setRealTravelTime(readDouble(stream));
    msg->setLaneChangeCount((int)stream.readUint32Be());
    msg->setChunkLength(inet::B(36));
    return msg;
}

// ═════════════════════════════════════════════════════════════
//  ServerRouteMsg
//  routeLength(4) + 64 × routeEdges(4) = 260 byte
// ═════════════════════════════════════════════════════════════
void ServerRouteMsgSerializer::serialize(inet::MemoryOutputStream& stream,
    const inet::Ptr<const inet::Chunk>& chunk) const
{
    const auto& msg = staticPtrCast<const ServerRouteMsg>(chunk);
    int len = msg->getRouteLength();
    stream.writeUint32Be((uint32_t)len);
    for (int i = 0; i < 64; i++) {
        stream.writeUint32Be((uint32_t)msg->getRouteEdges(i));
    }
}

const inet::Ptr<inet::Chunk> ServerRouteMsgSerializer::deserialize(inet::MemoryInputStream& stream) const
{
    auto msg = inet::makeShared<ServerRouteMsg>();
    int len = (int)stream.readUint32Be();
    msg->setRouteLength(len);
    for (int i = 0; i < 64; i++) {
        msg->setRouteEdges(i, (int)stream.readUint32Be());
    }
    msg->setChunkLength(inet::B(260));
    return msg;
}
