/////////////////////////////////////////////////////////////////////////////
/// \project		adaptit
/// \file			PlaceInternalPunct.cpp
/// \author			Bill Martin
/// \date_created	15 May 2004
/// \rcs_id $Id$
/// \copyright		2008 Bruce Waters, Bill Martin, SIL International
/// \license		The Common Public License or The GNU Lesser General Public License (see license directory)
/// \description	This is the implementation file for the CPlaceInternalPunct class. 
/// The CPlaceInternalPunct class provides a dialog for the user to manually control placement
/// of target text punctuation, or the user can ignore the placement. This class is instantiated
/// from only one place, in the view's MakeTargetStringIncludingPunctuation() function.
/// \derivation		The CPlaceInternalPunct class is derived from AIModalDialog.
/////////////////////////////////////////////////////////////////////////////
// Pending Implementation Items in PlaceInternalPunct.cpp (in order of importance): (search for "TODO")
// 1. 
//
// Unanswered questions: (search for "???")
// 1. 
// 
/////////////////////////////////////////////////////////////////////////////

// the following improves GCC compilation performance
#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma implementation "PlaceInternalPunct.h"
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

// other includes
#include <wx/docview.h> // needed for classes that reference wxView or wxDocument
#include <wx/valgen.h> // for wxGenericValidator
#include "Adapt_It.h"
#include "PlaceInternalPunct.h"
#include "SourcePhrase.h"
#include "Adapt_ItView.h"
#include "helpers.h"

extern wxArrayString* gpRemainderList;

/// This global is defined in Adapt_It.cpp.
extern CAdapt_ItApp* gpApp; // if we want to access it fast

// event handler table
BEGIN_EVENT_TABLE(CPlaceInternalPunct, AIModalDialog)
	EVT_INIT_DIALOG(CPlaceInternalPunct::InitDialog)
	EVT_BUTTON(IDC_BUTTON_PLACE, CPlaceInternalPunct::OnButtonPlace)
	EVT_BUTTON(wxID_OK, CPlaceInternalPunct::OnOK)
END_EVENT_TABLE()


CPlaceInternalPunct::CPlaceInternalPunct(wxWindow* parent) // dialog constructor
	: AIModalDialog(parent, -1, _("Placement Of Phrase-Medial Punctuation"),
				wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	// This dialog function below is generated in wxDesigner, and defines the controls and sizers
	// for the dialog. The first parameter is the parent which should normally be "this".
	// The second and third parameters should both be TRUE to utilize the sizers and create the right
	// size dialog.
	pPlaceInternalSizer = PlaceInternalPunctDlgFunc(this, TRUE, TRUE);
	// The declaration is: PlaceInternalPunctDlgFunc( wxWindow *parent, bool call_fit, bool set_sizer );
	
	m_srcPhrase = _T("");
	m_tgtPhrase = _T("");

	m_pView = gpApp->GetView();
	wxASSERT(m_pView->IsKindOf(CLASSINFO(CAdapt_ItView)));

	// use wxValidator for simple dialog data transfer
	m_psrcPhraseBox = (wxTextCtrl*)FindWindowById(IDC_EDIT_SRC);
	//m_psrcPhraseBox->SetValidator(wxGenericValidator(&m_srcPhrase));

	m_ptgtPhraseBox = (wxTextCtrl*)FindWindowById(IDC_EDIT_TGT);
	//m_ptgtPhraseBox->SetValidator(wxGenericValidator(&m_tgtPhrase));

	m_pListPunctsBox = (wxListBox*)FindWindowById(IDC_LIST_PUNCTS);

	pTextCtrlAsStaticPlaceIntPunct = (wxTextCtrl*)FindWindowById(ID_TEXTCTRL_AS_STATIC_PLACE_INT_PUNCT);
	wxASSERT(pTextCtrlAsStaticPlaceIntPunct != NULL);
	wxColor backgrndColor = this->GetBackgroundColour();
	//pTextCtrlAsStaticPlaceIntPunct->SetBackgroundColour(backgrndColor);
	pTextCtrlAsStaticPlaceIntPunct->SetBackgroundColour(gpApp->sysColorBtnFace);
}

CPlaceInternalPunct::~CPlaceInternalPunct() // destructor
{
	
}

void CPlaceInternalPunct::InitDialog(wxInitDialogEvent& WXUNUSED(event)) // InitDialog is method of wxWindow
{
	//InitDialog() is not virtual, no call needed to a base class

	// point to the source phrase and its medial puncts list
	wxArrayString* pList = gpRemainderList; // use the global pointer to the list
	wxASSERT(pList->GetCount() > 0); // there must be a reason for using this dialog

	// make the edit boxes & list box use the correct fonts, use user-defined size
	// & use the current source and target language fonts for the list box
	// and edit boxes (in case there are special characters)
	#ifdef _RTL_FLAGS
	gpApp->SetFontAndDirectionalityForDialogControl(gpApp->m_pSourceFont, m_psrcPhraseBox, NULL,
								NULL, NULL, gpApp->m_pDlgSrcFont, gpApp->m_bSrcRTL);
	#else // Regular version, only LTR scripts supported, so use default FALSE for last parameter
	gpApp->SetFontAndDirectionalityForDialogControl(gpApp->m_pSourceFont, m_psrcPhraseBox, NULL, 
								NULL, NULL, gpApp->m_pDlgSrcFont);
	#endif

	#ifdef _RTL_FLAGS
	gpApp->SetFontAndDirectionalityForDialogControl(gpApp->m_pTargetFont, m_ptgtPhraseBox, NULL,
								m_pListPunctsBox, NULL, gpApp->m_pDlgTgtFont, gpApp->m_bTgtRTL);
	#else // Regular version, only LTR scripts supported, so use default FALSE for last parameter
	gpApp->SetFontAndDirectionalityForDialogControl(gpApp->m_pTargetFont, m_ptgtPhraseBox, NULL, 
								m_pListPunctsBox, NULL, gpApp->m_pDlgTgtFont);
	#endif

	// set the list box contents to the punctuations stored in the list (however many remain)
	wxString str;
	int count = pList->GetCount();

	for ( int n = 0; n < count; n++ )
	{
		str = pList->Item(n);
		if (str.IsEmpty())
			continue;
		m_pListPunctsBox->Append(str);
	}

	// select the first string in the listbox by default
	if (m_pListPunctsBox->GetCount() > 0)
		m_pListPunctsBox->SetSelection(0,TRUE);

	// set the source text edit box contents, and ditto for the target text box
	m_srcPhrase = m_pSrcPhrase->m_srcPhrase;
	m_tgtPhrase = gpApp->m_targetPhrase; // at the time the Place... dialog is shown, which 
	// happens from within MakeTargetStringIncludingPunctuation(), the CSourcePhrase memter m_targetStr
	// will not have been updated yet, in fact, that updating is in progress -- and the
	// only place where the current form of the target text string is to be found, is in
	// the m_targetPhrase member on the application class. (MakeTargetStringIncludingPunctuation() may also
	// be called from within StoreText(), but for KB data insertions & updates, we
	// suppress its call, so we don't have a need to set m_tgtPhrase here from m_targetStr ever)

	m_psrcPhraseBox->SetValue(m_srcPhrase);
	m_ptgtPhraseBox->SetValue(m_tgtPhrase);
}

void CPlaceInternalPunct::OnButtonPlace(wxCommandEvent& WXUNUSED(event))
{
	long nStart;
	long nEnd;
	m_ptgtPhraseBox->GetSelection(&nStart,&nEnd);
	//int len = m_tgtPhrase.Length();
	// BEW 5Mar15 comment out this test and message. Working with Sally Barton in Canberra it was
	// a problem, she had a single word translation and the source text was 3 words with comma after
	// the first word, and this test prevented comma placement at the end of the single world. It's 
	// better to allow the placement be anywhere, and if at the end and there is stored ending punctuation
	// which is different, the placed one is given preference - that's consistent with how things behave
	// elsewhere in the app, so that's what we do now
	//if (nEnd > len || nEnd < 0 || nEnd == 0 || nEnd == len )
	//{
		// IDS_WRONG_PASTE_LOCATION
	//	wxMessageBox(_("Sorry, within this dialog it is an error to place phrase medial punctuation at the beginning or end of a phrase."),_T(""), wxICON_INFORMATION | wxOK);
	//	return;
	//}

	// must not allow a selection to cause loss of data, so to be safe
	// we remove any selection the user might have left in the box
	if (nEnd != nStart)
	{
		nStart = nEnd;
	}
	m_ptgtPhraseBox->SetSelection(nStart,nEnd);
	int strLen = 1; // default - we place something!

	if (m_pListPunctsBox->GetCount() == 0)
	{
		nEnd = 0;
		goto a;
	}

	{
		if (!ListBoxPassesSanityCheck((wxControlWithItems*)m_pListPunctsBox))
		{
			// message can be in English, it's never likely to occur
			wxMessageBox(_T("List box error when getting the current selection, Adaptit will do nothing."),
				_T(""), wxICON_EXCLAMATION | wxOK);
			return;
		}

		int nSel;
		nSel = m_pListPunctsBox->GetSelection();
		//if (nSel == -1) //LB_ERR 
		//{
		//	// message can be in English, it's never likely to occur
		//	wxMessageBox(_T("List box error when getting the current selection, Adaptit will do nothing."),
		//		_T(""), wxICON_EXCLAMATION);
		//	return;
		//}

		// get the selected string
		wxString str;
		str = m_pListPunctsBox->GetStringSelection(); // the list box punctuation at wherever selection is
		
		wxString target = m_tgtPhrase; // copy of the box's string
		m_pListPunctsBox->Delete(nSel); 
		if (m_pListPunctsBox->GetCount() > 0)
			m_pListPunctsBox->SetSelection(0,TRUE); // get the next one ready for placing
		strLen = str.Length();

		// put the punctuation at the cursor location & update the dialog
		target = InsertInString(target,(int)nEnd,str);
		m_tgtPhrase = target;
	}

a:	m_ptgtPhraseBox->SetValue(m_tgtPhrase); // keep the box contents agreeing with the modified string
	m_ptgtPhraseBox->SetFocus();
	m_ptgtPhraseBox->SetSelection(nEnd+strLen,nEnd+strLen); 
}

// whm added 13Jan12 to compensate for commenting out the SetValidator() calls in the constructor
void CPlaceInternalPunct::OnOK(wxCommandEvent& event)
{
	m_srcPhrase = m_psrcPhraseBox->GetValue();
	m_tgtPhrase = m_ptgtPhraseBox->GetValue();
	event.Skip(); //EndModal(wxID_OK); //AIModalDialog::OnOK(event); // not virtual in wxDialog
}

