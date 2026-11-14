// venv-vrequency — a page: one of the nine screens behind the tabs. It owns its state, draws
// itself with the toolkit (registering its touch zones as it goes) and may take keys.
#pragma once
#include "app/shell/part.h"

class Page : public Part {
public:
    using Part::Part;
    virtual ~Page() = default;
    virtual void paint() = 0;
    virtual bool key(int /*qtKey*/, int /*plainAct*/) { return false; }  // true: the page used the key
    virtual void disarm() {}                 // the page is being left: every armed mode is dropped
};
