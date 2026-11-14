// venv-vrequency — `vrequency --dump-zones`: prints every real zone of every tour page.
#include "app/tour/tour.h"
#include <QDir>
#include <cstdio>
#include <set>

int tourDumpZones(const QString &picDir) {
    std::vector<TourView::Pic> pics;
    tourBuildPictures(pics);
    if (!picDir.isEmpty()) {
        QDir().mkpath(picDir);
        for (int i = 0; i < (int)pics.size(); ++i) pics[i].img.save(QString("%1/%2%3.png").arg(picDir, TourView::pageInfo(TourView::picPage(i)).id).arg(i >= TourView::pageCount() ? QString("-v%1").arg(i) : QString()));
    }
    std::printf("# every real zone of every tour page, from the app's own screens (vrequency --dump-zones)\n");
    std::set<QString> lines;
    for (int i = 0; i < (int)pics.size(); ++i) {
        for (const TourView::Hot &h : pics[i].hot) if (h.real) lines.insert(QString("%1 %2").arg(TourView::pageInfo(TourView::picPage(i)).id, h.name));
    }
    for (const QString &l : lines) std::printf("%s\n", l.toUtf8().constData());
    return 0;
}

