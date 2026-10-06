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

#ifndef OBJECT_UPDATE_REPAIR_H
#define OBJECT_UPDATE_REPAIR_H

#include <array>
#include <chrono>

// Independent deadlines: a rotating object must not starve a position repair (or vice versa).
// One repair is enough. Unreliable and reliable messages share RakNet's HIGH_PRIORITY send queue,
// so the repair leaves after every earlier update, and the client handles packets in arrival order
// (a reliable ordered one can only be held back, never delivered early).
class ObjectUpdateRepair
{
public:
	using Clock = std::chrono::steady_clock;

	void updated(bool rotation, Clock::time_point now)
	{
		active[rotation] = true;
		due[rotation] = now + std::chrono::milliseconds(200);
	}

	bool ready(bool rotation, Clock::time_point now) const
	{
		return active[rotation] && now >= due[rotation];
	}

	void sent(bool rotation)
	{
		active[rotation] = false;
	}

	void defer(Clock::time_point now)
	{
		for (int property = 0; property < 2; ++property)
		{
			if (active[property] && due[property] < now + std::chrono::milliseconds(200))
			{
				due[property] = now + std::chrono::milliseconds(200);
			}
		}
	}

	bool done() const
	{
		return !active[0] && !active[1];
	}

private:
	std::array<bool, 2> active{};
	std::array<Clock::time_point, 2> due{};
};

#endif
