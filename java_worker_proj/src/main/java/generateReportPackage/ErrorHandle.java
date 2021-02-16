package generateReportPackage;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

import com.crystaldecisions.sdk.occa.report.lib.ReportSDKException;

import org.apache.log4j.Logger;

public class ErrorHandle {
	static final Logger logger = Logger.getLogger(GenerateReportImpl.class);
	private static final List<String> OpenReportIssuesList = Arrays.asList("failed to load the report");

	private static final List<String> knownDatabaseIssuesList = Arrays.asList("Logon Error: Cannot open database",
			"Login failed for user", "Cannot open database", "SQL Server does not exist");

	/**
	 * Private constructor
	 */
	private ErrorHandle() {
	}

	/**
	 * Will check the list of known issues so that we know how to treat the
	 * errors on the C++ layer
	 * 
	 * @param rse
	 * @return String[] - the error message and code
	 */
	public static String[] checkForKnownIssues(ReportSDKException rse) {
		String[] stackReturn = new String[2];
		stackReturn[0] = "";
		stackReturn[1] = "";

		/* Will always print stack trace */
		printStackTrace(rse);
		/*
		 * check if the stack has the OMS specific errors returned from the
		 * stored procedures
		 */
		stackReturn = captureOMSError(rse);
		if (!stackReturn[0].isEmpty() && !stackReturn[1].isEmpty()) {
			logger.error("Error Message: [" + stackReturn[0] + "]");
			logger.error("Error Code: [" + stackReturn[1] + "]");
			return stackReturn;
		}

		for (String knownIssue : knownDatabaseIssuesList) {
			if (rse.getMessage().contains(knownIssue)) {
				stackReturn[0] = "Database Vendor Code: "
						+ Integer.toString(rse.errorCode());
				stackReturn[1] = stackReturn[1] + ";" + Integer.toString((rse.errorCode() != 0) ? rse.errorCode() : -1);
			}
		}

		for (String knownIssue : OpenReportIssuesList) {
			if (rse.getMessage().contains(knownIssue)) {
				stackReturn[0] = stackReturn[0] + rse.getMessage() + "; " + Integer.toString(rse.errorCode());
				stackReturn[1] = stackReturn[1] + ";" + Integer.toString((rse.errorCode() != 0) ? rse.errorCode() : -1);
			}
		}

		/* If the error is now part of knows issues then it's unknown error */
		if (stackReturn[0].isEmpty() && stackReturn[1].isEmpty()) {
			stackReturn[0] = "Unknown RSDK exception found: " + rse.getMessage();
			stackReturn[1] = Integer.toString((rse.errorCode() != 0) ? rse.errorCode() : -1);
		}

		logger.error("Error Message: [" + stackReturn[0] + "]");
		logger.error("Error Code: [" + stackReturn[1] + "]");
		return stackReturn;
	}

	private static String[] captureOMSError(ReportSDKException rse) {
		String[] stackReturn = new String[2];
		stackReturn[0] = "";
		stackReturn[1] = "";

		StringWriter sw = new StringWriter();
		rse.printStackTrace(new PrintWriter(sw));
		String stackTrace = sw.toString();

		try {
			Pattern p = Pattern.compile("OMSErrorNo=\\[\\d*\\]");
			String omsErrorCode = "";
			String omsErrorMessage = "";
			Matcher m = p.matcher(stackTrace);
			if (m.find()) {
				omsErrorCode = m.group(0);
				stackReturn[1] = omsErrorCode.substring(omsErrorCode.indexOf("[") + 1, omsErrorCode.indexOf("]"));
			}
			p = Pattern.compile("OMSErrorDesc=\\[.*\\]");

			m = p.matcher(stackTrace);
			if (m.find()) {
				omsErrorMessage = m.group(0);
				stackReturn[0] = omsErrorCode + ", " + omsErrorMessage;
			}

		} catch (PatternSyntaxException pEx) {
			stackReturn[0] = pEx.getMessage();
			stackReturn[1] = "-1";
		}

		return stackReturn;
	}

	/**
	 * Just prints the stack trace to the log4j file
	 * 
	 * @param rse
	 */
	public static void printStackTrace(ReportSDKException rse) {
		StringWriter sw = new StringWriter();
		rse.printStackTrace(new PrintWriter(sw));
		String stackTrace = sw.toString();
		logger.error(stackTrace);
	}
	
	/**
	 * Just prints the stack trace to the log4j file
	 * 
	 * @param rse
	 */
	public static void printStackTrace(Exception e) {
		StringWriter sw = new StringWriter();
		e.printStackTrace(new PrintWriter(sw));
		String stackTrace = sw.toString();
		logger.error(stackTrace);
	}
}
