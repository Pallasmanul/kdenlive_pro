

#include "manimwidget.hpp"
#include "core.h"
#include "kdenlivesettings.h"

#include <KFileItem>
#include <KLocalizedString>
#include <KMessageBox>
#include <KRecentDirs>
#include <KSelectAction>
#include <KSqueezedTextLabel>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDatabase>
#include <QIcon>
#include <QInputDialog>
#include <QMenu>
#include <QProgressDialog>
#include <QToolBar>
#include <QtConcurrent>

ManimWidget::ManimWidget(QWidget *parent)
    : QWidget(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
}

ManimWidget::~ManimWidget() {}
