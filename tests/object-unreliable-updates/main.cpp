// Compile the production translation unit against small SDK doubles. No game server is needed.
#define MAIN_H
#define CORE_H
#include "bitstream.hpp"
#include "../../src/object-unreliable-updates.cpp"
#include <iostream>
#include <stdexcept>
#include <thread>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Fixture
{
    Core state;
    INetwork network;
    IPlayer peer;
    IPlayerObject live;
    ObjectUnreliableUpdates updates;
    Fixture()
    {
        core = &state;
        state.updates = &updates;
        peer.data.network = &network;
        state.players.peer = &peer;
        state.data.players[1].internalObjects[7] = 3;
        state.data.objects[7] = std::make_shared<Item::Object>();
        ompgdk::liveObject = &live;
    }
    bool update(bool rotation = false)
    {
        NetworkBitStream bs;
        bs.writeUINT16(3);
        bs.writeVEC3(rotation ? live.rotation.ToEuler() : live.position);
        updates.begin(1);
        bool passThrough = updates.onObjectUpdate(&peer, bs, rotation ? 46 : 45);
        updates.finish(state.data.players[1], 7, rotation);
        return passThrough;
    }
    void due() { std::this_thread::sleep_for(std::chrono::milliseconds(220)); }
};

int main()
{
    try
    {
        using namespace std::chrono;
        auto t = ObjectUpdateRepair::Clock::time_point{};
        ObjectUpdateRepair schedule;
        require(!schedule.ready(false, t + seconds(10)), "untouched properties must not be repaired");
        schedule.updated(false, t);
        for (int ms = 0; ms <= 1000; ms += 50) schedule.updated(true, t + milliseconds(ms));
        require(schedule.ready(false, t + milliseconds(200)), "rotation starved position repair");
        require(!schedule.ready(true, t + milliseconds(1100)), "rotation repaired while active");
        schedule.sent(false);
        require(!schedule.ready(false, t + seconds(10)), "idle property repaired more than once");
        schedule.updated(false, t + milliseconds(1250));
        require(!schedule.ready(false, t + milliseconds(1300)), "new animation must defer repair");
        require(schedule.ready(false, t + milliseconds(1450)), "new animation never settles");

        {
            Fixture f;
            f.state.pacer.congested = true;
            require(!f.update(), "legacy update was not captured");
            require(f.network.unreliable.empty(), "congested update should be dropped");
            f.due(); f.updates.settle();
            require(f.network.reliable.empty(), "repair ignored congestion");
            f.state.pacer.congested = false;
            f.state.data.players[1].networkBudget.allowance = 0;
            f.updates.settle();
            require(f.network.reliable.empty(), "repair ignored budget");
            f.state.data.players[1].networkBudget.allowance = 10000;
            f.network.success = false;
            f.updates.settle();
            f.network.success = true;
            f.live.position.x = 123;
            f.updates.settle();
            require(f.network.reliable.size() == 1, "failed send or dropped update was forgotten");
            auto& message = f.network.reliable.back();
            require(message.rpc == 45 && message.channel == OrderingChannel_SyncRPC, "wrong repair RPC/channel");
            float x;
            std::memcpy(&x, message.data.data() + 2, sizeof(x));
            require(x == 123, "repair did not read live omp state");
            require(f.state.data.players[1].networkBudget.allowance < 10000, "repair not charged to budget");
            std::this_thread::sleep_for(milliseconds(1020));
            f.updates.settle();
            require(f.network.reliable.size() == 1, "idle object repaired more than once");
        }
        {
            Fixture f;
            f.update(); f.update(true); f.due();
            f.live.moving = true; f.updates.settle();
            require(f.network.reliable.empty(), "repair interfered with MoveDynamicObject");
            f.live.moving = false;
            f.live.attachment.type = ObjectAttachmentData::Type::Vehicle;
            f.updates.settle();
            require(f.network.reliable.empty(), "repair interfered with attachment");
            f.live.attachment.type = ObjectAttachmentData::Type::None;
            f.due(); f.updates.settle();
            require(f.network.reliable.size() == 2, "repair lost during move/attachment");
        }
        {
            Fixture f;
            f.update(); f.due();
            f.state.data.objects[7] = std::make_shared<Item::Object>();
            f.updates.settle();
            require(f.network.reliable.empty(), "old repair applied to reused dynamic object ID");
            f.update(); f.due();
            f.state.data.players[1].internalObjects.clear();
            f.updates.settle();
            require(f.network.reliable.empty(), "repair survived stream-out");
            f.state.data.players[1].internalObjects[7] = 3;
            f.update(); f.due();
            f.updates.removePlayer(1);
            f.updates.settle();
            require(f.network.reliable.empty(), "repair survived disconnect");
        }
        {
            Fixture f;
            f.update(); f.due();
            f.update(true); // Continued rotation must not reset the position deadline.
            f.updates.settle();
            require(f.network.reliable.size() == 1 && f.network.reliable[0].rpc == 45,
                "production capture path couples position and rotation deadlines");
            f.due(); f.updates.settle();
            require(f.network.reliable.size() == 2 && f.network.reliable[1].rpc == 46,
                "rotation did not settle independently");
        }
        {
            Fixture f;
            f.network.type = 1;
            require(f.update(), "non-RakNet backend must use original RPC");
            f.due(); f.updates.settle();
            require(f.network.reliable.empty() && f.network.unreliable.empty(), "legacy packet sent to other backend");
        }
        std::cout << "All unreliable update regression scenarios passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
