// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QLabel>
#include <QWidget>


class MagnifyingGlass : public QLabel
{
    Q_OBJECT

public:
    MagnifyingGlass(QWidget *parent);

public slots:
    void shootScreen();

    void showMagnifyingGlass();

    void showMagnifyingGlass(int x, int y, int radius, double factor);

    void runTimer();

protected:

private:
    int radius = 200;
    double factor = 2;

    QPixmap currentScreenshot;
};
