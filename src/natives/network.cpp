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

#include "../main.h"

#include "../natives.h"
#include "../core.h"
#include "../utility.h"

cell AMX_NATIVE_CALL Natives::Streamer_ToggleNetworkThrottle(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	core->getNetworkPacer()->setEnabled(static_cast<int>(params[1]) != 0);
	return 1;
}

cell AMX_NATIVE_CALL Natives::Streamer_IsToggleNetworkThrottle(AMX *amx, cell *params)
{
	return static_cast<cell>(core->getNetworkPacer()->getEnabled());
}

cell AMX_NATIVE_CALL Natives::Streamer_ToggleNetworkThrottleDebug(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	core->getNetworkPacer()->setDebugEnabled(static_cast<int>(params[1]) != 0);
	return 1;
}

cell AMX_NATIVE_CALL Natives::Streamer_IsToggleNetworkThrottleDebug(AMX *amx, cell *params)
{
	return static_cast<cell>(core->getNetworkPacer()->getDebugEnabled());
}

cell AMX_NATIVE_CALL Natives::Streamer_GetNetworkQueueTime(AMX *amx, cell *params)
{
	return static_cast<cell>(core->getNetworkPacer()->getQueueTime());
}

cell AMX_NATIVE_CALL Natives::Streamer_SetNetworkQueueTime(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	return static_cast<cell>(core->getNetworkPacer()->setQueueTime(static_cast<int>(params[1])));
}

cell AMX_NATIVE_CALL Natives::Streamer_GetClientWorkRate(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Utility::storeFloatInNative(amx, params[1], core->getNetworkPacer()->getClientWorkRate());
	Utility::storeFloatInNative(amx, params[2], core->getNetworkPacer()->getClientWorkBurst());
	return 1;
}

cell AMX_NATIVE_CALL Natives::Streamer_SetClientWorkRate(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	return static_cast<cell>(core->getNetworkPacer()->setClientWorkRate(amx_ctof(params[1]), amx_ctof(params[2])));
}

cell AMX_NATIVE_CALL Natives::Streamer_GetInstantStreamRadius(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Utility::storeFloatInNative(amx, params[1], core->getNetworkPacer()->getInstantStreamRadius());
	return 1;
}

cell AMX_NATIVE_CALL Natives::Streamer_SetInstantStreamRadius(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	return static_cast<cell>(core->getNetworkPacer()->setInstantStreamRadius(amx_ctof(params[1])));
}

cell AMX_NATIVE_CALL Natives::Streamer_GetPlayerNetworkStats(AMX *amx, cell *params)
{
	CHECK_PARAMS(5);
	std::unordered_map<int, Player>::iterator p = core->getData()->players.find(static_cast<int>(params[1]));
	if (p != core->getData()->players.end())
	{
		NetworkStats stats = {};
		core->getNetworkPacer()->readStatistics(p->second, stats);
		Utility::storeIntegerInNative(amx, params[2], static_cast<int>(p->second.countPendingChunkItems()));
		Utility::storeIntegerInNative(amx, params[3], static_cast<int>(stats.messageSendBuffer));
		Utility::storeIntegerInNative(amx, params[4], static_cast<int>(stats.messagesOnResendQueue));
		Utility::storeFloatInNative(amx, params[5], static_cast<float>(stats.bitsPerSecond / 1000.0));
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::Streamer_ToggleInlineMaterials(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	core->getMaterialInliner()->setEnabled(static_cast<int>(params[1]) != 0);
	return 1;
}

cell AMX_NATIVE_CALL Natives::Streamer_IsToggleInlineMaterials(AMX *amx, cell *params)
{
	return static_cast<cell>(core->getMaterialInliner()->getEnabled());
}

cell AMX_NATIVE_CALL Natives::Streamer_GetPlayerClientWorkRate(AMX *amx, cell *params)
{
	CHECK_PARAMS(3);
	std::unordered_map<int, Player>::iterator p = core->getData()->players.find(static_cast<int>(params[1]));
	if (p != core->getData()->players.end())
	{
		Utility::storeFloatInNative(amx, params[2], p->second.networkBudget.clientWorkRate);
		Utility::storeFloatInNative(amx, params[3], p->second.networkBudget.clientWorkBurst);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::Streamer_SetPlayerClientWorkRate(AMX *amx, cell *params)
{
	CHECK_PARAMS(3);
	std::unordered_map<int, Player>::iterator p = core->getData()->players.find(static_cast<int>(params[1]));
	if (p != core->getData()->players.end())
	{
		float rate = amx_ctof(params[2]), burst = amx_ctof(params[3]);
		if (rate >= 0.0f && burst <= 0.0f)
		{
			return 0;
		}
		p->second.networkBudget.clientWorkRate = rate;
		p->second.networkBudget.clientWorkBurst = burst;
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::Streamer_ToggleObjectUnreliableUpdates(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	std::unordered_map<int, Item::SharedObject>::iterator o = core->getData()->objects.find(static_cast<int>(params[1]));
	if (o != core->getData()->objects.end())
	{
		o->second->unreliableUpdates = static_cast<int>(params[2]) != 0;
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::Streamer_IsToggleObjectUnreliable(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	std::unordered_map<int, Item::SharedObject>::iterator o = core->getData()->objects.find(static_cast<int>(params[1]));
	if (o != core->getData()->objects.end())
	{
		return static_cast<cell>(o->second->unreliableUpdates);
	}
	return 0;
}
