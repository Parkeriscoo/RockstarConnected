#include <xtl.h>
#include <ppcintrinsics.h>
#include <stdio.h>
#include <string.h>
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

// online policies (RGOP) - joaat of the native names
#define HASH_ARE_ONLINE_POLICIES_UP_TO_DATE       0x850690FF // -> 1 (already accepted)
#define HASH_IS_ONLINE_POLICIES_MENU_ACTIVE       0x9D4934F4 // -> 0 (menu never active)
#define HASH_OPEN_ONLINE_POLICIES_MENU            0x19FCBBB2 // -> no-op (never open/download)

// compatibility pack (DLC) checks - values mirror what PC hard-codes (PC has no compat packs)
#define HASH_DLC_CHECK_CLOUD_DATA_CORRECT         0x881B1FDB // -> 1 (PC: always 1)
#define HASH_DLC_CHECK_COMPAT_PACK_CONFIGURATION  0xF79A97F5 // -> 1 (PC: always 1)
#define HASH_GET_EXTRACONTENT_CLOUD_RESULT        0xC2169164 // -> 0 (PC: always 0)
#define HASH_ARE_ANY_CCS_PENDING                  0x0AF83036 // -> 0 (no compat content pending)
#define HASH_GET_EVER_HAD_BAD_PACK_ORDER          0xF69B729C // -> 0 (pack order never bad)
#define HASH_HAS_CLOUD_REQUESTS_FINISHED          0x6087C10C // -> 1, *arg0 = 0 (PC behaviour)

// tutorial skip
#define HASH_PLAYER_ID                            0x8AEA886C // wrapped: runs original, then the tutorial-skip tick
#define HASH_STAT_SET_BOOL                        0x55D79DFB // BOOL STAT_SET_BOOL(Hash stat, BOOL value, BOOL save)
#define HASH_STAT_GET_BOOL                        0x28A3DD2B // BOOL STAT_GET_BOOL(Hash stat, BOOL* out, Any p2 = -1)

// stats that mark the GTA Online tutorial as finished (joaat of the stat names).
// MP0_ = character slot 1 (the temp character), MP1_ = character slot 2.
// to add more: hash the stat name with joaat and add it here.
DWORD g_TutorialStats[] = {
	0xC410D8EC, // MP0_NO_MORE_TUTORIALS
	0x791E04FA, // MP0_FM_INTRO_CUT_DONE
	0x0B19ABF2, // MP0_FM_INTRO_MISS_DONE
	0xFD7B482C, // MP1_NO_MORE_TUTORIALS
	0x91C5D62F, // MP1_FM_INTRO_CUT_DONE
	0xA7B3D64C, // MP1_FM_INTRO_MISS_DONE
	0x58862927, // MP0_FM_RACETUT_DONE
	0xE346B631, // MP1_FM_RACETUT_DONE
	0xF8A7FBD8,

	// intro only on purpose: the online glitch needs part of the tutorial to run.
	// other tutorial stats that exist in 1.12 (probe run 17:46), NOT skipped:
	//   0x0C9DAB8F MP0_FM_TRIGTUT_DONE        0xE077B8C5 MP1_FM_TRIGTUT_DONE
	//   0xF8A7FBD8 MP0_SHOOTINGRANGE_SEEN_TUT 0x3DF62E43 MP1_SHOOTINGRANGE_SEEN_TUT
	//   0x4EE04218 MPPLY_NO_MORE_TUTORIALS
};

// ---------------------------------------------------------------------------
// jobs (UGC) - switches for testing. Without the plugin jobs load from local
// data because the cloud is unavailable; forcing "cloud available/succeeded"
// makes scripts ask the dead cloud for jobs instead.
// ---------------------------------------------------------------------------
// log 16:56 showed: real cloud = 0; forcing it to 1 made the job script take the
// cloud path (offline off + UGC_GET_GET_BY_CATEGORY, which fails -> no jobs).
// with the real value the script uses the local job data like it does without the plugin.
// log 16:59 showed: with the real value (0) the script sets offline(1) and reads
// the 366 local jobs - but the "online glitch" needs the cloud to look available.
// so: cloud = 1, offline forced on, and the cloud job query faked as started +
// succeeded, so the script reads its results from the local job data.
#define FORCE_CLOUD_AVAILABLE        1 // NETWORK_IS_CLOUD_AVAILABLE -> 1        (0 = pass the real value through)
#define FORCE_CLOUD_REQUEST_SUCCESS  1 // CLOUD_DID_REQUEST_SUCCEED  -> 1        (0 = pass the real value through)
#define FORCE_UGC_OFFLINE            1 // UGC_SET_QUERY_DATA_FROM_OFFLINE(TRUE) always (0 = leave it to the game)
#define FAKE_UGC_QUERY               1 // UGC_GET_* job queries -> "started", UGC_DID_GET_SUCCEED -> 1 (needs FORCE_UGC_OFFLINE)
#define LOG_JOB_NATIVES              1 // log UGC / datafile / cloud calls to xenia.log

#define HASH_UGC_SET_QUERY_DATA_FROM_OFFLINE      0x213C6D36 // void (BOOL p0)
#define HASH_DATAFILE_LOAD_OFFLINE_UGC            0x660C468E // BOOL (char* filename)
#define HASH_UGC_REQUEST_CONTENT_DATA_FROM_PARAMS 0x38FC2EEB // int  (char* type, char* contentId, int, int, int)
#define HASH_UGC_REQUEST_CONTENT_DATA_FROM_INDEX  0x84315226 // int  (int, int)
#define HASH_UGC_QUERY_BY_CONTENT_ID              0xDED82A6E // BOOL (char* contentId, BOOL latest, char* type)
#define HASH_UGC_GET_GET_BY_CATEGORY              0x67E74842 // BOOL (Any, Any, Any, Any*, Any*)
#define HASH_UGC_HAS_GET_FINISHED                 0x50296140 // BOOL ()
#define HASH_UGC_DID_GET_SUCCEED                  0x3970B0DA // BOOL ()
#define HASH_UGC_GET_CONTENT_NUM                  0x2DE69817 // int  ()
#define HASH_UGC_GET_QUERY_RESULT                 0xCC2356E3 // int  ()
#define HASH_DATAFILE_HAS_LOADED_FILE_DATA        0x36FB8B3F // BOOL (int id)
#define HASH_DATAFILE_HAS_VALID_FILE_DATA         0x2A9411DA // BOOL (int id)
#define HASH_CLOUD_HAS_REQUEST_COMPLETED          0xBAF52DD8 // BOOL (int handle)
#define HASH_UGC_GET_MY_CONTENT                   0x4C2C0D1F // BOOL (Any, Any, Any*, Any*)
#define HASH_UGC_GET_BOOKMARKED_CONTENT           0x4609D596 // BOOL (Any, Any, Any*, Any*)
#define HASH_UGC_GET_FRIEND_CONTENT               0x9EFBD5D1 // BOOL (Any, Any, Any*, Any*)
#define HASH_UGC_GET_CREW_CONTENT                 0xA6D8B798 // BOOL (Any, Any, Any, Any*, Any*)

// array of all hashes to patch
DWORD g_NativeHashes[] = {
	HASH_NETWORK_ARE_SC_POLICIES_CURRENT,
	HASH_NETWORK_HAS_SC_ACCOUNT,
	HASH_NETWORK_HAS_VALID_ROS_CREDENTIALS,
	HASH_CLOUD_GET_AVAILABILITY_CHECK_RESULT,
	HASH_NETWORK_HAVE_ROS_LEADERBOARD_PRIV,
	HASH_NETWORK_HAVE_SCS_PRIVATE_MSG_PRIV,
	HASH_NETWORK_HAVE_ROS_SC_PRIV,
	HASH_ARE_ONLINE_POLICIES_UP_TO_DATE,
	HASH_DLC_CHECK_CLOUD_DATA_CORRECT,
	HASH_DLC_CHECK_COMPAT_PACK_CONFIGURATION,
};

// natives that must report "false"
DWORD g_NativeHashesRet0[] = {
	HASH_IS_ONLINE_POLICIES_MENU_ACTIVE,
	HASH_GET_EXTRACONTENT_CLOUD_RESULT,
	HASH_ARE_ANY_CCS_PENDING,
	HASH_GET_EVER_HAD_BAD_PACK_ORDER,
};

// natives that must do nothing
DWORD g_NativeHashesNop[] = {
	HASH_OPEN_ONLINE_POLICIES_MENU,
};

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

// hook and return 1
void native_ret1_hook(NativeArg_s* args)
{
	args->ReturnValue[0] = 1;
}

// hook and return 0
void native_ret0_hook(NativeArg_s* args)
{
	args->ReturnValue[0] = 0;
}

// hook that does nothing (for void natives)
void native_nop_hook(NativeArg_s* args)
{
}

// HAS_CLOUD_REQUESTS_FINISHED(BOOL* variable, Any unused)
// mirrors PC: sets *variable = 0 and always returns true, so the
// compatibility pack check never waits on the (dead) cloud and times out
void native_cloud_requests_finished_hook(NativeArg_s* args)
{
	int* outVar = (int*)args->ArgValues[0];
	if (outVar != NULL)
	{
		*outVar = 0;
	}
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

// kernel debug print (xboxkrnl export) - xenia writes it to xenia.log
extern "C" ULONG __cdecl DbgPrint(const char* format, ...);

void LogNative(const char* msg, DWORD hash)
{
	DbgPrint("[RockstarConnected] %s 0x%08X\n", msg, hash);
}

typedef void (*NativeHook_t)(NativeArg_s*);

// ---------------------------------------------------------------------------
// calling natives from our code
// ---------------------------------------------------------------------------

NativeHook_t GetNativeHandler(DWORD hash)
{
	DWORD* slot = GetNativeFunctionPointer(hash);
	if (slot == NULL || *slot == 0)
		return NULL;
	return (NativeHook_t)*slot;
}

// calls a native with up to N int-sized args, returns ReturnValue[0]
int CallNative(NativeHook_t handler, int* argv, unsigned int argc)
{
	int ret[3] = { 0, 0, 0 };

	NativeArg_s ctx;
	ctx.ReturnValue = ret;
	ctx.ArgCount    = argc;
	ctx.ArgValues   = argv;
	ctx.ReadCount   = 0;

	handler(&ctx);
	return ret[0];
}

// ---------------------------------------------------------------------------
// tutorial skip - runs on the script thread (from inside the PLAYER_ID hook)
// ---------------------------------------------------------------------------

#define TUTORIAL_STAT_COUNT ARRAY_COUNT(g_TutorialStats)

NativeHook_t g_OrigPlayerId = NULL;
NativeHook_t g_StatSetBool  = NULL;
NativeHook_t g_StatGetBool  = NULL;

BOOL  g_TutStatDone[TUTORIAL_STAT_COUNT];   // stat reads back as TRUE
BOOL  g_TutStatLogged[TUTORIAL_STAT_COUNT]; // already logged a set result
DWORD g_TutTick = 0;

void TutorialSkipTick()
{
	// PLAYER_ID is called many times per frame, only check every so often
	if ((++g_TutTick % 200) != 0)
		return;

	if (g_StatSetBool == NULL) g_StatSetBool = GetNativeHandler(HASH_STAT_SET_BOOL);
	if (g_StatGetBool == NULL) g_StatGetBool = GetNativeHandler(HASH_STAT_GET_BOOL);
	if (g_StatSetBool == NULL || g_StatGetBool == NULL)
		return;

	for (unsigned int i = 0; i < TUTORIAL_STAT_COUNT; i++)
	{
		// read it back: STAT_GET_BOOL(stat, &value, -1)
		int value = 0;
		int getArgs[3] = { (int)g_TutorialStats[i], (int)&value, -1 };
		int gotIt = CallNative(g_StatGetBool, getArgs, 3);

		if (gotIt && value)
		{
			if (!g_TutStatDone[i])
			{
				LogNative("tutorial stat is set", g_TutorialStats[i]);
				g_TutStatDone[i] = TRUE;
			}
			continue;
		}

		// not set (or stats reloaded and it was cleared) -> set it and save
		g_TutStatDone[i] = FALSE;
		int setArgs[3] = { (int)g_TutorialStats[i], 1, 1 };
		int ok = CallNative(g_StatSetBool, setArgs, 3);

		// log each outcome once per stat (bit 1 = logged success, bit 2 = logged failure)
		int bit = ok ? 1 : 2;
		if ((g_TutStatLogged[i] & bit) == 0)
		{
			LogNative(ok ? "set tutorial stat" : "FAILED to set tutorial stat (stats not loaded yet or stat missing)", g_TutorialStats[i]);
			g_TutStatLogged[i] |= bit;
		}
	}
}

// PLAYER_ID wrapper: original behaviour + tutorial skip
void native_player_id_hook(NativeArg_s* args)
{
	if (g_OrigPlayerId != NULL)
		g_OrigPlayerId(args);

	TutorialSkipTick();
}

// ---------------------------------------------------------------------------
// job (UGC) natives: call-through wrappers that log what the game really does
// ---------------------------------------------------------------------------

struct CallLog_s
{
	const char*	Name;
	const char*	Format;		// "<ret>:<args>"  i = int, s = string, v = void
	DWORD		LastSig;	// signature of the last logged call (args + result)
	int			Count;		// lines logged so far
};

#define MAX_LOG_LINES_PER_NATIVE 60

// reads a script string pointer without trusting it
const char* SafeStr(int p)
{
	DWORD a = (DWORD)p;
	if (a < 0x40000000 || a >= 0xC0000000)
		return "<null>";
	return (const char*)a;
}

void AppendStr(char* buf, int size, const char* text)
{
	int len = (int)strlen(buf);
	_snprintf(buf + len, size - len, "%s", text);
	buf[size - 1] = 0;
}

void LogCall(CallLog_s* log, NativeArg_s* a, BOOL hasResult)
{
#if LOG_JOB_NATIVES
	if (log->Count >= MAX_LOG_LINES_PER_NATIVE)
		return;

	char line[256];
	char part[96];
	line[0] = 0;
	DWORD sig = 2166136261u;

	// args
	const char* f = strchr(log->Format, ':');
	f = f ? f + 1 : "";
	for (int i = 0; f[i] != 0 && i < (int)a->ArgCount; i++)
	{
		int v = a->ArgValues[i];
		if (f[i] == 's')
		{
			const char* s = SafeStr(v);
			_snprintf(part, sizeof(part), "%s\"%.48s\"", i ? ", " : "", s);
			for (const char* c = s; *c && c - s < 48; c++) sig = (sig ^ (BYTE)*c) * 16777619u;
		}
		else
		{
			_snprintf(part, sizeof(part), "%s%d", i ? ", " : "", v);
			sig = (sig ^ (DWORD)v) * 16777619u;
		}
		part[sizeof(part) - 1] = 0;
		AppendStr(line, sizeof(line), part);
	}

	// result
	char ret[80];
	ret[0] = 0;
	if (hasResult && log->Format[0] == 's')
	{
		_snprintf(ret, sizeof(ret), " -> \"%.48s\"", SafeStr(a->ReturnValue[0]));
		sig = (sig ^ 0x5) * 16777619u;
	}
	else if (hasResult && log->Format[0] == 'i')
	{
		_snprintf(ret, sizeof(ret), " -> %d", a->ReturnValue[0]);
		sig = (sig ^ (DWORD)a->ReturnValue[0]) * 16777619u;
	}
	ret[sizeof(ret) - 1] = 0;

	// only log when something changed (pollers like UGC_HAS_GET_FINISHED run every frame)
	if (log->Count > 0 && sig == log->LastSig)
		return;
	log->LastSig = sig;
	log->Count++;

	DbgPrint("[RockstarConnected] JOB %s(%s)%s\n", log->Name, line, ret);
#endif
}

// generates: g_Orig_<id>, g_Log_<id>, <id>_hook (calls original, then logs)
#define LOGGED_NATIVE(id, name, fmt) \
	NativeHook_t g_Orig_##id = NULL; \
	CallLog_s g_Log_##id = { name, fmt, 0, 0 }; \
	void id##_hook(NativeArg_s* a) \
	{ \
		if (g_Orig_##id != NULL) g_Orig_##id(a); \
		LogCall(&g_Log_##id, a, TRUE); \
	}

LOGGED_NATIVE(DatafileLoadOfflineUgc,   "DATAFILE_LOAD_OFFLINE_UGC",            "i:s")
LOGGED_NATIVE(UgcRequestFromParams,     "UGC_REQUEST_CONTENT_DATA_FROM_PARAMS", "i:ssiii")
LOGGED_NATIVE(UgcRequestFromIndex,      "UGC_REQUEST_CONTENT_DATA_FROM_INDEX",  "i:ii")
LOGGED_NATIVE(UgcQueryByContentId,      "UGC_QUERY_BY_CONTENT_ID",              "i:sis")
LOGGED_NATIVE(UgcHasGetFinished,        "UGC_HAS_GET_FINISHED",                 "i:")

// cloud job queries (the script uses these when it thinks the cloud is up).
// with FAKE_UGC_QUERY they don't touch the dead cloud: they report "query started"
// and the results come from the local (offline) job data that is already loaded.
#if FAKE_UGC_QUERY && FORCE_UGC_OFFLINE
#define FAKED_UGC_QUERY(id, name, fmt) \
	NativeHook_t g_Orig_##id = NULL; \
	CallLog_s g_Log_##id = { name " (faked -> 1)", fmt, 0, 0 }; \
	void id##_hook(NativeArg_s* a) \
	{ \
		a->ReturnValue[0] = 1; \
		LogCall(&g_Log_##id, a, TRUE); \
	}
#else
#define FAKED_UGC_QUERY(id, name, fmt) LOGGED_NATIVE(id, name, fmt)
#endif

FAKED_UGC_QUERY(UgcGetByCategory,       "UGC_GET_GET_BY_CATEGORY",              "i:iiiii")
FAKED_UGC_QUERY(UgcGetMyContent,        "UGC_GET_MY_CONTENT",                   "i:iiii")
FAKED_UGC_QUERY(UgcGetBookmarked,       "UGC_GET_BOOKMARKED_CONTENT",           "i:iiii")
FAKED_UGC_QUERY(UgcGetFriendContent,    "UGC_GET_FRIEND_CONTENT",               "i:iiii")
FAKED_UGC_QUERY(UgcGetCrewContent,      "UGC_GET_CREW_CONTENT",                 "i:iiiii")

// UGC_DID_GET_SUCCEED: real value logged, forced to 1 when faking queries
NativeHook_t g_Orig_UgcDidGetSucceed = NULL;
CallLog_s g_Log_UgcDidGetSucceed = { "UGC_DID_GET_SUCCEED (real)", "i:", 0, 0 };
void UgcDidGetSucceed_hook(NativeArg_s* a)
{
	if (g_Orig_UgcDidGetSucceed != NULL) g_Orig_UgcDidGetSucceed(a);
	LogCall(&g_Log_UgcDidGetSucceed, a, TRUE);
#if FAKE_UGC_QUERY && FORCE_UGC_OFFLINE
	a->ReturnValue[0] = 1;
#endif
}
LOGGED_NATIVE(UgcGetContentNum,         "UGC_GET_CONTENT_NUM",                  "i:")
LOGGED_NATIVE(UgcGetQueryResult,        "UGC_GET_QUERY_RESULT",                 "i:")
LOGGED_NATIVE(DatafileHasLoaded,        "DATAFILE_HAS_LOADED_FILE_DATA",        "i:i")
LOGGED_NATIVE(DatafileHasValid,         "DATAFILE_HAS_VALID_FILE_DATA",         "i:i")
LOGGED_NATIVE(CloudHasRequestCompleted, "CLOUD_HAS_REQUEST_COMPLETED",          "i:i")

// UGC_SET_QUERY_DATA_FROM_OFFLINE(BOOL): optionally force TRUE so job
// queries are answered from the local job data instead of the dead cloud
NativeHook_t g_Orig_UgcSetOffline = NULL;
CallLog_s g_Log_UgcSetOffline = { "UGC_SET_QUERY_DATA_FROM_OFFLINE (game's value)", "v:i", 0, 0 };
void UgcSetOffline_hook(NativeArg_s* a)
{
	LogCall(&g_Log_UgcSetOffline, a, FALSE);
#if FORCE_UGC_OFFLINE
	if (a->ArgCount >= 1)
		a->ArgValues[0] = 1;
#endif
	if (g_Orig_UgcSetOffline != NULL)
		g_Orig_UgcSetOffline(a);
}

// NETWORK_IS_CLOUD_AVAILABLE: log the real value, optionally force 1
NativeHook_t g_Orig_CloudAvailable = NULL;
CallLog_s g_Log_CloudAvailable = { "NETWORK_IS_CLOUD_AVAILABLE (real)", "i:", 0, 0 };
void CloudAvailable_hook(NativeArg_s* a)
{
	if (g_Orig_CloudAvailable != NULL) g_Orig_CloudAvailable(a);
	LogCall(&g_Log_CloudAvailable, a, TRUE);
#if FORCE_CLOUD_AVAILABLE
	a->ReturnValue[0] = 1;
#endif
}

// CLOUD_DID_REQUEST_SUCCEED(handle): log the real value, optionally force 1
NativeHook_t g_Orig_CloudDidSucceed = NULL;
CallLog_s g_Log_CloudDidSucceed = { "CLOUD_DID_REQUEST_SUCCEED (real)", "i:i", 0, 0 };
void CloudDidSucceed_hook(NativeArg_s* a)
{
	if (g_Orig_CloudDidSucceed != NULL) g_Orig_CloudDidSucceed(a);
	LogCall(&g_Log_CloudDidSucceed, a, TRUE);
#if FORCE_CLOUD_REQUEST_SUCCESS
	a->ReturnValue[0] = 1;
#endif
}

// one entry per native we hook
struct PatchEntry_s
{
	DWORD			Hash;
	NativeHook_t	Hook;
	BOOL			Patched;	// patched at least once (for logging)
	NativeHook_t*	Original;	// if set, receives the game's original handler (for wrapper hooks)
};

#define MAX_PATCHES 64
PatchEntry_s g_Patches[MAX_PATCHES];
unsigned int g_PatchCount = 0;

void AddPatches(const DWORD* hashes, unsigned int count, NativeHook_t hook)
{
	for (unsigned int i = 0; i < count && g_PatchCount < MAX_PATCHES; i++)
	{
		g_Patches[g_PatchCount].Hash    = hashes[i];
		g_Patches[g_PatchCount].Hook    = hook;
		g_Patches[g_PatchCount].Patched  = FALSE;
		g_Patches[g_PatchCount].Original = NULL;
		g_PatchCount++;
	}
}

// hook that calls through to the original (the original is saved before patching)
void AddWrapperPatch(DWORD hash, NativeHook_t hook, NativeHook_t* original)
{
	if (g_PatchCount >= MAX_PATCHES)
		return;

	g_Patches[g_PatchCount].Hash     = hash;
	g_Patches[g_PatchCount].Hook     = hook;
	g_Patches[g_PatchCount].Patched  = FALSE;
	g_Patches[g_PatchCount].Original = original;
	g_PatchCount++;
}

void BuildPatchList()
{
	AddPatches(g_NativeHashes,     ARRAY_COUNT(g_NativeHashes),     native_ret1_hook);
	AddPatches(g_NativeHashesRet0, ARRAY_COUNT(g_NativeHashesRet0), native_ret0_hook);
	AddPatches(g_NativeHashesNop,  ARRAY_COUNT(g_NativeHashesNop),  native_nop_hook);

	DWORD cloudFinished[] = { HASH_HAS_CLOUD_REQUESTS_FINISHED };
	AddPatches(cloudFinished, 1, native_cloud_requests_finished_hook);

	AddWrapperPatch(HASH_PLAYER_ID, native_player_id_hook, &g_OrigPlayerId);

	// cloud (log real value, force per switches above)
	AddWrapperPatch(HASH_NETWORK_IS_CLOUD_AVAILABLE, CloudAvailable_hook,  &g_Orig_CloudAvailable);
	AddWrapperPatch(HASH_CLOUD_DID_REQUEST_SUCCEED,  CloudDidSucceed_hook, &g_Orig_CloudDidSucceed);

	// jobs
	AddWrapperPatch(HASH_UGC_SET_QUERY_DATA_FROM_OFFLINE,      UgcSetOffline_hook,            &g_Orig_UgcSetOffline);
	AddWrapperPatch(HASH_DATAFILE_LOAD_OFFLINE_UGC,            DatafileLoadOfflineUgc_hook,   &g_Orig_DatafileLoadOfflineUgc);
	AddWrapperPatch(HASH_UGC_REQUEST_CONTENT_DATA_FROM_PARAMS, UgcRequestFromParams_hook,     &g_Orig_UgcRequestFromParams);
	AddWrapperPatch(HASH_UGC_REQUEST_CONTENT_DATA_FROM_INDEX,  UgcRequestFromIndex_hook,      &g_Orig_UgcRequestFromIndex);
	AddWrapperPatch(HASH_UGC_QUERY_BY_CONTENT_ID,              UgcQueryByContentId_hook,      &g_Orig_UgcQueryByContentId);
	AddWrapperPatch(HASH_UGC_GET_GET_BY_CATEGORY,              UgcGetByCategory_hook,         &g_Orig_UgcGetByCategory);
	AddWrapperPatch(HASH_UGC_GET_MY_CONTENT,                   UgcGetMyContent_hook,          &g_Orig_UgcGetMyContent);
	AddWrapperPatch(HASH_UGC_GET_BOOKMARKED_CONTENT,           UgcGetBookmarked_hook,         &g_Orig_UgcGetBookmarked);
	AddWrapperPatch(HASH_UGC_GET_FRIEND_CONTENT,               UgcGetFriendContent_hook,      &g_Orig_UgcGetFriendContent);
	AddWrapperPatch(HASH_UGC_GET_CREW_CONTENT,                 UgcGetCrewContent_hook,        &g_Orig_UgcGetCrewContent);
	AddWrapperPatch(HASH_UGC_HAS_GET_FINISHED,                 UgcHasGetFinished_hook,        &g_Orig_UgcHasGetFinished);
	AddWrapperPatch(HASH_UGC_DID_GET_SUCCEED,                  UgcDidGetSucceed_hook,         &g_Orig_UgcDidGetSucceed);
	AddWrapperPatch(HASH_UGC_GET_CONTENT_NUM,                  UgcGetContentNum_hook,         &g_Orig_UgcGetContentNum);
	AddWrapperPatch(HASH_UGC_GET_QUERY_RESULT,                 UgcGetQueryResult_hook,        &g_Orig_UgcGetQueryResult);
	AddWrapperPatch(HASH_DATAFILE_HAS_LOADED_FILE_DATA,        DatafileHasLoaded_hook,        &g_Orig_DatafileHasLoaded);
	AddWrapperPatch(HASH_DATAFILE_HAS_VALID_FILE_DATA,         DatafileHasValid_hook,         &g_Orig_DatafileHasValid);
	AddWrapperPatch(HASH_CLOUD_HAS_REQUEST_COMPLETED,          CloudHasRequestCompleted_hook, &g_Orig_CloudHasRequestCompleted);
}

// one pass over the native table: (re)apply every hook whose native is
// registered and not already pointing at our hook. returns how many are patched.
unsigned int ApplyPatches()
{
	unsigned int patchedNow = 0;

	for (unsigned int i = 0; i < g_PatchCount; i++)
	{
		DWORD* funcPtr = GetNativeFunctionPointer(g_Patches[i].Hash);
		if (funcPtr == NULL || *funcPtr == 0)
			continue; // not registered yet

		DWORD hookAddr = (DWORD)g_Patches[i].Hook;
		if (*funcPtr != hookAddr)
		{
			// wrapper hooks: remember the game's handler before replacing it
			// (must be set before the slot points at our hook)
			if (g_Patches[i].Original != NULL)
			{
				*g_Patches[i].Original = (NativeHook_t)*funcPtr;
				__sync();
			}

			*funcPtr = hookAddr;
			__dcbst(0, funcPtr);
			__sync();

			LogNative(g_Patches[i].Patched ? "RE-patched native (was overwritten)" : "patched native", g_Patches[i].Hash);
			g_Patches[i].Patched = TRUE;
		}
		patchedNow++;
	}

	return patchedNow;
}

// init
DWORD WINAPI Initialize(LPVOID lpParam)
{
	BuildPatchList();
	DbgPrint("[RockstarConnected] started, waiting for native registration\n");

	// Scripts resolve native hashes -> function pointers ONCE when they load,
	// so the hooks must be in the table before scripts like maintransition start.
	// Poll fast from boot and patch each native the moment it is registered.
	// Keep re-applying for a while in case the game rewrites a slot.
	const DWORD POLL_MS    = 5;
	const DWORD RUN_FOR_MS = 120000;

	DWORD start = GetTickCount();
	while (GetTickCount() - start < RUN_FOR_MS)
	{
		ApplyPatches();
		Sleep(POLL_MS);
	}

	// final report
	for (unsigned int i = 0; i < g_PatchCount; i++)
	{
		if (!g_Patches[i].Patched)
			LogNative("NOT FOUND native", g_Patches[i].Hash);
	}
	DbgPrint("[RockstarConnected] patch thread finished\n");

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