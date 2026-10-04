#pragma once
// Minimal SDK/network doubles. They model delivery/queueing, not RakNet wire encoding.
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>
constexpr int INVALID_PLAYER_ID = 65535, INVALID_OBJECT_ID = 65535;
constexpr int OrderingChannel_Unordered = 3, OrderingChannel_SyncRPC = 2;
constexpr int ENetworkType_RakNetLegacy = 0;
inline int bitsToBytes(int bits) { return (bits + 7) / 8; }
struct Vector3 { float x = 0, y = 0, z = 0; };
struct Rotation { Vector3 value; Vector3 ToEuler() const { return value; } };
template<class T> struct Span
{
    T* pointer; int count;
    Span(T* p, int n) : pointer(p), count(n) {}
};
class NetworkBitStream
{
    std::vector<uint8_t> bytes;
    template<class T> void write(T value)
    {
        auto p = reinterpret_cast<uint8_t*>(&value);
        bytes.insert(bytes.end(), p, p + sizeof(value));
    }
public:
    void writeUINT8(uint8_t v) { write(v); }
    void writeUINT16(uint16_t v) { write(v); }
    void writeVEC3(Vector3 v) { write(v); }
    void WriteCompressed(unsigned v) { write(v); }
    void WriteBits(const uint8_t* p, int bits, bool) { bytes.insert(bytes.end(), p, p + bitsToBytes(bits)); }
    uint8_t* GetData() { return bytes.data(); }
    int GetNumberOfBitsUsed() const { return static_cast<int>(bytes.size() * 8); }
};
struct IPlayer;
struct SingleNetworkOutEventHandler { virtual bool onSend(IPlayer*, NetworkBitStream&) = 0; };
struct Dispatcher { void addEventHandler(SingleNetworkOutEventHandler*, int) {} };
struct Message { int rpc, channel; std::vector<uint8_t> data; };
struct INetwork
{
    int type = ENetworkType_RakNetLegacy;
    bool success = true;
    std::vector<Message> reliable, unreliable;
    Dispatcher dispatcher;
    int getNetworkType() { return type; }
    Dispatcher& getPerRPCOutEventDispatcher() { return dispatcher; }
    bool sendRPC(IPlayer&, int id, Span<uint8_t> data, int channel, bool)
    {
        if (!success) return false;
        reliable.push_back({id, channel, {data.pointer, data.pointer + bitsToBytes(data.count)}});
        return true;
    }
    bool sendPacket(IPlayer&, Span<uint8_t> data, int channel, bool)
    {
        unreliable.push_back({0, channel, {data.pointer, data.pointer + bitsToBytes(data.count)}});
        return true;
    }
};
struct NetworkData { INetwork* network = nullptr; };
struct IPlayer
{
    int id = 1; NetworkData data;
    int getID() { return id; }
    NetworkData& getNetworkData() { return data; }
};
struct ObjectAttachmentData { enum class Type { None, Vehicle }; Type type = Type::None; };
struct IPlayerObject
{
    bool moving = false;
    Vector3 position{10,20,30}; Rotation rotation{{40,50,60}};
    ObjectAttachmentData attachment;
    bool isMoving() { return moving; }
    ObjectAttachmentData getAttachmentData() { return attachment; }
    Vector3 getPosition() { return position; }
    Rotation getRotation() { return rotation; }
};
struct IPlayerObjectData {};
namespace ompgdk
{
    inline IPlayerObject* liveObject = nullptr;
    template<class Pool, class Entity> Entity* GetPlayerEntity(int, int) { return liveObject; }
}
namespace Item { struct Object {}; }
struct NetworkBudget { int allowance = 10000; };
struct Player { int playerId = 1; std::unordered_map<int,int> internalObjects; NetworkBudget networkBudget; };
struct Data { std::unordered_map<int,Player> players; std::unordered_map<int,std::shared_ptr<Item::Object>> objects; };
struct Players { IPlayer* peer = nullptr; IPlayer* get(int id) { return peer && peer->id == id ? peer : nullptr; } };
namespace StreamCost { constexpr int MESSAGE_OVERHEAD_BYTES = 12; }
struct NetworkPacer
{
    bool enabled = true, congested = false;
    bool getEnabled() { return enabled; }
    bool isCongested(Player&) { return congested; }
    void consume(Player& p, int bytes, float) { if (enabled) p.networkBudget.allowance -= bytes; }
};
class ObjectUnreliableUpdates;
struct Core
{
    Data data; Players players; NetworkPacer pacer; ObjectUnreliableUpdates* updates = nullptr;
    Data* getData() { return &data; }
    Players* getPlayers() { return &players; }
    NetworkPacer* getNetworkPacer() { return &pacer; }
    ObjectUnreliableUpdates* getUnreliableUpdates() { return updates; }
};
inline Core* core = nullptr;
