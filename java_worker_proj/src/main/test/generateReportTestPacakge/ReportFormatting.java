package generateReportTestPacakge;

import static org.junit.Assert.*;

import java.io.IOException;

import org.apache.log4j.Logger;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import static org.hamcrest.CoreMatchers.instanceOf;

import com.crystaldecisions.sdk.occa.report.application.OpenReportOptions;
import com.crystaldecisions.sdk.occa.report.application.ReportClientDocument;
import com.crystaldecisions.sdk.occa.report.lib.ReportSDKException;

import generateReportPackage.GenerateReportImpl;
import generateReportPackage.ReportFormatUtils;

public class ReportFormatting {
	
	static final Logger logger = Logger.getLogger(ReportFormatting.class);
	private static GenerateReportImpl reportImpl = new GenerateReportImpl();
	
	@BeforeClass
	public static void setUpBeforeClass() throws Exception {
		reportImpl.setLogger("./bin/oms-reporting.log", 
				"./bin/sap-reporting.log", "DEBUG", "DEBUG");
	}

	@AfterClass
	public static void tearDownAfterClass() throws Exception {
	}

	@Test
	public void TestResetPageNumbering() throws ReportSDKException, IOException {
		String thisReport = "./src/main/resources/page_number.rpt";
		ReportClientDocument report = new ReportClientDocument();
		report.open(thisReport, OpenReportOptions._discardSavedData);
		ReportFormatUtils utils = new ReportFormatUtils();
		
		assertThat("Page number check.", utils.resetPageNumberingToMain(report), instanceOf(ReportClientDocument.class));
	}

}
