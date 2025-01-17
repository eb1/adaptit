/////////////////////////////////////////////////////////////////////////////
/// \project		adaptit
/// \file			KBSharing.h
/// \author			Bruce Waters
/// \date_created	14 January 2013
/// \rcs_id $Id: KBSharing.h 3025 2013-01-14 18:18:00Z jmarsden6@gmail.com $
/// \copyright		2013 Bruce Waters, Bill Martin, SIL International
/// \license		The Common Public License or The GNU Lesser General Public License (see license directory)
/// \description	This is the implementation file for the KBSharing class.
/// The KBSharing class provides a dialog for the turning on or off KB Sharing, and for
/// controlling the non-automatic functionalities within the KB sharing feature.
/// \derivation		The KBSharing class is derived from AIModalDialog.
/////////////////////////////////////////////////////////////////////////////
// Pending Implementation Items in KBSharing.cpp (in order of importance): (search for "TODO")
// 1.
//
// Unanswered questions: (search for "???")
// 1.
//
/////////////////////////////////////////////////////////////////////////////

// the following improves GCC compilation performance
#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma implementation "KBSharing.h"
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
#include <wx/spinctrl.h>

#include "Adapt_It.h"
#include "KB.h"
#include "KbServer.h"
#include "KBSharing.h"
#include "Timer_KbServerChangedSince.h"

/// This global is defined in Adapt_It.cpp.
extern CAdapt_ItApp* gpApp; // if we want to access it fast
extern bool gbIsGlossing;

// event handler table
BEGIN_EVENT_TABLE(KBSharing, AIModalDialog)
	EVT_INIT_DIALOG(KBSharing::InitDialog)
	EVT_BUTTON(wxID_OK, KBSharing::OnOK)
	EVT_BUTTON(wxID_CANCEL, KBSharing::OnCancel)
	EVT_BUTTON(ID_GET_ALL, KBSharing::OnBtnGetAll)
	EVT_BUTTON(ID_GET_RECENT, KBSharing::OnBtnChangedSince)
	EVT_BUTTON(ID_SEND_ALL, KBSharing::OnBtnSendAll)
	EVT_RADIOBOX(ID_RADIO_SHARING_OFF, KBSharing::OnRadioOnOff)
	EVT_SPINCTRL(ID_SPINCTRL_RECEIVE, KBSharing::OnSpinCtrlReceiving)
END_EVENT_TABLE()


KBSharing::KBSharing(wxWindow* parent) // dialog constructor
	: AIModalDialog(parent, -1, _("Controls For Knowledge Base Sharing"),
				wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{

	// This dialog function below is generated in wxDesigner, and defines the controls and sizers
	// for the dialog. The first parameter is the parent which should normally be "this".
	// The second and third parameters should both be TRUE to utilize the sizers and create the right
	// size dialog.
	kb_sharing_dlg_func(this, TRUE, TRUE);
	// The declaration is: GoToDlgFunc( wxWindow *parent, bool call_fit, bool set_sizer );
	bool bOK;
	bOK = gpApp->ReverseOkCancelButtonsForMac(this);
	bOK = bOK; // avoid warning
	oldReceiveInterval = gpApp->m_nKbServerIncrementalDownloadInterval; // get the current value (minutes)
	receiveInterval = oldReceiveInterval; // initialize to the current value
}

KBSharing::~KBSharing() // destructor
{

}

void KBSharing::OnOK(wxCommandEvent& myevent)
{
	// update the receiving interval, if the user has changed it, and if changed, then
	// restart the timer with the new interval; but do this only provided the timer is
	// currently instantiated and is running (KB sharing might be temporarily disabled, or
	// enabled, we don't care which - and it doesn't matter as far as setting the interval
	// is concerned)
	if (receiveInterval != oldReceiveInterval && 
		m_pApp->m_pKbServerDownloadTimer != NULL &&
		m_pApp->m_pKbServerDownloadTimer->IsRunning())
	{
		// The user has changed the interval setting (minutes), so store the new value
		m_pApp->m_nKbServerIncrementalDownloadInterval = receiveInterval;

		// restart the time with the new interval
		m_pApp->m_pKbServerDownloadTimer->Start(60000 * receiveInterval); // param is milliseconds
	}
	// Tell the app whether the user has kb sharing temporarily off, or not
	m_pApp->m_bKBSharingEnabled = bKBSharingEnabled;

	myevent.Skip();
}

void KBSharing::InitDialog(wxInitDialogEvent& WXUNUSED(event))
{
	m_pApp = (CAdapt_ItApp*)&wxGetApp();

	m_pBtnGetAll = (wxButton*)FindWindowById(ID_GET_ALL);
	m_pRadioBox = (wxRadioBox*)FindWindowById(ID_RADIO_SHARING_OFF);
	m_pSpinReceiving = (wxSpinCtrl*)FindWindowById(ID_SPINCTRL_RECEIVE);

	// initialize this; it applies to whatever KBserver(s) are open for business -
	// but only one at a time can be active, since glossing is a different mode
	// than adapting
	bKBSharingEnabled = TRUE;

	// get the current state of the two radio buttons
	KbServer* pAdaptingSvr = m_pApp->GetKbServer(1); // both are in same state, so this one is enough
	m_nRadioBoxSelection = pAdaptingSvr->IsKBSharingEnabled() ? 0 : 1;
	m_pRadioBox->SetSelection(m_nRadioBoxSelection);

	// update the 'save' boolean to whatever is the current state (user may Cancel and
	// we would need to restore the initial state)
	bSaveKBSharingEnabled = m_nRadioBoxSelection == 0 ? TRUE: FALSE;
	bKBSharingEnabled = bSaveKBSharingEnabled;

	// initialize the spin control to the current value (from project config file, or as
	// recently changed by the user)
	receiveInterval = m_pApp->m_nKbServerIncrementalDownloadInterval;
	// put the value in the box
	if (m_pSpinReceiving != NULL)
	{
		m_pSpinReceiving->SetValue(receiveInterval);
	}
}

void KBSharing::OnCancel(wxCommandEvent& myevent)
{
	m_pApp->m_bKBSharingEnabled = bSaveKBSharingEnabled; 
	myevent.Skip();
}

void KBSharing::OnSpinCtrlReceiving(wxSpinEvent& WXUNUSED(event))
{
	receiveInterval = m_pSpinReceiving->GetValue();
	if (receiveInterval > 120)
		receiveInterval = 120;
	if (receiveInterval < 1)
		receiveInterval = 1;
	// units for the above are minutes; so multiply by 60,000 to get milliseconds
}

void KBSharing::OnRadioOnOff(wxCommandEvent& WXUNUSED(event))
{
	// We shouldn't be able to see the dlg if its not a KB sharing project,
	// let alone get this far!!
	wxASSERT(m_pApp->m_bIsKBServerProject || m_pApp->m_bIsGlossingKBServerProject);

	// Get the new state of the radiobox
	m_nRadioBoxSelection = m_pRadioBox->GetSelection();
	// make the KB sharing state match the new setting; both KbServer instances must be
	// changed in parallel
	if (m_pApp->m_bIsKBServerProject || m_pApp->m_bIsGlossingKBServerProject)
	{
        // respond only if this project is currently designated as one for supporting KB
        // sharing
		if (m_nRadioBoxSelection == 0)
		{
			// This is the first button, the one for sharing to be ON
			KbServer* pAdaptingSvr = m_pApp->GetKbServer(1);
			KbServer* pGlossingSvr = m_pApp->GetKbServer(2);
			if (pAdaptingSvr != NULL)
			{
				pAdaptingSvr->EnableKBSharing(TRUE);
			}
			if (pGlossingSvr != NULL)
			{
				pGlossingSvr->EnableKBSharing(TRUE);
			}
			bKBSharingEnabled = TRUE;
		}
		else
		{
			// This is the second button, the one for sharing to be OFF
			KbServer* pAdaptingSvr = m_pApp->GetKbServer(1);
			KbServer* pGlossingSvr = m_pApp->GetKbServer(2);
			if (pAdaptingSvr != NULL)
			{
				pAdaptingSvr->EnableKBSharing(FALSE);
			}
			if (pGlossingSvr != NULL)
			{
				pGlossingSvr->EnableKBSharing(FALSE);
			}
			bKBSharingEnabled = FALSE;
		}
	}
}

void KBSharing::OnBtnGetAll(wxCommandEvent& WXUNUSED(event))
{
	KbServer* pKbServer;
	CKB* pKB = NULL; // it will be set either to m_pKB (the adapting KB) or m_pGlossingKB
	if (gbIsGlossing)
	{
		// work with the glossing entries only
		pKbServer = m_pApp->GetKbServer(2); // 2 indicates we deal with glosses
		// which CKB instance is now also determinate
		pKB = m_pApp->m_pGlossingKB;
		if (pKbServer != NULL)
		{
			pKbServer->DownloadToKB(pKB, getAll);
		}
		else
		{
			// The KbServer[1] instantiation for glossing entries either failed or has not yet been done,
			// tell the developer
			wxString msg = _T("OnBtnGetAll(): KbServer[1] is NULL, so instantiation for glossing entries either failed or has not yet been done.\nApp continues, but nothing was downloaded.");
			wxMessageBox(msg, _T("Error, no glossing KbServer instance available"), wxICON_ERROR | wxOK);
			m_pApp->LogUserAction(msg);
		}
	}
	else
	{
		// work with adaptations entries only
		pKbServer = m_pApp->GetKbServer(1); // 1 indicates we deal with adaptations
		// which CKB instance is now also determinate
		pKB = m_pApp->m_pKB; // the adaptations KB
		if (pKbServer != NULL)
		{
			pKbServer->DownloadToKB(pKB, getAll);
		}
		else
		{
			// The KbServer[0] instantiation for glossing entries either failed or has not yet been done,
			// tell the developer
			wxString msg = _T("OnBtnGetAll(): The KbServer[0] is NULL, so instantiation for adapting entries either failed or has not yet been done.\nApp continues, but nothing was downloaded.");
			wxMessageBox(msg, _T("Error, no adapting KbServer instance available"), wxICON_ERROR | wxOK);
			m_pApp->LogUserAction(msg);
		}
	}
	// make the dialog close
	EndModal(wxID_OK);
}

void KBSharing::OnBtnChangedSince(wxCommandEvent& WXUNUSED(event))
{
	KbServer* pKbServer;
	CKB* pKB = NULL; // it will be set either to m_pKB (the adapting KB) or m_pGlossingKB
	if (gbIsGlossing)
	{
		// work with the glossing entries only
		pKbServer = m_pApp->GetKbServer(2); // 2 indicates we deal with glosses
		// which CKB instance is now also determinate
		pKB = m_pApp->m_pGlossingKB;
		if (pKbServer != NULL)
		{
			pKbServer->DownloadToKB(pKB, changedSince);
		}
		else
		{
			// The KbServer[1] instantiation for glossing entries either failed or has not yet been done,
			// tell the developer
			wxString msg = _T("OnBtnChangedSince(): KbServer[1] is NULL, so instantiation for glossing entries either failed or has not yet been done.\nApp continues, but nothing was downloaded.");
			wxMessageBox(msg, _T("Error, no glossing KbServer instance available"), wxICON_ERROR | wxOK);
			m_pApp->LogUserAction(msg);
		}
	}
	else
	{
		// work with adaptations entries only
		pKbServer = m_pApp->GetKbServer(1); // 1 indicates we deal with adaptations
		// which CKB instance is now also determinate
		pKB = m_pApp->m_pKB; // the adaptations KB
		if (pKbServer != NULL)
		{
			pKbServer->DownloadToKB(pKB, changedSince);
		}
		else
		{
			// The KbServer[0] instantiation for glossing entries either failed or has not yet been done,
			// tell the developer
			wxString msg = _T("OnBtnChangedSince(): The KbServer[0] is NULL, so instantiation for adapting entries either failed or has not yet been done.\nApp continues, but nothing was downloaded.");
			wxMessageBox(msg, _T("Error, no adapting KbServer instance available"), wxICON_ERROR | wxOK);
			m_pApp->LogUserAction(msg);
		}
	}
	// make the dialog close
	EndModal(wxID_OK);
}

void KBSharing::OnBtnSendAll(wxCommandEvent& WXUNUSED(event))
{
	KbServer* pKbServer;

	pKbServer = m_pApp->GetKbServer((gbIsGlossing ? 2 : 1));

	// BEW comment 31Jan13
	// I think that temporally long operations like uploading a whole KB or downloading the
	// server's contents for a given project should not be done on a work thread. My
	// reasoning is the following... The dialog will close (or if EndModal() below is
	// omitted in this handler, the dialog will become responsive again) and the user may
	// think that the upload or download is completed, when in fact a separate process may
	// have a few minutes to run before it completes. The user, on closure of the dialog,
	// may exit the project, or shut down the application - either of which will destroy
	// the code resources which the thread is relying on to do its work. To avoid this
	// kind of problem, long operations should be done synchronously, and be tracked by
	// the progress indicator at least - and they should close the dialog when they complete.
	// 
	// BEW 29Jan15 Another kbserver client may happen to succeed in uploading a new entry 
	// which is being attempted to be inserted into the remote DB by one of the up to 50 
	// threads - if so, then when the thread with that same entry tries to have it entered
	// into the remote DB, it will generate a http 401 error in the BulkEntry() call. We
	// return all >= 400 errors as CURLE_HTTP_RETURNED_ERROR, (not that that has any special
	// significance) but m_returnedCurlCodes[] array will store it. We check for there being
	// at least one such error - if there is at least one, we call UploadToKbServer() a second
	// time (there should be far fewer entries to upload on the second try, and the probability
	// of the same error happening again almost non-zero; so we check again, and if we find
	// an error still, we call it a third time. We don't check again, and even if there was still
	// a failure of one of the bulk upload subsets to get all its entries into the mysql DB, 
	// we'll not tell the user and not try again. We just assume that the entries not inserted
	// won't change the adaptation experience in any significant way, and let the dialog be
	// dismissed (which to the user will be interpretted as full success)

	// BEW Note, 25Jun15. Even though care is taken to avoid uploading an entry which is
	// already in the remote server, my debug logging indicates that a few such do happen.
	// For example, my most recent out-of-kb-sharing-mode adaptations resulted in 145 new
	// pairs waiting for upload. When I uploaded them, there were 4 duplicates. However,
	// my code is done in such a way that this type of HTTP error, 400 Bad Request, returns
	// a CURLcode value of 0, because we don't want the user to have to see such occasional
	// errors - they do no harm to the remote kb, and no harm to to the client either, so
	// we simply ignore them. The bulk upload, similarly, therefore ignores them. Hence
	// this type of error will not cause a repeat try of the UploadToKbServer() call.

	// First iteration - it should succeed in all but rare circumstances - see above
	//
	// Timing feedback: the UploadToKbServer() call took 13 seconds, for 145 entries, in
	// my debug build (Release build would be quicker); most of the time is taken in the
	// bulk download and comparison of local versus remote data to find out what to upload.
	// I was uploading to a KBserver in VBox VM on the same computer - my XPS Win7 machine.
	pKbServer->ClearReturnedCurlCodes(); // sets the array to 50 zeros
	pKbServer->UploadToKbServer();

	// Check for an error, if there was one, redo the upload
	if (!pKbServer->AllEntriesGotEnteredInDB())
	{
#if defined(_DEBUG)
		wxLogDebug(_T("\nOnBtnSendAll(): UploadToKbServer() had one or more chunk errors. Calling it second time to upload any not sent.\n%s"),
			(pKbServer->ShowReturnedCurlCodes()).c_str());
#endif
		// Have a second try - far fewer entries should be involved in a second try
		pKbServer->ClearReturnedCurlCodes();
		pKbServer->UploadToKbServer();

		// Have a third and last try if necessary if the last call still generated an error, 
		// if there was one, redo the upload
		if (!pKbServer->AllEntriesGotEnteredInDB())
		{
#if defined(_DEBUG)
			wxLogDebug(_T("\n\nOnBtnSendAll(): UploadToKbServer() second try had one or more chunk errors. Calling it a third and last time to upload any not sent.\n%s"),
				(pKbServer->ShowReturnedCurlCodes()).c_str());
#endif
			// A third try - even fewer entries should be involved this time
			pKbServer->ClearReturnedCurlCodes();
			pKbServer->UploadToKbServer();
		}
	}
	pKbServer->ClearReturnedCurlCodes();
	
	// make the dialog close (a good way to say, "it's been done")
	EndModal(wxID_OK);
}

#endif
