import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;

namespace inet;

class VehicleReportMsg extends FieldsChunk
{
    int    edgeIndex;
    int    destEdgeIndex;
    double speed;
    double timestamp;
    double realTravelTime;
    int    laneChangeCount;
}

class ServerRouteMsg extends FieldsChunk
{
    int routeLength;
    int routeEdges[64];
}
