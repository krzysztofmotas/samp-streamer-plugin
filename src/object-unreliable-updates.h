/*
 * Copyright (C) 2017 Incognito
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OBJECT_UNRELIABLE_UPDATES_H
#define OBJECT_UNRELIABLE_UPDATES_H

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "object-update-repair.h"

struct Player;
namespace Item
{
	struct Object;
}

// Resends omp's SetObjectPosition/SetObjectRotation RPCs for objects updated many times per second
// as unreliable packets. A lost update is superseded by the next one anyway, while a reliable one
// is resent late, blocks the ordered channel and lowers RakNet's bandwidth estimate.
// An idle property is resent once reliably, 200ms after its last update, so a lost or dropped
// final update cannot leave the client showing a stale position.
class ObjectUnreliableUpdates
{
public:
	ObjectUnreliableUpdates();

	void begin(int playerId);
	void finish(Player &player, int objectId, bool rotation);
	void settle();
	void removePlayer(int playerId);

	bool onObjectUpdate(IPlayer *peer, NetworkBitStream &bs, int rpcId);

private:
	struct Pending
	{
		std::weak_ptr<Item::Object> object;
		int internalId = INVALID_OBJECT_ID;
		ObjectUpdateRepair repair;
	};

	int capturePlayerId;
	IPlayer *capturePeer;
	int captureRpcId;
	std::vector<uint8_t> data;
	int bits;
	std::unordered_map<uint64_t, Pending> pending;
};

// Lives in the omp component rather than in Core, because network dispatchers keep these
// pointers after Core is reset on Pawn unload
struct ObjectUnreliableUpdateHooks
{
	struct SetObjectPositionHook : public SingleNetworkOutEventHandler
	{
		bool onSend(IPlayer *peer, NetworkBitStream &bs) override;
	};

	struct SetObjectRotationHook : public SingleNetworkOutEventHandler
	{
		bool onSend(IPlayer *peer, NetworkBitStream &bs) override;
	};

	void registerOn(INetwork *network);

	SetObjectPositionHook setObjectPositionHook;
	SetObjectRotationHook setObjectRotationHook;
};

#endif
