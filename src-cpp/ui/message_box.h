#pragma once
/*
 * message_box.h — the modal message box's state, and only its state.
 *
 * VB6 calls MsgBox and blocks until the user answers, so the code that raises
 * the question and the code that acts on it are one statement.  ImGui has no
 * nested event loop, so the question spans frames: it is raised on one frame,
 * drawn on later ones, and answered on some frame after that.  The work that
 * was going to follow is parked in Editor::pendingAfterPrompt meanwhile.
 *
 * That makes two facts about the box distinct, and they must not share a flag:
 *
 *   - is it on screen?          -> kind != None
 *   - has a button been pressed
 *     that nobody has acted on? -> answered
 *
 * Conflating them means the answer is "processed" on the very frame the box is
 * raised, before the user has seen it: the parked continuation is taken and
 * dropped, and the real button press then finds nothing to run.  The visible
 * symptom is a dialog that does nothing at all -- File > Exit with unsaved
 * changes puts up "Save changes?" and No does not exit.
 *
 * The three operations below are the whole contract, so the two flags cannot
 * drift apart again, and they are free of ImGui so they can be tested.
 */

#include <string>

namespace pw {

struct MessageBox {
    enum class Kind { None, Info, Warning, Error, Confirm, ConfirmCancel };

    Kind        kind = Kind::None;
    std::string title;
    std::string text;
    int         answer = -1;   /* 0 no, 1 yes, 2 cancel */
    bool        answered = false;

    /* Raise the box.  Explicitly *not* answered: nothing has been pressed. */
    void show(Kind k, std::string boxTitle, std::string boxText) {
        kind = k;
        title = std::move(boxTitle);
        text = std::move(boxText);
        answer = -1;
        answered = false;
    }

    bool visible() const { return kind != Kind::None; }

    /* A button was pressed.  The box comes down and the answer waits. */
    void respond(int value) {
        answer = value;
        answered = true;
        kind = Kind::None;
    }

    /* Consume a pending answer, if there is one.  Returns false on every frame
       where the user has not just pressed a button, which is what keeps a
       freshly raised box from being acted on before it is seen. */
    bool takeAnswer(int& out) {
        if (!answered) {
            return false;
        }
        answered = false;
        out = answer;
        return true;
    }
};

}  // namespace pw
