// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "magnifyingglass.h"

#include <QApplication>
#include <QBitmap>
#include <QDesktopWidget>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include <algorithm>
#include <cstdio>


MagnifyingGlass::MagnifyingGlass(QWidget *parent)
    : QLabel(parent)
{
    const QRect screenGeometry = QApplication::desktop()->screenGeometry(this);
    this->setGeometry(screenGeometry);

    this->setAttribute(Qt::WA_TranslucentBackground, true);

    this->setWindowFlags(this->windowFlags() | Qt::FramelessWindowHint);
    this->setWindowFlags(this->windowFlags() | Qt::WindowStaysOnTopHint);
    this->setWindowFlags(this->windowFlags() | Qt::ToolTip);
}

void MagnifyingGlass::shootScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if(const QWindow *window = windowHandle())
        screen = window->screen();
    if(!screen)
        return;

    currentScreenshot = screen->grabWindow(0);
}

void MagnifyingGlass::showMagnifyingGlass()
{
    QPoint p = QCursor::pos();

    showMagnifyingGlass(p.x(), p.y(), this->radius, this->factor);
}

void MagnifyingGlass::showMagnifyingGlass(int x, int y, int radius, double factor)
{
    int w = currentScreenshot.width();
    int h = currentScreenshot.height();

    // limits for the region to zoom
    int fr = radius/factor;  // factored radius
    int roi_xlo = std::max(x-fr,   (int)(x/factor));
    int roi_xhi = std::min(x+fr+1, (int)((x+w)/factor));
    int roi_ylo = std::max(y-fr,   (int)(y/factor));
    int roi_yhi = std::min(y+fr+1, (int)((y+h)/factor));

    // limits for the region after zoom
    int xlo = std::max(x-radius,   0);
    int xhi = std::min(x+radius+1, w);
    int ylo = std::max(y-radius,   0);
    int yhi = std::min(y+radius+1, h);

    // the region to scale
    QPixmap roi = currentScreenshot.copy(
            roi_xlo, roi_ylo,
            roi_xhi-roi_xlo, roi_yhi-roi_ylo);

    // scale it
    roi = roi.scaled(
            xhi-xlo, yhi-ylo,
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // apply the circular mask
    QBitmap mask(roi.size());
    mask.fill(Qt::color0);

    QPainter painter(&mask);
    painter.setBrush(Qt::color1);

    painter.drawEllipse(x-xlo-radius,y-ylo-radius,2*radius,2*radius);
    roi.setMask(mask);

    // show the image
    this->setPixmap(roi);
    this->resize(roi.width(), roi.height());
    this->move(xlo, ylo);
}

void MagnifyingGlass::runTimer()
{
    QTimer *timer = new QTimer(this);
    QObject::connect(timer, SIGNAL(timeout()), this, SLOT(showMagnifyingGlass()));
    timer->start(10);
}
