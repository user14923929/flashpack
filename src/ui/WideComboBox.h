// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractItemView>
#include <QComboBox>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollBar>
#include <QStyle>

namespace flashpack {

// QComboBox whose popup is at least as wide as its widest item, even when the
// combo itself is narrow (Qt sizes the popup to the combo by default).
class WideComboBox : public QComboBox {
public:
    using QComboBox::QComboBox;

    void showPopup() override
    {
        QComboBox::showPopup();

        QWidget *container = view()->window();
        const int frame = 2 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, this);
        const int wanted = view()->sizeHintForColumn(0)
                           + view()->verticalScrollBar()->sizeHint().width() + frame + 8;
        if (wanted <= container->width())
            return;

        container->resize(wanted, container->height());

        // Keep the widened popup on screen.
        const QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
        if (scr) {
            const QRect avail = scr->availableGeometry();
            if (container->x() + wanted > avail.right())
                container->move(qMax(avail.left(), avail.right() - wanted), container->y());
        }
    }
};

} // namespace flashpack
