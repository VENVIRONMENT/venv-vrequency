// venv-vrequency — a page: one of the nine screens behind the tabs. It owns its state, draws
// itself with the toolkit (registering its touch zones as it goes) and may take keys.
#pragma once
#include "app/shell/part.h"

class Page : public Part {
public:
    using Part::Part;
    virtual ~Page() = default;
    virtual void paint() = 0;
    virtual bool tabs() const { return true; }   // the row of tabs is drawn above the page
    // A key went down while the page is showing (not during the key test). True: the page used it.
    // It is asked before the command layer, so a page that leaves Ctrl+key alone must say so itself.
    virtual bool key(int /*qtKey*/, int /*plainAct*/) { return false; }
    virtual void tick() {}                       // every frame, whichever page is showing
    virtual void disarm() {}                     // the page changed: every armed mode is dropped
};
