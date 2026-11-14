// venv-vrequency — the player's sounds: the lead sound being played, the ten slots, the undo
// steps and the favourites on disk. The pages, the keys and the control socket all change sounds
// through here, so the engine and the screen never disagree about which sound is the lead.
#pragma once
#include "app/shell/part.h"
#include "app/store/store.h"
#include "core/dsp.h"
#include <QVector>
#include <vector>

class QJsonObject;

class Sounds : public Part {
public:
    explicit Sounds(Shell &s);

    td::Patch lead;
    td::Patch bank[10];
    int curSlot = 0;                         // the slot the lead came from, or -1 once it was changed
    std::vector<td::Patch> undo;
    QVector<store::NamedPatch> favs;
    int extrasSeeded = 0;                    // how many of the engine's extra sounds have been offered as favourites
    bool slotSave = false;                   // Store is armed: the next slot chosen takes the lead sound

    void load(const QJsonObject &state);     // from the state file
    void save(QJsonObject &state) const;
    void pushUndo();
    void applyLead(bool resetSlot);          // hand the lead sound to the engine
    void selectSlot(int i);
    void randomise();
    void mutate();
    void undoSound();
    void saveFavourite();
    void recall(const td::Patch &p);         // a favourite becomes the lead; one undo step
    void setParam(int p, float v);
    void factoryReset();                     // slots 1-0 back to the built-in sounds
    int addExtraSounds(int from = 0);        // the engine's extra sounds -> favourites; returns files written

private:
    td::Rng rng;
};
