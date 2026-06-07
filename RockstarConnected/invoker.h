// 
// Native Invoker for GTA V TU13
// Native Table Address: 0x83D384E8
#pragma once

struct Native_s
{
	Native_s*		LastNativesTable;
	unsigned int	NativeFunctions[7];
	unsigned int	NativeCount;
	unsigned int	NativeHashes[7];
};

struct NativeArg_s
{
	int*			ReturnValue;
	unsigned int	ArgCount;
	int*			ArgValues;
	unsigned int	ReadCount;
};

// TU13 Native Table Address
static Native_s** g_Natives = (Native_s**)0x83D384E8;

// Argument buffer
static NativeArg_s* NativeArg = (NativeArg_s*)0x83DBC000;