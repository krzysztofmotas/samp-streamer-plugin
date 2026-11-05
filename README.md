# open.mp Streamer with Dynamic Vehicles
[![Build Status](https://github.com/bloxer09/samp-streamer-plugin/actions/workflows/build.yml/badge.svg)](https://github.com/bloxer09/samp-streamer-plugin/actions/workflows/build.yml)

This plugin streams objects, pickups, checkpoints, race checkpoints, map icons, 3D text labels, actors, and vehicles at user-defined server ticks. Basic area detection is also included. Because it is written entirely in C++, much of the overhead from PAWN is avoided. This streamer, as a result, is quite a bit faster than any other implementation currently available in PAWN.

This is a fork of [krzysztofmotas/samp-streamer-plugin](https://github.com/krzysztofmotas/samp-streamer-plugin), the open.mp component port of [samp-incognito/samp-streamer-plugin](https://github.com/samp-incognito/samp-streamer-plugin). It adds **dynamic vehicles**, so a server can place far more vehicles than the 2000-slot vehicle pool allows.

## Documentation

Documentation for everything except vehicles can be found on the upstream [wiki](https://github.com/samp-incognito/samp-streamer-plugin/wiki).

## Dynamic vehicles

```pawn
#include <open.mp>
#include <streamer>

public OnGameModeInit()
{
	// Thousands of these are fine: only the ones near players become real vehicles.
	new STREAMER_TAG_VEHICLE:vehicle = CreateDynamicVehicle(562, 1700.0, -2650.0, 13.6, 0.0, 1, 1, 120);
	AddDynamicVehicleComponent(vehicle, 1010);
	SetDynamicVehicleNumberPlate(vehicle, "STREAM");
	return 1;
}

public OnVehicleDeath(vehicleid, killerid)
{
	// Callbacks receive the real vehicle ID, which changes every time the vehicle streams in.
	new STREAMER_TAG_VEHICLE:dynamic = GetDynamicVehicleFromInternal(vehicleid);
	if (dynamic != STREAMER_TAG_VEHICLE:INVALID_STREAMER_ID)
	{
		printf("dynamic vehicle %d died", _:dynamic);
	}
	return 1;
}
```

### How it works

- A dynamic vehicle has one real vehicle at most, created when any player is within its stream distance (default `STREAMER_VEHICLE_SD`, 300.0) and released when none are. `Streamer_SetVisibleItems(STREAMER_TYPE_VEHICLE, n)` caps how many exist at once (default 1000). Static `CreateVehicle` vehicles share the same 2000-slot pool.
- State changed on the real vehicle, by your script or by players (mod shops, damage, driving it somewhere), is copied back when it streams out and applied again when it streams in: position, angle, health, damage, colours, paintjob, components, number plate, params, doors and windows, world, interior, respawn delay and trailer link.
- A vehicle with a driver or passenger is never streamed out. A cab and its trailer stream in and out together.
- Callbacks such as `OnVehicleDeath` and `OnPlayerEnterVehicle` still receive the real vehicle ID. Use `GetDynamicVehicleFromInternal(vehicleid)` (or `Streamer_GetItemStreamerID`) to get the dynamic ID, and `GetDynamicVehicleInternalID` for the reverse.
- A vehicle that dies is respawned by open.mp as usual while players are near. If everyone leaves before that, it comes back at its spawn point next time.
- Each dynamic vehicle lives in exactly one virtual world. `interiorid = -1` means not linked to an interior.

### Natives

| Group | Natives |
| --- | --- |
| Lifecycle | `CreateDynamicVehicle`, `CreateDynamicVehicleEx`, `DestroyDynamicVehicle`, `IsValidDynamicVehicle`, `IsDynamicVehicleStreamedIn` |
| IDs and players | `GetDynamicVehicleInternalID`, `GetDynamicVehicleFromInternal`, `GetPlayerDynamicVehicle`, `PutPlayerInDynamicVehicle` |
| Position and health | `Get/SetDynamicVehiclePos`, `Get/SetDynamicVehicleZAngle`, `Get/SetDynamicVehicleHealth`, `GetDynamicVehicleDamageStatus`, `UpdateDynamicVehicleDamageStatus`, `RepairDynamicVehicle` |
| Looks | `ChangeDynamicVehicleColours`, `GetDynamicVehicleColours`, `ChangeDynamicVehiclePaintjob`, `GetDynamicVehiclePaintjob`, `Add/RemoveDynamicVehicleComponent`, `GetDynamicVehicleComponentInSlot`, `Set/GetDynamicVehicleNumberPlate` |
| Params | `Set/GetDynamicVehicleParamsEx`, `Set/GetDynamicVehicleParamsCarDoors`, `Set/GetDynamicVehicleParamsCarWindows` |
| World and respawn | `Get/SetDynamicVehicleVirtualWorld`, `LinkDynamicVehicleToInterior`, `GetDynamicVehicleInterior`, `SetDynamicVehicleToRespawn`, `Get/SetDynamicVehicleRespawnDelay`, `GetDynamicVehicleModel`, `IsDynamicVehicleOccupied`, `IsDynamicVehicleDead` |
| Trailers | `AttachTrailerToDynamicVehicle`, `DetachTrailerFromDynamicVehicle`, `GetDynamicVehicleTrailer` |
| Callbacks | `OnDynamicVehicleStreamIn`, `OnDynamicVehicleStreamOut`, and `Streamer_OnItemStreamIn/Out` once `Streamer_ToggleItemCallbacks` is on |

The generic `Streamer_*` natives (counting, destroying, nearby and visible items, item position, static/area/player toggles, array data) accept `STREAMER_TYPE_VEHICLE`.

### Known limitations

- When the visible cap is full, vehicles already streamed in are not swapped out for closer ones, so a new vehicle next to a player can stay unstreamed until others stream out.
- `Streamer_GetFloatData`/`Streamer_GetIntData` do not support vehicles yet. Use the `GetDynamicVehicle*` natives.
- `Streamer_Update` does not force vehicles in immediately; they follow the global streamer tick.

## Building

```bash
cmake -B build -A Win32
```

```bash
cmake --build build --config Release
```

The component is written to `build/bin/Release/streamer.dll`. Copy it into your open.mp server's `components/` folder and use the `streamer.inc` from this repository.

## Testing

[tests/dynamic-vehicles/dynamic-vehicles.pwn](tests/dynamic-vehicles/dynamic-vehicles.pwn) is a gamemode that creates 4000 dynamic vehicles in parking grids at the four airports. Compile it with the open.mp `qawno` compiler, run it with the component installed, and use `/dvcount`, `/lsa`, `/lva`, `/sfa`, `/vm`, `/dvjet`, `/dvspawn`, `/dvcheck`, `/dvgoto`, `/dvtrailer` and `/dvrespawn` in game.
