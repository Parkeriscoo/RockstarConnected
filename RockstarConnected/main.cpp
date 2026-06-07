#include <xtl.h>
#include <ppcintrinsics.h>
#include "types.h"
#include "invoker.h"

// hashes
#define HASH_NETWORK_IS_CLOUD_AVAILABLE           0xC7FF5AFC
#define HASH_CLOUD_DID_REQUEST_SUCCEED            0x9B9AFFF1
#define HASH_NETWORK_ARE_SC_POLICIES_CURRENT      0x358D1D77
#define HASH_NETWORK_HAS_SC_ACCOUNT               0x66EC713F
#define HASH_NETWORK_HAS_VALID_ROS_CREDENTIALS    0x3FB40673
#define HASH_CLOUD_GET_AVAILABILITY_CHECK_RESULT  0x9262744C
#define HASH_NETWORK_HAVE_ROS_LEADERBOARD_PRIV    0xC6EA802E
#define HASH_NETWORK_HAVE_SCS_PRIVATE_MSG_PRIV    0x0CA1167F
#define HASH_NETWORK_HAVE_ROS_SC_PRIV             0x424C6E27

// array of all hashes to patch
DWORD g_NativeHashes[] = {
	HASH_NETWORK_IS_CLOUD_AVAILABLE,
	HASH_CLOUD_DID_REQUEST_SUCCEED,
	HASH_NETWORK_ARE_SC_POLICIES_CURRENT,
	HASH_NETWORK_HAS_SC_ACCOUNT,
	HASH_NETWORK_HAS_VALID_ROS_CREDENTIALS,
	HASH_CLOUD_GET_AVAILABILITY_CHECK_RESULT,
	HASH_NETWORK_HAVE_ROS_LEADERBOARD_PRIV,
	HASH_NETWORK_HAVE_SCS_PRIVATE_MSG_PRIV,
	HASH_NETWORK_HAVE_ROS_SC_PRIV,
};

#define NUM_NATIVES_TO_PATCH (sizeof(g_NativeHashes) / sizeof(DWORD))

// hook and return 1
void native_ret1_hook(NativeArg_s* args)
{
	args->ReturnValue[0] = 1;
}

// patching
DWORD* GetNativeFunctionPointer(unsigned int hash)
{
	Native_s* Natives = g_Natives[hash & 0xFF];
	while (Natives != 0)
	{
		for (unsigned int i = 0; i < Natives->NativeCount; i++)
		{
			if (Natives->NativeHashes[i] == hash)
			{
				return (DWORD*)&Natives->NativeFunctions[i];
			}
		}
		Natives = Natives->LastNativesTable;
	}
	return NULL;
}

void PatchCloudNatives()
{
	DWORD hookAddr = (DWORD)native_ret1_hook;
	
	for (int i = 0; i < NUM_NATIVES_TO_PATCH; i++)
	{
		DWORD* funcPtr = GetNativeFunctionPointer(g_NativeHashes[i]);
		if (funcPtr != NULL)
		{
			*funcPtr = hookAddr;
			__dcbst(0, funcPtr);
			__sync();
		}
	}
}

// init
DWORD WINAPI Initialize(LPVOID lpParam)
{
	// wait for game to load
	Sleep(30000);
	
	// patch natives
	PatchCloudNatives();
	
	return 0;
}

// ddl entry
BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
			CreateThread(NULL, 0, Initialize, NULL, 0, NULL);
			break;
			
		case DLL_PROCESS_DETACH:
			break;
	}
	
	return TRUE;
}