// venv-vrequency — the measures every page shares. The screen is a 720 x 720 canvas; a page's
// content sits between two 16 px margins, under the row of tabs.
#pragma once
#include <QRectF>
#include <Qt>
#include <cmath>

namespace lay {
inline constexpr qreal kCanvas = 720;                     // the canvas is square
inline constexpr qreal kMargin = 16;                      // from the edge of the canvas to a page's content
inline constexpr qreal kWidth = kCanvas - 2 * kMargin;    // 688: the width of a page's content
inline constexpr qreal kRight = kCanvas - kMargin;        // 704: where a page's content ends
inline constexpr qreal kGap = 8;                          // between two cells of a row

inline constexpr int kAlignLeft = Qt::AlignLeft | Qt::AlignVCenter;
inline constexpr int kAlignRight = Qt::AlignRight | Qt::AlignVCenter;

// Cell `i` of a row of `n` equal cells across the page, `gap` apart, on whole pixels.
inline QRectF cell(int i, int n, qreal y, qreal h, qreal gap = kGap) {
    const qreal w = (kWidth - (n - 1) * gap) / n;
    return QRectF(std::floor(kMargin + i * (w + gap)), y, std::floor(w), h);
}

// A list dragged with a finger: how many rows the drag has moved it since the last call.
struct DragScroll {
    qreal lastY = 0;
    void press(qreal y) { lastY = y; }
    int rows(qreal y, qreal rowHeight) {
        const int d = (int)std::round((y - lastY) / rowHeight);
        if (d) lastY = y;
        return d;
    }
};
} // namespace lay
