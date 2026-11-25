/**
 * @file  Merge20FileStates.h
 *
 * @brief Sidecar map holding one Merge20FileState per compared file.
 *
 * Merge20 (S1, design sections 2.1 and 2.4): the map is keyed by the
 * relative path (DIFFITEM::getItemRelativePath()), so it survives a rescan
 * (DIFFITEM objects are rebuilt, paths are not). Files with no entry are
 * treated as freshly defaulted (review = TODO); nothing is applied
 * automatically (spec section 4(c)).
 */
#pragma once

#include <map>
#include "UnicodeString.h"
#include "Merge20Types.h"

class Merge20FileStates
{
public:
	/** @brief Return the stored state, or a default (TODO) state if the file has no entry yet. */
	const Merge20FileState& GetState(const String& relPath) const;
	/** @brief Return the stored state, creating a default (TODO) entry if needed. */
	Merge20FileState& GetOrCreateState(const String& relPath);
	/** @brief Return the review state (TODO when the file has no entry yet). */
	ReviewState GetReviewState(const String& relPath) const;
	/** @brief Set the review state of one file, keeping the rest of its state. */
	void SetReviewState(const String& relPath, ReviewState state);
	/** @brief True when the file already has an entry (S2: distinguishes a stored output flag from the initial value). */
	bool HasState(const String& relPath) const;
	/** @brief Return the output checkbox flag (true when the file has no entry yet; the initial value itself is computed by the caller from DIFFITEM). */
	bool GetIncludeInOutput(const String& relPath) const;
	/** @brief Set the output checkbox flag of one file, keeping the rest of its state. */
	void SetIncludeInOutput(const String& relPath, bool include);
	/** @brief Return the audit memo (empty when the file has no entry yet). */
	String GetMemo(const String& relPath) const;
	/** @brief Set the audit memo of one file, keeping the rest of its state. */
	void SetMemo(const String& relPath, const String& memo);

private:
	std::map<String, Merge20FileState> m_states;
	static const Merge20FileState s_defaultState;
};
