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
// The second repair covers an unreliable packet delayed past the first one. Later arrivals stay
// unrepaired, because repairing an idle object forever costs more than the feature saves.
class ObjectUpdateRepair
{
public:
	using Clock = std::chrono::steady_clock;

	static constexpr int MAX_REPAIRS = 2;

	void updated(bool rotation, Clock::time_point now)
	{
		active[rotation] = true;
		repairs[rotation] = 0;
		due[rotation] = now + std::chrono::milliseconds(200);
	}

	bool ready(bool rotation, Clock::time_point now) const
	{
		return active[rotation] && now >= due[rotation];
	}

	void sent(bool rotation, Clock::time_point now)
	{
		if (++repairs[rotation] >= MAX_REPAIRS)
		{
			active[rotation] = false;
		}
		else
		{
			due[rotation] = now + std::chrono::seconds(1);
		}
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
	std::array<int, 2> repairs{};
	std::array<Clock::time_point, 2> due{};
};

#endif
