//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Copyright (c) 2008-2019 NVIDIA Corporation. All rights reserved.

#include "PxPhysXConfig.h"

#if PX_SUPPORT_GPU_PHYSX

#include "foundation/Px.h"
#include "PsFoundation.h"
#include "PxPhysics.h"
#include "PxGpu.h"

#include "cudamanager/PxCudaContextManager.h"

namespace physx
{
	// alias shared foundation to something usable
	namespace Ps = shdfnd;
}

#define STRINGIFY(x) #x
#define GETSTRING(x) STRINGIFY(x)

#if PX_X86
#define PLATFORM_SUB_STR "x86"
#elif PX_X64
#define PLATFORM_SUB_STR "x64"
#endif

#if defined(PX_PHYSX_DLL_NAME_POSTFIX)
#define CONFIG_SUB_STR GETSTRING(PX_PHYSX_DLL_NAME_POSTFIX)
#else
#define CONFIG_SUB_STR
#endif

#if PX_WINDOWS

#include "windows/PsWindowsInclude.h"
#include "windows/PxWindowsDelayLoadHook.h"
#include "windows/CmWindowsModuleUpdateLoader.h"
static const char*	gPhysXGpuLibraryName = "PhysX3Gpu" CONFIG_SUB_STR "_" PLATFORM_SUB_STR ".dll";

namespace physx
{
	const PxDelayLoadHook* PxGetPhysXDelayLoadHook();
}

#elif PX_LINUX

#include <dlfcn.h>
static const char*	gPhysXGpuLibraryName = "./libPhysX3Gpu" CONFIG_SUB_STR "_" PLATFORM_SUB_STR ".so";

#endif // PX_LINUX

#undef GETSTRING
#undef STRINGIFY

void PxSetPhysXGpuLoadHook(const PxGpuLoadHook* hook)
{
	if(strstr(gPhysXGpuLibraryName, "DEBUG"))
	{
		gPhysXGpuLibraryName = hook->getPhysXGpuDEBUGDllName();
		return;
	}

	if(strstr(gPhysXGpuLibraryName, "CHECKED"))
	{
		gPhysXGpuLibraryName = hook->getPhysXGpuCHECKEDDllName();
		return;
	}

	if(strstr(gPhysXGpuLibraryName, "PROFILE"))
	{
		gPhysXGpuLibraryName = hook->getPhysXGpuPROFILEDllName();
		return;
	}

	gPhysXGpuLibraryName = hook->getPhysXGpuDllName();
}

namespace physx
{
#if PX_VC
#pragma warning(disable: 4191)	//'operator/operation' : unsafe conversion from 'type of expression' to 'type required'
#endif

	class PxFoundation;
	class PxPhysXGpu;

	typedef physx::PxPhysXGpu* (PxCreatePhysXGpu_FUNC)();
	typedef physx::PxCudaContextManager* (PxCreateCudaContextManager_FUNC)(physx::PxFoundation& foundation, const physx::PxCudaContextManagerDesc& desc);
	typedef int (PxGetSuggestedCudaDeviceOrdinal_FUNC)(physx::PxErrorCallback& errc);

	PxCreatePhysXGpu_FUNC* g_PxCreatePhysXGpu_Func = NULL;
	PxCreateCudaContextManager_FUNC* g_PxCreateCudaContextManager_Func = NULL;
	PxGetSuggestedCudaDeviceOrdinal_FUNC* g_PxGetSuggestedCudaDeviceOrdinal_Func = NULL;

#if PX_WINDOWS

	typedef void (PxSetPhysXGpuDelayLoadHook_FUNC)(const PxDelayLoadHook* delayLoadHook);
	PxSetPhysXGpuDelayLoadHook_FUNC* g_PxSetPhysXGpuDelayLoadHook_Func = NULL;

#define DEFAULT_PHYSX_GPU_GUID    "D79FA4BF-177C-4841-8091-4375D311D6A3"

	void PxLoadPhysxGPUModule(const char* appGUID)
	{
		static HMODULE s_library;

		if (s_library == NULL)
			s_library = GetModuleHandle(gPhysXGpuLibraryName);

		if (s_library == NULL)
		{
			Cm::CmModuleUpdateLoader moduleLoader(UPDATE_LOADER_DLL_NAME);
			s_library = moduleLoader.LoadModule(gPhysXGpuLibraryName, appGUID == NULL ? DEFAULT_PHYSX_GPU_GUID : appGUID);
		}

		if (s_library)
		{
			g_PxSetPhysXGpuDelayLoadHook_Func = (PxSetPhysXGpuDelayLoadHook_FUNC*)GetProcAddress(s_library, "PxSetPhysXGpuDelayLoadHook");
			g_PxCreatePhysXGpu_Func = (PxCreatePhysXGpu_FUNC*)GetProcAddress(s_library, "PxCreatePhysXGpu");
			g_PxCreateCudaContextManager_Func = (PxCreateCudaContextManager_FUNC*)GetProcAddress(s_library, "PxCreateCudaContextManager");
			g_PxGetSuggestedCudaDeviceOrdinal_Func = (PxGetSuggestedCudaDeviceOrdinal_FUNC*)GetProcAddress(s_library, "PxGetSuggestedCudaDeviceOrdinal");
		}

		// Check for errors
		if (s_library == NULL)
		{
			Ps::getFoundation().error(PxErrorCode::eINTERNAL_ERROR, __FILE__, __LINE__, "Failed to load PhysXGpu dll!");
			return;
		}

		if (g_PxSetPhysXGpuDelayLoadHook_Func == NULL || g_PxCreatePhysXGpu_Func == NULL || g_PxCreateCudaContextManager_Func == NULL || g_PxGetSuggestedCudaDeviceOrdinal_Func == NULL)
		{
			Ps::getFoundation().error(PxErrorCode::eINTERNAL_ERROR, __FILE__, __LINE__, "PhysXGpu dll is incompatible with this version of PhysX!");
			return;
		}

		g_PxSetPhysXGpuDelayLoadHook_Func(PxGetPhysXDelayLoadHook());

	}

#elif PX_LINUX

	void PxLoadPhysxGPUModule(const char*)
	{
		static void* s_library;

		if (s_library == NULL)
		{
			// load libcuda.so here since gcc configured with --as-needed won't link to it
			// if there is no call from the binary to it.
			void* hLibCuda = dlopen("libcuda.so", RTLD_NOW | RTLD_GLOBAL);
			if (hLibCuda)
			{
				s_library = dlopen(gPhysXGpuLibraryName, RTLD_NOW);
			}
		}

		// no UpdateLoader
		if (s_library)
		{
			*reinterpret_cast<void**>(&g_PxCreatePhysXGpu_Func) = dlsym(s_library, "PxCreatePhysXGpu");
			*reinterpret_cast<void**>(&g_PxCreateCudaContextManager_Func) = dlsym(s_library, "PxCreateCudaContextManager");
			*reinterpret_cast<void**>(&g_PxGetSuggestedCudaDeviceOrdinal_Func) = dlsym(s_library, "PxGetSuggestedCudaDeviceOrdinal");
		}

		// Check for errors
		if (s_library == NULL)
			Ps::getFoundation().error(PxErrorCode::eINTERNAL_ERROR, __FILE__, __LINE__, "Failed to load PhysXGpu so!");
		if (g_PxCreatePhysXGpu_Func == NULL || g_PxCreateCudaContextManager_Func == NULL || g_PxGetSuggestedCudaDeviceOrdinal_Func == NULL)
			Ps::getFoundation().error(PxErrorCode::eINTERNAL_ERROR, __FILE__, __LINE__, "PhysXGpu so is incompatible with this version of PhysX!");
	}

#endif // PX_LINUX

} // end physx namespace

#endif // PX_SUPPORT_GPU_PHYSX