// venv-vrequency — which output device is PipeWire really sending us to? Asked of wpctl every few
// seconds, so the player can be told when it is the dummy device or a (late) Bluetooth one.
#pragma once
#include <QObject>
#include <QString>

class SinkProbe : public QObject {
public:
    void start();                            // ask now, then every four seconds

    QString description;                     // empty until the first answer
    bool dummy = false, bluetooth = false;
    int generation = 0;                      // bumps when the output device type changes

private:
    void probe();
};
