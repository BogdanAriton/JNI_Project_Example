package generateReportTestPacakge;

import static org.junit.Assert.*;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

import org.apache.log4j.Logger;

import static org.hamcrest.CoreMatchers.instanceOf;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import generateReportPackage.*;

import com.crystaldecisions.sdk.occa.report.application.OpenReportOptions;
import com.crystaldecisions.sdk.occa.report.application.PrintOutputController;
import com.crystaldecisions.sdk.occa.report.application.ReportClientDocument;
import com.crystaldecisions.sdk.occa.report.exportoptions.ReportExportFormat;
import com.crystaldecisions.sdk.occa.report.lib.ReportSDKException;

public class ReportValidity {

	static final Logger logger = Logger.getLogger(ReportValidity.class);
	
	private static GenerateReportImpl reportImpl = new GenerateReportImpl();
	private static String invalidFont = "WASP,MRV";
	private static String serverName = "usmlvv1srn811\\sql2014";
	private static String dataBaseName = "Soarian_Clin_e1538";
	private static String userName = ""; 
	private static String userPassword = "";
	
	@BeforeClass
	public static void setUpBeforeClass() throws Exception {
		reportImpl.setLogger("./bin/oms-reporting.log", 
				"./bin/sap-reporting.log", "DEBUG", "DEBUG");
	}

	@AfterClass
	public static void tearDownAfterClass() throws Exception {
	}
	
	@Test
	public void TestOpenGoodReport() throws ReportSDKException, InterruptedException {
		String thisReport = "./src/main/resources/page_number.rpt";
		assertThat(reportImpl.openReport(thisReport), instanceOf(ReportClientDocument.class));
	}
	
	@Test (expected = ReportSDKException.class)
	public void TestOpenNoReport() throws ReportSDKException, InterruptedException {
		assertThat(reportImpl.openReport("NoReport"), instanceOf(ReportSDKException.class));
	}
	
	@Test (expected = ReportSDKException.class)
	public void TestOpenBadReport() throws ReportSDKException, InterruptedException {
		String thisReport = "./src/main/resources/badReport.rpt";
		assertThat("", reportImpl.openReport(thisReport), instanceOf(ReportSDKException.class));
	}
	
	@Test
	public void TestForInvalidFont() throws ReportSDKException {
		String thisReport = "./src/main/resources/InvalidFonts.rpt";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		assertEquals("Looking for invalid font in the report.", true, reportImpl.hasInvalidFont(report, invalidFont));
	}
	
	@Test
	public void TestForNoInvalidFont() throws ReportSDKException {
		String thisReport = "./src/main/resources/page_number.rpt";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		assertEquals("The Report should not have invalid font.", false, reportImpl.hasInvalidFont(report, invalidFont));
	}
	
	@Test
	public void TestInvalidPageSize() throws ReportSDKException {
		String thisReport = "./src/main/resources/InvalidPageSize.rpt";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		assertEquals("The report has invalid page size",  true, reportImpl.checkInvalidPageSize(report));
	}
	
	@Test
	public void TestValidPageSize() throws ReportSDKException {
		String thisReport = "./src/main/resources/validPageSize.rpt";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		assertEquals("The report has invalid page size",  false, reportImpl.checkInvalidPageSize(report));
	}
	
	@Test
	public void TestCheckParamterMappings() throws ReportSDKException {
		String thisReport = "./src/main/resources/ParamCheck.rpt";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		DataAccess dataAccess = new DataAccess(serverName, dataBaseName, userName, userPassword);
		assertThat("Checking unmapped params from subreports", dataAccess.checkSubReportParamters(report), instanceOf(ReportClientDocument.class));
	}
	
	@Test
	public void TestUpdateReportDBConnection() throws ReportSDKException {
		String thisReport = "./src/main/resources/testReport.rpt";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		DataAccess dataAccess = new DataAccess(serverName, dataBaseName, userName, userPassword);
		assertThat("Updating DB test.", dataAccess.updateReportDBConnection(report), instanceOf(ReportClientDocument.class));
	}
	
	@Test
	public void TestSetDataSourceParams() throws ReportSDKException {
		String thisReport = "./src/main/resources/exportReport.rpt";
		dataBaseName = "JavaAPITestDB";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		DataAccess dataAccess = new DataAccess(serverName, dataBaseName, userName, userPassword);
		String[] paramNames = new String[1];
		paramNames[0] = "@Value";
		String[] paramValues = new String[1];
		paramValues[0] = "test text";
		assertThat("Checking parameters", dataAccess.setDataSourceParams(report, paramNames, paramValues), instanceOf(ReportClientDocument.class));	
	}
	
	@Test
	public void TestExportToPDF() throws ReportSDKException {
		String thisReport = "./src/main/resources/exportReport.rpt";
		dataBaseName = "JavaAPITestDB";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		DataAccess dataAccess = new DataAccess(serverName, dataBaseName, userName, userPassword);
		report = dataAccess.checkSubReportParamters(report);
		report = dataAccess.updateReportDBConnection(report);
		String[] paramNames = new String[1];
		paramNames[0] = "@Value";
		String[] paramValues = new String[1];
		paramValues[0] = "test text";
		report = dataAccess.setDataSourceParams(report, paramNames, paramValues);
		
		PrintOutputController printOutputCrtl = report.getPrintOutputController();
		ReportExportFormat exportTo = ReportExportFormat.PDF;
		assertTrue(printOutputCrtl.export(exportTo) != null);
	}
	
	@Test
	public void TestExportToRTF() throws ReportSDKException {
		String thisReport = "./src/main/resources/exportReport.rpt";
		dataBaseName = "JavaAPITestDB";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		DataAccess dataAccess = new DataAccess(serverName, dataBaseName, userName, userPassword);
		report = dataAccess.checkSubReportParamters(report);
		report = dataAccess.updateReportDBConnection(report);
		String[] paramNames = new String[1];
		paramNames[0] = "@Value";
		String[] paramValues = new String[1];
		paramValues[0] = "test text";
		report = dataAccess.setDataSourceParams(report, paramNames, paramValues);
		
		PrintOutputController printOutputCrtl = report.getPrintOutputController();
		ReportExportFormat exportTo = ReportExportFormat.RTF;
		assertTrue(printOutputCrtl.export(exportTo) != null);
	}
	
	@Test
	public void TestSaveExportToFile() throws ReportSDKException, IOException {
		String thisReport = "./src/main/resources/exportReport.rpt";
		dataBaseName = "JavaAPITestDB";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		DataAccess dataAccess = new DataAccess(serverName, dataBaseName, userName, userPassword);
		report = dataAccess.checkSubReportParamters(report);
		report = dataAccess.updateReportDBConnection(report);
		String[] paramNames = new String[1];
		paramNames[0] = "@Value";
		String[] paramValues = new String[1];
		paramValues[0] = "test text";
		report = dataAccess.setDataSourceParams(report, paramNames, paramValues);
		
		PrintOutputController printOutputCrtl = report.getPrintOutputController();
		ReportExportFormat exportTo = ReportExportFormat.PDF;
		InputStream result = printOutputCrtl.export(exportTo);
		ReportFormatUtils utils = new ReportFormatUtils();
		String exportLocation = "./bin/exportResult.pdf";
		utils.saveExportToFile(exportLocation, result);
		
		File export = new File(exportLocation);
		assertTrue(export.exists());
	}
}
