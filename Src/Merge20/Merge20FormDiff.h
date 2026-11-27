/**
 * @file  Merge20FormDiff.h
 *
 * @brief FORM/XML comparison (Wave 5 F1): matching and classification
 * (design merge20-form-compare-design.md sections 4.2, 4.4, 5.1-5.4
 * and 10 F1).
 *
 * This module is a port of the HTML tool's `executeDiff` and
 * `getDetailInterpretation` (FROM_____1.html), with the improvements
 * of design section 6 (1)-(3): stable keys, move/rename detection,
 * noise filtering and grouping. Like Merge20FormXml (F0) it is a
 * pure module: no MFC, no `_()`. The `desc` / `impact` texts are
 * Japanese (they are future UI strings); the file itself stays
 * ASCII and the Japanese literals in the .cpp are written with \u
 * escapes, like Merge20FormXml_test.cpp.
 *
 * Data model (`Merge20FormElement` / `Merge20FormShape` /
 * `Merge20FormDiffRecord` / `Merge20FormFileResult`) lives in
 * Merge20FormXml.h (F0). This module only adds the diff outcome
 * (grouping and the ignored-noise count), which cannot be stored in
 * the F0 result struct without changing it.
 *
 * Deliberate differences from the HTML tool:
 * - Stable keys (`structPath + tag + name`, section 5.1) replace the
 *   HTML keys (`tag[name='...']` or the attribute signature). The
 *   `element` field of a record is the stable key.
 * - A moved element is one `Moved` record instead of a DEL plus an
 *   ADD; a renamed element is one `Renamed` record (section 5.1).
 *   A rename keeps the element in (roughly) the same place: the
 *   old and new bounding rectangles must overlap (touching
 *   counts), or, when there is no shape, every attribute except
 *   the name must be equal. Without that check a deletion plus an
 *   addition at the same structural position with different
 *   coordinates would be misread as a rename (section 5.1
 *   test 4).
 * - Coordinate changes below 0.5 are ignored as noise (section
 *   5.2); changes from 0.5 to below 1.0 are kept but collected into
 *   the "minor" group (section 5.2).
 * - Coordinated changes are grouped (section 5.3); the group id is
 *   stored in the record's `groupId` field.
 * - `diffVal` keeps one decimal place at most (section 5.2: the
 *   HTML tool rounds to an integer, so 100 -> 112.5 shows as 13
 *   there and as 12.5 here). `totalNumericSum` is the unrounded
 *   double sum; rounding happens only for display (section 5.2).
 * - `getDetailInterpretation` is ported with the HTML wording, but
 *   a non-numeric coordinate value (or an attribute added or
 *   removed) falls through to the generic wording instead of
 *   producing an empty `desc` / `impact` as the HTML tool does,
 *   and a missing value is shown as `(deleted)` instead of the
 *   literal text `undefined` the HTML tool produces.
 * - An order-only swap of siblings under the same parent is
 *   ignored (design section 5.1, T-F-2 decision: display order
 *   alone does not change the layout).
 * - Element text changes are detected as `Mod` records (the HTML
 *   tool ignores element text; F0 keeps `text` for this
 *   comparison).
 * - A nameless element whose attributes change is paired by
 *   structural path plus tag (stage 3.5) instead of being
 *   reported as deleted plus added as the HTML tool does.
 * - The tendency gains `moved(N)` / `renamed(N)` parts so that a
 *   file whose only change is a move or a rename does not end
 *   up with an empty tendency.
 * - Overlap / overflow detection (design section 6 (5)) is F5, not
 *   F1: severity here is decided without geometry.
 */
#pragma once

#include <string>
#include <vector>
#include "Merge20/Merge20FormXml.h"

/** Noise threshold (design section 5.2): a coordinate change whose
 * absolute value is below this is ignored. The boundary value
 * itself (0.5) is still reported as a change. */
constexpr double kMerge20FormNoiseThreshold = 0.5;

/** Upper bound of a "minor" difference (design section 5.2): a
 * coordinate change from 0.5 (inclusive) to below this is kept but
 * collected into the minor group. */
constexpr double kMerge20FormMinorThreshold = 1.0;

/** @brief Interpretation of one attribute change: a port of the
 * HTML tool's `getDetailInterpretation` (design section 2.3). */
struct Merge20FormInterpretation
{
	std::wstring desc; /**< Concrete change description (Japanese). */
	std::wstring impact; /**< Impact analysis (Japanese). */
	double numericDiff = 0.0; /**< Absolute numeric change (0 for a non-numeric change). */
	bool isCoord = false; /**< True for a coordinate/size attribute change. */
};

/**
 * @brief Interpret one attribute change (HTML `getDetailInterpretation`).
 * @param [in] attr Attribute name.
 * @param [in] oldVal Old value (ignored when `hasOld` is false).
 * @param [in] newVal New value (ignored when `hasNew` is false).
 * @param [in] hasOld False when the attribute is absent on the old side.
 * @param [in] hasNew False when the attribute is absent on the new side.
 * @return The description, impact, numeric difference and whether
 * the change is a coordinate change. Coordinate attributes are
 * `x1` / `x2` / `x` / `left` / `width` (horizontal) and `y1` /
 * `y2` / `y` / `top` / `height` (vertical), like the HTML tool.
 */
Merge20FormInterpretation Merge20InterpretFormAttribute(
	const std::wstring& attr,
	const std::wstring& oldVal, const std::wstring& newVal,
	bool hasOld, bool hasNew);

/**
 * @brief Build the stable key of an element (design section 5.1):
 * `structPath + "|" + tag + "|" + name`, or the attribute
 * signature instead of the name when the element has no `name`
 * attribute. The signature lists attributes in sorted order,
 * excluding `xmlns` / `xmlns:*` (same rule as F0).
 */
std::wstring Merge20BuildStableKey(const Merge20FormElement& element);

/** @brief One group of coordinated changes (design section 5.3). */
struct Merge20FormDiffGroup
{
	std::wstring id; /**< Group id (`G1`, `G2`, ...; `minor` for the minor group). */
	std::wstring summary; /**< Group summary, e.g. `field 5 items moved down 12 units`. */
	bool minor = false; /**< True for the minor-differences group (section 5.2). */
	double dx = 0.0; /**< Representative horizontal move of the group (XML units). */
	double dy = 0.0; /**< Representative vertical move of the group (XML units). */
	std::vector<size_t> recordIndices; /**< Indices into `Merge20FormFileResult::diffRecords`. */
};

/** @brief Outcome of diffing one FORM file pair. */
struct Merge20FormDiffOutcome
{
	Merge20FormFileResult result; /**< Per-file result (F0 data model). */
	int ignoredNoiseCount = 0; /**< Coordinate changes below 0.5 ignored as noise (section 5.2). */
	std::vector<Merge20FormDiffGroup> groups; /**< Grouped changes (section 5.3). */
};

/**
 * @brief Diff one FORM file pair (HTML `executeDiff`, per-file part).
 * @param [in] oldXml Old XML text, or nullptr when the file exists
 * only in the new folder (FILE_ADD).
 * @param [in] newXml New XML text, or nullptr when the file exists
 * only in the old folder (FILE_DEL).
 * @param [in] fileName Relative path (common key of the two folders).
 * @return The diff outcome. `result.formName` is `(unregistered)`;
 * the name master (F6) resolves the real form name later.
 * `result.isWarning` and the warning-dependent part of
 * `result.severity` are NOT set here: call
 * `Merge20ApplyLargeChangeFlag` over the whole folder result once
 * the average is known (design section 4.4).
 */
Merge20FormDiffOutcome Merge20DiffFormFiles(
	const std::wstring* oldXml, const std::wstring* newXml,
	const std::wstring& fileName);

/**
 * @brief Diff two already-parsed FORM documents.
 * @param [in] oldParsed Old parse result (F0).
 * @param [in] newParsed New parse result (F0).
 * @param [in] fileName Relative path (common key of the two folders).
 * @return The diff outcome (same contract as Merge20DiffFormFiles,
 * without the FILE_ADD / FILE_DEL / ERROR cases).
 */
Merge20FormDiffOutcome Merge20DiffFormParsed(
	const Merge20FormParseResult& oldParsed,
	const Merge20FormParseResult& newParsed,
	const std::wstring& fileName);

/**
 * @brief Apply the large-change classification (design section 4.4)
 * to a whole folder result: `totalNumericSum > average * 1.5 and
 * totalNumericSum > 20` (average over the changed files, rounded
 * like the HTML tool) sets `isWarning`, and the severity of every
 * file is recomputed with the flag taken into account.
 * @param [in,out] results Per-file results of one folder comparison.
 */
void Merge20ApplyLargeChangeFlag(std::vector<Merge20FormFileResult>& results);

/**
 * @brief Classify the severity of one file's changes (design
 * section 5.4): `LayoutBreaking` for a file add / delete, an XML
 * error or a large change; `VisualOnly` for coordinate / size /
 * font / move / rename changes; `Info` otherwise (element add /
 * delete without a layout impact). Overlap / overflow detection
 * (design section 6 (5)) is F5 and is not considered here.
 */
Merge20FormSeverity Merge20ClassifyFormSeverity(const Merge20FormFileResult& result);

/**
 * @brief Build the one-line summary of a file (design section 6
 * (4)): the group summary when there is a single group, the group
 * summaries joined otherwise, or a counts sentence when there is
 * no group.
 * @param [in] result Per-file result.
 * @param [in] groups Groups of the file (from Merge20FormDiffOutcome).
 */
std::wstring Merge20BuildFormFileSummary(
	const Merge20FormFileResult& result,
	const std::vector<Merge20FormDiffGroup>& groups);

/**
 * @brief Build the summary text of one group (design section 5.3),
 * e.g. `field 5 items moved down 12 units`. The text holds the
 * move amount, the direction and the item count.
 */
std::wstring Merge20BuildGroupSummary(
	const Merge20FormFileResult& result,
	const Merge20FormDiffGroup& group);

/**
 * @brief Format a coordinate delta with its unit, keeping one
 * decimal place at most: `+12.5 units`, `-3 units`, `+0.5 units`.
 * The sign is always shown, like the HTML wording.
 */
std::wstring Merge20FormatFormDelta(double delta);
