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

#include "network-pacer.h"
#include "core.h"

namespace
{
	// Sync packets and small RPCs; RakNet splits anything above omp's default network.mtu into
	// MTU-sized pieces, and each piece counts as one queued message
	constexpr int MIN_QUEUED_MESSAGE_BYTES = 64;
	constexpr int MAX_QUEUED_MESSAGE_BYTES = 576;
	constexpr float MESSAGE_BYTES_SMOOTHING = 0.25f;
	constexpr int MINIMUM_QUEUE_BYTES = 1200;
	// Only reliable messages wait for acks, so a fast link with a high ping legitimately keeps
	// several hundred in flight; this depth is seconds of data at the minimum send rate
	constexpr unsigned RESEND_QUEUE_LIMIT = 2048;
	// RakPeer::GetStatistics scans every connection slot, so it is not sampled every tick
	constexpr std::chrono::milliseconds SAMPLE_INTERVAL(25);
	constexpr std::chrono::seconds DEBUG_INTERVAL(1);
}

NetworkPacer::NetworkPacer()
{
	enabled = true;
	debugEnabled = false;
	queueTime = 100;
	clientWorkRate = 0.0f;
	clientWorkBurst = 50.0f;
	instantStreamRadius = 50.0f;
}

void NetworkPacer::sample(Player &player)
{
	NetworkBudget &budget = player.networkBudget;
	budget.sampleTime = std::chrono::steady_clock::now();
	NetworkStats stats;
	if (!readStatistics(player, stats))
	{
		budget.allowance = std::numeric_limits<int>::max();
		budget.congested = false;
		return;
	}
	budget.bitsPerSecond = stats.bitsPerSecond;
	budget.resendQueue = stats.messagesOnResendQueue;
	budget.sendBuffer = stats.messageSendBuffer;
	if (stats.messagesOnResendQueue >= RESEND_QUEUE_LIMIT)
	{
		budget.allowance = 0;
		budget.congested = true;
		return;
	}
	// bitsPerSecond is RakNet's own bandwidth estimate for this connection, so the target
	// is how many bytes it can transmit within queueTime
	double targetBytes = std::clamp(stats.bitsPerSecond / 8000.0 * queueTime, static_cast<double>(MINIMUM_QUEUE_BYTES), static_cast<double>(std::numeric_limits<int>::max() / 2));
	// While streaming, the queue is mostly our own messages, so their recent size is used for all
	// of it; smaller sync packets in the queue only make this an overestimate
	int queuedMessageBytes = std::clamp(static_cast<int>(budget.messageBytes), MIN_QUEUED_MESSAGE_BYTES, MAX_QUEUED_MESSAGE_BYTES);
	int queuedBytes = static_cast<int>(stats.messageSendBuffer) * queuedMessageBytes;
	budget.allowance = static_cast<int>(targetBytes) - queuedBytes;
	budget.congested = queuedBytes >= static_cast<int>(targetBytes);
}

bool NetworkPacer::isCongested(Player &player)
{
	if (std::chrono::steady_clock::now() - player.networkBudget.sampleTime >= SAMPLE_INTERVAL)
	{
		sample(player);
	}
	return player.networkBudget.congested;
}

bool NetworkPacer::readStatistics(const Player &player, NetworkStats &stats) const
{
	IPlayer *omplayer = core->getPlayers() ? core->getPlayers()->get(player.playerId) : nullptr;
	if (!omplayer || !omplayer->getNetworkData().network)
	{
		return false;
	}
	stats = omplayer->getNetworkData().network->getStatistics(omplayer);
	return true;
}

void NetworkPacer::refill(Player &player)
{
	NetworkBudget &budget = player.networkBudget;
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	bool playerOverride = budget.clientWorkRate >= 0.0f;
	float rate = playerOverride ? budget.clientWorkRate : clientWorkRate;
	float burst = playerOverride ? budget.clientWorkBurst : clientWorkBurst;
	if (rate > 0.0f)
	{
		if (budget.refillTime.time_since_epoch().count())
		{
			float elapsed = std::chrono::duration<float>(now - budget.refillTime).count();
			budget.clientWorkTokens = std::min(burst, budget.clientWorkTokens + rate * elapsed);
		}
		else
		{
			budget.clientWorkTokens = burst;
		}
	}
	else
	{
		budget.clientWorkTokens = std::numeric_limits<float>::max();
	}
	budget.refillTime = now;
	if (now - budget.sampleTime >= SAMPLE_INTERVAL)
	{
		sample(player);
		if (debugEnabled && now - budget.debugTime >= DEBUG_INTERVAL && core->getOmpCore())
		{
			budget.debugTime = now;
			core->getOmpCore()->printLn("[Streamer] Player %d: pending=%u raknetQueue=%u resendQueue=%u bandwidth=%.0fkbps allowance=%dB clientWork=%.1f", player.playerId, static_cast<unsigned>(player.countPendingChunkItems()), budget.sendBuffer, budget.resendQueue, budget.bitsPerSecond / 1000.0, budget.allowance, budget.clientWorkTokens);
		}
	}
}

bool NetworkPacer::canSend(const Player &player, bool automatic) const
{
	return !automatic || !enabled || (player.networkBudget.allowance > 0 && player.networkBudget.clientWorkTokens > 0.0f);
}

void NetworkPacer::consume(Player &player, int bytes, float work, int messages) const
{
	if (enabled)
	{
		NetworkBudget &budget = player.networkBudget;
		budget.allowance -= bytes;
		budget.clientWorkTokens -= work;
		if (messages > 0)
		{
			budget.messageBytes += (static_cast<float>(bytes) / messages - budget.messageBytes) * MESSAGE_BYTES_SMOOTHING;
		}
	}
}
