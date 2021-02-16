#ifndef _CRYSTALJAVAJNITOOL_COMMON_H_
#define _CRYSTALJAVAJNITOOL_COMMON_H_

#ifndef _WIN32_WINNT 
#define _WIN32_WINNT 0x0501
#endif

#include <windows.h>
#include <Psapi.h>

// Soarian
#include <MXSUtil.h>
#include <StdHFCIncludes.h>
#include <HSQLData.h>
#include <HKernel.h>
#include <SecurityEnumTypes.h>
#include <SecurityManager.h>
#include <HEnvVar.h>
#include <HPersistence.h>
#include <MXSClientUtil.h>

#define E_JNI_INIT_OK (E_JNI_CAST*) 0
#define E_JNI_INIT_FAILED (E_JNI_CAST*) 1
#define E_JNI_CONFIG_FAILED (E_JNI_CAST*) 2
#define E_JNI_ERRMAX		 (E_JNI_CAST*) 32
#define JNI_FAILED(x)	((x != E_JNI_INIT_OK) && (x < E_JNI_ERRMAX))

#include "dbghelp.h"

#endif
