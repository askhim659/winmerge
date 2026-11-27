/**
 * @file  Merge20FormXml.h
 *
 * @brief FORM/XML comparison (Wave 5 F0): data model and a small
 * hand-written XML parser (design merge20-form-compare-design.md
 * sections 3.3 and 4).
 *
 * This module is a port of the HTML tool's `parseXmlToMapAndShapes`
 * (FROM_____1.html). It is deliberately free of MFC and of the
 * translation macro `_()`: the same code is compiled into WinMergeU
 * and into the GoogleTest runner.
 *
 * The diff/matching logic (stable keys, move/rename detection, noise
 * filtering, grouping) is NOT here; it lives in Merge20FormDiff
 * (F1). The `key` produced here is the HTML tool's key
 * (tag[name='...'] or the attribute signature, with a #N suffix for
 * duplicates); F1 replaces it with the stable key (structPath + tag
 * + name, design section 5.1).
 *
 * Deliberate differences from the HTML tool (DOMParser):
 * - An undefined entity reference (e.g. `&foo;`) is kept literally
 *   instead of failing the parse (design section 3.3). DOMParser
 *   treats it as a fatal well-formedness error.
 * - `xmlns` / `xmlns:*` declaration attributes are excluded when
 *   building the attribute-signature key (design section 3.3); the
 *   HTML tool includes them. The attributes themselves are still
 *   kept in `attrs`.
 * - The attribute-signature key lists attributes in sorted
 *   (std::map) order, while the HTML tool uses document order.
 *   Key strings are therefore not compared against the HTML ground
 *   truth in F0; the comparison is over the element set, the
 *   attributes and the shape coordinates (design section 10 F0 (c)).
 * - `Merge20FormElement::text` (the element's direct text content)
 *   has no counterpart in the HTML map values; it is kept here
 *   because the shape text (design section 4.1) and the F1 text
 *   comparison need it.
 */
#pragma once

#include <map>
#include <string>
#include <vector>

/** @brief One element (HTML map value {tag, name, attrs}). */
struct Merge20FormElement
{
	std::wstring tag; /**< Tag name (namespace prefix included). */
	std::wstring name; /**< Value of the `name` attribute, empty when absent. */
	std::wstring key; /**< Key (HTML rule for now; stable key in F1). */
	std::wstring structPath; /**< Structural path, e.g. `/form/page[1]/field[3]` (design section 5.1). */
	std::map<std::wstring, std::wstring> attrs; /**< All attributes, entity-decoded. */
	std::wstring text; /**< Direct text content (entity-decoded, CDATA included). No counterpart in the HTML map; kept for the shape text and F1. */
};

/** @brief One shape (HTML shapes entry). */
struct Merge20FormShape
{
	std::wstring key; /**< Key of the corresponding element. */
	std::wstring tag;
	std::wstring name; /**< Display name (`name` attribute, or `tag` when absent). */
	double x = 0.0; /**< Left coordinate (XML coordinate units). */
	double y = 0.0; /**< Top coordinate. */
	double x2 = 0.0; /**< Right coordinate (estimated when absent, see isEstimated). */
	double y2 = 0.0; /**< Bottom coordinate (estimated when absent). */
	double point = 10.0; /**< Font size (`point` / `fontsize` attribute, 10 when absent). */
	std::wstring fontName; /**< Font name (`fontname` / `font` attribute, empty when absent; F3 text drawing, design section 6 (6)). */
	bool isLine = false; /**< True when the tag name contains `line` (case-insensitive). */
	std::wstring text; /**< Element text content (for the F3 text drawing). */
	bool isEstimated = false; /**< True when x2 and/or y2 was estimated (design section 4.1). Estimated shapes are excluded from the F5 impact analysis. */
};

/** @brief Kinds of FORM diff records (design section 4.2). */
enum class Merge20FormDiffType
{
	FileAdd, /**< FILE_ADD: file only in the new folder. */
	FileDel, /**< FILE_DEL: file only in the old folder. */
	Error, /**< ERROR: XML syntax error. */
	Add, /**< ADD: element added. */
	Del, /**< DEL: element deleted. */
	Coord, /**< COORD: coordinate/size change. */
	Mod, /**< MOD: non-coordinate attribute change. */
	Moved, /**< Moved element (F1, design section 5.1). */
	Renamed, /**< Renamed element (F1, design section 5.1). */
};

/** @brief One diff record (HTML diffRecords entry). */
struct Merge20FormDiffRecord
{
	Merge20FormDiffType type = Merge20FormDiffType::Mod;
	std::wstring element; /**< Target element (stable key; translated to name/tag for display). */
	std::wstring attr; /**< Changed attribute (`element added` / `element deleted` / `whole file` for element/file changes). */
	std::wstring oldVal; /**< Old value. */
	std::wstring newVal; /**< New value. */
	double diffVal = 0.0; /**< Absolute numeric change (internal double; shown with its unit). */
	std::wstring desc; /**< Concrete change description (Japanese text). */
	std::wstring impact; /**< Impact analysis (Japanese text). */
	bool hasVisual = false; /**< True when the record maps to a shape (spotlight target). */
	std::wstring groupId; /**< Group id of a grouped change (design section 5.3); empty for a single change. */
};

/** @brief Severity of a file's changes (design section 5.4). */
enum class Merge20FormSeverity
{
	LayoutBreaking, /**< Layout-breaking change (overlap, overflow, file add/del, large change). */
	VisualOnly, /**< Visual-only change (coordinate / font-size change without overlap/overflow). */
	Info, /**< Informational change (element add/del without layout impact). */
};

/** @brief Per-file result (HTML rawResults entry). */
struct Merge20FormFileResult
{
	std::wstring fileName; /**< Relative path (common key of old/new). */
	std::wstring formName; /**< Form name resolved from the name master (`(unregistered)` when absent). */
	std::wstring tendency; /**< Change tendency (`added(N) / deleted(N) / changed/coordinate(N)`). */
	std::wstring summary; /**< One-line summary (design section 6 (4)). */
	double totalNumericSum = 0.0; /**< Sum of the numeric changes (rounded to an integer for display, like the HTML tool). */
	int changeCount = 0; /**< Number of diff records. */
	int addCount = 0; /**< Number of ADD records. */
	int delCount = 0; /**< Number of DEL records. */
	int modCount = 0; /**< Number of MOD records. */
	int coordCount = 0; /**< Number of COORD records. */
	int movedCount = 0; /**< Number of Moved records. */
	int renamedCount = 0; /**< Number of Renamed records. */
	std::vector<Merge20FormDiffRecord> diffRecords;
	std::vector<Merge20FormShape> oldShapes; /**< Old-side shapes. */
	std::vector<Merge20FormShape> newShapes; /**< New-side shapes. */
	double nativeW = 1000.0; /**< Canvas native width (max shape coordinate + 160, at least 1000). */
	double nativeH = 800.0; /**< Canvas native height (max shape coordinate + 160, at least 800). */
	std::vector<std::wstring> changedShapeKeys; /**< Keys of the changed shapes. */
	bool isWarning = false; /**< Large change (design section 4.4). */
	Merge20FormSeverity severity = Merge20FormSeverity::Info;
};

/** @brief Result of Merge20ParseFormXml(). */
struct Merge20FormParseResult
{
	bool ok = false; /**< False on parse failure (the HTML `null` case). */
	std::vector<Merge20FormElement> elements; /**< Elements in document order. */
	std::vector<Merge20FormShape> shapes; /**< Shapes in document order. */
};

/**
 * @brief Parse one FORM definition XML text (HTML `parseXmlToMapAndShapes`).
 * @param [in] xml XML text (already decoded to wchar_t; the file's
 * character set is resolved by the caller with WinMerge's charset
 * detection, design section 3.3).
 * @return Parse result; `ok` is false when the XML is malformed
 * (mismatched tags, unclosed attribute quote, stray `<` or `&`,
 * missing root, trailing content after the root). The elements and
 * shapes are valid only when `ok` is true.
 */
Merge20FormParseResult Merge20ParseFormXml(const std::wstring& xml);

/**
 * @brief Parse a number the way JavaScript `Number()` does.
 * @param [in] text Text to parse (leading/trailing whitespace is
 * ignored; an empty or whitespace-only string is 0).
 * @param [out] value Parsed value (NaN when the text is `NaN`).
 * @return False when the text is not a single number (e.g. `12px`,
 * `abc`). Hex integers (`0x10`) and `Infinity` are accepted like
 * JavaScript does; binary (`0b...`) and octal (`0o...`) literals
 * are not supported. The parse is locale-independent (the decimal
 * point is always `.`).
 */
bool Merge20FormParseNumber(const std::wstring& text, double& value);

