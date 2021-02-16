package generateReportPackage;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

import org.apache.commons.lang.StringUtils;
import org.apache.log4j.Logger;

import com.crystaldecisions.sdk.occa.report.application.DBOptions;
import com.crystaldecisions.sdk.occa.report.application.DatabaseController;
import com.crystaldecisions.sdk.occa.report.application.ParameterFieldController;
import com.crystaldecisions.sdk.occa.report.application.ReportClientDocument;
import com.crystaldecisions.sdk.occa.report.data.FieldValueType;
import com.crystaldecisions.sdk.occa.report.data.Fields;
import com.crystaldecisions.sdk.occa.report.data.IConnectionInfo;
import com.crystaldecisions.sdk.occa.report.data.ParameterField;
import com.crystaldecisions.sdk.occa.report.data.Tables;
import com.crystaldecisions.sdk.occa.report.definition.ISubreportLink;
import com.crystaldecisions.sdk.occa.report.definition.SubreportLinks;
import com.crystaldecisions.sdk.occa.report.lib.IStrings;
import com.crystaldecisions.sdk.occa.report.lib.PropertyBag;
import com.crystaldecisions.sdk.occa.report.lib.ReportSDKException;

public class DataAccess {
	private String dbServerName = "";
	private String dbSqlInstance = "";
	private String dbDataBaseName = "";
	private String dbUserName = "";
	private String dbUserPassword = "";
	private String jdbcConnectionURL = "";
	private String jdbcClassName = "com.microsoft.sqlserver.jdbc.SQLServerDriver";
	private String CONNECTION_STRING = "";
	private String SERVERTYPE = "JDBC (JNDI)";
	private String DATABASE_DLL = "crdb_jdbc.dll";
	private String BlankParameterList = "";
	static final Logger logger = Logger.getLogger(GenerateReportImpl.class);

	public DataAccess(String serverName, String dataBaseName, String userName, String userPassword) {
		String[] splitResult;
		logger.info("Database values: dbServerName = [" + serverName + "]; dbDataBaseName = [" + dataBaseName
				+ "]; dbUserName = [" + userName + "]; dbUserPassword = [**********]");
		if (serverName.contains("\\")) {
			splitResult = serverName.split("\\\\");
			this.dbServerName = splitResult[0];
			this.dbSqlInstance = ";instanceName=" + splitResult[1];
		} else {
			this.dbServerName = serverName;
		}

		this.dbDataBaseName = dataBaseName;
		if (userName.length() > 0 && userPassword.length() > 0) {
			dbUserName = userName;
			dbUserPassword = userPassword;

			jdbcConnectionURL = "jdbc:sqlserver://" + dbServerName + dbSqlInstance + ";databaseName=" + dbDataBaseName
					+ ";user=" + dbUserName + ";password=" + dbUserPassword + ";";
			CONNECTION_STRING = "!" + jdbcClassName + "" + "!jdbc:sqlserver://" + dbServerName + dbSqlInstance
					+ ";databaseName=" + dbDataBaseName + ";!user="+dbUserName+"!password="+dbUserPassword+"!";
		} else {
			jdbcConnectionURL = "jdbc:sqlserver://" + dbServerName + dbSqlInstance + ";databaseName=" + dbDataBaseName
					+ ";integratedSecurity=true;";
			CONNECTION_STRING = "!" + jdbcClassName + "" + "!jdbc:sqlserver://" + dbServerName + dbSqlInstance
					+ ";databaseName=" + dbDataBaseName + ";integratedSecurity=true;!user={userid}!password={password}";
		}
		SERVERTYPE = "JDBC (JNDI)";
	}
	
	/**
	 * This method will attempt to refresh the database source location for a specific database controller
	 * Works similar to verify DB
	 * @param databaseController
	 * @throws ReportSDKException
	 */
    private void refreshDataSourceLocation(com.crystaldecisions.sdk.occa.report.application.ISubreportClientDocument iSubreportClientDocument) throws ReportSDKException {
    	logger.info("Started refreshDataSourceLocation");
    	Tables tables = iSubreportClientDocument.getDatabaseController().getDatabase().getTables();
        for (int i = 0; i < tables.size(); i++) {
            tables.get(i).setQualifiedName(tables.get(i).getAlias());
            iSubreportClientDocument.getDatabaseController().setTableLocation(tables.get(i), tables.get(i));
        }
        logger.info("Finished refreshDataSourceLocation");
    }
    
    /**
     * Refresh this sub-report if necessary
     * @param iSubreportClientDocument
     * @param links
     * @throws ReportSDKException
     */
    private void refreshSubReport(com.crystaldecisions.sdk.occa.report.application.ISubreportClientDocument iSubreportClientDocument, SubreportLinks links) throws ReportSDKException{
		if (!iSubreportClientDocument.getDatabaseController().getDatabase().getTables().isEmpty()) {
			String spName = iSubreportClientDocument.getDatabaseController().getDatabase().getTables().get(0).getName();
			logger.debug("Subreport stored proc name = " + spName);
			spName = spName.substring(0, spName.length() - 2);
			int spParamNr = this.getSPParamNumber(spName);
			logger.debug("SP param value = " + spParamNr);
			logger.debug("Number of paramters found in rpt: "
					+ iSubreportClientDocument.getDataDefController().getDataDefinition().getParameterFields().size());
			/**
			 * We need to check if the sub-report has a lower number of
			 * parameters than the stored procedure. If we have more, then we
			 * need to refresh the report to update the parameter list. Since
			 * we've already updated the database connection we just need to
			 * refresh the report data source
			 */
			if (spParamNr > iSubreportClientDocument.getDataDefController().getDataDefinition().getParameterFields().size()) {
				refreshDataSourceLocation(iSubreportClientDocument);
				if (!iSubreportClientDocument.getDataDefController().getDataDefinition().getParameterFields().isEmpty()){
					ParameterFieldController parameterFieldController = iSubreportClientDocument.getDataDefController()
							.getParameterFieldController();
					for (Object field : iSubreportClientDocument.getDataDefController().getDataDefinition().getParameterFields()) {
						ParameterField fd = (ParameterField) field;
						String interrogatedParameter = fd.getShortName(Locale.ENGLISH).substring(1);
						if (!isParamLinked(links, interrogatedParameter)){
							if (interrogatedParameter.equalsIgnoreCase("@HSF_JAVA_SDK_SW")) {
								parameterFieldController.setCurrentValue(StringUtils.EMPTY, interrogatedParameter,
										"Y");
							} else {
								parameterFieldController.setCurrentValue(StringUtils.EMPTY, interrogatedParameter,
									StringUtils.EMPTY);
							}
								
							
						}		
					}
				}
			}
		}
    }
    
	/**
	 * This method is trying to correct the problem where we could have unmapped parameters under the reports sub-reports
	 * @param reportClientDoc
	 * @return ReportClientDocument
	 * @throws ReportSDKException
	 */
	public ReportClientDocument checkSubReportParamters(ReportClientDocument reportClientDoc)
			throws ReportSDKException {
		logger.info("Start - Checking sub-reports paramter mappings");
		IStrings subreportNames = reportClientDoc.getSubreportController().getSubreportNames();
		for (int i = 0; i < subreportNames.size(); i++) {
			logger.debug("Subreport: " + subreportNames.getString(i));
			refreshSubReport(reportClientDoc.getSubreportController().getSubreport(subreportNames.getString(i)), 
					reportClientDoc.getSubreportController().getSubreportLinks(subreportNames.getString(i)));
			
		}
		logger.info("End - Checking sub-reports paramter mappings");
		return reportClientDoc;
	}
	
	/**
	 * Get the number of parameters of the data source table from SQL 
	 * @param spName
	 * @return
	 * @throws ReportSDKException
	 */
	public int getSPParamNumber(String spName) throws ReportSDKException {
		int numberOfParams = 0;
		try {
			Connection conn = DriverManager.getConnection(jdbcConnectionURL, dbUserName, dbUserPassword);
			Statement stmt = conn.createStatement();
			ResultSet rs;

			rs = stmt.executeQuery("SELECT COUNT(name) as ParamNum FROM  sys.parameters where object_id = object_id('"
					+ spName + "')");
			while (rs.next()) {
				numberOfParams = rs.getInt(1);
			}
			stmt.close();
			rs.close();
			conn.close();
			return numberOfParams;
		} catch (SQLException e) {
			throw new ReportSDKException(e.getErrorCode(), e.getMessage());
		}
	}

	private void logDatabaseInfo(DatabaseController databaseController) throws ReportSDKException {
		logger.debug("GenerateReportASImpl: Start->logDatabaseInfo");
		logger.debug("Logging database info for tables:");
		Tables tables = databaseController.getDatabase().getTables();
		for (int i = 0; i < tables.size(); i++) {
			logger.debug("Table name: " + tables.get(i).getName());
			logger.debug("Table alias: " + tables.get(i).getAlias());
			logger.debug("Table QualifiedName: " + tables.get(i).getQualifiedName());
			logger.debug("UserName: " + tables.get(i).getConnectionInfo().getUserName());
			logger.debug("Dumping the property bag: ");
			PropertyBag bag = tables.get(i).getConnectionInfo().getAttributes();
			IStrings tableProps = tables.get(i).getConnectionInfo().getAttributes().getPropertyIDs();
			for (int j = 0; j < tableProps.size(); j++) {
				logger.debug("Name=" + tableProps.getString(j) + " , value=" + bag.get(tableProps.getString(j)));
			}
		}
		logger.debug("GenerateReportASImpl: Stop->logDatabaseInfo");
	}

	/**
	 * Will update parameters for the current report
	 * @param reportClientDoc
	 * @param paramNames
	 * @param paramValues
	 * @return
	 * @throws ReportSDKException
	 */
	public ReportClientDocument setDataSourceParams(ReportClientDocument reportClientDoc, String[] paramNames, String[] paramValues)
			throws ReportSDKException {

		logger.info("GenerateReportASImpl: Start->setDataSourceParams");
		String blankParams = "";
		if (!reportClientDoc.getDataDefController().getDataDefinition().getParameterFields().isEmpty()) {
			ParameterFieldController parameterFieldController = reportClientDoc.getDataDefController()
					.getParameterFieldController();

			String interrogatedParameter;
			String interrogatedParameterValue = "";

			logger.debug("RPT:" + reportClientDoc.displayName());
			for (String param : paramNames) {
				logger.debug("Paramter from filter:" + param);
			}

			for (Object field : reportClientDoc.getDataDefController().getDataDefinition().getParameterFields()) {
				ParameterField fd = (ParameterField) field;
				FieldValueType fdType = fd.getType();
				interrogatedParameter = fd.getShortName(Locale.ENGLISH).substring(1);
				logger.debug("RPT:" + reportClientDoc.displayName());
				logger.debug("Parameter in the RPT:" + interrogatedParameter);

				for (int i = 0; i < paramNames.length; i++) {
					if (paramNames[i].equalsIgnoreCase(interrogatedParameter)) {
						interrogatedParameterValue = paramValues[i];
					}
				}

				if (interrogatedParameterValue == null) {
					if ("@HSF_JAVA_SDK_SW".equalsIgnoreCase(interrogatedParameter)) {
						logger.debug("Setting paramter value to Y for @HSF_JAVA_SDK_SW");
						interrogatedParameterValue = "Y";
					} else {
					logger.debug("Paramter [" + interrogatedParameter
							+ "] was not passed. (Parameter found under RPT was not found in the list sent to the RPT)");
					logger.debug("Setting paramter value to null");
					blankParams = blankParams + "~BlankParam=" + interrogatedParameter;
				}
				}
				logger.debug("Name= [" + interrogatedParameter + "] Value= [" + "] Type=" + fdType.toString()
						+ " value=" + fdType.value());
				parameterFieldController.setCurrentValue(StringUtils.EMPTY, interrogatedParameter,
						interrogatedParameterValue);
				logger.debug("Name= [" + interrogatedParameter + "] Value= ["
						+ fd.getCurrentValues().getValue(0).computeText() + "] Type=" + fdType.toString() + " value="
						+ fdType.value());
				interrogatedParameterValue = null;
			}
		}
		updateBlankParamters(blankParams);
		logger.info("GenerateReportASImpl: Stop->setDataSourceParams");
		return reportClientDoc;
	}

	private void updateBlankParamters(String blankParams) {
		BlankParameterList = blankParams;
	}
	
	public String getBlankParamters(){
		return BlankParameterList;
	}

	@SuppressWarnings("rawtypes")
	/**
	 * Update data source for a specific database controller
	 * @param docDBController
	 * @throws ReportSDKException
	 */
	private ReportClientDocument changeDataSourceReplaceConnection(ReportClientDocument reportClientDoc) throws ReportSDKException {
		logger.info("GenerateReportASImpl: Start->changeDataSourceReplaceConnection");
		Fields fields = null;
		DatabaseController docDBController = reportClientDoc.getDatabaseController();
		if (!docDBController.getConnectionInfos(null).isEmpty()) {
			IConnectionInfo connInfo = docDBController.getConnectionInfos(null).getConnectionInfo(0);

			PropertyBag propertyBag = connInfo.getAttributes();
			propertyBag.clear();
			connInfo.setAttributes(propertyBag);
			propertyBag.put("JDBC Connection String", CONNECTION_STRING);
			propertyBag.put("Server Type", SERVERTYPE);
			propertyBag.put("Database DLL", DATABASE_DLL);
			propertyBag.put("Database Class Name", jdbcClassName);
			propertyBag.put("Server Name", jdbcConnectionURL);
			propertyBag.put("Connection URL", jdbcConnectionURL);

			connInfo.setAttributes(propertyBag);
			connInfo.setPassword(dbUserPassword);
			connInfo.setUserName(dbUserName);
			Tables tables = docDBController.getDatabase().getTables();
			for (int i = 0; i < tables.size(); i++) {
				logger.debug("GenerateReportASImpl: Start->changeDataSourceReplaceConnection; QN = "
						+ tables.get(i).getQualifiedName());
				logger.debug(
						"GenerateReportASImpl: Start->changeDataSourceReplaceConnection; Database = " + dbDataBaseName);
				tables.get(i).setQualifiedName(dbDataBaseName + ".dbo." + tables.get(i).getName());
			}
			int replaceParams = DBOptions._doNotVerifyDB;
			docDBController.replaceConnection(docDBController.getConnectionInfos(null).getConnectionInfo(0), connInfo,
					fields, replaceParams);
		}
		logger.info("GenerateReportASImpl: End->changeDataSourceReplaceConnection");
		return reportClientDoc;
	}
	
	@SuppressWarnings("rawtypes")
	/**
	 * Update data source for a specific database controller
	 * @param docDBController
	 * @throws ReportSDKException
	 */
	private void changeDataSourceReplaceConnection(DatabaseController docDBController) throws ReportSDKException {
		logger.info("GenerateReportASImpl: Start->changeDataSourceReplaceConnection");
		Fields fields = null;
		if (!docDBController.getConnectionInfos(null).isEmpty()) {
			IConnectionInfo connInfo = docDBController.getConnectionInfos(null).getConnectionInfo(0);

			PropertyBag propertyBag = connInfo.getAttributes();
			propertyBag.clear();
			connInfo.setAttributes(propertyBag);
			propertyBag.put("JDBC Connection String", CONNECTION_STRING);
			propertyBag.put("Server Type", SERVERTYPE);
			propertyBag.put("Database DLL", DATABASE_DLL);
			propertyBag.put("Database Class Name", jdbcClassName);
			propertyBag.put("Server Name", jdbcConnectionURL);
			propertyBag.put("Connection URL", jdbcConnectionURL);

			connInfo.setAttributes(propertyBag);
			connInfo.setPassword(dbUserPassword);
			connInfo.setUserName(dbUserName);
			Tables tables = docDBController.getDatabase().getTables();
			for (int i = 0; i < tables.size(); i++) {
				logger.debug("GenerateReportASImpl: Start->changeDataSourceReplaceConnection; QN = "
						+ tables.get(i).getQualifiedName());
				logger.debug(
						"GenerateReportASImpl: Start->changeDataSourceReplaceConnection; Database = " + dbDataBaseName);
				tables.get(i).setQualifiedName(dbDataBaseName + ".dbo." + tables.get(i).getName());
			}
			int replaceParams = DBOptions._doNotVerifyDB;
			docDBController.replaceConnection(docDBController.getConnectionInfos(null).getConnectionInfo(0), connInfo,
					fields, replaceParams);
		}
		logger.info("GenerateReportASImpl: End->changeDataSourceReplaceConnection");
	}
	
	/**
	 * Checking if the report has unmapped parameters
	 * @param reportClientDoc
	 * @param paramToBeChecked
	 * @return true/false
	 * @throws ReportSDKException
	 */
	public boolean isParamLinked(SubreportLinks links, String paramToBeChecked)
			throws ReportSDKException {
		boolean found = false;
		for (ISubreportLink link : links) {
			if (paramToBeChecked.contentEquals(link.getLinkedParameterName())) {
				logger.debug("Parameter: " + paramToBeChecked + " is linked to: " + link.getMainReportFieldName());
				found = true;
			}
		}
		if (found) { // we couldn't find the parameter in the
								// links list
			logger.debug("Parameter: " + paramToBeChecked + " is linked.");
			return true;
		}
		logger.debug("Parameter: " + paramToBeChecked + " is not linked.");
		return false;
	}

	/**
	 * This method will update the main report and the sub-report data access
	 * with the current one used in the application
	 * 
	 * @param reportClientDoc
	 * @param dsUtil
	 * @throws ReportSDKException
	 */
	public ReportClientDocument updateReportDBConnection(ReportClientDocument reportClientDoc) throws ReportSDKException {
		logger.debug("Initial Database Info");
		logDatabaseInfo(reportClientDoc.getDatabaseController());

		Map<String, SubreportLinks> linkMapper = new HashMap<>();
		for (String subreportName : reportClientDoc.getSubreportController().getSubreportNames()) {
			linkMapper.put(subreportName, (SubreportLinks) reportClientDoc.getSubreportController()
					.getSubreportLinks(subreportName).clone(true));
		}
		if (!reportClientDoc.getDatabaseController().getDatabase().getTables().isEmpty()){
			reportClientDoc = changeDataSourceReplaceConnection(reportClientDoc);
			logger.debug("Database Info after changeDataSource");
			logDatabaseInfo(reportClientDoc.getDatabaseController());
		}

		// Change the data source for any sub reports
		IStrings subreportNames = reportClientDoc.getSubreportController().getSubreportNames();
		// Set the data source for all the sub reports.
		logger.debug("Subreports changeDataSource:");
		for (int i = 0; i < subreportNames.size(); i++) {
			logger.debug("Subreport: " + subreportNames.getString(i));
			if (!reportClientDoc.getSubreportController()
					.getSubreport(subreportNames.getString(i)).getDatabaseController().getDatabase().getTables().isEmpty()) {
				logDatabaseInfo(reportClientDoc.getSubreportController()
						.getSubreport(subreportNames.getString(i)).getDatabaseController());
				changeDataSourceReplaceConnection(reportClientDoc.getSubreportController()
						.getSubreport(subreportNames.getString(i)).getDatabaseController());
				logDatabaseInfo(reportClientDoc.getSubreportController()
						.getSubreport(subreportNames.getString(i)).getDatabaseController());
			}
		}

		for (String subreportName : reportClientDoc.getSubreportController().getSubreportNames()) {
			reportClientDoc.getSubreportController().setSubreportLinks(subreportName, linkMapper.get(subreportName));
		}
		
		return reportClientDoc;
	}
}
