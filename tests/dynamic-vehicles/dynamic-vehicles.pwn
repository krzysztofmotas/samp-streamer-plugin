// In-game test gamemode for dynamic vehicle streaming.
// Creates 4000 dynamic vehicles, twice what the 2000-slot vehicle pool can hold, in parking grids at
// the four airports. Fly between them and watch /dvcount: the real pool only holds the ones near you.

#include <open.mp>
#include <streamer>

#define GRID_COLUMNS (40)
#define GRID_ROWS (25)
#define GRID_SPACING (6.0)

enum E_GRID
{
	Float:E_GRID_X,
	Float:E_GRID_Y,
	Float:E_GRID_Z,
	E_GRID_NAME[16]
}

// South-west corner of each grid, on flat airport ground.
static const gGrids[][E_GRID] =
{
	{ 1700.0, -2650.0, 13.6, "lsa" },
	{ 1330.0, 1250.0, 10.9, "lva" },
	{ -1550.0, -350.0, 14.2, "sfa" },
	{ 230.0, 2420.0, 16.5, "vm" }
};

// Ordinary cars and bikes only: no trains, trailers, aircraft or RC models.
static const gModels[] =
{
	400, 401, 402, 404, 405, 410, 411, 412, 415, 419, 421, 426, 429, 436, 439, 445, 451, 458, 461, 462,
	466, 467, 474, 475, 477, 479, 480, 489, 491, 492, 496, 500, 506, 507, 516, 517, 518, 521, 526, 527,
	529, 533, 534, 535, 536, 540, 541, 542, 545, 546, 547, 549, 550, 551, 555, 558, 559, 560, 561, 562,
	565, 566, 567, 575, 576, 580, 581, 585, 587, 589, 600, 602, 603
};

new STREAMER_TAG_VEHICLE:gLastVehicle = STREAMER_TAG_VEHICLE:INVALID_STREAMER_ID;

forward ReportCounts();

main()
{
}

CountRealVehicles()
{
	new real = 0;
	for (new v = 1; v < MAX_VEHICLES; v++)
	{
		if (IsValidVehicle(v))
		{
			real++;
		}
	}
	return real;
}

public OnGameModeInit()
{
	SetGameModeText("dynamic vehicles");
	AddPlayerClass(0, gGrids[0][E_GRID_X] - 10.0, gGrids[0][E_GRID_Y], gGrids[0][E_GRID_Z], 270.0);
	for (new g = 0; g < sizeof gGrids; g++)
	{
		for (new c = 0; c < GRID_COLUMNS; c++)
		{
			for (new r = 0; r < GRID_ROWS; r++)
			{
				CreateDynamicVehicle(gModels[random(sizeof gModels)], gGrids[g][E_GRID_X] + c * GRID_SPACING, gGrids[g][E_GRID_Y] + r * GRID_SPACING, gGrids[g][E_GRID_Z], 0.0, -1, -1, 120);
			}
		}
	}
	printf("[dynamic-vehicles] created %d dynamic vehicles, real pool %d", Streamer_CountItems(STREAMER_TYPE_VEHICLE, 1), CountRealVehicles());
	SetTimer("ReportCounts", 10000, true);
	return 1;
}

public ReportCounts()
{
	printf("[dynamic-vehicles] dynamic %d, streamed in %d, real pool %d", Streamer_CountItems(STREAMER_TYPE_VEHICLE, 1), Streamer_CountVisibleItems(INVALID_PLAYER_ID, STREAMER_TYPE_VEHICLE, 1), CountRealVehicles());
	return 1;
}

public OnPlayerRequestClass(playerid, classid)
{
	SetPlayerPos(playerid, gGrids[0][E_GRID_X] - 10.0, gGrids[0][E_GRID_Y], gGrids[0][E_GRID_Z]);
	SetPlayerCameraPos(playerid, gGrids[0][E_GRID_X] - 30.0, gGrids[0][E_GRID_Y] - 20.0, gGrids[0][E_GRID_Z] + 15.0);
	SetPlayerCameraLookAt(playerid, gGrids[0][E_GRID_X] + 60.0, gGrids[0][E_GRID_Y] + 60.0, gGrids[0][E_GRID_Z]);
	return 1;
}

public OnPlayerSpawn(playerid)
{
	SendClientMessage(playerid, -1, "Commands: /dvcount /lsa /lva /sfa /vm /dvjet /dvspawn /dvcheck /dvgoto /dvtrailer /dvrespawn");
	return 1;
}

public OnDynamicVehicleStreamIn(STREAMER_TAG_VEHICLE:vehicleid, forplayerid)
{
	return 1;
}

public OnVehicleDeath(vehicleid, killerid)
{
	new STREAMER_TAG_VEHICLE:dyn = GetDynamicVehicleFromInternal(vehicleid);
	if (dyn != STREAMER_TAG_VEHICLE:INVALID_STREAMER_ID)
	{
		printf("[dynamic-vehicles] dynamic vehicle %d died (dead flag %d)", _:dyn, IsDynamicVehicleDead(dyn));
	}
	return 1;
}

public OnPlayerCommandText(playerid, cmdtext[])
{
	new msg[144];
	for (new g = 0; g < sizeof gGrids; g++)
	{
		format(msg, sizeof msg, "/%s", gGrids[g][E_GRID_NAME]);
		if (!strcmp(cmdtext, msg, true))
		{
			SetPlayerPos(playerid, gGrids[g][E_GRID_X] - 10.0, gGrids[g][E_GRID_Y], gGrids[g][E_GRID_Z] + 1.0);
			return 1;
		}
	}
	if (!strcmp(cmdtext, "/dvcount", true))
	{
		format(msg, sizeof msg, "dynamic: %d, streamed in: %d, real pool: %d", Streamer_CountItems(STREAMER_TYPE_VEHICLE, 1), Streamer_CountVisibleItems(playerid, STREAMER_TYPE_VEHICLE, 1), CountRealVehicles());
		SendClientMessage(playerid, -1, msg);
		return 1;
	}
	if (!strcmp(cmdtext, "/dvjet", true))
	{
		// A Hydra to fly between the airports with.
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		new STREAMER_TAG_VEHICLE:jet = CreateDynamicVehicle(520, x, y, z + 2.0, 0.0, -1, -1, -1, false, GetPlayerVirtualWorld(playerid));
		PutPlayerInDynamicVehicle(playerid, jet, 0);
		return 1;
	}
	if (!strcmp(cmdtext, "/dvspawn", true))
	{
		// A modded, damaged, locked Elegy in front of the player, to check state survives streaming.
		new Float:x, Float:y, Float:z, Float:a;
		GetPlayerPos(playerid, x, y, z);
		GetPlayerFacingAngle(playerid, a);
		gLastVehicle = CreateDynamicVehicle(562, x + 5.0 * floatsin(-a, degrees), y + 5.0 * floatcos(-a, degrees), z, a, 1, 1, 60, false, GetPlayerVirtualWorld(playerid));
		AddDynamicVehicleComponent(gLastVehicle, 1010); // nitro
		AddDynamicVehicleComponent(gLastVehicle, 1080); // wheels
		ChangeDynamicVehiclePaintjob(gLastVehicle, 1);
		SetDynamicVehicleNumberPlate(gLastVehicle, "STREAM");
		SetDynamicVehicleHealth(gLastVehicle, 450.0);
		UpdateDynamicVehicleDamageStatus(gLastVehicle, 0x11111, 0x0202, 0x5, 0x3);
		SetDynamicVehicleParamsEx(gLastVehicle, 0, 1, 0, 1, 0, 0, 0);
		format(msg, sizeof msg, "spawned dynamic vehicle %d - fly 1000m away, come back and /dvcheck", _:gLastVehicle);
		SendClientMessage(playerid, -1, msg);
		return 1;
	}
	if (!strcmp(cmdtext, "/dvcheck", true))
	{
		if (!IsValidDynamicVehicle(gLastVehicle)) return SendClientMessage(playerid, -1, "no test vehicle, use /dvspawn");
		new Float:health, panels, doors, lights, tires, plate[33], engine, vlights, alarm, vdoors, bonnet, boot, objective;
		GetDynamicVehicleHealth(gLastVehicle, health);
		GetDynamicVehicleDamageStatus(gLastVehicle, panels, doors, lights, tires);
		GetDynamicVehicleNumberPlate(gLastVehicle, plate);
		GetDynamicVehicleParamsEx(gLastVehicle, engine, vlights, alarm, vdoors, bonnet, boot, objective);
		format(msg, sizeof msg, "internal %d hp %.0f dmg %x/%x/%x/%x pj %d nitro %d wheels %d plate %s locked %d",
			GetDynamicVehicleInternalID(gLastVehicle), health, panels, doors, lights, tires, GetDynamicVehiclePaintjob(gLastVehicle),
			GetDynamicVehicleComponentInSlot(gLastVehicle, 5), GetDynamicVehicleComponentInSlot(gLastVehicle, 7), plate, vdoors);
		SendClientMessage(playerid, -1, msg);
		return 1;
	}
	if (!strcmp(cmdtext, "/dvgoto", true))
	{
		// Put the player into a far-away vehicle that is not streamed in yet.
		if (!IsValidDynamicVehicle(gLastVehicle)) return SendClientMessage(playerid, -1, "no test vehicle, use /dvspawn");
		PutPlayerInDynamicVehicle(playerid, gLastVehicle, 0);
		return 1;
	}
	if (!strcmp(cmdtext, "/dvtrailer", true))
	{
		// A Linerunner with a petrol trailer attached through the streamer.
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		new STREAMER_TAG_VEHICLE:cab = CreateDynamicVehicle(403, x + 8.0, y, z, 0.0, -1, -1, 60);
		new STREAMER_TAG_VEHICLE:trailer = CreateDynamicVehicle(584, x + 8.0, y - 10.0, z, 0.0, -1, -1, 60);
		AttachTrailerToDynamicVehicle(trailer, cab);
		format(msg, sizeof msg, "cab %d trailer %d linked %d", _:cab, _:trailer, GetDynamicVehicleTrailer(cab) == trailer);
		SendClientMessage(playerid, -1, msg);
		return 1;
	}
	if (!strcmp(cmdtext, "/dvrespawn", true))
	{
		if (!IsValidDynamicVehicle(gLastVehicle)) return SendClientMessage(playerid, -1, "no test vehicle, use /dvspawn");
		SetDynamicVehicleToRespawn(gLastVehicle);
		return 1;
	}
	return 0;
}
