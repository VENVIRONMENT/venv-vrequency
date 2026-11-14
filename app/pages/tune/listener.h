// venv-vrequency — Listen: six seconds of the microphone, analysed for key and tempo on a thread of
// its own, so the page keeps drawing. The Tune page shows the result and applies it.
#pragma once
#include "app/shell/part.h"
#include "core/analysis/analysis.h"
#include <QString>
#include <atomic>
#include <mutex>
#include <thread>

class Listener : public Part {
public:
    using Part::Part;
    ~Listener();

    void start();
    void apply(bool key, bool tempo);        // use the guess shown: the key, the tempo or both

    std::atomic<int> state{0};               // 0 idle, 1 listening, 2 result, 3 failed
    std::mutex mx;                           // guards result and error
    td::ListenResult result;
    QString error;
    int guess = 0;                           // which of the three key guesses is shown
    qint64 started = 0;

private:
    std::thread thread;
};
