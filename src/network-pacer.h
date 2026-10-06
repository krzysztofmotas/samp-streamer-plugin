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

#ifndef NETWORK_PACER_H
#define NETWORK_PACER_H

#include <chrono>

struct Player;

namespace StreamCost
{
	// Wire sizes include RakNet's per-message overhead (RPC header, reliability, ordering index)
	constexpr int MESSAGE_OVERHEAD_BYTES = 12;
	constexpr int CREATE_OBJECT_BYTES = 40 + MESSAGE_OVERHEAD_BYTES;
	constexpr int ATTACH_OBJECT_BYTES = 30 + MESSAGE_OVERHEAD_BYTES;
	constexpr int MOVE_OBJECT_BYTES = 45 + MESSAGE_OVERHEAD_BYTES;
	constexpr int SMALL_OBJECT_RPC_BYTES = 2 + MESSAGE_OVERHEAD_BYTES;
	constexpr int MATERIAL_BYTES = 13 + MESSAGE_OVERHEAD_BYTES;
	constexpr int MATERIAL_TEXT_BYTES = 17 + MESSAGE_OVERHEAD_BYTES;
	constexpr int SET_MAP_ICON_BYTES = 19 + MESSAGE_OVERHEAD_BYTES;
	constexpr int REMOVE_MAP_ICON_BYTES = 1 + MESSAGE_OVERHEAD_BYTES;
	constexpr int CREATE_TEXT_LABEL_BYTES = 29 + MESSAGE_OVERHEAD_BYTES;
	constexpr int DELETE_TEXT_LABEL_BYTES = 2 + MESSAGE_OVERHEAD_BYTES;

	// Relative main-thread cost on the client. SetObjectMaterial may load the source model
	// synchronously, and material text is rendered to a texture in a single frame.
	constexpr float CREATE_WORK = 1.0f;
	constexpr float DESTROY_WORK = 0.25f;
	constexpr float MATERIAL_WORK = 3.0f;
	constexpr float MATERIAL_TEXT_WORK = 5.0f;
	constexpr float MAP_ICON_WORK = 0.25f;
}

struct NetworkBudget
{
	int allowance = 0;
	double bitsPerSecond = 0.0;
	// A negative rate means the global setting applies
	float clientWorkRate = -1.0f;
	float clientWorkBurst = 0.0f;
	float clientWorkTokens = 0.0f;
	bool congested = false;
	std::chrono::steady_clock::time_point debugTime;
	float messageBytes = 0.0f;
	std::chrono::steady_clock::time_point refillTime;
	unsigned resendQueue = 0;
	std::chrono::steady_clock::time_point sampleTime;
	unsigned sendBuffer = 0;
};

class NetworkPacer
{
public:
	NetworkPacer();

	inline bool getEnabled()
	{
		return enabled;
	}

	inline void setEnabled(bool value)
	{
		enabled = value;
	}

	inline bool getDebugEnabled()
	{
		return debugEnabled;
	}

	inline void setDebugEnabled(bool value)
	{
		debugEnabled = value;
	}

	inline int getQueueTime()
	{
		return queueTime;
	}

	inline bool setQueueTime(int value)
	{
		if (value > 0)
		{
			queueTime = value;
			return true;
		}
		return false;
	}

	inline float getClientWorkRate()
	{
		return clientWorkRate;
	}

	inline float getClientWorkBurst()
	{
		return clientWorkBurst;
	}

	inline bool setClientWorkRate(float rate, float burst)
	{
		if (rate >= 0.0f && burst > 0.0f)
		{
			clientWorkRate = rate;
			clientWorkBurst = burst;
			return true;
		}
		return false;
	}

	inline float getInstantStreamRadius()
	{
		return instantStreamRadius;
	}

	inline bool setInstantStreamRadius(float value)
	{
		if (value >= 0.0f)
		{
			instantStreamRadius = value;
			return true;
		}
		return false;
	}

	inline bool isInstantStreamDistance(float comparableDistance) const
	{
		return comparableDistance <= instantStreamRadius * instantStreamRadius;
	}

	void refill(Player &player);
	void sample(Player &player);
	bool readStatistics(const Player &player, NetworkStats &stats) const;
	bool isCongested(Player &player);
	bool canSend(const Player &player, bool automatic) const;
	void consume(Player &player, int bytes, float work, int messages = 1) const;

private:
	bool enabled;
	bool debugEnabled;
	int queueTime;
	float clientWorkRate;
	float clientWorkBurst;
	float instantStreamRadius;
};

#endif
