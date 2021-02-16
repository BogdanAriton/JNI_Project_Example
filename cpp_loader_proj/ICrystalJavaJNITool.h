#pragma warning(disable:4290)

#ifndef _IJNITOOL_H_
#define _IJNITOOL_H_

#ifndef E_JNI_CAST	
#define E_JNI_CAST	ICrystalJavaJNITool
#endif

#define NULL 0

#include "CrystalJavaJNITool_common.h";
// Soarian

class ICrystalJavaJNITool
{
public:

	// ***
	virtual long GenerateJVM() = 0;
	virtual long DestroyJVM() = 0;
	virtual	long ExportReport(HQIPtr<IHRecordset> recordSet, HString reportFileName, HString exportFileName, HString dbServerName, HString dbDataBaseName, HString dbUserName, HString dbUserPassword, HString exportFormat, HString &retMessage, HString &retCode)  = 0;
	// All purpose method for Export
	virtual long RunAllGenerateReport(HQIPtr<IHRecordset> recordSet, HString reportFileName, HString exportFileName, HString dbServerName, HString dbDataBaseName, HString dbUserName, HString dbUserPassword, HString exportFormat,
		HString invalidFonts, HString appLogFileLocation, HString sapLogFileLocation, HString appLogLevel, HString sapLogLevel, HString isLocalPrintorPreview, HString &retMessage, HString &retCode) = 0;
	virtual long setParametersAndDB(HQIPtr<IHRecordset> recordSet, HString reportFileName, HString dbServerName, HString dbDataBaseName, HString dbUserName, HString dbUserPassword, HString &retMessage, HString &retCode)  = 0;
	virtual long OpenReport(HString reportFileName, HString &retMessage, HString &retCode)  = 0;
	virtual long HasInvalidFonts(HString reportFileName, HString invalidFonts, HString &retMessage, HString &retCode)  = 0;
	virtual long CheckInvalidPageSize(HString reportFileName, HString &retMessage, HString &retCode)  = 0;
	virtual long GetMainSPName(HString reportFileName, HString &retMessage, HString &retCode)  = 0;
	virtual long SetLogging(HString appLogFileLocation, HString sapLogFileLocation, HString appLogLevel, HString sapLogLevel, HString &retMessage, HString &retCode) = 0;
	virtual char* GetReturnMessage() = 0;
	virtual char* GetReturnCode() = 0;
	virtual long GetLongReturnCode(char* returnCode) = 0;
	virtual HString GetMethodName() = 0;
	virtual HString GetMethodSignature() = 0;
	virtual void SetMethodName(HString newName) = 0;
	virtual void SetMethodSignature(HString newSignature) = 0;

	virtual void SetIsInvalidFonts(char* hasInvalidFonts) = 0;
	virtual void SetIsInvalidPageSize(char* hasInvalidPageSize) = 0;
	virtual void SetSPName(char* spName) = 0;
	virtual void SetBlankParams(char* blankParams) = 0;
	virtual HString GetIsInvalidFonts() = 0;
	virtual HString GetIsInvalidPageSize() = 0;
	virtual HString GetSPName() = 0;
	virtual HString GetBlankParams() = 0;
	
	virtual HString GetClassName() = 0;
	virtual HString GetClassPath() = 0;

};

#endif