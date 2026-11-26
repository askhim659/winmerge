#pragma once

#include "UnicodeString.h"
#include "Merge20/Merge20Types.h"

class PathContext;
struct IMergeDoc;
class CTempPathContext;
struct FileLocation;

struct IDirDoc
{
	virtual void InitCompare(const PathContext& paths, bool bRecursive, CTempPathContext*) = 0;
	virtual void SetHiddenItems(const std::vector<String>& hiddenItems) = 0;
	virtual void SetReportFile(const String& sReportFile) = 0;
	virtual void SetDescriptions(const String strDesc[]) = 0;
	virtual void SetTitle(const tchar_t* pszTitle) = 0;
	virtual void SetReadOnly(int nIndex, bool bRO) = 0;
	virtual void Rescan() = 0;
	virtual void AddMergeDoc(IMergeDoc* pMergeDoc) = 0;
	virtual void MergeDocClosing(IMergeDoc* pMergeDoc) = 0;
	virtual bool CloseMergeDocs() = 0;
	virtual bool IsArchiveFolders() const = 0;
	virtual bool HasDiffs() const = 0;
	virtual void UpdateChangedItem(const PathContext& paths, unsigned nDiffs, unsigned nTrivialDiffs, bool bIdentical) = 0;
	// Merge20 (S4): store the classification of one file (action
	// category, risk, rule-based summary) computed by its merge doc
	// after a rescan. `paths` are the merge doc's file paths, used to
	// find the folder-compare item.
	virtual void SetMerge20Classification(const PathContext& paths, const Merge20FileState& state) = 0;
	// Merge20 (Phase 4 D2): review state of one file, found by the
	// merge doc's file paths (same lookup as SetMerge20Classification).
	// The dedicated-mode bar's review buttons use these: a file compare
	// opened from a folder compare applies the state to its own folder
	// item (S2's ApplyMerge20ReviewState rule, incl. the output-flag
	// linkage). Default implementations do nothing and report TODO, so
	// existing IDirDoc users are unaffected; CDirDoc overrides them.
	virtual ReviewState GetMerge20ReviewState(const PathContext& paths) const { return ReviewState::TODO; }
	virtual void ApplyMerge20ReviewState(const PathContext& paths, ReviewState state) {}
	// Merge20 (Phase 4 D3): position of the merge doc's file in the
	// folder-compare list, for the dedicated bar's file name + (n/m)
	// display. `paths` is the merge doc's file paths (same lookup as
	// SetMerge20Classification). `bTodoOnly` selects the patrol set:
	// false counts all displayed files, true counts only the
	// unreviewed (TODO) files; `nPos` is then the 1-based ordinal
	// among that set, or 0 when the file is not in it (e.g. TODO-only
	// is on but the file is already reviewed). Returns false when
	// there is no folder compare (a file compare opened on its own)
	// or the file is not a displayed file row. Default implementation
	// reports no position, so existing IDirDoc users are unaffected;
	// CDirDoc overrides it.
	virtual bool GetMerge20FilePosition(const PathContext& paths, int& nPos, int& nTotal, bool bTodoOnly) const { return false; }
	virtual void ApplyDisplayRoot(int nIndex, String& sText) = 0;
	virtual bool CompareFilesIfFilesAreLarge(int nFiles, const FileLocation ifileloc[]) = 0;
	virtual bool MoveableToNextDiff() = 0;
	virtual bool MoveableToPrevDiff() = 0;
	virtual void MoveToPrevDiff(IMergeDoc* pMergeDoc) = 0;
	virtual void MoveToNextDiff(IMergeDoc* pMergeDoc) = 0;
	virtual void MoveToFirstFile(IMergeDoc* pMergeDoc) = 0;
	virtual void MoveToLastFile(IMergeDoc* pMergeDoc) = 0;
	virtual void MoveToNextFile(IMergeDoc* pMergeDoc) = 0;
	virtual void MoveToPrevFile(IMergeDoc* pMergeDoc) = 0;
	virtual bool IsFirstFile() = 0;
	virtual bool IsLastFile() = 0;
};

