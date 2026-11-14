// venv-vrequency — guided help. What lets the copilot (Rook) teach on the real screen: a highlight
// overlay that rings the controls being talked about, and a chat dock that lets the player answer
// on whatever page they are on. It also knows what a question is about: the highlighted controls,
// the ones the last message mentioned, or the one touched last.
#pragma once
#include "app/shell/part.h"
#include "app/ui/layout.h"
#include <QHash>
#include <QRectF>
#include <QString>
#include <QStringList>

class Chat;

class Guide : public Part {
public:
    Guide(Shell &s, Chat &c) : Part(s), chat(c) {}

    // ---- highlights ----
    // Highlights the named zones of the current page (names may be aliases or `prefix*`); returns the ones
    // that exist and puts the others in `unknown`. The zones must have been drawn first.
    QStringList setHighlight(const QStringList &names, const QString &label, int ms, QStringList &unknown);
    void clearHighlight();
    QStringList highlightedZones();          // active highlights on this page
    QRectF highlightCaptionRect() const { return hlCaption; }
    QString highlightLabel() const { return hl.label; }
    qint64 highlightEnds() const { return hl.until; }
    void paintHighlights();
    QString zoneLabel(const QString &name) const;   // a zone's name in words, from what is drawn on it
    const QHash<QString, QString> *rowLabels = nullptr; // the Help page's rows: zone -> description

    // ---- the chat dock ----
    bool dockIsOpen() const { return dockOpen; }
    bool dockHasFocus() const { return dockOpen && dockFocus && sh.page() != PG_CHAT; }
    QRectF dockRect() const { return dockOpen && sh.page() != PG_CHAT ? dockR : QRectF(); }
    void dockOpenFocus();
    void dockClose();
    void layoutDock();
    void paintDock();
    bool dockOpen = false, dockFocus = false;

    // ---- what is being discussed ----
    QStringList focus();                     // what a message sent now would carry as `focus`
    QString dockTopic() const { return topicLabel; }
    QString lastQuestionZone() const { return lastQuestion; }
    void noteTap(const QString &zone);       // the player touched a control of the page
    void beginFrame();                       // highlights, topics and taps belong to the page they were made on
    void agentSpoke();                       // a message with no highlight just before it is about nothing in particular
    // a question started from a tour card: about `zone`, typed but not sent
    void askFromTour(const QString &tourPage, const QString &zone);

    struct Hl { QStringList zones; QString label; qint64 until = 0, issued = -100000; int page = -1; };
    Hl hl;
    QString topicLabel; QStringList topicZones; int topicPage = -1;  // what the last agent message was about
    QString lastTapped, lastQuestion; int lastTappedPage = -1;
    QStringList askFocus; QString askPage, askVia;                   // a question started from the tour: sent with via "tour"

private:
    Chat &chat;
    QRectF hlCaption, dockR;
    int dockLines = 0;
    lay::DragScroll dockDrag;

    QStringList expandZoneNames(const QStringList &names, QStringList &unknown) const;
};
