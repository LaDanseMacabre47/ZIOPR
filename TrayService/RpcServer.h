#pragma once
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL StartRpcServer();
void StopRpcServer();
void WaitForRpcServer();
