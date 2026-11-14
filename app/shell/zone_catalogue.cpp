// venv-vrequency — the catalogue of every named touch zone of a page, for the control socket.
#include "app/shell/mainwindow.h"
#include "app/guide/zone_names.h"

QVector<MainWindow::ZoneInfo> MainWindow::zoneCatalogue(int page, int helpSub) {
    // draw the page off-screen, read its zones, and put everything back the way it was
    const int sPg = pg, sHelp = helpPage, sTopicPage = guide.topicPage, sTapPage = guide.lastTappedPage;
    const bool sDeep = deepEdit;
    const Guide::Hl sHl = guide.hl;
    const QString sTopic = guide.topicLabel, sTap = guide.lastTapped, sSel = helpSel;
    const QStringList sTopicZ = guide.topicZones;
    const bool sTourSusp = tourSuspended;
    tourSuspended = true;                                   // the page itself is wanted, not the tour's picture of it
    if (page != pg) { pg = page; deepEdit = false; }
    if (page == PG_HELP && helpSub >= 0) helpPage = qBound(0, helpSub, 3);
    guide.hl.page = pg; guide.hl.zones = sHl.page == page ? sHl.zones : QStringList(); guide.hl.until = sHl.until;
    guide.topicPage = pg;
    shot();
    QVector<ZoneInfo> out;
    const QRectF D = guide.dockRect();
    for (const Ui::Zone &z : ui.zones()) {
        ZoneInfo zi;
        zi.name = z.name; zi.r = z.r; zi.label = guide.zoneLabel(z.name);
        zi.covered = !D.isNull() && !hiddenZone(z.name) && D.intersects(z.r);
        out.push_back(zi);
    }
    pg = sPg; helpPage = sHelp; deepEdit = sDeep; guide.hl = sHl;
    tourSuspended = sTourSusp;
    guide.topicPage = sTopicPage; guide.topicLabel = sTopic; guide.topicZones = sTopicZ; guide.lastTapped = sTap; guide.lastTappedPage = sTapPage; helpSel = sSel;
    shot();
    return out;
}
