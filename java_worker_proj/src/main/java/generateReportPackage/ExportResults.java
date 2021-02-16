package generateReportPackage;

import java.io.InputStream;
import java.io.PrintWriter;
import java.io.StringWriter;

import org.apache.log4j.Logger;

import com.crystaldecisions.sdk.occa.report.application.PrintOutputController;
import com.crystaldecisions.sdk.occa.report.exportoptions.ReportExportFormat;
import com.crystaldecisions.sdk.occa.report.lib.ReportSDKException;

public class ExportResults implements Runnable {

    private volatile InputStream resultStream = null;
    public String strResult = "";
    private PrintOutputController printOutputCrtl;
    private ReportExportFormat exportFormat = ReportExportFormat.PDF;
    final static Logger logger = Logger.getLogger(ExportResults.class);

    public ExportResults(PrintOutputController printOutputCrtl, ReportExportFormat exportFormat) {
        logger.info("Thread started, thread id = " + Thread.currentThread().getId());
        setPrintOutputCrtl(printOutputCrtl);
        setExportFormat(exportFormat);
    }

    @Override
    public void run() {
        try {
            InputStream resultStreamLocal = null;
            synchronized (ExportResults.class) {
                resultStreamLocal = printOutputCrtl.export(exportFormat);
                setResultStream(resultStreamLocal);
            }
        } catch (ReportSDKException rse) {
            // DB is unavailable (DB is offline or DB name is not right in JNDI
            StringWriter sw = new StringWriter();
            rse.printStackTrace(new PrintWriter(sw));
            strResult = sw.toString();
            // logger.debug(strResult);

            RuntimeException e = new RuntimeException(strResult);
            e.setStackTrace(rse.getStackTrace());

            throw e;

        } finally {
            logger.info("Thread ended, thread id = " + Thread.currentThread().getId());
        }
    }

    public InputStream getResultStream() {
        return resultStream;
    }

    public void setResultStream(InputStream resultStream) {
        this.resultStream = resultStream;
    }

    public PrintOutputController getPrintOutputCrtl() {
        return printOutputCrtl;
    }

    public void setPrintOutputCrtl(PrintOutputController printOutputCrtl) {
        this.printOutputCrtl = printOutputCrtl;
    }

    public ReportExportFormat getExportFormat() {
        return exportFormat;
    }

    public void setExportFormat(ReportExportFormat exportFormat) {
        this.exportFormat = exportFormat;
    }
}
