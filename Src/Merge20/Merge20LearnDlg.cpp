// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20LearnDlg.cpp
 *
 * @brief Implementation of the Merge20 learning dialog (Phase 3 L4/L5).
 */
#include "StdAfx.h"
#include "Merge20/Merge20LearnDlg.h"
#include "Merge20/Merge20Learn.h"
#include "Merge20/Merge20LearnDoc.h"
#include "FileOrFolderSelect.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include <algorithm>

IMPLEMENT_DYNAMIC(Merge20LearnDlg, CTrDialog)

namespace
{
	/** @brief Rules shown in the dialog (HTML `learnList` shows the top 50). */
	constexpr size_t kMaxRulesShown = 50;
	/** @brief Exact keys are long; the HTML tool shows key[6..90) (84 chars). */
	constexpr size_t kMaxExactKeyChars = 84;

	/** @brief UTF-8 (ASCII) detail from the adapter as a display string. */
	String Utf8ToString(const std::string& utf8)
	{
		String out;
		Merge20Utf8Decode(utf8, out);
		return out;
	}
}

Merge20LearnDlg::Merge20LearnDlg(CWnd* pParent)
	: CTrDialog(Merge20LearnDlg::IDD, pParent)
{
}

void Merge20LearnDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MERGE20_LEARN_SOURCES, m_sourcesList);
	DDX_Control(pDX, IDC_MERGE20_LEARN_RULES, m_rulesList);
}

BEGIN_MESSAGE_MAP(Merge20LearnDlg, CTrDialog)
	ON_BN_CLICKED(IDC_MERGE20_LEARN_TOGGLE, OnToggle)
	ON_BN_CLICKED(IDC_MERGE20_LEARN_DELETE_SOURCE, OnDeleteSource)
	ON_BN_CLICKED(IDC_MERGE20_LEARN_EXPORT, OnExport)
	ON_BN_CLICKED(IDC_MERGE20_LEARN_IMPORT, OnImport)
	ON_BN_CLICKED(IDC_MERGE20_LEARN_MERGE_SOURCES, OnMergeSources)
	ON_BN_CLICKED(IDC_MERGE20_LEARN_CLEAR, OnClear)
	ON_LBN_SELCHANGE(IDC_MERGE20_LEARN_SOURCES, OnSelChangeSources)
	ON_CONTROL(CLBN_CHKCHANGE, IDC_MERGE20_LEARN_SOURCES, OnCheckChangeSources)
END_MESSAGE_MAP()

BOOL Merge20LearnDlg::OnInitDialog()
{
	__super::OnInitDialog();
	// L5 wires the export / import / merged-export / clear buttons
	// (design section 5). The merged-export and delete buttons stay
	// disabled until a source is checked (UpdateSourceButtons).
	RefreshAll();
	return TRUE;
}

void Merge20LearnDlg::RefreshAll()
{
	Merge20LearnStore& store = Merge20LearnGetStore();

	// ON/OFF state (same key as the compare options page, L1).
	const bool enabled = Merge20LearnIsEnabled();
	SetDlgItemText(IDC_MERGE20_LEARN_STATE,
		(enabled ?
			_("Current state: ON (suggestions and recording active)") :
			_("Current state: OFF (no suggestions, no recording)")).c_str());

	// Statistics: patterns / judgments / sources / candidates shown.
	// The HTML tool's "auto-judged in this analysis: N" is dropped:
	// there is no automatic application in the fork (spec section
	// 4(c)), so the live candidate count takes its place (design
	// section 3.3).
	const String patterns = strutils::to_str(static_cast<int>(store.RuleCount()));
	const String judgments = strutils::to_str(store.TotalJudgments());
	const String sources = strutils::to_str(static_cast<int>(store.GetSources().size()));
	const String candidates = strutils::to_str(Merge20LearnCountOpenCandidates());
	const String* statArgs[] = { &patterns, &judgments, &sources, &candidates };
	SetDlgItemText(IDC_MERGE20_LEARN_STATS,
		strutils::format_strings(_("Patterns: %1 / Judgments: %2 / Sources: %3 / Suggestions shown: %4"), statArgs, 4).c_str());

	// A broken learning file never blocks startup (L1); the failure
	// is reported here, when the dialog is opened.
	String loadError;
	if (Merge20LearnGetLoadError(loadError))
	{
		SetDlgItemText(IDC_MERGE20_LEARN_LOAD_ERROR,
			strutils::format_string1(_("Could not load the learning file: %1"), loadError).c_str());
		GetDlgItem(IDC_MERGE20_LEARN_LOAD_ERROR)->ShowWindow(SW_SHOW);
	}
	else
	{
		GetDlgItem(IDC_MERGE20_LEARN_LOAD_ERROR)->ShowWindow(SW_HIDE);
	}

	// Sources (HTML `learnSources()`, most judgments first), with the
	// rule / judgment counts each source contributes. Every source
	// is checked by default, like the HTML modal: the checked
	// sources are the merged-export / delete selection (L5).
	m_sourcesList.ResetContent();
	m_sourceIds.clear();
	const std::vector<Merge20LearnSourceSummary> summaries = store.GetSources();
	for (const Merge20LearnSourceSummary& summary : summaries)
	{
		const int row = m_sourcesList.AddString(
			strutils::format_string3(_("%1 (rules: %2, judgments: %3)"),
				summary.name, strutils::to_str(summary.rules), strutils::to_str(summary.n)).c_str());
		m_sourceIds.push_back(summary.id);
		if (row >= 0)
			m_sourcesList.SetCheck(row, TRUE);
	}
	if (summaries.empty())
		m_sourcesList.AddString(_("No learning data yet.").c_str());

	// Rules (HTML `learnList`): most judgments first, top 50. The
	// dominant strategy and its share come from Merge20LearnDominant,
	// the same function the matcher uses, so the list and the
	// candidate display can never disagree. The share is computed at
	// run time as an integer ratio, rounded like the HTML tool
	// (`Math.round(share * 100)`); no share is stored in the file.
	m_rulesList.ResetContent();
	std::vector<const Merge20LearnRule*> rules;
	rules.reserve(store.Rules().size());
	for (const auto& kv : store.Rules())
		rules.push_back(&kv.second);
	std::sort(rules.begin(), rules.end(),
		[](const Merge20LearnRule* a, const Merge20LearnRule* b) { return a->n > b->n; });
	if (rules.empty())
	{
		m_rulesList.AddString(_("No learning data yet. Judge a diff in the result pane to record it.").c_str());
	}
	else
	{
		size_t shown = 0;
		for (const Merge20LearnRule* pRule : rules)
		{
			if (shown >= kMaxRulesShown)
				break;
			Merge20LearnStrategy dominant = Merge20LearnStrategy::KeepNew;
			int best = 0;
			int total = 0;
			if (!Merge20LearnDominant(*pRule, dominant, best, total))
				continue;
			++shown;
			const bool isExact = pRule->key.rfind(_T("EXACT|"), 0) == 0;
			String keyText = isExact ? pRule->key.substr(6) : pRule->key;
			if (isExact && keyText.length() > kMaxExactKeyChars)
				keyText = keyText.substr(0, kMaxExactKeyChars);
			const int percent = (best * 100 + total / 2) / total;
			const String share = strutils::format_string2(_("%1 times / %2%"),
				strutils::to_str(best), strutils::to_str(percent));
			const String via = Merge20LearnViaDisplayName(isExact ? _T("exact") : _T("feat"));
			const String strategy = Merge20LearnStrategyDisplayName(dominant);
			const String* ruleArgs[] = { &via, &keyText, &strategy, &share };
			m_rulesList.AddString(
				strutils::format_strings(_("[%1] %2 - %3 (%4)"), ruleArgs, 4).c_str());
		}
	}

	UpdateSourceButtons();
}

std::vector<String> Merge20LearnDlg::GetCheckedSourceIds()
{
	std::vector<String> ids;
	const int count = m_sourcesList.GetCount();
	for (int i = 0; i < count; ++i)
	{
		if (i >= static_cast<int>(m_sourceIds.size()))
			break;
		if (m_sourcesList.GetCheck(i) != 0)
			ids.push_back(m_sourceIds[static_cast<size_t>(i)]);
	}
	return ids;
}

void Merge20LearnDlg::UpdateSourceButtons()
{
	// The delete and merged-export buttons act on the checked
	// sources; with nothing checked they stay disabled (L5).
	const bool hasChecked = !GetCheckedSourceIds().empty();
	GetDlgItem(IDC_MERGE20_LEARN_DELETE_SOURCE)->EnableWindow(hasChecked ? TRUE : FALSE);
	GetDlgItem(IDC_MERGE20_LEARN_MERGE_SOURCES)->EnableWindow(hasChecked ? TRUE : FALSE);
}

bool Merge20LearnDlg::AskExportConfirm(const String& filePath)
{
	// Spec section 4(d): the sharing notice is shown on export too,
	// not just in this dialog's footer text. The notice msgid is the
	// same one the .rc footer label uses.
	const String msg = _("Learning data contains source code fragments. Share only through approved internal channels. Do not take it outside the company or save it to a personal cloud.") +
		_T("\n\n") +
		strutils::format_string1(_("Export the learning data to:\n%1"), filePath);
	return AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void Merge20LearnDlg::OnToggle()
{
	GetOptionsMgr()->SaveOption(OPT_MERGE20_LEARNING_ENABLED, !Merge20LearnIsEnabled());
	// Switching OFF clears the candidates of all open docs, switching
	// ON recomputes them (L2, design section 3.5).
	Merge20LearnRefreshAllOpenDocs();
	RefreshAll();
}

void Merge20LearnDlg::OnDeleteSource()
{
	const std::vector<String> srcIds = GetCheckedSourceIds();
	if (srcIds.empty())
		return;
	// HTML `deleteLearnSource` / `deleteSelectedLearnSources`: only
	// the checked sources' contributions are deleted; the other
	// sources keep their judgments.
	if (srcIds.size() == 1)
	{
		const String srcId = srcIds.front();
		const Merge20LearnStore& store = Merge20LearnGetStore();
		String srcName = srcId;
		for (const Merge20LearnSourceSummary& summary : store.GetSources())
		{
			if (summary.id == srcId)
			{
				srcName = summary.name;
				break;
			}
		}
		if (AfxMessageBox(
				strutils::format_string1(_("Delete the learning data of \"%1\"? (Only this source's contribution is deleted.)"), srcName).c_str(),
				MB_YESNO | MB_ICONQUESTION) != IDYES)
			return;
	}
	else
	{
		if (AfxMessageBox(
				strutils::format_string1(_("Delete the learning data of %1 sources? (Only these sources' contributions are deleted.)"), strutils::to_str(static_cast<int>(srcIds.size()))).c_str(),
				MB_YESNO | MB_ICONQUESTION) != IDYES)
			return;
	}
	Merge20LearnStore& store = Merge20LearnGetStore();
	for (const String& srcId : srcIds)
		store.RemoveSource(srcId);
	String errStr;
	if (!Merge20LearnSaveStore(errStr))
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
	// The deletion may remove rules other docs matched; recompute.
	Merge20LearnRefreshAllOpenDocs();
	RefreshAll();
}

void Merge20LearnDlg::OnExport()
{
	Merge20LearnStore& store = Merge20LearnGetStore();
	// Default file name, as in the HTML tool (design section 4.2).
	// The save dialog (S6's SelectFile, save mode) decides the folder.
	const String defaultName = Merge20LearnDefaultExportFileName(false);
	String filePath;
	if (!SelectFile(GetSafeHwnd(), filePath, false, defaultName.c_str(), _T(""),
			_T("JSON Files (*.json)|*.json|All Files (*.*)|*.*||"), _T("json")))
		return;
	if (!AskExportConfirm(filePath))
		return;
	std::string errDetail;
	if (!Merge20LearnExportToFile(filePath, errDetail))
	{
		AfxMessageBox(
			strutils::format_string1(_("Could not export the learning data: %1"), Utf8ToString(errDetail)).c_str(),
			MB_ICONSTOP);
		return;
	}
	const String rules = strutils::to_str(static_cast<int>(store.RuleCount()));
	const String* args[] = { &rules, &filePath };
	AfxMessageBox(
		strutils::format_strings(_("Exported the learning data (%1 rules) to:\n%2"), args, 2).c_str(),
		MB_ICONINFORMATION);
}

void Merge20LearnDlg::OnImport()
{
	String filePath;
	if (!SelectFile(GetSafeHwnd(), filePath, true, nullptr, _T(""),
			_T("JSON Files (*.json)|*.json|All Files (*.*)|*.*||"), _T("json")))
		return;
	Merge20LearnFileHeader header;
	int skippedRules = 0;
	int addedRules = 0;
	int mergedRules = 0;
	std::string errDetail;
	if (!Merge20LearnImportFromFile(filePath, header, skippedRules, addedRules, mergedRules, errDetail))
	{
		AfxMessageBox(
			strutils::format_string1(_("Could not import the learning data: %1"), Utf8ToString(errDetail)).c_str(),
			MB_ICONSTOP);
		return;
	}
	// The import changes the store in memory; save it at once (HTML
	// `importLearn` saves too) and recompute the candidates, so the
	// imported rules can suggest from the next candidate computation.
	String errStr;
	const bool saved = Merge20LearnSaveStore(errStr);
	Merge20LearnRefreshAllOpenDocs();
	RefreshAll();
	if (!saved)
	{
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
		return;
	}
	// HTML `importLearn` toast: new / merged rule counts per source.
	const String added = strutils::to_str(addedRules);
	const String merged = strutils::to_str(mergedRules);
	const String* args[] = { &header.sourceName, &added, &merged };
	String msg = strutils::format_strings(_("Imported the learning data (%1: %2 new, %3 merged)"), args, 3);
	if (skippedRules > 0)
	{
		msg += _T("\n");
		msg += strutils::format_string1(_("Skipped %1 rules (invalid format)."), strutils::to_str(skippedRules));
	}
	AfxMessageBox(msg.c_str(), MB_ICONINFORMATION);
}

void Merge20LearnDlg::OnMergeSources()
{
	const std::vector<String> srcIds = GetCheckedSourceIds();
	if (srcIds.empty())
		return;
	Merge20LearnStore& store = Merge20LearnGetStore();
	// Nothing to merge when the checked sources hold no rules (the
	// merged output would be an empty file); report it instead.
	if (store.MergeSources(srcIds).empty())
	{
		AfxMessageBox(_("No learning data in the selected sources.").c_str(), MB_ICONINFORMATION);
		return;
	}
	const String defaultName = Merge20LearnDefaultExportFileName(true);
	String filePath;
	if (!SelectFile(GetSafeHwnd(), filePath, false, defaultName.c_str(), _T(""),
			_T("JSON Files (*.json)|*.json|All Files (*.*)|*.*||"), _T("json")))
		return;
	if (!AskExportConfirm(filePath))
		return;
	std::string errDetail;
	int ruleCount = 0;
	if (!Merge20LearnExportMergedToFile(srcIds, filePath, errDetail, ruleCount))
	{
		AfxMessageBox(
			strutils::format_string1(_("Could not export the learning data: %1"), Utf8ToString(errDetail)).c_str(),
			MB_ICONSTOP);
		return;
	}
	const String rules = strutils::to_str(ruleCount);
	const String sources = strutils::to_str(static_cast<int>(srcIds.size()));
	const String* args[] = { &rules, &sources, &filePath };
	AfxMessageBox(
		strutils::format_strings(_("Exported the merged learning data (%1 rules from %2 sources) to:\n%3"), args, 3).c_str(),
		MB_ICONINFORMATION);
}

void Merge20LearnDlg::OnClear()
{
	// HTML `clearLearn`: the shared JSON files are not affected.
	if (AfxMessageBox(
			_("Clear all learning data? (JSON files already shared with other workers are not affected.)").c_str(),
			MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;
	Merge20LearnStore& store = Merge20LearnGetStore();
	store.Clear();
	String errStr;
	const bool saved = Merge20LearnSaveStore(errStr);
	// The clear may remove rules other docs matched; recompute.
	Merge20LearnRefreshAllOpenDocs();
	RefreshAll();
	if (!saved)
	{
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
		return;
	}
	AfxMessageBox(_("Cleared the learning data.").c_str(), MB_ICONINFORMATION);
}

void Merge20LearnDlg::OnSelChangeSources()
{
	// Selection alone no longer drives the buttons (L5: the checked
	// state does), but a click also changes the selection, so refresh
	// the button state here too.
	UpdateSourceButtons();
}

void Merge20LearnDlg::OnCheckChangeSources()
{
	// CCheckListBox sends CLBN_CHKCHANGE when a check box is toggled
	// (also when the already-selected row's box is clicked, where no
	// selection change happens).
	UpdateSourceButtons();
}
