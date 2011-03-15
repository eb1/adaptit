/////////////////////////////////////////////////////////////////////////////
/// \project		adaptit
/// \file			Adapt_ItDoc.cpp
/// \author			Bill Martin
/// \date_created	05 January 2004
/// \date_revised	15 January 2008
/// \copyright		2008 Bruce Waters, Bill Martin, SIL International
/// \license		The Common Public License or The GNU Lesser General Public License (see license directory)
/// \description	This is the implementation file for the CAdapt_ItDoc class. 
/// The CAdapt_ItDoc class implements the storage structures and methods 
/// for the Adapt It application's persistent data. Adapt It's document 
/// consists mainly of a list of CSourcePhrases stored in order of occurrence 
/// of source text words. The document's data structures are kept logically 
/// separate from and independent of the view class's in-memory data structures. 
/// This schema is an implementation of the document/view framework. 
/// \derivation		The CAdapt_ItDoc class is derived from wxDocument.
/////////////////////////////////////////////////////////////////////////////

//#define _debugLayout

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma implementation "Adapt_ItDoc.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include <wx/wxprec.h>

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
// Include your minimal set of headers here, or wx.h
#include <wx/wx.h>
#endif

#include <wx/docview.h>	// includes wxWidgets doc/view framework
#include "Adapt_ItCanvas.h"
#include "Adapt_It_Resources.h"
#include <wx/filesys.h>
#include <wx/file.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h> // for wxZipInputStream & wxZipOutputStream
#include <wx/datstrm.h> // permanent
#include <wx/txtstrm.h> // temporary
#include <wx/mstream.h> // for wxMemoryInputStream
#include <wx/font.h> // temporary
#include <wx/fontmap.h> // temporary
#include <wx/fontenum.h> // temporary
#include <wx/list.h>
#include <wx/tokenzr.h>
#include <wx/progdlg.h>
#include <wx/busyinfo.h>

#if !defined(__APPLE__)
#include <malloc.h>
#else
#include <malloc/malloc.h>
#endif

// The following are from IBM's International Components for Unicode (icu) used under the LGPL license.
//#include "csdetect.h" // used in GetNewFile(). 
//#include "csmatch.h" // " "

// Other includes uncomment as implemented
#include "Adapt_It.h"
#include "OutputFilenameDlg.h"
#include "helpers.h"
#include "MainFrm.h"
#include "SourcePhrase.h"
#include "KB.h"
#include "AdaptitConstants.h"
#include "TargetUnit.h"
#include "Adapt_ItView.h"
#include "Strip.h"
#include "Pile.h" // must precede the include for the document
#include "Cell.h"
#include "Adapt_ItDoc.h"
#include "RefString.h"
#include "RefStringMetadata.h"
//#include "ProgressDlg.h" // removed in svn revision #562
#include "WaitDlg.h"
#include "XML.h"
#include "MoveDialog.h"
#include "SplitDialog.h"
#include "JoinDialog.h"
#include "UnpackWarningDlg.h"
#include "Layout.h"
#include "FreeTrans.h"
#include "Notes.h"
#include "ExportFunctions.h"
#include "ReadOnlyProtection.h"
#include "ConsistencyCheckDlg.h"
#include "ChooseConsistencyCheckTypeDlg.h" //whm added 9Feb04
#include "NavProtectNewDoc.h"

// GDLC Removed conditionals for PPC Mac (with gcc4.0 they are no longer needed)
void init_utf8_char_table();
const char* tellenc(const char* const buffer, const size_t len);

// struct for storing auto-fix inconsistencies when doing "Consistency Check..." menu item;
// for glossing we can use the same structure with the understanding that the oldAdaptation
// and finalAdaptation will in reality contain the old gloss and the final gloss,
// respectively; and nWords for glossing will always be 1.
struct	AutoFixRecord
{
	wxString	key;
	wxString	oldAdaptation;
	wxString finalAdaptation;
	int		nWords;
};

// Define type safe pointer lists
#include "wx/listimpl.cpp"

/// This macro together with the macro list declaration in the .h file
/// complete the definition of a new safe pointer list class called AFList.
WX_DEFINE_LIST(AFList);

/// This global is defined in Adapt_ItView.cpp.
extern bool gbVerticalEditInProgress;

/// This global is defined in Adapt_ItView.cpp.
extern EditRecord gEditRecord; // defined at start of Adapt_ItView.cpp

/// This global is defined in Adapt_It.cpp.
extern enum TextType gPreviousTextType; // moved to global space in the App, made extern here

// Other declarations from MFC version below

/// Length of the byte-order-mark (BOM) which consists of the three bytes 0xEF, 0xBB and 0xBF
/// in UTF-8 encoding.
#define nBOMLen 3

/// Length of the byte-order-mark (BOM) which consists of the two bytes 0xFF and 0xFE in
/// in UTF-16 encoding.
#define nU16BOMLen 2

#ifdef _UNICODE

/// The UTF-8 byte-order-mark (BOM) consists of the three bytes 0xEF, 0xBB and 0xBF
/// in UTF-8 encoding. Some applications like Notepad prefix UTF-8 files with
/// this BOM.
//static wxUint8 szBOM[nBOMLen] = {0xEF, 0xBB, 0xBF}; // MFC uses BYTE

/// The UTF-16 byte-order-mark (BOM) which consists of the two bytes 0xFF and 0xFE in
/// in UTF-16 encoding.
//static wxUint8 szU16BOM[nU16BOMLen] = {0xFF, 0xFE}; // MFC uses BYTE

#endif

/// This global is defined in Adapt_ItView.cpp. 
extern int	gnBeginInsertionsSequNum;	

/// This global is defined in Adapt_ItView.cpp. 
extern int	gnEndInsertionsSequNum;	

/// This global boolean informs the Doc's BackupDocument() function whether a split or 
/// join operation is in progress. If gbDoingSplitOrJoin is TRUE BackupDocument() exits 
/// immediately without performing any backup operations. Split operations especially 
/// could produce a plethora of backup docs, especially for a single-chapters document split.
bool gbDoingSplitOrJoin = FALSE; // TRUE during one of these 3 operations

/// This global is defined in DocPage.cpp. 
extern bool gbMismatchedBookCode; // BEW added 21Mar07

/// This global is defined in Adapt_It.cpp.
extern bool	gbTryingMRUOpen; // see Adapt_It.cpp globals list for an explanation

/// This global is defined in MainFrm.cpp.
extern bool gbIgnoreScriptureReference_Receive;

/// This global is defined in MainFrm.cpp.
extern bool gbIgnoreScriptureReference_Send;

/// This global is used only in RetokenizeText() to increment the number n associated with
/// the final number n composing the "Rebuild Logn.txt" files, which inform the user of
/// any problems encountered during document rebuilding.
int gnFileNumber = 0; // used for output of Rebuild Logn.txt file, to increment n each time

/// This global is defined in TransferMarkersDlg.cpp.
extern bool gbPropagationNeeded;

/// This global is defined in TransferMarkersDlg.cpp.
extern TextType gPropagationType;

// This global is defined in Adapt_ItView.cpp.  BEW removed 27Jan09
extern bool gbInhibitMakeTargetStringCall; // see view for reason for this

/// This global is defined in Adapt_ItView.cpp.
extern bool gbIsUnstructuredData;

// next four are for version 2.0 which includes the option of a 3rd line for glossing

/// This global is defined in Adapt_ItView.cpp.
extern bool	gbIsGlossing; // when TRUE, the phrase box and its line have glossing text

/// This global is defined in Adapt_ItView.cpp.
extern bool	gbEnableGlossing; // TRUE makes Adapt It revert to Shoebox functionality only

/// This global is defined in Adapt_ItView.cpp.
extern bool gbGlossingUsesNavFont;

/// This global is defined in DocPage.cpp.
extern bool  gbForceUTF8; // defined in CDocPage

/// This global is defined in Adapt_It.cpp.
extern wxChar gSFescapechar; // the escape char used for start of a standard format marker

// BEW 8Jun10, removed support for checkbox "Recognise standard format
// markers only following newlines"
// This global is defined in Adapt_It.cpp.
//extern bool  gbSfmOnlyAfterNewlines;

/// This global is defined in Adapt_It.cpp.
extern bool  gbDoingInitialSetup;

/// This global is defined in Adapt_It.cpp.
extern CAdapt_ItApp* gpApp; // if we want to access it fast

/// Indicates if a source word or phrase is to be considered special text when the propagation
/// of text attributes needs to be considered as after editing the source text or after rebuilding
/// the source text subsequent to filtering changes. Normally used to set or store the m_bSpecialText 
/// attribute of a source phrase instance.
bool   gbSpecialText = FALSE;

/// This global is defined in Adapt_ItView.cpp.
extern CSourcePhrase* gpPrecSrcPhrase; 

/// This global is defined in Adapt_ItView.cpp.
extern CSourcePhrase* gpFollSrcPhrase;

/// This global is defined in Adapt_ItView.cpp.
extern	bool	gbShowTargetOnly;

/// This global is defined in Adapt_ItView.cpp.
extern	int		gnSaveLeading;

/// This global is defined in Adapt_ItView.cpp.
extern	int		gnSaveGap;

/// This global is defined in PhraseBox.cpp.
extern	wxString	translation;

/// Indicates if the user has cancelled an operation.
bool	bUserCancelled = FALSE;

/// This global is defined in Adapt_It.cpp.
extern	bool	gbViaMostRecentFileList;

/// This global is defined in Adapt_ItView.cpp.
extern	bool	gbConsistencyCheckCurrent;

/// This global is defined in Adapt_ItView.cpp.
extern	int		gnOldSequNum;

/// This global is defined in Adapt_It.cpp.
extern	bool	gbAbortMRUOpen;

/// This global is defined in Adapt_It.cpp.
extern bool		gbPassedAppInitialization;

/// This global is defined in Adapt_It.cpp.
extern wxString szProjectConfiguration;

/// This global is defined in Adapt_It.cpp.
extern wxString szAdminProjectConfiguration;

/// This global is defined in Adapt_It.cpp.
extern bool gbHackedDataCharWarningGiven;

// globals needed due to moving functions here from mainly the view class
// next group for auto-capitalization support
extern bool	gbAutoCaps;
extern bool	gbSourceIsUpperCase;
extern bool	gbNonSourceIsUpperCase;
extern bool	gbMatchedKB_UCentry;
extern bool	gbNoSourceCaseEquivalents;
extern bool	gbNoTargetCaseEquivalents;
extern bool	gbNoGlossCaseEquivalents;
extern wxChar gcharNonSrcLC;
extern wxChar gcharNonSrcUC;
extern wxChar gcharSrcLC;
extern wxChar gcharSrcUC;

bool	gbIgnoreIt = FALSE; // used when "Ignore it, I will fix it later" button was hit
							// in consistency check dlg

// whm added 6Apr05 for support of export filtering of sfms and RTF output of the same in
// the appropriate functions in the Export-Import file, etc. These globals are defined 
// in ExportSaveAsDlg.cpp
extern wxArrayString m_exportMarkerAndDescriptions;
extern wxArrayString m_exportBareMarkers;
extern wxArrayInt m_exportFilterFlags;
extern wxArrayInt m_exportFilterFlagsBeforeEdit;

// support for USFM and SFM Filtering
// Since these special filter markers will be visible to the user in certain dialog
// controls, I've opted to use marker labels that should be unique (starting with \~) and
// yet still recognizable by containing the word 'FILTER' as part of their names.

/// A marker string used to signal the beginning of filtered material stored in a source
/// phrase's m_filteredInfo member.
const wxChar* filterMkr = _T("\\~FILTER");

/// A marker string used to signal the end of filtered material stored in a source phrase's
/// m_filteredInfo member.
const wxChar* filterMkrEnd = _T("\\~FILTER*");

/////////////////////////////////////////////////////////////////////////////
// CAdapt_ItDoc

IMPLEMENT_DYNAMIC_CLASS(CAdapt_ItDoc, wxDocument)

BEGIN_EVENT_TABLE(CAdapt_ItDoc, wxDocument)

	// The events that are normally handled by the doc/view framework use predefined
	// event identifiers, i.e., wxID_NEW, wxID_SAVE, wxID_CLOSE, wxID_OPEN, etc. 
	EVT_MENU(wxID_NEW, CAdapt_ItDoc::OnFileNew)
	EVT_MENU(wxID_SAVE, CAdapt_ItDoc::OnFileSave)
	EVT_UPDATE_UI(wxID_SAVE, CAdapt_ItDoc::OnUpdateFileSave)
	EVT_MENU(wxID_CLOSE, CAdapt_ItDoc::OnFileClose)
	EVT_UPDATE_UI(wxID_CLOSE, CAdapt_ItDoc::OnUpdateFileClose)
	EVT_MENU(ID_SAVE_AS, CAdapt_ItDoc::OnFileSaveAs)
	EVT_UPDATE_UI(ID_SAVE_AS, CAdapt_ItDoc::OnUpdateFileSaveAs)
	EVT_MENU(wxID_OPEN, CAdapt_ItDoc::OnFileOpen)
	EVT_MENU(ID_TOOLS_SPLIT_DOC, CAdapt_ItDoc::OnSplitDocument)
	EVT_UPDATE_UI(ID_TOOLS_SPLIT_DOC, CAdapt_ItDoc::OnUpdateSplitDocument)
	EVT_MENU(ID_TOOLS_JOIN_DOCS, CAdapt_ItDoc::OnJoinDocuments)
	EVT_UPDATE_UI(ID_TOOLS_JOIN_DOCS, CAdapt_ItDoc::OnUpdateJoinDocuments)
	EVT_MENU(ID_TOOLS_MOVE_DOC, CAdapt_ItDoc::OnMoveDocument)
	EVT_UPDATE_UI(ID_TOOLS_MOVE_DOC, CAdapt_ItDoc::OnUpdateMoveDocument)
	EVT_UPDATE_UI(ID_FILE_PACK_DOC, CAdapt_ItDoc::OnUpdateFilePackDoc)
	EVT_UPDATE_UI(ID_FILE_UNPACK_DOC, CAdapt_ItDoc::OnUpdateFileUnpackDoc)
	EVT_MENU(ID_FILE_PACK_DOC, CAdapt_ItDoc::OnFilePackDoc)
	EVT_MENU(ID_FILE_UNPACK_DOC, CAdapt_ItDoc::OnFileUnpackDoc)
	EVT_MENU(ID_EDIT_CONSISTENCY_CHECK, CAdapt_ItDoc::OnEditConsistencyCheck)
	EVT_UPDATE_UI(ID_EDIT_CONSISTENCY_CHECK, CAdapt_ItDoc::OnUpdateEditConsistencyCheck)
	EVT_MENU(ID_ADVANCED_RECEIVESYNCHRONIZEDSCROLLINGMESSAGES, CAdapt_ItDoc::OnAdvancedReceiveSynchronizedScrollingMessages)
	EVT_UPDATE_UI(ID_ADVANCED_RECEIVESYNCHRONIZEDSCROLLINGMESSAGES, CAdapt_ItDoc::OnUpdateAdvancedReceiveSynchronizedScrollingMessages)
	EVT_MENU(ID_ADVANCED_SENDSYNCHRONIZEDSCROLLINGMESSAGES, CAdapt_ItDoc::OnAdvancedSendSynchronizedScrollingMessages)
	EVT_UPDATE_UI(ID_ADVANCED_SENDSYNCHRONIZEDSCROLLINGMESSAGES, CAdapt_ItDoc::OnUpdateAdvancedSendSynchronizedScrollingMessages)
END_EVENT_TABLE()

/////////////////////////////////////////////////////////////////////////////
// CAdapt_ItDoc construction/destruction

/// **** DO NOT PUT INITIALIZATIONS IN THE DOCUMENT'S CONSTRUCTOR *****
/// **** ONLY INITIALIZATIONS OF DOCUMENT'S PRIVATE MEMBERS SHOULD ****
/// **** BE DONE HERE; DO OTHER INITIALIZATIONS IN THE APP'S      *****
/// **** OnInit() METHOD                                          *****
CAdapt_ItDoc::CAdapt_ItDoc()
{
	m_bHasPrecedingStraightQuote = FALSE; // this one needs to be initialized to
										  // FALSE every time a doc is recreated
	m_bLegacyDocVersionForSaveAs = FALSE; // whm added 14Jan11
	// WX Note: All Doc constructor initializations moved to the App
	// **** DO NOT PUT INITIALIZATIONS HERE IN THE DOCUMENT'S CONSTRUCTOR *****
	// **** ONLY INITIALIZATIONS OF DOCUMENT'S PRIVATE MEMBERS SHOULD      ****
	// **** BE DONE HERE; DO OTHER INITIALIZATIONS IN THE APP'S           *****
	// **** OnInit() METHOD                                               *****
}


/// **** ALL CLEANUP SHOULD BE DONE IN THE APP'S OnExit() METHOD ****
CAdapt_ItDoc::~CAdapt_ItDoc() // from MFC version
{
	// **** ALL CLEANUP SHOULD BE DONE IN THE APP'S OnExit() METHOD ****
}

///////////////////////////////////////////////////////////////////////////////
/// \return TRUE if new document was created successfully, FALSE otherwise
/// \remarks
/// Called from: the DocPage's OnWizardFinish() function.
/// In OnNewDocument, we aren't creating the document via serialization
/// of data from persistent storage (as does OnOpenDocument()), rather 
/// we are creating the new document from scratch, by doing the following:
/// 1. Making sure our working directory is set properly.
/// 2. Calling parts of the virtual base class wxDocument::OnNewDocument() method
/// 3. Create the buffer and list structures that will hold our data
/// 4. Providing KB structures are ready, call GetNewFile() to get
///    the sfm file for import into our app.
/// 5. Get an output file name from the user.
/// 6. Tidy up the frame's window title.
/// 7. Create/Recreate the list of paired source and target punctuation
///    correspondences, updating also the View's punctuation settings
/// 8. Remove any Ventura Publisher optional hyphens from the text buffer.
/// 9. Call TokenizeText, which separates the text into words, stores them
///    in m_pSourcePhrases list and returns the number
/// 10. Calculate the App's text heights, and get the View to calculate
///     its initial indices and do its RecalcLayout()
/// 11. Show/place the initial phrasebox at first empty target slot
/// 12. Keep track of sequence numbers and set initial global src phrase
///     node position.
/// 13. [added] call OnInitialUpdate() which needs to be called before the
///     view is shown.
/// BEW added 13Nov09: call of m_bReadOnlyAccess = SetReadOnlyProtection(), in order to give
/// the local user in the project ownership for writing permission (if FALSE is returned)
/// or READ-ONLY access (if TRUE is returned). (Also added to LoadKB() and OnOpenDocument()
/// and OnCreate() for the view class.)
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::OnNewDocument()
// ammended for support of glossing or adapting
{
	// refactored 10Mar09
	CAdapt_ItApp* pApp = GetApp();
	pApp->m_nSaveActiveSequNum = 0; // reset to a default initial value, safe for any length of doc 

	gnBeginInsertionsSequNum = -1; // reset for "no current insertions"
	gnEndInsertionsSequNum = -1; // reset for "no current insertions"

 	// get a pointer to the view
	CAdapt_ItView* pView = (CAdapt_ItView*) pApp->GetView();
	wxASSERT(pView->IsKindOf(CLASSINFO(CAdapt_ItView)));

    // BEW comment 6May09 -- this OnInitialUpdate() call contains a RecalcLayout() call
    // within its call of OnSize() call. If the RecalcLayout() call is late enough, it can
    // try the recalculation while piles do not exist, leading to a crash. So I've moved
    // OnInitialUpdate() to be early in OnNewDocument() and protected from a crash by
    // having the recalculation do nothing at all with the layout until all the layout
    // components are in place... (moved here from end of function)
	// 
	// whm added OnInitialUpdate(), since in WX the doc/view framework doesn't call it 
	// automatically we need to call it manually here. MFC calls its OnInitialUpdate()
	// method sometime after exiting its OnNewDocument() and before showing the View. See
	// Notes at OnInitialUpdate() for more info.
	pView->OnInitialUpdate(); // need to call it here because wx's doc/view doesn't 
								// automatically call it
	// ensure that the current work folder is the project one for default
	wxString dirPath = pApp->m_workFolderPath;
	bool bOK;
	if (pApp->m_lastSourceFileFolder.IsEmpty())
		bOK = ::wxSetWorkingDirectory(dirPath);
	else
		bOK = ::wxSetWorkingDirectory(pApp->m_lastSourceFileFolder);

	// the above may have failed, so if so use m_workFolderPath as the folder, 
	// or even the C: drive top level, so we can proceed to the file dialog safely
	// whm Note: TODO: The following block needs to be made cross-platform friendly
	if (!bOK)
	{
		pApp->m_lastSourceFileFolder = dirPath;
		bOK = ::wxSetWorkingDirectory(dirPath); // this should work, since m_workFolderPath can hardly 
												// be wrong!
		if (!bOK)
		{
			bOK = ::wxSetWorkingDirectory(_T("C:"));
			if (!bOK)
			{
				// we should never get a failure for the above, so just an English message will do
				wxMessageBox(_T(
				"OnNewDocument() failed, when setting current directory to C drive"),
				_T(""), wxICON_ERROR);
				return TRUE; // BEW 25Aug10, never return FALSE from OnNewDocument() if 
							 // you want the doc/view framework to keep working right
			}
		}
	}

	//if (!wxDocument::OnNewDocument()) // don't use this because it calls OnCloseDocument()
	//	return FALSE;
	// whm NOTES: The wxWidgets base class OnNewDocument() calls OnCloseDocument()
	// which fouls up the KB structures due to the OnCloseDocument() calls to
	// EraseKB(), etc. To get around this problem which arises because of 
	// different calling orders in the two doc/view frameworks, we'll not 
	// call the base class wxDocument::OnNewDocument() method in wxWidgets,
	// but instead we call the remainder of its contents here:
	// whm verified the need for this 20July2006
     DeleteContents();
     Modify(FALSE);
     SetDocumentSaved(FALSE);
     wxString name;
     GetDocumentManager()->MakeDefaultName(name);
     SetTitle(name);
     SetFilename(name, TRUE);
	 // above calls come from wxDocument::OnNewDocument()
	 // Note: The OnSaveModified() call is handled when needed in 
	 // the Doc's Close() and/or OnOpenDocument()

	// (SDI documents will reuse this document)
	if (pApp->m_pBuffer != 0)
	{
		delete pApp->m_pBuffer; // make sure wxString is not in existence
		pApp->m_pBuffer = (wxString*)NULL; // MFC had = 0
	}

	// BEW added 21Apr08; clean out the global struct gEditRecord & clear its deletion lists,
	// because each document, on opening it, it must start with a truly empty EditRecord; and
	// on doc closure and app closure, it likewise must be cleaned out entirely (the deletion
	// lists in it have content which persists only for the life of the document currently open)
	pView->InitializeEditRecord(gEditRecord);
	if (!gEditRecord.deletedAdaptationsList.IsEmpty())
		gEditRecord.deletedAdaptationsList.Clear(); // remove any stored deleted adaptation strings
	if (!gEditRecord.deletedGlossesList.IsEmpty())
		gEditRecord.deletedGlossesList.Clear(); // remove any stored deleted gloss strings
	if (!gEditRecord.deletedFreeTranslationsList.IsEmpty())
		gEditRecord.deletedFreeTranslationsList.Clear(); // remove any stored deleted free translations


	int width = wxSystemSettings::GetMetric(wxSYS_SCREEN_X);
#ifdef _RTL_FLAGS
	pApp->m_docSize = wxSize(width - 40,600); // a safe default width, the length doesn't matter 
											  // (it will change shortly)
#else
	pApp->m_docSize = wxSize(width - 80,600); // ditto
#endif

	// need a SPList to store the source phrases
	if (pApp->m_pSourcePhrases == NULL)
		pApp->m_pSourcePhrases = new SPList;
	wxASSERT(pApp->m_pSourcePhrases != NULL);


	bool bKBReady = FALSE;
	if (gbIsGlossing)
		bKBReady = pApp->m_bGlossingKBReady;
	else
		bKBReady = pApp->m_bKBReady;
	if (bKBReady)
	{
		pApp->m_nActiveSequNum = -1; // default, till positive value on layout of file

		pApp->m_pBuffer = new wxString; // on the heap, because this could be a large block of source text
		wxASSERT(pApp->m_pBuffer != NULL);
		pApp->m_nInputFileLength = 0;
		wxString filter = _T("*.*");
		wxString fileTitle = _T(""); // stores name (including extension) of user's 
				// chosen source file; however, when taken into the COutFilenameDlg
				// dialog below, the latter's InitDialog() call strips off any
				// filename extension before showing what's left to the user
		wxString pathName; // stores the path (including filename & extension) to the 
						   // chosen input source text file to be used for doc create

        // The following wxFileDialog part was originally in GetNewFile(), but moved here
        // 19Jun09 to consolidate file error message processing.
		wxString defaultDir;
		if (gpApp->m_lastSourceFileFolder.IsEmpty())
		{
			defaultDir = gpApp->m_workFolderPath;
		}
		else
		{
			defaultDir = gpApp->m_lastSourceFileFolder;
		}

		// BEW addition, 15Aug10, test for user navigation protection feature turned on,
		// and if so, show the monocline list of files in the Source Data folder only,
		// otherwise, show the standard File Open dialog, wxFileDialog, supplied by
		// wxWidgets which allows the user to navigate the hierarchical file/folder system
		// BEW 22Aug10, included m_bShowAdministratorMenu in the test, so that we don't
		// make the administrator have the Source Data folder restriction and
		// navigation-protection feature be force on him when the Administrator menu is
		// visible. I've also put a conditional compile here so that when the developer is
		// debugging, he can choose which behaviour he wants for testing purposes
		bool bUseSourceDataFolderOnly =  gpApp->UseSourceDataFolderOnlyForInputFiles();
		bool bUserNavProtectionInForce = FALSE;
#ifdef __WXDEBUG__
		// un-comment out the next line to have navigation protection for loading source
		// text files turned on when debugging only provided the administrator menu is not
		// showing - this is the way it is in the distributed application, that is, even
		// if user navigation protection is on, making the administrator menu visible will
		// override the 'on' setting so that the legacy File Open dialog is used; and
		// making the administrator menu invisible again automatically restores user
		// navigation protection to being 'on'
		
		//if (bUseSourceDataFolderOnly && !gpApp->m_bShowAdministratorMenu)

		// un-comment out the next line to have navigation protection for loading source
		// text files turned on when debugging, whether or not administrator menu is
		// visible; and comment out the line above
		bUserNavProtectionInForce = FALSE; // use this for allowing or suppressing
		// the COutputFilenameDlg further below, depending on whether the legacy
		// File New dialog is used, or the NavProtectNewDoc's dialog, respectively
		
		if (bUseSourceDataFolderOnly)
#else
		if (bUseSourceDataFolderOnly && !gpApp->m_bShowAdministratorMenu)
#endif
		{
            // This block encapsulates user file/folder navigation protection, by showing
            // to the user only all, or a subset of, the files in the monocline list of
            // files in the folder named "Source Data" within the current project's folder.
            // All the user can do is either Cancel, or select a single file to be loaded
            // as a new adaptation document, no navigation functionality is provided here
            bUserNavProtectionInForce = TRUE;

			gpApp->m_sortedLoadableFiles.Clear(); // we always recompute the array every
                // time the user tries to create a new document, because the administrator
                // may have added new source text files to the 'Source Data' folder since
                // the time of the last document creation attempt
			gpApp->EnumerateLoadableSourceTextFiles(gpApp->m_sortedLoadableFiles,
								gpApp->m_sourceDataFolderPath, filterOutUnloadableFiles);

			// now remove any array entries which have their filename title part
			// clashing with a document filename's title part (and book mode may be
			// currently on, so if it is we get the list of doc filenames from the
			// currently active bible book folder); to do this, first calculate the path
			// to the storage folder for the documents, and enumerate their filenames to a
			// wxArrayString local array, then call RemoveNameDuplicatesFromArray() to
			// compare the file titles and remove the duplicates
			wxString docsPath;
			if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
			{
				docsPath = gpApp->m_bibleBooksFolderPath; // path to a book folder within 
														  // the "Adaptations" folder
			}
			else
			{
				docsPath = gpApp->m_curAdaptionsPath; // path to the "Adaptations" 
													  // folder of the project
			}
			wxArrayString arrDocFilenames;
			gpApp->EnumerateDocFiles_ParametizedStore(arrDocFilenames,docsPath);
			// the following call removes any items from the first param's array which
			// have a duplicate file title for a filename in the second param's array;
			// the TRUE parameter is bSorted, we want the final list which the user sees
			// to be in alphabetical order (for Windows, a caseless compare is done, for
			// other operating systems, a case-sensitive compare is done - see the
			// sortCompareFunc() in helpers.cpp)
			RemoveNameDuplicatesFromArray(gpApp->m_sortedLoadableFiles, arrDocFilenames,
												TRUE, excludeExtensionsFromComparison);
			wxString strSelectedFilename;
			strSelectedFilename.Empty();

			// BEW 16Aug10, Note: we create the only and only instance of m_pNavProtectDlg here
			// rather than in the app's OnInit() function, because we want the dialog
			// handler's InitDialog() function called each time the dialog is to be shown using
			// ShowModal() so that the two buttons will be initialized correctly
			wxWindow* docWindow = GetDocumentWindow(); 
			gpApp->m_pNavProtectDlg = new NavProtectNewDoc(docWindow); 
            
			// display the dialog, it's list of filenames is monocline & no navigation
			// capability is provided
			if (gpApp->m_pNavProtectDlg->ShowModal() == wxID_CANCEL)
			{
				// the user has hit the Cancel button
				wxASSERT(strSelectedFilename.IsEmpty());
				wxMessageBox(_(
"Adapt It cannot do any useful work unless you select a source file to adapt. Please try again."),
				_T(""), wxICON_INFORMATION);

				// check if there was a document current, and if so, reinitialize everything
				if (pView != 0)
				{
					delete gpApp->m_pNavProtectDlg;
					gpApp->m_pNavProtectDlg = NULL;
					pApp->m_pTargetBox->SetValue(_T(""));
					delete pApp->m_pBuffer;
					pApp->m_pBuffer = (wxString*)NULL; // MFC had = 0
					pView->Invalidate();
					GetLayout()->PlaceBox();
				}
                //return FALSE; BEW removed 24Aug10 as it clobbers part of the wxWidgets
                //doc/view black box on which we rely, leading to our event handlers
                //failing to be called, so return TRUE instead
				return TRUE;
			}
			else
			{
				// the user has hit the "Input file" button
				strSelectedFilename = gpApp->m_pNavProtectDlg->GetUserFileName();
				wxASSERT(!strSelectedFilename.IsEmpty());

				// the dialog handler can now be deleted and its point set to NULL
				delete gpApp->m_pNavProtectDlg;
				gpApp->m_pNavProtectDlg = NULL;

				// create the path to the selected file (m_sourceDataFolderPath is always
				// defined when the app enters a project, as a folder "Source Data" which
				// is a direct child of the folder m_curProjectPath)
				pathName = gpApp->m_sourceDataFolderPath + gpApp->PathSeparator + strSelectedFilename;
				wxASSERT(::wxFileExists(pathName));

				// set fileTitle to the selected file's name (including extension, as the
				// latter will be removed later below)
				fileTitle = strSelectedFilename;
			}
		} // end of TRUE block for test: if (bUseSourceDataFolderOnly)
		else
		{
            // This block uses the legacy wxFileDialog call, which allows the user to
            // navigate the folder hierarchy to find loadable source text files anywhere
            // within any volume accessible to the system, there is no user navigation
            // protection in force if control enters this block
			wxFileDialog fileDlg(
				(wxWindow*)wxGetApp().GetMainFrame(), // MainFrame is parent window for file dialog
				_("Input Text File For Adaptation"),
				defaultDir,	// default dir (either m_workFolderPath, or m_lastSourceFileFolder)
				_T(""),		// default filename
				filter,
			wxFD_OPEN); // | wxHIDE_READONLY); wxHIDE_READONLY deprecated in 2.6 - the checkbox is never shown
						// GDLC wxOPEN deprecated in 2.8
			fileDlg.Centre();
			// open as modal dialog
			int returnValue = fileDlg.ShowModal(); // MFC has DoModal()
			if (returnValue == wxID_CANCEL)
			{
                // user cancelled, so cancel the New... command, or <New Document> choice,
                // as the case may be -- either user choice will have caused
                // OnNewDocument() to be called
				wxMessageBox(_(
"Adapt It cannot do any useful work unless you select a source file to adapt. Please try again."),
				_T(""), wxICON_INFORMATION);

				// check if there was a document current, and if so, reinitialize everything
				if (pView != 0)
				{
					pApp->m_pTargetBox->SetValue(_T(""));
					delete pApp->m_pBuffer;
					pApp->m_pBuffer = (wxString*)NULL; // MFC had = 0
					pView->Invalidate();
					GetLayout()->PlaceBox();
				}
                //return FALSE; BEW removed 24Aug10 as it clobbers part of the wxWidgets
                //doc/view black box on which we rely, leading to our event handlers
                //failing to be called, so return TRUE instead
				return TRUE;
			}
			else // must be wxID_OK 
			{
				pathName = fileDlg.GetPath(); //MFC's GetPathName() and wxFileDialog.GetPath both get whole dir + file name.
				fileTitle = fileDlg.GetFilename(); // just the file name (including any extension)
			} // end of else wxID_OK
			  // & fileDlg goes out of scope here
		} // end of else block for test: if (bUseSourceDataFolderOnly)

        // If control gets to here, (and it cannot do so if the user hit the Cancel
        // button), we've pathName and fileTitle wxString variables set ready for creating
        // the new document and getting an output document name from the user (the latter
        // calls COutFilenameDlg class, and it includes built-in invalid character
        // protection, and protection from a name conflict)
		wxFileName fn(pathName);
		wxString fnExtensionOnly = fn.GetExt(); // GetExt() returns the extension NOT including the dot

        // get the file, and it's length (which includes null termination byte/s) whm
        // modified 18Jun09 GetNewFile() now returns an enum getNewFileState (see
        // Adapt_It.h) which more specifically reports the success or error state
        // encountered in getting the file for input. It now uses a switch() structure.
		switch(GetNewFile(pApp->m_pBuffer,pApp->m_nInputFileLength,pathName))
		{
		case getNewFile_success:
		{
			//wxString tempSelectedFullPath = fileDlg.GetPath(); BEW changed 15Aug10 to
			// remove the second call to fileDlg.GetPath() here, as pathName has the path
			wxString tempSelectedFullPath = pathName;

			// wxFileDialog.GetPath() returns the full path with directory and filename. We
			// only want the path part, so we also call ::wxPathOnly() on the full path to
			// get only the directory part.
			gpApp->m_lastSourceFileFolder = ::wxPathOnly(tempSelectedFullPath);
	
            // Check if it has an \id line. If it does, get the 3-letter book code. If
            // a valid code is present, check that it is a match for the currently
            // active book folder. If it isn't tell the user and abort the <New
            // Document> operation, leaving control in the Document page of the wizard
            // for a new attempt with a different source text file, or a change of book
            // folder to be done and the same file reattempted after that. If it is a
            // matching book code, continue with setting up the new document.
			if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
			{
				// do the test only if Book Mode is turned on
				wxString strIDMarker = _T("\\id");
				int pos = (*gpApp->m_pBuffer).Find(strIDMarker);
				if ( pos != -1)
				{
					// the marker is in the file, so we need to go ahead with the check, but first
					// work out what the current book folder is and then what its associated code is
					wxString aBookCode = ((BookNamePair*)(*gpApp->m_pBibleBooks)[gpApp->m_nBookIndex])->bookCode;
					wxString seeNameStr = ((BookNamePair*)(*gpApp->m_pBibleBooks)[gpApp->m_nBookIndex])->seeName;
					gbMismatchedBookCode = FALSE;

					// get the code by advancing over the white space after the \id marker, and then taking
					// the next 3 characters as the code
					const wxChar* pStr = gpApp->m_pBuffer->GetData();
					wxChar* ptr = (wxChar*)pStr;
					ptr += pos;
					ptr += 4; // advance beyond \id and whatever white space character is next
					while (*ptr == _T(' ') || *ptr == _T('\n') || *ptr == _T('\r') || *ptr == _T('\t'))
					{
						// advance over any additional space, newline, carriage return or tab
						ptr++;
					}
					wxString theCode(ptr,3);	// make a 3-letter code, but it may be rubbish as we can't be
												// sure there is actually a valid one there

					// test to see if the string contains a valid 3-letter code
					bool bMatchedBookCode = CheckBibleBookCode(gpApp->m_pBibleBooks, theCode);
					if (bMatchedBookCode)
					{
						// it matches one of the 67 codes known to Adapt It, so we need to check if it
						// is the correct code for the active folder; if it's not, tell the user and
						// go back to the Documents page of the wizard; if it is, just let processing 
						// continue (the Title of a message box is just "Adapt It", only Palm OS permits naming)
						if (theCode != aBookCode)
						{
							// the codes are different, so the document does not belong in the active folder
							wxString aTitle;
							// IDS_INVALID_DATA_BOX_TITLE
							aTitle = _("Invalid Data For Current Book Folder");
							wxString msg1;
							// IDS_WRONG_THREELETTER_CODE_A
							msg1 = msg1.Format(_(
"The source text file's \\id line contains the 3-letter code %s which does not match the 3-letter \ncode required for storing the document in the currently active %s book folder.\n"),
								theCode.c_str(),seeNameStr.c_str());
							wxString msg2;
							//IDS_WRONG_THREELETTER_CODE_B
							msg2 = _(
"\nChange to the correct book folder and try again, or try inputting a different source text file \nwhich contains the correct code.");
							msg1 += msg2; // concatenate the messages
							wxMessageBox(msg1,aTitle, wxICON_WARNING); // I want a title on this other than "Adapt It"
							gbMismatchedBookCode = TRUE;// tell the caller about the mismatch

							return FALSE; // returns to OnWizardFinish() in DocPage.cpp (BEW 24Aug10, if 
										// that claim always is true, then no harm will be done;
										// but if it returns FALSE to the wxWidgets doc/view
										// framework, it partially clobbers the latter -- this can be
										// tested by returning FALSE here and then clicking the Open
										// icon button on the toolbar -- if that bypasses our event
										// handlers and just directly opens the "Select a file"
										// wxWidgets dialog, then the app is unstable and New and Open
										// whether on the File menu or as toolbar buttons will not
										// work right. In that case, we would need to return TRUE
										// here, not FALSE. For now, I'll let the FALSE value remain.)
						}
					}
					else
					{
						// not a known code, so we'll assume we accessed random text after the \id marker,
						// and so we just let processing proceed & the user can live with whatever happens
						;
					}
				}
				else
				{
					// if the \id marker is not in the source text file, then it is up to the user
					// to keep the wrong data from being stored in the current book folder, so all
					// we can do for that situation is to let processing proceed
					;
				}
			}

			// get a suitable output filename for use with the auto-save feature
			// BEW 23Aug10, changed so that if user navigation protection is in force,
			// this dialog is not put up, and so the user will have no chance to change
			// the filename title to anything different that the filename title of the
			// input source text file used to create the dialog. This inability to change
			// the filename makes the filename list's bleeding behaviour work reliably as
			// the user successively creates documents - until when all docs have been
			// created that can be created from the files in the Source Data folder, the
			// list will be empty
			wxString strUserTyped;
			if (bUserNavProtectionInForce)
			{
				// don't let the user have any chance to alter the filename
				strUserTyped = fileTitle; // while the RHS suggests it's a fileTitle, it's
                        // actually still got the filename extension on it (if there was
                        // one there originally); it doesn't get removed until
                        // SetDocumentWindowTitle() is called below
				// remove any extension user may have typed -- we'll keep control ourselves
				SetDocumentWindowTitle(strUserTyped, strUserTyped); // extensionless name is 
											// returned as the last parameter in the signature
				// for XML output
				pApp->m_curOutputFilename = strUserTyped + _T(".xml");
				pApp->m_curOutputBackupFilename = strUserTyped + _T(".BAK");
			}
			else
			{
				// legacy behaviour, the file title can be user-edited or typed to be
				// anything he wants
				COutputFilenameDlg dlg(GetDocumentWindow());
				dlg.Centre();
				dlg.m_strFilename = fileTitle;
				if (dlg.ShowModal() == wxID_OK)
				{
					// get the filename
					strUserTyped = dlg.m_strFilename;
					
                    // The COutputFilenameDlg::OnOK() handler checks for duplicate file
                    // name or a file name with bad characters in it.
					// abort the operation if user gave no explicit or bad output filename
					if (strUserTyped.IsEmpty())
					{
						// warn user to specify a non-null document name with valid chars
						if (strUserTyped.IsEmpty())
							wxMessageBox(_(
"Sorry, Adapt It needs an output document name. (An .xml extension will be automatically added.) Please try the New... command again."),
							_T(""),wxICON_INFORMATION);

						// reinitialize everything
						pApp->m_pTargetBox->ChangeValue(_T(""));
						delete pApp->m_pBuffer;
						pApp->m_pBuffer = (wxString*)NULL; // MFC had = 0
						pApp->m_curOutputFilename = _T("");
						pApp->m_curOutputPath = _T("");
						pApp->m_curOutputBackupFilename = _T("");
						pView->Invalidate(); // our own
						GetLayout()->PlaceBox();
						//return FALSE; BEW removed 24Aug10 as it clobbers part of the wxWidgets
						//doc/view black box on which we rely, leading to our event handlers
						//failing to be called, so return TRUE instead
						return TRUE;
					}

					// remove any extension user may have typed -- we'll keep control
					// ourselves
					SetDocumentWindowTitle(strUserTyped, strUserTyped); // extensionless name 
										// is returned as the last parameter in the signature

					// for XML output
					pApp->m_curOutputFilename = strUserTyped + _T(".xml");
					pApp->m_curOutputBackupFilename = strUserTyped + _T(".BAK");
				} // end of true block for test: if (dlg.ShowModal() == wxID_OK)
				else
				{
					// user cancelled, so cancel the New... command too
					wxMessageBox(_(
"Sorry, Adapt It will not work correctly unless you specify an output document name. Please try again."),
						_T(""), wxICON_INFORMATION);

					// reinitialize everything
					pApp->m_pTargetBox->ChangeValue(_T(""));
					delete pApp->m_pBuffer;
					pApp->m_pBuffer = (wxString*)NULL; // MFC had = 0
					pApp->m_curOutputFilename = _T("");
					pApp->m_curOutputPath = _T("");
					pApp->m_curOutputBackupFilename = _T("");
					pView->Invalidate();
					GetLayout()->PlaceBox();
					//return FALSE; BEW removed 24Aug10 as it clobbers part of the wxWidgets
					//doc/view black box on which we rely, leading to our event handlers
					//failing to be called, so return TRUE instead
					return TRUE;
				} // end of else block for test: if (dlg.ShowModal() == wxID_OK)
			} // end of else block for test: if (bUserNavProtectionInForce)

            // BEW modified 11Nov05, because the SetDocumentWindowTitle() call now updates
            // the window title
			// Set the document's path to reflect user input; the destination folder will
			// depend on whether book mode is ON or OFF; likewise for backups if turned on
			if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
			{
				pApp->m_curOutputPath = pApp->m_bibleBooksFolderPath + pApp->PathSeparator 
						+ pApp->m_curOutputFilename; // to send to the app when saving
													 // m_lastDocPath to config files
			}
			else
			{
				pApp->m_curOutputPath = pApp->m_curAdaptionsPath + pApp->PathSeparator 
						+ pApp->m_curOutputFilename; // to send to the app when saving
													 // m_lastDocPath to config files
			}

			SetFilename(pApp->m_curOutputPath,TRUE);// TRUE notify all views
			Modify(FALSE);

            // BEW added 26Aug10. In case we are loading a marked up file we earlier
            // exported, our custom markers in the exported output would have been changed
            // to \z-prefixed forms, \zfree, \zfree*, \znote, etc. Here we must convert
            // back to our internal marker forms, which lack the 'z'. (The z was to support
            // Paratext import of data containing 3rd party markers unknown to
            // Paratext/USFM.)
			ChangeParatextPrivatesToCustomMarkers(*pApp->m_pBuffer);
			
           // remove any optional hyphens in the source text for use by Ventura Publisher
            // (skips over any <-> sequences, and gives new m_pBuffer contents & new
            // m_nInputFileLength value)
			RemoveVenturaOptionalHyphens(pApp->m_pBuffer);

			// whm wx version: moved the following OverwriteUSFMFixedSpaces and
            // OverwriteUSFMDiscretionaryLineBreaks calls here from within TokenizeText
            // if user requires, change USFM fixed spaces (marked by the ~ character) to a space - this does not change the
            // length of the data in the buffer
			if (gpApp->m_bChangeFixedSpaceToRegularSpace)
				OverwriteUSFMFixedSpaces(pApp->m_pBuffer);

            // Change USFM discretionary line breaks // to a pair of spaces. We do this
            // unconditionally because these types of breaks are not likely to be
            // located in the same place if allowed to pass through to the target text,
            // and are usually placed in the translation in the final typesetting
            // stage. This does not change the length of the data in the buffer.
			OverwriteUSFMDiscretionaryLineBreaks(pApp->m_pBuffer);

	#ifndef __WXMSW__
	#ifndef _UNICODE
			// whm added 12Apr2007
			OverwriteSmartQuotesWithRegularQuotes(pApp->m_pBuffer);
	#endif
	#endif
			// BEW 16Dec10, added needed code to set gCurrentSfmSet to the current value
			// of the project's SfmSet. This code has been lacking from version 3 onwards
			// to 5.2.3 at least
			if (gpApp->gCurrentSfmSet != gpApp->gProjectSfmSetForConfig)
			{
				// the project's setting is not the same as the current setting for the
				// doc (the latter is either UsfmOnly if the app has just been launched,
				// as that is the default; or if there was some previous doc open, it has
				// the same value as that previous doc had)
				gpApp->gCurrentSfmSet = gpApp->gProjectSfmSetForConfig;
			}

			// parse the input file
			int nHowMany;
			nHowMany = TokenizeText(0,pApp->m_pSourcePhrases,*pApp->m_pBuffer,
									(int)pApp->m_nInputFileLength);

            // Get any unknown markers stored in the m_markers member of the Doc's
            // source phrases whm ammended 29May06: Bruce desired that the filter
            // status of unk markers be preserved for new documents created within the
            // same project within the same session, so I've changed the last parameter
            // of GetUnknownMarkersFromDoc from setAllUnfiltered to
            // useCurrentUnkMkrFilterStatus.
			GetUnknownMarkersFromDoc(gpApp->gCurrentSfmSet, &gpApp->m_unknownMarkers, 
									&gpApp->m_filterFlagsUnkMkrs, 
									gpApp->m_currentUnknownMarkersStr, 
									useCurrentUnkMkrFilterStatus);

	#ifdef _Trace_UnknownMarkers
			TRACE0("In OnNewDocument AFTER GetUnknownMarkersFromDoc (setAllUnfiltered) call:\n");
			TRACE1(" Doc's unk mrs from arrays  = %s\n", GetUnknownMarkerStrFromArrays(&m_unknownMarkers, &m_filterFlagsUnkMkrs));
			TRACE1(" m_currentUnknownMarkersStr = %s\n", m_currentUnknownMarkersStr);
	#endif

			// calculate the layout in the view
			int srcCount;
			srcCount = pApp->m_pSourcePhrases->GetCount(); // unused
			if (pApp->m_pSourcePhrases->IsEmpty())
			{
				// IDS_NO_SOURCE_DATA
				wxMessageBox(_(
"Sorry, but there was no source language data in the file you input, so there is nothing to be displayed. Try a different file."),
					_T(""), wxICON_EXCLAMATION);

				// restore everything
				pApp->m_pTargetBox->ChangeValue(_T(""));
				delete pApp->m_pBuffer;
				pApp->m_pBuffer = (wxString*)NULL; // MFC had = 0
				pView->Invalidate();
				GetLayout()->PlaceBox();
				return TRUE; // BEW 25Aug10, never return FALSE from OnNewDocument() if 
							 // you want the doc/view framework to keep working right
			}

			// try this for the refactored layout design....
			CLayout* pLayout = GetLayout();
			pLayout->SetLayoutParameters(); // calls InitializeCLayout() and 
						// UpdateTextHeights() and calls other relevant setters
#ifdef _NEW_LAYOUT
			bool bIsOK = pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_and_piles);
#else
			bool bIsOK = pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_and_piles);
#endif
			if (!bIsOK)
			{
				// unlikely to fail, so just have something for the developer here
				wxMessageBox(_T("Error. RecalcLayout(TRUE) failed in OnNewDocument()"),
				_T(""), wxICON_STOP);
				wxASSERT(FALSE);
				wxExit();
			}

			// mark document as modified
			Modify(TRUE);

			// show the initial phraseBox - place it at the first empty target slot
			pApp->m_pActivePile = GetPile(0);

			pApp->m_nActiveSequNum = 0;
			bool bTestForKBEntry = FALSE;
			CKB* pKB;
			if (gbIsGlossing) // should not be allowed to be TRUE when OnNewDocument is called,
							  // but I will code for safety, since it can be handled okay
			{
				bTestForKBEntry = pApp->m_pActivePile->GetSrcPhrase()->m_bHasGlossingKBEntry;
				pKB = pApp->m_pGlossingKB;
			}
			else
			{
				bTestForKBEntry = pApp->m_pActivePile->GetSrcPhrase()->m_bHasKBEntry;
				pKB = pApp->m_pKB;
			}
			if (bTestForKBEntry)
			{
				// it's not an empty slot, so search for the first empty one & do it there; but if
				// there are no empty ones, then revert to the first pile
				CPile* pPile = pApp->m_pActivePile;
				pPile = pView->GetNextEmptyPile(pPile);
				if (pPile == NULL)
				{
					// there was none, so we must place the box at the first pile
					pApp->m_pTargetBox->m_textColor = pApp->m_targetColor;
					pView->PlacePhraseBox(pApp->m_pActivePile->GetCell(1));
					pView->Invalidate();
					pApp->m_nActiveSequNum = 0;
					gnOldSequNum = -1; // no previous location exists yet
					// get rid of the stored rebuilt source text, leave a space there instead
					if (pApp->m_pBuffer)
						*pApp->m_pBuffer = _T(' ');
					return TRUE;
				}
				else
				{
					pApp->m_pActivePile = pPile;
					pApp->m_nActiveSequNum = pPile->GetSrcPhrase()->m_nSequNumber;
				}
			}

			// BEW added 10Jun09, support phrase box matching of the text colour chosen
			if (gbIsGlossing && gbGlossingUsesNavFont)
			{
				pApp->m_pTargetBox->SetOwnForegroundColour(pLayout->GetNavTextColor());
			}
			else
			{
				pApp->m_pTargetBox->SetOwnForegroundColour(pLayout->GetTgtColor());
			}

			// set initial location of the targetBox
			pApp->m_targetPhrase = pView->CopySourceKey(pApp->m_pActivePile->GetSrcPhrase(),FALSE);
			translation = pApp->m_targetPhrase;
			pApp->m_pTargetBox->m_textColor = pApp->m_targetColor;
			pView->PlacePhraseBox(pApp->m_pActivePile->GetCell(1),2); // calls RecalcLayout()

			// save old sequ number in case required for toolbar's Back button - in this case 
			// there is no earlier location, so set it to -1
			gnOldSequNum = -1;

			// set the initial global position variable
			break;
		}// end of case getNewFile_success
		case getNewFile_error_at_open:
		{
			wxString strMessage;
			strMessage = strMessage.Format(_("Error opening file %s."),pathName.c_str());
			wxMessageBox(strMessage,_T(""), wxICON_ERROR);
			gpApp->m_lastSourceFileFolder = gpApp->m_workFolderPath;
			break;
		}
		case getNewFile_error_opening_binary:
		{
            // A binary file - probably not a valid input file such as a MS Word doc.
            // Notify user that Adapt It cannot read binary input files, and abort the
            // loading of the file.
			wxString strMessage = _(
			"The file you selected for input appears to be a binary file.");
			if (fnExtensionOnly.MakeUpper() == _T("DOC"))
			{
				strMessage += _T("\n");
				strMessage += _(
				"Adapt It cannot use Microsoft Word Document (doc) files as input files.");
			}
			else if (fnExtensionOnly.MakeUpper() == _T("ODT"))
			{
				strMessage += _T("\n");
				strMessage += _(
				"Adapt It cannot use OpenOffice's Open Document Text (odt) files as input files.");
			}
			strMessage += _T("\n");
			strMessage += _("Adapt It input files must be plain text files.");
			wxString strMessage2;
			strMessage2 = strMessage2.Format(_("Error opening file %s."),pathName.c_str());
			strMessage2 += _T("\n");
			strMessage2 += strMessage;
			wxMessageBox(strMessage2,_T(""), wxICON_ERROR);
			gpApp->m_lastSourceFileFolder = gpApp->m_workFolderPath;
			break;
		}
		case getNewFile_error_ansi_CRLF_not_in_sequence:
		{
            // this error cannot occur, because the code where it may be generated is
            // never entered for a GetNewFile() call made in OnNewDocument, but the
            // compiler needs a case for this enum value otherwise there is a warning
            // generated
			wxMessageBox(_T("Input data malformed: CR and LF not in sequence"),
			_T(""),wxICON_ERROR);
			break;
		}
		case getNewFile_error_no_data_read:
		{
			// we got no data, so this constitutes a read failure
			wxMessageBox(_("File read error: no data was read in"),_T(""),wxICON_ERROR);
			break;
		}
		case getNewFile_error_unicode_in_ansi:
		{
			// The file is a type of Unicode, which is an error since this is the ANSI build. Notify
			// user that Adapt It Regular cannot read Unicode input files, and abort the loading of the
			// file.
			wxString strMessage = _("The file you selected for input is a Unicode file.");
			strMessage += _T("\n");
			strMessage += _("This Regular version of Adapt It cannot process Unicode text files.");
			strMessage += _T("\n");
			strMessage += _(
			"You should install and use the Unicode version of Adapt It to process Unicode text files.");
			wxString strMessage2;
			strMessage2 = strMessage2.Format(_("Error opening file %s."),pathName.c_str());
			strMessage2 += _T("\n");
			strMessage2 += strMessage;
			wxMessageBox(strMessage2,_T(""), wxICON_ERROR);
			gpApp->m_lastSourceFileFolder = gpApp->m_workFolderPath;
			break;
		}
		}// end of switch()
	}// end of if (bKBReady)
	
	// get rid of the stored rebuilt source text, leave a space there instead (the value of
	// m_nInputFileLength can be left unchanged)
	if (pApp->m_pBuffer)
		*pApp->m_pBuffer = _T(' ');
	gbDoingInitialSetup = FALSE; // turn it back off, the pApp->m_targetBox now exists, etc

    // BEW added 01Oct06: to get an up-to-date project config file saved (in case user
    // turned on or off the book mode in the wizard) so that if the app subsequently
    // crashes, at least the next launch will be in the expected mode
	if (gbPassedAppInitialization && !pApp->m_curProjectPath.IsEmpty())
	{
		bool bOK;
		if (pApp->m_bUseCustomWorkFolderPath && !pApp->m_customWorkFolderPath.IsEmpty())
		{
			// whm 10Mar10, must save using what paths are current, but when the custom
			// location has been locked in, the filename lacks "Admin" in it, so that it
			// becomes a "normal" project configuration file in m_curProjectPath at the 
			// custom location.
			if (pApp->m_bLockedCustomWorkFolderPath)
				bOK = pApp->WriteConfigurationFile(szProjectConfiguration, pApp->m_curProjectPath,projectConfigFile);
			else
				bOK = pApp->WriteConfigurationFile(szAdminProjectConfiguration, pApp->m_curProjectPath,projectConfigFile);
		}
		else
		{
			bOK = pApp->WriteConfigurationFile(szProjectConfiguration, pApp->m_curProjectPath,projectConfigFile);
		}
		// below is original
		//bOK = pApp->WriteConfigurationFile(szProjectConfiguration,pApp->m_curProjectPath,projectConfigFile);
	}

	// Note: On initial program startup OnNewDocument() is executed from OnInit()
	// to get a temporary doc and view. pApp->m_curOutputPath will be empty in 
	// that case, so only call AddFileToHistory() when it's not empty.
	if (!pApp->m_curOutputPath.IsEmpty())
	{
		wxFileHistory* fileHistory = pApp->m_pDocManager->GetFileHistory();
		fileHistory->AddFileToHistory(pApp->m_curOutputPath);
        // The next two lines are a trick to get past AddFileToHistory()'s behavior of
        // extracting the directory of the file you supply and stripping the path of all
        // files in history that are in this directoy. RemoveFileFromHistory() doesn't do
        // any tricks with the path, so the following is a dirty fix to keep the full
        // paths.
		fileHistory->AddFileToHistory(wxT("[tempDummyEntry]"));
		fileHistory->RemoveFileFromHistory(0); // 
	}

    // BEW added 13Nov09, for setting or denying ownership for writing permission. This is
    // something we want to do each time a doc is created (or opened) - if the local user
    // already has ownership for writing, no change is done and he retains it; but if he
    // had read only access, and the other person has relinquished the project, then the
    // local user will now get ownership. BEW modified 18Nov09: there is an OnFileNew()
    // call made in OnInit() at application initialization time, and control goes to
    // wxWidgets CreateDocument() which internally calls its OnNewDocument() function which
    // then calls Adapt_ItDoc::OnNewDocument(). If, therefore, we here allow
    // SetReadOnlyProtection() to be called while control is within OnInit(), we'll end up
    // setting read-only access off when the application is the only instance running and
    // accessing the last used project folder (and OnInit() then has to be given code to
    // RemoveReadOnlyProtection() immediately after the OnFileNew() call, because the
    // latter is bogus, it is just to get the wxWidgets doc/view framework set up, and the
    // "real" access of a project folder comes later, after OnInit() ends and OnIdle() runs
    // and so the start working wizard runs, etc. All that is fine until the user does the
    // following: the user starts Adapt It and opens a certain project; then the user
    // starts a second instance of Adapt It and opens the same project -- when this second
    // process runs, and while still within the OnInit() function, it detects that the
    // read-only protection file is currently open - and it is unable to remove it because
    // this is not the original process (although a 'bogus' one) that obtained ownership of
    // the project - and our code then aborts the second running Adapt It instance giving a
    // message that it is going to abort. This is unsatisfactory because we want anyone,
    // whether the same user or another, to be able to open a second instance of Adapt It
    // in read-only mode to look at what is being done, safely, in the first running
    // instance. Removing another process's open file is forbidden, so the only recourse is
    // to prevent the 'bogus' OnFileNew() call within OnInit() from creating a read-only
    // protection file here in OnNewDocument() if the call of the latter is caused from
    // within OnInit(). We can do this by having an app boolean which is TRUE during
    // OnInit() and FALSE thereafter. We'll call it:   m_bControlIsWithinOnInit
	if (!pApp->m_bControlIsWithinOnInit)
	{
		pApp->m_bReadOnlyAccess = pApp->m_pROP->SetReadOnlyProtection(pApp->m_curProjectPath);

		if (pApp->m_bReadOnlyAccess)
		{
			// if read only access is turned on, force the background colour change to show
			// now, instead of waiting for a user action requiring a canvas redraw
			pApp->GetView()->canvas->Refresh(); // needed? the call in OnIdle() is more efffective
		}
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param	event	-> wxCommandEvent (unused)
/// \remarks
/// Called from: the doc/view framework when wxID_SAVE event is generated. Also called from
/// CMainFrame's SyncScrollReceive() when it is necessary to save the current document before
/// opening another document when sync scrolling is on.
/// OnFileSave simply calls DoFileSave() and the latter sets an enum value of normal_save
/// 
/// BEW changed 28Apr10 A failure might be due to the Open call failing, in which case the
/// document's file is probably unchanged, or an unknown error, in which case the
/// document's file may have been truncated to zero length by the f.Open call done
/// beforehand. So we need code added in order to recover the document; also we have to
/// handle the possibility that the document may not yet have ever been saved, which
/// changes what we need to do in the event of failure.
/// BEW 29Apr10, added a public DoFileSave_Protected() file which returns boolean, because
/// the DoFileSave() function was called in a number of places and it was dangerous if it
/// failed (data would be lost), so I wrapped it with data protection code and called the
/// new function DoFileSave_Protected(), and put that in place of the other throughout the
/// app.
/// BEW 16Apr10, added enum, for support of Save As... menu item as well as Save
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnFileSave(wxCommandEvent& WXUNUSED(event)) 
{
	// not interested in the returned boolean from the following call, for OnFileSave() call
	DoFileSave_Protected(TRUE); // // TRUE means - show wait/progress dialog
}

// a smarter wrapper for DoFileSave(), to replace where that is called in various places
// Is called from the following 8 functions: the App's DoAutoSaveDoc(), OnFileSave(),
// OnSaveModified() and OnFilePackDoc(), the Doc's OnEditConsistencyCheck() and
// DoConsistencyCheck(), and SplitDialog's SplitAtPhraseBoxLocation_Interactive() and
// DoSplitIntoChapters(). Created 29Apr10.
bool CAdapt_ItDoc::DoFileSave_Protected(bool bShowWaitDlg)
{
	wxString pathToSaveFolder;
	wxULongLong originalSize = 0;
	wxULongLong copiedSize = 0;
	bool bRemovedSuccessfully = TRUE;
	ValidateFilenameAndPath(gpApp->m_curOutputFilename, gpApp->m_curOutputPath, pathToSaveFolder);
	bool bOutputFileExists = ::wxFileExists(gpApp->m_curOutputPath);
	wxString prefixStr = _T("tempSave_"); // don't localize this, it's never seen
	wxString newNameStr = prefixStr + gpApp->m_curOutputFilename;
	wxString newFileAbsPath = pathToSaveFolder + gpApp->PathSeparator + newNameStr;
	bool bCopiedSuccessfully = TRUE;
	if (bOutputFileExists)
	{
		// make a unique renamed copy which acts as a temporary backup in case of failure
		// in the call of DoFileSave()
		bCopiedSuccessfully = ::wxCopyFile(gpApp->m_curOutputPath, newFileAbsPath);
		wxASSERT(bCopiedSuccessfully);
		wxFileName fn(gpApp->m_curOutputPath);
		originalSize = fn.GetSize();
		if (bCopiedSuccessfully)
		{
			wxFileName fnNew(newFileAbsPath);
			copiedSize = fnNew.GetSize();
			wxASSERT( copiedSize == originalSize);
		}
	}
	// the call below to DoFileSave() requires that there be an active location - check,
	// and and if the box is at the doc end and not visible, then put it at the end of
	// the document before going on
	if (gpApp->m_pActivePile == NULL || gpApp->m_nActiveSequNum == -1)
	{
		int sequNumAtEnd = gpApp->GetMaxIndex();
		gpApp->m_pActivePile = GetPile(sequNumAtEnd);
		gpApp->m_nActiveSequNum = sequNumAtEnd;
		wxString boxValue;
		if (gbIsGlossing)
		{
			boxValue = gpApp->m_pActivePile->GetSrcPhrase()->m_gloss;
		}
		else
		{
			boxValue = gpApp->m_pActivePile->GetSrcPhrase()->m_adaption;
			translation = boxValue;
		}
		gpApp->m_targetPhrase = boxValue;
		gpApp->m_pTargetBox->ChangeValue(boxValue);
		gpApp->GetView()->PlacePhraseBox(gpApp->m_pActivePile->GetCell(1),2);
		gpApp->GetView()->Invalidate();
	}

    // SaveType enum value (2nd param) for the following call is default: normal_save BEW
    // added type, renamed filename, and bUserCancelled params 20Aug10, because they are
    // needed for when this DoFileSave() function is called in OnFileSaveAs(), however
    // other than the normal_save 2nd param, they are not needed when the call is here,
    // within DoFileSave_Protected() and so here we make no use here of the last two
    // returned values
	wxString renamedFilename; renamedFilename.Empty();
	bool bUserCancelled = FALSE;
	bool bSuccess = DoFileSave(bShowWaitDlg,normal_save,&renamedFilename,bUserCancelled);
	if (bSuccess)
	{
		if (bOutputFileExists && bCopiedSuccessfully)
		{
			// remove the temporary backup, the original was saved successfully
			bRemovedSuccessfully = ::wxRemoveFile(newFileAbsPath);
			wxASSERT(bRemovedSuccessfully);
		}
		return TRUE;
	}
	else // handle failure
	{
		wxASSERT(!bUserCancelled); // DoFileSave_Protected shows no GUI, 
								   // so bUserCancelled should be FALSE
		if (bOutputFileExists)
		{
			if (bCopiedSuccessfully)
			{
                // something failed, but we have a backup to fall back on. Determine if the
                // original remains untruncated, if so, retain it and remove the backup; if
                // not, remove the original and rename the backup to be the original
				bool bSomethingOfThatNameExists = ::wxFileExists(gpApp->m_curOutputPath);
				if (bSomethingOfThatNameExists)
				{
					wxULongLong thatSomethingsSize = 0;
					wxFileName fn(gpApp->m_curOutputPath);
					thatSomethingsSize = fn.GetSize();
					if (thatSomethingsSize == originalSize)
					{
						// we are in luck, the original is still good, so remove the backup
						bRemovedSuccessfully = ::wxRemoveFile(newFileAbsPath);
						wxASSERT(bRemovedSuccessfully);
					}
					else
					{
						// the size is different, therefore the original was truncated, so
						// restore the document file using the backup renamed
						bRemovedSuccessfully = ::wxRemoveFile(gpApp->m_curOutputPath);
						wxASSERT(bRemovedSuccessfully);
						bool bRenamedSuccessfully;
						bRenamedSuccessfully = ::wxRenameFile(newFileAbsPath, gpApp->m_curOutputPath);
						wxASSERT(bRenamedSuccessfully);
					}
				}
				wxMessageBox(_("Warning: document save failed for some reason.\n"),_T(""), wxICON_EXCLAMATION);
			}
			else // the original was not copied
			{
                // with no backup copy to fall back on, we have to do the best we can;
                // check if the original is still on disk: it may be, and untouched, or it
                // may be, but truncated; in the former case, if its size is unchanged, the
                // just retain it; but if the size is less, we must remove the truncated
                // fragment and tell the user to do an immediate File / Save
				bool bSomethingOfThatNameExists = ::wxFileExists(gpApp->m_curOutputPath);
				bool bOutOfLuck = FALSE;
				if (bSomethingOfThatNameExists)
				{
					wxULongLong thatSomethingsSize = 0;
					wxFileName fn(gpApp->m_curOutputPath);
					thatSomethingsSize = fn.GetSize();
					if (thatSomethingsSize < originalSize)
					{
						// we are out of luck, the original is truncated
						bOutOfLuck = TRUE;
						bRemovedSuccessfully = ::wxRemoveFile(gpApp->m_curOutputPath);
						wxASSERT(bRemovedSuccessfully);
					}
				}
				if (bOutOfLuck || !bSomethingOfThatNameExists)
				{
					// warn user to do a file save now while the doc is still in memory
					wxString msg;
					msg = msg.Format(_("Something went wrong. The adaptation document's file on disk was lost or destroyed. If the document is still visible, please click the Save command on the File menu immediately."));
					wxMessageBox(msg,_("Immediate Save Is Recommended"),wxICON_WARNING);
				}
			}
		}
		else // there was no original already saved to disk when OnFileSaveAs() was invoked
		{
			// either there is no original on disk still, or, there may be a truncated
			// save, either way, we must remove anything there..
			bool bTruncatedFragmentExists = ::wxFileExists(gpApp->m_curOutputPath);
			if (bTruncatedFragmentExists)
			{
				bRemovedSuccessfully = ::wxRemoveFile(gpApp->m_curOutputPath);
				wxASSERT(bRemovedSuccessfully);
				// warn user to do a file save now while the doc is still in memory
				wxString msg;
				msg = msg.Format(_("Something went wrong. The adaptation document was not saved to disk. Please click the Save command on the File menu immediately, and if the error persists, try the Save As... command instead - if that does not work, you are out of luck and the open document will not be saved, so shut down and start again."));
				wxMessageBox(msg,_("Immediate Save Is Recommended"),wxICON_WARNING);
			}
		}
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return TRUE if file was successfully saved; FALSE otherwise ( the return value
///         may not be used by some functions which call this one, such as OnFileSave()
///         or OnFileSaveAs() )
/// \param	bShowWaitDlg	 -> if TRUE the wait/progress dialog is shown, otherwise it 
///                             is not shown
/// \param  type             -> an enum value, either normal_save or, save_as, depending
///                             whether the user chose Save or Save As... command, respectively
/// \param  pRenamedFilename -> pointer to a string which is the new filename (it may 
///                             have an attached extension which our code will remove 
///                             and replace with .xml), if the user requested a rename,
///                             but will be (default) NULL if no renamed filename was
///                             supplied, or if the user chose Save command.
/// \param  bUserCancelled   <- ref to boolean, to tell caller when the return was due to
///                             user clicking the Cancel button in the OnFileSaveAs()
///                             function, however most calls (8 of the 9) are made from
///                             DoFileSave_Protected() and the latter makes no use of the
///                             values returned in the 3rd and 4th params.
/// \remarks
/// Called from: the Doc's OnFileSaveAs(); also called within DoFileSave_Protected() where
/// the latter is called from the following 8 functions: the App's DoAutoSaveDoc(),
/// OnFileSave(), OnSaveModified() and OnFilePackDoc(), the Doc's OnEditConsistencyCheck()
/// and DoConsistencyCheck(), and SplitDialog's SplitAtPhraseBoxLocation_Interactive() and
/// DoSplitIntoChapters().
/// Saves the current document and KB files in XML format and takes care of the necessary 
/// housekeeping involved.
/// Ammended for handling saving when glossing or adapting.
/// BEW modified 13Nov09, if the local user has only read-only access to a remote
/// project folder, the local user must not be able to cause his local copy of the 
/// remote document to be saved to the remote user's disk; if that could happen, the
/// remote user would almost certainly lose some of his edits
/// BEW 16Apr10, added SaveType param, for support of Save As... menu item
/// BEW 20Aug10, changed 2nd and 3rd params to have no default, and added the bool
/// reference 4th param (needed for OnFileSaveAs())
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::DoFileSave(bool bShowWaitDlg, enum SaveType type, 
							  wxString* pRenamedFilename, bool& bUserCancelled)
{
	bUserCancelled = FALSE;

	// BEW added 19Apr10 -- ensure we start with the latest doc version for saving if the
	// save is a normal_save, but if a Save As... was asked for, the user may be about to
	// choose a legacy doc version number for the save, in which case the call of the
	// wxFileDialog below may result in a different value being set by the code further
	// below 
	RestoreCurrentDocVersion();  // assume the default
	m_bLegacyDocVersionForSaveAs = FALSE; // initialize private member
	m_bDocRenameRequestedForSaveAs = FALSE; // initialize private member
	bool bDummySrcPhraseAdded = FALSE;
	SPList::Node* posLast = NULL;

	// prepare for progress dialog
	int counter;
	counter = 0;
	int nTotal = 0;
	wxString progMsg = _("%s  - %d of %d Total words and phrases");
	wxString msgDisplayed;
	wxProgressDialog* pProgDlg = (wxProgressDialog*)NULL;

	// refactored 9Mar09
	wxFile f; // create a CFile instance with default constructor
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);

	if (pApp->m_bReadOnlyAccess)
	{
		return TRUE; // let the caller think all is well, even though the save is suppressed
	}

    CAdapt_ItView* pView = (CAdapt_ItView*) GetFirstView();
	
	// make the working directory the "Adaptations" one; or the current Bible book folder
	// if the m_bBookMode flag is TRUE

	// There are at least three ways within wxWidgets to change the current
	// working directory:
	// (1) Use ChangePathTo() method of the wxFileSystem class,
	// (2) Use the static SetCwd() method of the wxFileName class,
	// (3) Use the global namespace method ::wxSetWorkingDirectory()
	// We'll regularly use ::wxSetWorkingDirectory()
	bool bOK;
	wxString pathToSaveFolder; // use this with Save As... to prevent a change of working directory
	if (pApp->m_bBookMode && !pApp->m_bDisableBookMode)
	{
		// save to the folder specified by app's member  m_bibleBooksFolderPath
		bOK = ::wxSetWorkingDirectory(pApp->m_bibleBooksFolderPath);
		pathToSaveFolder = pApp->m_bibleBooksFolderPath;
	}
	else
	{
		// do legacy save, to the Adaptations folder
		bOK = ::wxSetWorkingDirectory(pApp->m_curAdaptionsPath);
		pathToSaveFolder = pApp->m_curAdaptionsPath;
	}
	if (!bOK)
	{
        // BEW changed 23Apr10, I've never know the working directory set call to fail if a
        // valid path is supplied, so this would be an extraordinary situation - to proceed
        // may or may not result in a valid save, but we risk a crash, so we should play
        // save and abort the save attempt. But the message should not be localizable as it
        // is almost certain that it will never be seen.
		wxMessageBox(_T(
		"Failed to set the current working directory. The save operation was not attempted."),
		_T(""), wxICON_EXCLAMATION);
		m_bLegacyDocVersionForSaveAs = FALSE; // restore default
		m_bDocRenameRequestedForSaveAs = FALSE; // ditto
		return FALSE;
	}


    // if the phrase box is visible and has the focus, then its contents will have been
    // removed from the KB, so we must restore them to the KB, then after the save is done,
    // remove them again; but only provided the pApp->m_targetBox's window exists
    // (otherwise GetStyle call will assert)
	bool bNoStore = FALSE;
	bOK = FALSE;

	// In code below simply calling if (m_targetBox) or if (m_targetBox != NULL)
	// should be a sufficient test. 
    // BEW 6July10, code added for handling situation when the phrase box location has just
    // been made a <Not In KB> one (which marks all CRefString instances for that key as
    // m_bDeleted set TRUE and also stores a <Not In KB> for that key in the adaptation KB)
    // and then the user saves or closes and saves the document. Without this extra code,
    // the block immediatly below would re-store the active location's adaptation string
    // under the same key, thereby undeleting one of the deleted CRefString entries in the
    // KB for that key -- so we must prevent this happening by testing for m_bNotInKB set
    // TRUE in the CSourcePhrase instance there and if so, inhibiting the save
    bool bInhibitSave = FALSE;

    // BEW changed 26Oct10 to remove the wxASSERAT because if the phrasebox went past the
    // doc end, and the user wants a save or saveAs, then this assert will trip; so we have
    // to check and get start of doc as default active location in such a situation
	//wxASSERT(pApp->m_pActivePile != NULL); // whm added 18Aug10
	if (pApp->m_pActivePile == NULL)
	{
		// phrase box is not visible, put it at sequnum = 0 location
		pApp->m_nActiveSequNum = 0;
		pApp->m_pActivePile = GetPile(pApp->m_nActiveSequNum);
		CSourcePhrase* pSrcPhrase = pApp->m_pActivePile->GetSrcPhrase();
		pApp->GetView()->Jump(pApp,pSrcPhrase);
	}
	CSourcePhrase* pActiveSrcPhrase = pApp->m_pActivePile->GetSrcPhrase();
	if (pApp->m_pTargetBox != NULL)
	{
		if (pApp->m_pTargetBox->IsShown())// not focused on app closure
		{
			if (!gbIsGlossing)
			{
				pView->MakeTargetStringIncludingPunctuation(pActiveSrcPhrase,pApp->m_targetPhrase);
				pView->RemovePunctuation(this,&pApp->m_targetPhrase,from_target_text); //1 = from tgt
			}
			gbInhibitMakeTargetStringCall = TRUE;
			if (gbIsGlossing)
			{
				bOK = pApp->m_pGlossingKB->StoreText(pActiveSrcPhrase,pApp->m_targetPhrase);
			}
			else
			{
				// do the store, but don't store if it is a <Not In KB> location
				if (pActiveSrcPhrase->m_bNotInKB)
				{
					bInhibitSave = TRUE;
					bOK = TRUE; // need this, otherwise the message below will get shown
					// set the m_adaption member of the active CSourcePhrase instance,
					// because not doing the StoreText() call means it would not otherwise
					// get set; the above MakeTargetStringIncludingPunctuation() has
					// already set the m_targetStr member to include any punctuation
					// stored or typed
					pActiveSrcPhrase->m_adaption = pApp->m_targetPhrase; // punctuation was removed above
				}
				else
				{
					bOK = pApp->m_pKB->StoreText(pActiveSrcPhrase,pApp->m_targetPhrase);
				}
			}
			gbInhibitMakeTargetStringCall = FALSE;
			if (!bOK)
			{
				// something is wrong if the store did not work, but we can tolerate the error 
				// & continue
				wxMessageBox(_(
"Warning: the word or phrase was not stored in the knowledge base. This error is not destructive and can be ignored."),
				_T(""),wxICON_EXCLAMATION);
				bNoStore = TRUE;
			}
			else
			{
				if (gbIsGlossing)
				{
					pActiveSrcPhrase->m_bHasGlossingKBEntry = TRUE;
				}
				else
				{
					if (!bInhibitSave)
					{
						pActiveSrcPhrase->m_bHasKBEntry = TRUE;
					}
					else
					{
						bNoStore = TRUE;
					}
				}
			}
		}
	}

	// get the path correct, including correct filename extension (.xml) and the backup
    // doc filenames too; the m_curOutputPath returns is the full path, that is, it ends
    // with the contents of the returned m_curOutputFilename value built in; the third
    // param may be useful in some contexts (see OnFileSave() and OnFileSaveAs()), but not
    // here
	wxString unwantedPathToSaveFolder;
	ValidateFilenameAndPath(gpApp->m_curOutputFilename, gpApp->m_curOutputPath,
							unwantedPathToSaveFolder); // we don't use 3rd param here
	if (!f.Open(gpApp->m_curOutputFilename,wxFile::write))
	{
		return FALSE; // if we get here, we'll miss unstoring from the KB, but its not likely
					  // to happen, so we'll not worry about it - it wouldn't matter much anyway
	}

	CSourcePhrase* pSrcPhrase;
	CBString aStr;
	CBString openBraceSlash = "</"; // to avoid "warning: deprecated conversion from string constant to 'char*'"

	// prologue (Changed BEW 02July07 at Bob Eaton's request)
	gpApp->GetEncodingStringForXmlFiles(aStr);
	DoWrite(f,aStr);

	// add the comment with the warning about not opening the XML file in MS WORD 
	// 'coz is corrupts it - presumably because there is no XSLT file defined for it
	// as well. When the file is then (if saved in WORD) loaded back into Adapt It,
	// the latter goes into an infinite loop when the file is being parsed in.
	aStr = MakeMSWORDWarning(); // the warning ends with \r\n so we don't need to add them here

	// doc opening tag
	aStr += "<";
	aStr += xml_adaptitdoc;
	aStr += ">\r\n"; // eol chars OK for cross-platform???
	DoWrite(f,aStr);

	// in case file rename is wanted... from the Save As dialog
	wxString theNewFilename = _T("");
	bool bFileIsRenamed = FALSE;

	// if Save As... was chosen, its dialog should be shown here because the xml from this
	// point on needs to know which docVersion number to use
	if (type == save_as)
	{
		// get a file dialog (note: the user may ask for a save done with a legacy doc
		// version number in this dialog)
		wxString defaultDir = pathToSaveFolder; // set above
		wxString filter;
		filter = _("New XML format, for 6.0.0 and later (default)|*.xml|Legacy XML format, as in versions 3, 4 or 5. |*.xml||"); 
		wxString filename = gpApp->m_curOutputFilename;

retry:	bFileIsRenamed = FALSE;
		wxFileDialog fileDlg(
			(wxWindow*)wxGetApp().GetMainFrame(), // MainFrame is parent window for file dialog
			_("Save As"),
			defaultDir,	// an empty string would cause it to use the current working directory
			filename,	// the current document's filename
			filter, // the SaveType option - currently there are two, default is doc version 5, the other is doc version 4
			wxFD_SAVE ); // don't want wxFD_OVERWRITE_PROMPT as part of the style param
						 // because if user changes filename, we'll save with the new
						 // name and after verifying the file is on disk and okay, we'll
						 // silently remove the old version, so that there is only one
						 // file with the document's data after a save of any kind
		fileDlg.Centre();

		// make the dialog visible
		if (fileDlg.ShowModal() != wxID_OK)
		{
			// user cancelled, while this is not strictly a failure we return FALSE
			// because the original will have been truncated, and so the caller must
			// restore it
			RestoreCurrentDocVersion(); // ensure a subsequent save uses latest doc version number
			m_bLegacyDocVersionForSaveAs = FALSE; // restore default
			m_bDocRenameRequestedForSaveAs = FALSE; // ditto
			f.Close();
			bUserCancelled = TRUE; // inform the caller that the user hit the Cancel button
			return FALSE; 
		}

		// check that the user did not change the folder's path, the user must not be able
		// to do this in Adapt It, the location for saving documents is fixed and the
		// pathToSaveFolder has been set to whatever it is for this session
		wxFileName fn(fileDlg.GetPath());
		wxString usersChosenFolderPath = fn.GetPath();
		if (pathToSaveFolder != usersChosenFolderPath)
		{
			// warn user to try again
			wxString msg;
			msg = msg.Format(_("You must not use the Save As... dialog to change where Adapt It stores its document files. You can only rename the file, or make a different 'Save as type' choice, or both."));
			wxMessageBox(msg,_("Folder Change Is Not Allowed"),wxICON_WARNING);
			goto retry;
		}

        // Determine if a file rename is wanted and ensure there is no name clash; for a
        // clash, reenter the dialog and start afresh after warning the user, if no clash,
        // set a boolean because we will do the rename **AFTER** document backup (which may
        // or may not be wanted), at the end of the calling function (renames are only
        // possible from a call of the OnFileSaveAs() function. (And document backup will
        // also do the needed backup file renaming at the end of the BackupDocument()
        // function -- fortunately, BackupDocument() uses an independent
        // m_curOutputBackupFilename (an app member currently, but that may change soon so
        // as to be on the doc class) and so the backup document file and its path updating
        // can be done completely within BackupDocument() without affecting the delay of
        // renaming the document until control returns to OnFileSaveAs(); and we'll leave
        // OnFileSaveAs to do the needed path updates for the renamed doc file.)
		theNewFilename = fileDlg.GetFilename();
		if (theNewFilename != filename)
		{
			// check for illegal characters in the user's typed new filename (this code
			// taken from OutputFilenameDlg::OnOK() and tweaked a bit)
			wxString fn = theNewFilename;
			wxString illegals = wxFileName::GetForbiddenChars(); //_T(":?*\"\\/|<>");
			wxString scanned = SpanExcluding(fn, illegals);
			if (scanned != fn)
			{
				// there is at least one illegal character,; beep and show the illegals to the
				// user and then re-enter the dialog to start over from scratch; illegals
				// are characters such as:  :?*\"\|/<>
				::wxBell();
				wxString message;
				message = message.Format(
_("Filenames cannot include these characters: %s Please type a valid filename using none of those characters."),illegals.c_str());
				wxMessageBox(message, _("Bad Characters In Filename"), wxICON_INFORMATION);
				theNewFilename.Empty();
				goto retry;
			}

			// check for a name conflict
			if (FilenameClash(theNewFilename))
			{
				wxString msg;
				msg = msg.Format(_("The new filename you have typed conflicts with an existing filename. You cannot use that name, please type another."));
				wxMessageBox(msg,_("Conflicting Filename"),wxICON_WARNING);
				theNewFilename.Empty();
				goto retry;
			}
			else
			{
				bFileIsRenamed = TRUE; // theNewFilename has the renamed filename string
			}
		}
		if (bFileIsRenamed)
		{
			m_bDocRenameRequestedForSaveAs = TRUE; // set the private member, as the caller
												   // will need this flag for updating
												   // the window Title, and caller will
												   // restore its default FALSE value
			if (theNewFilename.IsEmpty())
			{
				// can't use an empty string as a filename, so stick with the current one,
				// return to the caller an empty string so that no rename is done
				pRenamedFilename->Empty();
			}
			else
			{
				// we've a string to return to caller for it to set up the new filename;
				// but first make sure we have an .xml extension on the new filename
				wxString thisFilename = theNewFilename;
				thisFilename = MakeReverse(thisFilename);
				wxString extn = thisFilename.Left(4);
				extn = MakeReverse(extn);
				if (extn.GetChar(0) == _T('.'))
				{
					// we can assume it is an extension because it begins with a period
					if (extn != _T(".xml"))
					{
						thisFilename = thisFilename.Mid(4); // remove the .adt extension or whatever
						thisFilename = MakeReverse(thisFilename);
						thisFilename += _T(".xml"); // it's now *.xml
					}
					else
					{
						thisFilename = MakeReverse(thisFilename); // it's already *.xml
					}
					*pRenamedFilename = thisFilename;
				}
				else // extn doesn't begin with a period
				{
					// assume the user didn't add and extension and that what we cut off
					// was part of his filetitle, so add .xml to what he typed
					theNewFilename += _T(".xml");
					*pRenamedFilename = theNewFilename;
				}
			}
		}
		else
		{
			pRenamedFilename->Empty();  // tells caller no rename is wanted
		}
		// delay any requested doc file rename to the end of the calling function...
		
        // get the docVersion number the user wants used for the save, an index value of 0
        // always uses the VERSION_NUMBER as currently set, but index values 1 or higher
        // select a legacy docVersion number (which gives different XML structure)
        // (currently the only other index value supported is 1, which maps to doc version
        // 4) Don't permit the possibility of a File Type change until the tests above
        // leading to reentrancy have been passed successfully
		int filterIndex = fileDlg.GetFilterIndex();
		SetDocVersion(filterIndex);

		// Execution control now takes one of two paths: if the user chose filterIndex ==
		// 0 item, which is VERSION_NUMBER's docVersion (currently == 5), then the code
		// for a norm Save is to be executed (except that in the Save As.. dialog he may
		// have also requested a document rename, in which case a block at the end of this
		// function will do that as well, as well as for when he makes the docVersion 4
		// choice). But if he chose filterIndex == 1 item, this is for docVersion set to
		// DOCVERSION4 (always == 4), in which case extra work has to be done - deep
		// copies of CSourcePhrase need to be created, and passed to a conversion function
		// FromDocVersion5ToDocVersion4() and the XML built from the converted deep copy
		// (to prevent corrupting the internal data structures which are docVersion5
		// compliant)
		m_bLegacyDocVersionForSaveAs = m_docVersionCurrent != (int)VERSION_NUMBER;
		
		if (m_bLegacyDocVersionForSaveAs)
		{
			// Saving in doc version 4 may require the addition of a doc-final dummy
			// CSourcePhrase instance to carry moved endmarkers. We'll add such temporarily,
			// but only when needed, and remove it when done. It's needed if the very last
			// CSourcePhrase instance has a non-empty m_endMarkers member.
			posLast = gpApp->m_pSourcePhrases->GetLast();
			CSourcePhrase* pLastSrcPhrase = posLast->GetData();
			wxASSERT(pLastSrcPhrase != NULL);
			if (!pLastSrcPhrase->GetEndMarkers().IsEmpty())
			{ 
				// we need a dummy one at the end
				bDummySrcPhraseAdded = TRUE;
				int aCount = gpApp->m_pSourcePhrases->GetCount();
				CSourcePhrase* pDummyForLast = new CSourcePhrase;
				gpApp->m_pSourcePhrases->Append(pDummyForLast);
				pDummyForLast->m_nSequNumber = aCount;
			}
		}
	} // end of TRUE block for test: if (type == save_as)

	// place the <Settings> element at the start of the doc (this has to know what the
	// user chose for the SaveType, so this call has to be made after the
	// SetDocVersion() call above - as that call sets the doc's save state which
	// remains in force until changed, or restored by a RestoreCurrentDocVersion() call
	aStr = ConstructSettingsInfoAsXML(1); // internally sets the docVersion attribute
							// to whatever is the current value of m_docVersionCurrent
	DoWrite(f,aStr);

	// setup the progress dialog
	nTotal = gpApp->m_pSourcePhrases->GetCount();
	msgDisplayed = progMsg.Format(progMsg,gpApp->m_curOutputFilename.c_str(),1,nTotal);
	if (bShowWaitDlg)
	{
		// whm note 27May07: Saving long documents takes some noticeable time, so I'm adding a
		// progress dialog here (not done in the MFC version)
		//wxProgressDialog progDlg(_("Saving File"),
		pProgDlg = new wxProgressDialog(_("Saving File"),
						msgDisplayed,
						nTotal,    // range
						gpApp->GetMainFrame(),   // parent
						//wxPD_CAN_ABORT |
						//wxPD_CAN_SKIP |
						wxPD_APP_MODAL |
						wxPD_AUTO_HIDE //| -- try this as well
						//wxPD_ELAPSED_TIME |
						//wxPD_ESTIMATED_TIME |
						//wxPD_REMAINING_TIME
						//| wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
						);
	}

	// process through the list of CSourcePhrase instances, building an xml element from
	// each
	SPList::Node* pos = gpApp->m_pSourcePhrases->GetFirst();

	// Branch and loop according to which doc version number is wanted. For a File / Save
	// it is VERSION_NUMBER's docVersion, also that is true for a Save As... in which the
	// top item (the default) was chosen as the filterIndex value of 0; but for a
	// filterIndex value of 1, the choice was for a legacy save (only DOCVERSION4 is
	// supported so far), and in this latter case, and only in this latter case, does
	// m_bLegacyDocVersionForSaveAs have a value of TRUE
	if (m_bLegacyDocVersionForSaveAs)
	{
		// user chose a legacy xml doc build, and so far there is only one such
		// choice, which is docVersion == 4
		wxString endMarkersStr; endMarkersStr.Empty();
		wxString inlineNonbindingEndMkrs; inlineNonbindingEndMkrs.Empty();
		wxString inlineBindingEndMkrs; inlineBindingEndMkrs.Empty();
		while (pos != NULL)
		{
			if (bShowWaitDlg)
			{
				counter++;
				if (counter % 1000 == 0) 
				{
					msgDisplayed = progMsg.Format(progMsg,gpApp->m_curOutputFilename.c_str(),counter,nTotal);
					pProgDlg->Update(counter,msgDisplayed);
				}
			}
			pSrcPhrase = (CSourcePhrase*)pos->GetData();
			// get a deep copy, so that we can change the data to what is compatible with
			// doc version 4 without corrupting the pSrcPhrase which remains in doc
			// version 5
			CSourcePhrase* pDeepCopy = new CSourcePhrase(*pSrcPhrase);
			pDeepCopy->DeepCopy();
			
			// do the conversion from docVersion 5 to docVersion 4 (if endMarkersStr is
			// passed in non-empty, the endmarkers are inserted internally at the start of
			// pDeepCopy's m_markers member (and if pDeepCopy is a merger, they are also
			// inserted in the first instance of pDeepCopy->m_pSavedWords's m_markers
			// member too); and before returning it must check for endmarkers stored on
			// pDeepCopy (whether a merger or not makes no difference in this case) and
			// reset endMarkersStr to whatever endmarker(s) are found there - so that the
			// next iteration of the caller's loop can place them on the next pDeepCopy
			// passed in. (FromDocVersion5ToDocVersion4() leverages the fact that the
			// legacy code for docVersion 4 xml construction knows nothing about the new
			// members m_endMarkers, m_freeTrans, etc - so as long as pDeepCopy's
			// m_markers member is reset correctly, and m_endMarkers's content is returned
			// to the caller for placement on the next iteration, the legacy xml code will
			// build correct docVersion 4 xml from the docVersion 5 CSourcePhrase instances)
			FromDocVersion5ToDocVersion4(pDeepCopy, &endMarkersStr, &inlineNonbindingEndMkrs,
										&inlineBindingEndMkrs);

			pos = pos->GetNext();
			aStr = pDeepCopy->MakeXML(1); // 1 = indent the element lines with a single tab
			DeleteSingleSrcPhrase(pDeepCopy,FALSE); // FALSE means "don't try delete a partner pile"
			DoWrite(f,aStr);
		}
	}
	else // use chose a normal docVersion 5 xml build
	{
		// this is identical to what the File / Save choice does, for building the
		// doc's XML, for VERSION_NUMBER (currently == 5) for docVersion
		while (pos != NULL)
		{
			if (bShowWaitDlg)
			{
				counter++;
				if (counter % 1000 == 0) 
				{
					msgDisplayed = progMsg.Format(progMsg,gpApp->m_curOutputFilename.c_str(),counter,nTotal);
					pProgDlg->Update(counter,msgDisplayed);
				}
			}
			pSrcPhrase = (CSourcePhrase*)pos->GetData();
			pos = pos->GetNext();
			aStr = pSrcPhrase->MakeXML(1); // 1 = indent the element lines with a single tab
			DoWrite(f,aStr);
		}
	}

	// doc closing tag
	aStr = xml_adaptitdoc;
	aStr = openBraceSlash + aStr; //"</" + aStr;
	aStr += ">\r\n"; // eol chars OK for cross-platform???
	DoWrite(f,aStr);

	// close the file
	f.Flush();
	f.Close();

	// remove the dummy that was appended, if we did append one in the code above
	if (type == save_as && m_bLegacyDocVersionForSaveAs)
	{	
		if (bDummySrcPhraseAdded)
		{
			posLast = gpApp->m_pSourcePhrases->GetLast();
			CSourcePhrase* pDummyWhichIsLast = posLast->GetData();
			wxASSERT(pDummyWhichIsLast != NULL);
			gpApp->GetDocument()->DeleteSingleSrcPhrase(pDummyWhichIsLast);
		}
	}

	// recompute m_curOutputPath, so it can be saved to config files as m_lastDocPath,
	// because the path computed at the end of OnOpenDocument() will have been invalidated
	// if the filename extension was changed by code earlier in DoFileSave()
	if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
	{
		gpApp->m_curOutputPath = pApp->m_bibleBooksFolderPath + 
									gpApp->PathSeparator + gpApp->m_curOutputFilename;
	}
	else
	{
		gpApp->m_curOutputPath = pApp->m_curAdaptionsPath + 
									gpApp->PathSeparator + gpApp->m_curOutputFilename;
	}
	gpApp->m_lastDocPath = gpApp->m_curOutputPath; // make it agree with what path was 
												   // used for this save operation
	if (bShowWaitDlg)
	{
		progMsg = _("Please wait while Adapt It saves the KB...");
		pProgDlg->Pulse(progMsg); // more general message during KB save
	}

	// Do the document backup if required (This call supports a docVersion 4 choice, and
	// also a request to rename the document; by internally accessing the private members
	// bool	m_bLegacyDocVersionForSaveAs, and bool m_bDocRenameRequestedForSaveAs either
	// or both of which may have been changed from their default values of FALSE depending
	// on the execution path through the code above
	if (gpApp->m_bBackupDocument)
	{
		bool bBackedUpOK;
		if (bFileIsRenamed)
		{
			bBackedUpOK = BackupDocument(gpApp, pRenamedFilename);
		}
		else
		{
		bBackedUpOK = BackupDocument(gpApp); // 2nd param is default NULL (no rename wanted)
		}
		if (!bBackedUpOK)
		{
			wxMessageBox(_(
			"Warning: the attempt to backup the current document failed."),
			_T(""), wxICON_EXCLAMATION);
		}
	}

    // Restore the latest document version number, in case the save done above was actually
    // a Save As... using an earlier doc version number. Must not restore earlier than
    // this, as a call of BackupDocument() will need to know what the user's chosen state
    // value currently is for docVersion.
	RestoreCurrentDocVersion();

	Modify(FALSE); // declare the document clean
	if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
		SetFilename(pApp->m_bibleBooksFolderPath+pApp->PathSeparator + 
			pApp->m_curOutputFilename,TRUE); // TRUE = notify all views
	else
		SetFilename(pApp->m_curAdaptionsPath+pApp->PathSeparator + 
			pApp->m_curOutputFilename,TRUE); // TRUE = notify all views

	// the KBs (whether glossing KB or normal KB) must always be kept up to date with a
	// file, so must store both KBs, since the user could have altered both since the last
	// save
	gpApp->StoreGlossingKB(FALSE); // FALSE = don't want backup produced
	gpApp->StoreKB(FALSE);
	
	// remove the phrase box's entry again (this code is sensitive to whether glossing is on
	// or not, because it is an adjustment pertaining to the phrasebox contents only, to undo
	// what was done above - namely, the entry put into either the glossing KB or the normal KB)
	if (pApp->m_pTargetBox != NULL)
	{
		if (pApp->m_pTargetBox->IsShown() && 
			pView->GetFrame()->FindFocus() == (wxWindow*)pApp->m_pTargetBox && !bNoStore)
		{
			wxString emptyStr = _T("");
			if (gbIsGlossing)
			{
				if (!bNoStore)
				{
					pApp->m_pGlossingKB->GetAndRemoveRefString(pApp->m_pActivePile->GetSrcPhrase(),
													emptyStr, useGlossOrAdaptationForLookup);
				}
				pApp->m_pActivePile->GetSrcPhrase()->m_bHasGlossingKBEntry = FALSE;
			}
			else
			{
				if (!bNoStore)
				{
					pApp->m_pKB->GetAndRemoveRefString(pApp->m_pActivePile->GetSrcPhrase(), 
													emptyStr, useGlossOrAdaptationForLookup);
				}
				pApp->m_pActivePile->GetSrcPhrase()->m_bHasKBEntry = FALSE;
			}
		}
	}
	if (pProgDlg != NULL)
		pProgDlg->Destroy();
	if (m_bLegacyDocVersionForSaveAs)
	{
		wxString msg;
		wxString appVerStr;
		appVerStr = pApp->GetAppVersionOfRunningAppAsString();
		msg = msg.Format(_("This document (%s) is now saved on disk in the older (version 3, 4, 5) xml format.\nHowever, if you now make any additional changes to this document or cause it to be saved using this version (%s) of Adapt It, the format of the disk file will be upgraded again to the newer format.\nIf you do not want this to happen, you should immediately close the document, or exit from this version of Adapt It."),gpApp->m_curOutputFilename.c_str(),appVerStr.c_str());
		wxMessageBox(msg,_T(""),wxICON_INFORMATION);
	}
	m_bLegacyDocVersionForSaveAs = FALSE; // restore default
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param	event	-> wxCommandEvent (unused)
/// \remarks
/// Called from: the doc/view framework when wxID_SAVEAS event is generated. Also called from
/// CMainFrame's SyncScrollReceive() when it is necessary to save the current document before
/// opening another document when sync scrolling is on.
/// OnFileSaveAs simply calls DoFileSave() and the latter sets an enum value of save_as
/// 
/// BEW changed 28Apr10 A failure might be due to the Open call failing, in which case the
/// document's file is unchanged, or to the user clicking the Cancel buton in the Save As
/// dialog, in which case the document's file will have been truncated to zero length by
/// the f.Open call done before the Save As dialog was opened. So we need code added in
/// order to recover the document, if either was the case; also we have to handle the
/// possibility that the document may not yet have ever been saved, which changes what we
/// need to do in the event of failure.
/// BEW 16Apr10, added enum, for support of Save As... menu item as well as Save
/// BEW 20Aug10, changed so that the temporary file with derived name
/// "tempSave_<filename>.xml" is saved in the project folder, and restored from there if
/// needed. Doing this means that the GUI never reveals it to the user, which is how it
/// should behave.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnFileSaveAs(wxCommandEvent& WXUNUSED(event)) 
{
	SaveType saveType = save_as;
	wxString renamedFilename; renamedFilename.Empty();
	wxString* pRenamedFilename = &renamedFilename;

	wxString pathToSaveFolder;
	wxULongLong originalSize = 0;
	wxULongLong copiedSize = 0;
	bool bRemovedSuccessfully = TRUE;
	ValidateFilenameAndPath(gpApp->m_curOutputFilename, gpApp->m_curOutputPath, pathToSaveFolder);
	bool bOutputFileExists = ::wxFileExists(gpApp->m_curOutputPath); // original doc file

	// BEW 20Aug, make the folder for saving the temporary backup copy renamed file be the
	// project folder - as the GUI is never opened on this folder while doing SaveAs()
	wxString prefixStr = _T("tempSave_"); // don't localize this, it's never seen
	wxString newNameStr = prefixStr + gpApp->m_curOutputFilename;
	wxString newFileAbsPath = gpApp->m_curProjectPath + gpApp->PathSeparator + newNameStr;
	bool bCopiedSuccessfully = TRUE;
	if (bOutputFileExists)
	{
		// make a unique renamed copy which acts as a temporary backup in case of failure
		// in the call of DoFileSave(), put the copy in the project folder
		bCopiedSuccessfully = ::wxCopyFile(gpApp->m_curOutputPath, newFileAbsPath);
		wxASSERT(bCopiedSuccessfully);
		wxFileName fn(gpApp->m_curOutputPath);
		originalSize = fn.GetSize();
		if (bCopiedSuccessfully)
		{
			wxFileName fnNew(newFileAbsPath);
			copiedSize = fnNew.GetSize();
			wxASSERT( copiedSize == originalSize);
		}
	}
	// in the following call, if pRenamedFilename returns an empty string, then no rename has been
	// requested; first param, value being TRUE, means "show wait/progress dialog"
	bool bUserCancelled = FALSE; // it's initialized to FALSE inside the DoFileSave() call to
	bool bSuccess = DoFileSave(TRUE, saveType, pRenamedFilename, bUserCancelled); 
	if (bSuccess)
	{
		if (bOutputFileExists && bCopiedSuccessfully)
		{
			// remove the temporary backup, the original was saved successfully
			bRemovedSuccessfully = ::wxRemoveFile(newFileAbsPath);
			wxASSERT(bRemovedSuccessfully);
		}

		// we do the rename only provided the save was successful (there won't be a
		// filename clash because that was checked for and prevented within DoFileSave())
		if (!pRenamedFilename->IsEmpty())
		{
			// a rename is wanted, set up its path and do the rename
			wxString newAbsPath = pathToSaveFolder + gpApp->PathSeparator + renamedFilename;
			bool bSuccess = ::wxRenameFile(gpApp->m_curOutputPath, newAbsPath);
			if (bSuccess)
			{
				// update the m_curOutputFilename accordingly, and reset the path to this
				// doc file
				gpApp->m_curOutputFilename = renamedFilename;
				ValidateFilenameAndPath(gpApp->m_curOutputFilename, gpApp->m_curOutputPath, pathToSaveFolder);
			}
		}

		if (m_bDocRenameRequestedForSaveAs)
		{
			// the above ValidateFilenameAndPath() will have reset m_curOutputPath to have
			// the renamed filename in it, so we can use the latter here - to reset the
			// filename Title in the document's window -- next bit of code pinched from
			// OnWizardFinish()
			wxString docPath = gpApp->m_curOutputPath;
			wxDocTemplate* pTemplate = GetDocumentTemplate();
			wxASSERT(pTemplate != NULL);
			wxString typeName,fpath,fname,fext;
			typeName = pTemplate->GetDescription(); // should be "Adapt It" or "Adapt It Unicode"
			wxFileName fn(docPath);
			fn.SplitPath(docPath,&fpath,&fname,&fext);
			//pFrame->SetTitle(fname + _T(" - ") + typeName);
			SetTitle(fname + _T(" - ") + typeName); // use the doc's call, not frame's
			SetFilename(gpApp->m_curOutputPath,TRUE); // TRUE = notify all views

			// a refresh of the status bar info would be appropriate here too
			gpApp->RefreshStatusBarInfo();
		}
	}
	else // handle failure, or a user cancel button click
	{
		if (bOutputFileExists)
		{
			if (bCopiedSuccessfully)
			{
				// something failed (either f.Open() failed, or user Cancelled); but we
				// have a backup to fall back on. Determine if the original remains
				// untruncated, if so, retain it and remove the backup; if not, remove the
				// original and rename the backup to be the original and copy it to the
				// same folder as the original was in
				bool bSomethingOfThatNameExists = ::wxFileExists(gpApp->m_curOutputPath);
				if (bSomethingOfThatNameExists)
				{
					wxULongLong thatSomethingsSize = 0;
					wxFileName fn(gpApp->m_curOutputPath);
					thatSomethingsSize = fn.GetSize();
					if (thatSomethingsSize == originalSize)
					{
						// we are in luck, the original is still good, so remove the backup
						bRemovedSuccessfully = ::wxRemoveFile(newFileAbsPath);
						wxASSERT(bRemovedSuccessfully);
					}
					else
					{
						// the size is different, therefore the original was truncated, so
						// restore the document file using the backup renamed, & move it
						bRemovedSuccessfully = ::wxRemoveFile(gpApp->m_curOutputPath);
						wxASSERT(bRemovedSuccessfully);
						bool bRenamedSuccessfully;
						bRenamedSuccessfully = ::wxRenameFile(newFileAbsPath, gpApp->m_curOutputPath);
						wxASSERT(bRenamedSuccessfully); // and it is moved at the same time
						// I'm not sure if it leaves the renamed file in the project
						// folder?, probably not, but just in case... I'll test for it and
						// if it is there, have it deleted
						if (::wxFileExists(newFileAbsPath))
						{
							// it's still in the project folder, so get rid of it (ignore
							// returned boolean)
							::wxRemoveFile(newFileAbsPath);
						}
					}
				}
				if (!bUserCancelled)
				{
					// user did not hit the Cancel button, so the returned FALSE value was
					// due to a processing error - inform the user, but keep the app alive
					wxMessageBox(_("Warning: document save failed for some reason.\n"),_T(""),
					wxICON_EXCLAMATION);
				}
			}
			else // the original was not copied with a "tempSave_" name prefix, to project folder
			{
				// with no backup copy to fall back on, we have to do the best we can;
				// check if the original is still on disk: it may be, and untouched, or it
				// may be, but truncated due to the user cancelling the dialog; in the
				// former case, if its size is unchanged, then just retain it; but if the
				// size is less, we must remove the truncated fragment and tell the user
				// to do an immediate File / Save
				bool bSomethingOfThatNameExists = ::wxFileExists(gpApp->m_curOutputPath);
				bool bOutOfLuck = FALSE;
				if (bSomethingOfThatNameExists)
				{
					wxULongLong thatSomethingsSize = 0;
					wxFileName fn(gpApp->m_curOutputPath);
					thatSomethingsSize = fn.GetSize();
					if (thatSomethingsSize < originalSize)
					{
						// we are out of luck, the original is truncated
						bOutOfLuck = TRUE;
						bRemovedSuccessfully = ::wxRemoveFile(gpApp->m_curOutputPath);
						wxASSERT(bRemovedSuccessfully);
					}
				}
				if (bOutOfLuck || !bSomethingOfThatNameExists)
				{
					// warn user to do a file save now while the doc is still in memory
					wxString msg;
					msg = msg.Format(_("Something went wrong, so the document protection failed.\nThe adaptation document's file on disk was lost or destroyed, but the document in memory is still good.\nPlease click the Save command on the File menu immediately."));
					wxMessageBox(msg,_("Immediate Save Is Recommended"),wxICON_WARNING);
				}
			}
		}
		else // there was no original already saved to disk when OnFileSaveAs() was invoked
		{
            // either there is no original on disk still, or, there is a truncated save -
            // which happens if the user cancelled from the Save As dialog, because a
            // little of the document file's xml was created, and the file specifier then
            // closed prematurely by the cancel
			bool bTruncatedFragmentExists = ::wxFileExists(gpApp->m_curOutputPath);
			if (bTruncatedFragmentExists)
			{
				bRemovedSuccessfully = ::wxRemoveFile(gpApp->m_curOutputPath);
				wxASSERT(bRemovedSuccessfully);
			}
		}

		// tell the user that if a filename rename was requested, it could not be done
		if (!pRenamedFilename->IsEmpty() && !bUserCancelled)
		{
			wxString msg;
			msg = msg.Format(_("Because the Save As was not successful, the file rename you requested could not be done."));
			wxMessageBox(msg,_("Rename Not Done"),wxICON_WARNING);
		}
	}
	m_bDocRenameRequestedForSaveAs = FALSE; // restore default
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param  curFilename       -> the current output filename (app's m_curOutputFilename member)
/// \param  curPath           <- the full path to current doc folder for saves, including the
///                               filename (app's m_curOutputPath)
/// \param  pathForSaveFolder <-  absolute path to the folder in which the doc will be saved
///                               (returned as a potential convenience to the caller,
///                               which may want to use this for some special purpose)
/// \remarks
/// Called from: OnFileSave(), OnFileSaveAs(), DoFileSave()
/// Takes the current save folder and the current doc filename, and rebuilds the output
/// full absolute path to the document, and ensuring the document filename has .xml
/// extension (version 4.0.0 and higher of Adapt It no longer save *.adt binary files)
/// BEW created 28Apr10, because this encapsulation of checks is need in more than one
/// place. Strictly speaking, the function is unneeded because now that we only save in
/// xml, nothing should ever change the extention to anything else - nevertheless, we'll
/// retain it
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::ValidateFilenameAndPath(wxString& curFilename, wxString& curPath, 
										   wxString& pathForSaveFolder)
{
    // m_curOutputFilename was set when user created the doc; or it an existing doc was
    // read back in; the extension will be .xml
	wxString thisFilename = curFilename;

	// we want an .xml extension - make it so if it happens to be .adt
	thisFilename = MakeReverse(thisFilename);
	wxString extn = thisFilename.Left(4);
	extn = MakeReverse(extn);
	if (extn != _T(".xml"))
	{
		thisFilename = thisFilename.Mid(4); // remove any extension
		thisFilename = MakeReverse(thisFilename);
		thisFilename += _T(".xml"); // it's now guaranteed to be *.xml
	}
	else
	{
		thisFilename = MakeReverse(thisFilename); // it's already *.xml
	}
	curFilename = thisFilename;

	// make sure the backup filename complies too (BEW added 23June07)
	MakeOutputBackupFilenames(curFilename);

	// the m_curOutputPath member can be redone now that m_curOutputFilename is what is wanted
	if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
	{
		pathForSaveFolder = gpApp->m_bibleBooksFolderPath;
	}
	else
	{
		pathForSaveFolder = gpApp->m_curAdaptionsPath;
	}

	curPath = pathForSaveFolder + gpApp->PathSeparator + curFilename;
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the File Menu is about
///                         to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected,
/// and before the menu is displayed.
/// Enables or disables menu and/or toolbar items associated with the wxID_SAVE identifier.
/// If Vertical Editing is in progress the File Save menu item is always disabled, and this
/// handler returns immediately. Otherwise, the item is enabled if the KB exists, and if 
/// m_pSourcePhrases has at least one item in its list, and IsModified() returns TRUE; 
/// otherwise the item is disabled.
/// BEW modified 13Nov09, if the local user has only read-only access to a remote
/// project folder, do not let him save his local copy of the remote document to the
/// remote machine, otherwise the remote user is almost certainly to lose some edits
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateFileSave(wxUpdateUIEvent& event) 
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	if (pApp->m_bReadOnlyAccess)
	{
		event.Enable(FALSE);
		return;
	}
	if (gbVerticalEditInProgress)
	{
		event.Enable(FALSE);
		return;
	}
	if (pApp->m_pKB != NULL && pApp->m_pSourcePhrases->GetCount() > 0 && IsModified())
		event.Enable(TRUE);
	else
		event.Enable(FALSE);	
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the File Menu is about
///                         to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected,
/// and before the menu is displayed.
/// Enables or disables menu and/or toolbar items associated with the wxID_SAVEAS
/// identifier. If Vertical Editing is in progress the File Save As... menu item is always
/// disabled, and this handler returns immediately. Otherwise, the item is enabled if the
/// KB exists, and if m_pSourcePhrases has at least one item in its list, and IsModified()
/// returns TRUE; otherwise the item is disabled.
/// BEW modified 13Nov09, if the local user has only read-only access to a remote
/// project folder, do not let him save his local copy of the remote document to the
/// remote machine, otherwise the remote user is almost certainly to lose some edits
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateFileSaveAs(wxUpdateUIEvent& event) 
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	if (pApp->m_bReadOnlyAccess)
	{
		event.Enable(FALSE);
		return;
	}
	if (gbVerticalEditInProgress)
	{
		event.Enable(FALSE);
		return;
	}
	// whm 14Jan11 removed the && IsModified() test below. Save As should be available
	// whether the document is "dirty" or not.
	//if (pApp->m_pKB != NULL && pApp->m_pSourcePhrases->GetCount() > 0 && IsModified())
	if (pApp->m_pKB != NULL && pApp->m_pSourcePhrases->GetCount() > 0)
		event.Enable(TRUE);
	else
		event.Enable(FALSE);	
}


///////////////////////////////////////////////////////////////////////////////
/// \return TRUE if the document is successfully opened, otherwise FALSE.
/// \param	lpszPathName	-> the name and path of the document to be opened
/// \remarks
/// Called from: the App's DoTransformationsToGlosses( ) function.
/// Opens a document in another project in preparation for transforming its adaptations into 
/// glosses in the current project. The other project's documents get copied in the process, but
/// are left unchanged in the other project. Since we are not going to look at the contents of
/// the document, we don't do anything except get it into memory ready for transforming.
/// BEW changed 31Aug05 so it would handle either .xml or .adt documents automatically (code pinched
/// from start of OnOpenDocument())
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::OpenDocumentInAnotherProject(wxString lpszPathName)
{
	// BEW added 31Aug05 for XML doc support (we have to find out what extension it has
	// and then choose the corresponding code for loading that type of doc 
	wxString thePath = lpszPathName;
	wxString extension = thePath.Right(4);
	extension.MakeLower();
	wxASSERT(extension[0] == _T('.')); // check it really is an extension

	wxFileName fn(thePath);
	wxString fullFileName;
	fullFileName = fn.GetFullName();
	if (extension == _T(".xml"))
	{
		// we have to input an xml document
		bool bReadOK = ReadDoc_XML(thePath,this);
		if (!bReadOK)
		{
			wxString s;
			s = _(
"There was an error parsing in the XML file. If you edited the XML file earlier, you may have introduced an error. Edit it in a word processor then try again.");
			wxMessageBox(s, fullFileName, wxICON_INFORMATION);
			return FALSE; // return FALSE to tell caller we failed
		}
	}
	else
	{
		wxMessageBox(_(
		"Sorry, the wxWidgets version of Adapt It does not read legacy .adt document format; it only reads the .xml format.")
		,fullFileName,wxICON_WARNING);
		return FALSE;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param	event	-> wxCommandEvent (unused)
/// \remarks
/// Called automatically within the doc/view framework when an event associated with the
/// wxID_OPEN identifier (such as File | Open) is generated within the framework. It is
/// also called by the Doc's OnOpenDocument(), by the DocPage's OnWizardFinish() and by
/// SplitDialog's SplitIntoChapters_Interactive() function.
/// Rather than using the doc/view's default behavior for OnFileOpen() this function calls 
/// our own DoFileOpen() function after setting the current work folder to the Adaptations
/// path, or the current book folder if book mode is on.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnFileOpen(wxCommandEvent& WXUNUSED(event)) 
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
    // ensure that the current work folder is the Adaptations one for default; unless book
    // mode is ON, in which case it must the the current book folder.
	wxString dirPath;
	if (pApp->m_bBookMode && !pApp->m_bDisableBookMode)
		dirPath = pApp->m_bibleBooksFolderPath;
	else
		dirPath = pApp->m_curAdaptionsPath;
	bool bOK;
	bOK = ::wxSetWorkingDirectory(dirPath); // ignore failures

	// NOTE: This OnFileOpen() handler calls DoFileOpen() in the App, which now simply
	// calls DoStartWorkingWizard().
	pApp->DoFileOpen();
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param	event	-> wxCommandEvent associated with the wxID_CLOSE identifier.
/// \remarks
/// This function is called automatically by the doc/view framework when an event
/// associated with the wxID_CLOSE identifier is generated. It is also called by: the App's
/// OnFileChangeFolder() and OnAdvancedBookMode(), by the View's OnFileCloseProject(), and
/// by DocPage's OnButtonChangeFolder() and OnWizardFinish() functions.
/// This override of OnFileClose does not close the app, it just clears out all the current view
/// structures, after calling our version of SaveModified. It simply closes files & leave the
/// app ready for other files to be opened etc. Our SaveModified() & this OnFileClose are
/// not OLE compliant. (A New... or Open... etc. will call DeleteContents on the doc structures
/// before a new doc can be made or opened). For version 2.0, which supports glossing, if one KB
/// gets saved, then the other should be too - this needs to be done in our SaveModified( ) function
/// NOTE: we don't change the values of the four flags associated with glossing, because this
/// function may be called for processes which serially open and close each document of a 
/// project, and the flags will have to maintain their values across the calls to ClobberDocument;
/// and certainly ClobberDocumen( ) will be called each time even if this one isn't.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnFileClose(wxCommandEvent& event)
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);	
	if (gbVerticalEditInProgress)
	{
		// don't allow doc closure until the vertical edit is finished
		::wxBell(); 
		return;
	}

	// ensure no selection remains, in case the layout is destroyed later and the app
	// tries to do a RemoveSelection() call on a non-existent layout
	gpApp->GetView()->RemoveSelection();

	if (gpApp->m_bFreeTranslationMode)
	{
		// free translation mode is on, so we must first turn it off
		gpApp->GetFreeTrans()->OnAdvancedFreeTranslationMode(event);
	}
	
	bUserCancelled = FALSE; // default
	if(!OnSaveModified())
	{
		bUserCancelled = TRUE;
		return;
	}

	// BEW added 19Nov09, for read-only support; when a document is closed, attempt
	// to remove any read-only protection that is current for this project folder, because
	// the owning process may have come to have abandoned its ownership prior to the local
	// user closing this document, and that gives the next document opened in this project
	// by the local user the chance to own it for writing
	if (!pApp->m_curProjectPath.IsEmpty())
	{
		// if unowned, or if my process has the ownership, then ownership will be removed
		// at the doc closure (the ~AIROP-*.lock file will have been deleted), and TRUE
		// will be returned to bRemoved - which is then used to clear m_bReadOnlyAccess
		// to FALSE. This makes this project ownable by whoever next opens a document in it.
		bool bRemoved = pApp->m_pROP->RemoveReadOnlyProtection(pApp->m_curProjectPath);
		if (bRemoved)
		{
			pApp->m_bReadOnlyAccess = FALSE; // project folder is now ownable for writing
			pApp->GetView()->canvas->Refresh(); // try force color change back to normal 
				// white background -- it won't work as the canvas is empty, but the
				// removal of read only protection is still done if possible
		}
		else
		{
			pApp->m_bReadOnlyAccess = TRUE; // this project folder is still read-only
				// for this running process, as we are still in this project folder
		}
	}
	
	bUserCancelled = FALSE;
	CAdapt_ItView* pView = (CAdapt_ItView*) GetFirstView();
	wxASSERT(pView != NULL);
	pView->RemoveSelection(); // required, else if a selection exists and user closes doc and
			// does a Rebuild Knowledge Base, the m_selection array will retain hanging
			// pointers, and Rebuild Knowledge Base's RemoveSelection() call will cause a
			// crash
	pView->ClobberDocument();

	// delete the buffer containing the filed-in source text
	if (pApp->m_pBuffer != 0)
	{
		delete pApp->m_pBuffer;
		pApp->m_pBuffer = (wxString*)NULL; // MFC had = 0
	}

	// show "Untitled" etc
	wxString viewTitle = _("Untitled - Adapt It");
	SetTitle(viewTitle);
	SetFilename(viewTitle,TRUE);	// here TRUE means "notify the views" whereas
									// in the MFC version TRUE meant "add to MRU list"
    // Note: SetTitle works, but the doc/view framework overwrites the result with "Adapt
    // It [unnamed1]", etc unless SetFilename() is also used.
	// 
	// whm modified 13Mar09: 
	// When the doc is explicitly closed on Linux, the Ctrl+O hot key doesn't work unless the focus is
	// placed on an existing element such as the toolbar's Open icon (which is where the next action
	// would probably happen).
	CMainFrame* pFrame = pApp->GetMainFrame();
	wxASSERT(pFrame != NULL);
	wxASSERT(pFrame->m_pControlBar != NULL);
	pFrame->m_pControlBar->SetFocus();
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the File Menu is about
///                         to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected, and before
/// the menu is displayed.
/// Enables or disables menu item associated with the wxID_CLOSE identifier.
/// If Vertical Editing is in progress the File Close menu item is disabled, and this handler
/// immediately returns. Otherwise, the item is enabled if m_pSourcePhrases has at least one 
/// item in its list; otherwise the item is disabled.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateFileClose(wxUpdateUIEvent& event) 
{
	if (gbVerticalEditInProgress)
	{
		event.Enable(FALSE);
		return;
	}
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	if (pApp->m_pSourcePhrases->GetCount() > 0)
	{
		event.Enable(TRUE);
	}
	else
	{
		event.Enable(FALSE);
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return TRUE if the document is successfully backed up, otherwise FALSE.
/// \param	pApp	         -> currently unused
/// \param  pRenamedFilename -> points to NULL (the default value), but if a valid
///                             different filename was supplied by the user in the 
///                             Save As dialog, then it points to the wxString which
///                             holds that name (the caller will have verified 
///                             beforehand that it is a valid filename))
/// \remarks
/// Called by the Doc's DoFileSave() function.
/// BEW added 23June07; do no backup if gbDoingSplitOrJoin is TRUE;
/// these operations could produce a plethora of backup docs, especially for a
/// single-chapters document split, so we just won't permit splitting, or joining
/// (except for the resulting joined file), or moving to generate new backups.
/// If rename is requested, we hold off on it to the very end because it will be the case
/// that any pre-existing backup will have the old filename, so we do the backup with the
/// old name, and only after that do we handle the rename. (We don't support renames nor
/// backing up when doing Split Document or Join Document either.)
/// BEW changed 29Apr10, to allow a rename option from user's use of the rename
/// functionality within the Save As dialog
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::BackupDocument(CAdapt_ItApp* WXUNUSED(pApp), wxString* pRenamedFilename)
{
	if (gbDoingSplitOrJoin)
		return TRUE;

	wxFile f; // create a CFile instance with default constructor

	// make the working directory the "Adaptations" one; or a bible book folder 
	// if in book mode
	wxString basePath;
	bool bOK;
	if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
	{
		basePath = gpApp->m_bibleBooksFolderPath;
		bOK = ::wxSetWorkingDirectory(gpApp->m_bibleBooksFolderPath);
	}
	else
	{
		basePath = gpApp->m_curAdaptionsPath;
		bOK = ::wxSetWorkingDirectory(gpApp->m_curAdaptionsPath);
	}
	if (!bOK)
	{
		wxString str;
		//IDS_DOC_BACKUP_PATH_ERR
		if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
			str = str.Format(_(
			"Warning: document backup failed for the path:  %s   No backup was done."),
			GetApp()->m_bibleBooksFolderPath.c_str());
		else
			str = str.Format(_(
			"Warning: document backup failed for the path:  %s   No backup was done."),
			GetApp()->m_curAdaptionsPath.c_str());
		wxMessageBox(str,_T(""), wxICON_EXCLAMATION);
		return FALSE;
	}

    // make sure the backup filename complies too (BEW added 23June07) -- the function sets
    // m_curOutputBackupFilename based on the passed in filename string

    // BEW changed 29Apr10, MakeOutputBackupFilenames() will reset
    // m_curOutputBackupFilename and if there is to be a filename rename done below, we
    // would lose the old value of m_curOutputBackupFilename; so we store the latter so
    // that we can be sure to remove the old backup file if it exists on disk (it won't
    // exist, for example, if the use has only just turned on document backups), and then
    // we'll use m_curOutputBackupFilename's contents, whether renamed or not, to create
    // the wanted backup file
	bool bOldBackupExists = FALSE;
	wxString saveOldFilename = gpApp->m_curOutputBackupFilename;
	if (pRenamedFilename == NULL)
	{
		// no rename is requested, so go ahead with the legacy call
		MakeOutputBackupFilenames(gpApp->m_curOutputFilename);
	}
	else
	{
		// a rename is requested, so the first param should be the new filename
		MakeOutputBackupFilenames(*pRenamedFilename);
	}

	// remove the old backup
	wxString aFilename = saveOldFilename;
	if (wxFileExists(aFilename))
	{
		// this backed up document file is on the disk, so delete it
		bOldBackupExists = TRUE;
		if (!::wxRemoveFile(aFilename))
		{
			wxString s;
			s = s.Format(_(
	"Could not remove the backed up document file: %s; the application will continue"),
			aFilename.c_str());
			wxMessageBox(s, _T(""), wxICON_EXCLAMATION);
			// do nothing else, let the app continue
		}
	}

	// the new backup will have the name which is now in m_curOutputBackupFilename,
	// whether based on the original filename, or a user-renamed filename
	int len = gpApp->m_curOutputBackupFilename.Length();
	if (gpApp->m_curOutputBackupFilename.IsEmpty() || len <= 4)
	{
		wxString str;
		// IDS_DOC_BACKUP_NAME_ERR
		str = str.Format(_(
"Warning: document backup failed because the following name is not valid: %s    No backup was done."),
		gpApp->m_curOutputBackupFilename.c_str());
		wxMessageBox(str,_T(""),wxICON_EXCLAMATION);
		return FALSE;
	}
	
	// copied from DoFileSave - I didn't change the share options, not likely to matter here
	bool bFailed = FALSE;
	if (!f.Open(gpApp->m_curOutputBackupFilename,wxFile::write))
	{
		wxString s;
		s = s.Format(_(
		"Could not open a file stream for backup, in BackupDocument(), for file %s"),
		gpApp->m_curOutputBackupFilename.c_str());
		wxMessageBox(s,_T(""),wxICON_EXCLAMATION);
		// if f failed to Open(), we've just lost any earlier backup file we already had;
		// well, we could build some protection code but we'll not bother as failure to
		// Open() is unlikely, and it's only a backup which gets lost - presumably the doc
		// file itself is still good
		return FALSE; 
	}

	CSourcePhrase* pSrcPhrase;
	CBString aStr;
	CBString openBraceSlash = "</"; // to avoid "warning: 
			// deprecated conversion from string constant to 'char*'"

	// prologue (BEW changed 02July07 to use Bob's huge switch in the
	// GetEncodingStrongForXmlFiles() function which he did, to better support
	// legacy KBs & doc conversions in SILConverters conversion engines)
	gpApp->GetEncodingStringForXmlFiles(aStr);
	DoWrite(f,aStr);

	// add the comment with the warning about not opening the XML file in MS WORD 
	// 'coz is corrupts it - presumably because there is no XSLT file defined for it
	// as well. When the file is then (if saved in WORD) loaded back into Adapt It,
	// the latter goes into an infinite loop when the file is being parsed in.
	aStr = MakeMSWORDWarning(); // the warning ends with \r\n 
								// so we don't need to add them here
	// doc opening tag
	aStr += "<";
	aStr += xml_adaptitdoc;
	aStr += ">\r\n";
	DoWrite(f,aStr);

	// place the <Settings> element at the start of the doc
	aStr = ConstructSettingsInfoAsXML(1);
	DoWrite(f,aStr);

	// add the list of sourcephrases
	SPList::Node* pos = gpApp->m_pSourcePhrases->GetFirst();

	if (m_bLegacyDocVersionForSaveAs)
	{
		// user chose a legacy xml doc build, and so far there is only one such
		// choice, which is docVersion == 4
		wxString endMarkersStr; endMarkersStr.Empty();
		wxString inlineNonbindingEndMkrs; inlineNonbindingEndMkrs.Empty();
		wxString inlineBindingEndMkrs; inlineBindingEndMkrs.Empty();
		while (pos != NULL)
		{
			pSrcPhrase = (CSourcePhrase*)pos->GetData();
            // get a deep copy, so that we can change the data to what is compatible
            // with docc version 4 without corrupting the pSrcPhrase which remains in
            // doc version 5
			CSourcePhrase* pDeepCopy = new CSourcePhrase(*pSrcPhrase);
			pDeepCopy->DeepCopy();
			
			// see comments in DoFileSave()
			FromDocVersion5ToDocVersion4(pDeepCopy, &endMarkersStr, &inlineNonbindingEndMkrs,
										&inlineBindingEndMkrs);

			pos = pos->GetNext();
			aStr = pDeepCopy->MakeXML(1); // 1 = indent the element lines with a single tab
			DeleteSingleSrcPhrase(pDeepCopy,FALSE); // FALSE means "don't try delete a partner pile"
			DoWrite(f,aStr);
		}
	}
	else // use chose a normal docVersion 5 xml build
	{
		while (pos != NULL)
		{
			pSrcPhrase = (CSourcePhrase*)pos->GetData();
			pos = pos->GetNext();
			aStr = pSrcPhrase->MakeXML(1);
			DoWrite(f,aStr);
		}
	}
	// doc closing tag
	aStr = xml_adaptitdoc;
	aStr = openBraceSlash + aStr; //"</" + aStr;
	aStr += ">\r\n";
	DoWrite(f,aStr);

	// close the file
	f.Close();
	f.Flush();
	if (bFailed)
		return FALSE;
	else
		return TRUE;
}

int CAdapt_ItDoc::GetCurrentDocVersion()
{
	return m_docVersionCurrent;
}

void CAdapt_ItDoc::RestoreCurrentDocVersion()
{
	m_docVersionCurrent = (int)VERSION_NUMBER; // VERSION_NUMBER is #defined in AdaptitConstants.h
}

void CAdapt_ItDoc::SetDocVersion(int index)
{	
	switch (index)
	{
	default: // default to the current doc version number
	case 0:
		m_docVersionCurrent = (int)VERSION_NUMBER; // currently #defined as 5 in AdaptitConstant.h
		break;
	case 1:
		m_docVersionCurrent = (int)DOCVERSION4;  // #defined as 4 in AdaptitConstant.h
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return a CBString composed of settings info formatted as XML.
/// \param	nTabLevel	-> defines how many indenting tab characters are placed before each
///			               constructed XML line; 1 gives one tab, 2 gives two, etc.
/// Called by the Doc's BackupDocument(), DoFileSave(), and DoTransformedDocFileSave()
/// functions. Creates a CBString that contains the XML prologue and settings information
/// formatted as XML.
///////////////////////////////////////////////////////////////////////////////
CBString CAdapt_ItDoc::ConstructSettingsInfoAsXML(int nTabLevel)
{
	CBString bstr;
	bstr.Empty();
	CBString btemp;
	int i;
	wxString tempStr;
    // wx note: the wx version in Unicode build refuses to assign a CBString to char
    // numStr[24] so I'll declare numStr as a CBString also
	CBString numStr; //char numStr[24];
#ifdef _UNICODE

	// first line -- element name and 4 attributes
	for (i = 0; i < nTabLevel; i++)
	{
		bstr += "\t"; // tab the start of the line
	}
	bstr += "<Settings docVersion=\"";
    // wx note: The itoa() operator is Microsoft specific and not standard; unknown to g++
    // on Linux/Mac. The wxSprintf() statement below in Unicode build won't accept CBString
    // or char numStr[24] for first parameter, therefore, I'll simply do the int to string
    // conversion in UTF-16 with wxString's overloaded insertion operatior << then convert
    // to UTF-8 with Bruce's Convert16to8() method. [We could also do it here directly with
    // wxWidgets' conversion macros rather than calling Convert16to8() - see the
    // Convert16to8() function in the App.]
	tempStr.Empty();
    // BEW 19Apr10, changed next line for support of Save As... command
	tempStr << GetCurrentDocVersion(); // tempStr is UTF-16
	numStr = gpApp->Convert16to8(tempStr);
	bstr += numStr; // add versionable schema number string

	bstr += "\" sizex=\"";
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
	tempStr << gpApp->m_docSize.x;
	numStr = gpApp->Convert16to8(tempStr);
	bstr += numStr; // add doc width number string
	// TODO: Bruce substituted m_nActiveSequNum for the m_docSize.cy value here. Should this be rolled back?
	// I think he no longer needed it this way for the Dana's progress gauge, and hijacking the m_docSize.cy
	// value for such purposes caused a Beep in OpenDocument on certain size documents because m_docSize was 
	// out of bounds. (BEW note, the error no longer happens, so we should keep this for
	// support of the Dana's Palm OS version of Adapt It)
	bstr += "\" sizey=\"";
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
	tempStr << gpApp->m_nActiveSequNum; // should m_docSize.cy be used instead???
	numStr = gpApp->Convert16to8(tempStr);
	bstr += numStr; // add index of active location's string (Dana uses this) // add doc length number string
	bstr += "\" specialcolor=\"";
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
	tempStr << WxColour2Int(gpApp->m_specialTextColor);
	numStr = gpApp->Convert16to8(tempStr);
	bstr += numStr; // add specialText color number string
	bstr += "\"\r\n";

	// second line -- 5 attributes
	for (i = 0; i < nTabLevel; i++)
	{
		bstr += "\t"; // tab the start of the line
	}
	bstr += "retranscolor=\"";
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
	tempStr << WxColour2Int(gpApp->m_reTranslnTextColor);
	numStr = gpApp->Convert16to8(tempStr);
	bstr += numStr; // add retranslation text color number string
	bstr += "\" navcolor=\"";
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
	tempStr << WxColour2Int(gpApp->m_navTextColor);
	numStr = gpApp->Convert16to8(tempStr);
	bstr += numStr; // add navigation text color number string
	bstr += "\" curchap=\"";
	btemp = gpApp->Convert16to8(gpApp->m_curChapter);
	bstr += btemp; // add current chapter text color number string (app makes no use of this)
	bstr += "\" srcname=\"";
	btemp = gpApp->Convert16to8(gpApp->m_sourceName);
	bstr += btemp; // add name of source text's language
	bstr += "\" tgtname=\"";
	btemp = gpApp->Convert16to8(gpApp->m_targetName);
	bstr += btemp; // add name of target text's language
	bstr += "\"\r\n"; // TODO: EOL chars need adjustment for Linux and Mac???

	// third line - one attribute (potentially large, containing unix strings with filter markers,
	// unknown markers, etc -- entities should not be needed for it though)
	for (i = 0; i < nTabLevel; i++)
	{
		bstr += "\t"; // tab the start of the line
	}
	bstr += "others=\"";
	btemp = gpApp->Convert16to8(SetupBufferForOutput(gpApp->m_pBuffer));
	bstr += btemp; // all all the unix string materials (could be a lot)
	bstr += "\"/>\r\n"; // TODO: EOL chars need adjustment for Linux and Mac??
	return bstr;
#else // regular version

	// first line -- element name and 4 attributes
	for (i = 0; i < nTabLevel; i++)
	{
		bstr += "\t"; // tab the start of the line
	}
	bstr += "<Settings docVersion=\"";
	// wx note: The itoa() operator is Microsoft specific and not standard; unknown to g++ on Linux/Mac.
	// The use of wxSprintf() below seems to work OK in ANSI builds, but I'll use the << insertion
	// operator here as I did in the Unicode build block above, so the code below should be the same
	// as that for the Unicode version except for the Unicode version's use of Convert16to8().
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
    // BEW 19Apr10, changed next line for support of Save As... command
	tempStr << GetCurrentDocVersion();
	numStr = tempStr;
	bstr += numStr; // add versionable schema number string
	bstr += "\" sizex=\"";
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
	tempStr << gpApp->m_docSize.x;
	numStr = tempStr;
	bstr += numStr; // add doc width number string
	// TODO: Bruce substituted m_nActiveSequNum for the m_docSize.cy value here. Should this be rolled back?
	// I think he no longer needed it this way for the Dana's progress gauge, and hijacking the m_docSize.cy
	// value for such purposes caused a Beep in OpenDocument on certain size documents because m_docSize was 
	// out of bounds.
	bstr += "\" sizey=\"";
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
	//wxSprintf(numStr,"%d",(int)gpApp->m_nActiveSequNum);
	tempStr << gpApp->m_nActiveSequNum; // should m_docSize.cy be used instead???
	numStr = tempStr;
	bstr += numStr; // add index of active location's string (Dana uses this) // add doc length number string
	bstr += "\" specialcolor=\"";
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
	tempStr << WxColour2Int(gpApp->m_specialTextColor);
	numStr = tempStr;
	bstr += numStr; // add specialText color number string
	bstr += "\"\r\n"; // TODO: EOL chars need adjustment for Linux and Mac??

	// second line -- 5 attributes
	for (i = 0; i < nTabLevel; i++)
	{
		bstr += "\t"; // tab the start of the line
	}
	bstr += "retranscolor=\"";
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
	tempStr << WxColour2Int(gpApp->m_reTranslnTextColor);
	numStr = tempStr;
	bstr += numStr; // add retranslation text color number string
	bstr += "\" navcolor=\"";
	tempStr.Empty(); // needs to start empty, otherwise << will append the string value of the int
	tempStr << WxColour2Int(gpApp->m_navTextColor);
	numStr = tempStr;
	bstr += numStr; // add navigation text color number string
	bstr += "\" curchap=\"";
	btemp = gpApp->m_curChapter;
	bstr += btemp; // add current chapter text color number string (app makes no use of this)
	bstr += "\" srcname=\"";
	btemp = gpApp->m_sourceName;
	bstr += btemp; // add name of source text's language
	bstr += "\" tgtname=\"";
	btemp = gpApp->m_targetName;
	bstr += btemp; // add name of target text's language
	bstr += "\"\r\n"; // TODO: EOL chars need adjustment for Linux and Mac??

	// third line - one attribute (potentially large, containing unix strings with filter markers,
	// unknown markers, etc -- entities should not be needed for it though)
	for (i = 0; i < nTabLevel; i++)
	{
		bstr += "\t"; // tab the start of the line
	}
	bstr += "others=\"";
	btemp = SetupBufferForOutput(gpApp->m_pBuffer);
	bstr += btemp; // add all the unix string materials (could be a lot)
	bstr += "\"/>\r\n"; // TODO: EOL chars need adjustment for Linux and Mac??
	return bstr;
#endif
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param	buffer	-> a wxString formatted into delimited fields containing the book mode,
///						the book index, the current sfm set, a list of the filtered markers,
///						and a list of the unknown markers
/// \remarks
/// Called from: the AtDocAttr() in XML.cpp.
/// RestoreDocParamsOnInput parses the buffer string and uses its stored information to
/// update the variables held on the App that hold the corresponding information.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::RestoreDocParamsOnInput(wxString buffer)
{
	int dataLen = buffer.Length();
    // This function encapsulates code which formerly was in the Serialize() function, but
    // now that version 3 allows xml i/o, we need this functionality for both binary input
    // and for xml input. For version 2.3.0 and onwards, we don't store the source text in
    // the document so when reading in a document produced from earlier versions, we change
    // the contents of the buffer to a space so that a subsequent save will give a smaller
    // file; recent changes to use this member for serializing in/out the book mode
    // information which is needed for safe use of the MRU list, mean that we might have
    // that info read in, or it could be a legacy document's source text data. We can
    // distinguish these by the fact that the book mode information will be a string of 3,
    // 4 at most, characters followed by a null byte and EOF; whereas a valid legacy doc's
    // source text will be much much longer [see more comments within the function].
	// whm revised 6Jul05
	// We have to account for three uses of the wxString buffer here:
	// 1. Legacy use in which the buffer contained the entire source text.
	//    In this case we simply ignore the text and overwrite it with a space
	// 2. The extended app version in which only the first 3 or 4 characters were
	//    used to store the m_bBookMode and m_nBookIndex values.
	//    In this case the length of buffer string will be < 5 characters, and we handle
	//    the parsing of the book mode and book index as did the extended app.
	// 3. The current version 3 app which adds a unique identifier @#@#: to the beginning
	//    of the string buffer, then follows this by colon delimited fields concatenated in
	//    the buffer string to represent config data.
	//    In this case we verify we have version 3 structure by presence of the @#@#: initial
	//    5 characters, then parse the string like a unix data string. The book mode and book
	//    index will be the first two fields, followed by version 3 specific config values.
    // Note: The gCurrentSfmSet is always changed to be the same as was last saved in the
    // document. The gCurrentFilterMarkers is also always changed to be the same as way
    // last saved in the document. To insure that the current settings in the active
    // USFMAnalysis structs are in agreement with what was last saved in the document we
    // call ResetUSFMFilterStructs().
	//
	// BEW modified 09Nov05 as follows:
    // The current value for the Book Folder mode (True or False), and the current book
    // index (-1 if book mode is not currently on) have to be made to override the values
    // saved in the document if different than what is in the document - but only provided
    // the project is still the same one as the project under which the document was last
    // saved. The reason is as follows. Suppose book mode is off, and you save the document
    // - it goes into the Adaptations folder. Now suppose you use turn book mode on and use
    // the enabled Move command to move the document to the oppropriate book folder. The
    // document is now in a book folder, but internally it still contains the information
    // that it was saved with book mode off, and so no book index is stored there either
    // but just a -1 value. If you then, from within the Start Working wizard open the
    // moved document, the RestoreDocParamsOnInput() function would, unless modified to not
    // do so, restore book mode to off, and set the book index to -1 (whether for an XML
    // file read, or a binary one). This is not what we want or expect. We must also check
    // that the project is unchanged, because the user has the option of opening an
    // arbitary recent document from any project by clicking its name in the MRU list, and
    // the stored source and target language names and book mode info is then used to set
    // up the right path to the document and work out what its project was and make that
    // project the current one -- when you do this, it would be most unlikely that that
    // document was saved after a Move and you did not then open it but changed to the
    // current project, so in this case the book mode and book index as stored in the
    // document SHOULD be used (ie. potentially can reset the mode and change the book
    // index) so that you are returning to the most likely former state. There is no way to
    // detect that the former project's document was moved without being opened within the
    // project, so if the current mode differs, then the constructed path would not be
    // valid and Adapt It will not do the file read -- but this failure is detected and the
    // user is told the document probably no longer exists and is then put into the Start
    // Working wizard -- where he can then turn the mode back on (or off) and locate the
    // document and open it safely and continue working, so the MRU open will not lead to a
    // crash even if the above very unlikely scenario obtains. But if the doc was opened in
    // the earlier project, then using the saved book mode and index values as described
    // above will indeed find it successfully. So, in summary, if the project is different,
    // we must use the stored info in the doc, but if the project is unchanged, then we
    // must override the info in the doc because the fact that it was just opened means
    // that we got it from whatever folder is consistent with the current mode (ie.
    // Adaptations if book folder mode is off, a book folder if it is on) and so the
    // current setting is what we must go with. Whew!! Hope you cotton on to all this!
	wxString curSourceName; // can't use app's m_sourceName & m_targetName because these will already be
	wxString curTargetName; // overwritten so we set curSourceName and curTargetName 
                // by extracting the names from the app's m_curProjectName member which
                // doesn't get updated until the doc read has been successfully completed
	gpApp->GetSrcAndTgtLanguageNamesFromProjectName(gpApp->m_curProjectName, curSourceName, curTargetName);
	bool bSameProject = (curSourceName == gpApp->m_sourceName) && (curTargetName == gpApp->m_targetName);
    // BEW added 27Nov05 to keep settings straight when doc may have been pasted here in
    // Win Explorer but was created and stored in another project
	if (!bSameProject && !gbTryingMRUOpen)
	{
        // bSameProject being FALSE may be because we opened a doc created and saved in a
        // different project and it was a legacy *adt doc and so m_sourceName and
        // m_targetName will have been set wrongly, so we override the document's stored
        // values in favour of curSourceName and curTargetName which we know to be correct
		gpApp->m_sourceName = curSourceName;
		gpApp->m_targetName = curTargetName;
	}

	bool bVersion3Data;
	wxString strFilterMarkersSavedInDoc; // inventory of filtered markers read from the Doc's Buffer

	// initialize strFilterMarkersSavedInDoc to the App's gCurrentFilterMarkers
	strFilterMarkersSavedInDoc = gpApp->gCurrentFilterMarkers;

	// initialize SetSavedInDoc to the App's gCurrentSfmSet
	// Note: The App's Get proj config routine may change the gCurrentSfmSet to PngOnly
	enum SfmSet SetSavedInDoc = gpApp->gCurrentSfmSet;

	// check for version 3 special buffer prefix
	bVersion3Data = (buffer.Find(_T("@#@#:")) == 0);
	wxString field;
	if (bVersion3Data)
	{
		// case 3 above
		// assume we have book mode and index information followed by version 3 data
		int curPos;
		curPos = 0;
		int fieldNum = 0;

		// Insure that first token is _T("@#@#");
		wxASSERT(buffer.Find(_T("@#@#:")) == 0);

		wxStringTokenizer tkz(buffer, _T(":"), wxTOKEN_RET_EMPTY_ALL );

		while (tkz.HasMoreTokens())
		{
			field = tkz.GetNextToken();
			switch(fieldNum)
			{
			case 0: // this is the first field which should be "@#@#" - we don't do anything with it
					break;
			case 1: // book mode field
				{
                    // BEW modified 27Nov05 to only use the T or F values when doing an MRU
                    // open; since for an Open done by a wizard selection in the Document
                    // page, the doc is accessed either in Adaptations folder or a book
                    // folder, and so we must go with whichever mode was the case when we
                    // did that (m_bBookMode false for the former, true for the latter) and
                    // we certainly don't want the document to be able to set different
                    // values (which it could do if it was a foreign document just copied
                    // into a folder and we are opening it on our computer for the first
                    // time). I think the bSameProject value is not needed actually, an MRU
                    // open requires we try using what's on the doc, and an ordinary wizard
                    // open requires us to ignore what's on the doc.
					if (gbTryingMRUOpen)
					{
						// let the app's current setting stand except when an MRU open is tried
						if (field == _T("T"))
						{
							gpApp->m_bBookMode = TRUE;
						}
						else if (field == _T("F"))
						{
							gpApp->m_bBookMode = FALSE;
						}
						else
							goto t;
					}
					break;
				}
			case 2: // book index field
				{
					// see comments above about MRU
					if (gbTryingMRUOpen)
					{
						// let the app's current setting stand except when an MRU open is tried
						// use the file's saved index setting
						int i = wxAtoi(field);
						gpApp->m_nBookIndex = i;
						if (i >= 0  && !gpApp->m_bDisableBookMode)
						{
							gpApp->m_pCurrBookNamePair = ((BookNamePair*)(*gpApp->m_pBibleBooks)[i]);
						}
						else
						{
							// it's a -1 index, or the mode is disabled due to a bad parse of the 
							//  books.xml file, so ensure no named pair and the folder path is empty
							gpApp->m_nBookIndex = -1;
							gpApp->m_pCurrBookNamePair = NULL;
							gpApp->m_bibleBooksFolderPath.Empty();
						}
					}
					break;
				}
			case 3: // gCurrentSfmSet field
				{
					// gCurrentSfmSet is updated below.
					SetSavedInDoc = (SfmSet)wxAtoi(field); //_ttoi(field);
					break;
				}
			case 4: // filtered markers string field
				{
					// gCurrentFilterMarkers is updated below.
					// Note: All Unknown markers that were also filtered, will also be listed
					// in the field input string.
					strFilterMarkersSavedInDoc = field;
							break;
				}
			case 5: // unknown markers string field
				{
					// The doc has not been serialized in yet so we cannot use 
					// GetUnknownMarkersFromDoc() here, so we'll populate the
					// unknown markers arrays here.
					gpApp->m_currentUnknownMarkersStr = field;

					// Initialize the unknown marker data arrays to zero, before we populate them
					// with any unknown markers saved with this document being serialized in
					gpApp->m_unknownMarkers.Clear();
					gpApp->m_filterFlagsUnkMkrs.Clear(); // wxArrayInt

					wxString tempUnkMrksStr = gpApp->m_currentUnknownMarkersStr;

					// Parse out the unknown markers in tempUnkMrksStr 
					wxString unkField, wholeMkr, fStr;

					wxStringTokenizer tkz2(tempUnkMrksStr); // use default " " whitespace here

					while (tkz2.HasMoreTokens())
					{
						unkField = tkz2.GetNextToken();
						// field1 should contain a token in the form of "\xx=0 " or "\xx=1 " 
						int dPos1 = unkField.Find(_T("=0"));
						int dPos2 = unkField.Find(_T("=1"));
						wxASSERT(dPos1 != -1 || dPos2 != -1);
						int dummyIndex;
						if (dPos1 != -1)
						{
							// has "=0", so the unknown marker is unfiltered
							wholeMkr = unkField.Mid(0,dPos1);
							fStr = unkField.Mid(dPos1,2); // get the "=0" filtering delimiter part
							if (!MarkerExistsInArrayString(&gpApp->m_unknownMarkers, wholeMkr, dummyIndex))
							{
								gpApp->m_unknownMarkers.Add(wholeMkr);
								gpApp->m_filterFlagsUnkMkrs.Add(FALSE);
							}
						}
						else
						{
							// has "=1", so the unknown marker is filtered
							wholeMkr = unkField.Mid(0,dPos2);
							fStr = unkField.Mid(dPos2,2); // get the "=1" filtering delimiter part
							if (!MarkerExistsInArrayString(&gpApp->m_unknownMarkers, wholeMkr, dummyIndex))
							{
								gpApp->m_unknownMarkers.Add(wholeMkr);
								gpApp->m_filterFlagsUnkMkrs.Add(TRUE);
							}
						}
					}

					break;
				}
				default:
				{
					// unknown field - ignore
					;
				}
			}
			fieldNum++;
		} // end of while (tkz.HasMoreTokens())
	}
	else if (dataLen < 5) 
	{
		// case 2 above
		// assume we have book mode information - so restore it
		wxChar ch = buffer.GetChar(0);
		if (ch == _T('T'))
			gpApp->m_bBookMode = TRUE;
		else if (ch == _T('F'))
			gpApp->m_bBookMode = FALSE;
		else
		{
			// oops, it's not book mode info, so do the other block instead
			goto t;
		}
		buffer = buffer.Mid(1); // get the index's string
		int i = wxAtoi(buffer);
		gpApp->m_nBookIndex = i;

		// set the BookNamePair pointer, but we don't have enough info for recreating the
		// m_bibleBooksFolderPath here, but SetupDirectories() can recreate it from the
		// doc-serialized m_sourceName and m_targetName strings, and so we do it there;
		// however, if book mode was off when this document was serialized out, then the
		// saved index value was -1, so we must check for this and not try to set up a 
		// name pair when that is the case
		if (i >= 0  && !gpApp->m_bDisableBookMode)
		{
			gpApp->m_pCurrBookNamePair = ((BookNamePair*)(*gpApp->m_pBibleBooks)[i]);
		}
		else
		{
			// it's a -1 index, or the mode is disabled due to a bad parse of the books.xml file, 
			// so ensure no named pair and the folder path is empty
			gpApp->m_pCurrBookNamePair = NULL;
			gpApp->m_bibleBooksFolderPath.Empty();
		}
	}
	else
	{
		// BEW changed 27Nov05, because we only let doc settings be used when MRU was being tried
t:		if (gbTryingMRUOpen /* && !bSameProject */)
		{
			// case 1 above
			// assume we have legacy source text data - for this there was no such thing
			// as book mode in those legacy application versions, so we can have book mode off
			gpApp->m_bBookMode = FALSE;
			gpApp->m_nBookIndex = -1;
			gpApp->m_pCurrBookNamePair = NULL;
			gpApp->m_bibleBooksFolderPath.Empty();
		}
	}

	// whm ammended 6Jul05 below in support of USFM and SFM Filtering
	// Apply any changes to the App's gCurrentSfmSet and gCurrentFilterMarkers indicated
	// by any existing values saved in the Doc's Buffer member
	gpApp->gCurrentSfmSet = SetSavedInDoc;
	gpApp->gCurrentFilterMarkers = strFilterMarkersSavedInDoc;

	// ResetUSFMFilterStructs also calls SetupMarkerStrings() and SetupMarkerStrings
	// builds the various rapid access marker strings including the Doc's unknown marker
	// string pDoc->m_currentUnknownMarkersStr, and adds the unknown markers to the App's
	// gCurrentFilterMarkers string.
	ResetUSFMFilterStructs(gpApp->gCurrentSfmSet, strFilterMarkersSavedInDoc, allInSet);
}


///////////////////////////////////////////////////////////////////////////////
/// \return a wxString
/// \param	pCString	-> pointer to a wxString formatted into delimited fields containing the 
///						book mode, the book index, the current sfm set, a list of the filtered
///						markers, and a list of the unknown markers
/// \remarks
/// Called from: ConstructSettingsInfoAsXML().
/// Creates a wxString composed of delimited fields containing the current book mode, the 
/// book index, the current sfm set, a list of the filtered markers, and a list of the 
/// unknown markers used in the document.
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::SetupBufferForOutput(wxString* pCString)
{
	// This function encapsulates code which formerly was in the Serialize() function, but now that
	// version 3 allows xml i/o, we need this code for both binary output and for xml output
	pCString = pCString; // to quiet warning
	// wx version: whatever contents pCString had will be ignored below
	wxString buffer; // = *pCString;
	// The legacy app (pre version 2+) used to save the source text to the document file, but this
	// no longer happens; so since doc serializating is not versionable I can use this CString buffer
	// to store book mode info (T for true, F for false, followed by the _itot() conversion of the
	// m_nBookIndex value; and reconstruct these when serializing back in. The doc has to have
	// the book mode info in it, otherwise I cannot make MRU list choices restore the correct state
	// and folder when a document was saved in book mode, from a Bible book folder
	// whm added 26Feb05 in support of USFM and SFM Filtering
	// Similar reasons require that we use this m_pBuffer space to store some things pertaining
	// to USFM and Filtering support that did not exist in the legacy app. The need for this
	// arises due to the fact that with version 3, what is actually adapted in the source text
	// is dependent upon which markers are filtered and on which sfm set the user has chosen,
	// which the user can change at any time.
	wxString strResult;
	// add the version 3 special buffer prefix
	buffer.Empty();
	buffer << _T("@#@#:"); // RestoreDocParamsOnInput case 0:
	// add the book mode
	if (gpApp->m_bBookMode)
	{
		buffer << _T("T:");  // RestoreDocParamsOnInput case 1:
	}
	else
	{
		buffer << _T("F:");  // RestoreDocParamsOnInput case 1:
	}
	// add the book index
	buffer << gpApp->m_nBookIndex;  // RestoreDocParamsOnInput case 2:
	buffer << _T(":");

#ifdef _Trace_FilterMarkers
		TRACE0("In SERIALIZE OUT DOC SAVE:\n");
		TRACE1("   App's gCurrentSfmSet = %d\n",gpApp->gCurrentSfmSet);
		TRACE1("   App's gCurrentFilterMarkers = %s\n",gpApp->gCurrentFilterMarkers);
		TRACE1("   Doc's m_sfmSetBeforeEdit = %d\n",gpApp->m_sfmSetBeforeEdit);
		TRACE1("   Doc's m_filterMarkersBeforeEdit = %s\n",gpApp->m_filterMarkersBeforeEdit);
#endif

	// add the sfm user set enum
	// whm note 6May05: We store the gCurrentSfmSet value, not the gProjectSfmSetForConfig in the doc
	// value which may have been different.
	buffer << (int)gpApp->gCurrentSfmSet;
	buffer << _T(":");

	buffer << gpApp->gCurrentFilterMarkers;
	buffer << _T(":");

	buffer << gpApp->m_currentUnknownMarkersStr;
	buffer << _T(":");
	return buffer;
}

///////////////////////////////////////////////////////////////////////////////
/// \return TRUE if file at path was successfully saved; FALSE otherwise
/// \param	path	-> path of the file to be saved
/// \remarks
/// Called from: the App's DoTransformationsToGlosses( ) in order to save another project's
/// document, which has just had its adaptations transformed into glosses in the current
/// project. The full path is passed in - it will have been made an *.xml path in the
/// caller.
/// We don't have to worry about the view, since the document is not visible during any
/// part of the transformation process.
/// We return TRUE if all went well, FALSE if something went wrong; but so far the caller
/// makes no use of the returned Boolean value and just assumes the function succeeded.
/// The save is done to a Bible book folder when that is appropriate, whether or not book
/// mode is currently in effect.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::DoTransformedDocFileSave(wxString path)
{
	wxFile f; // create a CFile instance with default constructor
	bool bFailed = FALSE;
	
	if (!f.Open(path,wxFile::write))
	{
		wxString s;
		s = s.Format(_(
		"When transforming documents, the Open function failed, for the path: %s"),
			path.c_str()); 
		wxMessageBox(s,_T(""), wxICON_EXCLAMATION);
		return FALSE;
	}

	CSourcePhrase* pSrcPhrase;
	CBString aStr;
	CBString openBraceSlash = "</"; // to avoid "warning: 
				// deprecated conversion from string constant to 'char*'"

	// prologue (BEW changed 02July07)
	gpApp->GetEncodingStringForXmlFiles(aStr);
	DoWrite(f,aStr);

	// add the comment with the warning about not opening the XML file in MS WORD 
	// 'coz is corrupts it - presumably because there is no XSLT file defined for it
	// as well. When the file is then (if saved in WORD) loaded back into Adapt It,
	// the latter goes into an infinite loop when the file is being parsed in.
	aStr = MakeMSWORDWarning(); // the warning ends with \r\n so 
								// we don't need to add them here

	// doc opening tag
	aStr += "<";
	aStr += xml_adaptitdoc;
	aStr += ">\r\n"; // eol chars OK in cross-platform version ???
	DoWrite(f,aStr);

	// place the <Settings> element at the start of the doc
	aStr = ConstructSettingsInfoAsXML(1);
	DoWrite(f,aStr);

	// add the list of sourcephrases
	SPList::Node* pos = gpApp->m_pSourcePhrases->GetFirst();
	while (pos != NULL)
	{
		pSrcPhrase = (CSourcePhrase*)pos->GetData();
		pos = pos->GetNext();
		aStr = pSrcPhrase->MakeXML(1); // 1 = indent the element lines with a single tab
		DoWrite(f,aStr);
	}

	// doc closing tag
	aStr = xml_adaptitdoc;
	aStr = openBraceSlash + aStr; //"</" + aStr;
	aStr += ">\r\n"; // eol chars OK in cross-platform version ???
	DoWrite(f,aStr);

	// close the file
	f.Close();
	f.Flush();
	if (bFailed)
		return FALSE;
	else
		return TRUE;
}


///////////////////////////////////////////////////////////////////////////////
/// \return TRUE if currently opened document was successfully saved; FALSE otherwise
/// \remarks
/// Called from: the Doc's OnFileClose() and CMainFrame's OnMRUFile().
/// Takes care of saving a modified document, saving the project configuration file, and 
/// other housekeeping tasks related to file saves.
/// BEW modified 13Nov09: if local user has read-only access to a remote project
/// folder, don't let any local actions result in saving the local copy of the document
/// on the remote machine, otherwise some loss of edits may happen
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::OnSaveModified()
{
	// save project configuration fonts and settings
	CAdapt_ItApp* pApp = &wxGetApp();

	if (pApp->m_bReadOnlyAccess)
	{
		return TRUE; // make the caller think a save etc was done
	}

	wxCommandEvent dummyevent;

	// should not close a document or project while in "show target only" mode; so detect if that 
	// mode is still active & if so, restore normal mode first
	if (gbShowTargetOnly)
	{
		//restore normal mode
		pApp->GetView()->OnFromShowingTargetOnlyToShowingAll(dummyevent);
	}

	// get name/title of document
	wxString name = pApp->m_curOutputFilename;

	wxString prompt;
	bool bUserSavedDoc = FALSE; // use this flag to cause the KB to be automatically saved
								// if the user saves the Doc, without asking; but if the user
								// does not save the doc, he should be asked for the KB
	bool bOK; // we won't care whether it succeeds or not, 
			  // since the later Get... can use defaults
	if (!pApp->m_curProjectPath.IsEmpty())
	{
		if (pApp->m_bUseCustomWorkFolderPath && !pApp->m_customWorkFolderPath.IsEmpty())
		{
			// whm 10Mar10, must save using what paths are current, but when the custom
			// location has been locked in, the filename lacks "Admin" in it, so that it
			// becomes a "normal" project configuration file in m_curProjectPath at the 
			// custom location.
			if (pApp->m_bLockedCustomWorkFolderPath)
				bOK = pApp->WriteConfigurationFile(szProjectConfiguration, pApp->m_curProjectPath,projectConfigFile);
			else
				bOK = pApp->WriteConfigurationFile(szAdminProjectConfiguration, pApp->m_curProjectPath,projectConfigFile);
		}
		else
		{
			bOK = pApp->WriteConfigurationFile(szProjectConfiguration, pApp->m_curProjectPath,projectConfigFile);
		}
		// original code below
		//bOK = pApp->WriteConfigurationFile(szProjectConfiguration,pApp->m_curProjectPath,projectConfigFile);
	}

	bool bIsModified = IsModified();
	if (!bIsModified)
		return TRUE;        // ok to continue

	// BEW added 11Aug06; for some reason IsModified() returns TRUE in the situation when the
	// user first launches the app, creates a project but cancels out of document creation, and
	// then closes the application by clicking the window's close box at top right. We don't
	// want MFC to put up the message "Save changes for ?", because if the user says OK, then
	// the app crashes. So we detect an empty document next and prevent the message from appearing
	if (pApp->m_pSourcePhrases->GetCount() == 0)
	{
		// if there are none, there is no document to save, so useless to go on, 
		// so return TRUE immediately
		return TRUE;
	}

	prompt = prompt.Format(_("The document %s has changed. Do you want to save it? "),name.c_str());
	int result = wxMessageBox(prompt, _T(""), wxYES_NO | wxCANCEL); //AFX_IDP_ASK_TO_SAVE
	wxCommandEvent dummyEvent; // BEW added 29Apr10
	switch (result)
	{
	case wxCANCEL:
		return FALSE;       // don't continue

	case wxYES:
		// If so, either Save or Update, as appropriate
        // BEW changed 29Apr10, DoFileSave_Protected() protects against loss of the
        // document file, which is safer
		//bUserSavedDoc = DoFileSave(TRUE); // TRUE - show wait/progress dialog	
		bUserSavedDoc = DoFileSave_Protected(TRUE); // TRUE - show wait/progress dialog	
		if (!bUserSavedDoc)
		{
			wxMessageBox(_("Warning: document save failed for some reason.\n"),
							_T(""), wxICON_EXCLAMATION);
			return FALSE;       // don't continue
		}
		break;

	case wxNO:
		// If not saving changes, revert the document (& ask for a KB save)
		break;

	default:
		wxASSERT(FALSE);
		break;
	}
	return TRUE;    // keep going	
}

////////////////////////////////////////////////////////////////////////////////////////
// NOTE: This OnSaveDocument() is from the docview sample program. 
//
// The wxWidgets OnSaveDocument() method "Constructs an output file stream 
// for the given filename (which must not be empty), and then calls SaveObject. 
// If SaveObject returns TRUE, the document is set to unmodified; otherwise, 
// an error message box is displayed.
//
//bool CAdapt_ItDoc::OnSaveDocument(const wxString& filename) // from wxWidgets mdi sample
//{
//    CAdapt_ItView* view = (CAdapt_ItView*) GetFirstView();
//
//    if (!view->textsw->SaveFile(filename))
//        return FALSE;
//    Modify(FALSE);
//    return TRUE;
//}

// below is code from the docview sample's original override (which doesn't call the
// base class member) converted to the first Adapt It prototype.
// The wxWidgets OnOpenDocument() "Constructs an input file stream
// for the given filename (which must not be empty), and calls LoadObject().
// If LoadObject returns TRUE, the document is set to unmodified; otherwise,
// an error message box is displayed. The document's views are notified that
// the filename has changed, to give windows an opportunity to update their
// titles. All of the document's views are then updated."
//
//bool CAdapt_ItDoc::OnOpenDocument(const wxString& filename) // from wxWidgets mdi sample
//{
//    CAdapt_ItView* view = (CAdapt_ItView*) GetFirstView();
//    
//    if (!view->textsw->LoadFile(filename)) 
//        return FALSE;
//
//    SetFilename(filename, TRUE);
//    Modify(FALSE);
//    UpdateAllViews();
//    
//    return TRUE;
//}

////////////////////////////////////////////////////////////////////////////////////////
// NOTE: The differences in design between MFC's doc/view framework
// and the wxWidgets implementation of doc/view necessitate some
// adjustments in order to not foul up the state of AI's data structures.
//
// Here below is the contents of the MFC base class CDocument::OnOpenDocument() 
// method (minus __WXDEBUG__ statements):
//BOOL CDocument::OnOpenDocument(LPCTSTR lpszPathName)
//{
//	CFileException fe;
//	CFile* pFile = GetFile(lpszPathName,
//		CFile::modeRead|CFile::shareDenyWrite, &fe);
//	if (pFile == NULL)
//	{
//		ReportSaveLoadException(lpszPathName, &fe,
//			FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
//		return FALSE;
//	}
//	DeleteContents();
//	SetModifiedFlag();  // dirty during de-serialize
//
//	CArchive loadArchive(pFile, CArchive::load | CArchive::bNoFlushOnDelete);
//	loadArchive.m_pDocument = this;
//	loadArchive.m_bForceFlat = FALSE;
//	TRY
//	{
//		CWaitCursor wait;
//		if (pFile->GetLength() != 0)
//			Serialize(loadArchive);     // load me
//		loadArchive.Close();
//		ReleaseFile(pFile, FALSE);
//	}
//	CATCH_ALL(e)
//	{
//		ReleaseFile(pFile, TRUE);
//		DeleteContents();   // remove failed contents
//
//		TRY
//		{
//			ReportSaveLoadException(lpszPathName, e,
//				FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
//		}
//		END_TRY
//		DELETE_EXCEPTION(e);
//		return FALSE;
//	}
//	END_CATCH_ALL
//
//	SetModifiedFlag(FALSE);     // start off with unmodified
//
//	return TRUE;
//}

// Here below is the contents of the wxWidgets base class WxDocument::OnOpenDocument() 
// method (minus alternate wxUSE_STD_IOSTREAM statements):
//bool wxDocument::OnOpenDocument(const wxString& file)
//{
//    if (!OnSaveModified())
//        return FALSE;
//
//    wxString msgTitle;
//    if (wxTheApp->GetAppName() != wxT(""))
//        msgTitle = wxTheApp->GetAppName();
//    else
//        msgTitle = wxString(_("File error"));
//
//    wxFileInputStream store(file);
//    if (store.GetLastError() != wxSTREAM_NO_ERROR)
//    {
//        (void)wxMessageBox(_("Sorry, could not open this file."), msgTitle, wxOK|wxICON_EXCLAMATION,
//                           GetDocumentWindow());
//        return FALSE;
//    }
//    int res = LoadObject(store).GetLastError();
//    if ((res != wxSTREAM_NO_ERROR) &&
//        (res != wxSTREAM_EOF))
//    {
//        (void)wxMessageBox(_("Sorry, could not open this file."), msgTitle, wxOK|wxICON_EXCLAMATION,
//                           GetDocumentWindow());
//        return FALSE;
//    }
//    SetFilename(file, TRUE);
//    Modify(FALSE);
//    m_savedYet = TRUE;
//
//    UpdateAllViews();
//
//    return TRUE;
//}

// The significant differences in the BASE class methods are:
// 1. MFC OnOpenDocument() calls DeleteContents() before loading the archived 
//    file (with Serialize(loadArchive)). The base class DeleteContents() of
//    both MFC and wxWidgets do nothing themselves. The overrides of DeleteContents()
//    have the same code in both versions.
// 2. wxWidgets' OnOpenDocument() does NOT call DeleteContents(), but first calls 
//    OnSaveModified(). OnSaveModified() calls Save() if the doc is dirty. Save() 
//    calls either SaveAs() or OnSaveDocument() depending on whether the doc was 
//    previously saved with a name. SaveAs() takes care of getting a name from 
//    user, then eventually also calls OnSaveDocument(). OnSaveDocument() finally
//    calls SaveObject(store).
// The Implications for our conversion to wxWidgets:
// 1. In our OnOpenDocument() override we need to first call DeleteContents().
// 2. We just comment out the call to the wxDocument::OnOpenDocument() base class 
//    transfer its calls to our override and make any appropriate adjustments to
//    the stream error messages.

///////////////////////////////////////////////////////////////////////////////
/// \return TRUE if file at filename was successfully opened; FALSE otherwise
/// \param	filename	-> the path/name of the file to open
/// \remarks
/// Called from: the App's DoKBRestore() and DiscardDocChanges(), the Doc's 
/// LoadSourcePhraseListFromFile() and DoUnpackDocument(), the View's OnEditConsistencyCheck(),
/// DoConsistencyCheck() and DoRetranslationReport(), the DocPage's OnWizardFinish(), and
/// CMainFrame's SyncScrollReceive() and OnMRUFile().
/// Opens the document at filename and does the necessary housekeeping and initialization of
/// KB data structures for an open document.
/// [see also notes within in the function]
/// BEW added 13Nov09: call of m_bReadOnlyAccess = SetReadOnlyProtection(), in order to give
/// the local user in the project ownership for writing permission (if FALSE is returned)
/// or READ-ONLY access (if TRUE is returned). (Also added to LoadKB() and OnNewDocument()
/// and OnCreate() for the view class.)
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::OnOpenDocument(const wxString& filename) 
{
	//wxLogDebug(_T("3538 at start of OnOpenDocument(), m_bCancelAndSelectButtonPressed = %d"),
	//	gpApp->m_pTargetBox->GetCancelAndSelectFlag());

	// refactored 10Mar09
	gpApp->m_nSaveActiveSequNum = 0; // reset to a default initial value, safe for any length of doc 
	
    // whm Version 3 Note: Since the WX version i/o is strictly XML, we do not need nor use
    // the legacy version's OnOpenDocument() serialization facilities, and can thus avoid
    // the black box problems it caused.
	// Old legacy version notes below:
	// The MFC code called the virtual methods base class OnOpenDocument here:
	//if (!CDocument::OnOpenDocument(lpszPathName)) // The MFC code
	//	return FALSE;
	// The wxWidgets equivalent is:
	//if (!wxDocument::OnOpenDocument(filename))
	//	return FALSE;
	// wxWidgets Notes:
	// 1. The wxWidgets base class wxDocument::OnOpenDocument() method DOES NOT
	//    automatically call DeleteContents(), so we must do so here. Also,
	// 2. The OnOpenDocument() base class handles stream errors with some
	//    generic messages. For these reasons then, rather than calling the base 
	//    class method, we first call DeleteContents(), then we just transfer 
	//    and/or merge the relevant contents of the base class method here, and 
	//    taylor its stream error messages to Adapt It's needs, as was done in 
	//    DoFileSave().

	// BEW added 06Aug05 for XML doc support (we have to find out what extension it has
	// and then choose the corresponding code for loading that type of doc
	// BEW modified 14Nov05 to add the doc instance to the XML doc reading call, and to
	// remove the assert which assumed that there would always be a backslash in the
	// lpszPathName string, and replace it with a test on curPos instead (when doing a
	// consistency check, the full path is not passed in)

	gnBeginInsertionsSequNum = -1; // reset for "no current insertions"
	gnEndInsertionsSequNum = -1; // reset for "no current insertions"

	wxString thePath = filename;
	wxString extension = thePath.Right(4);
	extension.MakeLower();
	wxASSERT(extension[0] == _T('.')); // check it really is an extension
	bool bWasXMLReadIn = TRUE;

	bool bBookMode;
	bBookMode = gpApp->m_bBookMode; // for debugging only. 01Oct06
	int nItsIndex;
	nItsIndex = gpApp->m_nBookIndex; // for debugging only

	// get the filename
	wxString fname = thePath;
	fname = MakeReverse(fname);
	int curPos = fname.Find(gpApp->PathSeparator);
	if (curPos != -1)
	{
		fname = fname.Left(curPos);
	}
	fname = MakeReverse(fname);
	wxString extensionlessName;

	if (extension == _T(".xml"))
	{
		// we have to input an xml document
		// BEW modified 07Nov05, to add pointer to the document, since we may be reading
		// in XML documents for joining to the current document, and we want to
		// populate the correct document's CSourcePhrase list
		wxString thePath = filename;
		wxFileName fn(thePath);
		wxString fullFileName;
		fullFileName = fn.GetFullName();
		bool bReadOK = ReadDoc_XML(thePath,this);
		if (!bReadOK)
		{
			wxString s;
			if (gbTryingMRUOpen)
			{
				// a nice warm & comfy message about the file perhaps not actually existing
				// any longer will keep the user from panic
				// IDS_MRU_NO_FILE
				s = _(
"The file you clicked could not be opened. It probably no longer exists. When you click OK the Start Working... wizard will open to let you open a project and document from there instead.");
				wxMessageBox(s, fullFileName, wxICON_INFORMATION);
				wxCommandEvent dummyevent;
				OnFileOpen(dummyevent); // have another go, via the Start Working wizard
				return TRUE;
			}
			else
			{
				// uglier message because we expect a good read, but we allow the user to continue
				// IDS_XML_READ_ERR
				s = _(
"There was an error parsing in the XML file.\nIf you edited the XML file earlier, you may have introduced an error.\nEdit it in a word processor then try again.");
				wxMessageBox(s, fullFileName, wxICON_INFORMATION);
			}
			return TRUE; // return TRUE to allow the user another go at it
		}
	}

	if (gpApp->m_bWantSourcePhrasesOnly) return TRUE; // Added by JF.  
        // From here on in for the rest of this function, all we do is set globals,
        // filenames, config file parameters, and change the view, all things we're not to
        // do if m_bWantSourcePhrasesOnly is set. Hence, we simply exit early; because all
        // we are wanting is the list of CSourcePhrase instances.
        
	// update the window title
	SetDocumentWindowTitle(fname, extensionlessName);

	wxFileName fn(filename);
	wxString filenameStr = fn.GetFullName(); //GetFileName(filename);
	if (bWasXMLReadIn)
	{
		// it was an *.xml file the user opened
		gpApp->m_curOutputFilename = filenameStr;

		// construct the backup's filename
		// BEW changed 23June07 to allow for the possibility that more than one period
		// may be used in a filename
		filenameStr = MakeReverse(filenameStr);
		filenameStr.Remove(0,4); //filenameStr.Delete(0,4); // remove "lmx."
		filenameStr = MakeReverse(filenameStr);
		filenameStr += _T(".BAK");
		//filenameStr += _T(".xml"); // produces *.BAK.xml BEW removed 3Mar11
	}
	gpApp->m_curOutputBackupFilename = filenameStr;
	gpApp->m_curOutputPath = filename;

    // filenames and paths for the doc and any backup are now guaranteed to be 
    // what they should be
	CAdapt_ItApp* pApp = GetApp();
	CAdapt_ItView* pView = pApp->GetView();
//#ifdef __WXDEBUG__
//	wxLogDebug(_T("OnOpenDocument at %d ,  Active Sequ Num  %d"),1,pApp->m_nActiveSequNum);
//#endif

	int width = wxSystemSettings::GetMetric(wxSYS_SCREEN_X);
	if (pApp->m_docSize.GetWidth() < 100 || pApp->m_docSize.GetWidth() > width)
	{
		::wxBell(); // tell me it was wrong
		pApp->m_docSize = wxSize(width - 40,600); // ensure a correctly sized document
		pApp->GetMainFrame()->canvas->SetVirtualSize(pApp->m_docSize);
	}

	// refactored version: try the following here
	CLayout* pLayout = GetLayout();
	pLayout->SetLayoutParameters(); // calls InitializeCLayout() and UpdateTextHeights()
									// and other setters
#ifdef _NEW_LAYOUT
	bool bIsOK = pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_and_piles);
#else
	bool bIsOK = pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_and_piles);
#endif
	if (!bIsOK)
	{
		// unlikely to fail, so just have something for the developer here
		wxMessageBox(_T("Error. RecalcLayout(TRUE) failed in OnOpenDocument()"),
		_T(""),wxICON_STOP);
		wxASSERT(FALSE);
		wxExit();
	}

	if (pApp->m_pSourcePhrases->GetCount() == 0)
	{
		// nothing to show
		// IDS_NO_DATA
		wxMessageBox(_(
"There is no data in this file. This document is not properly formed and so cannot be opened. Delete it."),
		_T(""), wxICON_EXCLAMATION);
		return FALSE;
	}
	pApp->m_pActivePile = GetPile(0); // a safe default for starters....
	pApp->m_nActiveSequNum = 0; // and this is it's sequ num; use these values 
								// unless changed below
	// BEW added 21Apr08; clean out the global struct gEditRecord & clear its deletion lists,
	// because each document, on opening it, it must start with a truly empty EditRecord; and
	// on doc closure and app closure, it likewise must be cleaned out entirely (the deletion
	// lists in it have content which persists only for the life of the document currently open)
	pView->InitializeEditRecord(gEditRecord);
	gEditRecord.deletedAdaptationsList.Clear(); // remove any stored deleted adaptation strings
	gEditRecord.deletedGlossesList.Clear(); // remove any stored deleted gloss strings
	gEditRecord.deletedFreeTranslationsList.Clear(); // remove any stored deleted free translations

	// if we get here by having chosen a document file from the Recent_File_List, then it is
	// possible to in that way to choose a file from a different project; so the app will crash
	// unless we here set up the required directory structures and load the document's KB
	if (pApp->m_pKB == NULL)
	{
        //ensure we have the right KB & project (the parameters SetupDirectories() needs
        //are stored in the document, so will be already serialized back in; and our
        //override of CWinApp's OnOpenRecentFile() first does a CloseProject (which ensures
        //m_pKB gets set to null) before OnOpenDocument is called. 
        //WX Note: We override CMainFrame's OnMRUFile().
        // If we did not come via a File... MRU file choice, then the m_pKB will not be
        // null and the following call won't be made. The SetupDirectories call has
        // everything needed in order to silently change the project to that of the
        // document being opened
        // Note; if gbAbortMRUOpen is TRUE, we can't open this document because it was
        // saved formerly in folder mode, and folder mode has become disabled (due to a bad
        // file read of books.xml or a failure to parse the XML document therein correctly)
        // The gbAbortMRUOpen flag is only set true by a test within SetupDirectories() -
        // and we are interested only in this after a click of an MRU item on the File
        // menu.
		pApp->SetupDirectories();
		if (gbViaMostRecentFileList)
		{
			// test for the ability to get the needed information from the document - we can't get
			// the BookNamePair info (needed for setting up the correct project path) if the
			// books.xml parse failed, the latter failure sets m_bDisableBookMode to true.
			if (gbAbortMRUOpen)
			{
				pApp->GetView()->ClobberDocument();
				gbAbortMRUOpen = FALSE; // restore default value
				// IDS_NO_MRU_NOW
				wxMessageBox(_(
"Sorry, while book folder mode is disabled, using the Most Recently Used menu to click a document saved earlier in book folder mode will not open that file."),
				_T(""), wxICON_EXCLAMATION);
				return FALSE;
			}
			bool bSaveFlag = gpApp->m_bBookMode;
			int nSaveIndex = gpApp->m_nBookIndex;
            // the next call may clobber user's possible earlier choice of mode and index,
            // so restore these after the call (project config file is not updated until
            // project exitted, and so the user could have changed the mode or the book
            // folder from what is in the config file)
			gpApp->GetProjectConfiguration(gpApp->m_curProjectPath); // ensure gbSfmOnlyAfterNewlines
															   // is set to what it should be,
															   // and same for gSFescapechar
			gpApp->m_bBookMode = bSaveFlag;
			gpApp->m_nBookIndex = nSaveIndex;
		}
		gbAbortMRUOpen = FALSE; // make sure the flag has its default setting again
		gbViaMostRecentFileList = FALSE; // clear it to default setting
	}

	gbDoingInitialSetup = FALSE; // turn it back off, the pApp->m_targetBox now exists, etc

    // place the phrase box, but inhibit placement on first pile if doing a consistency
    // check, because otherwise whatever adaptation is in the KB for the first word/phrase
    // gets removed unconditionally from the KB when that is NOT what we want to occur!
	if (!gbConsistencyCheckCurrent)
	{
		// ensure its not a retranslation - if it is, move the active location to first 
		// non-retranslation pile
		if (pApp->m_pActivePile->GetSrcPhrase()->m_bRetranslation)
		{
			// it is a retranslation, so move active location
			CPile* pNewPile;
			CPile* pOldPile = pApp->m_pActivePile;
			do {
				pNewPile = pView->GetNextPile(pOldPile);
				wxASSERT(pNewPile);
				pOldPile = pNewPile;
			} while (pNewPile->GetSrcPhrase()->m_bRetranslation);
			pApp->m_pActivePile = pNewPile;
			pApp->m_nActiveSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
		}

		// BEW added 10Jun09, support phrase box matching of the text colour chosen
		if (gbIsGlossing && gbGlossingUsesNavFont)
		{
			pApp->m_pTargetBox->SetOwnForegroundColour(pLayout->GetNavTextColor());
		}
		else
		{
			pApp->m_pTargetBox->SetOwnForegroundColour(pLayout->GetTgtColor());
		}

		pView->PlacePhraseBox(pApp->m_pActivePile->GetCell(1),2); // selector = 2, because we
			// were not at any previous location, so inhibit the initial StoreText call,
			// but enable the removal from KB storage of the adaptation text (see comments under
			// the PlacePhraseBox function header, for an explanation of selector values)

		// save old sequ number in case required for toolbar's Back button - no earlier one yet,
		// so just use the value -1
		gnOldSequNum = -1;
	}
	
	// update status bar with project name
	gpApp->RefreshStatusBarInfo();

	// determine m_curOutputPath, so it can be saved to config files as m_lastDocPath
	if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
	{
		pApp->m_curOutputPath = pApp->m_bibleBooksFolderPath + pApp->PathSeparator 
								+ pApp->m_curOutputFilename;
	}
	else
	{
		pApp->m_curOutputPath = pApp->m_curAdaptionsPath + pApp->PathSeparator 
								+ pApp->m_curOutputFilename;
	}

    // BEW added 01Oct06: to get an up-to-date project config file saved (in case user
    // turned on or off the book mode in the wizard) so that if the app subsequently
    // crashes, at least the next launch will be in the expected mode (see near top of
    // CAdapt_It.cpp for an explanation of the gbPassedAppInitialization global flag)
	// BEW added 12Nov09, m_bAutoExport test to suppress writing the project config file
	// when export is done from the command line export command
	if (gbPassedAppInitialization && !pApp->m_curProjectPath.IsEmpty() && !pApp->m_bAutoExport)
	{
        // BEW on 4Jan07 added change to WriteConfiguration to save the external current
        // work directory and reestablish it at the end of the WriteConfiguration call,
        // because the latter function resets the current directory to the project folder
        // before saving the project config file - and this clobbered the restoration of a
        // KB from the 2nd doc file accessed
		bool bOK;
		if (pApp->m_bUseCustomWorkFolderPath && !pApp->m_customWorkFolderPath.IsEmpty())
		{
			// whm 10Mar10, must save using what paths are current, but when the custom
			// location has been locked in, the filename lacks "Admin" in it, so that it
			// becomes a "normal" project configuration file in m_curProjectPath at the 
			// custom location.
			if (pApp->m_bLockedCustomWorkFolderPath)
				bOK = pApp->WriteConfigurationFile(szProjectConfiguration, pApp->m_curProjectPath,projectConfigFile);
			else
				bOK = pApp->WriteConfigurationFile(szAdminProjectConfiguration, pApp->m_curProjectPath,projectConfigFile);
		}
		else
		{
			bOK = pApp->WriteConfigurationFile(szProjectConfiguration, pApp->m_curProjectPath,projectConfigFile);
		}
	}

	// wx version addition:
	// Add the file to the file history MRU
	// BEW added 12Nov09, m_bAutoExport test to suppress history update when export is
	// done from the command line export command
	if (!pApp->m_curOutputPath.IsEmpty() && !pApp->m_bAutoExport)
	{
		wxFileHistory* fileHistory = pApp->m_pDocManager->GetFileHistory();
		fileHistory->AddFileToHistory(pApp->m_curOutputPath);
        // The next two lines are a trick to get past AddFileToHistory()'s behavior of
        // extracting the directory of the file you supply and stripping the path of all
        // files in history that are in this directoy. RemoveFileFromHistory() doesn't do
        // any tricks with the path, so the following is a dirty fix to keep the full
        // paths.
		fileHistory->AddFileToHistory(wxT("[tempDummyEntry]"));
		fileHistory->RemoveFileFromHistory(0); // 
	}

	// BEW added 12Nov09, do the auto-export here, if asked for, and shut
	// down the app before returning; otherwise, continue for normal user
	// GUI interaction
	if (pApp->m_bAutoExport)
	{
		wxLogNull logNo; // avoid spurious messages from the system

		// set up output path using m_autoexport_outputpath
		wxString docName = pApp->m_autoexport_docname;
		docName = MakeReverse(docName);
		docName = docName.Mid(4); // remove reversed ".xml"
		docName = MakeReverse(docName); // back to normal order without extension
		docName = docName + _T(".txt"); // make a plain text file
		pApp->m_curOutputFilename = docName;
		pApp->m_curOutputPath = pApp->m_autoexport_outputpath + pApp->PathSeparator + docName;

		// pinch what I need from ExportFunctions.cpp
		wxString target;	// a export data's buffer
		target.Empty();
		int nTextLength;
		nTextLength = RebuildTargetText(target);
		FormatMarkerBufferForOutput(target, targetTextExport);
		target = RemoveMultipleSpaces(target);

		// now write out the exported data string
		wxFile f;
		if( !f.Open(pApp->m_curOutputPath, wxFile::write))
		{
			wxString msg;
			msg = msg.Format(_("Unable to open the file for exporting the target text with path:\n%s"),pApp->m_curOutputPath.c_str());
			wxMessageBox(msg,_T(""),wxICON_EXCLAMATION);
			pApp->OnExit();
			return FALSE;
		}
		#ifndef _UNICODE // ANSI
		f.Write(target);
		#else // _UNICODE
		wxFontEncoding enc = wxFONTENCODING_UTF8; 
		pApp->ConvertAndWrite(enc,&f,target);
		#endif // for _UNICODE
		f.Close();
		// shut down forcefully
		//pApp->OnExit();
		unsigned long pid = ::wxGetProcessId();
		enum wxKillError killErr;
		int rv = ::wxKill(pid,wxSIGTERM,&killErr); // makes OnExit() be called
		rv  = rv; // prevent compiler warning

		return FALSE;
	}
	else
	{
		// BEW added 13Nov09, for setting or denying ownership for writing permission.
		// This is something we want to do each time a doc is opened - if the local user
		// already has ownership for writing, no change is done and he retains it; but
		// if he had read only access, and the other person has relinquished the project,
		// then the local user will now get ownership. We do this here in the else block
		// because we don't want to support this functionality for automated adaptation
		// exports from the command line because those have the app open only for a few 
		// seconds at most, and when they happen they change nothing so can be done
		// safely no matter who currently has ownership for writing.
		pApp->m_bReadOnlyAccess = pApp->m_pROP->SetReadOnlyProtection(pApp->m_curProjectPath);

		if (pApp->m_bReadOnlyAccess)
		{
			// if read only access is turned on, force the background colour change to show
			// now, instead of waiting for a user action requiring a canvas redraw
			pApp->GetView()->canvas->Refresh(); // needed? the call in OnIdle() is more effective
		}
	}

	return TRUE;
}

CLayout* CAdapt_ItDoc::GetLayout()
{
	CAdapt_ItApp* pApp = &wxGetApp();
	return pApp->m_pLayout;
}

// return the CPile* at the passed in index, or NULL if the index is out of bounds;
// the pile list is at CLayout::m_pileList.
// CAdapt_ItView also has a member function of the same name
CPile* CAdapt_ItDoc::GetPile(const int nSequNum)
{
	// refactored 10Mar09, for new view layout design (no bundles)
	CLayout* pLayout = GetLayout();
	wxASSERT(pLayout != NULL);
	PileList* pPiles = pLayout->GetPileList();
	int nCount = pPiles->GetCount();
	if (nSequNum < 0 || nSequNum >= nCount)
	{
		// bounds error, so return NULL
		return (CPile*)NULL;
	}
	PileList::Node* pos = pPiles->Item(nSequNum); // relies on parallelism of 
								// the m_pSourcePhrases and m_pileList lists
	wxASSERT(pos != NULL);
	return pos->GetData();
}

///////////////////////////////////////////////////////////////////////////////
/// \return TRUE if the current document has been modified; FALSE otherwise
/// \remarks
/// Called from: the App's GetDocHasUnsavedChanges(), OnUpdateFileSave(), OnSaveModified(),
/// CMainFrame's SyncScrollReceive() and OnIdle().
/// Internally calls the wxDocument::IsModified() method and the canvas->IsModified() method.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsModified() const // from wxWidgets mdi sample
{
  CAdapt_ItView* view = (CAdapt_ItView*) GetFirstView();

  if (view)
  {
      return (wxDocument::IsModified() || wxGetApp().GetMainFrame()->canvas->IsModified());
  }
  else
      return wxDocument::IsModified();
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param mod		-> if FALSE, discards any edits
/// \remarks
/// Called from: all places that need to set the document as either dirty or clean including:
/// the App's DoUsfmFilterChanges() and DoUsfmSetChanges(), the Doc's OnNewDocument(), 
/// OnFileSave(), OnCloseDocument(), the View's PlacePhraseBox(), StoreText(), StoreTextGoingBack(),
/// ClobberDocument(), OnAdvancedRemoveFilteredFreeTranslations(), OnButtonDeleteAllNotes(),
/// OnAdvancedRemoveFilteredBacktranslations(), the DocPage's OnWizardFinish(), the CKBEditor's
/// OnButtonUpdate(), OnButtonAdd(), OnButtonRemove(), OnButtonMoveUp(), OnButtonMoveDown(),
/// the CMainFrame's OnMRUFile(), the CNoteDlg's OnBnClickedNextBtn(), OnBnClickedPrevBtn(),
/// OnBnClickedFirstBtn(), OnBnClickedLastBtn(), OnBnClickedFindNextBtn(), the CPhraseBox's
/// OnPhraseBoxChanged(), CViewFilteredMaterialDlg's UpdateContentOnRemove(), OnOK(), 
/// OnBnClickedRemoveBtn().
/// Sets the Doc's dirty flag according to the value of mod by calling wxDocument::Modify(mod).
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::Modify(bool mod) // from wxWidgets mdi sample
{
  CAdapt_ItView* view = (CAdapt_ItView*) GetFirstView();
  CAdapt_ItApp* pApp = &wxGetApp();
  wxASSERT(pApp != NULL);
  wxDocument::Modify(mod);

  if (!mod && view && pApp->GetMainFrame()->canvas)
    pApp->GetMainFrame()->canvas->DiscardEdits();
}


///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param      pList -> pointer to a SPList of source phrases
/// \remarks
/// Called from: the View's InitializeEditRecord(), OnEditSourceText(), 
/// OnCustomEventAdaptationsEdit(), and OnCustomEventGlossesEdit().
/// If pList has any items this function calls DeleteSingleSrcPhrase() for each item in the 
/// list.
/// BEW 26Mar10, no changes needed for support of doc version 5
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::DeleteSourcePhrases(SPList* pList, bool bDoPartnerPileDeletionAlso)
{
    // BEW added 21Apr08 to pass in a pointer to the list which is to be deleted (overload
    // of the version which has no input parameters and internally assumes the list is
    // m_pSourcePhrases) This new version is required so that in the refactored Edit Source
    // Text functionality we can delete the deep-copied sublists using this function.
	// BEW modified 27May09, to take the bool bDoPartnerPileDeletionAlso parameter,
	// defaulting to FALSE because the deep copied sublists never have partner piles
	//CAdapt_ItApp* pApp = &wxGetApp();
	//wxASSERT(pApp != NULL);
	if (pList != NULL)
	{
		if (!pList->IsEmpty())
		{
			// delete all the tokenizations of the source text
			SPList::Node *node = pList->GetFirst();
			while (node)
			{
				CSourcePhrase* pSrcPhrase = (CSourcePhrase*)node->GetData();
				node = node->GetNext();
#ifdef __WXDEBUG__
				//wxLogDebug(_T("   DeleteSourcePhrases pSrcPhrase at %x = %s"),
				//pSrcPhrase->m_srcPhrase, pSrcPhrase->m_srcPhrase.c_str());
#endif
				DeleteSingleSrcPhrase(pSrcPhrase, bDoPartnerPileDeletionAlso); // default
					// for the boolean passed in to DeleteSourcePhrases() is FALSE
			}
			pList->Clear(); 
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \remarks
/// Called from: the Doc's DeleteContents(), the View's ClobberDocument().
/// If the App's m_pSourcePhrases SPList has any items this function calls 
/// DeleteSingleSrcPhrase() for each item in the list.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::DeleteSourcePhrases()
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	if (pApp->m_pSourcePhrases != NULL)
	{
		if (!pApp->m_pSourcePhrases->IsEmpty())
		{
			// delete all the tokenizations of the source text
			SPList::Node *node = pApp->m_pSourcePhrases->GetFirst();
			while (node)
			{
				CSourcePhrase* pSrcPhrase = (CSourcePhrase*)node->GetData();
				node = node->GetNext();
//#ifdef __WXDEBUG__
//				wxString msg;
//				msg = msg.Format(_T("Deleting    %s    at sequnum =  %d"),
//					pSrcPhrase->m_srcPhrase.c_str(), pSrcPhrase->m_nSequNumber);
//				wxLogDebug(msg);
//#endif
				DeleteSingleSrcPhrase(pSrcPhrase,FALSE); // FALSE is the
					// value for bDoPartnerPileDeletionAlso, because it is
					// more efficient to delete them later en masse with a call
					// to CLayout::DestroyPiles(), rather than one by one, as
					// the  one by one way involves a search to find the
					// partner, so it is slower
			}
			pApp->m_pSourcePhrases->Clear(); 
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param		pSrcPhrase -> the source phrase to be deleted
/// \remarks
/// Deletes the passed in CSourcePhrase instance, and if the bDoPartnerPileDeletionAlso bool value
/// is TRUE (its default value), then the partner pile in the CLayout::m_pileList list which
/// points at it is also deleted. Pass FALSE for this boolean if the CSourcePhrase being destroyed
/// is a temporary one in a list other than m_pSourcePhrases.
/// 
/// Called from: the App's DoTransformationsToGlosses(), DeleteSourcePhraseListContents(),
/// the Doc's DeleteSourcePhrases(), ConditionallyDeleteSrcPhrase(), 
/// ReconstituteOneAfterPunctuationChange(), ReconstituteOneAfterFilteringChange(),
/// DeleteListContentsOnly(), ReconstituteAfterPunctuationChange(), the View's 
/// ReplaceCSourcePHrasesInSpan(), TransportWidowedEndmarkersToFollowingContext(), 
/// TransferCOmpletedSrcPhrases(), and CMainFrame's DeleteSourcePhrases_ForSyncScrollReceive().
/// 
/// Clears and deletes any m_pMedialMarkers, m_pMedialPuncts and m_pSavedWords before deleting
/// pSrcPhrase itself.
/// BEW 11Oct10, to support doc version 5's better handling of USFM fixedspace symbol ~,
/// we also must delete instances storing word1~word2 kind of content - if ~ is present,
/// then m_pSavedWords will contain two child CSourcePhrase pointers also to be deleted -
/// fortunately no code changes are needed to handle this.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::DeleteSingleSrcPhrase(CSourcePhrase* pSrcPhrase, bool bDoPartnerPileDeletionAlso)
{
	// refactored 12Mar09
	if (pSrcPhrase == NULL)
		return;

	// if requested delete the CPile instance in CLayout::m_pileList which 
	// points to this pSrcPhrase
	if (bDoPartnerPileDeletionAlso)
	{
        // this call is safe to make even if there is no partner pile, or if a matching
        // pointer is not in the list
		DeletePartnerPile(pSrcPhrase); // marks its strip as invalid as well
	}

	if (pSrcPhrase->m_pMedialMarkers != NULL)
	{
		if (pSrcPhrase->m_pMedialMarkers->GetCount() > 0)
		{
			pSrcPhrase->m_pMedialMarkers->Clear();
		}
		delete pSrcPhrase->m_pMedialMarkers;
		pSrcPhrase->m_pMedialMarkers = (wxArrayString*)NULL;
	}

	if (pSrcPhrase->m_pMedialPuncts != NULL)
	{
		if (pSrcPhrase->m_pMedialPuncts->GetCount() > 0)
		{
			pSrcPhrase->m_pMedialPuncts->Clear();
		}
		delete pSrcPhrase->m_pMedialPuncts;
		pSrcPhrase->m_pMedialPuncts = (wxArrayString*)NULL;
	}

	// also delete any saved CSourcePhrase instances forming a phrase (and these
	// will never have medial puctuation nor medial markers nor will they store
	// any saved minimal phrases since they are CSourcePhrase instances for single
	// words only (nor will it point to any CRefString instances) (but these will
	// have SPList instances on heap, so must delete those) 
	// BEW note, 11Oct10, this block will also handle deletion of conjoined words using
	// USFM fixedspace symbol, ~
	if (pSrcPhrase->m_pSavedWords != NULL)
	{
		if (pSrcPhrase->m_pSavedWords->GetCount() > 0)
		{
			SPList::Node *node = pSrcPhrase->m_pSavedWords->GetFirst();
			while (node)
			{
				CSourcePhrase* pSP = (CSourcePhrase*)node->GetData();
				node = node->GetNext(); // need this for wxList
				delete pSP->m_pSavedWords;
				pSP->m_pSavedWords = (SPList*)NULL;
				delete pSP->m_pMedialMarkers;
				pSP->m_pMedialMarkers = (wxArrayString*)NULL;
				delete pSP->m_pMedialPuncts;
				pSP->m_pMedialPuncts = (wxArrayString*)NULL;
				delete pSP;
				pSP = (CSourcePhrase*)NULL;
			}
		}
		delete pSrcPhrase->m_pSavedWords; // delete the SPList* too
		pSrcPhrase->m_pSavedWords = (SPList*)NULL;
	}
	delete pSrcPhrase;
	pSrcPhrase = (CSourcePhrase*)NULL;
}

///////////////////////////////////////////////////////////////////////////////
/// \return     nothing
/// \param		pSrcPhrase -> the source phrase that was deleted
/// \remarks
/// Created 12Mar09 for layout refactoring. The m_pileList's CPile instances point to
/// CSourcePhrase instances and deleting a CSourcePhrase from the doc's m_pSourcePhrases
/// list usually needs to also have the CPile instance which points to it also deleted from
/// the corresponding place in the m_pileList. That task is done here. Called from
/// DeleteSingleSrcPhrase(), the latter having a bool parameter,
/// bDoPartnerPileDeletionAlso, which defaults to TRUE, and when FALSE is passed in this
/// function will not be called. (E.g. when the source phrase belongs to a temporary list
/// and so has no partner pile)
/// Note: the deletion will be done for some particular pSrcPhrase in the app's
/// m_pSourcePhrases list, and we want to have at least an approximate idea of the index
/// of which strip the copy of the pile pointer was in, because when we tweak the layout
/// we will want to know which strips to concentrate our efforts on. Therefore before we
/// do the deletion, we work out which strip the pile belongs to, mark it as invalid, and
/// store its index in CLayout::m_invalidStripArry. Later, our strip tweaking code will
/// use this information to make a speedy tweak of the layout before drawing is done (but
/// the information is not used whenever RecalcLayout() does a full rebuild of the
/// document's strips)
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::DeletePartnerPile(CSourcePhrase* pSrcPhrase)
{
	// refactored 4May09
	CLayout* pLayout = GetLayout();
	PileList* pPiles = pLayout->GetPileList();
	if (pPiles->IsEmpty())
		return;
	PileList::Node* posPile = pPiles->GetFirst();
	wxASSERT(posPile!= NULL);
	CPile* pPile = NULL;
	while (posPile != NULL)
	{
		pPile = posPile->GetData();
		if (pSrcPhrase == pPile->GetSrcPhrase())
		{
			// we have found the partner pile, so break out
			break;
		}
		posPile = posPile->GetNext(); // go to next Node*
	} // end of while loop with test: posPile != NULL
	if (posPile == NULL)
	{
		return; // we didn't find a partner pile, so just return;
	}
	else
	{
		// found the partner pile in CLayout::m_pileList, so delete it...
 		
        // get the CPile* instance currently at index, from it we can determine which strip
        // the deletion will take place from (even if we get this a bit wrong, it won't
        // matter)
		pPile = posPile->GetData();
		wxASSERT(pPile != NULL);
		MarkStripInvalid(pPile); // sets CStrip::m_bValid to FALSE, and adds the 
								 // strip index to CLayout::m_invalidStripArray

		// now go ahead and get rid ot the partner pile for the passed in pSrcPhrase
		pPile->SetStrip(NULL);

		// if destroying the CPile instance pointed at by app's m_pActivePile, set the
		// latter to NULL as well (otherwise, in the debugger it would have the value
		// 0xfeeefeee which is useless for pile ptr != NULL tests as it gives a spurious
		// positive result
		if (pLayout->m_pApp->m_nActiveSequNum != -1 && pLayout->m_pApp->m_pActivePile == pPile)
		{
			pLayout->m_pApp->m_pActivePile = NULL;
		}
		pLayout->DestroyPile(pPile,pLayout->GetPileList()); // destroy the pile, & remove 
					// its node from m_pileList, because bool param, bRemoveFromListToo, is
					// default TRUE
		pPile = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return     nothing
/// \param		pSrcPhrase -> the source phrase that was created and inserted in the 
///                           application's m_pSourcePhrases list
/// \remarks
/// Created 13Mar09 for layout refactoring. The m_pileList's CPile instances point to
/// CSourcePhrase instances and creating a new CSourcePhrase for the doc's m_pSourcePhrases
/// list always needs to have the CPile instance which points to it also created,
/// initialized and inserted into the corresponding place in the m_pileList. That task is
/// done here.
/// 
/// Called from various places. It is not made a part of the CSourcePhrase creation process
/// for a good reason. Quite often CSourcePhrase instances are created and are only
/// temporary - such as those deep copied to be saved in various local lists during the
/// vertical edit process, and in quite a few other contexts as well. Also, when documents
/// are loaded from disk and very many CSourcePhrase instances are created in that process,
/// it is more efficient to not create CPile instances as part of that process, but rather
/// to create them all in a loop after the document has been loaded. For example, the
/// current code creates new CSourcePhrases on the heap in about 30 places in the app's
/// code, but only about 25% of those instances require a CPile partner created for the
/// CLayout::m_pileList; therefore we call CreatePartnerPile() only when needed, and it
/// should be called immediately after a newly created CSourcePhrase has just been inserted
/// into the app's m_pSourcePhrases list - so that the so that the strip where the changes
/// happened can be marked as "invalid".
/// Note: take care when CSourcePhrase(s) are appended to the end of the m_pSourcePhrases
/// list, because Creating the partner piles cannot handle discontinuities in the sequence
/// of piles in PileList. So, iterate from left to right over the new pSrcPhrase at the
/// list end, so that each CreatePartnerPile call is creating the CPile instance which is
/// next to be appended to PileList. We test for non-compliance with this rule and abort
/// the application if it happens, because to continue would inevitably lead to an app crash.
/// BEW 20Jan11, replacing partner piles is a problem in some circumstances, because the
/// pile count could change larger and so some new piles can't then be assigned to a
/// strip. This circumstance needs create_strips_keep_piles to be used for the
/// layout_selector enum value in RecalcLayout() in order to get the strips and piles in
/// sync; this comment is for information only, no code was changed below, except the
/// addition of AddUniqueInt() (an unrelated issue, the latter is to prevent duplicate
/// strip indices being stored in the m_invalidStripArray of CLayout)
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::CreatePartnerPile(CSourcePhrase* pSrcPhrase)
{
	// refactor 13Mar09
	CLayout* pLayout = GetLayout();
	int index = IndexOf(pSrcPhrase); // the index in m_pSourcePhrases for the passed
									 // in pSrcPhrase

	PileList::Node* aPosition = NULL;
	CPile* aPilePtr = NULL;
	wxASSERT(index != wxNOT_FOUND); // it must return a valid index!
	PileList* pPiles = pLayout->GetPileList();

	// if the pSrcPhrase is one added to the end of the document, there won't be any
	// existing pile pointers in the PileList with indices that large, so check for this
	// because an Item(index) call with an index out of range will crash the app, it
	// doesn't return a NULL which is what I erroneously though would happen
	PileList::Node* posPile = NULL;
	int lastPileIndex = pLayout->GetPileList()->GetCount() - 1;
	bool bAppending = FALSE;
	if (index > lastPileIndex + 1)
	{
		// we've skipped a pile somehow, this is a fatal error, tell developer and abort
		wxMessageBox(_T(
"Ouch! CreatePartnerPile() has skipped a pSrcPhrase added to doc end, or they creations are not being done in left to right sequence. Must abort now."),
		_T(""), wxICON_ERROR);
		wxASSERT(FALSE);
		wxExit();
	}
	else if (index == lastPileIndex + 1)
	{
		// we are creating the CPile instance which is due to be appended next tp PileList
		bAppending = TRUE;
	}
	else
	{
		// if control gets here with bAppending still FALSE, then an insertion is required
		posPile = pPiles->Item(index);
	}
	CPile* pNewPile = pLayout->CreatePile(pSrcPhrase); // creates a detached CPile
					// instance, initializes it, sets m_nWidth and m_nMinWidth etc
	if (!bAppending)
	{
        // we are inserting a new partner pile's pointer in the CLayout::m_pileList does
        // not get it also inserted in the CLayout::m_stripArray, and so the laid out
        // strips don't know of it. However, we can work out which strip it would be
        // inserted within, or thereabouts, and mark that strip as invalid and put its
        // index into CLayout::m_invalidStripArray. The inventory of invalid strips does
        // not have to be 100% reliable - they are approximate indicators where the layout
        // needs to be tweaked, which is all we need for the RecalcLayout() call later on.
        // Therefore, use the current pPile pointer in the m_pileList at index, and find
        // which strip that one is in, even though it is not the newly created CPile
		aPosition = pPiles->Item(index);
		aPilePtr = aPosition->GetData();
		if (aPilePtr != NULL)
			MarkStripInvalid(aPilePtr); // use aPilePtr to have a good shot at which strip
                // will receive the newly created pile, and mark it as invalid, and save
                // its index in CLayout::m_invalidStripArray; nothing is done if
                // aPilePtr->m_pOwningStrip is NULL

        // the indexed location is within the unaugmented CLayout::m_pileList; therefore an
        // insert operation is required; the index posPile value determined by index is the
        // place where the insertion must be done
		posPile = pPiles->Insert(posPile, pNewPile);
	}
	else
	{
		// appending, (see comment in block above for more details), so just mark the last
		// strip in m_stripArray as invalid, don't call MarkStripInvalid()
		aPosition = pPiles->GetLast(); // the one after which we will append the new one
		aPilePtr = aPosition->GetData();
		if (aPilePtr != NULL)
		{
			// do this manually... just mark the last strip as the invalid one, etc
			CStrip* pLastStrip = (CStrip*)pLayout->GetStripArray()->Last();
			pLastStrip->SetValidityFlag(FALSE); // makes m_bValid be FALSE
			int nStripIndex = pLastStrip->GetStripIndex();
			// BEW 20Jan11, changed to only add unique index values to the array
			AddUniqueInt(pLayout->GetInvalidStripArray(), nStripIndex); // this array makes
									// it easy to quickly compute which strips are invalid
		}
		// now do the append
		posPile = pPiles->Append(pNewPile); // do this only after aPilePtr is calculated
	}
}

// return the index in m_pSourcePhrases for the passed in pSrcPhrase
int CAdapt_ItDoc::IndexOf(CSourcePhrase* pSrcPhrase)
{
	wxASSERT(pSrcPhrase != NULL);
	SPList* pList = GetApp()->m_pSourcePhrases;
	int nIndex = pList->IndexOf(pSrcPhrase);
	return nIndex;
}

void CAdapt_ItDoc::ResetPartnerPileWidth(CSourcePhrase* pSrcPhrase,
										   bool bNoActiveLocationCalculation)
{
	// refactored 13Mar09 & some more on 27Apr09
	int index = IndexOf(pSrcPhrase); // the index in m_pSourcePhrases for the passed in 
									 // pSrcPhrase, in the app's m_pSourcePhrases list
	wxASSERT(index != wxNOT_FOUND); // it must return a valid index!
	PileList* pPiles = GetLayout()->GetPileList();
	PileList::Node* posPile = pPiles->Item(index); // returns NULL if index lies beyond 
												   // the end of m_pileList
	if (posPile != NULL)
	{
		CPile* pPile = posPile->GetData();
		wxASSERT(pPile != NULL);
		pPile->SetMinWidth(); // set m_nMinWidth - it's the maximum extent of the src, 
							  // adapt or gloss text

        // if it is at the active location, then the width needs to be wider -
        // SetPhraseBoxGapWidth() in CPile does that, and sets m_nWidth in the partner pile
        // instance (but if not at the active location, the default value m_nMinWidth will
        // apply)
		if (!bNoActiveLocationCalculation)
		{
            // EXPLANATION FOR THE ABOVE TEST: we need the capacity to have the phrase box
            // be at the active location, but not have the gap width left in the layout be
            // wide enough to accomodate the box - this situation happens in
            // PlacePhraseBox() when we want a box width calculation for the pile which is
            // being left behind (and which at the point when the pile's width is being
            // calculated it is still the active pile, but later in the function when the
            // clicked pile is instituted as the new active location, the gap will have to
            // be based on what is at that new location, not the old one; hence setting the
            // bNoActiveLocationCalculation parameter TRUE just for such a situation
            // prevents the unwanted large gap calculation at the old location
			pPile->SetPhraseBoxGapWidth();
		}

		// mark the strip invalid and put the parent strip's index into 
		// CLayout::m_invalidStripArray if it is not in the array already
		MarkStripInvalid(pPile);
	}
	else
	{
		// if it is null, this is a catastrophic error, and we must terminate the application;
		// or in DEBUG mode, give the developer a chance to look at the call stack
		wxMessageBox(_T(
		"Ouch! ResetPartnerPileWidth() was unable to find the partner pile. Must abort now."),
		_T(""), wxICON_EXCLAMATION);
		wxASSERT(FALSE);
		wxExit();
	}
}

void CAdapt_ItDoc::MarkStripInvalid(CPile* pChangedPile)
{
	CLayout* pLayout = GetLayout();
	// we can mark a strip invalid only provided it exists; so if calling this in
	// RecalcLayout() after the strips were destroyed, and before they are rebuilt, we'd
	// be trying to access freed memory if we went ahead here without a test for strips
	// being in existence - so return if the m_stripArray has no contents currently
	if (pLayout->GetStripArray()->IsEmpty())
		return;
	// pChangedPile has to have a valid m_pOwningStrip (ie. a non-zero value), which may
	// not be the case for a set of newly created piles at the end of the document, so for
	// end-of-document scenarios we will code the caller to just assume the last of the
	// current strips and not call MarkStripInvalid() at all; but just in case one sneaks
	// through, test here and if it has a zero m_pOwningStrip value then exit without
	// doing anything (unfortunately we can't assume it will always be at the doc end)
	if (pChangedPile->GetStrip() == NULL)
	{
		// it's a newly created pile not yet within the current set of strips, so its
		// m_pOwningStrip member returned was NULL, we shouldn't have called this
		// function for this pChangedPile pointer, but since we did, we can only return
		// without doing anything
		return;
	}
    // if control gets to here, there are CStrip pointers stored in m_stripArray, and there
    // is an owning strip defined, so go ahead and mark the owning strip invalid
	wxArrayInt* pInvalidStripArray = pLayout->GetInvalidStripArray();
	CStrip* pStrip = pChangedPile->GetStrip();
	pStrip->SetValidityFlag(FALSE); // makes m_bValid be FALSE
	int nStripIndex = pStrip->GetStripIndex();
	// BEW 20Jan11, changed to only add unique index values to the array
	AddUniqueInt(pInvalidStripArray, nStripIndex); // this array makes it easy to quickly
												   // compute which strips are invalid
}

///////////////////////////////////////////////////////////////////////////////
/// \return nothing
/// \param		pSrcPhrase -> the source phrase to be deleted
/// \param		pOtherList -> another list of source phrases
/// \remarks
/// Called from : the Doc's ReconstituteAfterPunctuationChange().
/// This function is used in document reconstitution after a punctuation change forces a 
/// rebuild. 
/// Clears and deletes any m_pMedialMarkers, m_pMedialPuncts before deleting
/// pSrcPhrase itself.
/// SmartDeleteSingleSrcPhrase deletes only those pSrcPhrase instances in its m_pSavedWords
/// list which are not also in the pOtherList.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::SmartDeleteSingleSrcPhrase(CSourcePhrase* pSrcPhrase, SPList* pOtherList)
{
	// refactored 12Mar09 - nothing was done here because it can be called only in
	// ReconstituteAdfterPunctuationChange() - and only then provided a merger could not be
	// reconstituted -- and so because all partner piles will need to be recreated anyway,
	//  just dealing with a few is pointless; code in that function will handle all instead
	if (pSrcPhrase == NULL)
		return;

	if (pSrcPhrase->m_pMedialMarkers != NULL)
	{
		if (pSrcPhrase->m_pMedialMarkers->GetCount() > 0)
		{
			pSrcPhrase->m_pMedialMarkers->Clear();
		}
		delete pSrcPhrase->m_pMedialMarkers;
	}

	if (pSrcPhrase->m_pMedialPuncts != NULL)
	{
		if (pSrcPhrase->m_pMedialPuncts->GetCount() > 0)
		{
			pSrcPhrase->m_pMedialPuncts->Clear();
		}
		delete pSrcPhrase->m_pMedialPuncts;
	}

	// also delete any saved CSourcePhrase instances forming a phrase (and these
	// will never have medial puctuation nor medial markers nor will they store
	// any saved minimal phrases since they are CSourcePhrase instances for single
	// words only (nor will it point to any CRefString instances) (but these will
	// have SPList instances on heap, so must delete those)
	if (pSrcPhrase->m_pSavedWords != NULL)
	{
		if (pSrcPhrase->m_pSavedWords->GetCount() > 0)
		{
			SPList::Node* pos = pSrcPhrase->m_pSavedWords->GetFirst();
			while (pos != NULL)
			{
				CSourcePhrase* pSP = (CSourcePhrase*)pos->GetData();
				pos = pos->GetNext();
				SPList::Node* pos1 = pOtherList->Find(pSP);
				if (pos1 != NULL)
					continue; // it's in the other list, so don't delete it
				delete pSP->m_pSavedWords;
				delete pSP->m_pMedialMarkers;
				delete pSP->m_pMedialPuncts;
				delete pSP;
			}
		}
		pSrcPhrase->m_pSavedWords->Clear();
		delete pSrcPhrase->m_pSavedWords; // delete the SPList* too
	}
	delete pSrcPhrase;
}

/* deprecated 8Mar11
// /////////////////////////////////////////////////////////////////////////////
// \return nothing
// \param		pSrcPhrase -> the source phrase to be deleted
// \param		pOtherList -> another list of source phrases
// \remarks
// Called from: the Doc's ReconstituteAfterPunctuationChange().
// This function calls DeleteSingleSrcPhrase() but only if pSrcPhrase is not found in
// pOtherList. ConditionallyDeleteSrcPhrase() deletes pSrcPhrase conditionally - the
// condition is whether or not its pointer is present in the pOtherList. If it occurs in
// that list, it is not deleted (because the other list has to continue to manage it), but
// if not in that list, it gets deleted. (Don't confuse this function with
// SmartDeleteSingleSrcPhrase() which also similarly deletes dependent on not being
// present in pOtherList - in that function, it is not the passed in sourcephrase which is
// being considered, but only each of the sourcephrase instances in its m_pSavedWords
// member's sublist).
// /////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::ConditionallyDeleteSrcPhrase(CSourcePhrase* pSrcPhrase, SPList* pOtherList)
{
	SPList::Node* pos = pOtherList->Find(pSrcPhrase);
	if (pos != NULL)
	{
		// pSrcPhrase exists in the list pOtherList, so don't delete it
		return;
	}
	else
	{
		// pSrcPhrase is absent from pOtherList, so it can safely be deleted
		DeleteSingleSrcPhrase(pSrcPhrase);
	}
}
*/

// transfer info about punctuation which got changed, but which doesn't get transferred to
// the original pSrcPhrase (here, pDestSrcPhrase) from the new one resulting from the
// tokenization, for a conjoined pair (the new one is pFromSrcPhrase), unless we do it herein
void CAdapt_ItDoc::TransferFixedSpaceInfo(CSourcePhrase* pDestSrcPhrase, CSourcePhrase* pFromSrcPhrase)
{
	CSourcePhrase* pDestWord1SPh = NULL;
	CSourcePhrase* pDestWord2SPh = NULL;
	CSourcePhrase* pFromWord1SPh = NULL;
	CSourcePhrase* pFromWord2SPh = NULL;
	SPList::Node* destPos = pDestSrcPhrase->m_pSavedWords->GetFirst();
	pDestWord1SPh = destPos->GetData();
	destPos = pDestSrcPhrase->m_pSavedWords->GetLast();
	pDestWord2SPh = destPos->GetData();
	SPList::Node* fromPos = pFromSrcPhrase->m_pSavedWords->GetFirst();
	pFromWord1SPh = fromPos->GetData();
	fromPos = pFromSrcPhrase->m_pSavedWords->GetLast();
	pFromWord2SPh = fromPos->GetData();
	pDestWord1SPh->m_precPunct = pFromWord1SPh->m_precPunct; // copy any m_precPunct to list's first one
	pDestSrcPhrase->m_precPunct = pFromWord1SPh->m_precPunct; // put it also at top level
	pDestWord2SPh->m_follPunct = pFromWord2SPh->m_follPunct; // copy any 2nd word's m_follPunct to list's second one
	pDestSrcPhrase->m_follPunct = pFromWord2SPh->m_follPunct; // put it also at top level
	// handle transfer of m_follOuterPunct data
	pDestWord2SPh->SetFollowingOuterPunct(pFromWord2SPh->GetFollowingOuterPunct());
	pDestSrcPhrase->SetFollowingOuterPunct(pFromWord2SPh->GetFollowingOuterPunct()); // put it also at top level
	// the "outer" ones are handled, the "inner ones" are next - these are stored only on
	// the instances in each top level's m_pSavedWords list, and so we don't also copy
	// them to the top level
	pDestWord1SPh->m_follPunct = pFromWord1SPh->m_follPunct;
	// we do the next line, be it should always just be copying empty string to empty string
	pDestWord1SPh->SetFollowingOuterPunct(pFromWord1SPh->GetFollowingOuterPunct());
	pDestWord2SPh->m_precPunct = pFromWord2SPh->m_precPunct;

	// that handles the punctuation, now we must copy across any stored inline markers; in
	// a fixedspace conjoining, we store these only at the m_pSavedWords level, not at the
	// top level
	// First, do the inline ones for the first CSourcePhrase instance in m_pSavedWords
	pDestWord1SPh->SetInlineBindingEndMarkers(pFromWord1SPh->GetInlineBindingEndMarkers());
	pDestWord1SPh->SetInlineBindingMarkers(pFromWord1SPh->GetInlineBindingMarkers());
	pDestWord1SPh->SetInlineNonbindingMarkers(pFromWord1SPh->GetInlineNonbindingMarkers());
	pDestWord1SPh->SetInlineNonbindingEndMarkers(pFromWord1SPh->GetInlineNonbindingEndMarkers());
	// Second, do the inline ones for the second CSourcePhrase instance in m_pSavedWords
	pDestWord2SPh->SetInlineBindingEndMarkers(pFromWord2SPh->GetInlineBindingEndMarkers());
	pDestWord2SPh->SetInlineBindingMarkers(pFromWord2SPh->GetInlineBindingMarkers());
	pDestWord2SPh->SetInlineNonbindingMarkers(pFromWord2SPh->GetInlineNonbindingMarkers());
	pDestWord2SPh->SetInlineNonbindingEndMarkers(pFromWord2SPh->GetInlineNonbindingEndMarkers());

	// the caller will have already transferred data for m_markers and m_endMarkers at the
	// top level; here we have to copy the first to pDestWord1SPh, and the second to pDestWord2SPh
	pDestWord1SPh->m_markers = pDestSrcPhrase->m_markers;
	pDestWord2SPh->SetEndMarkers(pDestSrcPhrase->GetEndMarkers());

	// the lower level m_key members will need to be updated too, pFromSrcPhrase's lower
	// level instances have the new values which need to be transferred to the lower level
	// instances in pDestSrcPhrase
	pDestWord1SPh->m_key = pFromWord1SPh->m_key;
	pDestWord2SPh->m_key = pFromWord2SPh->m_key;
	// that should do it!
}

///////////////////////////////////////////////////////////////////////////////
/// \return     TRUE to indicate to the caller all is OK (the pSrcPhrase was updated),
///             otherwise return FALSE to indicate to the caller that fixesStr must have
///             a reference added, and the new CSourcePhrase instances must be abandoned
///             and the passed in pSrcPhrase retained unchanged
/// \param		pView		-> pointer to the View
/// \param		pList		-> pointer to m_pSourcePhrases (its use herein is deprecated)
/// \param		pos			-> the iterator position locating the passed in pSrcPhrase 
///                            pointer (its use herein is deprecated)
/// \param		pSrcPhrase	<- pointer of the source phrase
/// \param		fixesStr	-> (its use herein is deprecated, the caller adds to it if
///                            FALSE is returned)
/// \param		pNewList	<- the parsed new source phrase instances
/// \param		bIsOwned	-> specifies whether or not the pSrcPhrase passed in is one which is 
///                            owned by another sourcephrase instance or not (ie. TRUE
///                            means that it is one of the originals stored in an owning
///                            CSourcePhrase's m_pSavedWords list member, FALSE means it
///                            is not owned by another and so is a candidate for
///                            adaptation/glossing and entry of its data in the KB;
///                            owned ones cannot be stored in the KB - at least not
///                            while they continue as owned ones)
/// \remarks
/// Called from: the Doc's ReconstituteAfterPunctuationChange()
/// Handles one pSrcPhrase and ignores the m_pSavedWords list, since that is handled within
/// the ReconstituteAfterPunctuationChange() function for the owning srcPhrase with
/// m_nSrcWords > 1. For the return value, see ReconstituteAfterPunctuationChange()'s
/// return value - same deal applies here.
/// BEW 11Oct10 (actually 21Jan11) modified to use FromSingleMakeSstr(), and to use the
/// TokenizeText() parser - but using target text and punctuation in order to obtain the
/// punctuation-less target text
/// BEW 8Mar11, changed so as not to try inserting new CSourcePhrase instances if the
/// pSrcPhrase passed in results in numElements > 1 in the TokenizeTextString() call
/// below, but just to leave pSrcPhrase unchanged in that case, abandon the new instances,
/// and return FALSE to have the caller put an appropriate entry in fixesStr
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::ReconstituteOneAfterPunctuationChange(CAdapt_ItView* pView, 
		SPList*& WXUNUSED(pList), SPList::Node* WXUNUSED(pos), CSourcePhrase*& pSrcPhrase, 
		wxString& WXUNUSED(fixesStr), SPList*& pNewList, bool bIsOwned)
{
	// BEW added 5Apr05
	bool bHasTargetContent = TRUE; // assume it has an adaptation, clear below if not true
	bool bPlaceholder = FALSE; // default
	bool bNotInKB = FALSE; // default
	bool bRetranslation = FALSE; // default
	if (pSrcPhrase->m_bNullSourcePhrase) bPlaceholder = TRUE;
	if (pSrcPhrase->m_bRetranslation) bRetranslation = TRUE;
	if (pSrcPhrase->m_bNotInKB) bNotInKB = TRUE;

	wxString srcPhrase; // copy of m_srcPhrase member
	wxString targetStr; // copy of m_targetStr member
	wxString key; // copy of m_key member
	wxString adaption; // copy of m_adaption member
	wxString gloss; // copy of m_gloss member
	key.Empty(); adaption.Empty(); gloss.Empty();

    // setup the srcPhrase, targetStr and gloss strings - we must handle glosses too
    // regardless of the current mode (whether adapting or not)
	int numElements = 1; // default
	wxString mMarkersStr;
	wxString xrefStr;
	wxString filteredInfoStr;
	// BEW 21Jan11, changed to reconstruct srcPhrase using FromSingleMakeSstr() - which
	// will generate any binding markers, non-binding inline markers, and punctuation from
	// what is stored in pSrcPhrase, in their correct places
	bool bAttachFilteredInfo = TRUE;
	bool bAttach_m_markers = TRUE;
	bool bDoCount = FALSE;
	bool bCountInTargetText = FALSE;

//#ifdef __WXDEBUG__
	//int halt_here = 0;
	//if (pSrcPhrase->m_nSequNumber >= 1921) // was 397 when [Apos was in the \x ... \x* xref
	//if (pSrcPhrase->m_nSequNumber >= 397) // was 1921 when debugging for the heap corruption bug
	//{
	//	halt_here = 1;
	//}
	//wxLogDebug(_T("  ReconsistuteOneAfterPunctuationChange: 5076  pSrcPhrase sn = %d  m_srcPhrase = %s"),
	//				pSrcPhrase->m_nSequNumber, pSrcPhrase->m_srcPhrase.c_str());
//#endif

	srcPhrase = FromSingleMakeSstr(pSrcPhrase, bAttachFilteredInfo, bAttach_m_markers, 
					mMarkersStr, xrefStr, filteredInfoStr, bDoCount, bCountInTargetText);	
	gloss = pSrcPhrase->m_gloss; // we don't care if glosses have punctuation or not
	if (pSrcPhrase->m_adaption.IsEmpty())
	{
		bHasTargetContent = FALSE;	// use to suppress copying of source punctuation 
									// to an adaptation not yet existing
	}
	else
	{
		targetStr = pSrcPhrase->m_targetStr; // likewise, has punctuation, if any
	}

    // handle placeholders - these have elipsis ... as their m_key and m_srcPhrase
    // members, and so there is no possibility of punctuation changes having any effect
    // on these except possibly for the m_adaption member. Placeholders can occur
    // independently, or as part of a retranslation - the same treatment can be given
    // to instances occurring in either environment.
	SPList::Node* fpos;
	fpos = NULL;
	CSourcePhrase* pNewSrcPhrase;
	SPList::Node* newpos;
	if (bPlaceholder)
	{
		// the adaptation, and gloss if any, is already presumably what the user wants, 
		// so we'll just remove punctuation from the adaptation, and set the relevant members
		// (m_targetStr is already correct) - but only provided there is an existing adaptation
		pSrcPhrase->m_gloss = gloss;
		// remove any initial or final spaces before using it
		targetStr.Trim(TRUE); // trim right end
		targetStr.Trim(FALSE); // trim left end
		if (bHasTargetContent)
		{
			adaption = targetStr;
			// Note: RemovePunctuation() calls ParseWord() in order to give consistent
			// punctuation stripping with parsing text, src or tgt, in the rest of the app
			pView->RemovePunctuation(this, &adaption, from_target_text);
			pSrcPhrase->m_adaption = adaption;

			// update the KBs (both glossing and adapting KBs) provided it is 
			// appropriate to do so
			if (!bPlaceholder && !bRetranslation && !bNotInKB && !bIsOwned)
				pView->StoreKBEntryForRebuild(pSrcPhrase, adaption, gloss);
		}
		return TRUE;
	}

    // BEW 8Jul05: a pSrcPhrase with empty m_srcPhrase and empty m_key can't produce
    // anything when passed to TokenizeTextString, and so to prevent numElements being
    // set to zero we must here detect any such sourcephrases and just return TRUE -
    // for these punctuation changes can produce no effect
	if (pSrcPhrase->m_srcPhrase.IsEmpty() && pSrcPhrase->m_key.IsEmpty())
		return TRUE; // causes the caller to use pSrcPhase 'as is'

    // reparse the srcPhrase string - if we get a single CSourcePhrase as the result,
    // we have a simple job to complete the rebuild for this passed in pSrcPhrase; if
	// we get more than one, we'll have to do something smarter... actually, it's too
	// complex to be smart, so we'll rely on visual editing after the fact to fix problems
	// that may have arisen
	srcPhrase.Trim(TRUE); // trim right end
	srcPhrase.Trim(FALSE); // trim left end
	numElements = pView->TokenizeTextString(pNewList, srcPhrase,  pSrcPhrase->m_nSequNumber);
//#ifdef __WXDEBUG__
	//if (halt_here == 1)
	//{
	//	wxLogDebug(_T("  ReconsistuteOneAfterPunctuationChange: 5145  numElements = %d "),numElements);
	//}
//#endif
	wxASSERT(numElements >= 1);
	pNewSrcPhrase = NULL;
	newpos = NULL;
	if (numElements == 1)
	{
        // simple case - we can complete the rebuild in this block; note, the passed in
        // pSrcPhrase might be storing quite complex data - such as filtered material,
        // chapter & verse information and so forth, so we have to copy everything
        // across as well as update the source and target string members and
        // punctuation. The simplest direction for this copy is to copy from the parsed
        // new source phrase instance back to the original, since only m_key,
        // m_adaption, m_targetStr, precPunct and follPunct can possibly be different
		// in the new parse; it's unlikely m_srcPhrase will have changed, but just in case
		// I'll copy that too
		fpos = pNewList->GetFirst();
		pNewSrcPhrase = fpos->GetData();

		// BEW changed 10Mar11, so as to not copy anything other than the things affected,
		// as noted in the comment above. I'll leave the old code, which copied everything
		// redundantly, just in case I later change my mind.
		/* deprecated 10Mar11
		// first, copy over the flags
		CopyFlags(pNewSrcPhrase,pSrcPhrase); // copies boolean flags from param2 to param1

		// next, info not handled by ParseWord()
		pSrcPhrase->m_curTextType = pNewSrcPhrase->m_curTextType;
		pSrcPhrase->m_inform = pNewSrcPhrase->m_inform;
		pSrcPhrase->m_chapterVerse = pNewSrcPhrase->m_chapterVerse;
		pSrcPhrase->m_markers = pNewSrcPhrase->m_markers;
		pSrcPhrase->m_nSequNumber = pNewSrcPhrase->m_nSequNumber;
		*/
		// next the text info and m_markers member
		pSrcPhrase->m_srcPhrase = pNewSrcPhrase->m_srcPhrase;
		pSrcPhrase->m_key = pNewSrcPhrase->m_key;
		pSrcPhrase->m_precPunct = pNewSrcPhrase->m_precPunct;
		pSrcPhrase->m_follPunct = pNewSrcPhrase->m_follPunct;
		/* deprecated 10Mar11
		pSrcPhrase->m_markers = pNewSrcPhrase->m_markers;
		*/ 
		// finally, the new docV5 members...
		pSrcPhrase->SetFollowingOuterPunct(pNewSrcPhrase->GetFollowingOuterPunct());
		/* deprecated 10Mar11
		pSrcPhrase->SetInlineBindingEndMarkers(pNewSrcPhrase->GetInlineBindingEndMarkers());
		pSrcPhrase->SetInlineBindingMarkers(pNewSrcPhrase->GetInlineBindingMarkers());
		pSrcPhrase->SetInlineNonbindingMarkers(pNewSrcPhrase->GetInlineNonbindingMarkers());
		pSrcPhrase->SetInlineNonbindingEndMarkers(pNewSrcPhrase->GetInlineNonbindingEndMarkers());
		pSrcPhrase->SetEndMarkers(pNewSrcPhrase->GetEndMarkers());
		pSrcPhrase->SetNote(pNewSrcPhrase->GetNote());
		pSrcPhrase->SetFreeTrans(pNewSrcPhrase->GetFreeTrans());
		pSrcPhrase->SetCollectedBackTrans(pNewSrcPhrase->GetCollectedBackTrans());
		pSrcPhrase->SetFilteredInfo(pNewSrcPhrase->GetFilteredInfo());
		*/
		// the adaptation, and gloss if any, is already presumably what the user wants, 
		// so we'll just remove punctuation from the adaptation, and set the relevant members
		// (m_targetStr is already correct) - but only provided there is an existing adaptation
		/* deprecated 10Mar11
		pSrcPhrase->m_gloss = gloss;
		*/
		// remove any initial or final spaces before using it
		targetStr.Trim(TRUE); // trim right end
		targetStr.Trim(FALSE); // trim left end
		pSrcPhrase->m_targetStr = targetStr;
		if (bHasTargetContent)
		{
			adaption = targetStr;
			pView->RemovePunctuation(this,&adaption,from_target_text);
			pSrcPhrase->m_adaption = adaption;

			// update the KBs (both glossing and adapting KBs) provided it is 
			// appropriate to do so
			if (!bPlaceholder && !bRetranslation && !bNotInKB && !bIsOwned)
				pView->StoreKBEntryForRebuild(pSrcPhrase, pSrcPhrase->m_adaption, pSrcPhrase->m_gloss);
		}
		if (IsFixedSpaceSymbolWithin(pSrcPhrase))
		{
			// it's a conjoined pair, so there is more data to be transferred for the
			// instances in m_pSavedWords member (this actually transfers heaps of stuff
			// redundantly, but fixedspace conjoinings are so rare, it's not worth the
			// bother of making the adjustments to eliminate the redundant transfers)
			TransferFixedSpaceInfo(pSrcPhrase, pNewSrcPhrase);
		}
//#ifdef __WXDEBUG__
		//if (halt_here == 1)
		//{
		//	wxLogDebug(_T("  ReconsistuteOneAfterPunctuationChange: 5532  NORMAL situation, return TRUE "));
		//}
//#endif
		return TRUE;
	}
	else
	{
		// BEW 8Mar11, deprecated the code for replacing pSrcPhrase with the two or more
		// tokenized instances resulting from the punctuation change - it's better to just
		// accept pSrcPhrase unchanged, add a ref to fixesStr in the caller, and return
		// FALSE to ensure that happens. (Caller cleans up pNewList, so can just return
		// here without any prior cleanup.)
//#ifdef __WXDEBUG__
		//if (halt_here == 1)
		//{
		//	wxLogDebug(_T("  ReconsistuteOneAfterPunctuationChange: 5247  In the numElements > 1 BLOCK in the loop, before returning FALSE "));
		//}
//#endif
		return FALSE;
	} // end of else block for test: if (numElements == 1)
}

/////////////////////////////////////////////////////////////////////////////////
/// \return		FALSE if the rebuild fails internally at one or more locations so that the 
///				user must later inspect the doc visually and do something corrective at such 
///				locations; TRUE if the rebuild was successful everywhere.
/// \param		pView		-> pointer to the View
/// \param		pList		-> pointer to m_pSourcePhrases
/// \param		fixesStr	-> currently unused
/// \remarks
/// Called from: the Doc's RetokenizeText().
/// Rebuilds the document after a filtering change is indicated. The new document reflects
/// the new filtering changes.
/// BEW added 18May05
/// BEW (?)refactored 18Mar09
/// BEW 20Sep10, old code expected filtered info to only be in m_markers, so for
/// docVersion 5 the code for unfiltering needs to use m_filteredInfo instead; similarly
/// for the code for filtering (that is, store in m_filteredInfo, not m_markers)
/// BEW 22Sep10, finished updated for support of docVersion 5 (significant changes
/// required, and I expanded the comments considerably to make the logic easier to follow,
/// but I didn't remove the goto statements)
/// BEW 11Oct10, radically rewrote the inner loop and got rid of gotos (except one) and
/// used RebuildSourceText() call to construct the filteredStr -- which saves mobs of code
//////////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::ReconstituteAfterFilteringChange(CAdapt_ItView* pView,
													SPList*& pList, wxString& fixesStr)
{
	// Filtering has changed
	bool bSuccessful = TRUE;
	wxString endingMkrsStr; // BEW added 25May05 to handle endmarker sequences like \fq*\f*

	// Recalc the layout in case the view does some painting when the progress 
	// was removed earlier or when the bar is recreated below
	UpdateSequNumbers(0); // get the numbers updated, as a precaution
#ifdef _NEW_LAYOUT
	GetLayout()->RecalcLayout(gpApp->m_pSourcePhrases, keep_strips_keep_piles);
#else
	GetLayout()->RecalcLayout(gpApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
	gpApp->m_pActivePile = pView->GetPile(gpApp->m_nActiveSequNum);
	GetLayout()->PlaceBox();

	// put up a progress indicator
	int nOldTotal = pList->GetCount();
	if (nOldTotal == 0)
	{
		return 0;
	}
	int nOldCount = 0;

	wxString progMsg = _("Pass 1 - File: %s  - %d of %d Total words and phrases");
	wxString msgDisplayed = progMsg.Format(progMsg,gpApp->m_curOutputFilename.c_str(),1,nOldTotal);
    wxProgressDialog progDlg(_("Processing Filtering Change(s)"),
                            msgDisplayed,
                            nOldTotal,    // range
                            GetDocumentWindow(),   // parent
                            //wxPD_CAN_ABORT |
                            //wxPD_CAN_SKIP |
                            wxPD_APP_MODAL |
                            wxPD_AUTO_HIDE //| -- try this as well
                            //wxPD_ELAPSED_TIME |
                            //wxPD_ESTIMATED_TIME |
                            //wxPD_REMAINING_TIME
                            //| wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
                            );

	// BEW added 29Jul09, turn off CLayout Draw() while strips and piles could get
	// inconsistent with each other
	GetLayout()->m_bInhibitDraw = TRUE;

	// Set up a rapid access string for the markers changed to be now unfiltered,
	// and another for the markers now to be filtered. Unfiltering has to be done
	// before filtering is done.
	wxString strMarkersToBeUnfiltered;
	wxString strMarkersToBeFiltered;
	strMarkersToBeUnfiltered.Empty();
	strMarkersToBeFiltered.Empty();
	wxString valStr;
	// the m_FilterStatusMap has only the markers which have their filtering status changed
	MapWholeMkrToFilterStatus::iterator iter;
	for (iter = gpApp->m_FilterStatusMap.begin(); iter != gpApp->m_FilterStatusMap.end(); ++iter)
	{
		if (iter->second == _T("1"))
		{
			strMarkersToBeFiltered += iter->first + _T(' ');
		}
		else
		{
			strMarkersToBeUnfiltered += iter->first + _T(' ');
		}
	}

	// define some useful flags which will govern the code blocks to be entered
	bool bUnfilteringRequired = TRUE;
	bool bFilteringRequired = TRUE;
	// next two can have no markers but a single space, so the IsEmpty() test won't be
	// valid unless any whitespace preceding the markers is removed - which, of course,
	// removes any whitespace which comprises the whole content of either
	strMarkersToBeFiltered.Trim(FALSE); // trim whitespace off left end
	strMarkersToBeUnfiltered.Trim(FALSE); // trim whitespace off left end
	if (strMarkersToBeFiltered.IsEmpty())
		bFilteringRequired = FALSE;
	if (strMarkersToBeUnfiltered.IsEmpty())
		bUnfilteringRequired = FALSE;

	// in the block below we determine which SFM set's map to use, and determine what the full list
	// of filter markers is (the changed ones will be in m_FilterStatusMap); we need the map so we
	// can look up USFMAnalysis struct instances
	MapSfmToUSFMAnalysisStruct* pSfmMap;
	pSfmMap = gpApp->GetCurSfmMap(gpApp->gCurrentSfmSet);

	// reset the appropriate USFMAnalysis structs so that TokenizeText() calls will access
	// the changed filtering settings rather than the old settings (this also updates the app's
	// rapid access strings, by a call to SetupMarkerStrings() done just before returning)
	ResetUSFMFilterStructs(gpApp->gCurrentSfmSet, strMarkersToBeFiltered, strMarkersToBeUnfiltered);
	
	// Initialize for the loops. We must loop through the sourcephrases list twice - the
	// first pass will do all the needed unfilterings, the second pass will do the
	// required new filterings - trying to do these tasks in one pass would be much more
	// complicated and therefore error-prone.
	SPList::Node* pos;
	CSourcePhrase* pSrcPhrase = NULL;
	CSourcePhrase* pLastSrcPhrase = NULL;
	wxString mkr;
	mkr.Empty();
	int nFound = -1;
	int start = 0;
	int end = 0;
	int offset = 0;
	int offsetNextSection = 0;
	wxString preStr;
	wxString remainderStr;
	SPList* pSublist = new SPList;
	// BEW 20Sep10, for docVersion 5, the former tests of m_markers become tests of
	// m_filteredInfo, and restorage  to preStr uses content only from that

	// do the unfiltering pass through m_pSourcePhrases
	int curSequNum = -1;
	if (bUnfilteringRequired)
	{
		pos = pList->GetFirst();
		bool bDidSomeUnfiltering;
		while (pos != NULL)
		{
			SPList::Node* oldPos = pos;
			pSrcPhrase = (CSourcePhrase*)pos->GetData(); // just gets the pSrcPhrase

//#ifdef __WXDEBUG__
			//wxString theWord;
			//wxLogDebug(_T("* Unfiltering *: At sequNum = %d  m_srcPhrase =  %s"),pSrcPhrase->m_nSequNumber,pSrcPhrase->m_srcPhrase);
//#endif

			pos = pos->GetNext(); // moves the pointer/iterator to the next node
			curSequNum = pSrcPhrase->m_nSequNumber;
			bDidSomeUnfiltering = FALSE;
			SPList::Node* prevPos = oldPos;
			pLastSrcPhrase = prevPos->GetData(); // abandon; this one is the pSrcPhrase one
			prevPos = prevPos->GetPrevious();
			if (prevPos != NULL)
			{
				// the actual "previous" one
				pLastSrcPhrase = prevPos->GetData();
				prevPos = prevPos->GetPrevious();
				wxASSERT(pLastSrcPhrase);
			}
			else
			{
				// we were at the start of the document, so there is no previous instance
				pLastSrcPhrase = NULL;
			}

			offset = 0;
			offsetNextSection = 0; // points to next char beyond a moved section or unfiltered section
			preStr.Empty();
			remainderStr.Empty();
			bool bWeUnfilteredSomething = FALSE;
			wxString bareMarker;
			bool bGotEndmarker = FALSE; // whm initialized
			bool bHasEndMarker = FALSE; // whm initialized

			// acts on ONE instance of pSrcPhrase only each time it loops, but in so doing
			// it may add many by removing FILTERED status for a series of sourcephrase instances
			// (all such will be in m_filteredInfo, notes, free trans, and collected back
			// trans are not unfilterable)
			if (!pSrcPhrase->GetFilteredInfo().IsEmpty()) // do nothing when m_filteredInfo is empty
			{
				// m_markers often has an initial space, which is a nuisance, so check and remove
				// it if present (this can remain here, the change, if done, is benign)
				if (pSrcPhrase->m_markers[0] == _T(' '))
					pSrcPhrase->m_markers = pSrcPhrase->m_markers.Mid(1);

				// loop across any filtered substrings in m_filteredInfo, until no more are found
				while ((offset = FindFromPos(pSrcPhrase->GetFilteredInfo(),filterMkr,offset)) != -1)
				{
					// get the next one, its prestring and remainderstring too; on return start
					// will contain the offset to \~FILTER and end will contain the offset to the
					// character immediately following the space following the matching \~FILTER*
					wxString theFilteredInfo = pSrcPhrase->GetFilteredInfo();
					mkr = GetNextFilteredMarker(theFilteredInfo,offset,start,end);
					if (mkr.IsEmpty())
					{
						// there was an error here... post a reference to its location
						// sequence numbers may not be uptodate, so do so first over whole list so that
						// the attempt to find the chapter:verse reference string will not fail
						pView->UpdateSequNumbers(0);
						if (!gbIsUnstructuredData)
						{
							fixesStr += pView->GetChapterAndVerse(pSrcPhrase);
							fixesStr += _T("   ");
						}
						bSuccessful = FALSE; // make sure the caller will show the error box
						break; // exit this inner loop & iterate to the next CSourcePhrase instance
					}

					// get initial material into preStr, if any
					if (offset > 0)
					{
                        // get the marker information which is not to-be-unfiltered
                        // filtered material - this stuff will be transfered to the
                        // m_filteredInfo member of the first CSourcePhrase instance in the
                        // first unfiltered string we extract -- and we may append to it in
                        // blocks below; but if we dont find a marker to unfilter, the
                        // stuff put into preStr will be abandoned because m_filteredInfo
                        // on the sourcephrase will not have been altered
						preStr += pSrcPhrase->GetFilteredInfo().Mid(offsetNextSection,offset - offsetNextSection);
						offsetNextSection = offset; // update, since this section has been (potentially) moved
					}

                    // Unknown markers will ALL be possible candidates for filtering and
                    // unfiltering, and so we'd have to determine whether or not this
                    // marker is in strMarkersToBeUnfiltered - and if it is then we'd
                    // unfilter it, even though pSfm would be NULL when we try to create
                    // the pointer.

                    // determine whether or not the marker we found is one of those
                    // designated for being unfiltered (ie. its content made visible in the
                    // adaptation window)
					wxString mkrPlusSpace = mkr + _T(' '); // prevents spurious Finds, 
														   // such as \h finding \hr, etc
					if (strMarkersToBeUnfiltered.Find(mkrPlusSpace) == -1)
					{
                        // it's not one we need to unfilter, so append it, its content, and
                        // the bracketing \~FILTER and \~FILTER* markers, as is, to preStr
						wxASSERT(offset == start);
						wxASSERT(end > start);
						//wxString appendStr = pSrcPhrase->m_markers.Mid(offset, end - start);
						wxString appendStr = pSrcPhrase->GetFilteredInfo().Mid(offset, end - start);
						preStr += appendStr;
						offsetNextSection = end; // update, this section is deemed moved
					}
					else
					{
                        // we have found a marker with content needing to be unfiltered, so
                        // do so (note: the unfiltering operation will consume some of the
                        // early material, or perhaps all the material, in
                        // pSrcPhase->m_filteredInfo; so we will shorten the latter before we
                        // iterate after having done the unfilter for this marker
						bDidSomeUnfiltering = TRUE; // used for updating navText on original pSrcPhrase when done
						bWeUnfilteredSomething = TRUE; // used for reseting initial conditions in inner loop
						pSublist->Clear(); // clear list in preparation for Tokenizing
						wxASSERT(offset == start);
						wxASSERT(end > start);
						wxString extractedStr = pSrcPhrase->GetFilteredInfo().Mid(offset, end - start);

#ifdef _Trace_RebuildDoc
						//TRACE2("UNFILTERING: gCurrentSfmSet %d STRING: %s\n", gpApp->gCurrentSfmSet, extractedStr);
#endif

						extractedStr = RemoveAnyFilterBracketsFromString(extractedStr); // we'll tokenize this
                        // note, extractedStr will lack a trailing space if there is a
                        // contentless filtered marker in the passed in string, so we need
                        // to test & if needed adjust m_filteredInfo below
						remainderStr = pSrcPhrase->GetFilteredInfo().Mid(end); // remainder, from end of \~FILTER*
						if (remainderStr[0] == _T(' '))
							remainderStr = remainderStr.Mid(1); // remove an initial space if there is one
						offsetNextSection = end; // update, this section is to be 
												 // unfiltered (on this pass at least)
						// tokenize the substring (using this we get its inline marker handling for free)
						wxASSERT(extractedStr[0] == gSFescapechar);
						int count = pView->TokenizeTextString(pSublist,extractedStr,pSrcPhrase->m_nSequNumber);
						bool bIsContentlessMarker = FALSE;
						
						USFMAnalysis* pSfm = NULL;	// whm moved here from below to insure it is
													// initialized before call to AnalyseMarker

						// 20Sep10 -- this comment needs qualifying -- see below it...
						// if the unfiltered section ended with an endmarker, then the
                        // TokenizeText() parsing algorithm will create a final
                        // CSourcePhrase instance with m_key empty, but the endmarker
                        // stored in its m_markers member. This last sourcephrase is
                        // therefore spurious, and its stored endmarker really belongs in
                        // the original pSrcPhrase phrase's m_markers member at its
                        // beginning - so check for this and do the transfer if needed,
                        // then delete the spurious sourcephrase instance before
                        // continuing; but if there was no endmarker, then there will be no
                        // spurious sourcephrase instances created so no delete would be
                        // done -- but that is only true if the marker being unfiltered has
                        // text content following; so when unfiltering an unknown filtered
                        // contentless marker, we'll also get a spurious sourcephrase
                        // produced (ie. m_key and m_srcPhrase are empty, but m_markers has
                        // the unknown marker; so we have to take this special case into
                        // consideration too - we don't want to end up inserting this
                        // spurious sourcephrase into the doc's list.
                        // 
                        // BEW 20Sep10 note on the above comment; docVersion 5 tokenization
                        // stores an endmarker now on the CSourcePhrase which ends the
                        // content, not on the next in line, so the check is no longer
                        // needed; but the comment about the empty unknown marker remains
                        // valid
						bGotEndmarker = FALSE; // if it remains false, the endmarker was 
											   // omitted in the source
						bool bHaventAClueWhatItIs = FALSE; // if it's really something
														   // unexpected, set this TRUE below
						int curPos = -1;
						if (!pSublist->IsEmpty())
						{
                           // Note: wxList::GetLast() returns a node, not a pointer to a
                            // data item, so we do the GetLast operation in two parts
							SPList::Node* lastPos = pSublist->GetLast();
							CSourcePhrase* pTailSrcPhrase = lastPos->GetData();
							if (pTailSrcPhrase)
							{
								if (pTailSrcPhrase->m_key.IsEmpty() && !pTailSrcPhrase->m_markers.IsEmpty())
								{
									// m_markers should end with a space, so check and add one if necessary
									// transfer of an endmarker is required, or it may not be an endmarker
									// but a contentless marker - use a bool to track which it is
									// BEW 20Sep10, it won't ever now be an endmarker, but
									// it could be an unknown marker (see comments above)
									wxString endmarkersStr = pTailSrcPhrase->m_markers;
									curPos = endmarkersStr.Find(_T('*'));
									if (curPos == wxNOT_FOUND)
									{
										// it's not an endmarker, but a contentless (probably unknown) marker
										bIsContentlessMarker = TRUE;
										endingMkrsStr.Empty(); // no endmarkers, so none to later insert
															   // on this iteration
										goto f;
									}

									// it's not contentless, so it must be an endmarker, so ensure the 
									// endmarker is present
									curPos = endmarkersStr.Find(gSFescapechar);
									if (curPos == -1)
									{
                                        // we found no marker at all, don't expect this,
                                        // but do what we must (ie. ignore the rest of the
                                        // block), and set the bool which tells us we are
                                        // stymied (whatever it is, we'll just make it
                                        // appear as adaptable source text further down in
                                        // the code)
										bHaventAClueWhatItIs = TRUE;
										endingMkrsStr.Empty(); // can't preserve what we failed to find
										goto h; // skip all the marker calcs, since it's not a marker
									}
                                    // look for an asterisk after it - if we find one, we
                                    // assume we have an endmarker (but we don't need to
                                    // extract it here because we will do that from the
                                    // USFMAnalysis struct below, and that is safe because
                                    // unknown markers will, we assume, never have a
                                    // matching endmarker -- that is a requirement for
                                    // unknown markers as far as Adapt It is concerned, for
                                    // their safe processing within the app whm note:
                                    // placed .Mid on endmarkerStr since wxString:Find
                                    // doesn't have a start position parameter.
									curPos = FindFromPos(endmarkersStr,_T('*'),curPos + 1);
									if (curPos == wxNOT_FOUND)
									{
                                        // we did not find an asterisk, so ignore the rest
                                        // (we are not likely to ever enter this block)
										endingMkrsStr.Empty();
										goto f;
									}
									else
									{
										bGotEndmarker = TRUE;

                                        // BEW added 26May06; remember the string, because
                                        // it might not just be the matching endmarker we
                                        // are looking for, but that preceded by an
                                        // embedded endmarker (eg. \fq* preceding a \f*),
                                        // and so we'll later want to copy this string
                                        // verbatim into the start of the m_markers member
                                        // of the pSrcPhrase which is current in the outer
                                        // loop, so that the unfiltered section is
                                        // terminated correctly as well as any nested
                                        // markers being terminated correctly, if in fact
                                        // the last of any such have the endmarker (for
                                        // example, \fq ... \fq* can be nested within a \f
                                        // ... \f* footnote stretch, but while \fq has an
                                        // endmarker it is not obligatory to use it, and so
                                        // we have to reliably handle both \fq*\f* or \f*
                                        // as termination for nesting and the whole
                                        // footnote, as both of these are legal markup
                                        // situations)
										endingMkrsStr = endmarkersStr;
									}

                                    // remove the spurious sourcephrase when we've got an
                                    // endmarker identified (but for the other two cases
                                    // where we skip this bit of code, we'll do further
                                    // things with the spurious sourcephrase below) wxList
                                    // has no direct equivalent to RemoveTail(), but we can
                                    // point an iterator to the last element and erase it
									SPList::Node* lastOne;
									lastOne = pSublist->GetLast();
									pSublist->Erase(lastOne);

									// delete the memory block to prevent a leak, update the count
									DeleteSingleSrcPhrase(pTailSrcPhrase,FALSE); // don't delete a 
																	// non-existent partner pile
									count--;
								} // end of block for detecting the parsing of an endmarker
							} // end of TRUE block for test: if (pTailSrcPhrase)
						}

						// make the marker accessible, minus its backslash
f:						bareMarker = mkr.Mid(1); // remove backslash

						// determine if there is an endmarker
						bHasEndMarker = FALSE;
						extractedStr = MakeReverse(extractedStr);
						curPos = extractedStr.Find(_T('*')); // remember, extractedStr is reversed!!
						// determine if the extracted string has an endmarker at its end
						if (bIsContentlessMarker)
						{
							bHasEndMarker = FALSE;
						}
						else
						{
							if (curPos != wxNOT_FOUND)
							{
                                // there is an asterisk, but it may be in the text rather
                                // than an endmarker, so check it is part of a genuine
                                // endmarker (this is a safer test than checking
                                // bareMarker's USFMAnalysis, since some endmarkers can be
                                // optional)
								int nStart = curPos + 1; // point past it
								// find the next backslash
								curPos = FindFromPos(extractedStr,gSFescapechar,nStart);
								wxString possibleEndMkr = extractedStr.Mid(nStart, curPos - nStart);
								possibleEndMkr = MakeReverse(possibleEndMkr); // if an 
												// endmarker, this should equal bareMarker
								bHasEndMarker = bareMarker == possibleEndMkr;
							}
						}
						extractedStr = MakeReverse(extractedStr);// restore normal order

                        // point at the marker's backslash in the buffer (ready for calling
                        // AnalyseMarker) 
                        // whm moved code block that was here down past the
                        // h: label
						{	// this extra block extends for the next 28 lines. It avoids the bogus 
							// warning C4533: initialization of 'f_iter' is skipped by 'goto h'
							// by putting its code within its own scope
						bool bFound = FALSE;
						MapSfmToUSFMAnalysisStruct::iterator f_iter;
						f_iter = pSfmMap->find(bareMarker); // find returns an iterator
						if (f_iter != pSfmMap->end())
							bFound = TRUE; 
						if (bFound)
						{
							pSfm = f_iter->second;
                            // if it was not found, then pSfm will remain NULL, and we know
                            // it must be an unknown marker
						}
						else
						{
							pSfm = (USFMAnalysis*)NULL;
						}

                        // we'll need this value for the first of the parsed sourcephrases
                        // from the extracted text for unfiltering
						bool bWrap;
						if (bFound)
						{
							bWrap = pSfm->wrap;
						}
						else
						{
							bWrap = FALSE;
						}
						} // end of extra block

                        // set the members appropriately, note intial and final require
                        // extra code -- the TokenizeTextString call tokenizes without any
                        // context, and so we can assume that some sourcephrase members are
                        // not set up correctly (eg. m_bSpecialText, and m_curTextType) so
                        // we'll have to use some of TokenizeText's processing code to get
                        // things set up right. (a POSITION pos2 value of zero is
                        // sufficient test for being at the final sourcephrase, after the
                        // GetNext() call has obtained the final one)
h:						bool bIsInitial = TRUE;
						int nWhich = -1;

						int extractedStrLen = extractedStr.Length();
                        // wx version note: Since we require a read-only buffer we use
                        // GetData which just returns a const wxChar* to the data in the
                        // string.
						const wxChar* pChar = extractedStr.GetData();
						wxChar* pBufStart = (wxChar*)pChar;
						wxChar* pEnd;
						pEnd = (wxChar*)pChar + extractedStrLen; // whm added
						wxASSERT(*pEnd == _T('\0'));
						// lookup the marker in the active USFMAnalysis struct map, 
						// get its struct
						int mkrLen = mkr.Length(); // we want the length including 
												   // backslash for AnalyseMarker()
						SPList::Node* pos2 = pSublist->GetFirst();
						CSourcePhrase* pSPprevious = NULL;
						while (pos2 != NULL)
						{
							SPList::Node* savePos;
							savePos = pos2;
							CSourcePhrase* pSP = (CSourcePhrase*)pos2->GetData();
							pos2 = pos2->GetNext();
							wxASSERT(pSP);
							nWhich++; // 0-based value for the iteration number
							if (bIsInitial)
							{
                                // call AnalyseMarker() and set the flags etc correctly,
                                // taking context into account, for this we need the
                                // pLastSrcPhrase pointer - but it is okay if it is NULL
                                // (Note: pSP is still in the temporary list pSublist,
                                // while pLastSrcPhrase is in the m_pSourcePhrases main
                                // list of the document.)
								pSP->m_curTextType = verse; // assume verse unless AnalyseMarker changes it
								pSP->m_bSpecialText = AnalyseMarker(pSP,pLastSrcPhrase,pBufStart,mkrLen,pSfm);

                                // we have to handle the possibility that pSP might have a
                                // contentless marker, or actually something not a marker
                                // somehow in m_markers, so do these below
								if (bIsContentlessMarker)
								{
                                    //  we want this added 'as is' (including its following
                                    //  space) to pSrcPhrase's m_markers member, in the
                                    //  appropriate place and the remainderStr added, and
                                    //  this pSublist element removed (since its
                                    //  contentless, there can only be this one in the
                                    //  sublist), and continue - to effect the needed
                                    //  result we must set up remainderStr to have preStr
                                    //  plus this marker and space plus remainderStr's
                                    //  previous content, in that order
                                    //  
									//  BEW 20Sep10; preStr and remainderStr for
									//  docVersion 5 are now associated with
									//  m_filteredInfo; but the unknown marker - being at
									//  the tail of a set of CSourcePhrases comprising a
									//  single one with no m_key content, does have
									//  to go into the pSrcPhrase's (NOT pSP's)
									//  m_markers member - preceding what is already
									//  there, and we must deal with preStr and
									//  remainderStr here because we break out of the loop
									//  at the end of this block
									pSrcPhrase->m_markers = pSP->m_markers + pSrcPhrase->m_markers;
									// handle the filtered info that remains, if any
									wxString aString;
									if (!preStr.IsEmpty())
										aString = preStr;
									if (!remainderStr.IsEmpty())
										aString += remainderStr;
									if (!aString.IsEmpty())
										pSrcPhrase->SetFilteredInfo(aString);
									else
										pSrcPhrase->SetFilteredInfo(_T(""));
									nWhich = 0;
									DeleteSingleSrcPhrase(pSP,FALSE); // don't leak memory; 
										// & FALSE means don't delete a non-existent partner pile
									pSublist->Clear();
									break;
								}
								if (bHaventAClueWhatItIs)
								{
                                    // when we expected a marker in m_markers but instead
                                    // found text (which we hope would never be the case) -
                                    // but just in case something wrongly got shoved into
                                    // m_markers, we want to make it visible and adaptable
                                    // in in the document - if it's something which
                                    // shouldn't be there, then the user can edit the
                                    // source text manually to remove it. In this case, our
                                    // 'spurious' sourcephrase is going to be treated as
                                    // non-spurious, and we'll move the m_markers content
                                    // to m_srcPhrase, and remove punctuation etc and set
                                    // up m_key, m_precPunct and m_follPunct.
									SPList* pSublist2 = new SPList;
									wxString unexpectedStr = pSP->m_markers;
									int count;
									count = pView->TokenizeTextString(pSublist2,unexpectedStr,
																pSrcPhrase->m_nSequNumber);
                                    // the actual sequence number doesn't matter because we
                                    // renumber the whole list later on after the
                                    // insertions are done
									wxASSERT(count > 0);
									CSourcePhrase* pSP2;
									SPList::Node* posX = pSublist2->GetFirst();
									pSP2 = (CSourcePhrase*)posX->GetData();
									posX = posX->GetNext();
                                    // we'll make an assumption that there is only one
                                    // element in pSublist2, which should be a safe
                                    // assumption, and if not -- well, we'll just add the
                                    // append the extra strings and won't worry about
                                    // punctuation except what's on the first element
									pSP->m_markers.Empty();
									pSP->m_srcPhrase = pSP2->m_srcPhrase;
									pSP->m_key = pSP2->m_key;
									pSP->m_precPunct = pSP2->m_precPunct;
									pSP->m_follPunct = pSP2->m_follPunct;
                                    // that should do it, but if there's more, well just
                                    // add the text so we don't lose anything - user will
                                    // have the option of editing what he sees afterwards
									while (posX != NULL)
									{
										pSP2 = (CSourcePhrase*)posX->GetData();
										posX = posX->GetNext();
										pSP->m_srcPhrase += _T(" ") + pSP2->m_srcPhrase;
										pSP->m_key += _T(" ") + pSP2->m_key;
									}
									// delete all the elements in pSP2, and then delete 
									// the list itself
									DeleteListContentsOnly(pSublist2);
									delete pSublist2;
									bHaventAClueWhatItIs = FALSE;
								}

								// is it PNG SFM or USFM footnote marker?
								// comparing first two chars in mkr
								if (mkr.Left(2) == _T("\\f")) // is it PNG SFM or USFM footnote marker?
								{
									// if not already set, then do it here
									if (!pSP->m_bFootnote)
										pSP->m_bFootnote = TRUE;
								}

								// BEW 20Sep10, the comment below applied to the legacy
								// version, not to docVersion 5, the docVersion 5 comment
								// follows it...
                                // add any m_markers material to be copied from the parent
                                // (this stuff, if any, is in preStr, and it must be added
                                // BEFORE the current m_markers material in order to
                                // preserve correct source text ordering of previously
                                // filtered stuff)
                                // For docVersion 5:
                                // Put any m_filteredInfo material to be copied from the
                                // parent (this stuff, if any, is in preStr, and it must be
                                // put here in order to preserve correct source text
                                // ordering of previously filtered stuff); remainderStr's
                                // material, if any, must go instead into the CSourcePhrase
                                // following the unfiltered material, that is, into the
                                // pSrcPhrase's m_filteredInfo member - we do that below,
                                // outside this loop after it completes
								if (!preStr.IsEmpty())
								{
									pSP->SetFilteredInfo(preStr);
								}

                                // completely redo the navigation text, so it accurately
                                // reflects what is in the m_markers member of the
                                // unfiltered section
#ifdef _Trace_RebuildDoc
								//TRACE1("UNFILTERING: for old navText: %s\n", pSP->m_inform);
#endif
								pSP->m_inform = RedoNavigationText(pSP);

								pSPprevious = pSP; // set pSPprevious for the next iteration, for propagation
								bIsInitial = FALSE;
							} // end of TRUE block for test: if (bIsInitial)

							// when not the 0th iteration, we need to propagate the flags, 
							// texttype, etc
							if (nWhich > 0)
							{
								// do propagation
								wxASSERT(pSPprevious);
								pSP->CopySameTypeParams(*pSPprevious);
							}

                            // for the last pSP instance, there could be an endmarker which
                            // follows it; if that is the case, we can assume the main
                            // list's sourcephrase which will follow this final pSP
                            // instance after we've inserted pSublist into the main list,
                            // will already have its correct TextType and m_bSpecialText
                            // value set, and so we won't try change it (and won't call
                            // AnalyseMarker() again to invoke its endmarker-support code
                            // either) instead we will just set sensible end conditions -
                            // such as m_bBoundary set TRUE, and we'll let the TextType
                            // propagation do its job. We will need to check if we have
                            // just unfiltererd a footnote, and if so, set the
                            // m_bFootnoteEnd flag.
							if (pos2 == NULL || count == 1)
							{
                                // pSP is the final in pSublist, so do what needs to be
                                // done for such an instance; (if there is only one
                                // instance in pSublist, then the first one is also the
                                // last one, so we check for that using the count == 1 test
                                // -- which is redundant really since pos2 should be NULL
                                // in that case too, but no harm is done with the extra
                                // test)
								pSP->m_bBoundary = TRUE;
								// rely on the foonote TextType having been propagated
								if (pSP->m_curTextType == footnote)
									pSP->m_bFootnoteEnd = TRUE; 
							}
						} // end of while loop for pSublist elements

                        // now insert the completed CSourcePhrase instances into the
                        // m_pSourcePhrases list preceding the oldPos POSITION
						pos2 = pSublist->GetFirst();
						while (pos2 != NULL)
						{
							CSourcePhrase* pSP = (CSourcePhrase*)pos2->GetData();
							pos2 = pos2->GetNext();
							wxASSERT(pSP);
							// wxList::Insert() inserts before specified position in the list
							pList->Insert(oldPos,pSP); // m_pSourcePhrases will manage these now
						}

                        // remove the moved and unfiltered part from pSrcPhrase->m_filteredInfo,
                        // so that the next iteration starts with the as-yet-unhandled text
                        // material in that member (this stuff was stored in the local
                        // wxString remainderStr above) - but if there was an unfiltered
                        // endmarker, we'll later also have to put that preceding this
                        // stuff as well
						// BEW 20Sep10, unfiltered endmarkers are handled already in the
						// tokenizing, so we won't later need to handle them
						pSrcPhrase->SetFilteredInfo(remainderStr); // this could be empty, and usually would be
						if (bIsContentlessMarker)
						{
							pSrcPhrase->m_inform = RedoNavigationText(pSrcPhrase);
							bIsContentlessMarker = FALSE;
						}

						pSublist->Clear(); // clear the local list (but leave the memory chunks in RAM)

 //wxLogDebug(_T("Loc doc4603 INNER LOOP ; before SequNum Update: curSequNum %d ,  SN = %d"), curSequNum, gpApp->m_nActiveSequNum);
						
						// update the active sequence number on the view
						// BEW changed 29Jul09, the test needs to be > rather than >=,
						// because otherwise a spurious increment by 1 can happen at the
						// end of the first run through this block
						if (gpApp->m_nActiveSequNum > curSequNum)
						{
                            // adjustment of the value is needed (for unfilterings, the box
                            // location remains a valid one (but not necessarily so when
                            // filtering)
							gpApp->m_nActiveSequNum += count;
						}
					}

					// do the unfiltering adjustments needed when we unfiltered something
					if (bWeUnfilteredSomething)
					{
						bWeUnfilteredSomething = FALSE; // reset for next iteration of inner loop

                        // BEW added 8Mar11, I forgot to remove the unfiltered info from
                        // the saved originals of a merger! If the pSrcPhrase at oldPos is
                        // a merger, then the first of the stored original list of
                        // CSourcePhrase instances will also store in its m_filteredInfo
                        // member the same filtered information - do we must check here,
                        // and if filterMkr is within that instances m_filteredInfo, we
                        // must replace its m_filteredInfo content with remainderStr as set
                        // above
						CSourcePhrase* pSrcPhraseTopLevel = oldPos->GetData();
						// we deliberately check for a non-empty m_pSavedWords list,
						// rather than looking at m_nSrcWords; we want our test to handle
						// fixedspace (~) pseudo-merger conjoining, as well as a real merger
						if (!pSrcPhraseTopLevel->m_pSavedWords->IsEmpty())
						{
							// it's either a merger, or a fixedspace conjoining of two; in
							// either case, any filtered info can only be on the first in
							// the m_pSavedWords list
							SPList::Node* posOriginalsList = pSrcPhraseTopLevel->m_pSavedWords->GetFirst();
							if (posOriginalsList != NULL)
							{
								CSourcePhrase* pFirstOriginal = posOriginalsList->GetData();
								wxASSERT(pFirstOriginal != NULL);
								wxString firstOriginalFilteredInfo = pFirstOriginal->GetFilteredInfo();
								if (!firstOriginalFilteredInfo.IsEmpty())
								{
									int anOffset = firstOriginalFilteredInfo.Find(mkr); // is the
										// just-unfiltered marker also within this stored filtered material?
									if (anOffset != wxNOT_FOUND)
									{
										// it's present, so it has to be removed, as it was
										// from the parent - we do this by simply replacing
										// the content with the parent's altered content for
										// this member
										pFirstOriginal->SetFilteredInfo(remainderStr);
									}
								}
							}
						}

						// do the setup for next iteration of the loop
						preStr.Empty();
						remainderStr.Empty();
						offset = 0;
						offsetNextSection = 0;
						UpdateSequNumbers(0); // get all the sequence 
											  // numbers into correct order
					}
					else
					{
						// we didn't unfilter anything, so prepare for the next 
						// iteration of inner loop
						offset = offsetNextSection;
					}
				} // end of while loop
			}

			if (bDidSomeUnfiltering)
			{
                // the original pSrcPhrase still stores its original nav text in its
                // m_inform member and this is now out of date because some of its content
                // has been unfiltered and made visible, so we have to recalculate its
                // navtext now
				pSrcPhrase->m_inform = RedoNavigationText(pSrcPhrase);
			}

			// update progress bar every 200 iterations
			++nOldCount;
			if (nOldCount % 200 == 0) //if (20 * (nOldCount / 20) == nOldCount)
			{
				msgDisplayed = progMsg.Format(progMsg,gpApp->m_curOutputFilename.c_str(),
												nOldCount,nOldTotal);
				progDlg.Update(nOldCount,msgDisplayed);
			}

			endingMkrsStr.Empty();
		} // loop end for checking each pSrcPhrase for presence of material to be unfiltered

        // check for an orphan carrier of filtered info at the end of the document, which
        // has no longer got any filtered info, and if that is the case, remove it from the
        // document
		SPList::Node* posLast = pList->GetLast();
		CSourcePhrase* pLastSrcPhrase = posLast->GetData();
		if (pLastSrcPhrase->m_key.IsEmpty() && pLastSrcPhrase->GetFilteredInfo().IsEmpty())
		{
			// it needs to be deleted
			gpApp->GetDocument()->DeleteSingleSrcPhrase(pLastSrcPhrase); // delete it and its partner pile
			pList->Erase(posLast);
		}
	} // end block for test bUnfilteringRequired

	// reinitialize the variables we need
	pos = NULL;
	pSrcPhrase = NULL;
	mkr.Empty();
	nFound = -1;
	bool bBoxLocationDestroyed = FALSE; // set TRUE if the box was within a filtered 
                // section, since that will require resetting it an arbitrary location and
                // the latter could be within a retranslation - so we'd have to do an
                // adjustment
	// do the filtering pass now
	curSequNum = -1;
	if (bFilteringRequired)
	{
		// reinitialize the progress bar, and recalc the layout in case the view
		// does some painting when the progress bar is tampered with below
		UpdateSequNumbers(0); // get the numbers updated, as a precaution

		// reinitialize the progress window for the filtering loop
		nOldTotal = pList->GetCount();
		if (nOldTotal == 0)
		{
            // wx version note: Since the progress dialog is modeless we do not need to
            // destroy or otherwise end its modeless state; it will be destroyed when
            // ReconstituteAfterFilteringChange goes out of scope
			pSublist->Clear();
			delete pSublist;
			pSublist = NULL;
			return 0;
		}
		nOldCount = 0;

		progMsg = _("Pass 2 - File: %s  - %d of %d Total words and phrases");
		msgDisplayed = progMsg.Format(progMsg,gpApp->m_curOutputFilename.c_str(),1,nOldCount);

        // the following variables are for tracking how the active sequence number has to
        // be updated after each span of material designated for filtering is filtered out
		bool bBoxBefore = TRUE; // TRUE when the active location is before the first sourcephrase being filtered
		bool bBoxAfter = FALSE; // TRUE when it is located after the last sourcephrase being filtered
		// if both are FALSE, then the active location is within the section being filtered out
		int nStartLocation = -1; // gets set to the sequence number for the first sourcephrase being filtered
		int nAfterLocation = -1; // gets set to the sequ num for the first source phrase after the filter section
		int nCurActiveSequNum = gpApp->m_nActiveSequNum;
		wxASSERT(nCurActiveSequNum >= 0);

		wxString wholeMkr;
		wxString bareMkr; // wholeMkr without the latter's backslash
		wxString shortMkr; // wholeShortMkr without the latter's initial backslash
		wxString wholeShortMkr;
		wxString endMkr;
		endMkr.Empty();
		bool bHasEndmarker = FALSE;
		pos = pList->GetFirst();
		SPList::Node* posStart = NULL; // location of first sourcephrase instance 
									   // being filtered out
		SPList::Node* posEnd = NULL; // location of first sourcephrase instance 
									 // after the section being filtered out
		wxString filteredStr; // accumulate filtered source text here
		wxString tempStr;
		preStr.Empty(); // store in here m_markers content (from first pSrcPhrase) 
                        // which precedes a marker for filtering out
		remainderStr.Empty(); // store here anything in m_markers which follows
                              // the to-be-filtered marker (from first pSrcPhrase)
		wxString strFilteredStuffToCarryForward; // put already filtered stuff which
				// is stored on the first CSourcePhrase of a to-be-filtered section
				// in here, and at the end of the inner loop, deal with it
		while (pos != NULL)
		{
            // acts on ONE instance of pSrcPhrase only each time it loops, but in so doing
            // it may remove many by imposing FILTERED status on a series of instances
            // starting from when it finds a marker which is to be filtered out in an
            // instance's m_markers member - when that happens the loop takes up again at
            // the sourcephrase immediately after the section just filtered out
			SPList::Node* oldPos = pos;
			pSrcPhrase = (CSourcePhrase*)pos->GetData();

			pos = pos->GetNext();
			curSequNum = pSrcPhrase->m_nSequNumber;
			nCurActiveSequNum = gpApp->m_nActiveSequNum;
			//wxString embeddedMkr;
			//wxString embeddedEndMkr;

			// BEW 21Sep10, docVersion 5 changes some of the requirements below, as noted...
            // loop until we find a sourcephrase which is a candidate for filtering - such
            // a one will satisfy all of the following requirements:
			// 1. m_markers is not empty
			// 2. m_markers has at least one marker in it (look for gSFescapechar)
			// 3. the candidate marker, will be in m_markers currently, (because in
			//       docVersion 5 anything filtered which is potentially unfilterable is 
			//       stored in the m_filteredInfo member, bracketed there by \~FILTER and
			//       \~FILTER* markers)
			// 4. the candidate marker will NEVER be an endmarker (docVersion 5 stores
			//      them now in the m_endMarkers member)
			// 5. the marker's USFMAnalysis struct's filter member is currently set to TRUE
			//		(note: markers like xk, xr, xt, xo, fk, fr, ft etc which are inline 
            //      between \x and its matching \x* or \f and its matching \f*, etc, are
            //      marked filter==TRUE, but they have to be skipped, and their
            //      userCanSetFilter member is **always** FALSE. We won't use that fact,
            //      but we use the facts that the first character of their markers is
            //      always the same, x for cross references, f for footnotes, etc, and that
            //      they will have inLine="1" ie, their inLine value in the struct will be
            //      TRUE. Then when parsing over a stretch of text which is marked by a
            //      marker which has no endmarker, we'll know to halt parsing if we come to
            //      a marker with inLine == FALSE; but if TRUE, then a second test is
            //      needed, textType="none" NOT being current will effect the halt - so
            //      this pair of tests should enable us to prevent parsing overrun. (Note:
            //      we want our code to correctly filter a misspelled marker which is
            //      always to be filtered, after the user has edited it to be spelled
            //      correctly.
			// 6. the marker is listed for filtering in the wxString strMarkersToBeFiltered 
            //      (determined from the m_FilterStatusMap map which is set from the
            //      Filtering page of the GUI)
			wxString markersStr;
			if (pSrcPhrase->m_markers.IsEmpty())
			{

				// the following copied from bottom of loop to here in order to 
				// remove the goto c; and label
				++nOldCount;
				if (nOldCount % 200 == 0) //if (20 * (nOldCount / 20) == nOldCount)
				{
					msgDisplayed = progMsg.Format(progMsg,
						gpApp->m_curOutputFilename.c_str(),nOldCount,nOldTotal);
						progDlg.Update(nOldCount,msgDisplayed);
				}

				continue;
			}

            // NOTE: **** this algorithm allows the user to put italics substrings (marked
            // by \it ... \it*), or similar marker & endmarker pairs, within text spans
            // potentially filterable - this should be a safe because such embedded content
            // marker pairs should have a TextType of none in the XML marker specifications
            // document, and Adapt It will skip such ones, but stop scanning when either
            // inLine is FALSE, or if TRUE, then when TextType is not none ****
			markersStr = pSrcPhrase->m_markers;
			bool bIsUnknownMkr = FALSE;
			int nStartingOffset = 0;
			wxChar backslash[2] = {gSFescapechar,_T('\0')};
g:			int filterableMkrOffset = ContainsMarkerToBeFiltered(gpApp->gCurrentSfmSet,
						markersStr, strMarkersToBeFiltered, wholeMkr, wholeShortMkr, endMkr,
						bHasEndmarker, bIsUnknownMkr, nStartingOffset);
			if (filterableMkrOffset == wxNOT_FOUND)
			{
                // either wholeMkr is not filterable, or its not in strMarkersToBeFiltered
                // -- if so, just iterate to the next sourcephrase

				// the following copied from bottom of loop to here in order to remove the 
				// goto c; and label
				++nOldCount;
				if (nOldCount % 200 == 0) //if (20 * (nOldCount / 20) == nOldCount)
				{
					msgDisplayed = progMsg.Format(progMsg,
						gpApp->m_curOutputFilename.c_str(),nOldCount,nOldTotal);
						progDlg.Update(nOldCount,msgDisplayed);
				}

				continue;
			}

            // if we get there, it's a marker which is to be filtered out, and we know its
            // offset - so set up preStr and remainderStr so we can commence the filtering
            // properly...
            
            // Question: What if we filter a section containing within it filtered
            // information? Answer: This is not possible, except if there is filtered
            // information stored on the CSourcePhrase which has the wholeMkr in its
            // m_markers member -- in that case and that case alone, the to-be-filtered
            // section CAN contain filtered material which is to remain filtered -- if so,
            // we must carry that already filtered information forward along with the
            // to-be-filtered material eventually placed after it (to preserve the correct
            // order of the information should all be unfiltered).
            // SFM and USFM markup does not permit marker nesting, except for \f and \x,
            // and we've a TextType and special code to handle such information as wholes,
            // and so there will never be nesting of filtered information within the Adapt
            // It document; but the CSourcePhrase instance with the filterable marker is
            // the exception -- because if filtered markers all happened to stack at the
            // one CSourcePhrase, then unfiltering and refiltering must restore that
            // stacking in m_filteredInfo and do it in the correct order. So we have to
            // check now for this special case, and any m_filteredInfo content there has to
            // be carried forward.... the next line of code does it
			strFilteredStuffToCarryForward = pSrcPhrase->GetFilteredInfo(); // could be empty
#ifdef __WXDEBUG__
				//if (pSrcPhrase->m_nSequNumber >= 27)
				//{
				//	wxString theSrcPhrase = pSrcPhrase->m_srcPhrase;
				//}
#endif
			
			// BEW 21Sep10 -- read the next two paragraphs carefully, because for
			// docVersion 5 the protocols described in them need changing slightly - the
			// variations will be explained after these two paragraphs...
			// 
            // Legacy comments (when filtered info was all stored in m_markers):
            // We have to be careful here, we can't assume that .Mid(filterableMkrOffset)
            // will deliver the correct remainderStr to be stored till later on, because a
            // filterable marker like \b could be followed by an unfiltered marker like \v,
            // or something filtered (and hence \~FILTER would follow), or even a different
            // filterable marker (like \x), and so we have to check here for the presence
            // of another marker which follows it - if there is one, we have found a marker
            // which is to be filtered, but which has no content - such as \b, and in that
            // case all we need do is bracket it with \~FILTER and \~FILTER* and then retry
            // the ContainsMarkerToBeFiltered() call above.
			//
            // Note: markers like \b which have no content must always be
            // userCanSetFilter="0" because they must always be filtered, or always be
            // unfiltered, but never be able to have their filtering status changed. This
            // is because out code for filtering out when a marker has been changed always
            // assumes there is some following content to the marker, but in the case of \b
            // or similar contentless markers this would not be the case, and our code
            // would then incorrectly filter out whatever follows (it could be inspired
            // text!) until the next marker is encountered. At present, we have specified
            // that \b is always to be filtered, so the code below will turn \b as an
            // unknown and unfiltered marker when PngOnly is the SFM set, to \~FILTER \b
            // \~FILTER* when the user changes to the UsfmOnly set, or the UsfmAndPng set.
            // Similarly for other contentless markers.
            // 
			// Variations for docVersion 5: \~FILTER and \~FILTER* no longer will appear
			// in the CSourcePhrase m_markers member; \b and similar markers can still
			// appear there, and the protocols for this and other contentless filterable
			// markers are unchanged. If we encounter such a marker, we do not bracket it
			// with \~FILTER and \~FILTER* in the m_markers member, but rather move it to
			// the end of the m_filteredInfo member and provide \~FILTER and \~FILTER*
			// bracketing markers for it there instead.
			int itsLen = wholeMkr.Length();			
			int nOffsetToNextBit = filterableMkrOffset + itsLen;
			if ((wholeMkr != _T("\\f")) && (wholeMkr != _T("\\x")) &&
				(nFound = FindFromPos(markersStr,backslash,nOffsetToNextBit)) != wxNOT_FOUND) 
			{
                // there is a following SF marker which is not a \f or \x (the latter two
                // can have a following marker within their scope, so whether that happens
                // or not, they are not to be considered as contentless), so the current
                // one cannot have any text content -- this follows from the fact that the
                // text content of a marker cannot appear in m_markers (because in
                // docVersion 5 filtered markers are now not stored in m_markers, but in
                // m_filteredInfo), so if we find another marker using the FindFromPos()
                // call then we know the one found at the ContainsMarkerToBeFiltered() call
                // is a contentless marker.
                // Also, docVersion 5's storage in the CSourcePhrase implies that if a
                // marker which has content visible in the interlinear main window display
                // is encountered in m_markers, then it will be the last marker in
                // m_markers - this fact makes further assumptions below, safe to make

				// extract the marker (marker only, no following space included)
				wxString contentlessMkr = markersStr.Mid(filterableMkrOffset,
											nOffsetToNextBit - filterableMkrOffset);
                // wx version note: Since we require a read-only buffer we use GetData
                // which just returns a const wxChar* to the data in the string.
				const wxChar* ptr = contentlessMkr.GetData();
				
				// whm added two lines below because wxStringBuffer needs to insure buffer 
				// ends with null char
				wxChar* pEnd;
				pEnd = (wxChar*)ptr + itsLen;
				wxASSERT(*pEnd == _T('\0'));

				wxString temp;
				temp = GetFilteredItemBracketed(ptr,itsLen); // temp lacks a final space
				temp += _T(' '); // add the required trailing space

				// BEW 21Sep10, new code needed here for docVersion 5 - we have to remove
				// the contentless marker from the m_markers string (and chuck it away), and put the
				// bracketed version of it, stored in temp, into the end of m_filteredInfo
				markersStr.Remove(filterableMkrOffset,nOffsetToNextBit - filterableMkrOffset);
				// for an unknown reason it does not delete the space, so I have to test and
				// if so, delete it
				if (markersStr[filterableMkrOffset] == _T(' '))
				{
					// wxString::Remove needs 1 as second parameter otherwise it truncates 
					// remainder of string
					markersStr.Remove(filterableMkrOffset,1); // delete extra space 
															  // if one is here
				}
				// now update the m_filteredInfo member to contain this marker 
				// appropriately filtered
				wxString filteredStr = pSrcPhrase->GetFilteredInfo();
				filteredStr += temp;
				pSrcPhrase->SetFilteredInfo(filteredStr);
				// update the local string populated above, since we've added to m_filteredInfo
				strFilteredStuffToCarryForward = pSrcPhrase->GetFilteredInfo();
				// update the m_markers member with the shorter markersStr contents
				pSrcPhrase->m_markers = markersStr;
				
                // get the navigation text set up correctly (the contentless marker just
                // now filtered out should then no longer appear in the nav text line)
				pSrcPhrase->m_inform = RedoNavigationText(pSrcPhrase);

                // advance beyond this just-filtered contentless marker's location and try
                // again 
                // BEW 21Sep10, for docVersion 5 the calculation is different (simpler)
				nStartingOffset = nFound; // point at the next marker found at 
										  // the above FindFromPos() call
				goto g;
			}
			else
			{
                // there is no following SF marker, so the current one may be assumed to
                // have content currently visible in the interlinear display
				;
			}
			preStr = markersStr.Left(filterableMkrOffset); // this stuff we 
						// accumulate for later on -- it will later go to the CSourcePhrase
						// which follows the instances about to be filtered out; in
						// docVersion 5 I can't think of anything that might be in preStr
						// and so it is likely to always be empty, but just in case it
						// isn't, we must preserve whatever is there & transfer it
			remainderStr = markersStr.Mid(filterableMkrOffset); // this stuff is the 
                        // marker for the stuff we are about to filter, and there may 
                        // be other following markers - such as \p or \v etc which need
                        // to be accumulated and forwarded to the CSourcePhrase which
                        // follows the instances about to be filtered out (in docVersion
                        // 5, remainderStr won't ever have any filtered content data,
                        // but only markers and possibly a verse or chapter number)
						// We now must remove the marker we found, and retain anything
						// which remains other than spaces
			remainderStr = remainderStr.Mid(itsLen); // itsLen is the length of wholeMkr
			remainderStr.Trim(FALSE); // trim off any initial spaces, or if only spaces 
									  // remain, then this will empty remainderStr
									  

			/* I don't think this next bit is needed any longer
			bareMkr = wholeMkr.Mid(1); // remove the backslash, we already know 
									   // it has no final *
            // it is not valid to have formed a prefix like \f or \x if the marker from
            // which it is being formed is already only a single character preceded by a
            // backslash, so if this is the case then make WholeShortMkr, and shortMkr,
            // empty strings
            // BEW extended test, 2Jul05, to prevent a marker like \imt (which gives a
            // short form of \i) from giving spurious shortMkr matches with unrelated
            // markers like \ip \ipq \im \io1 2 or 3, \is1 etc.
			if (wholeShortMkr == wholeMkr || 
				((wholeShortMkr != _T("\\f")) && (wholeShortMkr != _T("\\x"))) )
			{
                // this will prevent spurious matches in our code below, and prevent \f or
                // \x from being wrongly interpretted as the embedded context markers like
                // \fr, \ft, etc, or \xo, etc
				shortMkr.Empty();
				wholeShortMkr.Empty();
			}
			else
			{
				shortMkr = wholeShortMkr.Mid(1);
			}
            // the only shortMkr forms which get made, as a result of the above, are \f and
            // \x - anything else will be forced to an empty string, so that the code for
            // determining the ending sourcephrase will not wrongly result in loop
            // iteration when loop ending should happen
             */

            // okay, we've found a marker to be filtered, we now have to look ahead to find
            // which sourcephrase instance is the last one in this filtering sequence - we
            // will assume the following:
            // BEW 21Sep10: the following protocols are valid also for docVersion 5
			// 1. unknown markers never have an associated endmarker - so their extent ends 
			//      when a subsequent marker is encounted which is not an inline one, and
			//      is found in m_markers - the owning CSourcePhrase instance is NOT in
			//      the filterable span     
			// 2. if the marker has an endmarker, then any other markers are skipped over -
			//      we just look for the matching endmarker as the last marker in m_endMarkers
			//      member - that owning CSourcePhrase instance would then be the last in the
			//      span
			// 3. filterable markers which lack an endmarker will end their content when the 
			//      next marker is encountered in a subsequent CSourcePhrase instance
			//      whose m_markers member contains that marker - which must not be an
			//      inline one (easily satisfied, since docV5 NEVER stores inline markers
			//      in m_markers, with the exception of \f, \fe or \x)
			// 4. markers which have an optional endmarker will end using criterion 3. above,
            //      unless the marker at that location has its first two characters
            //      identical to wholeShortMkr - in which case the section will end when
            //      the first marker is encountered for which that is not so

			// we can now partly or fully determine where the active location is in 
			// relation to this location
			if (nCurActiveSequNum < pSrcPhrase->m_nSequNumber)
			{
				// if control comes here, the location is determinate
				bBoxBefore = TRUE;
				bBoxAfter = FALSE;
			}
			else
			{
                // if control comes here, the location is indeterminate - it might yet be
                // within the filtering section, or after it - we'll assume the latter, and
                // change it later if it is wrong when we get to the first sourcephrase
                // instance following the section for filtering
				bBoxBefore = FALSE;
				bBoxAfter = TRUE;
			}
			nStartLocation = pSrcPhrase->m_nSequNumber;
			// NOTE: we'll deal with preStr when we set up pUnfilteredSrcPhrase's 
			// m_markers member later on; likewise for anything still in remainderStr

			posStart = oldPos; // preserve this starting location for later on

			// we can commence to build filteredStr now (Note: because filtering stores a
			// string, rather than a sequence of CSoucePhrase instances, any adaptations
			// will be thrown away irrecoverably. We could change this in the future, but
			// it would be a major core change and require a docVersion 6 and a
			// significant redevelopment effort; but it would be a better model.)
			filteredStr = filterMkr; // add the \~FILTER beginning marker
			filteredStr += _T(' '); // add space
			CSourcePhrase* pSrcPhraseFirst = new CSourcePhrase(*pSrcPhrase);
			pSrcPhraseFirst->DeepCopy();
			pSublist->Append(pSrcPhraseFirst); // we've already got the first to go in
										  // the sublist, so put it there and then loop
										  // to get the rest
			// Enter an inner loop which has as it's sole purpose finding which
			// CSourcePhrase instance at pos or beyond is the last one for filtering out
			// as part of the current filterable span. In the loop we make deep copies in
			// order to create a sublist of accepted within-the-span CSourcePhrase
			// instances; we then use UpdateSequNumbers() to renumber from 0 those
			// instances in the sublist, and then after the loop ends we process all the
			// sublist's contents in one hit by using the ExportFunctions.cpp function,
			// RebuildSourceText(), passing in a pointer to the sublist. Doing it this way
			// means that we have one place only for reconstituting the source text,
			// giving us consistency, and we get the inline markers handling done 'for
			// free' rather than having to add it to the complex code this approach replaces.
            // 
            // BEW 7Dec10:
			// Question: What if we filter a section where there is a note, or free
			// translation or a collected backtranslation? 
			// Answer: note information is irreversibly lost. Free and / or collected back
			// translation information halts parsing, so we retain those. (Legacy code
			// didn't retain those though.)
			SPList::Node* pos2; // this is the 'next' location
			CSourcePhrase* pSrcPhr; // we look for a section-ending matching endmarker
									// in this one, if we don't find one, we try the
									// m_markers member of pSrcPhraseNext instead; if
									// we find a matching endmarker, pSrcPhrase would be
									// WITHIN and at the end of the filterable section
			CSourcePhrase* pSrcPhraseNext; // this could have in its m_markers member 
										   // a non-inline marker which ends the section
										   // that is, this one would NOT be part of the
										   // filterable section
			SPList::Node* savePos2 = NULL;
			bool bAtEnd = FALSE; // use this for filtered section's end (set TRUE when
								 // we are at the final CSourcePhrase instance which is
								 // to be filtered out, in the loop below
			bool bAtDocEnd = FALSE; // BEW added 11Oct10, needed, as doc end needs to
								// be known, if reached, so we can append a carrier
								// CSourcePhrase for the filtered material there
			// put our deep copies of the span's CSourcePhrase instances in pSublist (see
			// above, it's on the heap)
			// Initialize pSrcPhr, to avoid a compiler warning after the loop
			pSrcPhr = (CSourcePhrase*)pos->GetData(); // redundant, avoids the warning later
			for (pos2 = pos; pos2 != NULL; )
			{
				savePos2 = pos2;
				pSrcPhr = (CSourcePhrase*)pos2->GetData();
				pos2 = pos2->GetNext(); // advance to next Node of the passed in pList
				wxASSERT(pSrcPhr);
				bAtEnd = FALSE;
				bAtDocEnd = FALSE;
				posEnd = pos2; // on exit of the loop, posEnd will be where 
							   // pSrcPhraseNext is located, or NULL if we reached
							   // the end of the document
				if (pos2 == NULL)
				{
					// we've come to the doc end, and that forces the span end too with
					// pSrcPhrase as the last one (and so we'll need an ophan created
					// later in order to "carry" the filtered information)
					pSrcPhraseNext = NULL;
				}
				else
				{
					pSrcPhraseNext = (CSourcePhrase*)pos2->GetData();
				}

				// check first for a halt to scanning caused by finding the required
				// matching endmarker for the contents of wholeMkr. If no match is found
				// (as would be the case if wholeMkr is not an SFM which has a pairing
				// endmarker defined), then the pSrcPhraseNext instance needs to be
				// checked - in it's m_markers member, for a halt-causing beginmarker - if
				// one such is not found, the m_markers content will be accumulated on the
				// next iteration into the source text being accumulated and scanning will
				// continue until a halt is achieved
				if (HasMatchingEndMarker(wholeMkr,pSrcPhr))
				{
					// halt here, this pSrcPhr is last in the filterable span
					bAtEnd = TRUE;
					// we may or may not be at the end of the document, check
					if (pSrcPhraseNext == NULL)
					{
						bAtDocEnd = TRUE;
					}
					break;
				}
				else if (pSrcPhraseNext != NULL)
				{
					if (IsEndingSrcPhrase(gpApp->gCurrentSfmSet, pSrcPhraseNext))
					{
						// this 'next' CSourcePhrase instance causes a halt, and is not
						// itself to be within the filterable span, so the present
						// pSrcPhr instance is last in the span
						bAtEnd = TRUE;  // and bAtDocEnd remains FALSE
						break;
					}
					// a FALSE value in the above test means that scanning should
					// continue, so just fall through to the code below which makes and
					// appends to the sublist the required deep copy of pSrcPhr
				}
				else // pSrcPhraseNext does not exist (the pointer is NULL)
				{
					// so pSrcPhr is the last CSourcePhrase instance in the document
					bAtEnd = TRUE;
					bAtDocEnd = TRUE;
					break;
				}
				// if control has not broken out of the loop, then we must continue
				// scanning over more CSourcePhrase instances till we halt; but first we
				// must create the needed deep copy and append it to the sublist
				CSourcePhrase* pSrcPhraseCopy = new CSourcePhrase(*pSrcPhr); // a shallow copy
				pSrcPhraseCopy->DeepCopy(); // now it's a deep copy of pSrcPhrase
				pSublist->Append(pSrcPhraseCopy);
			} // end of for loop: for (pos2 = pos; pos2 != NULL; )

			// do the final needed deep copy of pSrcPhrase and append to the sublist
			CSourcePhrase* pSrcPhraseCopy = new CSourcePhrase(*pSrcPhr); // a shallow copy
			pSrcPhraseCopy->DeepCopy(); // now it's a deep copy of pSrcPhrase
			pSublist->Append(pSrcPhraseCopy);
			// get the sequence numbers in the stored instances consecutive from 0
			UpdateSequNumbers(0, pSublist);

            // complete the determination of where the active location is in relation to
            // this filtered section, and work out the active sequ number adjustment needed
            // and make the adjustment
			if (bBoxBefore == FALSE)
			{
				// adjustment maybe needed only when we know the box was not located 
				// preceding the filter section
				if (posEnd == NULL)
				{
                    // at the document end, so everything up to the end is to be filtered;
                    // so either the active location is before the filtered section (ie.
                    // bBoxBefore == TRUE), or it is within the filtered section (it.
                    // bBoxBefore == FALSE)
					bBoxAfter = FALSE;
				}
				else
				{
					// posEnd is defined, so get the sequence number for this location
					nAfterLocation = posEnd->GetData()->m_nSequNumber;

					// work out if an adjustment to bBoxAfter is needed (bBoxAfter is 
					// set TRUE so far)
					if (nCurActiveSequNum < nAfterLocation)
						bBoxAfter = FALSE; // the box lay within this section 
										   // being filtered
				}
			}
			bool bPosEndNull = posEnd == NULL ? TRUE : FALSE; // used below to work out
											// where to set the final active location

			// here 'export' the src text into a wxString, and then append that to 
			// filteredStr
			wxString strFilteredStuff;
			strFilteredStuff.Empty();
			if (!pSublist->IsEmpty())
			{
				int textLen = RebuildSourceText(strFilteredStuff, pSublist);
				textLen = textLen; // to avoid a compiler warning
				// remove any initial whitespace
				strFilteredStuff.Trim(FALSE);
			}
			else
			{
				// we don't ever expect such an error, so an English message will do
				wxString msg;
				wxBell();
				msg = msg.Format(_T("Filtering the content for marker %s failed.\nDeep copies were not stored.\nSome source text data has been lost at sequNum %d.\nDo NOT save the document, exit, relaunch and try again."),
					wholeMkr.c_str(), nStartLocation);
				wxMessageBox(msg,_T(""), wxICON_ERROR);
				// put a message into the document so it is easy to track down where it
				// went wrong
				strFilteredStuff = _T("THIS IS WHERE THE FAILURE TO STORE DEEP COPIES OCCURRED. ");
			}
			filteredStr += strFilteredStuff;

			// add the bracketing end filtermarker \~FILTER*
			filteredStr.Trim(); // don't need a final space before \~FILTER*
			filteredStr += filterMkrEnd;

			// delete the sublist's deep copied CSourcePhrase instances
			bool bDoPartnerPileDeletionAlso = FALSE; // there are no partner piles to delete
			DeleteSourcePhrases(pSublist, bDoPartnerPileDeletionAlso);
			pSublist->Clear(); // ready it for a later filtering out

            // remove the pointers from the m_pSourcePhrases list (ie. those which were
            // filtered out), and delete their memory chunks; any adaptations on these are
            // lost forever, but not from the KB unless the latter is rebuilt from the
            // document contents at a later time
			SPList::Node* pos3; // use this to save the old location so as to delete the 
								// old node once the iterator has moved past it
			int filterCount = 0;
			for (pos2 = posStart; (pos3 = pos2) != posEnd; )
			{
				filterCount++;
				CSourcePhrase* pSP = (CSourcePhrase*)pos2->GetData();
				pos2 = pos2->GetNext();
				DeleteSingleSrcPhrase(pSP,TRUE); // don't leak memory, do also delete their
							// partner piles, as the latter should exist for information
							// unfiltered up to now and therefore was visible in the view
				pList->DeleteNode(pos3);
			}

            // update the sequence numbers on the sourcephrase instances which remain in
            // the document's list and reset nAfterLocation and nStartLocation accordingly
			UpdateSequNumbers(0);
			nAfterLocation = nStartLocation;
			nStartLocation = nStartLocation > 0 ? nStartLocation - 1: 0;

			// we can now work out what adjustment is needed
			// 1. if the active location was before the filter section, no adjustment is 
			//     needed
			// 2. if it was after the filter section, the active sequence number must be 
            //     decreased by the number of sourcephrase instances in the section being
            //     filtered out
			// 3. if it was within the filter section, it will not be possible to preserve 
            //     its location in which case we must try find a safe location (a) as close
            //     as possible after the filtered section (when posEnd exists), or (b), as
            //     close as possible before the filtered section (when posEnd is NULL)
			if (!bBoxBefore)
			{
				if (bBoxAfter)
				{
					nCurActiveSequNum -= filterCount;
				}
				else
				{
					// the box was located within the span of the material which was 
					// filtered out
					bBoxLocationDestroyed = TRUE;
					if (bPosEndNull)
					{
                        // put the box before the filtered section (this may not be a valid
                        // location, eg. it might be a retranslation section - but we'll
                        // adjust later when we set the bundle indices)
						nCurActiveSequNum = nStartLocation;
					}
					else
					{
                        // put it after the filtered section (this may not be a valid
                        // location, eg. it might be a retranslation section - but we'll
                        // adjust later when we set the bundle indices)
						nCurActiveSequNum = nAfterLocation;
					}
				}
			}
			gpApp->m_nActiveSequNum = nCurActiveSequNum;

            // construct m_markers on the first sourcephrase following the filtered
            // section; if the filtered section is at the end of the document (shouldn't
            // happen, but who knows what a user will do?) then we will need to detect this
            // and create a CSourcePhrase instance with empty key in order to be able to
            // store the filtered content in its m_markers member, and add it to the tail
            // of the doc. A filtered section at the end of the document will manifest by
            // pos2 being NULL on exit of the above loop
            // BEW 22Sep10, changes for docVersion 5:
			// (1) preStr and remainderStr will need to be inserted at the start of the
			// pUnfilteredSrcPhrase's m_markers member, if either or both are non-empty
			// (they'll almost certainly be both empty)
			// (2) filteredStr has to be appropriately stored, it no longer is embedded in
			// pUnfilteredSrcPhrase's m_markers member, but at the START of its
			// m_filteredInfo member (to preserve the order of source text information
			// across filtering/unfiltering changes)
			// (3) the new storage for already filtered stuff being carried forward, the
			// string strFilteredStuffToCarryForward, has to be handled here too (it
			// could, of course, be an empty string)
			CSourcePhrase* pUnfilteredSrcPhrase = NULL;
			if (posEnd == NULL)
			{
				pUnfilteredSrcPhrase = new CSourcePhrase;

				// force it to at least display an asterisk in nav text area to alert the 
				// user to its presence
				pUnfilteredSrcPhrase->m_bNotInKB = TRUE; 
				pList->Append(pUnfilteredSrcPhrase);
			}
			else
			{
				// get the first sourcephrase instance following the filtered section
				pUnfilteredSrcPhrase = (CSourcePhrase*)posEnd->GetData();
			}
			wxASSERT(pUnfilteredSrcPhrase);

			// fill its m_markers with the material it needs to store, 
			// in the correct order
			tempStr = pUnfilteredSrcPhrase->m_markers; // hold this stuff temporarily, 
							// as we must later add it to the end of everything else
			tempStr.Trim(FALSE); // this is easier than the above line
			pUnfilteredSrcPhrase->m_markers = preStr; // any previously assumulated 
													  // filtered info, or markers
			// BEW 22Sep10 added next 3 lines
			pUnfilteredSrcPhrase->m_markers.Trim();
			pUnfilteredSrcPhrase->m_markers += _T(" "); //ensure an intervening space
			pUnfilteredSrcPhrase->m_markers += remainderStr;
			pUnfilteredSrcPhrase->m_markers.Trim(FALSE); // delete contents 
												// if only spaces are present
			if (!tempStr.IsEmpty())
			{
				pUnfilteredSrcPhrase->m_markers.Trim();
				pUnfilteredSrcPhrase->m_markers += _T(" "); //ensure an intervening space
				// append whatever was originally on this srcPhrase::m_markers member
				pUnfilteredSrcPhrase->m_markers += tempStr;
			}

            // insert any already filtered stuff we needed to carry forward before the newly
			// filtered material (because if it was unfiltered, it would appear in the
			// view before it, and so we must retain that order)
			if (!strFilteredStuffToCarryForward.IsEmpty())
			{
				//(in the legacy code, this bit of work was done by remainderStr, because
				//filtered info was all in m_markers; but for docVersion 5 that was
				//inappropriate -- so I retained remainderStr only for contentless markers stuff
				//which might come after the marker to be filtered out, and stored already
				//filtered stuff in strFilteredStuffToCarryForward)
				filteredStr = strFilteredStuffToCarryForward + filteredStr;
			}
			// we've carried the already filtered info forward, so make sure it goes no further
			strFilteredStuffToCarryForward.Empty();

            // insert the newly filtered material (and any carried forward filtered info
            // which we appended above) to the start of m_filteredInfo on the CSourcePhrase
            // which follows the filtered out section - that one might have filtered material
            // already, so we have to check and take the appropriate branch (Note: if the
            // stuff being filtered precedes stuff already filtered, then filtered
            // information can stack up in a single CSourcePhrase's m_filteredInfo member
            // -- but this is unlikely as it would require an instance of the filterable
            // marker in successive subspans of CSourcePhrases in the document, and that
            // would not constitute good USFM markup - but we can handle it if it does
            // happen.)
			wxString filteredStuff = pUnfilteredSrcPhrase->GetFilteredInfo();
			if (filteredStuff.IsEmpty())
			{
				pUnfilteredSrcPhrase->SetFilteredInfo(filteredStr);
			}
			else
			{
				filteredStuff = filteredStr + filteredStuff;
				pUnfilteredSrcPhrase->SetFilteredInfo(filteredStuff);
			}

			preStr.Empty();
			remainderStr.Empty();

			// get the navigation text set up correctly
			pUnfilteredSrcPhrase->m_inform = RedoNavigationText(pUnfilteredSrcPhrase);

			// enable iteration from this location
			if (posEnd == NULL)
			{
				pos = NULL;
			}
			else
			{
				pos = posEnd; // this could be the start of a consecutive section 
							  // for filtering out
			}
			// update progress bar every 200 iterations (1000 is a bit too many)
			
			++nOldCount;
			if (nOldCount % 200 == 0) //if (20 * (nOldCount / 20) == nOldCount)
			{
				msgDisplayed = progMsg.Format(progMsg,
					gpApp->m_curOutputFilename.c_str(),nOldCount,nOldTotal);
					progDlg.Update(nOldCount,msgDisplayed);
			}
		} // end of loop for scanning contents of successive pSrcPhrase instances
		
		// remove the progress indicator window
		progDlg.Destroy();

        // prepare for update of view... locate the phrase box approximately where it was,
        // but if that is not a valid location then put it at the end of the doc
		int numElements = pList->GetCount();
		if (gpApp->m_nActiveSequNum > gpApp->GetMaxIndex())
			gpApp->m_nActiveSequNum = numElements - 1;

	} // end of block for bIsFilteringRequired == TRUE

    // GetSavePhraseBoxLocationUsingList calculates a safe location (ie. not in a
    // retranslation), sets the view's m_nActiveSequNumber member to that value, and
    // calculates and sets m_targetPhrase to agree with what will be the new phrase box
	// location; it doesn't move the location if it is already safe; in either case it
	// sets the box text to be the m_adaption contents for the current active location
	// at the time this call is made
	gpApp->GetSafePhraseBoxLocationUsingList(pView);

	// remove the progress window, clear out the sublist from memory
	// wx version note: Since the progress dialog is modeless we do not need to destroy
	// or otherwise end its modeless state; it will be destroyed when 
	// ReconstituteAfterFilteringChange goes out of scope
	if (pSublist)
	{
		pSublist->Clear();
		delete pSublist;
		pSublist = NULL;
	}
	// BEW added 29Jul09, turn back on CLayout Draw() so drawing of the view
	// can now be done
	GetLayout()->m_bInhibitDraw = FALSE;

	return bSuccessful;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE when we want the caller to copy the pLastSrcPhrase->m_curTextType value to the
///				global enum, gPreviousTextType; otherwise we return FALSE to suppress that global
///				from being changed by whatever marker has just been parsed (and the caller will
///				reset the global to default TextType verse when an endmarker is encountered).
/// \param		pChar		-> points at the marker just found (ie. at its backslash)
/// \param		pAnalysis	-> points at the USFMAnalysis struct for this marker, if the marker 
///								is not unknown otherwise it is NULL.
/// \remarks
/// Called from: the Doc's RetokenizeText().
/// TokenizeText() calls AnalyseMarker() to try to determine, among other things, what the TextType
/// propagation characteristics should be for any given marker which is not an endmarker; for some
/// such contexts, AnalyseMarker will want to preserve the TextType in the preceding context so it
/// can be restored when appropriate - so IsPreviousTextTypeWanted determines when this preservation
/// is appropriate so the caller can set the global which preserves the value
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsPreviousTextTypeWanted(wxChar* pChar,USFMAnalysis* pAnalysis)
{
	wxString bareMkr = GetBareMarkerForLookup(pChar);
	wxASSERT(!bareMkr.IsEmpty());
	wxString markerWithoutBackslash = GetMarkerWithoutBackslash(pChar);

	// if we have a \f or \x marker, then we always want to get the TextType on whatever
	// is the sourcephrase preceding either or these
	if (markerWithoutBackslash == _T("f") || markerWithoutBackslash == _T("x"))
		return TRUE;
	// for other markers, we want the preceding sourcephrase's TextType whenever we
	// have encountered some other inLine == TRUE marker which has TextType of none
	// because these are the ones we'll want to propagate the previous type across
	// their text content - to check for these, we need to look inside pAnalysis
	if (pAnalysis == NULL)
	{
		return FALSE;
	}
	else
	{
		// its a known marker, so check if it's an inline one
		if (pAnalysis->inLine && pAnalysis->textType == none)
		{
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		
/// \param		filename	-> the filename to associate with the current document
/// \param		notifyViews	-> defaults to FALSE; if TRUE wxView's OnChangeFilename 
///                            is called for all views 
/// \remarks
/// Called from: the App's OnInit(), DoKBRestore(), DoTransformationsToGlosses(),
/// ChangeDocUnderlyingFileDetailsInPlace(), the Doc's OnNewDocument(), OnFileClose(),
/// DoFileSave(), SetDocumentWindowTitle(), DoUnpackDocument(), the View's
/// OnEditConsistencyCheck(), DoConsistencyCheck(), DoRetranslationReport(), the DocPage's
/// OnWizardFinish(), CMainFrame's SyncScrollReceive() and OnMRUFile().
/// Sets the file name associated internally with the current document.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::SetFilename(const wxString& filename, bool notifyViews)
{
    m_documentFile = filename;
    if ( notifyViews )
    {
        // Notify the views that the filename has changed
        wxNode *node = m_documentViews.GetFirst();
        while (node)
        {
            wxView *view = (wxView *)node->GetData();
            view->OnChangeFilename(); 
			// OnChangeFilename() is called when the filename has changed. The default 
			// implementation constructs a suitable title and sets the title of 
			// the view frame (if any).
            node = node->GetNext();
        }
    }
}


///////////////////////////////////////////////////////////////////////////////
/// \return		a pointer to the running application (CAdapt_ItApp*)
/// \remarks
/// Called from: Most routines in the Doc which need a pointer to refer to the App.
/// A convenience function.
///////////////////////////////////////////////////////////////////////////////
CAdapt_ItApp* CAdapt_ItDoc::GetApp()
{
	return (CAdapt_ItApp*)&wxGetApp();
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if the character immediately before prt is a newline character (\n)
/// \param		ptr			-> a pointer to a character being examined/referenced
/// \param		pBufStart	-> the start of the buffer being examined
/// \remarks
/// Called from: the Doc's IsMarker().
/// Determines if the previous character in the buffer is a newline character, providing ptr
/// is not pointing at the beginning of the buffer (pBufStart).
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsPrevCharANewline(wxChar* ptr, wxChar* pBufStart)
{
	if (ptr <= pBufStart)
		return TRUE; // treat start of buffer as a virtual newline
	--ptr; // point at previous character
	if (*ptr == _T('\n'))
		return TRUE;
	else
		return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if the character at ptr is a whitespace character, FALSE otherwise.
/// \param		pChar	-> a pointer to a character being examined/referenced
/// \remarks
/// Called from: the Doc's ParseWhiteSpace(), ParseNumber(), IsVerseMarker(), ParseMarker(),
/// MarkerAtBufPtr(), ParseWord(), IsChapterMarker(), TokenizeText(), DoMarkerHousekeeping(),
/// the View's DetachedNonQuotePunctuationFollows(), DoExportSrcOrTgtRTF(), DoesTheRestMatch(),
/// PrecedingWhitespaceHadNewLine(), NextMarkerIsFootnoteEndnoteCrossRef(), 
/// the CViewFilteredMaterialDlg's InitDialog().
/// Note: The XML.cpp file has its own IsWhiteSpace() function which is used within XML.cpp (it
/// does not use wxIsspace() internally but defines whitespace explicitly as a space, tab, \r 
/// or \n.
/// Whitespace is generally defined as a space, a tab, or an end-of-line character/sequence.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsWhiteSpace(wxChar *pChar)
{
	// returns true for tab 0x09, return 0x0D or space 0x20
	if (wxIsspace(*pChar) == 0)// _istspace not recognized by g++ under Linux
		return FALSE;
	else
		return TRUE;

	// equivalent code:
	//if (*pChar == _T('\t') || *pChar == _T('\r') || *pChar == _T(' '))
	//	return TRUE;
	//else
	//	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		the number of whitespace characters parsed
/// \param		pChar	-> a pointer to a character being examined/referenced
/// \remarks
/// Called from: the Doc's GetMarkersAndTextFromString(), TokenizeText(), DoMarkerHousekeeping(),
/// the View's DetachedNonQuotePunctuationFollows(), FormatMarkerBufferForOutput(),
/// FormatUnstructuredTextBufferForOutput(), DoExportInterlinearRTF(), DoExportSrcOrTgtRTF(),
/// DoesTheRestMatch(), ProcessAndWriteDestinationText(), ApplyOutputFilterToText(),
/// ParseAnyFollowingChapterLabel(), NextMarkerIsFootnoteEndnoteCrossRef(), 
/// IsFixedSpaceAhead() and from Usfm2Oxes ParseMarker_Content_Endmarker()
/// Parses through a buffer's whitespace beginning at pChar.
///////////////////////////////////////////////////////////////////////////////
int CAdapt_ItDoc::ParseWhiteSpace(wxChar *pChar)
{
	int	length = 0;
	wxChar* ptr = pChar;
	while (IsWhiteSpace(ptr))
	{
		length++;
		ptr++;
	}
	return length;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		the number of filtering sfm characters parsed
/// \param		wholeMkr	-> the whole marker (including backslash) to be parsed
/// \param		pChar		-> pointer to the backslash character at the beginning of the marker
/// \param		pBufStart	-> pointer to the start of the buffer
/// \param		pEnd		-> pointer at the end of the buffer
/// \remarks
/// Called from: the Doc's TokenizeText().
/// Parses through the filtering marker beginning at pChar (the initial backslash).
/// Upon entry pChar must point to a filtering marker determined by a prior call to 
/// IsAFilteringSFM(). Parsing will include any embedded (inline) markers belonging to the 
/// parent marker.
/// BEW 9Sep10 removed need for papram pBufStart, since only IsMarker() used to use it as
/// its second param and with docVersion 5 changes that become unnecessary, so for now
/// I've resorted to the identity assignment hack to avoid the compiler warning
///////////////////////////////////////////////////////////////////////////////
int CAdapt_ItDoc::ParseFilteringSFM(const wxString wholeMkr, wxChar *pChar, 
									wxChar *pBufStart, wxChar *pEnd)
{
	pBufStart = pBufStart; // a hack to avoid compiler warning
	// whm added 10Feb2005 in support of USFM and SFM Filtering support
	// BEW ammended 10Jun05 to have better parse termination criteria
	// Used in TokenizeText(). For a similar named function used
	// only in DoMarkerHousekeeping(), see ParseFilteredMarkerText().
	// Upon entry pChar must point to a filtering marker determined
	// by prior call to IsAFilteringSFM().
	// ParseFilteringSFM advances the ptr until one of the following
	// conditions is true:
	// 1. ptr == pEnd (end of buffer is reached).
	// 2. ptr points just past a corresponding end marker.
	// 3. ptr points to a subsequent non-inLine and non-end marker. This
	//    means that the "content markers"
	// whm ammended 30Apr05 to include "embedded content markers" in
	// the parsed filtered marker, i.e., any \xo, \xt, \xk, \xq, and 
	// \xdc that follow the marker to be parsed will be included within
	// the span that is parsed. The same is true for any footnote content
	// markers (see notes below).
	int	length = 0;
	int endMkrLength = 0;
	wxChar* ptr = pChar;
	if (ptr < pEnd)
	{
		// advance pointer one to point past wholeMkr's initial backslash
		length++;
		ptr++;
	}
	while (ptr != pEnd)
	{
		//if (IsMarker(ptr,pBufStart)) BEW changed 7Sep10
		if (IsMarker(ptr))
		{
			if (IsCorresEndMarker(wholeMkr,ptr,pEnd))
			{
				// it is the corresponding end marker so parse it
				// Since end markers may not be followed by a space we cannot
				// use ParseMarker to reliably parse the endmarker, so
				// we'll just add the length of the end marker to the length
				// of the filtered text up to the end marker
				endMkrLength = wholeMkr.Length() + 1; // add 1 for *
				return length + endMkrLength;
			}
			else if (IsInLineMarker(ptr, pEnd) && *(ptr + 1) == wholeMkr.GetChar(1))
			{
				; // continue parsing
				// We continue incrementing ptr past all inLine markers following a 
				// filtering marker that start with the same initial letter (after 
				// the backslash) since those can be assumed to be "content markers"
				// embedded within the parent marker. For example, if our filtering
				// marker is the footnote marker \f, any of the footnote content 
				// markers \fr, \fk, \fq, \fqa, \ft, \fdc, \fv, and \fm that happen to 
				// follow \f will also be filtered. Likewise, if the cross reference
				// marker \x if filtered, any inLine "content" markers such as \xo, 
				// \xt, \xq, etc., that might follow \x will also be subsumed in the
				// parse and therefore become filtered along with the \x and \x*
				// markers. The check to match initial letters of the following markers
				// with the parent marker should eliminate the possibility that another
				// unrelated inLine marker (such as \em emphasis) would accidentally
				// be parsed over
			}
			else
			{
				wxString bareMkr = GetBareMarkerForLookup(ptr);
				wxASSERT(!bareMkr.IsEmpty());
				USFMAnalysis* pAnalysis = LookupSFM(bareMkr);
				if (pAnalysis)
				{
					if (pAnalysis->textType == none)
					{
						; // continue parsing
						// We also increment ptr past all inLine markers following a filtering
						// marker, if those inLine markers are ones which pertain to character
						// formatting for a limited stretch, such as italics, bold, small caps,
						// words of Jesus, index entries, ordinal number specification, hebrew or
						// greek words, and the like. Currently, these are: ord, bd, it, em, bdit,
						// sc, pro, ior, w, wr, wh, wg, ndx, k, pn, qs -- and their corresponding
						// endmarkers (not listed here) -- this list is specific to Adapt It, it
						// is not a formally defined subset within the USFM standard
					}
					else
					{
						break;	// it's another marker other than corresponding end marker, or
								// a subsequent inLine marker or one with TextType none, so break 
								// because we are at the end of the filtered text.
					}
				}
				else
				{
					// pAnalysis is null, this indicates either an unknown marker, or a marker from
					// a different SFM set which is not in the set currently active - eiher way, we
					// treat these as inLine == FALSE, and so such a marker halts parsing
					break;
				}
			}
		}
		length++;
		ptr++;
	}
	return length;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		the number of numeric characters parsed
/// \param		pChar		-> pointer to the first numeric character
/// \remarks
/// Called from: the Doc's TokenizeText(), DoMarkerHousekeeping(), and 
/// DoExportInterlinearRTF().
/// Parses through the number until whitespace is encountered (generally a newline)
///////////////////////////////////////////////////////////////////////////////
int CAdapt_ItDoc::ParseNumber(wxChar *pChar)
{
	wxChar* ptr = pChar;
	int length = 0;
	while (!IsWhiteSpace(ptr))
	{
		ptr++;
		length++;
	}
	return length;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if the marker being pointed to by pChar is a verse marker, FALSE otherwise.
/// \param		pChar		-> pointer to the first character to be examined (a backslash)
/// \param		nCount		<- returns the number of characters forming the marker
/// \remarks
/// Called from: the Doc's TokenizeText() and DoMarkerHousekeeping(), 
/// DoExportInterlinearRTF() and DoExportSrcOrTgtRTF().
/// Determines if the marker at pChar is a verse marker. Intelligently handles verse markers
/// of the form \v and \vn.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsVerseMarker(wxChar *pChar, int& nCount)
// version 1.3.6 and onwards will accomodate Indonesia branch's use
// of \vn as the marker for the number part of the verse (and \vt for
// the text part of the verse - AnalyseMarker() handles the latter)
{
	wxChar* ptr = pChar;
	ptr++;
	if (*ptr == _T('v'))
	{
		ptr++;
		if (*ptr == _T('n'))
		{
			// must be an Indonesia branch \vn 'verse number' marker
			// if white space follows
			ptr++;
			nCount = 3;
		}
		else
		{
			nCount = 2;
		}
		return IsWhiteSpace(ptr);
	}
	else
		return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar points at a \~FILTER (beginning filtered material marker)
/// \param		pChar		-> a pointer to the first character to be examined (a backslash)
/// \param		pEnd		-> a pointer to the end of the buffer
/// \remarks
/// Called from: the Doc's GetMarkersAndTextFromString() 
/// Determines if the marker being pointed at is a \~FILTER marking the beginning of filtered
/// material.
/// BEW 24Mar10 no changes needed for support of doc version 5
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsFilteredBracketMarker(wxChar *pChar, wxChar* pEnd)
{
	// whm added 10Feb2005 in support of USFM and SFM Filtering support
	// determines if pChar is pointing at the filtered text begin bracket \~FILTER
	wxChar* ptr = pChar;
	for (int i = 0; i < (int)wxStrlen_(filterMkr); i++) //_tcslen
	{
		if (ptr + i >= pEnd)
			return FALSE;
		if (*(ptr + i) != filterMkr[i])
			return FALSE;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar points at a \~FILTER* (ending filtered material marker)
/// \param		pChar		-> a pointer to the first character to be examined (a backslash)
/// \param		pEnd		-> a pointer to the end of the buffer
/// \remarks
/// Called from: the Doc's GetMarkersAndTextFromString().
/// Determines if the marker being pointed at is a \~FILTER* marking the end of filtered
/// material.
/// BEW 24Mar10 no changes needed for support of doc version 5
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsFilteredBracketEndMarker(wxChar *pChar, wxChar* pEnd)
{
	// whm added 18Feb2005 in support of USFM and SFM Filtering support
	// determines if pChar is pointing at the filtered text end bracket \~FILTER*
	wxChar* ptr = pChar;
	for (int i = 0; i < (int)wxStrlen_(filterMkrEnd); i++) //_tcslen
	{
		if (ptr + i >= pEnd)
			return FALSE;
		if (*(ptr + i) != filterMkrEnd[i])
			return FALSE;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		the number of characters parsed
/// \param		pChar		-> a pointer to the first character to be parsed (a backslash)
/// \remarks
/// Called from: the Doc's ReconstituteAfterFilteringChange(), GetWholeMarker(), TokenizeText(),
/// DoMarkerHousekeeping(), IsEndingSrcPhrase(), ContainsMarkerToBeFiltered(), 
/// RedoNavigationText(), GetNextFilteredMarker(), the View's FormatMarkerBufferForOutput(),
/// DoExportSrcOrTgtRTF(), FindFilteredInsertionLocation(), IsFreeTranslationEndDueToMarker(),
/// ParseFootnote(), ParseEndnote(), ParseCrossRef(), ProcessAndWriteDestinationText(),
/// ApplyOutputFilterToText(), ParseMarkerAndAnyAssociatedText().
/// Parses through to the end of a standard format marker.
/// Caution: This function will fail unless the marker pChar points at is followed
/// by whitespace of some sort - a potential crash problem if ParseMarker is used for parsing
/// markers in local string buffers; insure the buffer ends with a space so that if an end
/// marker is at the end of a string ParseMarker won't crash (TCHAR(0) won't help at the end
/// of the buffer here because _istspace which is called from IsWhiteSpace() only recognizes
/// 0x09 ?0x0D or 0x20 as whitespace for most locales.
/// BEW 1Feb11, added test for forbidden marker characters using app::m_forbiddenInMarkers
///////////////////////////////////////////////////////////////////////////////
int CAdapt_ItDoc::ParseMarker(wxChar *pChar)
{
    // whm Note: Caution: This function will fail unless the marker pChar points at is
    // followed by whitespace of some sort - a potential crash problem if ParseMarker is
    // used for parsing markers in local string buffers; insure the buffer ends with a
    // space so that if an end marker is at the end of a string ParseMarker won't crash
    // (TCHAR(0) won't help at the end of the buffer here because _istspace which is called
    // from IsWhiteSpace() only recognizes 0x09 ?0x0D or 0x20 as whitespace for most
    // locales.
    // whm modified 24Nov07 added the test to end the while loop if *ptr points to a null
    // char. Otherwise in the wx version a buffer containing "\fe" could end up with a
    // length of something like 115 characters, with an embedded null char after the third
    // character in the string. This would foul up subsequent comparisons and Length()
    // checks on the string, resulting in tests such as if (mkrStr == _T("\fe")) failing
    // even though mkrStr would appear to contain the simple string "\fe".
    // I still consider ParseMarker as designed to be dangerous and think it appropriate to
    // TODO: add a wxChar* pEnd parameter so that tests for the end of the buffer can be
    // made to prevent any such problems. The addition of the test for null seems to work
    // for the time being.
    // whm ammended 7June06 to halt if another marker is encountered before whitespace
    // BEW ammended 11Oct10 to halt if a closing bracket ] follows the (end)marker
	int len = 0;
	wxChar* ptr = pChar; // was wchar_t
	wxChar* pBegin = ptr;
	while (!IsWhiteSpace(ptr) && *ptr != _T('\0') && gpApp->m_forbiddenInMarkers.Find(*ptr) == wxNOT_FOUND)
	{
		if (ptr != pBegin && (*ptr == gSFescapechar || *ptr == _T(']'))) 
			break; 
		ptr++;
		len++;
		if (*(ptr -1) == _T('*')) // whm ammended 17May06 to halt after asterisk (end marker)
			break;
	}
	return len;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a wxString representing the marker being pointe to by pChar
/// \param		pChar		-> a pointer to the first character to be examined (a backslash)
/// \param		pEnd		-> a pointer to the end of the buffer
/// \remarks
/// Called from: the Doc's GetMarkersAndTextFromString().
/// Returns the whole marker by parsing through an existing marker until either whitespace is
/// encountered or another backslash is encountered.
/// BEW fixed 10Sep10, the last test used forward slash, and should be backslash
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::MarkerAtBufPtr(wxChar *pChar, wxChar *pEnd) // whm added 18Feb05
{
	int len = 0;
	wxChar* ptr = pChar;
	//while (ptr < pEnd && !IsWhiteSpace(ptr) && *ptr != _T('/'))
	while (ptr < pEnd && !IsWhiteSpace(ptr) && *ptr != _T('\\'))
	{
		ptr++;
		len++;
	}
	return wxString(pChar,len);
}

// return TRUE if the quotation character at pChar is either " or '
bool CAdapt_ItDoc::IsStraightQuote(wxChar* pChar)
{
	if (gpApp->m_bDoubleQuoteAsPunct)
	{
		if (*pChar == _T('\"')) return TRUE; // ordinary double quote
	}
	if (gpApp->m_bSingleQuoteAsPunct)
	{
		if (*pChar == _T('\'')) return TRUE; // ordinary single quote
	}
	return FALSE;
}

bool CAdapt_ItDoc::IsFootnoteInternalEndMarker(wxChar* pChar)
{
	wxString endMkr = GetWholeMarker(pChar);
	if (endMkr[0] != gSFescapechar)
		return FALSE;
	if (endMkr == _T("\\fig*"))
		return FALSE;
	wxString reversed = MakeReverse(endMkr);
	if (reversed[0] != _T('*'))
		return FALSE;
	else
	{
		reversed = reversed.Mid(1); // remove initial *
		endMkr = MakeReverse(reversed); // now \f should be first 2 characters
									// if it is an internal footnote endmarker
		int length = endMkr.Len();
		if (length < 3)
			return FALSE; // it could be \f at best, and that is not enough
		if (endMkr.Find(_T("\\f")) != 0)
			return FALSE; // \f has to be at the start of the marker, to qualify
		endMkr = endMkr.Mid(2); // chop off the initial \f
		if (endMkr.Len() > 0)
		{
            // if it has any content left, then it is an internal footnote endmarker,
            // any other value disqualifies it
			return TRUE;
		}
	}
	return FALSE; 
}

bool CAdapt_ItDoc::IsCrossReferenceInternalEndMarker(wxChar* pChar)
{
	wxString endMkr = GetWholeMarker(pChar);
	if (endMkr[0] != gSFescapechar)
		return FALSE;
	wxString reversed = MakeReverse(endMkr);
	if (reversed[0] != _T('*'))
		return FALSE;
	else
	{
		reversed = reversed.Mid(1); // remove initial *
		endMkr = MakeReverse(reversed); // now \x should be first 2 characters
							// if it is an internal crossReference endmarker
		int length = endMkr.Len();
		if (length < 3)
			return FALSE; // it could be \x at best, and that is not enough
		if (endMkr.Find(_T("\\x")) != 0)
			return FALSE; // \x has to be at the start of the marker, to qualify
		endMkr = endMkr.Mid(2); // chop off the initial \x
		if (endMkr.Len() > 0)
		{
			// if it has any content left, then it is an internal crossReference
			// endmarker, any other value disqualifies it
			return TRUE;
		}
	}
	return FALSE;
}

bool CAdapt_ItDoc::HasMatchingEndMarker(wxString& mkr, CSourcePhrase* pSrcPhrase)
{
	wxString endMkrs = pSrcPhrase->GetEndMarkers();
	if (gpApp->gCurrentSfmSet == PngOnly)
	{
        // check for one of the 'footnote end' markers, the only endmarkers in the PNG 1998
        // marker set
		if (endMkrs.IsEmpty())
		{
			return FALSE;
		}
		if (endMkrs == _T("\\fe") || endMkrs == _T("\\F"))
		{
			// it's one of those two, so if mkr is \f, we've got a match
			wxString mkrPlusSpace = mkr + _T(' ');
			if (mkrPlusSpace == _T("\\f "))
			{
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
	}
	// the UsfmOnly set must be currently in use; the matching endmarker, if it exists,
	// must be in m_endMarkers and it will be the last one if that member stores more than
	// one
	wxString endmarkers = pSrcPhrase->GetEndMarkers();
	if (endmarkers.IsEmpty())
	{
		return FALSE; // empty m_endMarkers member, so no match is possible
	}
	wxString wholeEndMkr = GetLastMarker(endmarkers);
	wxASSERT(!wholeEndMkr.IsEmpty());
	if (mkr + _T('*') == wholeEndMkr)
	{
		// we have a match
		return TRUE;
	}
	return FALSE; // no match
}

// This is an overload for the one below it with wxChar* pChar as param. str must commence
// with the endmarker being tested. I don't have a use for this overloaded version as yet.
/*
bool CAdapt_ItDoc::IsFootnoteOrCrossReferenceEndMarker(wxString str)
{
	const wxChar* pBuf = str.GetData();
	wxChar* pChar = (wxChar*)pBuf;
	return IsFootnoteOrCrossReferenceEndMarker(pChar);
}
*/

// NOTE: the endmarker for endnote is included in the test, so while the name of this
// function suggests only \f* and \x* return TRUE, \fe* will also return TRUE
// BEW 7Dec10, added check for \fe or \f when SFM set is PngOnly
bool CAdapt_ItDoc::IsFootnoteOrCrossReferenceEndMarker(wxChar* pChar)
{
	wxString endMkr = GetWholeMarker(pChar);
	if (gpApp->gCurrentSfmSet == PngOnly)
	{
		// check for 'footnote end' markers, the only endmarkers in the PNG 1998
		// marker set
		if (endMkr == _T("\\fe") || endMkr == _T("\\F"))
			return TRUE;
	}
	if (endMkr == _T("\\fig*"))
		return FALSE;
	if (endMkr == _T("\\fe*"))
	{
		// we include a test for the endmarker of an endnote in this function, because we
		// want the handling for \f* and \fe* to be the same  - either, if found, should
		// be stored in m_endMarkers, and either can have outer punctuation following it
		return TRUE;
	}
	if (endMkr[0] != gSFescapechar)
		return FALSE;
	wxString reversed = MakeReverse(endMkr);
	if (reversed[0] != _T('*'))
		return FALSE;
	else
	{
		reversed = reversed.Mid(1); // remove initial *
		endMkr = MakeReverse(reversed); // now \x or \f should be first 2
										// characters if it is to quality
		int length = endMkr.Len();
		if (length > 2)
			return FALSE; // what remains is more than \x or \f, so disqualified
		if ((endMkr.Find(_T("\\x")) == 0) || (endMkr.Find(_T("\\f")) == 0))
			return TRUE; // what remains is either \x or \f, so it qualifies
	}
	return FALSE;
}


///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing to an opening quote mark
/// \param		pChar		-> a pointer to the character to be examined
/// \remarks
/// Called from: the Doc's ParseWord(), the View's DetachedNonQuotePunctuationFollows().
/// Determines is the character being examined is some sort of opening quote mark. An
/// opening quote mark may be a left angle wedge <, a Unicode opening quote char L'\x201C'
/// or L'\x2018', or an ordinary quote or double quote or char 145 or 147 in the ANSI set.
/// Assumes that " is defined as m_bDoubleQuoteAsPunct in the App and/or that ' is defined
/// as m_bSingleQuoteAsPunct in the App.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsOpeningQuote(wxChar* pChar)
{
	// next three functions added by BEW on 17 March 2005 for support of
	// more clever parsing of sequences of quotes with delimiting space between
	// -- these are to be used in a new version of ParseWord(), which will then
	// enable the final couple of hundred lines of code in TokenizeText() to be
	// removed
	// include legacy '<' as in SFM standard, as well as smart quotes
	// and normal double-quote, and optional single-quote
	if (*pChar == _T('<')) return TRUE; // left wedge
#ifdef _UNICODE
	if (*pChar == L'\x201C') return TRUE; // unicode Left Double Quotation Mark
	if (*pChar == L'\x2018') return TRUE; // unicode Left Single Quotation Mark
#else // ANSI version
	if ((unsigned char)*pChar == 147) return TRUE; // Left Double Quotation Mark
	if ((unsigned char)*pChar == 145) return TRUE; // Left Single Quotation Mark
#endif
	if (gpApp->m_bDoubleQuoteAsPunct)
	{
		if (*pChar == _T('\"')) return TRUE; // ordinary double quote
	}
	if (gpApp->m_bSingleQuoteAsPunct)
	{
		if (*pChar == _T('\'')) return TRUE; // ordinary single quote
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing to a " or to a ' (apostrophe) quote mark
/// \param		pChar		-> a pointer to the character to be examined
/// \remarks
/// Called from: the Doc's ParseWord().
/// Assumes that " is defined as m_bDoubleQuoteAsPunct in the App and/or that ' is defined
/// as m_bSingleQuoteAsPunct in the App.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsAmbiguousQuote(wxChar* pChar)
{
	if (gpApp->m_bDoubleQuoteAsPunct)
	{
		if (*pChar == _T('\"')) return TRUE; // ordinary double quote
	}
	if (gpApp->m_bSingleQuoteAsPunct)
	{
		if (*pChar == _T('\'')) return TRUE; // ordinary single quote (ie. apostrophe)
	}
	return FALSE;
}

// BEW 15Dec10, changes needed to handle PNG 1998 marker set's \fe and \F
bool CAdapt_ItDoc::IsTextTypeChangingEndMarker(CSourcePhrase* pSrcPhrase)
{
	if (gpApp->gCurrentSfmSet == PngOnly || gpApp->gCurrentSfmSet == UsfmAndPng)
	{
		// in the PNG 1998 set, there is no marker for endnotes, and cross references were
		// not included in the standard but inserted manually from a separate file, and so
		// there is no \x nor any endmarker for a cross reference either; there were only
		// two marker synonyms for ending a footnote, \fe or \F
		wxString fnoteEnd1 = _T("\\fe");
		wxString fnoteEnd2 = _T("\\F");
		wxString endmarkers = pSrcPhrase->GetEndMarkers();
		if (endmarkers.IsEmpty())
			return FALSE;
		if (endmarkers.Find(fnoteEnd1) != wxNOT_FOUND)
		{
			return TRUE;
		}
		else if (endmarkers.Find(fnoteEnd2) != wxNOT_FOUND)
		{
			return TRUE;
		}
	}
	else
	{
		wxString fnoteEnd = _T("\\f*");
		wxString endnoteEnd = _T("\\fe*");
		wxString crossRefEnd = _T("\\x*");
		wxString endmarkers = pSrcPhrase->GetEndMarkers();
		if (endmarkers.IsEmpty())
			return FALSE;
		if (endmarkers.Find(fnoteEnd) != wxNOT_FOUND)
		{
			return TRUE;
		}
		else if (endmarkers.Find(endnoteEnd) != wxNOT_FOUND)
		{
			return TRUE;
		}
		else if (endmarkers.Find(crossRefEnd) != wxNOT_FOUND)
		{
			return TRUE;
		}
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing to a closing curly quote mark
/// \param		pChar		-> a pointer to the character to be examined
/// \remarks
/// Called from: the Doc's ParseWord().
/// Determines is the character being examined is some sort of non-straight closing quote
/// mark, that is, not one of ' or ". So a closing curly quote mark may be a right angle
/// wedge >, or a Unicode closing quote char L'\x201D' or L'\x2019'.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsClosingCurlyQuote(wxChar* pChar)
{
	// include legacy '>' as in SFM standard, as well as smart quotes
	// but not normal double-quote, nor single-quote
	if (*pChar == _T('>')) return TRUE; // right wedge
#ifdef _UNICODE
	if (*pChar == L'\x201D') return TRUE; // unicode Right Double Quotation Mark
	if (*pChar == L'\x2019') return TRUE; // unicode Right Single Quotation Mark
#else // ANSI version
	if ((unsigned char)*pChar == 148) return TRUE; // Right Double Quotation Mark
	if ((unsigned char)*pChar == 146) return TRUE; // Right Single Quotation Mark
#endif
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing to a closing quote mark
/// \param		pChar		-> a pointer to the character to be examined
/// \remarks
/// Called from: the Doc's ParseWord().
/// Determines is the character being examined is some sort of closing quote mark.
/// An closing quote mark may be a right angle wedge >, a Unicode closing quote char L'\x201D'
/// or L'\x2019', or an ordinary quote or double quote or char 146 or 148 in the ANSI set.
/// Assumes that " is defined as m_bDoubleQuoteAsPunct in the App and/or that ' is defined
/// as m_bSingleQuoteAsPunct in the App.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsClosingQuote(wxChar* pChar)
{
	// include legacy '>' as in SFM standard, as well as smart quotes
	// and normal double-quote, and optional single-quote
	if (*pChar == _T('>')) return TRUE; // right wedge
#ifdef _UNICODE
	if (*pChar == L'\x201D') return TRUE; // unicode Right Double Quotation Mark
	if (*pChar == L'\x2019') return TRUE; // unicode Right Single Quotation Mark
#else // ANSI version
	if ((unsigned char)*pChar == 148) return TRUE; // Right Double Quotation Mark
	if ((unsigned char)*pChar == 146) return TRUE; // Right Single Quotation Mark
#endif
	if (gpApp->m_bDoubleQuoteAsPunct)
	{
		if (*pChar == _T('\"')) return TRUE; // ordinary double quote
	}
	if (gpApp->m_bSingleQuoteAsPunct)
	{
		if (*pChar == _T('\'')) return TRUE; // ordinary single quote
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param	span		           -> span of characters extracted from the text buffer
///                                   by the FindParseHaltLocation() function; we parse this
/// \param  wordProper             <- the word itself [see a) below]
/// \param  firstFollPuncts	       <- any punctuation characters (out-of-place ones, [see b)
///                                   below])
/// \param	nEndMkrsCount          <- how many inline binding endmarkers there are in the
///                                   span string [this is known to the caller beforehand]
/// \param	inlineBindingEndMarkers <- one of more inline binding endmarkers following the
///                                    wordProper in the span string
/// \param  secondFollPuncts        <- normal "following punctuation" which, if an inline
///                                    binding endmarker is present, should (if the USFM
///                                    markup is correctly done) follow the marker [Note:
///                                    if there is no inline binding endmarker present,
///                                    only firstFollPuncts will have punctuation chars in
///                                    it, and they will be in standard position (of course)]
/// \param  ignoredWhiteSpaces      <- one or more characters of whitespace - because
///                                    Adapt It normalizes most \n and \r characters out of
///                                    the data (using space instead) typically this will just
///                                    be one of more spaces, but we don't rely on that being
///                                    true
/// \param  wordBuildersForPostWordLoc <- For storing one or more wordbuilding characters
///                                    which are at the end of m_follPunct because they were
///                                    formerly punctuation but the user has changed the
///                                    punctuation set and now they are word-building ones
///                                    (the caller will restore them to their word-final
///                                    location)
/// \param  spacelessPuncts          -> the (spaceless) punctuation set being used (usually
///                                    src punctuation, but target punctuation can be passed
///                                    if we want to parse target text for some reason - in
///                                    which case span should contain target text) Note, if
///                                    we want space to be part of the punctuation set, we
///                                    must add it here explicitly in a local string before
///                                    doing anything else
/// \remarks
/// Called from: IsFixedSpaceAhead()
/// FindParseHaltLocation() is used within IsFixedSpaceAhead() (itself within ParseWord()
/// called from TokenizeText()) to extract characters from the input buffer until a
/// halting location is reached - which could be at a ~ fixedspace marker, or if certain
/// other post-word data is encountered. That defines a span of characters which commence
/// with the characters of the word being parsed, but which could end with quite complex
/// possibilities. This ParseSpanBackwards() function parsed from the end of that span,
/// backwards towards its start, extracting each information type which it returns via the
/// signature's parameters. The material being parsed, in storage order (in a RTL script
/// this would be rendered RTL, not LTR of course, but both are stored in LTR order) may
/// be this:
/// a) the word proper (it may contain embedded punctuation which must remain invisible to
/// our parsers, that's why we parse backwards - we expect to reach characters at the end
/// of the word before the backwards parse has a chance to hit an embedded punct character)
/// b) out-of-place (for canonical USFM markup) following punctuation (which may contain
/// embedded space - such as for closing curly quote sequences)
/// c) inline binding endmarker(s) (we allow for more than one - we'll extract them as a
/// sequence and not try to remove any unneeded spaces between them - they normally would
/// have no space between any such pair) -- the FindParseHaltLocation() knows how many such
/// markers it scanned over to get to the halt location, and it returned a count for that,
/// and so we pass in that count value in the nEndMkrsCount param
/// d) more following punctuation (this is in the canonical location if there is an inline
/// binding marker present - punctuation should only follow such an endmarker in good USFM
/// markup, never precede it -- so the caller will coalesce the out of place puncts with
/// the in place puncts, to restore good USFM markup [white space between word and puncts,
/// keep it if present, because some languages require space between word and puncts at
/// either end.
/// e) some white space - this would be ignorable, and we'll return it so that the caller
/// can get it's iterator position set correctly, but the caller will then just ignore any
/// such white space returned
/// BEW created 11Oct10 (actually 27Jan11), to support the improved USFM parser build into
/// doc version 5 
/// BEW 2Feb11, added a string to signature for storing punctuation characters that have
/// changed their status to being word-building
//////////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::ParseSpanBackwards(wxString& span, wxString& wordProper, 
		wxString& firstFollPuncts, int nEndMkrsCount, wxString& inlineBindingEndMarkers,
		wxString& secondFollPuncts, wxString& ignoredWhiteSpaces,
		wxString& wordBuildersForPostWordLoc, wxString& spacelessPuncts)
{
	// initialize
	wordProper.Empty(); firstFollPuncts.Empty(); inlineBindingEndMarkers.Empty();
	secondFollPuncts.Empty(); ignoredWhiteSpaces.Empty(); 
	wordBuildersForPostWordLoc.Empty(); // potentially used when parsing the first or
										// second word of a conjoined pair, or when
										// parsing a non-conjoined word
	// reverse the string
	if (span.IsEmpty())
	{
		wxBell();
		wxMessageBox(_T("Error: in ParseSpanBackwards(), input string 'span' is empty"));
		return;
	}
	int length = span.Len();
	wxString str = MakeReverse(span);

	// check we have a non-empty punctuation characters set - if it's empty, we can skip
	// parsing for punctuation characters
	bool bPunctSetNonEmpty = TRUE;
	wxString punctSet; punctSet.Empty();
	if (spacelessPuncts.IsEmpty())
	{
		bPunctSetNonEmpty = FALSE;
	}
	else
	{
		// we need space to be in the punctuation set, so add it to a local string and use the
		// local string thereafter; we don't add it if there are no punctuation characters set
		punctSet = spacelessPuncts + _T(' ');
	}

	// get access to the wxString's buffer - then iterate across it, collecting the
	// substrings as we go
	const wxChar* pBuffer = str.GetData();
	wxChar* p = (wxChar*)pBuffer;
	wxChar* pEnd = p + length;
	wxChar* pStartHere = p;
	int punctsLen2 = 0;
	int punctsLen1 = 0;
	int ignoredWhitespaceLen = 0;
	int bindingEndMkrsLen = 0;

	// first get any ignorable whitespace
	ignoredWhitespaceLen = ParseWhiteSpace(p);
	if (ignoredWhitespaceLen > 0)
	{
		wxString ignoredSpaceRev(p, p + ignoredWhitespaceLen);
		ignoredWhiteSpaces = MakeReverse(ignoredSpaceRev); // normal order
		pStartHere = p + ignoredWhitespaceLen; // advance starting location
	}
    // next, any punctuation characters -- we'll put them in secondFollPuncts string
	// whether of not nEndMkrsCount is zero (because this is where we'd expect good USFM
	// markup to have them); skip this step if there are no punctuation characters defined
	p = pStartHere;
	wxString puncts; puncts.Empty();
	if (bPunctSetNonEmpty)
	{
		// allow \n and \r to be parsed over too (ie. whitespace, not just space),
		// space character is already in punctSet, so no need to add an explicit test;
		// Note, the loop will also end if it encounters what used to be a word-building
		// character which, due to user changing punctuation set, it became a punctuation
		// character and so got pulled off the word's end and stored in m_follPunct, but
		// subsequent to that the user again changed the punctuation set making it back
		// into a word building character - so that it is still in m_follPunct at its end
		// (it may be the only character in m_follPunct), and so we will have to test for
		// this and store it and any like it in a special string,
		// wordBuildersForPostWordLoc, to return such characters to the caller for
		// placement there back at the end of the parsed word
		while (p < pEnd)
		{
			if (punctSet.Find(*p) != wxNOT_FOUND ||  *p == _T('\n') || *p == _T('\r'))
				puncts += *p++;
			else
				break;
		}
        // add the puncts to secondFollPuncts, if any were found -- note, there could be a
        // space (it's not necessarily bad USFM markup, some languages require it) at the
        // end of the (reversed)substring -- we'll collect it & retain it if present
		if (!puncts.IsEmpty())	
		{
			secondFollPuncts = MakeReverse(puncts); // normal order
			punctsLen2 = secondFollPuncts.Len();
			puncts.Empty(); // in case we reuse it for a subsequent inline binding endmarker
			pStartHere = pStartHere + punctsLen2; // advance starting location
		}
	}
	// now, however many (reversed) inline binding endmarkers were found to be present --
	// since any such are reversed, and because there are no inline markers in the PNG
	// 1998 SFM marker set, we know that asterisk * must be the first character
	// encountered if a marker is present...
    // since we parse backwards we could use the version of ParseMarker() that is in
    // helpers.cpp, in a loop, because it checks for initial * and so can find reversed
    // USFM endmarkers, however the easiest way is to assume that the nEndMkrsCount value
    // passed in is correct, and just use FindFromPos() in a loop - searching for a
    // backslash on each iteration. Until the latter proves to be non-robust, that will
    // suffice
	// Note: we know that the marker or markers to be parsed next are all inline
	// binding endmarkers - that was verified in the prior call of
	// FindParseHaltLocation() which did the requisite test and set nEndMkrsCount
	p = pStartHere;

    // it's possible that changed punctuation resulted in a word-final character moving to
    // be in m_follPunct; this is benign except when there was also an inline binding
    // endmarker present - because Adapt It will restore the character to after the inline
    // marker, thinking it is to remain as punctuation, and if it is now no longer in the
    // punctuation set being used, then it needs to be stored for the caller to process it,
    // and the parsing point set to follow it before further parsing takes place. Test and
    // do that now. There could be more than one. 
	wxString nowWordBuilding; nowWordBuilding.Empty();
	bool bStoredSome = FALSE;
	if (nEndMkrsCount > 0 && *p != _T('*') && punctSet.Find(*p) == wxNOT_FOUND)
	{
		while (*p != _T('*') && punctSet.Find(*p) == wxNOT_FOUND)
		{
			bStoredSome = TRUE;
			nowWordBuilding += *p;
			p++;
			pStartHere = p;
		}
	}
	// any additional puncts which are between where p points and the * of the reversed
	// marker have to be taken to the end of the word - this will "bury" any such as
	// word-internal punctuation -- this is the cost we pay for refusing to generate a
	// pair of CSourcePhrase instances from a single instance when the user changes
	// punctuation settings - otherwise, we get potential messes, and this 'solution' is
	// the best compromise. 
	// To generate data to illustrate this, a sequence like \k extreme\k* is useful, 
    // make m and e become punctuation characters, then unmake e as a punctuation
    // (returning it to word-building status) -- when the reverse parse comes to the eme
    // 3-char sequence, the first e goes to the end of the word, but m continuing as
    // punctuation blocks the loop above, leaving 2-char sequence, me, before the * of the
    // reversed \k* endmarker. If we don't also move that "me" sequence to the end of the
    // word, the wxASSERT below would trip, and that "m" character would cause the
    // generation, of a second CSourcePhrase, and a rather unhelpful mess at that point in
    // the document. We have to get p pointing at * before we continue the parse.
	if (!nowWordBuilding.IsEmpty() && *p != _T('*'))
	{
		// grab and append the rest
		while (*p != _T('*'))
		{
			nowWordBuilding += *p;
			p++;
		}
	}
	if (!nowWordBuilding.IsEmpty())
	{
		wordBuildersForPostWordLoc = MakeReverse(nowWordBuilding); // return these to
				// IsFixedSpaceAhead() which in turn will return these to ParseWord() where,
                // if the string is not empty, they'll be appended to the word; ptr will
                // get updated in IsFixedSpaceAhead() I think, as probably will the len
                // value, if this function was called from there, else in
                // FinishOffConjoinedWordsParse() if called from the latter
    }
	// p should now be pointing at an * if nEndMkrsCount is not zero
#ifdef __WXDEBUG__
	if (nEndMkrsCount > 0)
	{
		wxASSERT(*p == _T('*'));
	}
#endif
	if (nEndMkrsCount > 0)
	{
		int lastPos = 0;
		wxString aReversedSpan(p, pEnd);
		int index;
		for (index = 0; index < nEndMkrsCount; index++)
		{
			// use the helpers.cpp function: int FindFromPos(const wxString& inputStr, 
			// const wxString& subStr, int startAtPos), it allows us to find several
			// instances of a substring within the string
			lastPos = FindFromPos(aReversedSpan,_T("\\"),lastPos);
			lastPos++; // include the backslash marker
		}
		wxString theBindingEndMarkers(p, p + lastPos);
		bindingEndMkrsLen = theBindingEndMarkers.Len();
		inlineBindingEndMarkers = MakeReverse(theBindingEndMarkers); // normal order
		pStartHere = p + bindingEndMkrsLen; // advance starting location
	}
    // next, any pre-marker punctuation characters in the unreversed string -- we'll put
    // them in firstFollPuncts string whether of not nEndMkrsCount is zero (because if
    // there was no inline binding endmarker just parsed, we'd have already collected all
    // the punctuation characters which follow the word; so any collected now must not have
    // been collected because of an intervening inline binding endmarker; but skip this
    // step if there are no punctuation characters defined
	p = pStartHere;
	if (bPunctSetNonEmpty)
	{
		// allow \n and \r to be parsed over too (ie. whitespace, not just space),
		// space character is already in punctSet, so no need to add an explicit test
		while (p < pEnd)
		{
			if (punctSet.Find(*p) != wxNOT_FOUND ||  *p == _T('\n') || *p == _T('\r'))
				puncts += *p++;
			else
				break;
		}
        // add the puncts to firstFollPuncts, if any were found -- note, there could be a
        // space (it's not necessarily bad USFM markup, some languages require it) at the
        // end of the (reversed)substring -- we'll collect it & retain it if present
		if (!puncts.IsEmpty())	
		{
			firstFollPuncts = MakeReverse(puncts); // normal order
			punctsLen1 = firstFollPuncts.Len();
			puncts.Empty();
			pStartHere = pStartHere + punctsLen1; // advance starting location
		}
	}

	
	// finally, what remains is the word proper (it could have embedded punctuation
	// 'invisible' to our parsing algorithms - it's invisible provided it has a
	// non-punctuation character both before and after it)
	p = pStartHere;
	wxString theReversedWord(p, pEnd);
	wordProper = MakeReverse(theReversedWord);

#ifdef __WXDEBUG__
	int wordLen = wordProper.Len();
	int storedRevertedPunctsLen = wordBuildersForPostWordLoc.Len();
	wxASSERT( bindingEndMkrsLen + wordLen + punctsLen1 + punctsLen2 + 
				ignoredWhitespaceLen + storedRevertedPunctsLen == length);
#endif
}


//////////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if ~ conjoins the word and the next, FALSE if there is no such
///             conjoining
/// \param		ptr			    <-> ref to the pointer to the next character to be parsed
///                                (it will be the first character of word about to be
///                                parsed)
/// \param      pEnd            -> pointer to first char past the end of the buffer
///                                (to ensure we don't overrun buffer end)
/// \param		pWdStart	    <- ptr value when function is just entered
/// \param	    pWdEnd          <- points at the first character past the last character
///                                of the first word parsed over
/// \param	    punctBefore     <- any punctuation (it can have space within, provided
///                                that it does not end with a space) which follows
///                                the word (*** this should always be empty, because the
///                                caller has already parsed over initial punctuation, and
///                                so this member never gets filled) -- remove later on ***
/// \param      endMkr          <- any inline binding endmarker, if present
/// \param      spacelessPuncts -> the (spaceless) punctuation set to be used herein
/// \remarks
/// Called from: ParseWord()
/// ******************************************************** NOTE *********************
/// NOTE: (this is now different, see next paragraph!) our parsing algorithms for scanning
/// words which are conjoined by ~ assumes that there is no punctuation within the word
/// proper - so xyz:abc would NOT be parsed as a single word; our general parser,
/// ParseWord() DOES handle this type of thing as a single word, but for word1~word2 type
/// of conjoining, word1 and word2 must have no internal punctuation. For the moment, we
/// feel this is a satisfactory simplification, because use of ~ in actual data is rare (no
/// known instances in a decade of Adapt It use), and so too is the use of punctuation as a
/// word-building character.
/// 
/// BEW 25Jan11: ***BIGGER NOTE**** I've left the above NOTE here, at the time it seemed a
/// reasonable simplification. But user's dynamic changes to punctuation settings proved
/// it be it's archilles heel. The early code used ScanExcluding() to parse over the word
/// proper, and if there is embedded punctuation (as there might be if a file is loaded
/// while inadequate punctuation settings were in effect), then the internal punctuation
/// can become visible to such a scan - and cause a disastrous result. The correct way to
/// handle scanning a word is to honour the fact that there may be internal punctuation
/// which must remain "unseen" by any scanning process - the way to do that is to scan
/// inward over punctuation from the start of the word, until a non-punct is reached, and
/// for scanning at the end of the word, reverse the word and scan inwards in the reversed
/// string until a non-punct is reached, and then undo the reversals. So to do these scans,
/// ScanIncluding() is to be used from either end, WE MUST NOT SCAN ACROSS THE WORD ITSELF
/// LOOKING FOR PUNCTUATION AT THE OTHER END OF IT. Instead, scan in from either end. That
/// gives us the problem of determining where the "other end" is before we can do the
/// reversal of what lies between and then do the scan in. If there is a ~ fixed space, we
/// can use that as defining the other end. But if there is no fixed space (~) present, we
/// have to define the other end as whitespace or a backslash (ie. ignore punctuation for
/// determining where the other end is). In support of these observations the code will be 
/// re-written below.
/// ******************************************************** END NOTE *****************
/// When the scanning ptr points at a word, we don't know whether the word will be a
/// singleton, or the first word of a pair conjoined by USFM ~ fixed space marker. We
/// support punctuation and inline binding markers before or after ~ too, so these
/// substrings may be present. The caller needs to know if it has to handle the word about
/// to be parsed as a conjoined pair, or not. To find this out, we first try to find if ~
/// is present. If it is, that's the dividing point between a conjoined pair (and we return
/// TRUE eventually). If there is no such character (we return FALSE eventually), it's not
/// a conjoined pair and the end of the word will be determined by scanning back from later
/// whitespace or a later marker. A ] character also is considered as an end point for the
/// word. We pass in references to the start and end locations for the word, etc, so that
/// the useful info we learn as we parse does not have to be reparsed in the caller. If
/// TRUE is returned, another function in the caller will be called in order to complete
/// the delimitation of the conjoined word pair, as far as the final character of the
/// second word. The ptr value returned must be, if ~ was detected, following the character
/// ~. If FALSE is returned, we've a normal word parsing, and the caller will only use the
/// pWdEnd value - resetting the caller's ptr variable to that location, since the caller
/// can successfully parse on from that point (this would mean throwing information away
/// about following punctuation, but that is a small matter because the latter is low
/// frequency in the text, and the caller will reparse that information quickly anyway).
/// BEW created 11Oct10, to support the improved USFM parser build into doc version 5
//////////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsFixedSpaceAhead(wxChar*& ptr, wxChar* pEnd, wxChar*& pWdStart, 
	wxChar*& pWdEnd, wxString& punctBefore, wxString& endMkr, 
	wxString& wordBuildersForPostWordLoc, wxString& spacelessPuncts)
{	
	wxChar* p = ptr; // scan with p, so that we can return a ptr value which is at
					 // the place we want the caller to pick up from (and that will
					 // be determined by what we find herein)
	wxString FixedSpace = _T("~");
	punctBefore.Empty();
	endMkr.Empty();
	pWdStart = ptr;
	// Find where ~ is, if present; we can't just call .Find() in the string defined by
	// ptr and pEnd, because it could contain thousands of words and a ~ may be many
	// hundreds of words ahead. Instead, we must scan ahead, parsing over any ignorable
	// white space, until we come to either ~, or non-ignorable whitespace, or a closing
	// bracket (]) - halting immediately before any such character. We need a function for
	// this and it can return, via its signature, what the specific halt condition was. If
	// we halt due to ] or whitespace, then we infer that we do not have conjoining of the
	// word being defined from the parse. We also may parse over an inline binding
	// endmarker, (perhaps more than one), these don't halt parsing - but we'll return the
	// info in the signature, along with a count of how many such markers we parsed over.
	wxChar* pHaltLoc = NULL;
	bool bFixedSpaceIsAhead = FALSE;
	bool bFoundInlineBindingEndMarker = FALSE;
	bool bFoundFixedSpaceMarker = FALSE;
	bool bFoundClosingBracket = FALSE;
	bool bFoundHaltingWhitespace = FALSE;
	int nFixedSpaceOffset = -1;
	int nEndMarkerCount = 0;
	pHaltLoc = FindParseHaltLocation( p, pEnd, &bFoundInlineBindingEndMarker, 
					&bFoundFixedSpaceMarker, &bFoundClosingBracket, 
					&bFoundHaltingWhitespace, nFixedSpaceOffset, nEndMarkerCount);
	bFixedSpaceIsAhead = bFoundFixedSpaceMarker;
	wxString aSpan(ptr,pHaltLoc); // this could be up to ~, or a [ or ], or a whitespace

	// we know whether or not we found a USFM fixedspace marker, what we do next depends
	// on whether we did or not
	wxString wordProper; // emptied at start of ParseSpanBackwards() call below
	wxString firstFollPuncts; // ditto
	wxString inlineBindingEndMarkers; // ditto
	wxString secondFollPuncts; // ditto
	wxString ignoredWhiteSpaces; // ditto
	// if ptr is already at pEnd (perhaps punctuation changes made a short word into all
	// puncts), then no point in calling ParseSpanBackwards() and generating a message
	// about an empty span, instead code to jump the call
	if (!aSpan.IsEmpty())
	{
		ParseSpanBackwards( aSpan, wordProper, firstFollPuncts, nEndMarkerCount, 
						inlineBindingEndMarkers, secondFollPuncts, 
						ignoredWhiteSpaces, wordBuildersForPostWordLoc, 
						spacelessPuncts);
	}
	else
	{
		wordProper.Empty();
		firstFollPuncts.Empty();
		nEndMarkerCount = 0;
		secondFollPuncts.Empty();
		ignoredWhiteSpaces.Empty();
		wordBuildersForPostWordLoc.Empty();
		inlineBindingEndMarkers.Empty();
	}
	// now use the info extracted to set the IsFixedSpaceAhead() param values ready
	// for returning to ParseWord()
	if (bFixedSpaceIsAhead)
	{
		// now use the info extracted to set the IsFixedSpaceAhead() param values ready
		// for returning to ParseWord()

		// first, pWdEnd -- this will be the length of wordProper after pWdStart
		pWdEnd = pWdStart + wordProper.Len();

		// second, punctuation which follows the word but precedes the fixed space; if
		// there is correct markup and there is an inline binding endmarker, it would all be
		// after that marker (or markers, if there is more than one here), but user markup
		// errors might have some or all before such a marker - if so, we move the
		// before-marker puncts to be immediately after the endmarker(s) and append
		// whatever is already after the endmarkers. We won't remove any initial whitespace
		// before the puncts, as that would be inappropriate -- some languages'
		// punctuation conventions are to have a space between the word and preceding or
		// following punctuation - so if there is space there, we must retain it 
		if (nEndMarkerCount == 0)
		{
			// all the punctuation is together in secondFollPuncts, if there is any at all
			if (secondFollPuncts.IsEmpty())
			{
				punctBefore.Empty();
			}
			else
			{
				punctBefore = secondFollPuncts;
			}
		}
		else
		{
			// handle any out-of-place puncts (will be in firstFollPuncts if there is any)
			// first, and then append any which follows the inline binding endmarker() to it
			if (firstFollPuncts.IsEmpty())
			{
				punctBefore.Empty();
			}
			else
			{
				punctBefore = firstFollPuncts;
			}
			if (!secondFollPuncts.IsEmpty())
			{
				punctBefore += secondFollPuncts;
			}
		}

		// third, the contents for endMkr; there could be space(s) in the string, and
		// they should be removed as they contribute nothing except to make things more
		// complicated than is necessary for rendinging the markup for publishing, so we
		// remove them
		endMkr.Empty();
		if (!inlineBindingEndMarkers.IsEmpty())
		{
			while (inlineBindingEndMarkers.Find(_T(' ')) != wxNOT_FOUND)
			{
				// remove all spaces, leaving only the one or more inline binding endmarkers
				inlineBindingEndMarkers.Remove(inlineBindingEndMarkers.Find(_T(' ')),1); 
			}
			endMkr = inlineBindingEndMarkers;
		}

		// last, since ~ is not in aSpan but immediately after it, set ptr to point past
		// the ~ fixedspace character
		ptr = ptr + nFixedSpaceOffset + 1;
	} // end of TRUE block for test: if (bFixedSpaceIsAhead)
	else
	{
		punctBefore.Empty(); // forget what we know about following punctuation
		endMkr.Empty(); // forget what we know about following inline binding endmarkers

		// first, pWdEnd -- this will be the length of wordProper after pWdStart
		// [ Note: wordProper will be shorter, if punctuation char(s) have just been
		// reverted to word-building ones - they are carried back to the caller in the
		// wordBuildersForPostWordLoc string, so as to to mess up the Len() counts here,
		// it is the caller that will append them to the word at the appropriate time]
		pWdEnd = pWdStart + wordProper.Len();

		// last, reset ptr to point where pWdEnd points -- for when we've not found any
		// fixed space, we let the caller do the final punctuation & endmarkers parsing etc
		ptr = pWdEnd;
		return FALSE; // tell the caller that no fixedspace was encountered
	} // end of else block for test: if (bFixedSpaceIsAhead)
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////
/// \return		                   nothing
/// \param		ptr			   <-> ref to the pointer to the next character to be parsed
///                                (it will be the first character after ~ character pair)
/// \param      pEnd            -> pointer to first char past the end of the buffer
///                                (to ensure we don't overrun buffer end)
/// \param		pWord2Start	    <- points at where 2nd of conjoined words starts (actual word)
/// \param	    pWord2End       <- points at the first character past the last character
///                                of the second of the conjoined words parsed over
/// \param	    punctAfter      <- any punctuation (it can have space within, provided
///                                that it does not end with a space) which follows ~ and
///                                precedes the second (conjoined) word
/// \param      bindingMkr      <- any inline binding beginmarker, if present
/// \remarks
/// Called from: ParseWord()
/// ******************************************************** NOTE *********************
/// NOTE: our parsing algorithms for scanning words which are conjoined by ~ assumes that
/// there is no punctuation within the word proper - so xyz:abc would NOT be parsed as a
/// single word; our general parser, ParseWord() DOES handle this type of thing as a single
/// word, but for word1~word2 type of conjoining, word1 and word2 must have no internal
/// punctuation. For the moment, we feel this is a satisfactory simplification, because
/// use of ~ in actual data is rare (no known instances in a decade of Adapt It use), and
/// so too is the use of punctuation as a word-building character.
/// ******************************************************** END NOTE *****************
/// On input, we know we have a USFM ~ marker conjoining two words (the words may have
/// punctuation before or after and inline binding marker and endmarker wrapping too), and
/// this function does the parsing from the character following ~ to the end of the
/// second word proper - but it does NOT attempt to parse into any following punctuation
/// or binding endmarker which may follow the second word - the caller will do that. When
/// ready to return, ptr must be set to point at whatever character following the end of
/// the second word. If the completion of the parse encounters any or all of, in the
/// following order, preceding punctuation before the second word (it may legally contain
/// space, eg. between nested quote symbols), or an inline binding beginmarker, these are
/// stored in the relevant strings in the signature to return their values to the caller.
/// The caller then has to use the returned ptr value to work out how many characters were
/// parsed over, update the callers len (length) value, and then parse on over anything
/// which may lie beyond the end of the second word (such as final punctuation, etc).
/// BEW created 11Oct10, to support the improved USFM parser build into doc version 5
/// BEW refactored 28Jan11, to parse 'inwards' from the ends, rather than across the word
/// BEW 2Feb11, added 4 more strings to signature, to return punctuation pulled off ends
/// of the word due to word-building status becoming punctuation status (2 of them), or to
/// return word-building characters to be added to ends of the word due to punctuation
/// status becoming changed to word-building status (because user used Preferences
/// Punctuation tab to dynamically change the punctuation settings)
//////////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::FinishOffConjoinedWordsParse(wxChar*& ptr, wxChar* pEnd, wxChar*& pWord2Start,
		wxChar*& pWord2End, wxString& punctAfter, wxString& bindingMkr, 
		wxString& newPunctFrom2ndPreWordLoc, wxString& newPunctFrom2ndPostWordLoc,
		wxString& wordBuildersFor2ndPreWordLoc, wxString& wordBuildersFor2ndPostWordLoc, 
		wxString& spacelessPuncts)
{
	// Note: the punctAfter param is "punctuation after the ~ fixedspace, which, since
	// this function is only used to parse the second of two conjoined words, is also the
	// preceding punctuation for the second of the two words (it is NOT the *'punctuation
	// after the second word' - the latter will be determined in the caller, ParseWord())
	wxChar* p = ptr;
	punctAfter.Empty();
	bindingMkr.Empty();
	pWord2Start = NULL;
	pWord2End = NULL;
	int length = 0;
	// this two for punctuation returning to word-building status
	wordBuildersFor2ndPreWordLoc.Empty();
	wordBuildersFor2ndPostWordLoc.Empty();
	// this two for word-building characters becoming punctuation characters
	newPunctFrom2ndPreWordLoc.Empty();
	newPunctFrom2ndPostWordLoc.Empty();
	// the FinishOffConjoinedWordsParse() needs all 4, of these, but for the first word or
	// a conjoined pair, or the only word when parsing a word not conjoined, the
	// equivalent tweaks and storage is scattered over several functions - in ParseWord(),
	// in IsFixedSpaceAhead() and in ParseSpanBackwards().

	// we need a punctuation string which includes space
	wxString punctuation = spacelessPuncts + _T(' ');
	if (p < pEnd)
	{
		// check out the possibility of word-initial punctuation preceding word2's
		// characters, and beware there may be detached opening quote, and so we can't
		// assume there won't be a space within the punctuation string (if there is a
		// punctuation string, that is)   
		punctAfter = SpanIncluding(p, pEnd, punctuation);
		length = punctAfter.Len();
		if (length > 0)
		{
			p = p + length;
		}
		// we've stopped because either we have come to a beginmarker, or to the
		// second word of the conjoined pair, or to the end of the buffer, or to a former
		// punctuation character which has just become a word-building one, and so is not
		// in the punctuation set
		if (p >= pEnd)
		{
			// this would be totally unexpected, all we can do is set the pointers to the
			// end and start of the second word to point where pEnd points, and return
			pWord2Start = p;
			pWord2End = p;
			ptr = p;
			return;
		}
		else
		{
			// there's more, so check out what is next - could be the start of the word,
			// or an inline binding beginmarker (could even be a sequence of these)
			// BEW 28Jan11, changed to using IsMarker() because it tests for \ followed by
			// a single alphabetic character, and so we don't have \ followed by space
			// giving a false positive
			// BEW 3Feb11, *p might be a punctuation character made into a word-building
			// one by the user just having altered the punctuation settings -- so if
			// inline marker(s) follow, such a character will need to end up at the start of the
			// word -- that is, jump the marker. We have a function for handling this
			// check etc.
			wordBuildersFor2ndPreWordLoc = SquirrelAwayMovedFormerPuncts(p, pEnd, spacelessPuncts);
			if (!wordBuildersFor2ndPreWordLoc.IsEmpty())
			{
				// advance pointer p, to point beyond the one or more puncts which are now
				// word-building and needing to be moved later on to start of the word proper
				size_t numChars = wordBuildersFor2ndPreWordLoc.Len();
				p += numChars;
			}

			// when we get here, p must be pointing at the marker if it is present, or at
			// the word proper if no marker is present
			bindingMkr.Empty();
			while (IsMarker(p))
			{
				wxString aBindingMkr = GetWholeMarker(p);
				length = aBindingMkr.Len();
				wxString mkrPlusSpace = aBindingMkr + _T(' ');
				if (gpApp->m_inlineBindingMarkers.Find(mkrPlusSpace) != wxNOT_FOUND)
				{
					// it is a beginmarker of the inline binding type (what USFM calls
					// 'Special Markers'), and so we need to deal with it - we store these
					// with their trailing space
					bindingMkr += mkrPlusSpace; // caller will store returned string(s) in
											    // m_inlineBindingMarkers member
					p += length;
					length = ParseWhiteSpace(p); // get past the whitespace after the marker
												 // (it might not be a single character)
					p += length;
				}
				else
				{
					// the marker is not the expected inline binding beginmarker, this
					// constitutes a USFM markup error. We can't process it as if it were
					// a binding marker, because it may be a marker preceding the next
					// word in the data and not conjoined, so we'll just return what we have
					// and put ptr back preceding any punctuation we may have found -- and
					// since we've not changed ptr yet, all we need do is return
					return;
				}
			} // end of loop for test: while (IsMarker(p))

            // We are potentially at the start of word2; the user may have changed
            // punctuation settings in such a way that one or more characters at the start
            // of the word have just become punctuation characters - we have to store these
            // in a string to return them to the caller (where they will be added to the
            // m_precPunct member of secondWord after any other puncts already in there) --
            // note that doing this means that if the source text is reconstituted, any
            // such puncts would move to being immediately preceding an inline binding
			// marker(s) if one or more of the latter precede the word). We must check
			// here for any such and remove them to the passed in storage string, and
			// advance our parsing pointer, p, to point beyond them ready to setting
			// pWord2Start further below.
			while (spacelessPuncts.Find(*p) != wxNOT_FOUND)
			{
				// *p is a punctuation character now, so store it and advance p
				newPunctFrom2ndPreWordLoc += *p++;
			}
			
			// we are at the start of word2, we can't scan over it using SpanExcluding()
			// because if there is embedded punctuation, it would foul the integrity of
			// the parse; so use FindParseHaltLocation() and ParseSpanBackwards() as the
			// IsFixedSpaceAhead() function does - this combination adhere's to our
			// word-parsing protocol, which is to parse inwards from either end, never
			// across it
			pWord2Start = p;
			ptr = p;

            // Find a halting location which is beyond the currently to-be-parsed word, but
            // not past the start of information which belongs to the following of what
            // could be thousands of words. Instead, we must scan ahead, parsing over any
            // ignorable white space, until we come to either ~, or non-ignorable
            // whitespace, or a closing bracket (]) - halting immediately before any such
            // character. We need a function for this and it can return, via its signature,
            // what the specific halt condition was. We also may parse over an inline
            // binding endmarker, (perhaps more than one), these don't halt parsing - but
            // we'll return the info in the signature, along with a count of how many such
            // markers we parsed over. We don't use much of what we find, just the
            // wordProper, because we let the caller handle everything to be parsed from
            // the end of the wordProper onwards
			wxChar* pHaltLoc = NULL;
			bool bFoundInlineBindingEndMarker = FALSE;
			bool bFoundFixedSpaceMarker = FALSE;
			bool bFoundClosingBracket = FALSE;
			bool bFoundHaltingWhitespace = FALSE;
			int nFixedSpaceOffset = -1;
			int nEndMarkerCount = 0;
			pHaltLoc = FindParseHaltLocation( p, pEnd, &bFoundInlineBindingEndMarker, 
							&bFoundFixedSpaceMarker, &bFoundClosingBracket, 
							&bFoundHaltingWhitespace, nFixedSpaceOffset, nEndMarkerCount);
			wxString aSpan(ptr,pHaltLoc); // this could be up to a [ or ], or a 
										  // whitespace or a beginmarker
			// now parse backwards to extract the span's info
			wxString wordProper; // emptied at start of ParseSpanBackwards() call below
			wxString firstFollPuncts; // ditto
			wxString inlineBindingEndMarkers; // ditto
			wxString secondFollPuncts; // ditto
			wxString ignoredWhiteSpaces; // ditto
			ParseSpanBackwards( aSpan, wordProper, firstFollPuncts, nEndMarkerCount, 
								inlineBindingEndMarkers, secondFollPuncts, ignoredWhiteSpaces,
								wordBuildersFor2ndPostWordLoc, spacelessPuncts);
            // now use the info extracted to set the FinishedOffConjoinedWordsParse() param
			// values ready for returning to ParseWord() -- all we want is wordProper --
			// note, if there is one or more now-word-building-characters in the
			// wordBuildersFor2ndPostWordLoc string, they are passed back to the caller
			// via the signature and will be appended to secondWord there, and the
			// caller's ptr value incremented by however many there are (we don't do it
			// here because it would return a wrong location for ptr and pWord2End to the
			// caller)
			newPunctFrom2ndPostWordLoc = firstFollPuncts; // new puncts pulled of end of word
			length = wordProper.Len();
			pWord2End = ptr + length;
			ptr = pWord2End;
		} // end of else block for test: if (p >= pEnd)
	} // end of TRUE block for test: if (p < pEnd)
}

///////////////////////////////////////////////////////////////////////////////
/// \return		the number of characters parsed over
/// \param  ptr            -> pointer to the next wxChar to be parsed (it should
///                           point at the starting character of the word proper,
///                           and after preceding punctuation for that word (if any)
/// \param  pEnd		   -> a pointer to the first character beyond the input 
///                           buffer's end (could be tens of kB ahead of ptr)
///                           marker, or an inline binding marker)
/// \param	pbFoundInlineBindingEndMarker   <- ptr to boolean, its name explains it; there
///                                    might be two or more in sequence, so a count of how
///                                    many of these there are is return in nEndMarkerCount
/// \param	pbFoundInlineNonbindingEndMarker <- ptr to boolean, its name explains it; these
///                                    are rare and only one will be at any one CSourcePhrase
/// \param	pbFoundFixedSpaceMarker <- ptr to boolean, TRUE if ~ encountered at or before
///                                    the halting location (~ IS the halting location
///                                    provided it precedes non-ignorable whitespace)
/// \param	pbFoundBracket          <- ptr to boolean, TRUE if ] or [ encountered (either
///                                    halts the scan, if it precedes ~ or whitespace)
/// \param  pbFoundHaltingWhitespace <- ptr to bool, TRUE if space or \n or \r encountered
///                                    and that whitespace is not ignorable (see comments
///                                    below for a definition of what is ignorable whitespace)
/// \remarks
/// Called from: the Doc's IsFixedSpaceAhead().
/// The IsFixedSpaceAhead() function, which is mission critical for delimiting a parsed
/// word or conjoined pair of words in the ParseWord() function, requires a smart subparser
/// which looks ahead for a fixed space marker (~), but only looks ahead a certain distance
/// - ensuring the parsing pointer does not encroach into material which belows to any of
/// the words which follow. This is that subparser. In doing it's job, it may parse over
/// whitespace which is ignorable, and possibly one or more inline binding endmarkers. 
/// The halting conditions are:
/// a) finding non-ignorable whitespace
/// b) finding ~ (the fixed space marker of USFM)
/// c) finding a closing bracket, ] or an opening bracket [
/// d) finding a begin-marker, or an endmarker which is not an inline binding one
/// We return, via the signature, information about the data types parsed over, to help
/// the caller to do it's more definitive parsing and data storage more easily. 
/// The following conditions define ignorable whitespace for the scanning process:
/// i)  immediately after an inline binding endmarker - provided what follows the whitespace
///     is either ] or ~ or another inline binding endmarker or punctuation which is a 
///     closing quote or closing doublequote
/// ii) between non-punctuation and an immediately following inline binding endmarker
/// iii)after punctuation, provided a closing quote or closing doublequote follows
/// No punctuation set is passed in, because this function deliberately does not
/// distinguish between punctuation and word-building characters -- halt location is
/// determined solely by ~ or [ or ] or certain SF markers.
/// BEW 11Oct10, (actually created 25Jan11)
//////////////////////////////////////////////////////////////////////////////////
wxChar* CAdapt_ItDoc::FindParseHaltLocation( wxChar* ptr, wxChar* pEnd,
											bool* pbFoundInlineBindingEndMarker,
											bool* pbFoundFixedSpaceMarker,
											bool* pbFoundClosingBracket,
											bool* pbFoundHaltingWhitespace,
											int& nFixedSpaceOffset,
											int& nEndMarkerCount)
{
	wxChar* p = ptr; // scan with p
	wxChar* pHaltLoc = ptr; // initialize to the start of the word proper
	enum SfmSet whichSFMSet = gpApp->gCurrentSfmSet;
	wxChar fixedSpaceChar = _T('~');
	// intialize return parameters
	*pbFoundInlineBindingEndMarker = FALSE;
	*pbFoundFixedSpaceMarker = FALSE;
	*pbFoundClosingBracket = FALSE;
	*pbFoundHaltingWhitespace = FALSE;
	nFixedSpaceOffset = -1;
	nEndMarkerCount = 0;
	wxString lastEndMarker; lastEndMarker.Empty();
	int offsetToEndOfLastBindingEndMkr = -1;
	// scan ahead, looking for the halt location prior to a following word or
	// end-of-buffer
	while (p < pEnd)
	{
		if (!IsMarker(p) && !IsWhiteSpace(p) && !IsFixedSpaceOrBracket(p))
		{
			// if none of those, then it's part of the word, or part of punctuation which
			// follows it, so keep scanning
			p++;
		}
		else
		{
			// it's one of those - handle each possibility appropriately
			if (*p == fixedSpaceChar)
			{
				nFixedSpaceOffset = (int)(p - ptr);
				*pbFoundFixedSpaceMarker = TRUE;
				break;
			}
			else if (*p == _T(']') || *p == _T('['))
			{
				*pbFoundClosingBracket = TRUE;
				break;
			}
            // if neither of the above, it must be one of the other conditions - try
            // endmarkers next; if it is one, and if it is an inline binding marker, we
            // note the fact and continue scanning; but other endmarkers halt scanning
            // (including the non-binding inline ones, like \wj*)
			if (IsMarker(p))
			{
				wxString wholeMkr = GetWholeMarker(p);
				int offset = wholeMkr.Find(_T('*'));
				if (whichSFMSet == PngOnly)
				{
					// this is a sufficient condition for determining that there is no ~
					// conjoining (endmarkers in this set are only \F or \fe - either is a
					// footnote end, and there would not be conjoining across that kind of
					// a boundary) and so we are at the end of a word for sure, so return
					break;
				}
				else // must be UsfmOnly or UsfmAndPng - we assume UsfmOnly
				{
					if (offset == wxNOT_FOUND)
					{
						//  there is no asterisk in the marker, so it is not an endmarker
						//  - it must then be a beginmarker, and they halt scanning
						break;
					}
					else
					{
						// it's an endmarker, but we parse over only those which are
						// inline binding ones, otherwise the marker halts scanning
						wxString beginMkr = wholeMkr;
						beginMkr = beginMkr.Truncate(beginMkr.Len() - 1); // remove 
											// the * (we are assuming the asterisk was at
											// the end where it should be)
						wxString mkrPlusSpace = beginMkr + _T(' '); // append a space
						int offset2 = gpApp->m_inlineBindingMarkers.Find(mkrPlusSpace);
						if (offset2 == wxNOT_FOUND)
						{
							// it's not one of the space-delimited markers in the fast access
							// string of inline binding beginmarkers, so it halts scanning
							break;
						}
						else
						{
							// it's an inline binding endmarker, so we scan over it and
							// let the caller handle it when parsing backwards to find the
							// end of the text part of the word just parsed over
							*pbFoundInlineBindingEndMarker = TRUE;
							lastEndMarker = wholeMkr;
							nEndMarkerCount++;
							unsigned int markerLen = wholeMkr.Len(); // use this to jump p forwards
							offsetToEndOfLastBindingEndMkr = (int)(p - ptr) + markerLen;
							p = p + markerLen;
						}
					}
				} // end of else block for test: if (whichSFMSet == PngOnly)
			} // end of TRUE block for test: if (IsMarker(p))
			else if (IsWhiteSpace(p))
			{
				// it's whitespace - some such can just be ignored, others constitute the
				// end of the word or word plus punctuation (and possibly binding
				// endmarker(s)) and so constitute grounds for halting - determine which
				// is the case 
				// first, handle condition (i) in the remarks of the function description
				if (*pbFoundInlineBindingEndMarker == TRUE && p == (ptr + offsetToEndOfLastBindingEndMkr))
				{
                    // The iterator, p, is pointing at a whitespace character immediately
                    // following an inline binding marker just parsed over. This halts
                    // scanning except when this whitespace (or several whitespace
                    // characters) is followed by ~ or one of [ or ], or another inline
                    // binding endmarker -- check these subconditions out, if one of them
                    // is satisfied, then advance p to the ~ or [ or ] and halt there, but
                    // if another inline binding endmarker follows, advance p to its start
                    // and let the scanning loop continue
					int whitespaceSpan = ParseWhiteSpace(p);
					if (*(p + whitespaceSpan) == fixedSpaceChar)
					{
						// there is a fixedspace marker following, so return with p
						// pointing at it, etc
						p = p + whitespaceSpan;
						nFixedSpaceOffset = (int)(p - ptr);
						*pbFoundFixedSpaceMarker = TRUE;
						break;
					}
					else if (*(p + whitespaceSpan) == _T(']') || *(p + whitespaceSpan) == _T('['))
					{
						// there is an opening or closing bracket following the
						// whitespace(s), this halts scanning and also means there is no
						// conjoining (the whitespace is ignorable)
						p = p + whitespaceSpan;
						break;
					}
					else if (IsMarker(p + whitespaceSpan))
					{
						// it's a marker -- if it is an inline binding endmarker, then
						// jump over it, etc, and continue scanning, otherwise, it halts
						// scanning (and might as well halt at the space where p currently
						// is if that is the case)
						wxString wholeMkr = GetWholeMarker(p + whitespaceSpan);
						int offset = wholeMkr.Find(_T('*'));
						if (whichSFMSet == PngOnly)
						{
							// this is a sufficient condition for determining that there is no ~
							// conjoining (endmarkers in this set are only \F or \fe - either is a
							// footnote end, and there would not be conjoining across that kind of
							// a boundary) and so we are at the end of a word for sure, so return
							*pbFoundHaltingWhitespace = TRUE;
							break;
						}
						else // must be UsfmOnly or UsfmAndPng - we assume UsfmOnly
						{
							if (offset == wxNOT_FOUND)
							{
								//  there is no asterisk in the marker, so it is not an endmarker
								//  - it must then be a beginmarker, and they halt scanning
								*pbFoundHaltingWhitespace = TRUE;
								break;
							}
							else
							{
								// it's an endmarker, but we parse over only those which are
								// inline binding ones, otherwise the marker halts scanning
								wxString beginMkr = wholeMkr.Truncate(wholeMkr.Len() - 1); // remove 
													// the * (we are assuming the asterisk was at
													// the end where it should be)
								wxString mkrPlusSpace = beginMkr + _T(' '); // append a space
								int offset2 = gpApp->m_inlineBindingMarkers.Find(mkrPlusSpace);
								if (offset2 == wxNOT_FOUND)
								{
									// it's not one of the space-delimited markers in the fast access
									// string of inline binding beginmarkers, so it halts scanning
									*pbFoundHaltingWhitespace = TRUE;
									break;
								}
								else
								{
									// it's an inline binding endmarker, so we scan over it and
									// let the caller handle it when parsing backwards to find the
									// end of the text part of the word just parsed over,
									// continue iterating
									*pbFoundInlineBindingEndMarker = TRUE;
									nEndMarkerCount++;
									unsigned int markerLen = wholeMkr.Len(); // use this to jump p forwards
									p = p + whitespaceSpan; // point p at the start of the binding endmarker 
									offsetToEndOfLastBindingEndMkr = (int)(p - ptr) + markerLen;
									p = p + markerLen;
								}
							}
						} // end of else block for test: if (whichSFMSet == PngOnly)
					} // end of TRUE block for test: else if (IsMarker(p + whitespaceSpan))
					else if (IsClosingCurlyQuote(p + whitespaceSpan))
					{
						// it's a closing curly quote, or a > chevron -- so scan over it &
						// continue 
						p = p + whitespaceSpan;
					}
					else
					{
						// any other punctuation coming after a space or spaces should be
						// considered as opening punctuation for the following word, so
						// halt now
						*pbFoundHaltingWhitespace = TRUE;
						break;
					}

				} // end of TRUE block for test: if (*pbFoundInlineBindingEndMarker == TRUE && 
				  //                                 p == (ptr + offsetToEndOfLastBindingEndMkr))
				else
				{
					// subcondition (i) does not apply, so now test for subcondition (ii)
					// -- between something and a following inline binding endmarker
					int whitespaceSpan = ParseWhiteSpace(p);
					if (IsMarker(p + whitespaceSpan))
					{
						// it's a marker -- if it is an inline binding endmarker, then
						// jump over it, etc, and continue scanning, otherwise, it halts
						// scanning (and might as well halt at the space where p currently
						// is if that is the case)
						wxString wholeMkr = GetWholeMarker(p + whitespaceSpan);
						int offset = wholeMkr.Find(_T('*'));
						if (whichSFMSet == PngOnly)
						{
							// this is a sufficient condition for determining that there is no ~
							// conjoining (endmarkers in this set are only \F or \fe - either is a
							// footnote end, and there would not be conjoining across that kind of
							// a boundary) and so we are at the end of a word for sure, so return
							*pbFoundHaltingWhitespace = TRUE;
							break;
						}
						else // must be UsfmOnly or UsfmAndPng - we assume UsfmOnly
						{
							if (offset == wxNOT_FOUND)
							{
								//  there is no asterisk in the marker, so it is not an endmarker
								//  - it must then be a beginmarker, and they halt scanning
								*pbFoundHaltingWhitespace = TRUE;
								break;
							}
							else
							{
								// it's an endmarker, but we parse over only those which are
								// inline binding ones, otherwise the marker halts scanning
								wxString beginMkr = wholeMkr.Truncate(wholeMkr.Len() - 1); // remove 
													// the * (we are assuming the asterisk was at
													// the end where it should be)
								wxString mkrPlusSpace = beginMkr + _T(' '); // append a space
								int offset2 = gpApp->m_inlineBindingMarkers.Find(mkrPlusSpace);
								if (offset2 == wxNOT_FOUND)
								{
									// it's not one of the space-delimited markers in the fast access
									// string of inline binding beginmarkers, so it halts scanning
									*pbFoundHaltingWhitespace = TRUE;
									break;
								}
								else
								{
									// it's an inline binding endmarker, so we scan over it and
									// let the caller handle it when parsing backwards to find the
									// end of the text part of the word just parsed over,
									// continue iterating
									*pbFoundInlineBindingEndMarker = TRUE;
									nEndMarkerCount++;
									unsigned int markerLen = wholeMkr.Len(); // use this to jump p forwards
									p = p + whitespaceSpan; // point p at the start of the binding endmarker 
									offsetToEndOfLastBindingEndMkr = (int)(p - ptr) + markerLen;
									p = p + markerLen;
								}
							}
						} // end of else block for test: if (whichSFMSet == PngOnly)
					} // end of TRUE block for test: else if (IsMarker(p + whitespaceSpan))
					else 
					{
						// subcondition (ii) doesn't apply, so try subconditon (iii) --
						// this boils down to testing for a closing (curly) quote or >
						// wedge after the whitespace, if we find that ignore the space
						// and continue scanning, otherwise we halt here
						int whitespaceSpan = ParseWhiteSpace(p);
						if (IsClosingCurlyQuote(p + whitespaceSpan))
						{
							// this space(s) is/are to be ignored, continue scanning
							p = p + whitespaceSpan;
						}
						else
						{
							// none of the subconditions for regarding this space as ignorable are
							// satisfied, so halt here
							*pbFoundHaltingWhitespace = TRUE;
							break;
						}
					}
				} // end of else block for test: if (*pbFoundInlineBindingEndMarker == TRUE && 
				  //                                 p == (ptr + offsetToEndOfLastBindingEndMkr))
			} // end of TRUE block for test: else if (IsWhiteSpace(p))
			else
			{
				// it's not whitespace -- control should never enter here, but if it does,
				// then halt for safety's sake
				break;
			}	
		} // end of else block for test: if (!IsMarker(p) && !IsWhiteSpace(p) && !IsFixedSpaceOrBracket(p))
	}
	pHaltLoc = p;
	return pHaltLoc;
}

wxString CAdapt_ItDoc::SquirrelAwayMovedFormerPuncts(wxChar* ptr, wxChar* pEnd, wxString& spacelessPuncts)
{
	wxString squirrel; squirrel.Empty();
	// first, find out if there is an inline binding beginmarker no more than
	// MAX_MOVED_FORMER_PUNCTS characters ahead of where ptr points on entry; if there
	// isn't, return an empty string because the caller must then assume that ptr on entry
	// is pointing at the actual start of the word which is to be parsed; if there is,
	// then make a further check - there must not be a space preceding the marker - if
	// there is, then return an empty string, because ptr must be pointing at a word to be
	// parsed
	int numCharsToCheck = (int)MAX_MOVED_FORMER_PUNCTS;
	bool bMarkerExists = FALSE;
	bool bItsAnInlineBindingMarker = FALSE;
	int count = 1;
	while (count <= numCharsToCheck)
	{
		if (IsWhiteSpace(ptr + count))
		{
			// white space encountered before a marker was reached, so return the empty
			// string
			return squirrel;
		}
		else if (IsMarker(ptr + count))
		{
			bMarkerExists = TRUE; // we must exit at the first found, we can't look beyond it
			break;
		}
		else
		{
			count++;
		}
	}
	// did we find a marker?
	if (!bMarkerExists)
	{
		// no marker within the allowed small span of following characters (3 is
		// MAX_MOVED_FORMER_PUNCTS value -- see AdaptItConstants.h) so the caller must
		// assume that ptr is the actual start of the word - it will deduce that fact if
		// the returned string is empty
		return squirrel;
	}
	else
	{
        // we found a marker, but it has to be an inline binding marker (and not an inline
        // binding endmarker); so check if it is an inline binding marker - if so, and
        // providing there was no preceding whitespace (tested in the loop above), we can
        // test for squirreling some non-restored word initial word-building characters
        // that got moved earlier to precede the inline binding marker, into the squirrel
        // string for safekeeping until the caller needs to insert them at the start of the
        // word to be parsed
		wxString wholeMkr = GetWholeMarker(ptr + count);
		// if it's an endmarker, return, we've not the situation we expect could happen
		if (wholeMkr[wholeMkr.Len() - 1] == _T('*'))
		{
			// it's an endmarker - return
			return squirrel;
		}
		wxString bareMkr = wholeMkr.Mid(1); // remove the initial backslash
		USFMAnalysis* pUsfmAnalysis = LookupSFM(bareMkr);
		if (pUsfmAnalysis == NULL)
		{
			// it's an unknown marker, therefore not an inline binding marker
			return squirrel; // caller will have to assume the char(s) at ptr are start of a word
		}
		else
		{
			wxString wholeMkrPlusSpace = wholeMkr + _T(' ');
			if (gpApp->m_inlineBindingMarkers.Find(wholeMkrPlusSpace) != wxNOT_FOUND)
			{
				// we've found a valid beginmarker from the set of inline binding markers
				bItsAnInlineBindingMarker = TRUE;
			}
		}
	}
	if (!bItsAnInlineBindingMarker)
	{
		squirrel.Empty();
		return squirrel;
	}
	wxChar* pNewEnd = ptr + count; // where the inline binding marker commences
	// now move each of them up to the marker, to squirrel string, provided they are not
	// in the punctuation set -- do this only if an inline binding marker was found ahead
	// of the characters at issue (because it's only such a marker that caused the move of
	// the former word-building char to become a punt in the first place, so it's only
	// from that kind of movement round the marker that we need a recovery mechanism for)
	while (bItsAnInlineBindingMarker && ptr < pNewEnd && ptr < pEnd && 
			spacelessPuncts.Find(*ptr) == wxNOT_FOUND)
	{
		squirrel += *ptr++;
	}
	if (bItsAnInlineBindingMarker && !squirrel.IsEmpty() && *ptr != gSFescapechar)
	{
		// there is at least one more moved here when it became punctuation, but it
		// remains as punctuation still... so to enable the caller to get the parsing ptr
		// to point at the marker's backslash when we return, we have to grab all the
		// rest, whether punctuation or not, and squirrel them away too. This can result
		// in punctuation being "buried" in word-medial location. We can't help this, it's
		// the price we pay for having one CSourcePhrase under punctuation changes
		// generate just one altered CSourcePhrase -- if not, we'll get more than one and
		// then things get messy
		while (*ptr != gSFescapechar)
		{
			squirrel += *ptr++;
		}
	}
	return squirrel;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		the number of characters parsed over
/// \param		pChar			-> a pointer to the next character to be parsed (it
///                                could be the first character of theWord, or
///                                preceding punctuation, or an inline non-binding
///                                marker, or an inline binding marker)
/// \param      pEnd            -> pointer to first char past the end of the buffer
/// \param		pSrcPhrase	   <-> ptr to the CSourcePhrase instance we are building
/// \param	spacelessPuncts     -> punctuation characters set used herein, spaces removed
/// \param	boundarySet	        -> same as spacelessSrcPunct but also with comma removed
/// \param inlineNonbindingMrks -> space-delimited markers, \wj \qt \sls \tl \fig
///	\param bIsInlineNonbindingMkr -> TRUE if pChar points at one of the above 5 mkrs								 
///	\param bIsInlineBindingMkr  -> TRUE if pChar points at any of the other lineline
///	                               markers (but not any from the footnote or
///	                               crossReference set, which start with \f or \x,
///	                               respectively)
/// \remarks
/// Called from: the Doc's TokenizeText(), the View's RemovePunctuation(),
/// DoExportSrcOrTgtRTF(), ProcessAndWriteDestinationText().
/// Parses a word of ordinary text, intelligently handling (and accumulating) characters
/// defined as punctuation, and dealing with any inline markers and endmarkers. The legacy
/// version of this function tried to intelligently handle sequences of straight
/// singlequote and straight doublequote following the word; but we no longer do that - we
/// assume good USFM markup, and expect either chevrons or curly quotes, not straight ones
/// - but we'll handle straight ones in sequence following the word only while no space
/// occurs between them - after that, any further one will be assumed to be at the start
/// of the next CSourcePhrase instance (if that is a bad assumption, the user can edit the
/// source text to make the quotes curly ones, and then the parse will be redone right).
/// 
/// BEW 11Oct10, changed for docVersion5, to support inline markers and following
/// punctuation which may occur both before and after endmarkers. Also, entry to
/// ParseWord() is potentially earlier - as soon as an inline marker is encountered which
/// is neither \f nor \x; and more processing is done within the function rather than
/// returning strings to the caller for the caller to set up the pSrcPhrase from what is
/// returned. We leave m_markers for the caller to add to pSrcPhrase, since we've already
/// collected it's contents in the tokBuffer string; so here we just support the following
/// protocols and the variations mentioned below - in order of occurrence:
/// (1) Preceding theWord (theWord is whatever punctuation-less word we want to parse)
/// a)m_markers (in caller) b)m_inlineNonbindingMkrs c) m_precPunct d) m_inlineBindingMkrs
/// (2) Following theWord:
/// a)m_inlineBindingEndMkrs b) m_follPunct c) m_endMarkers d) m_follOuterPunct
/// e) m_inlineNonbindingEndMarkers
/// We expect information after theWord to comply with the order above. However, we parse
/// without comment the following variations:
/// (i) following punctuation preceding a) -- but we'll export in the above order
/// (ii) inline non-binding endmarkers which occur preceding d) -- but we'll export in the
/// above order 
/// Note: the parser now supports ~ USFM fixed space conjoining of a word pair (but not
/// sequences or 3 words or more), and it will parse punctuation before or after (or both)
/// the ~ symbol and also any inline binding marker or endmarker occuring with either of
/// the conjoined words. The word~word conjoined pair are treated as a pseudo-merger.
/// Therefore, if the user does not want to retain the conjoining he can undo the merger
/// and he's just have two normal CSourcePhrase instances in sequence, storing those two
/// words separately, and punctuation on each where it should be. To restore the conjoining
/// would require selecting the two words and doing an "Edit Source Text" operation to
/// restore ~ to its place between them.
/// We also support [ and ] brackets delimiting material considered possibly
/// non-canonical. The parent, TokenizeText(), handles [, but we must handle ] within
/// ParseWord(). If ] is encountered (but we don't check within ~ conjoined pairs because
/// it is not a reasonable assumption that ] would occur between such a conjoining) then
/// we must immediately halt parsing, as that wordform and markers and puncts, etc, is
/// then deemed finished, so that on return to TokenizeText() ptr will be pointing at the
/// ] character, and then the latter function will do the parse of the closing ] symbol -
/// assigning it to an orphan CSourcePhrase, storing it in its m_follPunct member. This
/// protocol for handling ] works the same way regardless of whether or not the ]
/// character is designated a punctuation character - we treat it as if it was, even if not.
///////////////////////////////////////////////////////////////////////////////
int CAdapt_ItDoc::ParseWord(wxChar *pChar,
		wxChar* pEnd,
		CSourcePhrase* pSrcPhrase, 
		wxString& spacelessPuncts, // caller determines whether it's src set or tgt set
		wxString& inlineNonbindingMrks, // fast access string for \wj \qt \sls \tl \fig
		wxString& inlineNonbindingEndMrks, // for their endmarkers \wj* etc
		bool& bIsInlineNonbindingMkr, 
		bool& bIsInlineBindingMkr)
{
	int len = 0;
	wxChar* ptr = pChar;
	int itemLen;
	wxString emptyStr = _T("");
	wxString aSpace = _T(" ");
	wxString theSymbol = _T("~"); // USFM fixedspace symbol
	USFMAnalysis* pUsfmAnalysis = NULL;
	wxString bareMkr;
	wxString bareEndMkr;
	wxString wholeMkr;
	wxString wholeEndMkr;
	wxString wholeMkrPlusSpace;
	bool bExitParseWordOnReturn = FALSE;
	int nFound = wxNOT_FOUND;
	bool bHasPrecPunct = FALSE;
	bool bHasOpeningQuote = FALSE;
	bool bParsedInlineBindingMkr = FALSE;
	wxString finalPunctBeforeFixedSpaceSymbol;
	wxString precedingPunctAfterFixedSpaceSymbol;
	finalPunctBeforeFixedSpaceSymbol.Empty();
	precedingPunctAfterFixedSpaceSymbol.Empty();
	CSourcePhrase* pSrcPhrWord1 = NULL;
	CSourcePhrase* pSrcPhrWord2 = NULL;
	int nHowManyWhites = 0;
	wxChar* pMaybeWhitesStart = NULL;
	wxChar* pMaybeWhitesEnd = NULL;
	wxString wordBuildersForPreWordLoc;
	wxString wordBuildersForPostWordLoc; wordBuildersForPostWordLoc.Empty();
	// next pair for use with the second word in a conjoined pair, when a punct is being
	// restored to word-building status
	wxString wordBuildersFor2ndPreWordLoc;
	wxString wordBuildersFor2ndPostWordLoc; wordBuildersFor2ndPostWordLoc.Empty();
	// next pair for use with the second word in a conjoined pair, when a word-building
	// character has just been made a punct character
	wxString newPunctFrom2ndPreWordLoc;
	wxString newPunctFrom2ndPostWordLoc; newPunctFrom2ndPostWordLoc.Empty();

	// the first possibility to deal with is that we may be pointing at an inline
	// non-binding marker, there are 5 such, \wj \qt \sls \tl \fig, and the caller will
	// have provided a boolean telling us we are pointing at one
	if (bIsInlineNonbindingMkr)
	{
		// we are pointing at one of these five markers, handle this situation...
		pUsfmAnalysis = LookupSFM(ptr);
		wxASSERT(pUsfmAnalysis != NULL); // must not be an unknown marker
		bareMkr = emptyStr;
		bareMkr = pUsfmAnalysis->marker;
		wholeMkr = gSFescapechar + bareMkr;
		wholeMkrPlusSpace = wholeMkr + aSpace;
		wxASSERT(inlineNonbindingMrks.Find(wholeMkrPlusSpace) != wxNOT_FOUND);
#ifndef __WXDEBUG__
		inlineNonbindingMrks.IsEmpty(); // does nothing, but avoids compiler warning
#endif
		itemLen = wholeMkr.Len();

		// store the whole marker, and a following space
		pSrcPhrase->SetInlineNonbindingMarkers(wholeMkrPlusSpace);

		// point past the inline non-binding marker, and then parse
		// over the white space following it, and point past that too
		ptr += itemLen;
		len += itemLen;
		itemLen = ParseWhiteSpace(ptr);
		ptr += itemLen;
		len += itemLen;
		wxASSERT(ptr < pEnd);
	}
	else
	{
		// we are not pointing at one of the five
		pSrcPhrase->SetInlineNonbindingMarkers(emptyStr);
	}
	bIsInlineNonbindingMkr = FALSE; // it's passed by ref, so clear the value 
									// otherwise next entry will fail
	// what might ptr be pointing at now? One of the following 3 possibilities:
	// 1. punctuation which needs to be stored in m_precPunct, or
	// 2. an inlineBindingMkr, such as \k or \w etc, or
	// 3. the first character of the word to be stored as m_key in pSrcPhrase
	// The first to check for is punctuation, then inlineBinbdingMkr; however, if the
	// bIsInlineBindingMkr flag was passed in as TRUE, then there won't be punctuation
	// preceding the inlineBindingMkr which ptr points at - so we must check the flag and
	// deal with that marker now (because there might be bad USFM markup with punctuation
	// following such a marker, so we'll handle that and put things back together in
	// correct order in an export, if the markup is indeed incorrect in this way)
	itemLen = 0;
	if (bIsInlineBindingMkr)
	{
		pUsfmAnalysis = LookupSFM(ptr);
		wxASSERT(pUsfmAnalysis != NULL); // must not be an unknown marker
		bareMkr = emptyStr;
		bareMkr = pUsfmAnalysis->marker;
		wholeMkr = gSFescapechar + bareMkr;
		wholeMkrPlusSpace = wholeMkr + aSpace;
		wxASSERT(pUsfmAnalysis->inLine == TRUE);
		itemLen = wholeMkr.Len();

		// store the whole marker, and a following space
		pSrcPhrase->SetInlineBindingMarkers(wholeMkrPlusSpace);
		bParsedInlineBindingMkr = TRUE;

        // point past the inline binding marker, and then parse over the white space
        // following it, and point past that too
		ptr += itemLen;
		len += itemLen;
		itemLen = ParseWhiteSpace(ptr);
		ptr += itemLen;
		len += itemLen;
		wxASSERT(ptr < pEnd);
	}
	bIsInlineBindingMkr = FALSE; // it's passed by ref, so clear the value 
								 // otherwise next entry will fail

	// what might ptr be pointing at now? If the above block actually stored a marker,
	// then we'd expect to be pointing at the word proper'a first character. But...
	// there may be incorrect markup, and punctuation could be next. And if we didn't
	// enter the above block, then punctuation could be next, and then an inline binding
	// marker could follow that, so the following 3 possibilities still apply:
	// 1. punctuation which needs to be stored in m_precPunct, or
	// 2. an inlineBindingMkr, such as \k or \w etc, or
	// 3. the first character of the word to be stored as m_key in pSrcPhrase
	// The first to check for is punctuation, then inlineBinbdingMkr

    // first, parse over any 'detached' preceding punctuation, bearing in mind it may have
    // sequences of single and/or double opening quotation marks with one or more spaces
    // between each. We want to accumulate all such punctuation, and the spaces in-place,
    // into the precedePunct CString. We assume only left quotations and left wedges can be
    // set off by spaces from the actual word and whatever preceding punctuation is on it.
    // We make the same assumption for punctuation following the word - but in that case
    // there should be right wedges or right quotation marks. We'll allow ordinary
    // (vertical) double quotation, and single quotation if the latter is being considered
    // to be punctuation, even though this weakens the integrity of out algorithm - but it
    // would only be compromised if there were sequences of vertical quotes with spaces
    // both at the end of a word and at the start of the next word in the source text data,
    // and this would be highly unlikely to ever occur.
	while (IsOpeningQuote(ptr) || IsWhiteSpace(ptr))
	{
		// check if a straight quote is in the preceding punctuation - setting the boolean
		// true will help us decide if a straight quote following the word belongs to the
		// word as final punctuation, or to the next word as opening punctuation.
		bool bStraightQuote = IsStraightQuote(ptr);
		if (bStraightQuote)
			m_bHasPrecedingStraightQuote = TRUE; // a public boolean of CAdapt_ItDoc class
				// which gets cleared to default FALSE at the start of each new verse, and
				// also after having been used to help decide who owns a straight quote 
				// which was detected following the word being parsed

        // this block gets us over all detached preceding quotes and the spaces which
        // detach them; we exit this block either when the word proper has been reached, or
        // with ptr pointing at some non-quote punctuation attached to the start of the
        // word, or with ptr pointing at an inline bound marker. In the event we exist
        // pointing at non-quote punctuation, the next block will parse across any such
        // punctuation until the word proper has been reached, or until an inline bound mkr
        // is reached.
		if (IsWhiteSpace(ptr))
		{
			pSrcPhrase->m_precPunct += _T(' '); // normalize while we are at it
			ptr++;
		}
		else
		{
			bHasOpeningQuote = TRUE; // FALSE is used later to stop regular opening 
                // quote (when initial in a following word) from being interpretted as
                // belonging to the current sourcephrase in the circumstance where there is
                // detached non-quote punctuation being spanned in this current block. That
                // is, we want "... word1 ! "word2" word3 ..." to be handled that way,
                // instead of being parsed as "... word1 ! " word2" word3 ..." for example
			pSrcPhrase->m_precPunct += *ptr++;
		}
		len++;
	}
	// when control reaches here, we may be pointing at further punctuation having
	// iterated across some data in the above loop, or be pointing at the first character
	// of the word, or be pointing at the backslash of an inline binding marker - so
	// handle these possibilities
	if (IsMarker(ptr))
	{
        // we are pointing at an inline marker - it must be one with inLine TRUE, that is,
        // an inline binding marker; beware, we can have \k \w word\w*\k*, and so we could
        // be pointing at the first of a pair of them, so we can't assume there will be
        // only one every time
		//while (*ptr == gSFescapechar)
		while (IsMarker(ptr))
		{
			// parse across as many as there are, and the obligatory white space following
			// each - normalizing \n or \r to a space at the same time
			pUsfmAnalysis = LookupSFM(ptr);
			wxASSERT(pUsfmAnalysis != NULL); // must not be an unknown marker
			bareMkr = emptyStr;
			bareMkr = pUsfmAnalysis->marker;
			wholeMkr = gSFescapechar + bareMkr;
			wholeMkrPlusSpace = wholeMkr + aSpace;
			wxASSERT(pUsfmAnalysis->inLine == TRUE);
			itemLen = wholeMkr.Len();

			// store the whole marker, and a following space
			pSrcPhrase->AppendToInlineBindingMarkers(wholeMkrPlusSpace);
			bParsedInlineBindingMkr = TRUE;

			// shorten the buffer to point past the inline binding marker, and then parse
			// over the white space following it, and shorten the buffer to exclude that too
			ptr += itemLen;
			len += itemLen;
			itemLen = ParseWhiteSpace(ptr);
			ptr += itemLen;
			len += itemLen;
			wxASSERT(ptr < pEnd);
		}
		// once control gets to here, ptr should be pointing at the first character of the
		// actual word for saving in m_key of pSrcPhrase; we don't expect punctuation
		// after a binding inline marker, but because of the possibility of user markup
		// error, we'll allow it. It's not that we can't deal with it; it's just
		// inappropriate markup. So we won't have a wxASSERT() here. Also, when
		// ParseWord() is being called to rebuild a doc after user changed punctuation
		// settings, this can produce exceptions (see below) to the expectation we are now
		// at the start of the word to be parsed.
	}
	else
	{
		// the legacy parser's code still applies here - to finish parsing over any
		// non-quote punctuation which precedes the word
		while (!IsEnd(ptr) && (nFound = spacelessPuncts.Find(*ptr)) >= 0)
		{
            // the test checks to see if the character at the location of ptr belongs to
            // the set of source language punctuation characters (with space excluded from
            // the latter) - as long as the nFound value is positive we are parsing over
            // punctuation characters
			pSrcPhrase->m_precPunct += *ptr++;
			len++;
		}

		// handle the undoing of the above block's code when the user has changed his mind and
		// reverted punctuation character(s) to being word-building ones
		wordBuildersForPreWordLoc = SquirrelAwayMovedFormerPuncts(ptr, pEnd, spacelessPuncts);
		// if we actually squirreled some away, then we must advance ptr over them, and update
		// len value
		if (!wordBuildersForPreWordLoc.IsEmpty())
		{
			size_t theirLength = wordBuildersForPreWordLoc.Len();
			ptr += theirLength;
			len += theirLength;
			// we will insert that prior to the word where theWord wxString is created below,
			// and for when dealing with ~ fixedspaced conjoining, where wxString firstWord is
			// defined
		}
        // when the above loop exits, and any squirreling required has been done, ptr may
        // be pointing at the word, or at a preceding inline binding marker, like \k (for
        // keyword) or \w (for a wordlist word) or various other markers - many of which
        // are character formatting ones, like italics (\it), etc -- so handle the
        // possibility of one or more inline binding markers here within this else block,
        // so that when the else block is exited, we are pointing at the first character of
        // the actual word
		if (IsMarker(ptr))
		{
            // we are pointing at an inline marker - it must be one with inLine TRUE and
            // TextType none and not one of the 5 mentioned above, that is, an inline
            // binding marker
            // (beware, we can have \k \w word\w*\k*, and so we could be pointing at the
            // first of a pair of them, so we can't assume there will be only one every
            // time)
			while (IsMarker(ptr))
			{
                // parse across as many as there are, and the obligatory white space
                // following each - normalizing \n or \r to a space at the same time
				pUsfmAnalysis = LookupSFM(ptr);
				if (pUsfmAnalysis == NULL)
				{
					// must not be an unknown marker, tell the user to fix the input data
					// and try again
					wxString wholeMkr2 = GetWholeMarker(ptr);
					wxString msgStr;
					msgStr = msgStr.Format(
_("Adapt It does not recognise this marker: %s which is in the input file.\nEdit the input file in a word processor, save, and then retry creating the document.\nAdapt It will now abort."),
					wholeMkr2.c_str());
					wxMessageBox(msgStr, _T(""), wxICON_ERROR);
					abort();
					return 0;
				}
				bareMkr = emptyStr;
				bareMkr = pUsfmAnalysis->marker;
				wholeMkr = gSFescapechar + bareMkr;
				wholeMkrPlusSpace = wholeMkr + aSpace;
				if (pUsfmAnalysis->inLine == FALSE)
				{
					// it's not an inline marker, so abort
					wxString msgStr;
					msgStr = msgStr.Format(
_("This marker: %s  follows punctuation but is not an inline marker.\nIt is not one of the USFM Special Text and Character Styles markers.\nEdit the input file in a word processor, save, and then retry creating the document.\nAdapt It will now abort."),
					wholeMkr.c_str());
					wxMessageBox(msgStr, _T(""), wxICON_ERROR);
					abort();
					return 0;
				}
				itemLen = wholeMkr.Len();

				// store the whole marker, and a following space
				pSrcPhrase->AppendToInlineBindingMarkers(wholeMkrPlusSpace);
				bParsedInlineBindingMkr = TRUE;

                // point past the inline binding marker, and then parse over the white
                // space following it, and point past that too
				ptr += itemLen;
				len += itemLen;
				itemLen = ParseWhiteSpace(ptr);
				ptr += itemLen;
				len += itemLen;
				wxASSERT(ptr < pEnd);
			}
            // once control gets to here, ptr should be pointing at the first character of
            // the actual word for saving in m_key of pSrcPhrase ... well, usually.
		}
	} // end of else block for test: if (IsMarker(ptr))
	// determine if we've found preceding punctuation
	if (pSrcPhrase->m_precPunct.Len() > 0)
		bHasPrecPunct = TRUE;

	// this is where the first character of the word possibly starts...
	
	// BEW 31Jan11, it is possible that a dynamic punctuation change has just made one or
	// more of the former word-initial character/characters into punctuation
	// characters, and if that is the case, then our algorithm won't handle it without
	// something extra here if control has just parsed over an inline binding marker - so
	// test for this and do additional checks, removing any punctuation characters from
	// where ptr is pointing if they are in the being-used punctuation set, and moving
	// them to the m_precPunct member, and setting bHasPrecPunct if any such are moved.
	// [Note: removing a punctuation character from the punctuation set dynamically isn't a
	// problem, as it returns then to the word-building set, and when parsing the
	// reconstituted string, the parser will halt earlier, so that the non-word-building
	// character(s) are at the new starting point for the word proper - for this scenario,
	// no new code is needed.]
	// Note: when we put the new punctuation character into m_precPunct, we are producing
	// a connundrum for later on if the user changes his mind about that character's
	// punctuation status, because in the presence of an inline binding marker, the former
	// punctuation character then becomes pre-marker rather than pre-word, so we'll need
	// additional code (it's above, where pre-word non-quote puncts have finished being
	// parsed over) for that situation which will squirrel such characters away and
	// restore them to word-initial position later on when the word is actually defined
	if (bParsedInlineBindingMkr)
	{
		int offset = wxNOT_FOUND;
		while ((offset = spacelessPuncts.Find(*ptr)) != wxNOT_FOUND)
		{
			// we've a newly-defined initial punctuation character to move to m_precPunct
			bHasPrecPunct = TRUE;
			pSrcPhrase->m_precPunct += *ptr;
			ptr++;
			len++;
		}
	}

	// we are now a the first character of the word
	wxChar* pWordProper = ptr;
	// the next four variables are for support of words separated by ~ fixed space symbol
	wxChar* pEndWordProper = NULL;
	wxChar* pSecondWordBegins = NULL;
	wxChar* pSecondWordEnds = NULL;
	bool bMatchedFixedSpaceSymbol = FALSE;

    // We've come to the word proper. We now parse over it. Punctuation might be within it
    // (eg. boy's) - so we parse to a space or backslash or other determinate indicator of
    // the end of the word - such as word final punctuation. How do we distinguish medial
    // from final punctuation? We'll assume that medial punctuation is never a backslash,
    // is most likely to be a single punctuation character (such as a hyphen), or the ~
    // USFM fixedspace character, and that medial space never occurs. So for a punctuation
    // character to be medial, the next character must not be white space nor backslash,
    // and usually it won't be another punctuation character either - but we'll allow one
    // or more of the latter to be medial provided the punctuation character sequence
    // doesn't terminate at a backslash or whitespace. The intention of this section of the
    // parser is to get ptr to point at the first character following the end of the word -
    // which could be punctuation, whitespace, or an inline binding endmarker, or a ]
    // bracket - a later section of this word parser function will deal with those
    // post-word possibilities.
	wxChar* pPunctStart = NULL;
	wxChar* pPunctEnd = NULL;
	bool bStartedPunctParse = FALSE;
    // Better ~ parsing requires we parse word1<puncts2>~<puncts3>word2 when there is a ~
    // fixed space symbol conjoining, within a dedicated function -- and we need a test to
    // determine when ~ is present which gives TRUE or FALSE even though ptr is still
    // pointing at word1 -- so we'll need a function returning bool to parse over word1 and
    // its following <punct2> substring (the latter may be empty) to get to the ~ and
    // return TRUE if ~ is indeed present. Then we can return what we've found, and use the
    // returned bool to have a block in which a 'completion' function parses over
    // <punct3> and word2, ending with ptr pointing at the first character following word2
    // (it could be punctuation, a space, or a marker). Then we can have an else block to
    // do the word parse when no ~ is present. When either block ends, parsing can continue
    // with what follows word, or what follows word2 when there is ~ conjoining. (It is too
    // late to detect the ~ only after the word-parsing loop has been exitted, because we
    // won't be able at that time to get the second word of the conjoined pair parsed.) We
    // don't need any more than 2 extra local variables to store information parsed as
    // side-effects of our test function, and from the completion function. Just
    // inlineBindingEndMkrBeforeFixedSpace, and inlineBindingMkrAfterFixedSpace.
	wxString inlineBindingEndMkrBeforeFixedSpace;
	wxString inlineBindingMkrAfterFixedSpace;
	wxChar* savePtr = ptr;
	/*
	if (pSrcPhrase->m_nSequNumber == 5)
	{
		int break_point_here = 1;
	}
	*/
	// in the next call, if ~ is found, ptr returns pointing at whatever follows it, but
	// if ~ is not found, then ptr returns pointing at whatever pEndWordProper points at
	// (which is usually space, or endmarker, or punctuation)
	bMatchedFixedSpaceSymbol = IsFixedSpaceAhead(ptr, pEnd, pWordProper, pEndWordProper, 
				finalPunctBeforeFixedSpaceSymbol, inlineBindingEndMkrBeforeFixedSpace,
				wordBuildersForPostWordLoc, spacelessPuncts); // the punctuationSet 
												// passed in has all spaces removed
	if (bMatchedFixedSpaceSymbol)
	{
        // It's a pair of words conjoined by ~, so complete the parse of what follows the ~
        // symbol, the IsFixedSpaceAhead() function exits with ptr pointing at the first
        // character following ~
		int nChangeInLenValue = ptr - savePtr;
		len += nChangeInLenValue;
		savePtr = ptr;
		FinishOffConjoinedWordsParse(ptr, pEnd, pSecondWordBegins, pSecondWordEnds,
				precedingPunctAfterFixedSpaceSymbol, inlineBindingMkrAfterFixedSpace,
				newPunctFrom2ndPreWordLoc, newPunctFrom2ndPostWordLoc,
				wordBuildersFor2ndPreWordLoc, wordBuildersFor2ndPostWordLoc,
				spacelessPuncts);
		nChangeInLenValue = ptr - savePtr;
		// if word-building characters have become punctuation, ptr may not be pointing at
		// the end of the word, so augment it by the number of characters in the string
		// newPunctFrom2ndPostWordLoc to get it's correct location
		ptr += newPunctFrom2ndPostWordLoc.Len();
		// update len
		len += nChangeInLenValue; // note, if ptr and pSecondWordBegins point at different
								  // locations, then the number of word-initial characters
								  // which have just become punctuation due to the user just
								  // having changed the punctuation settings, have already 
								  // been included in the update for len, hence we don't do
								  // it again below -- see below for a note to this effect
								  // there also (yep, this stuff is VERY complex)
        // bStartedPunctParse is still FALSE, and we must NOT make it become TRUE here
        // because variables in code blocks below for when it is TRUE are not alive when
        // control comes through here
	}
	else
	{
		// it's not ~ conjoined words, so the word parsing loop begins instead, and ptr
		// will be pointing at the first character past the end of the word (which could
		// be following punctuation for the word), or at a ] if there was no following
		// punctuation for the word, or at buffer end (there could be former punctuation,
		// but now reverted to word-building characters, in wordBuildersForPostWordLoc
		// too, which are being held over for placement on the end of the word once we've
		// defined its string below somewhere)
		int nChangeInLenValue = ptr - savePtr;
		len += nChangeInLenValue;
		// we might have come to a closing bracket, ], and if so we must parse no further
		// but sign off on the current CSourcePhrase, and return to the caller so that
		// its ptr value will point at the ] symbol.
		if (*ptr == _T(']') || *ptr == _T('['))
		{
			// following punctuation won't be present if we exited with ptr pointing at a
			// ] character, so just set pSrcPhrase members accordingly and return len
			// value; similarly, if the input text had a word with no punctuation
			// following it and the next line starts with [\v then we'll get to this point
			// with ptr pointing at [, and so we must finish up the same way in this
			// circumstance and return control to the caller (ie. to TokenizeText())
			wxString theWord(pWordProper,nChangeInLenValue);
			if (!wordBuildersForPreWordLoc.IsEmpty())
			{
				// put the former punct(s) moved to m_precPunct earlier, but now reverted
				// back to word-building characters, back at the start of the word - if
				// there were any such found
				theWord = wordBuildersForPreWordLoc + theWord;
				//wordBuildersForPreWordLoc.Empty(); // make sure it can't be used again
				len += wordBuildersForPreWordLoc.Len();
			}
			if (!wordBuildersForPostWordLoc.IsEmpty())
			{
				// put the former punct(s) moved to m_follPunct earlier, but now reverted
				// back to word-building characters, back at the end of the word - if
				// there were any such found
				theWord += wordBuildersForPostWordLoc;
				len += wordBuildersForPostWordLoc.Len();
			}
			pSrcPhrase->m_key = theWord;
			// now m_srcPhrase except for ending punctuation - of which there is none present
			if (!pSrcPhrase->m_precPunct.IsEmpty())
			{
				pSrcPhrase->m_srcPhrase = pSrcPhrase->m_precPunct;
			}
			pSrcPhrase->m_srcPhrase += theWord;
			return len;
		}
		pEndWordProper = ptr;
		// Note, we may still have exited IsFixedSpaceAhead() because we came to a ]
		// bracket, but having parsed one or more following punctuation characters first.
		// In this case we returned with ptr pointing at word end (i.e. the start of the
		// following punctuation), so we must parse forward again, below, over those
		// punctuation characters a second time to get to the ] character which determines
		// our return point - so that's why we keep looking for presence of ] below
		//while (!IsEnd(ptr) && !IsWhiteSpace(ptr) && (*ptr != gSFescapechar))
		while (!IsEnd(ptr) && !IsWhiteSpace(ptr) && !IsMarker(ptr))
		{
			// check if we are pointing at a punctuation character (it can't be a ]
			// character because we've bled out that possibility in the preceding block)
			if ((nFound = spacelessPuncts.Find(*ptr)) != wxNOT_FOUND)
			{
				// we found a punctuation character - it could be medial, or word final, so we
				// have to parse on to determine which is the case
				if (bStartedPunctParse)
				{
					// we've already found at least one, so set pPunctEnd to the location
					// following the punctuation character we are point at 
					pPunctEnd = ptr + 1;
				}
				else
				{
					// we've not started parsing any punctuation yet, so set the pointer
					// to the word end as determined up to this point & turn on
					// the flag
					pEndWordProper = ptr; // we are potentially at the end of the
										  // first word
					bStartedPunctParse = TRUE;
					pPunctStart = ptr;
					pPunctEnd = ptr + 1;
				}
			}

			// advance over the character we are pointing at, whether it is part of the
			// word or a punctuation character
			ptr++;
			len++;
			
			// check for a closing bracket
			if (*ptr == _T(']'))
			{
				// We may be past some following punctuation and have come to a ] bracket
				// - this must terminate parsing, so a word-medial ] character will split
				// the word into two parts - which probably is the correct behaviour to
				// support, since ] within the body of a word seems crazy. So check for ]
				// and if we are pointing at it, we must terminate the parse, set
				// pSrcPhrase members not yet set, and return len so that the caller's ptr
				// is pointing at the ] bracket.
				size_t punctSpan = (size_t)(ptr - pEndWordProper);
				wxString follPuncts(pEndWordProper,punctSpan);
				pSrcPhrase->m_follPunct = follPuncts;
				wxString theWord(pWordProper,nChangeInLenValue);
				if (!wordBuildersForPreWordLoc.IsEmpty())
				{
					// put the former punct(s) moved to m_precPunct earlier, but now reverted
					// back to word-building characters, back at the start of the word - if
					// there were any such found
					theWord = wordBuildersForPreWordLoc + theWord;
					len += wordBuildersForPreWordLoc.Len();
				}
				if (!wordBuildersForPostWordLoc.IsEmpty())
				{
					// put the former punct(s) moved to m_follPunct earlier, but now reverted
					// back to word-building characters, back at the end of the word - if
					// there were any such found
					theWord += wordBuildersForPostWordLoc;
					len += wordBuildersForPostWordLoc.Len();
				}
				pSrcPhrase->m_key = theWord;
				// now m_srcPhrase except for ending punctuation - of which there is none present
				if (!pSrcPhrase->m_precPunct.IsEmpty())
				{
					pSrcPhrase->m_srcPhrase = pSrcPhrase->m_precPunct;
				}
				pSrcPhrase->m_srcPhrase += theWord;
				pSrcPhrase->m_srcPhrase += pSrcPhrase->m_follPunct;
				return len;
			}
			// are we at the end of word-medial punctuation? (but not at buffer end)
			//if (bStartedPunctParse && !IsWhiteSpace(ptr) && (*ptr != gSFescapechar)
			if (bStartedPunctParse && !IsWhiteSpace(ptr) && !IsMarker(ptr)
				&& (nFound = spacelessPuncts.Find(*ptr)) == wxNOT_FOUND && !IsEnd(ptr))
			{
                // Punctuation parsing had started, we are not pointing at a white space
                // nor a backslash, nor at a punctuation character (nor ]) - so we
                // must be pointing at a character of the word we are parsing... hence the
                // punctuation span was word-internal, so we forget about it
				bStartedPunctParse = FALSE;
				pPunctStart = NULL;
				pPunctEnd = NULL;
				if (bMatchedFixedSpaceSymbol)
				{
					pSecondWordEnds = NULL; // it's not to be set yet
				}
				else
				{
					pEndWordProper = NULL; // it's not to be set yet
				}
			}
		} // end of loop: while (!IsEnd(ptr) && !IsWhiteSpace(ptr) && !IsMarker(ptr))

	} // end of else block for test: if (bMatchedFixedSpaceSymbol)

	// control should exit the above block with ptr pointing at the first character
	// following the last word parsed (word being a stretch of text which is not
	// punctuation, whitespace or a marker)
	if (pEndWordProper == NULL && !bMatchedFixedSpaceSymbol)
	{
		pEndWordProper = ptr;
	}
	if (bMatchedFixedSpaceSymbol && pSecondWordEnds == NULL)
	{
		pSecondWordEnds = ptr;
	}
	if (bMatchedFixedSpaceSymbol)
	{
		// create the children & store them
		pSrcPhrWord1 = new CSourcePhrase;
		pSrcPhrWord2 = new CSourcePhrase;
		pSrcPhrase->m_pSavedWords->Append(pSrcPhrWord1);
		pSrcPhrase->m_pSavedWords->Append(pSrcPhrWord2);
		pSrcPhrase->m_nSrcWords = 2;
		if (bHasPrecPunct)
		{
			pSrcPhrWord1->m_precPunct = pSrcPhrase->m_precPunct;
		}
		// other members of these will get their content from code further below, and
		// more explanatory comments are there too to help complete the picture
	}

    // When we exit the above word-parsing loop, we included punctuation in the parse
    // because it may have been word-medial punctuation, so if bStartedPunctParse is still
    // TRUE, then the punctuation we parsed occurred following the word, or following the
    // second word of a pair of words separated by ~ the fixedspace symbol in USFM. We must
    // store such punctuation in the m_follPunct member of pSrcPhrase (even if we are
    // pointing at the backslash of an inline binding endmarker - that scenario would be a
    // markup error in all likelihood because character formatting and keyword etc markers
    // should not include punctuation in their scope; anyway, we'll store the punctuation
    // and on an export locate it following the endmarker (i.e. auto-correcting and hoping
    // that's really what would be wanted); if there is no inline binding endmarker
    // following then there is no problem -- however, we may have stopped at a space and
    // there could be detached additional punctuation (only closing quotes though) so we
    // must check for any such and append those we find until we come to a non-quote
    // punctuation or an inline non-binding endmarker, or an \f* or \x* or \fe* endmarker.
    // After the latter we have to check for further punctuation too, providing there is no
    // whitespace between the punctuation and the preceding \f* or \x* or \fe* or any other
    // endmarker within m_endMarkers - such 'outer' punctuation we must store in the
    // m_follOuterPunct member. All of the above is complex, so we will tackle it in up to
    // 5 sequential stages, to keep our algorithms comprehensible.
	
	// The first thing to do is handle any following punctuation's storage, and set up
	// m_key and m_srcPhrase members, and take into account the possibility that ~ may
	// conjoin a word pair when doing the latter. We have enough information to set the
	// m_key member, but not the m_srcPhrase member because the latter requires we have
	// determined all the final punctuation and we've not done that yet - so do what we
	// can do now, we can only do the internal part of m_srcPhrase here; however as we get
	// bits of the final punctuation characters, we'll append them to m_srcPhrase as we go.
	size_t length1 = 0;
	size_t length2 = 0;
	if (bMatchedFixedSpaceSymbol)
	{
		// NOTE: we are assuming ~ only ever joins TWO words in sequence, never 3 or more
		length1 = pEndWordProper - pWordProper;
		wxString firstWord(pWordProper,length1);
        // adjustments to firstWord for tranferring a punctuation character or characters
        // which by user action in Preferences have now become word-building and were
        // relocated preceding an inline binding beginmarker and/or following an inline
        // binding endmarker, must be done here and the len value updated; no adjustment is
        // needed if the relevant string returned from IsFixedSpaceAhead() and from a call
        // of SquirrelAwayMovedFormerPuncts() early in ParseWord() has no content
		if (!wordBuildersForPreWordLoc.IsEmpty())
		{
			// put the former punct(s) moved to m_precPunct earlier, but now reverted
			// back to word-building characters, back at the start of the word - if
			// there were any such found
			firstWord = wordBuildersForPreWordLoc + firstWord;
			len += wordBuildersForPreWordLoc.Len();
		}
		if (!wordBuildersForPostWordLoc.IsEmpty())
		{
			// put the former punct(s) moved to m_follPunct earlier, but now reverted
			// back to word-building characters, back at the end of the word - if
			// there were any such found
			firstWord += wordBuildersForPostWordLoc;
			len += wordBuildersForPostWordLoc.Len();
			// don't empty this one, we may need it later below
		}
		length2 = pSecondWordEnds - pSecondWordBegins;
		wxString secondWord(pSecondWordBegins, length2);
        // adjustments to secondWord for tranferring a punctuation character or characters
        // which by user action in Preferences have now become word-building and were
        // relocated preceding an inline binding beginmarker and/or following an inline
        // binding endmarker, must be done here and the len value updated; no adjustment is
        // needed if the relevant string returned from FinishOffConjoinedWordsParse() has
        // no content
		if (!wordBuildersFor2ndPreWordLoc.IsEmpty())
		{
			// put the former punct(s) moved to m_precPunct earlier, but now reverted
			// back to word-building characters, back at the start of the word - if
			// there were any such found
			secondWord = wordBuildersFor2ndPreWordLoc + secondWord;
			//len += wordBuildersFor2ndPreWordLoc.Len(); Don't do this here, because any
			//such characters have already been counted above, because they lie in the span
			//from ptr up to pWord2End, after FinishOffConjoinedWordParse() has returned
		}
		if (!wordBuildersFor2ndPostWordLoc.IsEmpty())
		{
			// put the former punct(s) moved to m_follPunct earlier, but now reverted
			// back to word-building characters, back at the end of the word - if
			// there were any such found
			secondWord += wordBuildersFor2ndPostWordLoc;
			len += wordBuildersFor2ndPostWordLoc.Len();
			// don't empty this one, we may need it later below
		}
		pSrcPhrase->m_key = firstWord;
		pSrcPhrase->m_key += theSymbol; // theSymbol is ~
		pSrcPhrase->m_key += secondWord;
		// an inline binding beginmarker on the parent also needs to be stored on word1
		if (!pSrcPhrase->GetInlineBindingMarkers().IsEmpty())
		{
			pSrcPhrWord1->SetInlineBindingMarkers(pSrcPhrase->GetInlineBindingMarkers());
		}
		// now m_srcPhrase except for ending punctuation
		if (!pSrcPhrase->m_precPunct.IsEmpty())
		{
			pSrcPhrase->m_srcPhrase = pSrcPhrase->m_precPunct;
		}
		pSrcPhrase->m_srcPhrase += firstWord;
		if (!finalPunctBeforeFixedSpaceSymbol.IsEmpty())
		{
			pSrcPhrWord1->m_follPunct = finalPunctBeforeFixedSpaceSymbol;
			// also add it to m_srcPhrase being built up
			pSrcPhrase->m_srcPhrase += finalPunctBeforeFixedSpaceSymbol;
		}
		pSrcPhrase->m_srcPhrase += theSymbol; // theSymbol is ~
		if (!precedingPunctAfterFixedSpaceSymbol.IsEmpty())
		{
			pSrcPhrWord2->m_precPunct = precedingPunctAfterFixedSpaceSymbol;
			// also add it to m_srcPhrase being built up
			pSrcPhrase->m_srcPhrase += precedingPunctAfterFixedSpaceSymbol;
		}
        // BEW added 2Feb11, if a word-building character at secondWord's start has
        // become punctuation, it has to be added to m_precPunct for that word's
        // CSourcePhrase & len incremented; there could be more than one such
		if (!newPunctFrom2ndPreWordLoc.IsEmpty())
		{
			pSrcPhrWord2->m_precPunct += newPunctFrom2ndPreWordLoc;
			//len += newPunctFrom2ndPreWordLoc.Len(); We must not add to the len value
			//like this here - an explanation is appropriate. Back when nChangeInLenValue
			//was calculated, it was computed from the ptr location (at word start before
			//any now-punctuation character due to user's punct changes was saved and
			//skipped over, so nChangeInLenValue already includes the len value for this
			//one or more characters, if in fact ptr and pSecondWordBegins don't point at
			//the same location (if they do, the user didn't make any punct changes that
			//resulted in any character at the start of the second word becoming punctuation)
			
			// also add the new punctuation char(s) to m_srcPhrase being built up
			pSrcPhrase->m_srcPhrase += newPunctFrom2ndPreWordLoc;
		}

		pSrcPhrase->m_srcPhrase += secondWord;
		// add any final punctuation to pSrcPhrase->m_srcPhrase further below
		
		// if we found an inline binding endmarker before the ~, store it - we can only
		// store it in the embedded CSourcePhrase which is first (the parent can't
		// distinguish such medial markers, so we don't try - we don't want a placement
		// dialog to show)
		if (!inlineBindingEndMkrBeforeFixedSpace.IsEmpty())
		{
			pSrcPhrWord1->SetInlineBindingEndMarkers(inlineBindingEndMkrBeforeFixedSpace);
		}
		// if we found an inline binding marker after the ~, store it - we can only
		// store it in the embedded CSourcePhrase which is second
		if (!inlineBindingMkrAfterFixedSpace.IsEmpty())
		{
			pSrcPhrWord2->SetInlineBindingMarkers(inlineBindingMkrAfterFixedSpace);
		}
		
		// next, the parent, pSrcPhrase, now needs to have the m_key and m_srcPhrase
		// members set on each of its children instances, in case the user unmerges later
		// on
		pSrcPhrWord1->m_key = firstWord;
		pSrcPhrWord2->m_key = secondWord;
		pSrcPhrWord1->m_srcPhrase = pSrcPhrase->m_precPunct; // parent's m_precPunct
		pSrcPhrWord1->m_srcPhrase += firstWord;
		pSrcPhrWord1->m_srcPhrase += finalPunctBeforeFixedSpaceSymbol;
        // now for secondWord, after the ~ symbol, except we don't know about any possible
        // final puncts on the former as yet
		pSrcPhrWord2->m_srcPhrase = precedingPunctAfterFixedSpaceSymbol;
		// append any former word-initial character just now having become punctuation but
		// don't increment len value because we've counted it above already
		if(!newPunctFrom2ndPreWordLoc.IsEmpty())
		{
			pSrcPhrWord2->m_srcPhrase += newPunctFrom2ndPreWordLoc;
		}
		pSrcPhrWord2->m_srcPhrase += secondWord;

		// add any final punctuation to pSrcPhrWord2->m_srcPhrase further below; however,
		// if the user just changed the punctuation settings such that the end of the
		// secondWord had one or more word-building characters that have just become final
		// punctuation characters, they will be in newPunctFrom2ndPostWordLoc and will
		// need to be put at the start of secondWord's CSourcePhrase's m_follPunct member,
		// and len incremented so that the parsing pointer is in sync with what was parsed
		// over - then later, the normal puncts (if any) can be appended
		if (!newPunctFrom2ndPostWordLoc.IsEmpty())
		{
			len += newPunctFrom2ndPostWordLoc.Len();
			pSrcPhrWord2->m_follPunct = newPunctFrom2ndPostWordLoc;
			// don't empty newPunctFrom2ndPostWordLoc in case we need it later to
			// increment ptr value without doing a further len increment
			
			// append it also to pSrcPhrase->m_srcPhrase which is being built up, and also
			// to pSrcPhrWord2->m_srcPhrase, which will then have further parsed following
			// punctuation appended in the blocks below if there is more present in the buffer
			pSrcPhrWord2->m_srcPhrase += newPunctFrom2ndPostWordLoc;
			pSrcPhrase->m_srcPhrase += newPunctFrom2ndPostWordLoc;
		}
		// At this point, the tweaks for the user's changing non-puncts to puncts, or
		// adding a new punct, are done -- only potentially more post-word stuff, such as
		// endmarkers and more punctuation is to be checked for below, as now ptr is in
		// sync with len
	}
	else // bMatchedFixedSpaceSymbol is FALSE
	{
		length1 = pEndWordProper - pWordProper;
		wxString theWord(pWordProper,length1);
		if (!wordBuildersForPreWordLoc.IsEmpty())
		{
			// put the former punct(s) moved to m_precPunct earlier, but now reverted
			// back to word-building characters, back at the start of the word - if
			// there were any such found
			theWord = wordBuildersForPreWordLoc + theWord;
			len += wordBuildersForPreWordLoc.Len();
			wordBuildersForPreWordLoc.Empty(); // make sure it can't be used again
		}
		if (!wordBuildersForPostWordLoc.IsEmpty())
		{
			// put the former punct(s) moved to m_follPunct earlier, but now reverted
			// back to word-building characters, back at the end of the word - if
			// there were any such found
			theWord += wordBuildersForPostWordLoc;
			len += wordBuildersForPostWordLoc.Len();
			// don't delete the wordBuildersForPostWordLoc string, we need it later in
			// order to get ptr to skip any characters in it when our parsing ptr gets to
			// be just past the last inline binding endmarker, if any
		}
		pSrcPhrase->m_key = theWord;
		// now m_srcPhrase except for ending punctuation
		if (!pSrcPhrase->m_precPunct.IsEmpty())
		{
			pSrcPhrase->m_srcPhrase = pSrcPhrase->m_precPunct;
		}
		pSrcPhrase->m_srcPhrase += theWord;
		// add any final punctuation further below
	} // end of else block for test: if (bMatchedFixedSpaceSymbol)

	bool bThrewAwayWhiteSpaceAfterWord = FALSE;
	if (bStartedPunctParse)
	{
        // Parsing of final punctuation started while parsing the word proper, so it is
        // likely that there is no inline binding endmarker after the word. We'll not
        // assume that however, and so if there is we'll again check for punctuation after
        // it and append to any collected beforehand (that means that export would put both
        // lots after the inline binding marker, where we think it should be anyway)
		size_t length = pPunctEnd - pPunctStart;
		wxString finalPunct(pPunctStart,length);
		pSrcPhrase->m_follPunct += finalPunct;
		pSrcPhrase->m_srcPhrase += finalPunct;  // append what we have so far, more									   									  
												// may be appended further below
		if (IsFixedSpaceSymbolWithin(pSrcPhrase))
		{
			pSrcPhrWord2->m_srcPhrase += finalPunct;
			pSrcPhrWord2->m_follPunct += finalPunct;
		}

        // Collect any additional detached quotes and intervening spaces, stopping at the
        // next marker, or the first non-endquote punctuation character. If straight quote
        // or straight doublequote (' or ") is found, it would be ambiguous whether it is
        // an opening quote for the next word in the buffer, or part of the final
        // punctuation on the currently parsed over word - so we'll distinguish these as
        // follows: straight quote or doublequote before an endmarker will be part of the
        // current following punctuation; and any such immediately after \f* or \x* with no
        // intervening space will be treated as belonging to m_follOuterPunct, in any other
        // situation the straight quote will be assumed to be initial punctuation for what
        // follows and so will not be part of the data for this call of ParseWord()
        bExitParseWordOnReturn = FALSE;
		wxString additions; additions.Empty();
		// in next call, FALSE is bPutInOuterStorage
		len = ParseAdditionalFinalPuncts(ptr, pEnd, pSrcPhrase, spacelessPuncts, len, 
					bExitParseWordOnReturn, m_bHasPrecedingStraightQuote, additions, FALSE);
		if (IsFixedSpaceSymbolWithin(pSrcPhrase) && !additions.IsEmpty())
		{
			pSrcPhrWord2->m_follPunct += additions;
			pSrcPhrWord2->m_srcPhrase += additions;
		}
		if (bExitParseWordOnReturn)
		{
			// m_srcPhrase has been updated with any additional final punctuation within
			// the above call, so just return len to the caller 
			return len;
		} 
	} // end of TRUE block for test: if (bStartedPunctParse)
	else
	{
		// Parsing over punctuation was not commenced while parsing the word proper, so
		// ptr may now be pointing at whitespace, or a marker (endmarker or, although very
		// unlikely, even a beginmarker). Whichever is the case, if pointing at
		// whitespace, it is not required for anything, and we can parse over it and throw
		// it away until we come to something we need to deal with.
		int saveLen = len;
		len = ParseOverAndIgnoreWhiteSpace(ptr, pEnd, len);
		if (len > saveLen)
		{
			// some whites were thrown away
			bThrewAwayWhiteSpaceAfterWord = TRUE; // if FALSE, code below knows we've 
							// started into parsing of following puncts & endmarkers
		}
	}

	// Before we check for markers, we can get to this point having parsed a word which
	// has a space following it. There could be detached punctuation now, which belongs to
	// the start of the next word to be parsed, so before we assume we are dealing with
	// endmarkers for the current word, check this out and return if so. Check for ] too.
	if (*ptr == _T(']'))
	{
		return len;
	}
	if (bThrewAwayWhiteSpaceAfterWord)
	{
		// the most likely thing is that we've halted at punctuation or begin marker for
		// the next word to be parsed, or the start of the next word itself. But if we
		// have come to closing quotes or endmarkers, we'll let parsing continue on the
		// assumption the user had a bogus space after the word which shouldn't be there
		if (!IsClosingCurlyQuote(ptr) && !IsEndMarker(ptr, pEnd))
		{
			// if it's opening quotes punctuation, then definitely return
			if (IsOpeningQuote(ptr))
			{
				return len;
			}
			else
			{
                // some languages can have opening punctuation other than quotes, so if it
                // is other punctuation characters, the situation is ambiguous. Our
                // solution is to look ahead for the first char past the puncts, if that is
                // an endmarker or a ] closing bracket we'll keep the punctuation as
                // following puncts for the current pSrcPhrase, but any other option -
                // we'll just return without advancing over the puncts.
				if (spacelessPuncts.Find(*ptr) != wxNOT_FOUND)
				{
					// it's some sort of nonquote punctuation (IsOpeningQuote() call above
					// returns true even if straight quote or double quote is matched)
					wxChar* ptr2 = ptr;
					int countThem = 0;
					while (spacelessPuncts.Find(*ptr2) != wxNOT_FOUND && *ptr2 != _T(']'))
					{
						ptr2++;
						countThem++;
					}
					if (IsEndMarker(ptr2, pEnd) || *ptr2 == _T(']'))
					{
						// we'll treat is as following puncts for our current pSrcPhrase
						// and return if we came to ], otherwise parse on...
						wxString finalPuncts(ptr, countThem);
						pSrcPhrase->m_follPunct += finalPuncts;
						pSrcPhrase->m_srcPhrase += finalPuncts;
						// also do it for the secondWord's CSourcePhrase when ~ is conjoining
						if (IsFixedSpaceSymbolWithin(pSrcPhrase))
						{
							pSrcPhrWord2->m_follPunct += finalPuncts;
							pSrcPhrWord2->m_srcPhrase += finalPuncts;
						}
						len += countThem;
						ptr += countThem;
						if (*ptr == _T(']'))
						{
							return len;
						}
						// let processing continue within this call of ParseWord()
					}
					else
					{
						// Nah, must belong to what follows on next ParseWord() call
						return len;
					}
				}
				else
				{
					// if we enter here, the usual scenario is that a word doesn't have
					// any following punctuation and we've just parsed over white space -
					// we could return now, except that it wouldn't be apppropriate to do
					// so if what we are pointing at is an endmarker which ends a TextType
					// (such as a footnote, endnote or crossReference in USFM, or a
					// footnote in PNG 1998 SFM set). The code for detecting the endmarker
					// in the caller uses that detection to end the TextType for the
					// earlier stuff, change to the next TextType, or if there's no begin
					// marker present, reinstore 'verse' and m_bSpecialText = FALSE for
					// the CSourcePhrase instances which will follow. So we must check for
					// the endmarker here (we care only about m_endMarkers content here,
					// because only what's in that member can disrupt the TextType
					// propagating in order to start a new value thereafter) and make sure
					// it gets stored before we return
					wxString theEndMarker;
					theEndMarker.Empty();
					if (IsEndMarkerRequiringStorageBeforeReturning(ptr, &theEndMarker))
					{
						int aLength = theEndMarker.Len();
						ptr += aLength;
						len += aLength;
						// get the marker stored
						pSrcPhrase->AddEndMarker(theEndMarker);
					}
					return len;
				}
			}
		} // end of TRUE block for test: if (!IsClosingCurlyQuote(ptr) && !IsEndMarker(ptr, pEnd))
	} // end of TRUE block for test: if (bThrewAwayWhiteSpaceAfterWord)

    // Next, there might be an inline binding endmarker (or even a pair of them), parse
    // over these and store them in m_inlineBindingEndMarkers if so. Any punctuation
    // between such can be thrown away. Alternatively, there may be endmarkers internal to
    // a footnote, endnote or crossReference which are to be parsed over, and stored in
	// m_endMarkers. Handle these possibilities. A ] bracket may follow the former, check
	// and if so, return with ptr pointing at it.
    bool bInlineBindingEndMkrFound = FALSE;
	if (IsEndMarker(ptr, pEnd))
	{
		// there could be more than one in sequence, so loop to collect each
		int saveLen;
		wxString endMarker; // initialized to empty string at start of ParseInlineEndMarkers()
		do {
			saveLen = len;
			bool bBindingEndMkrFound = FALSE;
			len = ParseInlineEndMarkers(ptr, pEnd, pSrcPhrase, 
										gpApp->m_inlineNonbindingEndMarkers, len, 
										bBindingEndMkrFound, endMarker);
			if (bBindingEndMkrFound)
			{
				bInlineBindingEndMkrFound = TRUE; // preserve TRUE value for when loop exits
			}
			// handle ~ conjoining
			if (IsFixedSpaceSymbolWithin(pSrcPhrase) && !endMarker.IsEmpty())
			{
				// there are child pSrcPhrase instances to update
				if (bBindingEndMkrFound)
				{
					// store the binding endmarker in m_inlineBindingEndMkr
					pSrcPhrWord2->AppendToInlineBindingEndMarkers(endMarker);
				}
				else
				{
					// store the footnote or endnote or crossReference internal endmarker
					// in the m_endMarkers member
					pSrcPhrWord2->AddEndMarker(endMarker);
				}
			}
			// check for ] marker, if we are pointing at it, update len and return
			if (*ptr == _T(']'))
			{
				return len;
			}
            // if we found an endmarker, there might be a bogus space following it which is
            // entirely unneeded and should be omitted, because any whitespace after an
            // inline binding endmarker is a nuisance - so check for it and remove it,
            // because if another follows, the loop must start with ptr pointing at a
            // backslash
			if (bBindingEndMkrFound && IsWhiteSpace(ptr))
			{
				len = ParseOverAndIgnoreWhiteSpace(ptr, pEnd, len); 
			}
		} while (len > saveLen); // repeat as long as there was ptr advancement each time

        // Once we have got past the one or more endmarkers, there could be following
        // punctuation which we try parse over in the code below. However, if a punctuation
        // character has reverted to being word-building, and it got moved to
        // post-endmarker position, it will have been restored (and counted in the len
        // value update done earlier) to its post word location. But the ParseWord()
        // parsing pointer ptr will now be pointing at however many such there are - it or
        // they, as the case may be, are still stored in the wordBuildersForPostWordLoc
        // string, which because we haven't returned, is still not emptied. So here we must
        // get the length of what we put into the wordBuildersForPostWordLoc string from
        // the character sequence and advance ptr by that much, and if it is we advance
        // over it WITHOUT INCREMENTING the len value - as that is already done above;
        // after this block is done, ptr must be left pointing at any genuine punctuation,
        // or space or whatever
        if (!wordBuildersForPostWordLoc.IsEmpty())
		{
			ptr += wordBuildersForPostWordLoc.Len();

		}
		// do the same adjustment if we are at this point having parsed the secondWord of
		// a conjoined word pair, and there is punctuation that has be reverted back to
		// being word-building and it occurs after the endmarker
		if (IsFixedSpaceSymbolWithin(pSrcPhrase))
		{
			if (!wordBuildersFor2ndPostWordLoc.IsEmpty())
			{
				ptr += wordBuildersFor2ndPostWordLoc.Len();

			}
		}	
	}

	// What now? Final punctuation may already have been collected and stored. But if we
	// just parsed over at least one inline binding endmarker, this type of marker should
	// preceded final punctuation, so we would need to have a further attempt to collect
	// final punctuation now - because if there are final puncts, that's where we'd expect
	// them to be. (If there's final puncts before the inline binding marker, wrongly, and
	// also some after it, then we'll just concatenate all those puncts to the one storage
	// location on pSrcPhrase, and so in an export they'd all be shown after the inline
	// binding marker (thereby auto-correcting, or so we think it should be done). If,
	// however, we are pointing at whitespace, we have a tricky situation. It could
	// indicate that parsing for the current word should end. On the other hand, it might
	// just be bad markup and either an inline non-binding endmarker (such as \wj*) might
	// follow (and the latter could conceivably even have punctuation before it which
	// would need to then be treated as final punctuation for the current word; or there
	// could be \f* \x* or \fe* still to come -- in these cases the whitespace is
	// unnecessary and probably unhelpful and should be ignored. This calls for some
	// tricky parsing and delays of the decisions about what to do until all the ducks are
	// lined up.
	//wxChar* pWhiteSpaceStarts = ptr; 
	//pWhiteSpaceStarts = pWhiteSpaceStarts; // avoid compiler warning
	wxChar* pWhiteSpaceEnds = ptr;
	int nWhiteSpaceSpan = 0;
	if (IsWhiteSpace(ptr))
	{
		// delineate the span of this substring of whitespace
		ptr++;
		pWhiteSpaceEnds = ptr;
		nWhiteSpaceSpan = 1;
		while (IsWhiteSpace(ptr))
		{
			ptr++;
			nWhiteSpaceSpan++;
			pWhiteSpaceEnds = ptr;
		}
		// delay our decision about what to do with this whitespace, but if we halted at ]
		// then return and throw away this whitespace, but return updated len value so
		// that caller's ptr will be pointing at the ] bracket
		if (*ptr == _T(']'))
		{
			len += nWhiteSpaceSpan;
			return len;
		}
	}
	bool bGotSomeMorePuncts = FALSE; // set TRUE if we find some more in the next bit

	//If ptr points at punctuation, start collecting it
	bool bMorePunctsHaveBeenAccepted = FALSE;
	wxChar* pMorePunctsStart = ptr;
	wxChar* pMorePunctsEnd = ptr;
	int nMorePunctsSpan = 0;
    // Note, if parsing of the 'following' punctuation was not commenced in the main loop
    // above -- as will be the case when bMatchedFixedSpaceSymbol is TRUE - as the main
    // loop is then exited and expects punct parsing to completed later on, it is here
    // that that punctuation first gets a chance to be parsed. The algorithm here was no
    // designed around supporting ~ conjoining's needs, and so if we parse punctuation
    // following the second word of such a conjoining in the following block, we'll
    // need, once the parsing of the puncts halts, a test for bMatchedFixedSpaceSymbol ==
    // TRUE in order to assign the matched final punctuation to the pSrcPhrase->m_follPunct
    // member, and to the same member in pSrcPhrWord2 - otherwise the ParseWord() function
    // will end without doing these assignments
	if (spacelessPuncts.Find(*ptr) != wxNOT_FOUND)
	{
		// ptr is pointing at punctuation. It could belong to the next word to be parsed,
		// as preceding punctuation for it, or to the current word as final punctuation if
		// an endmarker follows - so collect the punctuation but delay our decision about
		// what to do with it until we can work out later if an endmarker follows or not.
		// But if we halt at a ] bracket, any punctuation parsed over is deemed to belong
		// to the current word being parsed.
		nMorePunctsSpan = 1;
		ptr++;
		pMorePunctsEnd = ptr;
		bGotSomeMorePuncts = TRUE;
		while (spacelessPuncts.Find(*ptr) != wxNOT_FOUND && ptr < pEnd && *ptr != _T(']'))
		{
			nMorePunctsSpan++;
			ptr++;
			pMorePunctsEnd = ptr;
		}
		// if we got to the buffer end, accept the punctuation, store and return; likewise
		// if we got to a ] bracket
		if (ptr == pEnd || *ptr == _T(']'))
		{
			wxString moreFinalPuncts(pMorePunctsStart,nMorePunctsSpan);
			pSrcPhrase->m_follPunct += moreFinalPuncts;
			pSrcPhrase->m_srcPhrase += moreFinalPuncts;
			// also do it for the secondWord's CSourcePhrase when ~ is conjoining
			if (IsFixedSpaceSymbolWithin(pSrcPhrase))
			{
				pSrcPhrWord2->m_follPunct += moreFinalPuncts;
				pSrcPhrWord2->m_srcPhrase += moreFinalPuncts;
			}
			len += nMorePunctsSpan;
			if (nWhiteSpaceSpan > 0)
				len += nWhiteSpaceSpan;
			return len;
		}
        // we could now be pointing at white space, and there may be detached endquotes we
        // need to collect as final punctuation too - but if that is the case, there should
        // not have been any whitespace preceding the collected punctuation - that is,
        // nWhiteSpaceSpan should be zero, AND bInlineBindingEndMkrFound should be TRUE. So
        // if those tests are satisfied, then we'll accept the punctuation as valid, and
        // attempt to find any detached endquotes; but if both are not satisfied, we won't
        // attempt to find detached endquotes as in all probability there should not be any
        // (and if there were, the USFM markup is badly formed) and defer any decisions
        // until we know the situation with possible following endmarkers. 
        // We also do this block if we've been handling a ~ conjoined word pair and have
        // just parsed some final punctuation for the second word of the pair
		if ((nWhiteSpaceSpan == 0 && bInlineBindingEndMkrFound) || bMatchedFixedSpaceSymbol)
		{
			wxString moreFinalPuncts(pMorePunctsStart,nMorePunctsSpan);
            // add the extra final puncts
			pSrcPhrase->m_follPunct += moreFinalPuncts;
			pSrcPhrase->m_srcPhrase += moreFinalPuncts;
			// also do it for the secondWord's CSourcePhrase when ~ is conjoining
			if (IsFixedSpaceSymbolWithin(pSrcPhrase))
			{
				pSrcPhrWord2->m_follPunct += moreFinalPuncts;
				pSrcPhrWord2->m_srcPhrase += moreFinalPuncts;
			}
			bMorePunctsHaveBeenAccepted = TRUE; // this block can leave ptr at a
					// different location, but that shouldn't matter. We need this
					// boolean so we can later prevent storing the extra punctuation twice

			// now attempt any detached ones - we'll have to reset ptr for this parse
			bExitParseWordOnReturn = FALSE;
			int nOldLen = len;
			wxString additions; additions.Empty();
			// in next call, FALSE is bPutInOuterStorage
			len = ParseAdditionalFinalPuncts(ptr, pEnd, pSrcPhrase, spacelessPuncts, len, 
					bExitParseWordOnReturn, m_bHasPrecedingStraightQuote, additions, FALSE);
			if (IsFixedSpaceSymbolWithin(pSrcPhrase) && !additions.IsEmpty())
			{
				pSrcPhrWord2->m_follPunct += additions;
				pSrcPhrWord2->m_srcPhrase += additions;
			}
			// m_srcPhrase has been updated with any additional final punctuation within
			// the above call, so just return len to the caller after updating it
			if (bExitParseWordOnReturn)
			{
				// since we must now return, the tentative former decisions have to become
				// concrete, so do the arithmetic to get len value correct (if not
				// returning here, the arithmetic is done further down in ParseWord() when
				// more is known about endmarkers etc)
				if (nWhiteSpaceSpan > 0)
				{
					len += nWhiteSpaceSpan;
				}
				if (bMorePunctsHaveBeenAccepted && nMorePunctsSpan > 0)
				{
					len += nMorePunctsSpan;
				}
				return len;
			}
			if (len > nOldLen)
			{
				pMorePunctsEnd = ptr; // may need this value below, so make sure it is correct
			}
		}
	} // end of TRUE block for test: if (spacelessPuncts.Find(*ptr) != wxNOT_FOUND)

	// we might now be pointing at inline non-binding endmarker, or the endmarker for a
	// footnote, endnote or crossReference. There might be a small possibility that we get
	// here with ptr pointing at whitespace - just in case, remove any such before doing
	// our tests for the above
	bool bNotEither = FALSE; // default value (yes, it should be false for the default)
	int olderLen = len;
	pMaybeWhitesStart = ptr; // this will be null if control bypasses here
	pMaybeWhitesEnd = ptr; // this will be null only if control bypasses here
	if (IsWhiteSpace(ptr))
	{
		len = ParseOverAndIgnoreWhiteSpace(ptr, pEnd, len);
		nHowManyWhites = len - olderLen;
		if (nHowManyWhites > 0)
		{
			pMaybeWhitesEnd = ptr; // preserve the location of the end
		}
	}
	if (IsFootnoteOrCrossReferenceEndMarker(ptr)) // this test includes a check for endnote endmarker
	{
		// store it in m_endMarkers, and then check for any outer following punctuation
		// immediately following it (must be IMMEDIATELY following, no white space
		// between) and parse over any such as 'outer' following punctuation until a space
		// or non-punct character is reached (we can't assume it is endquotes, it could be
		// anything). Before we do that, our delayed decisions above are to be delayed no
		// longer - we must accept any 'more puncts' that we found, and throw away any
		// whitespace we parsed over preceding them
		if (nWhiteSpaceSpan > 0)
		{
			// ignore it, but add the count of its whitespace characters to len
			len += nWhiteSpaceSpan;
		}
		if (nHowManyWhites > 0)
		{
			len += nHowManyWhites;
		}
		if (bGotSomeMorePuncts && nMorePunctsSpan > 0)
		{
			len += nMorePunctsSpan;
		}
		if (bGotSomeMorePuncts && !bMorePunctsHaveBeenAccepted)
		{
			wxString moreFinalPuncts(pMorePunctsStart,nMorePunctsSpan);
			pSrcPhrase->m_follPunct += moreFinalPuncts;
			pSrcPhrase->m_srcPhrase += moreFinalPuncts;
			len += nMorePunctsSpan;
			if (IsFixedSpaceSymbolWithin(pSrcPhrase))
			{
				pSrcPhrWord2->m_follPunct += moreFinalPuncts;
				pSrcPhrWord2->m_srcPhrase += moreFinalPuncts;
			}
		}
		// now the \f* or \x* or \fe*
		wxString endMarker = GetWholeMarker(ptr);
		int itsLen = endMarker.Len();
		pSrcPhrase->AddEndMarker(endMarker);
		len += itsLen;
		ptr += itsLen;
		// now any outer punctuation...
		// *********** NOTE PROTOCOL CHANGE HERE ****************
		// Instead of just collecting until a space, there may be detached puncts after
		// \f* or \x* (or \fe*) and so we should check, and there could even be an inline
		// non-binding marker like \wj* following that, so we'll need to do the call for
		// trying to collect additional detached puncts and check for a non-binding inline
		// endmarker too. So the same delay tactics apply as we did earlier
		// ******   end note about protocol change   ********
		
		if (spacelessPuncts.Find(*ptr) != wxNOT_FOUND && *ptr != _T(']'))
		{
			// there is at least one, parse over as many as their are until non-punct
			// character is found, store the result in m_follOuterPunct
			wxString outerPuncts; outerPuncts.Empty();
			while (spacelessPuncts.Find(*ptr) != wxNOT_FOUND && *ptr != _T(']'))
			{
				outerPuncts += *ptr;
				ptr++;
				len++;
			}
			pSrcPhrase->SetFollowingOuterPunct(outerPuncts);
			pSrcPhrase->m_srcPhrase += outerPuncts;
			if (IsFixedSpaceSymbolWithin(pSrcPhrase))
			{
				pSrcPhrWord2->SetFollowingOuterPunct(outerPuncts);
				pSrcPhrWord2->m_srcPhrase += outerPuncts;
			}
		}
		if (*ptr == _T(']'))
		{
			// we must return
			return len;
		}
		// now handle the possibility of more (outer) detached punctuation characters,
		// before a further endmarker....
		bool bPutInOuterStorage = TRUE;

        // Collect any additional detached quotes and intervening spaces, stopping at the
        // next marker, or the first non-endquote punctuation character. If straight quote
        // or straight doublequote (' or ") is found, it would be ambiguous whether it is
        // an opening quote for the next word in the buffer, or part of the final
        // punctuation on the currently parsed over word - so we'll distinguish these as
        // follows: straight quote or doublequote before an endmarker will be part of the
        // current following punctuation; and any such immediately after \f* or \x* with no
        // intervening space will be treated as belonging to m_follOuterPunct, in any other
        // situation the straight quote will be assumed to be initial punctuation for what
        // follows and so will not be part of the data for this call of ParseWord()
        bExitParseWordOnReturn = FALSE;
		wxString additions; additions.Empty();
		len = ParseAdditionalFinalPuncts(ptr, pEnd, pSrcPhrase, spacelessPuncts, len, 
						bExitParseWordOnReturn, m_bHasPrecedingStraightQuote, additions, 
						bPutInOuterStorage);
		if (IsFixedSpaceSymbolWithin(pSrcPhrase) && !additions.IsEmpty() &&
			!bExitParseWordOnReturn)
		{
			pSrcPhrWord2->m_follPunct += additions;
			pSrcPhrWord2->m_srcPhrase += additions;
		}
		if (bExitParseWordOnReturn)
		{
			// m_srcPhrase has been updated with any additional final punctuation within
			// the above call, so just return len to the caller; ptr is updated too
			return len;
		} 

		// if we didn't return to the caller, then there must be an inline non-binding
		// endmarker to grab - get it, store, update len and ptr & then return
		if (IsEndMarker(ptr, pEnd))
		{
			wxString endmkr = GetWholeMarker(ptr);
			wxString endmkrPlusSpace = endmkr + aSpace;
			if (inlineNonbindingEndMrks.Find(endmkrPlusSpace) != wxNOT_FOUND)
			{
				wxString alreadyGot = pSrcPhrase->GetInlineNonbindingEndMarkers();
				alreadyGot += endmkr;
				pSrcPhrase->SetInlineNonbindingEndMarkers(alreadyGot);
				int length = endmkr.Len();
				len += length;
				ptr += length;
			}
		}
		// IsEndMarker() knows to halt at ] so if that happened, ptr and len will be such
		// that the caller's ptr will point at the ] bracket character
		return len;
	}
	else if (IsMarker(ptr))
	{
		if (IsEndMarker(ptr,pEnd))
		{
			wxString wholeMkr = GetWholeMarker(ptr);
			wxString wholeMkrPlusSpace = wholeMkr + aSpace;
			if (inlineNonbindingEndMrks.Find(wholeMkrPlusSpace) != wxNOT_FOUND)
			{
				// There is an inline non-binding endmarker, so it needs to be parsed and
				// stored on pSrcPhrase now; and after this we assume any punctuation will
				// belong to whatever word follows as part of its preceding punctuation.
				//   ****************** NOTE ADDITIONAL PROTOCOL NOTE *****************
				// However, life is never simple. It is possible to get \qt* following by
				// punctuation followed by \wj*, that is two inline non-binding endmarkers
				// with intervening space. So we must delay our choice again, parse any
				// following punctuation until we halt at non-punctuation, and if that
				// non-punctuation is another inline non-binding endmarker, we must store
				// on the current pSrcPhrase - and the punctuation before it must go in
				// m_follOuterPunct. On an export, we must check for two inline
				// non-binding endmarkers in m_inlineNonbindingEndMarkers; and if there
				// are such, and m_follOuterPunct has punctuation in it, we will assume
				// that always it belongs between any such pair, and no spaces are to go
				// in the export text there.
				// BEW 2Dec10 -- having written the above, I don't intend to ever
				// implement this in exports until someone complains. I think the
				// possibility that there would be following punctuation in such an
				// environment would be zero. It would be a USFM markup error if it were
				// so. 
				//             ********* END PROTOCOL NOTE ***********
                // So, having entered this block, now we know that any of the 'more
                // punctuation' from earlier one where we delayed our decision, if there is
                // some, we must accept it all as belonging to our current pSrcPhrase as
                // following punctuation - so deal with that first before parsing over the
                // endmarker we just found.
				if (nWhiteSpaceSpan > 0)
				{
					// ignore it, but add the count of its whitespace characters to len
					len += nWhiteSpaceSpan;
				}
				if (nHowManyWhites > 0)
				{
					len += nHowManyWhites;
				}
				if (bGotSomeMorePuncts && nMorePunctsSpan > 0)
				{
					len += nMorePunctsSpan;
				}
				if (bGotSomeMorePuncts && !bMorePunctsHaveBeenAccepted)
				{
					wxString moreFinalPuncts(pMorePunctsStart,nMorePunctsSpan);
					pSrcPhrase->m_follPunct += moreFinalPuncts;
					pSrcPhrase->m_srcPhrase += moreFinalPuncts;
					len += nMorePunctsSpan;
					if (IsFixedSpaceSymbolWithin(pSrcPhrase))
					{
						pSrcPhrWord2->m_follPunct += moreFinalPuncts;
						pSrcPhrWord2->m_srcPhrase += moreFinalPuncts;
					}
				}
				// now the inline non-binding endmarker just found
				int itsLen = wholeMkr.Len();
				pSrcPhrase->SetInlineNonbindingEndMarkers(wholeMkr);
				if (IsFixedSpaceSymbolWithin(pSrcPhrase))
				{
					pSrcPhrWord2->SetInlineNonbindingEndMarkers(wholeMkr);
				}
				len += itsLen;
				ptr += itsLen;
				// check for ptr pointing at ] bracket, return if it is
				if (*ptr == _T(']'))
				{
					return len;
				}
				// as noted above, check for additional punctuation, but no spaces must be
				// before it; and after it, that's the end unless there is another inline
				// non-binding endmarker, and if so then after that is the end!
				wxChar* pLastPunctsStart = ptr;
				wxChar* pLastPunctsEnd = ptr;
				int nCountLastPuncts = 0;
				while (spacelessPuncts.Find(*ptr) != wxNOT_FOUND && ptr < pEnd
						&& *ptr != _T(']'))
				{
					nCountLastPuncts++;
					ptr++;
					pLastPunctsEnd = ptr;
				}
				// if the halt was because we came to a ] bracket, accumulate the
				// punctuation and return
				if (*ptr == _T(']'))
				{
					if (nCountLastPuncts > 0)
					{
						wxString lastPuncts(pLastPunctsStart,nCountLastPuncts);
						wxString outers = pSrcPhrase->GetFollowingOuterPunct();
						outers += lastPuncts;
						pSrcPhrase->SetFollowingOuterPunct(outers);
						pSrcPhrase->m_srcPhrase += lastPuncts; // update m_srcPhrase for the view
						len += nCountLastPuncts; // ptr is already at this location
					}
					return len;
				}
				// not pointing at ] bracket, so check... did we get to a second endmarker?
				if (IsEndMarker(ptr, pEnd))
				{
					wxString wholeMkr = GetWholeMarker(ptr);
					wxString wholeMkrPlusSpace = wholeMkr + aSpace;
					if (inlineNonbindingEndMrks.Find(wholeMkrPlusSpace) != wxNOT_FOUND)
					{
						// it's a second non-binding endmarker, deal with it & any
						// preceding punctuation (store as outer punctuation)
						if (nCountLastPuncts > 0)
						{
							wxString lastPuncts(pLastPunctsStart,nCountLastPuncts);
							wxString outers = pSrcPhrase->GetFollowingOuterPunct();
							outers += lastPuncts;
							pSrcPhrase->SetFollowingOuterPunct(outers);
							pSrcPhrase->m_srcPhrase += lastPuncts; // update m_srcPhrase for the view
							len += nCountLastPuncts; // ptr is already at this location
						}
						// now the final inline non-binding endmarker
						int length = wholeMkr.Len();
						wxString enders = pSrcPhrase->GetInlineNonbindingEndMarkers();
						enders += wholeMkr;
						pSrcPhrase->SetInlineNonbindingEndMarkers(enders);
						len += length;
						ptr += length;
						return len;
					}
					else
					{
						// it' an endmarker, but not an inline non-binding one, this is probably
						// a USFM markup error - so just return and let the caller deal
						// with it
						ptr = pLastPunctsStart;
						return len;
					}
				} // end of TRUE block for test: if (IsEndMarker(ptr, pEnd)

				return len;
			} // end of TRUE block for test: 
			  // if (inlineNonbindingEndMrks.Find(wholeMkrPlusSpace) != wxNOT_FOUND)
			else
			{
				// dunno what is going on, posibly a markup error, so end the ParseWord()
				// parse
				bNotEither = TRUE;
			}
		} // end of TRUE block for test: if (IsEndMarker(ptr,pEnd))
		else
		{
			bNotEither = TRUE;
		}
	} // end of TRUE block for test: else if (IsMarker(ptr))
	if (bNotEither)
	{
		// our delayed decisions above need be delayed no longer - establish now where ptr
		// should be and what value len should have, and return len
		if (bMorePunctsHaveBeenAccepted)
		{
			// if this is the case, we have to count the white space and the 'more puncts'
			// in the parse span, and return with ptr pointing at pMorePunctsEnd
			len += nWhiteSpaceSpan;
			len += nHowManyWhites;
			len += nMorePunctsSpan;
			ptr = pMorePunctsEnd;
			return len;
		}
		else
		{
			// we can't risk including any 'more punctuation' in pSrcPhrase because it is
			// far more likely it is preceding punctuation for the next word, so end the
			// parse where the white space ends
			ptr = pWhiteSpaceEnds; // ptr is at least here
			len += nWhiteSpaceSpan; // count the white space (remember, could be zero)
			if (nHowManyWhites > 0 && pMaybeWhitesEnd != NULL)
			{
				len += nHowManyWhites;
				ptr = pMaybeWhitesEnd;
			}
			return len;
		}
	} // end TRUE block for test: if (bNotEither)
	return len;
}

/// returns     TRUE if the marker is \f* or \fe* or \x* for USFM set, or if PNG 1998 set
///             is current, if the marker is \F or \fe
/// ptr        ->  the scanning pointer for the parse
/// pWholeMkr  <-  ptr to the endmarker string, empty if there is no marker at ptr
/// Called when ptr has possibly reached an endmarker following parsing of the word (and
/// possibly some following whitespace, the case when there was following puncts and ptr
/// points past them will be handled somewhere else probably). The intention of this function
/// is to alert the caller that any endmarker which should be stored in m_endMarkers and which
/// is currently being pointed at by ptr, must be flagged as present (so the caller can
/// then get it stored in m_endMarkers before returning from the caller to TokenizeText().
/// This function is very specific to making the ParseWord() parser work properly, so is
/// private in the document class. It the sfm set is PNG 1998 one, and the marker is \fe,
/// elsewhere in the app we default to assuming \fe is a USFM marker, since it is in both
/// sets with different meaning. Here we have a problem, if the set is the combined
/// UsfmAndPng, because it could then be a footnote endmarker of PNG 1998 set, or an
/// endnote beginmarker of the USFM set - and either could occur in the context where the
/// parser's ptr is currently at, so that's no help. So we'll require the set to be
/// explicitly PngOnly before we interpret it as the former. If it's UsfmAndPng, we'll
/// interpret it as the latter - and it that gives a false parse, then too bad. People
/// should be using only Usfm by now anyway!
bool CAdapt_ItDoc::IsEndMarkerRequiringStorageBeforeReturning(wxChar* ptr, wxString* pWholeMkr)
{
	if (gpApp->gCurrentSfmSet == PngOnly)
	{
		//if (*ptr == gSFescapechar)
		if (IsMarker(ptr))
		{
			(*pWholeMkr) = GetWholeMarker(ptr);
			if (*pWholeMkr != _T("\\fe") && *pWholeMkr != _T("\\F"))
			{
				return FALSE;
			}
		}
		else
		{
			(*pWholeMkr).Empty();
			return FALSE;
		}
	}
	else
	{
		// it's either USFM set or UsfmAndPng set --  treat both as if USFM
		//if (*ptr == gSFescapechar)
		if (IsMarker(ptr))
		{
			(*pWholeMkr) = GetWholeMarker(ptr);
			if (*pWholeMkr != _T("\\f*") && *pWholeMkr != _T("\\fe*") && *pWholeMkr != _T("\\x*"))
			{
				return FALSE;
			}
		}
		else
		{
			(*pWholeMkr).Empty();
			return FALSE;
		}
	}
	return TRUE;
}

// returns     the updated value of len, agreeing with where ptr is on return
// ptr            <->  the scanning pointer for the parse
// pEnd            ->  first character past the end of the data buffer we are parsing
// pSrcPhrase     <->  where we are storing information parsed, here it is final
//                     punctuation to be appended to its m_follPunct member
// len             ->  the len value before ptr is advanced in this internal scan
// bInlineBindingEndMkrFound <-> input FALSE, and returned as TRUE if an inline binding
//                               endmarker was detected and parsed over & stored (we
//                               use the TRUE value in the caller to enable a second
//                               attempt to collect final punctuation following such a
//                               marker)
// endMkr          <-  the endmarker parsed over, in case the caller needs it
// Called when ptr has reached an endmarker following parsing of the word (and possibly
// also having parsed over punctuation too). There are two kinds of endmarker parse we need
// to handle here; first kind is parsing over one or more sequential inline binding
// endmarkers, that is, not \f* or \x* nor any of the 5 in the m_inlineNonbindingEndMarkers
// set. Any of these we will store in the m_inlineBindingEndMarkers member of pSrcPhrase.
// The other kind of endmarkers we must deal with are those internal to footnotes or
// crossReferences (but not \f* nor \x* which terminate those info types). These endmarkers
// in the case of footnotes start with \f and end with *, and for crossReferences, start
// with \x and end with *, and each has other characters between the \f and *, or the \x
// and *. These are \fdc* and \fm*, and we can include \fe* too (for endnotes); & for
// crossReferences, \xot* \xnt* \xdc* -- these are to be stored in the m_endMarkers member
// (as will \f* and \x* when we parse them further along in the caller).
// If this function finds neither kind of marker, ptr and len are returned unchanged to the
// caller, and further possibilities are then tried. If it finds either kind of marker or
// sequence of same, these are parsed over, stored, and ptr and len returned updated, with
// ptr pointing at the first character following (typically a space, or final punctuation)
int CAdapt_ItDoc::ParseInlineEndMarkers(wxChar*& ptr, wxChar* pEnd,
		CSourcePhrase*& pSrcPhrase, wxString& inlineNonBindingEndMkrs, int len,
		bool& bInlineBindingEndMkrFound, wxString& endMkr)
{
	endMkr.Empty();
	if (!IsMarker(ptr) || !IsEndMarker(ptr, pEnd))
	{
		// do no parse if we are not pointing to a marker, and if we are, do no parse if
		// what we are pointing at is not an endmarker
		return len;
	}
	else
	{
		// we exclude \f* \fe* or \x* from consideration in this function, we'll handle
		// them later in the caller's parse at the point where either inline non-binding
		// endmarkers or one of \f* \fe* or \x* may possibly occur - as this will give
		// better outcomes for self-correcting bad punctuation markup
		if (!IsFootnoteOrCrossReferenceEndMarker(ptr))
		{
			// it's not one of \f* \fe* or \x*; and we also in the function do not parse
			// over any of the 5 inline non-binding endmarkers (\wj* \qt* \sls* \tl*
			// \fig*) so test for these and exit without doing anything if we are pointing
			// at one of them
			wxString wholeEndMkr = GetWholeMarker(ptr);
			wxString wholeEndMkrPlusSpace = wholeEndMkr + _T(" ");
			int length = wholeEndMkr.Len();
			if (inlineNonBindingEndMkrs.Find(wholeEndMkrPlusSpace) == wxNOT_FOUND)
			{
				// There are two possibilities... distinguish between & process ...
                // (1) we are pointing at an inline binding endmarker so parse it, store,
                // and return (there may be more than one, so the caller will repeat the
                // call until the len value returned is equal to what was input - which is
                // a sufficient test for parsing over nothing during the call)
				// (2) we are pointing at an inline endmarker internal to a footnote,
				// endnote or crossReference - and if that is the case, parse it, store,
				// and return to the caller -- again, the caller receiving a len value
				// equal to what was input indicates the caller's loop must end
				if (IsCrossReferenceInternalEndMarker(ptr) || IsFootnoteInternalEndMarker(ptr))
				{
					// possibility (2) obtains
					pSrcPhrase->AddEndMarker(wholeEndMkr);
				}
				else
				{
					// possibility (1) obtains
					pSrcPhrase->AppendToInlineBindingEndMarkers(wholeEndMkr);
					bInlineBindingEndMkrFound = TRUE;
				}
				endMkr = wholeEndMkr;
				len += length;
				ptr += length;
			}
		} // end of TRUE block for test: if (!IsFootnoteOrCrossReferenceEndMarker(ptr))
	} // end of else block for test: if (!IsMarker(ptr) || !IsEndMarker(ptr, pEnd))
	return len;
}

// returns     the updated value of len, agreeing with where ptr is on return
// ptr            <->  the scanning pointer for the parse
// pEnd            ->  first character past the end of the data buffer we are parsing
// pSrcPhrase     <->  where we are storing information parsed, here it is final
//                     punctuation to be appended to its m_follPunct member
// spacelessPuncts ->  the punctuation set being used (either source puncts, or target ones)
// len             ->  the len value before ptr is advanced in this internal scan
// bExitOnReturn  <-   return TRUE if ParseWord() should be exited on return 
// bHasPrecedingStraightQuote <-> default is FALSE, the boolean passed in is stored
//                                on the CAdapt_ItDoc class with public access; and
//                                is set TRUE if a straight quote (" or ') is detected
//                                in TokenizeText() when parsing punctuation preceding
//                                a word. The matching closing straight quote could be
//                                many word parses further on, and so we leave it set
//                                until one of the following happens, whichever is first:
//                                a new verse is started, or, its TRUE value is used to
//                                assign ownership of ' or " to the currently being parsed
//                                word as its final punctuation, or part of its final 
//                                punctuation (if there are more than one, we'll assign them
//                                all - which could lead to a misparse if an opening
//                                straight quote occurs prior to the next word - it's
//                                opening quote would wrongly be put in the following puncts
//                                of the preceding word -- but probably this would never
//                                happen in practice [we hope])
// Called after a sequence of word-final punctuation ends at space - there could be
// additional detached endquotes, single or double - this function collects these, stores
// them appropriately in pSrcPhrase, and advances len and ptr to the end of the material
// parsed over.
// BEW created 11Oct10
// BEW 2Dec10 added ] character as cause to return, ptr should be pointing at it on return                
int CAdapt_ItDoc::ParseAdditionalFinalPuncts(wxChar*& ptr, wxChar* pEnd,
					CSourcePhrase*& pSrcPhrase, wxString& spacelessPuncts, 
					int len, bool& bExitOnReturn, bool& bHasPrecedingStraightQuote,
					wxString& additions, bool bPutInOuterStorage)
{
    wxChar* pPunctStart = ptr;
	wxChar* pPunctEnd = ptr;
	size_t counter = 0;
	bool bFoundClosingQuote = FALSE;
	wxChar* pLocation_OK = ptr; // every time we parse over curly endquotes, we 
								// set this pointer to the location after the 
								// acceptable quote character, but we won't update
								// it if we come to a straightquote character 
	while (!IsEnd(ptr) && (IsWhiteSpace(ptr) || IsClosingQuote(ptr)) && !IsMarker(ptr)
			&& *ptr != _T(']'))
	{
		if (IsClosingQuote(ptr))
		{
			bFoundClosingQuote = TRUE;
			// now determine if it is a curly endquote or right chevron, and if so
			// then it is definitely to be included in the following punctuation of
			// the current pSrcPhrase
			if (IsClosingCurlyQuote(ptr))
			{
				// mark the location following it
				pLocation_OK = (ptr + 1);
			}
		}
		counter++;
		ptr++;
		pPunctEnd = ptr;
		// if the punctuation end is also pEnd, then tell the caller not to parse further
		// on return
		if (IsEnd(pPunctEnd))
		{
			bExitOnReturn = TRUE;
		}
	}
	// on exit of the loop we are either at buffer end, or backslash of a marker, or a ]
    // closing bracket, or some character which is not whitespace nor a closing quote
    // (IsClosingQuote() also tests for a straightquote or doublequote, so
    // IsCLosingCurlyQuote() was also used as it does not test for straight ones)
    if (bFoundClosingQuote)
	{
		// we matched something more than just whitespace and the something is
		// curly endquote(s) and possibly a straight one (or more than one) - pLocation_OK
		// will point at the character following the last curly endquote scanned over, so
		// only white space and one or more straight quotes can follow that location
		if (IsEndMarker(ptr, pEnd) || IsEnd(ptr))
		{
            // an endmarker is what ptr points at now, or the buffer's end, so we'll accept
            // everything as valid final punctuation for the current pSrcPhrase; and if not
            // at buffer end then further parsing is needed in the caller because there may
            // be more markers and punctuation to be handled for the end of the current
            // word
 			if (IsEnd(ptr))
			{
				// may have been set above, but no harm in making the test again & setting
				// it here again
				bExitOnReturn = TRUE;
			}
            wxString finalPunct(pPunctStart,counter);
			if (bPutInOuterStorage)
			{
				pSrcPhrase->AddFollOuterPuncts(finalPunct);
			}
			else
			{
				pSrcPhrase->m_follPunct += finalPunct;
			}
 			pSrcPhrase->m_srcPhrase += finalPunct; // add any punct'n
			additions += finalPunct; // accumulate here, so that the caller can add any
									 // additions to secondWord of ~ conjoining, in the
									 // m_srcPhrase and m_follPunct members; note this
									 // setting of additions is done whether fixedspace
									 // was encountered or not, but the caller makes
									 // this safe by checking that pSrcPhrase has a
									 // fixedspace before trying to use this variable's
									 // contents
            // what ptr points at now could be an inline non-binding endmarker (like \wj*)
            // or one of \f* or \x* (or even an inline binding endmarker with misplaced
            // punctuation before it which we are now parsing over) - so while our parse of
            // the punctuation in this function halts, the caller's parse must continue
            // over potential endmarkers further on, and there could be outer following
            // punctuation too
			len += counter;
		} // end of TRUE block for test: if (IsEndMarker(ptr,pEnd) || IsEnd(ptr))
		else
		{
            // ptr is not pointing at the start of a marker; the situation is somewhat
            // ambiguous - the difficulty here is that we may have some ending
            // punctuation parsed over which belongs to the final punctuation of the
            // current pSrcPhrase, followed by some initial punctuation (it can only be
            // straight singlequote or straight doublequote) which belongs to the start
            // of the next word to be parsed - so we'll accept only up to where
            // pLocation_OK points as belonging to final punctuation of pSrcPhrase, and
            // anything after that belongs to the next call of ParseWord() as initial
			// punctuation for that call's word; however if bHasPrecedingStraightQuote is
			// TRUE, then we'll accept the whole lot, because we assume that any straight
			// quotes match any found earlier somewhere.
			size_t shortCount = 0;
			if (pLocation_OK > pPunctStart)
			{
				// we found at least one curly endquote, so work out if they all were
				// curly endquotes that we parsed over (and accept them all) or if
				// only some of them were (we accept only those up to pLocation_OK,
				// and if any quotes follow we assume they belong to the next word -
				// unless bHasPrecedingStraightQuote is TRUE)
				shortCount = pLocation_OK - pPunctStart;
				if (shortCount < counter)
				{
					// we parsed over non-curly (& non-right-chevron) non-endquote
					// quote character lying beyond pLocation_OK, or, we may have just
					// whitespace following pLocation_OK -- handle these possibilities
					wxString finalPunct(pPunctStart,shortCount);
					if (bPutInOuterStorage)
					{
						pSrcPhrase->AddFollOuterPuncts(finalPunct);
					}
					else
					{
						pSrcPhrase->m_follPunct += finalPunct;
					}
					pSrcPhrase->m_srcPhrase += finalPunct;
					additions += finalPunct; 
					size_t theRest = pPunctEnd - pLocation_OK;
					wxString remainder(pLocation_OK,theRest);
					wxString minusEndingSpaces = remainder.Trim();
					if (minusEndingSpaces.IsEmpty())
					{
						// there was only whitespace in remainder, so we can include
						// it in the parsed over data span & tell the caller to return
						len += counter;
						bExitOnReturn = TRUE;
						return len;
					}
					else
					{
						// remainer has some nonwhitespace content, but that must
						// belong (we assume) to the next word's parse, so set the ptr
						// location to be pLocation_OK; but if bHasPrecedingStraightQuote
						// is TRUE, accept it all
						if (bHasPrecedingStraightQuote)
						{
							if (bPutInOuterStorage)
							{
								pSrcPhrase->AddFollOuterPuncts(remainder);
							}
							else
							{
								pSrcPhrase->m_follPunct += remainder;
							}
							pSrcPhrase->m_srcPhrase += remainder;
							additions += remainder; 
							ptr += theRest;
							len += (int)theRest;
							bHasPrecedingStraightQuote = FALSE; // we've made a decision
									// based on it's TRUE value, so we must now restore
									// its default FALSE value in case further matching
									// of preceding and following straight quotes is 
									// required in the parse of further words
						}
						else
						{
							ptr = pLocation_OK;
							len += shortCount;
							bExitOnReturn = TRUE;
							return len;
						}
					}
				}
				else
				{
					// shortCount and counter are the same value, so we accept it all
					// and we must tell the caller to return because ptr is not pointing
					// at a marker but something else which is not a quote symbol.
					// Possibly it could be other punctuation ptr is pointing at, and if
					// it is so without any spaces, then we should accept it as belonging
					// to the end of the current final punctuation - so accumulate it
					// until space or non-puncts are encountered. But check for ] first,
					// if pointing at that, (it's defaulted as punctuation) don't
					// accumulate it but return instead
					wxString finalPunct(pPunctStart,counter);
					if (bPutInOuterStorage)
					{
						pSrcPhrase->AddFollOuterPuncts(finalPunct);
					}
					else
					{
						pSrcPhrase->m_follPunct += finalPunct;
					}
					pSrcPhrase->m_srcPhrase += finalPunct;
					additions += finalPunct; 
					len += counter;
					// are we pointing at ] bracket?
					if (*ptr == _T(']'))
					{
						// we must return
						bExitOnReturn = TRUE;
						return len;
					}
					// else, accumulate any more puncts until space or non-punct or a
					// ] bracket is reached
					while (ptr < pEnd && (spacelessPuncts.Find(*ptr) != wxNOT_FOUND)
						&& *ptr != _T(']'))
					{
						wxString aPunct = *ptr;
						if (bPutInOuterStorage)
						{
							pSrcPhrase->AddFollOuterPuncts(aPunct);
						}
						else
						{
							pSrcPhrase->m_follPunct += *ptr;
						}
						pSrcPhrase->m_srcPhrase += *ptr;
						additions += *ptr; 
						len++;
						ptr++;
					}
					bExitOnReturn = TRUE;
					return len;
				}
			} // end of TRUE block for test: if (pLocation_OK > pPunctStart)
			else
			{
                // pLocation_OK has not advanced from pPunctStart where we started this
                // parse, so we didn't find any curly endquotes; so count all the punct and
                // whitespace parsed over as belonging to a later word -- and ptr is not
                // pointing at an endmarker, but if might be pointing at ] closing bracket,
                // so either way we must be done & can return after we finish off the
                // pSrcPhrase members; however, if bHasPrecedingStraightQuote is TRUE, then
                // we assume that only straight quotes were in the parse and that they
                // should be included as following punctuation for the current word. This
                // might be a faulty decsion if some belong to the current word and some to
                // the following word yet to be parsed, but our code can't reasonably be
                // made smart enough to decide where to make the division - and so we'll
                // just hope that that situation won't ever occur.
				if (bHasPrecedingStraightQuote || *ptr == _T(']'))
				{
					// take the lot
					if (counter > 0)
					{
						wxString finalPunct(pPunctStart,counter);
						if (bPutInOuterStorage)
						{
							pSrcPhrase->AddFollOuterPuncts(finalPunct);
						}
						else
						{
							pSrcPhrase->m_follPunct += finalPunct;
						}
 						pSrcPhrase->m_srcPhrase += finalPunct;
						additions += finalPunct;
						len += counter;
						return len;
					}
				}
				else
				{
					// reject the lot
					ptr = pPunctStart; // restore ptr to location where we started from 
					bExitOnReturn = TRUE;
					return len;
				}
			} // end of else block for test: if (pLocation_OK > pPunctStart)
		} // end of else block for test: if (IsEndMarker(ptr,pEnd) || IsEnd(ptr))
	} // end of TRUE block for test: if (bFoundClosingQuote)
	else if (IsEndMarker(ptr,pEnd) || IsEnd(ptr))
	{
		if (counter > 0)
		{
			if (IsEnd(ptr))
			{
				// ensure that if ptr is at pEnd, the caller does not parse further
				bExitOnReturn = TRUE;
			}
            // an endmarker is what ptr points at now, or the buffer's end, so we'll accept
            // everything as valid final punctuation for the current pSrcPhrase; and if not
            // at buffer end then further parsing is needed in the caller because there may
            // be more markers and punctuation to be handled for the end of the current
            // word
            wxString finalPunct(pPunctStart,counter);
			pSrcPhrase->m_follPunct += finalPunct;
 			pSrcPhrase->m_srcPhrase += finalPunct; // add any punct'n
			additions += finalPunct; // accumulate here, so that the caller can add any
									 // additions to secondWord of ~ conjoining, in the
									 // m_srcPhrase and m_follPunct members
            // what ptr points at now could be an inline non-binding endmarker (like \wj*)
            // or one of \f* or \x* (or even an inline binding endmarker with misplaced
            // punctuation before it which we are now parsing over) - so while our parse of
            // the punctuation in this function halts, the caller's parse must continue
            // over potential endmarkers further on, and there could be outer following
            // punctuation too
			len += counter;
		} // end of TRUE block for test: if (counter > 0)
		else
		{
			// nothing extra, so return, but continue parsing in ParseWord() on return
			ptr = pPunctStart; // restore ptr to location where we started from 
			bExitOnReturn = FALSE;
			return len;
		}
	}
	else if (*ptr == _T(']'))
	{
		// accept everything up to that point
		if (counter > 0)
		{
			wxString finalPunct(pPunctStart,counter);
			if (bPutInOuterStorage)
			{
				pSrcPhrase->AddFollOuterPuncts(finalPunct);
			}
			else
			{
				pSrcPhrase->m_follPunct += finalPunct;
			}
			pSrcPhrase->m_srcPhrase += finalPunct;
			additions += finalPunct;
			len += counter;
			bExitOnReturn = TRUE;
			return len;
		}
	}
	else
	{
		// we either didn't parse over anything, including white space; or we only
		// parsed over whitespace -- the latter is of no interest (we let the caller
		// advance over it)
		ptr = pPunctStart; // restore ptr to location where we started from 
		bExitOnReturn = TRUE;
		return len;
	} // end of else block for test: if (bFoundClosingQuote)
	return len;
}

// returns the new updated value for len, and ptr, after parsing over any whitespace
int CAdapt_ItDoc::ParseOverAndIgnoreWhiteSpace(wxChar*& ptr, wxChar* pEnd, int len)
{
	wxChar* pParseStartLoc = ptr;
	wxChar* pParseHaltLoc = NULL;
	while (IsWhiteSpace(ptr) && ptr < pEnd)
	{
		ptr++;
	}
	pParseHaltLoc = ptr;
	if (pParseHaltLoc > pParseStartLoc)
	{
		len += (int)(pParseHaltLoc - pParseStartLoc);
	}
	return len;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
///	\param		useSfmSet	->	which of the three sfm set possibilities we are dealing with
/// \param		filterMkrs	->	concatenated markers (each with a following space) which are 
///								the markers formerly unfiltered but now designated by the user 
///								as to be filtered,
/// \param		unfilterMkrs ->	concatenated markers (each with a following space) which are 
///								the markers formerly filtered but now designated by the user 
///								as to be unfiltered.
/// \remarks
/// Called from: the App's DoUsfmFilterChanges(), the Doc's RestoreDocParamsOnInput(),
/// ReconstituteAfterFilteringChange(), RetokenizeText().
/// This is an overloaded version of another function called ResetUSFMFilterStructs.
/// Changes only the USFMAnalysis structs which deal with the markers in the filterMkrs
/// string and the unfilterMkrs string.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::ResetUSFMFilterStructs(enum SfmSet useSfmSet, wxString filterMkrs, 
										  wxString unfilterMkrs)
{
    // BEW added 25May05 in support of changing filtering settings for USFM, SFM or
    // combined Filtering set The second and third strings must have been set up in the
    // caller by iterating through the map m_FilterStatusMap, which contains associations
    // between the bare marker as key (ie. no backslash or final *) and a literal string
    // which is "1" when the marker is unfiltered and about to be filtered, and "0" when it
    // is filtered and about to be unfiltered. This map is constructed when the user exits
    // the Filter tab of the Preferences or Start Working... wizard.
	MapSfmToUSFMAnalysisStruct* pSfmMap;
	USFMAnalysis* pSfm;
	wxString fullMkr;

	pSfmMap = gpApp->GetCurSfmMap(useSfmSet);

	MapSfmToUSFMAnalysisStruct::iterator iter;
    // enumerate through all markers in pSfmMap and set those markers that occur in the
    // filterMkrs string to the equivalent of filter="1" and those in unfilterMkrs to the
    // equivalent of filter="0"; doing this means that any call of TokenizeText() (or
    // functions which call it such as TokenizeTextString() etc) will, when they get to the
    // LookupSFM(marker) call, get the USFMAnalysis with the filtering settings which need
    // to be in place at the time the lookup is done

	for( iter = pSfmMap->begin(); iter != pSfmMap->end(); ++iter )
	{
		wxString key = iter->first; // use only for inspection
		pSfm = iter->second;
		fullMkr = gSFescapechar + pSfm->marker + _T(' '); // each marker in filterMkrs is 
														  // delimited by a space
		if (filterMkrs.Find(fullMkr) != -1)
		{
			pSfm->filter = TRUE;
            // because of how the caller constructs filterMkrs and unfilterMkrs, it is
            // never possible that a marker will be in both these strings, so if we do this
            // block we can skip the next
			continue; 
		}
		if (unfilterMkrs.Find(fullMkr) != -1)
		{
			pSfm->filter = FALSE;
		}
	}
	// redo the special fast access marker strings to reflect any changes to pSfm->filter
	// attributes 
	gpApp->SetupMarkerStrings();
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
///	\param		useSfmSet	->	which of the three sfm set possibilities we are dealing with
/// \param		filterMkrs	->	concatenated markers (each with a following space) which are 
///								the markers formerly unfiltered but now designated by the user 
///								as to be filtered,
/// \param		resetMkrs	-> an enum indicating whether to reset allInSet or onlyThoseInString
/// \remarks
/// Called from: the Doc's RestoreDocParamsOnInput().
/// The filterMkrs parameter is a wxString of concatenated markers delimited by following spaces. 
/// If resetMkrs == allInSet, ResetUSFMFilterStructs() sets the filter attributes of the 
/// appropriate SfmSet of markers to filter="1" if the marker is present in the 
/// filterMkrs string, and for all others the filter attribute is set to filter="0" 
/// if it is not already zero.
/// If resetMkrs == onlyThoseInString ResetUSFMFilterStructs() sets the filter attributes
/// of the appropriate SfmSet of markers to filter="1" of only those markers which
/// are present in the filterMkrs string.
/// ResetUSFMFilterStructs does nothing to the USFMAnalysis structs nor their 
/// maps in response to the presence of unknown markers (filtered or not), since unknown markers
/// do not have any identifiable attributes, except for being considered userCanSetFilter
/// as far as the filterPage is concerned.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::ResetUSFMFilterStructs(enum SfmSet useSfmSet, wxString filterMkrs, 
										  enum resetMarkers resetMkrs)
{
	// whm added 5Mar2005 in support of USFM and SFM Filtering support
	MapSfmToUSFMAnalysisStruct* pSfmMap; 
	USFMAnalysis* pSfm;
	wxString key;
	wxString fullMkr;

	pSfmMap = gpApp->GetCurSfmMap(useSfmSet);

	MapSfmToUSFMAnalysisStruct::iterator iter;
	// enumerate through all markers in pSfmMap and set those markers that
	// occur in the filterMkrs string to filter="1" and, if resetMkrs is allInSet,
	// we also set those that don't occur in filterMkrs to filter="0"

	for( iter = pSfmMap->begin(); iter != pSfmMap->end(); ++iter )
	{
		wxString key = iter->first; // use only for inspection
		pSfm = iter->second;
		fullMkr = gSFescapechar + pSfm->marker + _T(' '); // each marker in filterMkrs 
														  // is delimited by a space
		if (filterMkrs.Find(fullMkr) != -1)
		{
			pSfm->filter = TRUE;
		}
		else if (resetMkrs == allInSet)
		{
			pSfm->filter = FALSE;
		}
	}
	// The m_filterFlagsUnkMkrs flags are already changed in the filterPage
	// so they should not be changed (reversed) here

    // redo the special fast access marker strings to reflect any changes to pSfm->filter
    // attributes or the presence of unknown markers
	gpApp->SetupMarkerStrings();
}


///////////////////////////////////////////////////////////////////////////////
/// \return		the whole standard format marker including the initial backslash and any ending *
/// \param		pChar			-> a pointer to the first character of the marker (a backslash)
/// \remarks
/// Called from: the Doc's GetMarkerWithoutBackslash(), IsInLineMarker(), IsCorresEndMarker(),
/// TokenizeText(), the View's RebuildSourceText(), FormatMarkerBufferForOutput(), 
/// DoExportInterlinearRTF(), ParseFootnote(), ParseEndnote(), ParseCrossRef(), 
/// ProcessAndWriteDestinationText(), ApplyOutputFilterToText(), IsCharacterFormatMarker(),
/// DetermineRTFDestinationMarkerFlagsFromBuffer().
/// Returns the whole standard format marker including the initial backslash and any ending
/// asterisk. 
/// BEW 15Sep10, it helps to have a predictable return if pChar on input is not pointing
/// at a backslash - so test and return the empty string. (Better this way for OXES support)
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::GetWholeMarker(wxChar *pChar)
{
	//if (*pChar != gSFescapechar)
	if (!IsMarker(pChar))
	{
		wxString s; s.Empty();
		return s;
	}
	// whm added 10Feb2005 in support of USFM and SFM Filtering support
	// returns the whole marker including backslash and any ending *
	wxChar* ptr = pChar;
	int itemLen = ParseMarker(ptr);
	return wxString(ptr,itemLen);
}

///////////////////////////////////////////////////////////////////////////////
/// \return		the whole standard format marker including the initial backslash 
///             and any ending *
/// \param		str	-> a wxString in which the initial backslash of the marker to be
///					   obtained is at the beginning of the string
/// \remarks
/// Called from: the View's RebuildSourceText().
/// Returns the whole standard format marker including the initial backslash and any ending
/// asterisk. Internally uses ParseMarker() just like the version of GetWholeMarker() that
/// uses a pointer to a buffer.
/// BEW 15Sep10, it helps to have a predictable return if pChar on input is not pointing
/// at a backslash - so test and return the empty string. (Better this way for OXES support)
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::GetWholeMarker(wxString str)
{
	if (str[0] != gSFescapechar)
	{
		wxString s; s.Empty();
		return s;
	}
	// BEW added 2Jun2006 for situations where a marker is at the start of a CString
	// returns the whole marker including backslash and any ending *
	int len = str.Length();
    // wx version note: Since we require a read-only buffer we use GetData which just
    // returns a const wxChar* to the data in the string.
	const wxChar* pChar = str.GetData();
	wxChar* pEnd;
	pEnd = (wxChar*)pChar + len;
	wxChar* pBufStart = (wxChar*)pChar;
	wxASSERT(*pEnd == _T('\0'));
	int itemLen = ParseMarker(pBufStart);
	wxString mkr = wxString(pChar,itemLen);
	return mkr;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		the standard format marker without the initial backslash, but including 
///             any ending *
/// \param		pChar	-> a pointer to the first character of the marker (a backslash)
/// \remarks
/// Called from: the Doc's IsPreviousTextTypeWanted(), GetBareMarkerForLookup(), 
/// IsEndMarkerForTextTypeNone(), the View's InsertNullSourcePhrase().
/// Returns the standard format marker without the initial backslash, but includes any end
/// marker asterisk. Internally calls GetWholeMarker().
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::GetMarkerWithoutBackslash(wxChar *pChar)
{
	// whm added 10Feb2005 in support of USFM and SFM Filtering support
	// Strips off initial backslash but leaves any final asterisk in place.
	// The bare marker string returned is suitable for marker lookup only if
	// it is known that no asterisk is present; if unsure, call
	// GetBareMarkerForLookup() instead.
	wxChar* ptr = pChar;
	wxString Mkr = GetWholeMarker(ptr);
	return Mkr.Mid(1); // strip off initial backslash
}


///////////////////////////////////////////////////////////////////////////////
/// \return		the standard format marker without the initial backslash and 
///             without any ending *
/// \param		pChar	-> a pointer to the first character of the marker in the 
///                        buffer (a backslash)
/// \remarks
/// Called from: the Doc's IsPreviousTextTypeWanted(), ParseFilteringSFM(), LookupSFM(),
/// AnalyseMarker(), IsEndMarkerForTextTypeNone(), the View's InsertNullSourcePhrase(),
/// DoExportInterlinearRTF(), IsFreeTranslationEndDueToMarker(), HaltCurrentCollection(),
/// ParseFootnote(), ParseEndnote(), ParseCrossRef(), ProcessAndWriteDestinationText(),
/// ApplyOutputFilterToText().
/// Returns the standard format marker without the initial backslash, and without any end
/// marker asterisk. Internally calls GetMarkerWithoutBackslash().
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::GetBareMarkerForLookup(wxChar *pChar)
{
	// whm added 10Feb2005 in support of USFM and SFM Filtering support
    // Strips off initial backslash and any ending asterisk.
    // The bare marker string returned is suitable for marker lookup.
    wxChar* ptr = pChar;
    wxString bareMkr = GetMarkerWithoutBackslash(ptr);
    int posn = bareMkr.Find(_T('*'));
    // The following GetLength() call could on rare occassions return a 
	// length of 1051 when processing the \add* marker.
	// whm comment: the reason for the erroneous result from GetLength
	// stems from the problem with the original code used in ParseMarker.
	// (see caution statement in ParseMarker). 
    if (posn >= 0) // whm revised 7Jun05
        // strip off asterisk for attribute lookup
		bareMkr = bareMkr.Mid(0,posn);
    return bareMkr;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		pMkrList	<- a wxArrayString that holds standard format markers and 
///							   associated content parsed from the input string str
/// \param		str			-> the string containing standard format markers and associated text 
/// \remarks
/// Called from: the Doc's GetUnknownMarkersFromDoc(), the View's GetMarkerInventoryFromCurrentDoc(),
/// CPlaceInternalMarkers::InitDialog().
/// Scans str and collects all standard format markers and their associated text into 
/// pMkrList, one marker and associated content text per array item (and final endmarker
/// if there is one).
/// whm added str param 18Feb05
/// BEW 24Mar10 no changes needed for support of doc version 5
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::GetMarkersAndTextFromString(wxArrayString* pMkrList, wxString str) 
{
	// Populates a wxArrayString containing sfms and their associated
	// text parsed from the input str. pMkrList will contain one list item for
	// each marker and associated text found in str in order from beginning of
	// str to end.
	int nLen = str.Length();
    // wx version note: Since we require a read-only buffer we use GetData which just
    // returns a const wxChar* to the data in the string.
	const wxChar* pBuf = str.GetData();
	wxChar* pEnd = (wxChar*)pBuf + nLen; // cast necessary because pBuf is const
	wxASSERT(*pEnd == _T('\0')); // whm added 18Jun06
	wxChar* ptr = (wxChar*)pBuf;
	//wxChar* pBufStart = (wxChar*)pBuf; // BEW 9Sep10, IsMarker() call no longer needs this
	wxString accumStr = _T("");
	// caller needs to call Clear() to start with empty list
	while (ptr < pEnd)
	{
		if (IsFilteredBracketMarker(ptr,pEnd))
		{
			// It's a filtered marker opening bracket. There should always
			// be a corresponding closing bracket, so parse and accumulate
			// chars until end of filterMkrEnd.
			while (ptr < pEnd && !IsFilteredBracketEndMarker(ptr,pEnd))
			{
				accumStr += *ptr;
				ptr++;
			}
			if (ptr < pEnd)
			{
				// accumulate the filterMkrEnd
				for (int i = 0; i < (int)wxStrlen_(filterMkrEnd); i++)
				{
					accumStr += *ptr;
					ptr++;
				}
			}
			accumStr.Trim(FALSE); // trim left end
			accumStr.Trim(TRUE); // trim right end
			// add the filter sfm and associated text to list
			pMkrList->Add(accumStr);
			accumStr.Empty();
		}
		//else if (IsMarker(ptr,pBufStart))
		else if (IsMarker(ptr))
		{
			// It's a non-filtered sfm. Non-filtered sfms can be followed by
			// a corresponding markers or no end markers. We'll parse and 
			// accumulate chars until we reach the next marker (or end of buffer).
			// If the marker is a corresponding end marker we'll parse and
			// accumulate it too, otherwise we'll not accumulate it with the
			// current accumStr.
			// First save the marker we are at to check that any end marker
			// that follows is indeed a corresponding end marker.
			wxString currMkr = MarkerAtBufPtr(ptr,pEnd);
			int itemLen;
			while (ptr < pEnd && *(ptr+1) != gSFescapechar)
			{
				accumStr += *ptr;
				ptr++;
			}
			itemLen = ParseWhiteSpace(ptr); // ignore return value
			ptr += itemLen;
			if (itemLen > 0)
				accumStr += _T(' ');
			if (IsEndMarker(ptr,pEnd))
			{
				//parse and accumulate to the * providing it is a corresponding end marker
				if (IsCorresEndMarker(currMkr,ptr,pEnd))
				{
					while (*ptr != _T('*'))
					{
						accumStr += *ptr;
						ptr++;
					}
					accumStr += *ptr; // add the end marker
					ptr++;
				}
			}
			accumStr.Trim(FALSE); // trim left end
			accumStr.Trim(TRUE); // trim right end
			// add the non-filter sfm and associated text to list
			pMkrList->Add(accumStr);
			accumStr.Empty();
		}
		else
			ptr++;
	} // end of while (ptr < pEnd)
	// We've finished building the wxArrayString
}

///////////////////////////////////////////////////////////////////////////////
/// \return     TRUE if there is a filename clash, FALSE if the typed name is unique
/// 
/// \remarks
/// Get the active document folder's document names into the app class's
/// m_acceptedFilesList and test them against the user's typed filename.
/// Use in OutputFilenameDlg.cpp's OnOK()button handler.
/// Before this protection was added in 22July08, an existing document with lots of
/// adaptation and other work contents already done could be wiped out without warning
/// merely by the user creating a new document with the same name as that document file.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::FilenameClash(wxString& typedName)
{
	gpApp->m_acceptedFilesList.Clear();
	wxString dirPath;
	if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
		dirPath = gpApp->m_bibleBooksFolderPath;
	else
		dirPath = gpApp->m_curAdaptionsPath;
	bool bOK;
	bOK = ::wxSetWorkingDirectory(dirPath); // ignore failures
	wxString docName;
	gpApp->GetPossibleAdaptionDocuments(&gpApp->m_acceptedFilesList, dirPath);
	int offset = -1;

	// remove any .xml or .adt which the user may have added to the passed in filename
	wxString rev = typedName;
	rev = MakeReverse(rev);

	// BEW note 26Apr10, .adt extensions occurred on in versions 1-3, but there is no harm
	// in leaving this line unremoved and similarly the test a little further below 
	wxString adtExtn = _T(".adt");

	wxString xmlExtn = _T(".xml");
	adtExtn = MakeReverse(adtExtn);
	adtExtn = MakeReverse(adtExtn);

    // BEW note 26Apr10, these next 6 lines could be removed for versions 4.0.0 onwards,
    // but we'll leave them is they waste very little time, and do no harm
	offset = rev.Find(adtExtn);
	if (offset == 0)
	{
		// it's there, so remove it
		rev = rev.Mid(4);
	}

	offset = rev.Find(xmlExtn);
	if (offset == 0)
	{
		// it's there, so remove it
		rev = rev.Mid(4);
	}
	rev = MakeReverse(rev);
	int len = rev.Length();

	// test for filename clash
	int ct;
	for (ct = 0; ct < (int)gpApp->m_acceptedFilesList.GetCount(); ct++)
	{
		docName = gpApp->m_acceptedFilesList.Item(ct);
		offset = docName.Find(rev);
		if (offset == 0)
		{
			// this one is a candidate for a clash, check further
			int docNameLen = docName.Length();
			if (docNameLen >= len + 1)
			{
				// there is a character at len, so see if it is the . of an extension
				wxChar ch = docName.GetChar(len);
				if (ch == _T('.'))
				{
					// the names clash
					gpApp->m_acceptedFilesList.Clear();
					return TRUE;
				}
			}
			else
			{
                // BEW changed 26Apr10, (to include a 'shorter' option) same length or
                // shorter; if equal then this is a clash and we'll return TRUE and give a
                // beep as well; but if shorter, then it's a different name & we'll accept
                // it by falling through and returning FALSE
                if (docNameLen == len)
				{ 
					// it's the same name
					::wxBell();
					gpApp->m_acceptedFilesList.Clear();
					return TRUE;
				}
			}
		}
	}
	gpApp->m_acceptedFilesList.Clear();
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a pointer to the USFMAnalysis struct associated with the marker at pChar,
///				or NULL if the marker was not found in the MapSfmToUSFMAnalysisStruct.
/// \param		pChar	-> a pointer to the first character of the marker in the 
///                        buffer (a backslash)
/// \remarks
/// Called from: the Doc's ParseWord(), IsMarker(), TokenizeText(), DoMarkerHousekeeping(),
/// IsEndMarkerForTextTypeNone(), the View's InsertNullSourcePhrase(),
/// Determines the marker pointed to at pChar and performs a look up in the 
/// MapSfmToUSFMAnalysisStruct hash map. If the marker has an association in the map it
/// returns a pointer to the USFMAnalysis struct. NULL is returned if no marker could be
/// parsed from pChar, or if the marker could not be found in the hash map.
///////////////////////////////////////////////////////////////////////////////
USFMAnalysis* CAdapt_ItDoc::LookupSFM(wxChar *pChar)
{
	// Looks up the sfm pointed at by pChar
	// Returns a USFMAnalysis struct filled out with attributes
	// if the marker is found in the appropriate map, otherwise
	// returns NULL.
	// whm ammended 11July05 to return the \bt USFM Analysis struct whenever
	// any bare marker of the form bt... exists at pChar
	wxChar* ptr = pChar;
	bool bFound = FALSE;
	// get the bare marker
	wxString bareMkr = GetBareMarkerForLookup(ptr);
	// look up and Retrieve the USFMAnalysis into our local usfmAnalysis struct 
	// variable. 
	// If bareMkr begins with bt... we will simply use bt which will return the
	// USFMAnalysis struct for \bt for all back-translation markers based on \bt...
	if (bareMkr.Find(_T("bt")) == 0)
	{
		// bareMkr starts with bt... so shorten it to simply bt for lookup purposes
		bareMkr = _T("bt");
	}
	MapSfmToUSFMAnalysisStruct::iterator iter;
	// The particular MapSfmToUSFMAnalysisStruct used for lookup below depends the appropriate 
	// sfm set being used as stored in gCurrentSfmSet enum.
	switch (gpApp->gCurrentSfmSet)
	{
		case UsfmOnly: 
			iter = gpApp->m_pUsfmStylesMap->find(bareMkr);
			bFound = (iter != gpApp->m_pUsfmStylesMap->end());
			break;
		case PngOnly: 
			iter = gpApp->m_pPngStylesMap->find(bareMkr);
			bFound = (iter != gpApp->m_pPngStylesMap->end()); 
			break;
		case UsfmAndPng: 
			iter = gpApp->m_pUsfmAndPngStylesMap->find(bareMkr);
			bFound = (iter != gpApp->m_pUsfmAndPngStylesMap->end()); 
			break;
		default: 
			iter = gpApp->m_pUsfmStylesMap->find(bareMkr);
			bFound = (iter != gpApp->m_pUsfmStylesMap->end());
	}
	if (bFound)
	{
		// iter->second points to the USFMAnalysis struct
		return iter->second;
	}
	else
	{
		return (USFMAnalysis*)NULL;
	}
}


///////////////////////////////////////////////////////////////////////////////
/// \return		a pointer to the USFMAnalysis struct associated with the bareMkr,
///				or NULL if the marker was not found in the MapSfmToUSFMAnalysisStruct.
/// \param		bareMkr	-> a wxString containing the bare marker to use in the lookup
/// \remarks
/// Called from: the Doc's ReconstituteAfterFilteringChange(), ParseFilteringSFM(),
/// GetUnknownMarkersFromDoc(), AnalyseMarker(), IsEndingSrcPhrase(), 
/// ContainsMarkerToBeFiltered(), RedoNavigationText(), DoExportInterlinearRTF(),
/// IsFreeTranslationEndDueToMarker(), HaltCurrentCollection(), ParseFootnote(),
/// ParseEndnote(), ParseCrossRef(), ParseMarkerAndAnyAssociatedText(), 
/// GetMarkerInventoryFromCurrentDoc(), MarkerTakesAnEndMarker(), 
/// CViewFilteredMaterialDlg::GetAndShowMarkerDescription().
/// Looks up the bareMkr in the MapSfmToUSFMAnalysisStruct hash map. If the marker has an
/// association in the map it returns a pointer to the USFMAnalysis struct. NULL is
/// returned if the marker could not be found in the hash map.
/// BEW 10Apr10, no changes for support of doc version 5
///////////////////////////////////////////////////////////////////////////////
USFMAnalysis* CAdapt_ItDoc::LookupSFM(wxString bareMkr)
{
	// overloaded version of the LookupSFM above to take bare marker
	// Looks up the bareMkr CString sfm in the appropriate map
	// Returns a USFMAnalysis struct filled out with attributes
	// if the marker is found in the appropriate map, otherwise
	// returns NULL.
	// whm ammended 11July05 to return the \bt USFM Analysis struct whenever
	// any bare marker of the form bt... is passed in
	if (bareMkr.IsEmpty())
		return (USFMAnalysis*)NULL;
	bool bFound = FALSE;
	// look up and Retrieve the USFMAnalysis into our local usfmAnalysis struct 
	// variable. 
	// If bareMkr begins with bt... we will simply use bt which will return the
	// USFMAnalysis struct for \bt for all back-translation markers based on \bt...
	if (bareMkr.Find(_T("bt")) == 0)
	{
		// bareMkr starts with bt... so shorten it to simply bt for lookup purposes
		bareMkr = _T("bt"); // bareMkr is value param so only affects local copy
	}
	MapSfmToUSFMAnalysisStruct::iterator iter;
    // The particular MapSfmToUSFMAnalysisStruct used for lookup below depends the
    // appropriate sfm set being used as stored in gCurrentSfmSet enum.
	switch (gpApp->gCurrentSfmSet)
	{
		case UsfmOnly: 
			iter = gpApp->m_pUsfmStylesMap->find(bareMkr);
			bFound = (iter != gpApp->m_pUsfmStylesMap->end());
			break;
		case PngOnly: 
			iter = gpApp->m_pPngStylesMap->find(bareMkr);
			bFound = (iter != gpApp->m_pPngStylesMap->end()); 
			break;
		case UsfmAndPng: 
			iter = gpApp->m_pUsfmAndPngStylesMap->find(bareMkr);
			bFound = (iter != gpApp->m_pUsfmAndPngStylesMap->end()); 
			break;
		default: 
			iter = gpApp->m_pUsfmStylesMap->find(bareMkr);
			bFound = (iter != gpApp->m_pUsfmStylesMap->end());
	}
	if (bFound)
	{
#ifdef _Trace_RebuildDoc
		TRACE2("LookupSFM: bareMkr = %s   gCurrentSfmSet = %d  USFMAnalysis FOUND\n",bareMkr,gpApp->gCurrentSfmSet);
		TRACE1("LookupSFM:         filtered?  %s\n", iter->second->filter == TRUE ? "YES" : "NO");
#endif
		// iter->second points to the USFMAnalysis struct
		return iter->second;
	}
	else
	{
#ifdef _Trace_RebuildDoc
		TRACE2("\n LookupSFM: bareMkr = %s   gCurrentSfmSet = %d  USFMAnalysis NOT FOUND  ...   Unknown Marker\n",
				bareMkr,gpApp->gCurrentSfmSet);
#endif
		return (USFMAnalysis*)NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if the passed-in marker unkMkr exists in any element of the pUnkMarkers 
///				array, FALSE otherwise
/// \param		pUnkMarkers		-> a pointer to a wxArrayString that contains a list of 
///									markers
/// \param		unkMkr			-> the whole marker being checked to see if it exists 
///                                in pUnkMarkers
/// \param		MkrIndex		<- the index into the pUnkMarkers array if unkMkr is 
///                                found, otherwise -1
/// \remarks
/// Called from: the Doc's RestoreDocParamsOnInput(), GetUnknownMarkersFromDoc(),
/// CFilterPageCommon::AddUnknownMarkersToDocArrays().
/// Determines if a standard format marker (whole marker including backslash) exists in any
/// element of the array pUnkMarkers.
/// If the whole marker exists, the function returns TRUE and the array's index where the
/// marker was found is returned in MkrIndex. If the marker doesn't exist in the array
/// MkrIndex returns -1.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::MarkerExistsInArrayString(wxArrayString* pUnkMarkers, 
											 wxString unkMkr, int& MkrIndex)
{
	// returns TRUE if the passed-in marker unkMkr already exists in the pUnkMarkers
	// array. MkrIndex is the index of the marker returned by reference.
	int ct;
	wxString arrayStr;
	MkrIndex = -1;
	for (ct = 0; ct < (int)pUnkMarkers->GetCount(); ct++)
	{
		arrayStr = pUnkMarkers->Item(ct);
		if (arrayStr.Find(unkMkr) != -1)
		{
			MkrIndex = ct;
			return TRUE;
		}
	}
	// if we get to here we didn't find unkMkr in the array; 
	// MkrIndex is still -1 and return FALSE
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if the passed-in marker wholeMkr exists in MarkerStr, FALSE otherwise
/// \param		MarkerStr	-> a wxString to be examined
/// \param		wholeMkr	-> the whole marker being checked to see if it exists in MarkerStr
/// \param		markerPos	<- the index into the MarkerStr if wholeMkr is found, otherwise -1
/// \remarks
/// Called from: the App's SetupMarkerStrings().
/// Determines if a standard format marker (whole marker including backslash) exists in a
/// given string. If the whole marker exists, the function returns TRUE and the zero-based
/// index into MarkerStr is returned in markerPos. If the marker doesn't exist in the
/// string markerPos returns -1.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::MarkerExistsInString(wxString MarkerStr, wxString wholeMkr, int& markerPos)
{
    // returns TRUE if the passed-in marker wholeMkr already exists in the string of
    // markers MarkerStr. markerPos is the position of the wholeMkr in MarkerStr returned
    // by reference.
	markerPos = MarkerStr.Find(wholeMkr);
	if (markerPos != -1)
		return TRUE;
    // if we get to here we didn't find wholeMkr in the string MarkerStr, so markerPos is
    // -1 and return FALSE
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if the pUsfmAnalysis represents a filtering marker, FALSE otherwise
/// \param		pUsfmAnalysis	-> a pointer to a USFMAnalysis struct
/// \remarks
/// Called from: the Doc's TokenizeText().
/// Determines if a USFMAnalysis struct indicates that the associated standard format marker
/// is a filtering marker.
/// Prior to calling IsAFilteringSFM, the caller should have called LookupSFM(wxChar* pChar)
/// or LookupSFM(wxString bareMkr) to populate the pUsfmAnalysis struct, which should then 
/// be passed to this function.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsAFilteringSFM(USFMAnalysis* pUsfmAnalysis)
{
	// whm added 10Feb2005 in support of USFM and SFM Filtering support
	// whm removed 2nd parameter 9Jun05
    // Prior to calling IsAFilteringSFM, the caller should have called LookupSFM(TCHAR
    // *pChar) or LookupSFM(CString bareMkr) to determine pUsfmAnalysis which should then
    // be passed to this function.

	// Determine the filtering state of the marker
	if (pUsfmAnalysis)
	{
		// we have a known filter marker so return its filter status from the USFMAnalysis
		// struct found by previous call to LookupSFM()
		return pUsfmAnalysis->filter;
	}
	else
	{
		// the passed in pUsfmAnalysis was NULL so
		return FALSE;
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if unkMkr is both an unknown marker and it also is designated as a 
///				filtering marker, FALSE otherwise.
/// \param		unkMkr	-> a bare marker (without a backslash)
/// \remarks
/// Called from: the Doc's AnalyseMarker(), RedoNavigationText().
/// Determines if a marker is both an unknown marker and it also is designated as a 
/// filtering marker.
/// unkMKr should be an unknown marker in bare form (without backslash)
/// Returns TRUE if unkMKr exists in the m_unknownMarkers array, and its filter flag 
/// in m_filterFlagsUnkMkrs is TRUE.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsAFilteringUnknownSFM(wxString unkMkr)
{
	// unkMKr should be an unknown marker in bare form (without backslash)
	// Returns TRUE if unkMKr exists in the m_unknownMarkers array, and its filter flag 
	// in m_filterFlagsUnkMkrs is TRUE.
	int ct;
	unkMkr = gSFescapechar + unkMkr; // add the backslash
	for (ct = 0; ct < (int)gpApp->m_unknownMarkers.GetCount(); ct++)
	{
	if (unkMkr == gpApp->m_unknownMarkers.Item(ct))
		{
		// we've found the unknown marker so check its filter status
			if (gpApp->m_filterFlagsUnkMkrs.Item(ct) == TRUE)
				return TRUE;
		}
	}
	// the unknown marker either wasn't found (an error), or wasn't flagged to be filtered, so
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing at a standard format marker, FALSE otherwise
/// \param		pChar		-> a pointer to a character in a buffer
/// \remarks
/// Called from: the Doc's ParseFilteringSFM(), ParseFilteredMarkerText(), 
/// GetMarkerAndTextFromString(), TokenizeText(), DoMarkerHousekeeping(), the View's
/// FormatMarkerBufferForOutput(), FormatUnstructuredTextBufferForOutput(), 
/// ApplyOutputFilterToText(), ParseMarkerAndAnyAssociatedText(), IsMarkerRTF(), and in
/// Usfm2Oxes class
/// Determines if pChar is pointing at a standard format marker in the given buffer
/// BEW 26Jan11, added test for character after the backslash, that it is alphabetic (this
/// prevents spurious TRUE returns if a \ is followed by whitespace)
/// BEW 31Jan11, made it smarter still
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsMarker(wxChar *pChar)
{
    // also use bool IsAnsiLetter(wxChar c) for checking character after backslash is an
    // alphabetic one; and in response to issues Bill raised in any email on Jan 31st about
    // spurious marker match positives, make the test smarter so that more which is not a
    // genuine marker gets rejected (and also, use IsMarker() in ParseWord() etc, rather
    // than testing for *ptr == gSFescapechar)
	if (*pChar == gSFescapechar)
	{
		// reject \n but allow the valid USFM markers \nb \nd \nd* \no \no* \ndx \ndx*
		if (*(pChar + 1) == _T('n'))
		{
			if (IsAnsiLetter(*(pChar + 2)))
			{
				// assume this is one of the allowed USFM characters listed in the above
				// comment
				return TRUE;
			}
			else if (IsWhiteSpace(pChar + 2)) // see helpers.cpp for definition
			{
				// it's an \n escaped linefeed indicator, not an SFM
				return FALSE;
			}
			else
			{
                // the sequence \n followed by some nonalphabetic character nor
                // non-whitespace character is unlikely to be a value SFM or USFM, so
                // return FALSE here too -- if we later want to make the function more
                // specific, we can put extra tests here
                return FALSE;
			}
		}
		else if (!IsAnsiLetter(*(pChar + 1)))
		{
			return FALSE;
		}
		else
		{
			// after the backslash is an alphabetic character, so assume its a valid marker
			return TRUE;
		}
	}
	else
	{
		// not pointing at a backslash, so it is not a marker
		return FALSE;
	}
}

bool CAdapt_ItDoc::IsMarker(wxString& mkr)
{
	const wxChar* pConstBuff = mkr.GetData();
	wxChar* ptr = (wxChar*)pConstBuff;
	return IsMarker(ptr);
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing at a standard format marker which is also an end 
///				marker (ends with an asterisk), FALSE otherwise.
/// \param		pChar	-> a pointer to a character in a buffer
/// \param		pEnd	-> a pointer to the end of the buffer
/// \remarks
/// Called from: the Doc's GetMarkersAndTextFromString(), AnalyseMarker(), the View's
/// FormatMarkerBufferForOutput(), DoExportInterlinearRTF(), ProcessAndWriteDestinationText().
/// Determines if the marker at pChar is a USFM end marker (ends with an asterisk). 
/// BEW added to it, 11Feb10, to handle SFM endmarkers \F or \fe for 'footnote end'
/// BEw added 11Oct10, support for halting at ] bracket
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsEndMarker(wxChar *pChar, wxChar* pEnd)
{
	// Returns TRUE if pChar points to a marker that ends with *
	wxChar* ptr = pChar;
	// Advance the pointer forward until one of the following conditions ensues:
	// 1. ptr == pEnd (return FALSE)
	// 2. ptr points to a space (return FALSE)
	// 3. ptr points to another marker (return FALSE)
	// 4. ptr points to a * (return TRUE)
	// 5. ptr points to a ]
	
	// First, handle the PngOnly special case of \fe or \F footnote end markers
	if (gpApp->gCurrentSfmSet == PngOnly)
	{
		wxString tempStr1(ptr,2);
		if (tempStr1 == _T("\\F"))
			return TRUE;
		wxString tempStr2(ptr,3);
		if (tempStr2 == _T("\\fe"))
			return TRUE;
	}

	// neither of those, so must by USFM endmarker if it is one at all
	while (ptr < pEnd)
	{
		ptr++;
		if (*ptr == _T('*'))
			return TRUE;
		else if (*ptr == _T(' ') || *ptr == gSFescapechar || *ptr == _T(']'))
			return FALSE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing at a standard format marker which is also 
///             an inLine marker (or embedded marker), FALSE otherwise.
/// \param		pChar	-> a pointer to a character in a buffer
/// \param		pEnd	<- currently unused
/// \remarks
/// Called from: the Doc's ParseFilteringSFM(), the View's FormatMarkerBufferForOutput().
/// Determines if the marker at pChar is a USFM inLine marker, i.e., one which is defined
/// in AI_USFM.xml with inLine="1" attribute. InLine markers are primarily "character
/// style" markers and also include all the embedded content markers whose parent markers
/// are footnote, endnotes and crossrefs.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsInLineMarker(wxChar *pChar, wxChar* WXUNUSED(pEnd))
{
	// Returns TRUE if pChar points to a marker that has inLine="1" [true] attribute
	wxChar* ptr = pChar;
	wxString wholeMkr = GetWholeMarker(ptr);
	int aPos = wholeMkr.Find(_T('*'));
	if (aPos != -1)
		wholeMkr.Remove(aPos,1);
	// whm revised 13Jul05. In order to get an accurate Find of wholeMkr below we
	// need to insure that the wholeMkr is followed by a space, otherwise Find would
	// give a false positive when wholeMkr is "\b" and the searched string has \bd, \bk
	// \bdit etc.
	wholeMkr.Trim(TRUE); // trim right end
	wholeMkr.Trim(FALSE); // trim left end
	wholeMkr += _T(' '); // insure wholeMkr has a single final space

	switch(gpApp->gCurrentSfmSet)
	{
	case UsfmOnly: 
		if (gpApp->UsfmInLineMarkersStr.Find(wholeMkr) != -1)
		{
			// it's an inLine marker
			return TRUE;
		}
		break;
	case PngOnly:
		if (gpApp->PngInLineMarkersStr.Find(wholeMkr) != -1)
		{
			// it's an inLine marker
			return TRUE;
		}
		break;
	case UsfmAndPng:
		if (gpApp->UsfmAndPngInLineMarkersStr.Find(wholeMkr) != -1)
		{
			// it's an inLine marker
			return TRUE;
		}
		break;
	default:
		if (gpApp->UsfmInLineMarkersStr.Find(wholeMkr) != -1)
		{
			// it's an inLine marker
			return TRUE;
		}
		break;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing at a standard format marker which is also a
///				corresponding end marker for the specified wholeMkr, FALSE otherwise.
/// \param		wholeMkr	-> a wxString containing the marker (including backslash)
/// \param		pChar		-> a pointer to a character in a buffer
/// \param		pEnd		-> a pointer to the end of the buffer
/// \remarks
/// Called from: the Doc's ParseFilteringSFM(), ParseFilteredMarkerText(), 
/// GetMarkersAndTextFromString(), the View's ParseFootnote(), ParseEndnote(),
/// ParseCrossRef(), ProcessAndWriteDestinationText(), ApplyOutputFilterToText()
/// ParseMarkerAndAnyAssociatedText(), and CViewFilteredMaterialDlg::InitDialog().
/// Determines if the marker at pChar is the corresponding end marker for the
/// specified wholeMkr. 
/// IsCorresEndMarker returns TRUE if the marker matches wholeMkr and ends with an
/// asterisk. It also returns TRUE if the gCurrentSfmSet is PngOnly and wholeMkr 
/// passed in is \f and marker being checked at ptr is \fe or \F.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsCorresEndMarker(wxString wholeMkr, wxChar *pChar, wxChar* pEnd)
{
	// Returns TRUE if the marker matches wholeMkr and ends with *
	// Also returns TRUE if the gCurrentSfmSet is PngOnly and wholeMkr passed in
	// is \f and marker being checked at ptr is \fe or \F
	wxChar* ptr = pChar;

	// First, handle the PngOnly special case of \fe footnote end marker
	if (gpApp->gCurrentSfmSet == PngOnly && wholeMkr == _T("\\f"))
	{
		wxString tempStr = GetWholeMarker(ptr);
		// debug
		//int len;
		//len = tempStr.Length();
		// debug
		if (tempStr == _T("\\fe") || tempStr == _T("\\F"))
		{
			return TRUE;
		}
	}

	// not a PngOnly footnote situation so do regular USFM check 
	// for like a marker ending with * 
	int wholeMkrLen = wholeMkr.Length(); // only needs to be calculated once
	for (int i = 0; i < wholeMkrLen; i++)
	{
		if (ptr < pEnd)
		{
			if (*ptr != wholeMkr[i])
				return FALSE;
			ptr++;
		}
		else
			return FALSE;
	}
	// markers match through end of wholeMkr
	if (ptr < pEnd)
	{
		if (*ptr != _T('*'))
			return FALSE;
	}
	// the marker at pChar has an asterisk on it so we have the corresponding end marker
	return TRUE;
}

bool CAdapt_ItDoc::IsLegacyDocVersionForFileSaveAs()
{
	return m_bLegacyDocVersionForSaveAs;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing at a standard format marker which is also a
///				chapter marker (\c ), FALSE otherwise.
/// \param		pChar		-> a pointer to a character in a buffer
/// \remarks
/// Called from: the Doc's TokenizeText(), DoMarkerHousekeeping(), 
/// DoExportInterlinearRTF(), DoExportSrcOrTgtRTF().
/// Returns TRUE only if the character following the backslash is a c followed by whitespace,
/// FALSE otherwise. Does not check to see if a number follows the whitespace.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsChapterMarker(wxChar *pChar)
{
	wxChar* ptr = pChar;
	ptr++;
	if (*ptr == _T('c'))
	{
		ptr++;
		return IsWhiteSpace(ptr);
	}
	else
		return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing at a Null character, i.e., (wxChar)0.
/// \param		pChar		-> a pointer to a character in a buffer
/// \remarks
/// Called from: the Doc's ParseWord(), TokenizeText(), DoMarkerHousekeeping(), the View's
/// DoExportSrcOrTgtRTF() and ProcessAndWriteDestinationText().
/// Returns TRUE if the buffer character at pChar is the null character (wxChar)0.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsEnd(wxChar *pChar)
{
	return *pChar == (wxChar)0;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a wxString constructed of the characters from the buffer, starting with the
///				character at ptr and including the next itemLen-1 characters.
/// \param		dest		<- a wxString that gets concatenated with the composed src string
/// \param		src			<- a wxString constructed of the characters from the buffer, starting with the
///								character at ptr and including the next itemLen-1 characters
/// \param		ptr			-> a pointer to a character in a buffer
/// \param		itemLen		-> the number of buffer characters to use in composing the src string
/// \remarks
/// Called from: the Doc's TokenizeText() and DoMarkerHousekeeping().
/// AppendItem() actually does double duty. It not only returns the wxString constructed from
/// itemLen characters (starting at ptr); it also returns by reference the composed
/// string concatenated to whatever was previously in dest.
/// In actual code, no use is made of the returned wxString of the AppendItem() 
/// function itself; only the value returned by reference in dest is used.
/// TODO: Change to a Void function since no use is made of the wxString returned.
///////////////////////////////////////////////////////////////////////////////
wxString& CAdapt_ItDoc::AppendItem(wxString& dest,wxString& src, const wxChar* ptr, int itemLen)
{
	src = wxString(ptr,itemLen);
	dest += src;
	return src;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a wxString constructed of dest + src (with inserted space between them if 
///				dest doesn't already end with a space).
/// \param		dest		<- a wxString that gets concatenated with the composed src string
/// \param		src			-> a wxString to be appended/concatenated to dest
/// \remarks
/// Called from: the Doc's TokenizeText().
/// AppendFilteredItem() actually does double duty. It not only returns the wxString src;
/// it also returns by reference in dest the composed/concatenated dest + src (insuring
/// that a space intervenes between unless dest was originally empty.
/// In actual code, no use is made of the returned wxString of the AppendFilteredItem() 
/// function itself; only the value returned by reference in dest is used.
/// TODO: Change to a Void function since no use is made of the wxString returned.
///////////////////////////////////////////////////////////////////////////////
wxString& CAdapt_ItDoc::AppendFilteredItem(wxString& dest,wxString& src)
{
	// whm added 11Feb05
	// insure the filtered item is padded with space if it is not first 
	// in dest
    if (!dest.IsEmpty())
	{
		if (dest[dest.Length() - 1] != _T(' '))
			// append a space, but only if there is not already one at the end
			dest += _T(' ');
	}
    dest += src;
    dest += _T(' ');
    return src;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		the wxString starting at ptr and composed of itemLen characters 
///             after enclosing the string between \~FILTER and \~FILTER* markers.
/// \param		ptr			-> a pointer to a character in a buffer
/// \param		itemLen		-> the number of buffer characters to use in composing the 
///                            bracketed string
/// \remarks
/// Called from: the Doc's ReconstituteAfterFilteringChange(), TokenizeText().
/// Constructs the string starting at ptr (whose length is itemLen in the buffer); then
/// makes the string a "filtered" item by bracketing it between \~FILTER ... \~FILTER*
/// markers. The passed in string may be just a marker (contentless, and having no
/// following space), or a marker followed by a space and some text content (and possibly
/// then a space and then possibly an endmarker as well)
/// BEW 21Sep10, no change needed for docVersion 5
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::GetFilteredItemBracketed(const wxChar* ptr, int itemLen)
{
    // whm added 11Feb05; BEW changed 06Oct05 to simpify a little and remove the unneeded
    // first argument (which was a CString& -- because it was being called with wholeMkr
    // supplied as that argument's string, which was clobbering the marker in the caller)
    // bracket filtered info with unique markers \~FILTER and \~FILTER*
	// wxString src;
	wxString temp(ptr,itemLen);
	temp.Trim(TRUE); // trim right end
	temp.Trim(FALSE); // trim left end
	//wx version handles embedded new lines correctly
	wxString temp2;
	temp2 << filterMkr << _T(' ') << temp << _T(' ') << filterMkrEnd;
	temp = temp2;
	return temp;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a wxString with any \~FILTER and \~FILTER* filter bracketing markers removed
/// \param		src		-> the string to be processed (which has \~FILTER and \~FILTER* markers)
/// \remarks
/// Called from: the View's IsWrapMarker().
/// Returns the string after removing any \~FILTER ... \~FILTER* filter bracketing markers 
/// that exist in the string. Strips out multiple sets of bracketing filter markers if found
/// in the string. If src does not have any \~FILTER and \~FILTER* bracketing markers, src is
/// returned unchanged. Trims off any remaining space at left end of the returned string.
/// BEW 22Feb10, no changes for support of doc version 5
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::GetUnFilteredMarkers(wxString& src)
{
	// whm added 11Feb05
	// If src does not have the unique markers \~FILTER and \~FILTER* we only need return 
	// the src unchanged
	// The src may have embedded \n chars in it. Note: Testing shows that use
	// of CString's Find method here, even with embedded \n chars works OK. 
	int beginMkrPos = src.Find(filterMkr);
	int endMkrPos = src.Find(filterMkrEnd);
	while (beginMkrPos != -1 && endMkrPos != -1)
	{
		// Filtered material markers exist so we can remove all text between
		// the two filter markers inclusive of the markers. Filtered material
		// is never embedded within other filtered material so we can assume
		// that each sequence of filtered text we encounter can be deleted as
		// we progress linearly from the beginning of the src string to its end.
		wxString temps = filterMkrEnd;
		src.Remove(beginMkrPos, endMkrPos - beginMkrPos + temps.Length());
		beginMkrPos = src.Find(filterMkr);
		endMkrPos = src.Find(filterMkrEnd);
	}
	// Note: The string returned by GetUnFilteredMarkers may have an initial 
	// space, which I think would not usually happen in the legacy app before
	// filtering. I have therefore added the following line, which is also
	// probably needed for proper functioning of IsWrapMarker in the View:
	src.Trim(FALSE); // FALSE trims left end only
	return src;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		0 (zero)
/// \remarks
/// Called from: the Doc's TokenizeText(), DoMarkerHousekeeping(), 
/// Clear's the App's working buffer.
/// TODO: Eliminate this function and the App's working buffer and just declare and use a local 
/// wxString buffer in the two Doc functions that call ClearBuffer(), and the View's version of 
/// ClearBuffer().
///////////////////////////////////////////////////////////////////////////////
int CAdapt_ItDoc::ClearBuffer()
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	pApp->buffer.Empty();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE unless the text in rText contains at least one marker that defines it 
///				as "structured" text, in which case returns FALSE
/// \param		rText	-> the string buffer being examined
/// \remarks
/// Called from: the Doc's TokenizeText().
/// Returns TRUE if rText does not have any of the following markers: \id \v \vt \vn \c \p \f \s \q
/// \q1 \q2 \q3 or \x.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsUnstructuredPlainText(wxString& rText)
// we deem the absence of \id and any of \v \vt \vn \c \p \f \s \q
// \q1 \q2 \q3 or \x standard format markers to be sufficient
// evidence that it is unstructured plain text
{
	wxString s1 = gSFescapechar;
	int nFound = -1;
	wxString s = s1 + _T("id ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \id
	s = s1 + _T("v ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \v
	s = s1 + _T("vn ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \vn
	s = s1 + _T("vt ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \vt
	s = s1 + _T("c ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \c

	// BEW added 10Apr06 to support small test files with just a few markers
	s = s1 + _T("p ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \p
	s = s1 + _T("f ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \f
	s = s1 + _T("s ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \s
	s = s1 + _T("q ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \q
	s = s1 + _T("q1 ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \q1
	s = s1 + _T("q2 ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \q2
	s = s1 + _T("q3 ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \q3
	s = s1 + _T("x ");
	nFound = rText.Find(s);
	if (nFound >= 0)
		return FALSE; // has \x
	// that should be enough, ensuring correct identification 
	// of even small test files with only a few SFM markers
	return TRUE; // assume unstructured plain text
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if some character other than end-of-line char(s) (\n and/or \r) is found 
///				past the nFound position in rText, otherwise FALSE.
/// \param		rText		-> the string being examined
/// \param		nTextLength	-> the length of the rText string
/// \param		nFound		-> the position in rText beyone which we examine content
/// \remarks
/// Called from: the Doc's AddParagraphMarkers().
/// Determines if there are any characters other than \n or \r beyond the nFound position in
/// rText. Used in AddParagraphMarkers() to add "\p " after each end-of-line in rText.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::NotAtEnd(wxString& rText, const int nTextLength, int nFound)
{
	nFound++; // get past the newline
	if (nFound >= nTextLength - 1)
		return FALSE; // we are at the end

	int index = nFound;
	wxChar ch;
	while (((ch = rText.GetChar(index)) == _T('\r') || (ch = rText.GetChar(index)) == _T('\n'))
			&& index < nTextLength)
	{
		index++; // skip the carriage return or newline
		if (index >= nTextLength)
			return FALSE; // we have arrived at the end
	}

	return TRUE; // we found some other character before the end was reached
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		rText		-> the string being examined
/// \param		nTextLength	-> the length of the rText string
/// \remarks
/// Called from: the Doc's TokenizeText().
/// Adds "\p " after each end-of-line in rText. The addition of \p markers is done to provide
/// minimal structuring of otherwise "unstructured" text for Adapt It's internal operations.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::AddParagraphMarkers(wxString& rText, int& nTextLength)
{
	// adds \p followed by a space following every \n in the text buffer
	wxString s = gSFescapechar; 
	s += _T("p ");
	const wxChar* paragraphMarker = (const wxChar*)s;
	int nFound = 0;
	int nNewLength = nTextLength;
	while (((nFound = FindFromPos(rText,_T("\n"),nFound)) >= 0) && 
			NotAtEnd(rText,nNewLength,nFound))
	{
		nFound++; // point past the newline

		// we are not at the end, so we insert \p here
		rText = InsertInString(rText,nFound,paragraphMarker);
		nNewLength = rText.Length();
	}
	nTextLength = nNewLength;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a wxString with any multiple spaces reduced to single spaces
/// \param		rString		<- 
/// \remarks
/// Called from: the Doc's TokenizeText() and the View's DoExportSrcOrTgt().
/// Cleans up rString by reducing multiple spaces to single spaces.
/// TODO: This function could do its work much faster if it were rewritten to use a read buffer
/// and a write buffer instead of reading each character from a write buffer and concatenating 
/// the character onto a wxString. See OverwriteUSFMFixedSpaces() for a shell routine to use
/// as a starting point for revision.
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::RemoveMultipleSpaces(wxString& rString)
// reduces multiple spaces to a single space (code to do it using Find( ) function fails,
// because Find( ) has a bug which causes it to return the wrong index value wxWidgets
// note: Our Find() probably works, but we'll convert this anyway
// As written this function doesn't really need a write buffer since rString is not
// modified by the routine. A better approach would be to get rString's buffer using
// GetData(), and set up a write buffer for atemp which is the same size as rString + 1.
// Then copy characters using pointers from the rString buffer to the atemp buffer (not
// advancing the pointer for where multiple spaces are adjacent to each other).
{
	int nLen = rString.Length();
	wxString atemp;
	atemp.Empty();
	wxASSERT(nLen >= 0);
	wxChar* pStr = rString.GetWriteBuf(nLen + 1);
	wxChar* pEnd = pStr + nLen;
	wxChar* pNext;
	while (pStr < pEnd)
	{
		if (*pStr == _T(' '))
		{
			atemp += *pStr;
			pNext = pStr;
x:			++pNext;
			if (pNext >= pEnd)
			{
				rString.UngetWriteBuf();
				return atemp;
			}
			if (*pNext == _T(' '))
				goto x;
			else
				pStr = pNext;
		}
		else
		{
			atemp += *pStr;
			++pStr;
		}
	}
	rString.UngetWriteBuf(); //ReleaseBuffer();

	// wxWidgets code below (avoids use of pointers and buffer)
	// wxWidgets Find works OK, but the Replace method is quite slow.
	//wxString atemp = rString;
	//while ( atemp.Find(_T("  ")) != -1 )
	//	atemp.Replace(_T("  "), _T(" "),FALSE);
	return atemp;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		pstr	<- the wxString buffer 
/// \remarks
/// Called from: the Doc's OnNewDocument(), 
/// Removes any existing fixed space ~ in pstr by overwriting it with a space. The
/// processed text is returned by reference in pstr. This function call would normally be
/// followed by a call to RemoveMultipleSpaces() to remove any remaining multiple spaces.
/// In our case, the subsequent call of TokenizeText() in OnNewDocument() discards any
/// extra spaces left by OverwriteUSFMFixedSpaces().
/// BEW 23Nov10, changed to support ~ rather than !$ (the latter is deprecated)
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OverwriteUSFMFixedSpaces(wxString*& pstr)
{
    // whm revised in wx version to have input string by reference in first parameter and
    // to set up a write buffer within this function.
	int len = (*pstr).Length();
	wxChar* pBuffer = (*pstr).GetWriteBuf(len + 1);
	wxChar* pBufStart = pBuffer;
	wxChar* pEnd = pBufStart + len;
	wxASSERT(*pEnd == _T('\0'));
	wxChar* ptr = pBuffer;
	while (ptr < pEnd)
	{
		if (*ptr == _T('~'))
		{
			// we are pointing at an instance of ~, 
			// so overwrite it and continue processing
			*ptr++ = _T(' ');
		}
		else
		{
			ptr++;
		}
	}
	// whm len should not have changed, just release the buffer
	(*pstr).UngetWriteBuf();
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		pstr	<- the wxString buffer 
/// \remarks
/// Called from: the Doc's OnNewDocument(), 
/// Removes any existing discretionary line break // sequences in pstr by overwriting the
/// sequence with spaces. The processed text is returned by reference in pstr.
/// This function call would normally be followed by a call to RemoveMultipleSpaces() to
/// remove any remaining multiple spaces. In our case, the subsequent call of
/// TokenizeText() in OnNewDocument() discards any extra spaces left by
/// .OverwriteUSFMDiscretionaryLineBreaks().
void CAdapt_ItDoc::OverwriteUSFMDiscretionaryLineBreaks(wxString*& pstr)
{
	int len = (*pstr).Length();
	wxChar* pBuffer = (*pstr).GetWriteBuf(len + 1);
	wxChar* pBufStart = pBuffer;
	wxChar* pEnd = pBufStart + len;
	wxASSERT(*pEnd == _T('\0'));
	wxChar* ptr = pBuffer;
	while (ptr < pEnd)
	{
		if (wxStrncmp(ptr,_T("//"),2) == 0)
		{
			// we are pointing at an instance of //, 
			// so overwrite it and continue processing
			*ptr++ = _T(' ');
			*ptr++ = _T(' ');
		}
		else
		{
			ptr++;
		}
	}
	// whm len should not have changed, just release the buffer
	(*pstr).UngetWriteBuf();
}

#ifndef __WXMSW__
#ifndef _UNICODE
///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		pstr	<- the wxString buffer 
/// \remarks
/// Called from: the Doc's OnNewDocument().
/// Changes MS Word "smart quotes" to regular quotes. The character values for smart quotes
/// are negative (-108, -109, -110, and -111). Warns the user if other negative character 
/// values are encountered in the text, i.e., that he should use TecKit to convert the data 
/// to Unicode then use the Unicode version of Adapt It.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OverwriteSmartQuotesWithRegularQuotes(wxString*& pstr)
{
	// whm added 12Apr2007
	bool hackedFontCharPresent = FALSE;
	int hackedCt = 0;
	wxString hackedStr;
	hackedStr.Empty();
	int len = (*pstr).Length();
	wxChar* pBuffer = (*pstr).GetWriteBuf(len + 1);
	wxChar* pBufStart = pBuffer;
	wxChar* pEnd = pBufStart + len;
	wxASSERT(*pEnd == _T('\0'));
	wxChar* ptr = pBuffer;
	while (ptr < pEnd)
	{
		if (*ptr == -111) // left single smart quotation mark
		{
			// we are pointing at a left single smart quote mark, so convert it to a regular single quote mark
			*ptr++ = _T('\'');
		}
		else if (*ptr == -110) // right single smart quotation mark
		{
			// we are pointing at a right single smart quote mark, so convert it to a regular single quote mark
			*ptr++ = _T('\'');
		}
		else if (*ptr == -109) // left double smart quotation mark
		{
			// we are pointing at a left double smart quote mark, so convert it to a regular double quote mark
			*ptr++ = _T('\'');
		}
		else if (*ptr == -108) // right double smart quotation mark
		{
			// we are pointing at a left double smart quote mark, so convert it to a regular double quote mark
			*ptr++ = _T('\'');
		}
		else if (*ptr < 0)
		{
			// there is a hacked 8-bit character besides smart quotes. Warn user that the data will not
			// display correctly in this version, that he should use TecKit to convert the data to Unicode
			// then use the Unicode version of Adapt It
			hackedFontCharPresent = TRUE;
			hackedCt++;
			if (hackedCt < 10)
			{
				int charValue = (int)(*ptr);
				hackedStr += _T("\n   character with ASCII value: ");
				hackedStr << (charValue+256);
			}
			else if (hackedCt == 10)
				hackedStr += _T("...\n");
			ptr++; // advance but don't change the char (we warn user below)
		}
		else
		{
			ptr++;
		}
	}
	// whm len should not have changed, just release the buffer
	(*pstr).UngetWriteBuf();

	// In this case we should warn every time a new doc is input that has the hacked chars
	// so we don't test for  && !gbHackedDataCharWarningGiven here.
	if (hackedFontCharPresent)
	{
		gbHackedDataCharWarningGiven = TRUE;
		wxString msg2 = _("\nYou should not use this non-Unicode version of Adapt It.\nYour data should first be converted to Unicode using TecKit\nand then you should use the Unicode version of Adapt It.");
		wxString msg1 = _("Extended 8-bit ASCII characters were detected in your\ninput document:");
		msg1 += hackedStr + msg2;
		wxMessageBox(msg1,_("Warning: Invalid Characters Detected"),wxICON_WARNING);
	}
}
#endif
#endif


///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if the passed in (U)SFM marker is \free, \note, or \bt or a derivative
///             FALSE otherwise
/// \param		mkr                     ->  the augmented marker (augmented means it ends with a space)
/// \param      bIsForeignBackTransMkr  <-  default FALSE, TRUE if the marker is \btxxx
///                                         where xxx is one or more non-whitespace
///                                         characters (such as \btv 'back trans of
///                                         verse', \bts 'back trans of subtitle' or
///                                         whatever - Bob Eaton uses such markers in SAG)
/// \remarks
/// Called from: the Doc's TokenizeText().
/// Test for one of the custom Adapt It markers which require the filtered information to
/// be stored on m_freeTrans, m_note, or m_collectedBackTrans string members used for 
/// document version 5 (see docVersion in the xml)
/// 
/// BEW modified 19Feb10 for support of doc version = 5. Bob Eaton's markers will be
/// parsed, and when identified, will be wrapped with filterMkr and filterMkrEnd, and
/// stored in m_filteredInfo; and got from there for any exports where requested; but
/// Adapt It will no longer attempt to treat such foreign markers as "collected", it will
/// just ignore them - but they will be displayed in the Filtered Information dialog.
/// Adapt It's \bt marker will have its content stored in m_collectedBackTrans member
/// instead, and without any preceding \bt. So the added parameter allows us to determine
/// when we are parsing a marker starting with \bt but is not our own because of extra
/// characters in it.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsMarkerFreeTransOrNoteOrBackTrans(const wxString& mkr, bool& bIsForeignBackTransMkr)
{
	bIsForeignBackTransMkr = FALSE; // initialize to default value
	if (mkr == _T("\\free "))
	{
		return TRUE;
	}
	else if (mkr == _T("\\note "))
	{
		return TRUE;
	}
	else
	{
		int offset = mkr.Find(_T("\\bt"));
		if (offset == 0)
		{
			// check for whether it is our own, or a foreign back trans marker
			int length = mkr.Len();
			if (length > 4)
			{
				// it has at least one extra character before the final space,
				// so it is a foreign one
				bIsForeignBackTransMkr = TRUE;
			}	
			return TRUE;
		}
	}
	return FALSE;
}

// BEW March 2010 for support of doc version 5
void CAdapt_ItDoc::SetFreeTransOrNoteOrBackTrans(const wxString& mkr, wxChar* ptr,
								size_t itemLen, CSourcePhrase* pSrcPhrase)
{
	// if it is one of the three custom markers, set the relevent
	// CSourcePhrase member directly here
	wxString filterStr(ptr,(size_t)itemLen);
	size_t len;
	wxChar aChar;
	if (mkr == _T("\\free"))
	{
		filterStr = filterStr.Mid(6); // start from after "\free "
		// remove |@number@| string -- don't bother to return the number value because it
		// is done later in TokenizeText() after this present function returns
		int nFound = filterStr.Find(_T("|@"));
		if (nFound != wxNOT_FOUND)
		{
			// there is the src word count number substring present, remove it and its
			// following space
			int nFound2 = filterStr.Find(_T("@| "));
			wxASSERT(nFound2 - nFound < 10);
			filterStr.Remove(nFound, nFound2 + 3 - nFound);
		}
		len = filterStr.Len();
		// end of filterStr will be "\free*" == 6 characters
		filterStr = filterStr.Left((size_t)len - 6);
		// it may also end in a space now, so remove it if there
		filterStr.Trim();
		// we now have the free translation text, so store it
		pSrcPhrase->SetFreeTrans(filterStr);
	}
	else if (mkr == _T("\\note"))
	{
		filterStr = filterStr.Mid(6); // start from after "\note "
		len = filterStr.Len();
		// end of filterStr will be "\note*" == 6 characters
		filterStr = filterStr.Left((size_t)len - 6);
		// it may also end in a space now, so remove it if there
		filterStr.Trim();
		// we now have the note text, so store it
		pSrcPhrase->SetNote(filterStr);
	}
	else
	{
		// could be \bt, or longer markers beginning with those 3 chars
		aChar = filterStr.GetChar(0);
		while (!IsWhiteSpace(&aChar))
		{
			// trim off from the front the marker info, a character at
			// a time
			filterStr = filterStr.Mid(1);
			aChar = filterStr.GetChar(0);
		}
		filterStr.Trim(FALSE); // trim any initial white space
		// it may also end in a space now, so remove it if there
		filterStr.Trim();
		// we now have the back trans text, so store it
		pSrcPhrase->SetCollectedBackTrans(filterStr);
	}
}


///////////////////////////////////////////////////////////////////////////////
/// \return		the number of elements/tokens in the list of source phrases (pList)
/// \param		nStartingSequNum	-> the initial sequence number
/// \param		pList				<- list of CSourcePhrase instances, each populated with 
///                                    a word token
/// \param		rBuffer				-> the buffer from which words are tokenized and stored 
///									   as CSourcePhrase instances in pList
/// \param		nTextLength			-> the initial text length
/// \param      bTokenizingTargetText -> default FALSE, set TRUE if rBuffer contains target
///                                    text which is to be parsed, using target punctuation,
///                                    so as to make use of its smarts in separating text,
///                                    punctuation and inline markers 
/// \remarks
/// Called from: the Doc's OnNewDocument(), the View's TokenizeTextString(),
/// DoExtendedSearch(), DoSrcOnlyFind(), DoTgtOnlyFind(), DoSrcAndTgtFind().
/// Intelligently parses the input text (rBuffer) and builds a list of CSourcePhrase
/// instances from the tokens. All the input text's source phrases are analyzed in the
/// process to determine each source phrase's many attributes and flags, stores any
/// filtered information in its m_filteredInfo member.
/// BEW Feb10, updated for support of doc version 5 (changes were needed)
/// BEW 11Oct10, updated for doc version 5 additional changes (better inline marker
/// support) quite significantly - somewhat simplifying TokenizeText() but completely
/// rewriting the ParseWord() function it calls -- and the latter potentially consumes more
/// data on each call. TokenizeText also reworked to handle text colouring better.
///////////////////////////////////////////////////////////////////////////////
int CAdapt_ItDoc::TokenizeText(int nStartingSequNum, SPList* pList, wxString& rBuffer, 
							   int nTextLength, bool bTokenizingTargetText)
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);

	// whm Note: I'm declaring a local tokBuffer, in place of the buffer that MFC had on the doc
	// and previously the wx version had on the App. This is in attempt to get beyond the string
	// corruption problems.
	wxString tokBuffer;
	tokBuffer.Empty();

    // BEW 11Oct10, for carrying a post-ParseWord() decision forward to pre-ParseWord()
    // location on next iteration
	bool bEnded_f_fe_x_span = FALSE;

    // for support of parsing in and filtering a pre-existing free translation (it has to
    // have CSourcePhrase instances have their m_bStartFreeTrans, m_bEndFreeTrans &
    // m_bHasFreeTrans members set TRUE at appropriate places)
	bool bFreeTranslationIsCurrent = FALSE;
	int nFreeTransWordCount = 0;

	wxString spacelessPuncts;
	// BEW 11Jan11, added test here so that the function can be used on target text as
	// well as on source text
	if (bTokenizingTargetText)
	{
		spacelessPuncts = pApp->m_punctuation[1];
	}
	else
	{
		spacelessPuncts = pApp->m_punctuation[0];
	}
	while (spacelessPuncts.Find(_T(' ')) != -1)
	{
		// remove all spaces, leaving only the list of punctation characters
		spacelessPuncts.Remove(spacelessPuncts.Find(_T(' ')),1); 
	}
	wxString boundarySet = spacelessPuncts;
	while (boundarySet.Find(_T(',')) != -1)
	{
		boundarySet.Remove(boundarySet.Find(_T(',')),1);
	}

    // if the user is inputting plain text unstructured, we do not want to destroy any
    // paragraphing by the normalization process. So we test for this kind of text (if
    // there is no \id at file start, and also none of \v \vn \vt \c markers in the text,
    // then we assume it is not a sfm file) and if it is such, then we add a \p and a space
    // (paragraph marker) following every newline; and later if the user exports the source
    // or target text, we check if it was a plain text unstructured file by looking for (1)
    // no \id marker on the first sourcephrase instance, and no instances of \v \vn or \vt
    // in any sourcephrase in the document - if it matches these conditions, we test for \p
    // markers and where such is found, change the preceding space to a newline in the
    // output.
	bool bIsUnstructured = IsUnstructuredPlainText(rBuffer);

	// if unstructured plain text, add a paragraph marker after any newline, to preserve 
	// user's paragraphing updating nTextLength for each one added
	int nDerivedLength = nTextLength;
	if (bIsUnstructured)
	{
		AddParagraphMarkers(rBuffer, nDerivedLength);
		wxASSERT(nDerivedLength >= nTextLength);
	}

	// continue with the parse
	int nTheLen;
	if (bIsUnstructured)
	{
		nTheLen = nDerivedLength; // don't use rBuffer.GetLength() - as any newlines 
        // don't get counted; Bruce commented out the next line 10May08, but I've left it
        // there because I've dealt with and checked that other code agrees with the code
        // as it stands.
		// BEW 30Jan11, I really think this should be commented out, but in the light of
		// Bill's comment, I'll compromise -- only do this is nDerivedLength and
		// nTextLength are different & the former more than the latter) -- anyway, I doubt
		// that it matters either way any more
		if (nDerivedLength > nTextLength)
		{
			++nTheLen; // make sure we include space for a null
		}
	}
	else
	{
		nTheLen = nTextLength; // nTextLength was probably m_nInputFileLength in 
							   // the caller, which already counts the null at the end
	}

    // whm revision: I've modified OverwriteUSFMFixedSpaces and
    // OverwriteUSFMDiscretionaryLineBreaks to use a write buffer internally, and moved
    // them out of TokenizeText, so now we can get along with a read-only buffer here in
    // TokenizeText
	const wxChar* pBuffer = rBuffer.GetData();
	int itemLen = 0;
	wxChar* ptr = (wxChar*)pBuffer;		 // point to start of text
	wxChar* pBufStart = ptr;	 // preserve start address for use in testing for
								 // contextually defined sfms
	wxChar* pEnd = pBufStart + rBuffer.Length(); // bound past which we must not go
	wxASSERT(*pEnd == _T('\0')); // ensure there is a null there
	wxString temp;		// small working buffer in which to build a string
	tokBuffer.Empty(); 
	int	 sequNumber = nStartingSequNum - 1;
	CSourcePhrase* pLastSrcPhrase = (CSourcePhrase*)NULL; // initially there isn't one
	bool bHitMarker;
	USFMAnalysis* pUsfmAnalysis = NULL; // whm added 11Feb05
	bool bIsFreeTransOrNoteOrBackTrans = FALSE;
	bool bIsForeignBackTrans = FALSE;
	// BEW added 11Oct10, two booleans for helping with inline mkrs other than \f & \x
	bool bIsInlineNonbindingMkr = FALSE;
	bool bIsInlineBindingMkr = FALSE;

	// the parsing loop commences...
	while (ptr < pEnd)
	{
		// we are not at the end, so we must have a new CSourcePhrase instance ready
		// BEWARE - for doc version 5, if the end of the buffer has endmarkers, pSrcPhrase
		// will receive them, but there will be empty m_key and m_srcPhrase members - we
		// have to test for this possibility and when it happens, move the endmarkers to
		// the preceding CSourcePhrase's m_endMarkers member, then delete the empty last
		// CSourcePhrase instance which then is no longer needed (provided it's
		// m_precPunct member is also empty, if not, we have to leave it to carry that
		// punctuation)
		CSourcePhrase* pSrcPhrase = new CSourcePhrase;
		wxASSERT(pSrcPhrase != NULL);
		sequNumber++;
		pSrcPhrase->m_nSequNumber = sequNumber; // number it in sequential order
		bHitMarker = FALSE;

		if (IsWhiteSpace(ptr))
		{
            // advance pointer past the white space (inter-word spaces should be thrown
            // away, they have no useful role once tokenization has taken place)
			itemLen = ParseWhiteSpace(ptr);
			ptr += itemLen; 
		}
		// BEW 11Oct10 we need to support [ and ] brackets as markers indicating 'this is
		// disputed material', and so we will do so by storing [ and ] on their own
		// CSourcPhrase 'orphan' instances (ie. m_key and m_srcPhrase will be empty), with
		// [ stored in m_precPunct (and follow it with space(s) only if space()s is in the
		// data), and store ] in m_follPunct (and include preceding space(s) only if
		// space(s) is in the input data). We do the above whether or not [ and ] are
		// specified as punctuation characters or not. They will be considered to be
		// punctuation characters for this delimitation purpose even if not listed in the
		// sets of source and target punctuation characters. (Further below, if [ follows
		// such things as \v 33 then the accumulated m_markers text has to be stored on
		// an orphaned CSourcePhrase carrying the [ bracket.)
		if (*ptr == _T('[') || *ptr == _T(']'))
		{
			if (*ptr == _T('['))
			{
				// we've come to an opening bracket, [
				pSrcPhrase->m_precPunct = *ptr; // store it here, whether punctuation or not
				if (IsWhiteSpace(ptr + 1))
				{
					// store a following space as well, but just one - any other white
					// space we'll ignore
					pSrcPhrase->m_precPunct += _T(" ");
					ptr += 2; // point past the [ and the first of the following white
							  // space chars
				}
				else
				{
					ptr++; // point past the [
				}
				pSrcPhrase->m_srcPhrase = pSrcPhrase->m_precPunct; // need this for the [ 
						// bracket (and its following space if we stored one) to be visible
				if (pSrcPhrase != NULL)
				{
					// put this completed orphan pSrcPhrase into the list
					pList->Append(pSrcPhrase);
				}
				// make this one become the 'last one' of the next iteration
				pLastSrcPhrase = pSrcPhrase;
				continue; // iterate
			}
			else
			{
				// must be a closing bracket,  ]
				pSrcPhrase->m_follPunct = *ptr; // store it here, whether punctuation or not
				if (IsWhiteSpace(ptr - 1)) // we can assume ] is not at the start of the input file
				{
					// store a preceding space as well, but just one - any other white
					// space we'll ignore
					pSrcPhrase->m_follPunct = _T(" ") + pSrcPhrase->m_follPunct;
				}
				ptr++; // point past the ]
				pSrcPhrase->m_srcPhrase = pSrcPhrase->m_follPunct; // need this for the ] 
						// bracket (and its preceding space if we stored one) to be visible
				if (pSrcPhrase != NULL)
				{
					// put this completed orphan pSrcPhrase into the list
					pList->Append(pSrcPhrase);
				}
				// make this one become the 'last one' of the next iteration
				pLastSrcPhrase = pSrcPhrase;
				continue; // iterate
			}
		}

		// are we at the end of the text?
		if (IsEnd(ptr) || ptr >= pEnd)
		{
			// check for an incomplete CSourcePhrase, it may need endmarkers moved, etc
			if (pSrcPhrase != NULL)
			{
				if (pSrcPhrase->m_key.IsEmpty())
				{
					if (!pSrcPhrase->GetEndMarkers().IsEmpty())
					{
						// there are endmarkers which belong on the previous instance, so
						// transfer them
						if (pLastSrcPhrase != NULL)
						{
							pLastSrcPhrase->SetEndMarkers(pSrcPhrase->GetEndMarkers());
						}
					}
				}
			}
			// BEW added 05Oct05
			if (bFreeTranslationIsCurrent)
			{
                // we default to always turning off a free translation section at the end
                // of the document if it hasn't been done already
				if (pLastSrcPhrase != NULL)
				{
					pLastSrcPhrase->m_bEndFreeTrans = TRUE;
				}
			}
			// delete only if there is nothing in m_precPunct
			if (pSrcPhrase->m_precPunct.IsEmpty())
			{
				DeleteSingleSrcPhrase(pSrcPhrase,FALSE); // don't try partner pile deletion
			}
			tokBuffer.Empty();
			break;
		}

		// BEW 11Oct10, use an inner loop... rather than gotos
		// are we pointing at a standard format marker?
		while (IsMarker(ptr))
		{
			bIsFreeTransOrNoteOrBackTrans = FALSE; // clear before 
									// checking which marker it is
			bIsForeignBackTrans = FALSE;
			bHitMarker = TRUE; // set whenever a marker of any type is reached
			bIsInlineNonbindingMkr = FALSE; // ensure it is initialized
			bIsInlineBindingMkr = FALSE; // ensure it is initialized

			int nMkrLen = 0;
			// its a marker of some kind
			if (IsVerseMarker(ptr,nMkrLen))
			{
                // starting a new verse, clear the following flag to FALSE so that it
                // has a chance to work helpfully for the parse of this verse
				m_bHasPrecedingStraightQuote = FALSE;

				// its a verse marker
				if (nMkrLen == 2)
				{
					tokBuffer += gSFescapechar;
					tokBuffer += _T("v");
					ptr += 2; // point past the \v marker
				}
				else
				{
					tokBuffer += gSFescapechar;
					tokBuffer += _T("vn");
					ptr += 3; // point past the \vn marker (Indonesia branch)
				}

				itemLen = ParseWhiteSpace(ptr);
				// temp returns the string at ptr with length itemLen, and the same string
				// is appended to tokBuffer as well (usually we just want tokBuffer, but
				// temp is available if we need the substring at ptr for any other purpose)
				AppendItem(tokBuffer,temp,ptr,itemLen); // add white space to buffer
				ptr += itemLen; // point at verse number or verse string eg. "3b"

				itemLen = ParseNumber(ptr);
				AppendItem(tokBuffer,temp,ptr,itemLen); // add number (or range eg. 3-5) to buffer
				pSrcPhrase->m_chapterVerse = pApp->m_curChapter; // set to n: form
				pSrcPhrase->m_chapterVerse += temp; // append the verse number
				pSrcPhrase->m_bVerse = TRUE; // set the flag to signal start of a new verse
				ptr += itemLen; // point past verse number

				// set pSrcPhrase attributes
				pSrcPhrase->m_bVerse = TRUE;

				itemLen = ParseWhiteSpace(ptr); // past white space after the marker
				AppendItem(tokBuffer,temp,ptr,itemLen);  // add it to the buffer
				ptr += itemLen; // point past the white space

				// BEW added 05Oct05
				if (bFreeTranslationIsCurrent)
				{
                    // we default to always turning off a free translation section at a new
                    // verse if a section is currently open -- this prevents assigning a
                    // free translation to the rest of the document if we come to where no
                    // free translations were assigned in another project's adaptations
                    // which we are inputting
					if (pLastSrcPhrase)
					{
						pLastSrcPhrase->m_bEndFreeTrans = TRUE;
					}
				}

				// BEW 25Feb11, a new verse should change TextType to verse, and m_bSpecialText
				// to FALSE, except when a previous poetry marker has set poetry TextType already
				// Removed because later code will override what we do here, so might as
				// well not do it
				//pSrcPhrase->m_bSpecialText = FALSE;	
				//if (pSrcPhrase->m_curTextType != poetry)
				//{
					// the text type is not poetry, so change to it to verse
				//	pSrcPhrase->m_curTextType = verse;
				//}
				continue; // iterate inner loop to check if another marker follows
			}
			else if (IsChapterMarker(ptr)) // is it some other kind of marker - 
										   // perhaps it's a chapter marker?
			{
				// its a chapter marker
				tokBuffer << gSFescapechar;
				tokBuffer << _T("c");
				ptr += 2; // point past the \c marker

				itemLen = ParseWhiteSpace(ptr);
				AppendItem(tokBuffer,temp,ptr,itemLen); // add white space to buffer
				ptr += itemLen; // point at chapter number

				itemLen = ParseNumber(ptr);
				AppendItem(tokBuffer,temp,ptr,itemLen); // add chapter number to buffer
				pApp->m_curChapter = temp;
				pApp->m_curChapter += _T(':'); // get it ready to append verse numbers
				ptr += itemLen; // point past chapter number

				// set pSrcPhrase attributes
				pSrcPhrase->m_bChapter = TRUE;
				
				itemLen = ParseWhiteSpace(ptr); // parse white space following the number
				AppendItem(tokBuffer,temp,ptr,itemLen); // add it to buffer
				ptr += itemLen; // point past it

				continue; // iterate inner loop to check if another marker follows
			}
			else
			{
				// neither verse nor chapter, but some other marker
				pUsfmAnalysis = LookupSFM(ptr); // If an unknown marker, pUsfmAnalysis
												// will be NULL

//#ifdef __WXDEBUG__
//		if (pSrcPhrase->m_nSequNumber == 139)
//		{
//			int break_point_here = 1;
//		}
//#endif		
                // whm revised this block 11Feb05 to support USFM and SFM Filtering. When
                // TokenizeText enounters previously filtered text (enclosed within
                // \~FILTER ... \~FILTER* brackets), it strips off those brackets so that
                // TokenizeText can evaluate anew the filtering status of the marker(s)
                // that had been embedded within the filtered text. If the markers and
                // associated text are still to be filtered (as determined by LookupSFM()
                // and IsAFilteringSFM()), the filtering brackets are added again. If the
                // markers should no longer be filtered, they and their associated text are
                // processed normally.

				// BEW comment 11Oct10, this block handles filtering. In the legacy
				// version of TokenizeText() propagation of TextType was also handled
				// here, resulting in a spagetti mishmash of code. I've taken out the
				// propagation code and put it after the ParseWord() call, so that what is
				// done here is simpler - just handling any needed filtering.
                 
                // BEW 11Oct10, A note about how we handle inline markers, other than those
                // beginning with \f (footnote type) or \x (crossReference type), is
                // appropriate. While not a USFM distinction, for Adapt It's purposes we
                // view these as coming in two subtypes - binding ones (these bind more
                // closely to the word than punctuation, eg \k \k*, \w \w*, etc) and
                // non-binding ones (only 5, \wj \wj*, \tl \tl*, \sls \sls*, \qt \qt*, and
                // \fig \fig*) which are 'outer' to punctuation - that is, punctuation
                // binds more closely to the word than these. We want ParseWord() to handle
                // parsing once any of the non-\f and non-\x inline markers are
                // encountered, and we don't want any of these to insert its TextType value
                // into the document in m_curTextType anywhere at all, in doc version 5. So
                // we check for these marker subtypes and process accordingly if any such
                // has just been come to. Such markers are never filtered nor filterable,
                // we just want to ignore them as much as possible, but reconstitute them
                // correctly to the exported text when an export has been requested. Also,
                // these are the marker types which very often have endmarkers, and we want
                // ParseWord() to take over all processing of endmarkers, so that
                // TokenizeText() only deals with beginmarkers. In doc version 5 it never
                // happens now that an endmarker will be stored in m_markers (in the legacy
                // parser, that was not true).
                // BEW 25Feb11, added code to use pUsfmAnalysis looked up to set
                // m_curTextType and m_bSpecialText which were forgottten here
				
				bool bDidSomeFiltering = FALSE;

				wxString wholeMkr = GetWholeMarker(ptr);
				wxString augmentedWholeMkr = wholeMkr + _T(' '); // prevent spurious matches
				wxString bareMkr = wholeMkr.Mid(1); // chop off the initial backslash
				int anOffset = wxNOT_FOUND;

				// BEW 11Oct10, the block for detecting an inline marker which is not
				// one of the \f set nor one of the \x set..., if the test succeeds, then
				// in the block we set needed flags and then break out of the inner loop
				// 
                // BEW 25Feb11, we don't want to hand off \va ...\va*, (verse alternate)
                // nor \vp ...\vp* (verse published) to ParseWord() - which will correctly
                // handle them as inline binding markers, but leave the number as adaptable
                // text in the view; instead, these are default filtered in the AI_USFM.xml
                // file, so we have to test for them here and skip this block if either of
				// these was what bareMkr is (only if the user unfilters them should their
				// number be seeable and adaptable)
				if (pUsfmAnalysis != NULL && pUsfmAnalysis->inLine &&
					bareMkr.Find('f') != 0 && bareMkr.Find('x') != 0 && 
					bareMkr.Find(_T("va")) != 0 && bareMkr.Find(_T("vp")) != 0)
				{
                    // inline markers are known to USFM, so pUsfmAnalysis will not be
                    // false; the test succeeds if it is not an unknown marker, and is an
                    // inline marker, but not one of the inline markers which begin with
                    // \x or \f; these are the ones which we immediately hand off to
                    // ParseWord() to deal with
					
					// the hand-off takes place outside the loop, so set the flags we need
					// to know beforehand, they are needed for ParseWord()'s signature
					anOffset = pApp->m_inlineNonbindingMarkers.Find(augmentedWholeMkr);
					if (anOffset != wxNOT_FOUND)
					{
						bIsInlineNonbindingMkr = TRUE;
						bIsInlineBindingMkr = FALSE;
					}
					else
					{
						bIsInlineNonbindingMkr = FALSE;
						bIsInlineBindingMkr = TRUE;
					}
					break; // break out of the inner loop, to get to ParseWord()
				}
				
                // this is the legacy code block, simplified - for handling all other
                // markers other than those we've bled out in the above block
				// BEW 11Oct10, removed code for propagating TextType and m_bSpecialText
				// from here, as we do it after ParseWord() call now
				
				// check if we have located an SFM designated as one to be filtered
				// pUsfmAnalysis is populated regardless of whether it's a filter marker
				// or not. If an unknown marker, pUsfmAnalysis will be NULL

				// BEW changed 26May06; because of the following scenario. Suppose \y is in
				// the source text. This is an unknown marker, and if the app has not seen
				// this marker yet or, if it has, and the user has not nominated it as one
				// which is to be filtered out, then it will not be listed in the
				// fast-access string gCurrentFilterMarkers. Earlier versions of
				// TokenizeText() did not examine the contents of gCurrentFilterMarkers
				// when parsing source text, consequently, when the app is in the state
				// where \y as been requested to be filtered out (eg. as when user opens a
				// document which has that marker and in which it was filtered out; or has
				// created a document earlier in which \y occurred and he then requested it
				// be filtered out and left that setting intact when creating the current
				// document (which also has an \y marker)) then the current document
				// (unless we do something different than before) would not look at
				// gCurrentFilterMarkers and so not filter it out when, in fact, it should.
				// Moreover this can get worse. Firstly, because the
				// IsAFilteringUnknownSFM() call in AnalyseMarker looks at
				// m_currentUnknownMarkersStr, it detects that \y is currently designated
				// as "to be filtered" - and so refrains from placing "?\y? in the m_inform
				// member of the CSourcePhrase where the (unfiltered) unknown marker starts
				// (and it has special text colour, of course). So then the user sees a
				// text colour change and does not know why. If the unknown marker happens
				// to occur after some other special text, such as a footnote, then (even
				// worse) both have the same colour and the text in the unknown marker
				// looks like it is part of the footnote! Yuck.
				// 
                // The solution is to ensure that TokenizeText() gets to look at the
                // contents of gCurrentFilterMarkers every time a new doc is created, and
                // if it finds the marker listed there, to ensure it is filtered out. It's
                // no good appealing to AnalyseMarker(), because it just uses what is in
                // pUsfmAnalysis, and that comes from AI_USFM.xml, which by definition,
                // never lists unknown markers. So the changes in the next few lines fix
                // all this - the test after the || had to be added.
				if (IsAFilteringSFM(pUsfmAnalysis) || 
					(gpApp->gCurrentFilterMarkers.Find(augmentedWholeMkr) != -1))
				{
					bDidSomeFiltering = TRUE;
					itemLen = ParseFilteringSFM(wholeMkr,ptr,pBufStart,pEnd);

					// get filtered text bracketed by \~FILTER and \~FILTER*
					bIsFreeTransOrNoteOrBackTrans = IsMarkerFreeTransOrNoteOrBackTrans(
													augmentedWholeMkr,bIsForeignBackTrans);
					if (bIsForeignBackTrans)
					{
                        // it's a back translation type of marker of foreign origin (extra
                        // chars after the t in \bt) so we just tuck it away in
                        // m_filteredInfo
						temp = GetFilteredItemBracketed(ptr,itemLen);
					}
					else if (bIsFreeTransOrNoteOrBackTrans)
					{
						SetFreeTransOrNoteOrBackTrans(wholeMkr, ptr, (size_t)itemLen, pSrcPhrase);
						wxString aTempStr(ptr,itemLen);
						temp = aTempStr; // don't need to wrap with \~FILTER etc, get just
						// enough for the code below to set the value between |@ and @|
					}
					else
					{
                        // other filterable markers go in m_filteredInfo, and have to be
                        // wrapped with \~FILTER and \~FILTER* and put into m_filteredInfo
						temp = GetFilteredItemBracketed(ptr,itemLen);
					}

					// We may be at some free translation's anchor pSrcPhrase, having just
					// set up the filter string to be put into m_markers; and if so, this
					// string will contain a count of the number of following words to
					// which the free translation applies; and this count will be
					// bracketted by |@ (bar followed by @) at the start and @|<space> at
					// the end, so we can search for these and if found, we extract the
					// number string and remove the whole substring because it is only
					// there to inform the parse operation and we don't want it in the
					// constructed document. We use the count to determine which pSrcPhrase
					// later encountered is the one to have its m_bEndFreeTrans member set
					// TRUE.
					int nFound = temp.Find(_T("|@"));
					if (nFound != -1)
					{
						// there is some free translation to be handled
						int nFound2 = temp.Find(_T("@| "));
						wxASSERT(nFound2 - nFound < 10); // characters between can't be 
														 // too big a number
						wxString aNumber = temp.Mid(nFound + 2, nFound2 - nFound - 2);
						nFreeTransWordCount = wxAtoi(aNumber);
						wxASSERT(nFreeTransWordCount >= 0);

						// now remove the substring
						temp.Remove(nFound, nFound2 + 3 - nFound);

						// now check for a word count value of zero -- we would get this if
						// the user, in the project which supplied the exported text data,
						// free translated a section of source text but did not supply any
						// target text (if a target text export was done -- the same can't
						// happen for source text of course). When such a \free field
						// occurs in the data, there will be no pSrcPhrase to hang it on,
						// because the parser will just collect all the empty markers into
						// m_markers; so when we get a count of zero we should throw away
						// the propagated free translation & let the user type another if
						// he later adapts this section
						if (nFreeTransWordCount == 0)
						{
							temp.Empty();
						}
					}

                    // BEW added 05Oct05; CSourcePhrase class has new BOOL attributes in
                    // support of notes, backtranslations and free translations, so we have
                    // to set these at appropriate places in the parse.
					if (!bIsFreeTransOrNoteOrBackTrans || bIsForeignBackTrans)
					{
						// other filtered stuff needs to be saved here (not later), it has
						// been wrapped with \~FILTER and \~FILTER*; there is no need to
						// use a delimiting space between the filter markers
						pSrcPhrase->AddToFilteredInfo(temp);
					}
					if (wholeMkr == _T("\\note"))
					{
						pSrcPhrase->m_bHasNote = TRUE;
					}
					if (bFreeTranslationIsCurrent)
					{
						// a free translation section is current
						if (wholeMkr == _T("\\free"))
						{
							// we've arrived at the start of a new section of free
							// translation -- we should have already turned it off, but
							// since we obviously haven't we'll do so now
							if (nFreeTransWordCount != 0)
							{
								bFreeTranslationIsCurrent = TRUE; // turn on this flag to 
									// inform parser in subsequent iterations that a new 
									// one is current
								if (pLastSrcPhrase->m_bEndFreeTrans == FALSE)
								{
									pLastSrcPhrase->m_bEndFreeTrans = TRUE; // turn off 
																	// previous section
								}

								// indicate start of new section
								pSrcPhrase->m_bHasFreeTrans = TRUE;
								pSrcPhrase->m_bStartFreeTrans = TRUE;
							}
							else
							{
								// we are throwing this section away, so turn off the flag
								bFreeTranslationIsCurrent = FALSE;

								if (pLastSrcPhrase->m_bEndFreeTrans == FALSE)
								{
									pLastSrcPhrase->m_bEndFreeTrans = TRUE; // turn off 
																	// previous section
								}
							}
						}
						else
						{
							// for any other marker, if the section is not already turned
							// off, then just propagate the free translation section to
							// this sourcephrase too
							pSrcPhrase->m_bHasFreeTrans = TRUE;
						}
					}
					else
					{
						// no free translation section is currently in effect, so check to
						// see if one is about to start
						if (wholeMkr == _T("\\free"))
						{
							bFreeTranslationIsCurrent = TRUE; // turn on this flag to inform 
								// parser in subsequent iterations that one is current
							pSrcPhrase->m_bHasFreeTrans = TRUE;
							pSrcPhrase->m_bStartFreeTrans = TRUE;
						}
					}
				} // end of filtering block
				else
				{
					// BEW added comment 21May05: it's not a filtering one, so the marker's
					// contents (if any) will be visible and adaptable. The code here will
					// ensure the marker is added to the tokBuffer variable, and eventually be
					// saved in m_markers; but for endmarkers we have to suppress their use
					// for display in the navigation text area - we must do that job in
					// AnalyseMarker() below - after the ParseWord() call.
					itemLen = ParseMarker(ptr);
					AppendItem(tokBuffer,temp,ptr,itemLen);
                    // being a non-filtering marker, it can't possibly be \free, and so we
                    // don't have to worry about the filter-related boolean flags in
                    // pSrcPhrase here
					// BEW 25Feb11, since it's neither inline nor filtering, it will be
					// stored in m_markers and so is a candidate for changing
					// m_bSpecialText and m_currTextType, so use the pUsfmAnalysis
					// obtained above, and compare with pLastSrcPhrase when the latter is
					// non-NULL
					// Removed because later code will override what we do here, so might as
					// well not do it
					//if (pLastSrcPhrase != NULL)
					//{
					//	if (pLastSrcPhrase->m_bSpecialText && !pUsfmAnalysis->special)
					//	{
							// we must switch back to non special text colour
					//		pSrcPhrase->m_bSpecialText = FALSE;
					//	}
					//	if (pLastSrcPhrase->m_curTextType != pUsfmAnalysis->textType)
					//	{
							// the text type is different, so change to it (a subsequent
							// marker may change it again though, if there is another to
							// be parsed after this current one)
					//		pSrcPhrase->m_curTextType = pUsfmAnalysis->textType;
					//	}
					//}
				}
				// advance pointer past the marker
				ptr += itemLen;

				itemLen = ParseWhiteSpace(ptr); // parse white space following it
				AppendItem(tokBuffer,temp,ptr,itemLen); // add it to buffer
				ptr += itemLen; // advance ptr past its following whitespace

				continue; // iterate the inner loop to check if another marker follows

			} // BEW added 11Oct10, end of else block for test for inline mrk,
			  // this else block has the legacy code for the pre-11Oct10 versions
			  
		} // end of inner loop, while (IsMarker(ptr)), BEW 11Oct10
		
		// Back in the outer loop now. We have one of the following two situations: 
		// (1) ptr is not pointing at an inline marker that we need to handle within
		// ParseWord(), so it may be pointing at punctuation or a word of text to be
		// parsed; or
		// (2) ptr is pointing at an inline marker we need to handle within ParseWord()
		// because these can have non-predictable interactions with punctuation,
		// especially punctuation which follows the wordform, and ParseWord() has the
		// smarts for dealing with all the possibilities that may occur. (It NEVER happens
		// that an ordinary marker will occur between the inline one and the word proper,
		// so we can safely hand off to ParseWord() knowing the inner loop above is
		// finished for this particular pSrcPhrase.)
		// (3) ptr is pointing at a [ opening bracket which needs to be stored on the
		// current pSrcPhrase (in its m_precPunct member, along with any following space
		// if one is present in the input file after the [ bracket) along with any
		// m_markers content already accumulated - we must do that before parsing further
		// because 'further' would then belong to the next CSourcePhrase instance
		
		// bleed out the ptr pointing at [ bracket situation
		if (*ptr == _T('['))
		{
			pSrcPhrase->m_precPunct = *ptr;
			pSrcPhrase->m_srcPhrase = *ptr;
			ptr++;
			if (IsWhiteSpace(ptr))
			{
				// also store a following single space, if there was some white space
				// following (this is so that SFM export will reproduce the space)
				pSrcPhrase->m_precPunct += _T(" ");
				ptr++;
			}
			if (!tokBuffer.IsEmpty())
			{
				pSrcPhrase->m_markers = tokBuffer;
				tokBuffer.Empty();
			}
			// store the pointer in the SPList (in order of occurrence in text)
			if (pSrcPhrase != NULL)
			{
				pList->Append(pSrcPhrase);
			}

			// make this one be the "last" one for next time through
			pLastSrcPhrase = pSrcPhrase; // note: pSrcPhrase might be NULL
			continue;
		}
		// not pointing at [ so do the parsing of the word - including puncts and inline
		// markers
#ifdef __WXDEBUG__
//		if (pSrcPhrase->m_nSequNumber == 49)
//		{
//			int breakpt_here = 1;
//		}
#endif
		// the TokenizeText() caller determines whether spacelessPuncts contains the
		// m_punctuation[0] source puncts set, or m_punctuation[1] target set; typically,
		// it is the source set, but we do use an override of the TokenizeTextString()
		// with a boolean at the end, when we explicitly want to parse target text - in
		// that case, the boolean is TRUE and the target puncts set get passed to
		// TokenizeText() which then removes spaces (whichever set it receives, it removes
		// spaces before passing the punctuation characters themselves to TokenizeText())
		// - if any functions within ParseWord() require space to be in the passed-in set
		// of punctuation characters, they can add an explicit space when they first run
		itemLen = ParseWord(ptr, pEnd, pSrcPhrase, spacelessPuncts,
							pApp->m_inlineNonbindingMarkers,
							pApp->m_inlineNonbindingEndMarkers,
							bIsInlineNonbindingMkr, bIsInlineBindingMkr);
		ptr += itemLen; // advance ptr over what we parsed

        // We do NormalizeToSpaces() only on the string of standard format markers which
        // we store on sourcephrase instances in m_markers, it's not needed elsewhere in
        // CSourcePhrase storage members.
		tokBuffer = NormalizeToSpaces(tokBuffer);

		// In the next call, doc version 4 would put all filtered and non-filted marker
		// stuff into m_markers, but version 5 saves filtered stuff in m_filteredInfo
		// and free translations, notes and collected back translations (each without
		// any markers, just the bare text) in member variables m_freeTrans, m_note,
		// and m_collectedBackTrans, before getting to this point, and inline markers in
		// four special members as well, preceding and following, depending on whether
		// they are binding or non-binding. So, for doc version 5 tokBuffer will have
		// within it only the beginmarkers accumulated from the parse before the
		// ParseWord() call takes place.
		pSrcPhrase->m_markers = tokBuffer;

		// ************ New Propagation Code Starts Here ******************
		bool bTextTypeChanges = FALSE;
		bool bEndedffexspanUsedToChangedTextType = FALSE;
		// if not done before ParseWord() was called, do the update here, if needed
		if (bEnded_f_fe_x_span)
		{
			// this block re-establishes verse type, and m_bSpecialText FALSE, after a
			// footnote, endnote or crossReference has ended - it will stay set unless
			// there is a marker in tokBuffer, which the next block will analyse, to
			// establish possibly different values
			pSrcPhrase->m_curTextType = verse;
			pSrcPhrase->m_bSpecialText = FALSE;
			bEnded_f_fe_x_span = FALSE; // once the TRUE value is used, it must default
										// back to FALSE
			bTextTypeChanges = TRUE;
			pSrcPhrase->m_bFirstOfType = TRUE;
			bEndedffexspanUsedToChangedTextType = TRUE;

		}
		if (!tokBuffer.IsEmpty())
		{
			// set TextType and m_bSpecialText according to what the type and special text
			// value are for the last marker in tokBuffer
			if (tokBuffer.Find(gSFescapechar) != wxNOT_FOUND)
			{
				// there is a marker there
				int offset = wxNOT_FOUND;
				int len = 0;
				wxString wholeMkr; wholeMkr.Empty();
				bool bContainsPoetryMarker = FALSE; // BEW added 25Feb11
				while ((offset = tokBuffer.Find(gSFescapechar)) != wxNOT_FOUND) 
				{
					tokBuffer = tokBuffer.Mid(offset);
					wholeMkr = GetWholeMarker(tokBuffer);
					// check for a poetry marker, set the flag if we find one
					wxString wholeMkrPlusSpace = wholeMkr + _T(' ');
					if (pApp->m_poetryMkrs.Find(wholeMkrPlusSpace) != wxNOT_FOUND)
					{
						// this is a poetry marker (\v may follow, so use the flag
						// later to prevent poetry TextType from being overridden by verse
						bContainsPoetryMarker = TRUE;
					}
					len = wholeMkr.Len();
					tokBuffer = tokBuffer.Mid(len);
				}
				// m_markers doesn't store endmarkers in docversion 5, so we know it must
				// be a beginmarker, or an empty string; if it is a beginmarker, it might
				// be an unknown (ie. not definied in AI_USFM.xml) marker, such as \y, and
				// so we must detect any such and ensure the text they mark is coloured
				// with the current m_bSpecialText = TRUE colour (default is red)
				if (len > 1)
				{
					wxString bareMkr = wholeMkr.Mid(1);
					int length = wholeMkr.Len();
					const wxChar* pChar2 = wholeMkr.GetData();
					wxChar* pEnd;
					pEnd = (wxChar*)pChar2 + length;
					wxChar* pBufStart = (wxChar*)pChar2;
					// in the next call NULL is returned if bareMkr is an unknown marker
					USFMAnalysis* pUsfmAnalysis = LookupSFM(bareMkr); 
					// the AnalyseMarker() call looks up the characteristics of the marker
					// and assigns TextType, returns m_bSpecialText value, puts appropriate
					// text into the m_inform member, deals appropriately with
					// pUsfmAnalysis being NULL (returns TRUE, and TextType noType is
					// assigned and m_inform gets the marker bracketted with ? and ? before
					// and after) etc
					pSrcPhrase->m_bSpecialText = AnalyseMarker(pSrcPhrase, pLastSrcPhrase,
														pBufStart, length, pUsfmAnalysis);
					// if there was a preceding poetry marker for a verse marker,
					// choose the poetry TextType instead
					if (bContainsPoetryMarker) 
					{
						pSrcPhrase->m_curTextType = poetry;
						bTextTypeChanges = TRUE;
					}
					if (pLastSrcPhrase != NULL)
					{
						// BEW 25Feb11, added the 2nd test after the OR, because some
						// markers (e.g. \d for poetry description) are given in
						// AI_USFM.xml as TRUE for special text, but TextType of
						// verse, and so just testing the TextType for a difference
						// isn't enough, because pLastSrcPhrase might have TRUE for
						// special text, and need to now change to FALSE, which won't
						// happen if both last and current have verse TextType - well,
						// at least not without the extra test I've just added
						if ((pLastSrcPhrase->m_curTextType != pSrcPhrase->m_curTextType) ||
							(pLastSrcPhrase->m_bSpecialText != pSrcPhrase->m_bSpecialText))
						{
							bTextTypeChanges = TRUE;
						}
					}
				}
				else
				{
					bTextTypeChanges = FALSE;
				}
			}
		}
		else
		{
			if (!bEndedffexspanUsedToChangedTextType)
			{
				// the bEnded_f_fe_x_span flag was not TRUE, and so was not used to cause
				// the above block to be entered and default verse, and non-special text
				// to be re-established there; and so, we allow the fact that tokBuffer
				// (which sets m_markers content) being empty to signal to the code below
				// (by the bTextTypeChanges value being cleared to FALSE) that it is okay
				// below to copy the values off of pLastSrcPhrase
				bTextTypeChanges = FALSE;
			}
		}
		tokBuffer.Empty();
		// implement the decisions regarding propagation made above...
		bool bTextTypeChangeBlockEntered = FALSE;
		if (pLastSrcPhrase != NULL)
		{
			// propagate or change the TextType and m_bSpecialText
			if (bTextTypeChanges)
			{
				// text type and specialText value are set above, so this block won't
				// change the value established above shortly after the ParseWord() call
				// returns; & we must restore the default value for the flag
				bTextTypeChanges = FALSE;
				bTextTypeChangeBlockEntered = TRUE;
				pSrcPhrase->m_bFirstOfType = TRUE;
			}
			else if (IsTextTypeChangingEndMarker(pSrcPhrase))
			{
				// the test is TRUE if pSrcPhase in its m_endMarkers member contains one
				// or \f* or \fe* or \x*; we set the bEnded_f_fe_x_span flag here only,
				// because it is the next word to be parsed that actually has the changed
				// TextType value etc - the TRUE value of the flag is detected above after
				// ParseWord() returns, and causes default to verse TextType and
				// m_bSpecialText FALSE - and subsequent code then may change it if
				// necessary.
				bEnded_f_fe_x_span = TRUE;
				// propagation from pLastSrcPhrase to pSrcPhrase is required here, because
				// this one's TextType isn't changed
				pSrcPhrase->m_bSpecialText = pLastSrcPhrase->m_bSpecialText;
				pSrcPhrase->m_curTextType = pLastSrcPhrase->m_curTextType;

				// Finally, we have to do one task which in the legacy parser was done in
				// AnalyseMarker(). If \f* is stored on pSrcPhrase, then we have to set
				// the flag m_bFootnoteEnd to TRUE (so that CPile::DrawNavTextInfoAndIcons(
				// wxDC* pDC) can add the "end fn" text to pSrcPhrase->m_inform, for
				// display in the nav text area of the view) -- note, \fe in m_endMarkers
				// can only be the PNG SFM 1998 footnote endmarker, not USFM endnote
				// beginmarker) likewise \F must be from the same 1998 set if found there
				if ((pSrcPhrase->GetEndMarkers().Find(_T("\\f*")) != wxNOT_FOUND) || 
					(pSrcPhrase->GetEndMarkers().Find(_T("\\fe")) != wxNOT_FOUND) ||
					(pSrcPhrase->GetEndMarkers().Find(_T("\\F")) != wxNOT_FOUND))
				{
					pSrcPhrase->m_bFootnoteEnd = TRUE;
				}
			}
			else
			{
				// don't propagate values from pLastSrcPhrase if the TRUE block for
				// bTextTypeChanges was entered - when that block is entered, a new
				// TextType and possibly a change of m_bSpecialText value is commencing
				// and we don't want the new values wiped out here by overwriting with
				// those from pLastSrcPhrase
				if (!bTextTypeChangeBlockEntered)
				{
					pSrcPhrase->m_bSpecialText = pLastSrcPhrase->m_bSpecialText;
					pSrcPhrase->m_curTextType = pLastSrcPhrase->m_curTextType;
				}
			}
		}
		// ************ New Propagation Code Ends Here ******************

		// BEW added 30May05, to remove any initial space that may be in m_markers 
		// from the parse
		if (pSrcPhrase->m_markers.GetChar(0) == _T(' '))
			pSrcPhrase->m_markers.Trim(FALSE);

		// BEW 11Oct10, Handle setting of the m_bBoundary flag here, rather than in
		// ParseWord() itself
		if (!pSrcPhrase->m_follPunct.IsEmpty() || 
			!pSrcPhrase->GetFollowingOuterPunct().IsEmpty())
		{
			wxChar anyChar;
			if (!pSrcPhrase->m_follPunct.IsEmpty())
			{
				anyChar = pSrcPhrase->m_follPunct[0]; // any char of punct will do
				if (boundarySet.Find(anyChar) != wxNOT_FOUND)
				{
                    // we found a non-comma final punctuation character on this word, so we
                    // have to set a boundary here
					pSrcPhrase->m_bBoundary = TRUE;
					// if this pSrcPhrase stores a conjoined word pair using USFM fixed
					// space symbol ~ then the boundary flag on the last child in the
					// m_pSavedWords member also needs to be set
					if (IsFixedSpaceSymbolWithin(pSrcPhrase))
					{
						SPList::Node* pos = pSrcPhrase->m_pSavedWords->GetLast();
						CSourcePhrase* pWord2 = pos->GetData();
						pWord2->m_bBoundary = TRUE;
					}
				}
			}
			if (!pSrcPhrase->GetFollowingOuterPunct().IsEmpty())
			{
				anyChar = (pSrcPhrase->GetFollowingOuterPunct())[0];
				if (boundarySet.Find(anyChar) != wxNOT_FOUND)
				{
                    // we found a non-comma final punctuation character on this word, so we
                    // have to set a boundary here
					pSrcPhrase->m_bBoundary = TRUE;
					// if this pSrcPhrase stores a conjoined word pair using USFM fixed
					// space symbol ~ then the boundary flag on the last child in the
					// m_pSavedWords member also needs to be set
					if (IsFixedSpaceSymbolWithin(pSrcPhrase))
					{
						SPList::Node* pos = pSrcPhrase->m_pSavedWords->GetLast();
						CSourcePhrase* pWord2 = pos->GetData();
						pWord2->m_bBoundary = TRUE;
					}
				}
			}
		}

		// get rid of any final spaces which make it through the parse -- shouldn't be any
		// now, but no harm in doing the check etc
		pSrcPhrase->m_follPunct.Trim(TRUE); // trim right end
		pSrcPhrase->m_follPunct.Trim(FALSE); // trim left end

		// BEW 11Oct10, do trim for the new m_follOuterPunct member
		wxString follOuterPunct = pSrcPhrase->GetFollowingOuterPunct();
		if (!follOuterPunct.IsEmpty())
		{
			follOuterPunct.Trim(TRUE); // trim right end
			follOuterPunct.Trim(FALSE); // trim left end
			pSrcPhrase->SetFollowingOuterPunct(follOuterPunct);
		}

        // handle propagation of the m_bHasFreeTrans flag, and termination of the free
        // translation section by setting m_bEndFreeTrans to TRUE, when we've counted
        // off the requisite number of words parsed - there will be one per
        // sourcephrase when parsing; we do this only when bFreeTranslationIsCurrent is
        // TRUE
		// BEW 11Oct10, now that we have better support for fixedspace ~ markup, we have
		// to count 2 words for any m_key which contains the ~ symbol
		if (bFreeTranslationIsCurrent)
		{
			if (nFreeTransWordCount != 0)
			{
				// decrement the count
				nFreeTransWordCount--;
				if (IsFixedSpaceSymbolWithin(pSrcPhrase))
				{
					// decrement by an additional one, if possible
					if (nFreeTransWordCount > 0)
					{
						nFreeTransWordCount--; // because m_key is word~word
					}
				}
				pSrcPhrase->m_bHasFreeTrans = TRUE;
				if (nFreeTransWordCount == 0)
				{
                    // we decremented to zero, so we are at the end of the current free
                    // translation section, so set the flags accordingly
					pSrcPhrase->m_bEndFreeTrans = TRUE; // indicate 'end of section'
					bFreeTranslationIsCurrent = FALSE; // indicate next sourcephrase 
                        // is not in the section (but a \free marker may turn it back
                        // on at next word)
				}
				
			}
		}
		// BEW added comment 11Oct10, the comment below was for the legacy parser, but it
		// is still apt if the new ParseWord() returns leaving some punctuation at the end
		// of the buffer - unlikely, but we can't rule it out. So keep this stuff.
		// 
        // If endmarkers are at the end of the buffer, code further up will have put them
        // into the m_endMarkers member of pLastSrcPhrase, and any punctuation following
        // that would be in the m_precPunt member of pSrcPhrase, but if the buffer end has
        // been reached, m_key in pSrcPhrase will be empty. So, providing m_precPunct is
        // empty, pSrcPhrase is not a valid CSourcePhrase instance. We need to check and
        // remove it.
        // But a complication is the possibility of filtered information at the end of the
        // parse buffer - it would be in the m_filteredInfo member. Our solution for this
        // complication is: don't remove the pSrcPhrase here - so if parsing a source text
        // file, the widow CSourcePhrase will just remain at the document end but be
        // unseen, while if we are parsing just-edited source text in OnEditSourceText(),
        // we can leave the widow there after moving endmarkers of it, because
        // OnEditSourceText() will later call
        // TransportWidowedFilteredInfoToFollowingContext() and if there is a following
        // context, the transfer can be done, but if not, we must just leave the source
        // phrase there in the document to carry the filtered information.
        // 
        // A further complication of similar kind is when the user types in non-endmarker
        // information at the end of the string. When editing the source text, this will
        // end up in m_markers but m_key will be empty, and so we need to leave it to
        // TransportWidowedFilteredInfoToFollowingContext() to handle transfer of this
        // information to the following context, or if there is no following context, in
        // this case we abandon the marker info typed because it would make no sense to
        // keep it - that's what to do whether we are at the document end when parsing in a
        // new source text USFM text file, or when editing source text - the
        // gbVerticalEditInProgress global boolean can help in testing for this.
        wxString someFilteredInfo = pSrcPhrase->GetFilteredInfo(); // could be empty
		bool bHasFilteredInfo = !someFilteredInfo.IsEmpty();
		bool bHasNonEndMarkers = !pSrcPhrase->m_markers.IsEmpty();
		if (pSrcPhrase->m_key.IsEmpty() && pSrcPhrase->m_precPunct.IsEmpty())
		{
			if ( (!bHasFilteredInfo && !bHasNonEndMarkers) ||
				(!bHasFilteredInfo && bHasNonEndMarkers && !gbVerticalEditInProgress))
			{
				// remove it if it is not a carrier for filtered information in its
				// m_filteredInfo member (see the more detailed explanation above) nor
				// non-endmarkers information in its m_markers member; OR, it has no
				// filtered information but it does have non-endmarkers in m_markers
				// but vertical edit (ie. we aren't in OnEditSourceText()) is not
				// current (ie. we are creating a document by parsing in a USFM plain
				// text file). The other possibilities can be left for
				// TransportWidowedFilteredInfoToFollowingContext() in
				// OnEditSourceText() to work out, and do deletion of of the carrier if
				// warranted.
				DeleteSingleSrcPhrase(pSrcPhrase, FALSE); // FALSE means 'don't try to
														  // delete a partner pile'
				pSrcPhrase = NULL;
			}
			if (bFreeTranslationIsCurrent)
			{
				// we default to always turning off a free translation section at the end
				// of the document if it hasn't been done already
				if (pLastSrcPhrase != NULL)
				{
					if (pLastSrcPhrase->m_bEndFreeTrans == FALSE)
					{
						pLastSrcPhrase->m_bEndFreeTrans = TRUE;

                        // BEW 11Oct10, and for ~ fixedspace support, set the same flag in
                        // the last child instance
						if (IsFixedSpaceSymbolWithin(pLastSrcPhrase))
						{
							SPList::Node* pos = pLastSrcPhrase->m_pSavedWords->GetLast();
							CSourcePhrase* pWord2 = pos->GetData();
							pWord2->m_bEndFreeTrans = TRUE;
						}
					}
				}
			}
		} // end of TRUE block for test: if (pSrcPhrase->m_key.IsEmpty() && 
		  //                                 pSrcPhrase->m_precPunct.IsEmpty())

		// store the pointer in the SPList (in order of occurrence in text)
		if (pSrcPhrase != NULL)
		{
			pList->Append(pSrcPhrase);
		}

		// make this one be the "last" one for next time through
		pLastSrcPhrase = pSrcPhrase; // note: pSrcPhrase might be NULL

	} // end of while (ptr < pEndText)

	// fix the sequence numbers, so that they are in sequence with no gaps, from the
	// beginning 
	tokBuffer.Empty();
	AdjustSequNumbers(nStartingSequNum,pList);

	// ensure pSrcPhrase->m_srcPhrase has no trailing spaces (a few CR LFs at the end
	// of the plain text input file find their way into the trailing text of the last
	// pSrcPhrase's m_srcPhrase member - so easiest thing to do is just clobber them here
	SPList::Node* pos = pList->GetLast();
	CSourcePhrase* pSrcPhraseVeryLast = pos->GetData();
	pSrcPhraseVeryLast->m_srcPhrase.Trim();

	return pList->GetCount();
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		useSfmSet		-> an enum of type SfmSet: UsfmOnly, PngOnly, or UsfmAndPng
/// \param		pUnkMarkers		<- a wxArrayString that gets populated with unknown (whole) markers,
///									always populated in parallel with pUnkMkrsFlags.
/// \param		pUnkMkrsFlags	<- a wxArrayInt of flags that gets populated with ones or zeros, 
///									always populated in parallel with pUnkMarkers.
/// \param		unkMkrsStr		-> a wxString containing the current unknown (whole) markers within
///									the string - the markers are delimited by spaces following each 
///									whole marker.
/// \param		mkrInitStatus	-> an enum of type SetInitialFilterStatus: setAllUnfiltered,
///									setAllFiltered, useCurrentUnkMkrFilterStatus, or
///									preserveUnkMkrFilterStatusInDoc
/// \remarks
/// Called from: the Doc's OnNewDocument(), CFilterPageCommon::AddUnknownMarkersToDocArrays()
/// and CFilterPagePrefs::OnOK().
/// Scans all the doc's source phrase m_markers and m_filteredInfo members and inventories
/// all the unknown markers used in the current document; it stores all unique markers in
/// pUnkMarkers, stores a flag (1 or 0) indicating the filtering status of the marker in
/// pUnkMkrsFlags, and maintains a string called unkMkrsStr which contains the unknown
/// markers delimited by following spaces.
/// An unknown marker may occur more than once in a given document, but is only stored once
/// in the unknown marker inventory arrays and string.
/// The SetInitialFilterStatus enum values can be used as follows:
///	  The setAllUnfiltered enum would gather the unknown markers into m_unknownMarkers 
///      and set them all to unfiltered state in m_filterFlagsUnkMkrs (currently
///      unused);
///	  The setAllFiltered could be used to gather the unknown markers and set them all to 
///      filtered state (currently unused);
///	  The useCurrentUnkMkrFilterStatus would gather the markers and use any currently 
///      listed filter state for unknown markers it already knows about (by inspecting 
///     m_filterFlagsUnkMkrs), but process any other "new" unknown markers as unfiltered.
///   The preserveUnkMkrFilterStatusInDoc causes GetUnknownMarkersFromDoc to preserve 
///     the filter state of an unknown marker in the Doc, i.e., set m_filterFlagsUnkMkrs 
///     to TRUE if the unknown marker in the Doc was within \~FILTER ... \~FILTER* brackets, 
///     otherwise sets the flag in the array to FALSE.
/// BEW 24Mar10 updated for support of doc version 5 (some changes were needed)
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::GetUnknownMarkersFromDoc(enum SfmSet useSfmSet,
											wxArrayString* pUnkMarkers,
											wxArrayInt* pUnkMkrsFlags,
											wxString & unkMkrsStr,
											enum SetInitialFilterStatus mkrInitStatus)
{
	CAdapt_ItDoc* pDoc = gpApp->GetDocument();
	SPList* pList = gpApp->m_pSourcePhrases;
	wxArrayString MarkerList; // gets filled with all the currently used markers including
							// filtered ones
	wxArrayString* pMarkerList = &MarkerList;

    // save the previous state of m_unknownMarkers and m_filterFlagsUnkMkrs to be able to
    // restore any previously set filter settings for the unknown markers, i.e., when the
    // useCurrentUnkMkrFilterStatus enum parameter is passed-in.
	wxArrayString saveUnknownMarkers;
    // wxArrayString does not have a ::Copy method like MFC's CStringArray::Copy, so we'll
    // do it by brute force CStringArray::Copy removes any existing items in the
    // saveUnknownMarkers array before copying all items from the m_unknownMarkers array
    // into it.
	saveUnknownMarkers.Clear();// start with an empty array
	int act;
	for (act = 0; act < (int)gpApp->m_unknownMarkers.GetCount(); act++)
	{
        // copy all items from m_unknownMarkers into saveUnknownMarkers note: do NOT use
        // subscript notation to avoid assert; i.e., do not use saveUnknownMarkers[act] =
        // gpApp->m_unknownMarkers[act]; instead use form below
		saveUnknownMarkers.Add(gpApp->m_unknownMarkers.Item(act));
	}
	wxArrayInt saveFilterFlagsUnkMkrs;
	// again copy by brute force elements from m_filterFlagsUnkMkrs to
	// saveFilterFlagsUnkMkrs 
	saveFilterFlagsUnkMkrs.Empty();
	for (act = 0; act < (int)gpApp->m_filterFlagsUnkMkrs.GetCount(); act++)
	{
		// copy all items from m_unknownMarkers into saveUnknownMarkers
		saveFilterFlagsUnkMkrs.Add(gpApp->m_filterFlagsUnkMkrs.Item(act));
	}

	// start with empty data
	pUnkMarkers->Empty();
	pUnkMkrsFlags->Empty();
	unkMkrsStr.Empty();
	wxString EqZero = _T("=0 "); // followed by space for parsing efficiency
	wxString EqOne = _T("=1 "); // " " "

	USFMAnalysis* pSfm;
	wxString key;

	MapSfmToUSFMAnalysisStruct* pSfmMap;
	pSfmMap = gpApp->GetCurSfmMap(useSfmSet);

	// Gather markers from all source phrase m_markers strings
	MapSfmToUSFMAnalysisStruct::iterator iter;
	SPList::Node* posn;
	posn = pList->GetFirst();
	CSourcePhrase* pSrcPhrase;
	while (posn != 0)
	{
		// process the markers in each source phrase m_markers string individually
		pSrcPhrase = (CSourcePhrase*)posn->GetData();
		posn = posn->GetNext();
		wxASSERT(pSrcPhrase);
		if (!pSrcPhrase->m_markers.IsEmpty() || !pSrcPhrase->GetFilteredInfo().IsEmpty())
		{
			// m_markers and/or m_filteredInfo for this source phrase has content to examine
			pMarkerList->Empty(); // start with an empty marker list

            // The GetMarkersAndTextFromString function below fills the CStringList
            // pMarkerList with all the markers and their associated texts, one per list
            // item. Each item will include end markers for those that have them. Also,
            // Filtered material enclosed within \~FILTER...\~FILTER* brackets will also be
            // listed as a single item (even though there may be other markers embedded
            // within the filtering brackets.
			GetMarkersAndTextFromString(pMarkerList, pSrcPhrase->m_markers + pSrcPhrase->GetFilteredInfo());

            // Now iterate through the strings in pMarkerList, check if the markers they
            // contain are known or unknown.
			wxString resultStr;
			resultStr.Empty();
			wxString wholeMarker, bareMarker;
			bool markerIsFiltered;
			int mlct;
			for (mlct = 0; mlct < (int)pMarkerList->GetCount(); mlct++) 
			{
				// examine this string list item
				resultStr = pMarkerList->Item(mlct);
				wxASSERT(resultStr.Find(gSFescapechar) == 0);
				markerIsFiltered = FALSE;
				if (resultStr.Find(filterMkr) != -1)
				{
					resultStr = pDoc->RemoveAnyFilterBracketsFromString(resultStr);
					markerIsFiltered = TRUE;
				}
				resultStr.Trim(FALSE); // trim left end
				resultStr.Trim(TRUE);  // trim right end
				int strLen = resultStr.Length();
				int posm = 1;
				wholeMarker.Empty();
				// get the whole marker from the string
				while (posm < strLen && resultStr[posm] != _T(' ') && 
						resultStr[posm] != gSFescapechar)
				{
					wholeMarker += resultStr[posm];
					posm++;
				}
				wholeMarker = gSFescapechar + wholeMarker;
				// do not include end markers in this inventory, so remove any final *
				int aPos = wholeMarker.Find(_T('*'));
				if (aPos == (int)wholeMarker.Length() -1)
					wholeMarker.Remove(aPos,1); 

				wxString tempStr = wholeMarker;
				tempStr.Remove(0,1);
				bareMarker = tempStr;
				wholeMarker.Trim(TRUE); // trim right end
				wholeMarker.Trim(FALSE); // trim left end
				bareMarker.Trim(TRUE); // trim right end
				bareMarker.Trim(FALSE); // trim left end
				wxASSERT(wholeMarker.Length() > 0);
                // Note: The commented out wxASSERT above can trip if the input text had an
                // incomplete end marker \* instead of \f* for instance, or just an
                // isolated backslash marker by itself \ in the text. Such typos become
                // unknown markers and show in the nav text line as ?\*? etc.

				// lookup the bare marker in the active USFMAnalysis struct map
                // whm ammended 11Jul05 Here we want to use the LookupSFM() routine which
                // treats all \bt... initial back-translation markers as known markers all
                // under the \bt marker with its description "Back-translation"
				pSfm = LookupSFM(bareMarker); // use LookupSFM which properly handles 
											  // \bt... forms as \bt
				bool bFound = pSfm != NULL;
				if (!bFound)
				{
                    // it's an unknown marker, so process it as such only add marker to
                    // m_unknownMarkers if it doesn't already exist there
					int newArrayIndex = -1;
					if (!MarkerExistsInArrayString(pUnkMarkers, wholeMarker, 
											newArrayIndex)) // 2nd param not used here
					{
						bool bFound = FALSE;
						// set the filter flag to unfiltered for all unknown markers
						pUnkMarkers->Add(wholeMarker);
						if (mkrInitStatus == setAllUnfiltered) // unused condition
						{
							pUnkMkrsFlags->Add(FALSE);
						}
						else if (mkrInitStatus == setAllFiltered) // unused condition
						{
							pUnkMkrsFlags->Add(TRUE);
						}
						else if (mkrInitStatus == preserveUnkMkrFilterStatusInDoc)
						{
                            // whm added 27Jun05. After any doc rebuild is finished, we
                            // need to insure that the unknown marker arrays and
                            // m_currentUnknownMarkerStr are up to date from what is now
                            // the situation in the Doc.
                            // Use preserveUnkMkrFilterStatusInDoc to cause
                            // GetUnknownMarkersFromDoc to preserve the filter state of an
                            // unknown marker in the Doc, i.e., set m_filterFlagsUnkMkrs to
                            // TRUE if the unknown marker in the Doc was within \~FILTER
                            // ... \~FILTER* brackets, otherwise the flag is FALSE.
							pUnkMkrsFlags->Add(markerIsFiltered);
						}
						else // mkrInitStatus == useCurrentUnkMkrFilterStatus
						{
                            // look through saved passed-in arrays and try to make the
                            // filter status returned for any unknown markers now in the
                            // Doc conform to the filter status in any corresponding saved
                            // passed-in arrays.
							int mIndex;
							for (mIndex = 0; mIndex < (int)saveUnknownMarkers.GetCount(); mIndex++)
							{
								if (saveUnknownMarkers.Item(mIndex) == wholeMarker)
								{
                                    // the new unknown marker is same as was in the saved
                                    // marker list so make the new unknown marker use the
                                    // same filter status as the saved one had
									bFound = TRUE;
									int oldFlag = saveFilterFlagsUnkMkrs.Item(mIndex);
									pUnkMkrsFlags->Add(oldFlag);
									break;
								}
							}
							if (!bFound)
							{
								// new unknown markers should always start being unfiltered
								pUnkMkrsFlags->Add(FALSE);
							}
						}
						unkMkrsStr += wholeMarker; // add it to the unknown markers string
						if (pUnkMkrsFlags->Item(pUnkMkrsFlags->GetCount()-1) == FALSE)
						{
							unkMkrsStr += EqZero; // add "=0 " unfiltered unknown marker
						}
						else
						{
							unkMkrsStr += EqOne; // add "=1 " filtered unknown marker
						}
					}
				}// end of if (!bFound)
			}// end of while (posMkrList != NULL)
		}// end of if (!pSrcPhrase->m_markers.IsEmpty())
	}// end of while (posn != 0)
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a wxString containing a list of whole unknown markers; each 
///             marker (xx) formatted as "\xx=0 " or "\xx=1 " with following space within
///             the string.
/// \param		pUnkMarkers*	-> pointer to a wxArrayString of whole unknown markers
/// \param		pUnkMkrsFlags*	-> pointer to a wxArrayInt of int flags indicating 
///                                whether the unknown marker is filtered (1) 
///                                or unfiltered (0).
/// \remarks
/// Called from: Currently GetUnknownMarkerStrFromArrays() is only called from debug trace
/// blocks of code and only when the _Trace_UnknownMarkers define is activated.
/// Composes a string of unknown markers suffixed with a zero flag and following space ("=0
/// ") if the filter status of the unknown marker is unfiltered; or with a one flag and
/// followoing space ("=1 ") if the filter status of the unknown marker is filtered. The
/// function also verifies the integrity of the arrays, i.e., that they are consistent in
/// length - required for them to operate in parallel.
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::GetUnknownMarkerStrFromArrays(wxArrayString* pUnkMarkers, 
													 wxArrayInt* pUnkMkrsFlags)
{
	int ctMkrs = pUnkMarkers->GetCount();
	// verify that our arrays are parallel
	pUnkMkrsFlags = pUnkMkrsFlags; // to avoid compiler warning
	wxASSERT (ctMkrs == (int)pUnkMkrsFlags->GetCount());
	wxString tempStr, mkrStr;
	tempStr.Empty();
	for (int ct = 0; ct < ctMkrs; ct++)
	{
		mkrStr = pUnkMarkers->Item(ct);
		mkrStr.Trim(FALSE); // trim left end
		mkrStr.Trim(TRUE); // trim right end
		mkrStr += _T("="); // add '='
		mkrStr << pUnkMkrsFlags->Item(ct); // add a 1 or 0 flag formatted as string
		mkrStr += _T(' '); // insure a single final space
		tempStr += mkrStr;
	}
	return tempStr;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		pNewSrcPhrasesList			-> a list of pointers to CSourcePhrase instances
/// \param		nNewCount					-> how many entries are in the pNewSrcPhrasesList
///												list (currently unused)
/// \param		propagationType				<- the TextType value for the last CSourcePhrase 
///												instance in the list
/// \param		bTypePropagationRequired	<- TRUE if the function determines that the caller 
///												must take the returned propagationType value 
///												and propagate it forwards until a CSourcePhrase 
///												instance with its m_bFirstOfType member set TRUE 
///												is encountered, otherwise FALSE (no propagation 
///												needed)
/// \remarks
/// Called from: the Doc's RetokenizeText(), the View's ReconcileLists() and OnEditSourceText(),
/// and also from XML's MurderTheDocV4Orphans()
/// There are two uses for this function:
///   (1) To do navigation text, special text colouring, and TextType value cleanup 
///         after the user has edited the source text - which potentially allows the user
///         to edit, add or remove markers and/or change their location. Editing of markers
///         potentially might make a typo marker into one currently designated as to be
///         filtered, so this is checked for and if it obtains, then the requisite
///         filtering is done at the end as an extra (automatic) step.
///   (2) To do the same type of cleanup job, but over the whole document from start to 
///         end, after the user has changed the SFM set (which may also involve changing
///         filtering settings in the newly chosen SFM set, or it may not) - when used in
///         this way, all filtering changes will already have been done by custom code for
///         that operation, so DoMarkerHousekeeping() only needs to do the final cleanup of
///         the navigation text and text colouring and (cryptic) TextType assignments.
/// NOTE: m_FilterStatusMap.Clear(); is done early in DoMarkerHousekeeping(), so the prior
///         contents of the former will be lost.
/// BEW 24Mar10, updated for support of doc version 5(changes needed - just a block of code
/// removed)
/// BEW 9July10, no changes needed for support of kbVersion 2
/// BEW 11Oct10 (actually 6Jan11) added code for closing off a TextType span because of
/// endmarker content within m_endMarkers - I missed doing this in the earlier tweaks
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::DoMarkerHousekeeping(SPList* pNewSrcPhrasesList,int WXUNUSED(nNewCount), 
							TextType& propagationType, bool& bTypePropagationRequired)
{
    // The following comments were in the legacy versions, when this function was only used
    // after the source text was edited...
    // Typically, when this function is entered, the TextType may need adjusting or
    // setting, chapter and verse numbers and associated strings may need adjusting,
    // certain flags may need setting or clearing. This ensures all the attributes in each
    // sourcephrase instance are mutually consistent with the standard format markers
    // resulting from the user's editing of the source text and subsequent marker
    // editing/transfer operations.
	// The following indented comments only apply to the pre-3.7.0 versions:
        // Note: gpFollSrcPhrase may need to be accessed; but because this function is
        // called before unwanted sourcephrase instances are removed from the main list in
        // the case when the new sublist is shorter than the modified selected instances
        // sublist, then there would be one or more sourcephrase instances between the end
        // of the new sublist and gpFollSrcPhrase. If TextType propagation is required
        // after the sublist is copied to the main list and any unwanted sourcephrase
        // instances removed, then the last 2 parameters enable the caller to know the fact
        // and act accordingly
    // For the refactored source text edit functionality of 3.7.0, the inserting of new
    // instances is done after the old user's selection span's instances have been removed,
    // so there are no intervening unwanted CSourcePhrase instances. Propagation still may
    // be necessary, so we still return the 2 parameters to the caller for it to do any
    // such propagating. The function cannot be called, however, if the passed in list is
    // empty - it is therefore the caller's job to detect this and refrain

	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	
	SPList* pL = pNewSrcPhrasesList;
	wxASSERT(pL);
	CSourcePhrase* pLastSrcPhrase = gpPrecSrcPhrase; // the one immediately preceding sublist 
													 // (it could be null)
	CSourcePhrase* pFollowing = gpFollSrcPhrase; // first, following the sublist (could be null)
	CSourcePhrase* pSrcPhrase = 0; // the current one, used in our iterations

	gpApp->m_FilterStatusMap.clear(); 
            // empty the map, we want to use it to handle filtering of an edited marker
            // when the editing changes it to be a marker designated as one which is to be
            // filtered; we repopulate it just before the AnalyseMarker() call below

    // we'll use code and functions used for parsing source text, so we need to set up some
    // buffers so we can simulate the data structures pertinent to those function calls we
    // have to propagate the preceding m_bSpecialText value, until a marker changes it; if
    // there is no preceding context, then we can assume it is FALSE (if a \id follows,
    // then it gets reset TRUE later on)
	bool bSpecialText = FALSE;
	if (gpPrecSrcPhrase != 0)
		bSpecialText = gpPrecSrcPhrase->m_bSpecialText;

	// set up some local variables
	wxString mkrBuffer; // where we will copy standard format marker strings to, for parsing
	int itemLen = 0;
	int strLen = ClearBuffer(); // clear's the class's buffer[256] buffer
	bool bHitMarker;

    // BEW added 01Oct06; if the sublist (ie. pNewSrcPhrasesList) is empty (because the
    // user deleted the selected source text, then we can't get a TextType value for the
    // end of the sublist contents; so we get it instead from the gpFollSrcPhrase global,
    // which will have been set in the caller already (if at the end of the doc we'll
    // default the value to verse so that the source text edit does not fail)
	TextType finalType; // set this to the TextType for the last sourcephrase instance 
                        // in the sublist -- but take note of the above comment sublist

    // whm Note: if the first position node of pL is NULL finalType will not have been
    // initialized (the while loop never entered) and a bogus value will get assigned to
    // propagationType after the while loop. It may never happen that pos == NULL, but to
    // be sure I'm initializing finalType to noType
	finalType = noType;

	USFMAnalysis* pUsfmAnalysis = NULL; // whm added 11Feb05

	SPList::Node* pos = pL->GetFirst();
	bool bInvalidLast = FALSE;
	if (pLastSrcPhrase == NULL) // MFC had == 0
		bInvalidLast = TRUE; // only possible if user edited source text at the very 
                             // first sourcephrase in the doc iterate over each
                             // sourcephrase instance in the sublist
    bool bStartDefaultTextTypeOnNextIteration = FALSE;
	bool bSkipPropagation = FALSE;
	while (pos != 0) // pos will be NULL if the pL list is empty
	{
		pSrcPhrase = (CSourcePhrase*)pos->GetData();
		pos = pos->GetNext();
		wxASSERT(pSrcPhrase);
		pSrcPhrase->m_inform.Empty(); // because AnalyseMarker() does +=, not =, 
									  // so we must clear its contents first

		// BEW 11Oct10 (actually 6Jan11) endmarkers in docV5 are no longer stored at the
		// start of the next CSourcePhrase's m_markers member, but on the current
		// CSourcePhrase which ends the actual TextType span. Therefore, interating
		// through the list we'll come to an m_endMarkers which ends a span such as a
		// footnote, endnore or cross reference before we come to the CSourcePhrase
		// instance which follows and for which m_bFirstOfType for the new TextType value
		// will be TRUE. So check here for non-empty m_endMarkers member (other storage of
		// markers, the inline types, doesn't affect TextType, so we can ignore those and
		// only need examine m_endMarkers), and if it contains \f* or \x* or \fe* for
		// USFM 2.x marker set, or \fe or \F for PNG 1998 marker set, then we must end the
		// type on the current pSrcPhrase (ie. it is the last with the current TextType
		// value) and tell the code that the next iteration of the loop must commence a
		// new TextType - which we'll default to 'verse' type, and m_bSpecialText = FALSE
		// -- the next iteration can then change to some other type if the next
		// iteration's pSrcPhrase->m_markers contains a final marker which changes the
		// TextType - the code for doing that is already in place below. How we'll tell
		// the code what to do on the next iteration will be done with a new local
		// boolean, bStartDefaultTextTypeOnNextIteration -- which we'll set TRUE if one of
		// the endmarkers of either set is found in m_endMarkers of the current instance;
		// We'll also need a bSkipPropagation flag, default FALSE, but when set TRUE, it causes
		// a skip of the code further down which propagates from the last CSourcePhrase
		// instance when m_markers is empty -- which is typically the case after an
		// endnote, footnote or cross reference
		bSkipPropagation = FALSE; // default value
		if (bStartDefaultTextTypeOnNextIteration)
		{
			// a switch to default TextType is requested - do it here, then let the code
			// below override the value we set here if m_markers has a last marker which
			// requires a different TextType value
			pSrcPhrase->m_curTextType = verse;
			pSrcPhrase->m_bSpecialText = FALSE;
			pSrcPhrase->m_bFirstOfType = TRUE;
			// m_inform has be cleared to default empty above, code further below may put
			// something in it (if m_markers is non-empty)
			bStartDefaultTextTypeOnNextIteration = FALSE; // restore default bool value
			bSkipPropagation = TRUE; // used about 70 lines below, to skip a code block
									 // for propagating from the previous instance, which
									 // is not wanted when we have already closed of a
									 // TextType span on the previous iteration (see the
									 // code block immediately below)
		}
		if (!pSrcPhrase->GetEndMarkers().IsEmpty())
		{
			wxArrayString array;
			array.Clear();
			pSrcPhrase->GetEndMarkersAsArray(&array);
			size_t count = array.GetCount();
			if (count > 0)
			{
				wxString wholeEndMkr; wholeEndMkr.Empty();
				wholeEndMkr = array.Item(count - 1); // get the last one, if there is more than one
									// for instance, it may have \ft*\f* in the old way of
									// doing USFM
				wxASSERT(!wholeEndMkr.IsEmpty());
				if (gpApp->gCurrentSfmSet == UsfmOnly || gpApp->gCurrentSfmSet == UsfmAndPng)
				{
					if (wholeEndMkr == _T("\\f*") || wholeEndMkr == _T("\\fe*") ||
						wholeEndMkr == _T("\\x*"))
					{
						// it's the end of either a footnote, endnote or cross reference
						bStartDefaultTextTypeOnNextIteration = TRUE;
					}
				}
				else
				{
					// must be PngOnly
					if (wholeEndMkr == _T("\\fe") || wholeEndMkr == _T("\\F"))
					{
                        // it's the end of a footnote (either the \fe marker or the \F
                        // marker can end a footnote in the legacy marker set)
						bStartDefaultTextTypeOnNextIteration = TRUE;
					}
				}
			}
		}

		// get any marker text into mkrBuffer
		mkrBuffer = pSrcPhrase->m_markers;
		int lengthMkrs = mkrBuffer.Length();
        // wx version note: Since we require a read-only buffer we use GetData which just
        // returns a const wxChar* to the data in the string.
		const wxChar* pBuffer = mkrBuffer.GetData();
		wxChar* pEndMkrBuff; // set this dynamically, for each 
							 // source phrase's marker string
		wxString temp; // can build a string here
		wxChar* ptr = (wxChar*)pBuffer;
		wxChar* pBufStart = (wxChar*)pBuffer;
		pEndMkrBuff = pBufStart + lengthMkrs; // point to null at end
		wxASSERT(*pEndMkrBuff == _T('\0')); // whm added for wx version - needs to be 
											// set explicitly when mkrBuffer is empty
		if (mkrBuffer.IsEmpty())
		{
            // there is no marker string on this sourcephrase instance, so if we are at the
            // beginning of the document, m_bFirstOfType will be TRUE, otherwise, there
            // will be a preceding sourcephrase instance and m_bFirstOfType will be FALSE,
            // and we can just copy it's value
			if (bInvalidLast)
			{
				pSrcPhrase->m_bFirstOfType = TRUE;
				pSrcPhrase->m_curTextType = verse; // this is the only possibility, 
												   // at start of doc & no marker
				bInvalidLast = FALSE; // all subsequent sourcephrases will have a valid 
									  // preceding one
				pSrcPhrase->m_inform.Empty();
				pSrcPhrase->m_chapterVerse.Empty();
				pSrcPhrase->m_bSpecialText = bSpecialText; // can not be special text here
			}
			else
			{
				if (!bSkipPropagation)
				{
					// if no skipping of this propagation code is wanted, then do the
					// propagation from the earlier instance; when bSkipPropagation is
					// TRUE, we skip because in code above we've closed of a TextType span
					// already and so this pSrcPhrase is the first of a new TextType span,
					// which defaults to verse and not special text in the absence of a
					// marker to specify otherwise 
					pSrcPhrase->m_bFirstOfType = FALSE;
					pSrcPhrase->m_curTextType = pLastSrcPhrase->m_curTextType; // propagate the 
																	// earlier instance's type
					pSrcPhrase->m_inform.Empty();
					pSrcPhrase->m_chapterVerse.Empty();
					// propagate the previous value
					pSrcPhrase->m_bSpecialText = pLastSrcPhrase->m_bSpecialText; 
				}
			}
		}
		else
		{
			// there is a marker string on this sourcephrase instance
			if (bInvalidLast)
			{
				// we are at the very beginning of the document
				pLastSrcPhrase = 0; // ensure its null
				bInvalidLast = FALSE; // all subsequent sourcephrases will have a valid 
									  // preceding one
				pSrcPhrase->m_bSpecialText = bSpecialText; // assume this value, the marker 
														   // may later change it 
				goto x; // code at x comes from TokenizeText, and should not break for 
						// pLast == 0
			}
			else
			{
				// we are not at the beginning of the document
				pSrcPhrase->m_bSpecialText = pLastSrcPhrase->m_bSpecialText; // propagate 
																	// previous, as default
x:				while (ptr < pEndMkrBuff)
				{
					bHitMarker = FALSE;

					if (IsWhiteSpace(ptr))
					{
						itemLen = ParseWhiteSpace(ptr);
						ptr += itemLen; // advance pointer past the white space
					}

					// are we at the end of the markers text string?
					if (IsEnd(ptr) || ptr >= pEndMkrBuff)
					{
						break;
					}

					// are we pointing at a standard format marker?
b:					if (IsMarker(ptr)) // pBuffer added for v1.4.1 
									   // contextual sfms
					{
						bHitMarker = TRUE;
						// its a marker of some kind
						int nMkrLen = 0;
						if (IsVerseMarker(ptr,nMkrLen))
						{
							if (nMkrLen == 2)
							{
								// its a verse marker
								pApp->buffer += gSFescapechar;
								pApp->buffer += _T("v");
								ptr += 2; // point past the \v marker
							}
							else
							{
								// its an Indonesia branch verse marker \vn
								pApp->buffer += gSFescapechar;
								pApp->buffer += _T("vn");
								ptr += 3; // point past the \vn marker
							}

							itemLen = ParseWhiteSpace(ptr);
							AppendItem(pApp->buffer,temp,ptr,itemLen); // add white 
																	   // space to buffer
							ptr += itemLen; // point at verse number

							itemLen = ParseNumber(ptr);
							AppendItem(pApp->buffer,temp,ptr,itemLen); // add number (or range 
																	   // eg. 3-5) to buffer
							if (pApp->m_curChapter.GetChar(0) == '0')
								pApp->m_curChapter.Empty(); // caller will have set it non-zero 
															// if there are chapters
							pSrcPhrase->m_chapterVerse = pApp->m_curChapter; // set to n: form
							pSrcPhrase->m_chapterVerse += temp; // append the verse number
							pSrcPhrase->m_bVerse = TRUE; // set the flag to signal start of a 
														 // new verse
							ptr += itemLen; // point past verse number

							// set pSrcPhrase attributes
							pSrcPhrase->m_bVerse = TRUE;
							if (pSrcPhrase->m_curTextType != poetry) // poetry sfm comes before \v
							{
								// if its already poetry, don't change it
								pSrcPhrase->m_curTextType = verse;
							}
							pSrcPhrase->m_bSpecialText = FALSE;

							itemLen = ParseWhiteSpace(ptr); // past white space which is 
															// after the marker
							AppendItem(pApp->buffer,temp,ptr,itemLen); // add it to the buffer
							ptr += itemLen; // point past the white space

							goto b; // check if another marker follows:
						}
						else
						{
							// some other kind of marker - perhaps its a chapter marker?
							if (IsChapterMarker(ptr))
							{
								// its a chapter marker
								pApp->buffer += gSFescapechar;
								pApp->buffer += _T("c");
								ptr += 2; // point past the \c marker

								itemLen = ParseWhiteSpace(ptr);
								AppendItem(pApp->buffer,temp,ptr,itemLen); // add white space
																		   // to buffer
								ptr += itemLen; // point at chapter number
								itemLen = ParseNumber(ptr);
								AppendItem(pApp->buffer,temp,ptr,itemLen); // add chapter number
                                                                           // to buffer
								pApp->m_curChapter = temp;
								pApp->m_curChapter += _T(':'); // get it ready to 
															   // append verse numbers
								ptr += itemLen; // point past chapter number

								// set pSrcPhrase attributes
								pSrcPhrase->m_bChapter = TRUE;
								pSrcPhrase->m_bVerse = TRUE; // always have verses following a 
															 // chapter
								if (pSrcPhrase->m_curTextType != poetry) // poetry sfm comes before \v
								{
									pSrcPhrase->m_curTextType = verse;
								}
								pSrcPhrase->m_bSpecialText = FALSE;

								itemLen = ParseWhiteSpace(ptr); // parse white space following 
																// the number
								AppendItem(pApp->buffer,temp,ptr,itemLen); // add it to buffer
								ptr += itemLen; // point past it

								goto b; // check if another marker follows
							}
							else
							{
                                // neither verse nor chapter, so we don't have to worry
                                // about a following number, so just append the marker to
                                // the buffer string

								pUsfmAnalysis = LookupSFM(ptr); // NULL if unknown marker

								itemLen = ParseMarker(ptr);
								AppendItem(pApp->buffer,temp,ptr,itemLen);

                                // we wish to know if this marker, which is not within a
                                // span bracketed by \~FILTER followed by \~FILTER*, has
                                // potentially been edited so that it really needs to be
                                // filtered, along with its following text content, rather
                                // than left unfiltered and its content visible for
                                // adapting in the doc. If it should be filtered, we will
                                // put an entry into m_FilterStatusMap to that effect, and
                                // the caller will later use the fact that that map is not
                                // empty to call RetokenizeText() with the option for
                                // filter changes turned on (ie. BOOL parameter 2 in the
                                // call is TRUE), and that will accomplish the required
                                // filtering.
								wxString mkr(ptr,itemLen); // construct the wholeMarker
								wxString mkrPlusSpace = mkr + _T(' '); // add the trailing space
								int curPos = gpApp->gCurrentFilterMarkers.Find(mkrPlusSpace);
								if (curPos >= 0)
								{
									// its a marker, currently unfiltered, which should be
									// filtered 
									wxString valStr;
									if (gpApp->m_FilterStatusMap.find(mkr) == gpApp->m_FilterStatusMap.end())
									{
                                        // marker does not already exist in
                                        // m_FilterStatusMap so add it as an entry meaning
                                        // 'now to be filtered' (a 1 value)
										(gpApp->m_FilterStatusMap)[mkr] = _T("1");
									}
								}

								// set default pSrcPhrase attributes
								if (pSrcPhrase->m_curTextType != poetry)
									pSrcPhrase->m_curTextType = verse; // assume verse unless 
																	 // AnalyseMarker changes it

								// analyse the marker and set fields accordingly
								pSrcPhrase->m_bSpecialText = AnalyseMarker(pSrcPhrase,pLastSrcPhrase,
															(wxChar*)ptr,itemLen,pUsfmAnalysis);

								// advance pointer past the marker
								ptr += itemLen;

								itemLen = ParseWhiteSpace(ptr); // parse white space after it
								AppendItem(pApp->buffer,temp,ptr,itemLen); // add it to buffer
								ptr += itemLen; // point past it
								goto b; // check if another marker follows
							}
						}
					}
					else
					{
						// get ready for next iteration
						strLen = ClearBuffer(); // empty the small working buffer
						itemLen = 0;
						ptr++;	// whm added. The legacy did not increment ptr here.
                                // The legacy app never reached this else block, because,
                                // if it had, it would enter an endless loop. The version 3
                                // app can have filtered text and can potentially reach
                                // this else block, so we must insure that we avoid an
                                // endless loop by incrementing ptr here.
					}
				}
			}
		}

		// make this one be the "last" one for next time through
		pLastSrcPhrase = pSrcPhrase;
		finalType = pSrcPhrase->m_curTextType; // keep this updated continuously, to be used 
											   // below
		gbSpecialText = pSrcPhrase->m_bSpecialText; // the value to be propagated at end of 
													// OnEditSourceText()
	} // end of while (pos != 0) loop

    // BEW added 01Oct06; handle an empty list situation (the above loop won't have been
    // entered so finalType won't yet be set
	if (pL->IsEmpty())
	{
		finalType = verse; // the most likely value, so the best default if the code 
						   // below doesn't set it
		if (gpFollSrcPhrase != NULL)
		{
            // using the following CSourcePhrase's TextType value is a sneaky way to ensure
            // we don't get any propagation done when the sublist was empty; as we don't
            // expect deleting source text to bring about the need for any propagation
            // since parameters should be correct already
			finalType = gpFollSrcPhrase->m_curTextType;
		}
        // BEW added 19Jun08; we need to also give a default value for gbSpecialText in
        // this case to, because prior to this change it was set only within the loop and
        // not here, and leaving it unset here would result in who knows what being
        // propagated, it could have been TRUE or FALSE when this function was called
		gbSpecialText = FALSE; // assume we want 'inspired text' colouring
	}

    // at the end of the (sub)list, we may have a different TextType than for the
    // sourcephrase which follows, if so, we will need to propagate the type if a standard
    // format marker does not follow, provided we are not at the document end, until either
    // we reach the doc end, or till we reach an instance with m_bFirstOfType set TRUE; but
    // nothing need be done if the types are the same already. We also have to propagate
    // the m_bSpecialText value, by the same rules.... if we have been cleaning up after an
    // SFM set change, which is done over the whole document (ie. m_pSourcePhrases list is
    // the first parameter), then pFollowing will have been set null in the caller, and no
    // propagation would be required
	bTypePropagationRequired = FALSE;
	propagationType = finalType;
	if (pFollowing == NULL) // MFC had == 0
		return;		// we are at the end of the document, so no propagation is needed
	if (pFollowing->m_curTextType == finalType)
		return;		// types are identical, so no propagation is needed
	if (pFollowing->m_bFirstOfType)
		return; // type changes here obligatorily (probably due to a marker), so 
                // we cannot propagate

	// if we get here, then propagation is required - so return that fact to the caller
	bTypePropagationRequired = TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if no errors; FALSE if an error occurred
/// \param		exportPathUsed		<- a wxString to return the path/name of the packed
///                                 document to the caller
/// \param      bInvokeFileDialog -> TRUE (default) presents the wxFileDialog; FALSE 
///                                 uses the <current doc name>.aip
/// \remarks
/// Called from: the Doc's OnFilePackDocument() and EmailReportDlg::OnBtnAttachPackedDoc.
/// Assembles the raw contents that go into an Adapt It Packed Document into the packByteStr
/// which is a CBString byte buffer. 
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::DoPackDocument(wxString& exportPathUsed, bool bInvokeFileDialog)
{
	CBString packByteStr; 
	wxString packStr;
	packStr.Empty();

    // first character needs to be a 1 for the regular app doing the pack, or a 2 for the
    // Unicode app (as resulting from sizeof(wxChar) ) and the unpacking app will have to
    // check that it is matching; and if not, warn user that continuing the unpack might
    // not result in properly encoded text in the docment (but allow him to continue,
    // because if source and target text are all ASCII, the either app can read the packed
    // data from the other and give valid encodings in the doc when unpacked.)
	//
    // whm Note: The legacy logic doesn't work cross-platform! The sizeof(char) and
    // sizeof(w_char) is not constant across platforms. On Windows sizeof(char) is 1 and
    // sizeof(w_char) is 2; but on all Unix-based systems (i.e., Linux and Mac OS X) the
    // sizeof(char) is 2 and sizeof(w_char) is 4. We can continue to use '1' to indicate
    // the file was packed by an ANSI version, and '2' to indicate the file was packed by
    // the Unicode app for back compatibility. However, the numbers cannot signify the size
    // of char and w_char across platforms. They can only be used as pure signals for ANSI
    // or Unicode contents of the packed file. Here in OnFilePackDoc we will save the
    // string _T("1") if we're packing from an ANSI app, or the string _T("2") if we're
    // packing from a Unicode app. See DoUnpackDocument() for how we can interpret "1" and
    // "2" in a cross-platform manner.
	//
#ifdef _UNICODE
	packStr = _T("2");
#else
	packStr = _T("1");
#endif

	packStr += _T("|*0*|"); // the zeroth unique delimiter

	// get source and target language names, or whatever is used for these
	wxString curSourceName;
	wxString curTargetName;
	gpApp->GetSrcAndTgtLanguageNamesFromProjectName(gpApp->m_curProjectName, 
											curSourceName, curTargetName);

    // get the book information (mode flag, disable flag, and book index; as ASCII string
    // with colon delimited fields)
	wxString bookInfoStr;
	bookInfoStr.Empty();
	if (gpApp->m_bBookMode)
	{
		bookInfoStr = _T("1:");
	}
	else
	{
		bookInfoStr = _T("0:");
	}
	if (gpApp->m_bDisableBookMode)
	{
		bookInfoStr += _T("1:");
	}
	else
	{
		bookInfoStr += _T("0:");
	}
	if (gpApp->m_nBookIndex != -1)
	{
		bookInfoStr << gpApp->m_nBookIndex;
	}
	else
	{
		bookInfoStr += _T("-1");
	}

	wxLogNull logNo; // avoid spurious messages from the system

	// update and save the project configuration file
	bool bOK = TRUE; // whm initialized, BEW changed to default TRUE 25Nov09
	// BEW added flag to the following test on 25Nov09
	if (!gpApp->m_curProjectPath.IsEmpty() && !gpApp->m_bReadOnlyAccess)
	{
		if (gpApp->m_bUseCustomWorkFolderPath && !gpApp->m_customWorkFolderPath.IsEmpty())
		{
			// whm 10Mar10, must save using what paths are current, but when the custom
			// location has been locked in, the filename lacks "Admin" in it, so that it
			// becomes a "normal" project configuration file in m_curProjectPath at the 
			// custom location.
			if (gpApp->m_bLockedCustomWorkFolderPath)
				bOK = gpApp->WriteConfigurationFile(szProjectConfiguration, gpApp->m_curProjectPath,projectConfigFile);
			else
				bOK = gpApp->WriteConfigurationFile(szAdminProjectConfiguration,gpApp->m_curProjectPath,projectConfigFile);
		}
		else
		{
			bOK = gpApp->WriteConfigurationFile(szProjectConfiguration, gpApp->m_curProjectPath,projectConfigFile);
		}
		// original code below
		//bOK = gpApp->WriteConfigurationFile(szProjectConfiguration,gpApp->m_curProjectPath,projectConfigFile);
	}
	// we don't expect any failure here, so an English message hard coded will do
	if (!bOK)
	{
		wxMessageBox(_T(
		"Writing out the configuration file failed in OnFilePackDoc, command aborted\n"),
		_T(""), wxICON_EXCLAMATION);
		return FALSE;
	}

	// get the size of the configuration file, in bytes
	wxFile f;
	wxString configFile = gpApp->m_curProjectPath + gpApp->PathSeparator + 
									szProjectConfiguration + _T(".aic");
	int nConfigFileSize = 0;
	if (f.Open(configFile,wxFile::read))
	{
		nConfigFileSize = f.Length();
		wxASSERT(nConfigFileSize);
	}
	else
	{
		wxMessageBox(_T(
	"Getting the configuration file's size failed in OnFilePackDoc, command aborted\n"),
		_T(""), wxICON_EXCLAMATION);
		return FALSE;
	}
	f.Close(); // needed because in wx we opened the file

	// save the doc as XML
	bool bSavedOK = TRUE;
	// BEW added test on 25Nov09, so documents can be packed when user has read only access
	// BEW changed 29Apr10, to use DoFileSave_Protected() rather than DoFileSave() because the
	// former gives better protection against data loss in the event of file truncation
	// due to a processing error.
	if (!gpApp->m_bReadOnlyAccess)
	{
		//bSavedOK = DoFileSave(TRUE);
		bSavedOK = DoFileSave_Protected(TRUE); // TRUE - show wait/progress dialog
	}

	// construct the absolute path to the document as it currently is on disk; if the
	// local user has read-only access, the document on disk may not have been recently
	// saved. (Read-only access must not force document saves on a remote user
	// who has ownership of writing permission for data in the project; otherwise, doing
	// so could cause data to be lost)
	wxString docPath;
	if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
	{
		docPath = gpApp->m_bibleBooksFolderPath;
	}
	else
	{
		docPath = gpApp->m_curAdaptionsPath;
	}
	docPath += gpApp->PathSeparator + gpApp->m_curOutputFilename; // it will have .xml extension

	// get the size of the document's XML file, in bytes
	int nDocFileSize = 0;
	if (f.Open(docPath,wxFile::read))
	{
		nDocFileSize = f.Length();
		wxASSERT(nDocFileSize);
	}
	else
	{
		wxMessageBox(_T(
	"Getting the document file's size failed in OnFilePackDoc, command aborted\n"),
		_T(""), wxICON_EXCLAMATION);
		return FALSE;
	}
	f.Close(); // needed for wx version which opened the file to determine its size

	// construct the composed information required for the pack operation, as a wxString
	packStr += curSourceName;
	packStr += _T("|*1*|"); // the first unique delimiter
	packStr += curTargetName;
	packStr += _T("|*2*|"); // the second unique delimiter
	packStr += bookInfoStr;
	packStr += _T("|*3*|"); // the third unique delimiter
	packStr += gpApp->m_curOutputFilename;
	packStr += _T("|*4*|"); // the fourth unique delimiter

    // set up the byte string for the data, taking account of whether we have unicode data
    // or not
#ifdef _UNICODE
	packByteStr = gpApp->Convert16to8(packStr);
#else
	packByteStr = packStr.c_str();
#endif

	// from here on we work with bytes, and so use CBString rather than wxString for the data

	if (!f.Open(configFile,wxFile::read))
	{
		// if error, just return after telling the user about it -- English will do, 
		// it shouldn't happen
		wxString s;
		s = s.Format(_T(
"Could not open a file stream for project config, in OnFilePackDoc(), for file %s"),
		gpApp->m_curProjectPath.c_str());
		wxMessageBox(s,_T(""), wxICON_EXCLAMATION);
		return FALSE; 
	}
	int nFileLength = nConfigFileSize; // our files won't require more than 
									   // an int for the length

    // create a buffer large enough to receive the whole lot, allow for final null byte (we
    // don't do anything with the data except copy it and resave it, so a char buffer will
    // do fine for unicode too), then fill it
	char* pBuff = new char[nFileLength + 1];
	memset(pBuff,0,nFileLength + 1);
	int nReadBytes = f.Read(pBuff,nFileLength);
	if (nReadBytes < nFileLength)
	{
		wxMessageBox(_T(
		"Project file read was short, some data missed so abort the command\n"),
		_T(""), wxICON_EXCLAMATION);
		return FALSE; 
	}
	f.Close(); // assume no errors

	// append the configuration file's data to packStr and add the next 
	// unique delimiter string
	packByteStr += pBuff;
	packByteStr += "|*5*|"; // the fifth unique delimiter

	// clear the buffer, then read in the document file in similar fashion & 
	// delete the buffer when done
	delete[] pBuff;
	if (!f.Open(docPath,wxFile::read))
	{
		// if error, just return after telling the user about it -- English will do, 
		// it shouldn't happen
		wxString s;
		s = s.Format(_T(
"Could not open a file stream for the XML document as text, in OnFilePackDoc(), for file %s"),
		docPath.c_str());
		wxMessageBox(s,_T(""), wxICON_EXCLAMATION);
		return FALSE; 
	}
	nFileLength = nDocFileSize; // our files won't require more than an int for the length
	pBuff = new char[nFileLength + 1];	
	memset(pBuff,0,nFileLength + 1);
	nReadBytes = f.Read(pBuff,nFileLength);
	if (nReadBytes < nFileLength)
	{
		wxMessageBox(_T(
		"Document file read was short, some data missed so abort the command\n"),
		_T(""), wxICON_EXCLAMATION);
		return FALSE; 
	}
	f.Close(); // assume no errors
	packByteStr += pBuff;
	delete[] pBuff;

	// whm Pack design notes for future consideration:
	// 1. Initial design calls for the packing/compression of a single Adapt It document
	//    at a time. With the freeware zip utils provided by Lucian Eischik (based on zlib
	//    and info-zip) it would be relatively easy in the future to have the capability of
	//    packing multiple files into the .aip zip archive.
	// 2. Packing/zipping can be accomplished by doing it on external files (as done below)
	//    or by doing it in internal buffers (in memory).
	// 3. If in future we want to do the packing/zipping in internal buffers, we would do it
	//    with the contents of packByteStr after this point in OnFilePackDoc, and before
	//    the pBuf is written out via CFile ff below.
	// 4. If done in a buffer, after compression we could add the following Warning statement 
	//    in uncompressed form to the beginning of the compressed character buffer (before 
	//    writing it to the .aip file): "|~WARNING: DO NOT ATTEMPT TO CHANGE THIS FILE WITH 
	//    AN EDITOR OR WORD PROCESSOR! IT CAN ONLY BE UNCOMPRESSED WITH THE UNPACK COMMAND
	//    FROM WITHIN ADAPT IT VERSION 3.X. COMPRESSED DATA FOLLOWS:~|" 
	//    The warning would serve as a warning to users if they were to try to load the file
	//    into a word processor, not to edit it or save it within the word processor,
	//    otherwise the packed file would be corrupted. The warning (without line breaks
	//    or quote marks) would be 192 bytes long. When the file would be read from disk
	//    by DoUnpackDocument, this 192 byte warning would be stripped off prior to
	//    uncompressing the remaining data using the zlib tools.
	
    // whm 22Sep06 update: The wx version now uses wxWidget's built-in wxZipOutputStream
    // facilities for compressing and uncompressing packed documents, hence, it no longer
    // needs the services of Lucian Eischik's zip and unzip library. The wxWidget's zip
    // format is based on the same free-ware zlib library, so there should be no problem
    // zipping and unzipping .aip files produced by the MFC version or the WX version.

	// make a suitable default output filename for the packed data
	wxString exportFilename = gpApp->m_curOutputFilename;
	int len = exportFilename.Length();
	exportFilename.Remove(len-3,3); // remove the xml extension
	exportFilename += _T("aip"); // make it a *.aip file type
	wxString exportPath;
	wxString defaultDir;
	defaultDir = gpApp->m_curProjectPath;
	if (bInvokeFileDialog)
	{
		wxString filter;

		// get a file Save As dialog for Source Text Output
		filter = _("Packed Documents (*.aip)|*.aip||"); // set to "Packed Document (*.aip) *.aip"

		wxFileDialog fileDlg(
			(wxWindow*)wxGetApp().GetMainFrame(), // MainFrame is parent window for file dialog
			_("Filename For Packed Document"),
			defaultDir,
			exportFilename,
			filter,
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT); 
			// wxHIDE_READONLY was deprecated in 2.6 - the checkbox is never shown
			// GDLC wxSAVE & wxOVERWRITE_PROMPT were deprecated in 2.8
		fileDlg.Centre();

		// set the default folder to be shown in the dialog (::SetCurrentDirectory does not
		// do it) Probably the project folder would be best.
		bOK = ::wxSetWorkingDirectory(gpApp->m_curProjectPath);

		if (fileDlg.ShowModal() != wxID_OK)
		{
			// user cancelled file dialog so return to what user was doing previously, because
			// this means he doesn't want the Pack Document... command to go ahead
			return FALSE; 
		}

		// get the user's desired path
		exportPath = fileDlg.GetPath();
	}
	else
	{
		exportPath = defaultDir + gpApp->PathSeparator + exportFilename;
	}

	// get the length of the total byte string in packByteStr (exclude the null byte)
	int fileLength = packByteStr.GetLength();

    // wx version: we use the wxWidgets' built-in zip facilities to create the zip file,
    // therefore we no longer need the zip.h, zip.cpp, unzip.h and unzip.cpp freeware files
    // required for the MFC version.
	// first, declare a simple output stream using the temp zip file name
	// we set up an input file stream from the file having the raw data to pack
	wxString tempZipFile;
	wxString nameInZip;
    int extPos = exportPath.Find(_T(".aip"));
	tempZipFile = exportPath.Left(extPos);
	extPos = exportFilename.Find(_T(".aip"));
	nameInZip = exportFilename.Left(extPos);
	nameInZip = nameInZip + _T(".aiz");
	
	wxFFileOutputStream zippedfile(exportPath);
	// then, declare a zip stream placed on top of it (as zip generating filter)
	wxZipOutputStream zipStream(zippedfile);
    // wx version: Since our pack data is already in an internal buffer in memory, we can
    // use wxMemoryInputStream to access packByteStr; run it through a wxZipOutputStream
    // filter and output the resulting zipped file via wxFFOutputStream.
	wxMemoryInputStream memStr(packByteStr,fileLength);
	// create a new entry in the zip file using the .aiz file name
	zipStream.PutNextEntry(nameInZip);
	// finally write the zipped file, using the data associated with the zipEntry
	zipStream.Write(memStr);
	if (!zipStream.Close() || !zippedfile.Close() || 
		zipStream.GetLastError() == wxSTREAM_WRITE_ERROR) // Close() finishes writing the 
													// zip returning TRUE if successfully
	{
		wxString msg;
		msg = msg.Format(_("Could not write to the packed/zipped file: %s"),exportPath.c_str());
		wxMessageBox(msg,_T(""),wxICON_ERROR);
	} 

	exportPathUsed = exportPath;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		the value of the m_bSpecialText member of pSrcPhrase
/// \param		pSrcPhrase		<- a pointer to the source phrase instance on 
///									which this marker will be stored
/// \param		pLastSrcPhrase	<- a pointer to the source phrase immediately preceding
///									the current pSrcPhrase one (may be null)
/// \param		pChar			-> pChar points to the marker itself (ie. the marker's 
///									backslash)
/// \param		len				-> len is the length of the marker at pChar in characters 
///									(not bytes), determined by ParseMarker() in the caller
/// \param		pUsfmAnalysis	<- a pointer to the struct on the heap that a prior call to 
///									LookupSFM(ptr) returned, and could be NULL for an 
///									unknown marker. AnalyseMarker can potentially change
///									this to NULL, but it doesn't appear that such a change
///									affects pUsfmAnalysis in any calling routine
/// \remarks
/// Called from: the Doc's ReconstituteAfterFilteringChange(), TokenizeText() and 
/// DoMarkerHousekeeping().
/// Analyzes the current marker at pChar and determines what TextType and/or other 
/// attributes should be applied to the the associated pSrcPhrase, and particularly
/// what the m_curTextType member should be. Determines if the current TextType should 
/// be propagated or changed to something else. The return value is used to set the 
/// m_bSpecialText member of pSrcPhrase in the caller.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::AnalyseMarker(CSourcePhrase* pSrcPhrase, CSourcePhrase* pLastSrcPhrase,
									wxChar* pChar, int len, USFMAnalysis* pUsfmAnalysis)
// BEW ammended 21May05 to improve handling of endmarkers and suppressing their display
// in navigation text area of main window
// pSrcPhrase is the source phrase instance on which this marker will be stored, 
// pChar points to the marker itself (ie. the marker's backslash), 
// len is its length in characters (not bytes), determined by ParseMarker() in the caller
// pUsfmAnalysis is the struct on the heap that a prior call to LookupSFM(ptr) returned,
// and could be NULL for an unknown marker.
// The returned BOOL is the value of the m_bSpecialText member of pSrcPhrase.
{
	CSourcePhrase* pThis = pSrcPhrase;
	CSourcePhrase* pLast = pLastSrcPhrase;
	wxString str(pChar,len);// need this to get access to wxString's overloaded operators
							// (some people define huge standard format markers, so we need
							// a dynamic string)
	wxString strMkr; // for version 1.4.1 and onwards, to hold the marker less the esc char
	strMkr = str.Mid(1); // we want everything after the sfm escape character
	bool bEndMarker = IsEndMarker(pChar,pChar+len);

	// BEW added 23Mayo5; the following test only can return TRUE when the passed in marker
	// at pChar is a beginning marker for an inline section (these have the potential to
	// temporarily interrupt the propagation of the TextType value, while another value 
	// takes effect), or is the beginning marker for a footnote
	wxString nakedMkr = GetBareMarkerForLookup(pChar);
	bool bIsPreviousTextTypeWanted = FALSE;
	if (!bEndMarker)
		bIsPreviousTextTypeWanted = IsPreviousTextTypeWanted(pChar,pUsfmAnalysis);
	if (bIsPreviousTextTypeWanted)
	{
		if (pLast)
		{
			// set the global, and it will stay set until an endmarker or a footnote
			// endmarker makes use of it, and then clears it
			gPreviousTextType = pLast->m_curTextType;
		}
		else
			gPreviousTextType = verse; // a reasonable default at the start of the doc
	}

	// BEW 23May05 Don't delete this summary from Bill, it is a good systematic
	// treatment of what needs to be handled by the code below.

	// Determine how to handle usfm markers that can occur embedded
	// within certain other usfm markers. These include:
	//
	// 1. The "footnote content elements" marked by \fr...\fr*, \fk...\fk*, 
	// \fq...\fq*, \fqa...\fqa*, \fv...\fv*, \ft...\ft* and \fdc...\fdc*.
	// These footnote content element markers would be found only between 
	// \f and \f*. Their presence outside of \f and \f*, i.e., outside of
	// the footnote textType, would be considered an error.
	// Note: The ending markers for these footnote content elements are 
	// optional according to UBS docs, and the default is to only use the 
	// beginning marker and \ft to return to regular footnote text.
	//
	// 2. The "cross reference content element" markers. These include:
	// \xo...\xo*, \xk...\xk*, \xq...\xq*, \xt...\xt* and \xdc...\xdc*.
	// These cross reference content elements would be found only between 
	// \x and \x*. Their presence outside of \x and \x*, i.e., outside of
	// the crossReference textType, would be considered an error.
	//
	// 3. The "Special kinds of text" markers. These include: 
	// \qt...\qt*, \nd...\nd*, \tl...\tl*, \dc...\dc*, \bk...\bk*, 
	// \sig...\sig*, \pn...\pn*, \wj...\wj*, \k...\k*, \sls...\sls*, 
	// \ord...\ord*, and \add...\add*. Note: \lit is a special kind of 
	// text but is a paragraph style. These special kinds of text markers
	// can occur in verse, poetry, note and noType textTypes. Most of the 
	// special kinds of text markers could also occur in footnote textType.
	//
	// 4. The "Character styling" markers. These are now considered 
	// "DEPRECATED" by UBS, but include:
	// \no...\no*, \bd...\bd*, \it...\it*, \bdit...\bdit*, \em...\em*, 
	// and \sc...\sc*. They also can potentially be found anywhere. Adapt
	// It could (optionally) convert bar coded character formatting to
	// the equivalent character styling markers.
	//
	// 5. The "Special features" markers. These include: 
	// \fig...\fig*  (with the bar code separated parameters 
	// Desc|Cat|Size|Loc|Copy|Cap|Ref), and also the markers \pro...\pro*, 
	// \w...\w*, \wh...\wh*, \wg...\wg*, and \ndx...\ndx*. These also
	// may potentially be found anywhere.
	// 
	// The only end markers used in the legacy app were the footnotes \f* and \fe.
	// In USFM, however, we can potentially have many end markers, so we need
	// to have some special processing. I think I've got it smart enough, but it
	// can be changed further if necessary.

	if (!bEndMarker)
		pThis->m_bFirstOfType = TRUE; //default
	bool bFootnoteEndMarker = FALSE; // BEW added 23May05
	// pUsfmAnalysis will be NULL for unknown marker
	if (pUsfmAnalysis)
	{
		if (bEndMarker)
		{
			// verify that the found USFM marker matches  
			if (pUsfmAnalysis->endMarker != strMkr)
			{
				bEndMarker = FALSE;		// it isn't a recognized end marker
				pUsfmAnalysis = NULL;	// although the bare form was found 
										// lookup didn't really succeed
			}
		}
		// check for legacy png \fe and \F endmarkers that have no asterisk 
        // (BEW changes below, 23May05)
		if (pUsfmAnalysis && pUsfmAnalysis->png && (pUsfmAnalysis->marker == _T("fe") 
								|| pUsfmAnalysis->marker == _T("F")))
		{
			bEndMarker = TRUE; // we have a png end marker
			bFootnoteEndMarker = TRUE; // need this for restoring previous TextType
		}
		if (pUsfmAnalysis && pUsfmAnalysis->usfm && pUsfmAnalysis->endMarker == _T("f*"))
			bFootnoteEndMarker = TRUE; // need this for restoring previous TextType
	}
    // If bEndMarker is TRUE, our marker is actually an end marker. If pUsfmAnalysis is
    // NULL we can assume that it's an unknown type, so we'll treat it as special text as
    // did the legacy app, and will assign similar default values to pLast and pThis.

	// pUsfmAnalysis will be NULL for unknown marker
	//bool bEndMarkerForTextTypeNone = FALSE;
	bool bIsFootnote = FALSE;
	if (pUsfmAnalysis)
	{
		// Handle common//typical cases first...

        // Beginning footnote markers must set m_bFootnote to TRUE for backwards
        // compatibility with the legacy app (fortunately both usfm and png use 
        // the same \f marker!)
		if (strMkr == _T("f"))
		{
			pThis->m_bFootnote = TRUE;
			bIsFootnote = TRUE;
		}

        // Version 3.x sets m_curTextType and m_inform according to the attributes
        // specified in AI_USFM.xml (or default strings embedded in program code when
        // AI_USFM.xml is not available); we set a default here, but the special cases
        // further down may set different values
        // 
        // BEW note 11Oct10: if AnalyseMarker() is called more than once for the same
        // marker, then this next block appends the navigationText value to m_inform once
        // per call; so if we leave it as is (which is what I'm doing for the present), we
        // must ensure that code only calls it once per marker -- TokenizeText() changes is
        // a place to watch out for carefully; otherwise, change it to search for
        // navigationText in m_inform and not add it on subsequent calls if it is already
        // stored there.
		
		pThis->m_curTextType = pUsfmAnalysis->textType;
		if (pUsfmAnalysis->inform && !bEndMarker)
		{
			if (!pThis->m_inform.IsEmpty() &&
				pThis->m_inform[pThis->m_inform.Length()-1] != _T(' '))
			{
				pThis->m_inform += _T(' ');
			}
			pThis->m_inform += pUsfmAnalysis->navigationText;
		}
		// Handle the special cases below....

		if (pLast != 0)
		{
			// stuff in here requires a valid 'last source phrase ptr'
			if (!bEndMarker)
			{
                // initial markers may, or may not, set a boundary on the last sourcephrase
                // (eg. those with TextType == none never set a boundary on the last
                // sourcephrase)
				pLast->m_bBoundary = pUsfmAnalysis->bdryOnLast;

				if (pUsfmAnalysis->inLine)
				{
					if (bIsFootnote || pUsfmAnalysis->marker == _T("x"))
					{
						pLast->m_bBoundary = TRUE;
						pThis->m_bFirstOfType = TRUE;
					}
					else
					{
                        // its not a footnote, or cross reference, but it is an inline
                        // section, so determine whether or not it's a section with
                        // TextType == none
						if (pUsfmAnalysis->textType == none)
						{
                            // this section is one where we just keep propagating the
                            // preceding context across it
							pThis->m_curTextType = pLast->m_curTextType;
							pThis->m_bSpecialText = pLast->m_bSpecialText;
							pThis->m_bBoundary = FALSE;
							pThis->m_bFirstOfType = FALSE;
							return pThis->m_bSpecialText;
						}
						else
						{
                            // this section takes its own TextType value, and other flags
                            // to suite (we'll set m_bFirstOfType only if there is to be a
                            // boundary on the preceding sourcephrse)
							pThis->m_bFirstOfType = pUsfmAnalysis->bdryOnLast == TRUE;
						}
					}
				}
				else
				{
					// let the type set in the common section stand unchanged
					;
				}
			}
			else
			{
				// we are dealing with an endmarker
				
				// BEW 11Oct10 ************* NOTE ****************
				// This else block won't be entered for doc version 5, because we only
				// call AnalyseMarker() on markers in m_markers and no longer will
				// endmarkers be stored in m_markers -- so things done here have to be
				// done elsewhere in the code, if at all.
				// The only thing we must compensate for is to find a suitable place
				// within TokenizeText() for setting the m_bFootnoteEnd flag TRUE when \f*
				// is stored on pSrcPhrase; as for the code below here, just comment it
				// out and return default FALSE in order to get a clean compile
				/*
				bEndMarkerForTextTypeNone = IsEndMarkerForTextTypeNone(pChar); // see if 
                    // its an endmarker from the marker subset: ord, bd, it, em, bdit, sc,
                    // pro, ior, w, wr, wh, wg, ndx, k, pn, qs ?)
				if (bEndMarkerForTextTypeNone)
				{
                    // these have had the TextType value, and m_bSpecialText value,
                    // propagated across the marker's content span which is now being
                    // closed off, so we can be sure that the TextType, etc, just needs to
                    // be propaged on here
					pThis->m_curTextType = pLast->m_curTextType;
					pThis->m_bSpecialText = pLast->m_bSpecialText;
					pThis->m_bFirstOfType = FALSE;
					pLast->m_bBoundary = FALSE;
					return pThis->m_bSpecialText;
				}
				else
				{
                    // it's one of the endmarkers for a TextType other than none; the
                    // subspan just traversed will typically have a TextType different from
                    // verse, and we can't just assume we are reverting to the verse
                    // TextType now (we did so in the legacy app which supported only the
                    // png sfm set, and the only endmarkers were footnote ones, but now in
                    // the usfm context we've a much greater set of possibilities, so we
                    // need smarter code), so we have to work out what the reversion type
                    // should be (note, it can of course be subsequently changed if a
                    // following marker is parsed and that marker sets a different
                    // TextType). Our approach is to copy the texttype saved ealier in
                    // gPreviousTextType, but before doing that we try get the right
                    // m_bFirstOfType value set
					
					// bleed out the footnote end case 
					if (bFootnoteEndMarker)
					{
                        // its the end of a footnote (either png set or usfm set) so retore
                        // previous type
						pLast->m_bFootnoteEnd = TRUE;
						pThis->m_bFirstOfType = TRUE; // it's going to be something 
													  // different from a footnote
					}
					else
					{
						// not the end of a footnote

                        // some inline markers have text type of poetry or verse, so we
                        // don't want to put a m_bFirstOfType == TRUE value here when we
                        // are at the endmarker for one of those, because the most likely
                        // assumption is that the type will continue on here to what
                        // follows; but otherwise we do need to set it TRUE here because
                        // the type is most likely about to change
						USFMAnalysis* pAnalysis = LookupSFM(nakedMkr);
						if (pAnalysis)
						{
							if (pAnalysis->textType == verse)
							{
								pThis->m_bFirstOfType = FALSE;
							}
							else if (pAnalysis->textType == poetry)
							{
								pThis->m_bFirstOfType = FALSE;
							}
							else
							{
								pThis->m_bFirstOfType = TRUE;
							}
						}
						else
						{
							// wasn't a recognised marker, so all we can do is 
							// default to verse TextType
							pThis->m_curTextType = verse;
						}
					}
					pThis->m_curTextType = gPreviousTextType; // restore previous 
															  // context's value
					if (gPreviousTextType == verse || gPreviousTextType == poetry)
						pThis->m_bSpecialText = FALSE;
					else
						pThis->m_bSpecialText = TRUE;
					gPreviousTextType = verse; // restore the global's default value 
											   // for safety's sake
					return pThis->m_bSpecialText; // once we know its value, 
												  // we must return with it
				}
				*/
				return FALSE;
			}
		}
		return pUsfmAnalysis->special;
	}
	else  // it's an unknown sfm, so treat as special text
	{
		// we don't have a pUsfmAnalysis for this situation
		// so set some reasonable defaults (as did the legacy app)
		if (pLast != NULL)
		{
			// stuff in here requires a valid 'last source phrase ptr'
			pLast->m_bBoundary = TRUE;
		}
		pThis->m_bFirstOfType = TRUE;
		pThis->m_curTextType = noType;
		// just show the marker bracketed by question marks, i.e., ?\tn?
        // whm Note 11Jun05: I assume that an unknown marker should not appear in the
        // navigation text line if it is filtered. I've also modified the code in
        // RedoNavigationText() to not include the unknown marker in m_inform when the
        // unknown marker is filtered there, and it seems that would be appropriate here
        // too. If Bruce thinks the conditional call to IsAFilteringUnknownSFM is not
        // appropriate here the conditional I've added should be removed; likewise the
        // parallel code I've added near the end of RedoNavigationText should be evaluated
        // for appropriateness.
        // BEW comment 15Jun05 - I agree unknowns which are filtered should not appear with
        // ?..? bracketing; in fact, I've gone as far as to say that no filtered marker
        // should have its nav text displayed in the main window -- and coded accordingly
		if (!IsAFilteringUnknownSFM(nakedMkr))
		{
		pThis->m_inform += _T("?");
			pThis->m_inform += str; // str is a whole marker here
		pThis->m_inform += _T("? ");
		}
		return TRUE; //ie. assume it's special text
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		always TRUE
/// \remarks
/// Called from: the doc/view framework - but at different times in MFC than 
/// in the wx framework.
/// Deletes the list of source phrases (by calling DeleteSourcePhrases) and destroys the
/// view's strips, piles and cells.
/// This override is never explicitly called in the MFC version. In the wx version,
/// however, DeleteContents() needs to be called explicitly from the Doc's OnNewDocument(),
/// OnCloseDocument() and the View's ClobberDocument() because the doc/view framework in wx
/// works differently. In wx code, the Doc's OnNewDocument() must avoid calling the
/// wxDocument::OnNewDocument base class - because it calls OnCloseDocument() which in turn
/// would foul up the KB structures because OnCloseDocument() calls EraseKB(), etc. Instead
/// the wx version just calls DeleteContents() explicitly where needed.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::DeleteContents()
// refactored 10Mar09
// This is an override of the doc/view method
// MFC docs say: "Called by the framework to delete the document's data without destroying
// the CDocument object itself. It is called just before the document is to be destroyed.
// It is also called to ensure that a document is empty before it is reused. This is
// particularly important for an SDI application, which uses only one document; the
// document is reused whenever the user creates or opens another document. Call this
// function to implement an "Edit Clear All" or similar command that deletes all of the
// document's data. The default implementation of this function does nothing. Override this
// function to delete the data in your document."
{
	CAdapt_ItApp* pApp = &wxGetApp();
	// zero the contents of the read-in file of source text data
	if (pApp->m_pBuffer != 0)
	{
		pApp->m_pBuffer->Empty();
	}

	// delete the source phrases
	DeleteSourcePhrases(); // this does not delete the partner piles as 
		// the internal DeleteSingleSrcPhrase() has the 2nd param FALSE so
		// that it does not call DeletePartnerPile() -- so delete those
		// separately below en masse with the DestroyPiles() call

	// the strips, piles and cells have to be destroyed to make way for the new ones
	CAdapt_ItView* pView = (CAdapt_ItView*)NULL;
	pView = (CAdapt_ItView*)GetFirstView();
	wxASSERT(pApp != NULL);
	if (pView != NULL)
	{
		CLayout* pLayout = GetLayout();
		pLayout->DestroyStrips();
		pLayout->DestroyPiles(); // destroy these en masse
		pLayout->GetPileList()->Clear();
		pLayout->GetInvalidStripArray()->Clear();

		if (pApp->m_pTargetBox != NULL)
		{
			pApp->m_pTargetBox->ChangeValue(_T(""));
		}

		pApp->m_targetPhrase = _T("");
	}

	translation = _T(""); // make sure the global var is clear
	
	return wxDocument::DeleteContents(); // just returns TRUE
}


///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		pKB		<- a pointer to the current KB instance
/// \remarks
/// Called from: the App's SubstituteKBBackup(), ClearKB(), AccessOtherAdaptionProject(),
/// the Doc's OnCloseDocument(), the View's OnFileCloseProject(), DoConsistencyCheck().
/// Deletes the Knowledge Base after emptying and deleting its maps and its contained 
/// list of CTargetUnit instances, and each CTargetUnit's contained list of CRefString 
/// objects. (Note: OnCloseDocument() is not called explicitly by the Adapt It
/// code. It is called only by the framework when the application is being shut down.
/// Therefore it really involves a project closure as well, and therefore Erasure
/// of the KB and GlossingKB is appropriate; if the doc is dirty, then the user will
/// be given a chance to save it, and if he does, the KBs get saved then automatically
/// too.)
/// BEW added 13Nov09: removal of read-only protection (which involves unlocking
/// the ~AIROP-machinename-user-name.lock file, and removing it from the project
/// folder, rendering the project ownable for writing by whoever first enters it
/// subsequently). Doing this is appropriate in EraseKB() because EraseKB() is
/// called whenever the project is being left by whoever has current ownership
/// permission.
/// BEW 28May10, removed TUList, as it is redundant & now unused
/// BEW 1Jun10, added deletion of CRefStringMetadata instance
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::EraseKB(CKB* pKB)
{
	// Empty the map and delete their contained objects
	if (pKB != NULL)
	{
		// Clear all elements from each map, and delete each map
		for (int i = 0; i < MAX_WORDS; i++)
		{
			if (pKB->m_pMap[i] != NULL) // test for NULL whm added 10May04
			{
				if (!pKB->m_pMap[i]->empty())
				{
					MapKeyStringToTgtUnit::iterator iter;
					for (iter = pKB->m_pMap[i]->begin(); iter != pKB->m_pMap[i]->end(); ++iter)
					{
						wxString srcKey = iter->first;
						CTargetUnit* pTU = iter->second;
						TranslationsList::Node* tnode = NULL;
						if (pTU->m_pTranslations->GetCount() > 0)
						{
							for (tnode = pTU->m_pTranslations->GetFirst(); 
									tnode; tnode = tnode->GetNext())
							{
								CRefString* pRefStr = (CRefString*)tnode->GetData();
								if (pRefStr != NULL)
								{
									pRefStr->DeleteRefString();
								}
							}
						}
						delete pTU;
						//pTU = (CTargetUnit*)NULL;
					}
					pKB->m_pMap[i]->clear();
				}
				delete pKB->m_pMap[i];
				pKB->m_pMap[i] = (MapKeyStringToTgtUnit*)NULL; // whm added 10May04
			}
		}
	}

	if (pKB != NULL)
	{
		// Lastly delete the KB itself
		delete pKB;
		pKB = (CKB*)NULL;
	}

	// BEW added 13Nov09, for restoring the potential for ownership for write permission
	// of the current accessed project folder.
	if (!gpApp->m_curProjectPath.IsEmpty())
	{
		bool bRemoved = gpApp->m_pROP->RemoveReadOnlyProtection(gpApp->m_curProjectPath);
		if (bRemoved)
		{
			gpApp->m_bReadOnlyAccess = FALSE; // project folder is now ownable for writing
		}
		// we are leaving this folder, so the local process must have m_bReadOnlyAccess unilaterally
		// returned to a FALSE value - whether or not a ~AIROP-*.lock file remains in the folder
		gpApp->m_bReadOnlyAccess = FALSE;
		gpApp->GetView()->canvas->Refresh(); // force color change back to normal white background
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		always TRUE
/// \remarks
/// Called from: the doc/view framework automatically calls OnCloseDocument() when
/// necessary; it is not explicitly called from program code.
/// Closes down the current document and clears out the KBs from memory. This override does
/// not call the base class wxDocument::OnCloseDocument(), but does some document
/// housekeeping and calls DeleteContents() and finally sets Modify(FALSE).
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::OnCloseDocument()
// MFC Note: This function just closes the doc down, with no saving, and clears out 
// the KBs from memory with no saving of them either (presumably, they were saved 
// to disk earlier if their contents were of value.) The disk copies of the KBs 
// therefore are unchanged from the last save.
//
// whm notes on CLOSING the App, the View, and the Doc:
// This note describes the flow of control when the MFC app (CFrameWnd) does OnClose() 
// and the WX app (wxDocParentFrame) does OnCloseWindow():
// IN MFC:	CFrameWnd::OnClose()
// - ->		pApp::CloseAllDocuments(FALSE) // parameter BOOL bEndSession
// - ->		CDocMagager::CloseAllDocuments(FALSE) // parameter BOOL bEndSession
// - ->		CDocTemplate::CloseAllDocuments(FALSE) // parameter BOOL bEndSession
// - ->		CAdapt_ItDoc::OnCloseDocument() [see note following:]
// - ->		CDocument::OnCloseDocument() // MFC app overrides this and calls it at end of override
// The MFC AI override calls EraseKB on the adapting and glossing KBs, updates
// some settings for the view and the app for saving to config files on closure, then
// lastly calls the CDocument::OnCloseDocument() which itself calls pFrame->DestroyWindow()
// for any/all views, then calls the doc's DeleteContents(), and finally calls delete on
// "this" the document if necessary (i.e., if m_bAutoDelete is TRUE). The important thing
// to note here is that WITHIN OnCloseDocument() ALL 3 of the following are done in this 
// order:	(1) The view(s) are closed (and associated frames destroyed) - a flag is used
//				to prevent (3) below from happening while closing/deleting the views
//			(2) The Doc's DeleteContents() is called
//			(3) The doc itself is deleted
//
// IN WX:	wxDocParentFrame::OnCloseWindow() // deletes all views and documents, then the wxDocParentFrame and exits the app
// - ->		wxDocManager::Clear() // calls CloseDocuments(force) and deletes the doc templates
// - ->*	wxDocManager::CloseDocuments(bool force) // On each doc calls doc->Close() then doc->DeleteAllViews() then delete doc
// - ->		wxDocument::Close() // first calls OnSaveModified, then if ok, OnCloseDocument()
// - ->		CAdapt_ItDoc::OnCloseDocument() [see note following:]
// - ->		[wxDocument::OnCloseDocument()] // WX app overrides this
// The WX AI override calls EraseKB on the adapting and glossing KBs and updates settings
// just as the MFC override does. Problems occur at line marked - ->* above due to the
// EARLY DeleteAllViews() call. This DeleteAllViews() also calls view->Close() on any
// views, and view->Close() calls view::OnClose() whose default behavior calls
// wxDocument::Close to "close the associated document." This results in OnCloseDocument()
// being called a second time in the process of closing the view(s), with damaging
// additional calls to EraseKB (the m_pMap[i] members have garbage pointers the second time
// around). To avoid this problem we need to override one or more of the methods that
// result in the additional damaging call to OnCloseDocument, or else move the erasing of
// our CKB structures out of OnCloseDocument() to a more appropriate place.
// Trying the override route, I tried first overriding view::OnClose() and doc::DeleteAllViews

// WX Note: Compare the following differences between WX and MFC
	// In wxWidgets, the default docview.cpp wxDocument::OnCloseDocument() looks like this:
				//bool wxDocument::OnCloseDocument()
				//{
				//	// Tell all views that we're about to close
				//	NotifyClosing();  // calls OnClosingDocument() on each view which does nothing in base class
				//	DeleteContents(); // does nothing in base class
				//	Modify(FALSE);
				//	return TRUE;
				//}

	// In MFC, the default doccore.cpp CDocument::OnCloseDocument() looks like this:
				//void CDocument::OnCloseDocument()
				//	// must close all views now (no prompting) - usually destroys this
				//{
				//	// destroy all frames viewing this document
				//	// the last destroy may destroy us
				//	BOOL bAutoDelete = m_bAutoDelete;
				//	m_bAutoDelete = FALSE;  // don't destroy document while closing views
				//	while (!m_viewList.IsEmpty())
				//	{
				//		// get frame attached to the view
				//		CView* pView = (CView*)m_viewList.GetHead();
				//		CFrameWnd* pFrame = pView->GetParentFrame();
				//		// and close it
				//		PreCloseFrame(pFrame);
				//		pFrame->DestroyWindow();
				//			// will destroy the view as well
				//	}
				//	m_bAutoDelete = bAutoDelete;
				//	// clean up contents of document before destroying the document itself
				//	DeleteContents();
				//	// delete the document if necessary
				//	if (m_bAutoDelete)
				//		delete this;
				//}
	// Compare wxWidgets wxDocument::DeletAllViews() below to MFC's OnCloseDocument() above:
				//bool wxDocument::DeleteAllViews()
				//{
				//    wxDocManager* manager = GetDocumentManager();
				//
				//    wxNode *node = m_documentViews.First();
				//    while (node)
				//    {
				//        wxView *view = (wxView *)node->Data();
				//        if (!view->Close())
				//            return FALSE;
				//
				//        wxNode *next = node->Next();
				//
				//        delete view; // Deletes node implicitly
				//        node = next;
				//    }
				//    // If we haven't yet deleted the document (for example
				//    // if there were no views) then delete it.
				//    if (manager && manager->GetDocuments().Member(this))
				//        delete this;
				//
				//    return TRUE;
				//}
	// Conclusion: Our wxWidgets version of OnCloseDocument() should NOT call the
	// wxDocument::OnCloseDocument() base class method, but instead should make the
	// following calls in its place within the OnCloseDocument() override:
	//		DeleteAllViews() // assumes 'delete this' at end can come before DeleteContents()
	//		DeleteContents()
	//		Modify(FALSE)
{
	CAdapt_ItApp* pApp = &wxGetApp();

	// the EraseKB() call will also try to remove any read-only protection
	EraseKB(pApp->m_pKB); // remove KB data structures from memory - EraseKB in the App in wx
	pApp->m_pKB = (CKB*)NULL; // whm added
	EraseKB(pApp->m_pGlossingKB); // remove glossing KB structures from memory - 
								  // EraseKB in the App in wx
	pApp->m_pGlossingKB = (CKB*)NULL; // whm added
	
	CAdapt_ItView* pView;
	CAdapt_ItDoc* pDoc;
	CPhraseBox* pBox;
	pApp->GetBasePointers(pDoc,pView,pBox);
	wxASSERT(pView);
// GDLC 2010-03-27 pFreeTrans is now unused in this function
//	CFreeTrans* pFreeTrans = pApp->GetFreeTrans();
//	wxASSERT(pFreeTrans);

	if (pApp->m_nActiveSequNum == -1)
		pApp->m_nActiveSequNum = 0;
	pApp->m_lastDocPath = pApp->m_curOutputPath;
	pApp->nLastActiveSequNum = pApp->m_nActiveSequNum;

    // BEW added 21Apr08; clean out the global struct gEditRecord & clear its deletion
    // lists, because each document, on opening it, it must start with a truly empty
    // EditRecord; and on doc closure and app closure, it likewise must be cleaned out
    // entirely (the deletion lists in it have content which persists only for the life of
    // the document currently open)
	pView->InitializeEditRecord(gEditRecord);
	gEditRecord.deletedAdaptationsList.Clear(); // remove any stored deleted adaptation strings
	gEditRecord.deletedGlossesList.Clear(); // remove any stored deleted gloss strings
	gEditRecord.deletedFreeTranslationsList.Clear(); // remove any stored deleted free translations

	// send the app the current size & position data, for saving to config files on closure
	wxRect rectFrame;
	CMainFrame *pFrame = wxGetApp().GetMainFrame();
	wxASSERT(pFrame);
	rectFrame = pFrame->GetRect(); // screen coords
	rectFrame = NormalizeRect(rectFrame); // use our own from helpers.h
	pApp->m_ptViewTopLeft.x = rectFrame.GetX();
	pApp->m_ptViewTopLeft.y = rectFrame.GetY();
	
	pApp->m_szView.SetWidth(rectFrame.GetWidth());
	pApp->m_szView.SetHeight(rectFrame.GetHeight());
	pApp->m_bZoomed = pFrame->IsMaximized();

	DeleteContents(); // this is required to avoid leaking heap memory on exit
	Modify(FALSE);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		nValueForFirst	-> the number to use for the first source phrase in pList
/// \param		pList			<- a SPList of source phrases whose m_nSequNumber members are
///									to be put in sequence
/// \remarks
/// Called from: the Doc's TokenizeText().
/// Resets the m_nSequNumber member of all the source phrases in pList so that they are in 
/// numerical sequence (ascending order) with no gaps, starting with nValueForFirst.
/// This function differs from UpdateSequNumbers() in that AdjustSequNumbers() effects its 
/// changes only on the pList passed to the function; in UpdateSequNumbers() the current 
/// document's source phrases beginning with nFirstSequNum are set to numerical sequence 
/// through to the end of the document.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::AdjustSequNumbers(int nValueForFirst, SPList* pList)
{
	CSourcePhrase* pSrcPhrase;
	SPList::Node *node = pList->GetFirst();
	wxASSERT(node != NULL);
	int sn = nValueForFirst-1;
	while (node)
	{
		sn++;
		pSrcPhrase = (CSourcePhrase*)node->GetData();
		node = node->GetNext(); 
		pSrcPhrase->m_nSequNumber = sn;
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		pstr	<- a wxString buffer containing text to process
/// \remarks
/// Called from: the Doc's OnNewDocument().
/// Removes any ventura publisher optional hyphen codes ("<->") from string buffer pstr.
/// After removing any ventura optional hyphens it resets the App's m_nInputFileLength 
/// global to reflect the new length.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::RemoveVenturaOptionalHyphens(wxString*& pstr)
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	wxString strVOH = _T("<->");
	int nFound = 0;
	int nNewLength = (*pstr).Length();
	while ((nFound = FindFromPos((*pstr),strVOH,nFound)) != -1)
	{
		// found an instance, so delete it
		// Note: wxString::Remove must have second param otherwise it will just
		// truncate the remainder of the string
		(*pstr).Remove(nFound,3);
	}

	// set the new length
	nNewLength = (*pstr).Length();
	pApp->m_nInputFileLength = (wxUint32)(nNewLength + 1); // include terminating null char
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a wxString in which any \n, \r, or \t characters have been converted to spaces
/// \param		str		-> a wxString that is examined for embedded whitespace characters
/// \remarks
/// Called from: the Doc's TokenizeText().
/// This version of NormalizeToSpaces() is used in the wx version only.
/// This function changes any kind of whitespace (\n, \r, or \t) in str to simple space(s).
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::NormalizeToSpaces(wxString str)
{
	// whm added to normalize white space when presented with wxString (as done in the wx version)
	str.Replace(_T("\n"),_T(" ")); // LF (new line on Linux and internally within Windows)
	str.Replace(_T("\r"),_T(" ")); // CR (new line on Mac)
	str.Replace(_T("\t"),_T(" ")); // tab
	return str;
	// alternate code below:
	//wxString temp;
	//temp = str;
	//int len = temp.Length();
	//int ct;
	//for (ct = 0; ct < len; ct++)
	//{
	//	if (wxIsspace(temp.GetChar(ct)))
	//		temp.SetChar(ct, _T(' '));
	//}
	//return temp;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if pChar is pointing at an end marker whose associated textType is none, 
///				otherwise FALSE
/// \param		pChar	-> a pointer to a character in a buffer
/// \remarks
/// Called from: the Doc's AnalyseMarker().
/// Determines if the marker at pChar in a buffer is an end marker and if so, if the end marker
/// has an associated textType of none.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsEndMarkerForTextTypeNone(wxChar* pChar)
{
	wxString bareMkr = GetBareMarkerForLookup(pChar);
	wxASSERT(!bareMkr.IsEmpty());
	USFMAnalysis* pAnalysis = LookupSFM(bareMkr);
	wxString marker = GetMarkerWithoutBackslash(pChar);
	wxASSERT(!marker.IsEmpty());
	if (marker == pAnalysis->endMarker && pAnalysis->textType == none)
		return TRUE;
	else
		return FALSE;

}

///////////////////////////////////////////////////////////////////////////////
/// \return		the count of how many CSourcePhrase instances are in the m_pSourcePhrases list
///				after the retokenize and doc rebuild is done.
/// \param		bChangedPunctuation	-> TRUE if punctuation has changed, FALSE otherwise
/// \param		bChangedFiltering	-> TRUE if one or more markers' filtering status has changed,
///										FALSE otherwise
/// \param		bChangedSfmSet		-> TRUE if the Sfm Set has changed, FALSE otherwise
/// \remarks
/// Called from: the App's DoPunctuationChanges(), DoUsfmFilterChanges(),
/// DoUsfmSetChanges(), the View's OnEditSourceText().
/// Calls the appropriate document rebuild function for the indicated changes. For
/// punctuation changes if calls ReconstituteAfterPunctuationChange(); for filtering
/// changes it calls ReconstituteAfterFilteringChange(); and for sfm set changes, the
/// document is processed three times, the first pass calls SetupForSFMSetChange() and
/// ReconstituteAfterFilteringChange() to unfilter any previously filtered material, the
/// second pass again calls SetupForSFMSetChange() with adjusted parameters and
/// ReconstituteAfterFilteringChange() to filter any new filtering changes. The third pass
/// calls DoMarkerHousekeeping() to insure that TextType, m_bSpecialText, and m_inform
/// members of pSrcPhrase are set correctly at each location after the other major changes
/// have been effected.
///////////////////////////////////////////////////////////////////////////////
int CAdapt_ItDoc::RetokenizeText(bool bChangedPunctuation,bool bChangedFiltering, 
								 bool bChangedSfmSet)
// bew modified signature 18Apr05
// Returns the count of how many CSourcePhrase instances are in the m_pSourcePhrases list
// after the retokenize and doc rebuild is done.
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	CAdapt_ItView* pView = pApp->GetView();
	wxASSERT(pView->IsKindOf(CLASSINFO(CAdapt_ItView)));

    // first determine whether or not the data was unstructured plain text - we need to
    // know because we don't want to try accumulate chapter:verse references for locations
    // where the rebuild failed if in fact there are no \c and \v markers in the original
    // source data!
	gbIsUnstructuredData = pView->IsUnstructuredData(gpApp->m_pSourcePhrases);

    // set up the string which is to preserve a record of where rebuilding will need to be
    // done manually later on, because what was expected did not occur
	wxString fixesStr;
	if (gbIsUnstructuredData)
	{
        // this one will not change, since there are no chapter verse references able to be
        // constructed
		fixesStr = _(
			"There were places where automatic rebuilding of the document potentially did not fully succeeded. Visually check and edit where necessary: ");
	}
	else
	{
        // it has standard format markers including \v and \c (presumably), so we can add
        // n:m style of references to the end of the string - we take whatever is in the
        // m_chapterVerse wxString member of the passed in pointer to the CSourcePhrase
        // instance
		// IDS_MANUAL_FIXES_REFS
		fixesStr = _(
			"Locate the following chapter:verse locations for potential errors in the rebuild of the document. Visually check and edit where necessary: ");
	}

	int nOldTotal = gpApp->m_pSourcePhrases->GetCount();
	if (nOldTotal == 0)
	{
		return 0;
	}

	// put up a progress indicator
    // whm Note: RetokenizeText doesn't really need a progress dialog; it is mainly called
    // by other routines that have their own progress dialog, with the result that to have
    // a separate progress dialog for RetokenizeText, we end up with two progress dialogs,
    // one partially hiding the other.

	int nOldCount = 0;

	// whatever initialization is needed
	SPList::Node *pos;
	SPList::Node *oldPos;
    CSourcePhrase* pSrcPhrase = NULL;
	bool bNeedMessage = FALSE;

	// perform each type of document rebuild
	bool bSuccessful;
    if (bChangedPunctuation)
    {
        pos = gpApp->m_pSourcePhrases->GetFirst();
        while (pos != NULL)
        {
			oldPos = pos;
			pSrcPhrase = (CSourcePhrase*)pos->GetData();
			pos = pos->GetNext();

            // acts on ONE instance of pSrcPhrase only each time it loops, & it may add
            // many to the list, or remove some, or leave number in the list unchanged
			bSuccessful = ReconstituteAfterPunctuationChange(pView, 
							gpApp->m_pSourcePhrases, oldPos, pSrcPhrase,fixesStr);
			if (!bSuccessful)
			{
                // adaptation abandoned, so add a chapter:verse reference to the fixesStr
                // if the source text was (U)SFM structured text. The code for adding to
                // fixesStr (a chapter:verse reference plus 3 spaces) needs to be within
                // each of ReconstituteAfterPunctuationChange's subfunctions, as it depends
                // on pSrcPhrase being correct and so we must do the update to fixesStr
                // before we mess with replacing the pSrcPhrase with what was the new
                // parse's CSourcePhrase instances when automatic rebuild could not be done
                // correctly
				// BEW changed 8Mar11 for punctuation reconstitution -- to include the
				// m_srcPhrase test after the chapter:verse & a delimiting space, and we
				// don't make any insertions of new CSourcePhrase instances into
				// m_pSourcePhrases, but wherever FALSE is returned, we just keep the
				// original pSrcPhrase unchanged and have a reference for where this was
				// in fixesStr
				bNeedMessage = TRUE;
			}
			// update progress bar every 20 iterations
			++nOldCount;
			//if (20 * (nOldCount / 20) == nOldCount)
			//{
			//	//prog.m_progress.SetValue(nOldCount); //prog.m_progress.SetPos(nOldCount);
			//	//prog.TransferDataToWindow(); //prog.UpdateData(FALSE);
			//	//wxString progMsg = _("Retokenizing - File: %s  - %d of %d Total words and phrases");
			//	//progMsg = progMsg.Format(progMsg,gpApp->m_curOutputFilename.c_str(),nOldCount,nOldTotal);
			//}
        }
    }

	if (bChangedSfmSet)
    {
        // We accomplish the desired effects by successive calls of
        // ReconstituteAfterFilteringChanges(), with a little massaging of the data
        // structures in which it relies, before each call.
		bool bSuccessful;

        // The first pass through the document has to be done with the previous SFM set in
        // effect, and the saved wxString of the previous filter markers for that set -
        // these have to be unfiltered, but we won't unfilter any which are markers in
        // common with the new set and which are also filtered in the new set. Since we are
        // going to fiddle with the SfmSet value, we need to save the current value and
        // restore it when we are done. m_sfmSetAfterEdit stores the current value in
        // effect after the Preferences are exited, so we will use that for restoring
        // gCurrentSfmSet later below

#ifdef _Trace_RebuildDoc
		TRACE1("\n saveCurrentSfmSet = %d\n",(int)m_sfmSetAfterEdit);
		TRACE3("\n bChangedSfmSet TRUE; origSet %d, newSet %d, origMkrs %s\n", m_sfmSetBeforeEdit,
					gpApp->gCurrentSfmSet, m_filterMarkersBeforeEdit);
		TRACE2("\n bChangedSfmSet TRUE; curFilterMkrs: %s\n and the secPassMkrs: %s, pass = FIRST\n\n", 
					gpApp->gCurrentFilterMarkers, m_secondPassFilterMarkers);
#endif

		SetupForSFMSetChange(gpApp->m_sfmSetBeforeEdit, gpApp->gCurrentSfmSet, 
				gpApp->m_filterMarkersBeforeEdit,gpApp->gCurrentFilterMarkers, 
				gpApp->m_secondPassFilterMarkers, first_pass);

		if (gpApp->m_FilterStatusMap.size() > 0)
		{
			// we only unfilter if there is something to unfilter
			bSuccessful = ReconstituteAfterFilteringChange(pView, 
								gpApp->m_pSourcePhrases, fixesStr);
			if (!bSuccessful)
			{
                // at least one error, so make sure there will be a message given (the
                // ReconstituteAfterFilteringChange() function will append the needed
                // material to fixesStr internally (each time there is such an error)
                // before returning
				bNeedMessage = TRUE;
			}
		}

        // restore the filtering status of the original set's markers, in case the user
        // later changes back to that set (gCurrentSfmSet's value is still the one for the
        // old set)
		ResetUSFMFilterStructs(gpApp->gCurrentSfmSet, gpApp->m_filterMarkersBeforeEdit, _T(""));

        // restore the current SFM set value. This is the value which the user changed to
        // in the USFMPage, and it has to be the current value when the second pass is
        // executed below
		gpApp->gCurrentSfmSet = gpApp->m_sfmSetAfterEdit;

#ifdef _Trace_RebuildDoc
		TRACE3("\n bChangedSfmSet TRUE; origSet %d, newSet %d, origMkrs %s\n", m_sfmSetBeforeEdit, 
				gpApp->gCurrentSfmSet, m_filterMarkersBeforeEdit);
		TRACE2("\n bChangedSfmSet TRUE; gCurrentFilterMarkers: %s\n and the secPassMkrs: %s, pass = SECOND\n\n", 
				gpApp->gCurrentFilterMarkers, m_secondPassFilterMarkers);
#endif

		SetupForSFMSetChange(gpApp->m_sfmSetBeforeEdit, 
					gpApp->gCurrentSfmSet, gpApp->m_filterMarkersBeforeEdit,
					gpApp->gCurrentFilterMarkers, gpApp->m_secondPassFilterMarkers, 
					second_pass);

		if (gpApp->m_FilterStatusMap.size() > 0)
		{
			// we only filter if there is something to filter
			bSuccessful = ReconstituteAfterFilteringChange(pView, 
											gpApp->m_pSourcePhrases, fixesStr);
			if (!bSuccessful)
			{
                // at least one error, so make sure there will be a message given (the
                // ReconstituteAfterFilteringChange() function will append the needed
                // material to fixesStr internally (each time there is such an error)
                // before returning
				bNeedMessage = TRUE;
			}
		}

        // Typically, after any refiltering is done, there will be errors remaining in the
        // document - these are old pSrcPhrase->m_inform strings which are now out of date,
        // TextType values which are set or changed at the wrong places and improperly
        // propagated in the light of the new SFM set now in effect, and likewise
        // m_bSpecialText will in many places be wrong, changed when it shouldn't be, etc.
        // To fix all this stuff we have to scan across the whole document with the
        // DoMarkerHousekeeping() function, which duplicates some of TokenizeText()'s code,
        // to get the TextType, m_bSpecialText, and m_inform members of pSrcPhrase correct
        // at each location Setup the globals for this call...
		gpFollSrcPhrase = NULL; // the "sublist" is the whole document, so there is no 		
		gpPrecSrcPhrase = NULL; // preceding or following source phrase to be considered
		gbSpecialText = FALSE;
		gPropagationType = verse; // default at start of a document
		gbPropagationNeeded = FALSE; // gpFollSrcPhrase is null, & we can't propagate 
									 // at end of doc
		int docSrcPhraseCount = gpApp->m_pSourcePhrases->size();
		DoMarkerHousekeeping(gpApp->m_pSourcePhrases,
								docSrcPhraseCount,gPropagationType,gbPropagationNeeded);
	}

    if (bChangedFiltering)
	{
        // if called, ReconstituteAfterFilteringChange() sets up the progress window again
        // and destroys it before returning
		bool bSuccessful = ReconstituteAfterFilteringChange(pView, 
													gpApp->m_pSourcePhrases, fixesStr);
		if (!bSuccessful)
		{
            // at least one error, so make sure there will be a message given (the
            // ReconstituteAfterFilteringChange() function will append the needed material
            // to fixesStr internally (each time there is such an error) before returning
			bNeedMessage = TRUE;
		}
	}

	// find out how many instances are in the list when all is done and return it to the
	// caller 
    int count = gpApp->m_pSourcePhrases->GetCount();

	// make sure everything is correctly numbered in sequence; shouldn't be necessary, 
    // but no harm done
	UpdateSequNumbers(0);

	// warn the user if he needs to do some visual checking etc.
	if (bNeedMessage)
	{
        // make sure the message is not too huge for display - if it exceeds 1200
        // characters, trim the excess and add "... for additional later locations where
        // manual editing is needed please check the document visually. A full list has
        // been saved in your project folder in Rebuild Log.txt" (the addition is not put
        // in the message if the data is unstructured as (U)SFM stuff)
		int len = fixesStr.Length();

		// build the path to the current project's folder and output the full log
		if (!gbIsUnstructuredData)
		{
			wxString path;
			path.Empty();
			path << gpApp->m_curProjectPath;
			path << gpApp->PathSeparator;
			path << _T("Rebuild Log");
            // add a unique number each time, incremented by one from previous number
            // (starts at 0 when app was launched)
			gnFileNumber++; // get next value
			wxString suffixStr;
			suffixStr = suffixStr.Format(_T("%d"),gnFileNumber);
			//suffixStr = _T("1");

			path += suffixStr;
			path += _T(".txt");
			//path << suffixStr;
			//path << _T(".txt");
		
			wxLogNull logNo; // avoid spurious messages from the system

			wxFile fout;
			bool bOK;
			bOK = fout.Open( path, wxFile::write );
			fout.Write(fixesStr,len);
			fout.Close();
		}

        // prepare a possibly shorter message - if there are not many bad locations it may
        // suffice; but if too long then the message itself will inform the user to look in
        // the project folder for the "Rebuild Log.txt" file
		if (len > 1200 && !gbIsUnstructuredData)
		{
			// trim the excess and add the string telling user to check visually 
			// & of the existence of Rebuild Log.txt
			fixesStr = fixesStr.Left(1200);
			fixesStr = MakeReverse(fixesStr);
			int nFound = fixesStr.Find(_T(' '));
			if (nFound != -1)
			{
				fixesStr = fixesStr.Mid(nFound);
			}
			fixesStr = MakeReverse(fixesStr);
			wxString appendStr;
			// IDS_APPEND_MSG
			appendStr = _(
" ... for additional later locations needing manual editing please check the document visually. The full list has been saved in your project folder in the file \"Rebuild Log.txt\"");
			fixesStr += appendStr;
		}
		else if (!gbIsUnstructuredData)
		{
			wxString appendLogStr;
			// IDS_APPEND_LOG_REF
			appendLogStr = _(
"    This list has been saved in your project folder in the file \"Rebuild Log.txt\"");
			fixesStr += appendLogStr; // tell the user about the log file:  Rebuild Log.txt
		}
        // display the message - in the case of unstructured data, there will be no list of
        // locations and the user will just have to search the document visually
		wxMessageBox(fixesStr, _T(""), wxICON_INFORMATION); 
	}

	// this is where we need to have the layout updated. We will do the whole lot, that is,
	// destroy and recreate both piles and strips
#ifdef _NEW_LAYOUT
	GetLayout()->RecalcLayout(pApp->m_pSourcePhrases, create_strips_and_piles);
#else
	GetLayout()->RecalcLayout(pApp->m_pSourcePhrases, create_strips_and_piles);
#endif

	gpApp->m_pActivePile = pView->GetPile(gpApp->m_nActiveSequNum);
	GetLayout()->m_docEditOperationType = retokenize_text_op;
	return count;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		nFirstSequNum	-> the number to use for the first source phrase in pList
/// \param      pOtherList      -> default value is NULL, but if a pointer to a list of
///                                CSourcePhrase instances is supplied (this list can be
///                                of any length), then the renumbering happens in that
///                                list. When NULL, the app's m_pSourcePhrases list is used.
/// \remarks
/// Called from: the Doc's TokenizeText(), and ReconstituteAfterFilteringChange() and
/// other places - about two dozen in all so far.
/// Fixes the m_nSequNumber member of the source phrases in the current document
/// starting with nFirstSequNum in pList, so that all the remaining source phrases' sequence
/// numbers continue in numerical sequence (ascending order) with no gaps. 
/// This function differs from AdjustSequNumbers() in that AdjustSequNumbers() effects its 
/// changes only on the pList passed to the function; in UpdateSequNumbers() the current 
/// document's source phrases beginning with nFirstSequNum are set to numerical sequence 
/// through to the end of the document.
/// BEW changed 16Jul09 to have a second parameter which defaults to NULL, but otherwise is
/// a pointer to the SPList on which the updating is to be done. This allows the function
/// to be used on sublists which have to be processed by RecalcLayout(), as when doing a
/// range print - the refactored view requires that the partner piles be accessible by a
/// sequence number, and this will only work if the sublist of CSourcePhrase instances has
/// the element's m_nSequNumber values reset so as to be 0-based and numbered from the
/// first in the sublist. pOtherList can be set to any list, including of course, the
/// m_pSourcePhrases list, if the former contents of that list have been stored elsewhere
/// beforehand
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::UpdateSequNumbers(int nFirstSequNum, SPList* pOtherList)
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	SPList* pList;
	if (pOtherList == NULL)
		// use the normal list which defines the whole document
		pList = pApp->m_pSourcePhrases;
	else
        // use some other list, typically a sublist with fewer elements, that stores
        // temporary subset of (shallow) copies of the main list's CSourcePhrase instances
		pList = pOtherList;

	// get the first
	SPList::Node* pos = pList->Item(nFirstSequNum);
	wxASSERT(pos != NULL);
	CSourcePhrase* pSrcPhrase = (CSourcePhrase*)pos->GetData();
	pos = pos->GetNext();
	wxASSERT(pSrcPhrase); 
	pSrcPhrase->m_nSequNumber = nFirstSequNum;
	int index = nFirstSequNum;

	while (pos != 0)
	{
		pSrcPhrase = (CSourcePhrase*)pos->GetData();
		pos = pos->GetNext();
		wxASSERT(pSrcPhrase); 
		index++; // next sequence number
		pSrcPhrase->m_nSequNumber = index;
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		event	-> a wxCommandEvent associated with the wxID_NEW identifier
/// \remarks
/// Called from: the doc/view framework when File | New menu item is selected. In the wx
/// version this override just calls the App's OnFileNew() method.
/// BEW 24Aug10, removed the bool bUserSelectedFileNew member from the application, now
/// the view's OnCreate() call just checks for m_pKB == NULL and m_pGlossingKB == NULL as
/// an indicator that the view was clobbered, and it then recreates the in-memory KBs
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnFileNew(wxCommandEvent& event)
{
	// called when File | New menu item selected specifically by user
	// Note: The App's OnInit() skips this and calls pApp->OnFileNew directly
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	pApp->OnFileNew(event);
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the File Menu
///                        is about to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected,
/// and before the menu is displayed.
/// If Vertical Editing is in progress the Split Document menu item is disabled and this
/// handler returns immediately. Otherwise, it enables the Split Document command on the 
/// Tools menu if a document is open, unless the app is in Free Translation Mode.
/// BEW modified 13Nov09, don't allow user with read-only access to cause document
/// change of this kind on the remote user's machine
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateSplitDocument(wxUpdateUIEvent& event)
{
	if (gpApp->m_bReadOnlyAccess)
	{
		event.Enable(FALSE);
		return;
	}
	if (gbVerticalEditInProgress)
	{
		event.Enable(FALSE);
		return;
	}
	if (gpApp->m_bFreeTranslationMode)
	{
		event.Enable(FALSE);
		return;
	}
    // BEW modified 03Nov05: let it be enabled for a dirty doc, but check for dirty flag
    // set and if so do an automatic save before attempting the split
	if (gpApp->m_pKB != NULL && gpApp->m_pSourcePhrases->GetCount() > 0)
		event.Enable(TRUE);
	else
		event.Enable(FALSE);	
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the Tools Menu
///                        is about to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected,
/// and before the menu is displayed.
/// If Vertical Editing is in progress or if the app is in Free Translation mode, the 
/// Join Documents menu item is disabled and this handler returns immediately. It 
/// enables the Join Document command if a document is open, otherwise it
/// disables the command.
/// BEW added 13Nov09, don't allow local user with read-only access to a remote project
/// folder to make document changes of this kind on the remote machine
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateJoinDocuments(wxUpdateUIEvent& event)
{
	if (gpApp->m_bReadOnlyAccess)
	{
		event.Enable(FALSE);
		return;
	}
	if (gbVerticalEditInProgress)
	{
		event.Enable(FALSE);
		return;
	}
	if (gpApp->m_bFreeTranslationMode)
	{
		event.Enable(FALSE);
		return;
	}
	if (gpApp->m_pKB != NULL && gpApp->m_pSourcePhrases->GetCount() > 0)
		event.Enable(TRUE);
	else
		event.Enable(FALSE);	
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the Tools Menu
///                        is about to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected,
/// and before the menu is displayed.
/// Disables the Move Document command on the Tools menu and returns immediately if
/// vertical editing is in progress, or if the application is in Free Translation Mode.
/// This event handler enables the Move Document command if a document is open, otherwise
/// it disables the command.
/// BEW added 13Nov09, don't allow local user with read-only access to a remote project
/// folder to make document changes of this kind on the remote machine
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateMoveDocument(wxUpdateUIEvent& event)
{
	if (gpApp->m_bReadOnlyAccess)
	{
		event.Enable(FALSE);
		return;
	}
	if (gbVerticalEditInProgress)
	{
		event.Enable(FALSE);
		return;
	}
	if (gpApp->m_bFreeTranslationMode)
	{
		event.Enable(FALSE);
		return;
	}
	if (gpApp->m_pKB != NULL && gpApp->m_bBookMode && gpApp->m_nBookIndex != -1 
			&& !gpApp->m_bDisableBookMode)
		event.Enable(TRUE);
	else
		event.Enable(FALSE);	
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if there is a SF marker in the passed in markers string which for the 
///				SfmSet is determined to be a marker which should halt the scanning of successive 
///				pSrcPhase instances during doc rebuild for filtering out a marker and its content 
///				which are required to be filtered, FALSE otherwise.
/// \param		sfmSet	   -> indicates which SFM set is to be considered active for the 
///						      LookupSFM() call
/// \param		pSrcPhrase -> the CSourcePhrase instance whose m_markers member contains the
///						      stuff we are wanting to check out for a halt condition
/// \remarks
/// Called from: the Doc's ReconstituteAfterFilteringChange(). 
/// Implements the protocol for determining when to stop scanning in a variety of
/// situations, if a prior call to a different function did not halt scanning at the
/// previous CSourcePhrase instance because of a matching endmarker being there... 
///    - pSrcPhrase->m_markers might contain filtered material (this must stop scanning because
///      we can't embed filtered material),
///    - or pSrcPhrase may contain a marker which Adapt It should ignore (eg. most inline markers)
///       those don't halt scanning
///    - or an unknown marker - these always halt scanning, 
///    - or an embedded content marker within an \x (cross reference section) or \f (footnote) 
///         or \fe (endnote) section - these don't halt scanning,
///    - or an inLine == FALSE marker - these halt scanning.
///    Note: when examining m_markers, it is the last SF marker we are interested in -
///    that's the one that gives TextType property to the unfiltered content which follows
///    BEW 11Oct10, changes needed for support of docVersion 5
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsEndingSrcPhrase(enum SfmSet sfmSet, CSourcePhrase* pSrcPhrase)
{
	// check for m_filteredInfo content first - if there is any, then we must exclude the
	// passed in CSourcePhrase instance from being in the to-be-filtered span, returning TRUE
	if (!pSrcPhrase->GetFilteredInfo().IsEmpty())
	{
		// something is filtered here, so we must halt
		return TRUE;
	}
	// we'll allow a Note to be scanned over and lost in the filtering process, but not if
	// we've come to a free translation anchor, or where collected back translations are
	// stored, the latter two must halt scanning
	if (!pSrcPhrase->GetFreeTrans().IsEmpty() || !pSrcPhrase->GetCollectedBackTrans().IsEmpty())
	{
		return TRUE;
	}

	// now check out whatever might be in m_markers member
	wxString markers = pSrcPhrase->m_markers;
	int nFound = markers.Find(gSFescapechar);
	if (nFound == wxNOT_FOUND)
	{
		// no SF markers in m_markers, so there is no reason to halt for this pSrcPhrase
		return FALSE;
	}
	wxString wholeMkr = GetLastMarker(markers);
	wxASSERT(!wholeMkr.IsEmpty());
	// m_markers does not contain any endmarkers, so wholeMkr is a beginmarker. Remove
	// its backslash so we can look it up
	wxString bareMkr = wholeMkr.Mid(1);

	// do lookup of the marker
	SfmSet saveSet = gpApp->gCurrentSfmSet; // save current set temporarily, 
											// as sfmSet may be different
	gpApp->gCurrentSfmSet = sfmSet; // install the set to be used - as passed in
	USFMAnalysis* analysis = LookupSFM(bareMkr); // internally uses gUserSfmSet
	if (analysis == NULL)
	{
        // this must be an unknown marker - we deem these all inLine == FALSE, so this
        // indicates we are located at an ending sourcephrase
		gpApp->gCurrentSfmSet = saveSet; // restore earlier setting
		return TRUE;
	}
	else
	{
        // This is a known marker in the sfmSet marker set, so check it out (We don't check
        // for inline endmarkers like \f*, \fe* or \x* because if one of these occurs, and
        // it must halt scanning at the CSourcePhase which stores them - and that
        // CSourcePhrase instance is then WITHIN the span, but such a marker will occur on
        // the previous CSourcePhrase to the one passed in here. So IsEndingSrcPhrase() is
        // only used when a previous span-ending function call which just looks at
        // endmarkers has returned FALSE.)
		gpApp->gCurrentSfmSet = saveSet; // restore earlier setting
		//markers.ReleaseBuffer(); // whm moved to main block above
		if (analysis->inLine == FALSE)
		{
			// it's not an inLine marker, so it must end the filtering scan
			gpApp->gCurrentSfmSet = saveSet; // restore earlier setting
			return TRUE; 
		}
		else
		{
            // Inline markers \f, \fe or \x also are stored in m_markers, and if present
            // must cause a halt, so test for these too; but we keep parsing over any
            // embedded content markers in footnotes or endnotes, and in cross references,
            // such as: any of \xo, \xk, etc or \fr, \fk, \fv, fm, etc). The OR test's RHS
            // part is for testing for embedded content markers within an endnote - these
            // don't halt scanning either.
            wxString bareMkr = wholeMkr.Mid(1); // remove initial backslash
			wxString shortMkr = bareMkr.Left(1); // take only the first character
			if (((shortMkr == _T("f") || shortMkr == _T("x")) && shortMkr != bareMkr) ||
				((shortMkr == _T("f")) && (bareMkr != _T("fe"))))
			{
				// its an embedded content marker of a kind which does not halt scanning
				;
			}
			else
			{
				// it must halt scanning
				gpApp->gCurrentSfmSet = saveSet; // restore earlier setting
				return TRUE;
			}
		}
	}
	// tell the caller it's not the ending instance yet
	gpApp->gCurrentSfmSet = saveSet; // restore earlier setting
	return FALSE;
}

/* legacy code
///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if there is a SF marker in the passed in markers string which for the 
///				SfmSet is determined to be a marker which should halt the scanning of successive 
///				pSrcPhase instances during doc rebuild for filtering out a marker and its content 
///				which are required to be filtered, FALSE otherwise.
/// \param		sfmSet	-> indicates which SFM set is to be considered active for the 
///							LookupSFM() call
/// \param		markers	-> the pSrcPhrase->m_markers string from the sourcephrase being 
///							currently considered
/// \remarks
/// Called from: the Doc's ReconstituteAfterFilteringChange(). 
/// Implements the protocol for determining when to stop scanning in a variety of
/// situations, if a prior call to a different function did not halt scanning at the
/// previous CSourcePhrase instance... 
///    - the param, markers, might contain filtered material (this must stop scanning because
///      we can't embed filtered material),
///    - or it may contain a marker which Adapt It should ignore (eg. most inline markers, but
///      not those for footnote, endnote or crossReference), 
///    - or an unknown marker - these always halt scanning, 
///    - or an embedded content marker within an \x (cross reference section) or \f (footnote) 
///         or \fe (endnote) section - these don't halt scanning,
///    - or an inLine == FALSE marker - these halt scanning.
///    BEW 22Sep10, no changes needed for support of docVersion 5
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::IsEndingSrcPhrase(enum SfmSet sfmSet, wxString& markers)
{
	int nFound = markers.Find(gSFescapechar);
	if (nFound == -1)
		return FALSE;

	int len = markers.Length();
    // wx version note: Since we require a read-only buffer we use GetData which just
    // returns a const wxChar* to the data in the string.
	const wxChar* pBuff = markers.GetData();
	wxChar* pEnd;
	pEnd = (wxChar*)pBuff + len; // point at the ending null
	wxASSERT(*pEnd == _T('\0')); // whm added, not in MFC
	wxChar* pBufStart = (wxChar*)pBuff;
	wxChar* pFound = pBufStart + nFound; // point at the backslash of the marker
	wxASSERT(pFound < pEnd);

	wxString fltrMkr(filterMkr); // filterMkr is a literal string defined at 
								 // start of the doc's .cpp file
	int lenFilterMkr = fltrMkr.Length();

    // if the first marker in markers is \~FILTER then we must be at the end of the section
    // for filtering out because we can't nest filtered material - so check out this case
    // first
	if (wxStrncmp(pFound,filterMkr,lenFilterMkr) == 0)
	{
		// it's a \~FILTER marker
		return TRUE;
	}

    // if we've been accumulating filtered sections and filtering them out, we could get to
    // a situation where the marker we want to be testing is not the one first one found
    // (which could be an unfiltered endmarker from much earlier in the list of
    // sourcephrases), but to test the last unfiltered marker - so we must check for this,
    // and if so, make the marker we are examining be that last one) (The following block
    // should only be relevant in only highly unusual circumstances)
	wxString str2 = markers;
	str2 = MakeReverse(str2);
	int itemLen2 = 0;
	int lenRest = len; // initialize to whole length
	int nFound2 = str2.Find(gSFescapechar);
	if (nFound2 > 0)
	{
		// there's a marker
		nFound2++; // include the backslash in the leftmost part (of the reversed 
				   // markers string)
		lenRest -= nFound2; // lenRest will be the offset of this marker, if we 
                    // determine we need to use it, later when we reverse back to normal
                    // order
		wxString endStr = str2.Left(nFound2); // this could be something like 
											  // "\v 1 " reversed
		endStr = MakeReverse(endStr); // restore natural order
		int len = endStr.Length(); // whm added 18Jun06
        // wx version note: Since we require a read-only buffer we use GetData which just
        // returns a const wxChar* to the data in the string.
		const wxChar* pChar = endStr.GetData();
		wxChar* pCharStart = (wxChar*)pChar;
		wxChar* pEnd;
		pEnd = (wxChar*)pChar + len; // whm added 18Jun06
		wxASSERT(*pEnd == _T('\0')); // whm added 18Jun06
		itemLen2 = ParseMarker(pCharStart);
        // determine if the marker at the end is actually the one at pFound, if so, we can
        // exit this block and just go on using the one at pFound; if not, we have to check
        // the marker at the end is not a filter endmarker, and providing it's not, we
        // would use the one at the end in subsequent code following this block
		if (wxStrncmp(pFound,pCharStart,itemLen2 + 1) == 0) // include space after the marker 
															// in the comparison
		{
			// the two are the same marker so no adjustment is needed
			;
		}
		else
		{
			// they are different markers, so we have to make sure we didn't just locate 
			// a \~FILTER* marker
			wxString theLastMkr(pCharStart,itemLen2);
			if (theLastMkr.Find(fltrMkr) != -1)
			{
                // theLastMkr is either \~FILTER or \~FILTER*, so there is no marker at the
                // end of m_markers which we need to be considering instead, so no
                // adjustment is needed
				;
			}
			else
			{
                // this marker has to be used instead of the one at the offset passed in;
                // to effect the required change, all we need do is exit this block with
                // pFound pointing at the marker at the end
				pFound = pBufStart + lenRest;
				wxASSERT(*pFound == gSFescapechar);
			}
		}
	}

	// get the marker which is at the backslash
	int itemLen = ParseMarker(pFound);
	wxString mkr(pFound,itemLen); // this is the whole marker, including its backslash
	wxString bareMkr = mkr;
	bareMkr = MakeReverse(bareMkr);
    // we must allow that the mkr which we have found might be an endmarker, since we could
    // be parsing across embedded content material, such as a keyword designation ( which
    // has a \k followed later by \k*), or italics, bold, or similar type of thing - these
    // markers don't indicate that parsing should end, so we have to allow for endmarkers
    // to be encountered
	if (bareMkr[0] == _T('*'))
	{
		bareMkr = bareMkr.Mid(1); // remove final * off an endmarker
	}
	bareMkr = MakeReverse(bareMkr); // restore normal order
	bareMkr = bareMkr.Mid(1); // remove initial backslash, so we can use it for Lookup

	// do lookup
	SfmSet saveSet = gpApp->gCurrentSfmSet; // save current set temporarily, 
											// as sfmSet may be different
	gpApp->gCurrentSfmSet = sfmSet; // install the set to be used - as passed in
	USFMAnalysis* analysis = LookupSFM(bareMkr); // internally uses gUserSfmSet
	if (analysis == NULL)
	{
        // this must be an unknown marker - we deem these all inLine == FALSE, so this
        // indicates we are located at an ending sourcephrase
		gpApp->gCurrentSfmSet = saveSet; // restore earlier setting
		//markers.ReleaseBuffer(); // whm moved to main block above
		return TRUE;
	}
	else
	{
		// this is a known marker in the sfmSet marker set, so check out inLine and
		// TextType 
		gpApp->gCurrentSfmSet = saveSet; // restore earlier setting
		//markers.ReleaseBuffer(); // whm moved to main block above
		if (analysis->inLine == FALSE)
			return TRUE; // it's not an inLine marker, 
						 // so it must end the filtering scan
		else
		{
			// its an inLine == TRUE marker, so we have to check if TextType == none
			if (analysis->textType == none)
			{
				// it's not the kind of marker that halts scanning
				return FALSE;
			}
			else
			{
                // it's something other than type none, so it must halt scanning - except
                // if the marker is one of the footnote or cross reference embedded content
                // markers - so check out those possibilities (note, in the compound test
                // below, if the last test were absent, then \f or \x would each satisfy
                // one of the first two tests and there would be no halt - but these, if
                // encountered, must halt the parse and so the test checks for shortMkr
                // being different than bareMkr (FALSE for \x or \f, but TRUE for any of
                // \xo, \xk, etc or \fr, \fk, \fv, fm, etc)) The OR test's RHS part is for
                // testing for embedded content markers within an endnote - these don't
                // halt scanning either.
				wxString shortMkr = bareMkr.Left(1);
				if (((shortMkr == _T("f") || shortMkr == _T("x")) && shortMkr != bareMkr) ||
					((shortMkr == _T("f")) && (bareMkr != _T("fe"))))
				{
					// its an embedded content marker of a kind which does 
					// not halt scanning
					return FALSE;
				}
				else
				{
					// it must halt scanning
					return TRUE;
				}
			}
		}
	}
}
*/

///////////////////////////////////////////////////////////////////////////////
/// \return		offset of the marker which is to be filtered (ie. offset of its backslash) if markers 
///				is a non-empty string containing a SFM which is designated as to be filtered (by being 
///				listed in filterList string) and it is not preceded by \~FILTER; otherwise it returns -1.
/// \param		sfmSet			-> indicates which SFM set is to be considered active for the 
///									LookupSFM() call
/// \param		markers			-> the pSrcPhrase->m_markers string from the sourcephrase being 
///									currently considered
/// \param		filterList		-> the list of markers to be filtered out, space delimited, including 
///									backslashes... (the list might be just those formerly unfiltered 
///									and now designated to be filtered, or in another context (such as 
///									changing the SFM set) it might be the full set of markers designated 
///									for filtering - which is the case is determined by the caller)
/// \param		wholeMkr		<- the SFM, including backslash, found to be designated for filtering out
/// \param		wholeShortMkr	<- the backslash and first character of wholeMkr (useful, when wholeMkr 
///									is \x or \f)
/// \param		endMkr			<- the endmarker for wholeMkr, or an empty string if it does not potentially 
///									take an endmarker
/// \param		bHasEndmarker	<- TRUE when wholeMkr potentially takes an endmarker (determined by Lookup())
/// \param		bUnknownMarker	<- TRUE if Lookup() determines the SFM does not exist in the sfmSet marker set
/// \param		startAt			-> the starting offset in the markers string at which the scanning is to be 
///									commenced
/// \remarks
/// Called from: the Doc's ReconstituteAfterFilteringChange().
/// Determines if a source phrase's m_markers member contains a marker to be filtered, and
/// if so, returns the offset into m_markers where the marker resides.
/// BEW 21Sep10, updated for docVersion 5
///////////////////////////////////////////////////////////////////////////////
int CAdapt_ItDoc::ContainsMarkerToBeFiltered(enum SfmSet sfmSet, wxString markers, 
				wxString filterList, wxString& wholeMkr, wxString& wholeShortMkr, 
				wxString& endMkr, bool& bHasEndmarker, bool& bUnknownMarker, 
				int startAt)
{
	int offset = startAt;
	if (markers.IsEmpty())
		return wxNOT_FOUND;
	bHasEndmarker = FALSE; // default
	bUnknownMarker = FALSE; // default
	int len = markers.Length();
    // wx version note: Since we require a read-only buffer we use GetData which just
    // returns a const wxChar* to the data in the string.
	const wxChar* pBuff = markers.GetData();
	wxChar* pEnd;
	pEnd = (wxChar*)pBuff + len; // point at the ending null
	wxASSERT(*pEnd == _T('\0')); // whm added 18Jun06, not in MFC
	wxChar* pBufStart = (wxChar*)pBuff;
	wxChar* ptr = pBufStart;
	wxChar* pFound = pBufStart; // pointer returned from _tcsstr() 
								// (ie. strstr() or wcsstr()), null if unfound
	pFound += offset; // get the scan start location (offset will be reused below, 
                      // so don't rely on it staying this present value)
	wxChar backslash[2] = {gSFescapechar,_T('\0')};
	int itemLen;

    // scan the buffer, looking for a filterable marker
	while ((pFound = wxStrstr(pFound, backslash)) != NULL)
	{	
		// we have come to a backslash
		ptr = pFound;
		itemLen = ParseMarker(ptr);
		wxString mkr(ptr,itemLen); // this is the whole marker, 
								   // including its backslash
		
        // it's potentially a marker which might be one for filtering out
		wxString mkrPlusSpace = mkr + _T(' '); // prevent spurious matches,
											   // eg. \h finding \hr
		int nFound = filterList.Find(mkrPlusSpace);
		if (nFound == wxNOT_FOUND)
		{
			// it's not in the list of markers designated as to be filtered, 
			// so keep iterating
			pFound++;
			continue;
		}
		else
		{
			// this marker is to be filtered, so set up the parameters to 
			// be returned etc
			offset = pFound - pBuff;
			wxASSERT(offset >= 0);
			wholeMkr = mkr;
			wholeShortMkr = wholeMkr.Left(2);

			// get its SFM characteristics, whether it has an endmarker, 
			// and whether it is unknown
			SfmSet saveSet = gpApp->gCurrentSfmSet; // save current set 
								// temporarily, as sfmSet may be different
			gpApp->gCurrentSfmSet = sfmSet; // install the set to be used 
											// - as passed in
			wxString bareMkr = wholeMkr.Mid(1); // lop off the backslash
			USFMAnalysis* analysis = LookupSFM(bareMkr); // uses gCurrentSfmSet
			if (analysis == NULL)
			{
				// this must be an unknown marker designated for filtering by 
				// the user in the GUI
				bUnknownMarker = TRUE;
				bHasEndmarker = FALSE; // unknown markers NEVER have endmarkers
				endMkr.Empty();
				gpApp->gCurrentSfmSet = saveSet; // restore earlier setting
				return offset; // return its offset
			}
			else
			{
				// this is a known marker in the sfmSet marker set
				endMkr = analysis->endMarker;
				bHasEndmarker = !endMkr.IsEmpty();
				if (bHasEndmarker)
				{
					endMkr = gSFescapechar + endMkr; // add the initial backslash
				}
				gpApp->gCurrentSfmSet = saveSet; // restore earlier setting
				if (analysis->filter)
					return offset; // it's filterable, so return its offset
				else
                    // it's either not filterable, or we've forgotten to update the
                    // filter member of the USFMAnalysis structs prior to calling
                    // this function
					return wxNOT_FOUND;
			}
		}
	}
	// didn't find a filterable marker that was not already filtered
	wholeShortMkr.Empty();
	wholeMkr.Empty();
	endMkr.Empty();
	return wxNOT_FOUND;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a copy of the wxString which is the appropriate nav text message 
///             for the passed in CSourcePhrase instance (typically returned to the
///             m_inform member in the caller)
/// \param		pSrcPhrase	-> the CSourcePhrase instance which is to have its 
///                            m_inform member reconstructed, modified, partly or wholely,
///                            by the set of markers changed by the user in the GUI)
/// \remarks
/// Called from: the App's AddBookIDToDoc(), the Doc's ReconstituteAfterFilteringChange(),
/// the View's OnRemoveFreeTranslationButton().
/// Re-composes the navigation text that is stored in a source phrase's m_inform member. 
/// The idea behind this function is to get the appropriate m_inform text redone when
/// rebuilding the document - as when filtering changes are made, or a change of SFM set
/// which has the side effect of altering filtering settings as well, or the insertion of a
/// sourcephrase with an \id in m_markers and a 3-letter book ID code in the m_key member.
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::RedoNavigationText(CSourcePhrase* pSrcPhrase)
{
	wxString strInform; 
	strInform.Empty();

	// get the current m_markers contents
	wxString markersStr = pSrcPhrase->m_markers;
	if (markersStr.IsEmpty() || (markersStr.Find(gSFescapechar) == -1))
		return strInform;

	// there is something more to be handled
	wxString f(filterMkr);
	//int fltrMkrLen = f.Length(); // unused
	int markersStrLen = markersStr.Length() + 1; // allow for null at end
	const wxChar* ptr = NULL;
	int mkrLen = 0;
	int curPos = 0;
	bool bFILTERwasLastMkr = FALSE;
	while ((curPos = FindFromPos(markersStr,gSFescapechar,curPos)) != -1)
	{
        // wx version note: Since we require a read-only buffer we use GetData which just
        // returns a const wxChar* to the data in the string.
		ptr = markersStr.GetData();
		wxChar* pEnd;
		pEnd = (wxChar*)ptr + markersStrLen - 1; // whm added -1 compensates for 
												 // increment of markerStrLen above
		wxASSERT(*pEnd == _T('\0')); // whm added 18Jun96
		wxChar* pBufStart = (wxChar*)ptr;
		mkrLen = ParseMarker(pBufStart + curPos);
		wxASSERT(mkrLen > 0);
		wxString bareMkr(ptr + curPos + 1, mkrLen - 1); // no gSFescapechar 
														// (but may be an endmarker)
		curPos++; // increment, so the loop's .Find() test will advance curPos 
				  // to the next marker
		wxString nakedMkr = bareMkr;
		nakedMkr = MakeReverse(nakedMkr);
		bool bEndMarker = FALSE;
		if (nakedMkr[0] == _T('*'))
		{
			// it's an endmarker
			nakedMkr = nakedMkr.Mid(1);
			bEndMarker = TRUE;
		}
		nakedMkr = MakeReverse(nakedMkr);
		wxString wholeMkr = gSFescapechar;
		wholeMkr += bareMkr;
		if (wholeMkr == filterMkr)
		{
			bFILTERwasLastMkr = TRUE; // suppresses forming nav text for 
									// the next marker (coz it's filtered)
			continue; // skip \~FILTER
		}
		if (wholeMkr == filterMkrEnd)
		{
			bFILTERwasLastMkr = FALSE; // this will re-enable possible forming 
									   // of nav text for the next marker
			continue; // skip \~FILTER*
		}
		
		// we only show navText for markers which are not endmarkers, and not filtered
		if (bFILTERwasLastMkr)
			continue; // this marker was preceded by \~FILTER, 
					  // so it must not have nav text formed
		if (!bEndMarker)
		{
			USFMAnalysis* pAnalysis = LookupSFM(nakedMkr);
			wxString navtextStr;
			if (pAnalysis)
			{
				// the marker was found, so get the stored navText
				bool bInform = pAnalysis->inform;
				// only those markers whose inform member is TRUE are to be used
				// for forming navText
				if (bInform)
				{
					navtextStr = pAnalysis->navigationText;
					if (!navtextStr.IsEmpty())
					{
//a:						
						if (strInform.IsEmpty())
						{
							strInform = navtextStr;
						}
						else
						{
							strInform += _T(' ');
							strInform += navtextStr;
						}
					}
				}
			}
			else
			{
                // whm Note 11Jun05: I assume that an unknown marker should not appear in
                // the navigation text line if it is filtered. I've also modified the code
                // in AnalyseMarker() to not include the unknown marker in m_inform when
                // the unknown marker is filtered there, and it seems that would be
                // appropriate here too. If Bruce thinks the conditional call to
                // IsAFilteringUnknownSFM is not appropriate here the conditional I've
                // added should be removed; likewise the parallel code I've added near the
                // end of AnalyseMarker should be evaluated for appropriateness. 
                // ( <--Bill's addition is fine, BEW 15Jun05)
				if (!IsAFilteringUnknownSFM(nakedMkr))
				{
					// the marker was not found, so form a ?mkr? string instead
					navtextStr = _T("?");
					navtextStr += gSFescapechar; // whm added 10Jun05
					navtextStr += nakedMkr;
                    // BEW commented out next line. It fails for a naked marker such as
                    // lb00296b.jpg which appeared in Bob Eaton's data as a picture
                    // filename; what happens is that ? followed by space does not get
                    // appended, but the IDE shows the result as "?\lb00296b.jpg" and in
                    // actual fact the navtextStr buffer contains that plus a whole long
                    // list of dozens of arbitrary memory characters (garbage) which
                    // subsequently gets written out as navText - Ugh! So I've had to place
                    // the required characters in the buffer explicitly.
					navtextStr += _T("? ");
				}
				// code block at goto a; copied down here to get rid of goto a; and label 
                // (and gcc warning)
				if (strInform.IsEmpty())
				{
					strInform = navtextStr;
				}
				else
				{
					strInform += _T(' ');
					strInform += navtextStr;
				}
			}
		} // end block for !bEndMarker

	} // end loop for searching for all markers to be used for navText
	return strInform;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		pList	<- the list of source phrases
/// \remarks
/// Called from: the Doc's ReconstituteAfterFilteringChange() and
/// ReconstituteAfterPunctuationChange().
/// Deletes the source phrase instances held in pList, but retains the empty list. It is
/// called during the rebuilding of the document after a filtering or punctuation change.
/// BEW added 23May09: since it is called only from the above two functions, and both of
/// those when they return to the caller will have the view updated by a RecalcLayout()
/// call with the enum parameter valoue of create_piles_create_strips, we do not need to
/// have DeletePartnerPile() called when each of the DeleteSingleSrcPhrase(() calls are
/// made in the loop below; therefore we specify the FALSE parameter for the call of the
/// latter function in order to suppress deletion of the partner pile (as RecalcLayout()
/// will do it much more efficiently, en masse, and speedily).
/// 
/// Note: if ever we use this function elsewhere, and need the partner pile deletion to
/// work there, then we will need to alter the signature to become:
/// (SPList*& pList, bool bDoPartnerPileDeletionAlso = TRUE) instead
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::DeleteListContentsOnly(SPList*& pList)
{
	// DeleteListContentsOnly is a useful utility in the rebuilding of the doc
	SPList::Node* pos = pList->GetFirst();
	CSourcePhrase* pSrcPh;
	while (pos != NULL)
	{
		pSrcPh = (CSourcePhrase*)pos->GetData();
		pos = pos->GetNext();
		DeleteSingleSrcPhrase(pSrcPh,FALSE); // no need to bother to delete the 
											 // partner pile
	}
	pList->Clear();
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a wxString with any filtered brackets removed from around its filtered 
///				information
/// \param		str		-> a wxString containing filtered information enclosed within 
///							\~FILTER ... \~FILTER* filtering brackets
/// \remarks
/// Called from: the Doc's ReconstituteAfterFilteringChange(), GetUnknownMarkersFromDoc(),
/// the View's RebuildSourceText(), DoPlacementOfMarkersInRetranslation(), 
/// RebuildTargetText(), DoExportInterlinearRTF(), GetMarkerInventoryFromCurrentDoc(),
/// and CViewFilteredMaterialDlg::InitDialog().
/// Removes any \~FILTER and \~FILTER* brackets from a string. The information the was
/// previously bracketed by these markers remains intact within the string.
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::RemoveAnyFilterBracketsFromString(wxString str) // whm added 6Apr05
{
    // ammended 29Apr05 to remove multiple sets of filter brackets from str. Previously the
    // function only removed the first set of brackets found in the str.
    // whm revised 2Oct05 to more properly handle the deletion of spaces upon/after the
    // removal of the filter brackets
	int mkrPos = str.Find(filterMkr);
	int endMkrPos = str.Find(filterMkrEnd);
	while (mkrPos != -1 && endMkrPos != -1 && endMkrPos > mkrPos)
	{
		str.Remove(endMkrPos, wxStrlen_(filterMkrEnd));
        // after deleting the end marker, endMkrPos will normally point to a following
        // space whenever the filtered material is medial to the string. In such cases we
        // want to also eliminate the following space. The only time there may not be a
        // space following the end marker is when the filtered material was the last item
        // in m_markers. In this case endMkrPos would no longer index a character within
        // the string after the deletion of the end marker.
		if (endMkrPos < (int)str.Length() && str.GetChar(endMkrPos) == _T(' '))
			str.Remove(endMkrPos,1);
		str.Remove(mkrPos, wxStrlen_(filterMkr));
		// after deleting the beginning marker, mkrPos should point to the space that
		// followed the beginning filter bracket marker - at least for well formed 
		// filtered material. Before deleting that space, however, we check to insure
		// it is indeed a space.
		if (str.GetChar(mkrPos) == _T(' '))
			str.Remove(mkrPos,1);
		// set positions for any remaining filter brackets in str
		mkrPos = str.Find(filterMkr);
		endMkrPos = str.Find(filterMkrEnd);
	}
	// change any left-over multiple spaces to single spaces
	int dblSpPos = str.Find(_T("  "));
	while (dblSpPos != -1)
	{
		str.Remove(dblSpPos, 1);
		dblSpPos = str.Find(_T("  "));
	}
	return str;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a wxString which is a copy of the filtered SFM or USFM next found  composed of
///				\~FILTER ... \~FILTER* plus the marker and associated string content in between,
///				or an empty string if one is not found
/// \param		markers	-> references the content of a CSourcePhrase instance's m_markers member
/// \param		offset	-> character offset to where to commence the Find operation for the
///						   \~FILTER marker
/// \param		nStart	<- offset (from beginning of markers) to the next \~FILTER instance found, 
///						   if none is found it is set to the offset value (so a Left() call can 
///						   extract any preceding non-filtered text stored in markers)
/// \param		nEnd	<- offset (ditto) to the first character past the matching \~FILTER* 
///						   (& space) instance, if none is found it is set to the offset value 
///						   (so a Mid() call can get the remainder)
/// \remarks
/// Called from: the Doc's ReconstituteAfterFilteringChange().
/// Copies from a m_markers string the whole string representing filtered information,
/// i.e., the filtering brackets \~FILTER ... \~FILTER* plus the marker and associated
/// string content in between. Offsets are returned in nBegin and nEnd that enable the
/// function to be called repeatedly within a while loop until no additional filtered
/// material is found.
/// The nStart and nEnd values can be used by the caller, along with Mid(), to extract the
/// filtered substring (including bracketing FILTER markers); nStart, along with Left() can
/// be used by the caller to extract whatever text precedes the \~FILTER instance (eg. to
/// store it in another CSourcePhrase instance's m_markers member), and nEnd, along with
/// Mid(), can be used by the caller to extract the remainder (if any) - which could be
/// useful in the caller when unfiltering in order to update m_markers after unfiltering
/// has ceased for the current CSourcePhrase instance. (To interpret the nStart and nEnd
/// values correctly in the caller upon return, the returned CString should be checked to
/// determine if it is empty or not.) The caller also can use the nEnd value to compute an
/// updated offset value for iterating the function call to find the next filtered marker.
/// BEw 20Sep10, updated for docVersion 5
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::GetNextFilteredMarker(wxString& markers, int offset, 
											 int& nStart, int& nEnd)
{
	wxString mkrStr;
	mkrStr.Empty();
	if (offset < 0 || offset > 10000)
		offset = 0; // assume it's a bad offset value, so start from zero

	// find the next \~FILTER marker starting from the offset (there may be no more)
	nStart = offset; // default 'not found' value
	nEnd = offset; // ditto
	int nFound = FindFromPos(markers,filterMkr,offset);
	if (nFound == -1)
	{
		// no \~FILTER marker was found at offset or beyond
		return mkrStr;
	}
	else
	{
		// we found one, so get the metrics calculated and the marker parsed
		nStart = nFound;
		wxString theRest = markers.Mid(nStart); // \~FILTER\marker followed 
												// by its content etc.
		wxString f(filterMkr); // so I can get a length calculation done 
							   // (this may be localized later)
		int len = f.Length() + 1; // + 1 because \~FILTER is always 
								  // followed by a space
		theRest = theRest.Mid(len); // \marker followed by its content etc.
		int len2 = theRest.Length();
        // wx version note: Since we require a read-only buffer we use GetData which just
        // returns a const wxChar* to the data in the string.
		const wxChar* ptr = theRest.GetData();
		wxChar* pEnd;
		pEnd = (wxChar*)ptr + len2; // whm added 19Jun06
		wxChar* pBufStart = (wxChar*)ptr;
		wxASSERT(*pEnd == _T('\0'));// whm added 19Jun06
		int len3 = ParseMarker(pBufStart);
		mkrStr = theRest.Left(len3);
		wxASSERT(mkrStr[0] == gSFescapechar); // make sure we got a valid marker

        // now find the end of the matching \~FILTER* endmarker - we must find this for the
        // function to succeeed; if we don't, then we must throw away the mkrStr value (ie.
        // empty it) and revert to the nStart and nEnd values for a failure
		int nStartFrom = nStart + len + len3; // offset to character after the marker 
											  // in mkrStr (a space)
		nFound = FindFromPos(markers,filterMkrEnd,nStartFrom);
		if (nFound == wxNOT_FOUND)
		{
			// no matching \~FILTER* marker; this is an error condition, so bail out
			nStart = nEnd = offset;
			mkrStr.Empty();
			return mkrStr;
		}
		else
		{
            // we found the matching filter marker which brackets the end, so get offset to
            // first character beyond it (?? and its following space?? see below) & we are
            // done...
            // 
			// BEW 20Sep10: in docVersion 5, not only do we store filtered stuff in
			// m_filteredInfo, but we but filter markers up against each other, so we can get
			// a sequence "\~FILTER*\~FILTER " and so we can't assume a space between these,
			// and indeed, we no longer even try to put one there; so only count a space
			// after \~FILTER* if there is indeed one there
			wxString fEnd(filterMkrEnd);
			len3 = fEnd.Length();
			nEnd = nFound + len3;
			if (markers[nEnd] == _T(' '))
				nEnd++; // count the following space too
		}
	}
	return mkrStr;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param	oldSet				-> indicates which SFM set was active before the user changed 
///									to a different one
/// \param	newSet				-> indicates which SFM set the user changed to
/// \param	oldFilterMarkers	-> the list of markers that was filtered out, space delimited, 
///									including backslashes, which were in effect when the oldSet 
///									was the current set,
/// \param	newFilterMarkers	-> the list of markers to be filtered out, along with their 
///									content, now that the newSet has become current
/// \param	secondPassFilterMkrs <- a list of the markers left after common ones have been 
///									removed from the newFilterMarkers string -- these markers 
///									are used on the second pass which does the filtering
/// \param	pass				-> an enum value, which can be first_pass or second_pass; the 
///									first pass through the document does unfiltering within 
///									the context of the oldSet, and the second pass does 
///									filtering within the context of the newSet. 
/// \remarks
/// Called from: the Doc's RetokenizeText().
/// Sets up the document's data structures for the two pass reconstitution that must be
/// done when there has been a SFM set change. The function gets all the required data
/// structures set up appropriately for whichever pass is about to be effected (The caller
/// must have saved the current SFM set gCurrentSfmSet before this function is called for
/// the first time, and it must restore it after this function is called the second time
/// and the changes effected.)
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::SetupForSFMSetChange(enum SfmSet oldSet, enum SfmSet newSet, 
					wxString oldFilterMarkers, wxString newFilterMarkers, 
					wxString& secondPassFilterMkrs, enum WhichPass pass)
{
    // get a MapWholeMkrToFilterStatus map filled first with the set of original filter
    // markers, then on the second pass to be filled with the new filter markers (minus
    // those in common which were removed on first pass)
	wxString mkr;
	MapWholeMkrToFilterStatus mkrMap;
	MapWholeMkrToFilterStatus* pMap = &mkrMap;

	if (pass == first_pass)
	{
		// fill the map
		GetMarkerMapFromString(pMap, oldFilterMarkers);

#ifdef _Trace_RebuildDoc
		TRACE1("first_pass    local mkrMap size = %d\n", pMap->GetSize());
#endif

        // iterate, getting each marker, adding a trailing space, and removing same from
        // each of oldFilterMarkers and newFilterMarkers if in each, but making no change
        // if not in each
		gpApp->m_FilterStatusMap.clear();
		MapWholeMkrToFilterStatus::iterator iter;
		for (iter = pMap->begin(); iter != pMap->end(); ++iter) 
		{
			mkr = iter->first;
			wxString mkrPlusSpace = mkr + _T(' ');
			bool bRemoved = RemoveMarkerFromBoth(mkrPlusSpace, oldFilterMarkers, 
												newFilterMarkers);

#ifdef _Trace_RebuildDoc
			TRACE2("first_pass    bRemoved = %d, mkr = %s\n", (int)bRemoved, mkr);
#endif

            // if it did not get removed from both, then we will need to unfilter this mkr
            // in the first pass - so put it in m_FilterStatusMap with a "0" string value
            // associated with the marker as the key, ready for the later
            // ReconstituteAfterFilteringChange() call in the caller
			if (!bRemoved)
			{
				(gpApp->m_FilterStatusMap)[mkr] = _T("0"); // "0" means 
											// "was filtered, but now to be unfiltered"
			}
		}

        // set secondPassFilterMkrs to whatever is left after common ones have been removed
        // from the newFilterMarkers string -- these will be used on the second pass which
        // does the filtering
		secondPassFilterMkrs = newFilterMarkers;

		// set the old SFM set to be the current one; setup is complete for the first pass 
        // through the doc
		gpApp->gCurrentSfmSet = oldSet;

#ifdef _Trace_RebuildDoc
		TRACE2("first_pass    gCurrentSfmSet = %d,\n\n Second pass marker set: =  %s\n", (int)oldSet, secondPassFilterMkrs);
#endif
	}
	else // this is the second_pass
	{
        // on the second pass, the set of markers which have to be filtered out are passed
        // in in the string secondPassFilterMkrs (computed on the previous call to
        // SetupForSFMSetChange()), so all we need to do here is set up m_FilterStatusMap
        // again, with the appropriate markers and an associated value of "1" for each,
        // ready for the ReconstituteAfterFilteringChange() call in the caller
		if (secondPassFilterMkrs.IsEmpty())
		{
			gpApp->m_FilterStatusMap.clear();
			goto h;
		}

		// fill the local map, then iterate through it filling m_FilterStatusMap
		GetMarkerMapFromString(pMap, secondPassFilterMkrs);

		// set up m_FilterStatusMap again
		gpApp->m_FilterStatusMap.clear();
		
		{	// this extra block extends for the next 28 lines. It avoids the bogus 
			// warning C4533: initialization of 'f_iter' is skipped by 'goto h'
			// by putting its code within its own scope
		MapWholeMkrToFilterStatus::iterator iter;
		for (iter = pMap->begin(); iter != pMap->end(); ++iter)
		{
			(gpApp->m_FilterStatusMap)[mkr] = _T("1"); // "1" means 
									// "was unfiltered, but now to be filtered"

#ifdef _Trace_RebuildDoc
			TRACE1("second_pass    m_FilterStatusMap.SetAt() =   %s    = \"1\"\n", mkr);
#endif
		}
		} // end of extra block

		// set the new SFM set to be the current one; second pass setup is now complete
h:		gpApp->gCurrentSfmSet = newSet; // caller has done it already, or should have

#ifdef _Trace_RebuildDoc
		TRACE1("second_pass    gCurrentSfmSet = %d    (0 is UsfmOnly, 1 is PngOnly)\n", (int)newSet);
#endif
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		pMkrMap		<- pointer to a map of wholeMkr (including backslash) strings 
///								derived from the str parameter, with the wholeMkr as key, 
///								and value being an empty string (we don't care a hoot about
///								the value, we just want the benefits of the hashing on the key)
/// \param		str			-> a sequence of whole markers (including backslashes), with a 
///								space following each, but there should not be any endmarkers 
///								in str, but we'll make them begin markers if there are
/// \remarks
/// Called from: the Doc's SetupForSFMSetChange().
/// Populates a MapWholeMkrToFilterStatus wxHashMap with bare markers. No mapping associations are
/// made from this function - all are simply associated with a null string.
/// Extracts each whole marker, removes the backslash, gets rid of any final * if an endmarkers somehow
/// crept in unnoticed, and if unique, stores the bareMkr in the map; if the input string str is empty,
/// it exits without doing anything.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::GetMarkerMapFromString(MapWholeMkrToFilterStatus*& pMkrMap, 
										  wxString str) // BEW added 06Jun05
{
	wxChar nix[] = _T(""); // an arbitrary value, we don't care what it is
	wxString wholeMkr = str;

	// get the first marker
	wxStringTokenizer tkz(wholeMkr); // use default " " whitespace here

	while (tkz.HasMoreTokens())
	{
		wholeMkr = tkz.GetNextToken();
		// remove final *, if it has one (which it shouldn't)
		wholeMkr = MakeReverse(wholeMkr);
		if (wholeMkr[0] == _T('*'))
			wholeMkr = wholeMkr.Mid(1);
		wholeMkr = MakeReverse(wholeMkr);

		// put it into the map
		(*pMkrMap)[wholeMkr] = nix;
	};
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if the passed in wholeMarker and trailing space were removed from both 
///				str1 and str2, FALSE otherwise
/// \param		mkr		-> the wholeMarker (includes backslash, but no * at end) plus a trailing space
/// \param		str1	<- a set of filter markers (wholeMarkers, with a single space following each)
/// \param		str2	<- another set of filter markers (wholeMarkers, with a single space following each)
/// \remarks
/// Called from: the Doc's SetupForSFMSetChange().
/// Removes any markers (and trailing space) from str1 and from str2 which are common to
/// both strings. Used in the filtering stage of changing from an earlier SFM set to a
/// different SFM set - we wish use RemoveMarkerFromBoth in order to remove from contention
/// any markers and their content which were previously filtered and are to remain filtered
/// when the change to the different SFM set has been effected (since these are already
/// filtered, we leave them that way)
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::RemoveMarkerFromBoth(wxString& mkr, wxString& str1, wxString& str2)
{
	int curPos1 = str1.Find(mkr);
	wxASSERT(curPos1 >= 0); // mkr MUST be in str1, since the set of mkr strings 
							// was obtained from str1 earlier
	int curPos2 = str2.Find(mkr);
	if (curPos2 != -1)
	{
		// mkr is in both strings, so remove it from both
		int len = mkr.Length();
		str1.Remove(curPos1,len);
		str2.Remove(curPos2,len);
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		dest	<- the source phrase whose flags are made to agree with src's flags
/// \param		src		-> the source phrase whose flags are copied to dest's flags
/// \remarks
/// Called from: the Doc's ReconstituteOneAfterPunctuationChange().
/// Copies the boolean values from src source phrase to the dest source phrase. The flags
/// copied are: m_bFirstOfType, m_bFootnoteEnd, m_bFootnote, m_bChapter, m_bVerse,
/// m_bParagraph, m_bSpecialText, m_bBoundary, m_bHasInternalMarkers, m_bHasInternalPunct,
/// m_bRetranslation, m_bNotInKB, m_bNullSourcePhrase, m_bHasKBEntry,
/// m_bBeginRetranslation, m_bEndRetranslation, and m_bHasGlossingKBEntry.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::CopyFlags(CSourcePhrase* dest, CSourcePhrase* src)
{
	// BEW added on 5April05, to simplify copying of CSourcePhrase flag values
	dest->m_bFirstOfType = src->m_bFirstOfType;
	dest->m_bFootnoteEnd = src->m_bFootnoteEnd;
	dest->m_bFootnote = src->m_bFootnote;
	dest->m_bChapter = src->m_bChapter;
	dest->m_bVerse = src->m_bVerse;
	// BEW 8Oct10, repurposed m_bParagraph as m_bUnused
	//dest->m_bParagraph = src->m_bParagraph;
	dest->m_bUnused = src->m_bUnused;
	dest->m_bSpecialText = src->m_bSpecialText;
	dest->m_bBoundary = src->m_bBoundary;
	dest->m_bHasInternalMarkers = src->m_bHasInternalMarkers;
	dest->m_bHasInternalPunct = src->m_bHasInternalPunct;
	dest->m_bRetranslation = src->m_bRetranslation;
	dest->m_bNotInKB = src->m_bNotInKB;
	dest->m_bNullSourcePhrase= src->m_bNullSourcePhrase;
	dest->m_bHasKBEntry = src->m_bHasKBEntry;
	dest->m_bBeginRetranslation = src->m_bBeginRetranslation;
	dest->m_bEndRetranslation = src->m_bEndRetranslation;
	dest->m_bHasGlossingKBEntry = src->m_bHasGlossingKBEntry;
}

// Returns the target text adaptation string resulting from a reparse of the m_targetStr
// member after punctuation settings were changed (the target language punctuation
// settings may have be changed or not, we don't care which is the case); the first
// parameter is the the original m_targetStr on the currently being accessed CSourcePhrase
// instance, and we reparse with target punctuation settings - so that we get the
// unpunctuated string we want in the m_key member of the instance() in pTempList which we use
// internally. We pass in the caller's CSourcePhrase instance's m_nSequNumber value in
// param 2, only in order that the reparse doesn't try deal with a value of -1; but the
// param 2 value actually makes no difference to what is computed for returning as the
// adaptation without any punctuation
wxString CAdapt_ItDoc::MakeAdaptionAfterPunctuationChange(wxString& targetStrWithPunctuation, 
														  int startingSequNum)
{
	wxString str = targetStrWithPunctuation;
	bool bHasFixedSpaceMkr = FALSE;
	bHasFixedSpaceMkr = IsFixedSpaceSymbolWithin(str);
	wxString adaption; adaption.Empty();
	SPList* pTempList = new SPList;
	CSourcePhrase* pSPhr = NULL;
	// TRUE in next call is bool bUseTargetTextPuncts, this call is an overload of the
	// legacy TokenizeTargetTextString() function
	int elementCount = gpApp->GetView()->TokenizeTargetTextString(pTempList, str, startingSequNum, TRUE);
	elementCount = elementCount; // avoid compiler warning
	wxASSERT(elementCount > 0); // there should be at least one CSourcePhrase instance in pTempList
	SPList::Node* pos = pTempList->GetFirst();
	// what we do depends on whether a fixed-space conjoining is present or not
	if (bHasFixedSpaceMkr)
	{
		// we assume ~ only conjoins a pair of words, not a series of three or more; and
		// pTempList will then contain only one CSourcePhrase instance which is a
		// pseudo-merger of the two conjoined parts, so m_nSrcWords == 2
		SPList::Node* pos2 = pTempList->GetFirst();
		CSourcePhrase* pSP = pos2->GetData();
		wxASSERT(pSP->m_nSrcWords == 2 && pTempList->GetCount() == 1);
		SPList::Node* pos3 = pSP->m_pSavedWords->GetFirst();
		CSourcePhrase* pWordSrcPhrase = pos3->GetData();
		adaption = pWordSrcPhrase->m_key;
		adaption += _T("~"); // fixed space marker
		pos3 = pSP->m_pSavedWords->GetLast();
		pWordSrcPhrase = pos3->GetData();
		adaption += pWordSrcPhrase->m_key;  // append the second word
	}
	else
	{
		// most of the time, control will go through this block and there will usually be
		// just a single CSourcePhrase instance in pTempList (exceptions will be when
		// dealing with a merger, or the reparse of a single instance results in 2 or more
		// due to the effect of the punctuation change)
		SPList::Node* pos2 = pTempList->GetFirst();
		while (pos2 != NULL)
		{
			CSourcePhrase* pSP = pos2->GetData();
			if (adaption.IsEmpty())
			{
				adaption = pSP->m_key;
			}
			else
			{
				adaption += _T(" ") + pSP->m_key;
			}
		}
	}
	// cleanup
	pos = pTempList->GetFirst();
	while (pos != NULL)
	{
		pSPhr = pos->GetData();
		DeleteSingleSrcPhrase(pSPhr,FALSE); // there is no partner pile
		pos = pos->GetNext();
	}
	pTempList->Clear();
	delete pTempList;
	return adaption;
}


///////////////////////////////////////////////////////////////////////////////
/// \return		FALSE if the rebuild potentially isn't done fully right internally (that
///             usually means that rebuilding one CSourcePhrase instance resulted in two
///             or more new ones being created - so we have to replace the old one with
///             the rebuilds; whereas a successful rebuild means one CSourcePhrase is
///             just changed internally and no new ones needed to be created), TRUE if 
///             the rebuild was successful
/// \param		pView		-> a pointer to the View
/// \param		pList		<- the list of source phrases of the current document 
///                            (i.e. m_pSourcePhrases)
/// \param		pos			-> the node location which stores the passed in pSrcPhrase
/// \param		pSrcPhrase	<- the pointer to the CSourcePhrase instance on the pos Node
///                            passed in as the previous parameter
/// \param		fixesStr	<- reference to the caller's storage string for accumulating
///							   a list of references to the locations where the rebuild 
///							   potentially isn't quite fully right, for specific 
///							   pSrcPhrase instances, if any
/// \remarks
/// Called from: the Doc's RetokenizeText(). 
/// Rebuilds the document after a punctuation change has been made. If the function "fails"
/// internally (ie. potentially isn't a simple rebuild, as discussed above for the return
/// value), that particular pSrcPhrase instance has to be noted with a reference to chapter
/// and verse in fixesStr so the user can later inspect the doc visually and edit at such
/// locations to restore the translation (or gloss) correctly; but while the legacy
/// function would throw away the adaptation in such a circumstance happened, this present
/// (11Oct10)refactoring will try to retain the adaptation - even if it means a new
/// CSourcePhrase is left with m_adaption and m_targetStr empty, we rely on the user's
/// manual check to then fix things if it's different than what it should be, after the
/// rebuild has finished. \So a "fail" of the rebuild means that the rebuild did not, for
/// the rebuild done on a single CSourcePhrase instance, result in a single CSourcePhrase
/// instance - but rather two or more (it is not possible for the rebuild of a single
/// instance to result in none).
/// 
/// ???? the next paragraph may be inaccurate -- I'm gunna reparse fully, so probably
/// we'll insert the new one or more before the old one, on every call, and delete the old
/// one, rather than messing with copying info etc -- remove the next paragraph if this is
/// indeed what I end up doing (BEW 13Jan11)
/// ???
/// Our approach is as follows: if the rebuild of each generates a single instance, we
/// re-setup the members of that passed in instance with the correct new values, (and throw
/// away the rebuilt one - fixing that one up would be too time-consuming); but if the
/// rebuild fails, we go to the bother of putting the possibly altered translation into the
/// m_adaption and m_targetStr members of the instance with the longest m_key member, and
/// other members we copy across to the first of the new instances; then insert the new
/// list into the main document's list, and throw away the original passed in one. We
/// internally keep track of how many new CSourcePhrase instances are created and how many
/// of these precede the view's m_nActiveSequNum value so we can progressively update the
/// latter and so reconstitute the phrase box at the correct location when done.
/// 
/// BEW ammended definition and coded the function
/// whm added to wxWidgets build 4Apr05
/// BEW 11Oct10 (actually 13Jan11)added code to base reparse on returned string from the
/// function FromSingleMakeSstr() (rather than on m_srcPhrase, because the latter would
/// ignore the stored inline markers etc); and also added code to use an overload of
/// TokenizeTextString() to parse the old m_targetStr adaptation, (whenever an adaptation
/// is present of course, this function has to be able to operate on unadapted
/// CSourcePhrase instances too), so as to extract a possibly adjusted m_adaption value to
/// use in the rebuild, and not to abandon the legacy adaptations if possible, - the final
/// result should be a much better rebuild, keeping much more (or all) of the information
/// without loss, and alerting the user to where we think a visual inspection should be
/// done in order to verify the results are acceptable - and edit if not.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::ReconstituteAfterPunctuationChange(CAdapt_ItView* pView, 
					SPList*& pList, SPList::Node* pos, CSourcePhrase*& pSrcPhrase, 
					wxString& fixesStr)
{
	int nOriginalCount = pSrcPhrase->m_nSrcWords;
	bool bNotInKB = FALSE; // default
	bool bRetranslation = FALSE; // default
	if (pSrcPhrase->m_bRetranslation) bRetranslation = TRUE;
	if (pSrcPhrase->m_bNotInKB) bNotInKB = TRUE;

	SPList* pResultList = new SPList; // where we'll store pointers to parsed 
            // new CSourcePhrase instances; but only use what is in it provided there is
            // only one stored there - if more than one, we delete them and retain
            // unchanged the original pSrcPhrase passed in
	bool bSucceeded = TRUE;

    // remove the CRefString entries for this instance from the KBs, or decrement its count
    // if several seen before but do restoring of KB entries within the called functions
    // (because they know whether the rebuild succeeded or not and they have the required
    // strings at hand) - but do the removal from the KB only if not a placeholder, not a
    // retranslation not specified as "not in the KB", and there is a non-empty adaptation
	if (!pSrcPhrase->m_bNullSourcePhrase && !pSrcPhrase->m_bRetranslation && 
		!pSrcPhrase->m_bNotInKB && !pSrcPhrase->m_adaption.IsEmpty())
	{
		pView->RemoveKBEntryForRebuild(pSrcPhrase);
	}
    // determine whether we are dealing with just one, or an owning one and the sublist
	// containing those it owns (note, a conjoined pair with joining by fixed space USFM
	// marker, ~ , is to be treated as a single CSourcePhrase even though formally it's a
	// pseudo-merger, so we must call IsFixedSpaceSymbolWithin() in the test and if it
	// returns TRUE, we don't enter the TRUE block below, but rather process such an
	// instance in the else block where ReconstituteOneAfterPunctuationChange() is called)
	if (pSrcPhrase->m_nSrcWords > 1 && !IsFixedSpaceSymbolWithin(pSrcPhrase))
	{
//#ifdef __WXDEBUG__
//		wxLogDebug(_T("  ReconsistuteAfterPunctuationChange: 17,734 For Merger:  pSrcPhrase sn = %d  m_srcPhrase = %s"),
//					pSrcPhrase->m_nSequNumber, pSrcPhrase->m_srcPhrase.c_str());
//#endif

		// BEW 10Mar11, the protocol we use for mergers is the following:
		// (a) we must change the punctuation & src & tgt language members not just of the
		// merged instance, but also for each of the instances in it's m_pSavedWords
		// sublist of originals. (Why? Because the user may sometime unmerge it, and we
		// don't want to be restoring instances to be viewed, which reflect the old
		// punctuation settings.)
		// (b)Care must be exercised, merging creates another level of CSourcePhrase
		// instances, so our algorithm must avoid calling Merge() (see SourcePhrase.cpp)
		// on the instances in the saved sublist, such as m_pSavedWords. But we also want
		// to keep the converted target text, where it exists, so....
		// (c)We use FromMergerMakeSstr() to get a source text string, srcPhrase, with all
		// the markers, unfilterings, punctuations etc in their proper place;
		// (d) We use TokenizeTextString(), passing in pResultList to get returned newly
		// created CSourcePhrase instances returned, having passed in srcPhrase wxString;
		// (e) provided, and only provided, the number of elements in pResultList equals
		// the element count of pSrcPhrase->m_pSavedWords, we iterate in parallel over
		// both the latter and the CSourcePhrase instances in pResultList, and copy over
		// from the latter the changed text and punctuation strings, to the former; we
		// also obtain from the pResultList's instances, each m_targetStr contents, and
		// using the new punctuation settings (ie. making a RemovePunctuation() call with
        // the appropriate punctuation string passed in)calculate a new m_adaption value
        // for each instance, and we then transfer the m_adaption values back to the same
        // members on the equivalent CSourcePhrase instances within the
        // pSrcPhrase->m_pSavedWords list (the m_targetStr values won't have changed)
		// (f) pSrcPhrase->m_pSavedWords's contents are now up-to-date for the changed
		// punctuation settings. The owning merged CSourcePhrase instance's m_srcPhrase
		// member will not have changed (punctuation settings changes don't add or remove
		// or alter the location of punctuation and word building characters in the source
		// text, it just redefines where the boundaries are between "the words" and the
		// punctuation characters at the start and end of them. So, to get the new value
		// for the m_adaption member of the merger (ie. of pSrcPhrase), all we need do is
		// pass pSrcPhrase->m_srcPhrase through RemovePunctuation() using the final
		// parameter from_target_text so as to do it with the target language's new
		// punctuation settings (which may, or may not, have changed). 
        // Once (f) is completed, the whole original merged pSrcPhrase has been
        // successfully updated to the new punctuation settings.
		SPList* pOwnedList = pSrcPhrase->m_pSavedWords; // for convenience's sake

        // placeholders can't be merged, and so won't be in the merger, so this block can
        // ignore them

		wxString srcPhrase; // for a copy of m_srcPhrase member
		wxString targetStr; // for a copy of m_targetStr member
		wxString adaption; // for a copy of m_adaption member
		wxString gloss; // for a copy of m_gloss member
		adaption.Empty(); 
		gloss.Empty();

        // setup the srcPhrase, targetStr and gloss strings - we must handle glosses too
        // regardless of the current mode (whether adapting or not) since rebuilding
        // affects everything
		gloss = pSrcPhrase->m_gloss; // we don't care if glosses have punctuation or not
        // Set srcPhrase string: this member has all the source punctuation, if any on this
        // word or phrase, as well as markers etc, as FromMergerMakeSstr() is docv5 aware
		//srcPhrase = pSrcPhrase->m_srcPhrase;
		srcPhrase = FromMergerMakeSstr(pSrcPhrase);
		// Set targetStr only to the punctuated m_targetStr member, because we only want
		// to deal with words, tgt punctuation and possibly fixed space marker (~) when we
		// come to reparsing the target text with target punctutation chars to see if
		// things have been changed in the target text
		targetStr = pSrcPhrase->m_targetStr;
		// calling RemovePunctuation() on this, using the target language punctuation
		// settings string, will produce an appropriate m_adaption member for the merged
		// instance
		adaption = targetStr;
		if (!adaption.IsEmpty())
		{
			pView->RemovePunctuation(this, &adaption, from_target_text);
			pSrcPhrase->m_adaption = adaption;
		}
        // Note: in case you are wondering... changing the punctuation settings, not matter
        // what kind of change is made to them, will have absolutely no effect on the
        // merger's m_srcPhrase value. The latter can't obtain new characters, nor lose
        // existing characters, by a punctuation settings change. All that can happen is
        // that the status of some characters already present will underlyingly change from
        // being word-building, to being punctuation, or vise versa. Nor can the relative
        // order of characters in m_srcPhrase be changed by a change to the punctuation
        // settings. It's only m_key and/or m_adaption which have the potential to have
        // different values after a punctuation change.

		// reparse the srcPhrase string - this will use the newly changed punctuation
		// settings, and hopefully, produce a new set of CSourcePhrase instances, with
		// same count as would be obtained from a GetCount() call on m_pSavedWords above;
		// this isn't guaranteed however, and if the count differs, we won't use the
		// results of this tokenization
		srcPhrase.Trim(TRUE); // trim right end
		srcPhrase.Trim(FALSE); // trim left end
		int numElements;
		numElements = pView->TokenizeTextString(pResultList, srcPhrase, pSrcPhrase->m_nSequNumber);
		wxASSERT(numElements > 1);

		// BEW 10Mar11, if the counts match, then we can copy data from one instance in
		// pResultsList to the corresponding instance in pSrcPhrase->m_pSavedWords, and if
		// that is the case, we get a robust conversion. Different element counts result
		// in indeterminacies in how to transfer the data appropriately. Rather than
		// guessing, we return FALSE to let fixesStr get an entry added, and the caller
		// will ensure it is shown to the user so he can visually inspect the document and
		// edit it as required at the appropriate places.

		// test to see if we have a candidate for updating successfully
		if ((int)pResultList->GetCount() == nOriginalCount)
		{
            // The number of new CSourcePhrase instances has not changed, because it
            // matches the count of the instances in pSrcPhrase->m_pSavedWords. So we can
			// update the merger.
			SPList::Node* posOwned = pOwnedList->GetFirst(); // i.e. from pSrcPhrase->m_pSavedWords list
			SPList::Node* posNew = pResultList->GetFirst(); // i.e. from the tokenization above
			bool bIsFirst = FALSE;
			bool bIsLast = FALSE;
			int count = 0;
			pSrcPhrase->m_pMedialPuncts->Clear(); // we refill it below, in the loop
			while (posOwned != NULL && posNew != NULL)
			{
				CSourcePhrase* pOwnedSrcPhrase = posOwned->GetData();
				posOwned = posOwned->GetNext();
				CSourcePhrase* pNewSrcPhrase = posNew->GetData();
				posNew = posNew->GetNext();

				count++;
				if (count == 1)
					bIsFirst = TRUE;
				if (count == nOriginalCount)
					bIsLast = TRUE;

				// Transfer m_key, m_srcPhrase, from pResultsList's instances, the latter
				// is built from the reconstituted source text, and so has no adaptation
				// information. But for m_adaption and m_targetStr for the owned
				// CSourcePhrase instances' list, we have to take the values in the
				// instances within pSrcPhrase->m_pSavedWords - their m_targetStr values
				// (for the values with punctuation), but for the m_adaption values, we'll
				// have to pass the former through RemovePunctuation() using the
				// use_target_punctuation enum value. The new values for m_precPunct,
				// m_follPunct and m_follOuter punct are more tricky - we can transfer
				// what is in each instance within pResultsList's instances, but only to
				// each of the instances in pSrcPhrase->m_pSavedWords; and as we do that
				// we have to use the bIsFirst and bIsLast flags to transfer only the
				// m_precPunct value from the first, and m_follPunct and m_follOuterPunct
				// from the last in pResultsList directly to the parent pSrcPhrase's
				// m_precPunct and m_follPunct and m_follOuterPunct members - but for all
				// others, they are "medial" to the merger, and so have to be added, in
				// order encountered, to pSrcPhrase->m_pMedialPuncts wxArrayString.
				// (m_pMedialMarkers values don't change when doing adjustments for a
				// change in the punctuation settings, so we can leave what is in
				// pSrcPhrase unchanged)
				pOwnedSrcPhrase->m_srcPhrase = pNewSrcPhrase->m_srcPhrase; // this line should be redundant
				pOwnedSrcPhrase->m_key = pNewSrcPhrase->m_key;
				wxString anAdaption;
				wxString aTargetStr = pOwnedSrcPhrase->m_targetStr; // m_targetStr shouldn't have changed
				anAdaption = aTargetStr;
				if (!anAdaption.IsEmpty())
				{
					pView->RemovePunctuation(this, &anAdaption, from_target_text);
					pOwnedSrcPhrase->m_adaption = anAdaption;
				}
				else
				{
					pOwnedSrcPhrase->m_adaption.Empty();
				}
				pOwnedSrcPhrase->m_precPunct = pNewSrcPhrase->m_precPunct;
				pOwnedSrcPhrase->m_follPunct = pNewSrcPhrase->m_follPunct;
				pOwnedSrcPhrase->SetFollowingOuterPunct(pNewSrcPhrase->GetFollowingOuterPunct());
				if (bIsFirst)
				{
					// first instance in pResultsList; anything in the pResultsList's
					// m_precPunct from the initial instance has to be copied directly
					pSrcPhrase->m_precPunct = pOwnedSrcPhrase->m_precPunct; // as set above

                    // but anything in m_follPunct and/or m_follOuterPunct has to be copied
                    // to pSrcPhrase->m_pMedialPuncts array instead
					wxString follPunctStr = pNewSrcPhrase->m_follPunct;
					if (!follPunctStr.IsEmpty())
					{
						pSrcPhrase->m_pMedialPuncts->Add(follPunctStr);
					}
					wxString follOuterPunctStr = pNewSrcPhrase->GetFollowingOuterPunct();
					if (!follOuterPunctStr.IsEmpty())
					{
						pSrcPhrase->m_pMedialPuncts->Add(follOuterPunctStr);
					}
					bIsFirst = FALSE; // after entered once, prevent re-entry to this block
				}
				else if (bIsLast)
				{
					// last instance in pResultsList; anything in m_follPunct and/or
					// m_follOuterPunct has to be copied direct to same members of pSrcPhrase
					pSrcPhrase->m_follPunct = pOwnedSrcPhrase->m_follPunct; // as set above
					pSrcPhrase->SetFollowingOuterPunct(pOwnedSrcPhrase->GetFollowingOuterPunct()); // as set above

					// but anything in m_precPunct in the instance from pResultsList, has to 
					// be copied to pSrcPhrase->m_pMedialPuncts array
					wxString precPunctStr = pNewSrcPhrase->m_precPunct;
					if (!precPunctStr.IsEmpty())
					{
						pSrcPhrase->m_pMedialPuncts->Add(precPunctStr);
					}
				}
				else if (!bIsLast && !bIsFirst)
				{
					// neither first nor last instance in pResultsList; any punctuation in
					// such instances' m_precPuncts, m_follPuncts, and/or m_follOuterPuncts
					// has to be copied to pSrcPhrase->m_pMedialPuncts array
					wxString precPunctStr = pNewSrcPhrase->m_precPunct;
					if (!precPunctStr.IsEmpty())
					{
						pSrcPhrase->m_pMedialPuncts->Add(precPunctStr);
					}
					wxString follPunctStr = pNewSrcPhrase->m_follPunct;
					if (!follPunctStr.IsEmpty())
					{
						pSrcPhrase->m_pMedialPuncts->Add(follPunctStr);
					}
					wxString follOuterPunctStr = pNewSrcPhrase->GetFollowingOuterPunct();
					if (!follOuterPunctStr.IsEmpty())
					{
						pSrcPhrase->m_pMedialPuncts->Add(follOuterPunctStr);
					}
				}
			} // end of loop for test: while (posOwned != NULL && posNew != NULL)
		} // end of TRUE block for test:  if ((int)pResultList->GetCount() == nOriginalCount)
		else // the reparsed source text has generated a different number of CSourcePhrase instances 
		{
			// we got too many or two few CSourcePhrase instances in the reparse of the
			// source text (including markers etc) from the merger, so we'll have to
			// abandon this merger; instead, just retain the pSrcPhrase passed in,
			// unchanged, - clear out pResultList, leave an entry in fixesStr and return
			// FALSE
			pView->UpdateSequNumbers(0); // make sure they are in sequence, 
										 // so next call won't fail
			if (!gbIsUnstructuredData)
			{
				// sequence numbers should be up-to-date
				fixesStr += pView->GetChapterAndVerse(pSrcPhrase);
				wxString srcStr = _T(' ');
				srcStr += pSrcPhrase->m_srcPhrase;
				fixesStr += srcStr;
				fixesStr += _T("   ");
			}
			// the pResultList list was not used in this block, 
			// so we can unilaterally clear it here
			SPList::Node* aPos = pResultList->GetFirst();
			CSourcePhrase* pASrcPhrase = NULL;
			while (aPos != NULL)
			{
				pASrcPhrase = (CSourcePhrase*)aPos->GetData();
				aPos = aPos->GetNext();
				DeleteSingleSrcPhrase(pASrcPhrase,FALSE); // FALSE means 
					// "don't delete its partner pile" as we'll let RecalcLayout()
					// delete them all quickly en masse later
			}
			pResultList->Clear();
			delete pResultList;
			return FALSE;
		} // end of else block for test: if ((int)pResultList->GetCount() == nOriginalCount)
	} // end of block for when dealing with a merged sourcephrase
	else // the test of pSrcPhrase->m_nSrcWords yielded 1 
		 // (ie. an unmerged sourcephrase)
	{
        // we are dealing with a plain vanila single-word non-owned sourcephrase in either
        // adaptation or glossing mode
        // FALSE is bIsOwned, i.e. not owned, when not owned it is visible in the layout,
        // if TRUE, it is one which is stored in the m_pSavedWords list of an unowned
        // CSourcePhrase and so is not visible in the layout
		bool bWasOK = ReconstituteOneAfterPunctuationChange(
						pView,pList,pos,pSrcPhrase,fixesStr,pResultList,FALSE);

//#ifdef __WXDEBUG__
//		wxLogDebug(_T("  17950 After ...One..., RETURNED bWasOK = %d  ,  pSrcPhrase sn = %d  m_srcPhrase = %s"),
//					bWasOK, pSrcPhrase->m_nSequNumber, pSrcPhrase->m_srcPhrase.c_str());
//#endif
		if (!bWasOK)
		{
			// we got more than one in the reparse, so we are going to retain the passed
			// in pSrcPhrase unchanged
			if (!gbIsUnstructuredData)
			{
                // sequence numbers should be up-to-date
				fixesStr += pView->GetChapterAndVerse(pSrcPhrase);
				wxString srcStr = _T(' ');
				srcStr += pSrcPhrase->m_srcPhrase;
				fixesStr += srcStr;
				fixesStr += _T("   ");
			}

			// the pResultList list was not used in this block, 
			// so we can unilaterally clear it here
			SPList::Node* aPos = pResultList->GetFirst();
			CSourcePhrase* pASrcPhrase = NULL;
			while (aPos != NULL)
			{
				pASrcPhrase = (CSourcePhrase*)aPos->GetData();
				aPos = aPos->GetNext();
				DeleteSingleSrcPhrase(pASrcPhrase,FALSE); // FALSE means 
					// "don't delete its partner pile" as we'll let RecalcLayout()
					// delete them all quickly en masse later
			}
			pResultList->Clear();
			delete pResultList;
			
			return FALSE;
		}
	} // end of else block for test: if (pSrcPhrase->m_nSrcWords > 1 && !IsFixedSpaceSymbolWithin(pSrcPhrase))

	// delete the local list and its managed memory chunks - don't leak memory
	SPList::Node* aPos = pResultList->GetFirst();
	CSourcePhrase* pASrcPhrase = NULL;
	while (aPos != NULL)
	{
		pASrcPhrase = (CSourcePhrase*)aPos->GetData();
		aPos = aPos->GetNext();
		DeleteSingleSrcPhrase(pASrcPhrase,FALSE); // FALSE means 
						// "don't delete its partner pile" as we'll let RecalcLayout()
						// delete them all quickly en masse later
	}
	pResultList->Clear();
	delete pResultList;

	// the 8Mar11 changes should mean that the m_nActiveSequNum will not be changed by
	// this function nor any it calls
	//gpApp->m_nActiveSequNum = nActiveSequNum; // update the view's member 
											  // so all keeps in sync
	// sequence numbers should not need updating, but we'll do so for safety's sake
	pView->UpdateSequNumbers(0);

	return bSucceeded;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		bKBFilename				-> if TRUE the KB's filename is to be updated
/// \param		bKBPath					-> if TRUE the KB path is to be updated
/// \param		bKBBackupPath			-> if TRUE the KB's backup path is to be updated
/// \param		bGlossingKBPath			-> if TRUE the glossing KB path is to be updated
/// \param		bGlossingKBBackupPath	-> if TRUE the glossing KB's backup path is to be updated
/// \remarks
/// Called from: the App's LoadGlossingKB(), LoadKB(), StoreGlossingKB(), StoreKB(), 
/// SaveKB(), SaveGlossingKB(), SubstituteKBBackup().
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::UpdateFilenamesAndPaths(bool bKBFilename,bool bKBPath,
						bool bKBBackupPath,bool bGlossingKBPath, 
						bool bGlossingKBBackupPath)
{
	// ensure the current KB filename ends with .xml extension
	if (bKBFilename)
	{
		wxString thisFilename = gpApp->m_curKBName;
		thisFilename = MakeReverse(thisFilename);
		int nFound = thisFilename.Find(_T('.'));
		wxString extn = thisFilename.Left(nFound);
		extn = MakeReverse(extn);
		if (extn != _T("xml"))
		{
			thisFilename = thisFilename.Mid(nFound); // chop off bad extn
			thisFilename = MakeReverse(thisFilename);
			thisFilename += _T("xml"); // add xml as the extension
			gpApp->m_curKBName = thisFilename; // update to correct filename
		}
	}
	
	// KB Path (m_curKBPath)
	if (bKBPath)
	{
		wxString thisPath = gpApp->m_curKBPath;
		thisPath = MakeReverse(thisPath);
		int nFound = thisPath.Find(_T('.'));
		wxString extn = thisPath.Left(nFound);
		extn = MakeReverse(extn);
		if (extn != _T("xml"))
		{
			thisPath = thisPath.Mid(nFound); // chop off bad extn
			thisPath = MakeReverse(thisPath);
			thisPath += _T("xml"); // add xml as the extension
			gpApp->m_curKBPath = thisPath;
		}
	}
	
	// KB Backup Path (m_curKBBackupPath)
	// BEW 3Mar11, changed for .BAK rather than .BAK.xml, at Bob Eaton's request
	if (bKBBackupPath)
	{
        // this has an extension, which is always to be .BAK
		wxString thisBackupPath = gpApp->m_curKBBackupPath;
		thisBackupPath = MakeReverse(thisBackupPath); // reversed
		int nFound = thisBackupPath.Find(_T("KAB."));
		if (nFound != wxNOT_FOUND)
		{
			// found outermost (reversed) .BAK -- nothing to do except reverse again
			thisBackupPath = MakeReverse(thisBackupPath);
		}
		else
		{
			// no extension! Therefore just take the name and add .BAK
			thisBackupPath = MakeReverse(thisBackupPath);
			thisBackupPath += _T(".BAK");
		}
		gpApp->m_curKBBackupPath = thisBackupPath;
	}

	// Glossing KB Path
	if (bGlossingKBPath)
	{
		wxString thisGlossingPath = gpApp->m_curGlossingKBPath;
		thisGlossingPath = MakeReverse(thisGlossingPath);
		int nFound = thisGlossingPath.Find(_T('.'));
		wxString extn = thisGlossingPath.Left(nFound);
		extn = MakeReverse(extn);
		if (extn != _T("xml"))
		{
			thisGlossingPath = thisGlossingPath.Mid(nFound); // chop off bad extn
			thisGlossingPath = MakeReverse(thisGlossingPath);
			thisGlossingPath += _T("xml"); // add xml as the extension
			gpApp->m_curGlossingKBPath = thisGlossingPath;
		}
	}

	// Glossing KB Backup Path
	if (bGlossingKBBackupPath)
	{
        // this has an extension, which is always to be .BAK
		wxString thisGlossingBackupPath = gpApp->m_curGlossingKBBackupPath;
		thisGlossingBackupPath = MakeReverse(thisGlossingBackupPath); // reversed
		int nFound = thisGlossingBackupPath.Find(_T("KAB."));
		if (nFound != wxNOT_FOUND)
		{
			// found outermost (reversed) .BAK -- nothing to do but reverse again
			thisGlossingBackupPath = MakeReverse(thisGlossingBackupPath);
		}
		else
		{
			// no extension! Therefore just take the name and add .BAK
			thisGlossingBackupPath = MakeReverse(thisGlossingBackupPath);
			thisGlossingBackupPath += _T(".BAK");
		}
		gpApp->m_curGlossingKBBackupPath = thisGlossingBackupPath;
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		title				-> the new title to be used on the document's title bar
/// \param		nameMinusExtension	<- the same name as title, but minus any extension returned by
///										reference to the caller
/// \remarks
/// Called from: the Doc's OnNewDocument(), OnOpenDocument() and CMainFrame's OnIdle().
/// Sets or updates the main frame's title bar with the appropriate name of the current
/// document. It also suffixes " - Adapt It" or " - Adapt It Unicode" to the document title,
/// depending on which version of Adapt It is being used. 
/// The extension for all documents in the wx version is .xml. This function also calls 
/// the doc/view framework's SetFilename() to inform the framework of the change in the
/// document's name.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::SetDocumentWindowTitle(wxString title, wxString& nameMinusExtension)
{
	// remove any extension user may have typed -- we'll keep control ourselves
	wxString noExtnName = gpApp->MakeExtensionlessName(title);
	nameMinusExtension = noExtnName; // return to the caller the name 
									 // without the final extension

    // we'll now put on it what the extension should be, according to the doc type we have
    // elected to save
	wxString extn = _T(".xml");
	title = noExtnName + extn;

	// whm Note: the m_strTitle is internal to CDocument
	// update the target platform's native storage for the doc title
	this->SetFilename(title, TRUE); // see above where default unnamedN is set - 
									// TRUE means "notify views"
		
    // our Adapt It main window should also show " - Adapt It" or " - Adapt It Unicode"
    // after the document title, so we'll set that up by explicitly overwriting the title
    // bar's document name (Also, via the Setup Wizard, an output filename is not
    // recognised as a name by MFC and the MainFrame continues to show "Untitled", so we
    // set the window title explicitly as above)
	wxDocTemplate* pTemplate = GetDocumentTemplate();
	wxASSERT(pTemplate != NULL);
	wxString typeName, typeName2; // see John's book p149
	// BEW added Unicode for unicode build, 06Aug05
	typeName = pTemplate->GetDocumentName(); // returns the document type name as passed
											 // to the doc template constructor
	typeName2 = pTemplate->GetDescription(); 
	if (!typeName.IsEmpty()) 
	{
		typeName = _T(" - Adapt It");
		#ifdef _UNICODE
		typeName += _T(" Unicode");
		#endif
	}
	else
	{
		typeName = _T(" - ") + typeName; // Untitled, I think
		#ifdef _UNICODE
		typeName += _T(" Unicode");
		#endif
	}
	this->SetTitle(title + typeName);
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		curOutputFilename	-> the current m_curOutputFilename value, or value
///                                    of some other filename string (eg. a renamed one)
/// \remarks
/// Called from: the Doc's BackupDocument() and DoFileSave().
/// Insures that the m_curOutputBackupFilename ends with ".BAK". The wx version does
/// not handle the legacy .adt binary file types/extensions. 
/// BEW 30Apr10, removed second param (the m_bSaveAsXML flag)
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::MakeOutputBackupFilenames(wxString& curOutputFilename)
{
    // input should be the current m_curOutputFilename value, a renamed filename; the 
	// function assumes that the caller's value for m_curOutputFilename, or the renamed
	// filename, is correct

    // we calculate the backup name here, .xml for the final extension is always done;
    // while it should not happen that the incoming filename has any other final extension
    // than .xml, we don't assume so and so we recalculate here to ensure .xml compliance
	wxString baseFilename = curOutputFilename;
	wxString thisBackupFilename;
	baseFilename = MakeReverse(baseFilename);
	int nFound = baseFilename.Find(_T('.'));
	wxString extn;
	if (nFound > -1)
	{
		nFound += 1;
		extn = baseFilename.Left(nFound); // include period in the extension
		thisBackupFilename = baseFilename.Mid(extn.Length());
	}
	else
	{
		// no extension
		thisBackupFilename = baseFilename;
	}	
	thisBackupFilename = MakeReverse(thisBackupFilename);

	// saving will be done in XML format, so backup filenames must comply with that...

	// add the required extensions; the complying backup filename is always of form:  *.BAK 
	thisBackupFilename += _T(".BAK");
	//gpApp->m_curOutputBackupFilename = thisBackupFilename + _T(".xml"); BEW removed 3Mar11
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		event	-> unused wxCommandEvent
/// \remarks
/// Called from: the Tools menu "Split Document..." command.
/// Invokes the CSplitDialog dialog.
/// Invokes the CSplitDialog dialog.
/// BEW 29Mar10, added RemoveSelection() call, because if the command is entered and acted
/// upon immediately after, say, a Find which gets the wanted location and shows it
/// selected, a later RemoveSelection() call will try remove m_selection data which by
/// then will contain only hanging CCell pointers - giving a crash
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnSplitDocument(wxCommandEvent& WXUNUSED(event))
{
	gpApp->GetView()->RemoveSelection();
	CSplitDialog d(gpApp->GetMainFrame());
	d.ShowModal();
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		event	-> unused wxCommandEvent
/// \remarks
/// Called from: the Tools menu "Join Document..." command.
/// Invokes the CJoinDialog dialog.
/// Invokes the CSplitDialog dialog.
/// BEW 29Mar10, added RemoveSelection() call, because if the command is entered and acted
/// upon immediately after, say, a Find which gets the wanted location and shows it
/// selected, a later RemoveSelection() call will try remove m_selection data which by
/// then will contain only hanging CCell pointers - giving a crash
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnJoinDocuments(wxCommandEvent& WXUNUSED(event))
{
	gpApp->GetView()->RemoveSelection();
	CJoinDialog d(gpApp->GetMainFrame());
	d.ShowModal();
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		event	-> unused wxCommandEvent
/// \remarks
/// Called from: the Tools menu "Move Document..." command.
/// Invokes the CMoveDialog dialog.
/// Invokes the CSplitDialog dialog.
/// BEW 29Mar10, added RemoveSelection() call, because if the command is entered and acted
/// upon immediately after, say, a Find which gets the wanted location and shows it
/// selected, a later RemoveSelection() call will try remove m_selection data which by
/// then will contain only hanging CCell pointers - giving a crash
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnMoveDocument(wxCommandEvent& WXUNUSED(event))
{
	gpApp->GetView()->RemoveSelection();
	CMoveDialog d(gpApp->GetMainFrame());
	d.ShowModal(); // We don't care about the results of the dialog - 
				   // it does all it's own work.
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a list of source phrases composing the document
/// \param		FilePath	-> the path + name of the file
/// \remarks
/// Called from: the App's LoadSourcePhraseListFromFile() and 
/// CJoinDialog::OnBnClickedJoinNow().
/// In the wx version the current doc's m_pSourcePhrases list is not on the Doc, but on the
/// App. Since OnOpenDocument() always places the source phrases it retrieves from the
/// opened file into the m_pSourcePhrases list on the App, this function temporarily saves
/// those source phrases from the currently open Doc, in order to allow OnOpenDocument() to
/// save its new source phrases in m_pSourcePhrases on the App. Then, once we have the new
/// ones loaded we copy them to the list being returned, and repopulate m_pSourcePhrases
/// list with the original (open) document's source phrases.
/// I'm taking this approach rather than redesigning things at this point. Having all the
/// doc's members moved to the App was necessitated at the beginning of the wx version
/// conversion effort (because of the volatility of the doc's member pointers within the wx
/// doc-view framework). It would have been helpful to redesign some other routines
/// (OnOpenDocument and the XML doc parsing routines) to pass the list of source phrases
/// being built as a parameter rather than keeping it as a global list.
/// At any rate, here we need to juggle the source phrase pointer lists in order to
/// load a source phrase list from a document file.
///////////////////////////////////////////////////////////////////////////////
SPList *CAdapt_ItDoc::LoadSourcePhraseListFromFile(wxString FilePath)
{
	SPList *rv; // Return Value.

	CAdapt_ItDoc d; // needed to call non-static OnOpenDocument() below
	// wx version note: In the wx version the current doc's m_pSourcePhrases list
	// is not on the doc, but on the app. Since OnOpenDocument() always places the
	// source phrases it retrieves from the opened file into the m_pSourcePhrases
	// list on the App, we need to temporarily save those source phrases from the
	// currently open doc, in order to allow OnOpenDocument() to save its new
	// source phrases in m_pSourcePhrases on the App. Then, once we have the new ones
	// we can copy them to the list being returned, and repopulate m_pSourcePhrases
	// list with the original (open) document's source phrases.
	// I'm taking this approach rather than redesigning things at this point. Having
	// all the doc's members moved to the App was necessitated at the beginning of the
	// wx version conversion effort (because of the volatility of the doc's member
	// pointers within the wx doc-view framework). It would have been helpful to 
	// redesign some other routines (OnOpenDocument and the XML doc parsing routines)
	// to pass the list of source phrases being built as a parameter rather than keeping
	// it as a global list.
	// At any rate, here we need to juggle the source phrase pointer lists in order to
	// load a source phrase list from a document file.
	
	rv = new SPList();
	rv->Clear();

	gpApp->m_bWantSourcePhrasesOnly = true;
	SPList* m_pSourcePhrasesSaveFromApp = new SPList(); // a temp list to save the 
												// SPList of the currently open document
	// save the list of pointers from those on the app to a temp list 
	// (the App's list to be restored later)
	for (SPList::Node *node = gpApp->m_pSourcePhrases->GetFirst(); 
			node; node = node->GetNext())
	{
		m_pSourcePhrasesSaveFromApp->Append((CSourcePhrase*)node->GetData());
	}
    // pointers are now saved in the temp SPList, so clear the list on the App to be ready
    // to receive the new list within OnOpenDocument()
	gpApp->m_pSourcePhrases->Clear();
	d.OnOpenDocument(FilePath); // OnOpenDocument loads source phrases into 
								// m_pSourcePhrases on the App
	gpApp->m_bWantSourcePhrasesOnly = false;
	// copy the pointers to the list we are returning from LoadSourcePhraseListFromFile
	for (SPList::Node *node = gpApp->m_pSourcePhrases->GetFirst(); 
			node; node = node->GetNext())
	{
		rv->Append((CSourcePhrase*)node->GetData());
	}
	// now restore original App list
	gpApp->m_pSourcePhrases->Clear();
	for (SPList::Node *node = m_pSourcePhrasesSaveFromApp->GetFirst(); 
			node; node = node->GetNext())
	{
		gpApp->m_pSourcePhrases->Append((CSourcePhrase*)node->GetData());
	}
	// now clear and delete the temp save list
	m_pSourcePhrasesSaveFromApp->Clear();
	delete m_pSourcePhrasesSaveFromApp;
	m_pSourcePhrasesSaveFromApp = NULL;
	// lastly return the new list loaded from the file
	return rv;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the File Menu
///                        is about to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected,
/// and before the menu is displayed.
/// If Vertical Editing is in progress it disables the File Pack Document menu item and
/// returns immediately. It enables the menu item if there is a KB ready (even if only a
/// stub), and the document is loaded, and documents are to be saved as XML is turned on;
/// and glossing mode is turned off, otherwise the command is disabled.
/// BEW modified 13Nov09, when read-only access to project folder, don't allow pack doc
/// BEW 25Nov09, to allow pack doc but to use the m_bReadOnlyAccess flag to suppress
/// doing a project config file write and a doc save, but instead to just take the project
/// config file and doc files as they currently are on disk in order to do the pack
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateFilePackDoc(wxUpdateUIEvent& event)
{
	if (gbVerticalEditInProgress)
	{
		event.Enable(FALSE);
		return;
	}
    // enable if there is a KB ready (even if only a stub), and the document loaded and 
    // glossing mode is turned off
	if ((gpApp->m_pLayout->GetStripArray()->GetCount() > 0) && gpApp->m_bKBReady && !gbIsGlossing)
	{
		event.Enable(TRUE);
	}
	else
	{
		event.Enable(FALSE);
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the File Menu is about
///                         to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected, and before
/// the menu is displayed.
/// If Vertical Editing is in progress it disables the Unpack Document..." command on the File 
/// menu, otherwise it enables the item as long as glossing mode is turned off.
/// BEW modified 13Nov09, if read-only access to project folder, don't permit unpack doc
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateFileUnpackDoc(wxUpdateUIEvent& event)
{
	if (gpApp->m_bReadOnlyAccess)
	{
		event.Enable(FALSE);
		return;
	}
	if (gbVerticalEditInProgress)
	{
		event.Enable(FALSE);
		return;
	}
    // enable provided glossing mode is turned off; we want it to be able to work even if
    // there is no project folder created yet, nor even a KB and/or document; but right
    // from the very first launch
	if (!gbIsGlossing)
	{
		event.Enable(TRUE);
	}
	else
	{
		event.Enable(FALSE);
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		event	-> unused wxCommandEvent
/// \remarks
/// Called from: the "Pack Document..." command on the File menu.
/// Packing creates a zipped (compressed *.aip file) containing sufficient information for
/// a remote Adapt It user to recreate the project folder, project settings, and unpack and
/// load the document on the remote computer. OnFilePackDoc collects six kinds of
/// information:
/// source language name; target language name; Bible book information; current output
/// filename for the document; the current project configuration file contents; the
/// document in xml format.
/// BEW 12Apr10, no changes needed for support of doc version 5
/// whm 4Feb11 modified to call a separate function DoPackDocument()
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnFilePackDoc(wxCommandEvent& WXUNUSED(event))
{
    // OnFilePackDoc(), for a unicode build, converts to UTF-8 internally, and so uses
    // CBString for the final output (config file and xml doc file are UTF-8 already).
	// DoPackDocument() assembles the raw data into the packByteStr byte buffer (CBString)
	wxString exportPathUsed;
	exportPathUsed.Empty();
	DoPackDocument(exportPathUsed,TRUE); // TRUE = invoke the wxFileDialog
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		event	-> unused wxCommandEvent
/// \remarks
/// Called from: the "Unpack Document..." command on the File menu.
/// OnFileUnpackDoc gets the name of a packed *.aip file from the user, uncompresses it and 
/// calls DoUnpackDocument() to do the remaining work of unpacking the document and loading
/// it into Adapt It ready to do work. 
/// If a document of the same name already exists on the destination machine in 
/// the same folder, the user is warned before the existing doc is overwritten by the document 
/// extracted from the packed file.
/// The .aip files pack with the Unicode version of Adapt It cannot be unpacked with the regular
/// version of Adapt It, nor vice versa.
/// BEW 12Apr10, no changes needed for support of doc version 5
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnFileUnpackDoc(wxCommandEvent& WXUNUSED(event))
{
	// OnFileUnpackDoc is the handler for the Unpack Document... command on the File menu. 
	// first, get the file and load it into a CBString
	wxString message;
	message = _("Load And Unpack The Compressed Document"); //IDS_UNPACK_DOC_TITLE
	wxString filter;
	wxString defaultDir;
	defaultDir = gpApp->m_curProjectPath; 
	filter = _("Packed Documents (*.aip)|*.aip||"); //IDS_PACKED_DOC_EXTENSION

	wxFileDialog fileDlg(
		(wxWindow*)wxGetApp().GetMainFrame(), // MainFrame is parent window for file dialog
		message,
		defaultDir,
		_T(""), // file name is null string
		filter,
		wxFD_OPEN); 
		// wxHIDE_READONLY was deprecated in 2.6 - the checkbox is never shown
		// GDLC wxOPEN weredeprecated in 2.8
	fileDlg.Centre();

	wxLogNull logNo; // avoid spurious messages from the system

	// open as modal dialog
	int returnValue = fileDlg.ShowModal();
	if (returnValue == wxID_CANCEL)
	{
		return; // user Cancelled, or closed the dialog box
	}
	else // must be wxID_OK
	{
		wxString pathName;
		pathName = fileDlg.GetPath();

        // whm Note: Since the "file" variable is created below and passed to
        // DoUnpackDocument and the DoUnpackDocument expects the file to already be
        // decompressed, we need decompress the file here before calling DoUnpackDocument.
        // We uncompress the packed file from the .aip compressed archive. It will have the
        // extension .aiz. We call DoUnpackDocument() on the .aiz file, then delete the
        // .aiz file which is of no usefulness after the unpacking and loading of the
        // document into Adapt It; we also would not want it hanging around for the user to
        // try to unpack it again which would fail because the routine would try to
        // uncompress an already uncompressed file and fail.

        // The wxWidgets version no longer needs the services of the separate freeware
        // unzip.h and unzip.cpp libraries.
        // whm 22Sep06 modified the following to use wxWidgets' own built-in zip filters
        // which act on i/o streams.
        // TODO: This could be simplified further by streaming the .aip file via
        // wxZipInputStream to a wxMemoryInputStream, rather than to an external
        // intermediate .aiz file, thus reducing complexity and the need to manipulate
        // (create, delete, rename) the external files.
		wxZipEntry* pEntry;
		// first we create a simple output stream using the zipped .aic file (pathName)
		wxFFileInputStream zippedfile(pathName);
        // then we construct a zip stream on top of this one; the zip stream works as a
        // "filter" unzipping the stream from pathName
		wxZipInputStream zipStream(zippedfile);
		wxString unzipFileName;
		pEntry = zipStream.GetNextEntry(); // gets the one and only zip entry in the 
										   // .aip file
		unzipFileName = pEntry->GetName(); // access the meta-data
        // construct the path to the .aiz file so that is goes temporarily in the project
        // folder this .aiz file is erased below after DoUnPackDocument is called on it
		pathName = gpApp->m_workFolderPath + gpApp->PathSeparator + unzipFileName;
		// get a buffered output stream
		wxFFileOutputStream outFile(pathName);
		// write out the filtered (unzipped) stream to the .aiz file
		outFile.Write(zipStream); // this form writes from zipStream to outFile until a 
								  // stream "error" (i.e., end of file)
		delete pEntry; // example in wx book shows the zip entry data being deleted
		outFile.Close();

		// get a CFile and do the unpack
		wxFile file;
		// In the wx version we need to explicitly call Open on the file to proceed.
		if (!file.Open(pathName,wxFile::read))
		{
			wxString msg;
			msg = msg.Format(_(
"Error uncompressing; cannot open the file: %s\n Make sure the file is not being used by another application and try again."),
			pathName.c_str());
			wxMessageBox(msg,_T(""),wxICON_WARNING);
			return;
		}
		if (!DoUnpackDocument(&file))//whm changed this to return bool for better error recovery
			return; // DoUnpackDocument issues its own error messages if it encounters an error

		// lastly remove the .aiz temporary file that was used to unpack from
		// leaving the compressed .aip in the work folder
		if (!::wxRemoveFile(pathName))
		{
			// if there was an error, we just get no unpack done, but app can continue; and
			// since we expect no error here, we will use an English message
			wxString strMessage;
			strMessage = strMessage.Format(_("Error removing %s after unpack document command."),
			pathName.c_str());
			wxMessageBox(strMessage,_T(""), wxICON_EXCLAMATION);
			return;
		}
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		TRUE if the unpack operation was successful, FALSE otherwise
/// \param		pFile	-> pointer to the wxFile object being unpacked
/// \remarks
/// Called from: the Doc's OnFileUnpackDocument().
/// Does the main work of unpacking the uncompressed file received from OnFileUnpackDocument().
/// It creates the required folder structure (including project folder) or 
/// makes that project current if it already exists on the destination machine, and then 
/// updates its project configuration file, and stores the xml document file in whichever of 
/// the Adaptations folder or one of its book folders if pack was done from a book folder, 
/// and then reads in the xml document, parses it and sets up the document and view leaving 
/// the user in the project and document ready to do work. 
/// If a document of the same name already exists on the destination machine in 
/// the same folder, the user is warned before the existing doc is overwritten by the document 
/// extracted from the packed file.
/// The .aip files pack with the Unicode version of Adapt It cannot be unpacked with the regular
/// version of Adapt It, nor vice versa.
///////////////////////////////////////////////////////////////////////////////
bool CAdapt_ItDoc::DoUnpackDocument(wxFile* pFile) // whm changed to return bool 22Jul06
{
	CAdapt_ItView* pView = gpApp->GetView();

	// get the file size
	int nFileSize = (int)pFile->Length();
	int nResult;

	// create a CBString with an empty buffer large enough for all this file
	CBString packByteStr;
	char* pBuff = packByteStr.GetBuffer(nFileSize + 1);

	// read in the file & close it
	int nReadBytes = pFile->Read(pBuff,nFileSize);
	if (nReadBytes < nFileSize)
	{
		wxMessageBox(_T(
"Compressed document file read was short, some data missed so abort the command.\n"),
			_T(""), wxICON_EXCLAMATION);
		return FALSE; 
	}
	pFile->Close(); // assume no errors

	// get the length (private member) set correctly for the CBString
	packByteStr.ReleaseBuffer();

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// whm Note 19Jan06: 
    // If the design included the addition of a warning string embedded in the compressed
    // file, and/or we wished to uncompress the data within an internal buffer, the
    // following considerations would need to be taken into account:
    // 1. Remove the Warning statement (see OnFilePackDoc) from the packByteStr which
    //    consists of the first 192 bytes of the file.
    // 2. The zlib inflate (decompression) call must be made at this point if executed on
    //    the compressed part of packByteStr expanding and uncompressing the data to a
    //    larger work buffer (CBString???), and the buffer receiving the uncompressed data
    //    needs to be used instead of the packByteStr one used below.
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    // Start extracting info. First we want to know whether Adapt It or Adapt It Unicode
    // did the packing? If our unpacking app is not same one, warn the user and disallow
    // the unpacking (The first byte will be an ascii 1 or 2, 1 for regular app did the
    // pack, 2 if unicode app did it)
	int offset = 0;
	char chDigit[2] = {'\0','\0'};

    // if the user has used winzip to unpack the packed doc file, maybe edited it, then
    // used Winzip to zip it and then changed the extension to .aip, Winzip (possibly only
    // if it detects UTF-8 data in the file, but I'm not sure of this) adds 3 bytes to the
    // start of the file - an i followed by a double right wedge character followed by an
    // upside down question mark character (as unsigned chars these have values of 239,
    // 187, and 191, respectively). This is all that prevents Adapt It from being able to
    // correctly unpack such a document - so to make it able to do so, instead of assuming
    // that the first character is a digit, we will scan till we find the first digit
    // (which will be 1 or 2) and then proceed with our unpack.
	unsigned char aOne = 49; // ASCII for digit 1 (isdigit() depends on locale, so 
							 // is unsafe to use here)
	unsigned char aTwo = 50; // ASCII for 2
	unsigned char charAtOffset = pBuff[offset];
	int limit = 7; // look at no more than first 6 bytes
	int soFar = 1;
	bool bFoundOneOrTwo = FALSE;
	while (soFar <= limit)
	{
		if (charAtOffset == aOne || charAtOffset == aTwo)
		{
			// we've successfully scanned past Winzip's inserted material
			offset = soFar - 1;
			bFoundOneOrTwo = TRUE;
			break;
		}
		else
		{
			// didn't find a 1 or 2, so try next byte
			soFar++;
			charAtOffset = pBuff[soFar - 1];
		}
	}
	if (!bFoundOneOrTwo)
	{
		// IDS_UNPACK_INVALID_PREDATA
		wxMessageBox(_T(
"Unpack failure. The uncompressed data has more than six unknown bytes preceding the digit 1 or 2, making interpretation impossible. Command aborted."),
		_T(""), wxICON_WARNING);
		return FALSE;
	}

    // we found a digit either at the start or no further than 7th byte from start, so
    // assume all is well formed and proceed from the offset value
	chDigit[0] = packByteStr[offset];
	offset += 6;
	packByteStr = packByteStr.Mid(offset); // remove extracted app type code 
										   // char & its |*0*| delimiter
	
    // whm Note: The legacy logic doesn't work cross-platform! The sizeof(char) and
    // sizeof(w_char) is not constant across platforms. On Windows sizeof(char) is 1 and
    // sizeof(w_char) is 2; but on all Unix-based systems (i.e., Linux and Mac OS X) the
    // sizeof(char) is 2 and sizeof(w_char) is 4. We can continue to use '1' to indicate
    // the file was packed by an ANSI version, and '2' to indicate the file was packed by
    // the Unicode app. However, the numbers cannot signify the size of char and w_char
    // across platforms. They can only be used as pure signals for ANSI or Unicode contents
    // of the packed file. Here in DoUnpackDocument we have to treat the string _T("1") as
    // an error if we're unpacking from within a Unicode app, or the string _T("2") as an
    // error if we're unpacking from an ANSI app.
    // In OnFilePackDoc() we simply pack the file using "1" if we are packing it from an
    // ANSI app; and we pack the file using "2" when we're packing it from a Unicode app,
    // regardless of the result returned by the sizeof operator on wxChar.
	//
	int nDigit = atoi(chDigit);
	// are we in the same app type?
#ifdef _UNICODE
	// we're a Unicode app and the packed file is ANSI, issue error and abort
	if (nDigit == 1)
#else
	// we're an ASNI app and the packed file is Unicode, issue error and abort
	if (nDigit == 2)
#endif
	{
		// mismatched application types. Doc data won't be rendered right unless all text was
		// ASCII 
		CUnpackWarningDlg dlg(gpApp->GetMainFrame());
		if (dlg.ShowModal() == wxID_OK)
		{
            // the only option is to halt the unpacking (there is only the one button);
            // because unicode app's config file format for punctuation is not compatible
            // with the regular app's config file (either type), and so while the doc would
            // render okay if ascii, the fact that the encapsulated config files are
            // incompatible makes it not worth allowing the user to continue
			return FALSE;
		}
	}

    // close the document and project currently open, clear out the book mode information
    // to defaults and the mode off, store and then erase the KBs, ready for the new
    // project and document being unpacked
	pView->CloseProject();

	// extract the rest of the information needed for setting up the document
	offset = packByteStr.Find("|*1*|");
	CBString srcName = packByteStr.Left(offset);
	offset += 5;
	packByteStr = packByteStr.Mid(offset); // remove the extracted source 
										   // language name & delimiter
	offset = packByteStr.Find("|*2*|");
	CBString tgtName = packByteStr.Left(offset);
	offset += 5;
	packByteStr = packByteStr.Mid(offset); // remove the extracted target 
										   // language name & delimiter
	offset = packByteStr.Find("|*3*|");
	CBString bookInfo = packByteStr.Left(offset);
	offset += 5;
	packByteStr = packByteStr.Mid(offset); // remove the extracted book 
										   // information & delimiter

    // offset now points at the start of the UTF-8 current output filename; if the packing
    // was done in the Unicode application, this will have to be converted back to UTF-16
    // further down before we make use of it

    // The book information has colon delimited subfields. Even in the unicode application
    // we can reliably compute from the char string without having to convert to UTF-16, so
    // do it now.
	int nFound = bookInfo.Find(':');
	wxASSERT(nFound);
	CBString theBool = bookInfo.Left(nFound);
	if (theBool == "1")
	{
		gpApp->m_bBookMode = TRUE;
	}
	else
	{
		gpApp->m_bBookMode = FALSE;
	}
	nFound++;
	bookInfo = bookInfo.Mid(nFound);
	nFound = bookInfo.Find(':');
	theBool = bookInfo.Left(nFound);
	if (theBool == "1")
	{
		gpApp->m_bDisableBookMode = TRUE;
	}
	else
	{
		gpApp->m_bDisableBookMode = FALSE;
	}
	nFound++;
	CBString theIndex = bookInfo.Mid(nFound);
	gpApp->m_nBookIndex = atoi(theIndex.GetBuffer()); // no ReleaseBuffer call 
            // is needed the later SetupDirectories() call will create the book folders
            // if necessary, and set up the current book folder and its path from the
            // m_nBookIndex value

	// extract the UTF-8 form of the current output filename
	offset = packByteStr.Find("|*4*|");
	CBString utf8Filename = packByteStr.Left(offset);
	offset += 5;
	packByteStr = packByteStr.Mid(offset); // remove the extracted utf-8 filename 
			// & delimiter (what remains is the project config file & document file)

    // we could be in the Unicode application, so we here might have to convert our srcName
    // and tgtName CBStrings into (Unicode) CStrings; we'll delay setting
    // m_curOutputFilename (using storeFilenameStr) to later on, to minimize the potential
    // for unwanted erasure.
#ifdef _UNICODE
	wxString sourceName;
	wxString targetName;
	wxString storeFilenameStr;
	gpApp->Convert8to16(srcName,sourceName);
	gpApp->Convert8to16(tgtName,targetName);
	gpApp->Convert8to16(utf8Filename,storeFilenameStr);
#else
	wxString sourceName = srcName.GetBuffer();
	wxString targetName = tgtName.GetBuffer();
	wxString storeFilenameStr(utf8Filename);
#endif

	// we now can set up the directory structures, if they are not already setup
	gpApp->m_sourceName = sourceName;
	gpApp->m_targetName = targetName;
	gpApp->m_bUnpacking = TRUE; // may be needed in SetupDirectories() 
								// if destination machine has same project folder
	bool bSetupOK = gpApp->SetupDirectories();
	if (!bSetupOK)
	{
		gpApp->m_bUnpacking = FALSE;
		wxMessageBox(_T(
"SetupDirectories returned false for Unpack Document.... The command will be ignored.\n"),
		_T(""), wxICON_EXCLAMATION);
		return FALSE;
	}
	gpApp->m_bUnpacking = FALSE;

    // check for the same document already in the project folder - if it's there, then ask
    // the user whether or not to have the being-unpacked-one overwrite it
	wxFile f;

	// save various current paths so they can be restored if the user bails out because of
	// a choice not to overwrite an existing document with the one being unpacked
	wxString saveMFCfilename = GetFilename(); // m_strPathName is internal to MFC's doc-view
	wxString saveBibleBooksFolderPath = gpApp->m_bibleBooksFolderPath;
	wxString saveCurOutputFilename = gpApp->m_curOutputFilename;
	wxString saveCurAdaptionsPath = gpApp->m_curAdaptionsPath;
	wxString saveCurOutputPath = gpApp->m_curOutputPath;

	// set up the paths consistent with the unpacked info
	gpApp->m_curOutputFilename = storeFilenameStr;
	if (gpApp->m_bBookMode && !gpApp->m_bDisableBookMode)
	{
		// m_strPathName is a member of MFC's Document class
		SetFilename(gpApp->m_bibleBooksFolderPath + gpApp->PathSeparator + 
														gpApp->m_curOutputFilename,TRUE);
		gpApp->m_curOutputPath = gpApp->m_bibleBooksFolderPath + gpApp->PathSeparator + 
														gpApp->m_curOutputFilename;
	}
	else
	{
		SetFilename(gpApp->m_curAdaptionsPath + gpApp->PathSeparator + 
														gpApp->m_curOutputFilename,TRUE);
		gpApp->m_curOutputPath = gpApp->m_curAdaptionsPath + gpApp->PathSeparator + 
														gpApp->m_curOutputFilename;
	}
	
    // if the document does not exist in the unpacking computer yet, then an attempt to get
    // its status struct will return FALSE - we need to check both possible paths
	bool bAskUser = FALSE;
	bool bItsXML = TRUE;
	if (::wxFileExists(gpApp->m_curOutputPath)) //if (f.GetStatus(m_curOutputPath,status))
	{
		// the xml document file is in the folder, so the user must be asked 
		// whether to overwrite or not
		bAskUser = TRUE;
	}

	wxString s1,s2,s3,msg;
	wxFileName fn(gpApp->m_curOutputPath);
	if (bAskUser)
	{
		//IDS_UNPACK_ASK_OVERWRITE
		s1 = _(
"There is a document of the same name in an Adapt It project of the same name on this computer.");
		s2 = s2.Format(_("\n      Document name: %s"),fn.GetFullName().c_str());
		s3 = s3.Format(_("\n      Project path : %s"),fn.GetPath().c_str());
		msg = msg.Format(_(
"%s%s%s\nDo you want the document being unpacked to overwrite the one already on this computer?"),
		s1.c_str(),s2.c_str(),s3.c_str());
		nResult = wxMessageBox(msg, _T(""), wxYES_NO | wxICON_INFORMATION);
		if(nResult  == wxYES)
		{
			// user wants the current file overwritten...
            // we have a valid status struct for it, so use it to just remove it here;
            // doing it this way we can be sure we get rid of it; we can't rely on the
            // CFile::modeCreate style bit causing the file to be emptied first because the
            // being-unpacked doc will be xml but the existing doc file on the destination
            // machine might be binary (ie. have .adt extension)
			if (bItsXML)
			{
				if (!::wxRemoveFile(gpApp->m_curOutputPath))
				{
					wxString thismsg;
					thismsg = thismsg.Format(_(
					"Failed removing %s before overwrite."), 
					gpApp->m_curOutputPath.c_str());
					wxMessageBox(thismsg,_T(""),wxICON_WARNING);
					goto a; // restore paths & exit, allow app to continue
				}
			}
		}
		else
		{
			// abort the unpack - this means we should restore all the saved path strings
			// before we return
a:			SetFilename(saveMFCfilename,TRUE); //m_strPathName = saveMFCfilename;
			gpApp->m_bibleBooksFolderPath = saveBibleBooksFolderPath;
			gpApp->m_curOutputFilename = saveCurOutputFilename;
			gpApp->m_curAdaptionsPath = saveCurAdaptionsPath;
			gpApp->m_curOutputPath = saveCurOutputPath;

			// restore the earlier document to the main window, if we have a valid path to
			// it
			wxString path = gpApp->m_curOutputPath;
			path = MakeReverse(path);
			int nFound1 = path.Find(_T("lmx."));
			if (nFound1 == 0) 
			{ 
                // m_curOutputPath is a valid path to a doc in a project's Adaptations or
                // book folder so open it again - the project is still in effect
				bool bGotItOK = OnOpenDocument(gpApp->m_curOutputPath);
				if (!bGotItOK)
				{
                    // some kind of error -- don't warn except for a beep, just leave the
                    // window blank (the user can instead use wizard to get a doc open)
					::wxBell();
				}
			}
			return FALSE;
		}
	}

	// if we get to here, then it's all systems go for updating the configuration file and
	// loading in the unpacked document and displaying it in the main window

	// next we must extract the embedded project configuration file
	offset = packByteStr.Find("|*5*|");
	CBString projConfigFileStr = packByteStr.Left(offset);
	offset += 5;
	packByteStr = packByteStr.Mid(offset); // remove the extracted configuration file 
                    // information & delimiter & the remainder now in packByteStr is the
                    // xml document file

	// construct the path to the project's configuration file so it can be saved to the 
	// project folder
	wxString projectPath = gpApp->m_curProjectPath;
	projectPath += gpApp->PathSeparator;
	projectPath += szProjectConfiguration;
	projectPath += _T(".aic");

    // temporarily rename any project file of this name already in the project folder - if
    // the new one fails to be written out, we must restore this renamed one before we
    // return to the caller, but if the new one is written out we must then delete this
    // renamed one
	wxString renamedPath;
	renamedPath.Empty();
	bool bRenamedConfigFile = FALSE;
	if (::wxFileExists(projectPath))
	{
		// do the renaming
		renamedPath = projectPath;
		int len = projectPath.Length();
		renamedPath.Remove(len-3,3); // delete the aic extension
		renamedPath += _T("BAK"); // make it a 'backup' type temporarily in case user 
								  // ever sees it in Win Explorer
		if (!::wxRenameFile(projectPath,renamedPath))
		{
			wxString message;
			message = message.Format(_(
			"Error renaming earlier configuration file with path %s."),
			projectPath.c_str());
			message += _("  Aborting the command.");
			wxMessageBox(message, _T(""), wxICON_INFORMATION);
			goto a;
		}
		bRenamedConfigFile = TRUE;
	}

	// get the length of the project configuration file's contents (exclude the null byte)
	int nFileLength = projConfigFileStr.GetLength();

    // write out the byte string (use CFile to avoid CStdioFile's mucking around with \n
    // and \r) because a config file written by CStdioFile's WriteString() won't be read
    // properly subsequently
	wxFile ff;
	if(!ff.Open(projectPath, wxFile::write))
	{
		wxString msg;
		msg = msg.Format(_(
"Unable to open the file for writing out the UTF-8 project configuration file, with path:\n%s"),
		projectPath.c_str());
		wxMessageBox(msg,_T(""), wxICON_EXCLAMATION);

		// if we renamed the earlier config file, we must restore its name before returning
		if (bRenamedConfigFile)
		{
			if (::wxFileExists(renamedPath))
			{
                // there is a renamed project config file to be restored; paths calculated
                // above are still valid, so just reverse their order in the parameter
                // block
				if (!::wxRenameFile(renamedPath,projectPath))
				{
					wxString message;
					message = message.Format(_(
					"Error restoring name of earlier configuration file with path %s."),
						renamedPath.c_str());
					message += _(
	"  Exit Adapt It and manually change .BAK to .aic for the project configuration file.");
					wxMessageBox(message,_T(""),wxICON_INFORMATION);
					goto a;
				}
			}
		}
		goto a;
	}

	// output the configuration file's content string
	char* pBuf = projConfigFileStr.GetBuffer(); // whm moved here from outer block above
	if (!ff.Write(pBuf,nFileLength))
	{
		// notify user, and then return without doing any more
		wxString thismsg;
		thismsg = _("Writing out the project configuration file's content failed.");
		wxString someMore;
		someMore = _(
" Exit Adapt It, and in Windows Explorer manually restore the .BAK extension on the renamed project configuration file to .aic before launching again. Beware, that configuration file may now be corrupted.");
		thismsg += someMore;
		wxMessageBox(thismsg);
		ff.Close();
		goto a;
	}
	projConfigFileStr.ReleaseBuffer(); // whm added 19Jun06

    // if control got here, we must remove the earlier (now renamed) project configuration
    // file, provided that we actually did find one with the same name earlier and renamed
    // it
	if (bRenamedConfigFile)
	{
		if (!::wxRemoveFile(renamedPath))
		{
			wxString thismsg;
			thismsg = _(
"Removing the renamed earlier project configuration file failed. Do it manually later in Windows Explorer - it has a .BAK extension.");
			wxMessageBox(thismsg,_T(""),wxICON_WARNING);
			goto a;
		}
	}

	// close the file
	ff.Close();

	// empty the buffer contents which are no longer needed
	projConfigFileStr.Empty();

    // we now need to parse in the configuration file, so the source user's settings are
    // put into effect; and we reset the KB paths. The KBs have already been loaded, or
    // stubs created, so the SetupKBPathsEtc call here just has the effect of getting paths
    // set up for the xml form of KB i/o.
	gpApp->GetProjectConfiguration(gpApp->m_curProjectPath); // has flag side effect as 
															// noted in comments above
	gpApp->SetupKBPathsEtc();

    // now we can save the xml document file to the destination folder (either Adaptations
    // or a book folder), then parse it in and display the document in the main window.

	// write out the xml document file to the folder it belongs in and with the same
	// filename as on the source machine (path is given by m_curOutputPath above)
	nFileLength = packByteStr.GetLength();
	if(!ff.Open(gpApp->m_curOutputPath, wxFile::write))
	{
		wxString msg;
		msg = msg.Format(_(
		"Unable to open the xml text file for writing to doc folder, with path:\n%s"),
		gpApp->m_curOutputPath.c_str());
		wxMessageBox(msg,_T(""),wxICON_EXCLAMATION);
		return FALSE; // don't goto a; instead leave the new paths intact because the 
                // config file is already written out, so the user can do something in the
                // project if he wants
	}
	pBuf = packByteStr.GetBuffer(); // whm moved here from outer block above (local scope)
	if (!ff.Write(pBuf,nFileLength)) 
	{
		// notify user, and then return without doing any more
		wxString thismsg;
		thismsg = _("Writing out the xml document file's content failed.");
		wxMessageBox(thismsg,_T(""),wxICON_WARNING);
		ff.Close();
		return FALSE;
	}
	packByteStr.ReleaseBuffer(); // whm added 19Jun06
	ff.Close();

	// empty the buffer contents which are no longer needed
	packByteStr.Empty();

	// now parse in the xml document file, setting up the view etc
	bool bGotItOK = OnOpenDocument(gpApp->m_curOutputPath);
	if (!bGotItOK)
	{
		// some kind of error --warn user (this shouldn't happen)
		wxString thismsg;
		thismsg = _(
"Opening the xml document file for Unpack Document... failed. (It was stored successfully on disk. Try opening it with the Open command on the File menu.)");
		wxMessageBox(thismsg,_T(""),wxICON_WARNING);
        // just proceed, there is nothing smart that can be done. Visual inspection of the
        // xml document file is possible in Windows Explorer if the user wants to check out
        // what is in it. A normal Open command can also be tried too.
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
/// \return		a wxString representing the path of the current working directory/folder
/// \remarks
/// Called from: the App's OnFileRestoreKb(), WriteConfigurationFile(), 
/// AccessOtherAdaptionProject(), the View's OnRetransReport() and CMainFrame's 
/// SyncScrollReceive().
/// Gets the path of the current working directory/folder as a wxString.
///////////////////////////////////////////////////////////////////////////////
wxString CAdapt_ItDoc::GetCurrentDirectory()
{
	// In wxWidgets it is simply:
	return ::wxGetCwd();
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the Advanced Menu is about
///                         to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected, and before
/// the menu is displayed.
/// If Vertical Editing is in progress it disables "Receive Synchronized Scrolling Messages" item 
/// on the Advanced menu and this handler returns immediately. Otherwise, it enables the 
/// "Receive Synchronized Scrolling Messages" item on the Advanced menu as long as a project
/// is open.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateAdvancedReceiveSynchronizedScrollingMessages(wxUpdateUIEvent& event)
{
	if (gbVerticalEditInProgress)
	{
		event.Enable(FALSE);
		return;
	}
	// the feature can be enabled only if we are in a project
	event.Enable(gpApp->m_bKBReady && gpApp->m_bGlossingKBReady);
#ifndef __WXMSW__
	event.Enable(FALSE); // sync scrolling not yet implemented on Linux and the Mac
#endif
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		event	-> unused wxCommandEvent
/// \remarks
/// Called from: the Advanced menu's "Receive Synchronized Scrolling Messages" selection.
/// is open. Toggles the menu item's check mark on and off.
/// whm modified 21Sep10 to make safe for when selected user profile removes this menu item.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnAdvancedReceiveSynchronizedScrollingMessages(wxCommandEvent& WXUNUSED(event))
{
	CMainFrame* pFrame = gpApp->GetMainFrame();
	wxASSERT(pFrame != NULL);
	wxMenuBar* pMenuBar = pFrame->GetMenuBar();
	wxASSERT(pMenuBar != NULL);
	wxMenuItem * pAdvancedMenuReceiveSSMsgs = 
				pMenuBar->FindItem(ID_ADVANCED_RECEIVESYNCHRONIZEDSCROLLINGMESSAGES);
	//wxASSERT(pAdvancedMenuReceiveSSMsgs != NULL);

	// toggle the setting
	if (!gbIgnoreScriptureReference_Receive)
	{
		// toggle the checkmark to OFF
		if (pAdvancedMenuReceiveSSMsgs != NULL)
		{
			pAdvancedMenuReceiveSSMsgs->Check(FALSE);
		}
		gbIgnoreScriptureReference_Receive = TRUE;
	}
	else
	{
		// toggle the checkmark to ON
		if (pAdvancedMenuReceiveSSMsgs != NULL)
		{
			pAdvancedMenuReceiveSSMsgs->Check(TRUE);
		}
		gbIgnoreScriptureReference_Receive = FALSE;
	}
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the Advanced Menu is about
///                         to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected, and before
/// the menu is displayed.
/// Enables the "Send Synchronized Scrolling Messages" item on the Advanced menu if a project
/// is open.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateAdvancedSendSynchronizedScrollingMessages(wxUpdateUIEvent& event)
{
	// the feature can be enabled only if we are in a project
	event.Enable(gpApp->m_bKBReady && gpApp->m_bGlossingKBReady);
#ifndef __WXMSW__
	event.Enable(FALSE); // sync scrolling not yet implemented on Linux and the Mac
#endif
}

///////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param		event	-> unused wxCommandEvent
/// \remarks
/// Called from: the Advanced menu's "Send Synchronized Scrolling Messages" selection.
/// is open. Toggles the menu item's check mark on and off.
/// whm modified 21Sep10 to make safe for when selected user profile removes this menu item.
///////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnAdvancedSendSynchronizedScrollingMessages(wxCommandEvent& WXUNUSED(event))
{
	CMainFrame* pFrame = gpApp->GetMainFrame();
	wxASSERT(pFrame != NULL);
	wxMenuBar* pMenuBar = pFrame->GetMenuBar();
	wxASSERT(pMenuBar != NULL);
	wxMenuItem * pAdvancedMenuSendSSMsgs = 
				pMenuBar->FindItem(ID_ADVANCED_SENDSYNCHRONIZEDSCROLLINGMESSAGES);
	//wxASSERT(pAdvancedMenuSendSSMsgs != NULL);

	// toggle the setting
	if (!gbIgnoreScriptureReference_Send)
	{
		// toggle the checkmark to OFF
		if (pAdvancedMenuSendSSMsgs != NULL)
		{
			pAdvancedMenuSendSSMsgs->Check(FALSE);
		}
		gbIgnoreScriptureReference_Send = TRUE;
	}
	else
	{
		// toggle the checkmark to ON
		if (pAdvancedMenuSendSSMsgs != NULL)
		{
			pAdvancedMenuSendSSMsgs->Check(TRUE);
		}
		gbIgnoreScriptureReference_Send = FALSE;
	}
}

// whm Modified 9Feb2004, to enable consistency checking of currently open document or,
// alternatively, select multiple documents from the project to check for consistency. If a
// document is open when call is made to this routine, the consistency check is completed
// and the user can continue working from the same position in the open document.
// BEW 12Apr10, no changes for support of doc version 5
// BEW 7July10, no changes for support of kbVersion 2 (but there may be changes needed in
// the DoConsistencyCheck() function which is called from several places here)
void CAdapt_ItDoc::OnEditConsistencyCheck(wxCommandEvent& WXUNUSED(event))
{
	// the 'accepted' list holds the document filenames to be used
	CAdapt_ItApp* pApp = (CAdapt_ItApp*)&wxGetApp();
	pApp->m_acceptedFilesList.Clear();

    // BEW added 01Aug06 Support for Book Mode was absent in 3.2.1 and earlier, but it is
    // now added here & below. For Book Mode, not all Bible book folders will be scanned,
    // instead, the check is done on the current doc, or on all the docs in the single
    // current book folder. To check other book folders, the user must first change to one,
    // and then the same two options will be available there.
	wxString dirPath;
	if (pApp->m_bBookMode && !pApp->m_bDisableBookMode)
		dirPath = pApp->m_bibleBooksFolderPath;
	else
		dirPath = pApp->m_curAdaptionsPath;
	bool bOK = ::wxSetWorkingDirectory(dirPath); // ignore failures

	// BEW added 05Jan07 to enable work folder on input to be restored when done
	wxString strSaveCurrentDirectoryFullPath = dirPath;

	// Determine if a document is currently open with data to check
	if (!pApp->m_pSourcePhrases->GetCount() == 0)
	{
		// A document is open with data to check, therefore see if
		// user wants to only check the open document or to select
		// from a list of all documents in the current project to be
		// checked.
		// Save current path and doc name for use in re-opening below
		wxString pathName = pApp->m_curOutputPath;
		wxString docName = pApp->m_curOutputFilename;

		// Save the phrase box's current position in the file
		int currentPosition = pApp->m_nActiveSequNum;

		// Put up the Choose Consistency Check Type dialog
		CChooseConsistencyCheckTypeDlg ccDlg(pApp->GetMainFrame());
		if (ccDlg.ShowModal() == wxID_OK) 
		{
			// handle user's choice of consistency check type
			if (ccDlg.m_bCheckOpenDocOnly)
			{
				// user want's to check only the currently open doc...

                // Save the Doc (and DoFileSave() also automatically saves, without backup,
                // both the glossing and adapting KBs)
				// BEW changed 29Apr10 to use DoFileSave_Protected() which gives better
				// protection against data loss in the event of a failure
				bool fsOK = DoFileSave_Protected(TRUE); // TRUE - show the wait/progress dialog
				if (!fsOK)
				{
					// something's real wrong!
					wxMessageBox(_(
					"Could not save the current document. Consistency Check Command aborted."),
					_T(""), wxICON_EXCLAMATION);
                    // whm note 5Dec06: Since EnumerateDocFiles has not yet been called the
                    // current working directory has not changed, so no need here to reset
                    // it before return.
					return;
				}

                // BEW added 01Aug06, ensure the current document's contents are removed,
                // otherwise we will get a doubling of the doc data when OnOpenDocument()
                // is called because the latter will append to whatever is in
                // m_pSourcePhrases, so the latter list must be cleared to avoid the data
                // doubling bug
				pApp->GetView()->ClobberDocument();

				// Ensure that our current document is the only doc in the accepted files list
				pApp->m_acceptedFilesList.Clear();
				pApp->m_acceptedFilesList.Add(docName);

				// do the consistency check on the doc
				DoConsistencyCheck(pApp);
				pApp->m_acceptedFilesList.Clear();
			}
			else
			{
				// User wants to check a selection of docs in current project.
				// This is like the multi-document type consistency check, except
				// that, in this case, there is a currenly open document.

                // BEW changed 01Aug06 Save the current doc and then clear out its contents
                // -- see block above for explanation of why this is necessary
                // BEW changed 29Apr10 to give better data protection 
				bool fsOK = DoFileSave_Protected(TRUE); // TRUE - show wait/progress dialog
				if (!fsOK)
				{
					// something's real wrong!
					wxMessageBox(_(
					"Could not save the current document. Consistency Check Command aborted."),
					_T(""), wxICON_EXCLAMATION);
                    // whm note 5Dec06: Since EnumerateDocFiles has not yet been called the
                    // current working directory has not changed, so no need here to reset
                    // it before return.
					return;
				}
				pApp->GetView()->ClobberDocument();

                // Enumerate the doc files and do the consistency check 
                // whm note: EnumerateDocFiles() has the side effect of changing the current
                // work directory to the passed in dirPath.
				bOK = pApp->EnumerateDocFiles(this, dirPath);
				if (bOK)
				{
					if (pApp->m_acceptedFilesList.GetCount() == 0)
					{
						// nothing to work on, so abort the operation
						// IDS_NO_DOCUMENTS_YET
						wxMessageBox(_(
"Sorry, there are no saved document files yet for this project. At least one document file is required for the operation you chose to be successful. The command will be ignored."),
						_T(""),wxICON_EXCLAMATION);
                        // whm note 5Dec06: EnumerateDocFiles above changes the current
                        // work directory, so to be safe I'll reset it here before the
                        // consistency check returns to what it was on entry (the line
                        // below was not added in MFC version).
						bool bOK;
						bOK = ::wxSetWorkingDirectory(strSaveCurrentDirectoryFullPath);
						return;
					}
					DoConsistencyCheck(pApp);
				}
				pApp->m_acceptedFilesList.Clear();
			}
		}
		else
		{
			// user cancelled
            // whm note 5Dec06: Since EnumerateDocFiles has not yet been called the current
            // working directory has not changed, so no need here to reset it before
            // return.
			return;
		}

		// BEW added 05Jan07 to restore the former current working directory
		// to what it was on entry
		bool bOK;
		bOK = ::wxSetWorkingDirectory(strSaveCurrentDirectoryFullPath);

		// Re-Open the CurrentDocName to continue editing at the point where
		// the phrase box was at closure
		bool bOpenOK;
		bOpenOK = OnOpenDocument(pathName);
		SetFilename(pathName,TRUE);
		// Return the phrase box to the active sequence number; but if the box is not
		// in existence because the user got to the end of the document and invoked the
		// test from there, then reset to the initial position
		if (currentPosition == -1)
			currentPosition = 0;
		CPile* pPile = pApp->GetView()->GetPile(currentPosition);
		pApp->GetView()->Jump(pApp,pPile->GetSrcPhrase());

	}// end of if document is open with data to check
	else
	{
		// No document open. User selected Consistency Check intending to check
		// a selection of documents in the current project
        // whm note: EnumerateDocFiles() has the side effect of changing the current work
        // directory to the passed in dirPath.
        // 
		// BEW 9July10, if Book Mode is on, there was no way to have a consistency check
		// done over the whole set of documents in the 67 book mode folders, so I'm adding
		// the capability here. To get it to happen, book mode must be on, and no document
		// open, and then a yes/no message will come up asking the user if he wants the
		// check to be done over the contents of all the book folders. If he responds yes,
		// then we'll iterate though the list of bible book folders and do a consistency
		// check on all the files in each; if he responds no, then the current book folder
		// only is checked as was the case earlier.
		bool bOK;
		if (pApp->m_bBookMode && !pApp->m_bDisableBookMode && (pApp->m_pSourcePhrases->GetCount() == 0))
		{
			// ask the user if he wants the check done over all book folders
			wxASSERT(pApp->AreBookFoldersCreated(pApp->m_curAdaptionsPath));
			wxString message;
			message = message.Format(
_("Do you want the check to be done for all document files within all the book folders?"));
			int response = ::wxMessageBox(message, _("Consistency Check of all folders?"), wxYES_NO | wxICON_QUESTION);
			if (response == wxYES)
			{
				// user wants it done over all folders
				
				// DoConsistencyCheck() relies on the fact that EnumerateDocFiles() having
				// been called in prior to DoConsistencyCheck() being called will have set
				// the current working directory to be the book folder from which all the
				// documents within that folder are to be checked; because internally
				// DoConsistencyCheck()calls OnOpenDocument() and passes in, in the
				// latter's single parameter, not an absolute path as is normally the case, 
				// but just the filename of a file in the currently targetted book folder.
				// [ OnOpenDocument() also computes the correct m_curOutputPath correctly
				// using the bible book path and filename variables, but we don't use that
				// because we prefer to call DoFileSave_Protected() which will also do it
				// for us as well as doing the actual saving. ]
				// DoConsistencyCheck() then, after checking each doc file in the
				// targetted folder, calls DoFileSave_Protected() and the latter, if book
				// folder mode is turned on, uses the bible book folders supporting string
				// variables to calculate the correct path for saving the checked document.
				// Because our loop across the bible book folders will alter the current
				// values of the latter, we must therefore preserve them so that we can
				// restore the current settings when the loop completes. Though we may not
				// need to, we'll also save and afterwards restore m_curOutputPath,
				// m_curOutputFilename and m_curOutputBackupFilename.
				wxString save_currentOutputPath = pApp->m_curOutputPath;
				wxString save_currentOutputFilename = pApp->m_curOutputFilename;
				wxString save_currentOutputBackupFilename = pApp->m_curOutputBackupFilename;
				int save_nBookIndex = pApp->m_nBookIndex;
				BookNamePair* save_pCurrBookNamePair = pApp->m_pCurrBookNamePair;
				wxString save_bibleBooksFolderPath = pApp->m_bibleBooksFolderPath;

				wxString bookName;
				wxString folderPath;
				BookNamePair aBookNamePair;
				BookNamePair* pBookNamePair = &aBookNamePair;
				int nMaxBookFolders = (int)pApp->m_pBibleBooks->GetCount();
				int bookIndex;

				// we create the copy of the KB or glossingKB, as the case may be, only
				// once so that we don't have to do it every time we process the contents
				// of the next Bible book folder
				CKB* pKB;
				if (gbIsGlossing)
					pKB = pApp->m_pGlossingKB;
				else
					pKB = pApp->m_pKB;
				wxASSERT(pKB != NULL);
				CKB* pKBCopy = new CKB();
				pKBCopy->Copy(*pKB);

				// loop over the Bible book folders
				for (bookIndex = 0; bookIndex < nMaxBookFolders; bookIndex++)
				{
                    // NOTE: a limitation of this calling of DoConsistencyCheck() in a loop
                    // is that the user's potential choice to 'auto-fix later instances the
                    // same way' will only apply on a per-folder basis
					pBookNamePair = ((BookNamePair*)(*pApp->m_pBibleBooks)[bookIndex]);
					folderPath = pApp->m_curAdaptionsPath + pApp->PathSeparator + pBookNamePair->dirName;
					// setting the index, book name pair, and bibleBooksFolderPath on the
					// app class ensures that the internal call in DoConsistencyCheck() to
					// the DoSaveFile_Protected() will construct the right path to the
					// folder and file for saving the checked document.
					pApp->m_nBookIndex = bookIndex;
					pApp->m_pCurrBookNamePair = pBookNamePair;
					pApp->m_bibleBooksFolderPath = folderPath;
					// clear the list of files to be processed (actually its a wxArrayString)
					pApp->m_acceptedFilesList.Clear();
					// get a list of all the document files, and set the working directory to
					// the passed in path ( DoConsistencyCheck() internally relies on this
					// being set here to the correct folder )
					bOK = pApp->EnumerateDocFiles(this, folderPath, TRUE); // TRUE in this call
												// is boolean bSuppressDialog; we don't want the
												// user to have to choose which file(s) in a
												// dialog which may be shown as many as 67
												// times, in the bookFolders loop!
					DoConsistencyCheck(pApp, pKB, pKBCopy); // the overloaded 3-param version
				}
				// erase the copied CKB which is no longer needed
				wxASSERT(pKBCopy != NULL);
				EraseKB(pKBCopy); // don't want memory leaks!

				// restore path and bible book folders variables to be what they were
				// before we looped across all the book folders
				pApp->m_acceptedFilesList.Clear();
				pApp->m_curOutputPath = save_currentOutputPath; // restore the original output path
				pApp->m_curOutputFilename = save_currentOutputFilename; // ditto for the output filename
				pApp->m_curOutputBackupFilename = save_currentOutputBackupFilename;
				pApp->m_nBookIndex = save_nBookIndex;
				pApp->m_pCurrBookNamePair = save_pCurrBookNamePair;
				pApp->m_bibleBooksFolderPath = save_bibleBooksFolderPath;

				// restore working directory
				bOK = ::wxSetWorkingDirectory(strSaveCurrentDirectoryFullPath);
				return;
			} // end of TRUE block for test: if (response == wxYes)
		}
		// legacy code - do this only if book mode check over all folders is not wanted
		bOK = pApp->EnumerateDocFiles(this, dirPath);
		if (bOK)
		{
			if (pApp->m_acceptedFilesList.GetCount() == 0)
			{
				// nothing to work on, so abort the operation
				wxMessageBox(_(
"Sorry, there are no saved document files yet for this project. At least one document file is required for the operation you chose to be successful. The command will be ignored."),
				_T(""),wxICON_EXCLAMATION);
                // whm note 5Dec06: EnumerateDocFiles above changes the current work
                // directory, so to be safe I'll reset it here before the consistency check
                // returns to what it was on entry (the line below was not added in MFC
                // version).
				bool bOK;
				bOK = ::wxSetWorkingDirectory(strSaveCurrentDirectoryFullPath);
				return;
			}
			DoConsistencyCheck(pApp);
		}
		pApp->m_acceptedFilesList.Clear();
		
		// BEW added 05Jan07 to restore the former current working directory
		// to what it was on entry
		bOK = ::wxSetWorkingDirectory(strSaveCurrentDirectoryFullPath);
	}
}

/////////////////////////////////////////////////////////////////////////////////
/// \return		nothing
/// \param      event   -> the wxUpdateUIEvent that is generated when the Edit Menu is about
///                         to be displayed
/// \remarks
/// Called from: The wxUpdateUIEvent mechanism when the associated menu item is selected,
/// and before the menu is displayed. If the application is in Free Translation mode, or
/// Vertical Editing is in progress, or it is only showing the target language text, or the
/// active KB is not in a ready state, this handler disables the "Consistency Check..."
/// item in the Edit menu, otherwise it enables the "Consistency Check..." item on the Edit
/// menu.
/// BEW modified 13Nov09, disable the consistency check menu item when the flag
/// m_bReadOnlyAccess is TRUE (it must not be possible to initiate a consistency
/// check when visiting a remote user's project folder -- it involves KB modifications
/// and that would potentially lose data added to the remote user's remote KB by him)
/////////////////////////////////////////////////////////////////////////////////
void CAdapt_ItDoc::OnUpdateEditConsistencyCheck(wxUpdateUIEvent& event)
{
	CAdapt_ItApp* pApp = (CAdapt_ItApp*)&wxGetApp();
	if (pApp->m_bReadOnlyAccess)
	{
		event.Enable(FALSE);
		return;
	}
	if (gbVerticalEditInProgress)
	{
		event.Enable(FALSE);
		return;
	}
	if (pApp->m_bFreeTranslationMode)
	{
		event.Enable(FALSE);
		return;
	}
	if (gbShowTargetOnly)
	{
		event.Enable(FALSE);
		return;
	}
	//if (pDoc == NULL)
	//{
	//	event.Enable(FALSE);
	//	return;
	//}
	bool bKBReady = FALSE;
	if (gbIsGlossing)
		bKBReady = pApp->m_bGlossingKBReady;
	else
		bKBReady = pApp->m_bKBReady;
	// Allow Consistency Check... while document is open
	if (bKBReady)
	{
		event.Enable(TRUE);
	}
	else
	{
		event.Enable(FALSE);
	}
}

// This function assumes that the current directory will have already been set correctly
// before being called. This is the legacy version, it handles a single file from a single
// folder, or a list of files from a single folder. (A prior call of EnumerateDocFiles()
// with the boolean parameter default FALSE has to have been made prior to calling this
// function. The boolean is for suppressing an internal dialog, & being FALSE permits the
// user to select a subset of doc files to be processed -- using the dialog which
// EnumerateDocFiles() puts up. The latter call also sets the working directory, a fact
// which DoConsistencyCheck() relies on, as it calls OnOpenDocument() with only a filename
// as parameter, instead of an absolute path to that file.
// Modified, July 2003, for support of Auto Capitalization
// BEW 12Apr10, no changes needed for support of doc version 5
// BEW 17May10, moved to here from CAdapt_ItView
// BEW 8July10, updated for support of kbVersion 2, and fixed some bugs (eg. progress dialog)
// BEW 13Nov10, no changes needed to support Bob Eaton's request for glosssing KB to use all
// maps, but it calls StoreText() and the latter needed changes
void CAdapt_ItDoc::DoConsistencyCheck(CAdapt_ItApp* pApp)
{
	gbConsistencyCheckCurrent = TRUE; // turn on Flag to inhibit placement of phrase box
									  // initially when OnOpenDocument() is called
	CLayout* pLayout = GetLayout();
	wxASSERT(pApp != NULL);
	CKB* pKB;
	wxArrayString* pList = &pApp->m_acceptedFilesList;
	if (gbIsGlossing)
		pKB = pApp->m_pGlossingKB;
	else
		pKB = pApp->m_pKB;
	wxASSERT(pKB != NULL);
	int nCount = pList->GetCount();
	if (nCount <= 0)
	{
		// something is real wrong, but this should never happen 
		// so an English message will suffice
		wxString error;
		error = error.Format(_T(
		"Error, the file count was found to be %d, so the command was aborted."),nCount);
		wxMessageBox(error,_T(""), wxICON_WARNING);
		gbConsistencyCheckCurrent = FALSE;
		return;
	}
	wxASSERT(nCount > 0);
	int nTotal = 0;
	int nCumulativeTotal = 0;

	// create a list to hold pointers to auto-fix records, if user checks the auto fix checkbox
	// in the dlg
	AFList afList;

	// create a copy of the KB, we check the copy for inconsistencies, but do updating within
	// the current KB pointed to by the app's m_pKB pointer, or m_pGlossingKB when glossing
	CKB* pKBCopy = new CKB(); // can't use a copy constructor, couldn't get it to work years ago
							  // BEW 12May10 note: default constructor here does not set the
							  // private m_bGlossingKB member, the next line does that based
							  // on the setting for that member in pKB
	pKBCopy->Copy(*pKB);  // this is a work-around for lack of copy constructor - see KB.h

	// iterate over the document files
	bool bUserCancelled = FALSE; // whm note: Caution: This bUserCancelled overrides the scope 
								 // of the extern global of the same name
	int i;
	for (i=0; i < nCount; i++)
	{
		wxString newName = pList->Item(i);
		wxASSERT(!newName.IsEmpty());

        // for debugging- check pile count before & after (failure to close doc before
        // calling this function resulted in the following OnOpenDocument() call appending
        // a copy of the document's contents to itself -- the fix is to ensure
        // OnFileClose() is done in the caller before DoConsistencyCheck() is called
		// int piles = pApp->m_pSourcePhrases->GetCount();

		bool bOK;
		bOK = OnOpenDocument(newName); // passing in just a filename, so we are relying
									   // on the working directory having previously
									   // being set in the caller at the call of
									   // EnumerateDocFiles()
		SetFilename(newName,TRUE);
		nTotal = pApp->m_pSourcePhrases->GetCount();
		if (nTotal == 0)
		{
			wxString str;
			str = str.Format(_T("Bad file:  %s"),newName.c_str());
			wxMessageBox(str,_T(""),wxICON_WARNING);
		}
		nCumulativeTotal += nTotal;

		// put up a progress indicator
		wxString progMsg = _("%s  - %d of %d Total words and phrases");
		wxString msgDisplayed = progMsg.Format(progMsg,newName.c_str(),1,nTotal);
		wxProgressDialog progDlg(_("Consistency Checking"),
                        msgDisplayed,
                        nTotal,    // range
                        pApp->GetMainFrame(),   // parent
                        //wxPD_CAN_ABORT |
                        //wxPD_CAN_SKIP |
                        wxPD_APP_MODAL |
                        wxPD_AUTO_HIDE  //-- try this as well
                        //| wxPD_ELAPSED_TIME |
                        //wxPD_ESTIMATED_TIME |
                        //wxPD_REMAINING_TIME
                       // | wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
                       // but wxPD_SMOOTH is not supported (see wxGauge) on all platforms, so
                       // perhaps this is why Bill didn't get good behaviour on gtk or mac
                        );
		SPList* pPhrases = pApp->m_pSourcePhrases;
		SPList::Node* pos1; 
		pos1 = pPhrases->GetFirst();
		wxASSERT(pos1 != NULL);
		int counter = 0;
		while (pos1 != NULL)
		{
			CSourcePhrase* pSrcPhrase = (CSourcePhrase*)pos1->GetData();
			pos1 = pos1->GetNext();
			counter++;

			// check the KBCopy has the required association of key with translation
			// BEW 13Nov10, changes to support Bob Eaton's request for glosssing KB to use all maps
			int nWords;
			//if (gbIsGlossing)
			//	nWords = 1;
			//else
				nWords = pSrcPhrase->m_nSrcWords;
			CTargetUnit* pTU = NULL;
			bool bOK = TRUE;
			bool bFoundTgtUnit = TRUE;
			bool bFoundRefString = TRUE;

			// any inconsistency with a <Not In KB> entry can be fixed automatically,
			// and this block must be ignored when glossing is ON
			if (!gbIsGlossing && !pSrcPhrase->m_bRetranslation && 
				pSrcPhrase->m_bNotInKB && !pSrcPhrase->m_bHasKBEntry)
			{
				// the inconsistency we are fixing here is that the CSourcePhrase instance
				// has its m_bNotInKB flag set TRUE, but the KB lacks a "<Not In KB>"
				// entry (and if that is the case, then for kbVersion 2, possibly other
				// CRefString instances are for that KB entry are not 'deleted' (ie.
				// m_bDeleted flag is not yet set TRUE for them) -- so check and fix if
				// necessary 
				wxString str = _T("<Not In KB>");
				// do the lookup
				bOK = pKB->AutoCapsLookup(pKBCopy->m_pMap[nWords-1], pTU, pSrcPhrase->m_key);
				if (!bOK)
				{
					// fix it silently...
					pApp->m_bSaveToKB = TRUE; // ensure it gets stored
					pKB->StoreText(pSrcPhrase,str);
					pSrcPhrase->m_bHasKBEntry = FALSE;
					pSrcPhrase->m_bNotInKB = TRUE;
				}
				else
				{
                    // BEW 7July10, in kbVersion 2 another way there entry could be
                    // consistent is that it does contain "<Not In KB>" but somehow other
                    // CRefStrings on the pTU instance are not deleted - so we must fix
                    // that. Instead of checking for any non-deleted ones, just delete them
                    // all and re-store "<Not In KB>" -- this will be done then on entries
                    // where this fix is not required, but since <Not In KB> is used seldom
                    // or never, this won't matter
					if (pTU != NULL)
					{
						pTU->DeleteAllToPrepareForNotInKB();
					}
					pApp->m_bSaveToKB = TRUE; // ensure it gets stored
					pKB->StoreText(pSrcPhrase,str);
					pSrcPhrase->m_bHasKBEntry = FALSE;
					pSrcPhrase->m_bNotInKB = TRUE;
				}
			}

			bool bTheTest = FALSE;
			// define the test's value
			if (gbIsGlossing)
			{
				if (pSrcPhrase->m_bHasGlossingKBEntry)
				{
					bTheTest = TRUE;
				}
			}
			else // adapting
			{
				if (!pSrcPhrase->m_bRetranslation && !pSrcPhrase->m_bNotInKB &&
					pSrcPhrase->m_bHasKBEntry && pSrcPhrase->m_adaption != _T("<Not In KB>"))
				{
					bTheTest = TRUE;
				}
			}
			if (bTheTest)
			{
				// do the lookup
				bOK = pKBCopy->AutoCapsLookup(pKBCopy->m_pMap[nWords-1], pTU, pSrcPhrase->m_key);
				if (!bOK)
				{
					// there was no target unit for this key in the map, so this is an
					// inconsistency
					bFoundTgtUnit = FALSE;
					bFoundRefString = FALSE;
				}
				else
				{
					// the target unit is in the map, so check if there is a corresponding
					// refString for the m_adaption, or m_gloss, member of the source phrase
					wxASSERT(pTU);
					bool bMatched = FALSE;
					TranslationsList* pList = pTU->m_pTranslations; 
					wxASSERT(pList != NULL);
                    // For kbVersion 2 don't make this assertion. If the phrase box is at a
                    // place where there is a m_refCount of 1 which has just been deleted
                    // because of 'landing' the box there, and it had just the one
                    // CRefString, then the count of non-deleted ones can legitimately be
                    // temporarily zero. Leave this here as a reminder to the developer.
					//wxASSERT(pTU->CountNonDeletedRefStringInstances() > 0);
					wxString srcPhraseStr;
					if (gbIsGlossing)
					{
						srcPhraseStr = pSrcPhrase->m_gloss;
					}
					else
					{
						srcPhraseStr = pSrcPhrase->m_adaption;
					}
					if (!((gbAutoCaps && gbSourceIsUpperCase && gbMatchedKB_UCentry) || !gbAutoCaps))
					{
                        // do a change to lc only if it is needed - that is, attempt it
                        // when it is not the case that gbMatchedKB_UCentry is TRUE
                        // (because then we want an unmodified string to be used),
                        // otherwise attempt it when auto-caps is ON
						srcPhraseStr = pKBCopy->AutoCapsMakeStorageString(srcPhraseStr,FALSE);
					}
					TranslationsList::Node* pos = pList->GetFirst();
					wxASSERT(pos != NULL);
					while (pos != NULL)
					{
						CRefString* pRefString = (CRefString*)pos->GetData();
						pos = pos->GetNext();
						wxASSERT(pRefString != NULL);
						// BEW 7July10, added second test, as kbVersion 2 treats a CRefString
						// instance as present only if it is not marked as deleted
						if ((pRefString->m_translation == srcPhraseStr) && !pRefString->GetDeletedFlag())
						{
							// a matching gloss was found
							bMatched = TRUE;
							break;
						}
					}
					if (!bMatched)
					{
						// no match was made, so this is an inconsistency
						bFoundRefString = FALSE;
					}
				}
			}

			// open the dialog if we have an inconsistency
			if (!bFoundTgtUnit || !bFoundRefString)
			{
				// make the CSourcePhrase instance is able to have a KB entry added
				if (gbIsGlossing)
					pSrcPhrase->m_bHasGlossingKBEntry = FALSE;
				else
					pSrcPhrase->m_bHasKBEntry = FALSE;

				// hide the progress window
				progDlg.Hide(); 
                // work out if this is an auto-fix item, if so, don't show the dialog, but
                // use the stored AutoFixRecord to fix the inconsistency without user
                // intervention (note: any items for which the "Ignore it, I will fix it 
                // later" button was pressed cannot occur as auto-fix records) The next 100
                // lines could be improved - it was "added to" in a rather ad hoc fashion,
                // so its a bit spagetti-like... something to do sometime when there is
                // plenty of time!
				AutoFixRecord* pAFRecord = NULL;
				if (MatchAutoFixItem(&afList, pSrcPhrase, pAFRecord))
				{
					// we matched an auto-fix element, so do the fix automatically...
					// update the original kb (not pKBCopy)
					wxString tempStr = pAFRecord->finalAdaptation; // could have punctuation in it

					// if the adaptation is null, then assume user wants it that way and so store
					// an empty string
					if (tempStr.IsEmpty())
					{
						pKB->StoreText(pSrcPhrase,tempStr,TRUE); // TRUE = allow saving empty adaptation
					}
					else
					{
						if (!gbIsGlossing)
						{
							pApp->GetView()->RemovePunctuation(this,&tempStr,from_target_text); 
                                // we don't want punctuation in adaptation KB if
                                // autocapitalization is ON, we could have an upper case
                                // source, but the user may have typed lower case for
                                // fixing the gloss or adaptation, but this is okay - the
                                // store will work right, so don't need anything here
                                // except the call to store it
							pKB->StoreText(pSrcPhrase,tempStr);
						}
						// do the gbIsGlossing case when no punct is to be removed, in next block
					}
					if (!tempStr.IsEmpty())
					{
                        // here we must be careful; pAFRecord->finalAdaptation may have a
                        // lower case string when the source text has upper case, and the
                        // user is expecting the application to do the fix for him; this
                        // would be easy if we could be sure that the first letter of the
                        // string was at index == 0, but the possible presence of preceding
                        // punctuation makes the assumption dangerous - so we must find
                        // where the actual text starts and do any changes there if needed.
                        // tempStr has punctuation stripped out, pAFRecord->finalAdaptation
                        // doesn't, so start by determining if there actually is a problem
                        // to be fixed.
						if (gbAutoCaps)
						{
							bool bNoError = SetCaseParameters(pSrcPhrase->m_key);
							if (bNoError && gbSourceIsUpperCase)
							{
								bNoError = SetCaseParameters(tempStr,FALSE); // FALSE means "it's target text"
								if (bNoError && !gbNonSourceIsUpperCase &&
									(gcharNonSrcUC != _T('\0')))
								{
                                    // source is upper case but nonsource is lower and is a
                                    // character with an upper case equivalent - we have a
                                    // problem; we need to fix the AutoFixRecord's
                                    // finalAdaptation string, and the sourcephrase too. At
                                    // this point we can fix the m_adaption member as
                                    // follows:
									pSrcPhrase->m_adaption.SetChar(0,gcharNonSrcUC);
								}
							}
						}
                        // In the next if/else block, the non-glossing-mode call of
                        // MakeTargetStringIncludingPunctuation() accomplishes the setting
                        // of the pSrcPhrase's m_targetStr member, handling any needed
                        // lower case to upper case conversion (even when typed initial
                        // punctuation is present), and the punctuation override protocol
                        // if the passed in string in the 2nd parameter has initial and/or
                        // final punctuation.
						if (!gbIsGlossing)
						{
                            // for auto capitalization support,
                            // MakeTargetStringIncludingPunctuation( ) is now able to do
                            // any needed change to upper case initial letter even when
                            // there is initial punctuation on pAFRecord->finalAdaptation
							pApp->GetView()->MakeTargetStringIncludingPunctuation(pSrcPhrase, 
															pAFRecord->finalAdaptation);
						}
						else
						{
							// store, for the glossing ON case, the gloss text, 
							// with any punctuation
							pKB->StoreText(pSrcPhrase, pAFRecord->finalAdaptation);
							if (gbAutoCaps)
							{
                                 // upper case may be wanted, we have to do it on the first
                                // character past any initial punctuation; glossing mode
                                // doesn't do punctuation stripping and copying, but the
                                // user may have punctuation included in the inconsistency
                                // fixing string, so we have to check etc.
								wxString str = pAFRecord->finalAdaptation;
								// make a copy and remove punctuation from it
								wxString str_nopunct = str;
								pApp->GetView()->RemovePunctuation(this,&str_nopunct,from_target_text);
								// use the punctuation-less string to get the initial charact and
								// its upper case equivalent if it exists
								bool bNoError = SetCaseParameters(str_nopunct,FALSE);
															 // FALSE means "using target punct list"
								// span punctuation-having str using target lang's punctuation...
								wxString strInitialPunct = SpanIncluding(str,pApp->m_punctuation[1]);
															// use our own SpanIncluding in helpers
								int punctLen = strInitialPunct.Length();

								// work out if there is a case change needed, and set the
								// relevant case globals
								bNoError = SetCaseParameters(tempStr,FALSE);  
															// FALSE means "it's target text"
								if (bNoError && gbSourceIsUpperCase && !gbNonSourceIsUpperCase
									&& (gcharNonSrcUC != _T('\0')))
								{
									if (strInitialPunct.IsEmpty())
									{
                                        // there is no initial punctuation, so the change
                                        // to upper case can be done at the string's start
										pSrcPhrase->m_gloss.SetChar(0,gcharNonSrcUC);
									}
									else
									{
										// set it at the first character past the initial
										// punctuation
										str.SetChar(punctLen,gcharNonSrcUC);
										pSrcPhrase->m_gloss = str;
									}
								}
							}
							else
							{
								// no auto capitalization, so just use finalAdaptation 
								// string 'as is'
								pSrcPhrase->m_gloss = pAFRecord->finalAdaptation;
							}
						}
					}
					pApp->m_targetPhrase = pAFRecord->finalAdaptation; // any brief glimpse
							// of the box should show the current adaptation, or gloss, string
					// show the progress window again but don't update it here
					progDlg.Show(TRUE); 
				}
				else
				{
					// no match, so this is has to be handled with user intervention via
					// the dialog
					CConsistencyCheckDlg dlg(pApp->GetMainFrame());
					dlg.m_bFoundTgtUnit = bFoundTgtUnit;
					dlg.m_bDoAutoFix = FALSE;
					dlg.m_pApp = pApp;
					dlg.m_pKBCopy = pKBCopy;
					dlg.m_pTgtUnit = pTU; // could be null
					dlg.m_finalAdaptation.Empty(); // initialize final chosen adaptation or gloss
					dlg.m_pSrcPhrase = pSrcPhrase;

                    // update the view to show the location where this source pile is, and
                    // put the phrase box there ready to accept user input indirectly from
                    // the dialog
					int nActiveSequNum = pSrcPhrase->m_nSequNumber;
					wxASSERT(nActiveSequNum >= 0);
					pApp->m_nActiveSequNum = nActiveSequNum; // added 16Apr09, should be okay
					// and is needed because CLayout::RecalcLayout() relies on the
					// m_nActiveSequNum value being correct
#ifdef _NEW_LAYOUT
					pLayout->RecalcLayout(pPhrases, keep_strips_keep_piles);
#else
					pLayout->RecalcLayout(pPhrases, create_strips_keep_piles);
#endif
					pApp->m_pActivePile = GetPile(nActiveSequNum);
					
					pApp->GetMainFrame()->canvas->ScrollIntoView(nActiveSequNum);
					CCell* pCell = pApp->m_pActivePile->GetCell(1); // the cell where
															 // the phraseBox is to be
					if (gbIsGlossing)
						pApp->m_targetPhrase = pSrcPhrase->m_gloss;
					else
						// make it look normal, don't use m_targetStr here
						pApp->m_targetPhrase = pSrcPhrase->m_adaption;

					GetLayout()->m_docEditOperationType = consistency_check_op;
														// sets 0,-1 'select all'
					pApp->GetView()->Invalidate(); // get the layout drawn
					GetLayout()->PlaceBox();

					// get the chapter and verse
					wxString chVerse = pApp->GetView()->GetChapterAndVerse(pSrcPhrase);
					dlg.m_chVerse = chVerse;

                    // provide hooks for the phrase box location so that the dialog can
                    // work out where to display itself so it does not obscure the active
                    // location
					dlg.m_ptBoxTopLeft = pCell->GetTopLeft(); // logical coords
					dlg.m_nTwoLineDepth = 2 * pLayout->GetTgtTextHeight();

					if (gbIsGlossing)
					{
						// really its three lines, but the code works provided the 
						// height is right
						if (gbGlossingUsesNavFont)
							dlg.m_nTwoLineDepth += pLayout->GetNavTextHeight();
					}

					// put up the dialog
					if (dlg.ShowModal() == wxID_OK)
					{
						//bool bNoError;
						wxString finalStr;
						// if the m_bDoAutoFix flag is set, add this 'fix' to a list for
						// subsequent use
						AutoFixRecord* pRec;
						if (dlg.m_bDoAutoFix)
						{
							if (gbIgnoreIt)
								// disallow record creation for a press of the "Ignore it,
								// I will fix it later" button
								goto x;
							pRec = new AutoFixRecord;
							pRec->key = pSrcPhrase->m_key; // case should be as wanted
							if (gbIsGlossing)
							{
								pRec->oldAdaptation = pSrcPhrase->m_gloss; // case as wanted
								pRec->nWords = 1;
							}
							else
							{
								pRec->oldAdaptation = pSrcPhrase->m_adaption; // case as wanted
								pRec->nWords = pSrcPhrase->m_nSrcWords;
							}

                            // BEW changed 16May; we don't want to convert the
                            // m_finalAdaptation member to upper case in ANY circumstances,
                            // so we will comment out the relevant lines here and
                            // unilaterally use the user's final string
							finalStr = dlg.m_finalAdaptation; // can have punctuation
									// in it, or can be null; can also be lower case and user
									// expects the app to switch it to upper case if source is upper
							pRec->finalAdaptation = finalStr;
							afList.Append(pRec);
						} // end of block for setting up a new AutoFixRecord

						// update the original kb (not pKBCopy)
x:						finalStr = dlg.m_finalAdaptation; // could have punctuation in it
                        // if the adaptation is null, then assume user wants it that way
                        // and so store an empty string; but if user wants the
                        // inconsistency ignored, then skip
						wxString tempStr = dlg.m_finalAdaptation;
						pApp->GetView()->RemovePunctuation(this,&tempStr,from_target_text);
						if (gbIgnoreIt)
						{
							// if the user hit the "Ignore it, I will fix it later" button,
							// then just put the existing adaptation or gloss back into the KB,
							// after clearing the flag
							if (gbIsGlossing)
							{
								tempStr = pSrcPhrase->m_gloss;
								pKB->StoreText(pSrcPhrase,tempStr,TRUE);
							}
							else // adapting
							{
								tempStr = pSrcPhrase->m_adaption; // no punctuation on this one
								pKB->StoreText(pSrcPhrase,tempStr,TRUE);
								pApp->GetView()->MakeTargetStringIncludingPunctuation(pSrcPhrase,pSrcPhrase->m_targetStr); 
																// m_targetStr may have punct
							}
							gbIgnoreIt = FALSE;
							goto y;
						}
						else
						{
							// don't ignore, so handle the dialog's contents
							if (tempStr.IsEmpty())
							{
								pKB->StoreText(pSrcPhrase,tempStr,TRUE); 
														// TRUE = allow empty string storage
							}
							else
							{
								if (!gbIsGlossing)
								{
									pKB->StoreText(pSrcPhrase,tempStr);
								}
								// do the gbIsGlossing case in next block
							}
                            // the next stuff is taken from code earlier than the DoModal()
                            // call, so comments will not be repeated here - see above if
                            // the details are wanted
							if (gbAutoCaps)
							{
								bool bNoError = SetCaseParameters(pSrcPhrase->m_key);
								if (bNoError && gbSourceIsUpperCase)
								{
									bNoError = SetCaseParameters(tempStr,FALSE); 
															// FALSE means "it's target text"
									if (bNoError && !gbNonSourceIsUpperCase && 
										(gcharNonSrcUC != _T('\0')))
									{
										pSrcPhrase->m_adaption.SetChar(0,gcharNonSrcUC); 
															// get m_adaption member done
									}
								}
							}
							if (!gbIsGlossing)
							{
								pApp->GetView()->MakeTargetStringIncludingPunctuation(pSrcPhrase,finalStr); // handles 
													// auto caps, punctuation, etc
							}
							else // we are in glossing mode
							{
								pKB->StoreText(pSrcPhrase,finalStr); // glossing store
														// can have punctuation in it
								if (gbAutoCaps)
								{
									// if Auto Caps is on, gloss text can be auto 
									// capitalized too... check it out
									wxString str_nopunct = finalStr;
									pApp->GetView()->RemovePunctuation(this,&str_nopunct,from_target_text);
									bool bNoError = SetCaseParameters(str_nopunct,FALSE);
															// FALSE means "using target punct list"
									wxString strInitialPunct = SpanIncluding(
														finalStr,pApp->m_punctuation[1]);
									int punctLen = strInitialPunct.Length();
									bNoError = SetCaseParameters(str_nopunct,FALSE); // FALSE 
															// means "using target punct list"
									if (bNoError && gbSourceIsUpperCase && !gbNonSourceIsUpperCase
										&& (gcharNonSrcUC != _T('\0')))
									{
										if (strInitialPunct.IsEmpty())
										{
											pSrcPhrase->m_gloss.SetChar(0,gcharNonSrcUC);
										}
										else
										{
											finalStr.SetChar(punctLen,gcharNonSrcUC);
											pSrcPhrase->m_gloss = finalStr;
										}
									}
								}
								else
								{
									pSrcPhrase->m_gloss = finalStr;
								}
							} // end of block for glossing mode
						} // end of else block for test: if (gbIgnoreIt)

						// show the progress window again
y:						;
						progDlg.Show(TRUE); // ensure it is visible
					} // end of TRUE block for test of ShowModal() == wxID_OK
					else
					{
						// user cancelled
						bUserCancelled = TRUE;
						progDlg.Show(TRUE);
						// to get the progress dialog hidden, simulate having reached the
						// end of the range -- need the appropriate Update() call
						int bUpdated = FALSE;
						msgDisplayed = progMsg.Format(progMsg,newName.c_str(),nTotal,nTotal);
						bUpdated = progDlg.Update(nTotal,msgDisplayed);
						break;
					}
				} // end of else block for test of presence of an 
				  // AutoFixRecord for this inconsistency
			}
			// update the progress bar every nth iteration (don't use a constant for n,
			// instead use the count of CSourcePhrase instances to set a value that will
			// result in at least a few advances of the progress bar - e.g. 5)
			int n = nTotal / 5;
			if (n > 1000) n = 1000; // safety first, very large files can show smaller 
									// increments more often
			bool bUpdated = FALSE;
			if (counter % n == 0 && counter < nTotal) 
			{
				msgDisplayed = progMsg.Format(progMsg,newName.c_str(),counter,nTotal);
				bUpdated = progDlg.Update(counter,msgDisplayed);
			}
			else if (counter == nTotal)
			{
                // need this block in order to get the progress dialog to
                // auto-hide when the limit is reached, because if the Update() call is not
                // given when the counter reaches the limit, the progress control hangs the
                // application and prevents processing from proceeding any further than the
                // last call of progDlg.Update() within the loop, once the loop has
				// terminated (note, I also specified non-smooth, so Bill or Graeme should
				// test it on Linux and Mac, as that may account for the failures he got)
				msgDisplayed = progMsg.Format(progMsg,newName.c_str(),nTotal,nTotal);
				bUpdated = progDlg.Update(nTotal,msgDisplayed);
			}
		}// end of while (pos1 != NULL)

		// save document and KB
		pApp->m_pTargetBox->Hide(); // this prevents DoFileSave() trying to
                // store to kb with a source phrase with m_bHasKBEntry flag
                // TRUE, which would cause an assert to trip
		pApp->m_pTargetBox->ChangeValue(_T("")); // need to set it to null str
											     // since it won't get recreated
		// BEW removed 29Apr10 in favour of the "_Protected" version below, to
		// give better data protection
		//bool bSavedOK = pDoc->DoFileSave(TRUE);
		
        // BEW 9July10, added test and changed param to FALSE if doing bible book folders
        // loop, as we don't want time wasted for a progress dialog for what are probably a
        // lot of short files. DoFileSave_Protected() computes pApp->m_curOutputPath for
        // each doc file that we check in the currently accessed folder
		bool bSavedOK = DoFileSave_Protected(TRUE); // TRUE - show wait/progress dialog
		if (!bSavedOK)
		{
			wxMessageBox(_("Warning: failure on document save operation."),
			_T(""), wxICON_EXCLAMATION);
		}
		pApp->GetView()->ClobberDocument();

		// delete the buffer containing the filed-in source text
		if (pApp->m_pBuffer != NULL)
		{
			delete pApp->m_pBuffer;
			pApp->m_pBuffer = NULL;
		}
		// remove the progress indicator window
		progDlg.Destroy();
		if (bUserCancelled)
			break; // don't do any more saves of the KB if user cancelled
	} // end iteration of document files for (int i=0; i < nCount; i++)

	// erase the copied CKB which is no longer needed
	wxASSERT(pKBCopy != NULL);
	EraseKB(pKBCopy); // don't want memory leaks!

	// inform user of success and some statistics; but refrain from doing so if the user
	// has requested that all the contents of the bible book folders be checked (otherwise
	// dismissing the stats dialog dozens of times would be total frustration!)
	if (!bUserCancelled)
	{
		// put up final statistics, provided user did not cancel from one
		// of the dialogs
		wxString stats;
		// IDS_CONSCHECK_OK
		stats = stats.Format(_(
"The consistency check was successful. There were %d source words and phrases  in %d  files."),
		nCumulativeTotal,nCount);
		wxMessageBox(stats,_T(""),wxICON_INFORMATION);
	}

	// make sure the global flag is cleared
	gbConsistencyCheckCurrent = FALSE;

	// delete the contents of the pointer list, the list is local 
	// so will go out of scope
	if (!afList.IsEmpty())
	{
		AFList::Node* pos = afList.GetFirst();
		wxASSERT(pos != 0);
		while (pos != 0)
		{
			AutoFixRecord* pRec = (AutoFixRecord*)pos->GetData();
			pos = pos->GetNext();
			delete pRec;
		}
	}
	afList.Clear();
	GetLayout()->m_docEditOperationType = consistency_check_op; // sets 0,-1 'select all'
}

// This function assumes that the current directory will have already been set correctly
// before being called. This is the an overloaded version, it handles all of the Bible
// book folders, looping over each and checking every document in each from a single folder.
// It does nothing if a book folder has no files in it, and just goes to the next book
// folder. The second parameter, and third parameters, pKB and pKBCopy are pointers to the
// same KB (either both the adapting KB, or both the glossing KB) for the current project's
// active KB (depending, of course, on whether glossing mode is on or off), and pKBCopy is
// used for finding inconsistencies, while KB updates are saved to pKB - these in the
// signature because when processing the set of Bible book folders, it makes no sense to
// open, copy, modify and close the KB and copied KB for every book folder processed - so
// we do the open and copy before the loop commences (in the caller), the updating within
// the loop, and the closure and saving when the loop terminates.
// 
// (A prior call of EnumerateDocFiles() with the boolean parameter default TRUE has to have
// been made prior to calling this function. The boolean is for suppressing an internal
// dialog, & being TRUE this dialog is prevented from being shown - we don't want it shown
// and the user have to choose files, as that might have to be done once for every book
// folder that has a document(s) in it, resulting in hundreds of openings of the dialog.
// The latter call also sets the working directory, a fact which DoConsistencyCheck()
// relies on, as it calls OnOpenDocument() with only a filename as parameter, instead of an
// absolute path to that file.
// 
// This overload does a few things differently: (a) no progress indicator is shown
// (processing shortish files makes it 'flash' if it is visible only a short time per
// folder processed); (b) no statistics dialog is shown, because the user would have to
// manually dismiss it after every folder that has at least one file in it.
// 
// Modified, July 2003, for support of Auto Capitalization
// BEW 12Apr10, no changes needed for support of doc version 5
// BEW 17May10, moved to here from CAdapt_ItView
// BEW 8July10, updated for support of kbVersion 2, and for processing all contents of all
// the bible book folders in a loop set up in the caller (which is
// OnEditConsistencyCheck())
// BEW 11Oct10, changed to support ~ conjoining (without this fix, such joined words end
// up in map 2, instead of being in map 1) 
// BEW 13Nov10, changes to support Bob Eaton's request for glosssing KB to use all maps
void CAdapt_ItDoc::DoConsistencyCheck(CAdapt_ItApp* pApp, CKB* pKB, CKB* pKBCopy)
{
	gbConsistencyCheckCurrent = TRUE; // turn on Flag to inhibit placement of phrase box
									  // initially when OnOpenDocument() is called
	CLayout* pLayout = GetLayout();
	wxASSERT(pApp != NULL);
	wxArrayString* pList = &pApp->m_acceptedFilesList;
	int nCount = pList->GetCount();
	if (nCount <= 0)
	{
        // BEW 8Julyl0, book mode is on and we are iterating through Bible book folders,
        // many of which may have no files in them yet. So we must allow for GetCount() to
        // return 0 and if so we return silently, and the loop will go on to the next
        // folder
		gbConsistencyCheckCurrent = FALSE;
		return;
	}
	wxASSERT(nCount > 0);
	int nTotal = 0;
	int nCumulativeTotal = 0;

	// create a list to hold pointers to auto-fix records, if user checks the auto fix checkbox
	// in the dlg
	AFList afList;

	// iterate over the document files
	bool bUserCancelled = FALSE; // whm note: Caution: This bUserCancelled overrides the scope 
								 // of the extern global of the same name
	int i;
	for (i=0; i < nCount; i++)
	{
		wxString newName = pList->Item(i);
		wxASSERT(!newName.IsEmpty());

        // for debugging- check pile count before & after (failure to close doc before
        // calling this function resulted in the following OnOpenDocument() call appending
        // a copy of the document's contents to itself -- the fix is to ensure
        // OnFileClose() is done in the caller before DoConsistencyCheck() is called
		// int piles = pApp->m_pSourcePhrases->GetCount();

		bool bOK;
		bOK = OnOpenDocument(newName); // passing in just a filename, so we are relying
									   // on the working directory having previously
									   // being set in the caller at the call of
									   // EnumerateDocFiles()
		SetFilename(newName,TRUE);
		nTotal = pApp->m_pSourcePhrases->GetCount();
		if (nTotal == 0)
		{
			wxString str;
			str = str.Format(_T("Bad file:  %s"),newName.c_str());
			wxMessageBox(str,_T(""),wxICON_WARNING);
		}
		nCumulativeTotal += nTotal;

		// prepare for the loop
		SPList* pPhrases = pApp->m_pSourcePhrases;
		SPList::Node* pos1; 
		pos1 = pPhrases->GetFirst();
		wxASSERT(pos1 != NULL);
		int counter = 0;
		// loop over all doc files in the current Bible book folder
		while (pos1 != NULL)
		{
			CSourcePhrase* pSrcPhrase = (CSourcePhrase*)pos1->GetData();
			pos1 = pos1->GetNext();
			counter++;

			// check the KBCopy has the required association of key with translation
			// BEW 13Nov10, changes to support Bob Eaton's request for glosssing KB to use all maps
			int nWords;
			//if (gbIsGlossing)
			//	nWords = 1;
			//else
				nWords = pSrcPhrase->m_nSrcWords;
			CTargetUnit* pTU = NULL;
			bool bOK = TRUE;
			bool bFoundTgtUnit = TRUE;
			bool bFoundRefString = TRUE;

			// any inconsistency with a <Not In KB> entry can be fixed automatically,
			// and this block must be ignored when glossing is ON
			if (!gbIsGlossing && !pSrcPhrase->m_bRetranslation && 
				pSrcPhrase->m_bNotInKB && !pSrcPhrase->m_bHasKBEntry)
			{
				// the inconsistency we are fixing here is that the CSourcePhrase instance
				// has its m_bNotInKB flag set TRUE, but the KB lacks a "<Not In KB>"
				// entry (and if that is the case, then for kbVersion 2, possibly other
				// CRefString instances are for that KB entry are not 'deleted' (ie.
				// m_bDeleted flag is not yet set TRUE for them) -- so check and fix if
				// necessary 
				wxString str = _T("<Not In KB>");
				// do the lookup
				bOK = pKB->AutoCapsLookup(pKBCopy->m_pMap[nWords-1], pTU, pSrcPhrase->m_key);
				if (!bOK)
				{
					// fix it silently...
					pApp->m_bSaveToKB = TRUE; // ensure it gets stored
					pKB->StoreText(pSrcPhrase,str);
					pSrcPhrase->m_bHasKBEntry = FALSE;
					pSrcPhrase->m_bNotInKB = TRUE;
				}
				else
				{
                    // BEW 7July10, in kbVersion 2 another way there entry could be
                    // consistent is that it does contain "<Not In KB>" but somehow other
                    // CRefStrings on the pTU instance are not deleted - so we must fix
                    // that. Instead of checking for any non-deleted ones, just delete them
                    // all and re-store "<Not In KB>" -- this will be done then on entries
                    // where this fix is not required, but since <Not In KB> is used seldom
                    // or never, this won't matter
					if (pTU != NULL)
					{
						pTU->DeleteAllToPrepareForNotInKB();
					}
					pApp->m_bSaveToKB = TRUE; // ensure it gets stored
					pKB->StoreText(pSrcPhrase,str);
					pSrcPhrase->m_bHasKBEntry = FALSE;
					pSrcPhrase->m_bNotInKB = TRUE;
				}
			}

			bool bTheTest = FALSE;
			// define the test's value
			if (gbIsGlossing)
			{
				if (pSrcPhrase->m_bHasGlossingKBEntry)
				{
					bTheTest = TRUE;
				}
			}
			else // adapting
			{
				if (!pSrcPhrase->m_bRetranslation && !pSrcPhrase->m_bNotInKB &&
					pSrcPhrase->m_bHasKBEntry && pSrcPhrase->m_adaption != _T("<Not In KB>"))
				{
					bTheTest = TRUE;
				}
			}
			if (bTheTest)
			{
				// do the lookup
				bOK = pKBCopy->AutoCapsLookup(pKBCopy->m_pMap[nWords-1], pTU, pSrcPhrase->m_key);
				if (!bOK)
				{
					// there was no target unit for this key in the map, so this is an
					// inconsistency
					bFoundTgtUnit = FALSE;
					bFoundRefString = FALSE;
				}
				else
				{
					// the target unit is in the map, so check if there is a corresponding
					// refString for the m_adaption, or m_gloss, member of the source phrase
					wxASSERT(pTU);
					bool bMatched = FALSE;
					TranslationsList* pList = pTU->m_pTranslations; 
					wxASSERT(pList != NULL);
                    // For kbVersion 2 don't make this assertion. If the phrase box is at a
                    // place where there is a m_refCount of 1 which has just been deleted
                    // because of 'landing' the box there, and it had just the one
                    // CRefString, then the count of non-deleted ones can legitimately be
                    // temporarily zero. Leave this here as a reminder to the developer.
					//wxASSERT(pTU->CountNonDeletedRefStringInstances() > 0);
					wxString srcPhraseStr;
					if (gbIsGlossing)
					{
						srcPhraseStr = pSrcPhrase->m_gloss;
					}
					else
					{
						srcPhraseStr = pSrcPhrase->m_adaption;
					}
					if (!((gbAutoCaps && gbSourceIsUpperCase && gbMatchedKB_UCentry) || !gbAutoCaps))
					{
                        // do a change to lc only if it is needed - that is, attempt it
                        // when it is not the case that gbMatchedKB_UCentry is TRUE
                        // (because then we want an unmodified string to be used),
                        // otherwise attempt it when auto-caps is ON
						srcPhraseStr = pKBCopy->AutoCapsMakeStorageString(srcPhraseStr,FALSE);
					}
					TranslationsList::Node* pos = pList->GetFirst();
					wxASSERT(pos != NULL);
					while (pos != NULL)
					{
						CRefString* pRefString = (CRefString*)pos->GetData();
						pos = pos->GetNext();
						wxASSERT(pRefString != NULL);
						// BEW 7July10, added second test, as kbVersion 2 treats a CRefString
						// instance as present only if it is not marked as deleted
						if ((pRefString->m_translation == srcPhraseStr) && !pRefString->GetDeletedFlag())
						{
							// a matching gloss was found
							bMatched = TRUE;
							break;
						}
					}
					if (!bMatched)
					{
						// no match was made, so this is an inconsistency
						bFoundRefString = FALSE;
					}
				}
			}

			// open the dialog if we have an inconsistency
			if (!bFoundTgtUnit || !bFoundRefString)
			{
				// make the CSourcePhrase instance is able to have a KB entry added
				if (gbIsGlossing)
					pSrcPhrase->m_bHasGlossingKBEntry = FALSE;
				else
					pSrcPhrase->m_bHasKBEntry = FALSE;

                // work out if this is an auto-fix item, if so, don't show the dialog, but
                // use the stored AutoFixRecord to fix the inconsistency without user
                // intervention (note: any items for which the "Ignore it, I will fix it 
                // later" button was pressed cannot occur as auto-fix records) The next 100
                // lines could be improved - it was "added to" in a rather ad hoc fashion,
                // so its a bit spagetti-like... something to do sometime when there is
                // plenty of time!
				AutoFixRecord* pAFRecord = NULL;
				if (MatchAutoFixItem(&afList, pSrcPhrase, pAFRecord))
				{
					// we matched an auto-fix element, so do the fix automatically...
					// update the original kb (not pKBCopy)
					wxString tempStr = pAFRecord->finalAdaptation; // could have punctuation in it

					// if the adaptation is null, then assume user wants it that way and so store
					// an empty string
					if (tempStr.IsEmpty())
					{
						pKB->StoreText(pSrcPhrase,tempStr,TRUE); // TRUE = allow saving empty adaptation
					}
					else
					{
						if (!gbIsGlossing)
						{
							pApp->GetView()->RemovePunctuation(this,&tempStr,from_target_text); 
                                // we don't want punctuation in adaptation KB if
                                // autocapitalization is ON, we could have an upper case
                                // source, but the user may have typed lower case for
                                // fixing the gloss or adaptation, but this is okay - the
                                // store will work right, so don't need anything here
                                // except the call to store it
							pKB->StoreText(pSrcPhrase,tempStr);
						}
						// do the gbIsGlossing case when no punct is to be removed, in next block
					}
					if (!tempStr.IsEmpty())
					{
                        // here we must be careful; pAFRecord->finalAdaptation may have a
                        // lower case string when the source text has upper case, and the
                        // user is expecting the application to do the fix for him; this
                        // would be easy if we could be sure that the first letter of the
                        // string was at index == 0, but the possible presence of preceding
                        // punctuation makes the assumption dangerous - so we must find
                        // where the actual text starts and do any changes there if needed.
                        // tempStr has punctuation stripped out, pAFRecord->finalAdaptation
                        // doesn't, so start by determining if there actually is a problem
                        // to be fixed.
						if (gbAutoCaps)
						{
							bool bNoError = SetCaseParameters(pSrcPhrase->m_key);
							if (bNoError && gbSourceIsUpperCase)
							{
								bNoError = SetCaseParameters(tempStr,FALSE); // FALSE means "it's target text"
								if (bNoError && !gbNonSourceIsUpperCase &&
									(gcharNonSrcUC != _T('\0')))
								{
                                    // source is upper case but nonsource is lower and is a
                                    // character with an upper case equivalent - we have a
                                    // problem; we need to fix the AutoFixRecord's
                                    // finalAdaptation string, and the sourcephrase too. At
                                    // this point we can fix the m_adaption member as
                                    // follows:
									pSrcPhrase->m_adaption.SetChar(0,gcharNonSrcUC);
								}
							}
						}
                        // In the next if/else block, the non-glossing-mode call of
                        // MakeTargetStringIncludingPunctuation() accomplishes the setting
                        // of the pSrcPhrase's m_targetStr member, handling any needed
                        // lower case to upper case conversion (even when typed initial
                        // punctuation is present), and the punctuation override protocol
                        // if the passed in string in the 2nd parameter has initial and/or
                        // final punctuation.
						if (!gbIsGlossing)
						{
                            // for auto capitalization support,
                            // MakeTargetStringIncludingPunctuation( ) is now able to do
                            // any needed change to upper case initial letter even when
                            // there is initial punctuation on pAFRecord->finalAdaptation
							pApp->GetView()->MakeTargetStringIncludingPunctuation(pSrcPhrase, 
															pAFRecord->finalAdaptation);
						}
						else
						{
							// store, for the glossing ON case, the gloss text, 
							// with any punctuation
							pKB->StoreText(pSrcPhrase, pAFRecord->finalAdaptation);
							if (gbAutoCaps)
							{
                                 // upper case may be wanted, we have to do it on the first
                                // character past any initial punctuation; glossing mode
                                // doesn't do punctuation stripping and copying, but the
                                // user may have punctuation included in the inconsistency
                                // fixing string, so we have to check etc.
								wxString str = pAFRecord->finalAdaptation;
								// make a copy and remove punctuation from it
								wxString str_nopunct = str;
								pApp->GetView()->RemovePunctuation(this,&str_nopunct,from_target_text);
								// use the punctuation-less string to get the initial charact and
								// its upper case equivalent if it exists
								bool bNoError = SetCaseParameters(str_nopunct,FALSE);
															 // FALSE means "using target punct list"
								// span punctuation-having str using target lang's punctuation...
								wxString strInitialPunct = SpanIncluding(str,pApp->m_punctuation[1]);
															// use our own SpanIncluding in helpers
								int punctLen = strInitialPunct.Length();

								// work out if there is a case change needed, and set the
								// relevant case globals
								bNoError = SetCaseParameters(tempStr,FALSE);  
															// FALSE means "it's target text"
								if (bNoError && gbSourceIsUpperCase && !gbNonSourceIsUpperCase
									&& (gcharNonSrcUC != _T('\0')))
								{
									if (strInitialPunct.IsEmpty())
									{
                                        // there is no initial punctuation, so the change
                                        // to upper case can be done at the string's start
										pSrcPhrase->m_gloss.SetChar(0,gcharNonSrcUC);
									}
									else
									{
										// set it at the first character past the initial
										// punctuation
										str.SetChar(punctLen,gcharNonSrcUC);
										pSrcPhrase->m_gloss = str;
									}
								}
							}
							else
							{
								// no auto capitalization, so just use finalAdaptation 
								// string 'as is'
								pSrcPhrase->m_gloss = pAFRecord->finalAdaptation;
							}
						}
					}
					pApp->m_targetPhrase = pAFRecord->finalAdaptation; // any brief glimpse
							// of the box should show the current adaptation, or gloss, string
				}
				else
				{
					// no match, so this is has to be handled with user intervention via
					// the dialog
					CConsistencyCheckDlg dlg(pApp->GetMainFrame());
					dlg.m_bFoundTgtUnit = bFoundTgtUnit;
					dlg.m_bDoAutoFix = FALSE;
					dlg.m_pApp = pApp;
					dlg.m_pKBCopy = pKBCopy;
					dlg.m_pTgtUnit = pTU; // could be null
					dlg.m_finalAdaptation.Empty(); // initialize final chosen adaptation or gloss
					dlg.m_pSrcPhrase = pSrcPhrase;

                    // update the view to show the location where this source pile is, and
                    // put the phrase box there ready to accept user input indirectly from
                    // the dialog
					int nActiveSequNum = pSrcPhrase->m_nSequNumber;
					wxASSERT(nActiveSequNum >= 0);
					pApp->m_nActiveSequNum = nActiveSequNum; // added 16Apr09, should be okay
					// and is needed because CLayout::RecalcLayout() relies on the
					// m_nActiveSequNum value being correct
#ifdef _NEW_LAYOUT
					pLayout->RecalcLayout(pPhrases, keep_strips_keep_piles);
#else
					pLayout->RecalcLayout(pPhrases, create_strips_keep_piles);
#endif
					pApp->m_pActivePile = GetPile(nActiveSequNum);
					
					pApp->GetMainFrame()->canvas->ScrollIntoView(nActiveSequNum);
					CCell* pCell = pApp->m_pActivePile->GetCell(1); // the cell where
															 // the phraseBox is to be
					if (gbIsGlossing)
						pApp->m_targetPhrase = pSrcPhrase->m_gloss;
					else
						// make it look normal, don't use m_targetStr here
						pApp->m_targetPhrase = pSrcPhrase->m_adaption;

					GetLayout()->m_docEditOperationType = consistency_check_op;
														// sets 0,-1 'select all'
					pApp->GetView()->Invalidate(); // get the layout drawn
					GetLayout()->PlaceBox();

					// get the chapter and verse
					wxString chVerse = pApp->GetView()->GetChapterAndVerse(pSrcPhrase);
					dlg.m_chVerse = chVerse;

                    // provide hooks for the phrase box location so that the dialog can
                    // work out where to display itself so it does not obscure the active
                    // location
					dlg.m_ptBoxTopLeft = pCell->GetTopLeft(); // logical coords
					dlg.m_nTwoLineDepth = 2 * pLayout->GetTgtTextHeight();

					if (gbIsGlossing)
					{
						// really its three lines, but the code works provided the 
						// height is right
						if (gbGlossingUsesNavFont)
							dlg.m_nTwoLineDepth += pLayout->GetNavTextHeight();
					}

					// put up the dialog
					if (dlg.ShowModal() == wxID_OK)
					{
						//bool bNoError;
						wxString finalStr;
						// if the m_bDoAutoFix flag is set, add this 'fix' to a list for
						// subsequent use
						AutoFixRecord* pRec;
						if (dlg.m_bDoAutoFix)
						{
							if (gbIgnoreIt)
								// disallow record creation for a press of the "Ignore it,
								// I will fix it later" button
								goto x;
							pRec = new AutoFixRecord;
							pRec->key = pSrcPhrase->m_key; // case should be as wanted
							if (gbIsGlossing)
							{
								pRec->oldAdaptation = pSrcPhrase->m_gloss; // case as wanted
								// BEW 13Nov10, changes to support Bob Eaton's request for 
								// glosssing KB to use all maps
								//pRec->nWords = 1;
								pRec->nWords = pSrcPhrase->m_nSrcWords;
							}
							else
							{
								pRec->oldAdaptation = pSrcPhrase->m_adaption; // case as wanted
								pRec->nWords = pSrcPhrase->m_nSrcWords;
							}

                            // BEW changed 16May; we don't want to convert the
                            // m_finalAdaptation member to upper case in ANY circumstances,
                            // so we will comment out the relevant lines here and
                            // unilaterally use the user's final string
							finalStr = dlg.m_finalAdaptation; // can have punctuation
									// in it, or can be null; can also be lower case and user
									// expects the app to switch it to upper case if source is upper
							pRec->finalAdaptation = finalStr;
							afList.Append(pRec);
						} // end of block for setting up a new AutoFixRecord

						// update the original kb (not pKBCopy)
x:						finalStr = dlg.m_finalAdaptation; // could have punctuation in it
                        // if the adaptation is null, then assume user wants it that way
                        // and so store an empty string; but if user wants the
                        // inconsistency ignored, then skip
						wxString tempStr = dlg.m_finalAdaptation;
						pApp->GetView()->RemovePunctuation(this,&tempStr,from_target_text);
						if (gbIgnoreIt)
						{
							// if the user hit the "Ignore it, I will fix it later" button,
							// then just put the existing adaptation or gloss back into the KB,
							// after clearing the flag
							if (gbIsGlossing)
							{
								tempStr = pSrcPhrase->m_gloss;
								pKB->StoreText(pSrcPhrase,tempStr,TRUE);
							}
							else // adapting
							{
								tempStr = pSrcPhrase->m_adaption; // no punctuation on this one
								pKB->StoreText(pSrcPhrase,tempStr,TRUE);
								pApp->GetView()->MakeTargetStringIncludingPunctuation(pSrcPhrase,pSrcPhrase->m_targetStr); 
																// m_targetStr may have punct
							}
							gbIgnoreIt = FALSE;
							goto y;
						}
						else
						{
							// don't ignore, so handle the dialog's contents
							if (tempStr.IsEmpty())
							{
								pKB->StoreText(pSrcPhrase,tempStr,TRUE); 
														// TRUE = allow empty string storage
							}
							else
							{
								if (!gbIsGlossing)
								{
									pKB->StoreText(pSrcPhrase,tempStr);
								}
								// do the gbIsGlossing case in next block
							}
                            // the next stuff is taken from code earlier than the DoModal()
                            // call, so comments will not be repeated here - see above if
                            // the details are wanted
							if (gbAutoCaps)
							{
								bool bNoError = SetCaseParameters(pSrcPhrase->m_key);
								if (bNoError && gbSourceIsUpperCase)
								{
									bNoError = SetCaseParameters(tempStr,FALSE); 
															// FALSE means "it's target text"
									if (bNoError && !gbNonSourceIsUpperCase && 
										(gcharNonSrcUC != _T('\0')))
									{
										pSrcPhrase->m_adaption.SetChar(0,gcharNonSrcUC); 
															// get m_adaption member done
									}
								}
							}
							if (!gbIsGlossing)
							{
								pApp->GetView()->MakeTargetStringIncludingPunctuation(pSrcPhrase,finalStr); // handles 
													// auto caps, punctuation, etc
							}
							else // we are in glossing mode
							{
								pKB->StoreText(pSrcPhrase,finalStr); // glossing store
														// can have punctuation in it
								if (gbAutoCaps)
								{
									// if Auto Caps is on, gloss text can be auto 
									// capitalized too... check it out
									wxString str_nopunct = finalStr;
									pApp->GetView()->RemovePunctuation(this,&str_nopunct,from_target_text);
									bool bNoError = SetCaseParameters(str_nopunct,FALSE);
															// FALSE means "using target punct list"
									wxString strInitialPunct = SpanIncluding(
														finalStr,pApp->m_punctuation[1]);
									int punctLen = strInitialPunct.Length();
									bNoError = SetCaseParameters(str_nopunct,FALSE); // FALSE 
															// means "using target punct list"
									if (bNoError && gbSourceIsUpperCase && !gbNonSourceIsUpperCase
										&& (gcharNonSrcUC != _T('\0')))
									{
										if (strInitialPunct.IsEmpty())
										{
											pSrcPhrase->m_gloss.SetChar(0,gcharNonSrcUC);
										}
										else
										{
											finalStr.SetChar(punctLen,gcharNonSrcUC);
											pSrcPhrase->m_gloss = finalStr;
										}
									}
								}
								else
								{
									pSrcPhrase->m_gloss = finalStr;
								}
							} // end of block for glossing mode
						} // end of else block for test: if (gbIgnoreIt)

y:						;
					} // end of TRUE block for test of ShowModal() == wxID_OK
					else
					{
						// user cancelled
						bUserCancelled = TRUE;
						break;
					}
				} // end of else block for test of presence of an 
				  // AutoFixRecord for this inconsistency
			}
		}// end of while (pos1 != NULL)

		// save document and KB
		pApp->m_pTargetBox->Hide(); // this prevents DoFileSave() trying to
                // store to kb with a source phrase with m_bHasKBEntry flag
                // TRUE, which would cause an assert to trip
		pApp->m_pTargetBox->ChangeValue(_T("")); // need to set it to null str
											     // since it won't get recreated
		// BEW removed 29Apr10 in favour of the "_Protected" version below, to
		// give better data protection
		//bool bSavedOK = pDoc->DoFileSave(TRUE);
		
        // BEW 9July10, added test and changed param to FALSE if doing bible book folders
        // loop, as we don't want time wasted for a progress dialog for what are probably a
        // lot of short files. DoFileSave_Protected() computes pApp->m_curOutputPath for
        // each doc file that we check in the currently accessed folder
		bool bSavedOK = DoFileSave_Protected(FALSE); // FALSE - dodn't show wait/progress dialog
		if (!bSavedOK)
		{
			wxMessageBox(_("Warning: failure on document save operation."),
			_T(""), wxICON_EXCLAMATION);
		}
		pApp->GetView()->ClobberDocument();

		// delete the buffer containing the filed-in source text
		if (pApp->m_pBuffer != NULL)
		{
			delete pApp->m_pBuffer;
			pApp->m_pBuffer = NULL;
		}
		if (bUserCancelled)
			break; // don't do any more saves of the KB if user cancelled
	} // end iteration of document files for (int i=0; i < nCount; i++)

	// make sure the global flag is cleared
	gbConsistencyCheckCurrent = FALSE;

	// delete the contents of the pointer list, the list is local 
	// so will go out of scope
	if (!afList.IsEmpty())
	{
		AFList::Node* pos = afList.GetFirst();
		wxASSERT(pos != 0);
		while (pos != 0)
		{
			AutoFixRecord* pRec = (AutoFixRecord*)pos->GetData();
			pos = pos->GetNext();
			delete pRec;
		}
	}
	afList.Clear();
	GetLayout()->m_docEditOperationType = consistency_check_op; // sets 0,-1 'select all'
}

// the rpRec value will be undefined if FALSE is returned, if TRUE is returned, rpRec will
// be the matched auto-fix item in the list. For version 2.0 and later which supports
// glossing, rpRec could contain glossing or adapting information, depending on the setting
// for the gbIsGlossing flag. Also, in support of auto capitalization; since these strings
// are coming from the sourcephrase instances in the documents, they will have upper or
// lower case as appropriate; but we will need to allow the user to just type lower case
// strings when correcting in the context of AutoCaps being turned ON, so be careful!
// BEW 12Apr10, no change needed for support of doc version 5
// BEW 17May10, moved to here from CAdapt_ItView
// BEW 13Nov10, no changes to support Bob Eaton's request for glosssing KB to use all maps
bool CAdapt_ItDoc::MatchAutoFixItem(AFList* pList,CSourcePhrase *pSrcPhrase,
									 AutoFixRecord*& rpRec)
{
	wxASSERT(pList != NULL);
	wxASSERT(pSrcPhrase != NULL);
	if (pList->IsEmpty())
		return FALSE;
	AFList::Node* pos = pList->GetFirst(); 
	wxASSERT(pos != NULL);
	while (pos != NULL)
	{
		AutoFixRecord* pRec = (AutoFixRecord*)pos->GetData();
		pos = pos->GetNext();
		bool bTest = FALSE;
		if (gbIsGlossing)
		{
			if (pRec->key == pSrcPhrase->m_key && 
				pRec->oldAdaptation == pSrcPhrase->m_gloss)
				bTest = TRUE;
		}
		else
		{
			if (pRec->key == pSrcPhrase->m_key && 
				pRec->oldAdaptation == pSrcPhrase->m_adaption)
				bTest = TRUE;
		}
		if (bTest)
		{
            // we can autofix with this one, so pass its values (including any upper case)
            // to the caller (ie. to DoConsistencyCheck( )) for processing - remember, it
            // may have an old gloss or adaptation which is upper case, and the user may
            // have typed a lower case string for the finalAdaptation member, so this will
            // need to be tested for
			rpRec = pRec;
			return TRUE;
		}
	}
	// if we get to here, no match was made
	rpRec = NULL;
	return FALSE;
}

// BEW 24Mar10, updated for support of doc version 5 (some changes needed)
void CAdapt_ItDoc::GetMarkerInventoryFromCurrentDoc()
{
    // Scans all the doc's source phrase m_markers and m_filteredInfo members and
    // inventories all the markers used in the current document, storing all unique markers
    // in m_exportBareMarkers, the full markers and their descriptions in the CStringArray
    // called m_exportMarkerAndDescriptions, and their corresponding include/exclude states
    // (boolean flags) in the CUIntArray called m_exportFilterFlagsBeforeEdit. A given
    // marker may occur more than once in a given document, but is only stored once in
    // these inventory arrays.
	// All the boolean flags in the m_exportFilterFlagsBeforeEdit array
	// are initially set to FALSE indicating that no markers are to be
	// filtered out of the export by default. If the user accesses and/or
	// changes the export options via the "Export/Filter Options" dialog
	// and thereby filters one or markers from export, then their
	// corresponding flags in the CUIntArray called m_exportFilterFlags
	// will be set to TRUE.

	// Any sfms that are currently filtered are listed with [FILTERED] prefixed
	// to the description. Unknown markers are listed with [UNKNOWN MARKER] as
	// their description. We list all markers that are used in the document, and
	// if the user excludes things illogically, then the output will reflect
	// that.
	CAdapt_ItApp* pApp = &wxGetApp();
	SPList* pList = pApp->m_pSourcePhrases;
	wxArrayString MarkerList;	// gets filled with all the currently
							    // used markers including filtered ones
	wxArrayString* pMarkerList = &MarkerList;
	SPList::Node* posn;
	USFMAnalysis* pSfm;
	wxString key;
	wxString lbStr;

	MapSfmToUSFMAnalysisStruct* pSfmMap;
	pSfmMap = pApp->GetCurSfmMap(pApp->gCurrentSfmSet);

	// Gather markers from all source phrase m_marker strings
	// BEW 24Mar10 changes for support of doc version 5: markers are now stored in
	// m_markers and in m_filteredInfo (markers and content wrapped, in the latter, with
	// \~FILTER and \~FILTER* bracketing markers). Also, in the legacy versions, free
	// translations, collected back translations, and notes, were stored likewise in
	// m_markers and wrapped with filter bracket markers, but now for doc version 5 these
	// three information types have dedicated wxString member storage in CSourcePhrase. So
	// for correct behaviour with the functionalities dependent on
	// GetMarkerInventoryFromCurrentDoc() we have to here treat those three information
	// types as logically "filtered" and supply \free & \free* wrapping markers to the
	// free translation string we recover, and \note & \note* to the note string we
	// recover, and \bt for any collected back translation string, when any of these is
	// present in m_freeTrans, m_note, and m_collectedBackTrans, respectively. We do that
	// below after the call to GetMarkersAndTextFromString(), as the latter can handle the
	// m_markers added to m_filteredInfo in the parameter list. 
	posn = pList->GetFirst();
	wxASSERT(posn != NULL);
	CSourcePhrase* pSrcPhrase = (CSourcePhrase*)posn->GetData();
	wxASSERT(pSrcPhrase);
	wxString str;
	str.Empty();
	wxString filtermkr = wxString(filterMkr);
	wxString filtermkrend = wxString(filterMkrEnd);
	while (posn != 0)
	{
		pSrcPhrase = (CSourcePhrase*)posn->GetData();
		posn = posn->GetNext();
		wxASSERT(pSrcPhrase);
		// retrieve sfms used from pSrcPhrase->m_markers & m_filteredInfo, etc
		if (!pSrcPhrase->m_markers.IsEmpty() || !pSrcPhrase->GetFilteredInfo().IsEmpty())
		{
            // GetMarkersAndTextFromString() retrieves each marker and its associated
            // string and places them in the CStringList. Any Filtered markers are stored
            // as a list item bracketed by \~FILTER ... \~FILTER* markers.
			// To avoid a large CStringList developing we'll process the markers in each
			// m_markers string individually, so empty the list. Information in
			// m_freeTrans, m_note, or m_collectedBackTrans is handled after the
			// GetMarkersAndTextFromString() call
			pMarkerList->Clear();
			GetMarkersAndTextFromString(pMarkerList, pSrcPhrase->m_markers + pSrcPhrase->GetFilteredInfo());
			if (!pSrcPhrase->GetFreeTrans().IsEmpty())
			{
				str = filtermkr + _T(" ") + _T("\\free ") + pSrcPhrase->GetFreeTrans() + _T("\\free* ") + filtermkrend;
				pMarkerList->Add(str);
				str.Empty();
			}
			if (!pSrcPhrase->GetNote().IsEmpty())
			{
				str = filtermkr + _T(" ") + _T("\\note ") + pSrcPhrase->GetNote() + _T("\\note* ") + filtermkrend;
				pMarkerList->Add(str);
				str.Empty();
			}
			if (!pSrcPhrase->GetCollectedBackTrans().IsEmpty())
			{
				str = filtermkr + _T(" ") + _T("\\bt ") + pSrcPhrase->GetCollectedBackTrans() + _T(" ") + filtermkrend;
				pMarkerList->Add(str);
				str.Empty();
			}
			wxString resultStr;
			resultStr.Empty();
			wxString displayStr;
			wxString bareMarker;
			wxString temp;
			int ct;
			for (ct = 0; ct < (int)pMarkerList->GetCount(); ct++)
			{
				resultStr = pMarkerList->Item(ct);
				bool markerIsFiltered;
				if (resultStr.Find(filterMkr) != -1)
				{
					resultStr = RemoveAnyFilterBracketsFromString(resultStr);
					markerIsFiltered = TRUE;
				}
				else
				{
					markerIsFiltered = FALSE;
				}
				resultStr.Trim(FALSE); // trim left end
				resultStr.Trim(TRUE); // trim right end
				wxASSERT(resultStr.Find(gSFescapechar) == 0);
				int strLen = resultStr.Length();
				int posm = 1; // skip initial backslash
				bareMarker.Empty();
				displayStr.Empty();
				while (posm < strLen && resultStr[posm] != _T(' ') && 
						resultStr[posm] != gSFescapechar)
				{
					bareMarker += resultStr[posm];
					posm++;
				}
				bareMarker.Trim(FALSE); // trim left end
				bareMarker.Trim(TRUE); // trim right end

				// do not include end markers in this inventory
				int aPos = bareMarker.Find(_T('*'));
				if (aPos == (int)bareMarker.Length() -1)
					bareMarker.Remove(aPos,1);
				wxASSERT(bareMarker.Length() > 0);
				// lookup the marker in the active USFMAnalysis struct map
                // whm ammended 11Jul05 Here we want to use the LookupSFM() routine which
                // treats all \bt... initial back-translation markers as known markers all
                // under the \bt marker with its description "Back-translation"
                // whm revised again 14Nov05. For output filtering purposes, we need to
                // treat all \bt... initial forms the same as simple \bt, in order to give
                // the user the placement options (boxed paragraphs or footnote format for
                // sfm RTF output; new table row or footnote format for interlinear RTF
                // output). Handling all backtranslation the same for the sake of these
                // placement options I think is preferable to not having the placement
                // options and being able to filter from output the possible different
                // kinds of backtranslation \bt... markers. Therefore here we will make all
                // \bt... be just simple \bt and hence only have \bt in the export options
                // list box. I've also renamed the \bt marker's description in AI_USFM.xml
                // file to read: "Back Translation (and all \bt... initial forms)".
				if (bareMarker.Find(_T("bt")) == 0)
				{
					bareMarker = _T("bt"); // make any \bt... initial forms be just 
										   // \bt in the listbox
				}
				pSfm = LookupSFM(bareMarker); // use LookupSFM which properly 
													// handles \bt... forms as \bt
				bool bFound = pSfm != NULL;
				lbStr = _T(' '); // prefix one initial space - looks better 
								 // in a CCheckListBox
				lbStr += gSFescapechar; // add backslash
				// Since LookupSFM will find any back-translation marker of the form
				// bt... we'll use the actual bareMarker to build the list box string
				lbStr += bareMarker;
                // We don't worry about adjusting for text extent here - that is done below
                // in FormatMarkerAndDescriptionsStringArray(). Here we will just add a
                // single space as delimiter between the whole marker and its description
				lbStr += _T(' ');
				if (!bFound)
				{
					// unknown marker so make the description [UNKNOWN MARKER]
					// IDS_UNKNOWN_MARKER
					temp = _("[UNKNOWN MARKER]"); // prefix description 
												  // with "[UNKNOWN MARKER]"
					lbStr = lbStr + temp;
				}
				else
				{
					if (markerIsFiltered)
					{
						// IDS_FILTERED
						temp = _("[FILTERED]"); // prefix description 
												// with "[FILTERED] ..."
						lbStr += temp;
						lbStr += _T(' ');
						lbStr += pSfm->description;
					}
					else
					{
						lbStr += pSfm->description;
					}
				}
				// Have we already stored this marker?
				bool mkrAlreadyExists = FALSE;
				for (int ct = 0; ct < (int)m_exportMarkerAndDescriptions.GetCount(); ct++)
				{
					if (lbStr == m_exportMarkerAndDescriptions[ct])
					{
						mkrAlreadyExists = TRUE;
						break;
					}
				}
				if (!mkrAlreadyExists)
				{
					// BEW addition 16Aug09, to exclude \note, \bt and/or \free markers
					// when exporting either free translations or glosses as text
					if (pApp->m_bExportingFreeTranslation || pApp->m_bExportingGlossesAsText)
					{
						if (bareMarker == _T("note") || 
							bareMarker == _T("free") ||
							bareMarker == _T("bt"))
						{
							continue; // ignore any of these marker types
						}
						else
						{
							// anything else gets added to the inventory
							m_exportBareMarkers.Add(bareMarker);
							m_exportMarkerAndDescriptions.Add(lbStr);
							m_exportFilterFlags.Add(FALSE);
							m_exportFilterFlagsBeforeEdit.Add(FALSE); 
						}
					}
					else
					{
						m_exportBareMarkers.Add(bareMarker);
						m_exportMarkerAndDescriptions.Add(lbStr);
						m_exportFilterFlags.Add(FALSE); // export defaults to nothing 
														// filtered out
						m_exportFilterFlagsBeforeEdit.Add(FALSE); // export defaults to 
																  // nothing filtered out
					}
				}
			}
		}
	}
	wxClientDC dC(pApp->GetMainFrame()->canvas);
	pApp->FormatMarkerAndDescriptionsStringArray(&dC,
							&m_exportMarkerAndDescriptions, 2, NULL);
	// last parameter in call above is 2 spaces min 
	// between whole marker and its description
}

/////////////////////////////////////////////////
///
/// Functions for support of Auto-Capitalization
///
////////////////////////////////////////////////

inline wxChar CAdapt_ItDoc::GetFirstChar(wxString& strText)
{
	return strText.GetChar(0);
}

// takes the input character chTest, and attempts to Find() it in the CString theCharSet,
// returning TRUE if it finds it, and setting index to the character index for its position
// in the string buffer; if not found, then index will be set to -1.
bool CAdapt_ItDoc::IsInCaseCharSet(wxChar chTest, wxString& theCharSet, int& index)
{
	index = theCharSet.Find(chTest);
	if (index > -1)
	{
		// it is in the list
		return TRUE;
	}
	else
	{
		// it is not in the list
		return FALSE;
	}
}

// returns the TCHAR at the passed in offset
wxChar CAdapt_ItDoc::GetOtherCaseChar(wxString& charSet, int nOffset)
{
	wxASSERT(nOffset < (int)charSet.Length());
	return charSet.GetChar(nOffset);
}

//return TRUE if all was well, FALSE if there was an error; strText is the language word or
//phrase the first character of which this function tests to determine its case, and from
//that to set up storage for the lower or upper case equivalent character, and the relevant
//flags. strText can be source text, target text, or gloss text; for the latter two
//possibilities bIsSrcText needs to be explicitly set to FALSE, otherwise it is TRUE by
//default. This is a diagnostic function used for Auto-Capitalization support.
bool CAdapt_ItDoc::SetCaseParameters(wxString& strText, bool bIsSrcText)
{
	CAdapt_ItApp* pApp = &wxGetApp();
	if (strText.IsEmpty())
	{
		return FALSE;
	}
	int nOffset = -1;
	wxChar chFirst = GetFirstChar(strText);

	bool bIsLower;
	bool bIsUpper;
	if (bIsSrcText)
	{
		// exit prematurely if the user has not defined any source case equivalents
		if (gbNoSourceCaseEquivalents)
		{
			gbSourceIsUpperCase = FALSE; // ensures an old style lookup or store
			return FALSE;
		}

		// determine if it is a lower case source character 
		// which has an upper case equivalent
		bIsLower = IsInCaseCharSet(chFirst,pApp->m_srcLowerCaseChars,nOffset);
		if (bIsLower)
		{
			// it's a lower case belonging to the source set,
			// so we don't have to capitalize it
			gbSourceIsUpperCase = FALSE;
			gcharSrcLC = chFirst;
			wxASSERT(nOffset >= 0);
			gcharSrcUC = GetOtherCaseChar(pApp->m_srcUpperCaseChars,nOffset);
		}
		else
		{
            // chFirst is not a lower case source character which has an upper case
            // equivalent, so it might be an upper case source character (having a lower
            // case equivalent), or it is of indeterminate case - in which case we treat
            // it as lower case
			bIsUpper = IsInCaseCharSet(chFirst,pApp->m_srcUpperCaseChars,nOffset);
			if (bIsUpper)
			{
				// it is an upper case source char for which there is a lower case 
				// equivalent
				gbSourceIsUpperCase = TRUE;
				gcharSrcUC = chFirst;
				wxASSERT(nOffset >= 0);
				gcharSrcLC = GetOtherCaseChar(pApp->m_srcLowerCaseChars,nOffset);
			}
			else
			{
				// it has indeterminate case, so treat as lower case with zero as its 
				// upper case equiv
				gbSourceIsUpperCase = FALSE;
				gcharSrcLC = chFirst;
				wxASSERT(nOffset == -1);
				gcharSrcUC = _T('\0');
			}
		}
	}
	else
	{
        // it is either gloss or adaptation data: use gbIsGlossing to determine which...
        // determine if it is a lower case character which has an upper case equivalent
		if (gbIsGlossing)
		{
			// it's gloss data
			// exit prematurely if the user has not specified any gloss case equivalents
			if (gbNoGlossCaseEquivalents)
			{
				gbNonSourceIsUpperCase = FALSE; // ensures an old style lookup or store
				gcharNonSrcUC = _T('\0'); // use the value as a test indicating failure here
				return FALSE;
			}
			bIsLower = IsInCaseCharSet(chFirst,pApp->m_glossLowerCaseChars,nOffset);
		}
		else
		{
			// it's adaptation data
			// exit prematurely if the user has not specified any target case equivalents
			if (gbNoTargetCaseEquivalents)
			{
				gbNonSourceIsUpperCase = FALSE; // ensures an old style lookup or store
				gcharNonSrcUC = _T('\0'); // use the value as a test indicating failure
				return FALSE;
			}
			bIsLower = IsInCaseCharSet(chFirst,pApp->m_tgtLowerCaseChars,nOffset);
		}
		if (bIsLower)
		{
			// it's a lower case belonging to the gloss or adaptation set,
			// so we don't have to capitalize it
			gbNonSourceIsUpperCase = FALSE;
			gcharNonSrcLC = chFirst;
			wxASSERT(nOffset >= 0);
			if (gbIsGlossing)
			{
				gcharNonSrcUC = GetOtherCaseChar(pApp->m_glossUpperCaseChars,nOffset);
			}
			else
			{
				gcharNonSrcUC = GetOtherCaseChar(pApp->m_tgtUpperCaseChars,nOffset);
			}
		}
		else // it's not lower case...
		{
            // chFirst is not a lower case adaptation or gloss character which has an upper
            // case equivalent, so it might be an upper case adaptation or gloss character
            // (having a lower case equivalent), or it is of indeterminate case - in which
            // case we treat it as lower case
			if (gbIsGlossing)
			{
				bIsUpper = IsInCaseCharSet(chFirst,pApp->m_glossUpperCaseChars,nOffset);
			}
			else
			{
				bIsUpper = IsInCaseCharSet(chFirst,pApp->m_tgtUpperCaseChars,nOffset);
			}
			if (bIsUpper)
			{
				// it is an upper case gloss or adaptation char for which there is a lower
				// case equivalent
				gbNonSourceIsUpperCase = TRUE;
				gcharNonSrcUC = chFirst;
				wxASSERT(nOffset >= 0);
				if (gbIsGlossing)
				{
					gcharNonSrcLC = GetOtherCaseChar(pApp->m_glossLowerCaseChars,nOffset);
				}
				else
				{
					gcharNonSrcLC = GetOtherCaseChar(pApp->m_tgtLowerCaseChars,nOffset);
				}
			}
			else
			{
				// it has indeterminate case, so treat as lower case with zero as 
				// its upper case equiv
				gbNonSourceIsUpperCase = FALSE;
				gcharNonSrcLC = chFirst;
				wxASSERT(nOffset == -1);
				gcharNonSrcUC = _T('\0');
			}
		}
	}
	return TRUE;
}



