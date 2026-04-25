#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ---------------------------------------------------------------
// RPC client wrappers for TrayApp
// All functions return RPC_S_OK (0) on success.
// ---------------------------------------------------------------

// Auth
long  RpcClientLogin(const wchar_t* username, const wchar_t* password);
void  RpcClientLogout();
long  RpcClientGetCurrentUser(long* authenticated, wchar_t username[256]);

// License
long  RpcClientGetLicenseStatus(long* hasLicense, __int64* expiryUnixTime);
long  RpcClientActivateProduct(const wchar_t* activationCode);

// Legacy stop
void  RpcStopServiceCall();
