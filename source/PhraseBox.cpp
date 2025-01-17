/////////////////////////////////////////////////////////////////////////////
/// \project		adaptit
/// \file			PhraseBox.cpp
/// \author			Bill Martin
/// \date_created	11 February 2004
/// \rcs_id $Id$
/// \copyright		2008 Bruce Waters, Bill Martin, SIL International
/// \license		The Common Public License or The GNU Lesser General Public License (see license directory)
/// \description	This is the implementation file for the CPhraseBox class.
/// The CPhraseBox class governs the behavior of the phrase or
/// target box where the user enters and/or edits translations while adapting text.
/// \derivation		The PhraseBox class derives from the wxTextCtrl class.
/////////////////////////////////////////////////////////////////////////////

// the following improves GCC compilation performance
#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma implementation "PhraseBox.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include <wx/wxprec.h>

#ifdef __BORLANDC__
#pragma hdrstop
#endif

// defines for debugging purposes
//#define _FIND_DELAY
//#define _AUTO_INS_BUG
//#define LOOKUP_FEEDBACK


#ifndef WX_PRECOMP
// Include your minimal set of headers here, or wx.h
#include <wx/wx.h>
#endif

#include <wx/textctrl.h>

// Other includes uncomment as implemented
#include "Adapt_It.h"
#include "PhraseBox.h"
#include "Adapt_ItView.h"
#include "Adapt_ItCanvas.h"
#include "Cell.h"
#include "Pile.h"
#include "Strip.h"
#include "SourcePhrase.h"
#include "Adapt_ItDoc.h"
//#include "SourceBundle.h"
#include "Layout.h"
#include "RefString.h"
#include "AdaptitConstants.h"
#include "KB.h"
#include "TargetUnit.h"
#include "ChooseTranslation.h"
#include "MainFrm.h"
#include "Placeholder.h"
#include "helpers.h"
// Other includes uncomment as implemented

// globals

/// This global is defined in MainFrm.cpp.
extern bool gbIgnoreScriptureReference_Send;

extern wxString gOldChapVerseStr;

extern bool gbVerticalEditInProgress;
extern EditStep gEditStep;
extern EditRecord gEditRecord;
extern CAdapt_ItApp* gpApp; // to access it fast

bool gbTunnellingOut = FALSE; // TRUE when control needs to tunnel out of nested procedures when
							  // gbVerticalEditInProgress is TRUE and a step-changing custom message
							  // has been posted in order to transition to a different edit step;
							  // FALSE (default) in all other circumstances

bool gbSavedTargetStringWithPunctInReviewingMode = FALSE; // TRUE if either or both of m_adaption and m_targetStr is empty
											 // and Reviewing mode is one (we want to preserve punctuation or
											 // lack thereof if the location is a hole)
wxString gStrSavedTargetStringWithPunctInReviewingMode; // works with the above flag, and stores whatever the m_targetStr was
										 // when the phrase box, in Reviewing mode, lands on a hole (we want to
										 // preserve what we found if the user has not changed it)

bool gbNoAdaptationRemovalRequested = FALSE; // TRUE when user hits backspace or DEL key to try remove an earlier
											 // assignment of <no adaptation> to the word or phrase at the active
											 // location - (affects one of m_bHasKBEntry or m_bHasGlossingKBEntry
											 // depending on the current mode, and removes the KB CRefString (if
											 // the reference count is 1) or decrements the count, as the case may be)

/// Used to delay the message that user has come to the end, until after last
/// adaptation has been made visible in the main window; in OnePass() only, not JumpForward().
bool gbCameToEnd = FALSE;

bool gTemporarilySuspendAltBKSP = FALSE; // to enable gbSuppressStoreForAltBackspaceKeypress flag to be turned
										 // back on when <Not In KB> next encountered after being off for one
										 // or more ordinary KB entry insertions; CTRL+ENTER also gives same result

/// To support the ALT+Backspace key combination for advance to immediate next pile without lookup or
/// store of the phrase box (copied, and perhaps SILConverters converted) text string in the KB or
/// Glossing KB. When ALT+Backpace is done, this is temporarily set TRUE and restored to FALSE
/// immediately after the store is skipped. CTRL+ENTER also can be used for the transliteration.
bool gbSuppressStoreForAltBackspaceKeypress = FALSE;

bool gbMovingToPreviousPile = FALSE; // added for when user calls MoveToPrevPile( ) and the
	// previous pile contains a merged phrase with internal punctuation - we don't want the
	// ReDoPhraseBox( ) call to call MakeTargetStringIncludingPunctuation( ) and so result in the PlaceMedialPunctuation
	// dialog being put up an unwanted couple of times. So we'll use the gbMovingToPreviousPile being
	// set to then set the gbInhibitMakeTargetStringCall to TRUE, at start of ReDoPhraseBox( ), and turn it off at
	// the end of that function. That should fix it.


/// This global is defined in Adapt_ItView.cpp.
extern bool gbInhibitMakeTargetStringCall; // see view for reason for this

/// This global is defined in Adapt_ItView.cpp.
extern bool gbInhibitMakeTargetStringCall; // see view for reason for this

// for support of auto-capitalization

/// This global is defined in Adapt_It.cpp.
extern bool	gbAutoCaps;

/// This global is defined in Adapt_It.cpp.
extern bool	gbSourceIsUpperCase;

/// This global is defined in Adapt_It.cpp.
extern bool	gbNonSourceIsUpperCase;

/// This global is defined in Adapt_It.cpp.
extern bool	gbMatchedKB_UCentry;

/// This global is defined in Adapt_It.cpp.
extern bool	gbNoSourceCaseEquivalents;

/// This global is defined in Adapt_It.cpp.
extern bool	gbNoTargetCaseEquivalents;

/// This global is defined in Adapt_It.cpp.
extern bool	gbNoGlossCaseEquivalents;

/// This global is defined in Adapt_It.cpp.
extern wxChar gcharNonSrcLC;

/// This global is defined in Adapt_It.cpp.
extern wxChar gcharNonSrcUC;

/// This global is defined in Adapt_It.cpp.
extern wxChar gcharSrcLC;

/// This global is defined in Adapt_It.cpp.
extern wxChar gcharSrcUC;

// next two are for version 2.0 which includes the option of a 3rd line for glossing

/// This global is defined in Adapt_ItView.cpp.
extern bool	gbIsGlossing; // when TRUE, the phrase box and its line have glossing text

/// This global is defined in Adapt_ItView.cpp.
extern bool gbGlossingUsesNavFont;

bool	gbRetainBoxContents = FALSE; // for version 1.4.2; we want deselection of copied source
		// text to set this flag true so that if the user subsequently selects words intending
		// to do a merge, then the deselected word won't get lost when he types something after
		// forming the source words selection (see OnKeyUp( ) for one place the flag is set -
		// (for a left or right arrow keypress), and the other place will be in the view's
		// OnLButtonDown I think - for a click on the phrase box itself)

/// This global is defined in Adapt_It.cpp.
//extern CAdapt_ItApp* gpApp; // if we want to access it fast, BEW removed 2Apr09

/// This global is defined in Adapt_It.cpp.
extern bool	gbRTL_Layout;	// ANSI version is always left to right reading; this flag can only
							// be changed in the NRoman version, using the extra Layout menu

bool			gbByCopyOnly = FALSE; // will be set TRUE when the target text is the result of a
					// copy operation from the source, and if user types to modify it, it is
					// cleared to FALSE, and similarly, if a lookup succeeds, it is cleared to
					// FALSE. It is used in PlacePhraseBox() to enforce a store to the KB when
					// user clicks elsewhere after an existing location is returned to somehow,
					// and the user did not type anything to the target text retrieved from the
					// KB. In this circumstance the m_bAbandonable flag is TRUE, and the retrieved
					// target text would not be re-stored unless we have this extra flag
					// gbByCopyOnly to check, and when FALSE we enforce the store operation

/// This global is defined in Adapt_ItView.cpp.
extern short	gnExpandBox;  // see start of Adapt_ItView.cpp for explanation of these two

/// This global is defined in Adapt_ItView.cpp.
extern short	gnNearEndFactor;

bool gbUnmergeJustDone = FALSE; // this is used to inhibit a second unmerge, when OnButtonRestore()
					// is called from other than in MoveToNextPile() (eg. a click on the Unmerge
					// A Phrase button); we want to allow the unmerge in OnButtonRestore() (which
					// will be the first unmerge), and suppress the second that could otherwise be
					// asked for if the user Cancels the Choose Translation dialog from within the
					// ChooseTranslation() call within the LookUpSrcWord() call within
					// OnButtonRestore(). This is the opposite situatio than for gbSuppressLookup
					// flag's use (the latter suppresses first unmerge but allows second)
bool gbSuppressMergeInMoveToNextPile = FALSE; // if a merge is done in LookAhead() so that the
					// phrase box can be shown at the correct location when the Choose Translation
					// dialog has to be put up because of non-unique translations, then on return
					// to MoveToNextPile() with an adaptation chosen in the dialog dialog will
					// come to code for merging (in the case when no dialog was needed), and if
					// not suppressed by this flag, a merge of an extra word or words is wrongly
					// done
bool gbSuppressLookup = FALSE; // used to suppress the LookUpSrcWord() call in view's
							   // OnButtonRestore() function when unmerging a merged phrase due
							   // to Cancel or Cancel And Select being chosen in the Choose
							   // Translation dialog
bool gbCompletedMergeAndMove = FALSE; // for support of Bill Martin's wish that the phrase box
						// be at the new location when the Choose Translation dialog is shown
bool gbEnterTyped = FALSE; // used in BuildPhrases() to speed up finding the current srcPhrase

bool gbMergeDone = FALSE;
bool gbUserCancelledChooseTranslationDlg = FALSE;
//bool gbUserWantsNoMove = FALSE;  BEW removed 5May09 because it is never set TRUE, so unneeded
    // TRUE if user wants an empty adaptation to not move because some text must be
    // supplied for the adaptation first; used in MoveToImmedNextPile() and the TAB case
    // block of code in OnChar() to suppress warning message

// Cursor location - needed for up & down arrow & page up & down arrows, since its
// already wrong by the time the handlers are invoked, so it needs to be set by OnChar().
//long gnEnd; // BEW removed 3Jul09

long gnSaveStart; //int gnSaveStart; // these two are for implementing Undo() for a backspace operation
long gnSaveEnd; //int gnSaveEnd;

//GDLC Removed definitions of gbExpanding & gbContracting 2010-02-09
//bool		gbExpanding = FALSE; // set TRUE when an expansion of phrase box was just done
					// (and used in view's CalcPileWidth to enable an extra pileWidth adjustment
					// and therefore to disable this adjustment when the phrase box is contracting
					// due to deleting some content - otherwise it won't contract)
				// GDLC The above comment appears to mean:
				// Set TRUE when an expansion of the phrase box was just done.
				// In this case the view's CalcPileWidth will add some extra pileWidth adjustment.
				// In all other cases no extra pileWidth adjustment will be done (and this will allow the
				// phrase box to contract when necessary).
//bool		gbContracting = FALSE; // BEW added 25Jun09, set to TRUE when a backspace
					// keypress has reduced the length of the phrase box's string to the
					// point where a reduction in size is required. We need this in
					// RecalcLayout() so that the ResetPartnerPileWidth() call at the
					// active pile, when contraction is needed, does not override the
					// contraction value already given to m_curBoxWidth with a larger
					// calculation (gbContracting is set TRUE only in FixBox(), cleared
					// at the end of RecalcLayout())

/// Contains the current sequence number of the active pile (m_nActiveSequNum) for use by auto-saving.
int nCurrentSequNum;

/// A global wxString containing the translation for a matched source phrase key.
wxString		translation = _T(""); // = _T("") whm added 8Aug04 // translation, for a matched source phrase key

CTargetUnit*	pCurTargetUnit = (CTargetUnit*)NULL; // when valid, it is the matched CTargetUnit instance
wxString		curKey = _T(""); // when non empty, it is the current key string which was matched
int				nWordsInPhrase = 0; // a matched phrase's number of words (from source phrase)
extern bool		bSuppressDefaultAdaptation; // normally FALSE, but set TRUE whenever user is
					// wanting a MergeWords done by typing into the phrase box (which also
				    // ensures cons.changes won't be done on the typing)
extern bool		gbInspectTranslations; // when TRUE suppresses the "Cancel and Select" button
					// in the CChooseTranslation dialog
/// This global is defined in Adapt_ItView.cpp.
extern int		gnOldSequNum;

extern bool		gbMergeSucceeded;
wxString		gSaveTargetPhrase = _T(""); // used by the SHIFT+END shortcut for unmerging
											// a phrase
/// This global is defined in Adapt_ItView.cpp.
extern bool gbLegacySourceTextCopy;	// default is legacy behaviour, to copy the source text (unless
									// the project config file establishes the FALSE value instead)



IMPLEMENT_DYNAMIC_CLASS(CPhraseBox, wxTextCtrl)

BEGIN_EVENT_TABLE(CPhraseBox, wxTextCtrl)
	EVT_MENU(wxID_UNDO, CPhraseBox::OnEditUndo)
	EVT_TEXT(-1, CPhraseBox::OnPhraseBoxChanged)
	EVT_CHAR(CPhraseBox::OnChar)
	EVT_KEY_DOWN(CPhraseBox::OnKeyDown)
	EVT_KEY_UP(CPhraseBox::OnKeyUp)
	EVT_LEFT_DOWN(CPhraseBox::OnLButtonDown)
	EVT_LEFT_UP(CPhraseBox::OnLButtonUp)
END_EVENT_TABLE()

CPhraseBox::CPhraseBox(void)
{
	// Problem: The MFC version destroys and recreates the phrasebox every time
	// the box is moved, layout changes, screen is redrawn, etc. In fact, it seems
	// often to be the case that the phrase box contents can remain unchanged, and
	// yet the phrase box itself can go through multiple deletions, and recreations.
	// The MFC design makes it impossible to keep track of a phrase box "dirty" flag
	// from here within the CPhraseBox class. It seems I could either keep a "dirty"
	// flag on the App, or else redesign the TargetBox/PhraseBox in such a way that
	// it doesn't need to be destroyed and recreated all the time, but can exist
	// at least for the life of a view (on the heap), and be hidden, moved, and
	// shown when needed. I've chosen the latter.

	m_textColor = wxColour(0,0,0); // default to black
	m_bMergeWasDone = FALSE;
	m_bCancelAndSelectButtonPressed = FALSE;
}

CPhraseBox::~CPhraseBox(void)
{
}

// returns number of phrases built; a return value of zero means that we have a halt condition
// and so none could be built (eg. source phrase is a merged one would halt operation)
// When glossing is ON, the build is constrained to phrases[0] only, and return value would then
// be 1 always.
// BEW 13Apr10, no changes needed for support of doc version 5
// BEW 23Apr15, changed to support / as a word-breaking whitespace char, if m_bFwdSlashDelimiter is TRUE
// because Lookups will, for mergers, want to test with ZWSP in strings of two or more words, since we
// store in the kb with / replaced by ZWSP for mergers, and show strings with ZWSP in the interlinear
// layout when the source and or target strings have 2 or more words
int CPhraseBox::BuildPhrases(wxString phrases[10], int nNewSequNum, SPList* pSourcePhrases)
{
	// refactored 25Mar09, -- nothing needs to be done
	// clear the phrases array
	phrases[0] = phrases[1] = phrases[2] = phrases[3] = phrases[4] = phrases[5] = phrases[6]
		= phrases[7] = phrases[8] = phrases[9] = _T("");

	// check we are within bounds
	int nMaxIndex = pSourcePhrases->GetCount() - 1;
	if (nNewSequNum > nMaxIndex)
	{
		// this is unlikely to ever happen, but play safe just in case
		wxMessageBox(_T("Index bounds error in BuildPhrases call\n"), _T(""), wxICON_EXCLAMATION | wxOK);
		wxExit();
	}

	// find position of the active pile's source phrase in the list
	SPList::Node *pos;

	pos = pSourcePhrases->Item(nNewSequNum);

	wxASSERT(pos != NULL);
	int index = 0;
	int counter = 0;
	CSourcePhrase* pSrcPhrase;

	// build the phrases array, breaking if a build halt condition is encounted
	// (These are: if we reach a boundary, or a source phrase with an adaption already, or
	// a change of TextType, or a null source phrase or a retranslation, or EOF, or max
	// number of words is reached, or we reached a source phrase which has been previously
	// merged.) MAX_WORDS is defined in AdaptitConstants.h (TextType changes can be ignored
	// here because they coincide with m_bBoundary == TRUE on the previous source phrase.)
	// When glossing is ON, there are no conditions for halting, because a pile will already
	// be active, and the src word at that location will be glossable no matter what.
	if (gbIsGlossing)
	{
		// BEW 6Aug13, I previously missed altering this block when I refactored in order
		// to have glossing KB use all ten tabs. So instead of putting the key into
		// phrases[0] as before, it now must be put into whichever one of the array
		// pertains to how many words, less one, are indicated by m_nSourceWords, and
		// counter needs to be set to the latter's value rather than to 1 as used to be
		// the case
		pSrcPhrase = (CSourcePhrase*)pos->GetData();
		int theIndex = pSrcPhrase->m_nSrcWords - 1;
		phrases[theIndex] = pSrcPhrase->m_key;
		return counter = pSrcPhrase->m_nSrcWords;
	}
	while (pos != NULL && index < MAX_WORDS)
	{
		// NOTE: MFC's GetNext(pos) retrieves the current pos data into
		// pScrPhrase, then increments pos
		pSrcPhrase = (CSourcePhrase*)pos->GetData();
		pos = pos->GetNext();
		if (pSrcPhrase->m_nSrcWords > 1 || !pSrcPhrase->m_adaption.IsEmpty() ||
			pSrcPhrase->m_bNullSourcePhrase || pSrcPhrase->m_bRetranslation)
			return counter; // don't build with this src phrase, it's a merged one, etc.

		if (index == 0)
		{
			phrases[0] = pSrcPhrase->m_key;
			counter++;
			if (pSrcPhrase->m_bBoundary || nNewSequNum + counter > nMaxIndex)
				break;
		}
		else
		{
			phrases[index] = phrases[index - 1] + PutSrcWordBreak(pSrcPhrase) + pSrcPhrase->m_key;
//#if defined(FWD_SLASH_DELIM)
			CAdapt_ItApp* pApp = (CAdapt_ItApp*)&wxGetApp();
			if (pApp->m_bFwdSlashDelimiter)
			{
				// BEW 23Apr15 if in a merger, we want / converted to ZWSP for the source text
				// to support lookups because we will have ZWSP rather than / in the KB
				// No changes are made if app->m_bFwdSlashDelimiter is FALSE
				phrases[index] = FwdSlashtoZWSP(phrases[index]);
			}
//#endif
			counter++;
			if (pSrcPhrase->m_bBoundary || nNewSequNum + counter > nMaxIndex)
				break;
		}
		index++;
	}
	return counter;
}

CLayout* CPhraseBox::GetLayout()
{
	CAdapt_ItApp* pApp = (CAdapt_ItApp*)&wxGetApp();
	return pApp->m_pLayout;
}

// returns TRUE if the phrase box, when placed at pNextEmptyPile, would not be within a
// retranslation, or FALSE if it is within a retranslation
// Side effects:
// (1) checks for vertical edit being current, and whether or not a vertical edit step
// transitioning event has just been posted (that would be the case if the phrase box at
// the new location would be in grayed-out text), and if so, returns FALSE after setting
// the global bool gbTunnellingOut to TRUE - so that MoveToNextPile() can be exited early
// and the vertical edit step's transition take place instead
// (2) for non-vertical edit mode, if the new location would be within a retranslation, it
// shows an informative message to the user, enables the button for copying punctuation,
// and returns FALSE
// (3) if within a retranslation, the global bool gbEnterTyped is cleared to FALSE
// BEW 13Apr10, no changes needed for support of doc version 5
// BEW 9Apr12, changed to support discontinuous hightlight spans for auto-inserts
bool CPhraseBox::CheckPhraseBoxDoesNotLandWithinRetranslation(CAdapt_ItView* pView,
												CPile* pNextEmptyPile, CPile* pCurPile)
{
	// created for refactored view layout, 24Mar09
	wxASSERT(pNextEmptyPile);
	if (gbIsGlossing)
		return TRUE; // allow phrase box to land in a retranslation when glossing mode is ON

	// the code below will only be entered when glossing mode is OFF, that is, in adapting mode
	// BEW 9Apr12,deprecated the check and clearing of the highlighting, discontinuity in the
	// highlighted spans of auto-inserted material is now supported
	pCurPile = pCurPile; // avoid compiler warning

	if (pNextEmptyPile->GetSrcPhrase()->m_bRetranslation)
	{
		// if the lookup and jump loop comes to an empty pile which is in a retranslation,
		// we halt the loop there. If vertical editing is in progress, this halt location
		// could be either within or beyond the edit span, in which case the former means
		// we don't do any step transition yet, the latter means a step transition is
		// called for. Test for these situations and act accordingly. If we transition
		// the step, there is no point in showing the user the message below because we
		// just want transition and where the jump-landing location might be is of no interest
		if (gbVerticalEditInProgress)
		{
			// bForceTransition is FALSE in the next call
			gbTunnellingOut = FALSE; // ensure default value set
			int nSequNum = pNextEmptyPile->GetSrcPhrase()->m_nSequNumber;
			bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(nSequNum,nextStep);
			if (bCommandPosted)
			{
				// don't proceed further because the current vertical edit step has ended
				gbTunnellingOut = TRUE; // caller needs to use it
				return FALSE; // use FALSE to help caller recognise need to tunnel out of the lookup loop
			}
		}
		// IDS_NO_ACCESS_TO_RETRANS
		wxMessageBox(_(
"Sorry, to edit or remove a retranslation you must use the toolbar buttons for those operations."),
						_T(""), wxICON_INFORMATION | wxOK);
		GetLayout()->m_pApp->m_pTargetBox->SetFocus();
		gbEnterTyped = FALSE;
		// if necessary restore default button image, and m_bCopySourcePunctuation to TRUE
		wxCommandEvent event;
		if (!GetLayout()->m_pApp->m_bCopySourcePunctuation)
		{
			pView->OnToggleEnablePunctuationCopy(event);
		}
		return FALSE;
	}
	if (gbVerticalEditInProgress)
	{
		// BEW 19Oct15 No transition of vert edit modes, and not landing in a retranslation,
		// so we can store this location on the app
		gpApp->m_vertEdit_LastActiveSequNum = pNextEmptyPile->GetSrcPhrase()->m_nSequNumber;
#if defined(_DEBUG)
		wxLogDebug(_T("VertEdit PhrBox, CheckPhraseBoxDoesNotLandWithinRetranslation() storing loc'n: %d "),
			pNextEmptyPile->GetSrcPhrase()->m_nSequNumber);
#endif
	}
	return TRUE;
}

// returns nothing
// this is a helper function to do some housecleaning tasks prior to the caller (which is
// a pile movement function such as MoveToNextPile(), returning FALSE to its caller
// BEW 13Apr10, no changes needed for support of doc version 5
// BEW 21Jun10, no changes needed for support of kbVersion 2
void CPhraseBox::DealWithUnsuccessfulStore(CAdapt_ItApp* pApp, CAdapt_ItView* pView,
										   CPile* pNextEmptyPile)
{
	if (!pApp->m_bSingleStep)
	{
		gbEnterTyped = FALSE;
		pApp->m_bAutoInsert = FALSE; // cause halt, if auto lookup & inserting is ON
	}
	gbEnterTyped = FALSE;
	// if necessary restore default button image, and m_bCopySourcePunctuation to TRUE
	wxCommandEvent event;
	if (!pApp->m_bCopySourcePunctuation)
	{
		pView->OnToggleEnablePunctuationCopy(event);
	}
	if (gbSuppressStoreForAltBackspaceKeypress)
		gSaveTargetPhrase.Empty();
	gTemporarilySuspendAltBKSP = FALSE;
	gbSuppressStoreForAltBackspaceKeypress = FALSE; // make sure it's off before returning

    // if vertical editing is in progress, the store failure may occur with the active
    // location within the editable span, (in which case we don't want a step transition),
    // or having determined the jump location's pile is either NULL (a bundle boundary was
    // reached before an empty pile could be located - in which case a step transition
    // should be forced), or a pile located which is beyond the editable span, in the gray
    // area, in which case transition is wanted; so handle these options using the value
    // for pNextEmptyPile obtained above Note: doing a transition in this circumstance
    // means the KB does not get the phrase box contents added, but the document still has
    // the adaptation or gloss, so the impact of the failure to store is minimal (ie. if
    // the box contents were unique, the adaptation or gloss will need to occur later
    // somewhere for it to make its way into the KB)
	if (gbVerticalEditInProgress)
	{
		// bForceTransition is TRUE in the next call
		gbTunnellingOut = FALSE; // ensure default value set
		bool bCommandPosted = FALSE;
		if (pNextEmptyPile == NULL)
		{
			bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(-1,nextStep,TRUE);
		}
		else
		{
			// bForceTransition is FALSE in the next call
			int nSequNum = pNextEmptyPile->GetSrcPhrase()->m_nSequNumber;
			bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(nSequNum,nextStep);
		}
		if (bCommandPosted)
		{
			// don't proceed further because the current vertical edit step has ended
			gbTunnellingOut = TRUE; // caller needs to use it
			// caller unilaterally returns FALSE  when this function returns,
			// this, together with gbTunnellingOut,  enables the caller of the caller to
			// recognise the need to tunnel out of the lookup loop
		}
		else
		{
			// BEW 19Oct15 No transition of vert edit modes,
			// so we can store this location on the app
			gpApp->m_vertEdit_LastActiveSequNum = pNextEmptyPile->GetSrcPhrase()->m_nSequNumber;
#if defined(_DEBUG)
		wxLogDebug(_T("VertEdit PhrBox, DealWithUnsuccessfulStore() storing loc'n: %d "),
			pNextEmptyPile->GetSrcPhrase()->m_nSequNumber);
#endif
		}
	}
}

// return TRUE if there were no problems encountered with the store, FALSE if there were
// (this function calls DealWithUnsuccessfulStore() if there was a problem with the store)
// BEW 13Apr10, no changes needed for support of doc version 5
// BEW 21Jun10, no changes needed for support of kbVersion 2
bool CPhraseBox::DoStore_NormalOrTransliterateModes(CAdapt_ItApp* pApp, CAdapt_ItDoc* pDoc,
		 CAdapt_ItView* pView, CPile* pCurPile, bool bIsTransliterateMode)
{
	bool bOK = TRUE;
	CSourcePhrase* pOldActiveSrcPhrase = pCurPile->GetSrcPhrase();

	// gbSuppressStoreForAltBackspaceKeypress is FALSE, so either we are in normal adapting
	// or glossing mode; or we could be in transliteration mode but the global boolean
	// happened to be FALSE because the user has just done a normal save in transliteration
	// mode because the transliterator did not produce a correct transliteration

	// we are about to leave the current phrase box location, so we must try to store what is
	// now in the box, if the relevant flags allow it. Test to determine which KB to store to.
	// StoreText( ) has been ammended for auto-capitalization support (July 2003)
	if (!gbIsGlossing)
	{
		pView->MakeTargetStringIncludingPunctuation(pOldActiveSrcPhrase, pApp->m_targetPhrase);
		pView->RemovePunctuation(pDoc,&pApp->m_targetPhrase,from_target_text);
	}
	if (gbIsGlossing)
	{
		// BEW added next line 27Jan09
		bOK = pApp->m_pGlossingKB->StoreText(pOldActiveSrcPhrase, pApp->m_targetPhrase);
	}
	else
	{
		// BEW added next line 27Jan09
		bOK = pApp->m_pKB->StoreText(pOldActiveSrcPhrase, pApp->m_targetPhrase);
	}

    // if in Transliteration Mode we want to cause gbSuppressStoreForAltBackspaceKeypress
    // be immediately turned back on, in case a <Not In KB> entry is at the next lookup
    // location and we will then want the special Transliteration Mode KB storage process
    // to be done rather than a normal empty phrasebox for such an entry
	if (bIsTransliterateMode)
	{
		gbSuppressStoreForAltBackspaceKeypress = TRUE;
	}
	return bOK;
}

// BEW 27Mar10 a change needed for support of doc version 5
void CPhraseBox::MakeCopyOrSetNothing(CAdapt_ItApp* pApp, CAdapt_ItView* pView,
									  CPile* pNewPile, bool& bWantSelect)
{
    // BEW 14Apr10, pass back bWantSelect as TRUE, because this will be used in caller to
    // pass the TRUE value to MoveToNextPile() which uses it to set the app members
    // m_nStartChar, m_nEndChar, to -1 and -1, and then when CLayout::PlaceBox()is called,
    // it internally calls ResizeBox() and the latter is what uses the m_nStartChar and
    // m_nEndChar values to set the phrase box content's selection. BEW 14Apr10
	bWantSelect = TRUE;

	// don't clear the private m_bCancelAndSelectButtonPressed flag here, otherwise when
	// the caller returns to it's caller and control advances to the delayed block for
	// testing the flag in order to call DoCancelAndSelect(), the function would be skipped
	if (pApp->m_bCopySource)
	{
		if (!pNewPile->GetSrcPhrase()->m_bNullSourcePhrase)
		{
			pApp->m_targetPhrase = pView->CopySourceKey(pNewPile->GetSrcPhrase(),
									pApp->m_bUseConsistentChanges);
		}
		else
		{
            // its a null source phrase, so we can't copy anything; and if we are glossing,
            // we just leave these empty whenever we meet them
			pApp->m_targetPhrase.Empty(); // this will cause pile's m_nMinWidth to be
										   // used for box width
		}
	}
	else
	{
		// no copy of source wanted, so just make it an empty string
		pApp->m_targetPhrase.Empty();
	}
}

// BEW 13Apr10, changes needed for support of doc version 5
void CPhraseBox::HandleUnsuccessfulLookup_InSingleStepMode_AsBestWeCan(CAdapt_ItApp* pApp,
	CAdapt_ItView* pView, CPile* pNewPile, bool m_bCancelAndSelect, bool& bWantSelect)
{
	pApp->m_pTargetBox->m_bAbandonable = TRUE;

	// it is single step mode & no adaptation available, so see if we can find a
	// translation, or gloss, for the single src word at the active location, if not,
	// depending on the m_bCopySource flag, either initialize the targetPhrase to
	// an empty string, or to a copy of the sourcePhrase's key string
	bool bGotTranslation = FALSE;
	if (!gbIsGlossing && m_bCancelAndSelect)
	{
		// in ChooseTranslation dialog the user wants the 'cancel and select'
		// option, and since no adaptation is therefore to be retrieved, it
		// remains just to either copy the source word or nothing...
		MakeCopyOrSetNothing(pApp, pView, pNewPile, bWantSelect);

		// BEW added 1Jul09, the flag should be TRUE if nothing was found
		pApp->m_pTargetBox->m_bAbandonable = TRUE;

	}
	else
	{
		// user didn't press the Cancel and Select button in the Choose Translation
		// dialog, but he may have pressed Cancel button, or OK button - so try to
		// find a translation given these possibilities (note, nWordsInPhrase equal
		// to 1 does not distinguish between adapting or glossing modes, so handle that in
		// the next block too)
		if (!gbUserCancelledChooseTranslationDlg || nWordsInPhrase == 1)
		{
            // try find a translation for the single word (from July 2003 this supports
            // auto capitalization) LookUpSrcWord() calls RecalcLayout()
			bGotTranslation = LookUpSrcWord(pNewPile);
		}
		else
		{
			// the user cancelled the ChooseTranslation dialog
			gbUserCancelledChooseTranslationDlg = FALSE; // restore default value

			// if the user cancelled the Choose Translation dialog when a phrase was
			// merged, then he will probably want a lookup done for the first word of
			// the now unmerged phrase; nWordsInPhrase will still contain the word count
			// for the formerly merged phrase, so use it; but when glossing is current,
			// the LookUpSrcWord call is done only in the first map, so nWordsInPhrase
			// will not be greater than 1 when doing glossing
			if (nWordsInPhrase > 1) // nWordsInPhrase is a global, set in LookAhead()
									// or in LookUpSrcWord()
			{
				// nWordsInPhrase can only be > 1 in adapting mode, so handle that
				bGotTranslation = LookUpSrcWord(pNewPile);
			}
		}
		pNewPile = pApp->m_pActivePile; // update the pointer, since LookUpSrcWord()
				// calls RecalcLayout() & resets m_pActivePile (in refactored code
		// this call is still needed because we replace the old pile with the
		// altered one (it has new width since its now active location)
		if (bGotTranslation)
		{
			// if it is a <Not In KB> entry we show any m_targetStr that the
			// sourcephrase instance may have, by putting it in the global
			// translation variable; when glossing is ON, we ignore
			// "not in kb" since that pertains to adapting only
			if (!gbIsGlossing && translation == _T("<Not In KB>"))
			{
				// make sure asterisk gets shown, and the adaptation is taken
				// from the sourcephrase itself - but it will be empty
				// if the sourcephrase has not been accessed before
				translation = pNewPile->GetSrcPhrase()->m_targetStr;
				pNewPile->GetSrcPhrase()->m_bHasKBEntry = FALSE;
				pNewPile->GetSrcPhrase()->m_bNotInKB = TRUE;
			}

			pApp->m_targetPhrase = translation; // set using the global var, set in
												 // LookUpSrcWord() call
			bWantSelect = TRUE;
		}
		else // did not get a translation, or gloss
		{
			// do a copy of the source (this never needs change of capitalization)
			MakeCopyOrSetNothing(pApp, pView, pNewPile, bWantSelect);

			// BEW added 1Jul09, the flag should be TRUE if nothing was found
			pApp->m_pTargetBox->m_bAbandonable = TRUE;
		}
	}
}

// BEW 13Apr10, changes needed for support of doc version 5
void CPhraseBox::HandleUnsuccessfulLookup_InAutoAdaptMode_AsBestWeCan(CAdapt_ItApp* pApp,
	CAdapt_ItView* pView, CPile* pNewPile, bool m_bCancelAndSelect, bool& bWantSelect)
{
	pApp->m_bAutoInsert = FALSE; // cause halt
	if (!gbIsGlossing && m_bCancelAndSelect)
	{
		// user cancelled CChooseTranslation dialog because he wants instead to
		// select for a merger of two or more source words
		pApp->m_pTargetBox->m_bAbandonable = TRUE;

		// no adaptation available, so depending on the m_bCopySource flag, either
		// initialize the targetPhrase to an empty string, or to a copy of the
		// sourcePhrase's key string; then select the first two words ready for a
		// merger or extension of the selection
		MakeCopyOrSetNothing(pApp, pView, pNewPile, bWantSelect);

		// the DoCancelAndSelect() call is below after the RecalcLayout calls
	}
	else // user does not want a "Cancel and Select" selection; or is glossing
	{
		// try find a translation for the single source word, use it if we find one;
		// else do the usual copy of source word, with possible cc processing, etc.
		// LookUpSrcWord( ) has been ammended (July 2003) for auto capitalization
		// support; it does any needed case change before returning, leaving the
		// resulting string in the global variable: translation
		bool bGotTranslation = FALSE;
		if (!gbUserCancelledChooseTranslationDlg || nWordsInPhrase == 1)
		{
			bGotTranslation = LookUpSrcWord(pNewPile);
		}
		else
		{
			gbUserCancelledChooseTranslationDlg = FALSE; // restore default value

			// if the user cancelled the Choose Translation dialog when a phrase was
			// merged, then he will probably want a lookup done for the first word
			// of the now unmerged phrase; nWordsInPhrase will still contain the
			// word count for the formerly merged phrase, so use it; but when glossing
			// nWordsInPhrase should never be anything except 1, so this block should
			// not get entered when glossing
			if (nWordsInPhrase > 1) // nWordsInPhrase is a global, set in LookAhead()
									// or in LookUpSrcWord()
			{
				// must be adapting mode
				bGotTranslation = LookUpSrcWord(pNewPile);
			}
		}
		pNewPile = pApp->m_pActivePile; // update the pointer (needed, because
				// RecalcLayout() was done by LookUpSrcWord(), and in its refactored
				// code we called ResetPartnerPileWidth() to get width updated and
				// a new copy of the pile replacing the old one at same location
				// in the list m_pileList

		if (bGotTranslation)
		{
			// if it is a <Not In KB> entry we show any m_targetStr that the
			// sourcephrase instance may have, by putting it in the global
			// translation variable; when glossing is ON, we ignore
			// "not in kb" since that pertains to adapting only
			if (!gbIsGlossing && translation == _T("<Not In KB>"))
			{
				// make sure asterisk gets shown, and the adaptation is taken
				// from the sourcephrase itself - but it will be empty
				// if the sourcephrase has not been accessed before
				translation = pNewPile->GetSrcPhrase()->m_targetStr;
				pNewPile->GetSrcPhrase()->m_bHasKBEntry = FALSE;
				pNewPile->GetSrcPhrase()->m_bNotInKB = TRUE;
			}

			pApp->m_targetPhrase = translation; // set using the global var,
												 // set in LookUpSrcWord call
			bWantSelect = TRUE;
		}
		else // did not get a translation, or a gloss when glossing is current
		{
#if defined(_DEBUG) & defined(LOOKUP_FEEDBACK)
			wxLogDebug(_T("HandleUnsuccessfulLookup_InAutoAdaptMode_AsBestWeCan() Before MakeCopy...: sn = %d , key = %s , m_targetPhrase = %s"),
				pNewPile->GetSrcPhrase()->m_nSequNumber, pNewPile->GetSrcPhrase()->m_key.c_str(), pApp->m_targetPhrase.c_str());
#endif
			MakeCopyOrSetNothing(pApp, pView, pNewPile, bWantSelect);
#if defined(_DEBUG) & defined(LOOKUP_FEEDBACK)
			wxLogDebug(_T("HandleUnsuccessfulLookup_InAutoAdaptMode_AsBestWeCan() After MakeCopy...: sn = %d , key = %s , m_targetPhrase = %s"),
				pNewPile->GetSrcPhrase()->m_nSequNumber, pNewPile->GetSrcPhrase()->m_key.c_str(), pApp->m_targetPhrase.c_str());
#endif

			// BEW added 1Jul09, the flag should be TRUE if nothing was found
			pApp->m_pTargetBox->m_bAbandonable = TRUE;
		}

		// is "Accept Defaults" turned on? If so, make processing continue
		if (pApp->m_bAcceptDefaults)
		{
			pApp->m_bAutoInsert = TRUE; // revoke the halt
		}
	}
}

// returns TRUE if the move was successful, FALSE if not successful
// In refactored version, transliteration mode is handled by a separate function, so
// MoveToNextPile() is called only when CAdapt_ItApp::m_bTransliterationMode is FALSE, so
// this value can be assumed. The global boolean gbIsGlossing, however, may be either FALSE
// (adapting mode) or TRUE (glossing mode)
// Ammended July 2003 for auto-capitalization support
// BEW 13Apr10, changes needed for support of doc version 5
// BEW 21Jun10, no changes for support of kbVersion 2, & removed pView from signature
bool CPhraseBox::MoveToNextPile(CPile* pCurPile)
{
	CAdapt_ItApp* pApp = (CAdapt_ItApp*)&wxGetApp();
	pApp->m_preGuesserStr.Empty(); // BEW 27Nov14, in case a src string, or modified string
		// is stored ready for user's Esc keypress to restore the pre-guesser
		// form, clear it, because the box is gunna move and we want it
		// restored to default empty ready for next box landing location
	// whm modified 29Mar12. Left mouse clicks now beep only when certain parts of
	// the canvas are clicked on, and allows other clicks to act normally (such as
	// the opening/closing of the ViewFilteredMaterial dialog and the Notes dialog).

	//bool bNoError = TRUE;
	bool bWantSelect = FALSE; // set TRUE if any initial text in the new location is to be
							  // shown selected
	// store the translation in the knowledge base
	//pApp->limiter = 0; // BEW added Aug13, to support OnIdle() hack for m_targetStr non-stick bug // bug fixed 24Sept13 BEW
	CAdapt_ItView* pView = pApp->GetView();
	CAdapt_ItDoc* pDoc = pApp->GetDocument();
	bool bOK;
	gbByCopyOnly = FALSE; // restore default setting
	CSourcePhrase* pOldActiveSrcPhrase = pCurPile->GetSrcPhrase();
	CLayout* pLayout = GetLayout();

/* #if defined(_DEBUG)
	CPile* myPilePtr = pApp->GetView()->GetPile(pApp->m_nActiveSequNum);
	CSourcePhrase* mySrcPhrasePtr = myPilePtr->GetSrcPhrase();
	wxLogDebug(_T("MoveToNextPile() at start: sn = %d , src key = %s , m_adaption = %s , m_targetStr = %s , m_targetPhrase = %s"),
		mySrcPhrasePtr->m_nSequNumber, mySrcPhrasePtr->m_key.c_str(), mySrcPhrasePtr->m_adaption.c_str(), 
		mySrcPhrasePtr->m_targetStr.c_str(), pApp->m_targetPhrase.c_str());
#endif */

	// make sure pApp->m_targetPhrase doesn't have any final spaces
	RemoveFinalSpaces(pApp->m_pTargetBox,&pApp->m_targetPhrase);

	CPile* pNextEmptyPile = pView->GetNextEmptyPile(pCurPile);
	if (pNextEmptyPile == NULL)
	{
		// no more empty piles in the current document. We can just continue at this point
		// since we do this call again below
		;
	}
	else
	{
		// don't move forward if it means moving to an empty retranslation pile, but only for
		// when we are adapting. When glossing, the box is allowed to be within retranslations
		bool bNotInRetranslation =
				CheckPhraseBoxDoesNotLandWithinRetranslation(pView, pNextEmptyPile, pCurPile);
		if (!bNotInRetranslation)
		{
			// the phrase box landed in a retranslation, so halt the lookup and insert loop
			// so the user can do something manually to achieve what he wants in the
			// viscinity of the retranslation
			return FALSE;
		}
		// continue processing below if the phrase box did not land in a retranslation
	}

	// if the location we are leaving is a <Not In KB> one, we want to skip the store & fourth
	// line creation --- as of Dec 18, version 1.4.0, according to Susanna Imrie's
	// recommendation, I've changed this so it will allow a non-null adaptation to remain at
	// this location in the document, but just to suppress the KB store; if glossing is ON, then
	// being a <Not In KB> location is irrelevant, and we will want the store done normally - but
	// to the glossing KB of course
	bOK = TRUE;
	if (!gbIsGlossing && !pOldActiveSrcPhrase->m_bHasKBEntry && pOldActiveSrcPhrase->m_bNotInKB)
	{
		// if the user edited out the <Not In KB> entry from the KB editor, we need to put
		// it back so that the setting is preserved (the "right" way to change the setting is to
		// use the toolbar checkbox - this applies when adapting, not glossing)
		pApp->m_pKB->Fix_NotInKB_WronglyEditedOut(pCurPile);
	}
	else
	{
		// make the punctuated target string, but only if adapting; note, for auto capitalization
		// ON, the function will change initial lower to upper as required, whatever punctuation
		// regime is in place for this particular sourcephrase instance
		// in the next call, the final bool flag, bIsTransliterateMode, is default FALSE
		
/* #if defined(_DEBUG)
	wxLogDebug(_T("MoveToNextPile() before DoStore_Normal...(): sn = %d , key = %s , m_targetPhrase = %s , m_targetStr = %s"),
		pCurPile->GetSrcPhrase()->m_nSequNumber, pCurPile->GetSrcPhrase()->m_key.c_str(), pApp->m_targetPhrase.c_str(), 
		pCurPile->GetSrcPhrase()->m_targetStr.c_str());
#endif */

		bOK = DoStore_NormalOrTransliterateModes(pApp, pDoc, pView, pCurPile);
		if (!bOK)
		{
			DealWithUnsuccessfulStore(pApp, pView, pNextEmptyPile);
			return FALSE; // can't move until a valid adaption (which could be null) is supplied
		}
	}
/* #if defined(_DEBUG)
	wxLogDebug(_T("MoveToNextPile() after DoStore_Normal...: sn = %d , key = %s , m_targetPhrase = %s , m_targetStr = %s"),
		pCurPile->GetSrcPhrase()->m_nSequNumber, pCurPile->GetSrcPhrase()->m_key.c_str(), pApp->m_targetPhrase.c_str(),
		pCurPile->GetSrcPhrase()->m_targetStr.c_str());
#endif */

	// since we are moving, make sure the default m_bSaveToKB value is set
	pApp->m_bSaveToKB = TRUE;

	// move to next pile's cell which has no adaptation yet
	pApp->m_bUserTypedSomething = FALSE; // user has not typed at the new location yet
	bool bAdaptationAvailable = FALSE;
	CPile* pNewPile = pView->GetNextEmptyPile(pCurPile); // this call does not update
														 // the active sequ number

	// if necessary restore default button image, and m_bCopySourcePunctuation to TRUE
	wxCommandEvent event;
	if (!pApp->m_bCopySourcePunctuation)
	{
		pView->OnToggleEnablePunctuationCopy(event);
	}
	if (pNewPile == NULL)
	{
		// we deem vertical editing current step to have ended if control gets into this
		// block, so user has to be asked what to do next if vertical editing is currently
		// in progress; and we tunnel out before m_nActiveSequNum can be set to -1 (otherwise
		// vertical edit will crash when recalc layout is tried with a bad sequ num value)
		if (gbVerticalEditInProgress)
		{
			gbTunnellingOut = FALSE; // ensure default value set
			bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(-1,
							nextStep, TRUE); // bForceTransition is TRUE
			if (bCommandPosted)
			{
				// don't proceed further because the current vertical edit step has ended
				gbTunnellingOut = TRUE; // so caller can use it
				return FALSE;
			}
		}

		if (!pApp->m_bSingleStep)
		{
			pApp->m_bAutoInsert = FALSE; // cause halt, if auto lookup & inserting is ON
		}

		// ensure the view knows the pile pointer is no longer valid
		pApp->m_pActivePile = (CPile*)NULL;
		pApp->m_nActiveSequNum = -1;
		gbEnterTyped = FALSE;
		if (gbSuppressStoreForAltBackspaceKeypress)
			gSaveTargetPhrase.Empty();
		gbSuppressStoreForAltBackspaceKeypress = FALSE; // make sure it's off before returning
		gTemporarilySuspendAltBKSP = FALSE;

		return FALSE; // we are at the end of the document
	}
	else
	{
		// the pNewPile is valid, so proceed

        // don't commit to the new pile if we are in vertical edit mode, until we've
        // checked the pile is not in the gray text area...
        // if vertical editing is currently in progress we must check if the lookup target
        // is within the editable span, if not then control has moved the box into the gray
        // area beyond the editable span and that means a step transition is warranted &
        // the user should be asked what step is next
		if (gbVerticalEditInProgress)
		{
			int nCurrentSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
			gbTunnellingOut = FALSE; // ensure default value set
			bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(
									nCurrentSequNum,nextStep); // bForceTransition is FALSE
			if (bCommandPosted)
			{
				// don't proceed further because the current vertical edit step has ended
				gbTunnellingOut = TRUE; // so caller can use it
				return FALSE; // try returning FALSE
			}
			else
			{
				// BEW 19Oct15 No transition of vert edit modes,
				// so we can store this location on the app
				gpApp->m_vertEdit_LastActiveSequNum = nCurrentSequNum;
#if defined(_DEBUG)
				wxLogDebug(_T("VertEdit PhrBox, MoveToNextPile() storing loc'n: %d "), nCurrentSequNum);
#endif
			}
		}

        // set active pile, and same var on the phrase box, and active sequ number - but
        // note that only the active sequence number will remain valid if a merge is
        // required; in the latter case, we will have to recalc the layout after the merge
        // and set the first two variables again
		pApp->m_pActivePile = pNewPile;
		pApp->m_nActiveSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
		nCurrentSequNum = pApp->m_nActiveSequNum; // global, for use by auto-saving

		// refactored design: we want the old pile's strip to be marked as invalid and the
		// strip index added to the CLayout::m_invalidStripArray
		pDoc->ResetPartnerPileWidth(pOldActiveSrcPhrase);

        // look ahead for a match with KB phrase content at this new active location
        // LookAhead (July 2003) has been ammended for auto-capitalization support; and
        // since it does a KB lookup, it will set gbMatchedKB_UCentry TRUE or FALSE; and if
        // an entry is found, any needed case change will have been done prior to it
        // returning (the result is in the global variable: translation)
		bAdaptationAvailable = LookAhead(pNewPile);
		pView->RemoveSelection();

		// check if we found a match and have an available adaptation string ready
		if (bAdaptationAvailable)
		{
			pApp->m_pTargetBox->m_bAbandonable = FALSE;

            // adaptation is available, so use it if the source phrase has only a single
            // word, but if it's multi-worded, we must first do a merge and recalc of the
            // layout
			if (!gbIsGlossing && !gbSuppressMergeInMoveToNextPile)
			{
                // this merge is suppressed if we get here after doing a merge in
                // LookAhead() in order to see the phrase box at the correct location when
				// the Choose Translation dialog is up; it's done here only if an
				// auto-insert can be done
				if (nWordsInPhrase > 1) // nWordsInPhrase is a global, set in LookAhead()
										// or in LookUpSrcWord()
				{
					// do the needed merge, etc.
					pApp->bLookAheadMerge = TRUE; // set static flag to ON
					bool bSaveFlag = m_bAbandonable; // the box is "this"
					pView->MergeWords();
					m_bAbandonable = bSaveFlag; // preserved the flag across the merge
					pApp->bLookAheadMerge = FALSE; // restore static flag to OFF
				}
			}

			// BEW changed 9Apr12 to support discontinuous highlighting spans
			if (nWordsInPhrase == 1 || !gbSuppressMergeInMoveToNextPile)
			{
                // When nWordsInPhrase > 1, since the call of LookAhead() didn't require
                // user choice of the adaptation or gloss for the merger, it wasn't done in
                // the latter function, and so is done here automatically (because there is
                // a unique adaptation available) and so it is appropriate to make this
                // location have background highlighting, since the adaptation is now to be
                // auto-inserted after the merger was done above. Note: the
                // gbSuppressMergeInMoveToNextPile flag will be FALSE if the merger was not
                // done in the prior LookAhead() call (with ChooseTranslation() being
                // required for the user to manually choose which adaptation is wanted); we
                // use that fact in the test above.
				// When nWordsInPhrase is 1, there is no merger involved and the
				// auto-insert to be done now requires background highlighting (Note, we
				// set the flag when appropriate, but only suppress doing the background
				// colour change in the CCell's Draw() function, if the user has requested
				// that no background highlighting be used - that uses the
				// m_bSuppressTargetHighlighting flag with TRUE value to accomplish the
				// suppression)
                pLayout->SetAutoInsertionHighlightFlag(pNewPile);
			}
            // assign the translation text - but check it's not "<Not In KB>", if it is,
            // phrase box can have m_targetStr, turn OFF the m_bSaveToKB flag, DON'T halt
            // auto-inserting if it is on, (in the very earliest versions I made it halt)
            // -- for version 1.4.0 and onwards, this does not change because when auto
            // inserting, we must have a default translation for a 'not in kb' one - and
            // the only acceptable default is a null string. The above applies when
            // gbIsGlossing is OFF
			wxString str = translation; // translation set within LookAhead()

			if (!gbIsGlossing && (translation == _T("<Not In KB>")))
			{
				pApp->m_bSaveToKB = FALSE;
				translation = pNewPile->GetSrcPhrase()->m_targetStr; // probably empty
				pApp->m_targetPhrase = translation;
				bWantSelect = FALSE;
				pApp->m_pTargetBox->m_bAbandonable = TRUE;
				pNewPile->GetSrcPhrase()->m_bHasKBEntry = FALSE;
				pNewPile->GetSrcPhrase()->m_bNotInKB = TRUE; // ensures * shows above
															 // this srcPhrase
			}
			else
			{
				pApp->m_targetPhrase = translation;
				bWantSelect = FALSE;
			}
#ifdef Highlighting_Bug
			// BEW changed 9Apr12 for support of discontinuous highlighting spans
			wxLogDebug(_T("PhraseBox::MoveToNextPile(), hilighting: sequnum = %d  where the user chose:  %s  for source:  %s"),
				nCurrentSequNum, translation, pNewPile->GetSrcPhrase()->m_srcPhrase);
#endif
            // treat auto insertion as if user typed it, so that if there is a
            // user-generated extension done later, the inserted translation will not be
            // removed and copied source text used instead; since user probably is going to
            // just make a minor modification
			pApp->m_bUserTypedSomething = TRUE;

            // get a widened pile pointer for the new active location, and we want the
            // pile's strip to be marked as invalid and the strip index added to the
            // CLayout::m_invalidStripArray
			if (pNewPile != NULL)
			{
				pDoc->ResetPartnerPileWidth(pNewPile->GetSrcPhrase());
			}
		}
		else // the lookup determined that no adaptation (or gloss when glossing), or
			 // <Not In KB> entry, is available
		{
			pNewPile = pApp->m_pActivePile;

            // RecalcLayout call when there is no adaptation available from the LookAhead,
            // (or user cancelled when shown the Choose Translation dialog from within the
            // LookAhead() function, having matched) we must cause auto lookup and
            // inserting to be turned off, so that the user can do a manual adaptation; but
            // if the m_bAcceptDefaults flag is on, then the copied source (having been
            // through c.changes) is accepted without user input, the m_bAutoInsert flag is
            // turned back on, so processing will continue; while if
            // m_bCancelAndSelectButtonPressed is TRUE, then the first two words are
            // selected instead ready for a merger or for extending the selection - if both
            // flags are TRUE, the m_bCancelAndSelectButtonPressed is to have priority
            pApp->m_nActiveSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
			if (!pApp->m_bSingleStep)
			{
				// This call internally sets m_bAutoInsert to FALSE at its first line, but
				// if in cc mode and m_bAcceptDefaults is true, then cc keeps the box moving
				// forward by resetting m_bAutoInsert to TRUE before it returns
				HandleUnsuccessfulLookup_InAutoAdaptMode_AsBestWeCan(
						pApp, pView, pNewPile, m_bCancelAndSelectButtonPressed, bWantSelect);
			}
			else // it's single step mode
			{
				HandleUnsuccessfulLookup_InSingleStepMode_AsBestWeCan(
						pApp, pView, pNewPile, m_bCancelAndSelectButtonPressed, bWantSelect);
			}
            // get a widened pile pointer for the new active location, and we want the
            // pile's strip to be marked as invalid and the strip index added to the
            // CLayout::m_invalidStripArray
			if (pNewPile != NULL)
			{
				pDoc->ResetPartnerPileWidth(pNewPile->GetSrcPhrase());
			}
		}

		// initialize the phrase box too, so it doesn't carry the old string to the next
		// pile's cell
		ChangeValue(pApp->m_targetPhrase); //SetWindowText(pApp->m_targetPhrase);

		// if we merged and moved, we have to update pNewPile, because we have done a
		// RecalcLayout in the LookAhead() function; it's possible to return from
		// LookAhead() without having done a recalc of the layout, so the else block
		// should cover that situation
		if (gbCompletedMergeAndMove)
		{
			pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
		}
		else
		{
			// do we need this one?? I think so, but should step it to make sure
#ifdef _NEW_LAYOUT
			pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
#else
			pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
			// get the new active pile
			pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
			wxASSERT(pApp->m_pActivePile != NULL);
		}

        // if the user has turned on the sending of synchronized scrolling messages send
        // the relevant message once auto-inserting halts, because we don't want to make
        // other applications sync scroll during auto-insertions, as it could happen very
        // often and the user can't make any visual use of what would be happening anyway;
        // even if a Cancel and Select is about to be done, a sync scroll is appropriate
        // now, provided auto-inserting has halted
		if (!gbIgnoreScriptureReference_Send && !pApp->m_bAutoInsert)
		{
			pView->SendScriptureReferenceFocusMessage(pApp->m_pSourcePhrases,
													pApp->m_pActivePile->GetSrcPhrase());
		}

        // we had to delay the call of DoCancelAndSelect() until now because earlier
        // RecalcLayout() calls will clobber any selection we try to make beforehand, so do
        // the selecting now; do it also before recalculating the phrase box, since if
        // anything moves, we want its location to be correct. When glossing, Cancel
        // and Select is not allowed, so we skip this block
		if (!gbIsGlossing && m_bCancelAndSelectButtonPressed)

		{
			DoCancelAndSelect(pView, pApp->m_pActivePile); // clears m_bCancelAndSelectButtonPressed
			pApp->m_bSelectByArrowKey = TRUE; // so it is ready for extending
		}
		// update status bar with project name
		pApp->RefreshStatusBarInfo();

		// recreate the phraseBox using the stored information
		if (bWantSelect)
		{
			//pApp->m_nStartChar = 0;
			pApp->m_nStartChar = -1; // wx uses -1, not 0 as in MFC
			pApp->m_nEndChar = -1;
		}
		else
		{
			int len = GetLineLength(0); // 0 = first line, only line
			pApp->m_nStartChar = len;
			pApp->m_nEndChar = len;
		}

        // fix the m_bSaveToKB flag, depending on whether or not srcPhrase is in kb; but
        // this applies only when adapting, not glossing
		if (!gbIsGlossing && !pApp->m_pActivePile->GetSrcPhrase()->m_bHasKBEntry &&
			pApp->m_pActivePile->GetSrcPhrase()->m_bNotInKB)
		{
			pApp->m_bSaveToKB = FALSE;
			pApp->m_targetPhrase.Empty();
		}
		else
		{
			pApp->m_bSaveToKB = TRUE;
		}

		gbCompletedMergeAndMove = FALSE; // make sure it's cleared

        // BEW note 24Mar09: later we may use clipping (the comment below may not apply in
        // the new design anyway)
		pView->Invalidate(); // do the whole client area, because if target font is larger
            // than the source font then changes along the line throw words off screen and
            // they get missed and eventually app crashes because active pile pointer will
            // get set to NULL
		pLayout->PlaceBox();

		if (bWantSelect)
			SetModify(TRUE); // our own SetModify(); calls MarkDirty()
		else
			SetModify(FALSE); // our own SetModify(); calls DiscardEdits()

		// make sure gbSuppressMergeInMoveToNextPile is reset to the default value
		gbSuppressMergeInMoveToNextPile = FALSE;

		return TRUE;
	}
}

bool CPhraseBox::GetCancelAndSelectFlag()
{
	return m_bCancelAndSelectButtonPressed;
}

void CPhraseBox::ChangeCancelAndSelectFlag(bool bValue)
{
	m_bCancelAndSelectButtonPressed = bValue;
}

// returns TRUE if all was well, FALSE if something prevented the move
// Note: this is based on MoveToNextPile(), but with added code for transliterating - and
// recall that when transliterating, the extra code may be used, or the normal KB lookup
// code may be used, depending on the user's assessment of whether the transliterating
// converter did its job correctly, or not correctly, respectively. When control is in this
// function CAdapt_ItApp::m_bTransliterationMode will be TRUE, and can therefore be
// assumed; likewise the global boolean gbIsGlossing will be FALSE (because
// transliteration mode is not available when glossing mode is turned ON), and so that too
// can be assumed
// BEW 13Apr10, changes needed for support of doc version 5
// BEW 21Jun10, no changes needed for support of kbVersion 2, & removed pView from signature
bool CPhraseBox::MoveToNextPile_InTransliterationMode(CPile* pCurPile)
{
	CAdapt_ItApp* pApp = (CAdapt_ItApp*)&wxGetApp();
	pApp->m_preGuesserStr.Empty(); // BEW 27Nov14, in case a src string, or modified string
		// is stored ready for user's Esc keypress to restore the pre-guesser
		// form, clear it, because the box is gunna move and we want it
		// restored to default empty ready for next box landing location
	// whm modified 29Mar12. Left mouse clicks now beep only when certain parts of
	// the canvas are clicked on, and allows other clicks to act normally (such as
	// the opening/closing of the ViewFilteredMaterial dialog and the Notes dialog).
	//bool bNoError = TRUE;
	bool bWantSelect = FALSE; // set TRUE if any initial text in the new location is to be
							  // shown selected
	// store the translation in the knowledge base
	//pApp->limiter = 0; // BEW added Aug13, to support OnIdle() hack for m_targetStr non-stick bug // bug fixed 24Sept13 BEW
	CAdapt_ItView* pView = pApp->GetView();
	CAdapt_ItDoc* pDoc = pApp->GetDocument();
	bool bOK;
	gbByCopyOnly = FALSE; // restore default setting
	CSourcePhrase* pOldActiveSrcPhrase = pCurPile->GetSrcPhrase();
	CLayout* pLayout = GetLayout();

	// make sure pApp->m_targetPhrase doesn't have any final spaces
	RemoveFinalSpaces(pApp->m_pTargetBox,&pApp->m_targetPhrase);

	CPile* pNextEmptyPile = pView->GetNextEmptyPile(pCurPile);
	if (pNextEmptyPile == NULL)
	{
		// no more empty piles in the current document. We can just continue at this point
		// since we do this call again below
		;
	}
	else
	{
		// don't move forward if it means moving to an empty retranslation pile, but only for
		// when we are adapting. When glossing, the box is allowed to be within retranslations
		bool bNotInRetranslation = CheckPhraseBoxDoesNotLandWithinRetranslation(pView,
															pNextEmptyPile, pCurPile);
		if (!bNotInRetranslation)
		{
			// the phrase box landed in a retranslation, so halt the lookup and insert loop
			// so the user can do something manually to achieve what he wants in the
			// viscinity of the retranslation
			return FALSE;
		}
		// continue processing below if the phrase box did not land in a retranslation
	}

	// if the location we are leaving is a <Not In KB> one, we want to skip the store & fourth
	// line creation --- as of Dec 18, version 1.4.0, according to Susanna Imrie's
	// recommendation, I've changed this so it will allow a non-null adaptation to remain at
	// this location in the document, but just to suppress the KB store; if glossing is ON, then
	// being a <Not In KB> location is irrelevant, and we will want the store done normally - but
	// to the glossing KB of course
	// BEW addition 21Apr06 to support transliterating better (showing transiterations)
	if (gbSuppressStoreForAltBackspaceKeypress)
	{
		pApp->m_targetPhrase = gSaveTargetPhrase; // set it up in advance, from last LookAhead() call
		goto c;
	}
	if (!gbIsGlossing && !pOldActiveSrcPhrase->m_bHasKBEntry && pOldActiveSrcPhrase->m_bNotInKB)
	{
		// if the user edited out the <Not In KB> entry from the KB editor, we need to put
		// it back so that the setting is preserved (the "right" way to change the setting is to
		// use the toolbar checkbox - this applies when adapting, not glossing)
		pApp->m_pKB->Fix_NotInKB_WronglyEditedOut(pCurPile);
		goto b;
	}

	// make the punctuated target string, but only if adapting; note, for auto capitalization
	// ON, the function will change initial lower to upper as required, whatever punctuation
	// regime is in place for this particular sourcephrase instance

    // BEW added 19Apr06 for support of Alt + Backspace keypress suppressing store on the
    // phrase box move which is a feature for power users requested by Bob Eaton; the code
    // in the first block is for this new (undocumented) feature - power uses will have
    // knowledge of it from an article to be placed in Word&Deed by Bob. Only needed for
    // adapting mode, so the glossing mode case is commented out
c:	bOK = TRUE;
	if (gbSuppressStoreForAltBackspaceKeypress)
	{
		// when we don't want to store in the KB, we still have some things to do
		// to get appropriate m_adaption and m_targetStr members set up for the doc...
		// when adapting, fill out the m_targetStr member of the CSourcePhrase instance,
		// and do any needed case conversion and get punctuation in place if required
		pView->MakeTargetStringIncludingPunctuation(pOldActiveSrcPhrase, pApp->m_targetPhrase);

		// the m_targetStr member may now have punctuation, so get rid of it
		// before assigning whatever is left to the m_adaption member
		wxString strKeyOnly = pOldActiveSrcPhrase->m_targetStr;
		pView->RemovePunctuation(pDoc,&strKeyOnly,from_target_text);

		// set the m_adaption member too
		pOldActiveSrcPhrase->m_adaption = strKeyOnly;

		// let the user see the unpunctuated string in the phrase box as visual feedback
		pApp->m_targetPhrase = strKeyOnly;

        // now do a store, but only of <Not In KB>, (StoreText uses
        // gbSuppressStoreForAltBackspaceKeypress == TRUE to get this job done rather than
        // a normal store) & sets flags appropriately (Note, while we pass in
        // pApp->m_targetPhrase, the phrase box contents string, we StoreText doesn't use
        // it when gbSuppressStoreForAltBackspaceKeypress is TRUE, but internally sets a
        // local string to "<Not In KB>" and stores that instead) BEW 27Jan09, nothing more
        // needed here
        if (gbIsGlossing)
		{
			bOK = pApp->m_pGlossingKB->StoreText(pOldActiveSrcPhrase, pApp->m_targetPhrase);
		}
		else
		{
			bOK = pApp->m_pKB->StoreText(pOldActiveSrcPhrase, pApp->m_targetPhrase);
		}
	}
	else
	{
		bOK = DoStore_NormalOrTransliterateModes(pApp, pDoc, pView, pCurPile,
												pApp->m_bTransliterationMode);
	}
	if (!bOK)
	{
		DealWithUnsuccessfulStore(pApp, pView, pNextEmptyPile);
		return FALSE; // can't move until a valid adaption (which could be null) is supplied
	}

	// since we are moving, make sure the default m_bSaveToKB value is set
b:	pApp->m_bSaveToKB = TRUE;

	// move to next pile's cell which has no adaptation yet
	pApp->m_bUserTypedSomething = FALSE; // user has not typed at the new location yet
	bool bAdaptationAvailable = FALSE;
	CPile* pNewPile = pView->GetNextEmptyPile(pCurPile); // this call does not update
														 // the active sequ number
	// if necessary restore default button image, and m_bCopySourcePunctuation to TRUE
	wxCommandEvent event;
	if (!pApp->m_bCopySourcePunctuation)
	{
		pApp->GetView()->OnToggleEnablePunctuationCopy(event);
	}
	if (pNewPile == NULL)
	{
		// we deem vertical editing current step to have ended if control gets into this
		// block, so user has to be asked what to do next if vertical editing is currently
		// in progress; and we tunnel out before m_nActiveSequNum can be set to -1 (otherwise
		// vertical edit will crash when recalc layout is tried with a bad sequ num value)
		if (gbVerticalEditInProgress)
		{
			gbTunnellingOut = FALSE; // ensure default value set
			bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(-1,
							nextStep, TRUE); // bForceTransition is TRUE
			if (bCommandPosted)
			{
				// don't proceed further because the current vertical edit step has ended
				gbTunnellingOut = TRUE; // so caller can use it
				return FALSE;
			}
			else
			{
				// BEW 19Oct15 No transition of vert edit modes,
				// so we can store this location on the app
				gpApp->m_vertEdit_LastActiveSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
#if defined(_DEBUG)
				wxLogDebug(_T("VertEdit PhrBox, MoveToNextPile_InTransliterationMode() storing loc'n: %d "), 
					pNewPile->GetSrcPhrase()->m_nSequNumber);
#endif
			}
		}

		if (!pApp->m_bSingleStep)
		{
			pApp->m_bAutoInsert = FALSE; // cause halt, if auto lookup & inserting is ON
		}

		// ensure the view knows the pile pointer is no longer valid
		pApp->m_pActivePile = (CPile*)NULL;
		pApp->m_nActiveSequNum = -1;
		gbEnterTyped = FALSE;
		if (gbSuppressStoreForAltBackspaceKeypress)
			gSaveTargetPhrase.Empty();
		gbSuppressStoreForAltBackspaceKeypress = FALSE; // make sure it's off before returning
		gTemporarilySuspendAltBKSP = FALSE;

		return FALSE; // we are at the end of the document
	}
	else
	{
		// the pNewPile is valid, so proceed

        // don't commit to the new pile if we are in vertical edit mode, until we've
        // checked the pile is not in the gray text area...
        // if vertical editing is currently in progress we must check if the lookup target
        // is within the editable span, if not then control has moved the box into the gray
        // area beyond the editable span and that means a step transition is warranted &
        // the user should be asked what step is next
		if (gbVerticalEditInProgress)
		{
			int nCurrentSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
			gbTunnellingOut = FALSE; // ensure default value set
			bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(
									nCurrentSequNum,nextStep); // bForceTransition is FALSE
			if (bCommandPosted)
			{
				// don't proceed further because the current vertical edit step has ended
				gbTunnellingOut = TRUE; // so caller can use it
				return FALSE; // try returning FALSE
			}
			else
			{
				// BEW 19Oct15 No transition of vert edit modes,
				// so we can store this location on the app
				gpApp->m_vertEdit_LastActiveSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
#if defined(_DEBUG)
				wxLogDebug(_T("VertEdit PhrBox, MoveToNextPile_InTransliterationMode() storing loc'n: %d "), 
					pNewPile->GetSrcPhrase()->m_nSequNumber);
#endif
			}
		}

        // set active pile, and same var on the phrase box, and active sequ number - but
        // note that only the active sequence number will remain valid if a merge is
        // required; in the latter case, we will have to recalc the layout after the merge
        // and set the first two variables again
		pApp->m_pActivePile = pNewPile;
		pApp->m_nActiveSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
		nCurrentSequNum = pApp->m_nActiveSequNum; // global, for use by auto-saving

        // adjust width pf the pile pointer for the new active location, and we want the
        // pile's strip to be marked as invalid and the strip index added to the
        // CLayout::m_invalidStripArray
		pDoc->ResetPartnerPileWidth(pOldActiveSrcPhrase);

        // look ahead for a match with KB phrase content at this new active location
        // LookAhead (July 2003) has been ammended for auto-capitalization support; and
        // since it does a KB lookup, it will set gbMatchedKB_UCentry TRUE or FALSE; and if
        // an entry is found, any needed case change will have been done prior to it
        // returning (the result is in the global variable: translation)
		bAdaptationAvailable = LookAhead(pNewPile);
		pView->RemoveSelection();

		// check if we found a match and have an available adaptation string ready
		if (bAdaptationAvailable)
		{
			pApp->m_pTargetBox->m_bAbandonable = FALSE;

             // adaptation is available, so use it if the source phrase has only a single
            // word, but if it's multi-worded, we must first do a merge and recalc of the
            // layout
			if (!gbIsGlossing && !gbSuppressMergeInMoveToNextPile)
			{
                // this merge is suppressed if we get here after doing a merge in
                // LookAhead() in order to see the phrase box at the correct location when
                // the Choose Translation dialog is up; it's done here only if an
				// auto-insert can be done
				if (nWordsInPhrase > 1) // nWordsInPhrase is a global, set in LookAhead()
										// or in LookUpSrcWord()
				{
					// do the needed merge, etc.
					pApp->bLookAheadMerge = TRUE; // set static flag to ON
					bool bSaveFlag = m_bAbandonable; // the box is "this"
					pView->MergeWords();
					m_bAbandonable = bSaveFlag; // preserved the flag across the merge
					pApp->bLookAheadMerge = FALSE; // restore static flag to OFF
				}
			}

			// BEW changed 9Apr12 to support discontinuous highlighting spans
			if (nWordsInPhrase == 1 || !gbSuppressMergeInMoveToNextPile)
			{
                // When nWordsInPhrase > 1, since the call of LookAhead() didn't require
                // user choice of the adaptation or gloss for the merger, it wasn't done in
                // the latter function, and so is done here automatically (because there is
                // a unique adaptation available) and so it is appropriate to make this
                // location have background highlighting, since the adaptation is now to be
                // auto-inserted after the merger was done above. Note: the
                // gbSuppressMergeInMoveToNextPile flag will be FALSE if the merger was not
                // done in the prior LookAhead() call (with ChooseTranslation() being
                // required for the user to manually choose which adaptation is wanted); we
                // use that fact in the test above.
				// When nWordsInPhrase is 1, there is no merger involved and the
				// auto-insert to be done now requires background highlighting (Note, we
				// set the flag when appropriate, but only suppress doing the background
				// colour change in the CCell's Draw() function, if the user has requested
				// that no background highlighting be used - that uses the
				// m_bSuppressTargetHighlighting flag with TRUE value to accomplish the
				// suppression)
                pLayout->SetAutoInsertionHighlightFlag(pNewPile);
			}
            // assign the translation text - but check it's not "<Not In KB>", if it is,
            // phrase box can have m_targetStr, turn OFF the m_bSaveToKB flag, DON'T halt
            // auto-inserting if it is on, (in the very earliest versions I made it halt)
            // -- for version 1.4.0 and onwards, this does not change because when auto
            // inserting, we must have a default translation for a 'not in kb' one - and
            // the only acceptable default is a null string. The above applies when
            // gbIsGlossing is OFF
			wxString str = translation; // translation set within LookAhead()

            // BEW added 21Apr06, so that when transliterating the lookup puts a fresh
            // transliteration of the source when it finds a <Not In KB> entry, since the
            // latter signals that the SIL Converters conversion yields a correct result
            // for this source text, so we want the user to get the feedback of seeing it,
            // but still just have <Not In KB> in the KB entry
			if (!pApp->m_bSingleStep && (translation == _T("<Not In KB>"))
											&& gTemporarilySuspendAltBKSP)
			{
				gbSuppressStoreForAltBackspaceKeypress = TRUE;
				gTemporarilySuspendAltBKSP = FALSE;
			}

			if (gbSuppressStoreForAltBackspaceKeypress && (translation == _T("<Not In KB>")))
			{
				pApp->m_bSaveToKB = FALSE;
 				CSourcePhrase* pSrcPhr = pNewPile->GetSrcPhrase();
				wxString str = pView->CopySourceKey(pSrcPhr, pApp->m_bUseConsistentChanges);
				bWantSelect = FALSE;
				pApp->m_pTargetBox->m_bAbandonable = TRUE;
				pSrcPhr->m_bHasKBEntry = FALSE;
				pSrcPhr->m_bNotInKB = TRUE; // ensures * shows above
				pSrcPhr->m_adaption = str;
				pSrcPhr->m_targetStr = pSrcPhr->m_precPunct + str;
				pSrcPhr->m_targetStr += pSrcPhr->m_follPunct;
				translation = pSrcPhr->m_targetStr;
				pApp->m_targetPhrase = translation;
				gSaveTargetPhrase = translation; // to make it available on
												 // next auto call of OnePass()
			}
			// continue with the legacy code
			else if (translation == _T("<Not In KB>"))
			{
				pApp->m_bSaveToKB = FALSE;
				translation = pNewPile->GetSrcPhrase()->m_targetStr; // probably empty
				pApp->m_targetPhrase = translation;
				bWantSelect = FALSE;
				pApp->m_pTargetBox->m_bAbandonable = TRUE;
				pNewPile->GetSrcPhrase()->m_bHasKBEntry = FALSE;
				pNewPile->GetSrcPhrase()->m_bNotInKB = TRUE; // ensures * shows above this srcPhrase
			}
			else
			{
				pApp->m_targetPhrase = translation;
				bWantSelect = FALSE;

				if (gbSuppressStoreForAltBackspaceKeypress)
				{
                    // was the normal entry found while the
                    // gbSuppressStoreForAltBackspaceKeypress flag was TRUE? Then we have
                    // to turn the flag off for a while, but turn it on programmatically
                    // later if we are still in Automatic mode and we come to another <Not
                    // In KB> entry. We can do this with another BOOL defined for this
                    // purpose
					gTemporarilySuspendAltBKSP = TRUE;
					gbSuppressStoreForAltBackspaceKeypress = FALSE;
				}
			}

            // treat auto insertion as if user typed it, so that if there is a
            // user-generated extension done later, the inserted translation will not be
            // removed and copied source text used instead; since user probably is going to
            // just make a minor modification
			pApp->m_bUserTypedSomething = TRUE;

            // get a widened pile pointer for the new active location, and we want the
            // pile's strip to be marked as invalid and the strip index added to the
            // CLayout::m_invalidStripArray
			if (pNewPile != NULL)
			{
				pDoc->ResetPartnerPileWidth(pNewPile->GetSrcPhrase());
			}
		}
		else // the lookup determined that no adaptation (or gloss when glossing), or
			 // <Not In KB> entry, is available
		{
			// we're gunna halt, so this is the time to clear the flag
			if (gbSuppressStoreForAltBackspaceKeypress)
				gSaveTargetPhrase.Empty();
			gbSuppressStoreForAltBackspaceKeypress = FALSE; // make sure it's off before returning

			pNewPile = pApp->m_pActivePile; // ensure its valid, we may get here after a
            // RecalcLayout call when there is no adaptation available from the LookAhead,
            // (or user cancelled when shown the Choose Translation dialog from within the
            // LookAhead() function, having matched) we must cause auto lookup and
            // inserting to be turned off, so that the user can do a manual adaptation; but
            // if the m_bAcceptDefaults flag is on, then the copied source (having been
            // through c.changes) is accepted without user input, the m_bAutoInsert flag is
            // turned back on, so processing will continue; while if
            // m_bCancelAndSelectButtonPressed is TRUE, then the first two words are
            // selected instead ready for a merger or for extending the selection - if both
            // flags are TRUE, the m_bCancelAndSelectButtonPressed is to have priority
            pApp->m_nActiveSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
			if (!pApp->m_bSingleStep)
			{
				HandleUnsuccessfulLookup_InAutoAdaptMode_AsBestWeCan(pApp, pView, pNewPile,
										m_bCancelAndSelectButtonPressed, bWantSelect);
			}
			else // it's single step mode
			{
				HandleUnsuccessfulLookup_InSingleStepMode_AsBestWeCan(pApp, pView, pNewPile,
										m_bCancelAndSelectButtonPressed, bWantSelect);
			}

            // get a widened pile pointer for the new active location, and we want the
            // pile's strip to be marked as invalid and the strip index added to the
            // CLayout::m_invalidStripArray
			if (pNewPile != NULL)
			{
				pDoc->ResetPartnerPileWidth(pNewPile->GetSrcPhrase());
			}
		}

		// initialize the phrase box too, so it doesn't carry the old string to the next
		// pile's cell
		ChangeValue(pApp->m_targetPhrase); //SetWindowText(pApp->m_targetPhrase);

		// if we merged and moved, we have to update pNewPile, because we have done a
		// RecalcLayout in the LookAhead() function; it's possible to return from
		// LookAhead() without having done a recalc of the layout, so the else block
		// should cover that situation
		if (gbCompletedMergeAndMove)
		{
			pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
		}
		else
		{
			// do we need this one?? I think so
#ifdef _NEW_LAYOUT
			pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
#else
			pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
			// in call above, m_stripArray gets rebuilt, but m_pileList is left untouched

			// get the new active pile
			pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
			wxASSERT(pApp->m_pActivePile != NULL);
		}

        // if the user has turned on the sending of synchronized scrolling messages send
        // the relevant message once auto-inserting halts, because we don't want to make
        // other applications sync scroll during auto-insertions, as it could happen very
        // often and the user can't make any visual use of what would be happening anyway;
        // even if a Cancel and Select is about to be done, a sync scroll is appropriate
        // now, provided auto-inserting has halted
		if (!gbIgnoreScriptureReference_Send && !pApp->m_bAutoInsert)
		{
			pView->SendScriptureReferenceFocusMessage(pApp->m_pSourcePhrases,
													pApp->m_pActivePile->GetSrcPhrase());
		}

        // we had to delay the call of DoCancelAndSelect() until now because earlier
        // RecalcLayout() calls will clobber any selection we try to make beforehand, so do
        // the selecting now; do it also before recalculating the phrase box, since if
        // anything moves, we want its location to be correct. When glossing, Cancel
        // and Select is not allowed, so we skip this block
		if (m_bCancelAndSelectButtonPressed)

		{
			DoCancelAndSelect(pView, pApp->m_pActivePile); // clears m_bCancelAndSelectButtonPressed
			pApp->m_bSelectByArrowKey = TRUE; // so it is ready for extending
		}

		// update status bar with project name
		pApp->RefreshStatusBarInfo();

		// recreate the phraseBox using the stored information
		if (bWantSelect)
		{
			pApp->m_nStartChar = -1; // WX uses -1, not 0 as in MFC
			pApp->m_nEndChar = -1;
		}
		else
		{
			int len = GetLineLength(0); // 0 = first line, only line
			pApp->m_nStartChar = len;
			pApp->m_nEndChar = len;
		}

        // fix the m_bSaveToKB flag, depending on whether or not srcPhrase is in kb; but
        // this applies only when adapting, not glossing
		if (!gbIsGlossing && !pApp->m_pActivePile->GetSrcPhrase()->m_bHasKBEntry &&
			pApp->m_pActivePile->GetSrcPhrase()->m_bNotInKB)
		{
			pApp->m_bSaveToKB = FALSE;
			pApp->m_targetPhrase.Empty();
		}
		else
		{
			pApp->m_bSaveToKB = TRUE;
		}

		gbCompletedMergeAndMove = FALSE; // make sure it's cleared

		// BEW note 24Mar09: later we may use clipping (the comment below may not apply in
		// the new design anyway)
		pView->Invalidate(); // do the whole client area, because if target font is larger
            // than the source font then changes along the line throw words off screen and
            // they get missed and eventually app crashes because active pile pointer will
            // get set to NULL
		pLayout->PlaceBox();

		if (bWantSelect)
			SetModify(TRUE); // our own SetModify(); calls MarkDirty()
		else
			SetModify(FALSE); // our own SetModify(); calls DiscardEdits()

		// make sure gbSuppressMergeInMoveToNextPile is reset to the default value
		gbSuppressMergeInMoveToNextPile = FALSE;

		return TRUE;
	}
}

// BEW 13Apr10, no changes needed for support of doc version 5
bool CPhraseBox::IsActiveLocWithinSelection(const CAdapt_ItView* WXUNUSED(pView), const CPile* pActivePile)
{
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	bool bYes = FALSE;
	const CCellList* pList = &pApp->m_selection;
	if (pList->GetCount() == 0)
		return bYes;
	CCellList::Node* pos = pList->GetFirst();
	while (pos != NULL)
	{
		CCell* pCell = (CCell*)pos->GetData();
		pos = pos->GetNext();
		//if (pCell->m_pPile == pActivePile)
		if (pCell->GetPile() == pActivePile)
		{
			bYes = TRUE;
			break;
		}
	}
	return bYes;
}

// return TRUE if we made a match and there is a translation to be inserted (see static var
// below); return FALSE if there was no match. This function assumes that the pNewPile pointer
// passed in is the active pile, and that CAdapt_ItApp::m_nActiveSequNum is the correct
// index within the m_pSourcePhrases list for the CSourcePhrase instance pointed at by the
// m_pSrcPhrase member of pNewPile
// BEW 13Apr10, a small change needed for support of doc version 5
// BEW 21Jun10, updated for support of kbVersion 2
// BEW 13Nov10, changed by Bob Eaton's request, for glossing KB to use all maps
bool CPhraseBox::LookAhead(CPile* pNewPile)
{
	// refactored 25Mar09 (old code is at end of file)
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	CAdapt_ItView *pView = pApp->GetView(); // <<-- BEWARE if we later support multiple views/panes
	CLayout* pLayout = GetLayout();
	CSourcePhrase* pSrcPhrase = pNewPile->GetSrcPhrase();
	int		nNewSequNum = pSrcPhrase->m_nSequNumber; // sequ number at the new location
	wxString	phrases[10]; // store built phrases here, for testing against KB stored source phrases
	int		numPhrases;  // how many phrases were built in any one call of this LookAhead function
	translation.Empty(); // clear the static variable, ready for a new translation, or gloss,
						 // if one can be found
	nWordsInPhrase = 0;	  // the global, initialize to value for no match
	gbByCopyOnly = FALSE; // restore default setting

	// we should never have an active selection at this point, so ensure there is no selection
	pView->RemoveSelection();


	// build the as many as 10 phrases based on first word at the new pile and the following
	// nine piles, or as many as are valid for phrase building (there are 7 conditions which
	// will stop the builds). When adapting, all 10 can be used; when glossing, only can use
	// the first of the ten and in that case numPhrases = 1 will be returned.
	// For auto capitalization support, the 10 phrases strings are built from the document's
	// CSourcePhrase instances as before, no case changes made; and any case changes, and secondary
	// lookups if the primary (lower case) lookup fails when the source is upper case, are done
	// within the AutoCapsLookup( ) function which is called within FindMatchInKB( ) as called
	// below; so nothing needs to be done here.
	numPhrases = BuildPhrases(phrases, nNewSequNum, pApp->m_pSourcePhrases);

	// if the returned value is zero, no phrases were built, so this constitutes a non-match
	// BEW changed,9May05, to allow for the case when there is a merger at the last sourcephrase
	// of the document and it's m_bHasKBEntry flag (probably erroneously) is FALSE - the move
	// will detect the merger and BuildPhrases will not build any (so it exits returning zero)
	// and if we were to just detect the zero and return FALSE, a copy of the source would
	// overwrite the merger's translation - to prevent this, we'll detect this state of affairs
	// and cause the box to halt, but with the correct (old) adaptation showing. Then we have
	// unexpected behaviour (ie. the halt), but not an unexpected adaptation string.
	// BEW 6Aug13, added a gbIsGlossing == TRUE block to the next test
	if (numPhrases == 0)
	{
		if (gbIsGlossing)
		{
			if (pSrcPhrase->m_nSrcWords > 1 && !pSrcPhrase->m_gloss.IsEmpty())
			{
				translation = pSrcPhrase->m_gloss;
				nWordsInPhrase = 1;
				pApp->m_bAutoInsert = FALSE; // cause a halt
				return TRUE;
			}
			else
			{
				return FALSE; // return empty string in the translation global wxString
			}
		}
		else
		{
			if (pSrcPhrase->m_nSrcWords > 1 && !pSrcPhrase->m_adaption.IsEmpty())
			{
				translation = pSrcPhrase->m_adaption;
				nWordsInPhrase = 1; // strictly speaking not correct, but it's the value we want
									// on return to the caller so it doesn't try a merger
				pApp->m_bAutoInsert = FALSE; // cause a halt
				return TRUE;
			}
			else
			{
				return FALSE; // return empty string in the translation global wxString
			}
		}
	}

	// check these phrases, longest first, attempting to find a match in the KB
	// BEW 13Nov10, changed by Bob Eaton's request, for glossing KB to use all maps
	CKB* pKB;
	int nCurLongest; // index of the map which is highest having content, maps at higher index
					 // values currently have no content
	if (gbIsGlossing)
	{
		pKB = pApp->m_pGlossingKB;
		nCurLongest = pKB->m_nMaxWords; // no matches are possible for phrases longer
										// than nCurLongest when adapting
		//nCurLongest = 1; // the caller should ensure this, but doing it explicitly here is
						 // some extra insurance to keep matching within the only map that exists
						 // when the glossing KB is in use
	}
	else
	{
		pKB = pApp->m_pKB;
		nCurLongest = pKB->m_nMaxWords; // no matches are possible for phrases longer
										// than nCurLongest when adapting
	}
	CTargetUnit* pTargetUnit = (CTargetUnit*)NULL;
	int index = nCurLongest - 1;
	bool bFound = FALSE;
	while (index > -1)
	{
		bFound = pKB->FindMatchInKB(index + 1, phrases[index], pTargetUnit);
		if (bFound)
		{
			// matched a source phrase which has identical key as the built phrase
			break;
		}
		else
		{
			index--; // try next-smaller built phrase
		}
	}

	// if no match was found, we return immediately with a return value of FALSE
	if (!bFound)
	{
		pCurTargetUnit = (CTargetUnit*)NULL; // the global pointer must be cleared
		curKey.Empty(); // global var curKey not needed, so clear it
		return FALSE;
	}
	pCurTargetUnit = pTargetUnit; // set global pointer so the dialog can use it if it
								  // is called
	curKey = phrases[index]; // set the curKey so the dialog can use it if it is called
							 // curKey is a global variable (the lookup of phrases[index] is
							 // done on a copy, so curKey retains the case of the key as in
							 // the document; but the lookup will have set global flags
							 // such as gbMatchedKB_UCentry to TRUE or FALSE as appropriate)
	nWordsInPhrase = index + 1; // static variable, needed for merging source phrases in
								// the caller; when glossing, index will == 0 so no merge will
								// be asked for below while in glossing mode

	// we found a target unit in the list with a matching m_key field, so we must now set
	// the static var translation to the appropriate adaptation, or gloss, text: this
	// will be the target unit's first entry in its list if the list has only one entry, else
	// we must present the user with a dialog to put up all possible adaptations (or glosses) for
	//  the user to choose one, or type a new one, or reject all - in which case we return
	// FALSE for manual typing of an adaptation (or gloss) etc. For autocapitalization support,
	// the dialog will show things as stored in the KB (if auto caps is ON, that could be with
	// all or most entries starting with lower case) and we let any needed changes to initial
	// upper case be done automatically after the user has chosen or typed.
	TranslationsList::Node *node = pTargetUnit->m_pTranslations->GetFirst();
	wxASSERT(node != NULL);
	//BEW 21Jun10, for kbVersion 2 we want a count of all non-deleted CRefString
	//instances, not the total number in the list, because there could be deletions stored
	//int count = pTargetUnit->m_pTranslations->GetCount();
	int count = pTargetUnit->CountNonDeletedRefStringInstances();
	// For kbVersion 2, a CTargetUnit can store only deleted CRefString instances, so that
	// means count can be zero - if this is the case, this is an 'unmatched' situation,
	// and should be handled the same as the if(!bFound) test above
	if (count == 0)
	{
		pCurTargetUnit = (CTargetUnit*)NULL; // the global pointer must be cleared
		curKey.Empty(); // global var curKey not needed, so clear it
		return FALSE;
	}
	wxASSERT(count > 0);
	if (count > 1 || pTargetUnit->m_bAlwaysAsk)
	{
		// move to new location, merge if necessary, so user has visual feedback about which
		// element(s) is involved
		pView->RemoveSelection();

		// set flag for telling us that we will have completed the move when the dialog is shown
		gbCompletedMergeAndMove = TRUE;

		CPile* pPile = pView->GetPile(pApp->m_nActiveSequNum);
		wxASSERT(pPile);

		// create the appropriate selection there
		CCell* pAnchorCell = pPile->GetCell(0); // always cell [0] in the refactored design

		// we do the merger here in LookAhead() rather than in the caller, such as
		// MoveToNextPile(), so that the merger can be seen by the user at the time it is
		// done (and helpful to see) rather than later
		if (nWordsInPhrase > 0)
		{
			pApp->m_pAnchor = pAnchorCell;
			CCellList* pSelection = &pApp->m_selection;
			wxASSERT(pSelection->IsEmpty());
			pApp->m_selectionLine = 1;
			wxClientDC aDC(pApp->GetMainFrame()->canvas); // make a temporary device context

			// then do the new selection, start with the anchor cell
			wxColour oldBkColor = aDC.GetTextBackground();
			aDC.SetBackgroundMode(pApp->m_backgroundMode);
			aDC.SetTextBackground(wxColour(255,255,0)); // yellow
			wxColour textColor = pAnchorCell->GetColor();
			pAnchorCell->DrawCell(&aDC,textColor);
			pApp->m_bSelectByArrowKey = FALSE;
			pAnchorCell->SetSelected(TRUE);

			// preserve record of the selection
			pSelection->Append(pAnchorCell);

			// extend the selection to the right, if more than one pile is involved
			// When glossing, the block will be skipped because nWordsInPhrase will equal 1
			if (nWordsInPhrase > 1)
			{
				// extend the selection
				pView->ExtendSelectionForFind(pAnchorCell, nWordsInPhrase);
			}
		}

		// the following code added to support Bill Martin's wish that the phrase box be shown
		// at its new location when the dialog is open (if the user cancels the dialog, the old
		// state will have to be restored - that is, any merge needs to be unmerged)
		pApp->m_pTargetBox->m_bAbandonable = FALSE;
		// adaptation is available, so use it if the source phrase has only a single word, but
		// if it's multi-worded, we must first do a merge and recalc of the layout
		gbMergeDone = FALSE; //  global, used in ChooseTranslation()
		if (nWordsInPhrase > 1) // nWordsInPhrase is a global, set in this function
								// or in LookUpSrcWord()
		{
			// do the needed merge, etc.
			pApp->bLookAheadMerge = TRUE; // set static flag to ON
			bool bSaveFlag = m_bAbandonable; // the box is "this"
			pView->MergeWords(); // calls protected OnButtonMerge() in CAdapt_ItView class
			m_bAbandonable = bSaveFlag; // preserved the flag across the merge
			pApp->bLookAheadMerge = FALSE; // restore static flag to OFF
			gbMergeDone = TRUE;
			gbSuppressMergeInMoveToNextPile = TRUE;
		}
		else
			pView->RemoveSelection(); // glossing, or adapting a single src word only

		// next code is taken from end of MoveToNextPile()
		// initialize the phrase box to be empty, so as not to confuse the user
		if (GetHandle() != NULL) // This won't happen in wx version since we don't destroy the targetbox window
		{
			// wx version note: we do the following elsewhere when we hide the m_pTargetBox
			ChangeValue(_T(""));
			pApp->m_targetPhrase = _T("");
		}

		// recalculate the layout
#ifdef _NEW_LAYOUT
		pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
#else
		pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif

		// get the new active pile
		pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
		wxASSERT(pApp->m_pActivePile != NULL);

		// scroll into view
		pApp->GetMainFrame()->canvas->ScrollIntoView(pApp->m_nActiveSequNum);

		// make what we've done visible
		pView->Invalidate();
		pLayout->PlaceBox();

		// put up a dialog for user to choose translation from a list box, or type new one
		// (note: for auto capitalization; ChooseTranslation (which calls the CChoseTranslation
		// dialog handler, only sets the 'translation' global variable, it does not make any case
		// adjustments - these, if any, must be done in the caller)

        // wx version addition: In wx the OnIdle handler continues to run even when modal
        // dialogs are being shown, so we'll save the state of m_bAutoInsert before calling
        // ChooseTranslation change m_bAutoInsert to FALSE while the dialog is being shown,
        // then restore m_bAutoInsert's prior state after ChooseTranslation returns.
        // whm update: I fixed the problem by using a derived AIModalDialog that turns off
        // idle processing while modal dialogs are being shown modal, but the following
        // doesn't hurt.
		bool saveAutoInsert = pApp->m_bAutoInsert;
		pApp->m_bAutoInsert = FALSE;

		bool bOK;
		if (gbIsGlossing)
			bOK = ChooseTranslation(TRUE); // TRUE causes Cancel And Select button to be hidden
		else
			bOK = ChooseTranslation(); // default makes Cancel And Select button visible

		// wx version: restore the state of m_bAutoInsert
		pApp->m_bAutoInsert = saveAutoInsert;

		pCurTargetUnit = (CTargetUnit*)NULL; // ensure the global var is cleared
											 //after the dialog has used it
		curKey.Empty(); // ditto for the current key string (global)
		if (!bOK)
		{
            // user cancelled, so return FALSE for a 'non-match' situation; the
            // m_bCancelAndSelectButtonPressed private bool variable (set from
            // CChooseTranslation's returned value for its m_bCancelAndSelect member) will
            // have already been set (if relevant) and can be used in the caller (ie. in
            // MoveToNextPile)
			gbCompletedMergeAndMove = FALSE;

			return FALSE;
		}
		// if bOK was TRUE, translation static var will have been set via the dialog; but
		// any needed case change to get the data ready for showing in the view will not have
		// been done within the dialog's handler code - so do them below

		// User has produced a translation from the Choose Translation dialog
		// so set the globals to the active sequence number, so we can track
		// any automatically inserted target/gloss text, starting from the
		// current phrase box location (where the Choose Translation dialog appears).

		// BEW changed 9Apr12 for support of discontinuous highlighting spans...
		// A new protocol is needed here. The legacy code wiped out any auto-insert
		// highlighting already accumulated, and setup for a possible auto-inserted span
		// starting at the NEXT pile following the merger location. The user-choice of the
		// adaptation or gloss rightly ends the previous auto-inserted span, but we want
		// that span to be visible while the ChooseTranslation dialog is open; so here we
		// do nothing, and in the caller (which usually is MoveToNextPile() we clobber the
		// old highlighted span and setup for a potential new one
#ifdef Highlighting_Bug
		// BEW changed 9Apr12 for support of discontinuous highlighting spans
		wxLogDebug(_T("PhraseBox::LookAhead(), hilited span ends at merger at: sequnum = %d  where the user chose:  %s  for source:  %s"),
			pApp->m_nActiveSequNum, translation, pApp->m_pActivePile->GetSrcPhrase()->m_srcPhrase);
#endif
	}
	else // count == 1 case (ie. a unique adaptation, or a unique gloss when glossing)
	{
        // BEW added 08Sep08 to suppress inserting placeholder translations for ... matches
        // when in glossing mode and within the end of a long retranslation
		if (curKey == _T("..."))
		{
			// don't allow an ellipsis (ie. placeholder) to trigger an insertion,
			// leave translation empty
			translation.Empty();
			return TRUE;
		}
		// BEW 21Jun10, we have to loop to find the first non-deleted CRefString instance,
		// because there may be deletions stored in the list
		CRefString* pRefStr = NULL;
		while (node != NULL)
		{
			pRefStr = node->GetData();
			node = node->GetNext();
			wxASSERT(pRefStr);
			if (!pRefStr->GetDeletedFlag())
			{
				translation = pRefStr->m_translation;
				break;
			}
		}
	}

	// adjust for case, if necessary; this algorithm will not make a lower case string start
	// with upper case when the source is uppercase if the user types punctuation at the start
	// of the string. The latter is, however, unlikely - provided the auto punctuation support
	// is utilized by the user
	if (gbAutoCaps && gbSourceIsUpperCase)
	{
		bool bNoError = pApp->GetDocument()->SetCaseParameters(translation,FALSE);
		if (bNoError && !gbNonSourceIsUpperCase && (gcharNonSrcUC != _T('\0')))
		{
			// make it upper case
			translation.SetChar(0,gcharNonSrcUC);
		}
	}
	return TRUE;
}

// BEW 13Apr10, no changes needed for support of doc version 5
// BEW 8July10, no changes needed for support of kbVersion 2
void CPhraseBox::JumpForward(CAdapt_ItView* pView)
{
	#ifdef _FIND_DELAY
		wxLogDebug(_T("9. Start of JumpForward"));
	#endif
	// refactored 25Mar09
	CLayout* pLayout = GetLayout();
	CAdapt_ItApp* pApp = pLayout->m_pApp;
	if (pApp->m_bDrafting)
	{
        // get an adjusted pile pointer for the new active location, and we want the
        // pile's strip to be marked as invalid and the strip index added to the
        // CLayout::m_invalidStripArray
		pLayout->m_pDoc->ResetPartnerPileWidth(pApp->m_pActivePile->GetSrcPhrase());

		// check the mode, whether or not it is single step, and route accordingly
		if (pApp->m_bSingleStep)
		{
			gbEnterTyped = TRUE; // try speed up by using GetSrcPhrasePos() call in
									// BuildPhrases()
			int bSuccessful;
			if (pApp->m_bTransliterationMode && !gbIsGlossing)
			{
				bSuccessful = MoveToNextPile_InTransliterationMode(pApp->m_pActivePile);
			}
			else
			{
				bSuccessful = MoveToNextPile(pApp->m_pActivePile);
			}
			if (!bSuccessful)
			{
				// BEW added 4Sep08 in support of transitioning steps within vertical
				// edit mode
				if (gbVerticalEditInProgress && gbTunnellingOut)
				{
                    // MoveToNextPile might fail within the editable span while vertical
                    // edit is in progress, so we have to allow such a failure to not cause
                    // tunnelling out; hence we use the gbTunnellingOut global to assist -
                    // it is set TRUE only when
                    // VerticalEdit_CheckForEndRequiringTransition() in the view class
                    // returns TRUE, which means that a PostMessage(() has been done to
                    // initiate a step transition
					gbTunnellingOut = FALSE; // caller has no need of it, so clear to
											 // default value
					pLayout->m_docEditOperationType = no_edit_op;
					return;
				}

                // if in vertical edit mode, the end of the doc is always an indication
                // that the edit span has been traversed and so we should force a step
                // transition, otherwise, continue to n:(tunnel out before m_nActiveSequNum
                // can be set to -1, which crashes the app at redraw of the box and recalc
                // of the layout)
				if (gbVerticalEditInProgress)
				{
					bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(
										-1, nextStep, TRUE); // bForceTransition is TRUE
					if (bCommandPosted)
					{
						// don't proceed further because the current vertical edit step
						// has ended
						pLayout->m_docEditOperationType = no_edit_op;
						return;
					}
				}

				// BEW changed 9Apr12, to support discontinuous highlighting
				// spans for auto-insertions...
				// When we come to the end, we could get there having done some
				// auto-insertions, and so we'd want them to be highlighted in the normal
				// way, and so we do nothing here -- the user can manually clear them, or
				// position the box elsewhere and initiate the lookup-and-insert loop from
				// there, in which case auto-inserts would kick off with new span(s)

				pLayout->Redraw(); // remove highlight before MessageBox call below
				pLayout->PlaceBox();

				// tell the user EOF has been reached...
				// BEW added 9Jun14, don't show this message when in clipboard adapt mode, because
				// it will come up every time a string of text is finished being adapted, and that
				// soon become a nuisance - having to click it away each time
				if (!pApp->m_bClipboardAdaptMode)
				{
					wxMessageBox(_(
"The end. Provided you have not missed anything earlier, there is nothing more to adapt in this file."),
					_T(""), wxICON_INFORMATION | wxOK);
				}
				wxStatusBar* pStatusBar;
				CMainFrame* pFrame = pApp->GetMainFrame();
				if (pFrame != NULL)
				{
					pStatusBar = pFrame->GetStatusBar();
					wxString str = _("End of the file; nothing more to adapt.");
					pStatusBar->SetStatusText(str,0); // use first field 0
				}
				// we are at EOF, so set up safe end conditions
				pApp->m_targetPhrase.Empty();
				pApp->m_nActiveSequNum = -1;
				pApp->m_pTargetBox->Hide(); // MFC version calls DestroyWindow()
				pApp->m_pTargetBox->ChangeValue(_T("")); // need to set it to null str
													  // since it won't get recreated
				pApp->m_pActivePile = NULL; // can use this as a flag for at-EOF condition too

				pView->Invalidate();
				pLayout->PlaceBox();

				translation.Empty(); // clear the static string storage for the
                    // translation save the phrase box's text, in case user hits SHIFT+END
                    // to unmerge a phrase
				gSaveTargetPhrase = pApp->m_targetPhrase;
				pLayout->m_docEditOperationType = no_edit_op;

			    RestorePhraseBoxAtDocEndSafely(pApp, pView);  // BEW added 8Sep14
				return; // must be at EOF;
			} // end of block for !bSuccessful

			// for a successful move, a scroll into view is often needed
			pLayout->m_pCanvas->ScrollIntoView(pApp->m_nActiveSequNum);

			// get an adjusted pile pointer for the new active location, and we want the
			// pile's strip to be marked as invalid and the strip index added to the
			// CLayout::m_invalidStripArray
			pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
			CSourcePhrase* pSPhr = pApp->m_pActivePile->GetSrcPhrase();
			pLayout->m_pDoc->ResetPartnerPileWidth(pSPhr);

			// try to keep the phrase box from coming too close to the bottom of
			// the client area if possible
			int yDist = pLayout->GetStripHeight() + pLayout->GetCurLeading();
			wxPoint scrollPos;
			int xPixelsPerUnit,yPixelsPerUnit;
			pLayout->m_pCanvas->GetScrollPixelsPerUnit(&xPixelsPerUnit,&yPixelsPerUnit);

            // MFC's GetScrollPosition() "gets the location in the document to which the
            // upper left corner of the view has been scrolled. It returns values in
            // logical units." wx note: The wx docs only say of GetScrollPos(), that it
            // "Returns the built-in scrollbar position." I assume this means it gets the
            // logical position of the upper left corner, but it is in scroll units which
            // need to be converted to device (pixel) units

			wxSize maxDocSize; // renamed barSizes to maxDocSize for clarity
			pLayout->m_pCanvas->GetVirtualSize(&maxDocSize.x,&maxDocSize.y);
															// gets size in pixels

			pLayout->m_pCanvas->CalcUnscrolledPosition(0,0,&scrollPos.x,&scrollPos.y);
			// the scrollPos point is now in logical pixels from the start of the doc

			wxRect rectClient(0,0,0,0);
			wxSize canvasSize;
			canvasSize = pApp->GetMainFrame()->GetCanvasClientSize(); // with
								// GetClientRect upper left coord is always (0,0)
			rectClient.width = canvasSize.x;
			rectClient.height = canvasSize.y;


			if (rectClient.GetBottom() >= 4 * yDist) // do the adjust only if at least 4
													 // strips are showing
			{
				wxPoint pt = pApp->m_pActivePile->GetCell(1)->GetTopLeft(); // logical coords
																		// of top of phrase box

				// if there are not two full strips below the top of the phrase box, scroll down
				wxASSERT(scrollPos.y + rectClient.GetBottom() >= 2*yDist);
				if (pt.y > scrollPos.y + rectClient.GetBottom() - 2*yDist)
				{
					int logicalViewBottom = scrollPos.y + rectClient.GetBottom();
																	// is always 0
					if (logicalViewBottom < maxDocSize.GetHeight())
					{
						if (logicalViewBottom <= maxDocSize.GetHeight() - yDist)
						{
							// a full strip + leading can be scrolled safely
							pApp->GetMainFrame()->canvas->ScrollDown(1);
						}
						else
						{
							// we are close to the end, but not a full strip +
							// leading can be scrolled, so just scroll enough to
							// reach the end - otherwise position of phrase box will
							// be set wrongly
							wxASSERT(maxDocSize.GetHeight() >= logicalViewBottom);
							yDist = maxDocSize.GetHeight() - logicalViewBottom;
							scrollPos.y += yDist;

							int posn = scrollPos.y;
							posn = posn / yPixelsPerUnit;
                            // Note: MFC's ScrollWindow's 2 params specify the xAmount and
                            // yAmount to scroll in device units (pixels). The equivalent
                            // in wx is Scroll(x,y) in which x and y are in SCROLL UNITS
                            // (pixels divided by pixels per unit). Also MFC's ScrollWindow
                            // takes parameters whose value represents an "amount" to
                            // scroll from the current position, whereas the
                            // wxScrolledWindow::Scroll takes parameters which represent an
                            // absolute "position" in scroll units. To convert the amount
                            // we need to add the amount to (or subtract from if negative)
                            // the logical pixel unit of the upper left point of the client
                            // viewing area; then convert to scroll units in Scroll().
							pLayout->m_pCanvas->Scroll(0,posn);
							//Refresh(); // BEW 25Mar09, this refreshes wxWindow, that is,
							//the phrase box control - but I think we need a layout draw
							//here - so I've called view's Invalidate() at a higher level
							//- see below
						}
					}
				}
			} // end of test for more than or equal to 4 strips showing

			// refresh the view
			pLayout->m_docEditOperationType = relocate_box_op;
			pView->Invalidate(); // BEW added 25Mar09, see comment about Refresh 10 lines above
			pLayout->PlaceBox();

		} // end of block for test for m_bSingleStep == TRUE
		else // auto-inserting -- sets flags and returns, allowing the idle handler to call OnePass()
		{
			// cause auto-inserting using the OnIdle handler to commence
			pApp->m_bAutoInsert = TRUE;
			gbEnterTyped = TRUE;

			// User has pressed the Enter key  (OnChar() calls JumpForward())
			// BEW changed 9Apr12, to support discontinuous highlighting
			// spans for auto-insertions...
			// Since OnIdle() will call OnePass() and the latter will call
			// MoveToNextPile(), and it is in MoveToNextPile() that CCell's
			// m_bAutoInserted flag can get set TRUE, we only here need to ensure that the
			// current location is a kick-off one, which we can do by clearing any earlier
			// highlighting currently in effect
			pLayout->ClearAutoInsertionsHighlighting();
#ifdef Highlighting_Bug
			wxLogDebug(_T("\nPhraseBox::JumpForward(), kickoff, from  pile at sequnum = %d   SrcText:  "),
				pSPhr->m_nSequNumber, pSPhr->m_srcPhrase);
#endif

			pLayout->m_docEditOperationType = relocate_box_op;
		}
		// save the phrase box's text, in case user hits SHIFT+End to unmerge a
		// phrase
		gSaveTargetPhrase = pApp->m_targetPhrase;
	} // end if for m_bDrafting
	else // we are in review mode
	{
		// we are in Review mode, so moves by the RETURN key can only be to
		// immediate next pile
		//int nOldStripIndex = pApp->m_pActivePile->m_pStrip->m_nStripIndex;

        // get an adjusted pile pointer for the new active location, and we want the
        // pile's strip to be marked as invalid and the strip index added to the
        // CLayout::m_invalidStripArray
        pApp->m_nActiveSequNum = pApp->m_pActivePile->GetSrcPhrase()->m_nSequNumber;
		pLayout->m_pDoc->ResetPartnerPileWidth(pApp->m_pActivePile->GetSrcPhrase());

        // if vertical editing is on, don't do the move to the next pile if it lies in the
        // gray area or is at the bundle end; so just check for the sequence number going
        // beyond the edit span bound & transition the step if it has, asking the user what
        // step to do next
		if (gbVerticalEditInProgress)
		{
            //gbTunnellingOut = FALSE; not needed here as once we are in caller, we've
            //tunnelled out
            //bForceTransition is FALSE in the next call
			int nNextSequNum = pApp->m_pActivePile->GetSrcPhrase()->m_nSequNumber + 1;
			bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(
																nNextSequNum,nextStep);
			if (bCommandPosted)
			{
				// don't proceed further because the current vertical edit step has ended

                // NOTE: since step transitions call mode changing handlers, and because
                // those handlers typically do a store of the phrase box contents to the kb
                // if appropriate, we'll rely on it here and not do a store
				//gbTunnellingOut = TRUE;
				pLayout->m_docEditOperationType = no_edit_op;
				return;
			}
			else
			{
				// BEW 19Oct15 No transition of vert edit modes,
				// so we can store this location on the app
				gpApp->m_vertEdit_LastActiveSequNum = pApp->m_nActiveSequNum;
#if defined(_DEBUG)
				wxLogDebug(_T("VertEdit PhrBox, JumpForward() storing loc'n: %d "), pApp->m_nActiveSequNum);
#endif
			}
		}
		if (pApp->m_bTransliterationMode)
		{
			::wxBell();
			wxMessageBox(_(
"Sorry, transliteration mode is not supported in review mode. Turn review mode off."),
			_T(""), wxICON_INFORMATION | wxOK);
			pLayout->m_docEditOperationType = no_edit_op;
			return;
		}

		// Note: transliterate mode is not supported in Review mode, so there is no
		// function such as MoveToImmedNextPile_InTransliterationMode()
		int bSuccessful = MoveToImmedNextPile(pApp->m_pActivePile);
		if (!bSuccessful)
		{
			CPile* pFwdPile = pView->GetNextPile(pApp->m_pActivePile); // old
                                // active pile pointer (should still be valid)
			//if ((!gbIsGlossing && pFwdPile->GetSrcPhrase()->m_bRetranslation) || pFwdPile == NULL)
			if (pFwdPile == NULL)
			{
				// tell the user EOF has been reached...
				// BEW added 9Jun14, don't show this message when in clipboard adapt mode, because
				// it will come up every time a string of text is finished being adapted, and that
				// soon become a nuisance - having to click it away each time
				if (!pApp->m_bClipboardAdaptMode)
				{
					wxMessageBox(_(
"The end. Provided you have not missed anything earlier, there is nothing more to adapt in this file."),
					_T(""), wxICON_INFORMATION | wxOK);
				}
				wxStatusBar* pStatusBar;
				CMainFrame* pFrame = pApp->GetMainFrame();
				if (pFrame != NULL)
				{
					pStatusBar = pFrame->GetStatusBar();
					wxString str = _("End of the file; nothing more to adapt.");
					pStatusBar->SetStatusText(str,0); // use first field 0
				}
				// we are at EOF, so set up safe end conditions
				pApp->m_pTargetBox->Hide(); // whm added 12Sep04
				pApp->m_pTargetBox->ChangeValue(_T("")); // need to set it to null
											// str since it won't get recreated
				pApp->m_pTargetBox->Enable(FALSE); // whm added 12Sep04
				pApp->m_targetPhrase.Empty();
				pApp->m_nActiveSequNum = -1;
				pApp->m_pActivePile = NULL; // can use this as a flag for
											// at-EOF condition too
				// recalc the layout without any gap for the phrase box, as it's hidden
#ifdef _NEW_LAYOUT
				pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
#else
				pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
			    RestorePhraseBoxAtDocEndSafely(pApp, pView); // BEW added 8Sep14
			}
			else // pFwdPile is valid, so must have bumped against a retranslation
			{
				wxMessageBox(_(
"Sorry, the next pile cannot be a valid active location, so no move forward was done."),
				_T(""), wxICON_INFORMATION | wxOK);
#ifdef _NEW_LAYOUT
				pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
#else
				pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
				pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
				pApp->m_pTargetBox->SetFocus();

			}
			translation.Empty(); // clear the static string storage for the translation
			// save the phrase box's text, in case user hits SHIFT+END to unmerge a phrase
			gSaveTargetPhrase = pApp->m_targetPhrase;

			// get the view redrawn
			pLayout->m_docEditOperationType = no_edit_op;
			pView->Invalidate();
			pLayout->PlaceBox();
			return;
		} // end of block for test !bSuccessful
		else
		{
			// it was successful
			CCell* pCell = pApp->m_pActivePile->GetCell(1); // the cell
											// where the phraseBox is to be

            //pView->ReDoPhraseBox(pCell); // like PlacePhraseBox, but calculations based
            //on m_targetPhrase BEW commented out above call 19Dec07, in favour of
            //RemakePhraseBox() call below. Also added test for whether document at new
            //active location has a hole there or not; if it has, we won't permit a copy of
            //the source text to fill the hole, as that would be inappropriate in Reviewing
            //mode; since m_targetPhrase already has the box text or the copied source
            //text, we must instead check the CSourcePhrase instance explicitly to see if
            //m_adaption is empty, and if so, then we force the phrase box to remain empty
            //by clearing m_targetPhrase (later, when the box is moved to the next
            //location, we must check again in MakeTargetStringIncludingPunctuation() and
            //restore the earlier state when the phrase box is moved on)

			//CSourcePhrase* pSPhr = pCell->m_pPile->m_pSrcPhrase;
			CSourcePhrase* pSPhr = pCell->GetPile()->GetSrcPhrase();
			wxASSERT(pSPhr != NULL);

			// get an adjusted pile pointer for the new active location, and we want the
			// pile's strip to be marked as invalid and the strip index added to the
			// CLayout::m_invalidStripArray
			pApp->m_nActiveSequNum = pApp->m_pActivePile->GetSrcPhrase()->m_nSequNumber;
			pLayout->m_pDoc->ResetPartnerPileWidth(pSPhr);
			
			// BEW 19Oct15 No transition of vert edit modes,
			// so we can store this location on the app, provided
			// we are in bounds
			if (gbVerticalEditInProgress)
			{
				if (gEditRecord.nAdaptationStep_StartingSequNum <= pApp->m_nActiveSequNum &&
					gEditRecord.nAdaptationStep_EndingSequNum >= pApp->m_nActiveSequNum)
				{
					// BEW 19Oct15, store new active loc'n on app
					gpApp->m_vertEdit_LastActiveSequNum = pApp->m_nActiveSequNum;
#if defined(_DEBUG)
				wxLogDebug(_T("VertEdit PhrBox, JumpForward() storing loc'n: %d "), pApp->m_nActiveSequNum);
#endif
				}
			}
			
			if (pSPhr->m_targetStr.IsEmpty() || pSPhr->m_adaption.IsEmpty())
			{
				// no text or punctuation, or no text and punctuation not yet placed,
				// or no text and punctuation was earlier placed -- whichever is the case
				// we need to preserve that state
				pApp->m_targetPhrase.Empty();
				gbSavedTargetStringWithPunctInReviewingMode = TRUE; // it gets cleared again
						// within MakeTargetStringIncludingPunctuation() at the end the block
						// it is being used in there
				gStrSavedTargetStringWithPunctInReviewingMode = pSPhr->m_targetStr; // cleared
						// again within MakeTargetStringIncludingPunctuation() at the end of
						// the block it is being used in there
			}
			// if neither test succeeds, then let
			// m_targetPhrase contents stand unchanged

			gbEnterTyped = FALSE;

			pLayout->m_docEditOperationType = relocate_box_op;
		} // end of block for bSuccessful == TRUE

		// save the phrase box's text, in case user hits SHIFT+END to unmerge a phrase
		gSaveTargetPhrase = pApp->m_targetPhrase;

#ifdef _NEW_LAYOUT
		#ifdef _FIND_DELAY
			wxLogDebug(_T("10. Start of RecalcLayout in JumpForward"));
		#endif
		pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
		#ifdef _FIND_DELAY
			wxLogDebug(_T("11. End of RecalcLayout in JumpForward"));
		#endif
#else
		pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
		pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
		wxASSERT(pApp->m_pActivePile != NULL);

		pLayout->m_pCanvas->ScrollIntoView(pApp->m_nActiveSequNum);

		pView->Invalidate();
		pLayout->PlaceBox();
	} // end Review mode (single src phrase move) block
	#ifdef _FIND_DELAY
		wxLogDebug(_T("12. End of JumpForward"));
	#endif
}

// internally calls CPhraseBox::FixBox() to see if a resizing is needed; this function
// is called for every character typed in phrase box (via OnChar() function which is
// called for every EVT_TEXT posted to event queue); it is not called for selector = 2
// value because OnChar() has a case statement that handles box contraction there, and for
// selector = 1 this function is only called elsewhere in the app within OnEditUndo()
// BEW 13Apr10, no changes needed for support of doc version 5
void CPhraseBox::OnPhraseBoxChanged(wxCommandEvent& WXUNUSED(event))
{
	// whm Note: This phrasebox handler is necessary in the wxWidgets version because the
	// OnChar() handler does not have access to the changed value of the new string
	// within the control reflecting the keystroke that triggers OnChar(). Because
	// of that difference in behavior, I moved the code dependent on updating
	// pApp->m_targetPhrase from OnChar() to this OnPhraseBoxChanged() handler.
	CAdapt_ItApp* pApp = (CAdapt_ItApp*)&wxGetApp();	if (this->IsModified())
	{
		CAdapt_ItView* pView = (CAdapt_ItView*) pApp->GetView();
		// preserve cursor location, in case we merge, so we can restore it afterwards
		long nStartChar;
		long nEndChar;
		GetSelection(&nStartChar,&nEndChar);

		wxPoint ptNew;
		wxRect rectClient;
		wxSize textExtent;
		wxString thePhrase; // moved to OnPhraseBoxChanged()

		// update status bar with project name
		pApp->RefreshStatusBarInfo();

		// restore the cursor position...
        // BEW added 05Oct06; to support the alternative algorithm for setting up the
        // phrase box text, and positioning the cursor, when the selection was done
        // leftwards from the active location... that is, since the cursor position for
        // leftwards selection merges is determined within OnButtonMerge() then we don't
        // here want to let the values stored at the start of OnChar() clobber what
        // OnButtonMerge() has already done - so we have a test to determine when to
        // suppress the cursor setting call below in this new circumstance
		if (!(gbMergeSucceeded && pApp->m_curDirection == toleft))
		{
			SetSelection(nStartChar,nEndChar);
			pApp->m_nStartChar = nStartChar;
			pApp->m_nEndChar = nEndChar;
		}

        // whm Note: Because of differences in the handling of events, in wxWidgets the
        // GetValue() call below retrieves the contents of the phrasebox after the
        // keystroke and so it includes the keyed character. OnChar() is processed before
        // OnPhraseBoxChanged(), and in that handler the key typed is not accessible.
        // Getting it here, therefore, is the only way to get it after the character has
        // been added to the box. This is in contrast to the MFC version where
        // GetWindowText(thePhrase) at the same code location in PhraseBox::OnChar there
        // gets the contents of the phrasebox including the just typed character.
		thePhrase = GetValue(); // current box text (including the character just typed)

		// BEW 6Jul09, try moving the auto-caps code from OnIdle() to here
		if (gbAutoCaps && pApp->m_pActivePile != NULL)
		{
			wxString str;
			CSourcePhrase* pSrcPhrase = pApp->m_pActivePile->GetSrcPhrase();
			wxASSERT(pSrcPhrase != NULL);
			bool bNoError = pApp->GetDocument()->SetCaseParameters(pSrcPhrase->m_key);
			if (bNoError && gbSourceIsUpperCase)
			{
				// a change of case might be called for... first
				// make sure the box exists and is visible before proceeding further
				if (pApp->m_pTargetBox != NULL && (pApp->m_pTargetBox->IsShown()))
				{
					// get the string currently in the phrasebox
					//str = pApp->m_pTargetBox->GetValue();
					str = thePhrase;

					// do the case adjustment only after the first character has been
					// typed, and be sure to replace the cursor afterwards
					int len = str.Length();
					if (len != 1)
					{
						; // don't do anything to first character if string has more than 1 char
					}
					else
					{
						// set cursor offsets
						int nStart = 1; int nEnd = 1;

						// check out its case status
						bNoError = pApp->GetDocument()->SetCaseParameters(str,FALSE); // FALSE is value for bIsSrcText

						// change to upper case if required
						if (bNoError && !gbNonSourceIsUpperCase && (gcharNonSrcUC != _T('\0')))
						{
							str.SetChar(0,gcharNonSrcUC);
							pApp->m_pTargetBox->ChangeValue(str);
							pApp->m_pTargetBox->Refresh();
							//pApp->m_targetPhrase = str;
							thePhrase = str;

							// fix the cursor location
							pApp->m_pTargetBox->SetSelection(nStart,nEnd);
						}
					}
				}
			}
		}

		// update m_targetPhrase to agree with what has been typed so far
		pApp->m_targetPhrase = thePhrase;

		bool bWasMadeDirty = FALSE;

		// whm Note: here we can eliminate the test for Return, BackSpace and Tab
		pApp->m_bUserTypedSomething = TRUE;
		pView->GetDocument()->Modify(TRUE);
		pApp->m_pTargetBox->m_bAbandonable = FALSE; // once we type something, it's not
												// considered abandonable
		gbByCopyOnly = FALSE; // even if copied, typing something makes it different so set
							// this flag FALSE
		MarkDirty();

		// adjust box size
		FixBox(pView, thePhrase, bWasMadeDirty, textExtent, 0); // selector = 0 for incrementing box extent

		// set the globals for the cursor location, ie. m_nStartChar & m_nEndChar,
		// ready for box display
		GetSelection(&pApp->m_nStartChar, &pApp->m_nEndChar);

		// save the phrase box's text, in case user hits SHIFT+END to unmerge a phrase
		gSaveTargetPhrase = pApp->m_targetPhrase;

		// scroll into view if necessary
		GetLayout()->m_pCanvas->ScrollIntoView(pApp->m_nActiveSequNum);

		GetLayout()->m_docEditOperationType = no_edit_op;

		// BEW 02May2016, thePhrase gets leaked, so clear it here
		thePhrase.clear();
	}
}

// FixBox() is the core function for supporting box expansion and contraction in various
// situations, especially when typing into the box; this version detects when adjustment to
// the layout is required, it calls CLayout::RecalcLayout() to tweak the strips at the
// active location - that is with input parameter keep_strips_keep_piles. FixBox() is
// currently called only in the following CPhraseBox functions: OnPhraseBoxChanged() with
// selector 0 passed in; OnChar() for backspace keypress, with selector 2 passed in;
// OnEditUndo() with selector 1 passed in, and (BEW added 14Mar11) OnDraw() of
// CAdapt_ItView.
// refactored 26Mar09
// BEW 13Apr10, no changes needed for support of doc version 5
// BEW 22Jun10, no changes needed for support of kbVersion 2
void CPhraseBox::FixBox(CAdapt_ItView* pView, wxString& thePhrase, bool bWasMadeDirty,
						wxSize& textExtent,int nSelector)
{
    // destroys the phrase box and recreates it with a different size, depending on the
    // nSelector value.
    // nSelector == 0, increment the box width using its earlier value
    // nSelector == 1, recalculate the box width using the input string extent
    // nSelector == 2, backspace was typed, so box may be contracting
    // This new version tries to be smarter for deleting text from the box, so that we
    // don't recalculate the layout for the whole screen on each press of the backspace key
    // - to reduce the blinking effect
    // version 2.0 takes text extent of m_gloss into consideration, if gbIsGlossing is TRUE

	// Note: the refactored design of this function is greatly simplified by the fact that
	// in the new layout system, the TopLeft location for the resized phrase box is
	// obtained directly from the layout after the layout has been recalculated. To
	// recalculate the layout correctly, all the new FixBox() needs to do is:
	// (1) work out if a RecalcLayout(), or other strip tweaking call, is needed, and
	// (2) prior to making the RecalcLayout() call, or other strip tweaking call, the new
	// phrase box width must be calculated and stored in CLayout::m_curBoxWidth so that
	// RecalcLayout() will have access to it when it is setting the width of the active pile.

	//GDLC Added 2010-02-09
	enum phraseBoxWidthAdjustMode nPhraseBoxWidthAdjustMode = steadyAsSheGoes;

	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	//bool bWholeScreenUpdate = TRUE; // suppress for now, we'll probably do it
	//differently (such as with a clipping rectangle, and may not calculate that here anyway)
	CLayout* pLayout = GetLayout();

	// ensure the active pile is up to date
	pApp->m_pActivePile = pLayout->m_pView->GetPile(pApp->m_nActiveSequNum);
	wxASSERT(pApp->m_pActivePile);

	//wxSize currBoxSize(pApp->m_curBoxWidth,pApp->m_nTgtHeight);
	wxSize sizePhraseBox = GetClientSize(); // BEW added 25Mar09; it's in pixels
	wxSize currBoxSize(pLayout->m_curBoxWidth, sizePhraseBox.y); // note, this is better, because if
						// glossing mode is on and glossing uses the navText font, the height
						// might be different from the height of the target text font

	wxClientDC dC(this);
	wxFont* pFont;
	if (gbIsGlossing && gbGlossingUsesNavFont)
		pFont = pApp->m_pNavTextFont;
	else
		pFont = pApp->m_pTargetFont;
	wxFont SaveFont = dC.GetFont();

	dC.SetFont(*pFont);
	dC.GetTextExtent(thePhrase, &textExtent.x, &textExtent.y); // measure using the current font
	wxSize textExtent2;
	wxSize textExtent3;
	wxSize textExtent4; // version 2.0 and onwards, for extent of m_gloss
	//CStrip* pPrevStrip;

	// if close to the right boundary, then recreate the box larger (by 3 chars of width 'w')
	wxString aChar = _T('w');
	wxSize charSize;
	dC.GetTextExtent(aChar, &charSize.x, &charSize.y);
	bool bResult;
	if (nSelector < 2)
	{
		// when either incrementing the box width from what it was, or recalculating a
		// width using m_targetPhrase contents, generate TRUE if the horizontal extent of
		// the text in it is greater or equal to the current box width less 3 'w' widths
		// (if so, an expansion is required, if not, current size can stand)
		bResult = textExtent.x >= currBoxSize.x - gnNearEndFactor*charSize.x;
	}
	else
	{
        // when potentially about to contract the box, generate TRUE if the horizontal
        // extent of the text in it is less than or equal to the current box width less 4
        // 'w' widths (if so, a contraction is required, if not, current size can stand)
		// BEW changed 25Jun05, the above criterion produced very frequent resizing; let's
		// do it far less often...
		//bResult = textExtent.x <= currBoxSize.x - (4*charSize.x); // the too-often way
		bResult = textExtent.x < currBoxSize.x - (8*charSize.x);
	}
	bool bUpdateOfLayoutNeeded = FALSE;
	if (bResult )
	{
		// a width change is required....therefore set m_curBoxWidth and call RecalcLayout()
		if (nSelector < 2)
			nPhraseBoxWidthAdjustMode = expanding; // this is passed on to the functions that
								// calculate the new width of the phrase box

		// make sure the activeSequNum is set correctly, we need it to be able
		// to restore the pActivePile pointer after the layout is recalculated
		//
		// BEW removed 26Mar09 because the m_nActiveSequNum variable is to be trusted over
		// the m_pActivePile - just in case our refactored code forgets to set the latter
		// at some point; but our code won't work if the former is ever wrong - we'd see
		// the muck up in the display of the layout immediately!
		//pApp->m_nActiveSequNum = pApp->m_pActivePile->GetSrcPhrase()->m_nSequNumber;

		// calculate the new width
		if (nSelector == 0)
		{
			pLayout->m_curBoxWidth += gnExpandBox*charSize.x;

			GetSelection(&pApp->m_nStartChar,&pApp->m_nEndChar); // store current phrase
					// box selection in app's m_nStartChar & m_nEndChar members

			bUpdateOfLayoutNeeded = TRUE;
		}
		else // next block is for nSelector == 1 or 2 cases
		{
			if (nSelector == 2)
			{
				// backspace was typed, box may be about to contract

				pApp->m_targetPhrase = GetValue(); // store current typed string

				//move old code into here & then modify it
				GetSelection(&pApp->m_nStartChar,&pApp->m_nEndChar); // store current selection

				// we are trying to delete text in the phrase box by pressing backspace key
				// shrink the box by 2 'w' widths if the space at end is >= 4 'w' widths
				// BEW changed 25Jun09, to have the box shrink done less often to reduce blinking,
				// the new criterion will shrink the box by 7 'w' widths -- no make it
				// just 5 w widths (26Jun09)
				//int newWidth = pLayout->m_curBoxWidth - 2 * charSize.x;
				//int newWidth = pLayout->m_curBoxWidth - 7 * charSize.x;
				int newWidth = pLayout->m_curBoxWidth - 5 * charSize.x;
				// we have to compare with a reasonable box width based on source text
				// width to ensure we don't reduce the width below that (otherwise piles
				// to the right will encroach over the end of the active location's source
				// text)
				wxString srcPhrase = pApp->m_pActivePile->GetSrcPhrase()->m_srcPhrase;
				wxFont* pSrcFont = pApp->m_pSourceFont;
				wxSize sourceExtent;
				dC.SetFont(*pSrcFont);
				dC.GetTextExtent(srcPhrase, &sourceExtent.x, &sourceExtent.y);
				int minWidth = sourceExtent.x + gnExpandBox*charSize.x;
				if (newWidth <= minWidth)
				{
					// we are contracting too much, so set to minWidth instead
					pLayout->m_curBoxWidth = minWidth;
					//GDLC 2010-02-09
					nPhraseBoxWidthAdjustMode = steadyAsSheGoes;
				}
				else
				{
					// newWidth is larger than minWidth, so we can do the full contraction
					pLayout->m_curBoxWidth = newWidth;
					//GDLC 2010-02-09
					nPhraseBoxWidthAdjustMode = contracting;
				}
				//GDLC I think that the normal SetPhraseBoxGapWidth() should be called with
				// nPhraseBoxWidthAdjustmentModepassed to it as a parameter instead of simply
				// using newWidth.
				pApp->m_pActivePile->SetPhraseBoxGapWidth(newWidth); // sets m_nWidth to newWidth
                // The gbContracting flag used above? RecalcLayout() will override
                // m_curBoxWidth if we leave this flag FALSE; setting it makes the
                // ResetPartnerPileWidth() call within RecalcLayout() not do an active pile
                // gap width calculation that otherwise sets the box width too wide and the
                // backspaces then done contract the width of the phrase box not as much as
                // expected (the RecalcLayout() call clears gbContracting after using it)
				bUpdateOfLayoutNeeded = TRUE;
			} // end block for nSelector == 2 case
			else
			{
				// nSelector == 1 case
				pLayout->m_curBoxWidth = textExtent.x + gnExpandBox*charSize.x;

				// move the old code into here
				GetSelection(&pApp->m_nStartChar,&pApp->m_nEndChar); // store current selection

				bUpdateOfLayoutNeeded = TRUE;
			} // end block for nSelector == 1 case
		} // end nSelector != 0 block

		if (bUpdateOfLayoutNeeded)
		{
#ifdef _NEW_LAYOUT
			pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles, nPhraseBoxWidthAdjustMode);
#else
			pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
			pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
			wxASSERT(pApp->m_pActivePile != NULL);
		}

		// resize using the stored information (version 1.2.3 and earlier used to recreate, but
		// this conflicted with Keyman (which sends backspace sequences to delete matched char
		// sequences to be converted, so I changed to a resize for version 1.2.4 and onwards.
		// Everything seems fine.
		// wx version: In the MFC version there was a CreateBox function as well as a ResizeBox
		// function used here. I have simplified the code to use ResizeBox everywhere, since
		// the legacy CreateBox now no longer recreates the phrasebox each time it's called.

		wxPoint ptCurBoxLocation;
		CCell* pActiveCell = pApp->m_pActivePile->GetCell(1);
		pActiveCell->TopLeft(ptCurBoxLocation); // returns the .x and .y values in the signature's ref variable
		// BEW 25Dec14, cells are 2 pixels larger vertically as of today, so move TopLeft of box
		// back up by 2 pixels, so text baseline keeps aligned
		ptCurBoxLocation.y -= 2;

		if (gbIsGlossing && gbGlossingUsesNavFont)
		{
			pView->ResizeBox(&ptCurBoxLocation, pLayout->m_curBoxWidth, pLayout->GetNavTextHeight(),
				pApp->m_targetPhrase, pApp->m_nStartChar, pApp->m_nEndChar, pApp->m_pActivePile);
		}
		else
		{
			pView->ResizeBox(&ptCurBoxLocation, pLayout->m_curBoxWidth, pLayout->GetTgtTextHeight(),
				pApp->m_targetPhrase, pApp->m_nStartChar, pApp->m_nEndChar, pApp->m_pActivePile);
		}
		if (bWasMadeDirty)
			pApp->m_pTargetBox->MarkDirty(); // TRUE (restore modified status)

		//GDLC Removed 2010-02-09
		//gbExpanding = FALSE;
//#ifdef Do_Clipping
//		// support clipping
//		if (!bUpdateOfLayoutNeeded)
//			pLayout->SetAllowClippingFlag(TRUE); // flag is turned off again at end of Draw()
//#endif
	} // end bResult == TRUE block
	else
	{
//#ifdef Do_Clipping
//		// no reason to change box size, so we should be able to support clipping
//		// (provided no scroll is happening - but that is deal with elsewhere, search for
//		// SetScrollingFlag() to find where)
//		pLayout->SetAllowClippingFlag(TRUE); // flag is turned off again at end of Draw()
//#endif
	}
	if (nSelector < 2)
		pApp->m_targetPhrase = thePhrase; // update the string storage on the view
			// (do it here rather than before the resizing code else selection bounds are wrong)

	dC.SetFont(SaveFont); // restore old font (ie "System")
	gSaveTargetPhrase = pApp->m_targetPhrase;

	// whm Note re BEW's note below: the non-visible phrasebox was not a problem in the wx version.
	// BEW added 20Dec07: in Reviewing mode, the box does not always get drawn (eg. if click on a
	// strip which is all holes and then advance the box by using Enter key, the box remains invisible,
	// and stays so for subsequent Enter presses in later holes in the same and following strips:
	// same addition is at the end of the ResizeBox() function, for the same reason
	//pView->m_targetBox.Invalidate(); // hopefully this will fix it - it doesn't unfortunately
	 // perhaps the box paint occurs too early and the view point wipes it. How then do we delay
	 // the box paint? Maybe put the invalidate call into the View's OnDraw() at the end of its handler?
	//pView->RemakePhraseBox(pView->m_pActivePile, pView->m_targetPhrase); // also doesn't work.
}

// MFC docs say about CWnd::OnChar "The framework calls this member function when
// a keystroke translates to a nonsystem character. This function is called before
// the OnKeyUp member function and after the OnKeyDown member function are called.
// OnChar contains the value of the keyboard key being pressed or released. This
// member function is called by the framework to allow your application to handle
// a Windows message. The parameters passed to your function reflect the parameters
// received by the framework when the message was received. If you call the
// base-class implementation of this function, that implementation will use the
// parameters originally passed with the message and not the parameters you supply
// to the function."
// Hence the calling order in MFC is OnKeyDown, OnChar, OnKeyUp.
// The wxWidgets docs say about wxKeyEvent "Notice that there are three different
// kinds of keyboard events in wxWidgets: key down and up events and char events.
// The difference between the first two is clear - the first corresponds to a key
// press and the second to a key release - otherwise they are identical. Just note
// that if the key is maintained in a pressed state you will typically get a lot
// of (automatically generated) down events but only one up so it is wrong to
// assume that there is one up event corresponding to each down one. Both key
// events provide untranslated key codes while the char event carries the
// translated one. The untranslated code for alphanumeric keys is always an upper
// case value. For the other keys it is one of WXK_XXX values from the keycodes
// table. The translated key is, in general, the character the user expects to
// appear as the result of the key combination when typing the text into a text
// entry zone, for example.

// OnChar is called via EVT_CHAR(OnChar) in our CPhraseBox Event Table.
// BEW 13Apr10, no changes needed for support of doc version 5
// BEW 22Jun10, no changes needed for support of kbVersion 2
void CPhraseBox::OnChar(wxKeyEvent& event)
{
	// whm Note: OnChar() is called before OnPhraseBoxChanged()
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	CLayout* pLayout = GetLayout();
	CAdapt_ItView* pView = pLayout->m_pView;
	wxASSERT(pView->IsKindOf(CLASSINFO(CAdapt_ItView)));

	// wx version note: In MFC this OnChar() function is NOT called for system key events,
	// however in the wx version it IS called for combination system key events such as
	// ALT+Arrow-key. We will immediately return if the CTRL or ALT key is down; Under
	// wx's wxKeyEvent we can test CTRL or ALT down with HasModifiers(), and we must also
	// prevent any arrow keys.
	if (event.HasModifiers() || event.GetKeyCode() == WXK_DOWN || event.GetKeyCode() == WXK_UP
		|| event.GetKeyCode() == WXK_LEFT || event.GetKeyCode() == WXK_RIGHT)
	{
		event.Skip(); // to allow OnKeyUp() to handle the event
		return;
	}

	// BEW added 25Sep13 to kill the "Non-sticking / Truncation bug" - where a Save and
	// then typing more in the phrase box, when user hit Enter to advance the box, the
	// string typed was not shown in the layout, but rather whatever was there in the box
	// at the Save (or Auto-save). The app variable, m_nPlacePunctDlgCallNumber, which
	// starts as 0, gets augmented each time MakeTargetStringIncludingPunction() is called
	// and if the value goes over 1, none of the code for setting m_targetStr within that
	// function gets called. The Save got the variable set to 1, and the advance of the
	// box gives another call of the function - to 2, and hence the Non-stick bug happens.
	// The solution is to clear m_nPlacePunctDlgCallNumber to 0 on every keystroke in the
	// phrasebox - so that's what we do here, and the non-stick bug is history
	pApp->m_nPlacePunctDlgCallNumber = 0; // initialize on every keystroke

#ifdef Do_Clipping
	//wxLogDebug(_T("In OnChar), ** KEY TYPED **"));
#endif

	m_bMergeWasDone = FALSE; //bool bMergeWasDone = FALSE;
	gbEnterTyped = FALSE;

	// whm Note: The following code for handling the WXK_BACK key is ok to leave here in
	// the OnChar() handler, because it is placed before the Skip() call (the OnChar() base
	// class call in MFC)

	GetSelection(&gnSaveStart,&gnSaveEnd);

    // MFC Note: CEdit's Undo() function does not undo a backspace deletion of a selection
    // or single char, so implement that here & in an override for OnEditUndo();
	if (event.GetKeyCode() == WXK_BACK)
	{
		m_backspaceUndoStr.Empty();
		wxString s;
		s = GetValue();
		if (gnSaveStart == gnSaveEnd)
		{
			// deleting previous character
			if (gnSaveStart > 0)
			{
				int index = gnSaveStart;
				index--;
				gnSaveStart = index;
				gnSaveEnd = index;
				wxChar ch = s.GetChar(index);
				m_backspaceUndoStr += ch;
			}
			else // gnSaveStart must be zero, as when the box is empty
			{
                // BEW added 20June06, because if the CSourcePhrase has m_bHasKBEntry TRUE,
                // but an empty m_adaption (because the user specified <no adaptation>
                // there earlier) then there was no way in earlier versions to "remove" the
                // <no adaptation> - so we want to allow a backspace keypress, with the box
                // empty, to effect the clearing of m_bHasKBEntry to FALSE (or if glossing,
                // m_bHasGlossingKBEntry to FALSE), and to decement the <no adaptation> ref
				// count or remove the entry entirely if the count is 1 from the KB entry
                // (BEW 22Jun10, for kbVersion 2, "remove" now means, "set m_bDeleted to
                // TRUE" for the CRefString instance storing the <Not In KB> entry)
				if (pApp->m_pActivePile->GetSrcPhrase()->m_adaption.IsEmpty() &&
					((pApp->m_pActivePile->GetSrcPhrase()->m_bHasKBEntry && !gbIsGlossing) ||
					(pApp->m_pActivePile->GetSrcPhrase()->m_bHasGlossingKBEntry && gbIsGlossing)))
				{
					gbNoAdaptationRemovalRequested = TRUE;
					CSourcePhrase* pSrcPhrase = pApp->m_pActivePile->GetSrcPhrase();
					wxString emptyStr = _T("");
					pApp->m_pKB->GetAndRemoveRefString(pSrcPhrase,emptyStr,useGlossOrAdaptationForLookup);
				}
			}
		}
		else
		{
			// deleting a selection
			int count = gnSaveEnd - gnSaveStart;
			if (count > 0)
			{
				m_backspaceUndoStr = s.Mid(gnSaveStart,count);
				gnSaveEnd = gnSaveStart;
			}
		}
	}

	//GDLC Removed 2010-02-09
	//gbExpanding = FALSE;

	// wxWidgets Note: The wxTextCtrl does not have a virtual OnChar() method,
	// so we'll just call .Skip() for any special handling of the WXK_RETURN and WXK_TAB
	// key events. In wxWidgets, calling event.Skip() is analagous to calling
	// the base class version of a virtual function. Note: wxTextCtrl has
	// a non-virtual OnChar() method. See "wxTextCtrl OnChar event handling.txt"
	// for a newsgroup sample describing how to use OnChar() to do "auto-
	// completion" while a user types text into a text ctrl.
	if (!(event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_TAB)) // I don't
											// want a bell when RET key or HT key typed
	{
        // whm note: Instead of explicitly calling the OnChar() function in the base class
        // (as would normally be done for C++ virtual functions), in wxWidgets, we call
        // Skip() instead, for the event to be processed either in the base wxWidgets class
        // or the native control. This difference in event handling also means that part of
        // the code in OnChar() that can be handled correctly in the MFC version, must be
        // moved to our wx-specific OnPhraseBoxChanged() handler. This is necessitated
        // because GetValue() does not get the new value of the control's contents when
        // called from within OnChar, but received the previous value of the string as it
        // existed before the keystroke that triggers OnChar.
		event.Skip();
	}

	// preserve cursor location, in case we merge, so we can restore it afterwards
	long nStartChar;
	long nEndChar;
	GetSelection(&nStartChar,&nEndChar);

    // whm Note: See note below about meeding to move some code from OnChar() to the
    // OnPhraseBoxChanged() handler in the wx version, because the OnChar() handler does
    // not have access to the changed value of the new string within the control reflecting
    // the keystroke that triggers OnChar().
	//
	wxSize textExtent;
	pApp->RefreshStatusBarInfo();

	// if there is a selection, and user forgets to make the phrase before typing, then do it
	// for him on the first character typed. But if glossing, no merges are allowed.

	int theCount = pApp->m_selection.GetCount();
	if (!gbIsGlossing && theCount > 1 && (pApp->m_pActivePile == pApp->m_pAnchor->GetPile()
		|| IsActiveLocWithinSelection(pView, pApp->m_pActivePile)))
	{
		if (pView->GetSelectionWordCount() > MAX_WORDS)
		{
			pApp->GetRetranslation()->DoRetranslation();
		}
		else
		{
			if (!pApp->m_bUserTypedSomething &&
				!pApp->m_pActivePile->GetSrcPhrase()->m_targetStr.IsEmpty())
			{
				bSuppressDefaultAdaptation = FALSE; // we want what is already there
			}
			else
			{
				// for version 1.4.2 and onwards, we will want to check gbRetainBoxContents
				// and two other flags, for a click or arrow key press is meant to allow
				// the deselected source word already in the phrasebox to be retained; so we
				// here want the bSuppressDefaultAdaptation flag set TRUE only when the
				// gbRetainBoxContents is FALSE (- though we use two other flags too to
				// ensure we get this behaviour only when we want it)
				if (gbRetainBoxContents && !m_bAbandonable && pApp->m_bUserTypedSomething)
				{
					bSuppressDefaultAdaptation = FALSE;
				}
				else
				{
					bSuppressDefaultAdaptation = TRUE; // the global BOOLEAN used for temporary
													   // suppression only
				}
			}
			pView->MergeWords(); // simply calls OnButtonMerge
			m_bMergeWasDone = TRUE;
			pLayout->m_docEditOperationType = merge_op;
			bSuppressDefaultAdaptation = FALSE;

			// we can assume what the user typed, provided it is a letter, replaces what was
			// merged together, but if tab or return was typed, we allow the merged text to
			// remain intact & pass the character on to the switch below; but since v1.4.2 we
			// can only assume this when gbRetainBoxContents is FALSE, if it is TRUE and
			// a merge was done, then there is probably more than what was just typed, so we
			// retain that instead; also, we we have just returned from a MergeWords( ) call in
			// which the phrasebox has been set up with correct text (such as previous target text
			// plus the last character typed for an extra merge when a selection was present, we
			// don't want this wiped out and have only the last character typed inserted instead,
			// so in OnButtonMerge( ) we test the phrasebox's string and if it has more than just
			// the last character typed, we assume we want to keep the other content - and so there
			// we also set gbRetainBoxContents
			if (gbRetainBoxContents && m_bMergeWasDone)
			{
				; // do nothing - note, exiting via here leaves the cursor after whatever
				// character the user typed, so if that was a hyphen then the cursor will
				// be preceding the concatenating space which usually the user will then
				// want to delete; so leave the cursor location unchanged as this
				// fortuitous location is precisely where we would want to be
			}
			gbRetainBoxContents = FALSE; // turn it back off (default) until next required
		}
	}
	else
	{
        // if there is a selection, but the anchor is removed from the active location, we
        // don't want to make a phrase elsewhere, so just remove the selection. Or if
        // glossing, just silently remove the selection - that should be sufficient alert
        // to the user that the merge operation is unavailable
		pView->RemoveSelection();
		wxClientDC dC(pLayout->m_pCanvas);
		pView->canvas->DoPrepareDC(dC); // adjust origin
		pApp->GetMainFrame()->PrepareDC(dC); // wxWidgets' drawing.cpp sample also calls
											 // PrepareDC on the owning frame
		pLayout->m_docEditOperationType = no_edit_op;
		pView->Invalidate();
	}

    // whm Note: The following code is moved to the OnPhraseBoxChanged() handler in the wx
    // version, because the OnChar() handler does not have access to the changed value of
    // the new string within the control reflecting the keystroke that triggers OnChar().
    // Because of that difference in behavior, I moved the code dependent on updating
    // pApp->m_targetPhrase from OnChar() to the OnPhraseBoxChanged() handler.

	long keycode = event.GetKeyCode();
	switch(keycode)
	{
	case WXK_RETURN: //13:	// RETURN key
		{
			// save old sequ number in case required for toolbar's Back button
			gnOldSequNum = pApp->m_nActiveSequNum;

			// whm Note: Beware! Setting breakpoints in OnChar() before this point can
			// affect wxGetKeyState() results making it appear that WXK_SHIFT is not detected
			// below. Solution: remove the breakpoint(s) for wxGetKeyState(WXK_SHIFT) to
			// register properly.
			if (wxGetKeyState(WXK_SHIFT))
			{
				// shift key is down, so move back a pile

				int bSuccessful = MoveToPrevPile(pApp->m_pActivePile);
				if (!bSuccessful)
				{
					// we were at the start of the document, so do nothing
					;
				}
				else
				{
					// it was successful
					pLayout->m_docEditOperationType = relocate_box_op;
					gbEnterTyped = FALSE;
				}

				pApp->GetMainFrame()->canvas->ScrollIntoView(pApp->m_nActiveSequNum);

				// save the phrase box's text, in case user hits SHIFT+End to unmerge a phrase
				gSaveTargetPhrase = pApp->m_targetPhrase;
				return;
			}
			else // we are moving forwards rather than backwards
			{
				JumpForward(pView);
			}
		} // end case 13: block
		return;
	case WXK_TAB: //9:		// TAB key
		{
			// save old sequ number in case required for toolbar's Back button
			gnOldSequNum = pApp->m_nActiveSequNum;

			// SHIFT+TAB is the 'universal' keyboard way to cause a move back, so implement it
			// whm Note: Beware! Setting breakpoints in OnChar() before this point can
			// affect wxGetKeyState() results making it appear that WXK_SHIFT is not detected
			// below. Solution: remove the breakpoint(s) for wxGetKeyState(WXK_SHIFT) to
			if (wxGetKeyState(WXK_SHIFT))
			{
				// shift key is down, so move back a pile

				// Shift+Tab (reverse direction) indicates user is probably
				// backing up to correct something that was perhaps automatically
				// inserted, so we will preserve any highlighting and do nothing
				// here in response to Shift+Tab.

				Freeze();

				int bSuccessful = MoveToPrevPile(pApp->m_pActivePile);
				if (!bSuccessful)
				{
					// we have come to the start of the document, so do nothing
					pLayout->m_docEditOperationType = no_edit_op;
				}
				else
				{
					// it was successful
					gbEnterTyped = FALSE;
					pLayout->m_docEditOperationType = relocate_box_op;
				}

				// scroll, if necessary
				pApp->GetMainFrame()->canvas->ScrollIntoView(pApp->m_nActiveSequNum);

				// save the phrase box's text, in case user hits SHIFT+END key to unmerge
				// a phrase
				gSaveTargetPhrase = pApp->m_targetPhrase;

				Thaw();
				return;
			}
			else
			{
				//BEW changed 01Aug05. Some users are familiar with using TAB key to advance
				// (especially when working with databases), and without thinking do so in Adapt It
				// and expect the Lookup process to take place, etc - and then get quite disturbed
				// when it doesn't happen that way. So for version 3 and onwards, we will interpret
				// a TAB keypress as if it was an ENTER keypress
				JumpForward(pView);
			}
			return;
		}
	case WXK_BACK: //8:		// BackSpace key
		{
			bool bWasMadeDirty = TRUE;
			pLayout->m_docEditOperationType = no_edit_op;
            // whm Note: pApp->m_targetPhrase is updated in OnPhraseBoxChanged, so the wx
            // version uses the global below, rather than a value determined in OnChar(),
            // which would not be current.
 			// BEW added test 1Jul09, because when a selection for a merge is current and
			// the merge is done by user pressing Backspace key, without the suppression
			// here FixBox() would be called and the box would be made shorter, resulting
			// in the width of the box being significantly less than the width of the
			// source text phrase from the merger, and that would potentially allow any
			// following piles with short words to be displayed after the merger -
			// overwriting the end of the source text; so in a merger where it is
			// initiated by a <BS> key press, we suppress the FixBox() call
			if (!gbMergeSucceeded)
			{
				FixBox(pView, pApp->m_targetPhrase, bWasMadeDirty, textExtent, 2);
										// selector = 2 for "contracting" the box
			}
			gbMergeSucceeded = FALSE; // clear to default FALSE, otherwise backspacing
									// to remove phrase box characters won't get the
									// needed box resizes done
		}
	default:
		;
	}
}

// returns TRUE if the move was successful, FALSE if not successful
// Ammended July 2003 for auto-capitalization support
// BEW 13Apr10, no changes needed for support of doc version 5
// BEW 21Jun10, no changes needed for support of kbVersion 2, & removed pView from signature
// BEW 17Jul11, changed for GetRefString() to return KB_Entry enum, and use all 10 maps
// for glossing KB
bool CPhraseBox::MoveToPrevPile(CPile *pCurPile)
{
	CAdapt_ItApp* pApp = (CAdapt_ItApp*)&wxGetApp();
	pApp->m_preGuesserStr.Empty(); // BEW 27Nov14, in case a src string, or modified string
		// is stored ready for user's Esc keypress to restore the pre-guesser
		// form, clear it, because the box is gunna move and we want it
		// restored to default empty ready for next box landing location
	// whm modified 29Mar12. Left mouse clicks now beep only when certain parts of
	// the canvas are clicked on, and allows other clicks to act normally (such as
	// the opening/closing of the ViewFilteredMaterial dialog and the Notes dialog).
	// store the current translation, if one exists, before retreating, since each retreat
    // unstores the refString's translation from the KB, so they must be put back each time
    // (perhaps in edited form, if user changed the string before moving back again)
	wxASSERT(pApp != NULL);
	//pApp->limiter = 0; // BEW added Aug13, to support OnIdle() hack for m_targetStr non-stick bug // bug fixed 24Sept13 BEW
	CAdapt_ItView *pView = pApp->GetView();
	CAdapt_ItDoc* pDoc = pView->GetDocument();
	gbByCopyOnly = FALSE; // restore default setting
	CLayout* pLayout = GetLayout();
	CSourcePhrase* pOldActiveSrcPhrase = pCurPile->GetSrcPhrase();

	// make sure m_targetPhrase doesn't have any final spaces either
	RemoveFinalSpaces(pApp->m_pTargetBox,&pApp->m_targetPhrase);

	// if we are at the start, we can't move back any further
	// - but check vertical edit situation first
	int nCurSequNum = pCurPile->GetSrcPhrase()->m_nSequNumber;

	// if vertical editing is in progress, don't permit a move backwards into the preceding
	// gray text area; just beep and return without doing anything
	if (gbVerticalEditInProgress)
	{
		EditRecord* pRec = &gEditRecord;
		if (gEditStep == adaptationsStep || gEditStep == glossesStep)
		{
			if (nCurSequNum <= pRec->nStartingSequNum)
			{
				// we are about to try to move back into the gray text area before the edit span, disallow
				::wxBell();
				pApp->m_pTargetBox->SetFocus();
				pLayout->m_docEditOperationType = no_edit_op;
				return FALSE;
			}
		}
		else if (gEditStep == freeTranslationsStep)
		{
			if (nCurSequNum <= pRec->nFreeTrans_StartingSequNum)
			{
                // we are about to try to move back into the gray text area before the free
                // trans span, disallow (I don't think we can invoke this function from
                // within free translation mode, but no harm to play safe)
				::wxBell();
				pApp->m_pTargetBox->SetFocus();
				pLayout->m_docEditOperationType = no_edit_op;
				return FALSE;
			}
		}
	}
	if (nCurSequNum == 0)
	{
		// IDS_CANNOT_GO_BACK
		wxMessageBox(_(
"You are already at the start of the file, so it is not possible to move back any further."),
		_T(""), wxICON_INFORMATION | wxOK);
		pApp->m_pTargetBox->SetFocus();
		pLayout->m_docEditOperationType = no_edit_op;
		return FALSE;
	}
	bool bOK;

	// don't move back if it means moving to a retranslation pile; but if we are
	// glossing it is okay to move back into a retranslated section
	{
		CPile* pPrev = pView->GetPrevPile(pCurPile);
		wxASSERT(pPrev);
		if (!gbIsGlossing && pPrev->GetSrcPhrase()->m_bRetranslation)
		{
			// IDS_NO_ACCESS_TO_RETRANS
			wxMessageBox(_(
"To edit or remove a retranslation you must use the toolbar buttons for those operations."),
			_T(""), wxICON_INFORMATION | wxOK);
			pApp->m_pTargetBox->SetFocus();
			pLayout->m_docEditOperationType = no_edit_op;
			return FALSE;
		}
	}

    // if the location is a <Not In KB> one, we want to skip the store & fourth line
    // creation --- as of Dec 18, version 1.4.0, according to Susanna Imrie's
    // recommendation, I've changed this so it will allow a non-null adaptation to remain
    // at this location in the document, but just to suppress the KB store; if glossing is
    // ON, then being a <Not In KB> location is irrelevant, and we will want the store done
    // normally - but to the glossing KB of course
	//bool bNoError = TRUE;
	if (!gbIsGlossing && !pCurPile->GetSrcPhrase()->m_bHasKBEntry
												&& pCurPile->GetSrcPhrase()->m_bNotInKB)
	{
        // if the user edited out the <Not In KB> entry from the KB editor, we need to put
        // it back so that the setting is preserved (the "right" way to change the setting
        // is to use the toolbar checkbox - this applies when adapting, not glossing)
		pApp->m_pKB->Fix_NotInKB_WronglyEditedOut(pCurPile);
		goto b;
	}

	// if the box contents is null, then the source phrase must store an empty string
	// as appropriate - either m_adaption when adapting, or m_gloss when glossing
	if (pApp->m_targetPhrase.IsEmpty())
	{
		pApp->m_bForceAsk = FALSE; // make sure it's turned off, & allow function to continue
		if (gbIsGlossing)
			pCurPile->GetSrcPhrase()->m_gloss.Empty();
		else
			pCurPile->GetSrcPhrase()->m_adaption.Empty();
	}

    // make the punctuated target string, but only if adapting; note, for auto
    // capitalization ON, the function will change initial lower to upper as required,
    // whatever punctuation regime is in place for this particular sourcephrase instance...
    // we are about to leave the current phrase box location, so we must try to store what
    // is now in the box, if the relevant flags allow it. Test to determine which KB to
    // store to. StoreText( ) has been ammended for auto-capitalization support (July 2003)
	if (gbIsGlossing)
	{
		// BEW added 27May10, to not save contents if backing back from a halt
		// location, when there is no content on the CSourcePhrase instance already
		if (pCurPile->GetSrcPhrase()->m_gloss.IsEmpty())
		{
			bOK = TRUE;
		}
		else
		{
			bOK = pApp->m_pGlossingKB->StoreTextGoingBack(pCurPile->GetSrcPhrase(), pApp->m_targetPhrase);
		}
	}
	else // adapting
	{
		// BEW added 27May10, to not save contents if backing back from a halt
		// location, when there is no content on the CSourcePhrase instance already
		if (pCurPile->GetSrcPhrase()->m_adaption.IsEmpty())
		{
			bOK = TRUE;
		}
		else
		{
			pView->MakeTargetStringIncludingPunctuation(pCurPile->GetSrcPhrase(), pApp->m_targetPhrase);
			pView->RemovePunctuation(pDoc, &pApp->m_targetPhrase, from_target_text);
			gbInhibitMakeTargetStringCall = TRUE;
			bOK = pApp->m_pKB->StoreTextGoingBack(pCurPile->GetSrcPhrase(), pApp->m_targetPhrase);
			gbInhibitMakeTargetStringCall = FALSE;
		}
	}
	if (!bOK)
	{
        // here, MoveToNextPile() calls DoStore_NormalOrTransliterateModes(), but for
        // MoveToPrevPile() we will keep it simple and not try to get text for the phrase
        // box
		pLayout->m_docEditOperationType = no_edit_op;
		return FALSE; // can't move if the adaptation or gloss text is not yet completed
	}

	// move to previous pile's cell
b:	CPile* pNewPile = pView->GetPrevPile(pCurPile); // does not update the view's
				// m_nActiveSequNum nor m_pActivePile pointer, so update these here,
				// provided NULL was not returned

	// restore default button image, and m_bCopySourcePunctuation to TRUE
	wxCommandEvent event;
	if (!pApp->m_bCopySourcePunctuation)
	{
		pApp->GetView()->OnToggleEnablePunctuationCopy(event);
	}
	if (pNewPile == NULL)
	{
		// we deem vertical editing current step to have ended if control gets into this
		// block, so user has to be asked what to do next if vertical editing is currently
		// in progress; and we tunnel out before m_nActiveSequNum can be set to -1 (otherwise
		// vertical edit will crash when recalc layout is tried with a bad sequ num value)
		if (gbVerticalEditInProgress)
		{
			gbTunnellingOut = FALSE; // ensure default value set
			bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(-1,
							nextStep, TRUE); // bForceTransition is TRUE
			if (bCommandPosted)
			{
				// don't proceed further because the current vertical edit step has ended
				gbTunnellingOut = TRUE; // so caller can use it
				pLayout->m_docEditOperationType = no_edit_op;
				return FALSE;
			}
		}
		pLayout->m_docEditOperationType = no_edit_op;
		return FALSE; // we are at the start of the file too, so can't go further back
	}
	else
	{
		// the previous pile ptr is valid, so proceed

        // don't commit to the new pile if we are in vertical edit mode, until we've
        // checked the pile is not in the gray text area...
        // if vertical editing is currently in progress we must check if the lookup target
        // is within the editable span, if not then control has moved the box into the gray
        // area beyond the editable span and that means a step transition is warranted &
        // the user should be asked what step is next
		if (gbVerticalEditInProgress)
		{
			int nCurrentSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
			gbTunnellingOut = FALSE; // ensure default value set
			bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(
									nCurrentSequNum,nextStep); // bForceTransition is FALSE
			if (bCommandPosted)
			{
				// don't proceed further because the current vertical edit step has ended
				gbTunnellingOut = TRUE; // so caller can use it
				pLayout->m_docEditOperationType = no_edit_op;
				return FALSE; // try returning FALSE
			}
			else
			{
				// BEW 19Oct15 No transition of vert edit modes,
				// so we can store this location on the app
				gpApp->m_vertEdit_LastActiveSequNum = nCurrentSequNum;
#if defined(_DEBUG)
				wxLogDebug(_T("VertEdit PhrBox, MoveToPrevPile() storing loc'n: %d "), nCurrentSequNum);
#endif
			}
		}

		pApp->m_bUserTypedSomething = FALSE; // user has not typed at the new location yet

		// update the active sequence number, and pile pointer
		pApp->m_nActiveSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
		pApp->m_pActivePile = pNewPile;

        // get an adjusted pile pointer for the new active location, and we want the
        // pile's strip to be marked as invalid and the strip index added to the
        // CLayout::m_invalidStripArray
		pDoc->ResetPartnerPileWidth(pOldActiveSrcPhrase);

        // since we are moving back, the prev pile is likely to have a refString
        // translation which is nonempty, so we have to put it into m_targetPhrase so that
        // ResizeBox will use it; but if there is none, the copy the source key if the
        // m_bCopySource flag is set, else just set it to an empty string. (bNeed Modify is
        // a helper flag used for setting/clearing the document's modified flag at the end
        // of this function)
		bool bNeedModify = FALSE; // reset to TRUE if we copy source
								  // because there was no adaptation
		// be careful, the pointer might point to <Not In KB>, rather than a normal entry
		CRefString* pRefString = NULL;
		KB_Entry rsEntry;
		if (gbIsGlossing)
		{
			rsEntry = pApp->m_pGlossingKB->GetRefString(pNewPile->GetSrcPhrase()->m_nSrcWords,
					pNewPile->GetSrcPhrase()->m_key, pNewPile->GetSrcPhrase()->m_gloss, pRefString);
		}
		else
		{
			rsEntry = pApp->m_pKB->GetRefString(pNewPile->GetSrcPhrase()->m_nSrcWords,
					pNewPile->GetSrcPhrase()->m_key, pNewPile->GetSrcPhrase()->m_adaption, pRefString);
		}
		if (pRefString != NULL && rsEntry == really_present)
		{
			pView->RemoveSelection(); // we won't do merges in this situation

			// assign the translation text - but check it's not "<Not In KB>", if it is, we
			// leave the phrase box empty, turn OFF the m_bSaveToKB flag -- this is changed
			// for v1.4.0 and onwards because we will want to leave any adaptation already
			// present unchanged, rather than clear it and so we will not make it abandonable
			// either
			wxString str = pRefString->m_translation; // no case change to be done here since
								// all we want to do is remove the refString or decrease its
								// reference count
			if (!gbIsGlossing && str == _T("<Not In KB>"))
			{
				pApp->m_bSaveToKB = FALSE;
				pApp->m_pTargetBox->m_bAbandonable = FALSE; // used to be TRUE;
				pNewPile->GetSrcPhrase()->m_bHasKBEntry = FALSE; // ensures * shows above this
															   // srcPhrase
				pNewPile->GetSrcPhrase()->m_bNotInKB = TRUE;
			}

            // remove the translation from the KB, in case user wants to edit it before
            // it's stored again (RemoveRefString also clears the m_bHasKBEntry flag on the
            // source phrase, or m_bHasGlossingKBEntry if gbIsGlossing is TRUE)
			if (gbIsGlossing)
			{
				pApp->m_pGlossingKB->RemoveRefString(pRefString, pNewPile->GetSrcPhrase(),
											pNewPile->GetSrcPhrase()->m_nSrcWords);
				pApp->m_targetPhrase = pNewPile->GetSrcPhrase()->m_gloss;
			}
			else
			{
				pApp->m_pKB->RemoveRefString(pRefString, pNewPile->GetSrcPhrase(),
											pNewPile->GetSrcPhrase()->m_nSrcWords);
				// since we have optional punctuation hiding, use the line with
				// the punctuation
				pApp->m_targetPhrase = pNewPile->GetSrcPhrase()->m_targetStr;
			}
		}
		else // the pointer to refString was null (ie. no KB entry) or rsEntry was present_but_deleted
		{
			if (gbIsGlossing)  // ensure the flag below is false when there is no KB entry
				pNewPile->GetSrcPhrase()->m_bHasGlossingKBEntry = FALSE;
			else
				pNewPile->GetSrcPhrase()->m_bHasKBEntry = FALSE;

			// just use an empty string, or copy the sourcePhrase's key if
			// the m_bCopySource flag is set
			if (pApp->m_bCopySource)
			{
				// whether glossing or adapting, we don't want a null source phrase
				// to initiate a copy
				if (!pNewPile->GetSrcPhrase()->m_bNullSourcePhrase)
				{
					pApp->m_targetPhrase = pView->CopySourceKey(pNewPile->GetSrcPhrase(),
											pApp->m_bUseConsistentChanges);
					bNeedModify = TRUE;
				}
				else
					pApp->m_targetPhrase.Empty();
			}
			else
				pApp->m_targetPhrase.Empty(); // this will cause pile's m_nMinWidth to be used
											  // to set the m_curBoxWidth value on the view
		}

        // initialize the phrase box too, so it doesn't carry an old string to the next
        // pile's cell
		ChangeValue(pApp->m_targetPhrase); //SetWindowText(pApp->m_targetPhrase);

        // get an adjusted pile pointer for the new active location, and we want the
        // pile's strip to be marked as invalid and the strip index added to the
        // CLayout::m_invalidStripArray
		if (pNewPile != NULL)
		{
			pDoc->ResetPartnerPileWidth(pNewPile->GetSrcPhrase());
		}

        pApp->m_nActiveSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
		pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
#ifdef _NEW_LAYOUT
		pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
#else
		pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif

		// make sure the new active pile's pointer is reset
		pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
		pLayout->m_docEditOperationType = relocate_box_op;

		// recreate the phraseBox using the stored information
		pApp->m_nStartChar = -1; pApp->m_nEndChar = -1;

		// fix the m_bSaveToKB flag, depending on whether or not srcPhrase is in kb
		if (!gbIsGlossing && !pApp->m_pActivePile->GetSrcPhrase()->m_bHasKBEntry &&
										pApp->m_pActivePile->GetSrcPhrase()->m_bNotInKB)
		{
			pApp->m_bSaveToKB = FALSE;
			pApp->m_targetPhrase.Empty();
		}
		else
		{
			pApp->m_bSaveToKB = TRUE;
		}

		// update status bar with project name
		pApp->RefreshStatusBarInfo();

		// BEW note 30Mar09: later we may set clipping.... in the meantime
		// just invalidate the lot
		pView->Invalidate();
		pLayout->PlaceBox();

		if (bNeedModify)
			SetModify(TRUE); // our own SetModify(); calls MarkDirty()
		else
			SetModify(FALSE); // our own SetModify(); calls DiscardEdits();

		return TRUE;
	}
}

// returns TRUE if the move was successful, FALSE if not successful
// Ammended, July 2003, for auto capitalization support
// BEW 13Apr10, no changes needed for support of doc version 5
// BEW 21Jun10, no changes needed for support of kbVersion 2, & removed pView from signature
// BEW 17Jul11, changed for GetRefString() to return KB_Entry enum, and use all 10 maps
// for glossing KB
bool CPhraseBox::MoveToImmedNextPile(CPile *pCurPile)
{
	CAdapt_ItApp* pApp = (CAdapt_ItApp*)&wxGetApp();
	pApp->m_preGuesserStr.Empty(); // BEW 27Nov14, in case a src string, or modified string
		// is stored ready for user's Esc keypress to restore the pre-guesser
		// form, clear it, because the box is gunna move and we want it
		// restored to default empty ready for next box landing location
	// whm modified 29Mar12. Left mouse clicks now beep only when certain parts of
	// the canvas are clicked on, and allows other clicks to act normally (such as
	// the opening/closing of the ViewFilteredMaterial dialog and the Notes dialog).
	// store the current translation, if one exists, before moving to next pile, since each move
	// unstores the refString's translation from the KB, so they must be put back each time
	// (perhaps in edited form, if user changed the string before moving back again)
	wxASSERT(pApp != NULL);
	//pApp->limiter = 0; // BEW added Aug13, to support OnIdle() hack for m_targetStr non-stick bug // bug fixed 24Sept13 BEW
	CAdapt_ItView *pView = pApp->GetView();
	CAdapt_ItDoc* pDoc = pView->GetDocument();
	gbByCopyOnly = FALSE; // restore default setting
	CSourcePhrase* pOldActiveSrcPhrase = pCurPile->GetSrcPhrase();
	bool bOK;

	// make sure m_targetPhrase doesn't have any final spaces
	RemoveFinalSpaces(pApp->m_pTargetBox, &pApp->m_targetPhrase);

	// BEW changed 25Oct09, altered syntax so it no longer exits here if pFwd
	// is NULL, otherwise the document-end typed meaning doesn't 'stick' in
	// the document
	CPile* pFwd = pView->GetNextPile(pCurPile);
	if (pFwd == NULL)
	{
		// no more piles, but continue so we can make the user's typed string
		// 'stick' before we prematurely exit further below
		;
	}
	else
	{
		// when adapting, don't move forward if it means moving to a
		// retranslation pile but we don't care when we are glossing
		bool bNotInRetranslation =
			CheckPhraseBoxDoesNotLandWithinRetranslation(pView,pFwd,pCurPile);
		if (!gbIsGlossing && !bNotInRetranslation)
		{
			// BEW removed this message, because it is already embedded in the prior
			// CheckPhraseBoxDoesNotLandWithinRetranslation(pView,pFwd,pCurPile) call, and
			// will be shown from there if relevant.
			//wxMessageBox(_(
            //"Sorry, to edit or remove a retranslation you must use the toolbar buttons for those operations."),
			//_T(""), wxICON_INFORMATION | wxOK);
			pApp->m_pTargetBox->SetFocus();
			GetLayout()->m_docEditOperationType = no_edit_op;
			return FALSE;
		}
	}

    // if the location is a <Not In KB> one, we want to skip the store & fourth line
    // creation but first check for user incorrectly editing and fix it
	if (!gbIsGlossing && !pCurPile->GetSrcPhrase()->m_bHasKBEntry &&
												pCurPile->GetSrcPhrase()->m_bNotInKB)
	{
        // if the user edited out the <Not In KB> entry from the KB editor, we need to put
        // it back so that the setting is preserved (the "right" way to change the setting
        // is to use the toolbar checkbox - this applies when adapting, not glossing)
		pApp->m_pKB->Fix_NotInKB_WronglyEditedOut(pCurPile);
		goto b;
	}

	if (pApp->m_targetPhrase.IsEmpty())
	{
		pApp->m_bForceAsk = FALSE; // make sure it's turned off, & allow
								   // function to continue
		if (gbIsGlossing)
			pCurPile->GetSrcPhrase()->m_gloss.Empty();
		else
			pCurPile->GetSrcPhrase()->m_adaption.Empty();
	}

    // make the punctuated target string, but only if adapting; note, for auto
    // capitalization ON, the function will change initial lower to upper as required,
    // whatever punctuation regime is in place for this particular sourcephrase instance...
    // we are about to leave the current phrase box location, so we must try to store what
    // is now in the box, if the relevant flags allow it. Test to determine which KB to
    // store to. StoreText( ) has been ammended for auto-capitalization support (July 2003)
	if (!gbIsGlossing)
	{
		pView->MakeTargetStringIncludingPunctuation(pCurPile->GetSrcPhrase(), pApp->m_targetPhrase);
		pView->RemovePunctuation(pDoc, &pApp->m_targetPhrase,from_target_text);
	}
	gbInhibitMakeTargetStringCall = TRUE;
	if (gbIsGlossing)
		bOK = pApp->m_pGlossingKB->StoreText(pCurPile->GetSrcPhrase(), pApp->m_targetPhrase);
	else
		bOK = pApp->m_pKB->StoreText(pCurPile->GetSrcPhrase(), pApp->m_targetPhrase);
	gbInhibitMakeTargetStringCall = FALSE;
	if (!bOK)
	{
		// restore default button image, and m_bCopySourcePunctuation to TRUE
		wxCommandEvent event;
		if (!pApp->m_bCopySourcePunctuation)
		{
			pApp->GetView()->OnToggleEnablePunctuationCopy(event);
		}
		GetLayout()->m_docEditOperationType = no_edit_op;
		return FALSE; // can't move if the storage failed
	}

b:	pDoc->ResetPartnerPileWidth(pOldActiveSrcPhrase);

	// move to next pile's cell
	CPile* pNewPile = pView->GetNextPile(pCurPile); // does not update the view's
				// m_nActiveSequNum nor m_pActivePile pointer, so update these here,
				// provided NULL was not returned

	// restore default button image, and m_bCopySourcePunctuation to TRUE
	wxCommandEvent event;
	if (!pApp->m_bCopySourcePunctuation)
	{
		pApp->GetView()->OnToggleEnablePunctuationCopy(event);
	}
	if (pNewPile == NULL)
	{
        // we deem vertical editing current step to have ended if control gets into this
        // block, so user has to be asked what to do next if vertical editing is currently
        // in progress; and we tunnel out before m_nActiveSequNum can be set to -1
        // (otherwise vertical edit will crash when recalc layout is tried with a bad sequ
        // num value)
		if (gbVerticalEditInProgress)
		{
			gbTunnellingOut = FALSE; // ensure default value set
			bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(-1,
							nextStep, TRUE); // bForceTransition is TRUE
			if (bCommandPosted)
			{
				// don't proceed further because the current vertical edit step has ended
				gbTunnellingOut = TRUE; // so caller can use it
				GetLayout()->m_docEditOperationType = no_edit_op;
				return FALSE;
			}
		}

		if (!pApp->m_bSingleStep)
		{
			pApp->m_bAutoInsert = FALSE; // cause halt, if auto lookup & inserting is ON
		}

		// ensure the view knows the pile pointer is no longer valid
		pApp->m_pActivePile = NULL;
		pApp->m_nActiveSequNum = -1;
		GetLayout()->m_docEditOperationType = no_edit_op;
		return FALSE; // we are at the end of the file
	}
	else // we have a pointer to the next pile
	{
		pApp->m_bUserTypedSomething = FALSE; // user has not typed at the new location yet

        // don't commit to the new pile if we are in vertical edit mode, until we've
        // checked the pile is not in the gray text area...
        // if vertical editing is currently in progress we must check if the lookup target
        // is within the editable span, if not then control has moved the box into the gray
        // area beyond the editable span and that means a step transition is warranted &
        // the user should be asked what step is next
		if (gbVerticalEditInProgress)
		{
			int nCurrentSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
			gbTunnellingOut = FALSE; // ensure default value set
			bool bCommandPosted = pView->VerticalEdit_CheckForEndRequiringTransition(
									nCurrentSequNum,nextStep); // bForceTransition is FALSE
			if (bCommandPosted)
			{
				// don't proceed further because the current vertical edit step has ended
				gbTunnellingOut = TRUE; // so caller can use it
				GetLayout()->m_docEditOperationType = no_edit_op;
				return FALSE; // try returning FALSE
			}
			else
			{
				// BEW 19Oct15 No transition of vert edit modes,
				// so we can store this location on the app
				gpApp->m_vertEdit_LastActiveSequNum = nCurrentSequNum;
#if defined(_DEBUG)
				wxLogDebug(_T("VertEdit PhrBox, MoveToImmedNextPile() storing loc'n: %d "), nCurrentSequNum);
#endif
			}
		}

		// update the active sequence number, and pile pointer
		pApp->m_nActiveSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber;
		pApp->m_pActivePile = pNewPile;

        // since we are moving forward from could be anywhere, the next pile is may have a
        // refString translation which is nonempty, so we have to put it into
        // m_targetPhrase so that ResizeBox will use it; but if there is none, the copy the
        // source key if the m_bCopySource flag is set, else just set it to an empty
        // string. (bNeed Modify is a helper flag used for setting/clearing the document's
        // modified flag at the end of this function)
		bool bNeedModify = FALSE; // reset to TRUE if we copy source because there was no
								  // adaptation

        // beware, next call the pRefString pointer may point to <Not In KB>, so take that
        // into account; GetRefString has been modified for auto-capitalization support
		CRefString* pRefString = NULL;
		KB_Entry rsEntry;
		if (gbIsGlossing)
			rsEntry = pApp->m_pGlossingKB->GetRefString(pNewPile->GetSrcPhrase()->m_nSrcWords,
				pNewPile->GetSrcPhrase()->m_key, pNewPile->GetSrcPhrase()->m_gloss, pRefString);
		else
			rsEntry = pApp->m_pKB->GetRefString(pNewPile->GetSrcPhrase()->m_nSrcWords,
				pNewPile->GetSrcPhrase()->m_key, pNewPile->GetSrcPhrase()->m_adaption, pRefString);
		if (pRefString != NULL && rsEntry == really_present)
		{
			pView->RemoveSelection(); // we won't do merges in this situation

			// assign the translation text - but check it's not "<Not In KB>", if it is, we
			// leave the phrase box unchanged (rather than empty as formerly), but turn OFF
			// the m_bSaveToKB flag
			wxString str = pRefString->m_translation;
			if (!gbIsGlossing && str == _T("<Not In KB>"))
			{
				pApp->m_bSaveToKB = FALSE;
				pApp->m_pTargetBox->m_bAbandonable = FALSE;
				pNewPile->GetSrcPhrase()->m_bHasKBEntry = FALSE; // ensures * shows above
															     // this srcPhrase
				pApp->m_targetPhrase = pNewPile->GetSrcPhrase()->m_targetStr;
				pNewPile->GetSrcPhrase()->m_bNotInKB = TRUE;
			}
			else
			{
				pApp->m_pTargetBox->m_bAbandonable = FALSE;
				pApp->m_targetPhrase = str;
			}

            // remove the translation from the KB, in case user wants to edit it before
            // it's stored again (RemoveRefString also clears the m_bHasKBEntry flag on the
            // source phrase)
			if (gbIsGlossing)
			{
				pApp->m_pGlossingKB->RemoveRefString(pRefString, pNewPile->GetSrcPhrase(),
											pNewPile->GetSrcPhrase()->m_nSrcWords);
				pApp->m_targetPhrase = pNewPile->GetSrcPhrase()->m_gloss;
			}
			else
			{
				pApp->m_pKB->RemoveRefString(pRefString, pNewPile->GetSrcPhrase(),
											pNewPile->GetSrcPhrase()->m_nSrcWords);
				pApp->m_targetPhrase = pNewPile->GetSrcPhrase()->m_targetStr;
			}
		}
		else // no kb entry or rsEntry == present_but_deleted
		{
			if (gbIsGlossing)
			{
				pNewPile->GetSrcPhrase()->m_bHasGlossingKBEntry = FALSE; // ensure it's
                                                // false when there is no KB entry
			}
			else
			{
				pNewPile->GetSrcPhrase()->m_bHasKBEntry = FALSE; // ensure it's false
												// when there is no KB entry
			}
            // try to get a suitable string instead from the sourcephrase itself, if that
            // fails then copy the sourcePhrase's key if the m_bCopySource flag is set
			if (gbIsGlossing)
			{
				pApp->m_targetPhrase = pNewPile->GetSrcPhrase()->m_gloss;
			}
			else
			{
				pApp->m_targetPhrase = pNewPile->GetSrcPhrase()->m_adaption;
			}
			if (pApp->m_targetPhrase.IsEmpty() && pApp->m_bCopySource)
			{
				if (!pNewPile->GetSrcPhrase()->m_bNullSourcePhrase)
				{
					pApp->m_targetPhrase = pView->CopySourceKey(
								pNewPile->GetSrcPhrase(),pApp->m_bUseConsistentChanges);
					bNeedModify = TRUE;
				}
				else
				{
					pApp->m_targetPhrase.Empty();
				}
			}
		}
		ChangeValue(pApp->m_targetPhrase); // initialize the phrase box too, so it doesn't
										// carry an old string to the next pile's cell

		// get the new active pile
		pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
		wxASSERT(pApp->m_pActivePile != NULL);

        // get an adjusted pile pointer for the new active location, and we want the
        // pile's strip to be marked as invalid and the strip index added to the
        // CLayout::m_invalidStripArray
		if (pNewPile != NULL)
		{
			pDoc->ResetPartnerPileWidth(pNewPile->GetSrcPhrase());
		}

		// if the user has turned on the sending of synchronized scrolling messages
		// send the relevant message, a sync scroll is appropriate now, provided
		// reviewing mode is ON when the MoveToImmedNextPile() -- which is likely as this
		// latter function is only called when in reviewing mode (in wx and legacy
		// versions, this scripture ref message was not sent here, and should have been)
		if (!gbIgnoreScriptureReference_Send && !pApp->m_bDrafting)
		{
			pView->SendScriptureReferenceFocusMessage(pApp->m_pSourcePhrases,
													pApp->m_pActivePile->GetSrcPhrase());
		}

		// recreate the phraseBox using the stored information
		pApp->m_nStartChar = -1; pApp->m_nEndChar = -1;
		GetLayout()->m_docEditOperationType = relocate_box_op;

		// fix the m_bSaveToKB flag, depending on whether or not srcPhrase is in KB
		if (!gbIsGlossing && !pApp->m_pActivePile->GetSrcPhrase()->m_bHasKBEntry &&
										pApp->m_pActivePile->GetSrcPhrase()->m_bNotInKB)
		{
			pApp->m_bSaveToKB = FALSE;
		}
		else
		{
			pApp->m_bSaveToKB = TRUE;
		}

		// BEW note 30Mar09: later we may set clipping here or somewhere
		pView->Invalidate(); // I think this call is needed
		GetLayout()->PlaceBox();

		if (bNeedModify)
			SetModify(TRUE); // our own SetModify(); calls MarkDirty()
		else
			SetModify(FALSE); // our own SetModify(); calls DiscardEdits()

		return TRUE;
	}
}

// BEW 13Apr10, no changes needed for support of doc version 5
void CPhraseBox::OnSysKeyUp(wxKeyEvent& event)
{
	// wx Note: This routine handles Adapt It's AltDown key events
	// and CmdDown events (= ControlDown on PCs; Apple Command key events on Macs).
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	CAdapt_ItView* pView = (CAdapt_ItView*) pApp->GetView();
	wxASSERT(pView->IsKindOf(CLASSINFO(CAdapt_ItView)));
	long nStart;
	long nEnd;

	bool bTRUE = FALSE;
	if (event.AltDown())// || event.CmdDown()) // CmdDown is same as ControlDown on PCs; Apple Command key on Macs.
	{
		// whm added 26Mar12. Don't allow AltDown + key events when in read-only mode
		if (pApp->m_bReadOnlyAccess)
		{
			return;
		}

		// ALT key or Control/Command key is down
		if (event.AltDown() && event.GetKeyCode() == WXK_RETURN)
		{
			// ALT key is down, and <RET> was nChar typed (ie. 13), so invoke the
			// code to turn selection into a phrase; but if glossing is ON, then
			// merging must not happen - in which case exit early
			if (gbIsGlossing || !(pApp->m_selection.GetCount() > 1))
				return;
			pView->MergeWords();
			GetLayout()->m_docEditOperationType = merge_op;

			// select the lot
			SetSelection(-1,-1);// -1,-1 selects all
			pApp->m_nStartChar = -1;
			pApp->m_nEndChar = -1;

			// set old sequ number in case required for toolbar's Back button - in this case
			// it may have been a location which no longer exists because it was absorbed in
			// the merge, so set it to the merged phrase itself
			gnOldSequNum = pApp->m_nActiveSequNum;
			return;
		}

		// BEW added 19Apr06 to allow Bob Eaton to advance phrasebox without having any lookup of the KB done.
		// We want this to be done only for adapting mode, and provided transliteration mode is turned on
		if (!gbIsGlossing && pApp->m_bTransliterationMode && event.GetKeyCode() == WXK_BACK) //nChar == VK_BACK)
		{
			// save old sequ number in case required for toolbar's Back button
			gnOldSequNum = pApp->m_nActiveSequNum;

			gbSuppressStoreForAltBackspaceKeypress = TRUE; // suppress store to KB for this move of box
			// the value is restored to FALSE in MoveToImmediateNextPile()

			// do the move forward to next empty pile, with lookup etc, but no store due to
			// the gbSuppressStoreForAltBackspaceKeypress global being TRUE until the StoreText()
			// call is jumped over in the MoveToNextPile() call within JumpForward()
			JumpForward(pView);
			return;
		}
		else if (!gbIsGlossing && !pApp->m_bTransliterationMode && event.GetKeyCode() == WXK_BACK)
		{
			// Alt key down & Backspace key hit, so user wanted to initiate a transliteration
			// advance of the phrase box, with its special KB storage mode, but forgot to turn the
			// transliteration mode on before using this feature, so warn him to turn it on and then
			// do nothing
			//IDS_TRANSLITERATE_OFF
			wxMessageBox(_("Transliteration mode is not yet turned on."),_T(""),wxICON_EXCLAMATION | wxOK);

			// restore focus to the phrase box
			if (pApp->m_pTargetBox != NULL)
				if (pApp->m_pTargetBox->IsShown())
					pApp->m_pTargetBox->SetFocus();
			return;
		}

		// ALT key is down, if an arrow key pressed, extend/retract sel'n left or right
		// or insert a null source phrase, or open the retranslation dialog; but don't
		// allow any of this (except Alt + Backspace) if glossing is ON - in those cases, just return
		if (gbIsGlossing)
			return;
		GetSelection(&nStart,&nEnd);
		if (event.GetKeyCode() == WXK_RIGHT)
		{
			if (gbRTL_Layout)
				bTRUE = pView->ExtendSelectionLeft();
			else
				bTRUE = pView->ExtendSelectionRight();
			if(!bTRUE)
			{
				if (gbVerticalEditInProgress)
				{
					::wxBell();
				}
				else
				{
					// did not succeed - do something eg. warn user he's collided with a boundary
					// IDS_RIGHT_EXTEND_FAIL
					wxMessageBox(_("Sorry, you cannot extend the selection that far to the right unless you also use one of the techniques for ignoring boundaries."),_T(""), wxICON_INFORMATION | wxOK);
				}
			}
			SetFocus();
			SetSelection(nStart,nEnd);
			pApp->m_nStartChar = nStart;
			pApp->m_nEndChar = nEnd;
		}
		else if (event.GetKeyCode() == WXK_LEFT)
		{
			if (gbRTL_Layout)
				bTRUE = pView->ExtendSelectionRight();
			else
				bTRUE = pView->ExtendSelectionLeft();
			if(!bTRUE)
			{
				if (gbVerticalEditInProgress)
				{
					::wxBell();
				}
				else
				{
					// did not succeed, so warn user
					// IDS_LEFT_EXTEND_FAIL
					wxMessageBox(_("Sorry, you cannot extend the selection that far to the left unless you also use one of the techniques for ignoring boundaries. "), _T(""), wxICON_INFORMATION | wxOK);
				}
			}
			SetFocus();
			SetSelection(nStart,nEnd);
			pApp->m_nStartChar = nStart;
			pApp->m_nEndChar = nEnd;
		}
		else if (event.GetKeyCode() == WXK_UP)
		{
			// up arrow was pressed, so get the retranslation dialog open
			if (pApp->m_pActivePile == NULL)
			{
				return;
				//goto a;
			}
			if (pApp->m_selectionLine != -1)
			{
				// if there is at least one source phrase with a selection defined,
				// then then use the selection and put up the dialog
				pApp->GetRetranslation()->DoRetranslationByUpArrow();
			}
			else
			{
				// no selection, so make a selection at the phrase box and invoke the
				// retranslation dialog on it
				//CCell* pCell = pApp->m_pActivePile->m_pCell[2];
				CCell* pCell = pApp->m_pActivePile->GetCell(1);
				wxASSERT(pCell);
				pApp->m_selection.Append(pCell);
				//pApp->m_selectionLine = 1;
				pApp->m_selectionLine = 0;// in refactored layout, src text line is always index 0
				wxUpdateUIEvent evt;
				//pView->OnUpdateButtonRetranslation(evt);
				// whm Note: calling OnUpdateButtonRetranslation(evt) here doesn't work because there
				// is not enough idle time for the Do A Retranslation toolbar button to be enabled
				// before the DoRetranslationByUpArrow() call below executes - which has code in it
				// to prevent the Retranslation dialog from poping up unless the toolbar button is
				// actually enabled. So, we explicitly enable the toolbar button here instead of
				// waiting for it to be done in idle time.
				CMainFrame* pFrame = pApp->GetMainFrame();
				wxASSERT(pFrame != NULL);
				wxAuiToolBarItem *tbi;
				tbi = pFrame->m_auiToolbar->FindTool(ID_BUTTON_RETRANSLATION);
				// Return if the toolbar item is hidden
				if (tbi == NULL)
				{
					return;
				}
				// enable the toolbar button
				pFrame->m_auiToolbar->EnableTool(ID_BUTTON_RETRANSLATION, true);
				pApp->GetRetranslation()->DoRetranslationByUpArrow();
			}
		}
		else if (event.GetKeyCode() == WXK_DOWN)
		{
			// whm Note 12Feb09: Control passes through here when a simultaneous Ctrl-Alt-Down press is
			// released. This equates to a Command-Option-Down combination, which is acceptable and
			// doesn't conflict with any reserved keys on a Mac. If only Ctrl-Down (Command-Down on a
			// Mac) is pressed, control does NOT pass through here, but through the WXK_DOWN block of
			// OnKeyUp().
			//
			// Insert of null sourcephrase but first save old sequ number in case required for toolbar's
			// Back button (this one is activated when CTRL key is not down, so it does the
			// default "insert before" case; the "insert after" case is done in the
			// OnKeyUp() handler)

			// Bill wanted the behaviour modified, so that if the box's m_bAbandonable flag is TRUE
			// (ie. a copy of source text was done and nothing typed yet) then the current pile
			// would have the box contents abandoned, nothing put in the KB, and then the placeholder
			// inserion - the advantage of this is that if the placeholder is inserted immediately
			// before the phrasebox's location, then after the placeholder text is typed and the user
			// hits ENTER to continue looking ahead, the former box location will get the box and the
			// copy of the source redone, rather than the user usually having to edit out an unwanted
			// copy from the KB, or remember to clear the box manually. A sufficient thing to do here
			// is just to clear the box's contents.
			if (pApp->m_pTargetBox->m_bAbandonable)
			{
				pApp->m_targetPhrase.Empty();
				if (pApp->m_pTargetBox != NULL
					&& (pApp->m_pTargetBox->IsShown()))
				{
					pApp->m_pTargetBox->ChangeValue(_T(""));
				}
			}

			// now do the 'insert before'
			gnOldSequNum = pApp->m_nActiveSequNum;
			pApp->GetPlaceholder()->InsertNullSrcPhraseBefore();
		}
		// BEW added 26Sep05, to implement Roland Fumey's request that the shortcut for unmerging
		// not be SHIFT+End as in the legacy app, but something else; so I'll make it ALT+Delete
		// and then SHIFT+End can be used for extending the selection in the phrase box's CEdit
		// to the end of the box contents (which is Windows standard behaviour, & what Roland wants)
		if (event.GetKeyCode() == WXK_DELETE)
		{
			// we have ALT + Delete keys held down, so unmerge the current merger - separating into
			// individual words but only when adapting, if glossing we instead just return
			CSourcePhrase* pSP;
			if (pApp->m_pActivePile == NULL)
			{
				return;
			}
			if (pApp->m_selectionLine != -1 && pApp->m_selection.GetCount() == 1)
			{
				CCellList::Node* cpos = pApp->m_selection.GetFirst();
				CCell* pCell = cpos->GetData();
				pSP = pCell->GetPile()->GetSrcPhrase();
				if (pSP->m_nSrcWords == 1)
					return;
			}
			else if (pApp->m_selectionLine == -1 && pApp->m_pTargetBox->GetHandle() != NULL
									&& pApp->m_pTargetBox->IsShown())
			{
				pSP = pApp->m_pActivePile->GetSrcPhrase();
				if (pSP->m_nSrcWords == 1)
					return;
			}
			else
			{
				return;
			}

			// if we get to here, we can go ahead & remove the merger, if we are adapting
			// but if glossing, the user should be explicitly warned the op is no available
			if (gbIsGlossing)
			{
				// IDS_NOT_WHEN_GLOSSING
				wxMessageBox(_("This particular operation is not available when you are glossing."),
				_T(""),wxICON_INFORMATION | wxOK);
				return;
			}
			pView->UnmergePhrase(); // calls OnButtonRestore() - which will attempt to do a lookup,
				// so don't remake the phrase box with the global (CString) gSaveTargetPhrase,
				// otherwise it will override a successful lookkup and make the ALT+Delete give
				// a different result than the Unmerge button on the toolbar. So we in effect
				// are ignoring gSaveTargetPhrase (the former is used in PlacePhraseBox() only
			GetLayout()->m_docEditOperationType = unmerge_op;

			// save old sequ number in case required for toolbar's Back button - the only safe
			// value is the first pile of the unmerged phrase, which is where the phrase box
			// now is
			gnOldSequNum = pApp->m_nActiveSequNum;
		}
	}
}

// return TRUE if we traverse this function without being at the end of the file, or
// failing in the LookAhead function (such as when there was no match); otherwise, return
// FALSE so as to be able to exit from the caller's loop
// BEW 13Apr10, no changes needed for support of doc version 5
// BEW 21May15 added freeze/thaw support

bool CPhraseBox::OnePass(CAdapt_ItView *pView)
{
	#ifdef _FIND_DELAY
		wxLogDebug(_T("1. Start of OnePass"));
	#endif
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);

	CLayout* pLayout = GetLayout();
	//CSourcePhrase* pOldActiveSrcPhrase = NULL; // set but not used
	int nActiveSequNum = pApp->m_nActiveSequNum;
	if (nActiveSequNum < 0)
		return FALSE;
	//else
	//{
	//	pOldActiveSrcPhrase = (pView->GetPile(nActiveSequNum))->GetSrcPhrase();
	//}
	// save old sequ number in case required for toolbar's Back button
	gnOldSequNum = nActiveSequNum;
	gbByCopyOnly = FALSE; // restore default setting

	// BEW 21May15 add here support for freezing the canvas for a NUMINSERTS number
	// of consecutive auto-inserts from the KB; the matching thaw calls are done
	// later below, before returning FALSE, or returning TRUE. The actual Freeze()
	// call is within CLayout's Redraw() and Draw() functions, and is managed by
	// the boolean m_bDoFreeze. Recall that OnePass() is called
	// from only one place in the whole app - from OnIdle(), and even then only when
	// several flags have certain values consistent with automatic insertions being
	// currently permitted. So the logic for doing a freeze and doing it's matching
	// thaw, is encapsulated within this one OnePass() function, except for an added
	// else block in OnIdle() where the m_nInsertCount value is defaulted to 0 whenever
	// m_bAutoInsert is FALSE.
	if (pApp->m_bSupportFreeze)
	{
		if (pApp->m_nInsertCount == 0 || (pApp->m_nInsertCount % (int)NUMINSERTS == 0))
		{
			// Make the freeze happen only when the felicity conditions are satisfied
			if (
				!pApp->m_bIsFrozen      // canvas must not currently be frozen
				&& pApp->m_bDrafting    // the GUI is in drafting mode (only then are auto-inserts possible)
				&& (pApp->m_bAutoInsert || !pApp->m_bSingleStep) // one or both of these conditions apply
				)
			{
				// Ask for the freeze
				pApp->m_bDoFreeze = TRUE;
				// Count this call of OnePass()
				pApp->m_nInsertCount = 1;
				// Do a half-second delay, if that was set to 201 ticks
				if (pApp->m_nCurDelay == 31)
				{
					// DoDelay() at the doc end causes DoDelay() to never exit, so
					// protect from calling near the app end
					int maxSN = pApp->m_pSourcePhrases->GetCount() - 1;
					int safeLocSN = maxSN - (int)NUMINSERTS;
					if (safeLocSN < 0)
					{
						safeLocSN = 0;
					}
					if (maxSN > 0 && nActiveSequNum <= safeLocSN)
					{
						DoDelay(); // see helpers.cpp
						pApp->m_nCurDelay = 0; // restore to zero, we want it only the once
						// at the start of each subsection of inserts
					}
				}
			}
			else
			{
				// Ensure it's not asked for
				pApp->m_bDoFreeze = FALSE;
			}
		}
		else if (pApp->m_bIsFrozen && pApp->m_nInsertCount < (int)NUMINSERTS)
		{
			// The canvas is frozen, and we've not yet halted the auto-inserts, nor
			// have we reached the final OnePass() call of this subsequence, so
			// increment the count (the final call of a subsequence should be done
			// with the canvas having just been thawed)
			pApp->m_nInsertCount++;
		}
	}
	int bSuccessful;
	if (pApp->m_bTransliterationMode && !gbIsGlossing)
	{
		bSuccessful = MoveToNextPile_InTransliterationMode(pApp->m_pActivePile);
	}
	else
	{
		#ifdef _FIND_DELAY
			wxLogDebug(_T("2. Before MoveToNextPile"));
		#endif
		bSuccessful = MoveToNextPile(pApp->m_pActivePile);
		#ifdef _FIND_DELAY
			wxLogDebug(_T("3. After MoveToNextPile"));
		#endif
	}
	if (!bSuccessful)
	{
		// BEW added 4Sep08 in support of transitioning steps within vertical edit mode
		if (gbVerticalEditInProgress && gbTunnellingOut)
		{
			// MoveToNextPile might fail within the editable span while vertical edit is
			// in progress, so we have to allow such a failure to not cause tunnelling out;
			// hence we use the gbTunnellingOut global to assist - it is set TRUE only when
			// VerticalEdit_CheckForEndRequiringTransition() in the view class returns TRUE,
			// which means that a PostMessage(() has been done to initiate a step transition
			gbTunnellingOut = FALSE; // caller has no need of it, so clear to default value
			pLayout->m_docEditOperationType = no_edit_op;
			return FALSE; // caller is OnIdle(), OnePass is not used elsewhere
		}


		// it will have failed because we are at eof without finding a "hole" at which to
		// land the phrase box for reaching the document's end, so we must handle eof state
		//if (pApp->m_pActivePile == NULL && pApp->m_endIndex < pApp->m_maxIndex)
		if (pApp->m_pActivePile == NULL || pApp->m_nActiveSequNum == -1)
		{
			// we got to the end of the doc...

			// BEW changed 9Apr12, support discontinuous auto-inserted spans highlighting
			//pLayout->ClearAutoInsertionsHighlighting(); <- I'm trying no call here,
			// hoping that JumpForward() will suffice, so that OnIdle()'s call of OnePass()
			// twice at the end of doc won't clobber the highlighting already established.
            // YES!! That works - the highlighting is now visible when the box has
            // disappeared and the end of doc message shows. Also, normal adapting still
            // works right despite this change, so that's a bug (or undesireable feature -
            // namely, the loss of highlighting when the doc is reached by auto-inserting)
            // now fixed.

			// BEW 21May13, first place for doing a thaw...
			if (pApp->m_bSupportFreeze && pApp->m_bIsFrozen)
			{
				// We want Thaw() done, and the next call of OnPass() will then get the view 
				// redrawn, and the one after that (if the sequence has not halted) will start
				// a new subsequence of calls where the canvas has been re-frozen 
				pApp->m_nInsertCount = 0;
				pView->canvas->Thaw();
				pApp->m_bIsFrozen = FALSE;
				// don't need a delay here
				if (pApp->m_nCurDelay == 31)
				{
					pApp->m_nCurDelay = 0; // set back to zero
				}
			}
			// remove highlight before MessageBox call below
			//pView->Invalidate();
			pLayout->Redraw(); // bFirstClear is default TRUE
			pLayout->PlaceBox();

			// tell the user EOF has been reached
			gbCameToEnd = TRUE;
			wxStatusBar* pStatusBar;
			CMainFrame* pFrame = (CMainFrame*)pView->GetFrame();
			if (pFrame != NULL)
			{
				pStatusBar = ((CMainFrame*)pFrame)->m_pStatusBar;
				wxASSERT(pStatusBar != NULL);
				wxString str;
				if (gbIsGlossing)
					//IDS_FINISHED_GLOSSING
					str = _("End of the file; nothing more to gloss.");
				else
					//IDS_FINISHED_ADAPTING
					str = _("End of the file; nothing more to adapt.");
				pStatusBar->SetStatusText(str,0);
			}
			// we are at EOF, so set up safe end conditions
			// wxWidgets version Hides the target box rather than destroying it
			pApp->m_pTargetBox->Hide(); // whm added 12Sep04 // MFC version calls DestroyWindow
			pApp->m_pTargetBox->ChangeValue(_T("")); // need to set it to null str since it won't get recreated
			pApp->m_pTargetBox->Enable(FALSE); // whm added 12Sep04
			pApp->m_targetPhrase.Empty();
			pApp->m_nActiveSequNum = -1;

#ifdef _NEW_LAYOUT
			#ifdef _FIND_DELAY
				wxLogDebug(_T("4. Before RecalcLayout"));
			#endif
			pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
			#ifdef _FIND_DELAY
				wxLogDebug(_T("5. After RecalcLayout"));
			#endif
#else
			pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
			pApp->m_pActivePile = (CPile*)NULL; // can use this as a flag for at-EOF condition too
			pLayout->m_docEditOperationType = no_edit_op;

			RestorePhraseBoxAtDocEndSafely(pApp, pView);  // BEW added 8Sep14

		} // end of TRUE block for test: if (pApp->m_pActivePile == NULL || pApp->m_nActiveSequNum == -1)

		else // we have a non-null active pile defined, and sequence number >= 0
		{
			pApp->m_pTargetBox->SetFocus();
			pLayout->m_docEditOperationType = no_edit_op; // is this correct for here?
		}

		// BEW 21May15, this is the second of three places in OnePass() where a Thaw() call
		// must be initiated when conditions are right; control passes through here when the
		// sequence of auto-inserts is just coming to a halt location - we want a Thaw() done
		// in that circumstance, regardless of the m_nInsertCount value, and the latter
		// reset to default zero
		if (pApp->m_bSupportFreeze && pApp->m_bIsFrozen)
		{
			pApp->m_nInsertCount = 0;
			pView->canvas->Thaw();
			pApp->m_bIsFrozen = FALSE;
			// Don't need a third-of-a-second delay 
			if (pApp->m_nCurDelay == 31)
			{
				pApp->m_nCurDelay = 0; // set back to zero
			}
		}
		translation.Empty(); // clear the static string storage for the translation (or gloss)
		// save the phrase box's text, in case user hits SHIFT+END to unmerge a phrase
		gSaveTargetPhrase = pApp->m_targetPhrase;
		pApp->m_bAutoInsert = FALSE; // ensure we halt for user to type translation
		pView->Invalidate(); // added 1Apr09, since we return at next line
		pLayout->PlaceBox();
		return FALSE; // must have been a null string, or at EOF;
	} // end of TRUE block for test: if (!bSuccessful)

	// if control gets here, all is well so far, so set up the main window
	//pApp->GetMainFrame()->canvas->ScrollIntoView(pApp->m_nActiveSequNum);
	#ifdef _FIND_DELAY
		wxLogDebug(_T("6. Before ScrollIntoView"));
	#endif
	pLayout->m_pCanvas->ScrollIntoView(pApp->m_nActiveSequNum);
	#ifdef _FIND_DELAY
		wxLogDebug(_T("7. After ScrollIntoView"));
	#endif

	// BEW 21May15, this is the final of the three places in OnePass() where a Thaw() call
	// must be initiated when conditions are right; control passes through here when the
	// sequence of auto-inserts has not yet come to a halt location - we want a Thaw() if
	// periodically, breaking up the auto-insert sequence with a single redraw when
	// m_nInsertCount reaches the NUMINSERTS value for each subsequence
	if (pApp->m_bSupportFreeze && pApp->m_bIsFrozen)
	{
		// Are we at the penultimate OnePass() call being completed? If so, we want the
		// Thaw() done, and the next call of OnPass() will then get the view redrawn, and
		// the one after that (if the sequence has not halted) will start a new subsequence
		// of calls where the canvas has been re-frozen 
		if (pApp->m_nInsertCount > 0 && ((pApp->m_nInsertCount + 1) % (int)NUMINSERTS == 0))
		{
			// Ask for the thaw of the canvas window
			pApp->m_nInsertCount = 0;
			pView->canvas->Thaw();
			pApp->m_bIsFrozen = FALSE;
			// Give user a 1-second delay in order to get user visually acquainted with the inserted words
			if (pApp->m_nCurDelay == 0)
			{
				// set delay of 31 ticks, it's unlikely to be accidently set to that value;
				// it's 31/100 of a second
				pApp->m_nCurDelay = 31; 
			}
		}
	}
	pLayout->m_docEditOperationType = relocate_box_op;
	gbEnterTyped = TRUE; // keep it continuing to use the faster GetSrcPhras BuildPhrases()
	pView->Invalidate(); // added 1Apr09, since we return at next line
	pLayout->PlaceBox();
	#ifdef _FIND_DELAY
		wxLogDebug(_T("8. End of OnePass"));
	#endif

	// whm added 20Nov10 reset the m_bIsGuess flag below. Can't do it in PlaceBox()
	// because PlaceBox() is called first via the MoveToNextPile() call near the beginning
	// of this function, then again in the line above - twice while control flow is going
	// through this function in the normal course of auto-insert adaptations.
	//pApp->m_bIsGuess = FALSE;

	return TRUE; // all was okay
}

void CPhraseBox::RestorePhraseBoxAtDocEndSafely(CAdapt_ItApp* pApp, CAdapt_ItView *pView)
{
	CLayout* pLayout = pApp->GetLayout();
	int maxSN = pApp->GetMaxIndex();
	CPile* pEndPile = pView->GetPile(maxSN);
	wxASSERT(pEndPile != NULL);
	CSourcePhrase* pTheSrcPhrase = pEndPile->GetSrcPhrase();
	if (!pTheSrcPhrase->m_bRetranslation)
	{
		// BEW 5Oct15, Buka island. Added test for free translation mode here, because
		// coming to the end in free trans mode, putting the phrasebox at last pile
		// results in the current section's last pile getting a spurious new 1-pile
		// section created at the doc end. The fix is to put the phrasebox back at the
		// current pile's anchor, so that no new section is created
		if (pApp->m_bFreeTranslationMode)
		{
			// Find the old anchor pile, closest to the doc end; but just in case
			// something is a bit wonky, test that the last pile has a free translation -
			// if it doesn't, then it is save to make that the active location (and it
			// would become an empty new free translation - which should be free
			// translated probably, and then this function will be reentered to get that
			// location as the final phrasebox location
			CPile* pPile = pView->GetPile(maxSN);
			if (!pPile->GetSrcPhrase()->m_bHasFreeTrans)
			{
				pApp->m_nActiveSequNum = maxSN;
			}
			else
			{
				while (!pPile->GetSrcPhrase()->m_bStartFreeTrans)
				{
					pPile = pView->GetPrevPile(pPile);
					wxASSERT(pPile != NULL);
				}
				pApp->m_nActiveSequNum = pPile->GetSrcPhrase()->m_nSequNumber;
			}
			pApp->m_pActivePile = pPile;
		}
		else
		{
			// Normal mode, use the last pile as active location
			pApp->m_nActiveSequNum = maxSN;
			pApp->m_pActivePile = pEndPile;
		}
	}
	else
	{
		// The end pile is within a retranslation, so find a safe active location
		int aSafeSN = maxSN + 1; // initialize to an illegal value 1 beyond
									// maximum index, this will force the search
									// for a safe location to search backwards
									// from the document's end
		int nFinish = 0; // can be set to num of piles in the retranslation but
							// this will do, as the loop will iterate over them
		// The next call, if successful, sets m_pActivePile
		bool bGotSafeLocation =  pView->SetActivePilePointerSafely(pApp,
							pApp->m_pSourcePhrases,aSafeSN,maxSN,nFinish);
		if (bGotSafeLocation)
		{
			pApp->m_nActiveSequNum = aSafeSN;
			pTheSrcPhrase = pApp->m_pActivePile->GetSrcPhrase();
			wxASSERT(pTheSrcPhrase->m_nSequNumber == aSafeSN);
		}
		else
		{
			// failed to get a safe location, so use start of document
			pApp->m_nActiveSequNum = 0;
			pApp->m_pActivePile = pView->GetPile(0);
		}
	}
	wxString transln = pTheSrcPhrase->m_adaption;
	pApp->m_targetPhrase = transln;
	pApp->m_pTargetBox->ChangeValue(transln);
	int length = transln.Len();
	pApp->m_pTargetBox->SetSelection(length,length);
	pApp->m_bAutoInsert = FALSE; // ensure we halt for user to type translation
#ifdef _NEW_LAYOUT
	pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
#else
	pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
	pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
	pApp->m_pTargetBox->SetFocus();
	pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
	pApp->m_pTargetBox->SetFocus();
	pLayout->m_docEditOperationType = no_edit_op;
	pView->Invalidate();
	pLayout->PlaceBox();
}

// This OnKeyUp function is called via the EVT_KEY_UP event in our CPhraseBox
// Event Table.
// BEW 13Apr10, no changes needed for support of doc version 5
void CPhraseBox::OnKeyUp(wxKeyEvent& event)
{
	//wxLogDebug(_T("OnKeyUp() %d called from PhraseBox"),event.GetKeyCode());
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	CAdapt_ItView* pView = (CAdapt_ItView*) pApp->GetView();
	wxASSERT(pView->IsKindOf(CLASSINFO(CAdapt_ItView)));

/*
// "2" key not doing anything turned out to be a pseudo key binding in
// accelerator keys code, in CMainFrame creator
#if defined(KEY_2_KLUGE) && !defined(__GNUG__) && !defined(__APPLE__)
    // kluge to workaround the problem of a '2' (event.m_keycode = 50) keypress being
    // interpretted as a F10 function keypress (event.m_keyCode == 121)
	if (event.m_keyCode == 50)
	{
			long from; long to;
			GetSelection(&from,&to);
			wxString a2key = _T('2');
			Replace(from,to,a2key);
			return;
	}
#endif
*/
	// Note: wxWidgets doesn't have a separate OnSysKeyUp() virtual method
	// so we'll simply detect if the ALT key was down and call the
	// OnSysKeyUp() method from here
	if (event.AltDown())// CmdDown() is same as ControlDown on PC,
						// and Apple Command key on Macs.
	{
		OnSysKeyUp(event);
		return;
	}

	// BEW 8Jul14 intercept the CTRL+SHIFT+<spacebar> combination to enter a ZWSP
	// (zero width space) into the composebar's editbox; replacing a selection if
	// there is one defined

	if (event.GetKeyCode() == WXK_SPACE)	
	{
		if (!event.AltDown() && event.CmdDown() && event.ShiftDown())
		{
			OnCtrlShiftSpacebar(this); // see helpers.h & .cpp
			return; // don't call skip - we don't want the end-of-line character entered
			// into the edit box
		}
	}

	// version 1.4.2 and onwards, we want a right or left arrow used to remove the
	// phrasebox's selection to be considered a typed character, so that if a subsequent
	// selection and merge is done then the first target word will not get lost; and so
	// we add a block of code also to start of OnChar( ) to check for entry with both
	// m_bAbandonable and m_bUserTypedSomething set TRUE, in which case we don't
	// clear these flags (the older versions cleared the flags on entry to OnChar( ) )

	// we use this flag cocktail to test for these values of the three flags as the
	// condition for telling the application to retain the phrase box's contents
	// when user deselects target word, then makes a phrase and merges by typing.
	// (When glossing, we don't need to do anything here - suppressions handled elsewhere)
	if (event.GetKeyCode() == WXK_RIGHT)
	{
		m_bAbandonable = FALSE;
		pApp->m_bUserTypedSomething = TRUE;
		gbRetainBoxContents = TRUE;
		event.Skip();
	}
	else if (event.GetKeyCode() == WXK_LEFT)
	{
		m_bAbandonable = FALSE;
		pApp->m_bUserTypedSomething = TRUE;
		gbRetainBoxContents = TRUE;
		event.Skip();
	}

	// does the user want to force the Choose Translation dialog open?
	// whm Note 12Feb09: This F8 action is OK on Mac (no conflict)
	if (event.GetKeyCode() == WXK_F8)
	{
		// whm added 26Mar12
		if (pApp->m_bReadOnlyAccess)
		{
			// Disable the F8 key invocation of the ChooseTranslation dialog
			return;
		}
		pView->ChooseTranslation();
		return;
	}

	// does user want to unmerge a phrase?

	// user did not want to unmerge, so must want a scroll
	// preserve cursor location (its already been moved by the time this function is entered)
	int nScrollCount = 1;

    // the code below for up arrow or down arrow assumes a one strip + leading scroll. If
    // we change so as to scroll more than one strip at a time (as when pressing page up or
    // page down keys), iterate by the number of strips needing to be scrolled
	CLayout* pLayout = GetLayout();
	int yDist = pLayout->GetStripHeight() + pLayout->GetCurLeading();

    // do the scroll (CEdit also moves cursor one place to left for uparrow, right for
    // downarrow hence the need to restore the cursor afterwards; however, the values of
    // nStart and nEnd depend on whether the app made the selection, or the user; and also
    // on whether up or down was pressed: for down arrow, the values will be either 1,1 or
    // both == length of line, for up arrow, values will be either both = length of line
    // -1; or 0,0 so code accordingly
	bool bScrollFinished = FALSE;
	int nCurrentStrip;
	int nLastStrip;
	if (event.GetKeyCode() == WXK_UP)
	{
a:		int xPixelsPerUnit,yPixelsPerUnit;
#ifdef Do_Clipping
		pLayout->SetScrollingFlag(TRUE); // need full screen drawing, so clipping can't happen
#endif
		//pApp->GetMainFrame()->canvas->GetScrollPixelsPerUnit(&xPixelsPerUnit,&yPixelsPerUnit);
		pLayout->m_pCanvas->GetScrollPixelsPerUnit(&xPixelsPerUnit,&yPixelsPerUnit);
		wxPoint scrollPos;
		// MFC's GetScrollPosition() "gets the location in the document to which the upper
		// left corner of the view has been scrolled. It returns values in logical units."
		// wx note: The wx docs only say of GetScrollPos(), that it
		// "Returns the built-in scrollbar position."
		// I assume this means it gets the logical position of the upper left corner, but
		// it is in scroll units which need to be converted to device (pixel) units

		//pApp->GetMainFrame()->canvas->CalcUnscrolledPosition(0,0,&scrollPos.x,&scrollPos.y);
		pLayout->m_pCanvas->CalcUnscrolledPosition(0,0,&scrollPos.x,&scrollPos.y);
		// the scrollPos point is now in logical pixels from the start of the doc

		if (scrollPos.y > 0)
		{
			if (scrollPos.y >= yDist)
			{
				// up uparrow was pressed, so scroll up a strip, provided we are not at the
				// start of the document; and we are more than one strip + leading from the start,
				// so it is safe to scroll
				pLayout->m_pCanvas->ScrollUp(1);
			}
			else
			{
				// we are close to the start of the document, but not a full strip plus leading,
				// so do a partial scroll only - otherwise phrase box will be put at the wrong
				// place
				yDist = scrollPos.y;
				scrollPos.y -= yDist;

				int posn = scrollPos.y;
				posn = posn / yPixelsPerUnit;
                // Note: MFC's ScrollWindow's 2 params specify the xAmount and yAmount to
                // scroll in device units (pixels). The equivalent in wx is Scroll(x,y) in
                // which x and y are in SCROLL UNITS (pixels divided by pixels per unit).
                // Also MFC's ScrollWindow takes parameters whose value represents an
                // "amount" to scroll from the current position, whereas the
                // wxScrolledWindow::Scroll takes parameters which represent an absolute
                // "position" in scroll units. To convert the amount we need to add the
                // amount to (or subtract from if negative) the logical pixel unit of the
                // upper left point of the client viewing area; then convert to scroll
                // units in Scroll().
				pLayout->m_pCanvas->Scroll(0,posn); //pView->ScrollWindow(0,yDist);
				Refresh();
				bScrollFinished = TRUE;
			}
		}
		else
		{
			::wxBell();
			bScrollFinished = TRUE;
		}

		if (bScrollFinished)
			goto c;
		else
		{
			--nScrollCount;
			if (nScrollCount == 0)
				goto c;
			else
			{
				goto a;
			}
		}

		// restore cursor location when done
c:		SetFocus();
		SetSelection(pApp->m_nStartChar,pApp->m_nEndChar);
		return;
	}
	else if (event.GetKeyCode() == WXK_DOWN)
	{
		if (event.ControlDown())
		{
			// whm added 26Mar12
			if (pApp->m_bReadOnlyAccess)
			{
				// Disable the Ctrl + down arrow invocation of the insert after of a null srcphrase
				return;
			}
            // CTRL + down arrow was pressed - asking for an "insert after" of a null
            // srcphrase. CTRL + ALT + down arrow also gives the same result (on Windows
            // and Linux) - see OnSysKeyUp().
            // whm 12Feb09 Note: Ctrl + Down (equates to Command-Down on a Mac) conflicts
            // with a Mac's system key assignment to "Move focus to another value/cell
            // within a view such as a table", so we'll prevent Ctrl+Down from calling
            // InsertNullSrcPhraseAfter() on the Mac port.
#ifndef __WXMAC__
			// first save old sequ number in case required for toolbar's Back button
			// If glossing is ON, we don't allow the insertion, and just return instead
			gnOldSequNum = pApp->m_nActiveSequNum;
			if (!gbIsGlossing)
				pApp->GetPlaceholder()->InsertNullSrcPhraseAfter();
#endif
			return;
		}

		// down arrow was pressed, so scroll down a strip, provided we are not at the end of
		// the bundle
b:		wxPoint scrollPos;
#ifdef Do_Clipping
		pLayout->SetScrollingFlag(TRUE); // need full screen drawing, so clipping can't happen
#endif
		int xPixelsPerUnit,yPixelsPerUnit;
		pLayout->m_pCanvas->GetScrollPixelsPerUnit(&xPixelsPerUnit,&yPixelsPerUnit);
		// MFC's GetScrollPosition() "gets the location in the document to which the upper
		// left corner of the view has been scrolled. It returns values in logical units."
		// wx note: The wx docs only say of GetScrollPos(), that it
		// "Returns the built-in scrollbar position."
		// I assume this means it gets the logical position of the upper left corner,
		// but it is in scroll units which need to be converted to device (pixel) units

		wxSize maxDocSize; // renaming barSizes to maxDocSize for clarity
		pLayout->m_pCanvas->GetVirtualSize(&maxDocSize.x,&maxDocSize.y); // gets size in pixels

		pLayout->m_pCanvas->CalcUnscrolledPosition(0,0,&scrollPos.x,&scrollPos.y);
		// the scrollPos point is now in logical pixels from the start of the doc

		wxRect rectClient(0,0,0,0);
		wxSize canvasSize;
		canvasSize = pApp->GetMainFrame()->GetCanvasClientSize();
		rectClient.width = canvasSize.x;
		rectClient.height = canvasSize.y;

		int logicalViewBottom = scrollPos.y + rectClient.GetBottom();
		if (logicalViewBottom < maxDocSize.GetHeight())
		{
			if (logicalViewBottom <= maxDocSize.GetHeight() - yDist)
			{
				// a full strip + leading can be scrolled safely
				//pApp->GetMainFrame()->canvas->ScrollDown(1);
				pLayout->m_pCanvas->ScrollDown(1);
			}
			else
			{
				// we are close to the end, but not a full strip + leading can be scrolled, so
				// just scroll enough to reach the end - otherwise position of phrase box will
				// be set wrongly
				wxASSERT(maxDocSize.GetHeight() >= logicalViewBottom);
				yDist = maxDocSize.GetHeight() - logicalViewBottom; // make yDist be what's
																	// left to scroll
				scrollPos.y += yDist;

				int posn = scrollPos.y;
				posn = posn / yPixelsPerUnit;
				pLayout->m_pCanvas->Scroll(0,posn);
				Refresh();
				bScrollFinished = TRUE;
			}
		}
		else
		{
			bScrollFinished = TRUE;
			::wxBell();
		}

		if (bScrollFinished)
			goto d;
		else
		{
			--nScrollCount;
			if (nScrollCount == 0)
				goto d;
			else
			{
				goto b;
			}
		}
		// restore cursor location when done
d:		SetFocus();
		SetSelection(pApp->m_nStartChar,pApp->m_nEndChar);
		return;
	}
	else if (event.GetKeyCode() == WXK_PAGEUP)
					// GDLC WXK_PRIOR deprecated in 2.8
	{
        // Note: an overload of CLayout::GetVisibleStripsRange() does the same job, so it
        // could be used instead here and for the other instance in next code block - as
        // these two calls are the only two calls of the view's GetVisibleStrips() function
        // in the whole application ** TODO ** ??
		pView->GetVisibleStrips(nCurrentStrip,nLastStrip);
		nScrollCount = nLastStrip - nCurrentStrip;
		goto a;
	}
	else if (event.GetKeyCode() == WXK_PAGEDOWN)
					// GDLC WXK_NEXT deprecated in 2.8
	{
        // Note: an overload of CLayout::GetVisibleStripsRange() does the same job, so it
        // could be used instead here and for the other instance in above code block - as
        // these two calls are the only two calls of the view's GetVisibleStrips() function
        // in the whole application ** TODO ** ??
		pView->GetVisibleStrips(nCurrentStrip,nLastStrip);
		nScrollCount = nLastStrip - nCurrentStrip;
		goto b;
	}
	// Note: The handling of Esc (WXK_ESCAPE) is done in OnKeyDown with a return before
	// calling Skip(). That is the only way I found to prevent the system beep from
	// occurring when hitting the Esc key to restore a Guess to the normal copy to the
	// phrase box.
	//else if (event.GetKeyCode() == WXK_ESCAPE)
	//{
	//	// user pressed the Esc key. If a Guess is current in the phrasebox we
	//	// will remove the Guess-highlight background color and the guess, and
	//	// restore the normal copied source word to the phrasebox. We also reset
	//	// the App's m_bIsGuess flag to FALSE.
	//	if (this->GetBackgroundColour() == pApp->m_GuessHighlightColor)
	//	{
	//		// get the pSrcPhrase at this active location
	//		CSourcePhrase* pSrcPhrase;
	//		pSrcPhrase = pApp->m_pActivePile->GetSrcPhrase();
	//		wxString str = pSrcPhrase->m_key;

	//		if (!gbLegacySourceTextCopy)
	//		{
	//			// the user wants smart copying done to the phrase box when the active location
	//			// landed on does not have any existing adaptation (in adapting mode), or, gloss
	//			// (in glossing mode). In the former case, it tries to copy a gloss to the box
	//			// if a gloss is available, otherwise source text used instead; in the latter case
	//			// it tries to copy an adaptation as the default gloss, if an adaptation is
	//			// available, otherwise source text is used instead
	//			if (gbIsGlossing)
	//			{
	//				if (!pSrcPhrase->m_adaption.IsEmpty())
	//				{
	//					str = pSrcPhrase->m_adaption;
	//				}
	//			}
	//			else
	//			{
	//				if (!pSrcPhrase->m_gloss.IsEmpty())
	//				{
	//					str = pSrcPhrase->m_gloss;
	//				}
	//			}
	//		}
	//		this->ChangeValue(str);
	//		this->SetBackgroundColour(wxColour(255,255,255)); // white;
	//		pApp->m_bIsGuess = FALSE;
	//		return; // return here so the system won't beep
	//	}
	//}
	else if (!gbIsGlossing && pApp->m_bTransliterationMode && event.GetKeyCode() == WXK_RETURN)
	{
        // CTRL + ENTER is a JumpForward() to do transliteration; bleed this possibility
        // out before allowing for any keypress to halt automatic insertion; one side
        // effect is that MFC rings the bell for each such key press and I can't find a way
        // to stop it. So Alt + Backspace can be used instead, for the same effect; or the
        // sound can be turned off at the keyboard if necessary. This behaviour is only
        // available when transliteration mode is turned on.
		if (event.ControlDown())
		{
			// save old sequ number in case required for toolbar's Back button
			gnOldSequNum = pApp->m_nActiveSequNum;

			gbSuppressStoreForAltBackspaceKeypress = TRUE; // suppress store to KB for
				// this move of box, the value is restored to FALSE in MoveToNextPile()

			// do the move forward to next empty pile, with lookup etc, but no store due to
			// the gbSuppressStoreForAltBackspaceKeypress global being TRUE until the StoreText()
			// call is jumped over in the MoveToNextPile() call within JumpForward()
			JumpForward(pView);
			return;
		}
	}
	else if (!gbIsGlossing && !pApp->m_bTransliterationMode && event.GetKeyCode() == WXK_RETURN)
	{
		if (event.ControlDown())
		{
			// user wanted to initiate a transliteration advance of the phrase box, with its
			// special KB storage mode, but forgot to turn the transliteration mode on before
			// using this feature, so warn him to turn it on and then do nothing
			// IDS_TRANSLITERATE_OFF
			wxMessageBox(_("Transliteration mode is not yet turned on."),_T(""),wxICON_EXCLAMATION | wxOK);

			// restore focus to the phrase box
			if (pApp->m_pTargetBox != NULL)
				if (pApp->m_pTargetBox->IsShown())
					pApp->m_pTargetBox->SetFocus();
		}
	}
	// MFC code was commented out below:
	event.Skip();
}

// This OnKeyDown function is called via the EVT_KEY_DOWN event in our CPhraseBox
// Event Table.
// BEW 13Apr10, no changes needed for support of doc version 5
void CPhraseBox::OnKeyDown(wxKeyEvent& event)
{
	// refactored 2Apr09
	//wxLogDebug(_T("OnKeyDown() %d called from PhraseBox"),event.GetKeyCode());
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	CAdapt_ItView* pView = (CAdapt_ItView*) pApp->GetView();
	wxASSERT(pView->IsKindOf(CLASSINFO(CAdapt_ItView)));
	//CLayout* pLayout = GetLayout();

	// whm added 15Mar12. When in read-only mode don't register any key strokes
	if (pApp->m_bReadOnlyAccess)
	{
		// return without calling Skip(). Beep for read-only feedback
		int keyCode = event.GetKeyCode();
		if (keyCode == WXK_DOWN || keyCode == WXK_UP || keyCode == WXK_LEFT || keyCode == WXK_RIGHT
			|| keyCode == WXK_PAGEUP || keyCode == WXK_PAGEDOWN
			|| keyCode == WXK_CONTROL || keyCode == WXK_ALT || keyCode == WXK_SHIFT
			|| keyCode == WXK_ESCAPE || keyCode == WXK_TAB || keyCode == WXK_BACK
			|| keyCode == WXK_RETURN || keyCode == WXK_DELETE || keyCode == WXK_SPACE
			|| keyCode == WXK_HOME || keyCode == WXK_END || keyCode == WXK_INSERT
			|| keyCode == WXK_F8)
		{
			; // don't beep for the various keys above
		}
		else
		{
			::wxBell();
		}
		// don't pass on the key stroke - don't call Skip()
		return;
	}

	if (!pApp->m_bSingleStep)
	{
		// halt the auto matching and inserting, if a key is typed
		if (pApp->m_bAutoInsert)
		{
			pApp->m_bAutoInsert = FALSE;
			// Skip() should not be called here, because we can ignore the value of
			// any keystroke that was made to stop auto insertions.
			return;
		}
	}

	// update status bar with project name
	pApp->RefreshStatusBarInfo();

	//event.Skip(); //CEdit::OnKeyDown(nChar, nRepCnt, nFlags);

	// [MFC Note] Delete key sends WM_KEYDOWN message only, WM_CHAR not sent
	// so we need to update things for forward deletions here (and it has to be
	// done after the call to the base class, otherwise the last deleted character
	// remains in the final target phrase text)
	// BEW added more on 20June06, so that a DEL keypress in the box when it has a <no adaptation>
	// KB or GKB entry will effect returning the CSourcePhrase instance back to a true "empty" one
	// (ie. m_bHasKBEntry or m_bHasGlossingKBEntry will be cleared, depending on current mode, and
	// the relevant KB's CRefString decremented in the count, or removed entirely if the count is 1.)
	// wx Note: In contrast to Bruce's note above OnKeyDown() in wx Delete key does trigger
	// OnChar()
	CPile* pActivePile = pView->GetPile(pApp->m_nActiveSequNum); // doesn't rely on pApp->m_pActivePile
																 // having been set already
	CSourcePhrase* pSrcPhrase = pActivePile->GetSrcPhrase();
	if (event.GetKeyCode() == WXK_DELETE)
	{
		if (pSrcPhrase->m_adaption.IsEmpty() && ((pSrcPhrase->m_bHasKBEntry && !gbIsGlossing) ||
			(pSrcPhrase->m_bHasGlossingKBEntry && gbIsGlossing)))
		{
			gbNoAdaptationRemovalRequested = TRUE;
			wxString emptyStr = _T("");
			if (gbIsGlossing)
				pApp->m_pGlossingKB->GetAndRemoveRefString(pSrcPhrase,emptyStr,useGlossOrAdaptationForLookup);
			else
				pApp->m_pKB->GetAndRemoveRefString(pSrcPhrase,emptyStr,useGlossOrAdaptationForLookup);
		}
		else
		{
			// legacy behaviour: handle the forward character deletion
			m_backspaceUndoStr.Empty();
			gnSaveStart = -1;
			gnSaveEnd = -1;

			wxString s;
			s = GetValue();
			pApp->m_targetPhrase = s; // otherwise, deletions using <DEL> key are not recorded
		}
	}
	else if (event.GetKeyCode() == WXK_ESCAPE)
	{
		// user pressed the Esc key. If a Guess is current in the phrasebox we
		// will remove the Guess-highlight background color and the guess, and
		// restore the normal copied source word to the phrasebox. We also reset
		// the App's m_bIsGuess flag to FALSE.
		if (this->GetBackgroundColour() == pApp->m_GuessHighlightColor)
		{
			// get the pSrcPhrase at this active location
			CSourcePhrase* pSrcPhrase;
			pSrcPhrase = pApp->m_pActivePile->GetSrcPhrase();
			//wxString str = pSrcPhrase->m_key; // BEW 27Bov14 deprecated, in favour of using the 
			// stored pre-guesser version of the string, because it may have been modified
			wxString str = pApp->m_preGuesserStr; // use this as the default restore string

			// It was Roland Fumey in 16July08 that wanted strings other than the source text
			// to be used for the restoration, if gbLegacySourceTextCopy was not in effect, so
			// keep the following as it is the protocol he requested
			if (!gbLegacySourceTextCopy)
			{
				// the user wants smart copying done to the phrase box when the active location
				// landed on does not have any existing adaptation (in adapting mode), or, gloss
				// (in glossing mode). In the former case, it tries to copy a gloss to the box
				// if a gloss is available, otherwise source text used instead; in the latter case
				// it tries to copy an adaptation as the default gloss, if an adaptation is
				// available, otherwise source text is used instead
				if (gbIsGlossing)
				{
					if (!pSrcPhrase->m_adaption.IsEmpty())
					{
						str = pSrcPhrase->m_adaption;
					}
				}
				else
				{
					if (!pSrcPhrase->m_gloss.IsEmpty())
					{
						str = pSrcPhrase->m_gloss;
					}
				}
			}
			this->ChangeValue(str);
			pApp->m_targetPhrase = str; // Required, otherwise the guess persists and gets used in auto-inserts subsequently
			this->SetBackgroundColour(wxColour(255,255,255)); // white;
			this->m_bAbandonable = TRUE;
			pApp->m_bIsGuess = FALSE;
			pApp->m_preGuesserStr.Empty(); // clear this to empty, it's job is done
			this->Refresh();
			return;
		}
	}
	event.Skip(); // allow processing of the keystroke event to continue
}

// BEW 26Mar10, some changes needed for support of doc version 5
// BEW 21Jun10, no changes needed for support of kbVersion 2
bool CPhraseBox::ChooseTranslation(bool bHideCancelAndSelectButton)
{
	// refactored 2Apr09
	// update status bar with project name
	CAdapt_ItApp* pApp = (CAdapt_ItApp*)&wxGetApp();
	CAdapt_ItDoc* pDoc;
	CAdapt_ItView* pView;
	CPhraseBox* pBox;
	pApp->GetBasePointers(pDoc,pView,pBox);
	if (pView == NULL)
	{
		m_bCancelAndSelectButtonPressed = FALSE;
		return FALSE;
	}
	wxASSERT(pView->IsKindOf(CLASSINFO(wxView)));
	CPile* pActivePile = pView->GetPile(pApp->m_nActiveSequNum); // doesn't rely on m_pActivePile
																 // having been set
	// BEW added 21May15, if user wants canvas freezing/unfreezing to suppress blinking, then
	// control can get to hear with the canvas frozen - so test, and if so, unfreeze (and redraw
	// if necessary - but appears to not be necessary) so the user can see the dialog against 
	// the document as background context
	if (pApp->m_bSupportFreeze && pApp->m_bIsFrozen)
	{
		pApp->m_nInsertCount = 0;
		pView->canvas->Thaw();
		pApp->m_bIsFrozen = FALSE;
		// don't need a delay here
		if (pApp->m_nCurDelay == 31)
		{
			pApp->m_nCurDelay = 0; // set back to zero
		}
	}

	CChooseTranslation dlg(pApp->GetMainFrame());
	dlg.Centre();

	// update status bar with project name
	pApp->RefreshStatusBarInfo();

	// initialize m_chosenTranslation, other initialization is in InitDialog()
	dlg.m_chosenTranslation.Empty();
	dlg.m_bHideCancelAndSelectButton = bHideCancelAndSelectButton; // defaults to FALSE if
																   // not set in caller
	// put up the dialog
	gbInspectTranslations = FALSE;
	if(dlg.ShowModal() == wxID_OK)
	{
		gbUserCancelledChooseTranslationDlg = FALSE;

		// set the translation static var from the member m_chosenTranslation
		translation = dlg.m_chosenTranslation;

		// do a case adjustment if necessary
		bool bNoError = TRUE;
		if (gbAutoCaps)
		{
			//bNoError = pView->SetCaseParameters(pApp->m_pActivePile->m_pSrcPhrase->m_key);
			bNoError = pApp->GetDocument()->SetCaseParameters(pActivePile->GetSrcPhrase()->m_key);
			if (bNoError && gbSourceIsUpperCase)
			{
				bNoError = pApp->GetDocument()->SetCaseParameters(translation, FALSE);
				if (bNoError && !gbNonSourceIsUpperCase && (gcharNonSrcUC != _T('\0')))
				{
					translation.SetChar(0, gcharNonSrcUC);
				}
			}
		}
		// bw added block above 16Aug04
		pView->RemoveSelection();
		return TRUE;
	}
	else
	{
		// must have hit Cancel button, or the Cancel And Select button
		if (dlg.m_bCancelAndSelect)
		{
			// set the private member boolean
			m_bCancelAndSelectButtonPressed = TRUE;
		}
		else
		{
			// clear the private member boolean
			m_bCancelAndSelectButtonPressed = FALSE;
		}

		// we have to undo any merge, but only provided the unmerge has not already
		// been done in the OnButtonRestore() function; a merge can only have been done
		// if adapting is current, so suppress the unmerge if glossing is current
		if (!gbIsGlossing && gbMergeDone && !gbUnmergeJustDone)
		{
			gbMergeDone = FALSE;
			gbSuppressLookup = TRUE; // don't want LookUpSrcWord() called from
									 // OnButtonRestore() because
			pView->UnmergePhrase();  // UnmergePhrase() otherwise calls LookUpSrcWord()
									 // which calls ChooseTranslation()
			gbSuppressMergeInMoveToNextPile = FALSE; // reinstate it, 20May09
		}
		else
		{
			if (gbIsGlossing)
			{
				// these should not be necessary, but will keep things safe when glossing
				gbMergeDone = FALSE;
				gbSuppressLookup = TRUE;
				gbSuppressMergeInMoveToNextPile = FALSE; // reinstate it, 20May09
			}
		}

		gbUserCancelledChooseTranslationDlg = TRUE; // use in MoveToNextPile() to
							// suppress a second showing of dialog from LookUpSrcWord()
		pView->RemoveSelection();
		return FALSE;
	}
}

void CPhraseBox::SetModify(bool modify)
{
	if (modify)
		MarkDirty(); // "mark text as modified (dirty)"
	else
		DiscardEdits(); // "resets the internal 'modified' flag
						// as if the current edits had been saved"
}

bool CPhraseBox::GetModify()
{
	return IsModified();
}


// the pPile pointer passed it must be the pointer to the active pile
// BEW 26Mar10 changes needed for support of doc version 5
void CPhraseBox::DoCancelAndSelect(CAdapt_ItView* pView, CPile* pPile)
{
	// refactored 2Apr09
	if (gbIsGlossing) return; // unneeded,  but ensures correct behaviour
							  // if I goofed elsewhere
	CSourcePhrase* pSrcPhrase = pPile->GetSrcPhrase();
	// BEW changed 3Aug09 to always have the selection line for Find Next be the source
	// text line - this makes the interface more consistent
	int nSelLineIndex = 0;
    // BEW changed 26Mar10, internally it tests bDoRecalcLayout and so it will do the
    // RecalcLayout(), resetting of active sequ num, and the active pile to that location
    // if TRUE. The m_bCancelAndSelectButtonPressed value passed in needs to be TRUE
    //
	// BEW added protection, 14May10; a hard-coded 2 will cause crash if the document has
	// only a single word and bad initialization has the m_targetBox's
	// m_bCancelAndSelectButtonPressed value set TRUE, since there are not enough piles to
	// be able to select 2 in that case!
	CAdapt_ItApp* pApp = &wxGetApp();
	if (pSrcPhrase->m_nSequNumber < pApp->GetMaxIndex())
	{
		pView->MakeSelectionForFind(pSrcPhrase->m_nSequNumber,2,nSelLineIndex,
									m_bCancelAndSelectButtonPressed);
	}
	m_bCancelAndSelectButtonPressed = FALSE; // must clear to default FALSE immediately
}

// BEW 13Apr10, no changes needed for support of doc version 5
void CPhraseBox::OnLButtonDown(wxMouseEvent& event)
{
	// This mouse event is only activated when user clicks mouse L button within
	// the phrase box, not elsewhere on the screen
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);

	// whm added 15Mar12. When in read-only mode don't register any key strokes
	if (pApp->m_bReadOnlyAccess)
	{
		// return without calling Skip(). Beep for read-only feedback
		::wxBell();
		return;
	}

    // whm addition: don't allow cursor to be placed in phrasebox when in free trans mode
    // and when it is not editable. Allows us to have a pink background in the phrase box
    // in free trans mode.
    // TODO? we could also make the text grayed out to more closely immulate MFC Windows
    // behavior (we could call Enable(FALSE) but that not only turns the text grayed out,
    // but also the background gray instead of the desired pink. It is better to do this
    // here than in OnLButtonUp since it prevents the cursor from being momemtarily seen in
    // the phrase box if clicked.
	if (pApp->m_bFreeTranslationMode && !this->IsEditable())
	{
		CMainFrame* pFrame;
		pFrame = pApp->GetMainFrame();
		wxASSERT(pFrame != NULL);
		wxASSERT(pFrame->m_pComposeBar != NULL);
		wxTextCtrl* pEdit = (wxTextCtrl*)
							pFrame->m_pComposeBar->FindWindowById(IDC_EDIT_COMPOSE);
		wxASSERT(pEdit != NULL);
		pEdit->SetFocus();
		int len = pEdit->GetValue().Length();
		if (len > 0)
		{
			pEdit->SetSelection(len,len);
		}
		::wxBell();
		// don't call Skip() as we don't want the mouse click processed elsewhere
		return;
	}
    // version 1.4.2 and onwards, we want a right or left arrow used to remove the
    // phrasebox's selection to be considered a typed character, so that if a subsequent
    // selection and merge is done then the first target word will not get lost; and so we
    // add a block of code also to start of OnChar( ) to check for entry with both
    // m_bAbandonable and m_bUserTypedSomething set TRUE, in which case we don't clear
    // these flags (the older versions cleared the flags on entry to OnChar( ) )

    // we use this flag cocktail elsewhere to test for these values of the three flags as
    // the condition for telling the application to retain the phrase box's contents when
    // user deselects target word, then makes a phrase and merges by typing
	m_bAbandonable = FALSE;
	pApp->m_bUserTypedSomething = TRUE;
	gbRetainBoxContents = TRUE;
	event.Skip();
	GetSelection(&pApp->m_nStartChar,&pApp->m_nEndChar);
}

// BEW 13Apr10, no changes needed for support of doc version 5
void CPhraseBox::OnLButtonUp(wxMouseEvent& event)
{
	CAdapt_ItApp* pApp = &wxGetApp();
	// This mouse event is only activated when user clicks mouse L button within
	// the phrase box, not elsewhere on the screen
	event.Skip();
	GetSelection(&pApp->m_nStartChar,&pApp->m_nEndChar);
}

// return TRUE if we made a match and there is a translation to be inserted (see static var
// below); return FALSE if there was no match; based on LookAhead(), but only looks up a
// single src word at pNewPile; assumes that the app member, m_nActiveSequNum is set and
// that the CPile which is at that index is the pNewPile which was passed in
// BEW 26Mar10, no changes needed for support of doc version 5
// BEW 21Jun10: simplified signature
// BEW 21Jun10: changed to support kbVersion 2's m_bDeleted flag
// BEW 6July10, added test for converting a looked-up <Not In KB> string to an empty string
bool CPhraseBox::LookUpSrcWord(CPile* pNewPile)
{
	// refactored 2Apr09
	CAdapt_ItApp* pApp = &wxGetApp();
	CAdapt_ItView *pView = pApp->GetView(); // <<-- BEWARE if we later have multiple views/panes
	CLayout* pLayout = pApp->m_pLayout;
	wxString strNot = _T("<Not In KB>");
	//int	nNewSequNum; // set but not used
	//nNewSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber; // sequ number at the new location
	wxString	phrases[1]; // store built phrases here, for testing
							// against KB stored source phrases
	//int	numPhrases; // set but not used
	//numPhrases = 1;  // how many phrases were built in any one call of this function
	translation.Empty(); // clear the static variable, ready for a new translation
						 // if one can be found
	nWordsInPhrase = 0;	  // assume no match
	gbByCopyOnly = FALSE; // restore default setting

	//wxLogDebug(_T("4967 near start of LookUpSrcWord(), m_bCancelAndSelectButtonPressed = %d"),
	//	pApp->m_pTargetBox->GetCancelAndSelectFlag());

	// we should never have an active selection at this point, so ensure it
	pView->RemoveSelection();

	// get the source word
	phrases[0] = pNewPile->GetSrcPhrase()->m_key;
	// BEW added 08Sep08: to prevent spurious words being inserted at the
	// end of a long retranslation when  mode is glossing mode
	if (pNewPile->GetSrcPhrase()->m_key == _T("..."))
	{
		// don't allow an ellipsis (ie. placeholder) to trigger an insertion,
		// leave translation empty
		translation.Empty();
		return TRUE;
	}

    // check this phrase (which is actually a single word), attempting to find a match in
    // the KB (if glossing, it might be a single word or a phrase, depending on what user
    // did earlier at this location before he turned glossing on)
	CKB* pKB;
	if (gbIsGlossing)
		pKB = pApp->m_pGlossingKB;
	else
		pKB = pApp->m_pKB;
	CTargetUnit* pTargetUnit = (CTargetUnit*)NULL;
	int index = 0;
	bool bFound = FALSE;
	// index is always 0 in this function, so lookup is only in the first map
	bFound = pKB->FindMatchInKB(index + 1, phrases[index], pTargetUnit);

	// if no match was found, we return immediately with a return value of FALSE
	if (!bFound)
	{
		pCurTargetUnit = (CTargetUnit*)NULL; // the global pointer must be cleared
		curKey.Empty(); // global var curKey not needed, so clear it
		return FALSE;
	}
	pCurTargetUnit = pTargetUnit; // set global pointer so the dialog can use it if it is called
	curKey = phrases[index]; // set the global curKey so the dialog can use it if it is called
							 // (phrases[0] is copied for the lookup, so curKey has initial case
							 // setting as in the doc's sourcephrase instance; we don't need
							 // to change it here (if ChooseTranslation( ) is called, it does
							 // any needed case changes internally)
	nWordsInPhrase = index + 1; // static variable, needed for merging source phrases in
								// the caller
    // BEW 21Jun10, for kbVersion 2 support, count the number of non-deleted CRefString
    // instances stored on this pTargetUnit
    int count =  pTargetUnit->CountNonDeletedRefStringInstances();
	if (count == 0)
	{
		// nothing in the KB for this key (except, possibly, one or more deleted
		// CRefString instances)
		pCurTargetUnit = (CTargetUnit*)NULL;
		curKey.Empty();
		return FALSE;
	}

	// we found a target unit in the list with a matching m_key field,
	// so we must now set the static var translation to the appropriate adaptation text: this
	// will be the target unit's first entry in its list if the list has only one entry, else
	// we must present the user with a dialog to put up all possible adaptations for the user to
	// choose one, or type a new one, or reject all - in which case we return FALSE for manual
	// typing of an adaptation etc.
	// BEW 21Jun10, changed to support kbVersion 2's m_bDeleted flag. It is now possible
	// that a CTargetUnit have just a single CRefString instance and the latter has its
	// m_bDeleted flag set TRUE. In this circumstance, matching this is to be regarded as
	// a non-match, and the function then would need to return FALSE for a manual typing
	// of the required adaptation (or gloss)
	TranslationsList::Node* pos = pTargetUnit->m_pTranslations->GetFirst();
	wxASSERT(pos != NULL);

	wxASSERT(count > 0);
	if (count > 1 || pTargetUnit->m_bAlwaysAsk)
	{
		// move view to new location and select, so user has visual feedback)
		// about which element(s) is/are involved
		pView->RemoveSelection();

		// next code is taken from end of MoveToNextPile()
		// initialize the phrase box to be empty, so as not to confuse the user
		ChangeValue(_T(""));
		pApp->m_targetPhrase = _T("");

		// recalculate the layout
#ifdef _NEW_LAYOUT
		pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
#else
		pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
		// get the new active pile
		pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
		wxASSERT(pApp->m_pActivePile != NULL);

		// scroll into view
		pApp->GetMainFrame()->canvas->ScrollIntoView(pApp->m_nActiveSequNum);

		// make what we've done visible
		pView->Invalidate();
		pLayout->PlaceBox();

		// put up a dialog for user to choose translation from a list box, or type
		// new one (and set bHideCancelAndSelectButton boolean to TRUE)
		// BEW changed 02Oct08: I don't see any good reason why user should be prevented
		// from overriding lookup of a single word by going immediately to a selection for
		// merger, so I've made the button be shown in this situation
		// BEW 29Jul11; added test of gbUserCancelledChooseTranslationDlg here to suppress
		// the 2nd call of ChooseTranslation() that this would be if the user chose to
		// Cancel earlier from the dialog. Without it, Cancel takes to Cancel button presses.
		if (gbUserCancelledChooseTranslationDlg)
		{
			// prohibit the lookup
			gbUserCancelledChooseTranslationDlg = FALSE; // restore default value
			return FALSE; // user cancelled, for a 'non-match' result
		}
		else
		{
			bool bOK = pApp->m_pTargetBox->ChooseTranslation(); // default for param bHideCancelAndSelectButton is FALSE
			pCurTargetUnit = (CTargetUnit*)NULL; // ensure the global var is cleared
												 // after the dialog has used it
			curKey.Empty(); // ditto for the current key string (global)
			if (!bOK)
			{
				// user cancelled, so return FALSE for a 'non-match' situation; the
				// m_bCancelAndSelectButtonPressed private member variable (set from
				// CChooseTranslation's m_bCancelAndSelect member) will have already been set
				// (if relevant) and can be used in the caller (ie. in MoveToNextPile)
				return FALSE;
			}
			// if bOK was TRUE, translation static var will have been set via the dialog; and
			// if autocaps is ON and a change to upper case was needed, it will not have been done
			// within the dialog's handler code
		}
	}
	else
	{
		// BEW 21Jun10, count has the value 1, but there could be deleted CRefString
		// intances also, so we must search to find the first non-deleted instance
		CRefString* pRefStr = NULL;
		while (pos != NULL)
		{
			pRefStr = pos->GetData();
			pos = pos->GetNext();
			if (!pRefStr->GetDeletedFlag())
			{
				// the adaptation string returned could be a "<Not In KB>" string, which
				// is something which never must be put into the phrase box, so check for
				// this and change to an empty string if that was what was fetched by the
				// lookup
				translation = pRefStr->m_translation;
				if (translation == strNot)
				{
					// change "<Not In KB>" to an empty string
					translation.Empty();
				}
				break;
			}
		}
	}

	// adjust for case, if necessary; this algorithm will not make a lower case string start
	// with upper case when the source is uppercase if the user types punctuation at the start
	// of the string. The latter is, however, unlikely - provided the auto punctuation support
	// is utilized by the user
	if (gbAutoCaps && gbSourceIsUpperCase)
	{
		bool bNoError = pApp->GetDocument()->SetCaseParameters(translation, FALSE);
		if (bNoError && !gbNonSourceIsUpperCase && (gcharNonSrcUC != _T('\0')))
		{
			// make it upper case
			translation.SetChar(0, gcharNonSrcUC);
		}
	}
	return TRUE;
}


// return TRUE if we made a match and there is a translation to be inserted (see static var
// below); return FALSE if there was no match; based on LookAhead(), but only looks up a
// single src word at pNewPile; assumes that the app member, m_nActiveSequNum is set and
// that the CPile which is at that index is the pNewPile which was passed in
// BEW 26Mar10, no changes needed for support of doc version 5
/*bool CPhraseBox::LookUpSrcWord(CAdapt_ItView *pView, CPile* pNewPile)
{
	// refactored 2Apr09
	CAdapt_ItApp* pApp = &wxGetApp();
	wxASSERT(pApp != NULL);
	CLayout* pLayout = GetLayout();
	int	nNewSequNum;
	nNewSequNum = pNewPile->GetSrcPhrase()->m_nSequNumber; // sequ number at the new location
	wxString	phrases[1]; // store built phrases here, for testing
							// against KB stored source phrases
	int	numPhrases;
	numPhrases = 1;  // how many phrases were built in any one call of this function
	translation.Empty(); // clear the static variable, ready for a new translation
						 // if one can be found
	nWordsInPhrase = 0;	  // assume no match
	gbByCopyOnly = FALSE; // restore default setting

	// we should never have an active selection at this point, so ensure it
	pView->RemoveSelection();

	// get the source word
	phrases[0] = pNewPile->GetSrcPhrase()->m_key;
	// BEW added 08Sep08: to prevent spurious words being inserted at the
	// end of a long retranslation when  mode is glossing mode
	if (pNewPile->GetSrcPhrase()->m_key == _T("..."))
	{
		// don't allow an ellipsis (ie. placeholder) to trigger an insertion,
		// leave translation empty
		translation.Empty();
		return TRUE;
	}

    // check this phrase (which is actually a single word), attempting to find a match in
    // the KB (if glossing, it might be a single word or a phrase, depending on what user
    // did earlier at this location before he turned glossing on)
	CKB* pKB;
	if (gbIsGlossing)
		pKB = pApp->m_pGlossingKB;
	else
		pKB = pApp->m_pKB;
	CTargetUnit* pTargetUnit = (CTargetUnit*)NULL;
	int index = 0;
	bool bFound = FALSE;
	bFound = pKB->FindMatchInKB(index + 1, phrases[index], pTargetUnit);

	// if no match was found, we return immediately with a return value of FALSE
	if (!bFound)
	{
		pCurTargetUnit = (CTargetUnit*)NULL; // the global pointer must be cleared
		curKey.Empty(); // global var curKey not needed, so clear it
		return FALSE;
	}
	pCurTargetUnit = pTargetUnit; // set global pointer so the dialog can use it if it is called
	curKey = phrases[index]; // set the global curKey so the dialog can use it if it is called
							 // (phrases[0] is copied for the lookup, so curKey has initial case
							 // setting as in the doc's sourcephrase instance; we don't need
							 // to change it here (if ChooseTranslation( ) is called, it does
							 // any needed case changes internally)
	nWordsInPhrase = index + 1; // static variable, needed for merging source phrases in
								// the caller
    // BEW 21Jun10, for kbVersion 2 support, count the number of non-deleted CRefString
    // instances stored on this pTargetUnit
    int count =  pKB->CountNonDeletedRefStringInstances(pTargetUnit);
	if (count == 0)
	{
		// nothing in the KB for this key (except, possibly, one or more deleted
		// CRefString instances)
		pCurTargetUnit = (CTargetUnit*)NULL;
		curKey.Empty();
		return FALSE;
	}

	// we found a target unit in the list with a matching m_key field,
	// so we must now set the static var translation to the appropriate adaptation text: this
	// will be the target unit's first entry in its list if the list has only one entry, else
	// we must present the user with a dialog to put up all possible adaptations for the user to
	// choose one, or type a new one, or reject all - in which case we return FALSE for manual
	// typing of an adaptation etc.
	// BEW 21Jun10, changed to support kbVersion 2's m_bDeleted flag. It is now possible
	// that a CTargetUnit have just a single CRefString instance and the latter has its
	// m_bDeleted flag set TRUE. In this circumstance, matching this is to be regarded as
	// a non-match, and the function then would need to return FALSE for a manual typing
	// of the required adaptation (or gloss)
	TranslationsList::Node* pos = pTargetUnit->m_pTranslations->GetFirst();
	wxASSERT(pos != NULL);

	wxASSERT(count > 0);
	if (count > 1 || pTargetUnit->m_bAlwaysAsk)
	{
		// move view to new location and select, so user has visual feedback)
		// about which element(s) is/are involved
		pView->RemoveSelection();

		// next code is taken from end of MoveToNextPile()
		// initialize the phrase box to be empty, so as not to confuse the user
		ChangeValue(_T(""));
		pApp->m_targetPhrase = _T("");

		// recalculate the layout
#ifdef _NEW_LAYOUT
		pLayout->RecalcLayout(pApp->m_pSourcePhrases, keep_strips_keep_piles);
#else
		pLayout->RecalcLayout(pApp->m_pSourcePhrases, create_strips_keep_piles);
#endif
		// get the new active pile
		pApp->m_pActivePile = pView->GetPile(pApp->m_nActiveSequNum);
		wxASSERT(pApp->m_pActivePile != NULL);

		// scroll into view
		pApp->GetMainFrame()->canvas->ScrollIntoView(pApp->m_nActiveSequNum);

		// make what we've done visible
		pView->Invalidate();
		pLayout->PlaceBox();

		// put up a dialog for user to choose translation from a list box, or type
		// new one (and set bHideCancelAndSelectButton boolean to TRUE)
		// BEW changed 02Oct08: I don't see any good reason why user should be prevented
		// from overriding lookup of a single word by going immediately to a selection for
		// merger, so I've made the button be shown in this situation
		bool bOK = ChooseTranslation(); // default for param bHideCancelAndSelectButton is FALSE
		pCurTargetUnit = (CTargetUnit*)NULL; // ensure the global var is cleared
											 // after the dialog has used it
		curKey.Empty(); // ditto for the current key string (global)
		if (!bOK)
		{
            // user cancelled, so return FALSE for a 'non-match' situation; the
            // m_bCancelAndSelectButtonPressed private member variable (set from
            // CChooseTranslation's m_bCancelAndSelect member) will have already been set
            // (if relevant) and can be used in the caller (ie. in MoveToNextPile)
			return FALSE;
		}
		// if bOK was TRUE, translation static var will have been set via the dialog; and
		// if autocaps is ON and a change to upper case was needed, it will not have been done
		// within the dialog's handler code
	}
	else
	{
		// BEW 21Jun10, count has the value 1, but there could be deleted CRefString
		// intances also, so we must search to find the first non-deleted instance
		CRefString* pRefStr = NULL;
		while (pos != NULL)
		{
			pRefStr = pos->GetData();
			pos = pos->GetNext();
			if (!pRefStr->GetDeletedFlag())
			{
				translation = pRefStr->m_translation;
				break;
			}
		}
	}

	// adjust for case, if necessary; this algorithm will not make a lower case string start
	// with upper case when the source is uppercase if the user types punctuation at the start
	// of the string. The latter is, however, unlikely - provided the auto punctuation support
	// is utilized by the user
	if (gbAutoCaps && gbSourceIsUpperCase)
	{
		bool bNoError = pApp->GetDocument()->SetCaseParameters(translation, FALSE);
		if (bNoError && !gbNonSourceIsUpperCase && (gcharNonSrcUC != _T('\0')))
		{
			// make it upper case
			translation.SetChar(0, gcharNonSrcUC);
		}
	}
	return TRUE;
}
*/
// BEW 13Apr10, no changes needed for support of doc version 5
void CPhraseBox::OnEditUndo(wxCommandEvent& WXUNUSED(event))
// no changes needed for support of glossing or adapting
{
    // We have to implement undo for a backspace ourselves, but for delete & edit menu
    // commands the CEdit Undo() will suffice; we use a non-empty m_backspaceUndoStr as a
    // flag that the last edit operation was a backspace
	CAdapt_ItApp* pApp = GetLayout()->m_pApp;
	CAdapt_ItView* pView = pApp->GetView();


	if (m_backspaceUndoStr.IsEmpty())
	{
		// last operation was not a <BS> keypress,
		// so Undo() will be done instead
		;
	}
	else
	{
		if (!(gnSaveStart == -1 || gnSaveEnd == -1))
		{
			bool bRestoringAll = FALSE;
			wxString thePhrase;
			thePhrase = GetValue();
			int undoLen = m_backspaceUndoStr.Length();
			if (!thePhrase.IsEmpty())
			{
				thePhrase = InsertInString(thePhrase,(int)gnSaveEnd,m_backspaceUndoStr);
			}
			else
			{
				thePhrase = m_backspaceUndoStr;
				bRestoringAll = TRUE;
			}

			// rebuild the box the correct size
			wxSize textExtent;
			bool bWasMadeDirty = TRUE;
			FixBox(pView,thePhrase,bWasMadeDirty,textExtent,1); // selector = 1 for using
																// thePhrase's extent
			// restore the box contents
			ChangeValue(thePhrase);
			m_backspaceUndoStr.Empty(); // clear, so it can't be mistakenly undone again

			// fix the cursor location
			if (bRestoringAll)
			{
				pApp->m_nStartChar = -1;
				pApp->m_nEndChar = -1;
				SetSelection(pApp->m_nStartChar,pApp->m_nEndChar); // all selected
			}
			else
			{
				pApp->m_nStartChar = pApp->m_nEndChar = (int)(gnSaveStart + undoLen);
				SetSelection(pApp->m_nStartChar,pApp->m_nEndChar);
			}
		}
	}
}

// DoStore_ForPlacePhraseBox added 3Apr09; it factors out some of the incidental
// complexity in the PlacePhraseBox() function, making the latter's design more
// transparent and the function shorter
// BEW 22Feb10, no changes needed for support of doc version 5
// BEW 21Jun10, no changes needed for support of kbVersion 2
// BEW 17Jul11, changed for GetRefString() to return KB_Entry enum, and use all 10 maps
// for glossing KB
bool CPhraseBox::DoStore_ForPlacePhraseBox(CAdapt_ItApp* pApp, wxString& targetPhrase)
{
	CAdapt_ItDoc* pDoc = pApp->GetDocument();
	bool bOK = TRUE;
	CRefString* pRefStr = NULL;
	KB_Entry rsEntry;
	if (gbIsGlossing)
	{
		if (targetPhrase.IsEmpty())
			pApp->m_pActivePile->GetSrcPhrase()->m_gloss = targetPhrase;

		// store will fail if the user edited the entry out of the glossing KB, since it
		// cannot know which srcPhrases will be affected, so these will still have their
		// m_bHasKBEntry set true. We have to test for this, ie. a null pRefString but
		// the above flag TRUE is a sufficient test, and if so, set the flag to FALSE
		rsEntry = pApp->m_pGlossingKB->GetRefString(pApp->m_pActivePile->GetSrcPhrase()->m_nSrcWords,
								pApp->m_pActivePile->GetSrcPhrase()->m_key, targetPhrase, pRefStr);
		if ((pRefStr == NULL || rsEntry == present_but_deleted) &&
			pApp->m_pActivePile->GetSrcPhrase()->m_bHasGlossingKBEntry)
		{
			pApp->m_pActivePile->GetSrcPhrase()->m_bHasGlossingKBEntry = FALSE;
		}
		bOK = pApp->m_pGlossingKB->StoreText(pApp->m_pActivePile->GetSrcPhrase(), targetPhrase);
	}
	else // is adapting
	{
		if (targetPhrase.IsEmpty())
			pApp->m_pActivePile->GetSrcPhrase()->m_adaption = targetPhrase;
		// re-express the punctuation
		pApp->GetView()->MakeTargetStringIncludingPunctuation(pApp->m_pActivePile->GetSrcPhrase(), targetPhrase);
		pApp->GetView()->RemovePunctuation(pDoc, &targetPhrase, from_target_text);

		// the store will fail if the user edited the entry out of the KB, as the latter
		// cannot know which srcPhrases will be affected, so these will still have their
		// m_bHasKBEntry set true. We have to test for this, ie. a null pRefString but
		// the above flag TRUE is a sufficient test, and if so, set the flag to FALSE
		rsEntry = pApp->m_pKB->GetRefString(pApp->m_pActivePile->GetSrcPhrase()->m_nSrcWords,
								pApp->m_pActivePile->GetSrcPhrase()->m_key, targetPhrase, pRefStr);
		if ((pRefStr == NULL || rsEntry == present_but_deleted) &&
			pApp->m_pActivePile->GetSrcPhrase()->m_bHasKBEntry)
		{
			pApp->m_pActivePile->GetSrcPhrase()->m_bHasKBEntry = FALSE;
		}
		gbInhibitMakeTargetStringCall = TRUE;
		bOK = pApp->m_pKB->StoreText(pApp->m_pActivePile->GetSrcPhrase(), targetPhrase);
		gbInhibitMakeTargetStringCall = FALSE;
	}
	return bOK;
}

// BEW refactored 21Jul14 for support of ZWSP storage and replacement;
// also moved the definition to be in PhraseBox.h & .cpp (was in view class)
void CPhraseBox::RemoveFinalSpaces(CPhraseBox* pBox, wxString* pStr)
{
	// empty strings don't need anything done
	if (pStr->IsEmpty())
		return;

	// remove any phrase final space characters
	bool bChanged = FALSE;
	int len = pStr->Length();
	int nIndexLast = len-1;
	// BEW 21Jul14 refactored for ZWSP support. The legacy code can be left unchanged for
	// handling latin space; but for exotic spaces we'll use the overridden
	// RemoveFinalSpaces() as its for any string - so test here for what is at the end.
	// We'll assume the end doesn't have a mix of latin space with exotic ones
	if (pStr->GetChar(nIndexLast) == _T(' '))
	{
		// Latin space is at the end, so do the legacy code
		do {
			if (pStr->GetChar(nIndexLast) == _T(' '))
			{
				// Note: wsString::Remove must have the second param as 1 here otherwise
				// it will truncate the remainder of the string!
				pStr->Remove(nIndexLast,1);
				// can't trust the Remove's returned value, it exceeds string length by one
				len = pStr->Length();
				nIndexLast = len -1;
				bChanged = TRUE;
			}
			else
			{
				break;
			}
		} while (len > 0 && nIndexLast > -1);
	}
	else
	{
		// There is no latin space at the end, but there might be one or more exotic ones,
		// such as ZWSP. (We'll assume there's no latin spaces mixed in with them)
		wxChar lastChar = pStr->GetChar(nIndexLast);
		CAdapt_ItApp* pApp = &wxGetApp();
		CAdapt_ItDoc* pDoc = pApp->GetDocument();
		if (pDoc->IsWhiteSpace(&lastChar))
		{
			// There must be at least one exotic space at the end, perhaps a ZWSP
			bChanged = TRUE;
			wxString revStr = *pStr; // it's not yet reversed, but will be in the next call
									 // and restored to non-reversed order before its returned
			RemoveFinalSpaces(revStr); // signature is ref to wxString

			*pStr = revStr;
			// pBox will have had its contents changed by at least one wxChar being
			// chopped off the end, so let the bChanged block below do the phrasebox update
		}
		else
		{
			// There is no exotic space at the end either, so pStr needs nothing removed,
			// so just return without changing the phrasebox contents
			return;
		}
	}
	if (bChanged) // need to do this, because for some reason rubbish is getting
            // left in the earlier box when the ChooseTranslation dialog gets put up. That
            // is, a simple call of SetWindowText with parameter pStr cast to (const char
            // *) doesn't work right; but the creation & setting of str below fixes it
	{
		wxString str = *pStr;
		pBox->ChangeValue(str);
	}
}

// BEW added 30Apr08, an overloaded version which deletes final spaces in any CString's
// text, and if there are only spaces in the string, it reduces it to an empty string
// BEW 21Jul14, refactored to also remove ZWSP and other exotic white spaces from end of
// the string as well; and moved to be in PhaseBox.h & .cpp (was in view class)
void CPhraseBox::RemoveFinalSpaces(wxString& rStr)
{
    // whm Note: This could be done with a single line in wx, i.e., rStr.Trim(TRUE), but
    // we'll go with the MFC version for now.
	if (rStr.IsEmpty())
		return;
	rStr = MakeReverse(rStr);
	wxChar chFirst = rStr[0];
	if (chFirst == _T(' '))
	{
		// The legacy code - just remove latin spaces, we'll assume that when this is apt,
		// there are no exotics there as well, such as ZWSP
		while (chFirst == _T(' '))
		{
			rStr = rStr.Mid(1);
			chFirst = rStr[0];
		}
		if (rStr.IsEmpty())
			return;
		else
			rStr = MakeReverse(rStr);
	}
	else
	{
		// BEW 21Jul14 new code, to support ZWSP removals, etc, from end
		// we reversed rStr, so chFirst is the last
		CAdapt_ItApp* pApp = &wxGetApp();
		CAdapt_ItDoc* pDoc = pApp->GetDocument();
		if (pDoc->IsWhiteSpace(&chFirst))
		{
			// There is an exotic space at the end, remove it and do any more
			rStr = rStr.Mid(1);
			if (rStr.IsEmpty())
			{
				return;
			}
			chFirst = rStr[0];
			while (pDoc->IsWhiteSpace(&chFirst))
			{
				rStr = rStr.Mid(1);
				if (rStr.IsEmpty())
				{
					return;
				}
				chFirst = rStr[0];
			}
			rStr = MakeReverse(rStr);
			return;
		}
		else
		{
			// No exotic space at the end, so re-reverse & return string
			rStr = MakeReverse(rStr);
		}
		return;
	}
}
//#if defined(FWD_SLASH_DELIM)
// BEW 23Apr15 functions for support of / as word-breaking whitespace, with
// conversion to ZWSP in strings not accessible to user editing, and replacement
// of ZWSP with / for those strings which are user editable; that is, when
// putting a string into the phrasebox, we restore / delimiters, when getting
// the phrasebox string for some purpose, we replace all / with ZWSP

void CPhraseBox::ChangeValue(const wxString& value)
{
	// uses function from helpers.cpp
	wxString convertedValue = value; // needed due to const qualifier in signature
	convertedValue = ZWSPtoFwdSlash(convertedValue); // no changes done if m_bFwdSlashDelimiter is FALSE
	wxTextCtrl::ChangeValue(convertedValue);
}

//wxString CPhraseBox::GetValue2()
//{
//	wxString s = GetValue();
//	// uses function from helpers.cpp
//	s = FwdSlashtoZWSP(s); // no changes done if m_bFwdSlashDelimiter is FALSE
//	return s;
//}

//#endif

