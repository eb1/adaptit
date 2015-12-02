/////////////////////////////////////////////////////////////////////////////
/// \project		adaptit
/// \file			ServDisc.h
/// \author			Bruce Waters
/// \date_created	10 November 2015
/// \rcs_id $Id$
/// \copyright		2008 Bruce Waters, Bill Martin, SIL International
/// \license		The Common Public License or The GNU Lesser General Public License (see license directory)
/// \description	This is the header file for a lightweight C++ class
///                 which has no wx.h dependency, and which instantiates
///                 the CServiceDiscovery class (doing so to avoid namespace
///                 classes with std:: names versus wxWidgets names in app.h
///                 in particular - the Yield() macro conflicts).
/// \derivation		The ServDisc class is derived from wxObject.
/////////////////////////////////////////////////////////////////////////////

#if defined(_KBSERVER)

#ifndef SERVDISC_h
#define SERVDISC_h

// the following improves GCC compilation performance
#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "ServDisc.h"
#endif

// Forward declarations
class wxString;
class CServiceDiscovery;
class CAdapt_ItApp;

namespace std {}
using namespace std;

//#include "wx/thread.h"
//#include <wx/event.h>


// The order of the base classes must be as below. Reverse order puts wxEvtHandler at a non-zero
// offset in the derived class, making it likely that ptr-to-member of wxEventHandler
// would generate incorrect code. A C4407 compiler error is given for the reverse order. Either
// I keep the order and have to use classnames to qualify pointer-to-member, or I need to alter
// the base class order - clearly the latter is easiest. And it worked.
class ServDisc : public wxEvtHandler, public wxThread
{
public:
	ServDisc(wxThreadKind = wxTHREAD_DETACHED); // detached is default, but we'll pass in wxTHREAD_JOINABLE
												// when we instantiate within DoServiceDiscovery
	//ServDisc(wxString workFolderPath, wxString serviceStr);
	virtual ~ServDisc();

	wxString		m_workFolderPath; // location where we'll temporarily store a file of results
	wxString m_serviceStr; // service to be scanned for

	bool     m_bServDiscCanExit;
	CAdapt_ItApp* m_pApp;
	CServiceDiscovery* m_pServiceDisc;
	CServiceDiscovery* m_backup_ThisPtr; // m_pServiceDisc gets reset to 0xcdcdcdcd before
		// it can be deleted, so I'll store a copy here, and use it to restore the pointer
		// in the onServDiscHalting() handler when I want to get m_pServiceDisc deleted

	// for support of subclassing from wxThread...
	// wxThread::OnExit() is called when the thread exits at termination - for self
	// destruction termination or by Delete(), but not if Kill() is used - the latter
	// should never be used, it can leave resources in an indeterminate state
	virtual void		OnExit();

	// This must be defined to have our work content - this is where thread execution
	// begins. Our thread will be of the joinable type (wxTHREAD_JOINABLE)
	virtual void*		Entry();

	bool    TestDestroy(); // terminate when m_bServDiscCanExit goes TRUE

protected:
	void onServDiscHalting(wxCommandEvent& event);

private:
	DECLARE_EVENT_TABLE();

    //DECLARE_DYNAMIC_CLASS(ServDisc)
};

#endif // SERVDISC_h

#endif // _KBSERVER


