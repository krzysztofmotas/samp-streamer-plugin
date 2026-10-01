# Unreliable object updates

`Streamer_ToggleObjectUnreliableUpdates(objectid, true)` opts an object into unreliable
`SetDynamicObjectPos` / `SetDynamicObjectRot` updates on the RakNet legacy backend.
Other network backends retain the original reliable RPC. The option is off by default.

This mode is intended for frequently updated visual objects where transient stale state is
acceptable. It does **not** guarantee ordering or uninterrupted correctness on a vanilla client.
Keep ordinary reliable updates for objects whose exact position must always be synchronized.

## Repair behaviour

- Position and rotation have independent deadlines. After a property has had no updates for
  200 ms, it gets one reliable ordered repair, and tracking of that property ends; a new update
  starts it again.
- One repair is enough. Unreliable and reliable messages share RakNet's HIGH_PRIORITY send queue,
  so the repair leaves the server after every earlier update, and the client handles packets in
  arrival order (a reliable ordered packet can be held back, never delivered early). Only network
  reordering of datagrams sent 100+ ms apart could put a stale update after it.
- Repairs use live open.mp player-object state, without calling setters or mutating server state.
- Congestion, an exhausted network budget, or a failed send postpones a repair; it is not forgotten.
  Successful repairs consume the streamer's network byte allowance. The intervals are eligibility
  times, not delivery deadlines.
- Repairs wait while the player object is moving through open.mp or is attached, rechecked every
  200 ms. They resume when it is stationary and detached. Frequent direct position updates do not block idle rotation repairs.
- Disconnects clear tracking. Missing objects, replaced dynamic-object instances and changed/missing
  internal object mappings invalidate tracking. Disabling the option does not cancel existing repairs:
  packets already in flight can still arrive.

There is a bandwidth cost: one small reliable RPC per property each time an object/player pair
goes idle. This traffic is subject to pacing. Tracking ends after those repairs, on stream-out
or object destruction as observed by the repair pass, or on disconnect.

## Protocol checks

The implementation was checked against the upstream source (master, 2026-09-27):

- [open.mp legacy transport](https://github.com/openmultiplayer/open.mp/blob/master/Server/Components/LegacyNetwork/legacy_network_impl.hpp):
  `sendPacket` with `OrderingChannel_Unordered` uses `UNRELIABLE`; `sendRPC` on
  `OrderingChannel_SyncRPC` uses `RELIABLE_ORDERED`, as does object creation.
- [RakNet RPC envelope](https://github.com/openmultiplayer/RakNet/blob/master/Source/RakPeer.cpp):
  legacy RPCs contain `ID_RPC`, numeric RPC ID, compressed payload bit length and payload.
  [Legacy packet IDs](https://github.com/openmultiplayer/RakNet/blob/master/Include/raknet/PacketEnumerations.h)
  assign `ID_RPC` the value 20.
- [RakNet reliability layer](https://github.com/openmultiplayer/RakNet/blob/master/Source/ReliabilityLayer.cpp):
  only reliable messages are acknowledged and put on the resend list, so an unreliable update is
  never resent and never counts as loss in the histogram that lowers the bandwidth estimate.
  open.mp never calls `SetUnreliableTimeout`, so a queued unreliable message waits for its turn
  like any other; that is why updates are dropped while the player's queue is congested.
  Sequenced receive indices are per ordering channel, so putting unrelated object/property updates
  on one sequenced channel could discard necessary updates.
- SA-MP client (0.3.7 R3, decompiled): `ScrSetObjectPosition`/`ScrSetObjectRotation` ignore an
  object ID that does not exist yet, and teleport without interpolation.
- [open.mp object RPCs](https://github.com/openmultiplayer/open.mp/blob/master/Shared/NetCode/object.hpp):
  RPCs 45 and 46 carry a uint16 object ID followed by a Vector3.
- [open.mp object implementation](https://github.com/openmultiplayer/open.mp/blob/master/Server/Components/Objects/object.cpp):
  player-object setters both mutate server state and send their RPC; repairs send only the RPC.

Reordering during animation, updates arriving before creation and old packets targeting reused client
object IDs remain protocol limitations. Strict per-object freshness requires client-side generation/
sequence checks or a different transport design.

## Regression tests

The standalone target compiles the production `object-unreliable-updates.cpp` against small SDK and
network doubles; it does not require the main project's submodules. It tests independent deadlines,
a single repair per idle property, congestion and send failures, live state, movement/attachment
handling, stream-out, ID reuse, disconnect and fallback for other network backends. It does not test
wire encoding or a real SA-MP client.

```powershell
cmake -S tests/object-unreliable-updates -B build/unreliable-tests -A Win32
cmake --build build/unreliable-tests --config Release
ctest --test-dir build/unreliable-tests -C Release --output-on-failure
```

For live validation, introduce packet loss/reordering, update multiple objects and properties, then
stop one property while continuing the other. Check eventual correction after congestion clears,
stream-out/in, reconnect, and transitions to/from `MoveDynamicObject` and attachment.
