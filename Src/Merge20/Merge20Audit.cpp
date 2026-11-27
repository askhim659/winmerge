// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Audit.cpp
 *
 * @brief Implementation of the append-only audit log (Merge20
 * best-workflow B11).
 *
 * This file stays ASCII: it stores codes and paths only and never
 * formats user-visible text. The JSON writer here is compact
 * (single-line) on purpose: the shared writer
 * (Merge20JsonStringify) pretty-prints, which would break the
 * one-line-per-entry layout of the log file. Parsing reuses the
 * shared parser (Merge20JsonParse), so writer and reader agree
 * on the escaping rules.
 */
#include "pch.h"
#include <filesystem>
#include <fstream>
#include "Merge20/Merge20Audit.h"
#include "Merge20/Merge20Csv.h"
#include "Merge20/Merge20Session.h"

namespace
{

	// Compact JSON string writer for one field value: the value is
	// escaped first (as wide text), then encoded to UTF-8 in one
	// go, so lone surrogates become U+FFFD exactly like the
	// session writer does.
	String EscapeJsonField(const String& value)
	{
		String out;
		out += _T('"');
		for (tchar_t ch : value)
		{
			switch (ch)
			{
			case _T('"'): out += _T("\\\""); break;
			case _T('\\'): out += _T("\\\\"); break;
			case _T('\b'): out += _T("\\b"); break;
			case _T('\f'): out += _T("\\f"); break;
			case _T('\n'): out += _T("\\n"); break;
			case _T('\r'): out += _T("\\r"); break;
			case _T('\t'): out += _T("\\t"); break;
			default:
				if (ch < 0x20)
				{
					static const tchar_t hex[] = _T("0123456789abcdef");
					out += _T("\\u00");
					out += hex[(ch >> 4) & 0xF];
					out += hex[ch & 0xF];
				}
				else
				{
					out += ch;
				}
				break;
			}
		}
		out += _T('"');
		return out;
	}

	// Text of a JSON object member, or the default when the member
	// is absent or not text (tolerant read: a missing field is not
	// a reason to drop the whole entry).
	String MemberText(const Merge20JsonValue& obj, const String& key, const String& defaultValue)
	{
		const Merge20JsonValue* pValue = obj.Find(key);
		return pValue != nullptr ? pValue->AsText(defaultValue) : defaultValue;
	}

} // namespace

String Merge20AuditKindName(Merge20AuditKind kind)
{
	switch (kind)
	{
	case Merge20AuditKind::ReviewState: return _T("ReviewState");
	case Merge20AuditKind::IncludeInOutput: return _T("IncludeInOutput");
	case Merge20AuditKind::DiffConclusion: return _T("DiffConclusion");
	case Merge20AuditKind::BatchCheck: return _T("BatchCheck");
	case Merge20AuditKind::LearnAccept: return _T("LearnAccept");
	case Merge20AuditKind::LearnDismiss: return _T("LearnDismiss");
	case Merge20AuditKind::SplitDiff: return _T("SplitDiff");
	case Merge20AuditKind::RegroupDiff: return _T("RegroupDiff");
	case Merge20AuditKind::ChangeReset: return _T("ChangeReset");
	default: return _T("ReviewState");
	}
}

bool Merge20AuditKindFromName(const String& name, Merge20AuditKind& out)
{
	if (name == _T("ReviewState")) { out = Merge20AuditKind::ReviewState; return true; }
	if (name == _T("IncludeInOutput")) { out = Merge20AuditKind::IncludeInOutput; return true; }
	if (name == _T("DiffConclusion")) { out = Merge20AuditKind::DiffConclusion; return true; }
	if (name == _T("BatchCheck")) { out = Merge20AuditKind::BatchCheck; return true; }
	if (name == _T("LearnAccept")) { out = Merge20AuditKind::LearnAccept; return true; }
	if (name == _T("LearnDismiss")) { out = Merge20AuditKind::LearnDismiss; return true; }
	if (name == _T("SplitDiff")) { out = Merge20AuditKind::SplitDiff; return true; }
	if (name == _T("RegroupDiff")) { out = Merge20AuditKind::RegroupDiff; return true; }
	if (name == _T("ChangeReset")) { out = Merge20AuditKind::ChangeReset; return true; }
	return false;
}

String Merge20AuditReviewCode(ReviewState state)
{
	switch (state)
	{
	case ReviewState::TODO: return _T("TODO");
	case ReviewState::DONE: return _T("DONE");
	case ReviewState::HOLD: return _T("HOLD");
	case ReviewState::SPECIAL: return _T("SPECIAL");
	case ReviewState::KEEP: return _T("KEEP");
	default: return _T("TODO");
	}
}

bool Merge20AuditReviewFromCode(const String& code, ReviewState& out)
{
	if (code == _T("TODO")) { out = ReviewState::TODO; return true; }
	if (code == _T("DONE")) { out = ReviewState::DONE; return true; }
	if (code == _T("HOLD")) { out = ReviewState::HOLD; return true; }
	if (code == _T("SPECIAL")) { out = ReviewState::SPECIAL; return true; }
	if (code == _T("KEEP")) { out = ReviewState::KEEP; return true; }
	return false;
}

String Merge20AuditConclusionCode(Merge20DiffConclusion conclusion)
{
	switch (conclusion)
	{
	case Merge20DiffConclusion::Undecided: return _T("Undecided");
	case Merge20DiffConclusion::UseOld: return _T("UseOld");
	case Merge20DiffConclusion::UseNew: return _T("UseNew");
	case Merge20DiffConclusion::UseBoth: return _T("UseBoth");
	case Merge20DiffConclusion::Reject: return _T("Reject");
	case Merge20DiffConclusion::Keep: return _T("Keep");
	default: return _T("Undecided");
	}
}

bool Merge20AuditConclusionFromCode(const String& code, Merge20DiffConclusion& out)
{
	if (code == _T("Undecided")) { out = Merge20DiffConclusion::Undecided; return true; }
	if (code == _T("UseOld")) { out = Merge20DiffConclusion::UseOld; return true; }
	if (code == _T("UseNew")) { out = Merge20DiffConclusion::UseNew; return true; }
	if (code == _T("UseBoth")) { out = Merge20DiffConclusion::UseBoth; return true; }
	if (code == _T("Reject")) { out = Merge20DiffConclusion::Reject; return true; }
	if (code == _T("Keep")) { out = Merge20DiffConclusion::Keep; return true; }
	return false;
}

String Merge20AuditIncludeCode(bool include)
{
	return include ? _T("ON") : _T("OFF");
}

bool Merge20AuditEntryHasChange(const Merge20AuditEntry& entry)
{
	switch (entry.kind)
	{
	case Merge20AuditKind::LearnDismiss:
		// A dismissal changes nothing but the candidate itself:
		// the candidate disappearing is the change, even when
		// the conclusion did not move (same rule as the B10 undo
		// entry for a dismissal).
		return true;
	case Merge20AuditKind::SplitDiff:
	case Merge20AuditKind::RegroupDiff:
		// A split / regroup always moves at least one diff into
		// (or out of) per-row hunks; the row count is in the
		// values, so any such entry is a change.
		return true;
	default:
		return entry.oldValue != entry.newValue || !entry.note.empty();
	}
}

bool Merge20AuditEntryToJson(const Merge20AuditEntry& entry, std::string& outUtf8)
{
	outUtf8.clear();
	// Fixed field order (see the header): time, file, kind, old,
	// new, operator, note, diff. The compact writer keeps the
	// entry on one line, which is what makes the log append-only.
	String line;
	line += _T("{\"time\":");
	line += EscapeJsonField(entry.time);
	line += _T(",\"file\":");
	line += EscapeJsonField(entry.relPath);
	line += _T(",\"kind\":");
	line += EscapeJsonField(Merge20AuditKindName(entry.kind));
	line += _T(",\"old\":");
	line += EscapeJsonField(entry.oldValue);
	line += _T(",\"new\":");
	line += EscapeJsonField(entry.newValue);
	line += _T(",\"operator\":");
	line += EscapeJsonField(entry.operatorId);
	line += _T(",\"note\":");
	line += EscapeJsonField(entry.note);
	line += _T(",\"diff\":");
	line += strutils::to_str(entry.diffIndex);
	line += _T('}');
	outUtf8 = Merge20Utf8Encode(line);
	return true;
}

bool Merge20AuditEntryFromJson(const std::string& lineUtf8, Merge20AuditEntry& out, std::string& errDetail)
{
	errDetail.clear();
	Merge20JsonValue root;
	if (!Merge20JsonParse(lineUtf8, root, errDetail))
		return false;
	if (root.type != Merge20JsonValue::Type::Object)
	{
		errDetail = "audit entry is not a JSON object";
		return false;
	}
	const Merge20JsonValue* pKind = root.Find(_T("kind"));
	if (pKind == nullptr)
	{
		errDetail = "audit entry has no kind";
		return false;
	}
	Merge20AuditKind kind = Merge20AuditKind::ReviewState;
	if (!Merge20AuditKindFromName(pKind->AsText(String()), kind))
	{
		errDetail = "audit entry has an unknown kind";
		return false;
	}
	Merge20AuditEntry entry;
	entry.time = MemberText(root, _T("time"), String());
	entry.relPath = MemberText(root, _T("file"), String());
	entry.kind = kind;
	entry.oldValue = MemberText(root, _T("old"), String());
	entry.newValue = MemberText(root, _T("new"), String());
	entry.operatorId = MemberText(root, _T("operator"), String());
	entry.note = MemberText(root, _T("note"), String());
	const Merge20JsonValue* pDiff = root.Find(_T("diff"));
	entry.diffIndex = pDiff != nullptr ? static_cast<int>(pDiff->AsInt(-1)) : -1;
	out = std::move(entry);
	return true;
}

bool Merge20AuditAppendToFile(const Merge20AuditEntry& entry, const String& filePath, std::string& errDetail)
{
	errDetail.clear();
	std::string lineUtf8;
	if (!Merge20AuditEntryToJson(entry, lineUtf8))
	{
		errDetail = "failed to serialize audit entry";
		return false;
	}
	std::ofstream out(std::filesystem::path(filePath), std::ios::binary | std::ios::app);
	if (!out)
	{
		errDetail = "cannot open audit log file";
		return false;
	}
	out.write(lineUtf8.data(), static_cast<std::streamsize>(lineUtf8.size()));
	out.put('\n');
	out.flush();
	if (!out)
	{
		errDetail = "failed to write audit log file";
		return false;
	}
	return true;
}

bool Merge20AuditLoadFromFile(const String& filePath, std::vector<Merge20AuditEntry>& out, int& skippedLines, std::string& errDetail)
{
	out.clear();
	skippedLines = 0;
	errDetail.clear();
	std::ifstream in(std::filesystem::path(filePath), std::ios::binary);
	if (!in)
	{
		errDetail = "cannot open audit log file";
		return false;
	}
	std::string utf8((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	size_t pos = 0;
	while (pos < utf8.size())
	{
		size_t eol = utf8.find('\n', pos);
		if (eol == std::string::npos)
			eol = utf8.size();
		std::string line = utf8.substr(pos, eol - pos);
		pos = (eol < utf8.size()) ? eol + 1 : utf8.size();
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty())
			continue;
		Merge20AuditEntry entry;
		std::string parseErr;
		if (!Merge20AuditEntryFromJson(line, entry, parseErr))
		{
			// A torn last line (crash mid-write) or a hand-edited
			// bad line costs that one entry, never the whole log.
			++skippedLines;
			continue;
		}
		out.push_back(std::move(entry));
	}
	return true;
}

String Merge20AuditFormatCsvRow(const Merge20AuditCsvRow& row)
{
	const std::vector<String> fields =
	{
		Merge20CsvEscapeField(row.time),
		Merge20CsvEscapeField(row.file),
		Merge20CsvEscapeField(row.kind),
		Merge20CsvEscapeField(row.oldValue),
		Merge20CsvEscapeField(row.newValue),
		Merge20CsvEscapeField(row.operatorId),
		Merge20CsvEscapeField(row.note),
	};
	return strutils::join(fields.begin(), fields.end(), String(_T(",")));
}

String Merge20AuditBuildCsvText(const std::vector<Merge20AuditCsvRow>& rows, const std::vector<String>& headers)
{
	// Header line joined bare (as in the S6 writer): none of the
	// headers contains a comma, a quote or a newline.
	String text = strutils::join(headers.begin(), headers.end(), String(_T(",")));
	for (const Merge20AuditCsvRow& row : rows)
	{
		text += _T("\r\n");
		text += Merge20AuditFormatCsvRow(row);
	}
	return text;
}

bool Merge20AuditWriteCsvFile(const std::vector<Merge20AuditCsvRow>& rows, const std::vector<String>& headers, const String& filePath, std::string& errDetail)
{
	errDetail.clear();
	const std::string utf8 = Merge20Utf8Encode(Merge20AuditBuildCsvText(rows, headers));
	std::ofstream out(std::filesystem::path(filePath), std::ios::binary | std::ios::trunc);
	if (!out)
	{
		errDetail = "cannot create audit CSV file";
		return false;
	}
	// BOM-prefixed UTF-8 (S6 rule): without the BOM, Excel opens
	// the Japanese text garbled.
	static const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
	out.write(reinterpret_cast<const char*>(bom), 3);
	out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
	out.flush();
	if (!out)
	{
		errDetail = "failed to write audit CSV file";
		return false;
	}
	return true;
}
