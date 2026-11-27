/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 * @file  DirView.cpp
 *
 * @brief Main implementation file for CDirView
 */

#include "StdAfx.h"
#include "DirView.h"
#include "Constants.h"
#include "Merge.h"
#include "Clipboard.h"
#include "DirActions.h"
#include "DirViewColItems.h"
#include "locality.h" // Merge20 (S0-F1): NumToLocaleStr for the excluded-lines tooltip
#include "DirFrame.h"  // StatePane
#include "DirDoc.h"
#include "IMergeDoc.h"
#include "FileLocation.h"
#include "MainFrm.h"
#include "MergeEditFrm.h"
#include "MergeDoc.h"
#include "MergeEditView.h"
#include "resource.h"
#include "FileTransform.h"
#include "SelectPluginDlg.h"
#include "paths.h"
#include "7zCommon.h"
#include "OptionsDef.h"
#include "DiffImageListUtils.h"
#include "OptionsMgr.h"
#include "BCMenu.h"
#include "DirCmpReportDlg.h"
#include "DirCmpReport.h"
#include "FileCmpReport.h"
#include "CompareStatisticsDlg.h"
#include "LoadSaveCodepageDlg.h"
#include "ConfirmFolderCopyDlg.h"
#include "DirColsDlg.h"
#include "Merge20/Merge20AuditDlg.h"
#include "Merge20/Merge20AuditDoc.h"
#include "Merge20/Merge20BatchCheckDlg.h"
#include "Merge20/Merge20CsvExport.h"
#include "Merge20/Merge20CsvExportDlg.h"
#include "Merge20/Merge20MemoDlg.h"
#include "Merge20/Merge20LearnDlg.h"
#include "Merge20/Merge20MasterDlg.h"
#include "Merge20/Merge20FormCompareDoc.h"
#include "Merge20/Merge20Measure.h"
#include "Merge20/Merge20Patrol.h"
#include "Merge20/Merge20SessionDoc.h"
#include "Merge20/Merge20FilterDeckBar.h"
#include "Merge20/Merge20Slim.h"
#include "Merge20/Merge20Summary.h"
#include "Merge20/Merge20SummaryDlg.h"
#include "Merge20/Merge20Undo.h"
#include "Merge20/Merge20VerifyGate.h"
#include "Merge20/Merge20VerifyReportExport.h"
#include "Merge20/Merge20HighRiskConfirmDlg.h"
#include "FileOrFolderSelect.h"
#include "DirAdditionalPropertiesDlg.h"
#include "DirSelectFilesDlg.h"
#include "ShellContextMenu.h"
#include "DiffItem.h"
#include "IListCtrlImpl.h"
#include "Merge7zFormatMergePluginImpl.h"
#include "FileOrFolderSelect.h"
#include "IntToIntMap.h"
#include "PatchTool.h"
#include "SyntaxColors.h"
#include "Shell.h"
#include "DirTravel.h"
#include "MouseHook.h"
#include "RenameMoveDetection.h"
#include "FileFilterHelper.h"
#include "MergeLogger.h"
#include "PluginMenu.h"
#include <numeric>
#include <functional>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using std::swap;
using namespace std::placeholders;

/**
 * @brief Limit (in seconds) to signal compare is ready for user.
 * If compare takes longer than this value (in seconds) we inform
 * user about it. Current implementation uses MessageBeep(IDOK).
 */
constexpr int TimeToSignalCompare = 3;

// The resource ID constants/limits for the Shell context menu
constexpr UINT LeftCmdFirst = 0x9000; // this should be greater than any of already defined command IDs
constexpr UINT BothCmdLast = 0xffff; // maximum available value
constexpr UINT LeftCmdLast = LeftCmdFirst + (BothCmdLast - LeftCmdFirst) / 4; // divide available range equally between two context menus
constexpr UINT MiddleCmdFirst = LeftCmdLast + 1;
constexpr UINT MiddleCmdLast = MiddleCmdFirst + (BothCmdLast - LeftCmdFirst) / 4;
constexpr UINT RightCmdFirst = MiddleCmdLast + 1;
constexpr UINT RightCmdLast = RightCmdFirst + (BothCmdLast - LeftCmdFirst) / 4;
constexpr UINT BothCmdFirst = RightCmdLast + 1;

/////////////////////////////////////////////////////////////////////////////
// CDirView

enum { 
	TIMER_ID_COLUMN_REORDER = 99,
	TIMER_ID_STATUSBAR_UPDATE = 100,
	TIMER_ID_DBLCLICK_OPEN = 101
};

IMPLEMENT_DYNCREATE(CDirView, CListView)

CDirView::CDirView()
		: m_pList(nullptr)
		, m_elapsed(0)
		, m_bTreeMode(false)
		, m_dirfilter(std::bind(&COptionsMgr::GetBool, GetOptionsMgr(), _1))
		, m_pShellContextMenuLeft(nullptr)
		, m_pShellContextMenuMiddle(nullptr)
		, m_pShellContextMenuRight(nullptr)
		, m_pShellContextMenuBoth(nullptr)
		, m_hCurrentMenu(nullptr)
		, m_pSavedTreeState(nullptr)
		, m_pColItems(nullptr)
		, m_nActivePane(-1)
		, m_nExpandSubdirs(DO_NOT_EXPAND)
		, m_bUserCancelEdit(false)
{
	m_dwDefaultStyle &= ~LVS_TYPEMASK;
	// Show selection all the time, so user can see current item even when
	// focus is elsewhere (ie, on file edit window)
	m_dwDefaultStyle |= LVS_REPORT | LVS_SHOWSELALWAYS | LVS_EDITLABELS | LVS_OWNERDATA;

	m_bTreeMode =  GetOptionsMgr()->GetBool(OPT_TREE_MODE);
	m_nExpandSubdirs = static_cast<eExpandSubfoldersType>(GetOptionsMgr()->GetInt(OPT_DIRVIEW_EXPAND_SUBDIRS));
	m_nEscCloses = GetOptionsMgr()->GetInt(OPT_CLOSE_WITH_ESC);
	Options::DirColors::Load(GetOptionsMgr(), m_cachedColors);
	m_bUseColors = GetOptionsMgr()->GetBool(OPT_DIRCLR_USE_COLORS);
}

CDirView::~CDirView()
{
}

BEGIN_MESSAGE_MAP(CDirView, CListView)
	//{{AFX_MSG_MAP(CDirView)
	ON_WM_CONTEXTMENU()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_CHAR()
	ON_WM_KEYDOWN()
	ON_WM_TIMER()
	ON_WM_SETTINGCHANGE()
	ON_MESSAGE(MSG_UI_UPDATE, OnUpdateUIMessage)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateSave)
	ON_COMMAND(ID_MERGE20_BATCH_CHECK, OnMerge20BatchCheck)
	ON_COMMAND(ID_MERGE20_CSV_EXPORT, OnMerge20CsvExport)
	ON_COMMAND(ID_MERGE20_VERIFY_REPORT, OnMerge20VerifyReport)
	ON_COMMAND(ID_MERGE20_SUMMARY, OnMerge20Summary)
	ON_COMMAND(ID_MERGE20_SESSION_SAVE, OnMerge20SessionSave)
	ON_COMMAND(ID_MERGE20_SESSION_RESTORE, OnMerge20SessionRestore)
	ON_COMMAND(ID_MERGE20_MEMO_EDIT, OnMerge20MemoEdit)
	ON_COMMAND(ID_MERGE20_LEARN, OnMerge20Learn)
	ON_COMMAND(ID_MERGE20_GREP, OnMerge20Grep)
	ON_COMMAND(ID_MERGE20_BULK_CLASSIFY, OnMerge20BulkClassify)
	ON_COMMAND(ID_MERGE20_MASTER, OnMerge20Master)
	ON_COMMAND(ID_MERGE20_FORM_COMPARE, OnMerge20FormCompare)
	ON_COMMAND(ID_MERGE20_SLIM, OnMerge20Slim)
	ON_COMMAND_RANGE(ID_MERGE20_SHOW_REVIEW_TODO, ID_MERGE20_SHOW_REVIEW_KEEP, OnMerge20ShowReview)
	ON_UPDATE_COMMAND_UI(ID_MERGE20_SESSION_SAVE, OnUpdateMerge20Session)
	ON_UPDATE_COMMAND_UI(ID_MERGE20_SESSION_RESTORE, OnUpdateMerge20Session)
	ON_COMMAND(ID_MERGE20_DIR_CHECK_ALL, OnMerge20DirCheckAll)
	ON_COMMAND(ID_MERGE20_DIR_UNCHECK_ALL, OnMerge20DirUncheckAll)
	// Merge20 (best-workflow B10): judgment undo / redo
	ON_COMMAND(ID_MERGE20_UNDO_JUDGMENT, OnMerge20UndoJudgment)
	ON_UPDATE_COMMAND_UI(ID_MERGE20_UNDO_JUDGMENT, OnUpdateMerge20UndoJudgment)
	ON_COMMAND(ID_MERGE20_REDO_JUDGMENT, OnMerge20RedoJudgment)
	ON_UPDATE_COMMAND_UI(ID_MERGE20_REDO_JUDGMENT, OnUpdateMerge20RedoJudgment)
	ON_COMMAND(ID_MERGE20_AUDIT_VIEW, OnMerge20AuditView)
	ON_COMMAND(ID_MERGE20_AUDIT_CSV, OnMerge20AuditCsv)
	ON_UPDATE_COMMAND_UI(ID_MERGE20_AUDIT_VIEW, OnUpdateMerge20Audit)
	ON_UPDATE_COMMAND_UI(ID_MERGE20_AUDIT_CSV, OnUpdateMerge20Audit)
	ON_UPDATE_COMMAND_UI(ID_MERGE20_DIR_CHECK_ALL, OnUpdateMerge20DirCheck)
	ON_UPDATE_COMMAND_UI(ID_MERGE20_DIR_UNCHECK_ALL, OnUpdateMerge20DirCheck)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnColumnClick)
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnItemChanged)
	ON_NOTIFY_REFLECT(LVN_BEGINLABELEDIT, OnBeginLabelEdit)
	ON_NOTIFY_REFLECT(LVN_ENDLABELEDIT, OnEndLabelEdit)
	ON_NOTIFY_REFLECT(LVN_ODFINDITEM, OnODFindItem)
	ON_NOTIFY_REFLECT(NM_CLICK, OnClick)
	ON_NOTIFY_REFLECT(LVN_BEGINDRAG, OnBeginDrag)
 	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
	// [File] menu
	ON_COMMAND(ID_FILE_LEFT_READONLY, OnReadOnly<SIDE_LEFT>)
	ON_COMMAND(ID_FILE_MIDDLE_READONLY, OnReadOnly<SIDE_MIDDLE>)
	ON_COMMAND(ID_FILE_RIGHT_READONLY, OnReadOnly<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_FILE_LEFT_READONLY, OnUpdateReadOnly<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_FILE_MIDDLE_READONLY, OnUpdateReadOnly<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_FILE_RIGHT_READONLY, OnUpdateReadOnly<SIDE_RIGHT>)
	ON_COMMAND(ID_FILE_ENCODING, OnFileEncoding)
	// [Edit] menu
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateSelectAll)
	// [View] menu
	ON_COMMAND(ID_OPTIONS_SHOWDIFFERENT, OnOptionsShowDifferent)
	ON_COMMAND(ID_OPTIONS_SHOWIDENTICAL, OnOptionsShowIdentical)
	ON_COMMAND(ID_OPTIONS_SHOWUNIQUELEFT, OnOptionsShowUniqueLeft)
	ON_COMMAND(ID_OPTIONS_SHOWUNIQUEMIDDLE, OnOptionsShowUniqueMiddle)
	ON_COMMAND(ID_OPTIONS_SHOWUNIQUERIGHT, OnOptionsShowUniqueRight)
	ON_COMMAND(ID_OPTIONS_SHOWBINARIES, OnOptionsShowBinaries)
	ON_COMMAND(ID_OPTIONS_SHOWSKIPPED, OnOptionsShowSkipped)
	ON_COMMAND(ID_OPTIONS_SHOWDIFFERENTLEFTONLY, OnOptionsShowDifferentLeftOnly)
	ON_COMMAND(ID_OPTIONS_SHOWDIFFERENTMIDDLEONLY, OnOptionsShowDifferentMiddleOnly)
	ON_COMMAND(ID_OPTIONS_SHOWDIFFERENTRIGHTONLY, OnOptionsShowDifferentRightOnly)
	ON_COMMAND(ID_OPTIONS_SHOWMISSINGLEFTONLY, OnOptionsShowMissingLeftOnly)
	ON_COMMAND(ID_OPTIONS_SHOWMISSINGMIDDLEONLY, OnOptionsShowMissingMiddleOnly)
	ON_COMMAND(ID_OPTIONS_SHOWMISSINGRIGHTONLY, OnOptionsShowMissingRightOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWDIFFERENT, OnUpdateOptionsShowdifferent)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWIDENTICAL, OnUpdateOptionsShowidentical)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWUNIQUELEFT, OnUpdateOptionsShowuniqueleft)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWUNIQUEMIDDLE, OnUpdateOptionsShowuniquemiddle)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWUNIQUERIGHT, OnUpdateOptionsShowuniqueright)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWBINARIES, OnUpdateOptionsShowBinaries)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWSKIPPED, OnUpdateOptionsShowSkipped)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWDIFFERENTLEFTONLY, OnUpdateOptionsShowDifferentLeftOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWDIFFERENTMIDDLEONLY, OnUpdateOptionsShowDifferentMiddleOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWDIFFERENTRIGHTONLY, OnUpdateOptionsShowDifferentRightOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWMISSINGLEFTONLY, OnUpdateOptionsShowMissingLeftOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWMISSINGMIDDLEONLY, OnUpdateOptionsShowMissingMiddleOnly)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWMISSINGRIGHTONLY, OnUpdateOptionsShowMissingRightOnly)
	ON_COMMAND(ID_VIEW_SHOWHIDDENITEMS, OnViewShowHiddenItems)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWHIDDENITEMS, OnUpdateViewShowHiddenItems)
	ON_COMMAND(ID_VIEW_SHOW_EMPTY_FOLDERS, OnViewShowEmptyFolders)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOW_EMPTY_FOLDERS, OnUpdateViewShowEmptyFolders)
	ON_COMMAND(ID_VIEW_TREEMODE, OnViewTreeMode)
	ON_COMMAND(ID_VIEW_EXPAND_ALLSUBDIRS, OnViewExpandAllSubdirs)
	ON_COMMAND(ID_VIEW_EXPAND_DIFFERENT_SUBDIRS, OnViewExpandDifferentSubdirs)
	ON_COMMAND(ID_VIEW_EXPAND_IDENTICAL_SUBDIRS, OnViewExpandIdenticalSubdirs)
	ON_COMMAND(ID_VIEW_COLLAPSE_ALLSUBDIRS, OnViewCollapseAllSubdirs)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TREEMODE, OnUpdateViewTreeMode)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EXPAND_ALLSUBDIRS, OnUpdateViewExpandSubdirs)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EXPAND_DIFFERENT_SUBDIRS, OnUpdateViewExpandSubdirs)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EXPAND_IDENTICAL_SUBDIRS, OnUpdateViewExpandSubdirs)
	ON_UPDATE_COMMAND_UI(ID_VIEW_COLLAPSE_ALLSUBDIRS, OnUpdateViewCollapseAllSubdirs)
	ON_COMMAND(ID_SWAPPANES_SWAP12, (OnViewSwapPanes<0, 1>))
	ON_COMMAND(ID_SWAPPANES_SWAP23, (OnViewSwapPanes<1, 2>))
	ON_COMMAND(ID_SWAPPANES_SWAP13, (OnViewSwapPanes<0, 2>))
	ON_UPDATE_COMMAND_UI(ID_SWAPPANES_SWAP12, (OnUpdateViewSwapPanes<0, 1>))
	ON_UPDATE_COMMAND_UI(ID_SWAPPANES_SWAP23, (OnUpdateViewSwapPanes<1, 2>))
	ON_UPDATE_COMMAND_UI(ID_SWAPPANES_SWAP13, (OnUpdateViewSwapPanes<0, 2>))
	ON_COMMAND(ID_VIEW_DIR_STATISTICS, OnViewCompareStatistics)
	ON_COMMAND(ID_REFRESH, OnRefresh)
	ON_UPDATE_COMMAND_UI(ID_REFRESH, OnUpdateRefresh)
	ON_COMMAND(ID_RESCAN, OnMarkedRescan)
	// [Merge] menu or Context menu
	ON_COMMAND_RANGE(ID_MERGE_COMPARE, ID_MERGE_COMPARE_IN_NEW_WINDOW, OnMergeCompare)
	ON_UPDATE_COMMAND_UI_RANGE(ID_MERGE_COMPARE, ID_MERGE_COMPARE_IN_NEW_WINDOW, OnUpdateMergeCompare)
	ON_COMMAND(ID_MERGE_COMPARE_WITH_MOVED_RENAMED, OnMergeCompareWithRenamedMoved)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE_WITH_MOVED_RENAMED, OnUpdateMergeCompare)
	ON_COMMAND(ID_FIRSTDIFF, OnFirstdiff)
	ON_COMMAND(ID_LASTDIFF, OnLastdiff)
	ON_COMMAND(ID_NEXTDIFF, OnNextdiff)
	ON_COMMAND(ID_PREVDIFF, OnPrevdiff)
	ON_COMMAND(ID_CURDIFF, OnCurdiff)
	ON_UPDATE_COMMAND_UI(ID_FIRSTDIFF, OnUpdateFirstdiff)
	ON_UPDATE_COMMAND_UI(ID_LASTDIFF, OnUpdateLastdiff)
	ON_UPDATE_COMMAND_UI(ID_NEXTDIFF, OnUpdateNextdiff)
	ON_UPDATE_COMMAND_UI(ID_PREVDIFF, OnUpdatePrevdiff)
	ON_UPDATE_COMMAND_UI(ID_CURDIFF, OnUpdateCurdiff)
	ON_COMMAND_RANGE(ID_L2R, ID_R2L, OnDirCopy)
	ON_UPDATE_COMMAND_UI_RANGE(ID_L2R, ID_R2L, OnUpdateDirCopy)
	ON_COMMAND(ID_MERGE_DELETE, OnDelete)
	ON_UPDATE_COMMAND_UI(ID_MERGE_DELETE, OnUpdateDelete)
	ON_COMMAND_RANGE(ID_FILTERMENU_FIRST, ID_FILTERMENU_LAST, OnFilterMenuCommand)
	// [Tools] menu
	ON_COMMAND(ID_TOOLS_CUSTOMIZECOLUMNS, OnCustomizeColumns)
	ON_COMMAND(ID_TOOLS_GENERATEREPORT, OnToolsGenerateReport)
	ON_COMMAND(ID_TOOLS_GENERATEPATCH, OnToolsGeneratePatch)
	ON_MESSAGE(MSG_GENERATE_FLIE_COMPARE_REPORT, OnGenerateFileCmpReport)
	// [Plugins] menu
	ON_COMMAND(ID_OPEN_WITH_UNPACKER, OnOpenWithUnpacker)
	ON_UPDATE_COMMAND_UI(ID_OPEN_WITH_UNPACKER, OnUpdateCtxtOpenWithUnpacker)
	// [Help] menu
	ON_COMMAND(ID_HELP, OnHelp)
	// Context menu
	// Context menu -> Compare Non-horizontally
	ON_COMMAND(ID_MERGE_COMPARE_LEFT1_LEFT2, OnMergeCompare2<SELECTIONTYPE_LEFT1LEFT2>)
	ON_COMMAND(ID_MERGE_COMPARE_RIGHT1_RIGHT2, OnMergeCompare2<SELECTIONTYPE_RIGHT1RIGHT2>)
	ON_COMMAND(ID_MERGE_COMPARE_LEFT1_RIGHT2, OnMergeCompare2<SELECTIONTYPE_LEFT1RIGHT2>)
	ON_COMMAND(ID_MERGE_COMPARE_LEFT2_RIGHT1, OnMergeCompare2<SELECTIONTYPE_LEFT2RIGHT1>)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE_LEFT1_LEFT2, OnUpdateMergeCompare2<SELECTIONTYPE_LEFT1LEFT2>)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE_RIGHT1_RIGHT2, OnUpdateMergeCompare2<SELECTIONTYPE_RIGHT1RIGHT2>)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE_LEFT1_RIGHT2, OnUpdateMergeCompare2<SELECTIONTYPE_LEFT1RIGHT2>)
	ON_UPDATE_COMMAND_UI(ID_MERGE_COMPARE_LEFT2_RIGHT1, OnUpdateMergeCompare2<SELECTIONTYPE_LEFT2RIGHT1>)
	ON_COMMAND(ID_MERGE_COMPARE_NONHORIZONTALLY, OnMergeCompareNonHorizontally)
	// Context menu -> Compare As
	ON_COMMAND_RANGE(ID_MERGE_COMPARE_TEXT, ID_MERGE_COMPARE_FOLDER, OnMergeCompareAs)
	ON_COMMAND_RANGE(ID_UNPACKERS_FIRST, ID_UNPACKERS_LAST, OnMergeCompareAs)
	ON_UPDATE_COMMAND_UI_RANGE(ID_MERGE_COMPARE_TEXT, ID_MERGE_COMPARE_FOLDER, OnUpdateMergeCompare)
	ON_UPDATE_COMMAND_UI(ID_NO_UNPACKER, OnUpdateNoUnpacker)
	// Context menu -> Copy
	ON_COMMAND(ID_DIR_COPY_LEFT_TO_RIGHT, (OnCtxtDirCopy<SIDE_LEFT, SIDE_RIGHT>))
	ON_COMMAND(ID_DIR_COPY_LEFT_TO_MIDDLE, (OnCtxtDirCopy<SIDE_LEFT, SIDE_MIDDLE>))
	ON_COMMAND(ID_DIR_COPY_RIGHT_TO_LEFT, (OnCtxtDirCopy<SIDE_RIGHT, SIDE_LEFT>))
	ON_COMMAND(ID_DIR_COPY_RIGHT_TO_MIDDLE, (OnCtxtDirCopy<SIDE_RIGHT, SIDE_MIDDLE>))
	ON_COMMAND(ID_DIR_COPY_MIDDLE_TO_LEFT, (OnCtxtDirCopy<SIDE_MIDDLE, SIDE_LEFT>))
	ON_COMMAND(ID_DIR_COPY_MIDDLE_TO_RIGHT, (OnCtxtDirCopy<SIDE_MIDDLE, SIDE_RIGHT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_LEFT_TO_RIGHT, (OnUpdateCtxtDirCopy<SIDE_LEFT, SIDE_RIGHT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_LEFT_TO_MIDDLE, (OnUpdateCtxtDirCopy<SIDE_LEFT, SIDE_MIDDLE>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_RIGHT_TO_LEFT, (OnUpdateCtxtDirCopy<SIDE_RIGHT, SIDE_LEFT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_RIGHT_TO_MIDDLE, (OnUpdateCtxtDirCopy<SIDE_RIGHT, SIDE_MIDDLE>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_MIDDLE_TO_LEFT, (OnUpdateCtxtDirCopy<SIDE_MIDDLE, SIDE_LEFT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_MIDDLE_TO_RIGHT, (OnUpdateCtxtDirCopy<SIDE_MIDDLE, SIDE_RIGHT>))
	ON_COMMAND(ID_DIR_COPY_LEFT_TO_BROWSE, OnCtxtDirCopyTo<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_COPY_MIDDLE_TO_BROWSE, OnCtxtDirCopyTo<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_COPY_RIGHT_TO_BROWSE, OnCtxtDirCopyTo<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_LEFT_TO_BROWSE, OnUpdateCtxtDirCopyTo<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_MIDDLE_TO_BROWSE, OnUpdateCtxtDirCopyTo<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_RIGHT_TO_BROWSE, OnUpdateCtxtDirCopyTo<SIDE_RIGHT>)
	// Context menu -> Move
	ON_COMMAND(ID_DIR_MOVE_LEFT_TO_RIGHT, (OnCtxtDirMove<SIDE_LEFT, SIDE_RIGHT>))
	ON_COMMAND(ID_DIR_MOVE_LEFT_TO_MIDDLE, (OnCtxtDirMove<SIDE_LEFT, SIDE_MIDDLE>))
	ON_COMMAND(ID_DIR_MOVE_RIGHT_TO_LEFT, (OnCtxtDirMove<SIDE_RIGHT, SIDE_LEFT>))
	ON_COMMAND(ID_DIR_MOVE_RIGHT_TO_MIDDLE, (OnCtxtDirMove<SIDE_RIGHT, SIDE_MIDDLE>))
	ON_COMMAND(ID_DIR_MOVE_MIDDLE_TO_LEFT, (OnCtxtDirMove<SIDE_MIDDLE, SIDE_LEFT>))
	ON_COMMAND(ID_DIR_MOVE_MIDDLE_TO_RIGHT, (OnCtxtDirMove<SIDE_MIDDLE, SIDE_RIGHT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_LEFT_TO_RIGHT, (OnUpdateCtxtDirMove<SIDE_LEFT, SIDE_RIGHT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_LEFT_TO_MIDDLE, (OnUpdateCtxtDirMove<SIDE_LEFT, SIDE_MIDDLE>))
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_RIGHT_TO_LEFT, (OnUpdateCtxtDirMove<SIDE_RIGHT, SIDE_LEFT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_RIGHT_TO_MIDDLE, (OnUpdateCtxtDirMove<SIDE_RIGHT, SIDE_MIDDLE>))
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_MIDDLE_TO_LEFT, (OnUpdateCtxtDirMove<SIDE_MIDDLE, SIDE_LEFT>))
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_MIDDLE_TO_RIGHT, (OnUpdateCtxtDirMove<SIDE_MIDDLE, SIDE_RIGHT>))
	ON_COMMAND(ID_DIR_MOVE_LEFT_TO_BROWSE, OnCtxtDirMoveTo<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_MOVE_MIDDLE_TO_BROWSE, OnCtxtDirMoveTo<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_MOVE_RIGHT_TO_BROWSE, OnCtxtDirMoveTo<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_LEFT_TO_BROWSE, OnUpdateCtxtDirMoveTo<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_MIDDLE_TO_BROWSE, OnUpdateCtxtDirMoveTo<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_MOVE_RIGHT_TO_BROWSE, OnUpdateCtxtDirMoveTo<SIDE_RIGHT>)
	// Context menu -> Delete
	ON_COMMAND(ID_DIR_DEL_LEFT, OnCtxtDirDel<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_DEL_RIGHT, OnCtxtDirDel<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_DEL_MIDDLE, OnCtxtDirDel<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_DEL_BOTH, OnCtxtDirDelBoth)
	ON_COMMAND(ID_DIR_DEL_ALL, OnCtxtDirDelBoth)
	ON_UPDATE_COMMAND_UI(ID_DIR_DEL_LEFT, OnUpdateCtxtDirDel<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_DEL_MIDDLE, OnUpdateCtxtDirDel<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_DEL_RIGHT, OnUpdateCtxtDirDel<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_DEL_BOTH, OnUpdateCtxtDirDelBoth)
	ON_UPDATE_COMMAND_UI(ID_DIR_DEL_ALL, OnUpdateCtxtDirDelBoth)
	// Context menu -> Rename, Hide Items
	ON_COMMAND(ID_DIR_ITEM_RENAME, OnItemRename)
	ON_UPDATE_COMMAND_UI(ID_DIR_ITEM_RENAME, OnUpdateItemRename)
	ON_COMMAND(ID_DIR_HIDE_FILENAMES, OnHideFilenames)
	ON_UPDATE_COMMAND_UI(ID_DIR_HIDE_FILENAMES, OnUpdateHideFilenames)
	// Context menu -> Open Left
	ON_COMMAND(ID_DIR_OPEN_LEFT, OnCtxtDirOpen<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_OPEN_LEFT_WITHEDITOR, OnCtxtDirOpenWithEditor<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_OPEN_LEFT_WITH, OnCtxtDirOpenWith<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_OPEN_LEFT_PARENT_FOLDER, OnCtxtDirOpenParentFolder<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_LEFT, OnUpdateCtxtDirOpen<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_LEFT_WITHEDITOR, OnUpdateCtxtDirOpenWithEditor<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_LEFT_WITH, OnUpdateCtxtDirOpenWith<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_LEFT_PARENT_FOLDER, OnUpdateCtxtDirOpenParentFolder<SIDE_LEFT>)
	// Context menu -> Open Middle
	ON_COMMAND(ID_DIR_OPEN_MIDDLE, OnCtxtDirOpen<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_OPEN_MIDDLE_WITHEDITOR, OnCtxtDirOpenWithEditor<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_OPEN_MIDDLE_WITH, OnCtxtDirOpenWith<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_OPEN_MIDDLE_PARENT_FOLDER, OnCtxtDirOpenParentFolder<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_MIDDLE, OnUpdateCtxtDirOpen<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_MIDDLE_WITHEDITOR, OnUpdateCtxtDirOpenWithEditor<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_MIDDLE_WITH, OnUpdateCtxtDirOpenWith<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_MIDDLE_PARENT_FOLDER, OnUpdateCtxtDirOpenParentFolder<SIDE_MIDDLE>)
	// Context menu -> Open Right
	ON_COMMAND(ID_DIR_OPEN_RIGHT, OnCtxtDirOpen<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_OPEN_RIGHT_WITHEDITOR, OnCtxtDirOpenWithEditor<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_OPEN_RIGHT_WITH, OnCtxtDirOpenWith<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_OPEN_RIGHT_PARENT_FOLDER, OnCtxtDirOpenParentFolder<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_RIGHT, OnUpdateCtxtDirOpen<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_RIGHT_WITHEDITOR, OnUpdateCtxtDirOpenWithEditor<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_RIGHT_WITH, OnUpdateCtxtDirOpenWith<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_OPEN_RIGHT_PARENT_FOLDER, OnUpdateCtxtDirOpenParentFolder<SIDE_RIGHT>)
	// Context menu -> Copy Pathnames
	ON_COMMAND(ID_DIR_COPY_PATHNAMES_LEFT, OnCopyPathnames<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_COPY_PATHNAMES_MIDDLE, OnCopyPathnames<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_COPY_PATHNAMES_RIGHT, OnCopyPathnames<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_COPY_PATHNAMES_BOTH, OnCopyBothPathnames)
	ON_COMMAND(ID_DIR_COPY_PATHNAMES_ALL, OnCopyBothPathnames)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_PATHNAMES_LEFT, OnUpdateCtxtDirCopy2<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_PATHNAMES_MIDDLE, OnUpdateCtxtDirCopy2<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_PATHNAMES_RIGHT, OnUpdateCtxtDirCopy2<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_PATHNAMES_BOTH, OnUpdateCtxtDirCopyBoth2)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_PATHNAMES_ALL, OnUpdateCtxtDirCopyBoth2)
	// Context menu -> Zip
	ON_COMMAND(ID_DIR_ZIP_LEFT, OnCtxtDirZip<DirViewZipFlags::Left>)
	ON_COMMAND(ID_DIR_ZIP_MIDDLE, OnCtxtDirZip<DirViewZipFlags::Middle>)
	ON_COMMAND(ID_DIR_ZIP_RIGHT, OnCtxtDirZip<DirViewZipFlags::Right>)
	ON_COMMAND(ID_DIR_ZIP_BOTH, OnCtxtDirZip<DirViewZipFlags::Original | DirViewZipFlags::Altered>)
	ON_COMMAND(ID_DIR_ZIP_ALL, OnCtxtDirZip<DirViewZipFlags::Original | DirViewZipFlags::Altered>)
	ON_COMMAND(ID_DIR_ZIP_BOTH_DIFFS_ONLY, OnCtxtDirZip<DirViewZipFlags::Original | DirViewZipFlags::Altered | DirViewZipFlags::DiffsOnly>)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_LEFT, OnUpdateCtxtDirCopyTo<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_MIDDLE, OnUpdateCtxtDirCopyTo<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_RIGHT, OnUpdateCtxtDirCopyTo<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_BOTH, OnUpdateCtxtDirCopyBothTo)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_ALL, OnUpdateCtxtDirCopyBothTo)
	ON_UPDATE_COMMAND_UI(ID_DIR_ZIP_BOTH_DIFFS_ONLY, OnUpdateCtxtDirCopyBothDiffsOnlyTo)
	// Context menu -> Left/Middle/Right Shell Menu
	ON_COMMAND_RANGE(ID_DIR_SHELL_CONTEXT_MENU_LEFT, ID_DIR_SHELL_CONTEXT_MENU_ALL, OnCtxtDirShellContextMenu)
	// Context menu -> Plugin settings
	ON_COMMAND_RANGE(ID_PREDIFFER_SETTINGS_NONE, ID_PREDIFFER_SETTINGS_SELECT, OnPluginSettings)
	ON_COMMAND_RANGE(ID_UNPACKER_SETTINGS_NONE, ID_UNPACKER_SETTINGS_SELECT, OnPluginSettings)
	ON_UPDATE_COMMAND_UI_RANGE(ID_PREDIFFER_SETTINGS_NONE, ID_PREDIFFER_SETTINGS_SELECT, OnUpdatePluginMode)
	ON_UPDATE_COMMAND_UI_RANGE(ID_UNPACKER_SETTINGS_NONE, ID_UNPACKER_SETTINGS_SELECT, OnUpdatePluginMode)
	// Context menu -> Copy Filenames
	ON_COMMAND(ID_DIR_COPY_FILENAMES, OnCopyFilenames)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_FILENAMES, OnUpdateCopyFilenames)
	// Context menu -> Copy Items to Clipboard
	ON_COMMAND(ID_DIR_COPY_LEFT_TO_CLIPBOARD, OnCopyToClipboard<SIDE_LEFT>)
	ON_COMMAND(ID_DIR_COPY_MIDDLE_TO_CLIPBOARD, OnCopyToClipboard<SIDE_MIDDLE>)
	ON_COMMAND(ID_DIR_COPY_RIGHT_TO_CLIPBOARD, OnCopyToClipboard<SIDE_RIGHT>)
	ON_COMMAND(ID_DIR_COPY_BOTH_TO_CLIPBOARD, OnCopyBothToClipboard)
	ON_COMMAND(ID_DIR_COPY_ALL_TO_CLIPBOARD, OnCopyBothToClipboard)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_LEFT_TO_CLIPBOARD, OnUpdateCtxtDirCopy2<SIDE_LEFT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_MIDDLE_TO_CLIPBOARD, OnUpdateCtxtDirCopy2<SIDE_MIDDLE>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_RIGHT_TO_CLIPBOARD, OnUpdateCtxtDirCopy2<SIDE_RIGHT>)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_BOTH_TO_CLIPBOARD, OnUpdateCtxtDirCopyBoth2)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_ALL_TO_CLIPBOARD, OnUpdateCtxtDirCopyBoth2)
	// Context menu -> Copy All Displayed Columns
	ON_COMMAND(ID_DIR_COPY_ALL_DISP_COLUMNS, OnCopyAllDisplayedColumns)
	ON_UPDATE_COMMAND_UI(ID_DIR_COPY_ALL_DISP_COLUMNS, OnUpdateCopyAllDisplayedColumns)
	// Status bar
	ON_NOTIFY(NM_CLICK, AFX_IDW_CONTROLBAR_FIRST+30, OnStatusBarClick)
	ON_UPDATE_COMMAND_UI(ID_STATUS_DIFFNUM, OnUpdateStatusNum)
	ON_UPDATE_COMMAND_UI(ID_STATUS_RIGHTDIR_RO, OnUpdateStatusRightRO)
	ON_UPDATE_COMMAND_UI(ID_STATUS_MIDDLEDIR_RO, OnUpdateStatusMiddleRO)
	ON_UPDATE_COMMAND_UI(ID_STATUS_LEFTDIR_RO, OnUpdateStatusLeftRO)
	// Display filter bar
	ON_COMMAND(ID_VIEW_DISPLAY_FILTER_BAR, OnViewDisplayFilterBar)
	ON_COMMAND(ID_APPLY_NOW, OnViewDisplayFilterBarApply)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDirView diagnostics

#ifdef _DEBUG

CDirDoc* CDirView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CDirDoc)));
	return (CDirDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CDirView message handlers

void CDirView::OnInitialUpdate()
{
	const int iconCX = []() {
		const int cx = GetSystemMetrics(SM_CXSMICON);
		if (cx < 24)
			return 16;
		if (cx < 32)
			return 24;
		if (cx < 48)
			return 32;
		return 48;
	}();
	const int iconCY = iconCX;
	__super::OnInitialUpdate();
	m_pList = &GetListCtrl();
	m_pIList.reset(new IListCtrlImpl(m_pList->m_hWnd, m_listViewItems));
	CDirDoc* pDoc = GetDocument();
	pDoc->SetDirView(this);

	auto properties = strutils::split<std::vector<String>>(GetOptionsMgr()->GetString(OPT_ADDITIONAL_PROPERTIES), ' ');
	m_pColItems.reset(new DirViewColItems(pDoc->m_nDirs, properties));

	m_pList->SendMessage(CCM_SETUNICODEFORMAT, TRUE, 0);

	// Load user-selected font
	if (GetOptionsMgr()->GetBool(OPT_FONT_DIRCMP + OPT_FONT_USECUSTOM))
	{
		m_font.CreateFontIndirect(&theApp.m_lfDir);
		CWnd::SetFont(&m_font, TRUE);
	}

	if (m_bUseColors)
		m_pList->SetBkColor(m_cachedColors.clrDirMargin);

	// Replace standard header with sort header
	HWND hWnd = ListView_GetHeader(m_pList->m_hWnd);
	if (hWnd != nullptr)
		m_ctlSortHeader.SubclassWindow(hWnd);

	DiffImageListUtils::InitializeDiffImageList(m_imageList);
	m_pList->SetImageList(&m_imageList, LVSIL_SMALL);

	// Load the icons used for the list view (expanded/collapsed state icons)
	VERIFY(m_imageState.Create(iconCX, iconCY, ILC_COLOR32 | ILC_MASK, 15, 1));
	for (auto id : { IDI_TREE_STATE_COLLAPSED, IDI_TREE_STATE_EXPANDED })
	{
		HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, iconCX, iconCY, 0);
		if (hIcon != nullptr)
		{
			VERIFY(-1 != m_imageState.Add(hIcon));
			::DestroyIcon(hIcon);
		}
	}

	// Restore column orders as they had them last time they ran
	m_pColItems->LoadColumnOrders(
		GetOptionsMgr()->GetString(pDoc->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_ORDERS : OPT_DIRVIEW3_COLUMN_ORDERS));

	// Merge20 (W1, D1.5; S0-F1 adds the sixth column): a layout saved
	// before the Merge20 columns existed hides them. Append the six
	// default-visible Merge20 columns (review state, output, action
	// category, risk, summary, excluded lines) at the end, once, and
	// remember the migration. The version is a single global key, so
	// whichever layout (2-way or 3-way) is opened first after the
	// upgrade gets migrated; a pre-existing layout of the other kind
	// is then skipped. Layouts migrated at version 1 already have
	// the first five columns; version 2 only appends the excluded
	// lines column (MigrateMerge20Columns skips columns that are
	// already displayed).
	if (GetOptionsMgr()->GetInt(OPT_MERGE20_COLUMN_LAYOUT_VERSION) < 2)
	{
		const String ordersKey = pDoc->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_ORDERS : OPT_DIRVIEW3_COLUMN_ORDERS;
		if (m_pColItems->MigrateMerge20Columns())
			GetOptionsMgr()->SaveOption(ordersKey, m_pColItems->SaveColumnOrders());
		GetOptionsMgr()->SaveOption(OPT_MERGE20_COLUMN_LAYOUT_VERSION, 2);
	}

	// Display column headers (in appropriate order)
	ReloadColumns();

	// Merge20 (Phase 4 D9): restore the slim list when the option
	// says so. The full layout was just loaded from the options
	// above, so SetMerge20Slim remembers it before hiding columns.
	if (GetOptionsMgr()->GetBool(OPT_MERGE20_SLIM))
		SetMerge20Slim(true);

	// Show selection across entire row.u
	// Also allow user to rearrange columns via drag&drop of headers.
	// Also enable infotips.
	DWORD exstyle = LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER;
	m_pList->SetExtendedStyle(exstyle);
	HWND hList = GetSafeHwnd();
	if (hList != nullptr)
	{
		DarkMode::setListViewCtrlSubclass(hList);
		DarkMode::setDarkTooltips(hList, static_cast<int>(DarkMode::ToolTipsType::listview));
		DarkMode::setDarkThemeExperimentalEx(hList, L"Explorer");
	}
}

BOOL CDirView::PreCreateWindow(CREATESTRUCT& cs)
{
	__super::PreCreateWindow(cs);
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return TRUE;
}

/**
 * @brief Called before compare is started.
 * CDirDoc calls this function before new compare is started, so this
 * is good place to setup GUI for compare.
 * @param [in] pCompareStats Pointer to class having current compare stats.
 */
void CDirView::StartCompare(CompareStats *pCompareStats)
{
	m_elapsed = 0;
}

/**
 * @brief Called when folder compare row is double-clicked with mouse.
 * Selected item is opened to folder or file compare.
 */
void CDirView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	LVHITTESTINFO lvhti;
	lvhti.pt = point;
	m_pList->SubItemHitTest(&lvhti);
	if (lvhti.iItem >= 0)
	{
		const DIFFITEM& di = GetDiffItem(lvhti.iItem);
		if (m_bTreeMode && GetDiffContext().m_bRecursive && di.diffcode.isDirectory())
		{
			if (di.customFlags & ViewCustomFlags::EXPANDED)
				CollapseSubdir(lvhti.iItem);
			else
				ExpandSubdir(lvhti.iItem);
		}
		else
		{
			// Do not open the file immediately to avoid leftover clicks affecting
			// other windows (e.g., status bar or another view). Use a timer
			// to process the open operation after a short delay.
			SetTimer(TIMER_ID_DBLCLICK_OPEN, 50, nullptr);
		}
	}
	if (GetFocus() == this)
		__super::OnLButtonDblClk(nFlags, point);
}

/**
 * @brief Load or reload the columns (headers) of the list view
 */
void CDirView::ReloadColumns()
{
	m_pList->SetRedraw(FALSE);	// Turn off updating (better performance)

	LoadColumnHeaderItems();

	UpdateColumnNames();
	m_pColItems->LoadColumnWidths(
		GetOptionsMgr()->GetString(GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS),
		std::bind(&CListCtrl::SetColumnWidth, m_pList, _1, _2), GetDefColumnWidth());
	SetColAlignments();
	// Merge20 (Phase 4 D9): re-apply the slim column widths (see
	// ApplyMerge20SlimColumnWidths). ReloadColumns loads widths from
	// the saved full layout, which would otherwise assign the
	// Path/Status widths to the slim columns.
	ApplyMerge20SlimColumnWidths();

	m_pList->SetRedraw(TRUE);	// Turn updating back on

}

/**
 * @brief Check whether the specified parent has any showable descendant.
 * @param [in] parent Parent item.
 * @return true if the parent has at least one showable descendant; otherwise false.
 */
bool CDirView::HasShowableDescendant(DIFFITEM* parent)
{
	auto it = m_hasShowableDescendantCache.find(parent);
	if (it != m_hasShowableDescendantCache.end())
		return it->second;

	const CDiffContext& ctxt = GetDiffContext();
	DIFFITEM* diffpos = ctxt.GetFirstChildDiffPosition(parent);

	while (diffpos != nullptr)
	{
		DIFFITEM* curdiffpos = diffpos;
		const DIFFITEM& di = ctxt.GetNextSiblingDiffPosition(diffpos);

		if (IsShowable(ctxt, di, m_dirfilter))
		{
			if (!di.diffcode.isDirectory())
			{
				m_hasShowableDescendantCache[parent] = true;
				return true;
			}

			if (di.HasChildren() && HasShowableDescendant(curdiffpos))
			{
				m_hasShowableDescendantCache[parent] = true;
				return true;
			}
		}
	}

	m_hasShowableDescendantCache[parent] = false;
	return false;
}

/**
 * @brief Redisplay items in subfolder
 * @param [in] parent Parent item (nullptr for root level).
 * @param [in] level Indent level
 * @param [in,out] index Index of the item to be inserted.
 * @param [in,out] alldiffs Number of different items
 * @return returns -1 if the comparison of some items was interrupted or an error occurred
 */
int CDirView::RedisplayChildren(DIFFITEM *parent, int level, UINT &index, int &alldiffs)
{
	int result = 0;
	const CDiffContext &ctxt = GetDiffContext();

	// Get first child of parent (or first root item if parent is nullptr)
	DIFFITEM *diffpos = parent ? ctxt.GetFirstChildDiffPosition(parent) : ctxt.GetFirstDiffPosition();

	while (diffpos != nullptr)
	{
		DIFFITEM *curdiffpos = diffpos;
		const DIFFITEM &di = ctxt.GetNextSiblingDiffPosition(diffpos);

		if (di.diffcode.isResultDiff() || (!di.diffcode.existAll() && !di.diffcode.isResultFiltered()))
			++alldiffs;
		if (di.diffcode.isResultError() || di.diffcode.isResultAbort())
			result = -1;

		bool bShowable = IsShowable(ctxt, di, m_dirfilter);

		if (m_bTreeMode && !m_dirfilter.show_empty_folders && di.diffcode.isDirectory() && !HasShowableDescendant(curdiffpos))
			bShowable = false;

		if (bShowable)
		{
			if (m_bTreeMode)
			{
				// In tree mode, add the item first
				AddNewItem(index, curdiffpos, I_IMAGECALLBACK, level);
				index++;

				// Process children if directory has children
				if (di.HasChildren())
				{
					if (di.customFlags & ViewCustomFlags::EXPANDED)
					{
						// Expanded: recursively display children
						if (RedisplayChildren(curdiffpos, level + 1, index, alldiffs) < 0)
							result = -1;
					}
					// Note: For collapsed directories, we don't process children
					// Empty folder filtering is handled when expanding
				}
			}
			else
			{
				// Flat mode
				if (!ctxt.m_bRecursive || !di.diffcode.isDirectory() || !di.diffcode.existAll())
				{
					AddNewItem(index, curdiffpos, I_IMAGECALLBACK, 0);
					index++;
				}

				if (di.HasChildren())
				{
					// Recursively process children
					if (RedisplayChildren(curdiffpos, level + 1, index, alldiffs) < 0)
						result = -1;
				}
			}
		}
	}
	m_firstDiffItem.reset();
	m_lastDiffItem.reset();
	return result;
}

/**
 * @brief Redisplay folder compare view.
 * This function clears folder compare view and then adds
 * items from current compare to it.
 */
void CDirView::Redisplay()
{
	const CDirDoc *pDoc = GetDocument();
	const CDiffContext &ctxt = GetDiffContext();
	PathContext pathsParent;
	CImageList emptyImageList;

	UINT cnt = 0;
	// Disable redrawing while adding new items
	SetRedraw(FALSE);

	DeleteAllDisplayItems();

	m_pList->SetImageList((m_bTreeMode && ctxt.m_bRecursive) ? &m_imageState : &emptyImageList, LVSIL_STATE);

	// If non-recursive compare, add special item(s)
	if (!ctxt.m_bRecursive ||
		CheckAllowUpwardDirectory(ctxt, pDoc->m_pTempPathContext, pathsParent) == AllowUpwardDirectory::ParentIsTempPath)
	{
		cnt += AddSpecialItems();
	}

	m_dirfilter.displayFilterHelper.SetDiffContext(&ctxt);
	int alldiffs = 0;
	m_hasShowableDescendantCache.clear();
	const int result = RedisplayChildren(nullptr, 0, cnt, alldiffs);
	const unsigned int threadState = pDoc->m_diffThread.GetThreadState();
	GetParentFrame()->SetLastCompareResult((threadState != CDiffThread::THREAD_COMPLETED || result < 0) ? -1 : alldiffs);
	SortColumnsAppropriately();
	SetRedraw(TRUE);
	m_pList->SetItemCount(static_cast<int>(m_listViewItems.size()));
	m_pList->Invalidate();
	// Merge20 (S3): the displayed set just changed (compare finished,
	// filter applied, refresh), so refresh the "N displayed, M checked"
	// count. Redisplay() is the single place the visible rows are rebuilt.
	UpdateMerge20StatusCount();
}

/**
 * @brief User right-clicked somewhere in this view
 */
void CDirView::OnContextMenu(CWnd*, CPoint point)
{
	if (CMouseHook::IsRightWheelScrolling())
		return;
	// Make sure window is active
	GetParentFrame()->ActivateFrame();

	int i = 0;
	if (point.x == -1 && point.y == -1)
	{
		//keystroke invocation
		CRect rect;
		GetClientRect(rect);
		ClientToScreen(rect);

		point = rect.TopLeft();
		point.Offset(5, 5);
	}
	else
	{
		// Check if user right-clicked on header
		// convert screen coordinates to client coordinates of header control
		CPoint insidePt = point;
		GetListCtrl().GetHeaderCtrl()->ScreenToClient(&insidePt);
		// Ask header control if click was on one of its header items
		HDHITTESTINFO hhti = { 0 };
		hhti.pt = insidePt;
		int col = static_cast<int>(GetListCtrl().GetHeaderCtrl()->SendMessage(HDM_HITTEST, 0, (LPARAM) & hhti));
		if (col >= 0)
		{
			// Presumably hhti.flags & HHT_ONHEADER is true
			HeaderContextMenu(point, m_pColItems->ColPhysToLog(col));
			return;
		}
		// bail out if point is not in any row
		LVHITTESTINFO lhti = { 0 };
		insidePt = point;
		ScreenToClient(&insidePt);
		lhti.pt = insidePt;
		i = GetListCtrl().HitTest(insidePt);
		TRACE(_T("i=%d\n"), i);
		if (i < 0)
			return;
	}

	if (GetListCtrl().GetItemCount() == 0)
		return;

	ListContextMenu(point, i);
}

/**
 * @brief Toggle context menu item
 */
static void NTAPI CheckContextMenu(BCMenu *pPopup, UINT uIDItem, BOOL bCheck)
{
	pPopup->CheckMenuItem(uIDItem, bCheck ? MF_CHECKED : MF_UNCHECKED);
}

// Merge20 (Phase 4 D11): option key of a review-state filter toggle,
// by ReviewState index (0 TODO, 1 DONE, 2 HOLD, 3 SPECIAL, 4 KEEP).
static const String& Merge20ShowReviewKey(int index)
{
	switch (index)
	{
	case 1: return OPT_MERGE20_SHOW_REVIEW_DONE;
	case 2: return OPT_MERGE20_SHOW_REVIEW_HOLD;
	case 3: return OPT_MERGE20_SHOW_REVIEW_SPECIAL;
	case 4: return OPT_MERGE20_SHOW_REVIEW_KEEP;
	case 0:
	default: return OPT_MERGE20_SHOW_REVIEW_TODO;
	}
}

// Merge20 (H-FILTER): option key of an existence-filter toggle, by
// Merge20FileStatus index (0 Modified, 1 OldOnly, 2 NewOnly,
// 3 Identical).
static const String& Merge20ShowStatusKey(int index)
{
	switch (index)
	{
	case 1: return OPT_MERGE20_SHOW_STATUS_OLD_ONLY;
	case 2: return OPT_MERGE20_SHOW_STATUS_NEW_ONLY;
	case 3: return OPT_MERGE20_SHOW_STATUS_IDENTICAL;
	case 0:
	default: return OPT_MERGE20_SHOW_STATUS_MODIFIED;
	}
}

// Merge20 (H-FILTER): option key of an action-filter toggle, by
// ActionCategory index (0 MERGE_CLEAN_OLD, 1 REVIEW_MIXED,
// 2 REVIEW_MOD_ONLY, 3 NO_MERGE_NEW_ADD, 4 COPY_OLD_FILE,
// 5 SKIP_NEW_FILE, 6 NO_ACTION_IDENTICAL).
static const String& Merge20ShowActionKey(int index)
{
	switch (index)
	{
	case 1: return OPT_MERGE20_SHOW_ACTION_REVIEW_MIXED;
	case 2: return OPT_MERGE20_SHOW_ACTION_REVIEW_MOD_ONLY;
	case 3: return OPT_MERGE20_SHOW_ACTION_NO_MERGE_NEW_ADD;
	case 4: return OPT_MERGE20_SHOW_ACTION_COPY_OLD_FILE;
	case 5: return OPT_MERGE20_SHOW_ACTION_SKIP_NEW_FILE;
	case 6: return OPT_MERGE20_SHOW_ACTION_NO_ACTION_IDENTICAL;
	case 0:
	default: return OPT_MERGE20_SHOW_ACTION_MERGE_CLEAN_OLD;
	}
}

// Merge20 (Phase 4 D11): menu name of a review state, by ReviewState
// index. These are the existing S2 column display names, reused so
// the filter submenu needs no new strings of its own.
static String Merge20ReviewStateMenuName(int index)
{
	switch (index)
	{
	case 1: return _("Done");
	case 2: return _("On hold");
	case 3: return _("Needs manual work");
	case 4: return _("Keep as is");
	case 0:
	default: return _("Not reviewed");
	}
}

/**
 * @brief User right-clicked in listview rows
?* @param [in] point Point where the mouse was right-clicked.
 */
void CDirView::ListContextMenu(CPoint point, int /*i*/)
{
	CDirDoc* pDoc = GetDocument();
	BCMenu menu;
	VERIFY(menu.LoadMenu(IDR_POPUP_DIRVIEW));
	VERIFY(menu.LoadToolbar(IDR_MAINFRAME, GetMainFrame()->GetToolbar()));
	I18n::TranslateMenu(menu.m_hMenu);

	// 1st submenu of IDR_POPUP_DIRVIEW is for item popup
	BCMenu *pPopup = static_cast<BCMenu*>(menu.GetSubMenu(0));
	ASSERT(pPopup != nullptr);

	int sel = GetFocusedItem();
	if (sel == -1)
		sel = GetFirstSelectedInd();
	if (sel == -1)
		return;

	bool bDirSelected = false;
	bool bMovedRenamed = true;
	for (DirItemIterator it = SelBegin(); it != SelEnd(); ++it)
	{
		const DIFFITEM& di = *it;
		if (di.renameMoveGroupId == -1)
			bMovedRenamed = false;
		if (di.diffcode.isDirectory())
		{
			bDirSelected = true;
			break;
		}
	}
	if (GetDiffContext().m_bRecursive && bDirSelected)
		pPopup->RemoveMenu(ID_MERGE_COMPARE, MF_BYCOMMAND);
	if (!bMovedRenamed)
		pPopup->RemoveMenu(ID_MERGE_COMPARE_WITH_MOVED_RENAMED, MF_BYCOMMAND);
	if (!bDirSelected)
		pPopup->RemoveMenu(ID_MERGE_COMPARE_IN_NEW_WINDOW, MF_BYCOMMAND);

	if (pDoc->m_nDirs < 3)
	{
		pPopup->RemoveMenu(ID_DIR_COPY_LEFT_TO_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_MIDDLE_TO_LEFT, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_MIDDLE_TO_RIGHT, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_MIDDLE_TO_BROWSE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_RIGHT_TO_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_MOVE_LEFT_TO_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_MOVE_MIDDLE_TO_LEFT, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_MOVE_MIDDLE_TO_RIGHT, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_MOVE_MIDDLE_TO_BROWSE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_MOVE_RIGHT_TO_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_DEL_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_DEL_ALL, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_OPEN_MIDDLE, MF_BYCOMMAND);

		for (int i = 0; i < pPopup->GetMenuItemCount(); ++i)
		{
			if (pPopup->GetMenuItemID(i) == ID_DIR_HIDE_FILENAMES)
				pPopup->RemoveMenu(i + 3, MF_BYPOSITION);
		}

		pPopup->RemoveMenu(ID_DIR_OPEN_MIDDLE_WITHEDITOR, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_OPEN_MIDDLE_WITH, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_PATHNAMES_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_PATHNAMES_ALL, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_MIDDLE_TO_CLIPBOARD, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_ALL_TO_CLIPBOARD, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_ZIP_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_ZIP_ALL, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_SHELL_CONTEXT_MENU_MIDDLE, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_SHELL_CONTEXT_MENU_ALL, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_MERGE_COMPARE_NONHORIZONTALLY, MF_BYCOMMAND);
	}
	else
	{
		pPopup->RemoveMenu(ID_DIR_COPY_PATHNAMES_BOTH, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_COPY_BOTH_TO_CLIPBOARD, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_ZIP_BOTH, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_DEL_BOTH, MF_BYCOMMAND);
		pPopup->RemoveMenu(ID_DIR_SHELL_CONTEXT_MENU_BOTH, MF_BYCOMMAND);
		pPopup->RemoveMenu(2, MF_BYPOSITION); // Compare Non-horizontally
	}

	CMenu menuPluginsHolder;
	menuPluginsHolder.LoadMenu(IDR_POPUP_PLUGINS_SETTINGS);
	I18n::TranslateMenu(menuPluginsHolder.m_hMenu);
	String s = _("Plugin Settings");
	pPopup->AppendMenu(MF_SEPARATOR);
	pPopup->AppendMenu(MF_POPUP, static_cast<int>(reinterpret_cast<uintptr_t>(menuPluginsHolder.m_hMenu)), s.c_str());

	// Merge20 (S3, design section 3.2, spec sections 3 and 4(b)): batch
	// check dialog and per-directory check/uncheck. The batch-check item
	// is always offered; the directory items only when a directory row
	// is among the selection (bDirSelected is computed above).
	pPopup->AppendMenu(MF_SEPARATOR);
	String sMerge20BatchCheck = _("Batch check...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_BATCH_CHECK, sMerge20BatchCheck.c_str());
	// Merge20 (best-workflow B10, gap analysis section 4.2 B10):
	// judgment undo / redo, next to the batch check. The items
	// are greyed out when there is nothing to undo / redo
	// (OnUpdateMerge20*Judgment). This is judgment undo, not the
	// text-edit undo: the shortcuts work in this list only
	// (PreTranslateMessage), and the menu text says so.
	String sMerge20UndoJudgment = _("Undo Judgment");
	sMerge20UndoJudgment += _T("\tCtrl+Z");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_UNDO_JUDGMENT, sMerge20UndoJudgment.c_str());
	String sMerge20RedoJudgment = _("Redo Judgment");
	sMerge20RedoJudgment += _T("\tCtrl+Shift+Z");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_REDO_JUDGMENT, sMerge20RedoJudgment.c_str());
	// Merge20 (S6, design section 3.5, spec section 4(f)): diff-list
	// CSV export, next to the batch check. Always offered.
	String sMerge20CsvExport = _("Export diff list as CSV...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_CSV_EXPORT, sMerge20CsvExport.c_str());
	// Merge20 (B8, gap analysis section 4.2 B8): verification
	// report export (standalone), next to the CSV export. Always offered.
	String sMerge20VerifyReport = _("Export verification report...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_VERIFY_REPORT, sMerge20VerifyReport.c_str());
	// Merge20 (B9, gap analysis section 4.2 B9): end-of-session
	// summary dialog, next to the CSV export. Always offered.
	String sMerge20Summary = _("End-of-session review...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_SUMMARY, sMerge20Summary.c_str());
	// Merge20 (B11, gap analysis section 4.2 B11): audit-log viewer
	// and CSV export, next to the diff-list CSV export and the
	// end-of-session summary. Always offered.
	String sMerge20AuditView = _("Audit log...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_AUDIT_VIEW, sMerge20AuditView.c_str());
	String sMerge20AuditCsv = _("Export audit log as CSV...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_AUDIT_CSV, sMerge20AuditCsv.c_str());
	// Merge20 (Phase 4 D7, design section 5.2/D7, spec section 3 (4/4)):
	// Grep cross-file search. Always offered.
	String sMerge20Grep = _("Cross-File Search...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_GREP, sMerge20Grep.c_str());
	String sMerge20BulkClassify = _("Bulk classify unopened files...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_BULK_CLASSIFY, sMerge20BulkClassify.c_str());
	// Merge20 (Phase 4 D8, design section 5.2/D8, spec section 3 (4/4)):
	// Master management (exclude words, highlight, risk keywords). Always offered.
	String sMerge20Master = _("Master Management...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_MASTER, sMerge20Master.c_str());
	// Merge20 (Wave 5 F2, design sections 3.2 and 7.1): FORM
	// compare (view-only). The two folder paths are handed over
	// as-is; the FORM window runs its own .xml-only scan (the
	// folder compare result is not reused).
	String sMerge20FormCompare = _("FORM Compare (view only)...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_FORM_COMPARE, sMerge20FormCompare.c_str());
	// Merge20 (Phase 4 D9, design section 5.2/D9, spec sections 3
	// (4/4) and 5.1): slim list. Always offered; the check shows
	// the current state (the menu is rebuilt on every popup, as for
	// the other Merge20 items, so no update handler is needed).
	String sMerge20Slim = _("Slim List");
	pPopup->AppendMenu(MF_STRING | (GetOptionsMgr()->GetBool(OPT_MERGE20_SLIM) ? MF_CHECKED : 0),
		ID_MERGE20_SLIM, sMerge20Slim.c_str());
	// Merge20 (Phase 4 D11, design section 5.2/D11): review-state
	// filter submenu. The check marks show the current filter (the
	// menu is rebuilt on every popup, as for the other Merge20
	// items, so no update handler is needed). menuReview lives on
	// the stack until TrackPopupMenu returns (as for the plugin
	// submenu above); it is not detached, because BCMenu's
	// owner-draw item data belongs to the submenu.
	BCMenu menuReview;
	VERIFY(menuReview.CreatePopupMenu());
	for (int i = 0; i < 5; ++i)
	{
		menuReview.AppendMenu(MF_STRING | (GetOptionsMgr()->GetBool(Merge20ShowReviewKey(i)) ? MF_CHECKED : 0),
			ID_MERGE20_SHOW_REVIEW_TODO + i, Merge20ReviewStateMenuName(i).c_str());
	}
	String sMerge20ShowReview = _("Show review states");
	pPopup->AppendMenu(MF_POPUP, static_cast<int>(reinterpret_cast<uintptr_t>(menuReview.m_hMenu)), sMerge20ShowReview.c_str());
	// Merge20 (S8, design section 3.6, spec section 6.1): session
	// save/restore, next to the batch check and CSV export. Always offered.
	String sMerge20SessionSave = _("Save session");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_SESSION_SAVE, sMerge20SessionSave.c_str());
	String sMerge20SessionRestore = _("Restore session...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_SESSION_RESTORE, sMerge20SessionRestore.c_str());
	// Merge20 (S8, design section 3.5): audit-memo editor. Only when a
	// single file row is selected (directories have no memo).
	if (GetSelectedCount() == 1 && sel != -1)
	{
		const DIFFITEM& di = GetDiffItem(sel);
		if (!di.diffcode.isDirectory())
		{
			String sMerge20MemoEdit = _("Edit audit memo...");
			pPopup->AppendMenu(MF_STRING, ID_MERGE20_MEMO_EDIT, sMerge20MemoEdit.c_str());
		}
	}
	// Merge20 (Phase 3 L4, design section 3.3, spec section 4(d)):
	// learning dialog, at the end of the Merge20 group. Always offered.
	String sMerge20Learn = _("Learning...");
	pPopup->AppendMenu(MF_STRING, ID_MERGE20_LEARN, sMerge20Learn.c_str());
	if (bDirSelected)
	{
		String sMerge20DirCheckAll = _("Check all files under folder");
		pPopup->AppendMenu(MF_STRING, ID_MERGE20_DIR_CHECK_ALL, sMerge20DirCheckAll.c_str());
		String sMerge20DirUncheckAll = _("Uncheck all files under folder");
		pPopup->AppendMenu(MF_STRING, ID_MERGE20_DIR_UNCHECK_ALL, sMerge20DirUncheckAll.c_str());
	}

	CFrameWnd *pFrame = GetTopLevelFrame();
	ASSERT(pFrame != nullptr);
	pFrame->m_bAutoMenuEnable = FALSE;
	// invoke context menu
	// this will invoke all the OnUpdate methods to enable/disable the individual items
	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
			AfxGetMainWnd());

	pFrame->m_bAutoMenuEnable = TRUE;
}

/**
 * @brief User right-clicked on specified logical column
 */
void CDirView::HeaderContextMenu(CPoint point, int i)
{
	BCMenu menu;
	VERIFY(menu.LoadMenu(IDR_POPUP_DIRVIEW));
	VERIFY(menu.LoadToolbar(IDR_MAINFRAME, GetMainFrame()->GetToolbar()));
	I18n::TranslateMenu(menu.m_hMenu);
	// 2nd submenu of IDR_POPUP_DIRVIEW is for header popup
	BCMenu* pPopup = static_cast<BCMenu *>(menu.GetSubMenu(1));
	ASSERT(pPopup != nullptr);

	const DirColInfo * col = m_pColItems->GetDirColInfo(i);
	ASSERT(col != nullptr);
	m_pFilterMenu = CFileFilterHelperMenu::AppendColumnFilterMenu(pPopup, col->regName, GetDocument()->m_nDirs >= 3);

	// invoke context menu
	// this will invoke all the OnUpdate methods to enable/disable the individual items
	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
			AfxGetMainWnd());
}

void CDirView::OnFilterMenuCommand(UINT nID)
{
	if (!m_pFilterMenu)
		return;
	String masks = m_dirfilter.displayFilterHelper.GetMaskOrExpression();
	CDirFrame* pFrame = GetParentFrame();
	auto* pFilterBar = pFrame->GetFilterBar();
	if (pFilterBar)
		pFilterBar->GetDlgItemText(IDC_FILTERFILE_MASK, masks);
	auto newMasks = m_pFilterMenu->HandleMenuCommand(masks, nID, this);
	if (!newMasks.has_value())
		return;
	m_dirfilter.displayFilterHelper.SetMaskOrExpression(*newMasks);
	OnViewDisplayFilterBar();
	OnViewDisplayFilterBarApply();
}

/**
 * @brief User chose (main menu) Copy from right to left
 */
void CDirView::OnDirCopy(UINT id)
{
	bool to_right = (id == ID_L2R) ? true : false;
	if (GetDocument()->m_nDirs < 3)
	{
		if (to_right)
			OnCtxtDirCopy<SIDE_LEFT, SIDE_RIGHT>();
		else
			OnCtxtDirCopy<SIDE_RIGHT, SIDE_LEFT>();
	}
	else
	{
		if (to_right)
		{
			switch (m_nActivePane)
			{
			case 0:
				OnCtxtDirCopy<SIDE_LEFT, SIDE_MIDDLE>();
				break;
			case 1:
			case 2:
				OnCtxtDirCopy<SIDE_MIDDLE, SIDE_RIGHT>();
				break;
			}
		}
		else
		{
			switch (m_nActivePane)
			{
			case 0:
			case 1:
				OnCtxtDirCopy<SIDE_MIDDLE, SIDE_LEFT>();
				break;
			case 2:
				OnCtxtDirCopy<SIDE_RIGHT, SIDE_MIDDLE>();
				break;
			}
		}
	}
}

/**
 * @brief Prompt user whether to copy only different items
 */
std::optional<bool> CDirView::PromptCopyOnlyDiffItems()
{
	bool copyOnlyDiffItems = true;
	Counts counts = Count(&DirActions::IsItemIdenticalOrSkipped);
	if (counts.count > 0)
	{
		int ans = AfxMessageBox(_("Some selected items are identical or skipped.\nProcess only items with differences?").c_str(),
			MB_YESNOCANCEL | MB_ICONWARNING | MB_DONT_ASK_AGAIN, IDS_ONLYDIFFITEMS_CONFIRM);
		if (ans == IDCANCEL)
			return std::nullopt;
		copyOnlyDiffItems = (ans == IDYES);
	}
	return copyOnlyDiffItems;
}

std::optional<bool> CDirView::PromptPatchOnlyDiffItems()
{
	bool patchOnlyDiffItems = true;
	Counts counts = Count(&DirActions::IsItemIdenticalOrSkipped);
	if (counts.count > 0)
	{
		int ans = AfxMessageBox(_("Some selected items are identical or skipped.\nProcess only items with differences?").c_str(),
			MB_YESNOCANCEL | MB_ICONWARNING | MB_DONT_ASK_AGAIN, IDS_ONLYDIFFITEMS_CONFIRM);
		if (ans == IDCANCEL)
			return std::nullopt;
		patchOnlyDiffItems = (ans == IDYES);
	}
	return patchOnlyDiffItems;
}

/// User chose (context men) Copy from right to left
template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
void CDirView::OnCtxtDirCopy()
{
	auto copyOnlyDiffItems = PromptCopyOnlyDiffItems();
	if (!copyOnlyDiffItems.has_value())
		return;
	DoDirAction(*copyOnlyDiffItems
		? &DirActions::CopyDiffItems<srctype, dsttype>
		: &DirActions::Copy<srctype, dsttype>,
		_("Copying files..."));
}

/// User chose (context menu) Copy left to...
template<SIDE_TYPE stype>
void CDirView::OnCtxtDirCopyTo()
{
	DoDirActionTo(stype, &DirActions::CopyTo<stype>, _("Copying files..."));
}

/// Update context menu Copy Right to Left item
template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
void CDirView::OnUpdateCtxtDirCopy(CCmdUI* pCmdUI)
{
	DoUpdateDirCopy<srctype, dsttype>(pCmdUI, eContext);
}

/// Update main menu Copy Right to Left item
void CDirView::OnUpdateDirCopy(CCmdUI* pCmdUI)
{
	bool to_right = pCmdUI->m_nID == ID_L2R ? true : false;
	if (GetDocument()->m_nDirs < 3)
	{
		if (to_right)
			DoUpdateDirCopy<SIDE_LEFT, SIDE_RIGHT>(pCmdUI, eContext);
		else
			DoUpdateDirCopy<SIDE_RIGHT, SIDE_LEFT>(pCmdUI, eContext);
	}
	else
	{
		if (to_right)
		{
			switch (m_nActivePane)
			{
			case 0:
				DoUpdateDirCopy<SIDE_LEFT, SIDE_MIDDLE>(pCmdUI, eContext);
				break;
			case 1:
			case 2:
				DoUpdateDirCopy<SIDE_MIDDLE, SIDE_RIGHT>(pCmdUI, eContext);
				break;
			}
		}
		else
		{
			switch (m_nActivePane)
			{
			case 0:
			case 1:
				DoUpdateDirCopy<SIDE_MIDDLE, SIDE_LEFT>(pCmdUI, eContext);
				break;
			case 2:
				DoUpdateDirCopy<SIDE_RIGHT, SIDE_MIDDLE>(pCmdUI, eContext);
				break;
			}
		}
	}
}

void CDirView::DoDirAction(DirActions::method_type func, const String& status_message)
{
	CWaitCursor waitstatus;

	try {
		// First we build a list of desired actions
		FileActionScript actionScript;
		DirItemWithIndexIterator begin(m_pIList.get(), -1, true);
		DirItemWithIndexIterator end;
		FileActionScript *rsltScript;
		rsltScript = std::accumulate(begin, end, &actionScript, MakeDirActions(func));
		ASSERT(rsltScript == &actionScript);
		actionScript.RemoveDuplicates();
		// Now we prompt, and execute actions
		ConfirmAndPerformActions(actionScript);
	} catch (ContentsChangedException& e) {
		AfxMessageBox(e.m_msg.c_str(), MB_ICONWARNING);
	} catch (FileOperationException& e) {
		AfxMessageBox(e.m_msg.c_str(), MB_ICONWARNING);
	}
}

void CDirView::DoDirActionTo(SIDE_TYPE stype, DirActions::method_type func, const String& status_message)
{
	String destPath;
	String startPath(m_lastCopyFolder);
	String selectfolder_title;

	if (stype == SIDE_LEFT)
		selectfolder_title = _("Left Side - Select Destination Folder:");
	else if (stype == SIDE_MIDDLE)
		selectfolder_title = _("Middle Side - Select Destination Folder:");
	else if (stype == SIDE_RIGHT)
		selectfolder_title = _("Right Side - Select Destination Folder:");

	if (!SelectFolder(destPath, startPath.c_str(), selectfolder_title))
		return;

	m_lastCopyFolder = destPath;
	CWaitCursor waitstatus;

	try {
		// First we build a list of desired actions
		FileActionScript actionScript;
		actionScript.m_destBase = std::move(destPath);
		DirItemWithIndexIterator begin(m_pIList.get(), -1, true);
		DirItemWithIndexIterator end;
		FileActionScript *rsltScript;
		rsltScript = std::accumulate(begin, end, &actionScript, MakeDirActions(func));
		ASSERT(rsltScript == &actionScript);
		actionScript.RemoveDuplicates();
		// Now we prompt, and execute actions
		ConfirmAndPerformActions(actionScript);
	} catch (ContentsChangedException& e) {
		AfxMessageBox(e.m_msg.c_str(), MB_ICONWARNING);
	}
}

// Confirm with user, then perform the action list
void CDirView::ConfirmAndPerformActions(FileActionScript & actionList)
{
	if (actionList.GetActionItemCount() == 0) // Not sure it is possible to get right-click menu without
		return;    // any selected items, but may as well be safe

	ASSERT(actionList.GetActionItemCount()>0); // Or else the update handler got it wrong

	// Set parent window so modality is correct and correct window gets focus
	// after dialogs.
	actionList.SetParentWindow(this->GetSafeHwnd());
	
	try {
		ConfirmActionList(GetDiffContext(), actionList);
	} catch (ConfirmationNeededException& e) {
		ConfirmFolderCopyDlg dlg;
		dlg.m_caption = e.m_caption;
		dlg.m_question = e.m_question;
		dlg.m_fromText = e.m_fromText;
		dlg.m_toText = e.m_toText;
		dlg.m_fromPath = e.m_fromPath;
		dlg.m_toPath = e.m_toPath;
		INT_PTR ans = dlg.DoModal();
		if (ans != IDOK && ans != IDYES)
			return;
		RootLogger::Info(e.m_question + _T(": ") + e.m_fromPath + _T(" -> ") + e.m_toPath + _T(": ") + _("Yes"));
	}
	PerformActionList(actionList);
}

/**
 * @brief Perform an array of actions
 * @note There can be only COPY or DELETE actions, not both!
 */
void CDirView::PerformActionList(FileActionScript& actionScript)
{
	// Check option and enable putting deleted items to Recycle Bin
	actionScript.UseRecycleBin(GetOptionsMgr()->GetBool(OPT_USE_RECYCLE_BIN));

	actionScript.SetParentWindow(GetMainFrame()->GetSafeHwnd());

	theApp.AddOperation();

	String src, dst, op;
	const int itemCount = static_cast<int>(actionScript.GetActionItemCount());
	if (itemCount >= 1)
	{
		FileActionItem item = actionScript.GetHeadActionItem();
		if (itemCount == 1)
		{
			src = item.src;
			dst = item.dest;
		}
		else
		{
			const CDiffContext& ctxt = GetDiffContext();
			src = ctxt.GetPath(item.UIOrigin);
			if (item.atype == FileActionItem::ACT_DEL)
			{
				dst = actionScript.UseRecycleBin() ? _("Recycle Bin") : _("Permanently deleted");
			}
			else
			{
				if (item.UIResult != FileActionItem::UI_DONT_CARE)
					dst = (item.UIDestination == -1) ? actionScript.m_destBase : ctxt.GetPath(item.UIDestination);
				else
					dst = (!actionScript.m_destBase.empty()) ? actionScript.m_destBase : item.dest;
			}
		}
		if (item.atype == FileActionItem::ACT_COPY)
			op = _("Copying files...");
		else if (item.atype == FileActionItem::ACT_MOVE)
			op = _("Moving files...");
		else if (item.atype == FileActionItem::ACT_DEL)
			op = _("Deleting files...");
		else
			ASSERT(FALSE);
		String msg = op + _T(" (") + src;
		if (itemCount > 1)
			msg += _T(", ") + strutils::format_string1(_("Items: %1"), strutils::to_str(itemCount));
		msg +=  + _T(" -> ") + dst + _T(")");
		RootLogger::Info(msg);
	}

	bool succeeded = actionScript.Run();
	if (succeeded)
		UpdateAfterFileScript(actionScript);
	theApp.RemoveOperation();
	if (!succeeded && !actionScript.IsCanceled())
		throw FileOperationException(_T("File operation failed"));
	m_firstDiffItem.reset();
	m_lastDiffItem.reset();

	if (succeeded)
		RootLogger::Info(_("File operation completed successfully"));
	if (actionScript.IsCanceled())
		RootLogger::Info(_("File operation canceled"));
}

/**
 * @brief Update results after running FileActionScript.
 * This functions is called after script is finished to update
 * results (including UI).
 * @param [in] actionlist Script that was run.
 */
void CDirView::UpdateAfterFileScript(FileActionScript & actionList)
{
	bool bItemsRemoved = false;
	int curSel = GetFirstSelectedInd();
	CDiffContext& ctxt = GetDiffContext();
	while (actionList.GetActionItemCount()>0)
	{
		// Start handling from tail of list, so removing items
		// doesn't invalidate our item indexes.
		FileActionItem act = actionList.RemoveTailActionItem();

		// Update doc (difflist)
		UPDATEITEM_TYPE updatetype = UpdateDiffAfterOperation(act, ctxt, GetDiffItem(act.context));
		if (updatetype == UPDATEITEM_REMOVE)
		{
			DeleteItem(act.context, true);
			bItemsRemoved = true;
		}
		else if (updatetype == UPDATEITEM_UPDATE)
			UpdateDiffItemStatus(act.context);
	}
	
	// Make sure selection is at sensible place if all selected items
	// were removed.
	if (bItemsRemoved)
	{
		UINT selected = GetSelectedCount();
		if (selected == 0)
		{
			if (curSel < 1)
				++curSel;
			MoveFocus(0, curSel - 1, selected);
		}
	}
}

Counts CDirView::Count(DirActions::method_type2 func) const
{
	return ::Count(SelBegin(), SelEnd(), MakeDirActions(func));
}

/// Should Copy to Left be enabled or disabled ? (both main menu & context menu use this)
template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
void CDirView::DoUpdateDirCopy(CCmdUI* pCmdUI, eMenuType menuType)
{
	Counts counts = Count(&DirActions::IsItemCopyableOnTo<srctype, dsttype>);
	pCmdUI->Enable(counts.count > 0);
	if (menuType == eContext)
		pCmdUI->SetText(FormatMenuItemString(srctype, dsttype, counts.count, counts.total).c_str());
}

/// Should Move to Left be enabled or disabled ? (both main menu & context menu use this)
template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
void CDirView::DoUpdateDirMove(CCmdUI* pCmdUI, eMenuType menuType)
{
	Counts counts = Count(&DirActions::IsItemMovableOnTo<srctype, dsttype>);
	pCmdUI->Enable(counts.count > 0);
	if (menuType == eContext)
		pCmdUI->SetText(FormatMenuItemString(srctype, dsttype, counts.count, counts.total).c_str());
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CDirView::UpdateResources()
{
	UpdateColumnNames();
	GetParentFrame()->UpdateResources();
}

/**
 * @brief User just clicked a column, so perform sort
 */
void CDirView::OnColumnClick(NMHDR *pNMHDR, LRESULT *pResult)
{
	// set sort parameters and handle ascending/descending
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*) pNMHDR;
	int oldSortColumn = GetOptionsMgr()->GetInt((GetDocument()->m_nDirs < 3) ? OPT_DIRVIEW_SORT_COLUMN : OPT_DIRVIEW_SORT_COLUMN3);
	int sortcol = m_pColItems->ColPhysToLog(pNMListView->iSubItem);
	if (sortcol == oldSortColumn)
	{
		// Swap direction
		bool bSortAscending = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SORT_ASCENDING);
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SORT_ASCENDING, !bSortAscending);
	}
	else
	{
		GetOptionsMgr()->SaveOption((GetDocument()->m_nDirs < 3) ? OPT_DIRVIEW_SORT_COLUMN : OPT_DIRVIEW_SORT_COLUMN3, sortcol);
		// most columns start off ascending, but not dates
		bool bSortAscending = m_pColItems->IsDefaultSortAscending(sortcol);
		GetOptionsMgr()->SaveOption(OPT_DIRVIEW_SORT_ASCENDING, bSortAscending);
	}

	SortColumnsAppropriately();
	*pResult = 0;
}

void CDirView::SortColumnsAppropriately()
{
	int sortCol = GetOptionsMgr()->GetInt((GetDocument()->m_nDirs < 3) ? OPT_DIRVIEW_SORT_COLUMN : OPT_DIRVIEW_SORT_COLUMN3);
	if (sortCol < 0 || sortCol >= m_pColItems->GetColCount())
		return;

	const bool comparing =
		GetDocument()->m_diffThread.GetThreadState() == CDiffThread::THREAD_COMPARING;

	// Do not sort by columns whose values are updated asynchronously,
	// as this may violate the strict weak ordering required by std::stable_sort.
	if (comparing && !m_pColItems->IsColSortableWhileComparing(sortCol))
		return;
	
	bool bSortAscending = GetOptionsMgr()->GetBool(OPT_DIRVIEW_SORT_ASCENDING);
	m_ctlSortHeader.SetSortImage(m_pColItems->ColLogToPhys(sortCol), bSortAscending);
	//sort using static CompareFunc comparison function
	CompareState cs(&GetDiffContext(), m_pColItems.get(), sortCol, bSortAscending, m_bTreeMode);
	std::stable_sort(m_listViewItems.begin(), m_listViewItems.end(), [&cs](const ListViewOwnerDataItem& a, const ListViewOwnerDataItem& b)
		{ return CompareState::CompareFunc(a.lParam, b.lParam, reinterpret_cast<LPARAM>(&cs)) < 0; });

	m_firstDiffItem.reset();
	m_lastDiffItem.reset();
	
	m_pList->Invalidate();
}

/// Do any last minute work as view closes
void CDirView::OnDestroy()
{
	// The Grep dialog is a child window: its own OnDestroy stops
	// the scan, but close it explicitly so no scan outlives the view.
	m_wndGrep.CloseGrep();
	// Same for the bulk classification dialog (Merge20 Phase 4 D10).
	m_wndBulkClassify.CloseBulkClassify();
	DeleteAllDisplayItems();

	{
		const String keyname = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_ORDERS : OPT_DIRVIEW3_COLUMN_ORDERS;
		// Merge20 (Phase 4 D9): while slim is on, the live orders are
		// the slim preset, not the user's layout. Save the remembered
		// full orders instead; if they were never stored, keep the
		// previously saved value by skipping the save.
		if (m_bMerge20Slim)
		{
			if (!m_sMerge20FullColumnOrders.empty())
				GetOptionsMgr()->SaveOption(keyname, m_sMerge20FullColumnOrders);
		}
		else
			GetOptionsMgr()->SaveOption(keyname, m_pColItems->SaveColumnOrders());
	}
	{
		const String keyname = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS;
		// Merge20 (Phase 4 D9): same guard as the orders above.
		if (m_bMerge20Slim)
		{
			if (!m_sMerge20FullColumnWidths.empty())
				GetOptionsMgr()->SaveOption(keyname, m_sMerge20FullColumnWidths);
		}
		else
			GetOptionsMgr()->SaveOption(keyname,
				m_pColItems->SaveColumnWidths(std::bind(&CListCtrl::GetColumnWidth, m_pList, _1)));
	}

	__super::OnDestroy();
}

/**
 * @brief Open selected item when user presses ENTER key.
 */
void CDirView::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_RETURN)
	{
		int sel = GetFocusedItem();
		if (sel >= 0)
		{
			const DIFFITEM& di = GetDiffItem(sel);
			if (m_bTreeMode && GetDiffContext().m_bRecursive && di.diffcode.isDirectory())
			{
				if (di.customFlags & ViewCustomFlags::EXPANDED)
					CollapseSubdir(sel);
				else
					ExpandSubdir(sel);
			}
			else
			{
				OpenSelection();
			}
		}
	}
	__super::OnChar(nChar, nRepCnt, nFlags);
}

/**
 * @brief Expand/collapse subfolder when "+/-" icon is clicked.
 */
void CDirView::OnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNM = (LPNMITEMACTIVATE)pNMHDR;
	LVHITTESTINFO lvhti;
	lvhti.pt = pNM->ptAction;
	m_pList->SubItemHitTest(&lvhti);
	if (lvhti.flags == LVHT_ONITEMSTATEICON)
	{
		const DIFFITEM &di = GetDiffItem(pNM->iItem);
		if (di.customFlags & ViewCustomFlags::EXPANDED)
			CollapseSubdir(pNM->iItem);
		else
			ExpandSubdir(pNM->iItem);
	}
	// Merge20 (S2, design section 3.2): clicking the Output column cell
	// toggles the file's output checkbox. Directory rows and the ".."
	// parent row have no output flag and are ignored. The column value
	// itself is supplied dynamically through LVN_GETDISPINFO (this is an
	// owner-data list), so the row is redrawn instead of setting text.
	if ((lvhti.flags & LVHT_ONITEM) != 0 && lvhti.iItem >= 0 && lvhti.iSubItem >= 0)
	{
		int logCol = m_pColItems->ColPhysToLog(lvhti.iSubItem);
		if (m_pColItems->IsColOutputCheck(logCol))
		{
			DIFFITEM *pKey = GetItemKey(lvhti.iItem);
			if (pKey != nullptr && !IsDiffItemSpecial(pKey))
			{
				const DIFFITEM &di = GetDiffItem(lvhti.iItem);
				if (!di.diffcode.isDirectory())
				{
					CDirDoc *pDoc = GetDocument();
					pDoc->SetMerge20IncludeInOutput(di, !pDoc->GetMerge20IncludeInOutput(di));
					GetListCtrl().RedrawItems(lvhti.iItem, lvhti.iItem);
					UpdateMerge20StatusCount();
				}
			}
		}
	}

	*pResult = 0;
}

/**
 * @brief Collapse subfolder
 * @param [in] sel Folder item index in listview.
 */
void CDirView::CollapseSubdir(int sel)
{
	DIFFITEM& dip = this->GetDiffItem(sel);
	if (!m_bTreeMode || !(dip.customFlags & ViewCustomFlags::EXPANDED) || !dip.HasChildren())
		return;

	m_pList->SetRedraw(FALSE);	// Turn off updating (better performance)

	dip.customFlags &= ~ViewCustomFlags::EXPANDED;

	// Find the range to delete: all items after sel that are descendants of dip
	int firstToDelete = sel + 1;
	int count = m_pList->GetItemCount();
	int lastToDelete = firstToDelete;

	for (int i = firstToDelete; i < count; i++)
	{
		const DIFFITEM& di = GetDiffItem(i);
		if (!di.IsAncestor(&dip))
			break;
		lastToDelete = i + 1;  // exclusive end
	}

	// Bulk delete from vector and list view
	if (lastToDelete > firstToDelete)
	{
		m_listViewItems.erase(m_listViewItems.begin() + firstToDelete, m_listViewItems.begin() + lastToDelete);

		// Delete items from list view in reverse order to maintain indices
		for (int i = lastToDelete - 1; i >= firstToDelete; i--)
			m_pList->DeleteItem(i);
	}

	m_pList->SetRedraw(TRUE);	// Turn updating back on
	m_pList->Invalidate();
}

/**
 * @brief Expand subfolder
 * @param [in] sel Folder item index in listview.
 */
void CDirView::ExpandSubdir(int sel, bool bRecursive)
{
	DIFFITEM& dip = GetDiffItem(sel);
	if (!m_bTreeMode || (dip.customFlags & ViewCustomFlags::EXPANDED) || !dip.HasChildren())
		return;

	m_pList->SetRedraw(FALSE);	// Turn off updating (better performance)

	size_t oldItemCount = m_listViewItems.size();

	CDiffContext &ctxt = GetDiffContext();
	dip.customFlags |= ViewCustomFlags::EXPANDED;
	if (bRecursive)
		ExpandSubdirs(ctxt, dip);

	UINT indext = sel + 1;
	int alldiffs = 0;
	m_hasShowableDescendantCache.clear();
	RedisplayChildren(GetItemKey(sel), dip.GetDepth() + 1, indext, alldiffs);

	SortColumnsAppropriately();

	if (m_pList->GetNextItem(sel, LVNI_SELECTED) != -1)
	{
		LVITEM lvi {0, sel + 1};
		for (size_t i = 0; i < m_listViewItems.size() - oldItemCount; i++)
			m_pList->InsertItem(&lvi);
	}
	else
	{
		m_pList->SetItemCountEx(static_cast<int>(m_listViewItems.size()), LVSICF_NOSCROLL);
	}

	m_pList->SetRedraw(TRUE);	// Turn updating back on
	m_pList->Invalidate();
}

/**
 * @brief Open parent folder if possible.
 */
void CDirView::OpenParentDirectory(CDirDoc *pDocOpen)
{
	CDirDoc *pDoc = GetDocument();
	PathContext pathsParent;
	switch (CheckAllowUpwardDirectory(GetDiffContext(), pDoc->m_pTempPathContext, pathsParent))
	{
	case AllowUpwardDirectory::ParentIsTempPath:
		pDoc->m_pTempPathContext = pDoc->m_pTempPathContext->DeleteHead();
		[[fallthrough]];
	case AllowUpwardDirectory::ParentIsRegularPath: 
		{
			fileopenflags_t dwFlags[3];
			for (int nIndex = 0; nIndex < pathsParent.GetSize(); ++nIndex)
				dwFlags[nIndex] = FFILEOPEN_NOMRU | (pDoc->GetReadOnly(nIndex) ? FFILEOPEN_READONLY : 0);
			CMainFrame::OpenFolderParams openFolderParams(GetDiffContext().m_bRecursive);
			GetMainFrame()->DoFileOrFolderOpen(&pathsParent, dwFlags, nullptr, _T(""), (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? nullptr : pDocOpen, nullptr, nullptr, 0, &openFolderParams);
		}
		[[fallthrough]];
	case AllowUpwardDirectory::No:
		break;
	default:
		I18n::MessageBox(IDS_INVALID_DIRECTORY, MB_ICONSTOP);
		break;
	}
}

/**
 * @brief Get one or two selected items
 *
 * Returns false if 0 or more than 3 items selecte
 */
bool CDirView::GetSelectedItems(int * sel1, int * sel2, int * sel3)
{
	*sel2 = -1;
	*sel3 = -1;
	*sel1 = m_pList->GetNextItem(-1, LVNI_SELECTED);
	if (*sel1 == -1)
		return false;
	*sel2 = m_pList->GetNextItem(*sel1, LVNI_SELECTED);
	if (*sel2 == -1)
		return true;
	*sel3 = m_pList->GetNextItem(*sel2, LVNI_SELECTED);
	if (*sel3 == -1)
		return true;
	int extra = m_pList->GetNextItem(*sel3, LVNI_SELECTED);
	return (extra == -1);
}

/**
 * @brief Open special items (parent folders etc).
 * @param [in] pDoc Pointer to CDirDoc object.
 * @param [in] pos1 First item position.
 * @param [in] pos2 Second item position.
 */
void CDirView::OpenSpecialItems(CDirDoc *pDoc, DIFFITEM *pos1, DIFFITEM *pos2, DIFFITEM *pos3)
{
	if (pos2==nullptr && pos3==nullptr)
	{
		// Browse to parent folder(s) selected
		// SPECIAL_ITEM_POS is position for
		// special items, but there is currenly
		// only one (parent folder)
		OpenParentDirectory(pDoc);
	}
	else
	{
		// Parent directory & something else selected
		// Not valid action
	}
}

/**
 * @brief Creates a pairing folder for unique folder item.
 * This function creates a pairing folder for unique folder item in
 * folder compare. This way user can browse into unique folder's
 * contents and don't necessarily need to copy whole folder structure.
 * @return true if user agreed and folder was created.
 */
static bool CreateFoldersPair(const PathContext& paths)
{
	bool created = false;
	for (const auto& path : paths)
	{
		if (!paths::DoesPathExist(path))
		{
			String message =
				strutils::format_string1( 
					_("Create matching folder:\n%1\non the other side and open?"),
					path);
			int res = AfxMessageBox(message.c_str(), MB_YESNO | MB_ICONWARNING | MB_DONT_ASK_AGAIN, IDS_CREATE_PAIR_FOLDER);
			if (res == IDYES)
				created = paths::CreateIfNeeded(path);
		}
	}
	return created;
}

void CDirView::Open(CDirDoc *pDoc, const PathContext& paths, fileopenflags_t dwFlags[3], FileTextEncoding encoding[3], PackingInfo * infoUnpacker)
{
	bool isdir = false;
	for (auto path : paths)
	{
		if (paths::DoesPathExist(path) == paths::IS_EXISTING_DIR)
			isdir = true;
	}
	if (isdir)
	{
		// Open subfolders
		// Don't add folders to MRU
		CMainFrame::OpenFolderParams openFolderParams(GetDiffContext().m_bRecursive);
		GetMainFrame()->DoFileOrFolderOpen(&paths, dwFlags, nullptr, _T(""),
			((GetAsyncKeyState(VK_CONTROL) & 0x8000) || GetDiffContext().m_bRecursive) ? nullptr : pDoc, nullptr, nullptr, 0, &openFolderParams);
	}
	else if (HasZipSupport() && std::count_if(paths.begin(), paths.end(), ArchiveGuessFormat) == paths.GetSize())
	{
		// Open archives, not adding paths to MRU
		CMainFrame::OpenFolderParams openFolderParams(GetDiffContext().m_bRecursive);
		GetMainFrame()->DoFileOrFolderOpen(&paths, dwFlags, nullptr, _T(""), nullptr, infoUnpacker, nullptr, 0, &openFolderParams);
	}
	else
	{
		// Regular file case

		// Binary attributes are set after files are unpacked
		// so after plugins such as the MS-Office plugins have had a chance to make them textual
		// We haven't done unpacking yet in this diff, but if a binary flag is already set,
		// then it was set in a previous diff after unpacking, so we trust it

		// Open identical and different files
		PathContext filteredPaths;
		FileLocation fileloc[3];
		String strDesc[3];
		const String sUntitled[] = { _("Untitled Left"), paths.GetSize() < 3 ? _("Untitled Right") : _("Untitled Middle"), _("Untitled Right") };
		for (int i = 0; i < paths.GetSize(); ++i)
		{
			if (paths::DoesPathExist(paths[i]) == paths::DOES_NOT_EXIST)
			{
				strDesc[i] = sUntitled[i];
				filteredPaths.SetPath(i, paths::NATIVE_NULL_DEVICE_NAME, false);
			}
			else
			{
				fileloc[i].setPath(paths[i]);
				fileloc[i].encoding = encoding[i];
				filteredPaths.SetPath(i, paths[i], false);
			}
		}

		PackingInfo* tmpInfoUnpacker = nullptr;
		PrediffingInfo* infoPrediffer = nullptr;
		String filteredFilenames = CDiffContext::GetFilteredFilenames(filteredPaths);
		GetDiffContext().FetchPluginInfos(filteredFilenames, !infoUnpacker ? &infoUnpacker : &tmpInfoUnpacker, &infoPrediffer);

		GetMainFrame()->ShowAutoMergeDoc(0, GetDocument(), paths.GetSize(), fileloc,
			dwFlags, strDesc, _T(""), infoUnpacker, infoPrediffer);
	}
}

/**
 * @brief Open selected files or directories.
 *
 * Opens selected files to file compare. If comparing
 * directories non-recursively, then subfolders and parent
 * folder are opened too.
 *
 * This handles the case that one item is selected
 * and the case that two or three items are selected (one on each side)
 *
 * @param [in] pDoc Pointer to CDirDoc object.
 * @param [in] selectionType Type of Item Selection.
 * @param [in] infoUnpacker Unpacker plugin.
 * @param [in] openableForDir Are items openable if the items are directories?
 */
void CDirView::OpenSelection(CDirDoc *pDoc, SELECTIONTYPE selectionType /*= SELECTIONTYPE_NORMAL*/, PackingInfo * infoUnpacker /*= nullptr*/, bool openableForDir /*= true*/)
{
	// First, figure out what was selected
	int sel1 = -1, sel2 = -1, sel3 = -1;
	if (!GetSelectedItems(&sel1, &sel2, &sel3))
	{
		// Must have 1 or 2 or 3 items selected
		// Not valid action
		return;
	}
	OpenSelection(sel1, sel2, sel3, pDoc, selectionType, infoUnpacker, openableForDir);
}

/**
 * @brief Open selected files or directories.
 *
 * Opens selected files to file compare. If comparing
 * directories non-recursively, then subfolders and parent
 * folder are opened too.
 *
 * This handles the case that one item is selected
 * and the case that two or three items are selected (one on each side)
 *
 * @param [in] sel1 First item's selection index in listview.
 * @param [in] sel2 Second item's selection index in listview.
 * @param [in] sel3 sel3 Third item's selection index in listview.
 * @param [in] pDoc Pointer to CDirDoc object.
 * @param [in] selectionType Type of Item Selection.
 * @param [in] infoUnpacker Unpacker plugin.
 * @param [in] openableForDir Are items openable if the items are directories?
 */
void CDirView::OpenSelection(int sel1, int sel2, int sel3, CDirDoc *pDoc, SELECTIONTYPE selectionType /*= SELECTIONTYPE_NORMAL*/, PackingInfo * infoUnpacker /*= nullptr*/, bool openableForDir /*= true*/)
{
	Merge7zFormatMergePluginScope scope(infoUnpacker);
	const CDiffContext& ctxt = GetDiffContext();

	// First, figure out what was selected (store into pos1 & pos2 & pos3)
	DIFFITEM *pos1 = nullptr, *pos2 = nullptr, *pos3 = nullptr;
	pos1 = GetItemKey(sel1);
	ASSERT(pos1 != nullptr);
	if (sel2 != -1)
	{
		pos2 = GetItemKey(sel2);
		ASSERT(pos2 != nullptr);
		if (sel3 != -1)
			pos3 = GetItemKey(sel3);
	}

	// Now handle the various cases of what was selected

	if (IsDiffItemSpecial(pos1))
	{
		OpenSpecialItems(pDoc, pos1, pos2, pos3);
		return;
	}

	// Common variables which both code paths below are responsible for setting
	PathContext paths;
	const DIFFITEM *pdi[3] = {0}; // left & right items (di1==di2 if single selection)
	bool isdir = false; // set if we're comparing directories
	int nPane[3];
	FileTextEncoding encoding[3];
	String errmsg;
	bool success;
	if (pos2 && !pos3)
		success = GetOpenTwoItems(ctxt, selectionType, pos1, pos2, pdi,
				paths, sel1, sel2, isdir, nPane, encoding, errmsg, openableForDir);
	else if (pos2 && pos3)
		success = GetOpenThreeItems(ctxt, pos1, pos2, pos3, pdi,
				paths, sel1, sel2, sel3, isdir, nPane, encoding, errmsg, openableForDir);
	else
	{
		// Only one item selected, so perform diff on its sides
		success = GetOpenOneItem(ctxt, pos1, pdi, 
				paths, sel1, isdir, nPane, encoding, errmsg, openableForDir);
		if (isdir)
			CreateFoldersPair(paths);
	}
	if (!success)
	{
		if (!errmsg.empty())
			AfxMessageBox(errmsg.c_str(), MB_ICONSTOP);
		return;
	}

	// Now pathLeft, pathRight, di1, di2, and isdir are all set
	// We have two items to compare, no matter whether same or different underlying DirView item

	fileopenflags_t dwFlags[3];
	for (int nIndex = 0; nIndex < paths.GetSize(); nIndex++)
		dwFlags[nIndex] = FFILEOPEN_NOMRU | (GetDocument()->GetReadOnly(nPane[nIndex]) ? FFILEOPEN_READONLY : 0);

	Open(pDoc, paths, dwFlags, encoding, infoUnpacker);
}

void CDirView::OpenSelection(SELECTIONTYPE selectionType /*= SELECTIONTYPE_NORMAL*/, PackingInfo* infoUnpacker /*= nullptr*/, bool openableForDir /*= true*/)
{
	OpenSelection(GetDocument(), selectionType, infoUnpacker, openableForDir);
}

/**
 * @brief Opens selected files to compare by specifying the comparison method.
 *
 * This handles the case that one item is selected
 * and the case that two or three items are selected (one on each side)
 *
 * @param [in] id Menu ID of the selected item
 */
void CDirView::OpenSelectionAs(UINT id)
{
	// First, figure out what was selected
	int sel1 = -1, sel2 = -1, sel3 = -1;
	if (!GetSelectedItems(&sel1, &sel2, &sel3))
	{
		// Must have 1 or 2 or 3 items selected
		// Not valid action
		return;
	}
	OpenSelectionAs(sel1, sel2, sel3, id);
}

/**
 * @brief Opens selected files to compare by specifying the comparison method.
 *
 * This handles the case that one item is selected
 * and the case that two or three items are selected (one on each side)
 *
 * @param [in] sel1 First item's selection index in listview.
 * @param [in] sel2 Second item's selection index in listview.
 * @param [in] sel3 Third item's selection index in listview.
 * @param [in] id Menu ID of the selected item
 */
void CDirView::OpenSelectionAs(int sel1, int sel2, int sel3, UINT id)
{
	CDirDoc * pDoc = GetDocument();
	const CDiffContext& ctxt = GetDiffContext();

	// First, figure out what was selected (store into pos1 & pos2 & pos3)
	DIFFITEM *pos1 = nullptr, *pos2 = nullptr, *pos3 = nullptr;
	pos1 = GetItemKey(sel1);
	ASSERT(pos1);
	if (sel2 != -1)
	{
		pos2 = GetItemKey(sel2);
		ASSERT(pos2 != nullptr);
		if (sel3 != -1)
			pos3 = GetItemKey(sel3);
	}

	// Now handle the various cases of what was selected

	if (IsDiffItemSpecial(pos1))
	{
		ASSERT(false);
		return;
	}

	// Common variables which both code paths below are responsible for setting
	PathContext paths;
	const DIFFITEM *pdi[3]; // left & right items (di1==di2 if single selection)
	bool isdir = false; // set if we're comparing directories
	int nPane[3];
	FileTextEncoding encoding[3];
	String errmsg;
	bool success;
	if (pos2 && !pos3)
		success = GetOpenTwoItems(ctxt, SELECTIONTYPE_NORMAL, pos1, pos2, pdi,
				paths, sel1, sel2, isdir, nPane, encoding, errmsg, false);
	else if (pos2 && pos3)
		success = GetOpenThreeItems(ctxt, pos1, pos2, pos3, pdi,
				paths, sel1, sel2, sel3, isdir, nPane, encoding, errmsg, false);
	else
	{
		// Only one item selected, so perform diff on its sides
		success = GetOpenOneItem(ctxt, pos1, pdi,
				paths, sel1, isdir, nPane, encoding, errmsg, false);
	}
	if (!success)
	{
		if (!errmsg.empty())
			AfxMessageBox(errmsg.c_str(), MB_ICONSTOP);
		return;
	}

	// Open identical and different files
	const String sUntitled[] = { _("Untitled Left"), paths.GetSize() < 3 ? _("Untitled Right") : _("Untitled Middle"), _("Untitled Right") };
	fileopenflags_t dwFlags[3] = { 0 };
	String strDesc[3];
	PathContext filteredPaths;
	FileLocation fileloc[3];
	for (int pane = 0; pane < paths.GetSize(); pane++)
	{
		if (paths::DoesPathExist(paths[pane]) == paths::DOES_NOT_EXIST)
		{
			strDesc[pane] = sUntitled[pane];
			filteredPaths.SetPath(pane, paths::NATIVE_NULL_DEVICE_NAME, false);
		}
		else
		{
			fileloc[pane].setPath(paths[pane]);
			fileloc[pane].encoding = encoding[pane];
			filteredPaths.SetPath(pane, paths[pane]);
		}
		dwFlags[pane] |= FFILEOPEN_NOMRU | (pDoc->GetReadOnly(nPane[pane]) ? FFILEOPEN_READONLY : 0);
	}
	PackingInfo* infoUnpacker = nullptr;
	PrediffingInfo* infoPrediffer = nullptr;
	GetDiffContext().FetchPluginInfos(CDiffContext::GetFilteredFilenames(filteredPaths), &infoUnpacker, &infoPrediffer);
	if (ID_UNPACKERS_FIRST <= id && id <= ID_UNPACKERS_LAST)
	{
		PackingInfo infoUnpackerAlt(
				PluginMenu::GetPluginPipelineByMenuId(nullptr, id, FileTransform::UnpackerEventNames, ID_UNPACKERS_FIRST));
		CMainFrame::OpenFolderParams openFolderParams(ctxt.m_bRecursive);
		GetMainFrame()->DoFileOrFolderOpen(&paths, dwFlags, strDesc, _T(""),
			nullptr, &infoUnpackerAlt, infoPrediffer, 0, &openFolderParams);
	}
	else
	{
		GetMainFrame()->ShowMergeDoc(id, pDoc, paths.GetSize(), fileloc, dwFlags, strDesc, _T(""), infoUnpacker, infoPrediffer);
	}
}

/// User chose (context menu) delete left
template<SIDE_TYPE stype>
void CDirView::OnCtxtDirDel()
{
	DoDirAction(&DirActions::DeleteOn<stype>, _("Deleting files..."));
}

/// User chose (context menu) delete both
void CDirView::OnCtxtDirDelBoth()
{
	DoDirAction(&DirActions::DeleteOnBoth, _("Deleting files..."));
}

/// Enable/disable Delete Left menu choice on context menu
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirDel(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemDeletableOn<stype>);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemString(stype, counts.count, counts.total).c_str());
}

/// Enable/disable Delete Both menu choice on context menu
void CDirView::OnUpdateCtxtDirDelBoth(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemDeletableOnBoth);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringAll(GetDocument()->m_nDirs, counts.count, counts.total).c_str());
}

/**
 * @brief Update "Copy | Right to..." item
 */
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirCopyTo(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemCopyableToOn<stype>);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringTo(stype, counts.count, counts.total).c_str());
}

void CDirView::OnUpdateCtxtDirCopyBothTo(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemCopyableBothToOn);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringAllTo(GetDocument()->m_nDirs, counts.count, counts.total).c_str());
}

void CDirView::OnUpdateCtxtDirCopyBothDiffsOnlyTo(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemNavigableDiff);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringDifferencesTo(counts.count, counts.total).c_str());
}
	
/**
 * @brief Update "Copy | Left/Right/Both " item
 */
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirCopy2(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemCopyableToOn<stype>);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemString(stype, counts.count, counts.total).c_str());
}

void CDirView::OnUpdateCtxtDirCopyBoth2(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemCopyableBothToOn);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringAll(GetDocument()->m_nDirs, counts.count, counts.total).c_str());
}

/**
 * @brief Get keydata associated with item in given index.
 * @param [in] idx Item's index to list in UI.
 * @return Key for item in given index.
 */
DIFFITEM *CDirView::GetItemKey(int idx) const
{
	ASSERT(idx >= 0 && idx < static_cast<int>(m_listViewItems.size()));
	return reinterpret_cast<DIFFITEM*>(m_listViewItems[idx].lParam);
}

// SetItemKey & GetItemKey encapsulate how the display list items
// are mapped to DiffItems, which in turn are DiffContext keys to the actual DIFFITEM data

/**
 * @brief Get DIFFITEM data for item.
 * This function returns DIFFITEM data for item in given index in GUI.
 * @param [in] sel Item's index in folder compare GUI list.
 * @return DIFFITEM for item.
 */
const DIFFITEM &CDirView::GetDiffItem(int sel) const
{
	CDirView * pDirView = const_cast<CDirView *>(this);
	return pDirView->GetDiffItem(sel);
}

/**
 * Given index in list control, get modifiable reference to its DIFFITEM data
 */
DIFFITEM &CDirView::GetDiffItem(int sel)
{
	DIFFITEM *diffpos = GetItemKey(sel);

	// If it is special item, return empty DIFFITEM
	if (IsDiffItemSpecial(diffpos))
	{
		return *DIFFITEM::GetEmptyItem();
	}
	return GetDiffContext().GetDiffRefAt(diffpos);
}

void CDirView::DeleteItem(int sel, bool removeDIFFITEM)
{
	DIFFITEM *diffpos = GetItemKey(sel);
	if (IsDiffItemSpecial(diffpos))
		return;
	if (m_bTreeMode)
	{
		CollapseSubdir(sel);
		m_listViewItems.erase(m_listViewItems.begin() + sel);
		m_pList->DeleteItem(sel);
	}
	else if (GetDiffContext().m_bRecursive || diffpos->HasChildren())
	{
		DirItemIterator it;
		for (it = RevBegin(); it != RevEnd(); )
		{
			DIFFITEM& di = *it;
			int cursel = it.m_sel;
			++it;
			if (di.IsAncestor(diffpos) || diffpos == &di)
			{
				m_listViewItems.erase(m_listViewItems.begin() + cursel);
				m_pList->DeleteItem(cursel);
			}
		}
	}
	else
	{
		m_listViewItems.erase(m_listViewItems.begin() + sel);
		m_pList->DeleteItem(sel);
	}
	if (removeDIFFITEM)
	{
		if (diffpos->renameMoveGroupId != -1)
			GetDiffContext().m_pRenameMoveDetection->RemoveItemFromGroup(diffpos);
		if (diffpos->HasChildren())
			diffpos->RemoveChildren();
		diffpos->DelinkFromSiblings();
		delete diffpos;
	}

	m_firstDiffItem.reset();
	m_lastDiffItem.reset();
}

void CDirView::DeleteAllDisplayItems()
{
	// item data are just positions (diffposes)
	// that is, they contain no memory needing to be freed
	m_pList->DeleteAllItems();
	m_listViewItems.clear();

	m_firstDiffItem.reset();
	m_lastDiffItem.reset();
}

/**
 * @brief Given key, get index of item which has it stored.
 * This function searches from list in UI.
 */
int CDirView::GetItemIndex(DIFFITEM *key)
{
	for (size_t i = 0; i < m_listViewItems.size(); ++i)
	{
		if (m_listViewItems[i].lParam == reinterpret_cast<LPARAM>(key))
			return static_cast<int>(i);
	}
	return 0;
}

/**
 * @brief Get the file names on both sides for specified item.
 * @note Return empty strings if item is special item.
 */
void CDirView::GetItemFileNames(int sel, String& strLeft, String& strRight) const
{
	DIFFITEM *diffpos = GetItemKey(sel);
	if (IsDiffItemSpecial(diffpos))
	{
		strLeft.erase();
		strRight.erase();
	}
	else
	{
		const CDiffContext& ctxt = GetDiffContext();
		::GetItemFileNames(ctxt, ctxt.GetDiffAt(diffpos), strLeft, strRight);
	}
}

/**
 * @brief Get the file names on both sides for specified item.
 * @note Return empty strings if item is special item.
 */
void CDirView::GetItemFileNames(int sel, PathContext * paths) const
{
	DIFFITEM *diffpos = GetItemKey(sel);
	if (IsDiffItemSpecial(diffpos))
	{
		for (int nIndex = 0; nIndex < GetDocument()->m_nDirs; nIndex++)
			paths->SetPath(nIndex, _T(""));
	}
	else
	{
		const CDiffContext& ctxt = GetDiffContext();
		*paths = ::GetItemFileNames(ctxt, ctxt.GetDiffAt(diffpos));
	}
}

/**
 * @brief Open selected file with registered application.
 * Uses shell file associations to open file with registered
 * application. We first try to use "Edit" action which should
 * open file to editor, since we are more interested editing
 * files than running them (scripts).
 * @param [in] stype Side of file to open.
 */
void CDirView::DoOpen(SIDE_TYPE stype)
{
	for (DirItemIterator it = SelBegin(); it != SelEnd(); ++it)
	{
		if (IsItemOpenableOn(*it, SideToIndex(GetDiffContext(), stype))) {
			String file = GetSelectedFileName(it, stype, GetDiffContext());
			if (!file.empty())
				shell::Edit(file.c_str());
		}
	}
}

/// Open with dialog for file on selected side
void CDirView::DoOpenWith(SIDE_TYPE stype)
{
	int sel = GetSingleSelectedItem();
	if (sel == -1) return;
	DirItemIterator dirBegin = SelBegin();
	String file = GetSelectedFileName(dirBegin, stype, GetDiffContext());
	if (file.empty()) return;
	shell::OpenWith(file.c_str());
}

/// Open selected file  on specified side to external editor
void CDirView::DoOpenWithEditor(SIDE_TYPE stype)
{
	for (DirItemIterator it = SelBegin(); it != SelEnd(); ++it)
	{
		if (IsItemOpenableOnWith(*it, SideToIndex(GetDiffContext(), stype))) {
			String file = GetSelectedFileName(it, stype, GetDiffContext());
			if (!file.empty())
				CMergeApp::OpenFileToExternalEditor(file);
		}
	}
}

void CDirView::DoOpenParentFolder(SIDE_TYPE stype)
{
	for (DirItemIterator it = SelBegin(); it != SelEnd(); ++it)
	{
		if (IsParentFolderOpenable(*it, SideToIndex(GetDiffContext(), stype))) {
			String file = GetSelectedFileName(it, stype, GetDiffContext());
			if (!file.empty())
				shell::OpenParentFolder(file.c_str());
		}
	}
}

/// Update context menuitem "Open left | with editor"
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirOpenWithEditor(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemOpenableOnWith<stype>);
	int selected = GetSelectedCount();
	pCmdUI->Enable(counts.count > 0 && selected == counts.count);
}

// return selected item index, or -1 if none or multiple
int CDirView::GetSingleSelectedItem() const
{
	int sel = -1, sel2 = -1;
	sel = m_pList->GetNextItem(sel, LVNI_SELECTED);
	if (sel == -1) return -1;
	sel2 = m_pList->GetNextItem(sel, LVNI_SELECTED);
	if (sel2 != -1) return -1;
	return sel;
}
// Enable/disable Open Left menu choice on context menu
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirOpen(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemOpenableOn<stype>);
	int selected = GetSelectedCount();
	pCmdUI->Enable(counts.count > 0 && selected == counts.count);
}

// Enable/disable Open Left With menu choice on context menu
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirOpenWith(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemOpenableOnWith<stype>);
	pCmdUI->Enable(counts.count > 0 && counts.total == 1);
}

// Enable/disable Open Parent Folder menu choice on context menu
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirOpenParentFolder(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsParentFolderOpenable<stype>);
	int selected = GetSelectedCount();
	pCmdUI->Enable(counts.count > 0 && selected == counts.count);
}

// Used for Open
/**
 * @brief Returns if the selected items are comparable.
 *
 * This handles the case that one item is selected
 * and the case that two or three items are selected (one on each side).
 *
 * @param [in] selectionType Type of Item Selection.
 * @param [in] openableForDir Are items openable if the items are directories?
 * @return Are the selected items comparable?
 */
bool CDirView::AreItemsComparable(SELECTIONTYPE selectionType, bool openableForDir /*= true*/)
{
	int sel1 = -1, sel2 = -1, sel3 = -1;
	if (!GetSelectedItems(&sel1, &sel2, &sel3))
	{
		// 0 items or more than 3 items seleted
		return false;
	}
	if (sel2 == -1)
	{
		// One item selected
		if (selectionType != SELECTIONTYPE_NORMAL)
			return false;
		if (!openableForDir)
		{
			const DIFFITEM& di1 = GetDiffItem(sel1);
			if (di1.diffcode.isDirectory() || IsDiffItemSpecial(GetItemKey(sel1)))
				return false;
		}
	}
	else if (sel3 == -1)
	{
		// Two items selected
		const DIFFITEM& di1 = GetDiffItem(sel1);
		const DIFFITEM& di2 = GetDiffItem(sel2);
		if (!AreItemsOpenable(GetDiffContext(), selectionType, di1, di2, openableForDir))
			return false;
	}
	else
	{
		// Three items selected
		const DIFFITEM& di1 = GetDiffItem(sel1);
		const DIFFITEM& di2 = GetDiffItem(sel2);
		const DIFFITEM& di3 = GetDiffItem(sel3);
		if (selectionType != SELECTIONTYPE_NORMAL || !::AreItemsOpenable(GetDiffContext(), di1, di2, di3, openableForDir))
			return false;
	}
	return true;
}

/**
 * @brief Returns if the selected items are comparable.
 * This function checks whether the selected items are individually comparable.
 *
 * @param [in] nID Menu ID of the selected item
 * @param [in] openableForDir Are items openable if the items are directories?
 * @return Are the selected items comparable?
 */
bool CDirView::AreItemsComparableIndivisually(UINT nID, bool openableForDir)
{
	int selected = GetSelectedCount();
	int count = 0;
	for (DirItemIterator it = SelBegin(); it != SelEnd(); ++it)
		if (openableForDir || !(*it).diffcode.isDirectory())
			count++;
	if (count <= 0 || selected != count)
		return false;

	// Disable "Compare" menu if multiple selected items contain directories when not in tree mode.
	if (nID == ID_MERGE_COMPARE && selected > 1 && !GetDiffContext().m_bRecursive)
		for (DirItemIterator it = SelBegin(); it != SelEnd(); ++it)
			if ((*it).diffcode.isDirectory())
				return false;

	return true;
}

/**
 * @brief Enable/disable the specified menu choice on menu or context menu.
 * @param [in] selectionType Type of Item Selection.
 * @param [in] pCmdUI UI component to update.
 * @param [in] openableForDir Are items openable if the items are directories?
 */
void CDirView::DoUpdateOpen(SELECTIONTYPE selectionType, CCmdUI* pCmdUI, bool openableForDir /*= true*/)
{
	bool bOn = AreItemsComparable(selectionType, openableForDir);
	pCmdUI->Enable(static_cast<BOOL>(bOn));
}

/**
 * @brief Return count of selected items in folder compare.
 */
UINT CDirView::GetSelectedCount() const
{
	return m_pList->GetSelectedCount();
}

/**
 * @brief Return index of first selected item in folder compare.
 */
int CDirView::GetFirstSelectedInd()
{
	return m_pList->GetNextItem(-1, LVNI_SELECTED);
}

// Go to first diff
// If none or one item selected select found item
// This is used for scrolling to first diff too
void CDirView::OnFirstdiff()
{
	DirItemIterator it =
		std::find_if(Begin(), End(), MakeDirActions(&DirActions::IsItemNavigableDiff));
	if (it != End())
		MoveFocus(GetFirstSelectedInd(), it.m_sel, GetSelectedCount());
}

void CDirView::OnUpdateFirstdiff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetFirstDifferentItem() > -1);
}

// Go to last diff
// If none or one item selected select found item
void CDirView::OnLastdiff()
{
	DirItemIterator it =
		std::find_if(RevBegin(), RevEnd(), MakeDirActions(&DirActions::IsItemNavigableDiff));
	if (it != RevEnd())
		MoveFocus(GetFirstSelectedInd(), it.m_sel, GetSelectedCount());
}

void CDirView::OnUpdateLastdiff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetFirstDifferentItem() > -1);
}

bool CDirView::HasNextDiff()
{
	int lastDiff = GetLastDifferentItem();

	// Check if different files were found and
	// there is different item after focused item
	return (lastDiff > -1) && (GetFocusedItem() < lastDiff);
}

bool CDirView::HasPrevDiff()
{
	int firstDiff = GetFirstDifferentItem();

	// Check if different files were found and
	// there is different item before focused item
	return (firstDiff > -1) && (firstDiff < GetFocusedItem());
}

void CDirView::MoveToNextDiff()
{
	int currentInd = GetFocusedItem();
	DirItemIterator begin(m_pIList.get(), currentInd + 1);
	DirItemIterator it =
		std::find_if(begin, End(), MakeDirActions(&DirActions::IsItemNavigableDiff));
	if (it != End())
		MoveFocus(currentInd, it.m_sel, GetSelectedCount());
}

void CDirView::MoveToPrevDiff()
{
	int currentInd = GetFocusedItem();
	if (currentInd <= 0)
		return;
	DirItemIterator begin(m_pIList.get(), currentInd - 1, false, true);
	DirItemIterator it =
		std::find_if(begin, RevEnd(), MakeDirActions(&DirActions::IsItemNavigableDiff));
	if (it != RevEnd())
		MoveFocus(currentInd, it.m_sel, GetSelectedCount());
}

void CDirView::OpenNextDiff()
{
	MoveToNextDiff();
	int currentInd = GetFocusedItem();
	const DIFFITEM& dip = GetDiffItem(currentInd);
	if (!dip.diffcode.isDirectory())
	{
		OpenSelection();
	}
	else
	{
		GetParentFrame()->ActivateFrame();
	}
}

void CDirView::OpenPrevDiff()
{
	MoveToPrevDiff();
	int currentInd = GetFocusedItem();
	const DIFFITEM& dip = GetDiffItem(currentInd);
	if (!dip.diffcode.isDirectory())
	{
		OpenSelection();
	}
	else
	{
		GetParentFrame()->ActivateFrame();
	}
}

void CDirView::OpenFirstFile()
{
	int currentInd = GetFocusedItem();
	int firstFileInd = 0;
	// Skip directories
	while (firstFileInd <= currentInd)
	{
		DIFFITEM& dip = GetDiffItem(firstFileInd);
		if (!dip.diffcode.isDirectory())
		{
			MoveFocus(currentInd, firstFileInd, 1);
			OpenSelection();
			break;
		}		
		firstFileInd++;
	}
}

bool CDirView::IsFirstFile()
{
	// Merge20 (Phase 4 D3): with unreviewed-only patrol on, "first"
	// means "no patrol move backward exists": the patrol wraps, so a
	// previous TODO file exists unless there is no TODO file at all,
	// or the current file is the only TODO file. The dedicated bar's
	// Prev button and the main frame's Prev/Next update handlers read
	// this (and IsLastFile).
	if (GetOptionsMgr()->GetBool(OPT_MERGE20_TODO_ONLY))
		return !Merge20HasOtherTarget(Merge20BuildPatrolTargets(true), GetFocusedItem());
	int currentInd = GetFocusedItem();
	int firstFileInd = 0;
	while (firstFileInd <= currentInd)
	{
		DIFFITEM& dip = GetDiffItem(firstFileInd);
		if (!dip.diffcode.isDirectory())
		{
			if (currentInd == firstFileInd)
				return true;
			else
				return false;
		}
		firstFileInd++;
	}
	return false;
}

void CDirView::OpenLastFile()
{
	const int count = m_pList->GetItemCount();
	int currentInd = GetFocusedItem();
	int lastFileInd = count - 1;
	// Skip directories
	while (lastFileInd >= 0)
	{
		DIFFITEM& dip = GetDiffItem(lastFileInd);
		if (!dip.diffcode.isDirectory())
		{
			MoveFocus(currentInd, lastFileInd, 1);
			OpenSelection();
			break;
		}
		lastFileInd--;
	}
}

bool CDirView::IsLastFile()
{
	// Merge20 (Phase 4 D3): see IsFirstFile. With unreviewed-only
	// patrol on, "last" means "no patrol move forward exists".
	if (GetOptionsMgr()->GetBool(OPT_MERGE20_TODO_ONLY))
		return !Merge20HasOtherTarget(Merge20BuildPatrolTargets(true), GetFocusedItem());
	const int count = m_pList->GetItemCount();
	int currentInd = GetFocusedItem();
	int lastFileInd = count - 1;
	while (lastFileInd >= currentInd)
	{
		DIFFITEM& dip = GetDiffItem(lastFileInd);
		if (!dip.diffcode.isDirectory())
		{
			if (currentInd == lastFileInd)
				return true;
			else
				return false;
		}
		lastFileInd--;
	}
	return false;
}

void CDirView::OpenNextFile()
{
	const int count = m_pList->GetItemCount();
	int currentInd = GetFocusedItem();
	// Merge20 (Phase 4 D3, spec section 4(g), HTML navFile): with
	// unreviewed-only patrol on, move to the next TODO file, wrapping
	// around to the first one. This is the choke point for the
	// dedicated bar's Next button, Ctrl+F8 and the menu (all go
	// through CDirDoc::MoveToNextFile), so all of them patrol TODO
	// files while the option is on, in dedicated mode and in normal
	// mode alike.
	// Merge20 (best-workflow B3): the patrol order comes from
	// Merge20BuildPatrolOrder: list order when risk-order patrol
	// is off (the D3 order, unchanged), risk order (High, Medium,
	// None, unclassified last) when it is on. The stepping (wrap)
	// is the same D3 rule. With TODO-only off and risk order on,
	// all displayed files are visited in risk order instead,
	// without wrapping (the plain patrol never wraps).
	if (GetOptionsMgr()->GetBool(OPT_MERGE20_TODO_ONLY))
	{
		if (currentInd < 0)
			return;
		const std::vector<int> order = Merge20BuildPatrolOrder(true);
		const int nextInd = Merge20FindNextInOrder(order, currentInd, 1, true);
		if (nextInd == -1)
		{
			// No unreviewed file at all: stay, and say so in the
			// status bar (the S3 count also shows "0 unreviewed").
			// No message box (D3 decision, see MERGE20_NOTES.md §25).
			GetParentFrame()->SetStatus(_("No unreviewed files").c_str());
			return;
		}
		if (nextInd == currentInd)
			return; // The current file is the only unreviewed one.
		MoveFocus(currentInd, nextInd, 1);
		m_pList->EnsureVisible(nextInd, FALSE);
		OpenSelection();
		return;
	}
	if (GetOptionsMgr()->GetBool(OPT_MERGE20_RISK_ORDER))
	{
		if (currentInd < 0)
			return;
		const std::vector<int> order = Merge20BuildPatrolOrder(false);
		const int nextInd = Merge20FindNextInOrder(order, currentInd, 1, false);
		if (nextInd == -1 || nextInd == currentInd)
			return; // Last file in risk order, or the only file.
		MoveFocus(currentInd, nextInd, 1);
		m_pList->EnsureVisible(nextInd, FALSE);
		OpenSelection();
		return;
	}
	int nextInd = currentInd + 1;
	if (currentInd >= 0)
	{
		while (nextInd < count)
		{
			DIFFITEM& dip = GetDiffItem(nextInd);
			MoveFocus(nextInd - 1, nextInd, 1);
			if (!dip.diffcode.isDirectory())
			{				
				OpenSelection();
				break;
			}
			nextInd++;
		}
	}
}

void CDirView::OpenPrevFile()
{
	int currentInd = GetFocusedItem();
	// Merge20 (Phase 4 D3): see OpenNextFile. Previous TODO file,
	// wrapping around to the last one.
	if (GetOptionsMgr()->GetBool(OPT_MERGE20_TODO_ONLY))
	{
		if (currentInd < 0)
			return;
		const std::vector<int> order = Merge20BuildPatrolOrder(true);
		const int prevInd = Merge20FindNextInOrder(order, currentInd, -1, true);
		if (prevInd == -1)
		{
			GetParentFrame()->SetStatus(_("No unreviewed files").c_str());
			return;
		}
		if (prevInd == currentInd)
			return; // The current file is the only unreviewed one.
		MoveFocus(currentInd, prevInd, 1);
		m_pList->EnsureVisible(prevInd, FALSE);
		OpenSelection();
		return;
	}
	// Merge20 (best-workflow B3): see OpenNextFile. Previous file
	// in risk order, without wrapping.
	if (GetOptionsMgr()->GetBool(OPT_MERGE20_RISK_ORDER))
	{
		if (currentInd < 0)
			return;
		const std::vector<int> order = Merge20BuildPatrolOrder(false);
		const int prevInd = Merge20FindNextInOrder(order, currentInd, -1, false);
		if (prevInd == -1 || prevInd == currentInd)
			return; // First file in risk order, or the only file.
		MoveFocus(currentInd, prevInd, 1);
		m_pList->EnsureVisible(prevInd, FALSE);
		OpenSelection();
		return;
	}
	int prevInd = currentInd - 1;
	if (currentInd >= 0)
	{
		while (prevInd >= 0)
		{
			DIFFITEM& dip = GetDiffItem(prevInd);
			MoveFocus(prevInd + 1, prevInd, 1);
			if (!dip.diffcode.isDirectory())
			{
				OpenSelection();
				break;
			}
			prevInd--;
		}
	}
}

/**
 * @brief Merge20 (Phase 4 D3): patrol targets over the displayed rows.
 * @param [in] bTodoOnly False marks every displayed file row, true
 *             marks only the unreviewed (TODO) file rows.
 * @return One entry per list row (directories and the ".." parent row
 *         are never targets), in list order, for Merge20Patrol.
 */
std::vector<bool> CDirView::Merge20BuildPatrolTargets(bool bTodoOnly) const
{
	const CDirDoc* pDoc = GetDocument();
	const int n = m_pList->GetItemCount();
	std::vector<bool> targets(static_cast<size_t>(n), false);
	for (int i = 0; i < n; ++i)
	{
		DIFFITEM* pKey = GetItemKey(i);
		if (pKey == nullptr || IsDiffItemSpecial(pKey))
			continue;
		const DIFFITEM& di = GetDiffItem(i);
		if (di.diffcode.isDirectory())
			continue;
		targets[static_cast<size_t>(i)] = !bTodoOnly ||
			pDoc->GetMerge20ReviewState(di) == ReviewState::TODO;
	}
	return targets;
}

/**
 * @brief Merge20 (Phase 4 D3): patrol order of the displayed rows.
 * @param [in] bTodoOnly False takes every displayed file row, true
 *             takes only the unreviewed (TODO) file rows (same set
 *             as Merge20BuildPatrolTargets).
 * @return The target row indices in patrol order: list order when
 *         risk-order patrol is off (the D3 order), risk order when
 *         it is on. The rank of a row comes from its S4
 *         classification (Merge20RiskRankOfFile); directories and
 *         the ".." parent row are never targets, so their rank is
 *         never read.
 */
std::vector<int> CDirView::Merge20BuildPatrolOrder(bool bTodoOnly) const
{
	const std::vector<bool> targets = Merge20BuildPatrolTargets(bTodoOnly);
	if (!GetOptionsMgr()->GetBool(OPT_MERGE20_RISK_ORDER))
	{
		std::vector<int> order;
		for (size_t i = 0; i < targets.size(); ++i)
		{
			if (targets[i])
				order.push_back(static_cast<int>(i));
		}
		return order;
	}
	const CDirDoc* pDoc = GetDocument();
	const CDiffContext& ctxt = pDoc->GetDiffContext();
	std::vector<Merge20RiskRank> ranks(targets.size(), Merge20RiskRank::UNCLASSIFIED);
	for (size_t i = 0; i < targets.size(); ++i)
	{
		if (!targets[i])
			continue;
		const DIFFITEM& di = GetDiffItem(static_cast<int>(i));
		const Merge20FileState& state = ctxt.GetMerge20FileStates().GetState(di.getItemRelativePath());
		ranks[static_cast<size_t>(i)] = Merge20RiskRankOfFile(state.classified, state.risk);
	}
	return Merge20BuildRiskOrder(ranks, targets);
}

/**
 * @brief Merge20 (Phase 4 D3): move the focus to the previous/next
 * TODO file (Up/Down-key patrol in the folder list). The file is not
 * opened: Up/Down move the selection, as they always did.
 * @param [in] direction +1 for the next TODO file, -1 for the previous
 *             one (wrapping around both ends).
 * @return False when there is no TODO file at all, or the current
 *         file is the only TODO file (the caller then shows
 *         "No unreviewed files" in the status bar).
 *
 * Merge20 (best-workflow B3): the move follows the patrol order
 * (Merge20BuildPatrolOrder), so with risk-order patrol on the
 * Up/Down keys visit the TODO files in risk order, like
 * OpenNextFile/OpenPrevFile do.
 */
bool CDirView::Merge20MoveFocusToTodo(int direction)
{
	const int currentInd = GetFocusedItem();
	if (currentInd < 0)
		return false;
	const std::vector<int> order = Merge20BuildPatrolOrder(true);
	const int nextInd = Merge20FindNextInOrder(order, currentInd, direction, true);
	if (nextInd == -1 || nextInd == currentInd)
		return false;
	MoveFocus(currentInd, nextInd, GetSelectedCount());
	m_pList->EnsureVisible(nextInd, FALSE);
	return true;
}

/**
 * @brief Merge20 (Phase 4 D3): position of one file in the displayed
 * list, for the dedicated bar's file name + (n/m) display (see
 * IDirDoc::GetMerge20FilePosition). The row is matched by relative
 * path (the same identity Merge20RestoreViewPosition uses).
 * @param [in] di The file to locate.
 * @param [out] nPos 1-based ordinal among the patrol set (0 when the
 *              file is not in it, e.g. TODO-only is on but the file
 *              is already reviewed). Merge20 (best-workflow B3):
 *              the ordinal is counted in the patrol order, so with
 *              risk-order patrol on it is the risk-order position.
 * @param [out] nTotal Number of files in the patrol set.
 * @param [in] bTodoOnly False counts all displayed files, true counts
 *             only the unreviewed (TODO) files.
 * @return False when the item is not a displayed file row.
 */
bool CDirView::Merge20GetFilePosition(const DIFFITEM& di, int& nPos, int& nTotal, bool bTodoOnly) const
{
	nPos = 0;
	nTotal = 0;
	// Merge20 (best-workflow B3): the position is counted in the
	// patrol order (list order when risk-order patrol is off, risk
	// order when it is on), so the dedicated bar's (n/m) follows
	// the order the Prev/Next buttons actually visit.
	const std::vector<int> order = Merge20BuildPatrolOrder(bTodoOnly);
	nTotal = static_cast<int>(order.size());
	const String relPath = di.getItemRelativePath();
	const int n = m_pList->GetItemCount();
	for (int i = 0; i < n; ++i)
	{
		DIFFITEM* pKey = GetItemKey(i);
		if (pKey == nullptr || IsDiffItemSpecial(pKey))
			continue;
		const DIFFITEM& row = GetDiffItem(i);
		if (row.diffcode.isDirectory())
			continue;
		if (row.getItemRelativePath() == relPath)
		{
			nPos = Merge20OrdinalInOrder(order, i);
			return true;
		}
	}
	return false;
}

void CDirView::SetActivePane(int pane)
{
	if (m_nActivePane >= 0)
		GetParentFrame()->GetHeaderInterface()->SetActive(m_nActivePane, false);
	GetParentFrame()->GetHeaderInterface()->SetActive(pane, true);
	m_nActivePane = pane;
}

// Go to next diff
// If none or one item selected select found item
void CDirView::OnNextdiff()
{
	MoveToNextDiff();
}


void CDirView::OnUpdateNextdiff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(HasNextDiff());
}

// Go to prev diff
// If none or one item selected select found item
void CDirView::OnPrevdiff()
{
	MoveToPrevDiff();
}


void CDirView::OnUpdatePrevdiff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(HasPrevDiff());
}

void CDirView::OnCurdiff()
{
	const int count = m_pList->GetItemCount();
	bool found = false;
	int i = GetFirstSelectedInd();

	// No selection - no diff to go
	if (i == -1)
		i = count;

	while (i < count && !found)
	{
		UINT selected = m_pList->GetItemState(i, LVIS_SELECTED);
		UINT focused = m_pList->GetItemState(i, LVIS_FOCUSED);

		if (selected == LVIS_SELECTED && focused == LVIS_FOCUSED)
		{
			m_pList->EnsureVisible(i, FALSE);
			found = true;
		}
		i++;
	}
}

void CDirView::OnUpdateCurdiff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetFirstSelectedInd() > -1);
}

int CDirView::GetFocusedItem()
{
	return m_pList->GetNextItem(-1, LVNI_FOCUSED);
}

int CDirView::GetFirstDifferentItem()
{
	if (!m_firstDiffItem.has_value())
	{
		DirItemIterator it =
			std::find_if(Begin(), End(), MakeDirActions(&DirActions::IsItemNavigableDiff));
		m_firstDiffItem = it.m_sel;
	}
	return m_firstDiffItem.value();
}

int CDirView::GetLastDifferentItem()
{
	if (!m_lastDiffItem.has_value())
	{
		DirItemIterator it =
			std::find_if(RevBegin(), RevEnd(), MakeDirActions(&DirActions::IsItemNavigableDiff));
		m_lastDiffItem = it.m_sel;
	}
	return m_lastDiffItem.value();
}

/**
 * @brief Move focus to specified item (and selection if multiple items not selected)
 *
 * Moves the focus from item [currentInd] to item [i]
 * Additionally, if there are not multiple items selected,
 *  deselects item [currentInd] and selects item [i]
 */
void CDirView::MoveFocus(int currentInd, int i, int selCount)
{
	if (selCount <= 1)
	{
		// Not multiple items selected, so bring selection with us
		m_pList->SetItemState(currentInd, 0, LVIS_SELECTED);
		m_pList->SetItemState(currentInd, 0, LVIS_FOCUSED);
		m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
		m_pList->SetSelectionMark(i);
	}

	// Move focus to specified item
	// (this automatically defocuses old item)
	m_pList->SetItemState(i, LVIS_FOCUSED, LVIS_FOCUSED);
	m_pList->EnsureVisible(i, FALSE);
}

void CDirView::OnUpdateSave(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(FALSE);
}

CDirFrame * CDirView::GetParentFrame()
{
	// can't verify cast without introducing more coupling
	// (CDirView doesn't include DirFrame.h)
	return static_cast<CDirFrame *>(__super::GetParentFrame());
}

/**
 * @brief Save focus and scroll position to UI state.
 */
DirViewUIState* CDirView::SaveUIState()
{
	DirViewUIState *pUIState = new DirViewUIState();
	pUIState->topIndex = m_pList->GetTopIndex();
	pUIState->focusedIndex = GetFocusedItem();
	if (pUIState->focusedIndex >= 0)
	{
		const auto* pdi = GetItemKey(pUIState->focusedIndex);
		if (pdi && pdi != (const DIFFITEM *)SPECIAL_ITEM_POS)
			pUIState->focusedItemPath = paths::ConcatPath(pdi->diffFileInfo[0].path, pdi->diffFileInfo[0].filename);
	}
	return pUIState;
}

/**
 * @brief Restore focus and scroll position from saved UI state.
 * @param [in] pUIState Saved tree state containing focus and scroll information.
 */
void CDirView::RestoreUIState(const DirViewUIState* pUIState)
{
	if (pUIState == nullptr)
		return;

	int focusedIndex = -1;

	// Restore focused item
	if (!pUIState->focusedItemPath.empty())
	{
		const CDiffContext& ctxt = GetDiffContext();
		DIFFITEM *diffpos = ctxt.GetFirstDiffPosition();
		while (diffpos != nullptr)
		{
			const DIFFITEM &di = ctxt.GetNextDiffPosition(diffpos);
			String relpath = paths::ConcatPath(di.diffFileInfo[0].path, di.diffFileInfo[0].filename);
			if (relpath == pUIState->focusedItemPath)
			{
				// Find this item in the list view
				for (size_t i = 0; i < m_listViewItems.size(); ++i)
				{
					if (m_listViewItems[i].lParam == reinterpret_cast<LPARAM>(&di))
					{
						focusedIndex = (int)i;
						m_pList->SetItemState(static_cast<int>(i), LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
						m_pList->SetSelectionMark(static_cast<int>(i));
						break;
					}
				}
				break;
			}
		}
	}

	if (focusedIndex == -1 && pUIState->focusedIndex >= 0 && pUIState->focusedIndex < m_pList->GetItemCount())
	{
		focusedIndex = pUIState->focusedIndex;
		m_pList->SetItemState(static_cast<int>(focusedIndex), LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
		m_pList->SetSelectionMark(static_cast<int>(focusedIndex));
	}

	// Restore scroll position
	if (pUIState->topIndex > 0 && pUIState->topIndex < m_pList->GetItemCount())
	{
		CRect rc;
		m_pList->GetItemRect(0, &rc, LVIR_BOUNDS);
		int offsetY = pUIState->topIndex * rc.Height();
		m_pList->Scroll(CSize(0, offsetY));
	}
}

void CDirView::OnRefresh()
{
	m_pSavedUIState.reset(SaveUIState());
	m_pSavedTreeState.reset(SaveTreeState(GetDiffContext()));
	GetDocument()->Rescan();
}

BOOL CDirView::PreTranslateMessage(MSG* pMsg)
{
	// Handle special shortcuts here
	if (pMsg->message == WM_KEYDOWN)
	{
		if (!IsLabelEdit())
		{
			// Check if we got 'ESC pressed' -message
			if (pMsg->wParam == VK_ESCAPE)
			{
				if (GetDocument()->m_diffThread.GetThreadState() == CDiffThread::THREAD_COMPARING)
				{
					GetDocument()->AbortCurrentScan();
					return TRUE;
				}

				if (m_nEscCloses != 0)
				{
					AfxGetMainWnd()->PostMessage(WM_COMMAND, ID_FILE_CLOSE);
					return FALSE;
				}
			}
			// Check if we got 'DEL pressed' -message
			if (pMsg->wParam == VK_DELETE)
			{
				AfxGetMainWnd()->PostMessage(WM_COMMAND, ID_MERGE_DELETE);
				return FALSE;
			}
			// Merge20 (best-workflow B10): Ctrl+Z undoes the newest
			// judgment (review state / output flag / diff conclusion)
			// of this folder compare; Ctrl+Shift+Z or Ctrl+Y redoes
			// it. This is judgment undo, not the text-edit undo: the
			// folder list has no text editing (label editing is
			// excluded above), so the keys are free here. In a file
			// compare Ctrl+Z stays the text undo; judgment undo there
			// is menu-only (result pane context menu).
			if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
				(GetKeyState(VK_MENU) & 0x8000) == 0)
			{
				if (pMsg->wParam == 'Z' && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
				{
					if (GetDocument()->Merge20CanUndo())
					{
						OnMerge20UndoJudgment();
						return TRUE;
					}
				}
				else if ((pMsg->wParam == 'Z' && (GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
					pMsg->wParam == 'Y')
				{
					if (GetDocument()->Merge20CanRedo())
					{
						OnMerge20RedoJudgment();
						return TRUE;
					}
				}
			}
			int sel = GetFocusedItem();
			// Check if we got 'Backspace pressed' -message
			if (pMsg->wParam == VK_BACK)
			{
				if (!GetDiffContext().m_bRecursive)
				{
					OpenParentDirectory(GetDocument());
					return FALSE;
				}
				else if (m_bTreeMode && sel >= 0)
				{
					const DIFFITEM& di = GetDiffItem(sel);
					assert(di.HasParent());
					if (di.HasParent())
					{
						int i = GetItemIndex(di.GetParentLink());
						if (i >= 0)
							MoveFocus(sel, i, GetSelectedCount());
					}
				}
			}
			if (sel >= 0)
			{
				DIFFITEM& dip = this->GetDiffItem(sel);
				if (pMsg->wParam == VK_LEFT)
				{
					if (m_bTreeMode && GetDiffContext().m_bRecursive && (!(dip.customFlags & ViewCustomFlags::EXPANDED) || !dip.HasChildren()))
						PostMessage(WM_KEYDOWN, VK_BACK);
					else
						CollapseSubdir(sel);
					return TRUE;
				}
				if (pMsg->wParam == VK_SUBTRACT)
				{
					CollapseSubdir(sel);
					return TRUE;
				}
				if (pMsg->wParam == VK_RIGHT)
				{
					if (m_bTreeMode && GetDiffContext().m_bRecursive && dip.customFlags & ViewCustomFlags::EXPANDED && dip.HasChildren())
						PostMessage(WM_KEYDOWN, VK_DOWN);
					else
						ExpandSubdir(sel);
					return TRUE;
				}
				if (pMsg->wParam == VK_ADD)
				{
					ExpandSubdir(sel);
					return TRUE;
				}
				if (pMsg->wParam == VK_MULTIPLY)
				{
					ExpandSubdir(sel, true);
					return TRUE;
				}
				// Merge20 (Phase 4 D3, spec section 4(g)): with
				// unreviewed-only patrol on, bare Up/Down move the
				// focus to the previous/next TODO file (wrapping
				// around), instead of the list's one-row step. The
				// file is not opened: Up/Down move the selection in
				// the folder list, as they always did (opening is
				// Enter / double-click / the dedicated bar's Prev and
				// Next buttons). Bare keys only, so Shift+Up/Down
				// (extend the selection) keeps working; when the
				// label is being edited, Up/Down are left alone
				// (the IsLabelEdit() branch above).
				if ((pMsg->wParam == VK_UP || pMsg->wParam == VK_DOWN) &&
					(GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
					(GetKeyState(VK_MENU) & 0x8000) == 0 &&
					(GetKeyState(VK_SHIFT) & 0x8000) == 0 &&
					GetOptionsMgr()->GetBool(OPT_MERGE20_TODO_ONLY))
				{
					if (Merge20MoveFocusToTodo(pMsg->wParam == VK_DOWN ? 1 : -1))
						return TRUE;
					GetParentFrame()->SetStatus(_("No unreviewed files").c_str());
					return TRUE;
				}
				// Merge20 (S2, design section 3.2, spec section 4(b)):
				// review-state keys 1-5 and Space to cycle the five states
				// (TODO -> DONE -> HOLD -> SPECIAL -> KEEP -> TODO). The
				// output-flag linkage of spec section 4(b) is applied by
				// CDirDoc::ApplyMerge20ReviewState, the same rule the HTML
				// tool's setRev/cycRev use. Only here, in the folder
				// compare list: in the file editor the same bare keys are
				// text input (design section 1.3 #3). Bare keys only, so
				// accelerators with Ctrl/Alt/Shift keep working.
				if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
					(GetKeyState(VK_MENU) & 0x8000) == 0 &&
					(GetKeyState(VK_SHIFT) & 0x8000) == 0)
				{
					if (pMsg->wParam >= '1' && pMsg->wParam <= '5')
					{
						GetDocument()->ApplyMerge20ReviewState(dip, static_cast<ReviewState>(pMsg->wParam - '1'));
						GetListCtrl().RedrawItems(sel, sel);
						UpdateMerge20StatusCount();
						return TRUE;
					}
					if (pMsg->wParam == VK_SPACE)
					{
						ReviewState curState = GetDocument()->GetMerge20ReviewState(dip);
						const ReviewState nextState = static_cast<ReviewState>((static_cast<int>(curState) + 1) % 5);
						// Merge20 (best-workflow B14): Space cycles
						// blindly, so the step that declares a
						// high-risk file done (TODO -> DONE, which
						// also turns its output checkbox ON) needs
						// one confirmation; a cancel changes
						// nothing. Directory rows hold no file
						// judgment, so they are not guarded. Keys
						// 1-5 stay unguarded: pressing '2' is an
						// explicit choice of DONE, unlike Space.
						if (!dip.diffcode.isDirectory() &&
							!Merge20ConfirmHighRiskFileReview(this,
								dip.getItemRelativePath(),
								GetDocument()->GetDiffContext().GetMerge20FileStates().GetState(dip.getItemRelativePath()),
								curState, nextState))
						{
							return TRUE;
						}
						GetDocument()->ApplyMerge20ReviewState(dip, nextState);
						GetListCtrl().RedrawItems(sel, sel);
						UpdateMerge20StatusCount();
						return TRUE;
					}
				}
			}
		}
		else
		{
			// ESC doesn't close window when user is renaming an item.
			if (pMsg->wParam == VK_ESCAPE)
			{
				m_bUserCancelEdit = true;

				// The edit control send LVN_ENDLABELEDIT when it loses focus,
				// so we use it to cancel the rename action.
				m_pList->SetFocus();

				// Stop the ESC before it reach the main frame which might
				// cause a program termination.
				return TRUE;
			}
		}
	}
	return __super::PreTranslateMessage(pMsg);
}

void CDirView::OnUpdateRefresh(CCmdUI* pCmdUI)
{
	UINT threadState = GetDocument()->m_diffThread.GetThreadState();
	pCmdUI->Enable(threadState != CDiffThread::THREAD_COMPARING);
}

/**
 * @brief Called when compare thread asks UI update.
 * @note Currently thread asks update after compare is ready
 * or aborted.
 */
LRESULT CDirView::OnUpdateUIMessage(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	CDirDoc * pDoc = GetDocument();
	ASSERT(pDoc != nullptr);

	// Since the Collect thread deletes the DiffItems in the rescan by "Update selection",
	// the UI update process should not be executed until the Collect thread process is completed 
	// to avoid accessing the deleted DiffItem.
	if (pDoc->m_diffThread.IsMarkedRescan() && pDoc->m_diffThread.GetCollectThreadState() != CDiffThread::THREAD_COMPLETED)
	{
		ASSERT(0);
		return 0;	// return value unused
	}

	if (wParam == CDiffThread::EVENT_COMPARE_COMPLETED)
	{
		if (!m_pSavedTreeState)
		{
			if (m_nExpandSubdirs == EXPAND_DIFFERENT)
				OnViewExpandDifferentSubdirs();
			if (m_nExpandSubdirs == EXPAND_IDENTICAL)
				OnViewExpandIdenticalSubdirs();
		}
		m_pSavedTreeState.reset();

		auto& ctxt = pDoc->GetDiffContext();

		if (ctxt.m_pPropertySystem && ctxt.m_pPropertySystem->HasHashProperties())
			ctxt.CreateDuplicateValueMap();

		pDoc->CompareReady();

		if (!pDoc->GetGeneratingReport())
			Redisplay();

		// Merge20 (Phase 4 D0, design section 3.11): the folder list
		// was rebuilt (or skipped for report generation); record the
		// list-finalized point and write the measurement to the log
		// (View > Log). The last result stays in memory for D10.
		Merge20MeasureListFinalized(ctxt.m_pCompareStats->GetTotalItems());
		if (Merge20MeasureHasResult())
		{
			const std::string line = Merge20MeasureFormatLast();
			RootLogger::Info(String(line.begin(), line.end()));
		}

		if (!pDoc->GetReportFile().empty())
		{
			OnToolsGenerateReport();
			pDoc->SetReportFile(_T(""));
		}

		if (!m_pSavedUIState)
		{
			if (GetOptionsMgr()->GetBool(OPT_SCROLL_TO_FIRST))
				OnFirstdiff();
			else
				MoveFocus(0, 0, 0);
		}
		else
		{
			RestoreUIState(m_pSavedUIState.get());
			m_pSavedUIState.reset();
		}

		// Merge20 (Phase 4 D11): apply a session stashed while its
		// saved compare options were applied and the folders were
		// compared again (Merge20SessionApplyToDoc). Runs before the
		// restore offer, so a deferred restore and a new offer never
		// compete for the same compare completion.
		if (pDoc->Merge20HasPendingSession())
		{
			Merge20SessionRestoreResult pendingResult;
			int pendingSkipped = 0;
			if (Merge20SessionApplyPending(*pDoc, pendingResult, pendingSkipped))
			{
				AfxMessageBox(Merge20SessionFormatRestoreReport(pendingResult, pendingSkipped).c_str(), MB_ICONINFORMATION);
				UpdateMerge20StatusCount();
			}
			else
			{
				AfxMessageBox(_("The saved session no longer matches these folders, so it was not applied.").c_str(), MB_ICONINFORMATION);
			}
		}

		// Merge20 (S8): offer to restore a saved session for this folder
		// pair, once per document. Runs after the list was rebuilt and
		// the focus/scroll position restored, so the session's display
		// settings (filters, columns, focused file, scroll position)
		// are applied last and stick.
		Merge20SessionOfferRestore(*pDoc);

		// If compare took more than TimeToSignalCompare seconds, notify user
		m_elapsed = pDoc->GetElapsedTime();
		SetTimer(TIMER_ID_STATUSBAR_UPDATE, 150, nullptr);
		if (m_elapsed > TimeToSignalCompare * CLOCKS_PER_SEC)
			MessageBeep(IDOK);
		GetMainFrame()->StartFlashing();
		MergeLogger::LogComparisonCompleted(*ctxt.m_pCompareStats);

		if (m_bTreeMode && ctxt.m_pRenameMoveDetection && ctxt.m_pRenameMoveDetection->HasMergedMovedItems())
		{
			int ans = AfxMessageBox(
				_("Some moved items have been merged.\n"
				  "In Tree Mode, these items may appear in positions that do not reflect the actual folder structure, which can be confusing.\n"
				  "Switch to Flat Mode?").c_str(), MB_YESNO | MB_ICONWARNING | MB_DONT_ASK_AGAIN, IDS_SWITCH_FLAT_MODE);
			if (ans == IDYES)
				OnViewTreeMode();
		}

	}
	else if (wParam == CDiffThread::EVENT_COMPARE_PROGRESSED)
	{
		InvalidateRect(nullptr, FALSE);
	}
	else if (wParam == CDiffThread::EVENT_COLLECT_COMPLETED)
	{
		if (m_pSavedTreeState != nullptr)
		{
			RestoreTreeState(GetDiffContext(), m_pSavedTreeState.get());
			Redisplay();
		}
		else
		{
			if (m_nExpandSubdirs == EXPAND_ALL)
				OnViewExpandAllSubdirs();
			else
				Redisplay();
		}

		HideItems(GetDiffContext().m_vCurrentlyHiddenItems);
	}

	return 0; // return value unused
}

BOOL CDirView::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	NMHDR * hdr = reinterpret_cast<NMHDR *>(lParam);
	if (hdr->code == HDN_ENDDRAG)
		return OnHeaderEndDrag((LPNMHEADER)hdr, pResult);
	if (hdr->code == HDN_BEGINDRAG)
		return OnHeaderBeginDrag((LPNMHEADER)hdr, pResult);

	return __super::OnNotify(wParam, lParam, pResult);
}

BOOL CDirView::OnChildNotify(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	if (uMsg == WM_NOTIFY)
	{
		NMHDR *pNMHDR = (NMHDR *)lParam;
		switch (pNMHDR->code)
		{
		case LVN_GETDISPINFO:
			ReflectGetdispinfo((NMLVDISPINFO *)lParam);
			return TRUE;
		case LVN_GETINFOTIPW:
		{
			// Merge20 (S0-F1): per-side breakdown tooltip for the
			// excluded-lines cell. The list view asks for a tip for
			// every subitem, but only this cell gets one; other
			// subitems keep today's behaviour (no tip: return TRUE
			// with empty text, as before).
			NMLVGETINFOTIP *pInfo = reinterpret_cast<NMLVGETINFOTIP*>(lParam);
			const int nIdx = pInfo->iItem;
			if (nIdx >= 0 && nIdx < static_cast<int>(m_listViewItems.size()) &&
				pInfo->iSubItem >= 0 && pInfo->iSubItem < m_pColItems->GetDispColCount() &&
				m_pColItems->IsColExcluded(m_pColItems->ColPhysToLog(pInfo->iSubItem)))
			{
				DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nIdx].lParam);
				if (!IsDiffItemSpecial(key) && GetDocument()->HasDiffs())
				{
					const CDiffContext &ctxt = GetDiffContext();
					const DIFFITEM &di = ctxt.GetDiffAt(key);
					if (di.GetExcludedTotal() > 0 && pInfo->pszText != nullptr && pInfo->cchTextMax > 0)
					{
						auto sideText = [](int n) -> String
						{
							return n < 0 ? String(_T("?")) : locality::NumToLocaleStr(n);
						};
						String s;
						if (GetDocument()->m_nDirs < 3)
						{
							s = strutils::format_string2(_("Excluded lines: old %1, new %2"),
								sideText(di.nexcluded[0]), sideText(di.nexcluded[1]));
						}
						else
						{
							s = strutils::format_string3(_("Excluded lines: left %1, middle %2, right %3"),
								sideText(di.nexcluded[0]), sideText(di.nexcluded[1]), sideText(di.nexcluded[2]));
						}
						_tcsncpy_s(pInfo->pszText, pInfo->cchTextMax, s.c_str(), _TRUNCATE);
					}
				}
			}
			return TRUE;
		}
		case LVN_GETINFOTIPA:
			return TRUE;
		}
	}
	return __super::OnChildNotify(uMsg, wParam, lParam, pResult);
}

/**
 * @brief User is starting to drag a column header
 */
bool CDirView::OnHeaderBeginDrag(LPNMHEADER hdr, LRESULT* pResult)
{
	// save column widths before user reorders them
	// so we can reload them on the end drag
	const String keyname = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS;
	GetOptionsMgr()->SaveOption(keyname,
		m_pColItems->SaveColumnWidths(std::bind(&CListCtrl::GetColumnWidth, m_pList, _1)).c_str());
	return true;
}

/**
 * @brief User just finished dragging a column header
 */
bool CDirView::OnHeaderEndDrag(LPNMHEADER hdr, LRESULT* pResult)
{
	int src = hdr->iItem;
	int dest = hdr->pitem->iOrder;
	bool allowDrop = true;
	*pResult = allowDrop ? FALSE : TRUE;
	if (allowDrop && src != dest && dest != -1)
	{
		m_pColItems->MoveColumn(src, dest);
		InitiateSort();
	}
	return true;
}

/**
 * @brief Remove any windows reordering of columns
 */
void CDirView::FixReordering()
{
	LVCOLUMN lvcol;
	lvcol.mask = LVCF_ORDER;
	lvcol.fmt = 0;
	lvcol.cx = 0;
	lvcol.pszText = nullptr;
	lvcol.iSubItem = 0;
	for (int i = 0; i < m_pColItems->GetColCount(); ++i)
	{
		lvcol.iOrder = i;
		GetListCtrl().SetColumn(i, &lvcol);
	}
}

/** @brief Add columns to display, loading width & order from registry. */
void CDirView::LoadColumnHeaderItems()
{
	bool dummyflag = false;

	CHeaderCtrl * h = m_pList->GetHeaderCtrl();
	if (h->GetItemCount())
	{
		dummyflag = true;
		while (m_pList->GetHeaderCtrl()->GetItemCount() > 1)
			m_pList->DeleteColumn(1);
	}

	for (int i = 0; i < m_pColItems->GetDispColCount(); ++i)
	{
		LVCOLUMN lvc;
		lvc.mask = LVCF_FMT + LVCF_SUBITEM + LVCF_TEXT;
		lvc.fmt = LVCFMT_LEFT;
		lvc.cx = 0;
		lvc.pszText = _T("text");
		lvc.iSubItem = i;
		m_pList->InsertColumn(i, &lvc);
	}
	if (dummyflag)
		m_pList->DeleteColumn(1);

}

void CDirView::SetFont(const LOGFONT & lf)
{
	m_font.DeleteObject();
	m_font.CreateFontIndirect(&lf);
	CWnd::SetFont(&m_font);
}

/** @brief Fire off a resort of the data, to take place when things stabilize. */
void CDirView::InitiateSort()
{
	PostMessage(WM_TIMER, TIMER_ID_COLUMN_REORDER);
}

void CDirView::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_ID_COLUMN_REORDER)
	{
		// Remove the windows reordering, as we're doing it ourselves
		FixReordering();
		// Now redraw screen
		UpdateColumnNames();
		m_pColItems->LoadColumnWidths(
			GetOptionsMgr()->GetString(GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS),
			std::bind(&CListCtrl::SetColumnWidth, m_pList, _1, _2), GetDefColumnWidth());
		// Merge20 (Phase 4 D9): a header drag-and-drop reorder also
		// reloads the full-layout widths; fix the slim widths again.
		ApplyMerge20SlimColumnWidths();
		Redisplay();
	}
	else if (nIDEvent == TIMER_ID_STATUSBAR_UPDATE)
	{
		KillTimer(TIMER_ID_STATUSBAR_UPDATE);
		int items = GetSelectedCount();
		String msg;
		if (m_elapsed != 0)
		{
			msg = strutils::format(_("Elapsed time: %ld ms"), m_elapsed);
			m_elapsed = 0;
		}
		else
		{
			msg = (items == 1) ? _("1 item selected") : strutils::format_string1(_("%1 items selected"), strutils::to_str(items));
		}
		// Merge20 (S3, design section 3.2, spec section 5.1): keep the
		// "N displayed, M checked" count in the status bar at all times.
		// This timer is the only writer of status pane 0 (see
		// CDirFrame::SetStatus), so a selection change would otherwise
		// wipe the count; append it here, and refresh it directly from
		// Redisplay() and from every Merge20 state change (T-2-2).
		msg = strutils::format_string2(_("%1 | %2"), msg, GetMerge20StatusCountText());
		GetParentFrame()->SetStatus(msg.c_str());
	}
	else if (nIDEvent == TIMER_ID_DBLCLICK_OPEN)
	{
		KillTimer(TIMER_ID_DBLCLICK_OPEN);
		PostMessage(WM_COMMAND, ID_MERGE_COMPARE);
	}
	
	__super::OnTimer(nIDEvent);
}

/**
 * @brief Merge20 (S3, design section 3.2, spec section 5.1): text for the
 * "N displayed, M checked" status bar count.
 * @return e.g. "12 displayed, 5 checked, 3 unreviewed". Directories and
 * the ".." parent row have no output flag and are not counted; the
 * denominator is the number of displayed file rows, so the current
 * filter applies. D3 adds the unreviewed count (design section 3.9:
 * whether to show the patrol-target count is D3's decision): it is
 * the persistent display of "no unreviewed files" (0 unreviewed),
 * since a one-shot status message would be overwritten by the next
 * status refresh (see CDirView::OnTimer).
 */
String CDirView::GetMerge20StatusCountText()
{
	int displayed = 0;
	int checked = 0;
	int unreviewed = 0;
	CDirDoc* pDoc = GetDocument();
	const int n = m_pList->GetItemCount();
	for (int i = 0; i < n; ++i)
	{
		DIFFITEM* pKey = GetItemKey(i);
		if (pKey == nullptr || IsDiffItemSpecial(pKey))
			continue;
		const DIFFITEM& di = GetDiffItem(i);
		if (di.diffcode.isDirectory())
			continue;
		++displayed;
		if (pDoc->GetMerge20IncludeInOutput(di))
			++checked;
		if (pDoc->GetMerge20ReviewState(di) == ReviewState::TODO)
			++unreviewed;
	}
	return strutils::format_string3(_("%1 displayed, %2 checked, %3 unreviewed"),
		strutils::to_str(displayed), strutils::to_str(checked), strutils::to_str(unreviewed));
}

/**
 * @brief Merge20 (S3): write the current count text to the status bar.
 * Called from Redisplay() (the displayed set changed) and from every
 * place that changes an output flag, so the count always follows
 * filters and check operations (T-2-2).
 * UI U2: the main-frame title shows the same unreviewed count, so
 * refresh it here too. This is the single funnel for review-state
 * changes (keys 1-5/Space, batch check, per-directory operations,
 * session restore, and the file-list panel via
 * CDirDoc::Merge20UpdateDirViewStatus), so the title follows every
 * confirmation change. The frame reads the active document, so a
 * background tab's refresh can only rewrite the title from the
 * active document, never from this view.
 */
void CDirView::UpdateMerge20StatusCount()
{
	GetParentFrame()->SetStatus(GetMerge20StatusCountText().c_str());
	if (CMainFrame* pMainFrame = GetMainFrame())
		pMainFrame->OnUpdateFrameTitle(TRUE);
}

/**
 * @brief Merge20 (S3): count displayed files per review state.
 * @param [out] counts Number of displayed files per state, indexed by
 *              static_cast<int>(ReviewState). Displayed means the rows
 *              currently in the list (the filter applies); directories and
 *              the ".." parent row are skipped.
 */
void CDirView::Merge20CountDisplayedByReviewState(int counts[5])
{
	for (int i = 0; i < 5; ++i)
		counts[i] = 0;
	CDirDoc* pDoc = GetDocument();
	const int n = m_pList->GetItemCount();
	for (int i = 0; i < n; ++i)
	{
		DIFFITEM* pKey = GetItemKey(i);
		if (pKey == nullptr || IsDiffItemSpecial(pKey))
			continue;
		const DIFFITEM& di = GetDiffItem(i);
		if (di.diffcode.isDirectory())
			continue;
		++counts[static_cast<int>(pDoc->GetMerge20ReviewState(di))];
	}
}

/**
 * @brief Merge20 (S3): open the batch-check dialog and apply the choice.
 * The dialog shows the matching file count before execution (spec section
 * 4(b)) and only runs after the user confirms with OK (or one of the
 * other action buttons); Cancel changes nothing. The "selected states"
 * action covers displayed files only, as in the HTML tool's batchChkSel,
 * which iterates the filtered files.
 */
void CDirView::OnMerge20BatchCheck()
{
	int counts[5] = {};
	Merge20CountDisplayedByReviewState(counts);
	Merge20BatchCheckDlg dlg(this, counts);
	if (dlg.DoModal() != IDOK)
		return;
	CDirDoc* pDoc = GetDocument();
	switch (dlg.GetAction())
	{
	case Merge20BatchCheckDlg::Action::Selected:
	{
		const int n = m_pList->GetItemCount();
		for (int i = 0; i < n; ++i)
		{
			DIFFITEM* pKey = GetItemKey(i);
			if (pKey == nullptr || IsDiffItemSpecial(pKey))
				continue;
			const DIFFITEM& di = GetDiffItem(i);
			if (di.diffcode.isDirectory())
				continue;
			if (dlg.IsStateSelected(pDoc->GetMerge20ReviewState(di)))
			{
				// Merge20 (best-workflow B11): one audit entry per
				// file whose output flag actually changed (the batch
				// is one operation, but the log is per file, so each
				// file's old -> new stays traceable). The undo stack
				// deliberately records nothing for a batch (B10).
				const bool bOldInclude = pDoc->GetMerge20IncludeInOutput(di);
				pDoc->SetMerge20IncludeInOutput(di, true, false);
				Merge20AuditLogIncludeChange(pDoc->GetDiffContext(),
					di.getItemRelativePath(), bOldInclude, true, Merge20AuditKind::BatchCheck);
			}
		}
		break;
	}
	case Merge20BatchCheckDlg::Action::CheckAllDisplayed:
	{
		const int n = m_pList->GetItemCount();
		for (int i = 0; i < n; ++i)
		{
			DIFFITEM* pKey = GetItemKey(i);
			if (pKey == nullptr || IsDiffItemSpecial(pKey))
				continue;
			const DIFFITEM& di = GetDiffItem(i);
			if (!di.diffcode.isDirectory())
			{
				// Merge20 (best-workflow B11): see the Selected case
				// above (one BatchCheck entry per changed file).
				const bool bOldInclude = pDoc->GetMerge20IncludeInOutput(di);
				pDoc->SetMerge20IncludeInOutput(di, true, false);
				Merge20AuditLogIncludeChange(pDoc->GetDiffContext(),
					di.getItemRelativePath(), bOldInclude, true, Merge20AuditKind::BatchCheck);
			}
		}
		break;
	}
	case Merge20BatchCheckDlg::Action::ClearAll:
		// As in the HTML popup (batchChk(false) iterates all files),
		// "Clear all checks" covers the whole comparison, including
		// files hidden by the current filter or a collapsed folder.
		Merge20SetOutputForAllFiles(false);
		break;
	case Merge20BatchCheckDlg::Action::None:
		break;
	}
	m_pList->Invalidate();
	UpdateMerge20StatusCount();
}

/**
 * @brief Merge20 (S3, design section 3.2, spec section 3): set the output
 * flag of every file below one directory row, recursively. The DIFFITEM
 * tree is walked (not the visible list), so files in collapsed
 * subfolders are included too. Directory rows themselves have no flag.
 */
void CDirView::Merge20SetOutputForDirectory(const DIFFITEM& dir, bool include)
{
	CDirDoc* pDoc = GetDocument();
	for (DIFFITEM* pChild = dir.GetFirstChild(); pChild != nullptr; pChild = pChild->GetFwdSiblingLink())
	{
		if (pChild->diffcode.isDirectory())
			Merge20SetOutputForDirectory(*pChild, include);
		else
		{
			// Merge20 (best-workflow B11): one BatchCheck entry per
			// changed file (as in the batch-check dialog).
			const bool bOldInclude = pDoc->GetMerge20IncludeInOutput(*pChild);
			pDoc->SetMerge20IncludeInOutput(*pChild, include, false);
			Merge20AuditLogIncludeChange(pDoc->GetDiffContext(),
				pChild->getItemRelativePath(), bOldInclude, include, Merge20AuditKind::BatchCheck);
		}
	}
}

void CDirView::OnMerge20DirCheckAll()
{
	for (DirItemIterator it = SelBegin(); it != SelEnd(); ++it)
	{
		const DIFFITEM& di = *it;
		if (di.diffcode.isDirectory())
			Merge20SetOutputForDirectory(di, true);
	}
	m_pList->Invalidate();
	UpdateMerge20StatusCount();
}

void CDirView::OnMerge20DirUncheckAll()
{
	for (DirItemIterator it = SelBegin(); it != SelEnd(); ++it)
	{
		const DIFFITEM& di = *it;
		if (di.diffcode.isDirectory())
			Merge20SetOutputForDirectory(di, false);
	}
	m_pList->Invalidate();
	UpdateMerge20StatusCount();
}

// Merge20 (best-workflow B10): failure message for a judgment undo /
// redo step (same msgids as CMergeDoc, so the translation is shared).
static String Merge20UndoFailureMessage(Merge20UndoApplyResult result, bool bUndo)
{
	switch (result)
	{
	case Merge20UndoApplyResult::NothingToUndo:
		return _("There is no judgment to undo.");
	case Merge20UndoApplyResult::NothingToRedo:
		return _("There is no judgment to redo.");
	case Merge20UndoApplyResult::Stale:
		return bUndo ?
			_("That judgment cannot be undone because its state has changed since then.") :
			_("That judgment cannot be redone because its state has changed since then.");
	case Merge20UndoApplyResult::TargetNotOpen:
		return bUndo ?
			_("That diff conclusion cannot be undone because its file comparison is not open. Open the file first, then try again.") :
			_("That diff conclusion cannot be redone because its file comparison is not open. Open the file first, then try again.");
	case Merge20UndoApplyResult::Applied:
	default:
		return String();
	}
}

void CDirView::OnMerge20UndoJudgment()
{
	CDirDoc* pDoc = GetDocument();
	const Merge20UndoApplyResult result = pDoc->Merge20Undo();
	if (result == Merge20UndoApplyResult::Applied)
		GetParentFrame()->SetStatus(_("Undid the last judgment.").c_str());
	else
		AfxMessageBox(Merge20UndoFailureMessage(result, true).c_str(), MB_OK | MB_ICONINFORMATION);
}

void CDirView::OnUpdateMerge20UndoJudgment(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetDocument()->Merge20CanUndo());
}

void CDirView::OnMerge20RedoJudgment()
{
	CDirDoc* pDoc = GetDocument();
	const Merge20UndoApplyResult result = pDoc->Merge20Redo();
	if (result == Merge20UndoApplyResult::Applied)
		GetParentFrame()->SetStatus(_("Redid the last judgment.").c_str());
	else
		AfxMessageBox(Merge20UndoFailureMessage(result, false).c_str(), MB_OK | MB_ICONINFORMATION);
}

void CDirView::OnUpdateMerge20RedoJudgment(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetDocument()->Merge20CanRedo());
}

/**
 * @brief Merge20 (S6, design section 3.5, spec section 4(f)): export
 * the diff list as a CSV file. The range dialog shows both ranges
 * with their file counts; the save dialog defaults to
 * DiffList_YYYY-MM-DD.csv, as in the HTML tool. Cancel in either
 * dialog exports nothing. The result (row count and path) is
 * reported in a message box.
 */
void CDirView::OnMerge20CsvExport()
{
	CDiffContext& ctxt = GetDiffContext();
	int selectedOnly = 0, allDiffs = 0;
	Merge20CsvCountRanges(ctxt, selectedOnly, allDiffs);
	if (selectedOnly == 0 && allDiffs == 0)
	{
		AfxMessageBox(_("There are no files with differences to export.").c_str(), MB_ICONINFORMATION);
		return;
	}
	Merge20CsvExportDlg dlg(this, selectedOnly, allDiffs);
	if (dlg.DoModal() != IDOK)
		return;
	// Default file name, as in the HTML tool (DiffList_YYYY-MM-DD.csv).
	const String defaultName = _T("DiffList_") + String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d")).GetString()) + _T(".csv");
	String filePath;
	if (!SelectFile(GetSafeHwnd(), filePath, false, defaultName.c_str(), _T(""),
		_T("CSV Files (*.csv)|*.csv|All Files (*.*)|*.*||"), _T("csv")))
		return;
	String errStr;
	int exported = 0;
	const Merge20CsvExportRange range = (dlg.GetRange() == Merge20CsvExportDlg::Range::SelectedOnly)
		? Merge20CsvExportRange::SelectedOnly : Merge20CsvExportRange::AllDiffs;
	if (!Merge20CsvExportToFile(ctxt, range, filePath, errStr, exported))
	{
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
		return;
	}
	AfxMessageBox(strutils::format_string2(_("%1 files exported to %2"), strutils::to_str(exported), filePath).c_str(), MB_ICONINFORMATION);
}

// Case-insensitive "path is under root" test (B8): the root has no
// trailing slash, so the character after the root must be a
// separator -- C:\Foo2 is not under C:\Foo.
static bool Merge20PathIsUnderRoot(const String& path, const String& root)
{
	String cleanRoot = root;
	while (!cleanRoot.empty() && (cleanRoot.back() == _T('\\') || cleanRoot.back() == _T('/')))
		cleanRoot.pop_back();
	if (cleanRoot.empty() || path.length() <= cleanRoot.length())
		return false;
	if (strutils::compare_nocase(path.substr(0, cleanRoot.length()), cleanRoot) != 0)
		return false;
	const wchar_t next = path[cleanRoot.length()];
	return next == _T('\\') || next == _T('/');
}

/**
 * @brief Merge20 (B8, gap analysis section 4.2 B8): export the
 * verification report on its own (without saving a merge result).
 * Verifies the open file comparisons that belong to this folder
 * compare (pane 0 under the left root, the NEW pane under the
 * right root) and writes one combined report to a file the user
 * picks (VerifyReport_YYYY-MM-DD.csv by default). Files that are
 * not open are not verified: opening a merge document in the
 * background just to verify it would need a headless compare,
 * which does not exist yet, so the report lists the open files
 * and says so (a file whose merge result was never built appears
 * as "not verified"). The verification runs synchronously on the
 * open documents only, so it stays fast.
 */
void CDirView::OnMerge20VerifyReport()
{
	CDiffContext& ctxt = GetDiffContext();
	const String leftRoot = ctxt.GetLeftPath();
	const String rightRoot = ctxt.GetRightPath();
	CMainFrame* pMainFrame = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
	std::vector<Merge20VerifyFileResult> verifyFiles;
	if (pMainFrame != nullptr)
	{
		int fileIndex = 0;
		for (IMergeDoc* pDoc : pMainFrame->GetAllMergeDocuments())
		{
			CMergeDoc* pMergeDoc = dynamic_cast<CMergeDoc*>(pDoc);
			if (pMergeDoc == nullptr || pMergeDoc->GetFileCount() < 2)
				continue;
			const int newPane = pMergeDoc->Merge20GetNewPane();
			if (newPane < 0 || newPane >= pMergeDoc->GetFileCount())
				continue;
			if (!Merge20PathIsUnderRoot(pMergeDoc->GetPath(0), leftRoot))
				continue;
			if (!Merge20PathIsUnderRoot(pMergeDoc->GetPath(newPane), rightRoot))
				continue;
			verifyFiles.push_back(Merge20VerifyCheckMergeDocFile(*pMergeDoc, fileIndex++));
		}
	}
	if (verifyFiles.empty())
	{
		AfxMessageBox(_("There are no open file comparisons in this folder compare to verify. Open a file by double-clicking it first.").c_str(), MB_ICONINFORMATION);
		return;
	}
	// Default file name, as in the S6 export (DiffList_YYYY-MM-DD.csv).
	const String defaultName = Merge20VerifyReportDefaultFileName(CTime::GetCurrentTime());
	String filePath;
	if (!SelectFile(GetSafeHwnd(), filePath, false, defaultName.c_str(), _T(""),
		_T("CSV Files (*.csv)|*.csv|All Files (*.*)|*.*||"), _T("csv")))
		return;
	const String verifiedAt =
		String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d %H:%M:%S")).GetString());
	const Merge20VerifyReport report = Merge20VerifyReportFromFileResults(verifyFiles, verifiedAt);
	String errStr;
	if (!Merge20VerifyReportWriteFile(report, Merge20VerifyReportMakeLabels(), filePath, errStr))
	{
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
		return;
	}
	AfxMessageBox(strutils::format_string2(_("%1 files exported to %2"), strutils::to_str(static_cast<int>(verifyFiles.size())), filePath).c_str(), MB_ICONINFORMATION);
}

/**
 * @brief Merge20 (B9, gap analysis section 4.2 B9): end-of-session
 * summary dialog. One line per file (file name, review state,
 * action category, risk, output on/off, one-line summary) under
 * the H-DASH counts, with a warning while unreviewed files
 * remain. [Show unreviewed only] enters the D3 TODO-only patrol
 * and opens the next unreviewed file; [Confirm and export]
 * exports the output targets with the S6 CSV (S7 does not exist
 * yet); [Back] changes nothing. Cancel in the save dialog
 * exports nothing.
 */
void CDirView::OnMerge20Summary()
{
	CDiffContext& ctxt = GetDiffContext();
	// The whole comparison is walked (as in the S6 export), so files
	// hidden by the current filter or inside a collapsed folder are
	// included too; directories are not rows.
	std::vector<Merge20SummaryFile> summaryFiles;
	std::vector<Merge20SummaryRow> rows;
	for (DIFFITEM* pPos = ctxt.GetFirstDiffPosition(); pPos != nullptr;)
	{
		const DIFFITEM& di = ctxt.GetNextDiffPosition(pPos);
		if (di.diffcode.isDirectory())
			continue;
		const Merge20FileState& state = ctxt.GetMerge20FileStates().GetState(di.getItemRelativePath());
		Merge20SummaryFile file;
		file.hasDiff = Merge20CsvHasDiff(di);
		file.isModified = di.diffcode.isResultDiff();
		file.isOldOnly = di.diffcode.isSideFirstOnly();
		file.isNewOnly = di.diffcode.isSideSecondOnly() || di.diffcode.isSideThirdOnly();
		file.review = state.review;
		file.includeInOutput = ctxt.GetMerge20IncludeInOutput(di);
		summaryFiles.push_back(file);
		Merge20SummaryRow row;
		row.fileName = Merge20CsvGetFileNameText(&ctxt, di);
		row.review = Merge20CsvGetReviewText(&ctxt, di);
		row.action = Merge20CsvGetActionText(&ctxt, di);
		row.risk = Merge20CsvGetRiskText(&ctxt, di);
		row.output = Merge20CsvGetOutputText(&ctxt, di);
		row.summary = Merge20CsvGetSummaryText(&ctxt, di);
		rows.push_back(row);
	}
	const Merge20SummaryCounts counts = Merge20SummaryComputeCounts(summaryFiles);
	if (counts.total == 0)
	{
		AfxMessageBox(_("There are no files to review.").c_str(), MB_ICONINFORMATION);
		return;
	}
	Merge20SummaryDlg dlg(this, rows, counts);
	if (dlg.DoModal() != IDOK)
		return; // Back (or close): change nothing.
	switch (dlg.GetResult())
	{
	case Merge20SummaryDlg::Result::TodoOnly:
		// Enter the D3 TODO-only patrol (the dedicated bar toggle
		// reads the same option) and open the next unreviewed
		// file. OpenNextFile reports "No unreviewed files" itself
		// when there is none, and stays when the current file is
		// the only unreviewed one.
		GetOptionsMgr()->Set(OPT_MERGE20_TODO_ONLY, true);
		OpenNextFile();
		break;
	case Merge20SummaryDlg::Result::Confirm:
	{
		// S7 does not exist yet: the output is the S6 CSV of the
		// output targets (the "selected only" range), so the
		// exported rows are exactly the dialog's output targets.
		const String defaultName = _T("Summary_") + String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d")).GetString()) + _T(".csv");
		String filePath;
		if (!SelectFile(GetSafeHwnd(), filePath, false, defaultName.c_str(), _T(""),
			_T("CSV Files (*.csv)|*.csv|All Files (*.*)|*.*||"), _T("csv")))
			return;
		String errStr;
		int exported = 0;
		if (!Merge20CsvExportToFile(ctxt, Merge20CsvExportRange::SelectedOnly, filePath, errStr, exported))
		{
			AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
			return;
		}
		AfxMessageBox(strutils::format_string2(_("%1 files exported to %2"), strutils::to_str(exported), filePath).c_str(), MB_ICONINFORMATION);
		break;
	}
	case Merge20SummaryDlg::Result::Back:
		break;
	}
}

/**
 * @brief Merge20 (best-workflow B11, gap analysis section 4.2 B11):
 * show the folder pair's audit log (chronological list). The dialog
 * only displays the loaded entries; [Export as CSV...] runs the
 * same CSV export as the context-menu item. Opening the viewer
 * writes nothing to the log (viewing is not a judgment).
 */
void CDirView::OnMerge20AuditView()
{
	CDirDoc* pDoc = GetDocument();
	if (pDoc == nullptr || !pDoc->HasDiffs())
		return;
	std::vector<Merge20AuditEntry> entries;
	int skippedLines = 0;
	String filePath;
	if (!Merge20AuditLoadForDoc(*pDoc, entries, skippedLines, filePath))
	{
		AfxMessageBox(_("There are no audit log entries yet. Make a judgment (review state, output check, diff conclusion) and it will be recorded here.").c_str(), MB_ICONINFORMATION);
		return;
	}
	const std::vector<Merge20AuditCsvRow> rows = Merge20AuditBuildDisplayRows(entries);
	Merge20AuditDlg dlg(this, rows, skippedLines);
	if (dlg.DoModal() != IDOK)
		return; // Close: change nothing.
	if (dlg.GetResult() == Merge20AuditDlg::Result::Export)
		OnMerge20AuditCsv();
}

/**
 * @brief Merge20 (best-workflow B11, gap analysis section 4.2 B11):
 * export the folder pair's audit log as CSV (BOM-prefixed UTF-8,
 * so it opens directly in Excel). The default name is
 * AuditLog_YYYY-MM-DD.csv, as in the S6 export
 * (DiffList_YYYY-MM-DD.csv).
 */
void CDirView::OnMerge20AuditCsv()
{
	CDirDoc* pDoc = GetDocument();
	if (pDoc == nullptr || !pDoc->HasDiffs())
		return;
	std::vector<Merge20AuditEntry> entries;
	int skippedLines = 0;
	String filePath;
	if (!Merge20AuditLoadForDoc(*pDoc, entries, skippedLines, filePath) || entries.empty())
	{
		AfxMessageBox(_("There are no audit log entries yet. Make a judgment (review state, output check, diff conclusion) and it will be recorded here.").c_str(), MB_ICONINFORMATION);
		return;
	}
	const String defaultName = _T("AuditLog_") + String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d")).GetString()) + _T(".csv");
	String outPath;
	if (!SelectFile(GetSafeHwnd(), outPath, false, defaultName.c_str(), _T(""),
		_T("CSV Files (*.csv)|*.csv|All Files (*.*)|*.*||"), _T("csv")))
		return;
	String errStr;
	int exported = 0;
	if (!Merge20AuditExportCsv(entries, outPath, errStr, exported))
	{
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
		return;
	}
	AfxMessageBox(strutils::format_string2(_("%1 entries exported to %2"), strutils::to_str(exported), outPath).c_str(), MB_ICONINFORMATION);
}

void CDirView::OnUpdateMerge20Audit(CCmdUI* pCmdUI)
{
	CDirDoc* pDoc = GetDocument();
	pCmdUI->Enable(pDoc != nullptr && pDoc->HasDiffs());
}

/**
 * @brief Merge20 (Phase 3 L4, design section 3.3): open the learning
 * dialog (statistics, rules, sources, ON/OFF toggle). The dialog only
 * displays and manages learning data; it never applies a candidate to
 * a merge result (spec section 4(c)).
 */
void CDirView::OnMerge20Learn()
{
	Merge20LearnDlg dlg(this);
	dlg.DoModal();
}

/**
 * @brief Merge20 (Phase 4 D7, design sections 3.7 and 5.2/D7, spec
 * section 3 (4/4)): open the Grep cross-file search dialog
 * (modeless, owned by this view). Ctrl+Shift+F and the context
 * menu both land here. The dialog rebuilds its scan file list
 * and history on every show.
 */
void CDirView::OnMerge20Grep()
{
	m_wndGrep.ShowGrep(this);
}

/**
 * @brief Merge20 (Phase 4 D10, design sections 3.11 and 5.2/D10,
 * spec sections 3 (2/4) and 7): open the bulk classification
 * dialog (modeless, owned by this view). The context menu lands
 * here. The dialog rebuilds its file list on every show.
 */
void CDirView::OnMerge20BulkClassify()
{
	m_wndBulkClassify.ShowBulkClassify(this);
}

/**
 * @brief Merge20 (Phase 4 D8, design sections 4.4 and 5.2/D8, spec
 * section 3 (4/4)): open the master management dialog (modal).
 * When the dialog closes with Save and the exclude rules changed,
 * re-compare all files (the HTML tool's "save and re-judge").
 */
void CDirView::OnMerge20Master()
{
	CMerge20MasterDlg dlg;
	if (dlg.DoModal() != IDOK)
		return;
	if (dlg.ExcludeChanged())
		GetDocument()->Rescan();
}

/**
 * @brief Merge20 (Wave 5 F2, design sections 3.2 and 7.1): open
 * the FORM compare window (view-only) for this folder pair. The
 * left/right folder paths are handed over as-is; no confirmation
 * dialog (the menu choice itself states the intent, design
 * section 1.4). A 3-way folder compare hands over its outer two
 * folders; the middle folder is ignored.
 */
void CDirView::OnMerge20FormCompare()
{
	CDirDoc* pDoc = GetDocument();
	if (pDoc == nullptr)
		return;
	const CDiffContext& ctxt = pDoc->GetDiffContext();
	Merge20FormCompareOpenFolders(ctxt.GetLeftPath(), ctxt.GetRightPath());
}

/**
 * @brief Merge20 (Phase 4 D9): slim-list toggle (folder-list context
 * menu). Switches between the full column layout and the slim preset.
 */
void CDirView::OnMerge20Slim()
{
	SetMerge20Slim(!m_bMerge20Slim);
}

/**
 * @brief Merge20 (Phase 4 D11): review-state filter toggle
 * (folder-list context menu, "Show review states" submenu). One of
 * the five states is toggled per command (ID_MERGE20_SHOW_REVIEW_*);
 * the toggle is written back to the global options and into the
 * view's filter settings, and the list is rebuilt (same pattern as
 * the show_* filters in Merge20SetDisplayFilter).
 */
void CDirView::OnMerge20ShowReview(UINT nID)
{
	const int index = static_cast<int>(nID - ID_MERGE20_SHOW_REVIEW_TODO);
	if (index < 0 || index >= 5)
		return;
	const bool bShow = !GetOptionsMgr()->GetBool(Merge20ShowReviewKey(index));
	GetOptionsMgr()->SaveOption(Merge20ShowReviewKey(index), bShow);
	m_dirfilter.show_review[index] = bShow;
	Redisplay();
	// Merge20 (H-FILTER): the deck bar shows the same five review
	// toggles; refresh its checkboxes so the submenu and the deck
	// never disagree.
	Merge20SyncFilterDeckBar();
}

/**
 * @brief Merge20 (H-FILTER): re-read the deck option keys into the
 * view's filter settings and rebuild the list (same pattern as
 * OnMerge20ShowReview, for all three deck groups at once). Called
 * by the deck bar after a checkbox, preset or show-all change.
 */
void CDirView::Merge20RefreshFilterDeck()
{
	for (int i = 0; i < 4; ++i)
		m_dirfilter.show_status[i] = GetOptionsMgr()->GetBool(Merge20ShowStatusKey(i));
	for (int i = 0; i < 7; ++i)
		m_dirfilter.show_action[i] = GetOptionsMgr()->GetBool(Merge20ShowActionKey(i));
	for (int i = 0; i < 5; ++i)
		m_dirfilter.show_review[i] = GetOptionsMgr()->GetBool(Merge20ShowReviewKey(i));
	Redisplay();
}

/**
 * @brief Merge20 (H-FILTER): push the current deck option state
 * into the deck bar's checkboxes. The bar guards itself while its
 * window does not exist yet (frame creation order).
 */
void CDirView::Merge20SyncFilterDeckBar()
{
	CDirFrame* pFrame = GetParentFrame();
	if (pFrame == nullptr)
		return;
	if (CMerge20FilterDeckBar* pBar = pFrame->GetFilterDeckBar())
		pBar->SyncCheckboxes();
}

/**
 * @brief Merge20 (Phase 4 D9, design sections 3.10 and 5.2/D9, spec
 * sections 3 (4/4) and 5.1): apply or leave the slim list.
 *
 * Slim shows only the file name, the output check and the review
 * state (HTML `tgSlim()`: check box + file name + review tag).
 * Everything else is hidden: the action category, the risk, the
 * summary (which carries the old/changed/new counts, the HTML
 * "old/changed/new" column), and all stock columns. The review
 * column is 126px wide and its tag wraps (see OnCustomDraw).
 *
 * The full layout is remembered on entry (orders + widths) and
 * restored on exit, and the slim orders/widths are never written
 * to the options: the saved layout is always the full one (see the
 * guards in OnDestroy, Merge20GetDisplaySettings and
 * Merge20ApplyColumnSettings). Re-entering slim while already slim
 * re-applies the preset, which is how a session restore refreshes
 * the remembered full layout without leaving slim mode.
 *
 * Redraw cost (T-4-8): the toggle is ReloadColumns (header rebuild
 * over ~40 columns) + Redisplay (O(N) item bookkeeping for the
 * virtual list, no per-item text fetch: text is supplied on paint
 * for visible rows only) + one repaint of the visible rows. The
 * custom-draw wrap adds one DrawText per review cell per paint.
 * Nothing in the path is proportional to the file contents, so a
 * 1000-file list toggles in well under a second; the exact time on
 * a real machine is a D9 real-machine check item.
 */
void CDirView::SetMerge20Slim(bool bSlim)
{
	if (m_pColItems == nullptr || m_pList == nullptr)
		return;
	if (bSlim)
	{
		const bool bFirstEntry = !m_bMerge20Slim;
		if (bFirstEntry)
		{
			m_sMerge20FullColumnOrders = m_pColItems->SaveColumnOrders();
			m_sMerge20FullColumnWidths = m_pColItems->SaveColumnWidths(std::bind(&CListCtrl::GetColumnWidth, m_pList, _1));
		}
		m_bMerge20Slim = true;
		GetOptionsMgr()->SaveOption(OPT_MERGE20_SLIM, true);
		const std::string sOrders = Merge20Slim::ColumnOrders();
		m_pColItems->LoadColumnOrders(String(sOrders.begin(), sOrders.end()));
		ReloadColumns();
		if (bFirstEntry)
			BuildMerge20SlimImageList();
		Redisplay();
	}
	else
	{
		if (!m_bMerge20Slim)
			return;
		m_bMerge20Slim = false;
		GetOptionsMgr()->SaveOption(OPT_MERGE20_SLIM, false);
		if (!m_sMerge20FullColumnOrders.empty())
			m_pColItems->LoadColumnOrders(m_sMerge20FullColumnOrders);
		else
			m_pColItems->ResetColumnOrdering();
		ReloadColumns();
		RestoreMerge20SlimImageList();
		Redisplay();
	}
}

/**
 * @brief Merge20 (Phase 4 D9): fix the slim column widths after any
 * width reload. ReloadColumns() (and the column-reorder timer) load
 * widths from the saved full layout, which assigns the Path/Status
 * widths to the slim columns. The physical indexes are looked up on
 * every call, so a header drag-and-drop reorder can never leave a
 * stale index behind.
 */
void CDirView::ApplyMerge20SlimColumnWidths()
{
	if (!m_bMerge20Slim || m_pList == nullptr)
		return;
	const int physOutput = GetMerge20SlimColumnPhys(_T("Soutput"));
	if (physOutput >= 0)
		m_pList->SetColumnWidth(physOutput, Merge20Slim::kOutputColumnWidth);
	const int physReview = GetMerge20SlimColumnPhys(_T("Sreview"));
	if (physReview >= 0)
		m_pList->SetColumnWidth(physReview, Merge20Slim::kReviewColumnWidth);
}

/**
 * @brief Merge20 (Phase 4 D9): logical column index of a regName,
 * or -1 when the column does not exist in this table (2-way/3-way).
 */
int CDirView::GetMerge20SlimColumnLog(const tchar_t* regName) const
{
	if (m_pColItems == nullptr)
		return -1;
	for (int log = 0; log < m_pColItems->GetColCount(); ++log)
	{
		const DirColInfo* pInfo = m_pColItems->GetDirColInfo(log);
		if (pInfo != nullptr && pInfo->regName != nullptr && String(pInfo->regName) == regName)
			return log;
	}
	return -1;
}

/**
 * @brief Merge20 (Phase 4 D9): physical (display) column index of a
 * regName, or -1 when the column is hidden or does not exist.
 */
int CDirView::GetMerge20SlimColumnPhys(const tchar_t* regName) const
{
	const int log = GetMerge20SlimColumnLog(regName);
	return log < 0 ? -1 : m_pColItems->ColLogToPhys(log);
}

/**
 * @brief Merge20 (Phase 4 D9): build the taller small-icon list
 * that gives slim rows two text lines, so the wrapped review tag
 * has room. The icons are the same resource icons as
 * `m_imageList` (see DiffImageListUtils), copied in order, so the
 * image indexes stay valid; the list is all-or-nothing (any
 * extraction failure keeps the original list). When the icons are
 * already at least two lines tall, no swap is needed. Restored by
 * RestoreMerge20SlimImageList on leaving slim mode.
 */
void CDirView::BuildMerge20SlimImageList()
{
	if (m_merge20SlimImageList.GetSafeHandle() != nullptr)
		return;
	IMAGEINFO info;
	if (m_imageList.GetSafeHandle() == nullptr || !m_imageList.GetImageInfo(0, &info))
		return;
	const int cx = info.rcImage.right - info.rcImage.left;
	const int cy = info.rcImage.bottom - info.rcImage.top;
	CClientDC dc(m_pList);
	CFont* pFont = m_pList->GetFont();
	CFont* pOldFont = dc.SelectObject(pFont);
	TEXTMETRIC tm;
	dc.GetTextMetrics(&tm);
	dc.SelectObject(pOldFont);
	const int nNeed = 2 * tm.tmHeight + 4;
	if (nNeed <= cy)
		return;
	CImageList slim;
	if (!slim.Create(cx, nNeed, ILC_COLOR32 | ILC_MASK, m_imageList.GetImageCount(), 1))
		return;
	std::vector<HICON> icons;
	bool bOk = true;
	for (int i = 0; i < m_imageList.GetImageCount(); ++i)
	{
		HICON hIcon = m_imageList.ExtractIcon(i);
		if (hIcon == nullptr)
		{
			bOk = false;
			break;
		}
		icons.push_back(hIcon);
	}
	if (!bOk)
	{
		for (HICON hIcon : icons)
			::DestroyIcon(hIcon);
		return;
	}
	for (HICON hIcon : icons)
	{
		slim.Add(hIcon);
		::DestroyIcon(hIcon);
	}
	m_merge20SlimImageList.Attach(slim.Detach());
	m_pList->SetImageList(&m_merge20SlimImageList, LVSIL_SMALL);
}

/**
 * @brief Merge20 (Phase 4 D9): restore the original small-icon list
 * and drop the taller slim list.
 */
void CDirView::RestoreMerge20SlimImageList()
{
	if (m_merge20SlimImageList.GetSafeHandle() == nullptr)
		return;
	m_pList->SetImageList(&m_imageList, LVSIL_SMALL);
	m_merge20SlimImageList.DeleteImageList();
}

/**
 * @brief Merge20 (Phase 4 D7): all compared files for the Grep
 * scan, in folder-list order (depth-first tree walk, the same
 * order the list shows). Directories are skipped; one-sided
 * files contribute only their existing side (empty path for the
 * missing side); hidden and filtered-out files are included
 * (design: OLD/NEW "all files", not just the displayed rows).
 */
std::vector<Merge20GrepFile> CDirView::Merge20BuildGrepFiles()
{
	CDiffContext& ctxt = GetDiffContext();
	std::vector<Merge20GrepFile> files;
	for (DIFFITEM* pos = ctxt.GetFirstDiffPosition(); pos != nullptr;)
	{
		DIFFITEM& di = ctxt.GetNextDiffRefPosition(pos);
		if (di.diffcode.isDirectory())
			continue;
		Merge20GrepFile file;
		file.relPath = di.getItemRelativePath();
		PathContext paths;
		ctxt.GetComparePaths(di, paths);
		const int nDirs = ctxt.GetCompareDirs();
		file.paths.resize(static_cast<size_t>(nDirs));
		for (int i = 0; i < nDirs; ++i)
			file.paths[static_cast<size_t>(i)] = di.diffcode.exists(i) ? paths[i] : String();
		files.push_back(std::move(file));
	}
	return files;
}

/**
 * @brief Merge20 (Phase 4 D10): unclassified files for the bulk
 * classification run, in folder-list order (depth-first tree
 * walk, the same order the list shows). Directories are skipped;
 * identical files are skipped (S4 stage 1, like the Rescan
 * hook, classifies only files with differences); already-
 * classified files are skipped (a re-run only covers what is
 * still unclassified). One-sided files are included: their line
 * count is all the classification needs. Both sides' expected
 * full paths are kept (the missing side's would-be path, not an
 * empty string), so CDirDoc::SetMerge20Classification can find
 * the item again (its FindItemFromPaths lookup needs the
 * expected path, unlike the Grep scan which only reads files
 * that exist).
 */
std::vector<Merge20BulkClassifyFile> CDirView::Merge20BuildBulkClassifyFiles()
{
	CDiffContext& ctxt = GetDiffContext();
	const Merge20FileStates& states = ctxt.GetMerge20FileStates();
	std::vector<Merge20BulkClassifyFile> files;
	for (DIFFITEM* pos = ctxt.GetFirstDiffPosition(); pos != nullptr;)
	{
		DIFFITEM& di = ctxt.GetNextDiffRefPosition(pos);
		if (di.diffcode.isDirectory())
			continue;
		if (!di.diffcode.isResultDiff() && !di.diffcode.isSideFirstOnly() && !di.diffcode.isSideSecondOnly())
			continue;
		const String relPath = di.getItemRelativePath();
		if (states.GetState(relPath).classified)
			continue;
		Merge20BulkClassifyFile file;
		file.relPath = relPath;
		PathContext paths;
		ctxt.GetComparePaths(di, paths);
		const int nDirs = ctxt.GetCompareDirs();
		file.paths.resize(static_cast<size_t>(nDirs));
		for (int i = 0; i < nDirs; ++i)
		{
			file.paths[static_cast<size_t>(i)] = paths[i];
			file.exists[static_cast<size_t>(i)] = di.diffcode.exists(i);
		}
		files.push_back(std::move(file));
	}
	return files;
}

/**
 * @brief Merge20 (Phase 4 D7, design section 4.3): jump from a
 * Grep hit to its file and diff. Selects the file row and opens
 * the file compare (the usual OpenSelection path), then moves
 * the compare to the hit line (GotoLine with the real line) and
 * selects its diff when the line is inside one (LineToDiff on
 * the apparent line; -1 for a hit in a common area or an
 * identical file means "line only, no diff"). A file hidden by
 * the current filter has no list row to select, so the jump is
 * refused with an explanation instead of opening the wrong file.
 */
void CDirView::Merge20OpenFileAtLine(const Merge20GrepHit& hit)
{
	CDiffContext& ctxt = GetDiffContext();
	DIFFITEM* pItem = nullptr;
	for (DIFFITEM* pos = ctxt.GetFirstDiffPosition(); pos != nullptr;)
	{
		DIFFITEM& di = ctxt.GetNextDiffRefPosition(pos);
		if (!di.diffcode.isDirectory() && di.getItemRelativePath() == hit.relPath)
		{
			pItem = &di;
			break;
		}
	}
	if (pItem == nullptr)
		return;
	const int nIdx = GetItemIndex(pItem);
	if (nIdx == -1)
	{
		AfxMessageBox(_("Cannot open the file because it is hidden by the current filter.").c_str(), MB_ICONINFORMATION);
		return;
	}
	// OpenSelection opens the selected rows: clear any other
	// selection first, or several files would open at once.
	const int nCount = m_pList->GetItemCount();
	for (int i = 0; i < nCount; ++i)
		m_pList->SetItemState(i, 0, LVIS_SELECTED | LVIS_FOCUSED);
	m_pList->SetItemState(nIdx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	m_pList->EnsureVisible(nIdx, FALSE);
	OpenSelection();

	// The compare just opened is the active MDI child; verify it
	// really is this file before moving inside it.
	CMergeEditFrame* pFrame = dynamic_cast<CMergeEditFrame*>(GetMainFrame()->GetActiveFrame());
	if (pFrame == nullptr)
		return;
	CMergeDoc* pDoc = pFrame->GetMergeDoc();
	if (pDoc == nullptr || pDoc->GetFileCount() <= hit.pane)
		return;
	PathContext paths;
	ctxt.GetComparePaths(*pItem, paths);
	for (int i = 0; i < pDoc->GetFileCount(); ++i)
	{
		if (!paths[i].empty() && pDoc->GetPath(i) != paths[i])
			return;
	}
	CMergeEditView* pView = pDoc->GetView(0, hit.pane);
	if (pView == nullptr)
		pView = pDoc->GetActiveMergeView();
	if (pView == nullptr)
		return;
	// Clamp the hit line to the buffer: the file may have changed
	// on disk between the scan and the click. LineToDiff works on
	// apparent (ghost-included) lines; GotoLine takes the real
	// file line when bRealLine is true.
	const int nLines = pDoc->m_ptBuf[hit.pane]->GetLineCount();
	const int nRealLine = (std::min)(hit.line, (std::max)(0, nLines - 1));
	const int nApparent = pDoc->m_ptBuf[hit.pane]->ComputeApparentLine(nRealLine);
	const int nDiff = pDoc->m_diffList.LineToDiff(nApparent);
	if (nDiff >= 0)
		pView->SelectDiff(nDiff);
	pView->SetActivePane();
	pView->GotoLine(static_cast<UINT>(nRealLine), true, hit.pane);
}

/**
 * @brief Merge20 (Panel P3, design section 12.7): open one folder item
 * in a file compare (P-R7). The panel's row click goes through
 * CDirDoc::Merge20MoveToFile, which closes the current file compare
 * first; this is the selection step, mirroring Merge20OpenFileAtLine
 * (Grep jump): select the item's row (clearing any other selection,
 * so only this file opens) and open the selection. GetItemIndex
 * returns 0 on a miss (upstream behaviour, not -1), so the row is
 * verified through GetItemKey: an item hidden by the current folder
 * filter cannot be opened, and the same message as the Grep jump
 * explains why instead of opening row 0.
 */
void CDirView::Merge20OpenFile(const DIFFITEM& di)
{
	// GetItemIndex takes a non-const pointer but only compares it
	// against the list keys (see its implementation); the item is
	// not modified.
	const int nIdx = GetItemIndex(const_cast<DIFFITEM*>(&di));
	if (nIdx < 0 || nIdx >= m_pList->GetItemCount() || GetItemKey(nIdx) != &di)
	{
		AfxMessageBox(_("Cannot open the file because it is hidden by the current filter.").c_str(), MB_ICONINFORMATION);
		return;
	}
	// OpenSelection opens the selected rows: clear any other
	// selection first, or several files would open at once.
	const int nCount = m_pList->GetItemCount();
	for (int i = 0; i < nCount; ++i)
		m_pList->SetItemState(i, 0, LVIS_SELECTED | LVIS_FOCUSED);
	m_pList->SetItemState(nIdx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	m_pList->EnsureVisible(nIdx, FALSE);
	OpenSelection();
}

/**
 * @brief Merge20 (S8, design section 3.6, spec section 6.1): save the
 * current session (review states, output checks, memos, merge
 * judgments, display settings) to the session file of this folder
 * pair. The result (file path) is reported in a message box; a failed
 * save is reported explicitly (spec section 7).
 */
void CDirView::OnMerge20SessionSave()
{
	CDirDoc* pDoc = GetDocument();
	String filePath, errStr;
	if (!Merge20SessionSaveDoc(*pDoc, filePath, errStr))
	{
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
		return;
	}
	AfxMessageBox(strutils::format_string1(_("Session saved to:\n%1"), filePath).c_str(), MB_ICONINFORMATION);
}

/**
 * @brief Merge20 (S8): restore a session from a file chosen in a file
 * dialog (defaulting to this folder pair's session file). The file
 * must belong to this folder pair; the restore report names the
 * restored, reset and skipped files.
 */
void CDirView::OnMerge20SessionRestore()
{
	CDirDoc* pDoc = GetDocument();
	String defaultPath;
	Merge20SessionExistsForDoc(*pDoc, defaultPath);
	String filePath;
	if (!SelectFile(GetSafeHwnd(), filePath, true, defaultPath.c_str(), _T(""),
		_T("JSON Files (*.json)|*.json|All Files (*.*)|*.*||"), _T("json")))
		return;
	Merge20Session session;
	String errStr;
	int skippedFiles = 0;
	if (!Merge20SessionLoadForDoc(*pDoc, filePath, session, errStr, skippedFiles))
	{
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
		return;
	}
	Merge20SessionRestoreResult result = Merge20SessionApplyToDoc(*pDoc, session, skippedFiles);
	if (result.deferred)
	{
		// Merge20 (Phase 4 D11): the saved compare options were
		// applied and a rescan started; the final report comes after
		// the pending apply (OnUpdateUIMessage).
		AfxMessageBox(Merge20SessionFormatDeferredMessage(result).c_str(), MB_ICONINFORMATION);
		return;
	}
	AfxMessageBox(Merge20SessionFormatRestoreReport(result, skippedFiles).c_str(), MB_ICONINFORMATION);
	UpdateMerge20StatusCount();
}

/**
 * @brief Merge20 (S8): enable the session commands once a folder
 * comparison has results.
 */
void CDirView::OnUpdateMerge20Session(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetDocument()->HasDiffs() ? TRUE : FALSE);
}

/**
 * @brief Merge20 (S8, design section 3.5): edit the audit memo of the
 * single selected file. The memo column repaints after the edit; the
 * memo is saved with the session and included in the CSV export.
 */
void CDirView::OnMerge20MemoEdit()
{
	const int sel = GetSingleSelectedItem();
	if (sel == -1)
		return;
	const DIFFITEM& di = GetDiffItem(sel);
	if (di.diffcode.isDirectory())
		return;
	CDirDoc* pDoc = GetDocument();
	const String relPath = di.getItemRelativePath();
	Merge20MemoDlg dlg(this, relPath, pDoc->GetMerge20Memo(di));
	if (dlg.DoModal() != IDOK)
		return;
	pDoc->SetMerge20Memo(di, dlg.GetMemo());
	UpdateDiffItemStatus(static_cast<UINT>(sel));
}

/**
 * @brief Merge20 (S8): read the display settings the session save
 * records: the show_* filters and tree mode, the column order/width
 * strings, the focused file and the top visible row.
 */
void CDirView::Merge20GetDisplaySettings(Merge20SessionDisplay& display)
{
	display.showSkipped = m_dirfilter.show_skipped;
	display.showUniqueLeft = m_dirfilter.show_unique_left;
	display.showUniqueMiddle = m_dirfilter.show_unique_middle;
	display.showUniqueRight = m_dirfilter.show_unique_right;
	display.showBinaries = m_dirfilter.show_binaries;
	display.showIdentical = m_dirfilter.show_identical;
	display.showDifferent = m_dirfilter.show_different;
	display.showDifferentLeftOnly = m_dirfilter.show_different_left_only;
	display.showDifferentMiddleOnly = m_dirfilter.show_different_middle_only;
	display.showDifferentRightOnly = m_dirfilter.show_different_right_only;
	display.showMissingLeftOnly = m_dirfilter.show_missing_left_only;
	display.showMissingMiddleOnly = m_dirfilter.show_missing_middle_only;
	display.showMissingRightOnly = m_dirfilter.show_missing_right_only;
	display.showEmptyFolders = m_dirfilter.show_empty_folders;
	display.treeMode = m_bTreeMode;
	display.reviewFilter[0] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_REVIEW_TODO);
	display.reviewFilter[1] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_REVIEW_DONE);
	display.reviewFilter[2] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_REVIEW_HOLD);
	display.reviewFilter[3] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_REVIEW_SPECIAL);
	display.reviewFilter[4] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_REVIEW_KEEP);
	// Merge20 (H-FILTER): the deck existence and action filters
	// travel with the session, like the review filter above.
	display.statusFilter[0] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_STATUS_MODIFIED);
	display.statusFilter[1] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_STATUS_OLD_ONLY);
	display.statusFilter[2] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_STATUS_NEW_ONLY);
	display.statusFilter[3] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_STATUS_IDENTICAL);
	display.actionFilter[0] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_ACTION_MERGE_CLEAN_OLD);
	display.actionFilter[1] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_ACTION_REVIEW_MIXED);
	display.actionFilter[2] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_ACTION_REVIEW_MOD_ONLY);
	display.actionFilter[3] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_ACTION_NO_MERGE_NEW_ADD);
	display.actionFilter[4] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_ACTION_COPY_OLD_FILE);
	display.actionFilter[5] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_ACTION_SKIP_NEW_FILE);
	display.actionFilter[6] = GetOptionsMgr()->GetBool(OPT_MERGE20_SHOW_ACTION_NO_ACTION_IDENTICAL);
	// Merge20 (Phase 4 D11, design section 4.1(b)): the slim flag
	// itself is saved, so a restore brings back the slim list (the
	// remembered full layout travels in columnOrders/columnWidths).
	display.slim = m_bMerge20Slim;
	// Merge20 (Phase 4 D9): while slim is on, the session stores
	// the remembered full layout, not the slim preset (D11 saves the
	// slim flag itself; the preset is rebuilt from it on restore).
	display.columnOrders = (m_bMerge20Slim && !m_sMerge20FullColumnOrders.empty()) ?
		m_sMerge20FullColumnOrders : m_pColItems->SaveColumnOrders();
	display.columnWidths = (m_bMerge20Slim && !m_sMerge20FullColumnWidths.empty()) ?
		m_sMerge20FullColumnWidths : m_pColItems->SaveColumnWidths(std::bind(&CListCtrl::GetColumnWidth, m_pList, _1));
	const int focused = GetFocusedItem();
	if (focused >= 0)
	{
		const DIFFITEM* pKey = GetItemKey(focused);
		if (pKey != nullptr && !IsDiffItemSpecial(pKey))
			display.focusedPath = pKey->getItemRelativePath();
	}
	display.topIndex = m_pList->GetTopIndex();
}

/**
 * @brief Merge20 (S8, D11): apply the saved show_* filters, the
 * review-state filter and tree mode (same effect as toggling the
 * filter bar by hand: the values are written back to the global
 * options), then rebuild the list. The slim flag is restored before
 * the rebuild, so the slim preset applies to this list; the saved
 * column layout (the remembered full layout) is applied afterwards
 * by Merge20ApplyColumnSettings.
 */
void CDirView::Merge20SetDisplayFilter(const Merge20SessionDisplay& display)
{
	m_dirfilter.show_skipped = display.showSkipped;
	GetOptionsMgr()->SaveOption(OPT_SHOW_SKIPPED, display.showSkipped);
	m_dirfilter.show_unique_left = display.showUniqueLeft;
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_LEFT, display.showUniqueLeft);
	m_dirfilter.show_unique_middle = display.showUniqueMiddle;
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_MIDDLE, display.showUniqueMiddle);
	m_dirfilter.show_unique_right = display.showUniqueRight;
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_RIGHT, display.showUniqueRight);
	m_dirfilter.show_binaries = display.showBinaries;
	GetOptionsMgr()->SaveOption(OPT_SHOW_BINARIES, display.showBinaries);
	m_dirfilter.show_identical = display.showIdentical;
	GetOptionsMgr()->SaveOption(OPT_SHOW_IDENTICAL, display.showIdentical);
	m_dirfilter.show_different = display.showDifferent;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT, display.showDifferent);
	m_dirfilter.show_different_left_only = display.showDifferentLeftOnly;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_LEFT_ONLY, display.showDifferentLeftOnly);
	m_dirfilter.show_different_middle_only = display.showDifferentMiddleOnly;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_MIDDLE_ONLY, display.showDifferentMiddleOnly);
	m_dirfilter.show_different_right_only = display.showDifferentRightOnly;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_RIGHT_ONLY, display.showDifferentRightOnly);
	m_dirfilter.show_missing_left_only = display.showMissingLeftOnly;
	GetOptionsMgr()->SaveOption(OPT_SHOW_MISSING_LEFT_ONLY, display.showMissingLeftOnly);
	m_dirfilter.show_missing_middle_only = display.showMissingMiddleOnly;
	GetOptionsMgr()->SaveOption(OPT_SHOW_MISSING_MIDDLE_ONLY, display.showMissingMiddleOnly);
	m_dirfilter.show_missing_right_only = display.showMissingRightOnly;
	GetOptionsMgr()->SaveOption(OPT_SHOW_MISSING_RIGHT_ONLY, display.showMissingRightOnly);
	m_dirfilter.show_empty_folders = display.showEmptyFolders;
	GetOptionsMgr()->SaveOption(OPT_SHOW_EMPTY_FOLDERS, display.showEmptyFolders);
	m_bTreeMode = display.treeMode;
	m_dirfilter.tree_mode = display.treeMode;
	GetOptionsMgr()->SaveOption(OPT_TREE_MODE, display.treeMode);
	// Merge20 (Phase 4 D11): apply the saved review-state filter
	// (D9 saved only the keys). Same pattern as the show_* filters:
	// written back to the global options and into the view's filter
	// settings, so IsShowable (DirActions.cpp) picks it up.
	m_dirfilter.show_review[0] = display.reviewFilter[0];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_REVIEW_TODO, display.reviewFilter[0]);
	m_dirfilter.show_review[1] = display.reviewFilter[1];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_REVIEW_DONE, display.reviewFilter[1]);
	m_dirfilter.show_review[2] = display.reviewFilter[2];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_REVIEW_HOLD, display.reviewFilter[2]);
	m_dirfilter.show_review[3] = display.reviewFilter[3];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_REVIEW_SPECIAL, display.reviewFilter[3]);
	m_dirfilter.show_review[4] = display.reviewFilter[4];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_REVIEW_KEEP, display.reviewFilter[4]);
	// Merge20 (H-FILTER): apply the saved deck existence and
	// action filters. Same pattern as the show_* filters: written
	// back to the global options and into the view's filter
	// settings, so IsShowable (DirActions.cpp) picks them up.
	m_dirfilter.show_status[0] = display.statusFilter[0];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_STATUS_MODIFIED, display.statusFilter[0]);
	m_dirfilter.show_status[1] = display.statusFilter[1];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_STATUS_OLD_ONLY, display.statusFilter[1]);
	m_dirfilter.show_status[2] = display.statusFilter[2];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_STATUS_NEW_ONLY, display.statusFilter[2]);
	m_dirfilter.show_status[3] = display.statusFilter[3];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_STATUS_IDENTICAL, display.statusFilter[3]);
	m_dirfilter.show_action[0] = display.actionFilter[0];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_ACTION_MERGE_CLEAN_OLD, display.actionFilter[0]);
	m_dirfilter.show_action[1] = display.actionFilter[1];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_ACTION_REVIEW_MIXED, display.actionFilter[1]);
	m_dirfilter.show_action[2] = display.actionFilter[2];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_ACTION_REVIEW_MOD_ONLY, display.actionFilter[2]);
	m_dirfilter.show_action[3] = display.actionFilter[3];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_ACTION_NO_MERGE_NEW_ADD, display.actionFilter[3]);
	m_dirfilter.show_action[4] = display.actionFilter[4];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_ACTION_COPY_OLD_FILE, display.actionFilter[4]);
	m_dirfilter.show_action[5] = display.actionFilter[5];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_ACTION_SKIP_NEW_FILE, display.actionFilter[5]);
	m_dirfilter.show_action[6] = display.actionFilter[6];
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SHOW_ACTION_NO_ACTION_IDENTICAL, display.actionFilter[6]);
	// Merge20 (Phase 4 D11): the slim flag is restored before the
	// list is rebuilt. SetMerge20Slim rebuilds the list itself
	// (except when leaving slim off while already off); Redisplay is
	// only needed in that case, so the list is built exactly once.
	if (display.slim || m_bMerge20Slim)
		SetMerge20Slim(display.slim);
	else
		Redisplay();
	// Merge20 (H-FILTER): the restored deck state must reach the
	// deck bar's checkboxes too, or the bar would show the
	// pre-restore state while the list already follows the session.
	Merge20SyncFilterDeckBar();
}

/**
 * @brief Merge20 (Phase 4 D11, design section 4.1(b)): put the saved
 * Grep search string back at the head of the search history (and
 * into the open Grep dialog, if any). An empty string is a no-op.
 */
void CDirView::Merge20RestoreGrepText(const String& text)
{
	if (text.empty())
		return;
	m_wndGrep.RestoreSearchText(text);
}

/**
 * @brief Merge20 (S8): apply the saved column order and widths
 * (written back to the global options, as OnDestroy does, then
 * reloaded like at startup).
 */
void CDirView::Merge20ApplyColumnSettings(const String& orders, const String& widths)
{
	const String ordersKey = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_ORDERS : OPT_DIRVIEW3_COLUMN_ORDERS;
	const String widthsKey = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS;
	GetOptionsMgr()->SaveOption(ordersKey, orders);
	GetOptionsMgr()->SaveOption(widthsKey, widths);
	// Merge20 (Phase 4 D9): the restored orders/widths are the full
	// layout. While slim is on they become the remembered full layout
	// and the slim preset is re-applied on top, so the view stays slim.
	if (m_bMerge20Slim)
	{
		m_sMerge20FullColumnOrders = orders;
		m_sMerge20FullColumnWidths = widths;
		SetMerge20Slim(true);
		return;
	}
	m_pColItems->LoadColumnOrders(orders);
	ReloadColumns();
}

/**
 * @brief Merge20 (S8): restore the focused file and scroll position
 * (same steps as RestoreUIState, but matched by relative path). Must
 * run after the list was rebuilt, or the position is lost.
 */
void CDirView::Merge20RestoreViewPosition(const String& focusedPath, int topIndex)
{
	if (!focusedPath.empty())
	{
		const CDiffContext& ctxt = GetDiffContext();
		for (DIFFITEM* pPos = ctxt.GetFirstDiffPosition(); pPos != nullptr;)
		{
			const DIFFITEM& di = ctxt.GetNextDiffPosition(pPos);
			if (di.getItemRelativePath() != focusedPath)
				continue;
			for (size_t i = 0; i < m_listViewItems.size(); ++i)
			{
				if (m_listViewItems[i].lParam == reinterpret_cast<LPARAM>(&di))
				{
					m_pList->SetItemState(static_cast<int>(i), LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
					m_pList->SetSelectionMark(static_cast<int>(i));
					break;
				}
			}
			break;
		}
	}
	if (topIndex > 0 && topIndex < m_pList->GetItemCount())
	{
		CRect rc;
		m_pList->GetItemRect(0, &rc, LVIR_BOUNDS);
		int offsetY = topIndex * rc.Height();
		m_pList->Scroll(CSize(0, offsetY));
	}
}

void CDirView::OnUpdateMerge20DirCheck(CCmdUI* pCmdUI)
{
	bool bDirSelected = false;
	for (DirItemIterator it = SelBegin(); it != SelEnd(); ++it)
	{
		const DIFFITEM& di = *it;
		if (di.diffcode.isDirectory())
		{
			bDirSelected = true;
			break;
		}
	}
	pCmdUI->Enable(bDirSelected);
}

/**
 * @brief Merge20 (S3): set the output flag of every file in the whole
 * comparison (depth-first walk of the DIFFITEM tree, so filtered-out and
 * collapsed files are included too).
 */
void CDirView::Merge20SetOutputForAllFiles(bool include)
{
	if (m_pList->GetItemCount() == 0)
		return;
	CDiffContext& ctxt = GetDiffContext();
	CDirDoc* pDoc = GetDocument();
	for (DIFFITEM* pPos = ctxt.GetFirstDiffPosition(); pPos != nullptr;)
	{
		const DIFFITEM& di = ctxt.GetNextDiffPosition(pPos);
		if (!di.diffcode.isDirectory())
		{
			// Merge20 (best-workflow B11): one BatchCheck entry per
			// changed file (as in the batch-check dialog).
			const bool bOldInclude = pDoc->GetMerge20IncludeInOutput(di);
			pDoc->SetMerge20IncludeInOutput(di, include, false);
			Merge20AuditLogIncludeChange(ctxt,
				di.getItemRelativePath(), bOldInclude, include, Merge20AuditKind::BatchCheck);
		}
	}
}

/**
 * @brief Called when the user changes the system settings.
 */
void CDirView::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	if (WinMergeDarkMode::IsImmersiveColorSet(lpszSection))
	{
		HWND hList = GetSafeHwnd();
		if (hList != nullptr)
		{
			DarkMode::setListViewCtrlSubclass(hList);
			DarkMode::setDarkTooltips(hList, static_cast<int>(DarkMode::ToolTipsType::listview));
			DarkMode::setDarkThemeExperimentalEx(hList, L"Explorer");
		}
	}
	__super::OnSettingChange(uFlags, lpszSection);
}

/**
 * @brief Change left-side readonly-status
 */
template<SIDE_TYPE stype>
void CDirView::OnReadOnly()
{
	const int index = SideToIndex(GetDiffContext(), stype);
	bool bReadOnly = GetDocument()->GetReadOnly(index);
	GetDocument()->SetReadOnly(index, !bReadOnly);
}

/**
 * @brief Update left-readonly menu item
 */
void CDirView::OnUpdateReadOnly(CCmdUI* pCmdUI, SIDE_TYPE stype)
{
	const int index = SideToIndex(GetDiffContext(), stype);
	bool bReadOnly = GetDocument()->GetReadOnly(index);
	if (stype != SIDE_MIDDLE)
	{
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck(bReadOnly);
	}
	else
	{
		pCmdUI->Enable(GetDocument()->m_nDirs > 2);
		pCmdUI->SetCheck(bReadOnly && GetDocument()->m_nDirs > 2);
	}
}

/**
 * @brief Update left-side readonly statusbar item
 */
void CDirView::OnUpdateStatusLeftRO(CCmdUI* pCmdUI)
{
	bool bROLeft = GetDocument()->GetReadOnly(0);
	pCmdUI->Enable(bROLeft);
}

/**
 * @brief Update middle readonly statusbar item
 */
void CDirView::OnUpdateStatusMiddleRO(CCmdUI* pCmdUI)
{
	bool bROMiddle = GetDocument()->GetReadOnly(1);
	pCmdUI->Enable(bROMiddle && GetDocument()->m_nDirs > 2);
}

/**
 * @brief Update right-side readonly statusbar item
 */
void CDirView::OnUpdateStatusRightRO(CCmdUI* pCmdUI)
{
	bool bRORight = GetDocument()->GetReadOnly(GetDocument()->m_nDirs - 1);
	pCmdUI->Enable(bRORight);
}

/**
 * @brief Open dialog to customize dirview columns
 */
void CDirView::OnCustomizeColumns()
{
	// Located in DirViewColHandler.cpp
	OnEditColumns();
	const String keyname = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_ORDERS : OPT_DIRVIEW3_COLUMN_ORDERS;
	GetOptionsMgr()->SaveOption(keyname, m_pColItems->SaveColumnOrders());
}

void CDirView::OnOpenWithUnpacker()
{
	int sel = -1;
	sel = m_pList->GetNextItem(sel, LVNI_SELECTED);
	if (sel != -1)
	{
		PackingInfo* infoUnpacker = nullptr;
		PrediffingInfo* infoPrediffer = nullptr;
		CDiffContext& ctxt = GetDiffContext();
		String filteredFilenames = ctxt.GetFilteredFilenames(GetDiffItem(sel));
		ctxt.FetchPluginInfos(filteredFilenames, &infoUnpacker, &infoPrediffer);
		// let the user choose a handler
		CSelectPluginDlg dlg(infoUnpacker->GetPluginPipeline(), filteredFilenames,
			CSelectPluginDlg::PluginType::Unpacker, false, this);
		if (dlg.DoModal() == IDOK)
		{
			PackingInfo infoUnpackerNew(dlg.GetPluginPipeline());
			OpenSelection(SELECTIONTYPE_NORMAL, &infoUnpackerNew, false);
		}
	}

}

void CDirView::OnUpdateCtxtOpenWithUnpacker(CCmdUI* pCmdUI)
{
	if (!GetOptionsMgr()->GetBool(OPT_PLUGINS_ENABLED))
	{
		pCmdUI->Enable(FALSE);
		return;
	}

	// we need one selected file, existing on both side
	if (m_pList->GetSelectedCount() != 1)
		pCmdUI->Enable(FALSE);
	else
	{
		int sel = -1;
		sel = m_pList->GetNextItem(sel, LVNI_SELECTED);
		const DIFFITEM& di = GetDiffItem(sel);
		if (di.diffcode.isDirectory())
		{
			pCmdUI->Enable(FALSE);
			return;
		}
		pCmdUI->Enable(IsItemDeletableOnBoth(GetDiffContext(), di));
	}
}

/**
 * @brief Fill string list with current dirview column registry key names
 */
std::vector<String> CDirView::GetCurrentColRegKeys()
{
	std::vector<String> colKeys;
	int nphyscols = GetListCtrl().GetHeaderCtrl()->GetItemCount();
	for (int col = 0; col < nphyscols; ++col)
	{
		int logcol = m_pColItems->ColPhysToLog(col);
		colKeys.push_back(m_pColItems->GetColRegValueNameBase(logcol));
	}
	return colKeys;
}

struct FileCmpReportMsg { String sReportPath; int nIndex = 0; HANDLE hEvent = nullptr; };

struct FileCmpReport: public IFileCmpReport
{
	explicit FileCmpReport(CDirView *pDirView) : m_pDirView(pDirView) {}
	~FileCmpReport() override {}
	bool operator()(REPORT_TYPE nReportType, IListCtrl *pList, int nIndex, const String &sDestDir, String &sLinkPath) override
	{
		const CDiffContext& ctxt = m_pDirView->GetDiffContext();
		const DIFFITEM &di = m_pDirView->GetDiffItem(nIndex);
		
		String sLinkFullPath = paths::ConcatPath(ctxt.GetLeftPath(), di.diffFileInfo[0].GetFile());

		if (di.diffcode.isDirectory() || !IsItemNavigableDiff(ctxt, di) || IsArchiveFile(sLinkFullPath))
		{
			sLinkPath.clear();
			return false;
		}

		sLinkPath = strutils::format(_T("%d_"), nIndex) + di.diffFileInfo[0].GetFile();

		strutils::replace(sLinkPath, _T("\\"), _T("_"));
		sLinkPath += _T(".html");
		auto pMsg = std::make_unique<FileCmpReportMsg>();
		pMsg->sReportPath = paths::ConcatPath(sDestDir, sLinkPath);
		pMsg->nIndex = nIndex;
		pMsg->hEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!pMsg->hEvent)
			return false;
		m_pDirView->PostMessage(MSG_GENERATE_FLIE_COMPARE_REPORT, reinterpret_cast<WPARAM>(pMsg.get()), 0);
		::WaitForSingleObject(pMsg->hEvent, INFINITE);
		CloseHandle(pMsg->hEvent);

		return true;
	}
private:
	FileCmpReport();
	CDirView *m_pDirView;
};

LRESULT CDirView::OnGenerateFileCmpReport(WPARAM wParam, LPARAM lParam)
{
	FileCmpReportMsg* pMsg = reinterpret_cast<FileCmpReportMsg*>(wParam);

	MoveFocus(GetFirstSelectedInd(), pMsg->nIndex, GetSelectedCount());

	OpenSelection();

	if (IMergeDoc * pMergeDoc = GetMainFrame()->GetActiveIMergeDoc())
	{
		CMainFrame::GenerateDocumentReport({ pMergeDoc }, pMsg->sReportPath);
		pMergeDoc->CloseNow();
	}
	MSG msg;
	while (::PeekMessage(&msg, nullptr, NULL, NULL, PM_NOREMOVE))
		if (!AfxGetApp()->PumpMessage())
			break;
	GetMainFrame()->OnUpdateFrameTitle(FALSE);

	SetEvent(pMsg->hEvent);

	return 0;
}

/**
 * @brief Generate report from dir compare results.
 */
void CDirView::OnToolsGenerateReport()
{
	CDirDoc *pDoc = GetDocument();
	DirCmpReportDlg dlg;
	dlg.LoadSettings();
	dlg.m_sReportFile = pDoc->GetReportFile();
	if (dlg.m_sReportFile.empty() && dlg.DoModal() != IDOK)
		return;

	pDoc->SetGeneratingReport(true);
	const CDiffContext& ctxt = GetDiffContext();

	PathContext paths = ctxt.GetNormalizedPaths();

	// If inside archive, convert paths
	if (pDoc->IsArchiveFolders())
	{
		for (int i = 0; i < paths.GetSize(); i++)
			pDoc->ApplyDisplayRoot(i, paths[i]);
	}

	DirCmpReport *pReport = new DirCmpReport(GetCurrentColRegKeys());
	pReport->SetRootPaths(paths);
	pReport->SetColumns(m_pColItems->GetDispColCount());
	pReport->SetFileCmpReport(new FileCmpReport(this));
	pReport->SetList(new IListCtrlImpl(m_pList->m_hWnd, m_listViewItems));
	pReport->SetReportType(dlg.m_nReportType);
	pReport->SetReportFile(dlg.m_sReportFile);
	pReport->SetCopyToClipboard(dlg.m_bCopyToClipboard);
	pReport->SetIncludeFileCmpReport(dlg.m_bIncludeFileCmpReport);
	pDoc->SetReport(pReport);
	pDoc->Rescan();
}

/**
 * @brief Generate patch from files selected.
 *
 * Creates a patch from selected files in active directory compare, or
 * active file compare. Files in file compare must be saved before
 * creating a patch.
 */
void CDirView::OnToolsGeneratePatch()
{
	CPatchTool patcher;
	const CDiffContext& ctxt = GetDiffContext();

	// Prompt user about diff-only filtering
	auto patchOnlyDiffItems = PromptPatchOnlyDiffItems();
	if (!patchOnlyDiffItems.has_value())
		return;

	// Collect patch items using the new CreatePatchItems function
	auto patchItems = CreatePatchItems(ctxt, SelBegin(), SelEnd(), patchOnlyDiffItems.value());

	// Add items to patcher (binary files will be skipped during patch creation)
	for (const auto& item : patchItems)
	{
		patcher.AddFiles(item.leftFile, item.leftpatch, item.rightFile, item.rightpatch, 
				(!item.leftpatch.empty() ? item.leftpatch : item.rightpatch), true, item.diffStatus);
	}

	patcher.CreatePatch();
}

/**
 * @brief Add special items for non-recursive compare
 * to directory view.
 *
 * Currently only special item is ".." for browsing to
 * parent folders.
 * @return number of items added to view
 */
int CDirView::AddSpecialItems()
{
	CDirDoc *pDoc = GetDocument();
	int retVal = 0;
	bool bEnable = true;
	PathContext pathsParent;
	switch (CheckAllowUpwardDirectory(GetDiffContext(), pDoc->m_pTempPathContext, pathsParent))
	{
	case AllowUpwardDirectory::No:
		bEnable = false;
		[[fallthrough]];
	default:
		AddParentFolderItem(bEnable);
		retVal = 1;
		[[fallthrough]];
	case AllowUpwardDirectory::Never:
		break;
	}
	return retVal;
}

/**
 * @brief Add "Parent folder" ("..") item to directory view
 */
void CDirView::AddParentFolderItem(bool bEnable)
{
	AddNewItem(0, (DIFFITEM *)SPECIAL_ITEM_POS, bEnable ? DIFFIMG_DIRUP : DIFFIMG_DIRUP_DISABLE, 0);
}

void CDirView::OnCtxtDirZip(int flag)
{
	if (!HasZipSupport())
	{
		I18n::MessageBox(IDS_NO_ZIP_SUPPORT, MB_ICONINFORMATION);
		return;
	}

	const bool bDiffsOnly = (flag & DirViewZipFlags::DiffsOnly) != 0;
	const bool bBoth = (flag & DirViewZipFlags::Original) && (flag & DirViewZipFlags::Altered);
	const int nDirs = GetDocument()->m_nDirs;
	const CDiffContext & ctxt = GetDiffContext();
	DirItemWithIndexIterator begin(m_pIList.get(), -1, true);
	DirItemWithIndexIterator end;

	std::vector<CompressibleItem> items;
	if (bBoth)
	{
		String dirnames[3];
		if (nDirs < 3)
		{
			dirnames[0] = _T("original\\");
			dirnames[1] = _T("altered\\");
		}
		else
		{
			dirnames[0] = _T("1\\");
			dirnames[1] = _T("2\\");
			dirnames[2] = _T("3\\");
		}
		for (int i = 0; i < nDirs; ++i)
		{
			auto zipItems = CreateZipItems(ctxt, begin, end, i, bDiffsOnly);
			for (auto& ci : zipItems)
			{
				ci.name.insert(0, dirnames[i].c_str());
				items.push_back(std::move(ci));
			}
		}
	}
	else
	{
		int index;
		if (nDirs < 3)
			index = (flag & DirViewZipFlags::Right) ? 1 : 0;
		else
			index = (flag & DirViewZipFlags::Right) ? 2 : (flag & DirViewZipFlags::Middle) ? 1 : 0;
		items = CreateZipItems(ctxt, begin, end, index, bDiffsOnly);
	}

	CompressibleItemEnumerator(std::move(items)).CompressArchive();
}

void CDirView::ShowShellContextMenu(UINT id)
{
	std::vector<SIDE_TYPE> stypes;
	CShellContextMenu *pContextMenu = nullptr;
	switch (id)
	{
	case ID_DIR_SHELL_CONTEXT_MENU_LEFT:
		if (m_pShellContextMenuLeft == nullptr)
			m_pShellContextMenuLeft.reset(new CShellContextMenu(LeftCmdFirst, LeftCmdLast));
		pContextMenu = m_pShellContextMenuLeft.get();
		stypes = { SIDE_LEFT };
		break;
	case ID_DIR_SHELL_CONTEXT_MENU_MIDDLE:
		if (m_pShellContextMenuMiddle == nullptr)
			m_pShellContextMenuMiddle.reset(new CShellContextMenu(MiddleCmdFirst, MiddleCmdLast));
		pContextMenu = m_pShellContextMenuMiddle.get();
		stypes = { SIDE_MIDDLE };
		break;
	case ID_DIR_SHELL_CONTEXT_MENU_RIGHT:
		if (m_pShellContextMenuRight == nullptr)
			m_pShellContextMenuRight.reset(new CShellContextMenu(RightCmdFirst, RightCmdLast));
		pContextMenu = m_pShellContextMenuRight.get();
		stypes = { SIDE_RIGHT };
		break;
	default:
		if (m_pShellContextMenuBoth == nullptr)
			m_pShellContextMenuBoth.reset(new CShellContextMenu(BothCmdFirst, BothCmdLast));
		pContextMenu = m_pShellContextMenuBoth.get();
		if (GetDocument()->m_nDirs < 3)
			stypes = { SIDE_LEFT, SIDE_RIGHT };
		else
			stypes = { SIDE_LEFT, SIDE_MIDDLE, SIDE_RIGHT };
		break;
	}

	if (!pContextMenu)
		return;

	pContextMenu->Initialize();
	for (const auto stype : stypes)
	{
		ApplyFolderNameAndFileName(SelBegin(), SelEnd(), stype, GetDiffContext(),
			[&](const String& path, const String& filename) { pContextMenu->AddItem(path, filename); });
	}
	if (pContextMenu->RequeryShellContextMenu())
	{
		CPoint point;
		GetCursorPos(&point);
		HWND hWnd = GetSafeHwnd();
		CFrameWnd* pFrame = GetTopLevelFrame();
		ASSERT(pFrame != nullptr);
		BOOL bAutoMenuEnableOld = pFrame->m_bAutoMenuEnable;
		pFrame->m_bAutoMenuEnable = FALSE;
		BOOL nCmd = TrackPopupMenu(pContextMenu->GetHMENU(), TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, 0, hWnd, nullptr);
		if (nCmd)
			pContextMenu->InvokeCommand(nCmd, hWnd);
		pContextMenu->ReleaseShellContextMenu();
		pFrame->m_bAutoMenuEnable = bAutoMenuEnableOld;
	}
}

/**
 * @brief Select all visible items in dir compare
 */
void CDirView::OnSelectAll()
{
	// While the user is renaming an item, select all the edited text.
	CEdit *pEdit = m_pList->GetEditControl();
	if (pEdit != nullptr)
	{
		pEdit->SetSel(pEdit->GetWindowTextLength());
	}
	else
	{
		int selCount = m_pList->GetItemCount();

		for (int i = 0; i < selCount; i++)
		{
			// Don't select special items (SPECIAL_ITEM_POS)
			DIFFITEM *diffpos = GetItemKey(i);
			if (!IsDiffItemSpecial(diffpos))
				m_pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
		}
	}
}

/**
 * @brief Update "Select All" item
 */
void CDirView::OnUpdateSelectAll(CCmdUI* pCmdUI)
{
	bool bEnable = (!IsLabelEdit()) || (m_pList->GetItemCount() > 0);
	pCmdUI->Enable(bEnable);
}

/**
 * @brief Handle clicks in plugin context view in list
 */
void CDirView::OnPluginSettings(UINT nID)
{
	bool unpacker = (ID_UNPACKER_SETTINGS_NONE <= nID && nID <= ID_UNPACKER_SETTINGS_SELECT);
	String pluginPipeline;
	switch (nID)
	{
	case ID_PREDIFFER_SETTINGS_NONE:
	case ID_UNPACKER_SETTINGS_NONE:
		pluginPipeline.clear();
		break;
	case ID_PREDIFFER_SETTINGS_AUTO:
	case ID_UNPACKER_SETTINGS_AUTO:
		pluginPipeline = _T("<Automatic>");
		break;
	case ID_PREDIFFER_SETTINGS_SELECT:
	case ID_UNPACKER_SETTINGS_SELECT:
		int sel = m_pList->GetNextItem(-1, LVNI_SELECTED);
		PackingInfo* infoUnpacker = nullptr;
		PrediffingInfo* infoPrediffer = nullptr;
		CDiffContext& ctxt = GetDiffContext();
		String filteredFilenames = ctxt.GetFilteredFilenames(GetDiffItem(sel));
		ctxt.FetchPluginInfos(filteredFilenames, &infoUnpacker, &infoPrediffer);
		GetDiffContext().FetchPluginInfos(filteredFilenames, &infoUnpacker, &infoPrediffer);
		CSelectPluginDlg dlg(unpacker ? infoUnpacker->GetPluginPipeline() : infoPrediffer->GetPluginPipeline(), filteredFilenames,
			unpacker ? CSelectPluginDlg::PluginType::Unpacker : CSelectPluginDlg::PluginType::Prediffer, false, this);
		if (dlg.DoModal() != IDOK)
			return;
		pluginPipeline = dlg.GetPluginPipeline();
		break;
	}
	ApplyPluginPipeline(SelBegin(), SelEnd(), GetDiffContext(), unpacker, pluginPipeline);
	Invalidate();
}

/**
 * @brief Updates just before displaying plugin context view in list
 */
void CDirView::OnUpdatePluginMode(CCmdUI* pCmdUI)
{
	// 2004-04-03, Perry
	// CMainFrame::OnUpdatePluginUnpackMode handles this for global unpacking
	// and is the template to copy, but here, this is a bit tricky
	// as a group of files may be selected
	// and they may not all have the same setting
	// so I'm not trying this right now

	pCmdUI->Enable(GetOptionsMgr()->GetBool(OPT_PLUGINS_ENABLED));

	BCMenu *pPopup = static_cast<BCMenu*>(pCmdUI->m_pSubMenu);
	if (pPopup == nullptr)
		return;

	bool unpacker = (ID_UNPACKER_SETTINGS_NONE <= pCmdUI->m_nID && pCmdUI->m_nID <= ID_UNPACKER_SETTINGS_SELECT);
	auto counts = CountPluginNoneAutoOthers(SelBegin(), SelEnd(), GetDiffContext(), unpacker);

	CheckContextMenu(pPopup, unpacker ? ID_UNPACKER_SETTINGS_NONE   : ID_PREDIFFER_SETTINGS_NONE,   (std::get<0>(counts) > 0));
	CheckContextMenu(pPopup, unpacker ? ID_UNPACKER_SETTINGS_AUTO   : ID_PREDIFFER_SETTINGS_AUTO,   (std::get<1>(counts) > 0));
	CheckContextMenu(pPopup, unpacker ? ID_UNPACKER_SETTINGS_SELECT : ID_PREDIFFER_SETTINGS_SELECT, (std::get<2>(counts) > 0));
}

/**
 * @brief Refresh cached options.
 */
void CDirView::RefreshOptions()
{
	m_nEscCloses = GetOptionsMgr()->GetInt(OPT_CLOSE_WITH_ESC);
	m_nExpandSubdirs = static_cast<eExpandSubfoldersType>(GetOptionsMgr()->GetInt(OPT_DIRVIEW_EXPAND_SUBDIRS));
	Options::DirColors::Load(GetOptionsMgr(), m_cachedColors);
	m_bUseColors = GetOptionsMgr()->GetBool(OPT_DIRCLR_USE_COLORS);
	m_pList->SetBkColor(m_bUseColors ? m_cachedColors.clrDirMargin : GetSysColor(COLOR_WINDOW));
	Invalidate();
}

/**
 * @brief Copy selected item left side paths (containing filenames) to clipboard.
 */
void CDirView::OnCopyPathnames(SIDE_TYPE stype)
{
	std::list<String> list;
	CopyPathnames(SelBegin(), SelEnd(), std::back_inserter(list), stype, GetDiffContext());
	ClipboardUtils::Put(strutils::join(list.begin(), list.end(), _T("\r\n")), GetMainFrame()->GetSafeHwnd());
}

void CDirView::OnCopyBothPathnames()
{
	std::list<String> list;
	CopyBothPathnames(SelBegin(), SelEnd(), std::back_inserter(list), GetDiffContext());
	ClipboardUtils::Put(strutils::join(list.begin(), list.end(), _T("\r\n")), GetMainFrame()->GetSafeHwnd());
}

/**
 * @brief Copy selected item filenames to clipboard.
 */
void CDirView::OnCopyFilenames()
{
	std::list<String> list;
	CopyFilenames(SelBegin(), SelEnd(), std::back_inserter(list));
	ClipboardUtils::Put(strutils::join(list.begin(), list.end(), _T("\r\n")), GetMainFrame()->GetSafeHwnd());
}

/**
 * @brief Enable/Disable dirview Copy Filenames context menu item.
 */
void CDirView::OnUpdateCopyFilenames(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(Count(&DirActions::IsItemFile).count > 0);
}

/**
 * @brief Copy selected item left side to clipboard.
 */
void CDirView::OnCopyToClipboard(SIDE_TYPE stype)
{
	std::list<String> list;
	CopyPathnames(SelBegin(), SelEnd(), std::back_inserter(list), stype, GetDiffContext());
	ClipboardUtils::PutFiles(list, GetMainFrame()->GetSafeHwnd());
}

/**
 * @brief Copy selected item both side to clipboard.
 */
void CDirView::OnCopyBothToClipboard()
{
	std::list<String> list;
	CopyBothPathnames(SelBegin(), SelEnd(), std::back_inserter(list), GetDiffContext());
	ClipboardUtils::PutFiles(list, GetMainFrame()->GetSafeHwnd());
}

/**
 * @brief Copy all displayed columns of selected items to clipboard.
 */
void CDirView::OnCopyAllDisplayedColumns()
{
	int ncols = m_pColItems->GetDispColCount();
	String text;

	int row = -1;
	while (true)
	{
		row = m_pList->GetNextItem(row, LVNI_SELECTED);
		if (row == -1)
			break;
		for (int col = 0; col < ncols; col++)
		{
			text += m_pList->GetItemText(row, col);
			if (col < ncols - 1)
				text += _T("\t");
		}
		text += _T("\r\n");
	}

	ClipboardUtils::Put(text, GetMainFrame()->GetSafeHwnd());
}

/**
 * @brief Enable/Disable dirview Copy All Displayed Columns context menu item.
 * @param[in] pCmdUI UI component to update.
 */
void CDirView::OnUpdateCopyAllDisplayedColumns(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pList->GetSelectedCount() != 0);
}

/**
 * @brief Rename a selected item on both sides.
 *
 */
void CDirView::OnItemRename()
{
	ASSERT(1 == m_pList->GetSelectedCount());
	int nSelItem = m_pList->GetNextItem(-1, LVNI_SELECTED);
	ASSERT(-1 != nSelItem);
	m_pList->EditLabel(nSelItem);
}

/**
 * @brief Enable/Disable dirview Rename context menu item.
 *
 */
void CDirView::OnUpdateItemRename(CCmdUI* pCmdUI)
{
	bool bEnabled = (1 == m_pList->GetSelectedCount());
	if (bEnabled)
	{
		Counts counts = Count(&DirActions::IsItemRenamable);
		bEnabled = (counts.count > 0 && counts.total == 1);
	}
	pCmdUI->Enable(bEnabled);
}

/**
 * @brief hide selected item filenames (removes them from the ListView)
 */
void CDirView::OnHideFilenames()
{
	CDiffContext& ctxt = GetDiffContext();
	int selection_index;
	String hiddden_item_path;
	
	m_pList->SetRedraw(FALSE);	// Turn off updating (better performance)
	DirItemIterator it;

	while ((it = SelRevBegin()) != SelRevEnd())
	{
		DIFFITEM &di = *it;
		selection_index = it.m_sel;
		hiddden_item_path = di.getItemRelativePath();
		SetItemViewFlag(di, ViewCustomFlags::HIDDEN, ViewCustomFlags::VISIBILITY);
		DeleteItem(selection_index);
		ctxt.m_vCurrentlyHiddenItems.push_back(hiddden_item_path);
	}
	m_pList->SetRedraw(TRUE);	// Turn updating back on
}

/**
 * @brief determine if an item-relative-path is contained in the list of items to hide
 */
bool CDirView::IsItemToHide(const String& currentItem, const std::vector<String>& ItemsToHide) const
{
	return std::find(ItemsToHide.begin(), ItemsToHide.end(), currentItem) != ItemsToHide.end();
}

/**
 * @brief hides items specified in the .winmerge file
 */

void CDirView::HideItems(const std::vector<String>& ItemsToHide)
{
	CDiffContext& ctxt = GetDiffContext();
	DIFFITEM *diffpos = ctxt.GetFirstDiffPosition();
	while (diffpos != nullptr)
	{
		DIFFITEM &di = ctxt.GetNextDiffRefPosition(diffpos);
		if (IsItemToHide(di.getItemRelativePath(), ItemsToHide))
			SetItemViewFlag(di, ViewCustomFlags::HIDDEN, ViewCustomFlags::VISIBILITY);
	}

	m_pList->SetRedraw(FALSE);	// Turn off updating (better performance)

	int num_hidden = 0;
	const size_t num_to_hide = ItemsToHide.size();
	DirItemIterator it = RevBegin();
	while((num_hidden < num_to_hide) && (it != RevEnd()))
	{
		DIFFITEM& di = *it;
		if (di.customFlags & ViewCustomFlags::HIDDEN)
		{
			DeleteItem(it.m_sel);
			num_hidden++;
		}
		++it;
	}

	m_pList->SetRedraw(TRUE);	// Turn updating back on
}

/**
 * @brief update menu item
 */
void CDirView::OnUpdateHideFilenames(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pList->GetSelectedCount() != 0);
}

/// User chose (context men) Move from right to left
template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
void CDirView::OnCtxtDirMove()
{
	DoDirAction(&DirActions::Move<srctype, dsttype>, _("Moving files..."));
}

/// User chose (context menu) Move left to...
template<SIDE_TYPE stype>
void CDirView::OnCtxtDirMoveTo()
{
	DoDirActionTo(stype, &DirActions::MoveTo<stype>, _("Moving files..."));
}

/// Update context menu Move Right to Left item
template<SIDE_TYPE srctype, SIDE_TYPE dsttype>
void CDirView::OnUpdateCtxtDirMove(CCmdUI* pCmdUI)
{
	DoUpdateDirMove<srctype, dsttype>(pCmdUI, eContext);
}

/**
 * @brief Update "Move | Left to..." item
 */
template<SIDE_TYPE stype>
void CDirView::OnUpdateCtxtDirMoveTo(CCmdUI* pCmdUI)
{
	Counts counts = Count(&DirActions::IsItemMovableToOn<stype>);
	pCmdUI->Enable(counts.count > 0);
	pCmdUI->SetText(FormatMenuItemStringTo(stype, counts.count, counts.total).c_str());
}

/**
 * @brief Update title after window is resized.
 */
void CDirView::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	GetDocument()->SetTitle(nullptr);
}

/**
 * @brief Called when user selects 'Delete' from 'Merge' menu.
 */
void CDirView::OnDelete()
{
	DoDirAction(&DirActions::DeleteOnEitherOrBoth, _("Deleting files..."));
}

/**
 * @brief Enables/disables 'Delete' item in 'Merge' menu.
 */
void CDirView::OnUpdateDelete(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(Count(&DirActions::IsItemDeletableOnEitherOrBoth).count > 0);
}

/**
 * @brief Called when item state is changed.
 *
 * Show count of selected items in statusbar.
 */
void CDirView::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

	// If item's selected state changed
	if ((pNMListView->uOldState & LVIS_SELECTED) !=
			(pNMListView->uNewState & LVIS_SELECTED))
	{
		SetTimer(TIMER_ID_STATUSBAR_UPDATE, 100, nullptr);
	}
	*pResult = 0;
}

/**
 * @brief Called before user start to item label edit.
 *
 * Disable label edit if initiated from a user double-click.
 */
afx_msg void CDirView::OnBeginLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
	Counts counts = Count(&DirActions::IsItemRenamable);
	*pResult = !(counts.count > 0 && counts.total == 1);

	// If label edit is allowed.
	if (*pResult == FALSE)
	{
		const NMLVDISPINFO *pdi = (NMLVDISPINFO*)pNMHDR;
		ASSERT(pdi != nullptr);

		// Locate the edit box on the right column in case the user changed the
		// column order.
		const int nColPos = m_pColItems->ColLogToPhys(0);

		// Get text from the "File Name" column.
		CString sText = m_pList->GetItemText(pdi->item.iItem, nColPos);
		ASSERT(!sText.IsEmpty());

		// Keep only left file name (separated by '|'). This form occurs
		// when two files exists with same name but not in same case.
		int nPos = sText.Find('|');
		if (-1 != nPos)
		{
			sText = sText.Left(nPos);
		}

		// Set the edit control with the updated text.
		CEdit *pEdit = m_pList->GetEditControl();
		ASSERT(pEdit != nullptr);
		pEdit->SetWindowText(sText);
		// Select file name without file extension
		if (!GetDiffItem(pdi->item.iItem).diffcode.isDirectory())
		{
			int nPosExt = sText.ReverseFind('.');
			if (nPosExt >= 1)
				pEdit->SetSel(0, nPosExt);
		}

		m_bUserCancelEdit = false;
	}
}

/**
 * @brief Called when user done with item label edit.
 *
 */
afx_msg void CDirView::OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = FALSE;

	// We can't use the normal condition of pszText==`nullptr` to know if the
	// user cancels editing when file names had different case (e.g.
	// "file.txt|FILE.txt"). The edit text was changed to "file.txt" and
	// if the user accept it as the new file name, pszText is `nullptr`.

	if (!m_bUserCancelEdit)
	{
		CEdit *pEdit = m_pList->GetEditControl();
		ASSERT(pEdit != nullptr);

		CString sText;
		pEdit->GetWindowText(sText);

		if (!sText.IsEmpty() && paths::IsValidName(String(sText)))
		{
			try {
				DirItemIterator it(m_pIList.get(), reinterpret_cast<NMLVDISPINFO *>(pNMHDR)->item.iItem);
				if (!it.IsValid())
					return;
				DIFFITEM& di = *it;
				unsigned sideFlags = (di.diffcode.diffcode & DIFFCODE::SIDEFLAGS);
				*pResult = DoItemRename(it, GetDiffContext(), String(sText));
				// Rescan the item if side flags change due to renaming.
				if (*pResult)
				{
					if ((di.diffcode.diffcode & DIFFCODE::SIDEFLAGS) != sideFlags)
					{
						// Delete the item with the same file name as after renaming.
						if (di.HasParent())
						{
							for (DIFFITEM* pItem = di.GetParentLink()->GetFirstChild(); pItem != nullptr; pItem = pItem->GetFwdSiblingLink())
							{
								if ((pItem != &di) && (pItem->diffcode.isDirectory() == di.diffcode.isDirectory()) && (collstr(pItem->diffFileInfo[0].filename, di.diffFileInfo[0].filename, false) == 0))
								{
									if (pItem->renameMoveGroupId !=	-1)
										GetDiffContext().m_pRenameMoveDetection->RemoveItemFromGroup(pItem);
									pItem->DelinkFromSiblings();
									delete pItem;
									break;
								}
							}
						}
						// Rescan the item.
						MarkForRescan(di);
						m_pSavedUIState.reset(SaveUIState());
						m_pSavedTreeState.reset(SaveTreeState(GetDiffContext()));
						GetDocument()->SetMarkedRescan();
						GetDocument()->Rescan();
					}
					else {
						int nDirs = GetDiffContext().GetCompareDirs();
						assert(nDirs == 2 || nDirs == 3);
						UpdatePaths(nDirs, di);

						int nIdx = reinterpret_cast<NMLVDISPINFO*>(pNMHDR)->item.iItem;
						UpdateDiffItemStatus(nIdx);
					}
				}
			} catch (ContentsChangedException& e) {
				AfxMessageBox(e.m_msg.c_str(), MB_ICONWARNING);
			}
		}
		else
		{
			I18n::MessageBox(IDS_ERROR_INVALID_DIR_FILE_NAME, MB_ICONWARNING);
		}
	}
}

void CDirView::OnODFindItem(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVFINDITEM* pFindItem = reinterpret_cast<NMLVFINDITEM*>(pNMHDR);
	if (pFindItem->lvfi.flags & LVFI_STRING)
	{
		String text = strutils::makelower(pFindItem->lvfi.psz);
		for (size_t i = pFindItem->iStart; i < m_listViewItems.size(); ++i)
		{
			DIFFITEM *di = GetItemKey(static_cast<int>(i));
			if (di && !IsDiffItemSpecial(di))
			{
				String filename = strutils::makelower(di->diffFileInfo[0].filename);

				if (tc::tcsncmp(text.c_str(), filename.c_str(), text.length()) == 0)
				{
					*pResult = i;
					return;
				}
			}
		}
	}
	*pResult = -1;
}

/**
 * @brief Called when item is marked for rescan.
 * This function marks selected items for rescan and rescans them.
 */
void CDirView::OnMarkedRescan()
{
	std::for_each(SelBegin(), SelEnd(), MarkForRescan);
	if (std::distance(SelBegin(), SelEnd()) > 0)
	{
		m_pSavedUIState.reset(SaveUIState());
		m_pSavedTreeState.reset(SaveTreeState(GetDiffContext()));
		GetDocument()->SetMarkedRescan();
		GetDocument()->Rescan();
	}
}

/**
 * @brief Called to update the item count in the status bar
 */
void CDirView::OnUpdateStatusNum(CCmdUI* pCmdUI)
{
	String s; // text to display

	int count = m_pList->GetItemCount();
	int focusItem = GetFocusedItem();

	if (focusItem == -1)
	{
		// No item has focus
		// "Items: %1"
		s = strutils::format_string1(_("Items: %1"), strutils::to_str(count));
	}
	else
	{
		// Don't show number to special items
		DIFFITEM *pos = GetItemKey(focusItem);
		if (!IsDiffItemSpecial(pos))
		{
			// If compare is non-recursive reduce special items count
			bool bRecursive = GetDiffContext().m_bRecursive;
			if (!bRecursive)
			{
				--focusItem;
				--count;
			}
			// "Item %1 of %2"
			s = strutils::format_string2(_("Item %1 of %2"), 
					strutils::to_str(focusItem + 1), strutils::to_str(count));
		}
	}
	pCmdUI->SetText(s.c_str());
}

/**
 * @brief Show all hidden items.
 */
void CDirView::OnViewShowHiddenItems()
{
	CDiffContext& ctxt = GetDiffContext();
	SetItemViewFlag(GetDiffContext(), ViewCustomFlags::VISIBLE, ViewCustomFlags::VISIBILITY);
	ctxt.m_vCurrentlyHiddenItems.clear();
	Redisplay();
}

/**
 * @brief Enable/Disable 'Show hidden items' menuitem.
 */
void CDirView::OnUpdateViewShowHiddenItems(CCmdUI* pCmdUI)
{
	const CDiffContext& ctxt = GetDiffContext();
	pCmdUI->Enable(ctxt.m_vCurrentlyHiddenItems.size() > 0);
}

/**
 * @brief Toggle Tree Mode
 */
void CDirView::OnViewTreeMode()
{
	m_bTreeMode = !m_bTreeMode;
	m_dirfilter.tree_mode = m_bTreeMode;
	GetOptionsMgr()->SaveOption(OPT_TREE_MODE, m_bTreeMode); // reverse
	Redisplay();
}

/**
 * @brief Check/Uncheck 'Tree Mode' menuitem.
 */
void CDirView::OnUpdateViewTreeMode(CCmdUI* pCmdUI)
{
	// Don't show Tree Mode as 'checked' if the
	// menu item is greyed out (disabled).  Its very confusing.
	if( GetDocument()->GetDiffContext().m_bRecursive ) {
		pCmdUI->SetCheck(m_bTreeMode);
		pCmdUI->Enable(TRUE);
	} else {
		pCmdUI->SetCheck(FALSE);
		pCmdUI->Enable(FALSE);
	}
}

/**
 * @brief Toggle Show Empty Folders
 */
void CDirView::OnViewShowEmptyFolders()
{
	m_dirfilter.show_empty_folders = !m_dirfilter.show_empty_folders;
	GetOptionsMgr()->SaveOption(OPT_SHOW_EMPTY_FOLDERS, m_dirfilter.show_empty_folders);
	Redisplay();
}

/**
 * @brief Update 'Show Empty Folders' menuitem.
 */
void CDirView::OnUpdateViewShowEmptyFolders(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_dirfilter.show_empty_folders);
	pCmdUI->Enable(GetDocument()->GetDiffContext().m_bRecursive && m_bTreeMode);
}

/**
 * @brief Expand all subfolders
 */
void CDirView::OnViewExpandAllSubdirs()
{
	ExpandAllSubdirs(GetDiffContext());
	Redisplay();
}

/**
 * @brief Expand different subfolders
 */
void CDirView::OnViewExpandDifferentSubdirs()
{
	ExpandDifferentSubdirs(GetDiffContext());
	Redisplay();
}

/**
 * @brief Expand identical subfolders
 */
void CDirView::OnViewExpandIdenticalSubdirs()
{
	ExpandIdenticalSubdirs(GetDiffContext());
	Redisplay();
}

/**
 * @brief Update "Expand All Subfolders" item
 */
void CDirView::OnUpdateViewExpandSubdirs(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_bTreeMode && GetDiffContext().m_bRecursive);
}

/**
 * @brief Collapse all subfolders
 */
void CDirView::OnViewCollapseAllSubdirs()
{
	CollapseAllSubdirs(GetDiffContext());
	Redisplay();
}

/**
 * @brief Update "Collapse All Subfolders" item
 */
void CDirView::OnUpdateViewCollapseAllSubdirs(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_bTreeMode && GetDiffContext().m_bRecursive);
}

void CDirView::OnViewSwapPanes(int pane1, int pane2)
{
	GetDocument()->Swap(pane1, pane2);
	Redisplay();
}

void CDirView::OnUpdateViewSwapPanes(CCmdUI* pCmdUI, int pane1, int pane2)
{
	pCmdUI->Enable(pane2 < GetDocument()->m_nDirs &&
		GetDocument()->m_diffThread.GetThreadState() == CDiffThread::THREAD_COMPLETED);
}

/**
 * @brief Show/Hide different files/directories
 */
void CDirView::OnOptionsShowDifferent() 
{
	m_dirfilter.show_different = !m_dirfilter.show_different;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT, m_dirfilter.show_different);
	Redisplay();
}

/**
 * @brief Show/Hide identical files/directories
 */
void CDirView::OnOptionsShowIdentical() 
{
	m_dirfilter.show_identical = !m_dirfilter.show_identical;
	GetOptionsMgr()->SaveOption(OPT_SHOW_IDENTICAL, m_dirfilter.show_identical);
	Redisplay();
}

/**
 * @brief Show/Hide left-only files/directories
 */
void CDirView::OnOptionsShowUniqueLeft() 
{
	m_dirfilter.show_unique_left = !m_dirfilter.show_unique_left;
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_LEFT, m_dirfilter.show_unique_left);
	Redisplay();
}

/**
 * @brief Show/Hide middle-only files/directories
 */
void CDirView::OnOptionsShowUniqueMiddle() 
{
	m_dirfilter.show_unique_middle = !m_dirfilter.show_unique_middle;
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_MIDDLE, m_dirfilter.show_unique_middle);
	Redisplay();
}

/**
 * @brief Show/Hide right-only files/directories
 */
void CDirView::OnOptionsShowUniqueRight() 
{
	m_dirfilter.show_unique_right = !m_dirfilter.show_unique_right;
	GetOptionsMgr()->SaveOption(OPT_SHOW_UNIQUE_RIGHT, m_dirfilter.show_unique_right);
	Redisplay();
}

/**
 * @brief Show/Hide binary files
 */
void CDirView::OnOptionsShowBinaries()
{
	m_dirfilter.show_binaries = !m_dirfilter.show_binaries;
	GetOptionsMgr()->SaveOption(OPT_SHOW_BINARIES, m_dirfilter.show_binaries);
	Redisplay();
}

/**
 * @brief Show/Hide skipped files/directories
 */
void CDirView::OnOptionsShowSkipped()
{
	m_dirfilter.show_skipped = !m_dirfilter.show_skipped;
	GetOptionsMgr()->SaveOption(OPT_SHOW_SKIPPED, m_dirfilter.show_skipped);
	Redisplay();
}

/**
 * @brief Show/Hide different files/folders (Middle and right are identical)
 */
void CDirView::OnOptionsShowDifferentLeftOnly() 
{
	m_dirfilter.show_different_left_only = !m_dirfilter.show_different_left_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_LEFT_ONLY, m_dirfilter.show_different_left_only);
	Redisplay();
}

/**
 * @brief Show/Hide different files/folders (Left and right are identical)
 */
void CDirView::OnOptionsShowDifferentMiddleOnly() 
{
	m_dirfilter.show_different_middle_only = !m_dirfilter.show_different_middle_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_MIDDLE_ONLY, m_dirfilter.show_different_middle_only);
	Redisplay();
}

/**
 * @brief Show/Hide different files/folders (Left and middle are identical)
 */
void CDirView::OnOptionsShowDifferentRightOnly() 
{
	m_dirfilter.show_different_right_only = !m_dirfilter.show_different_right_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_DIFFERENT_RIGHT_ONLY, m_dirfilter.show_different_right_only);
	Redisplay();
}

/**
 * @brief Show/Hide missing left only files/folders
 */
void CDirView::OnOptionsShowMissingLeftOnly() 
{
	m_dirfilter.show_missing_left_only = !m_dirfilter.show_missing_left_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_MISSING_LEFT_ONLY, m_dirfilter.show_missing_left_only);
	Redisplay();
}

/**
 * @brief Show/Hide missing middle only files/folders
 */
void CDirView::OnOptionsShowMissingMiddleOnly() 
{
	m_dirfilter.show_missing_middle_only = !m_dirfilter.show_missing_middle_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_MISSING_MIDDLE_ONLY, m_dirfilter.show_missing_middle_only);
	Redisplay();
}

/**
 * @brief Show/Hide missing right only files/folders
 */
void CDirView::OnOptionsShowMissingRightOnly() 
{
	m_dirfilter.show_missing_right_only = !m_dirfilter.show_missing_right_only;
	GetOptionsMgr()->SaveOption(OPT_SHOW_MISSING_RIGHT_ONLY, m_dirfilter.show_missing_right_only);
	Redisplay();
}

void CDirView::OnUpdateOptionsShowdifferent(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_dirfilter.show_different);
}

void CDirView::OnUpdateOptionsShowidentical(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_dirfilter.show_identical);
}

void CDirView::OnUpdateOptionsShowuniqueleft(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_dirfilter.show_unique_left);
}

void CDirView::OnUpdateOptionsShowuniquemiddle(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_unique_middle);
}

void CDirView::OnUpdateOptionsShowuniqueright(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_dirfilter.show_unique_right);
}

void CDirView::OnUpdateOptionsShowBinaries(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_dirfilter.show_binaries);
}

void CDirView::OnUpdateOptionsShowSkipped(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_dirfilter.show_skipped);
}

void CDirView::OnUpdateOptionsShowDifferentLeftOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_different_left_only);
}

void CDirView::OnUpdateOptionsShowDifferentMiddleOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_different_middle_only);
}

void CDirView::OnUpdateOptionsShowDifferentRightOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_different_right_only);
}

void CDirView::OnUpdateOptionsShowMissingLeftOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_missing_left_only);
}

void CDirView::OnUpdateOptionsShowMissingMiddleOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_missing_middle_only);
}

void CDirView::OnUpdateOptionsShowMissingRightOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetDocument()->m_nDirs > 2);
	pCmdUI->SetCheck(m_dirfilter.show_missing_right_only);
}

/**
 * @brief User chose the following menu on menu or context menu:
 * - Compare
 * - Compare in New Window
 *
 * @param [in] nID Menu ID of the selected item
 */
void CDirView::OnMergeCompare(UINT nID)
{
	bool openableForDir = !((nID >= ID_MERGE_COMPARE_TEXT && nID <= ID_MERGE_COMPARE_FOLDER) ||
		(nID >= ID_UNPACKERS_FIRST && nID <= ID_UNPACKERS_LAST));

	if (AreItemsComparable(SELECTIONTYPE_NORMAL, openableForDir))
	{
		OpenSelection(nID == ID_MERGE_COMPARE ? GetDocument() : nullptr);
	}
	else if (AreItemsComparableIndivisually(nID, openableForDir))
	{
		int sel1 = -1;
		while (true)
		{
			sel1 = m_pList->GetNextItem(sel1, LVNI_SELECTED);
			if (sel1 == -1)
				break;
			OpenSelection(sel1, -1, -1, nID == ID_MERGE_COMPARE ? GetDocument() : nullptr);
		}
	}
}

void CDirView::OnMergeCompareWithRenamedMoved()
{
	CDirDoc *pDoc = GetDocument();
	CDiffContext& ctxt = GetDiffContext();
	std::vector<PathContext> pathContextVec;
	std::vector<std::array<fileopenflags_t, 3>> flagsVec;
	std::vector<std::array<FileTextEncoding, 3>> encodingVec;
	const int nDirs = ctxt.GetCompareDirs();

	if (!ctxt.m_pRenameMoveDetection)
		return;
	
	// Iterate through all selected items
	for (int sel = m_pList->GetNextItem(-1, LVNI_SELECTED); sel != -1; sel = m_pList->GetNextItem(sel, LVNI_SELECTED))
	{
		const DIFFITEM* pdi = &GetDiffItem(sel);
		
		// Get renamed/moved items for each pane
		std::vector<std::vector<const DIFFITEM*>> renameMoveItemsVec;
		for (int nIndex = 0; nIndex < nDirs; ++nIndex)
		{
			renameMoveItemsVec.push_back(ctxt.m_pRenameMoveDetection->GetRenameMoveGroupItemsForSide(*pdi, nIndex));
			if (renameMoveItemsVec[nIndex].empty())
				renameMoveItemsVec[nIndex].push_back(DIFFITEM::GetEmptyItem());
		}
		
		// Generate all combinations of renamed/moved items
		const DIFFITEM* pdiTmp[3];
		auto generateCombinations = [&]()
		{
			PathContext paths;
			std::array<FileTextEncoding, 3> encoding;
			std::array<fileopenflags_t, 3> dwFlags = {};
			
			for (int nIndex = 0, j = 0; nIndex < nDirs; ++nIndex)
			{
				const bool isEmpty = pdiTmp[nIndex] == DIFFITEM::GetEmptyItem();
				if (pdi->diffcode.isDirectory() && isEmpty)
					continue;
				paths.SetPath(j, isEmpty ?  _T("") : GetItemFileName(ctxt, *pdiTmp[nIndex], nIndex));
				encoding[j] = pdiTmp[nIndex]->diffFileInfo[nIndex].encoding;
				dwFlags[j] = FFILEOPEN_NOMRU | (pDoc->GetReadOnly(nIndex) ? FFILEOPEN_READONLY : 0);
				j++;
			}
			
			pathContextVec.push_back(paths);
			encodingVec.push_back(encoding);
			flagsVec.push_back(dwFlags);
		};
		
		// Create combinations based on number of directories
		for (size_t i = 0; i < renameMoveItemsVec[0].size(); ++i)
		{
			pdiTmp[0] = renameMoveItemsVec[0][i];
			for (size_t j = 0; j < renameMoveItemsVec[1].size(); ++j)
			{
				pdiTmp[1] = renameMoveItemsVec[1][j];
				
				if (nDirs == 2)
				{
					generateCombinations();
				}
				else if (nDirs == 3)
				{
					for (size_t k = 0; k < renameMoveItemsVec[2].size(); ++k)
					{
						pdiTmp[2] = renameMoveItemsVec[2][k];
						generateCombinations();
					}
				}
			}
		}
	}

	// Open all collected items
	for (size_t i = 0; i < pathContextVec.size(); ++i)
		Open(pDoc, pathContextVec[i], flagsVec[i].data(), encodingVec[i].data());
}

void CDirView::OnMergeCompareNonHorizontally()
{
	int sel1, sel2, sel3;
	if (!GetSelectedItems(&sel1, &sel2, &sel3))
		return;
	DirSelectFilesDlg dlg;
	if (sel1 != -1)
		dlg.m_pdi[0] = &GetDiffItem(sel1);
	if (sel2 != -1)
		dlg.m_pdi[1] = &GetDiffItem(sel2);
	if (sel3 != -1)
		dlg.m_pdi[2] = &GetDiffItem(sel3);
	if (dlg.DoModal() == IDOK && dlg.m_selectedButtons.size() > 0)
	{
		CDirDoc *pDoc = GetDocument();
		FileTextEncoding encoding[3];
		fileopenflags_t dwFlags[3] = {};
		PathContext paths;
		for (int nIndex = 0; nIndex < static_cast<int>(dlg.m_selectedButtons.size()); ++nIndex)
		{
			int n = dlg.m_selectedButtons[nIndex];
			dwFlags[nIndex] = FFILEOPEN_NOMRU | (pDoc->GetReadOnly(n % 3) ? FFILEOPEN_READONLY : 0);
			if (dlg.m_pdi[n / 3])
			{
				paths.SetPath(nIndex, GetItemFileName(pDoc->GetDiffContext(), *dlg.m_pdi[n / 3], n % 3));
				encoding[nIndex] = dlg.m_pdi[n / 3]->diffFileInfo[n % 3].encoding;
			}
		}
		if (paths.GetSize() == 1)
			paths.SetRight(_T(""));
		Open(GetDocument(), paths, dwFlags, encoding);
	}
}

/**
 * @brief User chose the following menu on context menu:
 * - Compare As > Text
 * - Compare As > Table
 * - Compare As > Binary
 * - Compare As > Image
 * - Compare As > Webpage
 *
 * @param [in] nID Menu ID of the selected item
 */
void CDirView::OnMergeCompareAs(UINT nID)
{
	if (nID >= ID_MERGE_COMPARE_TEXT && nID <= ID_MERGE_COMPARE_FOLDER)
	{
		if (AreItemsComparable(SELECTIONTYPE_NORMAL, false))
		{
			OpenSelectionAs(nID);
		}
		else if (AreItemsComparableIndivisually(nID, false))
		{
			int sel1 = -1;
			while (true)
			{
				sel1 = m_pList->GetNextItem(sel1, LVNI_SELECTED);
				if (sel1 == -1)
					break;
				OpenSelectionAs(sel1, -1, -1, nID);
			}
		}
	}
	else
	{
		OpenSelectionAs(nID);
	}
}

/**
 * @brief Enable/disable the following menu choice on menu or context menu:
 * - Compare
 * - Compare in New Window
 * - Compare As > Text
 * - Compare As > Table
 * - Compare As > Binary
 * - Compare As > Image
 * - Compare As > Webpage
 * 
 * @param [in] pCmdUI UI component to update.
 */
void CDirView::OnUpdateMergeCompare(CCmdUI *pCmdUI)
{
	bool openableForDir = !((pCmdUI->m_nID >= ID_MERGE_COMPARE_TEXT && pCmdUI->m_nID <= ID_MERGE_COMPARE_FOLDER) ||
		(pCmdUI->m_nID >= ID_UNPACKERS_FIRST && pCmdUI->m_nID <= ID_UNPACKERS_LAST));

	bool bOn = AreItemsComparable(SELECTIONTYPE_NORMAL, openableForDir);
	if (!bOn)
		bOn = AreItemsComparableIndivisually(pCmdUI->m_nID, openableForDir);
	pCmdUI->Enable(static_cast<BOOL>(bOn));
}

void CDirView::OnUpdateNoUnpacker(CCmdUI *pCmdUI)
{
	pCmdUI->Enable();
	pCmdUI->m_pMenu->DeleteMenu(pCmdUI->m_nID, MF_BYCOMMAND);

	int sel = GetSingleSelectedItem();
	if (sel == -1 || GetItemKey(sel) == reinterpret_cast<DIFFITEM *>(SPECIAL_ITEM_POS))
		return;

	String filteredFilenames = GetDiffContext().GetFilteredFilenames(*GetItemKey(sel));
	PluginMenu::AppendPluginMenus(pCmdUI->m_pMenu, nullptr, filteredFilenames,
		FileTransform::UnpackerEventNames, PluginMenu::AddAllMenu|PluginMenu::AddSelectMenu, ID_UNPACKERS_FIRST);
}

void CDirView::OnViewCompareStatistics()
{
	CompareStatisticsDlg dlg(GetDocument()->GetCompareStats());
	dlg.DoModal();
}

/**
 * @brief Count left & right files, and number with editable text encoding
 * @param nLeft [out]  #files on left side selected
 * @param nLeftAffected [out]  #files on left side selected which can have text encoding changed
 * @param nRight [out]  #files on right side selected
 * @param nRightAffected [out]  #files on right side selected which can have text encoding changed
 *
 * Affected files include all except unicode files
 */
void CDirView::FormatEncodingDialogDisplays(CLoadSaveCodepageDlg * dlg)
{
	IntToIntMap currentCodepages = CountCodepages(SelBegin(), SelEnd(), GetDiffContext());

	Counts left, middle, right;
	left = Count(&DirActions::IsItemEditableEncoding<SIDE_LEFT>);
	if (GetDocument()->m_nDirs > 2)
		middle = Count(&DirActions::IsItemEditableEncoding<SIDE_MIDDLE>);
	right = Count(&DirActions::IsItemEditableEncoding<SIDE_RIGHT>);

	// Format strings such as "25 of 30 Files Affected"
	String sLeftAffected = FormatFilesAffectedString(left.count, left.total);
	String sMiddleAffected = (GetDocument()->m_nDirs < 3) ? _T("") : FormatFilesAffectedString(middle.count, middle.total);
	String sRightAffected = FormatFilesAffectedString(right.count, right.total);
	dlg->SetLeftRightAffectStrings(sLeftAffected, sMiddleAffected, sRightAffected);
	int codepage = currentCodepages.FindMaxKey();
	dlg->SetCodepages(codepage);
}

/**
 * @brief Display file encoding dialog to user & handle user's choices
 *
 * This handles DirView invocation, so multiple files may be affected
 */
void CDirView::DoFileEncodingDialog()
{
	CLoadSaveCodepageDlg dlg(GetDocument()->m_nDirs);
	// set up labels about what will be affected
	FormatEncodingDialogDisplays(&dlg);
	dlg.EnableSaveCodepage(false); // disallow setting a separate codepage for saving

	// Invoke dialog
	if (dlg.DoModal() != IDOK)
		return;

	bool affected[3];
	affected[0] = dlg.DoesAffectLeft();
	affected[1] = dlg.DoesAffectMiddle();
	affected[SideToIndex(GetDiffContext(), SIDE_RIGHT)] = dlg.DoesAffectRight();

	ApplyCodepage(SelBegin(), SelEnd(), GetDiffContext(), affected, dlg.GetLoadCodepage());

	m_pList->InvalidateRect(nullptr);
	m_pList->UpdateWindow();

	// TODO: We could loop through any active merge windows belonging to us
	// and see if any of their files are affected
	// but, if they've been edited, we cannot throw away the user's work?
}

/**
 * @brief Display file encoding dialog & handle user's actions
 */
void CDirView::OnFileEncoding()
{
	DoFileEncodingDialog();
}

/** @brief Open help from mainframe when user presses F1*/
void CDirView::OnHelp()
{
	CMergeApp::ShowHelp(DirViewHelpLocation);
}

/**
 * @brief true while user is editing a file name.
 */
bool CDirView::IsLabelEdit() const
{
	return (m_pList->GetEditControl() != nullptr);
}

/**
 * @brief Allow edit "Paste" when renaming an item.
 */
void CDirView::OnEditCopy()
{
	CEdit *pEdit = m_pList->GetEditControl();
	if (pEdit != nullptr)
	{
		pEdit->Copy();
	}
}

/**
 * @brief Allow edit "Cut" when renaming an item.
 */
void CDirView::OnEditCut()
{
	CEdit *pEdit = m_pList->GetEditControl();
	if (pEdit != nullptr)
	{
		pEdit->Cut();
	}
}

/**
* @brief Allow edit "Paste" when renaming an item.
 */
void CDirView::OnEditPaste()
{
	CEdit *pEdit = m_pList->GetEditControl();
	if (pEdit != nullptr)
	{
		pEdit->Paste();
	}
}

/**
 * @brief Allow edit "Undo" when renaming an item.
 */
void CDirView::OnEditUndo()
{
	CEdit *pEdit = m_pList->GetEditControl();
	if (pEdit != nullptr)
	{
		pEdit->Undo();
	}
}

/**
 * @brief Update the tool bar's "Undo" icon. It should be enabled when
 * renaming an item and undo is possible.
 */
void CDirView::OnUpdateEditUndo(CCmdUI* pCmdUI)
{
	CEdit *pEdit = m_pList->GetEditControl();
	pCmdUI->Enable(pEdit && pEdit->CanUndo());
}
/**
 * @brief Returns CShellContextMenu object that owns given HMENU.
 *
 * @param [in] hMenu Handle to the menu to check ownership of.
 * @return Either m_pShellContextMenuLeft, m_pShellContextMenuRight
 *   or `nullptr` if hMenu is not owned by these two.
 */
CShellContextMenu* CDirView::GetCorrespondingShellContextMenu(HMENU hMenu) const
{
	CShellContextMenu* pMenu = nullptr;
	if (m_pShellContextMenuLeft!=nullptr && hMenu == m_pShellContextMenuLeft->GetHMENU())
		pMenu = m_pShellContextMenuLeft.get();
	else if (m_pShellContextMenuMiddle!=nullptr && hMenu == m_pShellContextMenuMiddle->GetHMENU())
		pMenu = m_pShellContextMenuMiddle.get();
	else if (m_pShellContextMenuRight!=nullptr && hMenu == m_pShellContextMenuRight->GetHMENU())
		pMenu = m_pShellContextMenuRight.get();

	return pMenu;
}

/**
 * @brief Handle messages related to correct menu working.
 *
 * We need to requery shell context menu each time we switch from context menu
 * for one side to context menu for other side. Here we check whether we need to
 * requery and call ShellContextMenuHandleMenuMessage.
 */
LRESULT CDirView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	while (message == WM_INITMENUPOPUP)
	{
		HMENU hMenu = (HMENU)wParam;
		if (CShellContextMenu* pMenu = GetCorrespondingShellContextMenu(hMenu))
		{
			if (m_hCurrentMenu != hMenu)
			{
				// re-query context menu once more, because if context menu was queried for right
				// group of files and we are showing menu for left group (or vice versa) menu will
				// be shown incorrectly
				// also, if context menu was last queried for right group of files and we are
				// invoking command for left command will be executed for right group (the last
				// group that menu was requested for)
				// may be a "feature" of Shell

				pMenu->RequeryShellContextMenu();
				m_hCurrentMenu = hMenu;
			}
		}
		break;
	}

	CShellContextMenu* pMenu = GetCorrespondingShellContextMenu(m_hCurrentMenu);

	if (pMenu != nullptr)
	{
		LRESULT res = 0;
		pMenu->HandleMenuMessage(message, wParam, lParam, res);
	}

	return __super::WindowProc(message, wParam, lParam);
}

/**
 * @brief Implement background item coloring
 */
void CDirView::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if (!m_bUseColors) {
		return;
	}

	LPNMLISTVIEW pNM = (LPNMLISTVIEW)pNMHDR;
	*pResult = CDRF_DODEFAULT;

	if (pNM->hdr.code == NM_CUSTOMDRAW)
	{
		LPNMLVCUSTOMDRAW lpC = (LPNMLVCUSTOMDRAW)pNMHDR;

		// Merge20 (Phase 4 D9): in slim mode the review-state tag is
		// drawn wrapped (DT_WORDBREAK) in its 126px column (HTML
		// tgSlim), so a long tag is never clipped. The two-line rows
		// come from the taller slim image list
		// (BuildMerge20SlimImageList). Only the review subitem of an
		// unselected row is custom-drawn: selected rows keep the stock
		// selection highlight, and every other subitem keeps the color
		// handling below. Gated on m_bUseColors like the rest of this
		// function (list colors are on by default).
		if (m_bMerge20Slim &&
			(lpC->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM) ||
			 lpC->nmcd.dwDrawStage == (CDDS_ITEMPOSTPAINT | CDDS_SUBITEM)) &&
			lpC->iSubItem == GetMerge20SlimColumnPhys(_T("Sreview")))
		{
			const int nRow = static_cast<int>(lpC->nmcd.dwItemSpec);
			if ((m_pList->GetItemState(nRow, LVIS_SELECTED) & LVIS_SELECTED) == 0)
			{
				if (lpC->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
				{
					*pResult = CDRF_NOTIFYPOSTPAINT;
					return;
				}
				CDC* pDC = CDC::FromHandle(lpC->nmcd.hdc);
				CRect rc(lpC->nmcd.rc);
				COLORREF clrBk = 0, clrText = 0;
				GetColors(nRow, lpC->iSubItem, clrBk, clrText);
				pDC->FillSolidRect(&rc, clrBk);
				const DIFFITEM& di = GetDiffItem(nRow);
				const String sText = m_pColItems->ColGetTextToDisplay(&GetDiffContext(), GetMerge20SlimColumnLog(_T("Sreview")), di);
				CFont* pOldFont = pDC->SelectObject(m_pList->GetFont());
				pDC->SetBkMode(TRANSPARENT);
				pDC->SetTextColor(clrText);
				rc.DeflateRect(2, 2, 2, 2);
				pDC->DrawText(sText.c_str(), &rc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
				pDC->SelectObject(pOldFont);
				*pResult = CDRF_SKIPDEFAULT;
				return;
			}
		}

		if (lpC->nmcd.dwDrawStage == CDDS_PREPAINT)
		{
			*pResult =  CDRF_NOTIFYITEMDRAW;
			return;
		}

		if (lpC->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
		{
			*pResult = CDRF_NOTIFYITEMDRAW;
			return;
		}

		if (lpC->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM ))
		{
			GetColors (static_cast<int>(lpC->nmcd.dwItemSpec), lpC->iSubItem, lpC->clrTextBk, lpC->clrText);
		}
	}
}

/**
 * @brief Populate colors for items in view, depending on difference status
 */
void CDirView::GetColors (int nRow, int nCol, COLORREF& clrBk, COLORREF& clrText) const
{
	const DIFFITEM& di = GetDiffItem (nRow);

	if (di.isEmpty())
	{
		clrText = theApp.GetMainSyntaxColors()->GetColor(COLORINDEX_NORMALTEXT);
		clrBk = theApp.GetMainSyntaxColors()->GetColor(COLORINDEX_BKGND);
	}
	else if (di.diffcode.isResultFiltered())
	{
		clrText = m_cachedColors.clrDirItemFilteredText;
		clrBk = m_cachedColors.clrDirItemFiltered;
	}
	else if (!IsItemExistAll(GetDiffContext(), di))
	{
		clrText = m_cachedColors.clrDirItemNotExistAllText;
		clrBk = m_cachedColors.clrDirItemNotExistAll;
	}
	else if (di.diffcode.isResultDiff())
	{
		clrText = m_cachedColors.clrDirItemDiffText;
		clrBk = m_cachedColors.clrDirItemDiff;
	}
	else if (di.diffcode.isResultSame())
	{
		clrText = m_cachedColors.clrDirItemEqualText;
		clrBk = m_cachedColors.clrDirItemEqual;
	}
	else
	{
		clrText = theApp.GetMainSyntaxColors()->GetColor(COLORINDEX_NORMALTEXT);
		clrBk = theApp.GetMainSyntaxColors()->GetColor(COLORINDEX_BKGND);
	}

	// Merge20 (S2, design section 3.2, spec section 5.1): tint the row
	// background by review state so the state is visible at a glance.
	// Only the background is tinted; the text color from the difference
	// status above is kept, so both pieces of information stay visible.
	// Fixed colors for now; theme (ColorSchemes) support is a later step.
	switch (GetDiffContext().GetMerge20ReviewState(di.getItemRelativePath()))
	{
	case ReviewState::SPECIAL:
		clrBk = RGB(255, 200, 200);
		break;
	case ReviewState::HOLD:
		clrBk = RGB(255, 255, 180);
		break;
	case ReviewState::KEEP:
		clrBk = RGB(230, 230, 230);
		break;
	case ReviewState::TODO:
	case ReviewState::DONE:
	default:
		break;
	}
}

/**
 * @brief Drag files/directories from folder compare listing view.
 */
void CDirView::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult) 
{
	std::list<String> list;
	CopyPathnamesForDragAndDrop(SelBegin(), SelEnd(), std::back_inserter(list), GetDiffContext());
	String filesForDroping = strutils::join(list.begin(), list.end(), _T("\n")) + _T("\n");

	CSharedFile file(GMEM_DDESHARE | GMEM_MOVEABLE | GMEM_ZEROINIT);
	file.Write(filesForDroping.data(), static_cast<unsigned>((filesForDroping.length() + 1) * sizeof(tchar_t)));
	
	HGLOBAL hMem = GlobalReAlloc(file.Detach(), (filesForDroping.length() + 1) * sizeof(tchar_t), 0);
	if (hMem != nullptr) 
	{
		COleDataSource* DropData = new COleDataSource();
		DropData->CacheGlobalData(CF_UNICODETEXT, hMem);
		DROPEFFECT de = DropData->DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_MOVE, nullptr);
	}

	*pResult = 0;
}

void CDirView::OnStatusBarClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	LPNMMOUSE pNMMouse = reinterpret_cast<LPNMMOUSE>(pNMHDR);
	int index = static_cast<int>(pNMMouse->dwItemSpec);
	switch (index)
	{
	case 0:
		break;
	case 1:
	{
		GetMainFrame()->SelectFilter();
		break;
	}
	case 2:
	{
		CPoint point;
		::GetCursorPos(&point);
		CMenu menu;
		VERIFY(menu.LoadMenu(IDR_POPUP_DIRVIEW_COMPAREMETHOD));
		I18n::TranslateMenu(menu.m_hMenu);
		menu.GetSubMenu(0)->CheckMenuRadioItem(ID_DIFF_OPTIONS_COMPMETHOD_FULL_CONTENTS, ID_DIFF_OPTIONS_COMPMETHOD_EXISTENCE, 
			ID_DIFF_OPTIONS_COMPMETHOD_FULL_CONTENTS + GetOptionsMgr()->GetInt(OPT_CMP_METHOD), MF_BYCOMMAND);
		int nID = menu.GetSubMenu(0)->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
		if (nID != 0)
		{
			GetOptionsMgr()->SaveOption(OPT_CMP_METHOD, nID - ID_DIFF_OPTIONS_COMPMETHOD_FULL_CONTENTS);
			m_pSavedUIState.reset(SaveUIState());
			m_pSavedTreeState.reset(SaveTreeState(GetDiffContext()));
			GetDocument()->Rescan();
		}
		break;
	}
	case 3:
	case 4:
	case 5:
	{
		const int idx = std::clamp(index - 3, 0, GetDocument()->m_nDirs - 1);
		GetDocument()->SetReadOnly(idx, !GetDocument()->GetReadOnly(idx));
		break;
	}
	default:
		break;
	}
}

void CDirView::OnViewDisplayFilterBarApply()
{
	CWaitCursor waitstatus;
	auto* pFilterBar = GetParentFrame()->GetFilterBar();
	pFilterBar->SaveFilterText();
	m_dirfilter.displayFilterHelper.SetMaskOrExpression(pFilterBar->GetFilterText());
	if (!m_dirfilter.displayFilterHelper.GetMaskOrExpression().empty() && m_dirfilter.displayFilterHelper.GetErrorList().empty())
		pFilterBar->SetFilterApplied(true);
	Redisplay();
	SetFocus();
}

void CDirView::OnViewDisplayFilterBar()
{
	CDirFrame* pFrame = GetParentFrame();
	pFrame->ShowFilterBar();
	auto* pFilterBar = pFrame->GetFilterBar();
	if (!m_dirfilter.displayFilterHelper.IsEmpty())
		pFilterBar->SetDlgItemText(IDC_FILTERFILE_MASK, m_dirfilter.displayFilterHelper.GetMaskOrExpression());
	pFilterBar->GetDlgItem(IDC_FILTERFILE_MASK)->SetFocus();
}

/// Assign column name, using string resource & current column ordering
void CDirView::NameColumn(const DirColInfo *col, int subitem)
{
	int phys = m_pColItems->ColLogToPhys(subitem);
	if (phys>=0)
	{
		String s = col->GetDisplayName();
		LV_COLUMN lvc;
		lvc.mask = LVCF_TEXT;
		lvc.pszText = const_cast<tchar_t*>(s.c_str());
		m_pList->SetColumn(phys, &lvc);
	}
}

/// Load column names from string table
void CDirView::UpdateColumnNames()
{
	int ncols = m_pColItems->GetColCount();
	for (int i=0; i<ncols; ++i)
	{
		const DirColInfo* col = m_pColItems->GetDirColInfo(i);
		if (col)
			NameColumn(col, i);
	}
}

/**
 * @brief Set alignment of columns.
 */
void CDirView::SetColAlignments()
{
	int ncols = m_pColItems->GetColCount();
	for (int i=0; i<ncols; ++i)
	{
		const DirColInfo * col = m_pColItems->GetDirColInfo(i);
		if (col)
		{
			LVCOLUMN lvc;
			lvc.mask = LVCF_FMT;
			lvc.fmt = col->alignment;
			m_pList->SetColumn(m_pColItems->ColLogToPhys(i), &lvc);
		}
	}
}

CDirView::CompareState::CompareState(const CDiffContext *pCtxt, const DirViewColItems *pColItems, int sortCol, bool bSortAscending, bool bTreeMode)
: pCtxt(pCtxt)
, pColItems(pColItems)
, sortCol(sortCol)
, bSortAscending(bSortAscending)
, bTreeMode(bTreeMode)
{
}

/// Compare two specified rows during a sort operation (windows callback)
int CALLBACK CDirView::CompareState::CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CompareState *pThis = reinterpret_cast<CompareState*>(lParamSort);
	// Sort special items always first in dir view
	if (lParam1 == -1)
		return -1;
	if (lParam2 == -1)
		return 1;

	DIFFITEM *diffposl = (DIFFITEM *)lParam1;
	DIFFITEM *diffposr = (DIFFITEM *)lParam2;
	const DIFFITEM &ldi = pThis->pCtxt->GetDiffAt(diffposl);
	const DIFFITEM &rdi = pThis->pCtxt->GetDiffAt(diffposr);
	// compare 'left' and 'right' parameters as appropriate
	int retVal = pThis->pColItems->ColSort(pThis->pCtxt, pThis->sortCol, ldi, rdi, pThis->bTreeMode);
	// return compare result, considering sort direction
	return pThis->bSortAscending ? retVal : -retVal;
}

/// Add new item to list view
void CDirView::AddNewItem(int i, DIFFITEM *diffpos, int iImage, int iIndent)
{
	ListViewOwnerDataItem lvItem;
	lvItem.iIndent = iIndent;
	lvItem.lParam = (LPARAM)diffpos;
	lvItem.iImage = iImage;
	if (i == static_cast<int>(m_listViewItems.size()))
		m_listViewItems.push_back(lvItem);
	else
		m_listViewItems.insert(m_listViewItems.begin() + i, lvItem);
}

/**
 * @brief Update listview display of details for specified row
 * @note Customising shownd data should be done here
 */
void CDirView::UpdateDiffItemStatus(UINT nIdx)
{
	GetListCtrl().RedrawItems(nIdx, nIdx);
	const DIFFITEM& di = GetDiffItem(nIdx);
	if (di.diffcode.isDirectory())
	{
		DirItemIterator it;
		for (it = RevBegin(); it != RevEnd(); )
		{
			DIFFITEM& di2 = *it;
			int cursel = it.m_sel;
			++it;
			if (di2.IsAncestor(&di))
			{
				if ((di2.diffcode.diffcode & DIFFCODE::SIDEFLAGS) == 0)
					DeleteItem(cursel, true);
				else
					GetListCtrl().RedrawItems(cursel, cursel);
			}
		}
	}
}

static String rgDispinfoText[2]; // used in function below

/**
 * @brief Allocate a text buffer to assign to NMLVDISPINFO::item::pszText
 * Quoting from SDK Docs:
 *	If the LVITEM structure is receiving item text, the pszText and cchTextMax
 *	members specify the address and size of a buffer. You can either copy text to
 *	the buffer or assign the address of a string to the pszText member. In the
 *	latter case, you must not change or delete the string until the corresponding
 *	item text is deleted or two additional LVN_GETDISPINFO messages have been sent.
 */
static tchar_t* NTAPI AllocDispinfoText(const String &s)
{
	static int i = 0;
	const tchar_t* pszText = (rgDispinfoText[i] = s).c_str();
	i ^= 1;
	return (tchar_t*)pszText;
}

/**
 * @brief Respond to LVN_GETDISPINFO message
 */
void CDirView::ReflectGetdispinfo(NMLVDISPINFO *pParam)
{
	int nIdx = pParam->item.iItem;
	if (nIdx >= static_cast<int>(m_listViewItems.size()))
		return;
	DIFFITEM *key = reinterpret_cast<DIFFITEM*>(m_listViewItems[nIdx].lParam);
	int i = m_pColItems->ColPhysToLog(pParam->item.iSubItem);
	if (IsDiffItemSpecial(key))
	{
		pParam->item.iImage = m_listViewItems[nIdx].iImage;
		if (m_pColItems->IsColName(i))
		{
			pParam->item.pszText = _T("..");
		}
		return;
	}
	if (!GetDocument()->HasDiffs())
		return;
	const CDiffContext &ctxt = GetDiffContext();
	const DIFFITEM &di = ctxt.GetDiffAt(key);
	if (pParam->item.mask & LVIF_TEXT)
	{
		String s = m_pColItems->ColGetTextToDisplay(&ctxt, i, di);
		pParam->item.pszText = AllocDispinfoText(s);
	}
	if (pParam->item.mask & LVIF_IMAGE)
	{
		pParam->item.iImage = GetColImage(di);
		if ((pParam->item.mask & LVIF_STATE) == 0)
		{
			// for WinXP
			pParam->item.mask |= LVIF_STATE;
			pParam->item.state = m_pList->GetItemState(nIdx, static_cast<UINT>(~LVIS_STATEIMAGEMASK));
		}
	}
	if (pParam->item.mask & LVIF_INDENT)
	{
		pParam->item.iIndent = m_listViewItems[nIdx].iIndent;
	}
	if (pParam->item.mask & LVIF_STATE)
	{
		pParam->item.stateMask |= LVIS_STATEIMAGEMASK;
		if (di.HasChildren())
			pParam->item.state |= INDEXTOSTATEIMAGEMASK((di.customFlags & ViewCustomFlags::EXPANDED) ? 2 : 1);
	}
}

/**
 * @brief User examines & edits which columns are displayed in dirview, and in which order
 */
void CDirView::OnEditColumns()
{
	// Merge20 (Phase 4 D9): the column dialog edits the full layout.
	// Leave slim mode first so the dialog shows, and saves, the
	// user's real columns instead of the 3-column slim preset.
	if (m_bMerge20Slim)
		SetMerge20Slim(false);
	bool bReset = false;
	CDirColsDlg::ColumnArray cols;

	for (;;)
	{
		CDirColsDlg dlg;
		// List all the currently displayed columns
		for (int col=0; col<GetListCtrl().GetHeaderCtrl()->GetItemCount(); ++col)
		{
			int l = m_pColItems->ColPhysToLog(col);
			dlg.AddColumn(m_pColItems->GetColDisplayName(l), m_pColItems->GetColDescription(l), l, col);
		}
		// Now add all the columns not currently displayed
		int l=0;
		for (l=0; l<m_pColItems->GetColCount(); ++l)
		{
			if (m_pColItems->ColLogToPhys(l)==-1)
			{
				dlg.AddColumn(m_pColItems->GetColDisplayName(l), m_pColItems->GetColDescription(l), l);
			}
		}
		assert(m_pColItems->GetColCount() == dlg.GetColumns().size());

		// Add default order of columns for resetting to defaults
		for (l = 0; l < m_pColItems->GetColCount(); ++l)
		{
			int phy = m_pColItems->GetColDefaultOrder(l);
			dlg.AddDefColumn(m_pColItems->GetColDisplayName(l), l, phy);
		}

		if (dlg.DoModal() != IDOK)
			return;

		if (!dlg.GetShowAdditionalProperties())
		{
			bReset = dlg.m_bReset;
			cols = dlg.GetColumns();
			break;
		}
		
		CDirAdditionalPropertiesDlg dlgAdditionalProperties(m_pColItems->GetAdditionalPropertyNames());
		if (dlgAdditionalProperties.DoModal() == IDOK)
		{
			auto& selectedCanonicalNames = dlgAdditionalProperties.GetSelectedCanonicalNames();
			GetOptionsMgr()->SaveOption(OPT_ADDITIONAL_PROPERTIES,
				strutils::join(selectedCanonicalNames.begin(), selectedCanonicalNames.end(), _T(" ")));
			m_pColItems->SetAdditionalPropertyNames(selectedCanonicalNames);
			m_pColItems->SaveColumnOrders();
			GetDiffContext().m_pPropertySystem.reset(new PropertySystem(m_pColItems->GetAdditionalPropertyNames()));
			GetDiffContext().ClearAllAdditionalProperties();
			ReloadColumns();
		}
	} 

	const String keyname = GetDocument()->m_nDirs < 3 ? OPT_DIRVIEW_COLUMN_WIDTHS : OPT_DIRVIEW3_COLUMN_WIDTHS;
	GetOptionsMgr()->SaveOption(keyname,
		(bReset ? m_pColItems->ResetColumnWidths(GetDefColumnWidth()) :
		                m_pColItems->SaveColumnWidths(std::bind(&CListCtrl::GetColumnWidth, m_pList, _1))));

	// Reset our data to reflect the new data from the dialog
	m_pColItems->ClearColumnOrders();
	const int sortColumn = GetOptionsMgr()->GetInt((GetDocument()->m_nDirs < 3) ? OPT_DIRVIEW_SORT_COLUMN : OPT_DIRVIEW_SORT_COLUMN3);
	std::vector<int> colorder(m_pColItems->GetColCount(), -1);
	for (CDirColsDlg::ColumnArray::const_iterator iter = cols.begin();
		iter != cols.end(); ++iter)
	{
		int log = iter->log_col; 
		int phy = iter->phy_col;
		colorder[log] = phy;

		// If sorted column was hidden, reset sorting
		if (log == sortColumn && phy < 0)
		{
			GetOptionsMgr()->Reset((GetDocument()->m_nDirs < 3) ? OPT_DIRVIEW_SORT_COLUMN : OPT_DIRVIEW_SORT_COLUMN3);
			GetOptionsMgr()->Reset(OPT_DIRVIEW_SORT_ASCENDING);
		}
	}

	m_pColItems->SetColumnOrdering(&colorder[0]);

	if (m_pColItems->GetDispColCount() < 1)
	{
		// Set them back to default if they didn't leave a column showing
		// (However, if none of the items are checked, this process will not be executed because the "OK" button in the "Display Columns" dialog cannot be pressed.)
		m_pColItems->ResetColumnOrdering();
	}
	ReloadColumns();
	Redisplay();
}

DirActions CDirView::MakeDirActions(DirActions::method_type func) const
{
	const CDirDoc *pDoc = GetDocument();
	return DirActions(pDoc->GetDiffContext(), pDoc->GetReadOnly(), func);
}

DirActions CDirView::MakeDirActions(DirActions::method_type2 func) const
{
	const CDirDoc *pDoc = GetDocument();
	return DirActions(pDoc->GetDiffContext(), pDoc->GetReadOnly(), nullptr, func);
}

const CDiffContext& CDirView::GetDiffContext() const
{
	return GetDocument()->GetDiffContext();
}

CDiffContext& CDirView::GetDiffContext()
{
	return GetDocument()->GetDiffContext();
}
