/////////////////////////////////////////////////////////////////////////////
/// \project		adaptit
/// \file			KBSharingMgrTabbedDlg.h
/// \author			Bruce Waters
/// \date_created	02 July 2013
/// \rcs_id $Id: KBSharingMgrTabbedDlg.h 2883 2012-11-12 03:58:57Z adaptit.bruce@gmail.com $
/// \copyright		2013 Bruce Waters, Bill Martin, Erik Brommers, SIL International
/// \license		The Common Public License or The GNU Lesser General Public License (see license directory)
/// \description	This is the header file for the KBSharingMgrTabbedDlg class.
/// The KBSharingMgrTabbedDlg class provides a dialog with tabbed pages in which an
/// appropriately authenticated user/manager of a remote KBserver installation may add,
/// edit or remove users stored in the user table of the mysql server, and/or add or
/// remove knowledge base definitions stored in the kb table of the mysql server.
/// \derivation		The KBSharingMgrTabbedDlg class is derived from AIModalDialog.
/////////////////////////////////////////////////////////////////////////////

#ifndef KBSharingMgrTabbedDlg_h
#define KBSharingMgrTabbedDlg_h

// the following improves GCC compilation performance
#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "KBSharingMgrTabbedDlg.h"
#endif
#if defined(_KBSERVER)
// needed for the KbServerUser and KbServerKb structures
#include "KbServer.h"

// forward declarations
class UsersList;
class CodesList;
class LanguagesList;

class KBSharingMgrTabbedDlg : public AIModalDialog
{
public:
	KBSharingMgrTabbedDlg(wxWindow* parent); // constructor
	virtual ~KBSharingMgrTabbedDlg(void); // destructor

	KbServer* GetKbServer(); // gets whatever m_pKbServer is pointing at
	KbServer* m_pKbServer;   // we'll assign the stateless one to this pointer


protected:
	wxNotebook*    m_pKBSharingMgrTabbedDlg;
	wxListBox*     m_pUsersListBox;

	wxTextCtrl*	   m_pConnectedTo;
	wxTextCtrl*    m_pTheUsername;
	wxTextCtrl*    m_pEditInformalUsername;
	wxTextCtrl*    m_pEditPersonalPassword;
	wxTextCtrl*    m_pEditPasswordTwo;
	wxCheckBox*    m_pCheckUserAdmin;
	wxCheckBox*    m_pCheckKbAdmin;

	wxButton*      m_pBtnUsersClearControls;
	wxButton*      m_pBtnUsersAddUser;
	wxButton*      m_pBtnUsersEditUser;
	wxButton*      m_pBtnUsersRemoveUser;


	// For Create KB Definitions page

	wxRadioButton* m_pRadioKBType1;
	wxRadioButton* m_pRadioKBType2;
	wxListBox*     m_pKbsListBox;
	wxStaticText*  m_pNonSrcLabel;
	wxStaticText*  m_pAboveListBoxLabel;

	wxTextCtrl*    m_pEditSourceCode;
	wxTextCtrl*    m_pEditNonSourceCode;
	wxTextCtrl*    m_pKbDefinitionCreator; // this one is read-only
	wxButton*	   m_pBtnUsingRFC5646Codes;
	wxButton*      m_pBtnAddKbDefinition;
	wxButton*      m_pBtnClearBothLangCodeBoxes;
	wxButton*      m_pBtnLookupLanguageCodes;
	wxButton*      m_pBtnRemoveSelectedKBDefinition;
	wxButton*	   m_pBtnClearListSelection;

	// For Create or Delete Custom Codes page
	wxListBox*     m_pCustomCodesListBox;
	wxTextCtrl*    m_pEditCustomCode;
	wxTextCtrl*    m_pEditDescription;
	wxTextCtrl*    m_pCustomCodeDefinitionCreator; // this one is read-only
	wxButton*	   m_pBtnUsingRFC5646CustomCodes;
	wxButton*      m_pBtnCreateCustomCode;
	wxButton*      m_pBtnDeleteCustomCode;
	wxButton*      m_pBtnClearBothBoxes;
	wxButton*      m_pBtnLookupISO639Code;
	wxButton*	   m_pBtnRemoveListSelection;

	// local copies of globals on the App, for the person using the Manager dialog
	bool           m_bKbAdmin;   // for m_kbserver_kbadmin
	bool		   m_bUserAdmin; // for m_kbserver_useradmin

	wxString rfc5654guidelines; // read in from "RFC56554_guidelines.txt in adaptit\docs\ folder,
								// & is a versioned file
	int m_nCurPage;
#ifdef __WXGTK__
	bool			m_bUsersListBoxBeingCleared;
	bool			m_bSourceKbsListBoxBeingCleared;
	bool			m_bTargetKbsListBoxBeingCleared;
	bool			m_bGlossKbsListBoxBeingCleared;
#endif

	void			InitDialog(wxInitDialogEvent& WXUNUSED(event));
	void			OnOK(wxCommandEvent& event);
	void			OnCancel(wxCommandEvent& event);
public:
	void			LoadDataForPage(int pageNumSelected);
protected:
	void			DisplayRFC5646Message();
	void			OnTabPageChanged(wxNotebookEvent& event);


	// Functions needed by the Users page
	KbServerUser*	GetUserStructFromList(UsersList* pUsersList, size_t index);
	void			LoadUsersListBox(wxListBox* pListBox, size_t count, UsersList* pUsersList);
	void			CopyUsersList(UsersList* pSrcList, UsersList* pDestList);
	KbServerUser*	CloneACopyOfKbServerUserStruct(KbServerUser* pExistingStruct);
	void			DeleteClonedKbServerUserStruct();
	bool			CheckThatPasswordsMatch(wxString password1, wxString password2);
	bool			AreBothPasswordsEmpty(wxString password1, wxString password2);
	wxString		GetEarliestUseradmin(UsersList* pUsersList);
	KbServerUser*	GetThisUsersStructPtr(wxString& username, UsersList* pUsersList);
	KbServerLanguage* GetLanguageStructFromList(LanguagesList* pLanguagesList, size_t index);

	// event handlers - Users page
	void		  OnButtonUserPageClearControls(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonUserPageAddUser(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonUserPageRemoveUser(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonUserPageEditUser(wxCommandEvent& WXUNUSED(event));
	void		  OnSelchangeUsersList(wxCommandEvent& WXUNUSED(event));
	void		  OnCheckboxUseradmin(wxCommandEvent& WXUNUSED(event));
	void		  OnCheckboxKbadmin(wxCommandEvent& WXUNUSED(event));

	// Functions needed by the Kb Definitions page
	KbServerKb*	  CloneACopyOfKbServerKbStruct(KbServerKb* pExistingStruct);
	void		  CopyKbsList(KbsList* pSrcList, KbsList* pDestList);
	void		  DeleteClonedKbServerKbStruct();
	bool		  IsThisKBDefinitionInSessionList(KbServerKb* pKbDefToTest, KbsList* pKbsList);
	void		  LoadLanguageCodePairsInListBox_KbsPage(bool bKBTypeIsSrcTgt,
							KbsList* pSrcTgtKbsList, KbsList* pSrcGlsKbsList,
							wxListBox* pListBox);
	bool		  MatchExistingKBDefinition(wxListBox* pKbsList,
							wxString& srcLangCodeStr, wxString& nonsrcLangCodeStr);
	void		  SeparateKbServerKbStructsByType(KbsList* pAllKbStructsList,
							KbsList* pKbStructs_TgtList, KbsList* pKbStructs_GlsList);
	bool		  IsAKbDefinitionAltered(wxArrayString* pBeforeArr, wxArrayString* pAfterArr);

	// event handlers - Create KB Definitions page
	void		  OnRadioButton1KbsPageType1(wxCommandEvent& WXUNUSED(event));
	void		  OnRadioButton2KbsPageType2(wxCommandEvent& WXUNUSED(event));
	void		  OnBtnKbsPageLookupCodes(wxCommandEvent& WXUNUSED(event));
	void		  OnBtnKbsPageRFC5646Codes(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonKbsPageClearListSelection(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonKbsPageClearBoxes(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonKbsPageAddKBDefinition(wxCommandEvent& WXUNUSED(event));
	void		  OnSelchangeKBsList(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonKbsPageRemoveKb(wxCommandEvent& WXUNUSED(event));

	// Functions needed by the Create or Delete Custom Codes (3rd) page
	void          LoadLanguagesListBox(wxListBox* pListBox, LanguagesList* pLanguagesList);
	KbServerLanguage* GetThisLanguageStructPtr(wxString& customCode, LanguagesList* pLanguagesList);

	// event handlers - Create or Delete Custom Codes page
	void		  OnBtnLanguagesPageLookupCode(wxCommandEvent& WXUNUSED(event));
	void		  OnBtnLanguagesPageRFC5646Codes(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonLanguagesPageClearBoxes(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonLanguagesPageClearListSelection(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonLanguagesPageCreateCustomCode(wxCommandEvent& WXUNUSED(event));
	void		  OnButtonLanguagesPageDeleteCustomCode(wxCommandEvent& WXUNUSED(event));
	void		  OnSelchangeLanguagesList(wxCommandEvent& WXUNUSED(event));

private:
	// All the lists, users, kbs and custom language definitions, are SORTED.
	CAdapt_ItApp*     m_pApp;
	int				  m_nSel; // index value (0 based) for selection in the the listbox of one
							  // of the pages, and has value wxNOT_FOUND when nothing is selected
	wxString		  m_earliestUseradmin; // this person cannot be deleted or demoted
	UsersList*        m_pUsersList; // initialize in InitDialog() as the KbServer instance has the list
	UsersList*        m_pOriginalUsersList; // store copies of KbServerUser structs at
									        // entry, for comparison with final list
											// after the edits, removals and additions
											// are done
	KbsList*		  m_pKbsList; // initialize in InitDialog() as the KbServer instance has the list
	KbsList*		  m_pOriginalKbsList; // store copies of KbServerKb structs at
										  // entry, for comparison with final list after
										  // edits or additions are done
	KbsList*		  m_pKbsAddedInSession; // store KbKbserverKb struct ptrs for those added
	// NOTE: the next two are required so we can separate out the Type1 KBs (adaptation
	// ones) from the Type2 KBs (glossing ones) into separate lists - and when we populate
	// these lists, we'll do so with deep copies of the structs, so that we can call
	// ClearKbsList() on these as we do on the m_pOriginalKbsList and m_pKbsList
	KbsList*		  m_pKbsList_Tgt;
	KbsList*		  m_pKbsList_Gls;

	CodesList*		  m_pCustomCodesList; // initialize in InitDialog() as the KbServer instance has the list
	CodesList*		  m_pOriginalCustomCodesList; // store copies of KbServerLanguage structs at entry,
										// for comparison with final list after edits or additions are done
	KbServerUser*     m_pUserStruct; // scratch variable to get at returned values
								     // for a user entry's fields
	KbServerUser*     m_pOriginalUserStruct; // scratch variable to get at returned values
								     // for a user entry's fields, this one stores the
								     // struct immediately after the user's click on the
									 // user item in the listbox, freeing up the
									 // m_pUserStruct to be given values as edited by
									 // the user
	KbServerKb*		  m_pKbStruct; // scratch variable to hold returned values for
								   // a kb entry's pair of language codes, etc
	KbServerKb*		  m_pOriginalKbStruct; // performs the same service for m_pKbStruct that
									 // m_pOriginalUserStruct does for m_pUserStruct

	KbServerLanguage* m_pLanguageStruct; // scratch variable to hold a returned value for
										 // a custom code etc
	KbServerLanguage* m_pOriginalLanguageStruct; // performs the same service for m_pLanguageStruct
										 // that m_pOriginalUserStruct does for m_pUserStruct
	LanguagesList*	  m_pLanguagesList;
	FilteredList*	  m_pFilteredList;

	// Next members are additional ones needed for the kbs page
	bool		m_bKBisType1; // TRUE for adaptations KB definition, FALSE for a glosses KB definition
	wxString	m_tgtLanguageCodeLabel; // InitDialog() sets it to "Target language code:"
	wxString	m_glossesLanguageCodeLabel; // InitDialog() sets it to "Glossing language code:"
	wxString	m_tgtListLabel; // InitDialog() sets it to
					// "Existing shared databases (as   source,target   comma delimited code pairs):"
	wxString	m_glsListLabel; // InitDialog() sets it to
					// "Existing shared databases (as   source,glossing   comma delimited code pairs):"
	wxString	m_sourceLangCode;
	wxString	m_targetLangCode;
	wxString	m_glossLangCode;
	// Next members are additional ones needed for the custom language codes page
	wxString	m_customLangCode;
	wxString	m_description;
	wxString	m_creator;


	// Support for showing informative message when user attempts to alter one or both
	// language codes for a KB definition which is parent to entries in the entry table -
	// the attmept will fail without giving any feedback to the administrator unless we
	// compare the before and after values for the codes being changed, and give the
	// helpful message when they have not changed as expected.
	bool			m_bUpdateTried;
	wxArrayString	m_listBeforeUpdate;
	wxArrayString	m_listAfterUpdate;

	// Support for prevention of the Manager user trying to remove a kb definition for the
	// project which is currently the active project and it is set up to be sharing to the
	// same remote kb and kbtype! (Clearly, to allow entries to flow in while entries are
	// being actively removed would be crazy - so we check for this and advise the Manager
	// user to first remove the sharing setup with the kb definition which is to be removed.
	// (In the app class there are also:
	// 	wxString		m_srcLangCodeOfCurrentRemoval;  and
	//  wxString		m_nonsrcLangCodeOfCurrentRemoval;
	//  which preserve the code values which a deletion is in progress, so we can ensure
	//  that nonone sets up a sharing to the old kb definition while the removal is in
	//  progress.)
	wxString		m_srcLangCodeOfDeletion;
	wxString		m_nonsrcLangCodeOfDeletion;
	int				m_kbTypeOfDeletion; //1, 2, or undefined (-1)

	DECLARE_EVENT_TABLE()
};
#endif
#endif /* KBSharingMgrTabbedDlg_h */
