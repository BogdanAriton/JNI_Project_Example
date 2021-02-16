package generateReportPackage;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Random;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

import javax.swing.text.BadLocationException;
import javax.swing.text.Document;
import javax.swing.text.rtf.RTFEditorKit;
import org.apache.log4j.Logger;

import com.crystaldecisions.sdk.occa.report.application.ISubreportClientDocument;
import com.crystaldecisions.sdk.occa.report.application.ReportClientDocument;
import com.crystaldecisions.sdk.occa.report.application.ReportDefController;
import com.crystaldecisions.sdk.occa.report.data.Fields;
import com.crystaldecisions.sdk.occa.report.data.IFormulaField;
import com.crystaldecisions.sdk.occa.report.definition.FieldObject;
import com.crystaldecisions.sdk.occa.report.definition.IArea;
import com.crystaldecisions.sdk.occa.report.definition.IReportObject;
import com.crystaldecisions.sdk.occa.report.definition.ISection;
import com.crystaldecisions.sdk.occa.report.definition.ISubreportObject;
import com.crystaldecisions.sdk.occa.report.definition.ITextObject;
import com.crystaldecisions.sdk.occa.report.definition.TextObject;
import com.crystaldecisions.sdk.occa.report.exportoptions.ReportExportFormat;
import com.crystaldecisions.sdk.occa.report.lib.IStrings;
import com.crystaldecisions.sdk.occa.report.lib.ReportObjectKind;
import com.crystaldecisions.sdk.occa.report.lib.ReportSDKException;

public class ReportFormatUtils {
	static final Logger logger = Logger.getLogger(GenerateReportImpl.class);

	/**
	 * Will look for a string in all formulas
	 * 
	 * @param reportClientDoc
	 * @param whatToFind
	 * @return The list of formulas that contains this string
	 * @throws ReportSDKException
	 */
	public List<String> findStringInFormulas(ReportClientDocument reportClientDoc, String whatToFind)
			throws ReportSDKException {
		logger.debug("Looking for formulas that contain: " + whatToFind);
		List<String> sharedVar = new ArrayList<>();
		try {
			Pattern findPage = Pattern.compile(whatToFind);
			Matcher m;
			for (IFormulaField formula : reportClientDoc.getDataDefController().getDataDefinition()
					.getFormulaFields()) {
				m = findPage.matcher(formula.getText());
				while (m.find()) {
					sharedVar.add(m.group());
					logger.debug("Found " + whatToFind + " and returned the variable that uses this: " + m.group());
				}
			}
		} catch (PatternSyntaxException pEx) {
			throw new ReportSDKException(-1, "ReportSDKException caught: " + pEx.getMessage());
		}
		logger.debug("Finished looking for formulas that contain: " + whatToFind);
		return sharedVar;
	}

	/**
	 * @param reportClientDoc
	 * @param pair
	 * @return ISection
	 * @throws ReportSDKException
	 */
	public ISection getSectionByObjectName(ReportClientDocument reportClientDoc, String name, ReportObjectKind kind)
			throws ReportSDKException {
		for (IArea area : reportClientDoc.getReportDefController().getReportDefinition().getAreas()) {
			for (ISection section : area.getSections()) {
				for (IReportObject object : section.getReportObjects()) {
					if (object.getKind() == kind) {
						ISubreportClientDocument foundSubreport = reportClientDoc.getSubreportController()
								.getSubreport(((ISubreportObject) object).getSubreportName());
						if (foundSubreport.getName().equalsIgnoreCase(name)) {
							logger.debug("Found subreports: " + foundSubreport.getName());
							logger.debug("Area: " + area.getName());
							return (ISection) section.clone(true);
						}
					}
				}
			}
		}
		return null;
	}

	/**
	 * @return foundFormulas
	 * @param reportClientDoc
	 * @param getAllVars
	 * @throws ReportSDKException
	 * @throws IOException 
	 */
	public List<String> getFormulasWithText(ReportClientDocument reportClientDoc, List<String> whatToFind)
			throws ReportSDKException, IOException {
		List<String> foundFormulas = new ArrayList<>();

		for (String text : whatToFind) {
			logger.debug("find which formula contains: " + text);
			Fields<IFormulaField> reportFormulas = getAllSubreportsFormulas(reportClientDoc);
			for (IFormulaField formula : reportFormulas) {
				try {
					Pattern findPage = Pattern.compile(text);
					Matcher m = findPage.matcher(formula.getText());
					logger.debug("Lookin in formula: " + formula.getName());
					if (m.find()) {
						logger.debug("Found [" + text + "] in formula: " + formula.getName());

						/* we found a formula that uses this variable */
						/* Add this formula to the main report if it's not there */
						addFormulaToMainReport(reportClientDoc, (IFormulaField)formula.clone(true));
						String formulaName = "{@" + formula.getName() + "}";
						foundFormulas.add(formulaName);
						logger.debug("Formula name added: " + formulaName);
					}
				} catch (PatternSyntaxException pEx) {
					throw new ReportSDKException(-1, "ReportSDKException caught: " + pEx.getMessage());
				}
			}
		}
		return foundFormulas;
	}
	
	/**
	 * This method will just add a new formula to the report if it doesn't exists
	 * @param reportClientDoc
	 * @param newFormula
	 * @return
	 * @throws ReportSDKException
	 */
	private boolean addFormulaToMainReport(ReportClientDocument reportClientDoc, IFormulaField newFormula) throws ReportSDKException{
		/* Check to see if the formula exists */
		for (IFormulaField formula : reportClientDoc.getDataDefinition().getFormulaFields()){
			if (formula.getName().equalsIgnoreCase(newFormula.getName())){
				/* Exist if we found the formula we want to add */
				return false;
			}
		}
		logger.debug("Adding formula " + newFormula.getName() + " to main report.");
		reportClientDoc.getDataDefController().getFormulaFieldController().add(newFormula);
		return true;
	}
	
	/**
	 * Will return a list of all formulas from the report
	 * @param reportClientDoc
	 * @return
	 */
	private Fields<IFormulaField> getAllSubreportsFormulas(ReportClientDocument reportClientDoc) throws ReportSDKException{
		Fields<IFormulaField> returnFields = new Fields<>();
		logger.debug("Start - Getting all formulas from this report.");
		
		/* Add formulas from sub-reports */
		IStrings subreportNames = reportClientDoc.getSubreportController().getSubreportNames();
		/* Cycle trough sub-reports */
		for (int i = 0; i < subreportNames.size(); i++) {
			ISubreportClientDocument subreportClientDoc = reportClientDoc.getSubreportController()
					.getSubreport(subreportNames.getString(i));
			for (IFormulaField formula : subreportClientDoc.getDataDefController().getDataDefinition()
					.getFormulaFields()){
				logger.debug("adding formula: " + formula.getName());
				returnFields.add(formula);
			}
		}
		
		logger.debug("Stop - Getting all formulas from this report.");
		return returnFields;
	}

	private Fields<IFormulaField> getFormulasFromSubReport(ISubreportClientDocument subRpt) throws ReportSDKException{
		Fields<IFormulaField> returnFields = new Fields<>();
		logger.debug("Start - Getting formulas from this subreport.");
		for (IFormulaField formula : subRpt.getDataDefController().getDataDefinition()
				.getFormulaFields()){
			logger.debug("adding formula: " + formula.getName());
			returnFields.add(formula);
		}
		logger.debug("Stop - Getting formulas from this subreport.");
		return returnFields;
	}

	/**
	 * Will check a HashMap<IReportObject, String> for duplicates
	 * 
	 * @param <IReportObject>
	 * @param <String>
	 * @param objectsFound
	 * @param objectCopy
	 * @return
	 */
	private boolean canBeAddedToMap(HashMap<IReportObject, String> mapToCheck, IReportObject objectToCheck) {
		Iterator<Entry<IReportObject, String>> it = mapToCheck.entrySet().iterator();
		while (it.hasNext()) {
			Map.Entry<IReportObject, String> pair = it.next();
			/*
			 * checking if we already have the object in the map
			 */
			if (pair.getKey().getName().equalsIgnoreCase(objectToCheck.getName())) {
				return false;
			}
		}
		return true;
	}

	/**
	 * Will generate a map of ReportObjects from all found sub-reports that
	 * contains text found in the filter list
	 * 
	 * @param reportClientDoc
	 * @param filters
	 * @return Map< T , L >
	 * @throws ReportSDKException
	 */
	public Map<IReportObject, String> getAllObjectsWithText(ReportClientDocument reportClientDoc, List<String> filters)
			throws ReportSDKException {
		HashMap<IReportObject, String> objectsFound = new HashMap<>();
		IStrings subreportNames = reportClientDoc.getSubreportController().getSubreportNames();
		/* Cycle trough sub-reports */
		for (int i = 0; i < subreportNames.size(); i++) {
			ISubreportClientDocument subreportClientDoc = reportClientDoc.getSubreportController()
					.getSubreport(subreportNames.getString(i));
			List<IReportObject> objectFileds = getObjectsByType(subreportClientDoc.getReportDefController(),
					ReportObjectKind.field);

			for (IReportObject object : objectFileds) {
				FieldObject field = (FieldObject) object;
				for (String filter : filters) {
					if (canBeAddedToMap(objectsFound, object) && field.getDataSourceName().equalsIgnoreCase(filter)) {
						IReportObject objectCopy = (IReportObject) field.clone(true);
						/*
						 * Add the object to the map with the sub-report that he
						 * is part of
						 */
						objectsFound.put(objectCopy, subreportNames.getString(i));
						logger.debug("Object: " + field.getName() + " will be added to the map.");
					}
				}
			}
			List<IReportObject> objectTexts = getObjectsByType(subreportClientDoc.getReportDefController(),
					ReportObjectKind.text);
			for (IReportObject object : objectTexts) {
				ITextObject text = (TextObject) object;
				for (String filter : filters) {
					if (canBeAddedToMap(objectsFound, object) && text.getText().contains(filter)) {
						IReportObject objectCopy = (IReportObject) text.clone(true);
						objectsFound.put(objectCopy, subreportNames.getString(i));
						logger.debug("Object: " + text.getName() + " will be added to the map.");
					}
				}
			}
		}
		return objectsFound;
	}

	/**
	 * Gets a list of report objects from a report or sub-report definition
	 * controller, based on a specified report object type
	 * 
	 * @return foundFormulas
	 * @param reportClientDoc
	 * @param getAllVars
	 * @throws ReportSDKException
	 */
	public List<IReportObject> getObjectsByType(ReportDefController thisDocumentDefController, ReportObjectKind kind)
			throws ReportSDKException {
		List<IReportObject> filedObjects = new ArrayList<>();
		for (IArea area : thisDocumentDefController.getReportDefinition().getAreas()) {
			for (ISection section : area.getSections()) {
				for (IReportObject object : section.getReportObjects()) {
					if (object.getKind() == kind) {
						filedObjects.add((IReportObject) object.clone(true));
					}
				}
			}
		}
		return filedObjects;
	}

	/**
	 * Will parse a report and will check the text for all Text Object elements
	 * to see if a specific list of fonts is being used The fonts string will be
	 * a comma separated list of names
	 * 
	 * @param reportDefController
	 * @param fonts
	 * @return
	 * @throws ReportSDKException
	 */
	private boolean checkTextForInvalidFont(ReportDefController reportDefController, String fonts)
			throws ReportSDKException {
		for (IArea area : reportDefController.getReportDefinition().getAreas()) {
			for (ISection section : area.getSections()) {
				for (IReportObject object : section.getReportObjects()) {
					if (object.getKind() == ReportObjectKind.text) {
						TextObject thisText = (TextObject) object;
						String filedfont = thisText.getFontColor().getIFont().getName();
						String[] invalidFonts = fonts.split(",");
						for (String str : invalidFonts) {
							logger.debug("Compare this font: " + str + " with font of object: " + object.getName() + " font: " + filedfont);
							if (filedfont.contains(str)) {
								// found invalid font
								logger.info("isInvalidFont - found invalid font: " + str);
								return true;
							}
						}
					}
				}
			}
		}
		return false;
	}

	/**
	 * 
	 * @param reportDefController
	 * @param fonts
	 * @return
	 * @throws ReportSDKException
	 */
	private boolean checkFieldForInvalidFont(ReportDefController reportDefController, String fonts)
			throws ReportSDKException {
		for (IArea area : reportDefController.getReportDefinition().getAreas()) {
			for (ISection section : area.getSections()) {
				for (IReportObject object : section.getReportObjects()) {
					if (object.getKind() == ReportObjectKind.field) {
						FieldObject thisFieldObj = (FieldObject) object;
						thisFieldObj.getFontColor().getFont().getFontName();
						String textfont = thisFieldObj.getFontColor().getIFont().getName();
						String[] invFonts = fonts.split(",");
						for (String str : invFonts) {
							logger.debug("Compare this font: " + str + " with font of object: " + object.getName() + " font: " + textfont);
							if (textfont.contains(str)) {
								// found invalid font
								logger.info("isInvalidFont - found invalid font: " + str);
								return true;
							}
						}
					}
				}
			}
		}
		return false;
	}

	/**
	 * 
	 * @param reportClientDoc
	 * @param fonts
	 * @return
	 * @throws ReportSDKException
	 */
	public boolean isInvalidFont(ReportClientDocument reportClientDoc, String fonts) throws ReportSDKException {
		logger.info("isInvalidFont - Start iteration trough report areas.");

		/* Check fonts on the main report */
		if (checkTextForInvalidFont(reportClientDoc.getReportDefController(), fonts)
				|| checkFieldForInvalidFont(reportClientDoc.getReportDefController(), fonts)) {
			return true;
		}

		/* Check fonts on each sub-reports */
		for (int i = 0; i < reportClientDoc.getSubreportController().getSubreportNames().size(); i++) {
			ISubreportClientDocument subreportClientDoc = reportClientDoc.getSubreportController()
					.getSubreport(reportClientDoc.getSubreportController().getSubreportNames().getString(i));
			if (checkTextForInvalidFont(subreportClientDoc.getReportDefController(), fonts)
					|| checkFieldForInvalidFont(subreportClientDoc.getReportDefController(), fonts)) {
				return true;
			}
		}
		return false;
	}

	/**
	 * 
	 * @param exportFormatIn
	 * @return
	 */
	public ReportExportFormat determineExportFormat(String exportFormatIn) {
		logger.debug("export format in = " + exportFormatIn);
		if (exportFormatIn.toLowerCase().contains("PDF".toLowerCase())) {
			return ReportExportFormat.PDF;
		}
		if (exportFormatIn.toLowerCase().contains("RTF".toLowerCase())) {
			return ReportExportFormat.RTF;
		}
		if (exportFormatIn.toLowerCase().contains("txt".toLowerCase())) {
			return ReportExportFormat.RTF;
		}
		if (exportFormatIn.toLowerCase().contains("csv".toLowerCase())) {
			return ReportExportFormat.characterSeparatedValues;
		}
		if (exportFormatIn.toLowerCase().contains("xls".toLowerCase())) {
			return ReportExportFormat.recordToMSExcel;
		}
		if (exportFormatIn.toLowerCase().contains("xml".toLowerCase())) {
			return ReportExportFormat.XML;
		}
		// will use PDF by default
		return ReportExportFormat.PDF;
	}

	/**
	 * @param exportFileName
	 * @param myIS
	 * @throws FileNotFoundException
	 * @throws IOException
	 */
	public void saveExportToFile(String exportFileName, InputStream myIS) throws IOException {
		ByteArrayInputStream byteArrayInputStream = (ByteArrayInputStream) myIS;
		// Use the Java I/O libraries to write the exported content to the file
		// system.
		byte[] byteArray = new byte[byteArrayInputStream.available()];
		// Create a new file that will contain the exported result.
		logger.info("File to export: " + exportFileName);

		File file = new File(exportFileName);
		FileOutputStream fileOutputStream = new FileOutputStream(file);

		ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream(byteArrayInputStream.available());
		int x = byteArrayInputStream.read(byteArray, 0, byteArrayInputStream.available());

		byteArrayOutputStream.write(byteArray, 0, x);
		byteArrayOutputStream.writeTo(fileOutputStream);

		byteArrayInputStream.close();
		byteArrayOutputStream.close();
		fileOutputStream.close();
	}

	/**
	 * @param exportFileName
	 * @param myIS
	 * @param extension
	 * @throws FileNotFoundException
	 * @throws IOException
	 * @throws BadLocationException 
	 */
	public void saveExportToFile(String exportFileName, InputStream myIS, String extension) throws IOException, BadLocationException {
		if (extension.toLowerCase().contains("txt")) {
			RTFEditorKit rtfParser = new RTFEditorKit();
			Document document = rtfParser.createDefaultDocument();
			rtfParser.read(myIS, document, 0);
			
			logger.info("File to export: " + exportFileName);
			FileWriter fw = new FileWriter(exportFileName);
			fw.write(document.getText(0, document.getLength()));
			fw.close();
			
		} else {
			ByteArrayInputStream byteArrayInputStream = (ByteArrayInputStream) myIS;
			// Use the Java I/O libraries to write the exported content to the
			// file
			// system.
			byte[] byteArray = new byte[byteArrayInputStream.available()];
			// Create a new file that will contain the exported result.
			logger.info("File to export: " + exportFileName);

			File file = new File(exportFileName);
			FileOutputStream fileOutputStream = new FileOutputStream(file);

			ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream(byteArrayInputStream.available());
			int x = byteArrayInputStream.read(byteArray, 0, byteArrayInputStream.available());

			byteArrayOutputStream.write(byteArray, 0, x);
			byteArrayOutputStream.writeTo(fileOutputStream);

			byteArrayInputStream.close();
			byteArrayOutputStream.close();
			fileOutputStream.close();
		}
	}

	/**
	 * This method will look for all PageNofM parameters that will be used in
	 * sub-reports and will attempt to move them under the main report.
	 * 
	 * @param reportClientDoc
	 * @throws ReportSDKException
	 * @throws IOException
	 */
	public ReportClientDocument resetPageNumberingToMain(ReportClientDocument reportClientDoc) throws ReportSDKException, IOException {
		List<String> getAllVars = findStringInFormulas(reportClientDoc, ".*=.*PageNofM");
		List<String> whatToFind = new ArrayList<>();
		if (!getAllVars.isEmpty()) {
			for (String var : getAllVars) {
				whatToFind.add("formula.*=.*" + var.split("=")[0].trim());
				whatToFind.add("PageNofM");
			}
		}
		List<String> foundFormulas = getFormulasWithText(reportClientDoc, whatToFind);
		foundFormulas.add("PageNofM");
		foundFormulas.add("PageNumber");

		HashMap<IReportObject, String> listOfpageNofM = (HashMap<IReportObject, String>) getAllObjectsWithText(reportClientDoc, foundFormulas);
		Iterator<Entry<IReportObject, String>> it = listOfpageNofM.entrySet().iterator();
		while (it.hasNext()) {
			Map.Entry<IReportObject, String> pair = it.next();
			logger.debug(pair.getKey().getName() + " from subreport " + pair.getValue());
		
			/* get the section of the sub-report */
			ISection thisSection = getSectionByObjectName(reportClientDoc, pair.getValue(),
					ReportObjectKind.subreport);
			ISubreportObject subReportObject = getSubreportFromSectionByName(pair.getValue(), thisSection);
			
			/* Adding the object to the section in the report */
			if (thisSection != null && subReportObject != null) {
				IReportObject tempObject = (IReportObject) pair.getKey().clone(true);
				
				/* If this is text object we only want to keep the text with our page numbering formula */
				
				Random r = new Random();
				Integer randomNumber = r.nextInt(99999);
				tempObject.setName(pair.getKey().getName() + pair.getValue().substring(0, pair.getValue().length() - 4) + randomNumber.toString());
				/* We need to readjusts the position of the object based on where the sub-report is situated in the section  */
				int top = tempObject.getTop() + subReportObject.getTop() + getSizeForPreviousSections(reportClientDoc, pair.getValue(), pair.getKey().getName());
				tempObject.setTop(top);
				logger.debug("Object position top: " + top);
				tempObject.setLeft(tempObject.getLeft() + subReportObject.getLeft());
				
				
				/* need to add formula to the main report if it's not there */
				reportClientDoc = addMissingFormulas(reportClientDoc, tempObject, pair.getValue());
				logger.debug("Adding " + tempObject.getName() + " to section " + thisSection.getName());
				reportClientDoc.getReportDefController().getReportObjectController().add(tempObject, thisSection, -1);
				
				/* remove the found objects from the each sub-report */
				reportClientDoc.getSubreportController().getSubreport(pair.getValue()).getReportDefController()
				.getReportObjectController().remove(pair.getKey());
			}
		}
		return reportClientDoc;
	}
	
	private int getSizeForPreviousSections(ReportClientDocument reportClientDoc, String subReport, String pageNumbObjName) throws ReportSDKException {
		int preSectionsSize = 0;
		ISubreportClientDocument subreportClientDoc = reportClientDoc.getSubreportController()
				.getSubreport(subReport);
		for (IArea area : subreportClientDoc.getReportDefController().getReportDefinition().getAreas()) {
			logger.debug("area name: " + area.getName());
			logger.debug("Is area suppressed: " + area.getFormat().getEnableSuppress());
			if (area.getFormat().getEnableSuppress()) {
				continue;
			}
			for (ISection section : area.getSections()) {
				logger.debug("section name: " + section.getName());
				logger.debug(section.getHeight());
				logger.debug("Is section supressed: " + section.getFormat().getEnableSuppress());
				logger.debug("Is section supressed if blank: " + section.getFormat().getEnableSuppressIfBlank());
				logger.debug("Does section has content: " + section.getReportObjects().size());
				for (IReportObject object : section.getReportObjects()) {
					if (object.getName().equalsIgnoreCase(pageNumbObjName)) {
						return preSectionsSize;
					}
				}
				if (!section.getFormat().getEnableSuppress()){ // && !section.getFormat().getEnableSuppressIfBlank()
					preSectionsSize = preSectionsSize + section.getHeight();
				}
			}
		}
		
		return preSectionsSize;
	}
	
	/**
	 * Will attempt to update the report with all the formulas found in addition to the page numbering formulas from the report object text with
	 * page numbering formulas.
	 * Example: There can be a text on the report that contains more than one formula, like: {@fromula2} {@fromula1} {@fromula7} {@fromula9}
	 * @param reportClientDoc
	 * @param rptObjectText
	 * @return
	 * @throws ReportSDKException
	 */
	@SuppressWarnings("unused")
	private ReportClientDocument updateMainRptWithMissingFormulas(ReportClientDocument reportClientDoc, IReportObject rptObjectText) throws ReportSDKException{
		if (rptObjectText.getKind() == ReportObjectKind.text){
			ITextObject text = (TextObject) rptObjectText;
			Pattern findFormula = Pattern.compile("[{][@].*?[}]");
			Matcher m = findFormula.matcher(text.getText());
			Fields<IFormulaField> subReportFormulas =  getAllSubreportsFormulas(reportClientDoc);
			while (m.find()){
				for (IFormulaField formula : subReportFormulas){
					logger.debug("Pattern found: " + m.group());
					if (m.group().equalsIgnoreCase("{@" + formula.getName() + "}")){
						addFormulaToMainReport(reportClientDoc, (IFormulaField)formula.clone(true));
					}
				}
			}
		}
		return reportClientDoc;
	}
	
	/**
	 * Will attempt to update the report with all the formulas found in addition to the page numbering formulas from the report object text with
	 * page numbering formulas.
	 * Example: There can be a text on the report that contains more than one formula, like: {@fromula2} {@fromula1} {@fromula7} {@fromula9}
	 * @param reportClientDoc
	 * @param rptObjectText
	 * @param subReportName - will get only formulas from the sub-report in question
	 * @return reportClientDoc
	 * @throws ReportSDKException
	 */
	private ReportClientDocument addMissingFormulas(ReportClientDocument reportClientDoc, IReportObject rptObjectText, String subReportName) throws ReportSDKException{
		if (rptObjectText.getKind() == ReportObjectKind.text){
			ITextObject text = (TextObject) rptObjectText;
			Pattern findFormula = Pattern.compile("[{][@].*?[}]");
			Matcher m = findFormula.matcher(text.getText());
			ISubreportClientDocument subreportClientDoc = reportClientDoc.getSubreportController()
					.getSubreport(subReportName);
			Fields<IFormulaField> subReportFormulas =  getFormulasFromSubReport(subreportClientDoc);
			while (m.find()){
				for (IFormulaField formula : subReportFormulas){
					logger.debug("Pattern found: " + m.group());
					if (m.group().equalsIgnoreCase("{@" + formula.getName() + "}")){
						addFormulaToMainReport(reportClientDoc, (IFormulaField)formula.clone(true));
					}
				}
			}
		}
		return reportClientDoc;
	}

	/**
	 * Get a sub-report object based on the sub-report name from a section
	 * @param subReportName
	 * @param thisSection
	 * @return
	 */
	private ISubreportObject getSubreportFromSectionByName(String subReportName, ISection thisSection) {
		ISubreportObject subRpt = null;
		for (IReportObject obj : thisSection.getReportObjects()){
			if (obj.getKind() == ReportObjectKind.subreport){
				subRpt = (ISubreportObject) obj.clone(true);
				if (subRpt.getSubreportName().equalsIgnoreCase(subReportName)){
					return subRpt;
				}
			}
		}
		return subRpt;
	}
	
	/**
	 * Checks the extension the file we want to export to and updates if the
	 * export format and the extension doesn't match. The export format is given
	 * trough the extension parameter
	 * 
	 * @param fileName
	 * @param extension
	 * @return The updated file location
	 * @throws ReportSDKException
	 */
	public String updateExtension(String fileName, String extension) throws ReportSDKException {
		try {
			Pattern findExt = Pattern.compile("(?<=\\.)\\w+$");
			Matcher m = findExt.matcher(fileName);
			if (m.find()) {
				if (!m.group(0).equalsIgnoreCase(extension)) {
					return m.replaceFirst(extension);
				}
			} else if (fileName.endsWith(".")) {
				return fileName + extension;
			}
		} catch (PatternSyntaxException pEx) {
			throw new ReportSDKException(-1, "PatternSyntaxException caught: " + pEx.getMessage());
		}
		return fileName;
	}
	
	/**
	 * Check the log file location for validity
	 * @param logFilePath
	 * @param thisLogFile
	 */
	public void validateLogFilePath(String logFilePath, String thisLogFile) throws ReportSDKException
	{
	    if (!thisLogFile.equalsIgnoreCase(logFilePath)) {
	        Pattern p = Pattern.compile("[^\\/:*?\\\"<>|\\r\\n]+\\.\\w{3}$");
	        String logFilename = "";
	        Matcher m = p.matcher(logFilePath);
	        if (m.find()) {
	            logFilename = m.group(0);
	            String logFolderPath = m.replaceFirst(""); // remove filename

	            File loggerFolder = new File(logFolderPath);
	            if (!loggerFolder.isDirectory()) {
	                try {
	                    loggerFolder.mkdirs();
	                } catch (SecurityException sec) {
	                    throw new ReportSDKException(-1, sec.getMessage());
	                }
	            }
	        }
	        if (logFilename == "") {
	            throw new ReportSDKException(-1,
	            "The log file location is incorrect. File name missing! Please check your settings and retry.");
	        }
	    }
	}	
}
