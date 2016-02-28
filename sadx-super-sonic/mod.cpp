#include "stdafx.h"
#include <SADXModLoader.h>

#define EXPORT __declspec(dllexport)

DataPointer(NJS_TEXLIST, SUPERSONIC_TEXLIST, 0x0142272C);
DataPointer(bool, SuperSonicFlag, 0x03C55D45);
DataPointer(int, CurrentSong, 0x00912698);
DataPointer(int, LastSong, 0x0091269C);
DataPointer(Bool, Music_Enabled, 0x0091268C);

FunctionPointer(void, ForcePlayerAction, (Uint8 playerNum, Uint8 action), 0x00441260);
FunctionPointer(int, _rand, (void), 0x006443BF);

static int ring_timer = 0;
static int super_count = 0;	// Dirty hack for multitap mod compatibility
static short last_level = 0;
static short last_act = 0;
static int LevelSong = 0;
static Uint8 last_action[8] = {};

static int clips[] = {
	402,
	508,
	874,
	1427,
	1461
};

static void SetMusic()
{
	if (!Music_Enabled)
		return;

	last_level = CurrentLevel;
	last_act = CurrentAct;

	LevelSong = LastSong;
	LastSong = CurrentSong = MusicIDs_ThemeOfSuperSonic;
}
static void RestoreMusic()
{
	if (!Music_Enabled)
		return;

	LastSong = CurrentSong = LevelSong;
}

static void __cdecl Sonic_SuperPhysics_Delete(ObjectMaster* _this)
{
	char index = *(char*)_this->UnknownB_ptr;
	CharObj2* data2 = CharObj2Ptrs[index];

	if (data2 != nullptr)
		data2->PhysicsData = PhysicsArray[Characters_Sonic];
}

static void __cdecl SuperSonicManager_Main(ObjectMaster* _this)
{
	if (super_count < 1)
	{
		DeleteObject_(_this);
		return;
	}

	if (CurrentSong != -1 && (CurrentLevel != last_level || CurrentAct != last_act))
		SetMusic();

	// HACK: Result screen disables P1 control. There's probably a nicer way to do this, we just need to find it.
	if (IsControllerEnabled(0))
	{
		++ring_timer %= 60;

		if (!ring_timer)
			AddRings(-1);
	}
}
static void __cdecl SuperSonicManager_Delete(ObjectMaster* _this)
{
	super_count = 0;
	ring_timer = 0;
	RestoreMusic();
}
static void SuperSonicManager_Load()
{
	ObjectMaster* obj = LoadObject(0, 2, SuperSonicManager_Main);
	if (obj)
		obj->DeleteSub = SuperSonicManager_Delete;

	SetMusic();
}

static int __stdcall SuperWaterCheck_C(CharObj1* data1, CharObj2* data2)
{
	return (int)(data1->CharID == Characters_Sonic && (data2->Upgrades & Upgrades_SuperSonic) != 0);
}

static const void* surface_solid = (void*)0x004496E7;
static const void* surface_water = (void*)0x004497B6;
static void __declspec(naked) SuperWaterCheck()
{
	__asm
	{
		// If Gamma, treat surface as solid
		jnz not_gamma
		jmp surface_solid

	not_gamma:
		// Save whatever's in EAX
		push eax

		push [esp + 6A8h + 4h + 0Ch]	// CharObj2
		push ebx						// CharObj1
		call SuperWaterCheck_C

		test eax, eax

		jnz is_true

		// Restore EAX
		pop eax

		jmp surface_water

	is_true:
		// Restore EAX
		pop eax
		jmp surface_solid
	}
}

static const PVMEntry SuperSonicPVM = { "SUPERSONIC", &SUPERSONIC_TEXLIST };

static inline bool IsStageBlacklisted()
{
	switch (CurrentLevel)
	{
		default:
			return false;

		case LevelIDs_Casinopolis:
			return (CurrentAct == 2 || CurrentAct == 3);

		case LevelIDs_SpeedHighway:
			return CurrentAct == 1;
	}
}

extern "C"
{
	EXPORT ModInfo SADXModInfo = { ModLoaderVer };

	void EXPORT Init(const char* path, HelperFunctions* helper)
	{
		helper->RegisterCharacterPVM(Characters_Sonic, SuperSonicPVM);
		WriteData((void**)0x004943C2, (void*)Sonic_SuperPhysics_Delete);
		WriteJump((void*)0x004496E1, SuperWaterCheck);

		LandTable* ec2mesh = (LandTable*)0x01039E9C;
		NJS_OBJECT* obj = ec2mesh->COLList[1].OBJECT;
		obj->ang[0] = 32768;
		obj->pos[1] = -3.0f;
		obj->pos[2] = -5850.0f;
	}

	void EXPORT OnFrame()
	{
		if (GameState != 15 || MetalSonicFlag)
			return;

#ifndef _DEBUG
		if (!GetEventFlag(EventFlags_SuperSonicAdventureComplete))
			return;
#endif

		bool isBlacklisted = IsStageBlacklisted();

		for (Uint8 i = 0; i < 8; i++)
		{
			CharObj1* data1 = CharObj1Ptrs[i];
			CharObj2* data2 = CharObj2Ptrs[i];

			if (data1 == nullptr || data1->CharID != Characters_Sonic)
				continue;

			bool isSuper = (data2->Upgrades & Upgrades_SuperSonic) != 0;
			bool toggle = (Controllers[i].PressedButtons & Buttons_B) != 0;
			bool action = !isSuper ? (last_action[i] == 8 && data1->Action == 12) : (last_action[i] == 82 && data1->Action == 78);

			if (!isSuper)
			{
#ifdef _DEBUG
				if (!Rings)
					Rings = 50;
#endif
				if (toggle && action)
				{
					if (isBlacklisted)
					{
						PlayVoice(clips[_rand() % LengthOfArray(clips)]);
					}
					else if (Rings >= 50)
					{
						// Transform into Super Sonic
						data1->Status &= ~Status_LightDash;
						ForcePlayerAction(i, 46);
						data2->Upgrades |= Upgrades_SuperSonic;
						PlayVoice(396);

						if (!super_count++)
							SuperSonicManager_Load();
					}
				}
			}
			else
			{
				// TODO: Consider storing the queued action in the case of NextAction 13, then re-applying
				// the stored queued action next frame to fix the spindashy things.
				bool detransform = data1->Status & Status_DoNextAction &&
					(data1->NextAction == 12 || data1->NextAction == 13 && CurrentLevel == LevelIDs_TwinklePark);

				if (isBlacklisted || detransform || action && toggle || !Rings)
				{
					if (detransform)
						PlayVoice(clips[_rand() % LengthOfArray(clips)]);

					// Change back to normal Sonic
					ForcePlayerAction(i, 47);
					data2->Upgrades &= ~Upgrades_SuperSonic;
					--super_count;
				}
			}

			last_action[i] = data1->Action;
		}

		SuperSonicFlag = super_count > 0;
	}
}
