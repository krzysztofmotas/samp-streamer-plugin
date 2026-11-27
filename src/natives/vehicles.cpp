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
#include "../vehicle-state.h"

#include "Server/Components/Vehicles/vehicle_components.hpp"

namespace
{
	Item::SharedVehicle findVehicle(cell id)
	{
		std::unordered_map<int, Item::SharedVehicle>::iterator v = core->getData()->vehicles.find(static_cast<int>(id));
		if (v != core->getData()->vehicles.end())
		{
			return v->second;
		}
		return Item::SharedVehicle();
	}

	Item::SharedVehicle createVehicle(AMX *amx, cell *params)
	{
		if (core->getData()->getGlobalMaxItems(STREAMER_TYPE_VEHICLE) == core->getData()->vehicles.size())
		{
			return Item::SharedVehicle();
		}
		int modelId = static_cast<int>(params[1]);
		if (!Impl::isValidVehicleModel(modelId))
		{
			Utility::logError("CreateDynamicVehicle: Invalid vehicle model %d.", modelId);
			return Item::SharedVehicle();
		}
		Item::SharedVehicle vehicle(new Item::Vehicle);
		vehicle->amx = amx;
		vehicle->vehicleId = Item::Vehicle::identifier.get();
		vehicle->inverseAreaChecking = false;
		vehicle->originalComparableStreamDistance = -1.0f;
		vehicle->positionOffset = Eigen::Vector3f::Zero();
		vehicle->modelId = modelId;
		vehicle->spawnPosition = Eigen::Vector3f(amx_ctof(params[2]), amx_ctof(params[3]), amx_ctof(params[4]));
		vehicle->spawnAngle = amx_ctof(params[5]);
		vehicle->position = vehicle->spawnPosition;
		vehicle->angle = vehicle->spawnAngle;
		vehicle->color1 = static_cast<int>(params[6]);
		vehicle->color2 = static_cast<int>(params[7]);
		vehicle->respawnDelay = static_cast<int>(params[8]);
		vehicle->siren = static_cast<int>(params[9]) != 0;
		vehicle->health = 1000.0f;
		vehicle->interior = 0;
		vehicle->priority = 0;
		return vehicle;
	}

	void finishVehicle(const Item::SharedVehicle &vehicle, float streamDistance)
	{
		// A real vehicle lives in exactly one world.
		if (vehicle->worlds.empty())
		{
			vehicle->worlds.insert(0);
		}
		else if (vehicle->worlds.size() > 1)
		{
			int world = *vehicle->worlds.begin();
			vehicle->worlds.clear();
			vehicle->worlds.insert(world);
		}
		if (vehicle->interiors.size() == 1)
		{
			vehicle->interior = *vehicle->interiors.begin();
		}
		vehicle->comparableStreamDistance = streamDistance < STREAMER_STATIC_DISTANCE_CUTOFF ? streamDistance : streamDistance * streamDistance;
		vehicle->streamDistance = streamDistance;
		core->getGrid()->addVehicle(vehicle);
		core->getData()->vehicles.insert(std::make_pair(vehicle->vehicleId, vehicle));
	}

	void setLink(int cabId, int trailerId)
	{
		std::unordered_map<int, Item::SharedVehicle> &vehicles = core->getData()->vehicles;
		std::unordered_map<int, Item::SharedVehicle>::iterator cab = vehicles.find(cabId);
		if (cab == vehicles.end())
		{
			return;
		}
		std::unordered_map<int, Item::SharedVehicle>::iterator old = vehicles.find(cab->second->trailer);
		if (old != vehicles.end() && old->second->cab == cabId)
		{
			old->second->cab = INVALID_STREAMER_ID;
		}
		cab->second->trailer = INVALID_STREAMER_ID;
		std::unordered_map<int, Item::SharedVehicle>::iterator trailer = vehicles.find(trailerId);
		if (trailer != vehicles.end())
		{
			std::unordered_map<int, Item::SharedVehicle>::iterator oldCab = vehicles.find(trailer->second->cab);
			if (oldCab != vehicles.end() && oldCab->second->trailer == trailerId)
			{
				oldCab->second->trailer = INVALID_STREAMER_ID;
			}
			trailer->second->cab = cabId;
			cab->second->trailer = trailerId;
		}
	}
}

cell AMX_NATIVE_CALL Natives::CreateDynamicVehicle(AMX *amx, cell *params)
{
	CHECK_PARAMS(15);
	Item::SharedVehicle vehicle = createVehicle(amx, params);
	if (!vehicle)
	{
		return INVALID_STREAMER_ID;
	}
	Utility::addToContainer(vehicle->worlds, static_cast<int>(params[10]));
	Utility::addToContainer(vehicle->interiors, static_cast<int>(params[11]));
	Utility::addToContainer(vehicle->players, static_cast<int>(params[12]));
	Utility::addToContainer(vehicle->areas, static_cast<int>(params[14]));
	vehicle->priority = static_cast<int>(params[15]);
	finishVehicle(vehicle, amx_ctof(params[13]));
	return static_cast<cell>(vehicle->vehicleId);
}

cell AMX_NATIVE_CALL Natives::CreateDynamicVehicleEx(AMX *amx, cell *params)
{
	CHECK_PARAMS(19);
	Item::SharedVehicle vehicle = createVehicle(amx, params);
	if (!vehicle)
	{
		return INVALID_STREAMER_ID;
	}
	Utility::convertArrayToContainer(amx, params[11], params[16], vehicle->worlds);
	Utility::convertArrayToContainer(amx, params[12], params[17], vehicle->interiors);
	Utility::convertArrayToContainer(amx, params[13], params[18], vehicle->players);
	Utility::convertArrayToContainer(amx, params[14], params[19], vehicle->areas);
	vehicle->priority = static_cast<int>(params[15]);
	finishVehicle(vehicle, amx_ctof(params[10]));
	return static_cast<cell>(vehicle->vehicleId);
}

cell AMX_NATIVE_CALL Natives::DestroyDynamicVehicle(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	std::unordered_map<int, Item::SharedVehicle>::iterator v = core->getData()->vehicles.find(static_cast<int>(params[1]));
	if (v != core->getData()->vehicles.end())
	{
		Utility::destroyVehicle(v);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::IsValidDynamicVehicle(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	return findVehicle(params[1]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Natives::IsDynamicVehicleStreamedIn(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	IPlayer *player = ompgdk::GetPlayer(static_cast<int>(params[2]));
	if (vehicle && player)
	{
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			return internal->isStreamedInForPlayer(*player) ? 1 : 0;
		}
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleInternalID(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	std::unordered_map<int, int>::iterator i = core->getData()->internalVehicles.find(static_cast<int>(params[1]));
	if (i != core->getData()->internalVehicles.end())
	{
		return static_cast<cell>(i->second);
	}
	return INVALID_VEHICLE_ID;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleFromInternal(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	return static_cast<cell>(VehicleState::getStreamerId(static_cast<int>(params[1])));
}

cell AMX_NATIVE_CALL Natives::GetPlayerDynamicVehicle(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	IPlayerVehicleData *data = ompgdk::GetPlayerData<IPlayerVehicleData>(static_cast<int>(params[1]));
	if (data && data->getVehicle())
	{
		return static_cast<cell>(VehicleState::getStreamerId(data->getVehicle()->getID()));
	}
	return INVALID_STREAMER_ID;
}

cell AMX_NATIVE_CALL Natives::PutPlayerInDynamicVehicle(AMX *amx, cell *params)
{
	CHECK_PARAMS(3);
	IPlayer *player = ompgdk::GetPlayer(static_cast<int>(params[1]));
	Item::SharedVehicle vehicle = findVehicle(params[2]);
	if (!player || !vehicle)
	{
		return 0;
	}
	IVehicle *internal = VehicleState::getInternal(vehicle);
	if (!internal)
	{
		if (VehicleState::create(vehicle) == INVALID_VEHICLE_ID)
		{
			return 0;
		}
		VehicleState::applyTrailerLinks(vehicle);
		internal = VehicleState::getInternal(vehicle);
	}
	// Keep it alive until the player is inside and the streamer has caught up with their position.
	vehicle->forcedUntil = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	internal->putPlayer(*player, static_cast<int>(params[3]));
	return 1;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehiclePos(AMX *amx, cell *params)
{
	CHECK_PARAMS(4);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		Utility::storeFloatInNative(amx, params[2], vehicle->position[0]);
		Utility::storeFloatInNative(amx, params[3], vehicle->position[1]);
		Utility::storeFloatInNative(amx, params[4], vehicle->position[2]);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::SetDynamicVehiclePos(AMX *amx, cell *params)
{
	CHECK_PARAMS(4);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		vehicle->position = Eigen::Vector3f(amx_ctof(params[2]), amx_ctof(params[3]), amx_ctof(params[4]));
		core->getGrid()->removeVehicle(vehicle, true);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setPosition(Vector3(vehicle->position[0], vehicle->position[1], vehicle->position[2]));
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleZAngle(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		Utility::storeFloatInNative(amx, params[2], vehicle->angle);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::SetDynamicVehicleZAngle(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		vehicle->angle = amx_ctof(params[2]);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setZAngle(vehicle->angle);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleHealth(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		Utility::storeFloatInNative(amx, params[2], vehicle->health);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::SetDynamicVehicleHealth(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		vehicle->health = amx_ctof(params[2]);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setHealth(vehicle->health);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleDamageStatus(AMX *amx, cell *params)
{
	CHECK_PARAMS(5);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		Utility::storeIntegerInNative(amx, params[2], vehicle->damagePanels);
		Utility::storeIntegerInNative(amx, params[3], vehicle->damageDoors);
		Utility::storeIntegerInNative(amx, params[4], vehicle->damageLights);
		Utility::storeIntegerInNative(amx, params[5], vehicle->damageTyres);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::UpdateDynamicVehicleDamageStatus(AMX *amx, cell *params)
{
	CHECK_PARAMS(5);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		vehicle->damagePanels = static_cast<int>(params[2]);
		vehicle->damageDoors = static_cast<int>(params[3]);
		vehicle->damageLights = static_cast<int>(params[4]);
		vehicle->damageTyres = static_cast<int>(params[5]);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setDamageStatus(vehicle->damagePanels, vehicle->damageDoors, static_cast<uint8_t>(vehicle->damageLights), static_cast<uint8_t>(vehicle->damageTyres));
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::RepairDynamicVehicle(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		vehicle->health = 1000.0f;
		vehicle->damagePanels = 0;
		vehicle->damageDoors = 0;
		vehicle->damageLights = 0;
		vehicle->damageTyres = 0;
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->repair();
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::ChangeDynamicVehicleColours(AMX *amx, cell *params)
{
	CHECK_PARAMS(3);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		vehicle->color1 = static_cast<int>(params[2]);
		vehicle->color2 = static_cast<int>(params[3]);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setColour(vehicle->color1, vehicle->color2);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleColours(AMX *amx, cell *params)
{
	CHECK_PARAMS(3);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		Utility::storeIntegerInNative(amx, params[2], vehicle->color1);
		Utility::storeIntegerInNative(amx, params[3], vehicle->color2);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::ChangeDynamicVehiclePaintjob(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		vehicle->paintjob = static_cast<int>(params[2]);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setPaintJob(vehicle->paintjob);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehiclePaintjob(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		return static_cast<cell>(vehicle->paintjob);
	}
	return -1;
}

cell AMX_NATIVE_CALL Natives::AddDynamicVehicleComponent(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		int component = static_cast<int>(params[2]);
		int slot = Impl::getVehicleComponentSlot(component);
		if (slot < 0 || slot >= MAX_VEHICLE_COMPONENT_SLOT || !Impl::isValidComponentForVehicleModel(vehicle->modelId, component))
		{
			return 0;
		}
		vehicle->components[slot] = component;
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->addComponent(component);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::RemoveDynamicVehicleComponent(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		int component = static_cast<int>(params[2]);
		int slot = Impl::getVehicleComponentSlot(component);
		if (slot < 0 || slot >= MAX_VEHICLE_COMPONENT_SLOT)
		{
			return 0;
		}
		VehicleState::refresh(vehicle);
		if (vehicle->components[slot] != component)
		{
			return 0;
		}
		vehicle->components[slot] = 0;
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->removeComponent(component);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleComponentInSlot(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	int slot = static_cast<int>(params[2]);
	if (vehicle && slot >= 0 && slot < MAX_VEHICLE_COMPONENT_SLOT)
	{
		VehicleState::refresh(vehicle);
		return static_cast<cell>(vehicle->components[slot]);
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::SetDynamicVehicleNumberPlate(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		vehicle->plate = Utility::convertNativeStringToString(amx, params[2]);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setPlate(vehicle->plate);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleNumberPlate(AMX *amx, cell *params)
{
	CHECK_PARAMS(3);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		Utility::convertStringToNativeString(amx, params[2], params[3], vehicle->plate);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::SetDynamicVehicleParamsEx(AMX *amx, cell *params)
{
	CHECK_PARAMS(8);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		vehicle->params.engine = static_cast<int8_t>(params[2]);
		vehicle->params.lights = static_cast<int8_t>(params[3]);
		vehicle->params.alarm = static_cast<int8_t>(params[4]);
		vehicle->params.doors = static_cast<int8_t>(params[5]);
		vehicle->params.bonnet = static_cast<int8_t>(params[6]);
		vehicle->params.boot = static_cast<int8_t>(params[7]);
		vehicle->params.objective = static_cast<int8_t>(params[8]);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setParams(vehicle->params);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleParamsEx(AMX *amx, cell *params)
{
	CHECK_PARAMS(8);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		Utility::storeIntegerInNative(amx, params[2], vehicle->params.engine);
		Utility::storeIntegerInNative(amx, params[3], vehicle->params.lights);
		Utility::storeIntegerInNative(amx, params[4], vehicle->params.alarm);
		Utility::storeIntegerInNative(amx, params[5], vehicle->params.doors);
		Utility::storeIntegerInNative(amx, params[6], vehicle->params.bonnet);
		Utility::storeIntegerInNative(amx, params[7], vehicle->params.boot);
		Utility::storeIntegerInNative(amx, params[8], vehicle->params.objective);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::SetDynamicVehicleParamsCarDoors(AMX *amx, cell *params)
{
	CHECK_PARAMS(5);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		vehicle->params.doorDriver = static_cast<int8_t>(params[2]);
		vehicle->params.doorPassenger = static_cast<int8_t>(params[3]);
		vehicle->params.doorBackLeft = static_cast<int8_t>(params[4]);
		vehicle->params.doorBackRight = static_cast<int8_t>(params[5]);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setParams(vehicle->params);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleParamsCarDoors(AMX *amx, cell *params)
{
	CHECK_PARAMS(5);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		Utility::storeIntegerInNative(amx, params[2], vehicle->params.doorDriver);
		Utility::storeIntegerInNative(amx, params[3], vehicle->params.doorPassenger);
		Utility::storeIntegerInNative(amx, params[4], vehicle->params.doorBackLeft);
		Utility::storeIntegerInNative(amx, params[5], vehicle->params.doorBackRight);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::SetDynamicVehicleParamsCarWindows(AMX *amx, cell *params)
{
	CHECK_PARAMS(5);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		vehicle->params.windowDriver = static_cast<int8_t>(params[2]);
		vehicle->params.windowPassenger = static_cast<int8_t>(params[3]);
		vehicle->params.windowBackLeft = static_cast<int8_t>(params[4]);
		vehicle->params.windowBackRight = static_cast<int8_t>(params[5]);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setParams(vehicle->params);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleParamsCarWindows(AMX *amx, cell *params)
{
	CHECK_PARAMS(5);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		VehicleState::refresh(vehicle);
		Utility::storeIntegerInNative(amx, params[2], vehicle->params.windowDriver);
		Utility::storeIntegerInNative(amx, params[3], vehicle->params.windowPassenger);
		Utility::storeIntegerInNative(amx, params[4], vehicle->params.windowBackLeft);
		Utility::storeIntegerInNative(amx, params[5], vehicle->params.windowBackRight);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleVirtualWorld(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		return static_cast<cell>(VehicleState::getWorld(vehicle));
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::SetDynamicVehicleVirtualWorld(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		int world = static_cast<int>(params[2]);
		vehicle->worlds.clear();
		vehicle->worlds.insert(world < 0 ? 0 : world);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setVirtualWorld(VehicleState::getWorld(vehicle));
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::LinkDynamicVehicleToInterior(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		int interior = static_cast<int>(params[2]);
		vehicle->interiors.clear();
		Utility::addToContainer(vehicle->interiors, interior);
		vehicle->interior = interior < 0 ? 0 : interior;
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setInterior(vehicle->interior);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleInterior(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		return static_cast<cell>(vehicle->interior);
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::SetDynamicVehicleToRespawn(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			// Fires onVehicleSpawn, which copies the respawned state back into the item.
			internal->respawn();
		}
		else
		{
			VehicleState::resetToSpawn(vehicle);
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleRespawnDelay(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		return static_cast<cell>(vehicle->respawnDelay);
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::SetDynamicVehicleRespawnDelay(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		vehicle->respawnDelay = static_cast<int>(params[2]);
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal)
		{
			internal->setRespawnDelay(Seconds(vehicle->respawnDelay));
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleModel(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		return static_cast<cell>(vehicle->modelId);
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::IsDynamicVehicleOccupied(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		IVehicle *internal = VehicleState::getInternal(vehicle);
		if (internal && (internal->getDriver() || !internal->getPassengers().empty()))
		{
			return 1;
		}
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::IsDynamicVehicleDead(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle vehicle = findVehicle(params[1]);
	if (vehicle)
	{
		return vehicle->dead ? 1 : 0;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::AttachTrailerToDynamicVehicle(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	Item::SharedVehicle trailer = findVehicle(params[1]);
	Item::SharedVehicle cab = findVehicle(params[2]);
	if (!trailer || !cab || trailer == cab)
	{
		return 0;
	}
	setLink(cab->vehicleId, trailer->vehicleId);
	VehicleState::applyTrailerLinks(cab);
	return 1;
}

cell AMX_NATIVE_CALL Natives::DetachTrailerFromDynamicVehicle(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle cab = findVehicle(params[1]);
	if (!cab)
	{
		return 0;
	}
	setLink(cab->vehicleId, INVALID_STREAMER_ID);
	IVehicle *internal = VehicleState::getInternal(cab);
	if (internal && internal->getTrailer())
	{
		internal->detachTrailer();
	}
	return 1;
}

cell AMX_NATIVE_CALL Natives::GetDynamicVehicleTrailer(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	Item::SharedVehicle cab = findVehicle(params[1]);
	if (cab)
	{
		return static_cast<cell>(cab->trailer);
	}
	return INVALID_STREAMER_ID;
}
