// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Undo.cpp
 *
 * @brief Implementation of the judgment undo / redo stack
 * (Merge20 best-workflow B10 PR1).
 *
 * This file stays ASCII: it stores ordinals and paths only and
 * never formats user-visible text.
 */
#include "pch.h"
#include "Merge20/Merge20Undo.h"

bool Merge20UndoEntryHasChange(const Merge20UndoEntry& entry)
{
	switch (entry.kind)
	{
	case Merge20UndoKind::ReviewState:
		return entry.oldReview != entry.newReview || entry.oldInclude != entry.newInclude;
	case Merge20UndoKind::IncludeInOutput:
		return entry.oldInclude != entry.newInclude;
	case Merge20UndoKind::DiffConclusion:
	default:
		return entry.oldConclusion != entry.newConclusion ||
			entry.oldReview != entry.newReview || entry.oldInclude != entry.newInclude;
	}
}

bool Merge20UndoEntryMatchesDiff(const Merge20UndoEntry& entry, int op, const int begin[3], const int end[3])
{
	if (entry.kind != Merge20UndoKind::DiffConclusion)
		return true;
	if (entry.diffOp != op)
		return false;
	for (int i = 0; i < 3; ++i)
	{
		if (entry.diffBegin[i] != begin[i] || entry.diffEnd[i] != end[i])
			return false;
	}
	return true;
}

void Merge20UndoStack::Push(const Merge20UndoEntry& entry)
{
	if (!Merge20UndoEntryHasChange(entry))
		return;
	m_redo.clear();
	m_undo.push_back(entry);
	while (m_undo.size() > Merge20UndoMaxDepth)
		m_undo.pop_front();
}

const Merge20UndoEntry* Merge20UndoStack::PeekUndo() const
{
	return m_undo.empty() ? nullptr : &m_undo.back();
}

const Merge20UndoEntry* Merge20UndoStack::PeekRedo() const
{
	return m_redo.empty() ? nullptr : &m_redo.back();
}

bool Merge20UndoStack::PopUndoToRedo()
{
	if (m_undo.empty())
		return false;
	m_redo.push_back(m_undo.back());
	m_undo.pop_back();
	return true;
}

bool Merge20UndoStack::PopRedoToUndo()
{
	if (m_redo.empty())
		return false;
	m_undo.push_back(m_redo.back());
	m_redo.pop_back();
	return true;
}

bool Merge20UndoStack::DiscardUndoTop()
{
	if (m_undo.empty())
		return false;
	m_undo.pop_back();
	return true;
}

bool Merge20UndoStack::DiscardRedoTop()
{
	if (m_redo.empty())
		return false;
	m_redo.pop_back();
	return true;
}

void Merge20UndoStack::Clear()
{
	m_undo.clear();
	m_redo.clear();
}
