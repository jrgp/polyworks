#include "undo_stack.h"
#include "pms_io.h"

#include <stdexcept>

UndoStack::UndoStack(int maxDepth) : m_maxDepth(maxDepth) {}

void UndoStack::push(const MapDocument& doc) {
    /* Serialize current document state to bytes. */
    PmsData snap;
    docToPmsData(doc, snap);

    std::vector<uint8_t> bytes;
    std::string err;
    if (!pmsDataToBytes(snap, bytes, err))
        throw std::runtime_error("UndoStack::push: " + err);

    /* Push, trim to maxDepth. */
    m_undoStack.push_back(std::move(bytes));
    if (static_cast<int>(m_undoStack.size()) > m_maxDepth)
        m_undoStack.erase(m_undoStack.begin());

    /* Any new edit discards the redo stack. */
    m_redoStack.clear();
}

bool UndoStack::undo(MapDocument& doc) {
    if (m_undoStack.empty()) return false;

    /* Save current state to redo stack before overwriting. */
    PmsData current;
    docToPmsData(doc, current);
    std::vector<uint8_t> currentBytes;
    std::string err;
    if (!pmsDataToBytes(current, currentBytes, err))
        return false;
    m_redoStack.push_back(std::move(currentBytes));

    /* Restore from top of undo stack. */
    auto bytes = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    PmsData restored;
    if (!pmsBytesToData(bytes, restored, err))
        return false;

    pmsDataToDoc(restored, doc);
    return true;
}

bool UndoStack::redo(MapDocument& doc) {
    if (m_redoStack.empty()) return false;

    /* Save current state back to undo stack. */
    PmsData current;
    docToPmsData(doc, current);
    std::vector<uint8_t> currentBytes;
    std::string err;
    if (!pmsDataToBytes(current, currentBytes, err))
        return false;
    m_undoStack.push_back(std::move(currentBytes));
    if (static_cast<int>(m_undoStack.size()) > m_maxDepth)
        m_undoStack.erase(m_undoStack.begin());

    /* Restore from top of redo stack. */
    auto bytes = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    PmsData restored;
    if (!pmsBytesToData(bytes, restored, err))
        return false;

    pmsDataToDoc(restored, doc);
    return true;
}

void UndoStack::clear() {
    m_undoStack.clear();
    m_redoStack.clear();
}

void UndoStack::pop() {
    if (!m_undoStack.empty())
        m_undoStack.pop_back();
}
