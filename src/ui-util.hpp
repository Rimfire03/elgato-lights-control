#pragma once

#include <QLayout>
#include <QString>
#include <QWidget>

// Source strings are UTF-8 literals; this keeps accented text readable in the code.
inline QString U(const char *s)
{
	return QString::fromUtf8(s);
}

// Removes and schedules deletion of every widget in a layout.
inline void clearLayout(QLayout *layout)
{
	while (QLayoutItem *item = layout->takeAt(0)) {
		if (QWidget *w = item->widget())
			w->deleteLater();
		delete item;
	}
}
