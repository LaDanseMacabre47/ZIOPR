

/* this ALWAYS GENERATED file contains the RPC client stubs */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 06:14:07 2038
 */
/* Compiler settings for rpc_interface\TrayService.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.01.0628 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if !defined(_M_IA64) && !defined(_M_AMD64) && !defined(_ARM_)


#pragma warning( disable: 4049 )  /* more than 64k source lines */
#if _MSC_VER >= 1200
#pragma warning(push)
#endif

#pragma warning( disable: 4211 )  /* redefine extern to static */
#pragma warning( disable: 4232 )  /* dllimport identity*/
#pragma warning( disable: 4024 )  /* array to pointer mapping*/
#pragma warning( disable: 4100 ) /* unreferenced arguments in x86 call */

#pragma optimize("", off ) 

#include <string.h>

#include "TrayService.h"

#define TYPE_FORMAT_STRING_SIZE   23                                
#define PROC_FORMAT_STRING_SIZE   207                               
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   0            

typedef struct _TrayService_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } TrayService_MIDL_TYPE_FORMAT_STRING;

typedef struct _TrayService_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } TrayService_MIDL_PROC_FORMAT_STRING;

typedef struct _TrayService_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } TrayService_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax_2_0 = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};

#if defined(_CONTROL_FLOW_GUARD_XFG)
#define XFG_TRAMPOLINES(ObjectType)\
NDR_SHAREABLE unsigned long ObjectType ## _UserSize_XFG(unsigned long * pFlags, unsigned long Offset, void * pObject)\
{\
return  ObjectType ## _UserSize(pFlags, Offset, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserMarshal_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserMarshal(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserUnmarshal_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserUnmarshal(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE void ObjectType ## _UserFree_XFG(unsigned long * pFlags, void * pObject)\
{\
ObjectType ## _UserFree(pFlags, (ObjectType *)pObject);\
}
#define XFG_TRAMPOLINES64(ObjectType)\
NDR_SHAREABLE unsigned long ObjectType ## _UserSize64_XFG(unsigned long * pFlags, unsigned long Offset, void * pObject)\
{\
return  ObjectType ## _UserSize64(pFlags, Offset, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserMarshal64_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserMarshal64(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE unsigned char * ObjectType ## _UserUnmarshal64_XFG(unsigned long * pFlags, unsigned char * pBuffer, void * pObject)\
{\
return ObjectType ## _UserUnmarshal64(pFlags, pBuffer, (ObjectType *)pObject);\
}\
NDR_SHAREABLE void ObjectType ## _UserFree64_XFG(unsigned long * pFlags, void * pObject)\
{\
ObjectType ## _UserFree64(pFlags, (ObjectType *)pObject);\
}
#define XFG_BIND_TRAMPOLINES(HandleType, ObjectType)\
static void* ObjectType ## _bind_XFG(HandleType pObject)\
{\
return ObjectType ## _bind((ObjectType) pObject);\
}\
static void ObjectType ## _unbind_XFG(HandleType pObject, handle_t ServerHandle)\
{\
ObjectType ## _unbind((ObjectType) pObject, ServerHandle);\
}
#define XFG_TRAMPOLINE_FPTR(Function) Function ## _XFG
#define XFG_TRAMPOLINE_FPTR_DEPENDENT_SYMBOL(Symbol) Symbol ## _XFG
#else
#define XFG_TRAMPOLINES(ObjectType)
#define XFG_TRAMPOLINES64(ObjectType)
#define XFG_BIND_TRAMPOLINES(HandleType, ObjectType)
#define XFG_TRAMPOLINE_FPTR(Function) Function
#define XFG_TRAMPOLINE_FPTR_DEPENDENT_SYMBOL(Symbol) Symbol
#endif


extern const TrayService_MIDL_TYPE_FORMAT_STRING TrayService__MIDL_TypeFormatString;
extern const TrayService_MIDL_PROC_FORMAT_STRING TrayService__MIDL_ProcFormatString;
extern const TrayService_MIDL_EXPR_FORMAT_STRING TrayService__MIDL_ExprFormatString;

#define GENERIC_BINDING_TABLE_SIZE   0            


/* Standard interface: TrayService, ver. 1.0,
   GUID={0xAABBCCDD,0x1234,0x5678,{0xAB,0xCD,0x12,0x34,0x56,0x78,0x90,0xAB}} */



static const RPC_CLIENT_INTERFACE TrayService___RpcClientInterface =
    {
    sizeof(RPC_CLIENT_INTERFACE),
    {{0xAABBCCDD,0x1234,0x5678,{0xAB,0xCD,0x12,0x34,0x56,0x78,0x90,0xAB}},{1,0}},
    {{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}},
    0,
    0,
    0,
    0,
    0,
    0x00000000
    };
RPC_IF_HANDLE TrayService_v1_0_c_ifspec = (RPC_IF_HANDLE)& TrayService___RpcClientInterface;
#ifdef __cplusplus
namespace {
#endif

extern const MIDL_STUB_DESC TrayService_StubDesc;
#ifdef __cplusplus
}
#endif

static RPC_BINDING_HANDLE TrayService__MIDL_AutoBindHandle;


long RpcLogin( 
    /* [in] */ handle_t IDL_handle,
    /* [string][in] */ const wchar_t *username,
    /* [string][in] */ const wchar_t *password)
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TrayService_StubDesc,
                  (PFORMAT_STRING) &TrayService__MIDL_ProcFormatString.Format[0],
                  ( unsigned char * )&IDL_handle);
    return ( long  )_RetVal.Simple;
    
}


void RpcLogout( 
    /* [in] */ handle_t IDL_handle)
{

    NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TrayService_StubDesc,
                  (PFORMAT_STRING) &TrayService__MIDL_ProcFormatString.Format[46],
                  ( unsigned char * )&IDL_handle);
    
}


long RpcGetCurrentUser( 
    /* [in] */ handle_t IDL_handle,
    /* [out] */ long *authenticated,
    /* [string][out] */ wchar_t **username)
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TrayService_StubDesc,
                  (PFORMAT_STRING) &TrayService__MIDL_ProcFormatString.Format[74],
                  ( unsigned char * )&IDL_handle);
    return ( long  )_RetVal.Simple;
    
}


long RpcGetLicenseStatus( 
    /* [in] */ handle_t IDL_handle,
    /* [out] */ long *hasLicense,
    /* [out] */ __int64 *expiryUnixTime)
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TrayService_StubDesc,
                  (PFORMAT_STRING) &TrayService__MIDL_ProcFormatString.Format[120],
                  ( unsigned char * )&IDL_handle);
    return ( long  )_RetVal.Simple;
    
}


long RpcActivateProduct( 
    /* [in] */ handle_t IDL_handle,
    /* [string][in] */ const wchar_t *activationCode)
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TrayService_StubDesc,
                  (PFORMAT_STRING) &TrayService__MIDL_ProcFormatString.Format[166],
                  ( unsigned char * )&IDL_handle);
    return ( long  )_RetVal.Simple;
    
}


#if !defined(__RPC_WIN32__)
#error  Invalid build platform for this stub.
#endif

#if !(TARGET_IS_NT50_OR_LATER)
#error You need Windows 2000 or later to run this stub because it uses these features:
#error   /robust command line switch.
#error However, your C/C++ compilation flags indicate you intend to run this app on earlier systems.
#error This app will fail with the RPC_X_WRONG_STUB_VERSION error.
#endif


static const TrayService_MIDL_PROC_FORMAT_STRING TrayService__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure RpcLogin */

			0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x0 ),	/* 0 */
/*  8 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 10 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 12 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 14 */	NdrFcShort( 0x0 ),	/* 0 */
/* 16 */	NdrFcShort( 0x8 ),	/* 8 */
/* 18 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 20 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x0 ),	/* 0 */
/* 26 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter username */

/* 28 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 30 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 32 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter password */

/* 34 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 36 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 38 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Return value */

/* 40 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 42 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 44 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RpcLogout */

/* 46 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 48 */	NdrFcLong( 0x0 ),	/* 0 */
/* 52 */	NdrFcShort( 0x1 ),	/* 1 */
/* 54 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 56 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 58 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 60 */	NdrFcShort( 0x0 ),	/* 0 */
/* 62 */	NdrFcShort( 0x0 ),	/* 0 */
/* 64 */	0x40,		/* Oi2 Flags:  has ext, */
			0x0,		/* 0 */
/* 66 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 68 */	NdrFcShort( 0x0 ),	/* 0 */
/* 70 */	NdrFcShort( 0x0 ),	/* 0 */
/* 72 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Procedure RpcGetCurrentUser */

/* 74 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 76 */	NdrFcLong( 0x0 ),	/* 0 */
/* 80 */	NdrFcShort( 0x2 ),	/* 2 */
/* 82 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 84 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 86 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 88 */	NdrFcShort( 0x0 ),	/* 0 */
/* 90 */	NdrFcShort( 0x24 ),	/* 36 */
/* 92 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 94 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 96 */	NdrFcShort( 0x0 ),	/* 0 */
/* 98 */	NdrFcShort( 0x0 ),	/* 0 */
/* 100 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter authenticated */

/* 102 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 104 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 106 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter username */

/* 108 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 110 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 112 */	NdrFcShort( 0xa ),	/* Type Offset=10 */

	/* Return value */

/* 114 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 116 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 118 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RpcGetLicenseStatus */

/* 120 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 122 */	NdrFcLong( 0x0 ),	/* 0 */
/* 126 */	NdrFcShort( 0x3 ),	/* 3 */
/* 128 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 130 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 132 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 134 */	NdrFcShort( 0x0 ),	/* 0 */
/* 136 */	NdrFcShort( 0x48 ),	/* 72 */
/* 138 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 140 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 142 */	NdrFcShort( 0x0 ),	/* 0 */
/* 144 */	NdrFcShort( 0x0 ),	/* 0 */
/* 146 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter hasLicense */

/* 148 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 150 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 152 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter expiryUnixTime */

/* 154 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 156 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 158 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */

/* 160 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 162 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 164 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure RpcActivateProduct */

/* 166 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 168 */	NdrFcLong( 0x0 ),	/* 0 */
/* 172 */	NdrFcShort( 0x4 ),	/* 4 */
/* 174 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 176 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 178 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 180 */	NdrFcShort( 0x0 ),	/* 0 */
/* 182 */	NdrFcShort( 0x8 ),	/* 8 */
/* 184 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 186 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 188 */	NdrFcShort( 0x0 ),	/* 0 */
/* 190 */	NdrFcShort( 0x0 ),	/* 0 */
/* 192 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter activationCode */

/* 194 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 196 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 198 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Return value */

/* 200 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 202 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 204 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const TrayService_MIDL_TYPE_FORMAT_STRING TrayService__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/*  4 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/*  6 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/*  8 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 10 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 12 */	NdrFcShort( 0x2 ),	/* Offset= 2 (14) */
/* 14 */	
			0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 16 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/* 18 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 20 */	0xb,		/* FC_HYPER */
			0x5c,		/* FC_PAD */

			0x0
        }
    };

static const unsigned short TrayService_FormatStringOffsetTable[] =
    {
    0,
    46,
    74,
    120,
    166
    };


#ifdef __cplusplus
namespace {
#endif
static const MIDL_STUB_DESC TrayService_StubDesc = 
    {
    (void *)& TrayService___RpcClientInterface,
    MIDL_user_allocate,
    MIDL_user_free,
    &TrayService__MIDL_AutoBindHandle,
    0,
    0,
    0,
    0,
    TrayService__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x50002, /* Ndr library version */
    0,
    0x8010274, /* MIDL Version 8.1.628 */
    0,
    0,
    0,  /* notify & notify_flag routine table */
    0x1, /* MIDL flag */
    0, /* cs routines */
    0,   /* proxy/server info */
    0
    };
#ifdef __cplusplus
}
#endif
#if _MSC_VER >= 1200
#pragma warning(pop)
#endif


#endif /* !defined(_M_IA64) && !defined(_M_AMD64) && !defined(_ARM_) */

