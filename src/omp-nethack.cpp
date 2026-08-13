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
#include "core.h"

#include "bitstream.hpp"

#include "omp-nethack.h"

static int GetObjectStreamerId(int playerid, int objectid)
{
	std::unordered_map<int, Player>::iterator p = core->getData()->players.find(playerid);
	if (p != core->getData()->players.end())
	{
		std::unordered_map<int, int>::iterator i = p->second.internalObjectsReverse.find(objectid);
		if (i != p->second.internalObjectsReverse.end())
		{
			return i->second;
		}
	}
	return INVALID_STREAMER_ID;
}

static int GetObjectInternalId(int playerid, int streamerid)
{
	std::unordered_map<int, Player>::iterator p = core->getData()->players.find(playerid);
	if (p != core->getData()->players.end())
	{
		std::unordered_map<int, int>::iterator i = p->second.internalObjects.find(streamerid);
		if (i != p->second.internalObjects.end())
		{
			return i->second;
		}
	}
	return INVALID_OBJECT_ID;
}

void OMPNetHack::Process(IPlayerPool* players, IPlayer* peer, NetworkBitStream& bs)
{
	if (!peer)
	{
		return;
	}

	uint16_t playerid;
	bs.SetReadOffset(8);
	bs.readUINT16(playerid);

	IPlayer* player = players->get(playerid);
	if (!player)
	{
		return;
	}

	PlayerSurfingData surfingData = player->getSurfingData();
	if (surfingData.type != PlayerSurfingData::Type::PlayerObject)
	{
		return;
	}

	int objectStreamerId = GetObjectStreamerId(playerid, surfingData.ID);
	if (objectStreamerId == INVALID_STREAMER_ID)
	{
		return;
	}

	int playerObjectId = GetObjectInternalId(peer->getID(), objectStreamerId);
	if (playerObjectId == INVALID_OBJECT_ID)
	{
		return;
	}

	bool conditionalRead;
	uint16_t animationId;
	uint16_t animationFlags;

	bs.readBIT(conditionalRead);
	if (conditionalRead)
	{
		bs.IgnoreBits(16);
	}
	bs.readBIT(conditionalRead);
	if (conditionalRead)
	{
		bs.IgnoreBits(16);
	}

	bs.IgnoreBits(16 + (32 * 3) + (4 + 16 * 3) + 8 + 8 + 8);

	float magnitude;
	if (bs.readFLOAT(magnitude) && magnitude > 0.00001f)
	{
		bs.IgnoreBits(3 * 16);
	}

	int surfingDataWriteOffset = bs.GetReadOffset();

	bs.IgnoreBits(1);

	bs.readBIT(conditionalRead);
	if (conditionalRead)
	{
		bs.readUINT16(animationId);
		bs.readUINT16(animationFlags);
	}

	bs.SetWriteOffset(surfingDataWriteOffset);
	bs.writeBIT(true);
	bs.writeUINT16(playerObjectId + VEHICLE_POOL_SIZE);
	bs.writeVEC3(surfingData.offset);
	bs.writeBIT(conditionalRead);
	if (conditionalRead)
	{
		bs.writeUINT16(animationId);
		bs.writeUINT16(animationFlags);
	}
	return;
}
