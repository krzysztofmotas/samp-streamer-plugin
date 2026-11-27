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

#ifndef VEHICLE_STATE_H
#define VEHICLE_STATE_H

#include "item.h"

// Dynamic vehicles are streamed globally: one real open.mp vehicle exists while any player is in range.
// Everything the script or the players changed on the real vehicle is copied back into the item before
// the real vehicle is released, and applied again when it is recreated.
namespace VehicleState
{
	IVehiclesComponent *getComponent();

	IVehicle *getInternal(int vehicleId);
	IVehicle *getInternal(const Item::SharedVehicle &vehicle);
	int getStreamerId(int internalId);

	int getWorld(const Item::SharedVehicle &vehicle);

	// Creates the real vehicle and applies the stored state. Returns INVALID_VEHICLE_ID on failure.
	int create(const Item::SharedVehicle &vehicle);
	// Copies the real vehicle's state into the item, then releases it.
	void destroy(const Item::SharedVehicle &vehicle, bool snapshotState = true);

	void snapshot(const Item::SharedVehicle &vehicle, IVehicle &internal);
	// Snapshots the real vehicle if it is streamed in. Returns it, or nullptr.
	IVehicle *refresh(const Item::SharedVehicle &vehicle);
	void apply(const Item::SharedVehicle &vehicle, IVehicle &internal);
	void applyTrailerLinks(const Item::SharedVehicle &vehicle);

	// A pinned vehicle is never streamed out: it is occupied, or force-streamed by PutPlayerInDynamicVehicle.
	// Cabs and trailers are not pinned; streamVehicles keeps a linked pair in or out together.
	bool isPinned(const Item::SharedVehicle &vehicle, IVehicle &internal);

	// Follows real vehicles that players moved, so the item stays in the right grid cell.
	void syncPositions();

	void resetToSpawn(const Item::SharedVehicle &vehicle);
}

#endif
