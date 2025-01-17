/////////////////////////////////////////////////////////////////////////////
/// \project		adaptit
/// \file			KbSharingSetup.cpp
/// \author			Bruce Waters
/// \date_created	8 October 2013
/// \rcs_id $Id: KBSharingSetup.cpp 3028 2013-01-15 11:38:00Z bruce_waters@sil.org $
/// \copyright		2013 Bruce Waters, Bill Martin, Erik Brommers, SIL International
/// \license		The Common Public License or The GNU Lesser General Public License (see license directory)
/// \description	This is the implementation file for the KbSharingSetup class.
/// The KbSharingSetup class provides a dialog for the turning on, or off, KB Sharing for
/// the currently open Adapt It project. When setting up, checkboxes stipulate which of the
/// local KBs gets shared. Default is to share just the adapting KB. As long as at least
/// one of the local KBs is shared, we have a "shared project". When removing a setup,
/// whichever or both of the shared KBs are no longer shared. "No longer shared" just means
/// that the booleans, m_bIsKBServerProject and m_bIsGlossingKBServerProject are both
/// FALSE. (Note: temporary enabling/disabling is possible within the KbServer
/// instantiation itself, this does not destroy the setup however. By default, when a setup
/// is done, sharing is enabled.)
/// This dialog does everthing of the setup except the authentication step, the latter
/// fills out the url, username, and password - after that, sharing can go ahead.
/// \derivation		The KbSharingSetup class is derived from AIModalDialog.
/////////////////////////////////////////////////////////////////////////////

// the following improves GCC compilation performance
#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma implementation "KbSharingSetup.h"
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

#if defined(_KBSERVER)

// other includes
//#include <wx/docview.h> // needed for classes that reference wxView or wxDocument
#include "Adapt_It.h"
#include "MainFrm.h"
#include "helpers.h"
#include "KbServer.h"
#include "KbSvrHowGetUrl.h"
#include "KBSharingStatelessSetupDlg.h" //  misnamed, it's actually just an authentication class
#include "KbSharingSetup.h"

// event handler table
BEGIN_EVENT_TABLE(KbSharingSetup, AIModalDialog)
	EVT_INIT_DIALOG(KbSharingSetup::InitDialog)
	EVT_BUTTON(wxID_OK, KbSharingSetup::OnOK)
	EVT_BUTTON(wxID_CANCEL, KbSharingSetup::OnCancel)
	EVT_CHECKBOX(ID_CHECKBOX_SHARE_MY_TGT_KB, KbSharingSetup::OnCheckBoxShareAdaptations)
	EVT_CHECKBOX(ID_CHECKBOX_SHARE_MY_GLOSS_KB, KbSharingSetup::OnCheckBoxShareGlosses)
END_EVENT_TABLE()

// The non-stateless contructor (internally sets m_bStateless to FALSE)
KbSharingSetup::KbSharingSetup(wxWindow* parent) // dialog constructor
	: AIModalDialog(parent, -1, _("Setup Or Remove Knowledge Base Sharing"),
				wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{

    // This dialog function below is generated in wxDesigner, and defines the controls and
    // sizers for the dialog. The first parameter is the parent which is the pointer this.
    // (Warning, do not use the frame window as the parent - the dialog will show empty and
    // the frame window will resize down to the dialog's size!) The second and third
    // parameters should both be TRUE to utilize the sizers and create the right size
    // dialog.
	m_pApp = &wxGetApp();
	kb_share_setup_or_remove_func(this, TRUE, TRUE);
	// The declaration is: functionname( wxWindow *parent, bool call_fit, bool set_sizer );
	bool bOK;
	bOK = m_pApp->ReverseOkCancelButtonsForMac(this);
	wxUnusedVar(bOK);
 }

KbSharingSetup::~KbSharingSetup() // destructor
{
}

void KbSharingSetup::InitDialog(wxInitDialogEvent& WXUNUSED(event))
{
	m_pAdaptingCheckBox = (wxCheckBox*)FindWindowById(ID_CHECKBOX_SHARE_MY_TGT_KB);
	m_pGlossingCheckBox = (wxCheckBox*)FindWindowById(ID_CHECKBOX_SHARE_MY_GLOSS_KB);
	m_pSetupBtn = (wxButton*)FindWindowById(wxID_OK);

    // If the project is currently a KB sharing project, then initialise to the current
    // values for which of the two KBs (or both) is being shared; otherwise, set the member
	// variables to the most commonly expected values (TRUE & FALSE, which is what the
	// wxDesigner initial values are, for adapting and glossing choices, respectively)
	m_bSharingAdaptations = FALSE; // initialize (user's final choice on exit is stored here)
	m_bSharingGlosses = FALSE; // initialize (user's final choice on exit is stored here)
	if (m_pApp->m_bIsKBServerProject) // adaptations KBserver 
	{
		// It's an existing shared kb project - so initialize to what the current settings
		// are, and make the checkbox comply
		m_bSharingAdaptations = TRUE; // initialize
	}
	else
	{
		m_bSharingAdaptations = FALSE; // initialize
	}
	if (m_pApp->m_bIsGlossingKBServerProject) // glosses KBserver
	{
		// It's an existing shared glossing kb project - so initialize to what the current
		// settings are, and make the checkbox comply
		m_bSharingGlosses = TRUE; // initialize
	}
	else
	{
		m_bSharingGlosses = FALSE; // initialize
	}
}

void KbSharingSetup::OnCheckBoxShareAdaptations(wxCommandEvent& WXUNUSED(event))
{
	// This handler is entered AFTER the checkbox value has been changed
	bool bTicked = m_pAdaptingCheckBox->GetValue();
	if (bTicked)
	{
		// The values set in the dialog must win
		m_pApp->m_bIsKBServerProject = TRUE;
		m_pApp->m_bIsKBServerProject_FromConfigFile = TRUE; // sets m_bSharingAdaptations to TRUE
					// within the function AuthenticateCheckAndSetupKBSharing()
	}
	else
	{
		// The values set in the dialog must win
		m_pApp->m_bIsKBServerProject = FALSE;
		m_pApp->m_bIsKBServerProject_FromConfigFile = FALSE; // sets m_bSharingAdaptations to FALSE
					// within the function AuthenticateCheckAndSetupKBSharing()
	}
}

void KbSharingSetup::OnCheckBoxShareGlosses(wxCommandEvent& WXUNUSED(event))
{
	// This handler is entered AFTER the checkbox value has been changed
	bool bTicked = m_pGlossingCheckBox->GetValue();
	if (bTicked)
	{
		// The values set in the dialog must win
		m_pApp->m_bIsGlossingKBServerProject = TRUE;
		m_pApp->m_bIsGlossingKBServerProject_FromConfigFile = TRUE; // sets m_bSharingGlosses to TRUE
					// within the function AuthenticateCheckAndSetupKBSharing()
	}
	else
	{
		// The values set in the dialog must win
		m_pApp->m_bIsGlossingKBServerProject = FALSE;
		m_pApp->m_bIsGlossingKBServerProject_FromConfigFile = FALSE; // sets m_bSharingGlosses to FALSE
					// within the function AuthenticateCheckAndSetupKBSharing()
	}
}

void KbSharingSetup::OnOK(wxCommandEvent& myevent)
{
	// Get the checkbox values - & set app flags accordingly
	bool bAdaptationsTicked = m_pAdaptingCheckBox->GetValue();
	bool bGlossesTicked = m_pGlossingCheckBox->GetValue();

	if (bAdaptationsTicked || bGlossesTicked)
	{
		// Show the dialog which allows the user to set the boolean: m_bServiceDiscoveryWanted, 
		// for the later AuthenticateCheckAndSetupKBSharing() call to use
		bool bUserCancelled = FALSE; // initialize
		KbSvrHowGetUrl* pHowGetUrl = new KbSvrHowGetUrl(m_pApp->GetMainFrame());
		pHowGetUrl->Center();
		int dlgReturnCode;

		// Hide parent window
		this->Hide();

		dlgReturnCode = pHowGetUrl->ShowModal();
		// The dialog's OnOK() handler will have set m_bServiceDiscoveryWanted to the
		// user's chosen value
		if (dlgReturnCode == wxID_OK)
		{
			if (bAdaptationsTicked)
			{
				m_pApp->m_bIsKBServerProject = TRUE;
			}
			if (bGlossesTicked)
			{
				m_pApp->m_bIsGlossingKBServerProject = TRUE;
			}
			// m_bServiceDiscoveryWanted will have been set or cleared in
			// the OnOK() handler of the above dialog
			bUserCancelled = pHowGetUrl->m_bUserClickedCancel;
			wxASSERT(pHowGetUrl->m_bUserClickedCancel == FALSE);
		}
		else
		{
			// User cancelled. This clobbers the sharing setup - that clobbering is
			// already done in the OnCancel() handler
			bUserCancelled = pHowGetUrl->m_bUserClickedCancel;
			wxASSERT(pHowGetUrl->m_bUserClickedCancel == TRUE);
		}
		delete pHowGetUrl; // We don't want the dlg showing any longer

		// If the user didn't cancel, then call Authenticate....()
		if (!bUserCancelled) // if user did not cancel...
		{
			// We don't call AuthenticateCheckAndSetupKBSharing() directly here, if we did, 
			// the Authenticate dialog is ends up lower in the z-order and the parent
			// KbSharingSetup dialog hides it - and as both are modal, the user cannot
			// get to the Authenticate dialog if control is sent back there (e.g. when the
			// password is empty, or there is a curl error, or the URL is wrong or the wanted
			// KBserver is not running). So, we post a custom event here, and the event's
			// handler will run the Authenticate dialog at idle time, when KbSharingSetup will
			// have been closed
			wxCommandEvent eventCustom(wxEVT_Call_Authenticate_Dlg);
			wxPostEvent(m_pApp->GetMainFrame(), eventCustom); // custom event handlers are in CMainFrame
		}
		else
		{
			// User canceled before Authentication could be attempted - so tell him
			// that sharing is OFF
			ShortWaitSharingOff(); //displays "Knowledge base sharing is OFF" for 1.3 seconds
		}
	}
	myevent.Skip(); // close the KbSharingSetup dialog
}

void KbSharingSetup::OnCancel(wxCommandEvent& myevent)
{
	// Cancelling from this dialog should leave the older setting unchanged. The way to
	// remove the setup is now to untick both checkboxes, and then dismiss the dialog
	myevent.Skip();
}

#endif
