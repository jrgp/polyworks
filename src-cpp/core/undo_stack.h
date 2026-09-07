#pragma once
/*
 * undo_stack.h — Undo/redo stack backed by in-memory PMS snapshots.
 *
 * Original VB6 used files under appPath\undo\undoN.pwn.
 * We use in-memory byte buffers instead (simpler, equally correct).
 *
 * Usage:
 *   stack.push(doc);          // before a destructive edit
 *   stack.undo(doc);          // revert one step
 *   stack.redo(doc);          // reapply one step
 *   stack.canUndo()           // check before calling undo()
 *   stack.canRedo()
 *
 * After push() any redo history is discarded (standard linear undo).
 */

#include "map_document.h"
#include <vector>
#include <cstddef>

class UndoStack {
public:
    explicit UndoStack(int maxDepth = 64);

    /* Save the current document state. */
    void push(const MapDocument& doc);

    /* Revert to the previous state.  Returns true if successful. */
    bool undo(MapDocument& doc);

    /* Reapply the reverted state.  Returns true if successful. */
    bool redo(MapDocument& doc);

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }

    void clear();

    int undoDepth() const { return static_cast<int>(m_undoStack.size()); }
    int redoDepth() const { return static_cast<int>(m_redoStack.size()); }

private:
    int m_maxDepth;
    std::vector<std::vector<uint8_t>> m_undoStack;
    std::vector<std::vector<uint8_t>> m_redoStack;
};
