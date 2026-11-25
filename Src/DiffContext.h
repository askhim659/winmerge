/**
 *  @file DiffContext.h
 *
 *  @brief Declarations of CDiffContext and diff structures
 */
#pragma once

#define POCO_NO_UNWINDOWS 1
#include <Poco/Mutex.h>
#include <memory>
#include "PathContext.h"
#include "DiffItemList.h"
#include "FilterList.h"
#include "SubstitutionList.h"
#include "PropertySystem.h"
#include "Merge20/Merge20FileStates.h"

class PackingInfo;
class PrediffingInfo;
class IDiffFilter;
class CompareStats;
class IAbortable;
class CDiffWrapper;
class CompareOptions;
class RenameMoveDetection;
struct FilterExpression;
struct DIFFOPTIONS;

/** Interface to a provider of plugin info */
class IPluginInfos
{
public:
	virtual void FetchPluginInfos(const String& filteredFilenames, 
                                      PackingInfo ** infoUnpacker, 
                                      PrediffingInfo ** infoPrediffer) = 0;
};

/** Information on the number of duplicate hash values */
struct DuplicateInfo
{
	int groupid;
	int count[3];
	bool nonpaired;
};

/**
 * The folder compare context.
 * This class holds data of the current folder compare. There are paths
 * to compare, filters used, compare options etc. And compare results list
 * is also contained in this class. Many compare classes and functions have
 * a pointer to instance of this class. 
 *
 * @note If you add new member variables, remember to copy values in
 * CDiffContext::CDiffContext(..,CDiffContext) constructor!
 */
class CDiffContext : public DiffItemList
{
public:
	/** @brief Special values for difference counts. */
	enum
	{
		DIFFS_UNKNOWN = -1, /**< Difference count unknown (generally). */
		DIFFS_UNKNOWN_QUICKCOMPARE = -9, /**< Unknown because of quick-compare method. */
	};

	CDiffContext(const PathContext & paths, int compareMethod);
	~CDiffContext();

	void UpdateVersion(DIFFITEM &di, int nIndex) const;

	/**
	 * Get the main compare method used in this compare.
	 * @return Compare method used.
	 */
	int GetCompareMethod(void) const { return m_nCompMethod; }

	//@{
	/**
	 * @name Path accessor functions.
	 *
	 * These functions return left/right path associated to DiffContext.
	 * There is no setter fuctions and path can be set only via constructor.
	 * Normalized paths are preferred to use - short paths are expanded
	 * and trailing slashes removed (except from root path).
	 */
	/**
	 * Get left-side compare path.
	 * @return full path in left-side.
	 */
	String GetLeftPath() const { return m_paths.GetLeft(false); }
	String GetMiddlePath() const { return m_paths.GetMiddle(false); }
	/**
	 * Get right-side compare path.
	 * @return full path in right-side.
	 */
	String GetRightPath() const { return m_paths.GetRight(false); }
	String GetPath(int nIndex) const { return m_paths.GetPath(nIndex, false); }
	/**
	 * Get left-side compare path in normalized form.
	 * @return full path in left-side.
	 */
	String GetNormalizedLeft() const { return m_paths.GetLeft(); }
	String GetNormalizedMiddle() const { return m_paths.GetMiddle(); }
	/**
	 * Get right-side compare path in normalized form.
	 * @return full path in left-side.
	 */
	String GetNormalizedRight() const { return m_paths.GetRight(); }
	String GetNormalizedPath(int nIndex) const { return m_paths.GetPath(nIndex, true); }
	PathContext GetNormalizedPaths() const
	{
		PathContext paths;
		for (int nIndex = 0; nIndex < m_paths.GetSize(); nIndex++)
			paths.SetPath(nIndex, m_paths.GetPath(nIndex, true));
		return paths;
	}
	//@}

	// change an existing difference
	bool UpdateInfoFromDiskHalf(DIFFITEM &di, int nIndex);
	void UpdateStatusFromDisk(DIFFITEM *diffpos, int nIndex);

	bool CreateCompareOptions(int compareMethod, const DIFFOPTIONS & options);
	CompareOptions * GetCompareOptions(int compareMethod);

	// retrieve or manufacture plugin info for specified file comparison
	void FetchPluginInfos(const String& filteredFilenames,
		PackingInfo ** infoUnpacker, PrediffingInfo ** infoPrediffer);

	//@{
	/**
	 * @name Compare aborting interface.
	 * These functions handle compare aborting using IAbortable interface.
	 */
	bool ShouldAbort() const;

	/**
	 * Set pointer to IAbortable interface.
	 * This function sets pointer to interface used to abort the compare when
	 * user wants to.
	 * @param [in] piAbortable Pointer to interface.
	 */
	void SetAbortable(IAbortable * piAbortable) { m_piAbortable = piAbortable; }

	/**
	 * Returns a pointer to current IAbortable interface.
	 * This function returns a pointer to interface used to abort the compare.
	 * @return Pointer to current IAbortable interface.
	 */
	const IAbortable * GetAbortable() const { return m_piAbortable; }
	//@}

	int GetCompareDirs() const { return m_paths.GetSize(); }

	/**
	 * @name Merge20 per-file work state (S1).
	 * The review state (and later the output checkbox, action category,
	 * risk, summary and memo) lives in a sidecar map keyed by relative
	 * path, not in DIFFITEM (DiffItem.h forbids GUI state there). The map
	 * lives here, in CDiffContext, rather than in CDirDoc as design
	 * section 2.2 first sketched it: column getters only receive a
	 * `const CDiffContext *` plus a DIFFITEM, and CDiffContext has no
	 * back-pointer to CDirDoc, so a CDirDoc member would be unreachable
	 * from the "Review State" column. CDirDoc owns the single CDiffContext
	 * (m_pCtxt, reused across rescans), so the lifetime is the same.
	 * CDirDoc exposes the same accessors for the key handling in S2.
	 */
	/** @brief Review state of one file (TODO when it has no entry yet). */
	ReviewState GetMerge20ReviewState(const String& relPath) const;
	/** @brief Set the review state of one file. */
	void SetMerge20ReviewState(const String& relPath, ReviewState state);
	/**
	 * @brief Set the review state of one item.
	 * Unlike the path-based overload above, this one also records the
	 * initial output flag (see GetMerge20IncludeInOutput) when the file
	 * gets its first entry, so the stored flag never falls back to the
	 * struct default (true) for a NEW_ONLY or identical file.
	 */
	void SetMerge20ReviewState(const DIFFITEM& di, ReviewState state);
	/**
	 * @brief Output checkbox flag of one file: the stored flag when the
	 * file has an entry, otherwise the initial value from its existence
	 * category (design section 3.2: MODIFIED / OLD_ONLY start ON,
	 * NEW_ONLY / IDENTICAL start OFF).
	 */
	bool GetMerge20IncludeInOutput(const DIFFITEM& di) const;
	/** @brief Set the output checkbox flag of one file. */
	void SetMerge20IncludeInOutput(const DIFFITEM& di, bool include);
	/**
	 * @brief Set the review state of one file and apply the output-flag
	 * linkage (spec section 4(b), HTML setRev/cycRev): DONE turns the
	 * output flag back ON except for NEW_ONLY files (2-way right-only),
	 * HOLD and KEEP turn it OFF, TODO and SPECIAL leave it unchanged.
	 */
	void ApplyMerge20ReviewState(const DIFFITEM& di, ReviewState state);
	/** @brief Direct access to the sidecar map (for S2 and later slices). */
	Merge20FileStates& GetMerge20FileStates() { return m_merge20FileStates; }
	/** @brief Direct access to the sidecar map (const version). */
	const Merge20FileStates& GetMerge20FileStates() const { return m_merge20FileStates; }

	void Swap(int idx1, int idx2)
	{
		String tmp;
		tmp = m_paths.GetPath(idx1);
		m_paths.SetPath(idx1, m_paths.GetPath(idx2));
		m_paths.SetPath(idx2, tmp);
		DiffItemList::Swap(idx1, idx2);
	}

	const DIFFOPTIONS *GetOptions() const { return m_pOptions.get(); }

	void GetComparePaths(const DIFFITEM& di, PathContext& tFiles) const;
	String GetFilteredFilenames(const DIFFITEM& di) const;
	static String GetFilteredFilenames(const PathContext& paths) { return strutils::join(paths.begin(), paths.end(), _T("|")); }
	void CreateDuplicateValueMap();

	IDiffFilter * m_piFilterGlobal; /**< Interface for file filtering. */
	IDiffFilter * m_pImgfileFilter; /**< Interface for image file filtering */
	IPluginInfos * m_piPluginInfos;
	int m_iGuessEncodingType;

	bool m_bIgnoreSmallTimeDiff; /**< Ignore small timedifferences when comparing by date */
	CompareStats *m_pCompareStats; /**< Pointer to compare statistics */

	/**
	 * Optimize compare by stopping after first difference.
	 * In some compare methods (currently quick compare) we can stop the
	 * compare right after finding the first difference. This speeds up the
	 * compare, but also causes compare statistics to be inaccurate.
	 */
	bool m_bStopAfterFirstDiff;

	/**
	 * Threshold size for switching to quick compare.
	 * When diffutils compare is selected, files bigger (in bytes) than this
	 * value are compared using Quick compare. This is because diffutils simply
	 * cannot compare large files. And large files are usually binary files.
	 */
	int m_nQuickCompareLimit;

	int m_nBinaryCompareLimit;

	/**
	 * Walk into unique folders and add contents.
	 * This enables/disables walking into unique folders. If we don't walk into
	 * unique folders, they are shown as such in folder compare results. If we
	 * walk into unique folders, we'll show all files in the unique folder and
	 * in possible subfolders.
	 *
	 * This value is true by default.
	 */
	bool m_bWalkUniques;
	bool m_bIgnoreReparsePoints;
	bool m_bIgnoreCodepage;
	bool m_bEnableImageCompare;
	double m_dColorDistanceThreshold;
	bool m_bPreferWICDecoder;

	bool m_bRecursive; /**< Do we include subfolders to compare? */
	bool m_bPluginsEnabled; /**< Are plugins enabled? */
	std::shared_ptr<FilterList> m_pFilterList; /**< Filter list for line filters */
	std::shared_ptr<SubstitutionList> m_pSubstitutionList; /// list for Substitution Filters
	std::unique_ptr<PropertySystem> m_pPropertySystem; /**< pointer to Property System */
	std::vector<std::map<std::vector<uint8_t>, DuplicateInfo>> m_duplicateValues; /**< Number of duplicate hash values */
	std::vector<String> m_vCurrentlyHiddenItems; /**< The list of currently hidden items */
	Merge20FileStates m_merge20FileStates; /**< Merge20 per-file work state (review state etc.), keyed by relative path; see GetMerge20ReviewState(). */
	std::unique_ptr<FilterExpression> m_pAdditionalCompareExpression; /** Additional compare condition applied in folder comparison */
	std::unique_ptr<RenameMoveDetection> m_pRenameMoveDetection; /** Move detection object */

private:
	/**
	 * The main compare method used.
	 * This is the main compare method set when compare is started. There
	 * can be temporary switches to other method (e.g. for large file) but
	 * this main method must be set back for next file.
	 */
	int m_nCompMethod;

	std::unique_ptr<DIFFOPTIONS> m_pOptions; /**< Generalized compare options. */
	std::unique_ptr<CompareOptions> m_pContentCompareOptions; /**< Per compare method compare options. */
	std::unique_ptr<CompareOptions> m_pQuickCompareOptions;   /**< Per compare method compare options. */
	PathContext m_paths; /**< (root) paths for this context */
	IAbortable *m_piAbortable; /**< Interface for aborting the compare. */
	Poco::FastMutex m_mutex;
};
