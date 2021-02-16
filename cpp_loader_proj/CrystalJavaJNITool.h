#ifndef JavaJNITool_USE 
#error "Incorrect Usage of CrystalJavaJNITool.h. Please use ICrystalJavaJNITool.h instead."
#endif

#ifndef _JNITOOL_H_
#define _JNITOOL_H_

#define NULL 0

#include "ICrystalJavaJNITool.h"
#include <string>
#include <iostream>
#include <windows.h>
#include <vector>
#include <cstdio>
#include <stdio.h>
#include <sstream>
#include <Psapi.h>
#include <jni.h>
#include <HFCString.h>
#include <HSQLData.h>
#include <HTraceUtils.h>
#include <map>

using namespace std;

typedef std::map<int, HString> ReturnMap;

class JavaJNI : public ICrystalJavaJNITool
{
public:
	virtual ~JavaJNI();
	static JavaJNI* GetInstance();
	
	long GenerateJVM();
	long DestroyJVM();
	long ExportReport(HQIPtr<IHRecordset> recordSet, HString reportFileName, HString exportFileName, HString dbServerName, HString dbDataBaseName, HString dbUserName, HString dbUserPassword, HString exportFormat, HString &retMessage, HString &retCode);
	long setParametersAndDB(HQIPtr<IHRecordset> recordSet, HString reportFileName, HString dbServerName, HString dbDataBaseName, HString dbUserName, HString dbUserPassword, HString &retMessage, HString &retCode);
	long OpenReport(HString reportFileName, HString &retMessage, HString &retCode);
	long HasInvalidFonts(HString reportFileName, HString invalidFonts, HString &retMessage, HString &retCode);
	long CheckInvalidPageSize(HString reportFileName, HString &retMessage, HString &retCode);
	long GetMainSPName(HString reportFileName, HString &retMessage, HString &retCode);
	long SetLogging(HString appLogFileLocation, HString sapLogFileLocation, HString appLogLevel, HString sapLogLevel, HString &retMessage, HString &retCode);

	// All purpose method for Export
	long RunAllGenerateReport(HQIPtr<IHRecordset> recordSet, HString reportFileName, HString exportFileName, HString dbServerName, HString dbDataBaseName, HString dbUserName, HString dbUserPassword, HString exportFormat,
		HString invalidFonts, HString appLogFileLocation, HString sapLogFileLocation, HString appLogLevel, HString sapLogLevel, HString isLocalPrintorPreview, HString &retMessage, HString &retCode);

	char* GetReturnMessage(){return m_strErrorMessage;};
	char* GetReturnCode(){return m_strErrorCode;};
	long GetLongReturnCode(char* returnCode);
	HString GetMethodName() {return m_methodName;};
	HString GetMethodSignature() {return m_methodSignature;};
	void SetMethodName(HString newName) { m_methodName.Format(L"%s", (BSTR)newName); return;};
	void SetMethodSignature(HString newSignature) { m_methodSignature.Format(L"%s", (BSTR)newSignature); return;};

	HString GetClassName() {return m_className;};
	HString GetClassPath(){return m_classPath;};
	HString GetIsInvalidFonts() { return m_hasInvalidFonts; };
	HString GetIsInvalidPageSize() { return m_hasInvalidPageSize; };
	HString GetSPName() { return m_spName; };
	HString GetBlankParams() { return m_blankParams; };

protected:
	// Constructor
	JavaJNI();

private:
	// Instance
	static JavaJNI* m_pInstance;
	//*** Vars
	static JNIEnv *m_env;
	static JavaVM *m_jvm;
	jboolean jCopy;
	jclass m_Class;
	jobject m_classifierObj;

	HString m_libLocation;
	HString m_jreLocation;
	LPVOID m_lpErrorCode;
	
	HString m_methodName;
	HString m_methodSignature;
	HString m_classPath;
	HString m_className;
	char* m_strErrorMessage;
	char* m_strErrorCode;
	
	void SetReturnMessage(char* text){m_strErrorMessage = text; return;};
	void SetReturnCode(char* code){m_strErrorCode = code; return;};
	HString getDjavaclasspath(HString libLocationa);
	

	// Class specific fnc
	long LoadClass(jclass& m_Class);
	long LoadConstructor(jobject& m_classifierObj, jclass m_class); // constructor will always be: "<init>", "()V"
	void SetJVMClassMethodAndType(HString className /* Default Value will be */  = L"generateReportPackage/GenerateReportImpl", 
		HString javaMethodName /* Default Value will be */ = L"GenerateReport", 
		HString javaMethodSignature /* Includes method type and the type for each paramter*/ = L"([Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
	
	HString m_hasInvalidFonts;
	void SetIsInvalidFonts(char* hasInvalidFonts) { m_hasInvalidFonts.assign(hasInvalidFonts); };
	HString m_hasInvalidPageSize;
	void SetIsInvalidPageSize(char* hasInvalidPageSize) { m_hasInvalidPageSize.assign(hasInvalidPageSize); };
	HString m_spName;
	void SetSPName(char* spName) { m_spName.assign(spName); };
	HString m_blankParams;
	void SetBlankParams(char* blankParams) { m_blankParams.assign(blankParams); };
	
};

#endif