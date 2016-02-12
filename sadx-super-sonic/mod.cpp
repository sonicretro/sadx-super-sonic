#include "stdafx.h"
#include <SADXModLoader.h>

#define EXPORT __declspec(dllexport)

DataPointer(NJS_TEXLIST, SUPERSONIC_TEXLIST, 0x0142272C);
DataPointer(bool, SuperSonicFlag, 0x03C55D45);
DataPointer(int, CurrentSong, 0x00912698);
DataPointer(int, LastSong, 0x0091269C);
DataPointer(Bool, Music_Enabled, 0x0091268C);

FunctionPointer(void, ForcePlayerAction, (Uint8 playerNum, Uint8 action), 0x00441260);

static int ring_timer = 0;
static int super_count = 0;	// Dirty hack for multitap mod compatibility
static Uint8 last_action[8] = {};

void __cdecl Sonic_SuperPhysics_Delete(ObjectMaster* _this)
{
	char index = *(char*)_this->UnknownB_ptr;
	CharObj2* data2 = CharObj2Ptrs[index];

	if (data2 != nullptr)
		data2->PhysicsData = PhysicsArray[Characters_Sonic];
}

void __cdecl SuperSonicManager_Main(ObjectMaster* _this)
{
	if (super_count < 1)
	{
		DeleteObject_(_this);
		return;
	}

	// HACK: Result screen disables P1 control. There's probably a nicer way to do this, we just need to find it.
	if (IsControllerEnabled(0))
	{
		if (Music_Enabled && CurrentSong != 86)
			CurrentSong = 86;

		++ring_timer %= 60;

		if (!ring_timer)
			AddRings(-1);
	}
}
void __cdecl SuperSonicManager_Delete(ObjectMaster* _this)
{
	super_count = 0;
	ring_timer = 0;

	if (Music_Enabled)
		CurrentSong = LastSong;
}
void SuperSonicManager_Load()
{
	ObjectMaster* obj = LoadObject(0, 2, SuperSonicManager_Main);
	if (obj)
		obj->DeleteSub = SuperSonicManager_Delete;
}

int __stdcall SuperWaterCheck_C(CharObj1* data1, CharObj2* data2)
{
	return (int)(data1->CharID == Characters_Sonic && (data2->Upgrades & Upgrades_SuperSonic) != 0);
}

void* jump_to;
void __declspec(naked) SuperWaterCheck()
{
	__asm
	{
		jnz dothing

		mov jump_to, 004497B6h
		jmp jump_to

	dothing:
		// Save whatever's in EAX
		push eax

		push [esp+6a8h+4h+0ch]	// CharObj2
		push ebx				// CharObj1
		call SuperWaterCheck_C

		test eax, eax

		jnz is_true

		// Restore EAX
		pop eax

		mov jump_to, 004497B6h
		jmp jump_to

	is_true:
		// Restore EAX
		pop eax

		mov jump_to, 004496E7h
		jmp jump_to
	}
}

static const PVMEntry SuperSonicPVM = { "SUPERSONIC", &SUPERSONIC_TEXLIST };

extern "C"
{
	void EXPORT Init(const char* path, HelperFunctions* helper)
	{
		helper->RegisterCharacterPVM(Characters_Sonic, SuperSonicPVM);
		WriteData((void**)0x004943C2, (void*)Sonic_SuperPhysics_Delete);
		WriteJump((void*)0x004496E1, SuperWaterCheck);
	}

	void EXPORT OnFrame()
	{
		if (GameState != 15)
			return;

#ifndef _DEBUG
		if (!GetEventFlag(EventFlags_SuperSonicAdventureComplete))
			return;
#endif

		for (Uint8 i = 0; i < 8; i++)
		{
			CharObj1* data1 = CharObj1Ptrs[i];
			CharObj2* data2 = CharObj2Ptrs[i];

			if (data1 == nullptr || data1->CharID != Characters_Sonic)
				continue;

			bool isSuper = (data2->Upgrades & Upgrades_SuperSonic) != 0;
			bool toggle = (Controllers[i].PressedButtons & Buttons_B) != 0;

			if (!isSuper)
			{
				if (toggle && last_action[i] == 8 && data1->Action == 12 && Rings >= 50)
				{
					// Transform into Super Sonic
					ForcePlayerAction(i, 46);
					data2->Upgrades |= Upgrades_SuperSonic;
					PlayVoice(396);

					if (!super_count++)
						SuperSonicManager_Load();
				}
			}
			else if (toggle && last_action[i] == 82 && data1->Action == 78 || !Rings)
			{
				// Change back to normal Sonic
				ForcePlayerAction(i, 47);
				data2->Upgrades &= ~Upgrades_SuperSonic;
				--super_count;
			}

			last_action[i] = data1->Action;
		}

		SuperSonicFlag = super_count > 0;
	}

	EXPORT ModInfo SADXModInfo = { ModLoaderVer };
}
