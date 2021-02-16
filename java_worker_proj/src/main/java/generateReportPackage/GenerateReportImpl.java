package generateReportPackage;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URISyntaxException;
import java.util.Locale;

import org.apache.log4j.Logger;
import org.apache.log4j.xml.DOMConfigurator;

import com.crystaldecisions.sdk.occa.report.application.ISubreportClientDocument;
import com.crystaldecisions.sdk.occa.report.application.OpenReportOptions;
import com.crystaldecisions.sdk.occa.report.application.PrintOutputController;
import com.crystaldecisions.sdk.occa.report.application.ReportClientDocument;
import com.crystaldecisions.sdk.occa.report.data.FieldValueType;
import com.crystaldecisions.sdk.occa.report.data.ParameterField;
import com.crystaldecisions.sdk.occa.report.data.Tables;
import com.crystaldecisions.sdk.occa.report.definition.ISubreportLink;
import com.crystaldecisions.sdk.occa.report.definition.SubreportLinks;
import com.crystaldecisions.sdk.occa.report.document.PaperSize;
import com.crystaldecisions.sdk.occa.report.exportoptions.ReportExportFormat;
import com.crystaldecisions.sdk.occa.report.lib.IStrings;
import com.crystaldecisions.sdk.occa.report.lib.PropertyBag;
import com.crystaldecisions.sdk.occa.report.lib.ReportSDKException;

public class GenerateReportImpl {

	private static String appLogFile = "";
	private static String sapLogFile = "";
	private static String appLogLevel = "";
	private static String sapLogLevel = "";
	static final Logger logger = Logger.getLogger(GenerateReportImpl.class);

	/* This is the constructor needed for JNI Object Creation */
	public GenerateReportImpl() {}

	/**
	 * This method will concatenate the export along side checking for invalid fonts and page size and checking the parameters
	 * @param paramNames
	 * @param paramValues
	 * @param reportFileName
	 * @param exportFileName
	 * @param dbSrvName
	 * @param dbDBName
	 * @param dbUsrName
	 * @param dbUsrPassword
	 * @param exportFormat
	 * @param invalidFonts
	 * @param appLogFileLocation
	 * @param sapLogFileLocation
	 * @param appLogFileDebug
	 * @param sapLogFileDebug
	 * @param isLocalPrint
	 * @return String[6]
	 */
	public String[] RunAllGenerateReport(String[] paramNames, String[] paramValues, String reportFileName,
			String exportFileName, String dbSrvName, String dbDBName, String dbUsrName, String dbUsrPassword,
			String exportFormat, String invalidFonts, String appLogFileLocation, String sapLogFileLocation, 
			String appLogFileDebug, String sapLogFileDebug, String isLocalPrint) {
		String[] returnStatus = new String[6];
		returnStatus[0] = "Success";
		returnStatus[1] = "0";
		returnStatus[2] = ""; // Invalid Fonts
		returnStatus[3] = ""; // Invalid Page Size
		returnStatus[4] = ""; // SP Name
		returnStatus[5] = ""; // Blank Params
		
		boolean hasInvalidFont = false;
		boolean hasInvalidPageSize = false;
		
		ReportClientDocument reportClientDoc = new ReportClientDocument();
		DataAccess dsUtil = new DataAccess(dbSrvName, dbDBName, dbUsrName, dbUsrPassword);
		try {
			String[] loggerStatus = new String[2];
			loggerStatus = setLogger(appLogFileLocation, sapLogFileLocation, appLogFileDebug, sapLogFileDebug);
			if (!loggerStatus[1].equalsIgnoreCase("0")) {
				returnStatus[0] = loggerStatus[0];
				returnStatus[1] = loggerStatus[1];
				return returnStatus;
			}
				
			logger.info("Working with report: " + reportFileName);
			reportClientDoc.setLocale(Locale.getDefault());
			/* Open the report */
			logger.info("Opening report: " + reportFileName);
			reportClientDoc = openReport(reportFileName);
			
			logger.debug("In params: [export format = " + exportFormat + "]");
			
			/**
			 * Will check for invalid font and invalid page size before doing anything
			 */
			hasInvalidFont = hasInvalidFont(reportClientDoc, invalidFonts); // we set the format to rtf in case of invalid fonts
			returnStatus[2] = (hasInvalidFont?"yes":"no");
			logger.debug("Has invalid fonts: " + returnStatus[2]);
			
			hasInvalidPageSize = checkInvalidPageSize(reportClientDoc);
			returnStatus[3] = (hasInvalidPageSize?"yes":"no");
			logger.debug("Has invalid page size: " + returnStatus[3]);
			if (hasInvalidPageSize){
				throw new ReportSDKException(-1,
						"Error: Report has invalid page size.");
			}
			
			returnStatus[4] = getMainSPName(reportClientDoc);
			logger.debug("Main SP: " + returnStatus[4]);
			
			/* Fixing the report page numbering issue */
			ReportFormatUtils rptFormat = new ReportFormatUtils();
			reportClientDoc = rptFormat.resetPageNumberingToMain(reportClientDoc);
			/* Update the report connection info with the */
			reportClientDoc = dsUtil.updateReportDBConnection(reportClientDoc);

			if (!reportClientDoc.getDatabaseController().getDatabase().getTables().isEmpty()) {
				String spName = reportClientDoc.getDatabase().getTables().get(0).getName();
				spName = spName.substring(0, spName.length() - 2);
				int spParamNr = dsUtil.getSPParamNumber(spName);
				int rptParamNr = reportClientDoc.getDataDefController().getDataDefinition().getParameterFields().size();
				logger.debug("SP param value = " + spParamNr);
				logger.debug("Number of paramters found in rpt: " + rptParamNr);
				if (spParamNr > rptParamNr) {
					// Before verify DB we need to set the parameter values
					reportClientDoc = dsUtil.setDataSourceParams(reportClientDoc, paramNames, paramValues);
					logger.info("Verify DB");
					reportClientDoc.verifyDatabase(); // this can fail for
														// several
														// reports
					int updatedReportParams = reportClientDoc.getDataDefController().getDataDefinition()
							.getParameterFields().size();
					logger.debug("After verify db we have " + updatedReportParams + " paramters found in rpt");
				}
				reportClientDoc = dsUtil.setDataSourceParams(reportClientDoc, paramNames, paramValues);
				reportClientDoc = dsUtil.checkSubReportParamters(reportClientDoc);
				
				String blankParams = dsUtil.getBlankParamters();
				logger.debug(blankParams);
				if (blankParams != "") {
					returnStatus[5] = blankParams;
					}
			}
			/* Export the report */
			PrintOutputController printOutputCrtl = reportClientDoc.getPrintOutputController();
			logger.info("Start Export for report: " + reportFileName);
			
			// if invalid fonts are found we change the format to rtf (in spite of what was the format sent)
			if (hasInvalidFont) {
				exportFormat = "rtf";
			}
			
			ReportExportFormat exportTo = null;
			InputStream myIS = null;

			exportTo = rptFormat.determineExportFormat(exportFormat);

			myIS = printOutputCrtl.export(exportTo);
			if (myIS == null) {
				throw new ReportSDKException(-1,
						"Java Generation Engine was unable to retrieve content for this report.");
			}
			/* Save export */
			String exportLocation = rptFormat.updateExtension(exportFileName, exportFormat);
			rptFormat.saveExportToFile(exportLocation, myIS, exportFormat);
			logger.info("Export done!");
			
			// We will also generate the report as pdf in case we're in the
			// situation of local print/preview
			if (isLocalPrint.equalsIgnoreCase("true") && exportFormat.equalsIgnoreCase("rtf") && !hasInvalidFont) {
				exportFormat = "pdf";
				exportTo = rptFormat.determineExportFormat(exportFormat);
				myIS = printOutputCrtl.export(exportTo);
				if (myIS == null) {
					throw new ReportSDKException(-1,
							"Java Generation Engine was unable to retrieve content for this report.");
				}
				/* Save export */
				exportLocation = rptFormat.updateExtension(exportFileName, exportFormat);
				rptFormat.saveExportToFile(exportLocation, myIS, exportFormat);
				logger.info("Export done!");
			}
			logger.info("Successfully exported report to " + exportLocation);

		} catch (ReportSDKException rse) {
			String[] errorCheck = new String[2];
			errorCheck[0] = ErrorHandle.checkForKnownIssues(rse)[0];
			errorCheck[1] = ErrorHandle.checkForKnownIssues(rse)[1];
			returnStatus[0] = errorCheck[0].toString();
			returnStatus[1] = errorCheck[1].toString();
		} catch (Exception e) {
			// Catch random error
			ErrorHandle.printStackTrace(e);
			returnStatus[0] = "Unknown exception found: " + e.toString();
			returnStatus[1] = Integer.toString(-1);
			logger.error(returnStatus[0]);
		} finally {
			try {
				if (reportClientDoc.isOpen()) {
					logger.info("Closing report: " + reportFileName);
					reportClientDoc.close();
				}
			} catch (ReportSDKException e) {
				String[] errorCheck = new String[2];
				errorCheck[0] = ErrorHandle.checkForKnownIssues(e)[0];
				errorCheck[1] = ErrorHandle.checkForKnownIssues(e)[1];
				returnStatus[0] = errorCheck[0].toString();
				returnStatus[1] = errorCheck[1].toString();
			}
			logger.info("Report was closed: " + reportFileName);
		}
		// In the end if all goes well will have a string with MainSP and Blank Parameters and error code 0
		logger.info("Return message: " + returnStatus[0]);
		logger.info("Return code: " + returnStatus[1]);
		logger.info("Return info has invalid fonts: " + returnStatus[2]);
		logger.info("Return info has invalid size: " + returnStatus[3]);
		logger.info("Return info SP Name: " + returnStatus[4]);
		logger.info("Return info Blank Params: " + returnStatus[5]);
		return returnStatus;
	}
	
	/**
	 * This method with attempt to generate the report
	 * @param paramNames
	 * @param paramValues
	 * @param reportFileName
	 * @param exportFileName
	 * @param dbSrvName
	 * @param dbDBName
	 * @param dbUsrName
	 * @param dbUsrPassword
	 * @param exportFormat
	 * @return String[2]
	 */
	public String[] GenerateReport(String[] paramNames, String[] paramValues, String reportFileName,
			String exportFileName, String dbSrvName, String dbDBName, String dbUsrName, String dbUsrPassword,
			String exportFormat) {
		String[] returnStatus = new String[2];
		returnStatus[0] = "Success";
		returnStatus[1] = "0";

		logger.info("Working with report: " + reportFileName);
		ReportClientDocument reportClientDoc = new ReportClientDocument();
		DataAccess dsUtil = new DataAccess(dbSrvName, dbDBName, dbUsrName, dbUsrPassword);
		try {
			setLogger(appLogFile, sapLogFile, appLogLevel, sapLogLevel);
			reportClientDoc.setLocale(Locale.getDefault());

			/* Open the report */
			logger.info("Opening report: " + reportFileName);
			reportClientDoc.open(reportFileName, OpenReportOptions._discardSavedData);

			/* Fixing the report page numbering issue */
			ReportFormatUtils rptFormat = new ReportFormatUtils();
			reportClientDoc = rptFormat.resetPageNumberingToMain(reportClientDoc);
			/* Update the report connection info with the */
			reportClientDoc = dsUtil.updateReportDBConnection(reportClientDoc);

			if (!reportClientDoc.getDatabaseController().getDatabase().getTables().isEmpty()) {
				String spName = reportClientDoc.getDatabase().getTables().get(0).getName();
				spName = spName.substring(0, spName.length() - 2);
				int spParamNr = dsUtil.getSPParamNumber(spName);
				int rptParamNr = reportClientDoc.getDataDefController().getDataDefinition().getParameterFields().size();
				logger.debug("SP param value = " + spParamNr);
				logger.debug("Number of paramters found in rpt: " + rptParamNr);
				if (spParamNr > rptParamNr) {
					// Before verify DB we need to set the parameter values
					reportClientDoc = dsUtil.setDataSourceParams(reportClientDoc, paramNames, paramValues);
					logger.info("Verify DB");
					reportClientDoc.verifyDatabase(); // this can fail for
														// several
														// reports
					int updatedReportParams = reportClientDoc.getDataDefController().getDataDefinition()
							.getParameterFields().size();
					logger.debug("After verify db we have " + updatedReportParams + " paramters found in rpt");
				}
				reportClientDoc = dsUtil.setDataSourceParams(reportClientDoc, paramNames, paramValues);
				reportClientDoc = dsUtil.checkSubReportParamters(reportClientDoc);
			}
			/* Export the report */
			PrintOutputController printOutputCrtl = reportClientDoc.getPrintOutputController();
			logger.info("Start Export for report: " + reportFileName);
			ReportExportFormat exportTo = rptFormat.determineExportFormat(exportFormat);
			InputStream myIS = printOutputCrtl.export(exportTo);
			if (myIS == null) {
				throw new ReportSDKException(-1,
						"Java Generation Engine was unable to retrieve content for this report.");
			}
			logger.info("Export done!");

			/* Save export */
			String exportLocation = rptFormat.updateExtension(exportFileName, exportFormat);
			rptFormat.saveExportToFile(exportLocation, myIS);
			logger.info("Successfully exported report to " + exportLocation);
			
		} catch (ReportSDKException rse) {
			returnStatus = ErrorHandle.checkForKnownIssues(rse);
		} catch (Exception e) {
			// Catch random error
			returnStatus[0] = "Unknown exception found: " + e.getMessage();
			returnStatus[1] = Integer.toString(-1);
			logger.error(returnStatus[0]);
		} finally {
			try {
				if (reportClientDoc.isOpen()) {
					logger.info("Closing report: " + reportFileName);
					reportClientDoc.close();
				}
			} catch (ReportSDKException e) {
				returnStatus = ErrorHandle.checkForKnownIssues(e);
			}
			logger.info("Report was closed: " + reportFileName);
		}
		return returnStatus;
	}
	
	public String[] exportOnly(String reportFileName) {

		String[] returnStatus = new String[2];
		returnStatus[0] = "Success";
		returnStatus[1] = "0";

		/* Export the report */
		ReportClientDocument reportClientDoc = new ReportClientDocument();

		try {
			setLogger(appLogFile, sapLogFile, appLogLevel, sapLogLevel);
			reportClientDoc.setLocale(Locale.getDefault());

			/* Open the report */
			logger.info("Opening report: " + reportFileName);
			reportClientDoc.open(reportFileName, OpenReportOptions._discardSavedData);

			logger.info("Start Export for report: " + reportFileName);
			PrintOutputController printOutputCrtl = reportClientDoc.getPrintOutputController();
			ReportExportFormat exportTo = ReportExportFormat.PDF;
			InputStream myIS = printOutputCrtl.export(exportTo);
			if (myIS == null) {
				throw new ReportSDKException(-1,
						"Java Generation Engine was unable to retrieve content for this report.");
			}
			logger.info("Export done!");

			ReportFormatUtils rptFormat = new ReportFormatUtils();
			/* Save export */
			String exportLocation = rptFormat.updateExtension(reportFileName, "pdf");
			rptFormat.saveExportToFile(exportLocation, myIS);
			logger.info("Successfully exported report to " + exportLocation);
		} catch (ReportSDKException rse) {
			returnStatus = ErrorHandle.checkForKnownIssues(rse);
		} catch (IOException ioex) {
			returnStatus[0] = "IO exception found: " + ioex.getMessage();
			returnStatus[1] = Integer.toString(-1);
			logger.error(returnStatus[0]);
		} finally {
			try {
				if (reportClientDoc.isOpen()) {
					logger.info("Closing report: " + reportFileName);
					reportClientDoc.close();
				}
			} catch (ReportSDKException e) {
				returnStatus = ErrorHandle.checkForKnownIssues(e);
			}
			logger.info("Report was closed: " + reportFileName);
		}
		return returnStatus;
	}
	 
	/**
	 * This Method will check if we can set all the DB type Parameters
	 * Will check if the report needs to be updated in case of parameter differences
	 * @param paramNames
	 * @param paramValues
	 * @param reportFileName
	 * @param dbServerName
	 * @param dbDataBaseName
	 * @param dbUserName
	 * @param dbUserPassword
	 * @return String[]
	 */
	public String[] setParametersAndDB(String[] paramNames, String[] paramValues, String reportFileName,
			String dbServerName, String dbDataBaseName, String dbUserName, String dbUserPassword) {
		String[] returnStatus = new String[2];
		returnStatus[0] = "";
		returnStatus[1] = "0";

		ReportClientDocument reportClientDoc = new ReportClientDocument();
		DataAccess dsUtil = new DataAccess(dbServerName, dbDataBaseName, dbUserName, dbUserPassword);
		try {
			setLogger(appLogFile, sapLogFile, appLogLevel, sapLogLevel);
			// Open the report
			logger.info("Opening report: " + reportFileName);
			reportClientDoc.open(reportFileName, OpenReportOptions._discardSavedData);
			reportClientDoc = dsUtil.updateReportDBConnection(reportClientDoc);
			
			if (!reportClientDoc.getDatabase().getTables().isEmpty()) {
				String spName = reportClientDoc.getDatabase().getTables().get(0).getName();
				logger.debug(spName);

				spName = spName.substring(0, spName.length() - 2);
				int spParamNr = dsUtil.getSPParamNumber(spName);
				logger.debug("SP param value = " + spParamNr);
				logger.debug("Number of paramters found in rpt: "
						+ reportClientDoc.getDataDefController().getDataDefinition().getParameterFields().size());
				if (spParamNr > reportClientDoc.getDataDefController().getDataDefinition().getParameterFields()
						.size()) {
					// Before verify DB we need to set the parameter values
					dsUtil.setDataSourceParams(reportClientDoc, paramNames, paramValues);
					logger.info("Verify DB");
					reportClientDoc.verifyDatabase(); /** this can fail for
													      several reports **/
					logger.debug("Number of paramters found in rpt: "
							+ reportClientDoc.getDataDefController().getDataDefinition().getParameterFields().size());
				}
				reportClientDoc = dsUtil.setDataSourceParams(reportClientDoc, paramNames, paramValues);
				String blankParams = dsUtil.getBlankParamters();
				logger.debug(blankParams);
				if (blankParams != "") {
					returnStatus[0] = blankParams; /**
													 * we return the list of
													 * blank parameters in case
													 * we have to put in extra
													 * parameters
													 **/
				}
			}
			// ===========================================================================

		} catch (ReportSDKException rse) {
			returnStatus = ErrorHandle.checkForKnownIssues(rse);
		} catch (Exception e) {
			// Catch random error
			returnStatus[0] = "Unknown exception found: " + e.getMessage();
			returnStatus[1] = Integer.toString(-1);
			logger.error(returnStatus[0]);
		} finally {
			try {
				if (reportClientDoc.isOpen()) {
					logger.info("Closing report: " + reportFileName);
					reportClientDoc.close();
				}
			} catch (ReportSDKException e) {
				returnStatus = ErrorHandle.checkForKnownIssues(e);
			}
			logger.info("Report was closed: " + reportFileName);
		}
		return returnStatus;
	}

	/**
	 * Sets log4j file and system properties
	 * @param appLogFileLocation
	 * @param sapLogFileLocation
	 * @param appLogFileDebug
	 * @param sapLogFileDebug
	 * @return String[]
	 */
	public String[] setLogger(String appLogFileLocation, String sapLogFileLocation, String appLogFileDebug,
			String sapLogFileDebug) {
		String[] returnStatus = new String[2];
		returnStatus[0] = "Success";
		returnStatus[1] = "0";
		if (appLogFileLocation.isEmpty() || sapLogFileLocation.isEmpty()
				|| appLogFileDebug.isEmpty() || sapLogFileDebug.isEmpty()) {
			returnStatus[0] = "Please supply the log4j paramters.";
			returnStatus[1] = "-1";
			return returnStatus;
		}
		try {
			
			ReportFormatUtils util = new ReportFormatUtils();
			util.validateLogFilePath(appLogFileLocation, appLogFile);
			util.validateLogFilePath(sapLogFileLocation, sapLogFile);

			System.setProperty("ReportGeneratorAppenderLogFile", appLogFileLocation);
			System.setProperty("SAPAppenderLogFile", sapLogFileLocation);
			System.setProperty("ReportGeneratorLogLevel", appLogFileDebug);
			System.setProperty("ReportGeneratorRootLogLevel", sapLogFileDebug);

			File myJar = new File(
					GenerateReportImpl.class.getProtectionDomain().getCodeSource().getLocation().toURI().getPath());
			String log4jxmlFilePath = myJar.getParent() + "/log4j.xml";

			DOMConfigurator.configure(log4jxmlFilePath);

		} catch (URISyntaxException urie) {
			returnStatus[0] = urie.getMessage();
			returnStatus[1] = "-1";
		} catch (ReportSDKException res) {
			returnStatus[0] = res.getMessage();
			returnStatus[1] = Integer.toString(res.errorCode());
		} catch (Exception e) {
			returnStatus[0] = e.getMessage();
			returnStatus[1] = "-1";
		} finally {
			appLogFile = appLogFileLocation;
			sapLogFile = sapLogFileLocation;
			appLogLevel = appLogFileDebug;
			sapLogLevel = sapLogFileDebug;
		}
		return returnStatus;
	}



	/**
	 * Checking report for invalid page size
	 * Valid paper size list: paperA4, paperLetter, paperLegal
	 * @param reportFileName
	 * @return String[2]
	 */
	public String[] CheckInvalidPageSize(String reportFileName) {
		String[] returnStatus = new String[2];
		returnStatus[0] = "N";
		returnStatus[1] = "0";

		ReportClientDocument reportClientDoc = new ReportClientDocument();
		try {
			setLogger(appLogFile, sapLogFile, appLogLevel, sapLogLevel);
			// ===========================================================================
			// Open the report
			logger.info("Opening report: " + reportFileName);
			reportClientDoc.open(reportFileName, OpenReportOptions._discardSavedData);
			if (checkInvalidPageSize(reportClientDoc)){
				returnStatus[0] = "Y";
				returnStatus[1] = "0";
			}
		} catch (ReportSDKException rse) {
			returnStatus = ErrorHandle.checkForKnownIssues(rse);
		} catch (Exception e) {
			// Cat4ch random error
			returnStatus[0] = "Unknown exception found: " + e.getMessage();
			returnStatus[1] = Integer.toString(-1);
			logger.error(returnStatus[0]);
		} finally {
			logger.info("Closing report: " + reportFileName);
			try {
				if (reportClientDoc.isOpen()) {
					logger.debug("Closing report!");
					reportClientDoc.close();
				}
			} catch (ReportSDKException e) {
				returnStatus = ErrorHandle.checkForKnownIssues(e);
			} catch (Exception e) {
				returnStatus[0] = "Unknown exception found: " + e.getMessage();
				returnStatus[1] = Integer.toString(-1);
				logger.error(returnStatus[0]);
			}
			logger.info("Report was closed: " + reportFileName);
		}
		return returnStatus;
	}
	
	/**
	 * Local check for invalid page size
	 * Valid paper size list: paperA4, paperLetter, paperLegal
	 * @param reportClientDoc
	 * @return boolean
	 * @throws ReportSDKException
	 */
	public boolean checkInvalidPageSize(ReportClientDocument reportClientDoc) throws ReportSDKException {
		logger.info("Check for invalid page sizes. Valid sizes are: A4, Letter, Legal.");
		boolean isA4 = true;
		if (reportClientDoc.getPrintOutputController().getPrintOptions().getPaperSize() != PaperSize.paperA4) {
			isA4 = false;
		}
		boolean isLetter = true;
		if (reportClientDoc.getPrintOutputController().getPrintOptions().getPaperSize() != PaperSize.paperLetter) {
			isLetter = false;
		}
		boolean isLegal = true;
		if (reportClientDoc.getPrintOutputController().getPrintOptions().getPaperSize() != PaperSize.paperLegal) {
			isLegal = false;
		}
		if ((!isA4) && (!isLetter) && (!isLegal)) {
			return true;
		}
		logger.info("Paper size found: "
				+ reportClientDoc.getPrintOutputController().getPrintOptions().getPaperSize().toString());

		return false;
	}

	/**
	 * Get the main stored procedure name to show it under the view details for the report
	 * @param reportFileName
	 * @return String[]
	 */
	public String[] GetMainSPName(String reportFileName) {
		String[] returnStatus = new String[2];
		returnStatus[0] = "";
		returnStatus[1] = "0";

		ReportClientDocument reportClientDoc = new ReportClientDocument();

		try {
			setLogger(appLogFile, sapLogFile, appLogLevel, sapLogLevel);
			// ===========================================================================
			// Open the report
			logger.info("Opening report: " + reportFileName);
			reportClientDoc.open(reportFileName, OpenReportOptions._discardSavedData);
			returnStatus[0] = getMainSPName(reportClientDoc);
			// ===========================================================================
		} catch (ReportSDKException rse) {
			returnStatus = ErrorHandle.checkForKnownIssues(rse);
		} catch (Exception e) {
			// Catch random error
			returnStatus[0] = "Unknown exception found: " + e.getMessage();
			returnStatus[1] = Integer.toString(-1);
			logger.error(returnStatus[0]);
		} finally {
			try {
				if (reportClientDoc.isOpen()) {
					logger.info("Closing report: " + reportFileName);
					reportClientDoc.close();
				}
			} catch (ReportSDKException e) {
				returnStatus = ErrorHandle.checkForKnownIssues(e);
			}
			logger.info("Report was closed: " + reportFileName);
		}
		return returnStatus;
	}

	/**
	 * Local version for getting main stored procedure name
	 * @param reportClientDoc
	 * @return String
	 * @throws ReportSDKException
	 */
	public String getMainSPName(ReportClientDocument reportClientDoc) throws ReportSDKException {
		String SPName = "";
		if (!reportClientDoc.getDatabaseController().getDatabase().getTables().isEmpty()) {
			SPName = reportClientDoc.getDatabaseController().getDatabase().getTables().get(0).getName();
			logger.debug("GetMainSPName = " + SPName);
			return SPName;
		}
		return SPName;
	}
	
	/**
	 * Public method for font checking
	 * This method will be called directly from C++ through JNI
	 * @param invalidFonts
	 * @param reportFileName
	 * @return Y/N and Error Code (in string format)
	 */
	public String[] HasInvalidFont(String invalidFonts, String reportFileName) {
		String[] returnStatus = new String[2];
		returnStatus[0] = "N";
		returnStatus[1] = "0";
		ReportClientDocument reportClientDoc = new ReportClientDocument();
		
		try {
			setLogger(appLogFile, sapLogFile, appLogLevel, sapLogLevel);
			// ===========================================================================
			// Open the report
			logger.info("Opening report: " + reportFileName);
			reportClientDoc.open(reportFileName, OpenReportOptions._discardSavedData);
			if (hasInvalidFont(reportClientDoc, invalidFonts)){
				returnStatus[0] = "Y";
				returnStatus[1] = "0";
			}
			// ===========================================================================
		} catch (ReportSDKException rse) {
			returnStatus = ErrorHandle.checkForKnownIssues(rse);
		} catch (Exception e) {
			// Catch random error
			returnStatus[0] = "Unknown exception found: " + e.getMessage();
			returnStatus[1] = Integer.toString(-1);
			logger.error(returnStatus[0]);
		} finally {
			try {
				if (reportClientDoc.isOpen()) {
					logger.info("Closing report: " + reportFileName);
					reportClientDoc.close();
				}
			} catch (ReportSDKException e) {
				returnStatus = ErrorHandle.checkForKnownIssues(e);
			}
			logger.info("Report was closed: " + reportFileName);
		}
		return returnStatus;
	}
	
	/**
	 * Local invalid font checker
	 * Will use this function from various places
	 * @param reportClientDoc
	 * @param invalidFonts
	 * @return boolean
	 * @throws ReportSDKException
	 */
	public boolean hasInvalidFont(ReportClientDocument reportClientDoc, String invalidFonts) throws ReportSDKException{
		ReportFormatUtils rptFormat = new ReportFormatUtils();
		if (rptFormat.isInvalidFont(reportClientDoc, invalidFonts)) {
			return true;
		}
		return false;
	}

	/**
	 * Use this function to try multiple times (5) to open the report
	 * @param reportFileName
	 * @return ReportClientDocument
	 * @throws ReportSDKException
	 * @throws InterruptedException
	 */
	public ReportClientDocument openReport(String reportFileName) throws ReportSDKException, InterruptedException {
		ReportClientDocument myReportClientDoc = new ReportClientDocument();
		int retryCount = 0;
		do {
			try {
				myReportClientDoc.open(reportFileName, OpenReportOptions._discardSavedData);
				return myReportClientDoc;
			} catch (ReportSDKException ex) {
				logger.error("Unable to open report - will retry (" + retryCount + ") out of 5.");
				logger.error(ex.getMessage());
				
				if (retryCount > 5){
					logger.error("Unable to open the report. Max retry counter exceeded. [5]");
					throw ex;
				}
				
				Thread.sleep(200);
				retryCount++;
				
				continue;
			}
		} while (true);
	}
	
	/**
	 * Public open report that can be called from C++
	 * @param reportFileName
	 * @return String[]
	 */
	public String[] OpenReport(String reportFileName) {
		String[] returnStatus = new String[2];
		returnStatus[0] = "Success";
		returnStatus[1] = "0";
		ReportClientDocument reportClientDoc = new ReportClientDocument();
		logger.info("=====================================================");
		logger.info("New Report: " + reportFileName);
		try {
			setLogger(appLogFile, sapLogFile, appLogLevel, sapLogLevel);
			// ===========================================================================
			// Open the report
			logger.info("Opening report: " + reportFileName);
			reportClientDoc = openReport(reportFileName);
			// ===========================================================================
		} catch (ReportSDKException rse) {
			returnStatus = ErrorHandle.checkForKnownIssues(rse);
		} catch (Exception e) {
			// Catch random error
			returnStatus[0] = "Unknown exception found: " + e.getMessage();
			returnStatus[1] = Integer.toString(-1);
			logger.error(returnStatus[0]);
		} finally {
			try {
				if (reportClientDoc.isOpen()) {
					logger.info("Closing report: " + reportFileName);
					reportClientDoc.close();
				}
			} catch (ReportSDKException e) {
				returnStatus = ErrorHandle.checkForKnownIssues(e);
			}
			logger.info("Report was closed: " + reportFileName);
		}
		return returnStatus;
	}

	/**
	 * Currently not used
	 * 
	 * @param reportFileName
	 * @param logggingLocation
	 * @return String[]
	 */
	public String[] CheckForUnMappedParameters(String reportFileName, String logggingLocation) {
		String[] localstatus = new String[4];
		localstatus[0] = "";
		localstatus[1] = "0";
		localstatus[2] = "";
		localstatus[3] = "";
		int unmapped = 0;
		ReportClientDocument reportClientDoc = new ReportClientDocument();

		try {
			setLogger(logggingLocation + "/oms-reporting.log", logggingLocation + "/sap-reporting.log", "DEBUG",
					"ERROR");
			// ===========================================================================
			// Open the report
			logger.info("Opening report: " + reportFileName);
			reportClientDoc.open(reportFileName, OpenReportOptions._discardSavedData);
			// logDatabaseInfo(reportClientDoc.getDatabaseController());
			Tables tables = reportClientDoc.getDatabaseController().getDatabase().getTables();
			for (int ii = 0; ii < tables.size(); ii++) {
				PropertyBag bag = tables.get(ii).getConnectionInfo().getAttributes();
				localstatus[2] = localstatus[2] + "mainReport-" + reportClientDoc.displayName() + ": "
						+ bag.get("Server Type") + ", Provider: " + bag.get("Provider") + ";";
			}

			logger.info("Report Opened");
			IStrings subreportNames = reportClientDoc.getSubreportController().getSubreportNames();
			// Set the data source for all the sub reports.
			logger.debug("Subreports:");
			for (int i = 0; i < subreportNames.size(); i++) {
				logger.debug("Subreport: " + subreportNames.getString(i));
				ISubreportClientDocument subreportClientDoc = reportClientDoc.getSubreportController()
						.getSubreport(subreportNames.getString(i));
				// logDatabaseInfo(subreportClientDoc.getDatabaseController());
				SubreportLinks links = reportClientDoc.getSubreportController()
						.getSubreportLinks(subreportNames.getString(i));
				for (Object field : subreportClientDoc.getDataDefController().getDataDefinition()
						.getParameterFields()) {
					ParameterField fd = (ParameterField) field;
					boolean found = false;
					for (ISubreportLink link : links) {
						if (fd.getName().contentEquals(link.getLinkedParameterName())) {
							logger.debug(
									"Parameter: " + fd.getName() + " is linked to: " + link.getMainReportFieldName());
							// we found the parameter in the links so we can
							// brake out of this iteration
							// we should add it to the list of parameters linked
							found = true;
						}
					}
					if (found == false) { // we couldn't find the parameter in
											// the links list
						unmapped = unmapped + 1;
						localstatus[0] = localstatus[0] + "; subreport: " + subreportNames.getString(i) + ", param: "
								+ fd.getName();
						logger.debug("Parameter: " + fd.getName() + " is not linked.");
					}
				}

				tables = subreportClientDoc.getDatabaseController().getDatabase().getTables();
				for (int ii = 0; ii < tables.size(); ii++) {
					PropertyBag bag = tables.get(ii).getConnectionInfo().getAttributes();
					localstatus[2] = localstatus[2] + " subReport-" + subreportNames.getString(i) + ": "
							+ bag.get("Server Type") + ", Provider: " + bag.get("Provider") + ";";
				}
			}
						
			localstatus[0] = "count: " + unmapped + localstatus[0];
			localstatus[1] = "0";

			//=======================
			// Get DB Param Types
			String parameterName = "";
			for (Object field : reportClientDoc.getDataDefController().getDataDefinition().getParameterFields()) {
				ParameterField fd = (ParameterField) field;
				FieldValueType fdType = fd.getType(); //xsd:string anything but
				parameterName = fd.getShortName(Locale.ENGLISH).substring(1);
				if (!fdType.toString().equalsIgnoreCase("xsd:string")){
					localstatus[3] = localstatus[3] + "ParamName=[" + parameterName + "] Type=[" + fdType.toString() + "]; ";
				}
			}
			
			
			// ===========================================================================
		} catch (ReportSDKException rse) {
			localstatus = ErrorHandle.checkForKnownIssues(rse);
		} catch (Exception e) {
			// Catch random error
			localstatus[0] = "Unknown exception found: " + e.getMessage();
			localstatus[1] = Integer.toString(-1);
			logger.error(localstatus[0]);
		} finally {
			try {
				if (reportClientDoc.isOpen()) {
					logger.info("Closing report: " + reportFileName);
					reportClientDoc.close();
				}
			} catch (ReportSDKException e) {
				localstatus = ErrorHandle.checkForKnownIssues(e);
			}
			logger.info("Report was closed: " + reportFileName);
		}
		return localstatus;
	}	
}
