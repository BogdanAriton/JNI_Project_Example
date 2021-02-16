#define JavaJNITool_USE 1
#include "CrystalJavaJNITool.h"

JavaJNI* JavaJNI::m_pInstance = NULL;
JNIEnv* JavaJNI::m_env = NULL;
JavaVM* JavaJNI::m_jvm = NULL;

#define CHECK_HR(HResult, Message) if(FAILED(HResult)){throw std::exception(Message, HResult);}
/*
# Lambdas:
#	javaExceptionCheck - will get the java message for jthrowable exception caught
#
*/
auto javaExceptionCheck = [](JNIEnv* m_env, jthrowable exc, jboolean* jBool)
	{					
		return m_env->GetStringUTFChars((jstring)m_env->CallObjectMethod(exc, m_env->GetMethodID(m_env->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;")), jBool);
	};

/*
#	Scope: Constructor
#		   
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*/
JavaJNI::JavaJNI()
{
	jCopy = false;
	long returnCode = 0;

	HEnvVar env;
	HString installDir = L"";
	env.GET_CSA_VAR_DEFAULT(L"SC", L"INSTALL_DIRECTORY", installDir, L"");
	env.GET_CSA_VAR_DEFAULT(L"HFSOMS",L"CR_JAVA_LIB_LOCATION", m_libLocation,L"");
	if ((m_libLocation.icompare(L"") == 0) || installDir.icompare(L"") == 0)
	{
		HTRACE_MOD(L"OMS", 0, L"Error intializing Java Crystal generator. Contact system administrator.");
		m_lpErrorCode = E_JNI_INIT_FAILED;
		return;
	}
	m_libLocation = installDir + m_libLocation;

	env.GET_CSA_VAR_DEFAULT(L"HFSOMS", L"CR_JAVA_JRE_LOCATION", m_jreLocation, L"");
	if (m_jreLocation.icompare(L"") == 0)
	{
		HTRACE_MOD(L"OMS", 0, L"Error intializing Java Crystal generator. Contact system administrator.");
		m_lpErrorCode = E_JNI_INIT_FAILED;
		return;
	}
	m_jreLocation = installDir + m_jreLocation;

	HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GetInstance() - JVM JRE Location: %s", (BSTR)m_jreLocation)));
	HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GetInstance() - JVM Lib Location: %s", (BSTR)m_libLocation)));
	returnCode = GenerateJVM();
	if (returnCode != 0)
	{
		HTRACE_MOD(L"OMS", 0, L"Error intializing Java Crystal generator. Contact system administrator.");
		m_lpErrorCode = E_JNI_INIT_FAILED;
	}

	m_lpErrorCode = E_JNI_INIT_OK;
}
/*
#	Scope: Initialize local vars
#
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created	
*/
void JavaJNI::SetJVMClassMethodAndType(HString className, HString javaMethodName, HString javaMethodSignature)
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::JavaJNI() - Start Initialize...")));

	m_methodName = javaMethodName;
	m_className = className;
	m_methodSignature = javaMethodSignature;

	if (m_Class == 0)
	{
		if (LoadClass(m_Class) != 0) {return;}
	}

	if (m_classifierObj ==0)
	{
		if (LoadConstructor(m_classifierObj, m_Class) != 0) {return;}
	}

	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::JavaJNI() - End of initialization.")));
}
/*  Just an empty destructor
	JNI will handle releasing all of it local variables
	We will destory all gloabal JNI var as part of DestroyJVM function which can be called on demand if needed
*/

JavaJNI::~JavaJNI(){}

/*
#	Scope: Returns the JNItool instance
#		   
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*/
JavaJNI* JavaJNI::GetInstance()
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GetInstance() - Create JavaJNI tool instance...")));
	HEnvVar env;
	HString isJavaEnabled = L"";
	env.GET_CSA_VAR_DEFAULT(L"HFSOMS",L"USE_JAVA_CRYSTAL_API",isJavaEnabled,L"N");
	if(isJavaEnabled.icompare(L"Y")==0)
	{
		if (m_pInstance == NULL)
		{
			m_pInstance = new JavaJNI(); // Create the instance

			if (JNI_FAILED(m_pInstance->m_lpErrorCode))
			{
				HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GetInstance()- Instance could not be created!")))
					return (JavaJNI *) m_pInstance->m_lpErrorCode;
			}
		}
	}
	HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GetInstance() - Returning JavaJNI Instance!")));
	return m_pInstance;
}

/*
#	Scope: This function will generate the needed JVM with the following options:
#			-Djava.class.path=<m_libLocation> -verbose -verbose:jni
#		   Returns either -1 or 0 depends if there is an error or not
#		   !!This function can only be called once per executable!!
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*/
long JavaJNI::GenerateJVM()
{
	// check if the JVM was created previously
	// only try to generated if the JVM pointer is NULL
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GenerateJVM() - Start!")));
	if (m_jvm == NULL)
	{
		HString library = L"-Djava.library.path=";
		library = library + m_libLocation;

		/* Creating a list of JVM options that will be used to start the JVM */
		JavaVMOption* options = new JavaVMOption[2];
		//-Xms128m -Xmx1024m -Xmaxf0.6 -Xminf0.3 -XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=./heapDump.hprof -XX:+PrintPromotionFailure -XX:PrintFLSStatistics=1 -verbose:gc -XX:+UseConcMarkSweepGC -XX:+CMSClassUnloadingEnabled
		options[0].optionString = getDjavaclasspath(m_libLocation).GetAsciiString(); // -Djava.class.path=ceva.jar;altceva.jar
		options[1].optionString = library.GetAsciiString();
		JavaVMInitArgs vmArgs;
		memset(&vmArgs, 0, sizeof(vmArgs));
		vmArgs.version = JNI_VERSION_1_8;
		vmArgs.nOptions = 2;
		vmArgs.options = options;
		vmArgs.ignoreUnrecognized = false;

		jint status = -1;
		HMODULE hinstLib;
		typedef jint (JNICALL *CreateJavaVM)(JavaVM **, void **, void *);
		CreateJavaVM createJavaVM;

		try
		{
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GenerateJVM() - JVM Location: %s", (BSTR)m_jreLocation)));
			hinstLib = LoadLibrary(m_jreLocation);
			if (hinstLib == NULL)
			{
				m_jvm = NULL;
				m_env = NULL;
				HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GenerateJVM() - Unable to load JVM dll. Error code: %d", (int)status)));

				HString errMsg = L"";
				errMsg.Format(Format(L"Unable to load JVM dll. Exception: %d", (int)status));
				SetReturnMessage(errMsg.GetAsciiString());
				SetReturnCode("-1");
				return -1;
			}
			
			createJavaVM = (CreateJavaVM)GetProcAddress(hinstLib,"JNI_CreateJavaVM");
			if (createJavaVM != NULL)
			{
				status = createJavaVM(&m_jvm, (void**)&m_env, &vmArgs);
			}
			else
			{
				m_jvm = NULL;
				m_env = NULL;
				HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GenerateJVM() - Unable to create pointer to JNI_CreateJavaVM. Error code: %d", (int)status)));

				HString errMsg = L"";
				errMsg.Format(Format(L"Unable to create pointer to JNI_CreateJavaVM. Exception: %d", (int)status));
				SetReturnMessage(errMsg.GetAsciiString());
				SetReturnCode("-1");
				FreeLibrary(hinstLib);
				return -1;
			}
			//jint status = JNI_CreateJavaVM(&m_jvm, (void**)&m_env, &vmArgs);
			delete options;

			if (status == JNI_OK)
			{
				m_Class = 0;
				m_classifierObj = 0;
				SetJVMClassMethodAndType();
				HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GenerateJVM()- JVM has been created.")));
				return 0;
			}
			else if (status == JNI_EVERSION)
			{
				// Pointer could not be created
				m_jvm = NULL;
				m_env = NULL;
				HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GenerateJVM() - JVM is outdated and doesn't meet requirements. Error code: %d", (int)status)));

				HString errMsg = L"";
				errMsg.Format(Format(L"JVM is outdated and doesn't meet requirements. Exception: %d", (int)status));
				SetReturnMessage(errMsg.GetAsciiString());
				SetReturnCode("-3");
				return -1;
			}
			else if (status == JNI_ENOMEM)
			{
				// Pointer could not be created
				m_jvm = NULL;
				m_env = NULL;
				HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GenerateJVM() - not enough memory for JVM. Error code: %d", (int)status)));

				HString errMsg = L"";
				errMsg.Format(Format(L"Not enough memory for JVM. Exception: %d", (int)status));
				SetReturnMessage(errMsg.GetAsciiString());
				SetReturnCode("-1");
				return -4;
			}
			else
			{
				// Pointer could not be created
				m_jvm = NULL;
				m_env = NULL;
				HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GenerateJVM() - An error has occured while trying to create the JVM. Error code: %d", (int)status)));

				HString errMsg = L"";
				errMsg.Format(Format(L"An error has occured while trying to create the JVM. Exception: %d", (int)status));
				SetReturnMessage(errMsg.GetAsciiString());
				SetReturnCode("-1");
				return -1; // unkonw error
			}
		}
		catch (exception e)
		{
			// Pointer could not be created
			m_jvm = NULL;
			m_env = NULL;
			HString exc = L"";
			exc.assign((char*)e.what());
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GenerateJVM() - An error has occured while trying to create the JVM. Exception: %s", (BSTR)exc)));
			
			HString errMsg = L"";
			errMsg.Format(Format(L"An error has occured while trying to create the JVM. Exception: %s", (BSTR)exc));
			SetReturnMessage(errMsg.GetAsciiString());

			SetReturnCode("-1");
			return -1; // unkonw error
		}
		catch(...) // exception
		{
			// Pointer could not be created
			m_jvm = NULL;
			m_env = NULL;
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GenerateJVM() - Unknown error has occured while trying to create the JVM.")));

			SetReturnMessage("Unknown error has occured while trying to create the JVM.");
			SetReturnCode("-1");
			FreeLibrary(hinstLib);
			return -1; // unkonw error
		}
	}
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GenerateJVM() - JVM Exists!")));
	// the jvm exists
	return 0;
}

/*
#	Scope: You can set the:
#		- app log file location
#		- sap log file location
#		- 
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*/ 	
long JavaJNI::SetLogging(HString appLogFileLocation, HString sapLogFileLocation, HString appLogLevel, HString sapLogLevel, HString &retMessage, HString &retCode)
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::SetLogging() - Start")));
	SetJVMClassMethodAndType(L"generateReportPackage/GenerateReportImpl", L"setLogger", L"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
	//JNI
	char* strErrorMessage = "";
	char* strErrorCode = "";
	//Local JNI refs
	jobjectArray resultArray = 0;
	jmethodID methodOpenReport = 0;

	long hR = 0;
	HString hsFieldName = L"";
	HString hsFieldValue = L"";
	try
	{
		if ((m_jvm != NULL) && (m_env != NULL) && (m_Class != 0) && (m_classifierObj !=0))
		{
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::SetLogging() - GetMethod: %s ...", (BSTR)m_methodName)));
			printf(m_methodName.GetAsciiString());
			methodOpenReport = m_env->GetMethodID(m_Class, m_methodName.GetAsciiString(), m_methodSignature.GetAsciiString());
			if (methodOpenReport == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::SetLogging() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}
				else
				{

					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::SetLogging() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}

				m_env->DeleteLocalRef(resultArray);
				return 2;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::SetLogging() - Found method: %s ...", (BSTR)m_methodName)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::SetLogging() - Running method: %s ...", (BSTR)m_methodName)));

			// We make the CallObjectMethod call with the report parameters and the databse info
			// The call will return an array with two values: returnMessage and returnCode

			resultArray = (jobjectArray)(m_env->CallObjectMethod(
				m_classifierObj,			/* This is the constructor */
				methodOpenReport,			
				m_env->NewStringUTF(appLogFileLocation.GetAsciiString()), /* Param: String appLogFileLocation */
				m_env->NewStringUTF(sapLogFileLocation.GetAsciiString()), /* Param: String sapLogFileLocation */
				m_env->NewStringUTF(appLogLevel.GetAsciiString()),		  /* Param: String appLogLevel */
				m_env->NewStringUTF(sapLogLevel.GetAsciiString())         /* Param: String sapLogLevel */
				));			

			if (resultArray == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{					
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::SetLogging() - Exception occurred while trying to run method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("3");
					free(getJavaError);
					m_env->ExceptionClear();

					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s", (BSTR)errorMessage)));
				}
				else
				{
					HString errMsg = L"";
					errMsg.Format(Format(L"Exception occurred while trying to run java method: %s/%s", (BSTR)m_className, (BSTR)m_methodName));
					SetReturnMessage(errMsg.GetAsciiString());
					SetReturnCode("3");

					HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::SetLogging() - Exception occurred while trying to run method: %s/%s.", (BSTR)m_className, (BSTR)m_methodName)));
				}
				m_env->DeleteLocalRef(resultArray);
				return 3;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::SetLogging() - Method: %s Finished!", (BSTR)m_methodName)));

			jstring resultMessage = (jstring) m_env->GetObjectArrayElement(resultArray, 0);
			strErrorMessage = strdup(m_env->GetStringUTFChars( resultMessage, 0));
			retMessage.assign(strErrorMessage);
			free(strErrorMessage);
			//m_env->ReleaseStringUTFChars(resultMessage, strErrorMessage);

			jstring resultCode = (jstring) m_env->GetObjectArrayElement(resultArray, 1);
			strErrorCode = strdup(m_env->GetStringUTFChars( resultCode, 0));
			retCode.assign(strErrorCode);
			free(strErrorCode);
			//m_env->ReleaseStringUTFChars(resultCode, strErrorCode);
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::SetLogging() - Java returned: %s. Return Code: %s.", (BSTR)retMessage, (BSTR)retCode)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::SetLogging() - End of function!")));

			m_env->DeleteLocalRef(resultArray);
			return GetLongReturnCode(retCode.GetAsciiString());
		}
	}
	catch (std::exception& e)
	{
		HString exc = L"";
		exc.assign((char*)e.what());
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::SetLogging() - Standard exception caught. %s", (BSTR)exc)));
		HString temp;
		temp.Format(L"Caught std::exception: %s", (BSTR)exc);

		SetReturnMessage(temp.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(resultArray);
		return 3;
	}
	catch(...)
	{
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::SetLogging() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__)));
		HString errMsg = L"";
		errMsg.Format(L"JavaJNI::SetLogging() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__);
		SetReturnMessage(errMsg.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(resultArray);
		return 3; // We need to decide what to do if we face unknown errors
	}

	// Should never get here, JVM would not be initialized
	return -1;
}

/*
#	Scope: GetSPName
#		   Example: class = generateReportPackage\GenerateReportImpl
#					method = GetMainSPName
#		   
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*   
*/
long JavaJNI::GetMainSPName(HString reportFileName, HString &retMessage, HString &retCode)
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GetMainSPName() - Start: %s/%s!", (BSTR)m_methodName, (BSTR)m_className)));
	SetJVMClassMethodAndType(L"generateReportPackage/GenerateReportImpl", L"GetMainSPName", L"(Ljava/lang/String;)[Ljava/lang/String;");
	//JNI
	char* strErrorMessage = "";
	char* strErrorCode = "";
	//Local JNI refs
	jobjectArray resultArray = 0;
	jmethodID methodOpenReport = 0;

	long hR = 0;
	HString hsFieldName = L"";
	HString hsFieldValue = L"";

	try
	{
		if ((m_jvm != NULL) && (m_env != NULL) && (m_Class != 0) && (m_classifierObj !=0))
		{
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GetMainSPName() - GetMethod: %s ...", (BSTR)m_methodName)));
			methodOpenReport = m_env->GetMethodID(m_Class, m_methodName.GetAsciiString(), m_methodSignature.GetAsciiString());
			if (methodOpenReport == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::GetMainSPName() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}
				else
				{

					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::GetMainSPName() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}

				m_env->DeleteLocalRef(resultArray);
				return 2;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GetMainSPName() - Found method: %s ...", (BSTR)m_methodName)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GetMainSPName() - Running method: %s ...", (BSTR)m_methodName)));

			// We make the CallObjectMethod call with the report parameters and the databse info
			// The call will return an array with two values: returnMessage and returnCode

			resultArray = (jobjectArray)(m_env->CallObjectMethod(
				m_classifierObj,			/* This is the constructor */
				methodOpenReport,			
				m_env->NewStringUTF(reportFileName.GetAsciiString())));			/* Param: String reportFileName */

			if (resultArray == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{					
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::GetMainSPName() - Exception occurred while trying to run method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("3");
					free(getJavaError);
					m_env->ExceptionClear();

					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s", (BSTR)errorMessage)));
				}
				else
				{
					HString errMsg = L"";
					errMsg.Format(Format(L"Exception occurred while trying to run java method: %s/%s", (BSTR)m_className, (BSTR)m_methodName));
					SetReturnMessage(errMsg.GetAsciiString());
					SetReturnCode("3");

					HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GetMainSPName() - Exception occurred while trying to run method: %s/%s.", (BSTR)m_className, (BSTR)m_methodName)));
				}
				m_env->DeleteLocalRef(resultArray);
				return 3;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GetMainSPName() - Method: %s Finished!", (BSTR)m_methodName)));

			jstring resultMessage = (jstring) m_env->GetObjectArrayElement(resultArray, 0);
			strErrorMessage = strdup(m_env->GetStringUTFChars( resultMessage, 0));
			retMessage.assign(strErrorMessage);
			free(strErrorMessage);
			//m_env->ReleaseStringUTFChars(resultMessage, strErrorMessage);

			jstring resultCode = (jstring) m_env->GetObjectArrayElement(resultArray, 1);
			strErrorCode = strdup(m_env->GetStringUTFChars( resultCode, 0));
			retCode.assign(strErrorCode);
			free(strErrorCode);
			//m_env->ReleaseStringUTFChars(resultCode, strErrorCode);
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GetMainSPName() - Java returned: %s. Return Code: %s.", (BSTR)retMessage, (BSTR)retCode)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::GetMainSPName() - End of function!")));

			m_env->DeleteLocalRef(resultArray);
			return GetLongReturnCode(retCode.GetAsciiString());
		}
	}
	catch (std::exception& e)
	{
		HString exc = L"";
		exc.assign((char*)e.what());
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GetMainSPName() - Standard exception caught. %s", (BSTR)exc)));
		HString temp;
		temp.Format(L"Caught std::exception: %s", (BSTR)exc);

		SetReturnMessage(temp.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(resultArray);
		return 3;
	}
	catch(...)
	{
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GetMainSPName() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__)));
		HString errMsg = L"";
		errMsg.Format(L"JavaJNI::RunJavaGenerateReport() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__);
		SetReturnMessage(errMsg.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(resultArray);
		return 3; // We need to decide what to do if we face unknown errors
	}
	// Should never get here, JVM would not be initialized
	return -1;
}

/*
#	Scope: Check for invalid paper size
#		   Example: class = generateReportPackage\GenerateReportImpl
#					method = HasInvalidFont
#		   
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*   
*/
long JavaJNI::CheckInvalidPageSize(HString reportFileName, HString &retMessage, HString &retCode)
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::CheckInvalidPageSize() - Start: %s/%s!", (BSTR)m_methodName, (BSTR)m_className)));
	SetJVMClassMethodAndType(L"generateReportPackage/GenerateReportImpl", L"CheckInvalidPageSize", L"(Ljava/lang/String;)[Ljava/lang/String;");
	//JNI
	char* strErrorMessage = "";
	char* strErrorCode = "";
	//Local JNI refs
	jobjectArray resultArray = 0;
	jmethodID methodOpenReport = 0;

	long hR = 0;
	HString hsFieldName = L"";
	HString hsFieldValue = L"";

	try
	{
		if ((m_jvm != NULL) && (m_env != NULL) && (m_Class != 0) && (m_classifierObj !=0))
		{
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::CheckInvalidPageSize() - GetMethod: %s ...", (BSTR)m_methodName)));
			methodOpenReport = m_env->GetMethodID(m_Class, m_methodName.GetAsciiString(), m_methodSignature.GetAsciiString());
			if (methodOpenReport == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::CheckInvalidPageSize() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}
				else
				{

					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::CheckInvalidPageSize() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}

				m_env->DeleteLocalRef(resultArray);
				return 2;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::CheckInvalidPageSize() - Found method: %s ...", (BSTR)m_methodName)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::CheckInvalidPageSize() - Running method: %s ...", (BSTR)m_methodName)));

			// We make the CallObjectMethod call with the report parameters and the databse info
			// The call will return an array with two values: returnMessage and returnCode

			resultArray = (jobjectArray)(m_env->CallObjectMethod(
				m_classifierObj,			/* This is the constructor */
				methodOpenReport,			
				m_env->NewStringUTF(reportFileName.GetAsciiString())));			/* Param: String reportFileName */

			if (resultArray == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{					
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::CheckInvalidPageSize() - Exception occurred while trying to run method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("3");
					free(getJavaError);
					m_env->ExceptionClear();

					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s", (BSTR)errorMessage)));
				}
				else
				{
					HString errMsg = L"";
					errMsg.Format(Format(L"Exception occurred while trying to run java method: %s/%s", (BSTR)m_className, (BSTR)m_methodName));
					SetReturnMessage(errMsg.GetAsciiString());
					SetReturnCode("3");

					HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::CheckInvalidPageSize() - Exception occurred while trying to run method: %s/%s.", (BSTR)m_className, (BSTR)m_methodName)));
				}
				m_env->DeleteLocalRef(resultArray);
				return 3;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::CheckInvalidPageSize() - Method: %s Finished!", (BSTR)m_methodName)));

			jstring resultMessage = (jstring) m_env->GetObjectArrayElement(resultArray, 0);
			strErrorMessage = strdup(m_env->GetStringUTFChars( resultMessage, 0));
			retMessage.assign(strErrorMessage);
			free(strErrorMessage);
			//m_env->ReleaseStringUTFChars(resultMessage, strErrorMessage);

			jstring resultCode = (jstring) m_env->GetObjectArrayElement(resultArray, 1);
			strErrorCode = strdup(m_env->GetStringUTFChars( resultCode, 0));
			retCode.assign(strErrorCode);
			free(strErrorCode);
			//m_env->ReleaseStringUTFChars(resultCode, strErrorCode);
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::CheckInvalidPageSize() - Java returned: %s. Return Code: %s.", (BSTR)retMessage, (BSTR)retCode)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::CheckInvalidPageSize() - End of function!")));

			m_env->DeleteLocalRef(resultArray);
			return GetLongReturnCode(retCode.GetAsciiString());
		}
	}
	catch (std::exception& e)
	{
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::CheckInvalidPageSize() - Standard exception caught. %s", e.what())));
		HString temp;
		temp.Format(L"Caught std::exception: %s", e.what());

		SetReturnMessage(temp.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(resultArray);
		return 3;
	}
	catch(...)
	{
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::CheckInvalidPageSize() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__)));
		HString errMsg = L"";
		errMsg.Format(L"JavaJNI::CheckInvalidPageSize() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__);
		SetReturnMessage(errMsg.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(resultArray);
		return 3; // We need to decide what to do if we face unknown errors
	}
	// Should never get here, JVM would not be initialized
	return -1;
}

/*
#	Scope: Check for invalid fonts
#		   Example: class = generateReportPackage\GenerateReportImpl
#					method = HasInvalidFont
#		   
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*   
*/
long JavaJNI::HasInvalidFonts(HString reportFileName, HString invalidFonts, HString &retMessage, HString &retCode)
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::HasInvalidFonts() - Start: %s/%s!", (BSTR)m_methodName, (BSTR)m_className)));
	SetJVMClassMethodAndType(L"generateReportPackage/GenerateReportImpl", L"HasInvalidFont", L"(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
	//JNI
	char* strErrorMessage = "";
	char* strErrorCode = "";
	//Local JNI refs
	jobjectArray resultArray = 0;
	jmethodID methodOpenReport = 0;

	long hR = 0;
	HString hsFieldName = L"";
	HString hsFieldValue = L"";

	try
	{
		if ((m_jvm != NULL) && (m_env != NULL) && (m_Class != 0) && (m_classifierObj !=0))
		{
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::HasInvalidFonts() - GetMethod: %s ...", (BSTR)m_methodName)));
			methodOpenReport = m_env->GetMethodID(m_Class, m_methodName.GetAsciiString(), m_methodSignature.GetAsciiString());
			if (methodOpenReport == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::HasInvalidFonts() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}
				else
				{

					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::HasInvalidFonts() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}

				m_env->DeleteLocalRef(resultArray);
				return 2;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::HasInvalidFonts() - Found method: %s ...", (BSTR)m_methodName)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::HasInvalidFonts() - Running method: %s ...", (BSTR)m_methodName)));

			// We make the CallObjectMethod call with the report parameters and the databse info
			// The call will return an array with two values: returnMessage and returnCode

			resultArray = (jobjectArray)(m_env->CallObjectMethod(
				m_classifierObj,			/* This is the constructor */
				methodOpenReport,
				m_env->NewStringUTF(invalidFonts.GetAsciiString()),			/* Param: String invalidFonts */
				m_env->NewStringUTF(reportFileName.GetAsciiString())));			/* Param: String reportFileName */

			if (resultArray == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{					
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::HasInvalidFonts() - Exception occurred while trying to run method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("3");
					free(getJavaError);
					m_env->ExceptionClear();

					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s", (BSTR)errorMessage)));
				}
				else
				{
					HString errMsg = L"";
					errMsg.Format(Format(L"Exception occurred while trying to run java method: %s/%s", (BSTR)m_className, (BSTR)m_methodName));
					SetReturnMessage(errMsg.GetAsciiString());
					SetReturnCode("3");

					HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::HasInvalidFonts() - Exception occurred while trying to run method: %s/%s.", (BSTR)m_className, (BSTR)m_methodName)));
				}
				m_env->DeleteLocalRef(resultArray);
				return 3;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::HasInvalidFonts() - Method: %s Finished!", (BSTR)m_methodName)));

			jstring resultMessage = (jstring) m_env->GetObjectArrayElement(resultArray, 0);
			strErrorMessage = strdup(m_env->GetStringUTFChars( resultMessage, 0));
			retMessage.assign(strErrorMessage);
			free(strErrorMessage);
			//m_env->ReleaseStringUTFChars(resultMessage, strErrorMessage);

			jstring resultCode = (jstring) m_env->GetObjectArrayElement(resultArray, 1);
			strErrorCode = strdup(m_env->GetStringUTFChars( resultCode, 0));
			retCode.assign(strErrorCode);
			free(strErrorCode);
			//m_env->ReleaseStringUTFChars(resultCode, strErrorCode);
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::HasInvalidFonts() - Java returned: %s. Return Code: %s.", (BSTR)retMessage, (BSTR)retCode)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::HasInvalidFonts() - End of function!")));

			m_env->DeleteLocalRef(resultArray);
			return GetLongReturnCode(retCode.GetAsciiString());
		}
	}
	catch (std::exception& e)
	{
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::HasInvalidFonts() - Standard exception caught. %s", e.what())));
		HString temp;
		temp.Format(L"Caught std::exception: %s", e.what());

		SetReturnMessage(temp.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(resultArray);
		return 3;
	}
	catch(...)
	{
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::HasInvalidFonts() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__)));
		HString errMsg = L"";
		errMsg.Format(L"JavaJNI::HasInvalidFonts() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__);
		SetReturnMessage(errMsg.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(resultArray);
		return 3; // We need to decide what to do if we face unknown errors
	}
	// Should never get here, JVM would not be initialized
	return -1;
}

/*
#	Scope: Try to open the report and return the result
#		   Example: class = generateReportPackage\GenerateReportImpl
#					method = Openreport
#		   
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*   
*/
long JavaJNI::OpenReport(HString reportFileName, HString &retMessage, HString &retCode)
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::OpenReport() - Start: %s/%s!", (BSTR)m_methodName, (BSTR)m_className)));
	SetJVMClassMethodAndType(L"generateReportPackage/GenerateReportImpl", L"OpenReport", L"(Ljava/lang/String;)[Ljava/lang/String;");
	//JNI
	char* strErrorMessage = "";
	char* strErrorCode = "";
	//Local JNI refs
	jobjectArray resultArray = 0;
	jmethodID methodOpenReport = 0;

	long hR = 0;
	HString hsFieldName = L"";
	HString hsFieldValue = L"";

	try
	{
		if ((m_jvm != NULL) && (m_env != NULL) && (m_Class != 0) && (m_classifierObj !=0))
		{
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::OpenReport() - GetMethod: %s ...", (BSTR)m_methodName)));
			methodOpenReport = m_env->GetMethodID(m_Class, m_methodName.GetAsciiString(), m_methodSignature.GetAsciiString());
			if (methodOpenReport == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::OpenReport() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}
				else
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::OpenReport() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}

				m_env->DeleteLocalRef(resultArray);
				return 2;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::OpenReport() - Found method: %s ...", (BSTR)m_methodName)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::OpenReport() - Running method: %s ...", (BSTR)m_methodName)));

			// We make the CallObjectMethod call with the report parameters and the databse info
			// The call will return an array with two values: returnMessage and returnCode

			resultArray = (jobjectArray)(m_env->CallObjectMethod(
				m_classifierObj,			/* This is the constructor */
				methodOpenReport,
				m_env->NewStringUTF(reportFileName.GetAsciiString())));			/* Param: String reportFileName */

			if (resultArray == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{					
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::OpenReport() - Exception occurred while trying to run method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("3");
					free(getJavaError);
					m_env->ExceptionClear();

					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s", (BSTR)errorMessage)));
				}
				else
				{
					HString errMsg = L"";
					errMsg.Format(Format(L"Exception occurred while trying to run java method: %s/%s", (BSTR)m_className, (BSTR)m_methodName));
					SetReturnMessage(errMsg.GetAsciiString());
					SetReturnCode("3");

					HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::OpenReport() - Exception occurred while trying to run method: %s/%s.", (BSTR)m_className, (BSTR)m_methodName)));
				}
				m_env->DeleteLocalRef(resultArray);
				return 3;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::OpenReport() - Method: %s Finished!", (BSTR)m_methodName)));

			jstring resultMessage = (jstring) m_env->GetObjectArrayElement(resultArray, 0);
			strErrorMessage = strdup(m_env->GetStringUTFChars( resultMessage, 0));
			retMessage.assign(strErrorMessage);
			free(strErrorMessage);
			//m_env->ReleaseStringUTFChars(resultMessage, strErrorMessage);

			jstring resultCode = (jstring) m_env->GetObjectArrayElement(resultArray, 1);
			strErrorCode = strdup(m_env->GetStringUTFChars( resultCode, 0));
			retCode.assign(strErrorCode);
			free(strErrorCode);
			//m_env->ReleaseStringUTFChars(resultCode, strErrorCode);
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::OpenReport() - Java returned: %s. Return Code: %s.", (BSTR)retMessage, (BSTR)retCode)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::OpenReport() - End of function!")));

			m_env->DeleteLocalRef(resultArray);
			return GetLongReturnCode(retCode.GetAsciiString());
		}
		else
		{
			// JVM was not initialized
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::OpenReport() - JVM is not initialized, please run GenerateJVM() before calling a java method.")));
			SetReturnMessage("JVM is not initialized, please run GenerateJVM() before calling a java method.");
			SetReturnCode("1");
			m_env->DeleteLocalRef(resultArray);
			return 1;
		}
	}
	catch (std::exception& e)
	{
		HString msg; msg.assign((char*)e.what());
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::OpenReport() - Standard exception caught. %s", (BSTR)msg)));
		HString temp;
		temp.Format(L"Caught std::exception: %s", e.what());

		SetReturnMessage(temp.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(resultArray);
		return 3;
	}
	catch(...)
	{
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::OpenReport() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__)));
		HString errMsg = L"";
		errMsg.Format(L"JavaJNI::OpenReport() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__);
		SetReturnMessage(errMsg.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(resultArray);
		return 3; // We need to decide what to do if we face unknown errors
	}
}

/*
#	Scope: Running Export report
#		   Example: class = generateReportPackage\GenerateReportImpl
#					method = GenerateReport
#		   
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*   
*/
long JavaJNI::ExportReport(HQIPtr<IHRecordset> recordSet, HString reportFileName, HString exportFileName, HString dbServerName, HString dbDataBaseName, HString dbUserName, HString dbUserPassword, HString exportFormat, HString &retMessage, HString &retCode)
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Start: %s/%s!", (BSTR)m_methodName, (BSTR)m_className)));
	SetJVMClassMethodAndType(L"generateReportPackage/GenerateReportImpl", L"GenerateReport", L"([Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
	//JNI
	char* strErrorMessage = "";
	char* strErrorCode = "";
	//Local JNI refs
	jobjectArray arrNames = 0;
	jobjectArray arrValues = 0;
	jobjectArray resultArray = 0;
	jmethodID methodGenerateReport = 0;

	long lFieldCount = 0;
	long iResultCode = 0;
	long hR = 0;
	long lRecordCount = 0;
	HString hsFieldName = L"";
	HString hsFieldValue = L"";

	// We cannot continue if JVM was not initialized
	// We cannot continue if the GenerateReportImplClass was not found and the constructor was not loaded
	try
	{
		if ((m_jvm != NULL) && (m_env != NULL) && (m_Class != 0) && (m_classifierObj !=0))
		{
			if (recordSet != NULL)
			{
				hR = recordSet->MoveFirstRecordset();CHECK_HR(hR, "recordSet->MoveRecordSet Failed");
				hR = recordSet->MoveFirst();CHECK_HR(hR, "recordSet->MoveFirst Failed");
				hR = recordSet->GetFieldCount(&lFieldCount);CHECK_HR(hR, "recordSet->GetFieldCount Failed");
				// Initialize param argument names and values
				arrNames = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""
				arrValues = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""

				int i = 0;
				HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Load parameters for report: %s ...", (BSTR)reportFileName)));
				for (long lCurrentField = 1; lCurrentField <= lFieldCount; lCurrentField++)
				{
					recordSet->GetStringFieldAt(lCurrentField,&hsFieldName, &hsFieldValue);
					// TO DO - Uncomment the line below
					hsFieldName = L"@" + hsFieldName;
					m_env->SetObjectArrayElement( arrNames, i,  m_env->NewStringUTF(hsFieldName.GetAsciiString()));
					m_env->SetObjectArrayElement( arrValues, i, m_env->NewStringUTF(hsFieldValue.GetAsciiString()));
					
					HTRACE_MOD (L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() found parameter: [%s] = [%s]", (BSTR)hsFieldName, (BSTR)hsFieldValue)));
					i++;
				}

			}
			else
			{
				// There is nothing in the recordset
				// Initialize param argument names and values
				arrNames = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""
				arrValues = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""

				// will send in empty string arrays
			}

			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - GetMethod: %s ...", (BSTR)m_methodName)));
			methodGenerateReport = m_env->GetMethodID(m_Class, m_methodName.GetAsciiString(), m_methodSignature.GetAsciiString());
			if (methodGenerateReport == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::RunJavaGenerateReport() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}
				else
				{
					
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::RunJavaGenerateReport() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}

				m_env->DeleteLocalRef(arrNames);
				m_env->DeleteLocalRef(arrValues);
				m_env->DeleteLocalRef(resultArray);
				return 2;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Found method: %s ...", (BSTR)m_methodName)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Running method: %s ...", (BSTR)m_methodName)));

			// We make the CallObjectMethod call with the report parameters and the databse info
			// The call will return an array with two values: returnMessage and returnCode
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Generate Report: %s ...", (BSTR)reportFileName)));
			resultArray = (jobjectArray)(m_env->CallObjectMethod(
				m_classifierObj,			/* This is the constructor */
				methodGenerateReport,		/* This is the class object already loaded */
				arrNames,					/* Param: String[] paramNames */
				arrValues,					/* Param: String[] paramValues */
				m_env->NewStringUTF(reportFileName.GetAsciiString()),			/* Param: String reportFileName */
				m_env->NewStringUTF(exportFileName.GetAsciiString()),			/* Param: String exportFileName */
				m_env->NewStringUTF(dbServerName.GetAsciiString()),				/* Param: String dbServerName */
				m_env->NewStringUTF(dbDataBaseName.GetAsciiString()),			/* Param: String dbDataBaseName */
				m_env->NewStringUTF(dbUserName.GetAsciiString()),				/* Param: String dbUserName */
				m_env->NewStringUTF(dbUserPassword.GetAsciiString()),			/* Param: String dbUserPassword */
				m_env->NewStringUTF(exportFormat.GetAsciiString())));			/* Param: String exportFormat */
			
			if (resultArray == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{					
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::RunJavaGenerateReport() - Exception occurred while trying to run method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("3");
					free(getJavaError);
					m_env->ExceptionClear();

					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s", (BSTR)errorMessage)));
				}
				else
				{
					HString errMsg = L"";
					errMsg.Format(Format(L"Exception occurred while trying to run java method: %s/%s", (BSTR)m_className, (BSTR)m_methodName));
					SetReturnMessage(errMsg.GetAsciiString());
					SetReturnCode("3");

					HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Exception occurred while trying to run method: %s/%s.", (BSTR)m_className, (BSTR)m_methodName)));
				}
				m_env->DeleteLocalRef(arrNames);
				m_env->DeleteLocalRef(arrValues);
				m_env->DeleteLocalRef(resultArray);
				return 3;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Method: %s Finished!", (BSTR)m_methodName)));

			jstring resultMessage = (jstring) m_env->GetObjectArrayElement(resultArray, 0);
			strErrorMessage = strdup(m_env->GetStringUTFChars( resultMessage, 0));
			retMessage.assign(strErrorMessage);
			free(strErrorMessage);
			//m_env->ReleaseStringUTFChars(resultMessage, strErrorMessage);

			jstring resultCode = (jstring) m_env->GetObjectArrayElement(resultArray, 1);
			strErrorCode = strdup(m_env->GetStringUTFChars( resultCode, 0));
			retCode.assign(strErrorCode);
			free(strErrorCode);
			//m_env->ReleaseStringUTFChars(resultCode, strErrorCode);

			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Java returned: %s. Return Code: %s.", (BSTR)retMessage, (BSTR)retCode)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - End of function!")));

			m_env->DeleteLocalRef(arrNames);
			m_env->DeleteLocalRef(arrValues);
			m_env->DeleteLocalRef(resultArray);
			//ResetResources();
			return GetLongReturnCode(retCode.GetAsciiString());
		}
		else
		{
			SetReturnMessage("JVM is not initialized, please run GenerateJVM() before calling a java method.");
			SetReturnCode("1");
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::RunJavaGenerateReport() - JVM is not initialized, please run GenerateJVM() before calling a java method.")));
			m_env->DeleteLocalRef(arrNames);
			m_env->DeleteLocalRef(arrValues);
			m_env->DeleteLocalRef(resultArray);
			return 1;
		}
	}
	catch (std::exception& e)
	{
		HString msg; msg.assign((char*)e.what());
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Standard exception caught. %s", (BSTR)msg)));
		HString temp;
		temp.Format(L"Caught std::exception: %s", e.what());

		SetReturnMessage(temp.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(arrNames);
		m_env->DeleteLocalRef(arrValues);
		m_env->DeleteLocalRef(resultArray);
		return 3;
	}
	catch(...)
	{
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__)));
		HString errMsg = L"";
		errMsg.Format(L"JavaJNI::RunJavaGenerateReport() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__);
		SetReturnMessage(errMsg.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(arrNames);
		m_env->DeleteLocalRef(arrValues);
		m_env->DeleteLocalRef(resultArray);
		return 3; // We need to decide what to do if we face unknown errors
	}
}

/*
#	Scope: Running Export report along with all the entire check for performance improvement trying to limit the number of JNI calls to 1
#		   Example: class = generateReportPackage\GenerateReportImpl
#					method = GenerateReport
#
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	14/01/19	Created
*
*/
long JavaJNI::RunAllGenerateReport(HQIPtr<IHRecordset> recordSet, HString reportFileName, HString exportFileName, HString dbServerName, HString dbDataBaseName, HString dbUserName, HString dbUserPassword, HString exportFormat,
	HString invalidFonts, HString appLogFileLocation, HString sapLogFileLocation, HString appLogLevel, HString sapLogLevel, HString isLocalPrintorPreview, HString &retMessage, HString &retCode)
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Start: %s/%s!", (BSTR)m_methodName, (BSTR)m_className)));
	SetJVMClassMethodAndType(L"generateReportPackage/GenerateReportImpl", L"RunAllGenerateReport", 
		L"([Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
	//JNI
	char* strErrorMessage = "";
	char* strErrorCode = "";
	//Local JNI refs
	jobjectArray arrNames = 0;
	jobjectArray arrValues = 0;
	jobjectArray resultArray = 0;
	jmethodID methodGenerateReport = 0;

	long lFieldCount = 0;
	long iResultCode = 0;
	long hR = 0;
	long lRecordCount = 0;
	HString hsFieldName = L"";
	HString hsFieldValue = L"";

	// We cannot continue if JVM was not initialized
	// We cannot continue if the GenerateReportImplClass was not found and the constructor was not loaded
	try
	{
		if ((m_jvm != NULL) && (m_env != NULL) && (m_Class != 0) && (m_classifierObj != 0))
		{
			if (recordSet != NULL)
			{
				hR = recordSet->MoveFirstRecordset(); CHECK_HR(hR, "recordSet->MoveRecordSet Failed");
				hR = recordSet->MoveFirst(); CHECK_HR(hR, "recordSet->MoveFirst Failed");
				hR = recordSet->GetFieldCount(&lFieldCount); CHECK_HR(hR, "recordSet->GetFieldCount Failed");
				// Initialize param argument names and values
				arrNames = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""
				arrValues = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""

				int i = 0;
				HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Load parameters for report: %s ...", (BSTR)reportFileName)));
				for (long lCurrentField = 1; lCurrentField <= lFieldCount; lCurrentField++)
				{
					recordSet->GetStringFieldAt(lCurrentField, &hsFieldName, &hsFieldValue);
					// TO DO - Uncomment the line below
					hsFieldName = L"@" + hsFieldName;
					m_env->SetObjectArrayElement(arrNames, i, m_env->NewStringUTF(hsFieldName.GetAsciiString()));
					m_env->SetObjectArrayElement(arrValues, i, m_env->NewStringUTF(hsFieldValue.GetAsciiString()));

					HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() found parameter: [%s] = [%s]", (BSTR)hsFieldName, (BSTR)hsFieldValue)));
					i++;
				}
			}
			else
			{
				// There is nothing in the recordset
				// Initialize param argument names and values
				arrNames = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""
				arrValues = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""

				// will send in empty string arrays
			}

			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - GetMethod: %s ...", (BSTR)m_methodName)));
			methodGenerateReport = m_env->GetMethodID(m_Class, m_methodName.GetAsciiString(), m_methodSignature.GetAsciiString());
			if (methodGenerateReport == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L""; HString getJavaErrorH = L""; getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::RunJavaGenerateReport() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}
				else
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L""; HString getJavaErrorH = L""; getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::RunJavaGenerateReport() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}

				m_env->DeleteLocalRef(arrNames);
				m_env->DeleteLocalRef(arrValues);
				m_env->DeleteLocalRef(resultArray);
				return 2;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Found method: %s ...", (BSTR)m_methodName)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Running method: %s ...", (BSTR)m_methodName)));

			// We make the CallObjectMethod call with the report parameters and the databse info
			// The call will return an array with two values: returnMessage and returnCode
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Generate Report: %s ...", (BSTR)reportFileName)));
			resultArray = (jobjectArray)(m_env->CallObjectMethod(
				m_classifierObj,												/* This is the constructor */
				methodGenerateReport,											/* This is the class object already loaded */
				arrNames,														/* Param: String[] paramNames */
				arrValues,														/* Param: String[] paramValues */
				m_env->NewStringUTF(reportFileName.GetAsciiString()),			/* Param: String reportFileName */
				m_env->NewStringUTF(exportFileName.GetAsciiString()),			/* Param: String exportFileName */
				m_env->NewStringUTF(dbServerName.GetAsciiString()),				/* Param: String dbServerName */
				m_env->NewStringUTF(dbDataBaseName.GetAsciiString()),			/* Param: String dbDataBaseName */
				m_env->NewStringUTF(dbUserName.GetAsciiString()),				/* Param: String dbUserName */
				m_env->NewStringUTF(dbUserPassword.GetAsciiString()),			/* Param: String dbUserPassword */
				m_env->NewStringUTF(exportFormat.GetAsciiString()),				/* Param: String exportFormat */
				m_env->NewStringUTF(invalidFonts.GetAsciiString()),				/* Param: String invalidFonts */
				m_env->NewStringUTF(appLogFileLocation.GetAsciiString()),		/* Param: String appLogFileLocation */
				m_env->NewStringUTF(sapLogFileLocation.GetAsciiString()),		/* Param: String sapLogFileLocation */
				m_env->NewStringUTF(appLogLevel.GetAsciiString()),				/* Param: String appLogLevel */
				m_env->NewStringUTF(sapLogLevel.GetAsciiString()),				/* Param: String sapLogLevel */
				m_env->NewStringUTF(isLocalPrintorPreview.GetAsciiString())));	/* Param: String isLocalPrintorPreview */

			if (resultArray == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L""; HString getJavaErrorH = L""; getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::RunJavaGenerateReport() - Exception occurred while trying to run method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("3");
					free(getJavaError);
					m_env->ExceptionClear();

					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s", (BSTR)errorMessage)));
				}
				else
				{
					HString errMsg = L"";
					errMsg.Format(Format(L"Exception occurred while trying to run java method: %s/%s", (BSTR)m_className, (BSTR)m_methodName));
					SetReturnMessage(errMsg.GetAsciiString());
					SetReturnCode("3");

					HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Exception occurred while trying to run method: %s/%s.", (BSTR)m_className, (BSTR)m_methodName)));
				}
				m_env->DeleteLocalRef(arrNames);
				m_env->DeleteLocalRef(arrValues);
				m_env->DeleteLocalRef(resultArray);
				return 3;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Method: %s Finished!", (BSTR)m_methodName)));

			jstring resultMessage = (jstring)m_env->GetObjectArrayElement(resultArray, 0);
			strErrorMessage = strdup(m_env->GetStringUTFChars(resultMessage, 0));
			retMessage.assign(strErrorMessage);
			free(strErrorMessage);

			jstring resultCode = (jstring)m_env->GetObjectArrayElement(resultArray, 1);
			strErrorCode = strdup(m_env->GetStringUTFChars(resultCode, 0));
			retCode.assign(strErrorCode);
			free(strErrorCode);

			char* strIsInvalidFont = "";
			jstring resultIsInvalidFont = (jstring)m_env->GetObjectArrayElement(resultArray, 2); // invalid fonts
			strIsInvalidFont = strdup(m_env->GetStringUTFChars(resultIsInvalidFont, 0));
			SetIsInvalidFonts(strIsInvalidFont);
			free(strIsInvalidFont);

			char* strIsInvalidPageSize = "";
			jstring resulIsInvalidPageSize = (jstring)m_env->GetObjectArrayElement(resultArray, 3); // invalid page size
			strIsInvalidPageSize = strdup(m_env->GetStringUTFChars(resulIsInvalidPageSize, 0));
			SetIsInvalidPageSize(strIsInvalidPageSize);
			free(strIsInvalidPageSize);

			char* strSPName = "";
			jstring resultSPName = (jstring)m_env->GetObjectArrayElement(resultArray, 4); // sp name
			strSPName = strdup(m_env->GetStringUTFChars(resultSPName, 0));
			SetSPName(strSPName);
			free(strSPName);

			char* strBlankParams = "";
			jstring resultBlankParams = (jstring)m_env->GetObjectArrayElement(resultArray, 5); // blank params
			strBlankParams = strdup(m_env->GetStringUTFChars(resultBlankParams, 0));
			SetBlankParams(strBlankParams);
			free(strBlankParams);

			m_env->DeleteLocalRef(arrNames);
			m_env->DeleteLocalRef(arrValues);
			m_env->DeleteLocalRef(resultArray);
			//ResetResources();
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Java returned: %s. Return Code: %s.", (BSTR)retMessage, (BSTR)retCode)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::RunJavaGenerateReport() - End of function!")));

			return GetLongReturnCode(retCode.GetAsciiString());
		}
		else
		{
			SetReturnMessage("JVM is not initialized, please run GenerateJVM() before calling a java method.");
			SetReturnCode("1");
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::RunJavaGenerateReport() - JVM is not initialized, please run GenerateJVM() before calling a java method.")));
			m_env->DeleteLocalRef(arrNames);
			m_env->DeleteLocalRef(arrValues);
			m_env->DeleteLocalRef(resultArray);
			return 1;
		}
	}
	catch (std::exception& e)
	{
		HString msg; msg.assign((char*)e.what());
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Standard exception caught. %s", (BSTR)msg)));
		HString temp;
		temp.Format(L"Caught std::exception: %s", e.what());

		SetReturnMessage(temp.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(arrNames);
		m_env->DeleteLocalRef(arrValues);
		m_env->DeleteLocalRef(resultArray);
		return 3;
	}
	catch (...)
	{
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__)));
		HString errMsg = L"";
		errMsg.Format(L"JavaJNI::RunJavaGenerateReport() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__);
		SetReturnMessage(errMsg.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(arrNames);
		m_env->DeleteLocalRef(arrValues);
		m_env->DeleteLocalRef(resultArray);
		return 3; // We need to decide what to do if we face unknown errors
	}
}

/*
#	Scope: We can separatly run set paramters and get the entire list of paramters needed for the report
#		   Example: class = generateReportPackage\GenerateReportImpl
#					method = GenerateReport
#		   
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*   
*/
long JavaJNI::setParametersAndDB(HQIPtr<IHRecordset> recordSet, HString reportFileName, HString dbServerName, HString dbDataBaseName, HString dbUserName, HString dbUserPassword, HString &retMessage, HString &retCode)
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::setParametersAndDB() - Start: %s/%s!", (BSTR)m_methodName, (BSTR)m_className)));
	SetJVMClassMethodAndType(L"generateReportPackage/GenerateReportImpl", L"setParametersAndDB", L"([Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
	//JNI
	char* strErrorMessage = "";
	char* strErrorCode = "";
	//Local JNI refs
	jobjectArray arrNames = 0;
	jobjectArray arrValues = 0;
	jobjectArray resultArray = 0;
	jmethodID methodGenerateReport = 0;

	long lFieldCount = 0;
	long iResultCode = 0;
	long hR = 0;
	long lRecordCount = 0;
	HString hsFieldName = L"";
	HString hsFieldValue = L"";

	// We cannot continue if JVM was not initialized
	// We cannot continue if the GenerateReportImplClass was not found and the constructor was not loaded
	try
	{
		if ((m_jvm != NULL) && (m_env != NULL) && (m_Class != 0) && (m_classifierObj !=0))
		{
			if (recordSet != NULL)
			{
				hR = recordSet->MoveFirstRecordset();CHECK_HR(hR, "recordSet->MoveRecordSet Failed");
				hR = recordSet->MoveFirst();CHECK_HR(hR, "recordSet->MoveFirst Failed");
				hR = recordSet->GetFieldCount(&lFieldCount);CHECK_HR(hR, "recordSet->GetFieldCount Failed");
				// Initialize param argument names and values
				arrNames = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""
				arrValues = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""

				int i = 0;
				for (long lCurrentField = 1; lCurrentField <= lFieldCount; lCurrentField++)
				{
					recordSet->GetStringFieldAt(lCurrentField,&hsFieldName, &hsFieldValue);
					// TO DO - Uncomment the line below
					hsFieldName = L"@" + hsFieldName;
					m_env->SetObjectArrayElement( arrNames, i,  m_env->NewStringUTF(hsFieldName.GetAsciiString()));
					m_env->SetObjectArrayElement( arrValues, i, m_env->NewStringUTF(hsFieldValue.GetAsciiString()));
					
					HTRACE_MOD (L"OMS", 2, FORMAT((L"JavaJNI::setParametersAndDB() found parameter: [%s] = [%s]", (BSTR)hsFieldName, (BSTR)hsFieldValue)));
					i++;
				}

			}
			else
			{
				// There is nothing in the recordset
				// Initialize param argument names and values
				arrNames = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""
				arrValues = m_env->NewObjectArray(lFieldCount, m_env->FindClass("java/lang/String"), m_env->NewStringUTF("")); // New string array each value initialized with ""

				// will send in empty string arrays
			}

			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::setParametersAndDB() - GetMethod: %s ...", (BSTR)m_methodName)));
			methodGenerateReport = m_env->GetMethodID(m_Class, m_methodName.GetAsciiString(), m_methodSignature.GetAsciiString());
			if (methodGenerateReport == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::setParametersAndDB() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}
				else
				{
					
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::setParametersAndDB() - Exception occurred while trying to open method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("2");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
				}

				m_env->DeleteLocalRef(arrNames);
				m_env->DeleteLocalRef(arrValues);
				m_env->DeleteLocalRef(resultArray);
				return 2;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::setParametersAndDB() - Found method: %s ...", (BSTR)m_methodName)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::setParametersAndDB() - Running method: %s ...", (BSTR)m_methodName)));

			// We make the CallObjectMethod call with the report parameters and the databse info
			// The call will return an array with two values: returnMessage and returnCode
	
			resultArray = (jobjectArray)(m_env->CallObjectMethod(
				m_classifierObj,			/* This is the constructor */
				methodGenerateReport,		/* This is the class object already loaded */
				arrNames,					/* Param: String[] paramNames */
				arrValues,					/* Param: String[] paramValues */
				m_env->NewStringUTF(reportFileName.GetAsciiString()),			/* Param: String reportFileName */
				m_env->NewStringUTF(dbServerName.GetAsciiString()),				/* Param: String dbServerName */
				m_env->NewStringUTF(dbDataBaseName.GetAsciiString()),			/* Param: String dbDataBaseName */
				m_env->NewStringUTF(dbUserName.GetAsciiString()),				/* Param: String dbUserName */
				m_env->NewStringUTF(dbUserPassword.GetAsciiString())));			/* Param: String dbUserPassword */
			
			if (resultArray == 0)
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{					
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::setParametersAndDB() - Exception occurred while trying to run method: %s/%s.! Message = [%s]", (BSTR)m_className, (BSTR)m_methodName, (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("3");
					free(getJavaError);
					m_env->ExceptionClear();

					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s", (BSTR)errorMessage)));
				}
				else
				{
					HString errMsg = L"";
					errMsg.Format(Format(L"Exception occurred while trying to run java method: %s/%s", (BSTR)m_className, (BSTR)m_methodName));
					SetReturnMessage(errMsg.GetAsciiString());
					SetReturnCode("3");

					HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::setParametersAndDB() - Exception occurred while trying to run method: %s/%s.", (BSTR)m_className, (BSTR)m_methodName)));
				}
				m_env->DeleteLocalRef(arrNames);
				m_env->DeleteLocalRef(arrValues);
				m_env->DeleteLocalRef(resultArray);
				return 3;
			}
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::setParametersAndDB() - Method: %s Finished!", (BSTR)m_methodName)));

			jstring resultMessage = (jstring) m_env->GetObjectArrayElement(resultArray, 0);
			strErrorMessage = strdup(m_env->GetStringUTFChars( resultMessage, 0));
			retMessage.assign(strErrorMessage);
			free(strErrorMessage);
			//m_env->ReleaseStringUTFChars(resultMessage, strErrorMessage);

			jstring resultCode = (jstring) m_env->GetObjectArrayElement(resultArray, 1);
			strErrorCode = strdup(m_env->GetStringUTFChars( resultCode, 0));
			retCode.assign(strErrorCode);
			free(strErrorCode);
			//m_env->ReleaseStringUTFChars(resultCode, strErrorCode);

			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::setParametersAndDB() - Java returned: %s. Return Code: %s.", (BSTR)retMessage, (BSTR)retCode)));
			HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::setParametersAndDB() - End of function!")));

			m_env->DeleteLocalRef(arrNames);
			m_env->DeleteLocalRef(arrValues);
			m_env->DeleteLocalRef(resultArray);
			//ResetResources();
			return GetLongReturnCode(retCode.GetAsciiString());
		}
		else
		{
			SetReturnMessage("JVM is not initialized, please run GenerateJVM() before calling a java method.");
			SetReturnCode("1");
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::setParametersAndDB() - JVM is not initialized, please run GenerateJVM() before calling a java method.")));
			m_env->DeleteLocalRef(arrNames);
			m_env->DeleteLocalRef(arrValues);
			m_env->DeleteLocalRef(resultArray);
			return 1;
		}
	}
	catch (std::exception& e)
	{
		HString msg; msg.assign((char*)e.what());
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::setParametersAndDB() - Standard exception caught. %s", (BSTR)msg)));
		HString temp;
		temp.Format(L"Caught std::exception: %s", e.what());

		SetReturnMessage(temp.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(arrNames);
		m_env->DeleteLocalRef(arrValues);
		m_env->DeleteLocalRef(resultArray);
		return 3;
	}
	catch(...)
	{
		HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::setParametersAndDB() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__)));
		HString errMsg = L"";
		errMsg.Format(L"JavaJNI::setParametersAndDB() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]", __LINE__);
		SetReturnMessage(errMsg.GetAsciiString());
		SetReturnCode("3");
		m_env->DeleteLocalRef(arrNames);
		m_env->DeleteLocalRef(arrValues);
		m_env->DeleteLocalRef(resultArray);
		return 3; // We need to decide what to do if we face unknown errors
	}
}

/*
#	Scope: We need to destroy the JVM instance if we don't need it anymore
#		   
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*/
long JavaJNI::DestroyJVM()
{
	// Destroy also global references
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::DestroyJVM() - JVM is being destroyed...")));

	if (m_jvm != NULL)
	{
		try
		{
			jint result = 0;
			result = m_jvm->DetachCurrentThread();
			result = m_jvm->DestroyJavaVM();
			if (result != JNI_ERR)
			{
				m_jvm = NULL;delete m_jvm;
				m_env = NULL;delete m_env;
				HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::DestroyJVM() - JVM has been destroyed.")));
				SetReturnMessage("JVM has been destroyed.");
				SetReturnCode("0");
				return 0;
			}
			else
			{
				jthrowable exc = m_env->ExceptionOccurred();
				if (exc)
				{
					char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
					HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
					errorMessage.Format(L"JavaJNI::DestroyJVM() - An error in JNI has occured, unable to destroy JVM! Message! Message = [%s]", (BSTR)getJavaErrorH);
					SetReturnMessage(errorMessage.GetAsciiString());
					SetReturnCode("-1");
					free(getJavaError);
					m_env->ExceptionClear();
					HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
					return -1;
				}
				else
				{
					HTRACE_MOD(L"OMS", 1, FORMAT((L"JavaJNI::DestroyJVM() - An error in JNI has occured, unable to destroy JVM!")));
					SetReturnMessage("An error in JNI has occured, unable to destroy JVM!");
					SetReturnCode("-1");
					return -1;
				}
			}
		}
		catch (...)
		{
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::RunJavaGenerateReport() - Unknown exception was caught! File: [CrystalJNITool.cpp] Line: [%d]",  __LINE__)));
			HString errMsg = L"";
			errMsg.Format(L"JavaJNI::RunJavaGenerateReport() - Unknown exception, outside of JNI Calls! File: [CrystalJNITool.cpp] Line: [%d]",  __LINE__);
			SetReturnMessage(errMsg.GetAsciiString());
			SetReturnCode("-2");
			return -2; // We need to decide what to do if we face unknown errors
		}
	}
	else
	{
		HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::DestroyJVM() - JVM has not been initialized, no need to be destroyed.")));
		SetReturnMessage("JVM has not been initialized, no need to be destroyed.");
		SetReturnCode("0");
		return 0;
	}
}

/*
#	Scope: Takes the location that should point to the folder where all the necessary libs are found
#		   and reads all the files to create a string that will look like: -Djava.class.path=some.jar;another.jar;etc.jar
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*/
HString JavaJNI::getDjavaclasspath(HString folder)
{
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::getDjavaclasspath() - Create Djava.class.path from the lib location: %s", (BSTR)folder)));
	m_classPath = L"";
	HString files = L"-Djava.class.path="; //-Xms128m -Xmx1024m -Xmaxf0.6 -Xminf0.3 -XX:HeapDumpPath=path -XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=./heapDump.hprof
	WIN32_FIND_DATA fd;

	HString folderSearch = folder + L"\\*.*";
	HANDLE hFind = ::FindFirstFile((LPCWSTR)folderSearch, &fd);

	if(hFind != INVALID_HANDLE_VALUE) 
	{
		do
		{
			if(! (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
			{
				files = files + folder + L"\\" + fd.cFileName + L";"; // -Djava.class.path=C:\temp\my1jar.jar;C:\temp\my2jar.jar....
			}
		}
		while (::FindNextFile(hFind, &fd));
		m_classPath = files;
	}

	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::getDjavaclasspath() - -Djava.class.path=%s.", (BSTR)files)));
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::getDjavaclasspath() - End of function.")));
	return files;
}

/*
#	Scope: Find Class Wrapper with exception check
#
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*/
long JavaJNI::LoadClass(jclass& m_Class)
{
	// We want to try and reset the class only if it's the first time or if it's a new name
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::LoadClass() - Loading Class: %s ...", (BSTR)m_className)));
	
	jclass tempClass = m_env->FindClass(m_className.GetAsciiString());
	if (tempClass == 0)
	{
		jthrowable exc = m_env->ExceptionOccurred();
		if (exc)
		{
			char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
			HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
			errorMessage.Format(L"JavaJNI::LoadClass() - Exception occurred while trying to open class! Message = [%s]", (BSTR)getJavaErrorH);
			SetReturnMessage(errorMessage.GetAsciiString());
			SetReturnCode("-1");
			free(getJavaError);
			m_env->ExceptionClear();
			HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
			return -1;
		}
		else
		{
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::LoadClass() - Exception occurred while trying to FindClass: %s.", (BSTR)m_className)));
			m_strErrorMessage = "JavaJNI::LoadClass() - Exception occurred while trying to FindClass";
			SetReturnCode("-1");
			return -1;
		}

	}

	m_Class = (jclass)m_env->NewGlobalRef(tempClass);
	if (m_Class == 0)
	{
		jthrowable exc = m_env->ExceptionOccurred();
		if (exc)
		{
			char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
			HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
			errorMessage.Format(L"JavaJNI::LoadClass() - Exception occurred while trying to open class! Message = [%s]", (BSTR)getJavaErrorH);
			SetReturnMessage(errorMessage.GetAsciiString());
			SetReturnCode("-1");
			free(getJavaError);
			m_env->ExceptionClear();
			HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
			return -1;
		}
		else
		{
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::LoadClass() - Exception occurred while trying to create global reference class: %s.", (BSTR)m_className)));
			HString errMsg = L"";
			errMsg.Format(Format(L"JavaJNI::LoadClass() - Exception occurred while trying to create global reference class: %s.", (BSTR)m_className));
			SetReturnMessage(errMsg.GetAsciiString());
			SetReturnCode("-1");
			return -1;
		}
	}

	m_env->DeleteLocalRef(tempClass);
	HTRACE_MOD(L"OMS", 2, FORMAT((L"JavaJNI::LoadClass() - Class: %s Loaded ...", (BSTR)m_className)));

	return 0;
}

/*
#	Scope: Load Class Constructor Wrapper with exception check
#	This should only be called from generate JVM function and will not run unless java class is loaded
#	Author: Bogdan Ariton
#	History:
#	-----------------------------------
#	Author			Date		Reason:
#	Bogdan Ariton	24/05/17	Created
*/
long JavaJNI::LoadConstructor(jobject& m_classifierObj, jclass m_Class)
{
	HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::LoadConstructor() - Load Java Constructor...")));

	jmethodID m_Constructor = m_env->GetMethodID(m_Class,"<init>", "()V");
	if (m_Constructor == 0)
	{
		jthrowable exc = m_env->ExceptionOccurred();
		if (exc)
		{
			char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
			HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
			errorMessage.Format(L"JavaJNI::LoadConstructor() - Unable to load Java Constructor.! Message = [%s]", (BSTR)getJavaErrorH);
			SetReturnMessage(errorMessage.GetAsciiString());
			SetReturnCode("-1");
			free(getJavaError);
			m_env->ExceptionClear();
			HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
			return -1;
		}
		else
		{
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::LoadConstructor() - Unable to load Java Constructor.")));
			SetReturnMessage("Unable to load Java Constructor.");
			SetReturnCode("-1");
			return -1;
		}
	}

	m_classifierObj = m_env->NewGlobalRef(m_env->NewObject(m_Class, m_Constructor));
	if (m_classifierObj == 0)
	{
		jthrowable exc = m_env->ExceptionOccurred();
		if (exc)
		{
			char* getJavaError = strdup(javaExceptionCheck(m_env, exc, &jCopy));
			HString errorMessage = L"";HString getJavaErrorH = L"";getJavaErrorH.assign(getJavaError);
			errorMessage.Format(L"JavaJNI::LoadConstructor() - Unable to load Java Constructor.! Message = [%s]", (BSTR)getJavaErrorH);
			SetReturnMessage(errorMessage.GetAsciiString());
			SetReturnCode("-1");
			free(getJavaError);
			m_env->ExceptionClear();
			HTRACE_MOD(L"OMS", 0, FORMAT((L"%s.", (BSTR)errorMessage)));
			return -1;
		}
		else
		{
			HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::GenerateJVM() - Unable to load Java Constructor.")));
			SetReturnMessage("Unable to load Java Constructor.");
			SetReturnCode("-1");
			return -1;
		}
	}
	m_Constructor = 0;
	HTRACE_MOD(L"OMS", 0, FORMAT((L"JavaJNI::LoadConstructor() - Load Java Constructor loaded.")));
	return 0;
}

long JavaJNI::GetLongReturnCode(char* returnCode)
{
	if (strlen(returnCode) > 0)
	{
		const char *s = returnCode;
		std::string str(s);
		return std::stol(str);
	}
	else
		return 0;
}