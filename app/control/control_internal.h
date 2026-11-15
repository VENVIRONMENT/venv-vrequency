// venv-vrequency — private helpers shared by the control socket's source files (not public).
#pragma once
#include "app/control/control.h"
#include "app/chat/chat.h"
#include "app/shell/mainwindow.h"
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <cmath>

namespace ctl {

const int kMaxLine = 1 << 20;     // longest request line
const int kMaxClients = 8;
const int kLinesPerTurn = 100;    // requests handled before the GUI gets a turn
const int kListenSecs = 45000;
// Argument reading: the first problem found is kept in `e`, and the caller refuses the request.
// Argument reading: the first problem found is kept in `e`, and the caller refuses the request.
struct Args {
    const QJsonObject &o;
    QString &e;
    bool has(const char *k) const { return o.contains(k) && !o[k].isNull(); }
    void fail(const QString &m) const { if (e.isEmpty()) e = m; }
    double num(const char *k, double lo, double hi, double def, bool required = false) const {
        if (!has(k)) { if (required) fail(QString("missing '%1'").arg(k)); return def; }
        if (!o[k].isDouble()) { fail(QString("'%1' must be a number").arg(k)); return def; }
        const double d = o[k].toDouble();
        if (!(d >= lo && d <= hi)) { fail(QString("'%1' must be between %2 and %3").arg(k).arg(lo).arg(hi)); return def; }
        return d;
    }
    int integer(const char *k, int lo, int hi, int def, bool required = false) const {
        const double d = num(k, lo, hi, def, required);
        if (has(k) && d != std::floor(d)) { fail(QString("'%1' must be a whole number").arg(k)); return def; }
        return (int)d;
    }
    QString str(const char *k, const QString &def, int maxLen, bool required = false) const {
        if (!has(k)) { if (required) fail(QString("missing '%1'").arg(k)); return def; }
        if (!o[k].isString()) { fail(QString("'%1' must be a string").arg(k)); return def; }
        const QString s = o[k].toString();
        if (s.size() > maxLen) { fail(QString("'%1' is longer than %2 characters").arg(k).arg(maxLen)); return def; }
        return s;
    }
    bool boolean(const char *k, bool def) const {
        if (!has(k)) return def;
        if (!o[k].isBool()) { fail(QString("'%1' must be true or false").arg(k)); return def; }
        return o[k].toBool();
    }
};

extern const char *loopNames[];     // the loop's states, by number

int parseRoot(const QJsonValue &v);    // a key name or 0..11, else -1
QJsonObject msgJson(const ChatMsg &m);

} // namespace ctl
