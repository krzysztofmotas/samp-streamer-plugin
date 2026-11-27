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

#include "vehicle-state.h"
#include "core.h"

#include "Server/Components/Vehicles/vehicle_components.hpp"

IVehiclesComponent *VehicleState::getComponent()
{
	return ompgdk::GDKManager::Get()->GetVehicles();
}

IVehicle *VehicleState::getInternal(int vehicleId)
{
	std::unordered_map<int, int>::iterator i = core->getData()->internalVehicles.find(vehicleId);
	if (i != core->getData()->internalVehicles.end())
	{
		IVehiclesComponent *component = getComponent();
		if (component)
		{
			return component->get(i->second);
		}
	}
	return nullptr;
}

IVehicle *VehicleState::getInternal(const Item::SharedVehicle &vehicle)
{
	return getInternal(vehicle->vehicleId);
}

int VehicleState::getStreamerId(int internalId)
{
	std::unordered_map<int, int>::iterator r = core->getData()->internalVehiclesReverse.find(internalId);
	if (r != core->getData()->internalVehiclesReverse.end())
	{
		return r->second;
	}
	return INVALID_STREAMER_ID;
}

int VehicleState::getWorld(const Item::SharedVehicle &vehicle)
{
	if (vehicle->worlds.empty())
	{
		return 0;
	}
	return *vehicle->worlds.begin();
}

int VehicleState::create(const Item::SharedVehicle &vehicle)
{
	IVehiclesComponent *component = getComponent();
	if (!component)
	{
		return INVALID_VEHICLE_ID;
	}
	VehicleSpawnData spawnData;
	spawnData.respawnDelay = Seconds(vehicle->respawnDelay);
	spawnData.modelID = vehicle->modelId;
	spawnData.position = Vector3(vehicle->spawnPosition[0], vehicle->spawnPosition[1], vehicle->spawnPosition[2]);
	spawnData.zRotation = vehicle->spawnAngle;
	spawnData.colour1 = vehicle->color1;
	spawnData.colour2 = vehicle->color2;
	spawnData.siren = vehicle->siren;
	spawnData.interior = vehicle->interior;
	IVehicle *internal = component->create(spawnData);
	if (!internal)
	{
		return INVALID_VEHICLE_ID;
	}
	int internalId = internal->getID();
	core->getData()->internalVehicles[vehicle->vehicleId] = internalId;
	core->getData()->internalVehiclesReverse[internalId] = vehicle->vehicleId;
	apply(vehicle, *internal);
	return internalId;
}

void VehicleState::destroy(const Item::SharedVehicle &vehicle, bool snapshotState)
{
	std::unordered_map<int, int>::iterator i = core->getData()->internalVehicles.find(vehicle->vehicleId);
	if (i == core->getData()->internalVehicles.end())
	{
		return;
	}
	IVehiclesComponent *component = getComponent();
	if (component)
	{
		IVehicle *internal = component->get(i->second);
		if (internal)
		{
			if (snapshotState)
			{
				snapshot(vehicle, *internal);
			}
			component->release(i->second);
		}
	}
	core->getData()->internalVehiclesReverse.erase(i->second);
	core->getData()->internalVehicles.erase(i);
}

void VehicleState::snapshot(const Item::SharedVehicle &vehicle, IVehicle &internal)
{
	Vector3 position = internal.getPosition();
	vehicle->position = Eigen::Vector3f(position.x, position.y, position.z);
	vehicle->angle = internal.getZAngle();
	vehicle->health = internal.getHealth();
	internal.getDamageStatus(vehicle->damagePanels, vehicle->damageDoors, vehicle->damageLights, vehicle->damageTyres);
	Pair<int, int> colour = internal.getColour();
	vehicle->color1 = colour.first;
	vehicle->color2 = colour.second;
	vehicle->paintjob = internal.getPaintJob();
	for (int slot = 0; slot < MAX_VEHICLE_COMPONENT_SLOT; ++slot)
	{
		vehicle->components[slot] = internal.getComponentInSlot(slot);
	}
	vehicle->plate = std::string(internal.getPlate().data(), internal.getPlate().size());
	vehicle->params = internal.getParams();
	vehicle->interior = internal.getInterior();
	vehicle->respawnDelay = static_cast<int>(internal.getRespawnDelay().count());
}

IVehicle *VehicleState::refresh(const Item::SharedVehicle &vehicle)
{
	IVehicle *internal = getInternal(vehicle);
	if (internal)
	{
		snapshot(vehicle, *internal);
	}
	return internal;
}

void VehicleState::apply(const Item::SharedVehicle &vehicle, IVehicle &internal)
{
	internal.setVirtualWorld(getWorld(vehicle));
	internal.setInterior(vehicle->interior);
	if (vehicle->position != vehicle->spawnPosition || vehicle->angle != vehicle->spawnAngle)
	{
		internal.setPosition(Vector3(vehicle->position[0], vehicle->position[1], vehicle->position[2]));
		internal.setZAngle(vehicle->angle);
	}
	internal.setColour(vehicle->color1, vehicle->color2);
	internal.setHealth(vehicle->health);
	internal.setDamageStatus(vehicle->damagePanels, vehicle->damageDoors, static_cast<uint8_t>(vehicle->damageLights), static_cast<uint8_t>(vehicle->damageTyres));
	for (int slot = 0; slot < MAX_VEHICLE_COMPONENT_SLOT; ++slot)
	{
		if (vehicle->components[slot] > 0)
		{
			internal.addComponent(vehicle->components[slot]);
		}
	}
	if (vehicle->paintjob >= 0 && vehicle->paintjob <= 2)
	{
		internal.setPaintJob(vehicle->paintjob);
	}
	if (!vehicle->plate.empty())
	{
		internal.setPlate(vehicle->plate);
	}
	VehicleParams params = vehicle->params;
	if (params.isSet())
	{
		internal.setParams(params);
	}
}

void VehicleState::applyTrailerLinks(const Item::SharedVehicle &vehicle)
{
	if (vehicle->trailer != INVALID_STREAMER_ID)
	{
		IVehicle *cab = getInternal(vehicle);
		IVehicle *trailer = getInternal(vehicle->trailer);
		if (cab && trailer && cab->getTrailer() != trailer)
		{
			cab->attachTrailer(*trailer);
		}
	}
	if (vehicle->cab != INVALID_STREAMER_ID)
	{
		std::unordered_map<int, Item::SharedVehicle>::iterator c = core->getData()->vehicles.find(vehicle->cab);
		if (c != core->getData()->vehicles.end() && c->second->trailer == vehicle->vehicleId)
		{
			IVehicle *cab = getInternal(c->second);
			IVehicle *trailer = getInternal(vehicle);
			if (cab && trailer && cab->getTrailer() != trailer)
			{
				cab->attachTrailer(*trailer);
			}
		}
	}
}

bool VehicleState::isPinned(const Item::SharedVehicle &vehicle, IVehicle &internal)
{
	if (internal.getDriver() || !internal.getPassengers().empty())
	{
		return true;
	}
	if (vehicle->forcedUntil > std::chrono::steady_clock::now())
	{
		return true;
	}
	return false;
}

void VehicleState::syncPositions()
{
	IVehiclesComponent *component = getComponent();
	if (!component)
	{
		return;
	}
	for (std::unordered_map<int, int>::iterator i = core->getData()->internalVehicles.begin(); i != core->getData()->internalVehicles.end(); ++i)
	{
		std::unordered_map<int, Item::SharedVehicle>::iterator v = core->getData()->vehicles.find(i->first);
		IVehicle *internal = component->get(i->second);
		if (v == core->getData()->vehicles.end() || !internal)
		{
			continue;
		}
		Vector3 position = internal->getPosition();
		Eigen::Vector3f current(position.x, position.y, position.z);
		if ((current - v->second->position).squaredNorm() > 1.0f)
		{
			v->second->position = current;
			core->getGrid()->removeVehicle(v->second, true);
		}
		IVehicle *trailer = internal->getTrailer();
		int trailerId = trailer ? getStreamerId(trailer->getID()) : INVALID_STREAMER_ID;
		if (trailerId == INVALID_STREAMER_ID && !getInternal(v->second->trailer))
		{
			// The stored trailer is not streamed in yet, so the link is still pending, not detached.
			continue;
		}
		if (trailerId != v->second->trailer)
		{
			std::unordered_map<int, Item::SharedVehicle>::iterator o = core->getData()->vehicles.find(v->second->trailer);
			if (o != core->getData()->vehicles.end() && o->second->cab == v->first)
			{
				o->second->cab = INVALID_STREAMER_ID;
			}
			v->second->trailer = trailerId;
			std::unordered_map<int, Item::SharedVehicle>::iterator n = core->getData()->vehicles.find(trailerId);
			if (n != core->getData()->vehicles.end())
			{
				n->second->cab = v->first;
			}
		}
	}
}

void VehicleState::resetToSpawn(const Item::SharedVehicle &vehicle)
{
	vehicle->position = vehicle->spawnPosition;
	vehicle->angle = vehicle->spawnAngle;
	vehicle->health = 1000.0f;
	vehicle->damagePanels = 0;
	vehicle->damageDoors = 0;
	vehicle->damageLights = 0;
	vehicle->damageTyres = 0;
	vehicle->dead = false;
	std::unordered_map<int, Item::SharedVehicle>::iterator t = core->getData()->vehicles.find(vehicle->trailer);
	if (t != core->getData()->vehicles.end() && t->second->cab == vehicle->vehicleId)
	{
		t->second->cab = INVALID_STREAMER_ID;
	}
	vehicle->trailer = INVALID_STREAMER_ID;
	std::unordered_map<int, Item::SharedVehicle>::iterator c = core->getData()->vehicles.find(vehicle->cab);
	if (c != core->getData()->vehicles.end() && c->second->trailer == vehicle->vehicleId)
	{
		c->second->trailer = INVALID_STREAMER_ID;
	}
	vehicle->cab = INVALID_STREAMER_ID;
	core->getGrid()->removeVehicle(vehicle, true);
}
