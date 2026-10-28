// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPainter>
#include <QStyledItemDelegate>

namespace flashpack {

// Draws the partition name on the left (default) and an extra, dimmed text
// (size, "logical") on the right. The extra text lives in SizeTextRole.
class PartitionDelegate : public QStyledItemDelegate {
public:
    enum { SizeTextRole = Qt::UserRole + 1 };

    using QStyledItemDelegate::QStyledItemDelegate;

    // Reserve room for the extra text so it never overlaps the name.
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        const QString extra = index.data(SizeTextRole).toString();
        if (!extra.isEmpty())
            size.rwidth() += QFontMetrics(option.font).horizontalAdvance(extra) + 32;
        return size;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);

        const QString extra = index.data(SizeTextRole).toString();
        if (extra.isEmpty())
            return;

        painter->save();
        QColor color = (option.state & QStyle::State_Selected)
                           ? option.palette.color(QPalette::HighlightedText)
                           : option.palette.color(QPalette::Text);
        color.setAlpha(150);
        painter->setPen(color);
        painter->drawText(option.rect.adjusted(8, 0, -8, 0),
                          Qt::AlignRight | Qt::AlignVCenter, extra);
        painter->restore();
    }
};

} // namespace flashpack
