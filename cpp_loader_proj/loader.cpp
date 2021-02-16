// OMSOPRServiceProvider.cpp : Defines the entry point for the console application.
//
#include "OMSOPRServiceProvider_Precompiled.h"
#include "ISpinnerMonitor.h"
#include "..\CrystalReportJavaGenerator\ICrystalJavaJNITool.h"
#include "..\OMSCommon\JNIToolInstance.h"
#include "..\OMSCommon\SpinnerMonitorInstance.h"

#pragma warning(disable:4786)

typedef LPVOID (*pvGetSpinnerMonitor)();

int main(int argc, char** argv)
{
	HQIPtr<IErrorInfo> spErr;
	ISpinnerMonitor* piSpinnerMonitor = NULL;

	InitializeSEF(argc, argv);
	
	// Read the monitor configuration to check weather is it is active or not
	piSpinnerMonitor = SpinnerMonitorInstance::getSpinnerInstance();
	if (piSpinnerMonitor != NULL)
	{
		HTRACE_MOD(L"OMS", 0, L"OMSOPRServiceProvider: [ SpinnerMonitor ] Preparing to Register Spinner Monitor.");	
		piSpinnerMonitor->RegisterMonitor();
		HTRACE_MOD(L"OMS", 0, L"OMSOPRServiceProvider: [ SpinnerMonitor ] Spinner Monitor registered successfully.");
	}
	else
		HTRACE_MOD(L"OMS", 0, L"OMSOPRServiceProvider: [ SpinnerMonitor ] Spinner Monitor Not registered.");
	

	try
	{
		// Create Java pointer
		HTRACE_MOD(L"OMS", 0, L"OMSOPRServiceProvider() - Initializing Java Generator.");
		ICrystalJavaJNITool* javaGenerator = JNIToolInstance::getJNIToolInstance();
		if (javaGenerator == NULL)
		{
			// we will continue with loading the service provider and throw the error later on when a JAVA report is being printed.
			HTRACE_MOD(L"OMS", 0, L"Java Crystal generator. Is not initialized.");
		}
		else
			HTRACE_MOD(L"OMS", 0, L"OMSOPRServiceProvider() - Java Generator was initialized succesfully.");

		HRESULT hr = S_OK;				
		HQIPtr<IHServiceProvider> spServProvider;		
		CreateServiceProvider(&spServProvider);		
		/*Provide information about the Service configuration file*/
		hr = spServProvider->InitialiseServiceCatalog(L"OMSOPR_Services");
		hr = spServProvider->Run();	
		spServProvider.Release();		
		UninitializeSEF();		
	}
	catch(...)
	{ 
		HRESULT hr = S_OK; 
		HString hsErrDesc; 
		hr = GetErrorInfo(0, &spErr); 
		if (SUCCEEDED(hr) && spErr != NULL) 
		{ 
			spErr->GetDescription(&hsErrDesc); 
			HString hsErrMsg(L"OMSOPRServiceProvider::Main : ProcessID = "); 
			HString hsProcID; 
			//swprintf(hsProcID, L"%s", GetCurrentProcessId()); 
			hsProcID.Format(L"%lu", GetCurrentProcessId());
			hsErrMsg += hsProcID; 
			hsErrMsg += L" : "; 
			hsErrMsg += hsErrDesc; 
			HTRACE_MESSAGE(0, hsErrMsg); 
		} 
	} 
	
	/*
	 * Charm 179842 (child 182764) - OMSOPR Spinning provider (the craxdrt hang)
	 * For the following code to be executed, make sure you have set ARC_MEMORYSCRUBBING=Y for OMSOPR broker
	 */
	HTRACE_MOD(L"OMS",0, L"Terminating current OMSOPRServiceProvider process to overcome the spinning issue (charm 182764)");
	TerminateProcess(GetCurrentProcess(), 0);
	
	return 0;
}