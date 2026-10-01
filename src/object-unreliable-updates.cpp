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

#include "main.h"

#include "bitstream.hpp"

#include "object-unreliable-updates.h"
#include "core.h"

namespace
{
	constexpr int SET_OBJECT_POSITION_RPC = 45;
	constexpr int SET_OBJECT_ROTATION_RPC = 46;
	// ID_RPC in omp's RAKNET_LEGACY build; an RPC is sent as this byte, the RPC id, the compressed
	// payload bit length and the payload, which the client handles regardless of reliability
	constexpr uint8_t ID_RPC = 20;

	uint64_t pendingKey(int playerId, int objectId)
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(playerId)) << 32) | static_cast<uint32_t>(objectId);
	}
}

ObjectUnreliableUpdates::ObjectUnreliableUpdates()
{
	capturePlayerId = INVALID_PLAYER_ID;
	capturePeer = nullptr;
	captureRpcId = 0;
	bits = 0;
}

void ObjectUnreliableUpdates::begin(int playerId)
{
	capturePlayerId = playerId;
	capturePeer = nullptr;
	bits = 0;
}

void ObjectUnreliableUpdates::finish(Player &player, int objectId, bool rotation)
{
	capturePlayerId = INVALID_PLAYER_ID;
	if (!capturePeer || !bits)
	{
		return;
	}
	auto object = core->getData()->objects.find(objectId);
	auto internal = player.internalObjects.find(objectId);
	if (object == core->getData()->objects.end() || internal == player.internalObjects.end())
	{
		capturePeer = nullptr;
		bits = 0;
		return;
	}
	Pending &entry = pending[pendingKey(player.playerId, objectId)];
	if (entry.object.lock() != object->second || entry.internalId != internal->second)
	{
		entry = Pending{};
		entry.object = object->second;
		entry.internalId = internal->second;
	}
	entry.repair.updated(rotation, std::chrono::steady_clock::now());
	INetwork *network = capturePeer->getNetworkData().network;
	if (network && !(core->getNetworkPacer()->getEnabled() && core->getNetworkPacer()->isCongested(player)))
	{
		NetworkBitStream bs;
		bs.writeUINT8(ID_RPC);
		bs.writeUINT8(static_cast<uint8_t>(captureRpcId));
		bs.WriteCompressed(static_cast<unsigned int>(bits));
		bs.WriteBits(data.data(), bits, false);
		network->sendPacket(*capturePeer, Span<uint8_t>(bs.GetData(), bs.GetNumberOfBitsUsed()), OrderingChannel_Unordered, false);
	}
	capturePeer = nullptr;
	bits = 0;
}

void ObjectUnreliableUpdates::settle()
{
	const auto now = std::chrono::steady_clock::now();
	for (auto e = pending.begin(); e != pending.end();)
	{
		const int playerId = static_cast<int>(e->first >> 32);
		const int objectId = static_cast<int>(static_cast<uint32_t>(e->first));
		auto p = core->getData()->players.find(playerId);
		auto o = core->getData()->objects.find(objectId);
		if (p == core->getData()->players.end() || o == core->getData()->objects.end() || e->second.object.lock() != o->second)
		{
			e = pending.erase(e);
			continue;
		}
		auto i = p->second.internalObjects.find(objectId);
		if (i == p->second.internalObjects.end() || i->second != e->second.internalId)
		{
			e = pending.erase(e);
			continue;
		}
		Pending &entry = e->second;
		if (!entry.repair.ready(false, now) && !entry.repair.ready(true, now))
		{
			++e;
			continue;
		}
		IPlayer *peer = core->getPlayers() ? core->getPlayers()->get(playerId) : nullptr;
		IPlayerObject *object = ompgdk::GetPlayerEntity<IPlayerObjectData, IPlayerObject>(playerId, i->second);
		if (!peer || !object || !peer->getNetworkData().network)
		{
			++e;
			continue;
		}
		// Do not teleport objects driven by MoveDynamicObject or attachments. Keep the repair
		// pending until those operations end; read the live omp state, not cached streamer state.
		if (object->isMoving() || object->getAttachmentData().type != ObjectAttachmentData::Type::None)
		{
			entry.repair.defer(now);
			++e;
			continue;
		}
		NetworkPacer *pacer = core->getNetworkPacer();
		for (int property = 0; property < 2; ++property)
		{
			const bool rotation = property != 0;
			if (!entry.repair.ready(rotation, now))
			{
				continue;
			}
			if (pacer->getEnabled() && (pacer->isCongested(p->second) || p->second.networkBudget.allowance <= 0))
			{
				continue;
			}
			NetworkBitStream bs;
			bs.writeUINT16(i->second);
			bs.writeVEC3(rotation ? object->getRotation().ToEuler() : object->getPosition());
			// Use the same reliable ordered channel as CreateObject. Sending the RPC directly
			// avoids mutating the server object just to refresh a client's copy.
			if (peer->getNetworkData().network->sendRPC(*peer, rotation ? SET_OBJECT_ROTATION_RPC : SET_OBJECT_POSITION_RPC,
				Span<uint8_t>(bs.GetData(), bs.GetNumberOfBitsUsed()), OrderingChannel_SyncRPC, false))
			{
				entry.repair.sent(rotation);
				pacer->consume(p->second, bitsToBytes(bs.GetNumberOfBitsUsed()) + StreamCost::MESSAGE_OVERHEAD_BYTES, 0.0f);
			}
		}
		if (entry.repair.done())
		{
			e = pending.erase(e);
		}
		else
		{
			++e;
		}
	}
}

void ObjectUnreliableUpdates::removePlayer(int playerId)
{
	for (auto e = pending.begin(); e != pending.end();)
	{
		if (static_cast<int>(e->first >> 32) == playerId)
		{
			e = pending.erase(e);
		}
		else
		{
			++e;
		}
	}
}

bool ObjectUnreliableUpdates::onObjectUpdate(IPlayer *peer, NetworkBitStream &bs, int rpcId)
{
	if (capturePlayerId == INVALID_PLAYER_ID || !peer || peer->getID() != capturePlayerId || bits
		|| !peer->getNetworkData().network || peer->getNetworkData().network->getNetworkType() != ENetworkType_RakNetLegacy)
	{
		return true;
	}
	capturePeer = peer;
	captureRpcId = rpcId;
	bits = bs.GetNumberOfBitsUsed();
	data.assign(bs.GetData(), bs.GetData() + bitsToBytes(bits));
	return false;
}

bool ObjectUnreliableUpdateHooks::SetObjectPositionHook::onSend(IPlayer *peer, NetworkBitStream &bs)
{
	return !core || core->getUnreliableUpdates()->onObjectUpdate(peer, bs, SET_OBJECT_POSITION_RPC);
}

bool ObjectUnreliableUpdateHooks::SetObjectRotationHook::onSend(IPlayer *peer, NetworkBitStream &bs)
{
	return !core || core->getUnreliableUpdates()->onObjectUpdate(peer, bs, SET_OBJECT_ROTATION_RPC);
}

void ObjectUnreliableUpdateHooks::registerOn(INetwork *network)
{
	network->getPerRPCOutEventDispatcher().addEventHandler(&setObjectPositionHook, SET_OBJECT_POSITION_RPC);
	network->getPerRPCOutEventDispatcher().addEventHandler(&setObjectRotationHook, SET_OBJECT_ROTATION_RPC);
}
