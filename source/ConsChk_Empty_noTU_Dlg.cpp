/////////////////////////////////////////////////////////////////////////////
/// \project		adaptit
/// \file			ConsChk_Empty_noTU_Dlg.cpp
/// \author			Bruce Waters
/// \date_created	31 August2011
/// \rcs_id $Id$
/// \copyright		2011 Bruce Waters, Bill Martin, SIL International
/// \license		The Common Public License or The GNU Lesser General Public License (see license directory)
/// \description	This is the implementation file for the ConsChk_Empty_noTU_Dlg class. 
/// The ConsChk_Empty_noTU_Dlg class provides an "inconsistency found" dialog which the
/// user employs for for fixing a KB-Document inconsistency. Deals with the document pile
/// having pSrcPhrase with m_bHasKBEntry (or m_bHasGlossingKBEntry if the current mode is
/// glossing mode) TRUE, but KB lookup failed to find a CTargetUnit for the source text at
/// this location in the document
/// \derivation		The ConsChk_Empty_noTU_Dlg class is derived from AIModalDialog.
/////////////////////////////////////////////////////////////////////////////

// the following improves GCC compilation performance
#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma implementation "ConsChk_Empty_noTU_Dlg.h"
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
//#include <wx/docview.h> // needed for classes that reference wxView or wxDocument
//#include <wx/valgen.h> // for wxGenericValidator
#include "Adapt_It.h" // need this for AIModalDialog definition
#include "Adapt_It_wdr.h"
#include "helpers.h"
#include "Adapt_ItDoc.h"
#include "Adapt_ItCanvas.h"
#include "MainFrm.h"
#include "ConsChk_Empty_noTU_Dlg.h"

/// This global is defined in Adapt_It.cpp.
extern CAdapt_ItApp* gpApp; // if we want to access it fast
extern bool gbIsGlossing;

// event handler table
BEGIN_EVENT_TABLE(ConsChk_Empty_noTU_Dlg, AIModalDialog)
	EVT_INIT_DIALOG(ConsChk_Empty_noTU_Dlg::InitDialog)
	EVT_BUTTON(wxID_OK, ConsChk_Empty_noTU_Dlg::OnOK)
	//EVT_BUTTON(wxID_CANCEL, ConsChk_Empty_noTU_Dlg::OnCancel)
	EVT_RADIOBUTTON(ID_RADIO_NO_ADAPTATION, ConsChk_Empty_noTU_Dlg::OnRadioEnterEmpty)
	EVT_RADIOBUTTON(ID_RADIO_LEAVE_HOLE, ConsChk_Empty_noTU_Dlg::OnRadioLeaveHole)
	EVT_RADIOBUTTON(ID_RADIO_NOT_IN_KB, ConsChk_Empty_noTU_Dlg::OnRadioNotInKB)
	EVT_RADIOBUTTON(ID_RADIOBUTTON_TYPE_AORG, ConsChk_Empty_noTU_Dlg::OnRadioTypeAorG)	
END_EVENT_TABLE()

ConsChk_Empty_noTU_Dlg::ConsChk_Empty_noTU_Dlg(
		wxWindow* parent,
		wxString* title,
		wxString* srcPhrase,
		wxString* tgtPhrase,
		wxString* modeWord,
		wxString* modeWordPlusArticle,
		wxString* notInKBStr,
		wxString* noneOfThisStr,
		bool      bShowCentered) : AIModalDialog(parent, -1, *title, wxDefaultPosition, 
					wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    // This dialog function below is generated in wxDesigner, and defines the controls and
    // sizers for the dialog. The first parameter is the parent which should normally be
    // "this". The second and third parameters should both be TRUE to utilize the sizers
    // and create the right size dialog.
	pConsChk_Empty_noTU_DlgSizer = ConsistencyCheck_EmptyNoTU_DlgFunc(this, TRUE, TRUE);
	// The declaration is: NameFromwxDesignerDlgFunc( wxWindow *parent, bool call_fit, bool set_sizer );
	
	bool bOK;
	bOK = gpApp->ReverseOkCancelButtonsForMac(this);
	bOK = bOK; // avoid warning
	m_bShowItCentered = bShowCentered;
	m_sourcePhrase = *srcPhrase;
	m_modeWord = *modeWord;
	m_modeWordPlusArticle = *modeWordPlusArticle;
	m_notInKBStr = *notInKBStr;
	m_noneOfThisStr = *noneOfThisStr;
	m_aorgTextCtrlStr = *tgtPhrase;
	m_saveAorG = *tgtPhrase;
}

ConsChk_Empty_noTU_Dlg::~ConsChk_Empty_noTU_Dlg() // destructor
{
}

// BEW 23Apr15 added support for / as a word-breaking whitespace character
void ConsChk_Empty_noTU_Dlg::InitDialog(wxInitDialogEvent& WXUNUSED(event)) // InitDialog is method of wxWindow
{
	//InitDialog() is not virtual, no call needed to a base class
	//m_aorgTextCtrlStr.Empty(); // we'll pass it in from caller instead, from the
								 //tgtPhrase param
	m_emptyStr.Empty();

	m_bDoAutoFix = FALSE;
	m_pAutoFixChkBox = (wxCheckBox*)FindWindowById(ID_CHECK_DO_SAME);
	wxASSERT(m_pAutoFixChkBox != NULL);
	m_pAutoFixChkBox->SetValue(FALSE); // start with it turned off
	actionTaken = no_GUI_needed; // temporary default, OnOK() will set it

	m_pTextCtrlSrcText = (wxTextCtrl*)FindWindowById(ID_TEXTCTRL_SOURCE_PHRASE_1);
	wxASSERT(m_pTextCtrlSrcText != NULL);

//#if defined(FWD_SLASH_DELIM)
	// BEW added 23Apr15
	m_sourcePhrase = FwdSlashtoZWSP(m_sourcePhrase); // not editable for the user, so use ZWSP delimiters
//#endif

	// put the passed in source phrase value into the wxTextCtrl, then make it read only
	m_pTextCtrlSrcText->ChangeValue(m_sourcePhrase);
	m_pTextCtrlSrcText->SetEditable(FALSE); // now it's read-only

	m_pAorGRadioBtn = (wxRadioButton*)FindWindowById(ID_RADIOBUTTON_TYPE_AORG);
	wxASSERT(m_pAorGRadioBtn != NULL);
	// put in the correct string, for this radio button label; either "an adaptation"
	// or "a gloss" if called when in glossing mode
	wxString msg3 = m_pAorGRadioBtn->GetLabel();
	wxString radio4Str;
	radio4Str = radio4Str.Format(msg3, m_modeWordPlusArticle.c_str());
	m_pAorGRadioBtn->SetLabel(radio4Str);
	int difference2 = CalcLabelWidthDifference(msg3, radio4Str, m_pAorGRadioBtn);

	// for the adaptation or gloss text control, set it initially empty - it is always
	// editable, but the radio buttons only are enabled when it is empty
	wxString emptyStr = _T("");
	m_pTextCtrlAorG = (wxTextCtrl*)FindWindowById(ID_TEXTCTRL_TYPED_AORG);
	wxASSERT(m_pTextCtrlAorG != NULL);
	// put an empty string into the wxTextCtrl, & make it writable
	m_pTextCtrlAorG->ChangeValue(emptyStr);
	m_pTextCtrlAorG->SetEditable(TRUE);
	
	m_pMessageLabel = (wxStaticText*)FindWindowById(ID_TEXT_EMPTY_STR);
	wxASSERT(m_pMessageLabel != NULL);
	// put in the correct string, "adaptation" or "gloss" for this message label
	wxString msg = m_pMessageLabel->GetLabel(); // "The %s is empty, a knowledge base entry
												// is expected, but is absent"
	m_messageLabelStr = m_messageLabelStr.Format(msg, m_modeWord.c_str()); // %s filled in
	m_pMessageLabel->SetLabel(m_messageLabelStr);
	int difference1 = CalcLabelWidthDifference(msg, m_messageLabelStr, m_pMessageLabel);

	// calc the pixel difference needed to accomodate both labels' changed text
	int difference = wxMax(difference1, difference2);

	m_pNoAdaptRadioBtn = (wxRadioButton*)FindWindowById(ID_RADIO_NO_ADAPTATION);
	wxASSERT(m_pNoAdaptRadioBtn != NULL);
	// put in the correct strings, for this radio button label; first is
	// "adaptation" and second is "<no adaptation>" if called when in adaptation mode,
	// else "gloss" and "<no gloss>" if called when in glossing mode
	wxString msg1 = m_pNoAdaptRadioBtn->GetLabel();
	wxString radioStr1;
	radioStr1 = radioStr1.Format(msg1, m_modeWord.c_str(), m_noneOfThisStr.c_str());
	m_pNoAdaptRadioBtn->SetLabel(radioStr1);

	m_pLeaveHoleRadioBtn = (wxRadioButton*)FindWindowById(ID_RADIO_LEAVE_HOLE);
	wxASSERT(m_pLeaveHoleRadioBtn != NULL);

	m_pNotInKBRadioBtn = (wxRadioButton*)FindWindowById(ID_RADIO_NOT_IN_KB);
	wxASSERT(m_pNotInKBRadioBtn != NULL);
	m_pNotInKBRadioBtn->SetValue(FALSE);
	// hide this 3rd radio button if we are in glossing mode
	if (m_messageLabelStr.Find(_("gloss")) != wxNOT_FOUND)
	{
		m_pNotInKBRadioBtn->Hide();

		m_bGlossing = TRUE;
	}
	else
	{
		// put in the correct strings, for this radio button label; first is
		// "an adaptation" and second is "<Not In KB>" if called when in adaptation mode,
		// else "a gloss" and "<Not In KB>" if called when in glossing mode
		wxString msg2 = m_pNotInKBRadioBtn->GetLabel();
		wxString radioStr2;
		radioStr2 = radioStr2.Format(msg2, m_modeWordPlusArticle.c_str(), m_notInKBStr.c_str());
		m_pNotInKBRadioBtn->SetLabel(radioStr2);

		m_bGlossing = FALSE;
	}

	// make the fonts show user-defined font point size in the dialog
	#ifdef _RTL_FLAGS
	gpApp->SetFontAndDirectionalityForDialogControl(gpApp->m_pSourceFont, m_pTextCtrlSrcText, NULL,
								NULL, NULL, gpApp->m_pDlgSrcFont, gpApp->m_bSrcRTL);
	#else // Regular version, only LTR scripts supported, so use default FALSE for last parameter
	gpApp->SetFontAndDirectionalityForDialogControl(gpApp->m_pSourceFont, m_pTextCtrlSrcText, NULL, 
								NULL, NULL, gpApp->m_pDlgSrcFont);
	#endif

	#ifdef _RTL_FLAGS
	gpApp->SetFontAndDirectionalityForDialogControl(gpApp->m_pTargetFont, m_pTextCtrlAorG, NULL,
								NULL, NULL, gpApp->m_pDlgTgtFont, gpApp->m_bTgtRTL);
	#else // Regular version, only LTR scripts supported, so use default FALSE for last parameter
	gpApp->SetFontAndDirectionalityForDialogControl(gpApp->m_pTargetFont, m_pTextCtrlAorG, NULL, 
								NULL, NULL, gpApp->m_pDlgTgtFont);
	#endif
	// get the dialog to resize to the new label string lengths
	int width = 0;
	int myheight = 0;
	this->GetSize(&width, &myheight);
	// use the difference value calculated above to widen the dialog window and then call
	// Layout() to get the attached sizer hierarchy recalculated and laid out
	int sizeFlags = 0;
	sizeFlags |= wxSIZE_USE_EXISTING;
	int clientWidth = 0;
	int clientHeight = 0;
	CMainFrame *pFrame = gpApp->GetMainFrame();
	pFrame->GetClientSize(&clientWidth,&clientHeight);
    // ensure the adjusted width of the dialog won't exceed the client area's width for the
    // frame window
    if (difference < clientWidth - width)
	{
		this->SetSize(wxDefaultCoord, wxDefaultCoord, width + difference, wxDefaultCoord);
	}else
	{
		this->SetSize(wxDefaultCoord, wxDefaultCoord, clientWidth - 2, wxDefaultCoord);
	}
	this->Layout(); // automatically calls Layout() on top level sizer

	if (m_bShowItCentered)
	{
		this->Centre(wxHORIZONTAL);
	}
	// work out where to place the dialog window
	wxRect rectScreen;
	rectScreen = wxGetClientDisplayRect();

	wxClientDC dc(gpApp->GetMainFrame()->canvas);
	gpApp->GetMainFrame()->canvas->DoPrepareDC(dc);// adjust origin
	gpApp->GetMainFrame()->PrepareDC(dc); // wxWidgets' drawing.cpp sample also 
										  // calls PrepareDC on the owning frame
	int newXPos,newYPos;
	// CalcScrolledPosition translates logical coordinates to device ones. 
	gpApp->GetMainFrame()->canvas->CalcScrolledPosition(m_ptBoxTopLeft.x,
											m_ptBoxTopLeft.y,&newXPos,&newYPos);
	m_ptBoxTopLeft.x = newXPos;
	m_ptBoxTopLeft.y = newYPos;
	// we leave the width and height the same
	gpApp->GetMainFrame()->canvas->ClientToScreen(&m_ptBoxTopLeft.x,
									&m_ptBoxTopLeft.y); // now it's screen coords
	int stripheight = m_nTwoLineDepth;
	wxRect rectDlg;
	//GetClientSize(&rectDlg.width, &rectDlg.height); // dialog's window client area
	GetSize(&rectDlg.width, &rectDlg.height); // dialog's window frame
	rectDlg = NormalizeRect(rectDlg); // in case we ever change from MM_TEXT mode // use our own
	int dlgHeight = rectDlg.GetHeight();
	int dlgWidth = rectDlg.GetWidth();
	wxASSERT(dlgHeight > 0);

	// BEW 16Sep11, new position calcs needed, the dialog often sits on top of the phrase
	// box - better to try place it above the phrase box, and right shifted, to maximize
	// the viewing area for the layout; or if a low position is required, at bottom right
	int phraseBoxHeight;
	int phraseBoxWidth;
	gpApp->m_pTargetBox->GetSize(&phraseBoxWidth,&phraseBoxHeight); // it's the width we want
	int pixelsAvailableAtTop = m_ptBoxTopLeft.y - stripheight; // remember box is in line 2 of strip
	int pixelsAvailableAtBottom = rectScreen.GetBottom() - stripheight - pixelsAvailableAtTop - 20; // 20 for status bar
	int pixelsAvailableAtLeft = m_ptBoxTopLeft.x - 10; // -10 to clear away from the phrase box a little bit
	int pixelsAvailableAtRight = rectScreen.GetWidth() - phraseBoxWidth - m_ptBoxTopLeft.x;
	bool bAtTopIsBetter = pixelsAvailableAtTop > pixelsAvailableAtBottom;
	bool bAtRightIsBetter = pixelsAvailableAtRight > pixelsAvailableAtLeft;
	int myTopCoord;
	int myLeftCoord;
	if (bAtTopIsBetter)
	{
		if (dlgHeight + 2*stripheight < pixelsAvailableAtTop)
			myTopCoord = pixelsAvailableAtTop - (dlgHeight + 2*stripheight);
		else
		{
			if (dlgHeight > rectScreen.GetBottom())
			{
				//cut off top of dialog in preference to the bottom, where it's buttons are
				myTopCoord = rectScreen.GetBottom() - dlgHeight + 6;
				if (myTopCoord > 0)
					myTopCoord = 0;
			}
			else
				myTopCoord = 0;
		}
	}
	else
	{
		if (m_ptBoxTopLeft.y +stripheight + dlgHeight < rectScreen.GetBottom()) 
			myTopCoord = m_ptBoxTopLeft.y +stripheight;
		else
		{
			myTopCoord = rectScreen.GetBottom() - dlgHeight - 20;
			if (myTopCoord < 0)
				myTopCoord = myTopCoord + 20; // if we have to cut off any, cut off the dialog's top
		}
	}
	if (bAtRightIsBetter)
	{
		myLeftCoord = rectScreen.GetWidth() - dlgWidth;
	}
	else
	{
		myLeftCoord = 0;
	}
	// now set the position
	SetSize(myLeftCoord, myTopCoord,wxDefaultCoord,wxDefaultCoord,wxSIZE_USE_EXISTING);

	// start with the adaptation or gloss text control disabled
	EnableAdaptOrGlossBox(FALSE);

	// It's a simple dialog, I'm not bothering with validators and TransferDataTo/FromWindow calls
	//TransferDataToWindow();
}

void ConsChk_Empty_noTU_Dlg::EnableAdaptOrGlossBox(bool bEnable)
{
	if (bEnable)
	{
		// make it enabled, and it's radio button, give it input focus
		m_pTextCtrlAorG->SetEditable(TRUE);
		m_pTextCtrlAorG->Enable(TRUE);
		m_pTextCtrlAorG->SetFocus();
		m_aorgTextCtrlStr = m_saveAorG;
		m_pTextCtrlAorG->ChangeValue(m_aorgTextCtrlStr); // put earlier string back in it

		m_pAorGRadioBtn->SetValue(TRUE);
		m_pNoAdaptRadioBtn->SetValue(FALSE);
		m_pLeaveHoleRadioBtn->SetValue(FALSE);
		if (!m_bGlossing)
		{
			m_pNotInKBRadioBtn->SetValue(FALSE);
		}
	}
	else
	{
		// disable it, and all four radio buttons
		if (m_pTextCtrlAorG->IsEnabled())
		{
			// This function with FALSE passed in may be called several times if the user
			// clicks other radio buttons, so we want to disable it only the once,
			// otherwise we'd lose the saved adaptation or gloss string from it
			m_aorgTextCtrlStr = m_pTextCtrlAorG->GetValue();
			m_saveAorG = m_aorgTextCtrlStr;
			// put empty string into it before disabling it (but save earlier one first)
			m_pTextCtrlAorG->ChangeValue(m_emptyStr); 

			m_pTextCtrlAorG->SetEditable(FALSE);
			m_pTextCtrlAorG->Enable(FALSE);
		}											 
		m_pAorGRadioBtn->SetValue(FALSE);
		m_pNoAdaptRadioBtn->SetValue(FALSE);
		m_pLeaveHoleRadioBtn->SetValue(FALSE);
		if (!m_bGlossing)
		{
			m_pNotInKBRadioBtn->SetValue(FALSE);
		}
	}
}

void ConsChk_Empty_noTU_Dlg::OnRadioTypeAorG(wxCommandEvent& WXUNUSED(event)) 
{
	// enable the box for typing gloss or adaptation and put earlier string in it and make
	// the radio buttons match this state, give the box the input focus
	EnableAdaptOrGlossBox(TRUE);

	// also make the relevant radio button be turned on
	wxASSERT(m_pAorGRadioBtn != NULL);
	m_pAorGRadioBtn->SetValue(TRUE);
}


void ConsChk_Empty_noTU_Dlg::OnRadioEnterEmpty(wxCommandEvent& WXUNUSED(event)) 
{
	// disable the box for typing gloss or adaptation and put empty string in it and make
	// the radio buttons match this state
	EnableAdaptOrGlossBox(FALSE);

	// also make the relevant radio button be turned on
	wxASSERT(m_pNoAdaptRadioBtn != NULL);
	m_pNoAdaptRadioBtn->SetValue(TRUE);
}

void ConsChk_Empty_noTU_Dlg::OnRadioLeaveHole(wxCommandEvent& WXUNUSED(event)) 
{
	// disable the box for typing gloss or adaptation and put empty string in it and make
	// the radio buttons match this state
	EnableAdaptOrGlossBox(FALSE);

	// also make the relevant radio button be turned on
	wxASSERT(m_pLeaveHoleRadioBtn != NULL);
	m_pLeaveHoleRadioBtn->SetValue(TRUE);
}

void ConsChk_Empty_noTU_Dlg::OnRadioNotInKB(wxCommandEvent& WXUNUSED(event)) 
{
	// disable the box for typing gloss or adaptation and put empty string in it and make
	// the radio buttons match this state
	EnableAdaptOrGlossBox(FALSE);

	// also make the relevant radio button be turned on
	wxASSERT(m_pNotInKBRadioBtn != NULL);
	m_pNotInKBRadioBtn->SetValue(TRUE);
}


void ConsChk_Empty_noTU_Dlg::OnOK(wxCommandEvent& event)
{
	// get the auto-fix flag
	m_bDoAutoFix = m_pAutoFixChkBox->GetValue();

	// get the value of the 4th radio button, the one preceding it's textctrl
	bool bUseText = m_pAorGRadioBtn->GetValue();

	// caller should grab this string if bUseText is TRUE; otherwise it should use an
	// empty string with whatever action is indicated by which of the other radio buttons
	// is turned on
	m_aorgTextCtrlStr = m_pTextCtrlAorG->GetValue(); // set in case caller wants it

	if (!bUseText)
	{
		m_aorgTextCtrlStr = m_emptyStr; // ensure the caller can't get anything but 
										// an empty string if radio button 4 is off
		// set the fixit action
		if (m_pNoAdaptRadioBtn->GetValue())
		{
			actionTaken = store_empty_meaning;
		}
		else if (m_pLeaveHoleRadioBtn->GetValue())
		{
			actionTaken = turn_flag_off;
		}
		else
		{
			actionTaken = make_it_Not_In_KB;
		}
	}
	else
	{
		// the user wants the typed adaptation, or gloss in glossing mode, to be used; but
		// if the string is empty, beep and take control back to the dialog
		if (m_aorgTextCtrlStr.IsEmpty())
		{
			// if use wants an empty string, then only the radio buttons 1 to 3 are to be
			// used, or 1-2 if in glossing mode
			wxBell();
			return;
		}
		actionTaken = store_nonempty_meaning;
	}
	event.Skip();
}
