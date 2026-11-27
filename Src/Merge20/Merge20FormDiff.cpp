/**
 * @file  Merge20FormDiff.cpp
 *
 * @brief Implementation of the FORM/XML matching and classification
 * (Wave 5 F1, design sections 5.1-5.4).
 *
 * This file stays ASCII: the Japanese `desc` / `impact` literals
 * are written with \u escapes (see the note in Merge20FormDiff.h).
 * The conversion from the authored UTF-8 text is mechanical; the
 * escaped form is what is committed.
 */
#include "pch.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include "Merge20/Merge20FormDiff.h"

namespace
{

	// Lower-case ASCII letters only (enough for the `fontsize` /
	// `size` checks; non-ASCII letters are left unchanged).
	std::wstring ToLowerAscii(const std::wstring& text)
	{
		std::wstring lower = text;
		for (wchar_t& c : lower)
		{
			if (c >= L'A' && c <= L'Z')
				c = static_cast<wchar_t>(c - L'A' + L'a');
		}
		return lower;
	}

	bool IsXmlnsAttr(const std::wstring& attr)
	{
		return attr == L"xmlns" || attr.compare(0, 6, L"xmlns:") == 0;
	}

	// Coordinate attributes (HTML `getDetailInterpretation`).
	bool IsCoordAttr(const std::wstring& attr)
	{
		return attr == L"x1" || attr == L"x2" || attr == L"x" || attr == L"left" ||
			attr == L"width" || attr == L"y1" || attr == L"y2" || attr == L"y" ||
			attr == L"top" || attr == L"height";
	}

	// Horizontal coordinate attributes (HTML `getDetailInterpretation`).
	bool IsHorizontalAttr(const std::wstring& attr)
	{
		return attr == L"x1" || attr == L"x2" || attr == L"x" || attr == L"left" ||
			attr == L"width";
	}

	// Attribute signature of a nameless element: attributes in
	// sorted order, excluding `xmlns` / `xmlns:*` (same rule as F0).
	std::wstring BuildAttributeSignature(const Merge20FormElement& element)
	{
		std::wstring signature;
		bool first = true;
		for (const std::pair<const std::wstring, std::wstring>& attr : element.attrs)
		{
			if (IsXmlnsAttr(attr.first))
				continue;
			if (!first)
				signature += L",";
			first = false;
			signature += attr.first + L"=" + attr.second;
		}
		return signature;
	}

	// Parent structural path (`/form/page[1]/field[3]` -> `/form/page[1]`).
	std::wstring ParentPath(const std::wstring& structPath)
	{
		const size_t slash = structPath.rfind(L'/');
		if (slash == std::wstring::npos || slash == 0)
			return std::wstring();
		return structPath.substr(0, slash);
	}

	// Tag part of a stable key (`structPath|tag|name`).
	std::wstring TagFromStableKey(const std::wstring& stableKey)
	{
		const size_t first = stableKey.find(L'|');
		if (first == std::wstring::npos)
			return stableKey;
		const size_t second = stableKey.find(L'|', first + 1);
		if (second == std::wstring::npos)
			return stableKey.substr(first + 1);
		return stableKey.substr(first + 1, second - first - 1);
	}

	// Format a number with one decimal place at most (`12.5`, `12`).
	std::wstring FormatNumber(double value)
	{
		const double rounded = std::round(value * 10.0) / 10.0;
		if (rounded == std::trunc(rounded))
			return std::to_wstring(static_cast<long long>(rounded));
		wchar_t buf[64];
		std::swprintf(buf, 64, L"%.1f", rounded);
		return std::wstring(buf);
	}

	// Signed delta (`+12.5`, `-3`, `+0.5`).
	std::wstring FormatSigned(double value)
	{
		return (value < 0.0 ? std::wstring() : std::wstring(L"+")) + FormatNumber(value);
	}

	// Display name of an element: the `name` attribute, or the tag.
	std::wstring DisplayName(const Merge20FormElement& element)
	{
		return element.name.empty() ? element.tag : element.name;
	}

	// Parse a number strictly (both sides must be finite numbers).
	bool ParseFiniteNumber(const std::wstring& text, double& value)
	{
		if (!Merge20FormParseNumber(text, value))
			return false;
		return std::isfinite(value);
	}

	// Attribute maps equal, ignoring coordinate attributes (when
	// `exceptCoord`), the `name` attribute (when `exceptName`) and
	// `xmlns` declarations. Used for the move / rename detection.
	bool AttrsEqualExcept(const Merge20FormElement& oldEl, const Merge20FormElement& newEl,
		bool exceptCoord, bool exceptName)
	{
		for (const std::pair<const std::wstring, std::wstring>& attr : oldEl.attrs)
		{
			if (IsXmlnsAttr(attr.first))
				continue;
			if (exceptCoord && IsCoordAttr(attr.first))
				continue;
			if (exceptName && attr.first == L"name")
				continue;
			const std::map<std::wstring, std::wstring>::const_iterator it =
				newEl.attrs.find(attr.first);
			if (it == newEl.attrs.end() || it->second != attr.second)
				return false;
		}
		for (const std::pair<const std::wstring, std::wstring>& attr : newEl.attrs)
		{
			if (IsXmlnsAttr(attr.first))
				continue;
			if (exceptCoord && IsCoordAttr(attr.first))
				continue;
			if (exceptName && attr.first == L"name")
				continue;
			const std::map<std::wstring, std::wstring>::const_iterator it =
				oldEl.attrs.find(attr.first);
			if (it == oldEl.attrs.end())
				return false;
		}
		return true;
	}

	// Counts sentence (`added 2, deleted 1, changed/coordinate 4`).
	std::wstring CountsText(int addCount, int delCount, int modCount,
		int movedCount, int renamedCount)
	{
		std::vector<std::wstring> parts;
		if (addCount > 0)
			parts.push_back(L"\u8ffd\u52a0" + std::to_wstring(addCount) + L"\u4ef6");
		if (delCount > 0)
			parts.push_back(L"\u524a\u9664" + std::to_wstring(delCount) + L"\u4ef6");
		if (modCount > 0)
			parts.push_back(L"\u5909\u66f4/\u5ea7\u6a19" + std::to_wstring(modCount) + L"\u4ef6");
		if (movedCount > 0)
			parts.push_back(L"\u79fb\u52d5" + std::to_wstring(movedCount) + L"\u4ef6");
		if (renamedCount > 0)
			parts.push_back(L"\u6539\u540d" + std::to_wstring(renamedCount) + L"\u4ef6");
		std::wstring text;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			if (i > 0)
				text += L"\u3001";
			text += parts[i];
		}
		return text;
	}

} // namespace

std::wstring Merge20FormatFormDelta(double delta)
{
	return FormatSigned(delta) + L"\u5358\u4f4d";
}

Merge20FormInterpretation Merge20InterpretFormAttribute(
	const std::wstring& attr,
	const std::wstring& oldVal, const std::wstring& newVal,
	bool hasOld, bool hasNew)
{
	Merge20FormInterpretation out;

	// Coordinate / size attributes (HTML `getDetailInterpretation`).
	// A missing side or a non-numeric value falls through to the
	// generic wording below (the HTML tool leaves `desc` /
	// `impact` empty there).
	if (IsCoordAttr(attr) && hasOld && hasNew)
	{
		double oldNum = 0.0;
		double newNum = 0.0;
		if (ParseFiniteNumber(oldVal, oldNum) && ParseFiniteNumber(newVal, newNum))
		{
			const double diff = newNum - oldNum;
			out.isCoord = true;
			out.numericDiff = std::fabs(diff);
			if (attr == L"width")
			{
				out.desc = L"\u6a2a\u5e45\u304c " + FormatSigned(diff) + L" \u5358\u4f4d " +
					(diff > 0.0 ? L"\u62e1\u5927" : L"\u7e2e\u5c0f");
			}
			else if (attr == L"height")
			{
				out.desc = L"\u7e26\u5e45\u304c " + FormatSigned(diff) + L" \u5358\u4f4d " +
					(diff > 0.0 ? L"\u62e1\u5927" : L"\u7e2e\u5c0f");
			}
			else if (IsHorizontalAttr(attr))
			{
				out.desc = diff > 0.0
					? L"\u53f3\u65b9\u5411\u3078 " + FormatSigned(diff) + L" \u5358\u4f4d \u79fb\u52d5"
					: L"\u5de6\u65b9\u5411\u3078 " + FormatSigned(diff) + L" \u5358\u4f4d \u79fb\u52d5";
			}
			else
			{
				out.desc = diff > 0.0
					? L"\u4e0b\u65b9\u5411\u3078 " + FormatSigned(diff) + L" \u5358\u4f4d \u79fb\u52d5"
					: L"\u4e0a\u65b9\u5411\u3078 " + FormatSigned(diff) + L" \u5358\u4f4d \u79fb\u52d5";
			}
			if (IsHorizontalAttr(attr))
			{
				out.impact = diff > 0.0
					? L"\u7f6b\u7dda\u3084\u5370\u5b57\u9805\u76ee\u304c\u53f3\u3078\u30b7\u30d5\u30c8\u3057\u3001\u96a3\u63a5\u67a0\u3068\u306e\u5e72\u6e09\u3084\u53f3\u7aef\u4f59\u767d\u306e\u306f\u307f\u51fa\u3057\u306b\u6ce8\u610f"
					: L"\u5de6\u5074\u306e\u56fa\u5b9a\u30e9\u30d9\u30eb\u3084\u67a0\u7dda\u306b\u63a5\u8fd1\u30fb\u91cd\u306a\u308a\u304c\u767a\u751f\u3057\u3066\u3044\u306a\u3044\u304b\u5b9f\u6a5f\u5370\u5b57\u78ba\u8a8d\u3092\u63a8\u5968";
			}
			else
			{
				out.impact = diff > 0.0
					? L"\u4e0b\u65b9\u5411\u3078\u306e\u30ec\u30a4\u30a2\u30a6\u30c8\u79fb\u52d5\u306b\u3088\u308a\u3001\u4e0b\u90e8\u660e\u7d30\u884c\u3084\u30d5\u30c3\u30bf\u30fc\u67a0\u30fb\u6539\u30da\u30fc\u30b8\u4f4d\u7f6e\u3078\u306e\u5e72\u6e09\u30ea\u30b9\u30af\u3042\u308a"
					: L"\u4e0a\u65b9\u5411\u3078\u306e\u79fb\u52d5\u306b\u3088\u308a\u3001\u76f4\u4e0a\u306e\u898b\u51fa\u3057\u3084\u7f6b\u7dda\u3068\u306e\u884c\u9593\u8a70\u307e\u308a\u30fb\u6587\u5b57\u91cd\u306a\u308a\u306e\u53ef\u80fd\u6027\u3042\u308a";
			}
			return out;
		}
	}

	// Font size (HTML `getDetailInterpretation`). The attribute
	// name is compared case-insensitively, like the HTML tool.
	const std::wstring lowerAttr = ToLowerAscii(attr);
	if ((lowerAttr == L"point" || lowerAttr == L"fontsize" || lowerAttr == L"size") &&
		hasOld && hasNew)
	{
		double oldNum = 0.0;
		double newNum = 0.0;
		const bool numeric = ParseFiniteNumber(oldVal, oldNum) &&
			ParseFiniteNumber(newVal, newNum);
		out.numericDiff = numeric ? std::round(std::fabs(newNum - oldNum)) : 1.0;
		out.desc = L"\u30d5\u30a9\u30f3\u30c8\u30b5\u30a4\u30ba\u304c " + oldVal + L"pt \u2794 " + newVal + L"pt \u3078\u5909\u66f4";
		out.impact = (numeric && newNum > oldNum)
			? L"\u6587\u5b57\u62e1\u5927\u306b\u4f34\u3044\u3001\u6841\u6570\u304c\u591a\u3044\u30c7\u30fc\u30bf\u3067\u67a0\u5185\u304b\u3089\u306e\u6587\u5b57\u3042\u3075\u308c\u30fb\u610f\u56f3\u3057\u306a\u3044\u6298\u308a\u8fd4\u3057\u767a\u751f\u306e\u6050\u308c"
			: L"\u6587\u5b57\u7e2e\u5c0f\u306b\u3088\u308a\u67a0\u5185\u306e\u53ce\u307e\u308a\u306f\u5411\u4e0a\u3057\u307e\u3059\u304c\u3001\u5370\u5b57\u6642\u306e\u8996\u8a8d\u6027\u30fb\u53ef\u8aad\u6027\u306e\u5909\u5316\u306b\u6ce8\u610f";
		return out;
	}

	if (attr == L"version")
	{
		out.numericDiff = 1.0;
		out.desc = L"\u30d5\u30a9\u30fc\u30e0\u5b9a\u7fa9\u30d0\u30fc\u30b8\u30e7\u30f3\u304c " + oldVal + L" \u2794 " + newVal + L" \u3078\u66f4\u65b0";
		out.impact = L"SVFX-Designer\u7b49\u306e\u5b9a\u7fa9\u898f\u683c\u66f4\u65b0\u306b\u4f34\u3046\u5168\u4f53\u30d8\u30c3\u30c0\u5c5e\u6027\u306e\u5909\u66f4\uff08\u5370\u5b57\u30ec\u30a4\u30a2\u30a6\u30c8\u81ea\u4f53\u3078\u306e\u76f4\u63a5\u5f71\u97ff\u306f\u8efd\u5fae\uff09";
		return out;
	}

	// Generic attribute change (HTML `getDetailInterpretation`).
	// Unlike the HTML tool, a missing value is shown as
	// `(not set)` / `(deleted)` instead of the literal text
	// `undefined`.
	out.numericDiff = 1.0;
	const std::wstring oldShown = !hasOld ? std::wstring(L"(\u7a7a)")
		: (oldVal.empty() ? std::wstring(L"(\u7a7a)") : oldVal);
	const std::wstring newShown = hasNew ? newVal : std::wstring(L"(\u524a\u9664)");
	out.desc = L"\u5c5e\u6027\u5024\u300c" + attr + L"\u300d\u304c " + oldShown + L" \u2794 " + newShown + L" \u306b\u5909\u66f4";
	out.impact = L"\u66f8\u5f0f\u30d5\u30a9\u30fc\u30de\u30c3\u30c8\u3001\u7de8\u96c6\u30de\u30b9\u30af\u3001\u307e\u305f\u306f\u51fa\u529b\u6761\u4ef6\u30d7\u30ed\u30d1\u30c6\u30a3\u306e\u6319\u52d5\u304c\u5909\u5316\u3059\u308b\u53ef\u80fd\u6027\u3042\u308a";
	return out;
}

std::wstring Merge20BuildStableKey(const Merge20FormElement& element)
{
	if (!element.name.empty())
		return element.structPath + L"|" + element.tag + L"|" + element.name;
	return element.structPath + L"|" + element.tag + L"|" + BuildAttributeSignature(element);
}

std::wstring Merge20BuildGroupSummary(
	const Merge20FormFileResult& result,
	const Merge20FormDiffGroup& group)
{
	if (group.minor)
	{
		return L"\u5fae\u5c0f\u306a\u5dee " + std::to_wstring(group.recordIndices.size()) +
			L"\u4ef6\uff080.5\u5358\u4f4d\u4ee5\u4e0a1.0\u5358\u4f4d\u672a\u6e80\uff09";
	}
	// Most frequent tag of the grouped records.
	std::map<std::wstring, int> tagCounts;
	for (size_t index : group.recordIndices)
	{
		if (index >= result.diffRecords.size())
			continue;
		++tagCounts[TagFromStableKey(result.diffRecords[index].element)];
	}
	std::wstring tag;
	int best = 0;
	for (const std::pair<const std::wstring, int>& entry : tagCounts)
	{
		if (entry.second > best)
		{
			best = entry.second;
			tag = entry.first;
		}
	}
	std::wstring direction;
	if (group.dx > 0.0)
		direction += L"\u53f3\u306b" + FormatNumber(group.dx) + L"\u5358\u4f4d";
	if (group.dx < 0.0)
		direction += L"\u5de6\u306b" + FormatNumber(-group.dx) + L"\u5358\u4f4d";
	if (group.dy > 0.0)
	{
		if (!direction.empty())
			direction += L"\u30fb";
		direction += L"\u4e0b\u306b" + FormatNumber(group.dy) + L"\u5358\u4f4d";
	}
	if (group.dy < 0.0)
	{
		if (!direction.empty())
			direction += L"\u30fb";
		direction += L"\u4e0a\u306b" + FormatNumber(-group.dy) + L"\u5358\u4f4d";
	}
	if (direction.empty())
		direction = L"\u4f4d\u7f6e\u304c\u5909\u308f\u3089\u305a";
	return tag + L" " + std::to_wstring(group.recordIndices.size()) + L"\u9805\u76ee\u304c" +
		direction + L"\u79fb\u52d5";
}

std::wstring Merge20BuildFormFileSummary(
	const Merge20FormFileResult& result,
	const std::vector<Merge20FormDiffGroup>& groups)
{
	if (!groups.empty())
	{
		std::vector<std::wstring> parts;
		for (const Merge20FormDiffGroup& group : groups)
			parts.push_back(group.summary);
		// Ungrouped records, if any, are summarised by counts.
		int addCount = 0;
		int delCount = 0;
		int modCount = 0;
		int movedCount = 0;
		int renamedCount = 0;
		for (const Merge20FormDiffRecord& record : result.diffRecords)
		{
			if (!record.groupId.empty())
				continue;
			switch (record.type)
			{
			case Merge20FormDiffType::Add:
			case Merge20FormDiffType::FileAdd:
				++addCount;
				break;
			case Merge20FormDiffType::Del:
			case Merge20FormDiffType::FileDel:
				++delCount;
				break;
			case Merge20FormDiffType::Coord:
			case Merge20FormDiffType::Mod:
				++modCount;
				break;
			case Merge20FormDiffType::Moved:
				++movedCount;
				break;
			case Merge20FormDiffType::Renamed:
				++renamedCount;
				break;
			case Merge20FormDiffType::Error:
				break;
			}
		}
		const std::wstring rest = CountsText(addCount, delCount, modCount, movedCount, renamedCount);
		if (!rest.empty())
			parts.push_back(L"\u307b\u304b" + rest);
		std::wstring text;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			if (i > 0)
				text += L"\u3001";
			text += parts[i];
		}
		return text;
	}
	if (result.diffRecords.empty())
		return std::wstring();
	return CountsText(result.addCount, result.delCount, result.modCount,
		result.movedCount, result.renamedCount) + L"\u306e\u5909\u66f4\u304c\u3042\u308a\u307e\u3059";
}

Merge20FormSeverity Merge20ClassifyFormSeverity(const Merge20FormFileResult& result)
{
	for (const Merge20FormDiffRecord& record : result.diffRecords)
	{
		if (record.type == Merge20FormDiffType::FileAdd ||
			record.type == Merge20FormDiffType::FileDel ||
			record.type == Merge20FormDiffType::Error)
		{
			return Merge20FormSeverity::LayoutBreaking;
		}
	}
	if (result.isWarning)
		return Merge20FormSeverity::LayoutBreaking;
	for (const Merge20FormDiffRecord& record : result.diffRecords)
	{
		if (record.type == Merge20FormDiffType::Coord ||
			record.type == Merge20FormDiffType::Mod ||
			record.type == Merge20FormDiffType::Moved ||
			record.type == Merge20FormDiffType::Renamed)
		{
			return Merge20FormSeverity::VisualOnly;
		}
	}
	return Merge20FormSeverity::Info;
}

void Merge20ApplyLargeChangeFlag(std::vector<Merge20FormFileResult>& results)
{
	double sum = 0.0;
	int changedFiles = 0;
	for (const Merge20FormFileResult& result : results)
	{
		if (result.changeCount <= 0)
			continue;
		sum += std::round(result.totalNumericSum);
		++changedFiles;
	}
	if (changedFiles == 0)
		return;
	// The HTML tool rounds the average to an integer.
	const double average = std::round(sum / changedFiles);
	for (Merge20FormFileResult& result : results)
	{
		const double total = std::round(result.totalNumericSum);
		result.isWarning = total > average * 1.5 && total > 20.0;
		result.severity = Merge20ClassifyFormSeverity(result);
	}
}

namespace
{

	// Working state for one parsed file pair.
	struct FormDiffWork
	{
		const Merge20FormParseResult& oldParsed;
		const Merge20FormParseResult& newParsed;
		std::vector<std::wstring> oldKeys;
		std::vector<std::wstring> newKeys;
		std::unordered_map<std::wstring, size_t> oldByKey;
		std::unordered_map<std::wstring, size_t> newByKey;
		std::unordered_map<std::wstring, const Merge20FormElement*> oldByPath;
		std::unordered_map<std::wstring, const Merge20FormElement*> newByPath;
		std::unordered_map<std::wstring, const Merge20FormShape*> oldShapeByKey;
		std::unordered_map<std::wstring, const Merge20FormShape*> newShapeByKey;
		std::vector<char> oldUsed;
		std::vector<char> newUsed;
		Merge20FormDiffOutcome outcome;

		FormDiffWork(const Merge20FormParseResult& oldResult,
			const Merge20FormParseResult& newResult)
			: oldParsed(oldResult)
			, newParsed(newResult)
		{
			oldKeys.reserve(oldParsed.elements.size());
			newKeys.reserve(newParsed.elements.size());
			for (size_t i = 0; i < oldParsed.elements.size(); ++i)
			{
				oldKeys.push_back(Merge20BuildStableKey(oldParsed.elements[i]));
				oldByKey[oldKeys.back()] = i;
				oldByPath[oldParsed.elements[i].structPath] = &oldParsed.elements[i];
			}
			for (size_t i = 0; i < newParsed.elements.size(); ++i)
			{
				newKeys.push_back(Merge20BuildStableKey(newParsed.elements[i]));
				newByKey[newKeys.back()] = i;
				newByPath[newParsed.elements[i].structPath] = &newParsed.elements[i];
			}
			for (const Merge20FormShape& shape : oldParsed.shapes)
				oldShapeByKey[shape.key] = &shape;
			for (const Merge20FormShape& shape : newParsed.shapes)
				newShapeByKey[shape.key] = &shape;
			oldUsed.assign(oldParsed.elements.size(), 0);
			newUsed.assign(newParsed.elements.size(), 0);
		}

		const Merge20FormShape* OldShape(const Merge20FormElement& element) const
		{
			const std::unordered_map<std::wstring, const Merge20FormShape*>::const_iterator it =
				oldShapeByKey.find(element.key);
			return it != oldShapeByKey.end() ? it->second : nullptr;
		}

		const Merge20FormShape* NewShape(const Merge20FormElement& element) const
		{
			const std::unordered_map<std::wstring, const Merge20FormShape*>::const_iterator it =
				newShapeByKey.find(element.key);
			return it != newShapeByKey.end() ? it->second : nullptr;
		}

		// Display name of the parent element (`name` or tag).
		std::wstring ParentDisplay(const std::wstring& structPath, bool oldSide) const
		{
			const std::unordered_map<std::wstring, const Merge20FormElement*>& byPath =
				oldSide ? oldByPath : newByPath;
			const std::unordered_map<std::wstring, const Merge20FormElement*>::const_iterator it =
				byPath.find(ParentPath(structPath));
			if (it == byPath.end())
				return std::wstring(L"(\u30eb\u30fc\u30c8)");
			return DisplayName(*it->second);
		}
	};

	// One attribute change of a matched pair. Coordinate changes
	// below 0.5 are counted as ignored noise (section 5.2) instead
	// of producing a record.
	void CompareMatchedPair(FormDiffWork& work, size_t oldIndex, size_t newIndex,
		std::vector<Merge20FormDiffRecord>& attrRecords)
	{
		const Merge20FormElement& oldEl = work.oldParsed.elements[oldIndex];
		const Merge20FormElement& newEl = work.newParsed.elements[newIndex];
		const bool hasVisual = work.OldShape(oldEl) != nullptr || work.NewShape(newEl) != nullptr;
		const std::wstring elementKey = work.newKeys[newIndex];

		// Union of the attribute names, in sorted order.
		std::unordered_set<std::wstring> seen;
		std::vector<std::wstring> names;
		for (const std::pair<const std::wstring, std::wstring>& attr : oldEl.attrs)
		{
			if (seen.insert(attr.first).second)
				names.push_back(attr.first);
		}
		for (const std::pair<const std::wstring, std::wstring>& attr : newEl.attrs)
		{
			if (seen.insert(attr.first).second)
				names.push_back(attr.first);
		}
		std::sort(names.begin(), names.end());

		for (const std::wstring& attr : names)
		{
			const std::map<std::wstring, std::wstring>::const_iterator oldIt = oldEl.attrs.find(attr);
			const std::map<std::wstring, std::wstring>::const_iterator newIt = newEl.attrs.find(attr);
			const bool hasOld = oldIt != oldEl.attrs.end();
			const bool hasNew = newIt != newEl.attrs.end();
			if (hasOld && hasNew && oldIt->second == newIt->second)
				continue;

			// Noise filtering (section 5.2): a numeric coordinate
			// change below 0.5 is ignored, but counted.
			if (IsCoordAttr(attr) && hasOld && hasNew)
			{
				double oldNum = 0.0;
				double newNum = 0.0;
				if (ParseFiniteNumber(oldIt->second, oldNum) &&
					ParseFiniteNumber(newIt->second, newNum) &&
					std::fabs(newNum - oldNum) < kMerge20FormNoiseThreshold)
				{
					++work.outcome.ignoredNoiseCount;
					continue;
				}
			}

			const Merge20FormInterpretation interp = Merge20InterpretFormAttribute(attr,
				hasOld ? oldIt->second : std::wstring(),
				hasNew ? newIt->second : std::wstring(), hasOld, hasNew);
			Merge20FormDiffRecord record;
			record.type = interp.isCoord ? Merge20FormDiffType::Coord : Merge20FormDiffType::Mod;
			record.element = elementKey;
			record.attr = attr;
			record.oldVal = hasOld ? oldIt->second : std::wstring(L"(\u672a\u8a2d\u5b9a)");
			record.newVal = hasNew ? newIt->second : std::wstring(L"(\u524a\u9664)");
			record.diffVal = interp.numericDiff;
			record.desc = interp.desc;
			record.impact = interp.impact;
			record.hasVisual = hasVisual;
			attrRecords.push_back(record);
		}

		// Text content change (the HTML tool ignores element text;
		// F0 keeps `text` for this comparison).
		if (oldEl.text != newEl.text)
		{
			Merge20FormDiffRecord record;
			record.type = Merge20FormDiffType::Mod;
			record.element = elementKey;
			record.attr = L"\u30c6\u30ad\u30b9\u30c8";
			record.oldVal = oldEl.text.empty() ? std::wstring(L"(\u7a7a)") : oldEl.text;
			record.newVal = newEl.text.empty() ? std::wstring(L"(\u7a7a)") : newEl.text;
			record.diffVal = 1.0;
			record.desc = L"\u8868\u793a\u30c6\u30ad\u30b9\u30c8\u304c\u300c" + record.oldVal + L"\u300d\u304b\u3089\u300c" + record.newVal + L"\u300d\u306b\u5909\u66f4\u3055\u308c\u307e\u3057\u305f";
			record.impact = L"\u5370\u5b57\u5185\u5bb9\u304c\u5909\u308f\u308b\u305f\u3081\u3001\u51fa\u529b\u7d50\u679c\u306e\u78ba\u8a8d\u304c\u5fc5\u8981\u3067\u3059";
			record.hasVisual = hasVisual;
			attrRecords.push_back(record);
		}
	}

	// `Moved` record (section 5.1). The coordinate change, if any,
	// is folded into the same record.
	Merge20FormDiffRecord MakeMovedRecord(FormDiffWork& work,
		const Merge20FormElement& oldEl, const Merge20FormElement& newEl)
	{
		const Merge20FormShape* oldShape = work.OldShape(oldEl);
		const Merge20FormShape* newShape = work.NewShape(newEl);
		Merge20FormDiffRecord record;
		record.type = Merge20FormDiffType::Moved;
		record.element = Merge20BuildStableKey(newEl);
		record.attr = L"\u8981\u7d20\u79fb\u52d5";
		record.oldVal = oldEl.structPath;
		record.newVal = newEl.structPath;
		double dx = 0.0;
		double dy = 0.0;
		if (oldShape != nullptr && newShape != nullptr)
		{
			dx = newShape->x - oldShape->x;
			dy = newShape->y - oldShape->y;
		}
		record.diffVal = std::fabs(dx) + std::fabs(dy);
		record.desc = L"\u8981\u7d20\u300c" + DisplayName(oldEl) + L"\u300d\u304c\u300c" +
			work.ParentDisplay(oldEl.structPath, true) + L"\u300d\u304b\u3089\u300c" +
			work.ParentDisplay(newEl.structPath, false) + L"\u300d\u3078\u79fb\u52d5\u3057\u307e\u3057\u305f";
		if (oldShape != nullptr && newShape != nullptr)
		{
			// Only the axes that actually moved are shown.
			std::wstring coordPart;
			if (std::fabs(dx) >= kMerge20FormNoiseThreshold)
			{
				coordPart += L"x: " + FormatNumber(oldShape->x) + L" \u2192 " + FormatNumber(newShape->x);
			}
			if (std::fabs(dy) >= kMerge20FormNoiseThreshold)
			{
				if (!coordPart.empty())
					coordPart += L"\u3001";
				coordPart += L"y: " + FormatNumber(oldShape->y) + L" \u2192 " + FormatNumber(newShape->y);
			}
			if (!coordPart.empty())
				record.desc += L"\uff08" + coordPart + L"\uff09";
		}
		record.impact = L"\u79fb\u52d5\u5148\u3067\u65e2\u5b58\u9805\u76ee\u3068\u306e\u91cd\u306a\u308a\u3084\u30ec\u30a4\u30a2\u30a6\u30c8\u306e\u305a\u308c\u304c\u767a\u751f\u3057\u3066\u3044\u306a\u3044\u304b\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044";
		record.hasVisual = oldShape != nullptr || newShape != nullptr;
		return record;
	}

	// `Renamed` record (section 5.1). A simultaneous coordinate
	// change is folded into the same record.
	Merge20FormDiffRecord MakeRenamedRecord(FormDiffWork& work,
		const Merge20FormElement& oldEl, const Merge20FormElement& newEl)
	{
		const Merge20FormShape* oldShape = work.OldShape(oldEl);
		const Merge20FormShape* newShape = work.NewShape(newEl);
		Merge20FormDiffRecord record;
		record.type = Merge20FormDiffType::Renamed;
		record.element = Merge20BuildStableKey(newEl);
		record.attr = L"\u8981\u7d20\u6539\u540d";
		record.oldVal = oldEl.name.empty() ? std::wstring(L"(\u540d\u79f0\u306a\u3057)") : oldEl.name;
		record.newVal = newEl.name.empty() ? std::wstring(L"(\u540d\u79f0\u306a\u3057)") : newEl.name;
		double dx = 0.0;
		double dy = 0.0;
		if (oldShape != nullptr && newShape != nullptr)
		{
			dx = newShape->x - oldShape->x;
			dy = newShape->y - oldShape->y;
		}
		record.diffVal = std::fabs(dx) + std::fabs(dy);
		record.desc = L"\u8981\u7d20\u540d\u304c\u300c" + record.oldVal + L"\u300d\u304b\u3089\u300c" + record.newVal + L"\u300d\u306b\u5909\u66f4\u3055\u308c\u307e\u3057\u305f";
		if (oldShape != nullptr && newShape != nullptr)
		{
			// Only the axes that actually moved are shown.
			std::wstring coordPart;
			if (std::fabs(dx) >= kMerge20FormNoiseThreshold)
			{
				coordPart += L"x: " + FormatNumber(oldShape->x) + L" \u2192 " + FormatNumber(newShape->x);
			}
			if (std::fabs(dy) >= kMerge20FormNoiseThreshold)
			{
				if (!coordPart.empty())
					coordPart += L"\u3001";
				coordPart += L"y: " + FormatNumber(oldShape->y) + L" \u2192 " + FormatNumber(newShape->y);
			}
			if (!coordPart.empty())
				record.desc += L"\uff08" + coordPart + L"\uff09";
		}
		record.impact = L"\u540d\u79f0\u5909\u66f4\u306b\u3088\u308a\u3001\u5e33\u7968\u5b9a\u7fa9\u3092\u53c2\u7167\u3059\u308b\u51e6\u7406\u3084\u51fa\u529b\u6761\u4ef6\u3078\u306e\u5f71\u97ff\u304c\u306a\u3044\u304b\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044";
		record.hasVisual = oldShape != nullptr || newShape != nullptr;
		return record;
	}

	// Group the records of one file (section 5.3). Coordinated
	// moves (same parent, same direction and amount within 0.5)
	// become one group; leftover minor coordinate changes (0.5 to
	// below 1.0) become the `minor` group. A group is created only
	// for two or more coordinated records; the minor group is
	// created for one or more minor records.
	void GroupRecords(FormDiffWork& work)
	{
		Merge20FormFileResult& result = work.outcome.result;
		struct GroupWork
		{
			std::wstring key;
			std::vector<size_t> indices;
			double dx = 0.0;
			double dy = 0.0;
		};
		std::vector<GroupWork> groups;
		std::unordered_map<std::wstring, size_t> groupByKey;
		std::vector<char> grouped(result.diffRecords.size(), 0);

		// Quantise a move amount to the 0.5 grid (the noise
		// threshold, section 5.2) so that amounts within 0.5 of
		// each other share a group key.
		const auto quantise = [](double value) {
			return FormatNumber(std::round(value * 2.0) / 2.0);
		};

		for (size_t i = 0; i < result.diffRecords.size(); ++i)
		{
			const Merge20FormDiffRecord& record = result.diffRecords[i];
			double dx = 0.0;
			double dy = 0.0;
			std::wstring parent;
			if (record.type == Merge20FormDiffType::Coord)
			{
				// Size changes (width / height) are not moves.
				if (record.attr == L"width" || record.attr == L"height")
					continue;
				double oldNum = 0.0;
				double newNum = 0.0;
				if (!ParseFiniteNumber(record.oldVal, oldNum) ||
					!ParseFiniteNumber(record.newVal, newNum))
				{
					continue;
				}
				const double delta = newNum - oldNum;
				// Minor changes are deferred to the minor group.
				if (std::fabs(delta) >= kMerge20FormNoiseThreshold &&
					std::fabs(delta) < kMerge20FormMinorThreshold)
				{
					continue;
				}
				if (IsHorizontalAttr(record.attr))
					dx = delta;
				else
					dy = delta;
				const std::unordered_map<std::wstring, size_t>::const_iterator it =
					work.newByKey.find(record.element);
				if (it == work.newByKey.end())
					continue;
				parent = ParentPath(work.newParsed.elements[it->second].structPath);
			}
			else if (record.type == Merge20FormDiffType::Moved)
			{
				// A Moved record carries the old struct path in
				// `oldVal` and the new struct path in `newVal`.
				const std::unordered_map<std::wstring, const Merge20FormElement*>::const_iterator oldIt =
					work.oldByPath.find(record.oldVal);
				const std::unordered_map<std::wstring, const Merge20FormElement*>::const_iterator newIt =
					work.newByPath.find(record.newVal);
				if (oldIt == work.oldByPath.end() || newIt == work.newByPath.end())
					continue;
				const Merge20FormElement* oldEl = oldIt->second;
				const Merge20FormElement* newEl = newIt->second;
				const Merge20FormShape* oldShape = work.OldShape(*oldEl);
				const Merge20FormShape* newShape = work.NewShape(*newEl);
				if (oldShape == nullptr || newShape == nullptr)
					continue;
				dx = newShape->x - oldShape->x;
				dy = newShape->y - oldShape->y;
				parent = ParentPath(newEl->structPath);
			}
			else
			{
				continue;
			}
			const std::wstring key = parent + L"|" + quantise(dx) + L"|" + quantise(dy);
			const std::unordered_map<std::wstring, size_t>::const_iterator found = groupByKey.find(key);
			size_t groupIndex = 0;
			if (found == groupByKey.end())
			{
				groupIndex = groups.size();
				groupByKey[key] = groupIndex;
				GroupWork group;
				group.key = key;
				group.dx = dx;
				group.dy = dy;
				groups.push_back(group);
			}
			else
			{
				groupIndex = found->second;
			}
			groups[groupIndex].indices.push_back(i);
		}

		int groupNumber = 0;
		for (const GroupWork& group : groups)
		{
			if (group.indices.size() < 2)
				continue;
			Merge20FormDiffGroup out;
			out.id = L"G" + std::to_wstring(++groupNumber);
			out.dx = group.dx;
			out.dy = group.dy;
			out.recordIndices = group.indices;
			out.summary = Merge20BuildGroupSummary(result, out);
			for (size_t index : group.indices)
			{
				result.diffRecords[index].groupId = out.id;
				grouped[index] = 1;
			}
			work.outcome.groups.push_back(out);
		}

		// Minor group (section 5.2): leftover coordinate changes
		// from 0.5 to below 1.0, including size changes.
		std::vector<size_t> minorIndices;
		for (size_t i = 0; i < result.diffRecords.size(); ++i)
		{
			if (grouped[i])
				continue;
			const Merge20FormDiffRecord& record = result.diffRecords[i];
			if (record.type != Merge20FormDiffType::Coord)
				continue;
			double oldNum = 0.0;
			double newNum = 0.0;
			if (!ParseFiniteNumber(record.oldVal, oldNum) ||
				!ParseFiniteNumber(record.newVal, newNum))
			{
				continue;
			}
			const double delta = std::fabs(newNum - oldNum);
			if (delta >= kMerge20FormNoiseThreshold && delta < kMerge20FormMinorThreshold)
				minorIndices.push_back(i);
		}
		if (!minorIndices.empty())
		{
			Merge20FormDiffGroup minor;
			minor.id = L"minor";
			minor.minor = true;
			minor.recordIndices = minorIndices;
			minor.summary = Merge20BuildGroupSummary(result, minor);
			for (size_t index : minorIndices)
				result.diffRecords[index].groupId = minor.id;
			work.outcome.groups.push_back(minor);
		}
	}

	// Re-key the result shapes to the stable keys (design section
	// 4.1: a shape key is the element's stable key). F0 leaves the
	// HTML key (`tag[name='...']`) on the shapes; the records, the
	// changed-shape list, the canvas and the spotlight all address
	// elements by the stable key, so without this step every shape
	// lookup by record key misses: the canvas never highlights a
	// changed shape, the shape click never finds its record, and
	// the spotlight never scrolls and shows no old/new preview
	// (found while wiring the F5 table rows to FocusDiffRecord).
	// Runs last, on the result copies only: the work's own maps
	// still use the F0 keys. Old and new sides get separate maps:
	// the same HTML key can sit at different structural paths on
	// the two sides (a moved element), so one shared map would
	// hand the old shape the new side's key.
	void ReKeyShapes(FormDiffWork& work)
	{
		Merge20FormFileResult& result = work.outcome.result;
		std::unordered_map<std::wstring, std::wstring> oldStableByHtmlKey;
		for (size_t i = 0; i < work.oldParsed.elements.size(); ++i)
			oldStableByHtmlKey[work.oldParsed.elements[i].key] = work.oldKeys[i];
		std::unordered_map<std::wstring, std::wstring> newStableByHtmlKey;
		for (size_t i = 0; i < work.newParsed.elements.size(); ++i)
			newStableByHtmlKey[work.newParsed.elements[i].key] = work.newKeys[i];
		for (Merge20FormShape& shape : result.oldShapes)
		{
			const std::unordered_map<std::wstring, std::wstring>::const_iterator it =
				oldStableByHtmlKey.find(shape.key);
			if (it != oldStableByHtmlKey.end())
				shape.key = it->second;
		}
		for (Merge20FormShape& shape : result.newShapes)
		{
			const std::unordered_map<std::wstring, std::wstring>::const_iterator it =
				newStableByHtmlKey.find(shape.key);
			if (it != newStableByHtmlKey.end())
				shape.key = it->second;
		}
	}

	// Counts, totals, tendency, canvas size, summary and severity
	// for one finished file result.
	void FinishOutcome(FormDiffWork& work, const std::wstring& tendency)
	{
		Merge20FormFileResult& result = work.outcome.result;
		result.changeCount = static_cast<int>(result.diffRecords.size());
		for (const Merge20FormDiffRecord& record : result.diffRecords)
		{
			result.totalNumericSum += record.diffVal;
			switch (record.type)
			{
			case Merge20FormDiffType::Add:
				++result.addCount;
				break;
			case Merge20FormDiffType::Del:
				++result.delCount;
				break;
			case Merge20FormDiffType::Mod:
			case Merge20FormDiffType::Coord:
				++result.modCount;
				if (record.type == Merge20FormDiffType::Coord)
					++result.coordCount;
				break;
			case Merge20FormDiffType::Moved:
				++result.movedCount;
				break;
			case Merge20FormDiffType::Renamed:
				++result.renamedCount;
				break;
			case Merge20FormDiffType::FileAdd:
				++result.addCount;
				break;
			case Merge20FormDiffType::FileDel:
				++result.delCount;
				break;
			case Merge20FormDiffType::Error:
				break;
			}
			if (record.hasVisual)
			{
				if (std::find(result.changedShapeKeys.begin(), result.changedShapeKeys.end(),
					record.element) == result.changedShapeKeys.end())
				{
					result.changedShapeKeys.push_back(record.element);
				}
			}
		}
		// Canvas native size (HTML rule: max coordinate + 160,
		// at least 1000x800; 1000x800 when there is no shape).
		const std::vector<Merge20FormShape>& allOld = work.oldParsed.shapes;
		const std::vector<Merge20FormShape>& allNew = work.newParsed.shapes;
		if (allOld.empty() && allNew.empty())
		{
			result.nativeW = 1000.0;
			result.nativeH = 800.0;
		}
		else
		{
			double maxX = 800.0;
			double maxY = 600.0;
			const std::vector<Merge20FormShape>* shapeLists[2] = { &allOld, &allNew };
			for (int list = 0; list < 2; ++list)
			{
				for (const Merge20FormShape& shape : *shapeLists[list])
				{
					if (std::isfinite(shape.x) && std::isfinite(shape.x2))
						maxX = (std::max)(maxX, (std::max)(shape.x, shape.x2));
					if (std::isfinite(shape.y) && std::isfinite(shape.y2))
						maxY = (std::max)(maxY, (std::max)(shape.y, shape.y2));
				}
			}
			result.nativeW = std::ceil(maxX + 160.0);
			result.nativeH = std::ceil(maxY + 160.0);
		}
		result.oldShapes = allOld;
		result.newShapes = allNew;
		ReKeyShapes(work);
		result.tendency = tendency;
		// A file-level record (file added / deleted, XML error)
		// summarises as its tendency text.
		bool fileLevel = false;
		for (const Merge20FormDiffRecord& record : result.diffRecords)
		{
			if (record.type == Merge20FormDiffType::FileAdd ||
				record.type == Merge20FormDiffType::FileDel ||
				record.type == Merge20FormDiffType::Error)
			{
				fileLevel = true;
				break;
			}
		}
		result.summary = fileLevel ? tendency :
			Merge20BuildFormFileSummary(result, work.outcome.groups);
		result.severity = Merge20ClassifyFormSeverity(result);
	}

} // namespace

Merge20FormDiffOutcome Merge20DiffFormParsed(
	const Merge20FormParseResult& oldParsed,
	const Merge20FormParseResult& newParsed,
	const std::wstring& fileName)
{
	FormDiffWork work(oldParsed, newParsed);
	Merge20FormFileResult& result = work.outcome.result;
	result.fileName = fileName;
	result.formName = L"\uff08\u540d\u79f0\u672a\u767b\u9332\uff09";

	// Stage 1: exact stable-key matches.
	std::vector<std::pair<size_t, size_t>> pairs;
	for (size_t i = 0; i < work.oldParsed.elements.size(); ++i)
	{
		const std::unordered_map<std::wstring, size_t>::const_iterator it =
			work.newByKey.find(work.oldKeys[i]);
		if (it == work.newByKey.end() || work.newUsed[it->second])
			continue;
		work.oldUsed[i] = 1;
		work.newUsed[it->second] = 1;
		pairs.push_back(std::make_pair(i, it->second));
	}

	// Stage 1.5: same parent, same tag, same name (or signature),
	// ignoring the sibling index. Duplicates are paired in
	// document order; this keeps a pure insertion from shifting
	// every later element into a false move.
	{
		std::map<std::wstring, std::vector<size_t>> oldGroups;
		std::map<std::wstring, std::vector<size_t>> newGroups;
		for (size_t i = 0; i < work.oldParsed.elements.size(); ++i)
		{
			if (work.oldUsed[i])
				continue;
			const Merge20FormElement& el = work.oldParsed.elements[i];
			const std::wstring key = ParentPath(el.structPath) + L"|" + el.tag + L"|" +
				(el.name.empty() ? BuildAttributeSignature(el) : el.name);
			oldGroups[key].push_back(i);
		}
		for (size_t i = 0; i < work.newParsed.elements.size(); ++i)
		{
			if (work.newUsed[i])
				continue;
			const Merge20FormElement& el = work.newParsed.elements[i];
			const std::wstring key = ParentPath(el.structPath) + L"|" + el.tag + L"|" +
				(el.name.empty() ? BuildAttributeSignature(el) : el.name);
			newGroups[key].push_back(i);
		}
		for (const std::pair<const std::wstring, std::vector<size_t>>& group : oldGroups)
		{
			const std::map<std::wstring, std::vector<size_t>>::const_iterator found =
				newGroups.find(group.first);
			if (found == newGroups.end())
				continue;
			const size_t count = (std::min)(group.second.size(), found->second.size());
			for (size_t k = 0; k < count; ++k)
			{
				const size_t oldIndex = group.second[k];
				const size_t newIndex = found->second[k];
				work.oldUsed[oldIndex] = 1;
				work.newUsed[newIndex] = 1;
				pairs.push_back(std::make_pair(oldIndex, newIndex));
			}
		}
	}

	// Attribute changes of the matched pairs, in new-document
	// order (like the HTML tool's iteration over the new map).
	std::sort(pairs.begin(), pairs.end(),
		[](const std::pair<size_t, size_t>& a, const std::pair<size_t, size_t>& b) {
			return a.second < b.second;
		});
	std::vector<Merge20FormDiffRecord> attrRecords;
	for (const std::pair<size_t, size_t>& pair : pairs)
		CompareMatchedPair(work, pair.first, pair.second, attrRecords);

	// Stage 2: move detection (section 5.1). Same tag, same name
	// (or same signature when nameless), different parent, and
	// equal non-coordinate attributes. A same-parent order-only
	// swap is not a move (T-F-2 decision); such pairs were already
	// consumed by stage 1.5.
	std::vector<Merge20FormDiffRecord> movedRecords;
	for (size_t i = 0; i < work.oldParsed.elements.size(); ++i)
	{
		if (work.oldUsed[i])
			continue;
		const Merge20FormElement& oldEl = work.oldParsed.elements[i];
		for (size_t j = 0; j < work.newParsed.elements.size(); ++j)
		{
			if (work.newUsed[j])
				continue;
			const Merge20FormElement& newEl = work.newParsed.elements[j];
			if (oldEl.tag != newEl.tag)
				continue;
			if (oldEl.name != newEl.name)
				continue;
			if (oldEl.name.empty() &&
				BuildAttributeSignature(oldEl) != BuildAttributeSignature(newEl))
			{
				continue;
			}
			if (ParentPath(oldEl.structPath) == ParentPath(newEl.structPath))
				continue;
			if (!AttrsEqualExcept(oldEl, newEl, true, false))
				continue;
			work.oldUsed[i] = 1;
			work.newUsed[j] = 1;
			movedRecords.push_back(MakeMovedRecord(work, oldEl, newEl));
			break;
		}
	}

	// Stage 3: rename detection (section 5.1). Same structural
	// path, same tag, only the name differs. Coordinates may
	// differ as well (a rename plus a coordinate change is one
	// `Renamed` record); the other attributes must be equal.
	std::vector<Merge20FormDiffRecord> renamedRecords;
	for (size_t i = 0; i < work.oldParsed.elements.size(); ++i)
	{
		if (work.oldUsed[i])
			continue;
		const Merge20FormElement& oldEl = work.oldParsed.elements[i];
		for (size_t j = 0; j < work.newParsed.elements.size(); ++j)
		{
			if (work.newUsed[j])
				continue;
			const Merge20FormElement& newEl = work.newParsed.elements[j];
			if (oldEl.structPath != newEl.structPath)
				continue;
			if (oldEl.tag != newEl.tag)
				continue;
			if (oldEl.name == newEl.name)
				continue;
			if (!AttrsEqualExcept(oldEl, newEl, true, true))
				continue;
			// Same slot check (design section 5.1 test 4): a
			// rename keeps the element in (roughly) the same
			// place. Without it, a deletion plus an addition at
			// the same structural position with different
			// coordinates would be misread as a rename. When
			// both sides have shapes the rectangles must
			// overlap (touching counts); without shapes every
			// attribute except the name must be equal.
			const Merge20FormShape* oldShape = work.OldShape(oldEl);
			const Merge20FormShape* newShape = work.NewShape(newEl);
			if (oldShape != nullptr && newShape != nullptr)
			{
				if (oldShape->x2 < newShape->x || newShape->x2 < oldShape->x ||
					oldShape->y2 < newShape->y || newShape->y2 < oldShape->y)
				{
					continue;
				}
			}
			else if (!AttrsEqualExcept(oldEl, newEl, false, true))
			{
				continue;
			}
			work.oldUsed[i] = 1;
			work.newUsed[j] = 1;
			renamedRecords.push_back(MakeRenamedRecord(work, oldEl, newEl));
			break;
		}
	}

	// Stage 3.5: a nameless element whose attributes changed.
	// Its attribute signature (part of the stable key) changes
	// with the attributes, so stages 1 and 1.5 cannot pair it;
	// the structural path plus the tag still identify the same
	// slot, so the pair is matched and its attributes are
	// compared instead of reporting a deletion plus an addition.
	// (The HTML tool reports DEL + ADD here; this is an
	// improvement of the same kind as move / rename detection.)
	for (size_t i = 0; i < work.oldParsed.elements.size(); ++i)
	{
		if (work.oldUsed[i])
			continue;
		const Merge20FormElement& oldEl = work.oldParsed.elements[i];
		if (!oldEl.name.empty())
			continue;
		for (size_t j = 0; j < work.newParsed.elements.size(); ++j)
		{
			if (work.newUsed[j])
				continue;
			const Merge20FormElement& newEl = work.newParsed.elements[j];
			if (oldEl.structPath != newEl.structPath)
				continue;
			if (oldEl.tag != newEl.tag)
				continue;
			if (!newEl.name.empty())
				continue;
			work.oldUsed[i] = 1;
			work.newUsed[j] = 1;
			CompareMatchedPair(work, i, j, attrRecords);
			break;
		}
	}

	// Stage 4: the rest is added / deleted (HTML order: additions
	// first, then deletions).
	std::vector<Merge20FormDiffRecord> addRecords;
	for (size_t j = 0; j < work.newParsed.elements.size(); ++j)
	{
		if (work.newUsed[j])
			continue;
		const Merge20FormElement& newEl = work.newParsed.elements[j];
		Merge20FormDiffRecord record;
		record.type = Merge20FormDiffType::Add;
		record.element = work.newKeys[j];
		record.attr = L"\u8981\u7d20\u8ffd\u52a0";
		record.oldVal = L"(\u306a\u3057)";
		record.newVal = L"<" + newEl.tag + L"> \u8ffd\u52a0";
		record.diffVal = 1.0;
		record.desc = L"\u65b0\u898f\u8981\u7d20 (" + DisplayName(newEl) + L") \u304c\u30d5\u30a9\u30fc\u30e0\u306b\u8ffd\u52a0\u3055\u308c\u307e\u3057\u305f";
		record.impact = L"\u65b0\u305f\u306a\u5370\u5b57\u9805\u76ee\u30fb\u7f6b\u7dda\u30fb\u56fa\u5b9a\u67a0\u304c\u51fa\u529b\u3055\u308c\u308b\u305f\u3081\u3001\u65e2\u5b58\u9805\u76ee\u3068\u306e\u91cd\u306a\u308a\u3092\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044";
		record.hasVisual = work.NewShape(newEl) != nullptr;
		addRecords.push_back(record);
	}
	std::vector<Merge20FormDiffRecord> delRecords;
	for (size_t i = 0; i < work.oldParsed.elements.size(); ++i)
	{
		if (work.oldUsed[i])
			continue;
		const Merge20FormElement& oldEl = work.oldParsed.elements[i];
		Merge20FormDiffRecord record;
		record.type = Merge20FormDiffType::Del;
		record.element = work.oldKeys[i];
		record.attr = L"\u8981\u7d20\u524a\u9664";
		record.oldVal = L"<" + oldEl.tag + L"> \u5b58\u5728";
		record.newVal = L"(\u524a\u9664)";
		record.diffVal = 1.0;
		record.desc = L"\u65e2\u5b58\u8981\u7d20 (" + DisplayName(oldEl) + L") \u304c\u30d5\u30a9\u30fc\u30e0\u304b\u3089\u524a\u9664\u3055\u308c\u307e\u3057\u305f";
		record.impact = L"\u73fe\u5730\u3067\u4f7f\u7528\u3057\u3066\u3044\u305f\u5370\u5b57\u9805\u76ee\u3084\u67a0\u7dda\u304c\u51fa\u529b\u3055\u308c\u306a\u304f\u306a\u308b\u305f\u3081\u3001\u696d\u52d9\u5f71\u97ff\u3092\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044";
		record.hasVisual = work.OldShape(oldEl) != nullptr;
		delRecords.push_back(record);
	}

	result.diffRecords.reserve(addRecords.size() + delRecords.size() +
		movedRecords.size() + renamedRecords.size() + attrRecords.size());
	for (const Merge20FormDiffRecord& record : addRecords)
		result.diffRecords.push_back(record);
	for (const Merge20FormDiffRecord& record : delRecords)
		result.diffRecords.push_back(record);
	for (const Merge20FormDiffRecord& record : movedRecords)
		result.diffRecords.push_back(record);
	for (const Merge20FormDiffRecord& record : renamedRecords)
		result.diffRecords.push_back(record);
	for (const Merge20FormDiffRecord& record : attrRecords)
		result.diffRecords.push_back(record);

	GroupRecords(work);

	// Tendency (HTML `executeDiff`); the counts are taken from
	// the records directly because FinishOutcome fills the
	// result counts only later. Moved / renamed elements get
	// their own parts: a file whose only change is a move must
	// not end up with an empty tendency.
	std::vector<std::wstring> tendencyParts;
	std::wstring tendency;
	{
		int addCount = 0;
		int delCount = 0;
		int modCount = 0;
		int movedCount = 0;
		int renamedCount = 0;
		for (const Merge20FormDiffRecord& record : result.diffRecords)
		{
			if (record.type == Merge20FormDiffType::Add)
				++addCount;
			else if (record.type == Merge20FormDiffType::Del)
				++delCount;
			else if (record.type == Merge20FormDiffType::Coord ||
				record.type == Merge20FormDiffType::Mod)
				++modCount;
			else if (record.type == Merge20FormDiffType::Moved)
				++movedCount;
			else if (record.type == Merge20FormDiffType::Renamed)
				++renamedCount;
		}
		if (addCount > 0)
			tendencyParts.push_back(L"\u8ffd\u52a0(" + std::to_wstring(addCount) + L")");
		if (delCount > 0)
			tendencyParts.push_back(L"\u524a\u9664(" + std::to_wstring(delCount) + L")");
		if (modCount > 0)
			tendencyParts.push_back(L"\u5909\u66f4/\u5ea7\u6a19(" + std::to_wstring(modCount) + L")");
		if (movedCount > 0)
			tendencyParts.push_back(L"\u79fb\u52d5(" + std::to_wstring(movedCount) + L")");
		if (renamedCount > 0)
			tendencyParts.push_back(L"\u6539\u540d(" + std::to_wstring(renamedCount) + L")");
		for (size_t i = 0; i < tendencyParts.size(); ++i)
		{
			if (i > 0)
				tendency += L" / ";
			tendency += tendencyParts[i];
		}
	}
	FinishOutcome(work, tendency);
	return work.outcome;
}

Merge20FormDiffOutcome Merge20DiffFormFiles(
	const std::wstring* oldXml, const std::wstring* newXml,
	const std::wstring& fileName)
{
	// File only in the new folder (HTML `executeDiff` FILE_ADD).
	if (oldXml == nullptr)
	{
		Merge20FormParseResult newParsed;
		if (newXml != nullptr)
			newParsed = Merge20ParseFormXml(*newXml);
		Merge20FormParseResult emptyParsed;
		FormDiffWork work(emptyParsed, newParsed);
		Merge20FormFileResult& result = work.outcome.result;
		result.fileName = fileName;
		result.formName = L"\uff08\u540d\u79f0\u672a\u767b\u9332\uff09";
		Merge20FormDiffRecord record;
		record.type = Merge20FormDiffType::FileAdd;
		record.element = fileName;
		record.attr = L"\u30d5\u30a1\u30a4\u30eb\u5168\u4f53";
		record.oldVal = L"\u5b58\u5728\u306a\u3057 (\u73fe\u5730\u5438\u3044\u4e0a\u3052\u306b\u306a\u3057)";
		record.newVal = L"\u65b0\u898f\u8ffd\u52a0 (\u30d1\u30c3\u30b1\u30fc\u30b8\u306b\u5b58\u5728)";
		record.diffVal = 1.0;
		record.desc = L"\u30d1\u30c3\u30b1\u30fc\u30b8\u30d5\u30a9\u30eb\u30c0\u306e\u307f\u306b\u5b58\u5728\u3059\u308b\u65b0\u898f\u5e33\u7968XML\u30d5\u30a1\u30a4\u30eb";
		record.impact = L"\u65b0\u898f\u5e33\u7968\u3068\u3057\u3066\u8ffd\u52a0\u5c0e\u5165\u3055\u308c\u308b\u305f\u3081\u3001\u51fa\u529b\u30c6\u30b9\u30c8\u304a\u3088\u3073\u904b\u7528\u5bfe\u8c61\u304b\u306e\u78ba\u8a8d\u304c\u5fc5\u8981";
		record.hasVisual = false;
		result.diffRecords.push_back(record);
		FinishOutcome(work, L"\u65b0\u898f\u30d5\u30a1\u30a4\u30eb\u8ffd\u52a0");
		return work.outcome;
	}
	// File only in the old folder (HTML `executeDiff` FILE_DEL).
	if (newXml == nullptr)
	{
		Merge20FormParseResult oldParsed;
		if (oldXml != nullptr)
			oldParsed = Merge20ParseFormXml(*oldXml);
		Merge20FormParseResult emptyParsed;
		FormDiffWork work(oldParsed, emptyParsed);
		Merge20FormFileResult& result = work.outcome.result;
		result.fileName = fileName;
		result.formName = L"\uff08\u540d\u79f0\u672a\u767b\u9332\uff09";
		Merge20FormDiffRecord record;
		record.type = Merge20FormDiffType::FileDel;
		record.element = fileName;
		record.attr = L"\u30d5\u30a1\u30a4\u30eb\u5168\u4f53";
		record.oldVal = L"\u5b58\u5728\u3042\u308a (\u73fe\u5730\u5438\u3044\u4e0a\u3052\u306b\u5b58\u5728)";
		record.newVal = L"\u524a\u9664 (\u30d1\u30c3\u30b1\u30fc\u30b8\u306b\u306a\u3057)";
		record.diffVal = 1.0;
		record.desc = L"\u73fe\u5730\u5438\u3044\u4e0a\u3052\u30d5\u30a9\u30eb\u30c0\u306e\u307f\u306b\u5b58\u5728\u3059\u308bXML\u30d5\u30a1\u30a4\u30eb";
		record.impact = L"\u65b0\u30d1\u30c3\u30b1\u30fc\u30b8\u3067\u5ec3\u6b62\u3001\u307e\u305f\u306f\u73fe\u5730\u72ec\u81ea\u30ab\u30b9\u30bf\u30de\u30a4\u30ba\u5e33\u7968\u304c\u6b20\u843d\u3059\u308b\u6050\u308c\u304c\u3042\u308b\u305f\u3081\u8981\u6ce8\u610f";
		record.hasVisual = false;
		result.diffRecords.push_back(record);
		FinishOutcome(work, L"\u65e2\u5b58\u30d5\u30a1\u30a4\u30eb\u524a\u9664");
		return work.outcome;
	}

	const Merge20FormParseResult oldParsed = Merge20ParseFormXml(*oldXml);
	const Merge20FormParseResult newParsed = Merge20ParseFormXml(*newXml);
	// XML syntax error (HTML `executeDiff` ERROR).
	if (!oldParsed.ok || !newParsed.ok)
	{
		Merge20FormParseResult emptyParsed;
		FormDiffWork work(emptyParsed, emptyParsed);
		Merge20FormFileResult& result = work.outcome.result;
		result.fileName = fileName;
		result.formName = L"\uff08\u540d\u79f0\u672a\u767b\u9332\uff09";
		Merge20FormDiffRecord record;
		record.type = Merge20FormDiffType::Error;
		record.element = fileName;
		record.attr = L"XML\u69cb\u6587";
		record.oldVal = L"-";
		record.newVal = L"-";
		record.diffVal = 1.0;
		record.desc = L"XML\u306e\u30d1\u30fc\u30b9\u306b\u5931\u6557\u3057\u307e\u3057\u305f";
		record.impact = L"\u6587\u5b57\u30b3\u30fc\u30c9\u307e\u305f\u306f\u30bf\u30b0\u69cb\u9020\u304c\u7834\u640d\u3057\u3066\u3044\u308b\u53ef\u80fd\u6027\u304c\u3042\u308a\u307e\u3059";
		record.hasVisual = false;
		result.diffRecords.push_back(record);
		FinishOutcome(work, L"XML\u69cb\u6587\u30a8\u30e9\u30fc");
		return work.outcome;
	}
	return Merge20DiffFormParsed(oldParsed, newParsed, fileName);
}
