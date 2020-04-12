/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2020 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#include "barwindow.h"
#include "ui_barwindow.h"

#include <QScreen>
#include <QGraphicsOpacityEffect>
#include <Wm/desktopwm.h>

#include "mainbarwidget.h"
#include "statuscenter/statuscenter.h"
#include <tvariantanimation.h>

#include <statemanager.h>
#include <statuscentermanager.h>
#include <barmanager.h>

struct BarWindowPrivate {
    MainBarWidget* mainBarWidget;
    StatusCenter* statusCenterWidget;

    tVariantAnimation* heightAnim;
    tVariantAnimation* barStatusCenterTransitionAnim;

    QGraphicsOpacityEffect* mainBarOpacity;
    QGraphicsOpacityEffect* statusCenterOpacity;

    QScreen* oldPrimaryScreen = nullptr;

    bool expanding = true;
};

BarWindow::BarWindow(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::BarWindow) {

    ui->setupUi(this);
    d = new BarWindowPrivate();

    d->mainBarWidget = new MainBarWidget(this);
    connect(d->mainBarWidget, &MainBarWidget::expandedHeightChanged, this, &BarWindow::barHeightChanged);
    connect(d->mainBarWidget, &MainBarWidget::statusBarHeight, this, &BarWindow::barHeightChanged);

    d->statusCenterWidget = new StatusCenter(this);

    d->mainBarOpacity = new QGraphicsOpacityEffect(d->mainBarWidget);
    d->mainBarOpacity->setEnabled(false);
    d->mainBarWidget->setGraphicsEffect(d->mainBarOpacity);

    d->statusCenterOpacity = new QGraphicsOpacityEffect(d->statusCenterWidget);
    d->statusCenterOpacity->setEnabled(false);
    d->statusCenterWidget->setGraphicsEffect(d->statusCenterOpacity);
    d->statusCenterWidget->setVisible(false);

    d->heightAnim = new tVariantAnimation(this);
    d->heightAnim->setDuration(500);
    d->heightAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(d->heightAnim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
        this->setFixedHeight(value.toInt() + 1);
    });

    d->barStatusCenterTransitionAnim = new tVariantAnimation();
    d->barStatusCenterTransitionAnim->setStartValue(0.0);
    d->barStatusCenterTransitionAnim->setEndValue(1.0);
    d->barStatusCenterTransitionAnim->setDuration(250);
    d->barStatusCenterTransitionAnim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(d->barStatusCenterTransitionAnim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
        double percentage = value.toDouble();
        if (qFuzzyIsNull(percentage)) { //Fully show bar view
            d->statusCenterWidget->setVisible(false);
            d->mainBarWidget->setVisible(true);
            d->statusCenterOpacity->setEnabled(false);
            d->mainBarOpacity->setEnabled(false);
            this->setFixedHeight(d->mainBarWidget->expandedHeight());
        } else if (qFuzzyCompare(percentage, 1)) { //Fully show Status Center view
            d->statusCenterWidget->setVisible(true);
            d->mainBarWidget->setVisible(false);
            d->statusCenterOpacity->setEnabled(false);
            d->mainBarOpacity->setEnabled(false);
            this->setFixedHeight(d->statusCenterWidget->height() + 1);
        } else {
            if (percentage < 0.5) { //Animate the bar
                d->mainBarOpacity->setOpacity(1 - (percentage * 2));
                d->mainBarOpacity->setEnabled(true);
                d->mainBarWidget->setVisible(true);
                d->statusCenterWidget->setVisible(false);
                d->statusCenterOpacity->setEnabled(false);
            } else { //Animate the Status Center
                d->statusCenterOpacity->setOpacity(percentage * 2 - 1);
                d->statusCenterOpacity->setEnabled(true);
                d->statusCenterWidget->setVisible(true);
                d->mainBarWidget->setVisible(false);
                d->mainBarOpacity->setEnabled(false);
            }
            this->setFixedHeight((d->statusCenterWidget->height() - d->mainBarWidget->expandedHeight()) * percentage + d->mainBarWidget->expandedHeight());
        }
    });

    //Tell the window manager that this is a "taskbar" type window
    DesktopWm::instance()->setSystemWindow(this, DesktopWm::SystemWindowTypeTaskbar);
    this->barHeightChanged();

    connect(qApp, &QApplication::primaryScreenChanged, this, &BarWindow::updatePrimaryScreen);
    updatePrimaryScreen();

    connect(StateManager::statusCenterManager(), &StatusCenterManager::showStatusCenter, this, &BarWindow::showStatusCenter);
    connect(StateManager::statusCenterManager(), &StatusCenterManager::hideStatusCenter, this, &BarWindow::hideStatusCenter);

    ui->line->raise();
}

BarWindow::~BarWindow() {
    delete ui;
    delete d;
}

void BarWindow::resizeEvent(QResizeEvent* event) {
    ui->line->setGeometry(0, this->height() - 1, this->width(), this->height());
    d->mainBarWidget->setFixedWidth(this->width());
    d->mainBarWidget->barHeightChanged(this->height() - 1);
    d->statusCenterWidget->setFixedWidth(this->width());

    StateManager::barManager()->setBarHeight(this->height());
}

void BarWindow::enterEvent(QEvent* event) {
    //If we're showing the status bar, don't touch the height
    if (!StateManager::statusCenterManager()->isShowingStatusCenter()) {
        QSignalBlocker blocker(d->heightAnim);
        d->heightAnim->setStartValue(this->height() - 1);
        d->heightAnim->setEndValue(d->mainBarWidget->expandedHeight());

        d->heightAnim->stop();
        d->heightAnim->start();
    }
}

void BarWindow::leaveEvent(QEvent* event) {
    //If we're showing the status bar, don't touch the height
    if (!StateManager::statusCenterManager()->isShowingStatusCenter()) {
        QSignalBlocker blocker(d->heightAnim);
        d->heightAnim->setStartValue(this->height() - 1);
        d->heightAnim->setEndValue(d->mainBarWidget->statusBarHeight());

        d->heightAnim->stop();
        d->heightAnim->start();
    }
}

void BarWindow::updatePrimaryScreen() {
    if (d->oldPrimaryScreen) {
        disconnect(d->oldPrimaryScreen, &QScreen::geometryChanged, this, &BarWindow::updatePrimaryScreen);
    }

    QScreen* primaryScreen = qApp->primaryScreen();
    connect(primaryScreen, &QScreen::geometryChanged, this, &BarWindow::updatePrimaryScreen);
    d->oldPrimaryScreen = primaryScreen;

    this->setFixedWidth(primaryScreen->geometry().width());
    this->move(primaryScreen->geometry().topLeft());
    d->statusCenterWidget->setFixedHeight(primaryScreen->geometry().height());
}

void BarWindow::barHeightChanged() {
    QSignalBlocker blocker(d->heightAnim);
    d->heightAnim->setStartValue(this->height() - 1);
    d->heightAnim->setEndValue(d->expanding ? d->mainBarWidget->expandedHeight() : d->mainBarWidget->statusBarHeight());

    d->heightAnim->stop();
    d->heightAnim->start();
    d->mainBarWidget->setFixedHeight(d->mainBarWidget->expandedHeight());

    DesktopWm::setScreenMarginForWindow(this, qApp->primaryScreen(), Qt::TopEdge, d->mainBarWidget->statusBarHeight() + 1);
}

void BarWindow::showStatusCenter() {
    QSignalBlocker blocker(d->heightAnim);
    d->heightAnim->stop();

    StateManager::statusCenterManager()->setIsShowingStatusCenter(true);
    d->barStatusCenterTransitionAnim->setDirection(tVariantAnimation::Forward);
    d->barStatusCenterTransitionAnim->start();
}

void BarWindow::hideStatusCenter() {
    StateManager::statusCenterManager()->setIsShowingStatusCenter(false);
    d->barStatusCenterTransitionAnim->setDirection(tVariantAnimation::Backward);
    d->barStatusCenterTransitionAnim->start();
}

