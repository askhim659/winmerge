// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20AuditDoc.cpp
 *
 * @brief Adapter between the folder/file compare documents and the
 * UI-independent audit module (Merge20Audit). See Merge20AuditDoc.h.
 *
 * The log file lives next to the session file (same directory, same
 * folder-pair hash, prefix `Merge20Audit_`); the operator id is
 * the learning source id `u-<GUID>` (Merge20LearnGetOwnerId,
 * created once on first use and then stable). The stored values
 * are codes; the display mapping here turns them into the same
 * words the folder list, the focus dock and the B1 conclusion
 * menu already show, so the viewer, the CSV and the list never
 * disagree.
 */
#include "StdAfx.h"
#include "Merge20/Merge20AuditDoc.h"
#include "Merge20/Merge20Audit.h"
#include "Merge20/Merge20LearnDoc.h"
#include "Merge20/Merge20SessionDoc.h"
#include "DiffContext.h"
#include "DirDoc.h"
#include "I18n.h"
#include "paths.h"

namespace
{

	// "include OFF->ON": the output flag a review-state change or
	// a conclusion also moved (S2 / B1 linkage). Stored as codes;
	// the display mapping below turns it into words.
	String IncludeNote(bool oldInclude, bool newInclude)
	{
		if (oldInclude == newInclude)
			return String();
		return _T("include ") + Merge20AuditIncludeCode(oldInclude) + _T("->") + Merge20AuditIncludeCode(newInclude);
	}

	String NowString()
	{
		return String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d %H:%M:%S")).GetString());
	}

	// Display text of one stored value, by kind: review codes
	// become the S1/S2 words, conclusion codes the B1 words,
	// "ON"/"OFF" stay as codes, split/regroup counts become
	// "1 diff" / "N rows".
	String ValueDisplayName(Merge20AuditKind kind, const String& code, bool isOld)
	{
		switch (kind)
		{
		case Merge20AuditKind::ReviewState:
		case Merge20AuditKind::ChangeReset:
		{
			ReviewState state = ReviewState::TODO;
			if (Merge20AuditReviewFromCode(code, state))
				return Merge20AuditReviewDisplayName(state);
			return code;
		}
		case Merge20AuditKind::DiffConclusion:
		case Merge20AuditKind::LearnAccept:
		case Merge20AuditKind::LearnDismiss:
		{
			Merge20DiffConclusion conclusion = Merge20DiffConclusion::Undecided;
			if (Merge20AuditConclusionFromCode(code, conclusion))
				return Merge20AuditConclusionDisplayName(conclusion);
			return code;
		}
		case Merge20AuditKind::SplitDiff:
		case Merge20AuditKind::RegroupDiff:
		{
			// Split: old is "1", new is the row count. Regroup:
			// the reverse. Both directions read the same way:
			// "1" is one diff, anything else is that many rows.
			if (code == _T("1"))
				return _("1 diff");
			const String* args[] = { &code };
			return strutils::format_strings(_("%1 rows"), args, 1);
		}
		case Merge20AuditKind::IncludeInOutput:
		case Merge20AuditKind::BatchCheck:
		default:
			(void)isOld;
			return Merge20AuditIncludeDisplayName(code == _T("ON"));
		}
	}

	// Display text of one note segment ("include OFF->ON",
	// "via exact", "candidate UseNew"); segments are joined with
	// "; " in the stored note.
	String NoteSegmentDisplayName(const String& segment)
	{
		static const String includePrefix = _T("include ");
		if (segment.rfind(includePrefix, 0) == 0)
		{
			const String rest = segment.substr(includePrefix.size());
			const size_t arrow = rest.find(_T("->"));
			if (arrow != String::npos)
			{
				const String oldFlag = rest.substr(0, arrow);
				const String newFlag = rest.substr(arrow + 2);
				const String* args[] = { &oldFlag, &newFlag };
				return strutils::format_strings(_("Output: %1 to %2"), args, 2);
			}
			return segment;
		}
		static const String viaPrefix = _T("via ");
		if (segment.rfind(viaPrefix, 0) == 0)
			return Merge20LearnViaDisplayName(segment.substr(viaPrefix.size()));
		static const String candidatePrefix = _T("candidate ");
		if (segment.rfind(candidatePrefix, 0) == 0)
		{
			Merge20DiffConclusion conclusion = Merge20DiffConclusion::Undecided;
			const String code = segment.substr(candidatePrefix.size());
			const String name = Merge20AuditConclusionFromCode(code, conclusion) ?
				Merge20AuditConclusionDisplayName(conclusion) : code;
			const String* args[] = { &name };
			return strutils::format_strings(_("Candidate: %1"), args, 1);
		}
		return segment;
	}

} // namespace

String Merge20AuditGetFilePath(const CDiffContext& ctxt)
{
	const int nDirs = ctxt.GetCompareDirs();
	return Merge20AuditGetFilePath(
		ctxt.GetNormalizedPath(0),
		nDirs > 1 ? ctxt.GetNormalizedPath(1) : String(),
		nDirs > 2 ? ctxt.GetNormalizedPath(2) : String());
}

String Merge20AuditGetFilePath(const String& leftPath, const String& rightPath, const String& thirdPath)
{
	const String fileName = _T("Merge20Audit_") + Merge20SessionPairHashHex(leftPath, rightPath, thirdPath) + _T(".json");
	return paths::ConcatPath(Merge20SessionGetDirectory(), fileName);
}

bool Merge20AuditAppend(CDiffContext& ctxt, const Merge20AuditEntry& entry)
{
	Merge20AuditEntry filled = entry;
	filled.time = NowString();
	filled.operatorId = Merge20LearnGetOwnerId();
	std::string errDetail;
	return Merge20AuditAppendToFile(filled, Merge20AuditGetFilePath(ctxt), errDetail);
}

bool Merge20AuditLogReviewChange(CDiffContext& ctxt, const String& relPath,
	ReviewState oldReview, ReviewState newReview, bool oldInclude, bool newInclude)
{
	Merge20AuditEntry entry;
	entry.relPath = relPath;
	entry.kind = Merge20AuditKind::ReviewState;
	entry.oldValue = Merge20AuditReviewCode(oldReview);
	entry.newValue = Merge20AuditReviewCode(newReview);
	entry.note = IncludeNote(oldInclude, newInclude);
	if (!Merge20AuditEntryHasChange(entry))
		return false;
	return Merge20AuditAppend(ctxt, entry);
}

bool Merge20AuditLogIncludeChange(CDiffContext& ctxt, const String& relPath,
	bool oldInclude, bool newInclude, Merge20AuditKind kind)
{
	Merge20AuditEntry entry;
	entry.relPath = relPath;
	entry.kind = kind;
	entry.oldValue = Merge20AuditIncludeCode(oldInclude);
	entry.newValue = Merge20AuditIncludeCode(newInclude);
	if (!Merge20AuditEntryHasChange(entry))
		return false;
	return Merge20AuditAppend(ctxt, entry);
}

bool Merge20AuditLogConclusion(CDiffContext& ctxt, const String& relPath, int diffIndex,
	Merge20DiffConclusion oldConclusion, Merge20DiffConclusion newConclusion,
	ReviewState oldReview, ReviewState newReview, bool oldInclude, bool newInclude)
{
	Merge20AuditEntry entry;
	entry.relPath = relPath;
	entry.kind = Merge20AuditKind::DiffConclusion;
	entry.oldValue = Merge20AuditConclusionCode(oldConclusion);
	entry.newValue = Merge20AuditConclusionCode(newConclusion);
	entry.diffIndex = diffIndex;
	// The B1 linkage may also have moved the review state and the
	// output flag; both go into the note as codes.
	String note;
	if (oldReview != newReview)
		note = _T("review ") + Merge20AuditReviewCode(oldReview) + _T("->") + Merge20AuditReviewCode(newReview);
	const String includeNote = IncludeNote(oldInclude, newInclude);
	if (!includeNote.empty())
	{
		if (!note.empty())
			note += _T("; ");
		note += includeNote;
	}
	entry.note = note;
	if (!Merge20AuditEntryHasChange(entry))
		return false;
	return Merge20AuditAppend(ctxt, entry);
}

bool Merge20AuditLogLearn(CDiffContext& ctxt, const String& relPath, int diffIndex,
	Merge20AuditKind kind,
	Merge20DiffConclusion oldConclusion, Merge20DiffConclusion newConclusion,
	ReviewState oldReview, ReviewState newReview, bool oldInclude, bool newInclude,
	const String& note)
{
	Merge20AuditEntry entry;
	entry.relPath = relPath;
	entry.kind = kind;
	entry.oldValue = Merge20AuditConclusionCode(oldConclusion);
	entry.newValue = Merge20AuditConclusionCode(newConclusion);
	entry.diffIndex = diffIndex;
	// The acceptance linkage may also have moved the review state
	// and the output flag; both go into the note as codes, after
	// the candidate note the caller passed ("via exact" /
	// "candidate UseNew").
	String fullNote = note;
	if (oldReview != newReview)
	{
		if (!fullNote.empty())
			fullNote += _T("; ");
		fullNote += _T("review ") + Merge20AuditReviewCode(oldReview) + _T("->") + Merge20AuditReviewCode(newReview);
	}
	const String includeNote = IncludeNote(oldInclude, newInclude);
	if (!includeNote.empty())
	{
		if (!fullNote.empty())
			fullNote += _T("; ");
		fullNote += includeNote;
	}
	entry.note = fullNote;
	// A dismissal is always a change (the candidate disappearing
	// is the change); Merge20AuditEntryHasChange knows that.
	if (!Merge20AuditEntryHasChange(entry))
		return false;
	return Merge20AuditAppend(ctxt, entry);
}

bool Merge20AuditLogSplit(CDiffContext& ctxt, const String& relPath, int diffIndex,
	Merge20AuditKind kind, int rows)
{
	Merge20AuditEntry entry;
	entry.relPath = relPath;
	entry.kind = kind;
	entry.diffIndex = diffIndex;
	if (kind == Merge20AuditKind::SplitDiff)
	{
		entry.oldValue = _T("1");
		entry.newValue = strutils::to_str(rows);
	}
	else
	{
		entry.oldValue = strutils::to_str(rows);
		entry.newValue = _T("1");
	}
	if (!Merge20AuditEntryHasChange(entry))
		return false;
	return Merge20AuditAppend(ctxt, entry);
}

bool Merge20AuditLogChangeReset(CDirDoc& doc, const String& relPath, ReviewState savedReview)
{
	CDiffContext& ctxt = doc.GetDiffContext();
	Merge20AuditEntry entry;
	entry.relPath = relPath;
	entry.kind = Merge20AuditKind::ChangeReset;
	entry.oldValue = Merge20AuditReviewCode(savedReview);
	entry.newValue = Merge20AuditReviewCode(ReviewState::TODO);
	if (!Merge20AuditEntryHasChange(entry))
		return false;
	return Merge20AuditAppend(ctxt, entry);
}

bool Merge20AuditLoadForDoc(CDirDoc& doc, std::vector<Merge20AuditEntry>& entries, int& skippedLines, String& filePath)
{
	entries.clear();
	skippedLines = 0;
	filePath.clear();
	if (!doc.HasDiffs())
		return false;
	filePath = Merge20AuditGetFilePath(doc.GetDiffContext());
	std::string errDetail;
	return Merge20AuditLoadFromFile(filePath, entries, skippedLines, errDetail);
}

bool Merge20AuditExportCsv(const std::vector<Merge20AuditEntry>& entries, const String& filePath, String& errStr, int& exportedCount)
{
	exportedCount = 0;
	errStr.clear();
	const std::vector<Merge20AuditCsvRow> rows = Merge20AuditBuildDisplayRows(entries);
	std::string errDetail;
	if (!Merge20AuditWriteCsvFile(rows, Merge20AuditCsvHeaders(), filePath, errDetail))
	{
		String errMsg;
		Merge20Utf8Decode(errDetail, errMsg);
		errStr = strutils::format_string1(_("Error creating the report:\n%1"), errMsg);
		return false;
	}
	exportedCount = static_cast<int>(rows.size());
	return true;
}

String Merge20AuditKindDisplayName(Merge20AuditKind kind, int diffIndex /*= -1*/)
{
	String name;
	switch (kind)
	{
	case Merge20AuditKind::ReviewState: name = _("Review state change"); break;
	case Merge20AuditKind::IncludeInOutput: name = _("Output toggle"); break;
	case Merge20AuditKind::DiffConclusion: name = _("Diff conclusion"); break;
	case Merge20AuditKind::BatchCheck: name = _("Batch check"); break;
	case Merge20AuditKind::LearnAccept: name = _("Learning candidate accepted"); break;
	case Merge20AuditKind::LearnDismiss: name = _("Learning candidate dismissed"); break;
	case Merge20AuditKind::SplitDiff: name = _("Split diff"); break;
	case Merge20AuditKind::RegroupDiff: name = _("Regroup diff"); break;
	case Merge20AuditKind::ChangeReset: name = _("Reset to unreviewed (file changed)"); break;
	default: name = _("Review state change"); break;
	}
	if (diffIndex >= 0)
	{
		const String diffNo = strutils::to_str(diffIndex + 1);
		const String* args[] = { &diffNo };
		name += _T(" ");
		name += strutils::format_strings(_("Diff %1"), args, 1);
	}
	return name;
}

String Merge20AuditReviewDisplayName(ReviewState state)
{
	// The S1/S2 short forms (DirViewColItems.cpp, DirView.cpp):
	// the same words the folder list shows.
	switch (state)
	{
	case ReviewState::DONE: return _("Done");
	case ReviewState::HOLD: return _("On hold");
	case ReviewState::SPECIAL: return _("Needs manual work");
	case ReviewState::KEEP: return _("Keep as is");
	case ReviewState::TODO:
	default: return _("Not reviewed");
	}
}

String Merge20AuditConclusionDisplayName(Merge20DiffConclusion conclusion)
{
	// The B1 words (Merge20FocusDock.cpp, Src/Merge.rc): the same
	// words the conclusion menu and the focus dock show.
	switch (conclusion)
	{
	case Merge20DiffConclusion::UseOld: return _("Adopt old lines");
	case Merge20DiffConclusion::UseNew: return _("Adopt new lines");
	case Merge20DiffConclusion::UseBoth: return _("Adopt both");
	case Merge20DiffConclusion::Reject: return _("Reject this diff");
	case Merge20DiffConclusion::Keep: return _("Keep as is");
	case Merge20DiffConclusion::Undecided:
	default: return _("Undecided");
	}
}

String Merge20AuditIncludeDisplayName(bool include)
{
	return Merge20AuditIncludeCode(include);
}

String Merge20AuditNoteDisplayName(const String& note)
{
	if (note.empty())
		return String();
	// Segments are joined with "; " in the stored note; each
	// segment is mapped on its own. A "review TODO->DONE" segment
	// (from a conclusion / acceptance linkage) maps the same way
	// as the include segment, with the review words.
	std::vector<String> segments;
	size_t pos = 0;
	while (pos <= note.size())
	{
		size_t sep = note.find(_T("; "), pos);
		if (sep == String::npos)
			sep = note.size();
		segments.push_back(note.substr(pos, sep - pos));
		if (sep >= note.size())
			break;
		pos = sep + 2;
	}
	String out;
	for (size_t i = 0; i < segments.size(); ++i)
	{
		const String& segment = segments[i];
		static const String reviewPrefix = _T("review ");
		String display;
		if (segment.rfind(reviewPrefix, 0) == 0)
		{
			const String rest = segment.substr(reviewPrefix.size());
			const size_t arrow = rest.find(_T("->"));
			if (arrow != String::npos)
			{
				ReviewState oldState = ReviewState::TODO, newState = ReviewState::TODO;
				const String oldCode = rest.substr(0, arrow);
				const String newCode = rest.substr(arrow + 2);
				if (Merge20AuditReviewFromCode(oldCode, oldState) &&
					Merge20AuditReviewFromCode(newCode, newState))
				{
					const String oldName = Merge20AuditReviewDisplayName(oldState);
					const String newName = Merge20AuditReviewDisplayName(newState);
					const String* args[] = { &oldName, &newName };
					display = strutils::format_strings(_("Review: %1 to %2"), args, 2);
				}
				else
				{
					display = segment;
				}
			}
			else
			{
				display = segment;
			}
		}
		else
		{
			display = NoteSegmentDisplayName(segment);
		}
		if (i > 0)
			out += _T("; ");
		out += display;
	}
	return out;
}

Merge20AuditCsvRow Merge20AuditBuildDisplayRow(const Merge20AuditEntry& entry)
{
	Merge20AuditCsvRow row;
	row.time = entry.time;
	row.file = entry.relPath;
	row.kind = Merge20AuditKindDisplayName(entry.kind, entry.diffIndex);
	row.oldValue = ValueDisplayName(entry.kind, entry.oldValue, true);
	row.newValue = ValueDisplayName(entry.kind, entry.newValue, false);
	row.operatorId = entry.operatorId;
	row.note = Merge20AuditNoteDisplayName(entry.note);
	return row;
}

std::vector<Merge20AuditCsvRow> Merge20AuditBuildDisplayRows(const std::vector<Merge20AuditEntry>& entries)
{
	std::vector<Merge20AuditCsvRow> rows;
	rows.reserve(entries.size());
	for (const Merge20AuditEntry& entry : entries)
		rows.push_back(Merge20AuditBuildDisplayRow(entry));
	return rows;
}

std::vector<String> Merge20AuditCsvHeaders()
{
	return {
		_("Date/time"), _("File"), _("Operation"),
		_("Old value"), _("New value"), _("Operator"), _("Remarks"),
	};
}
