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
#include "redshiftdaemon.h"

#include <QTimer>
#include <QTime>
#include <tsettings.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <quickswitch.h>
#include <Screens/screendaemon.h>
#include <Screens/systemscreen.h>
#include "redshift/colorramp.h"

struct RedshiftDaemonPrivate {
    enum RedshiftState {
        Initialising,
        Idle,
        ManualOff,
        ManualOn,
        Scheduled
    };

    QTimer* redshiftStateTimer;
    QuickSwitch* sw;

    RedshiftState state = Initialising;
    bool updatingState = false;

    tSettings settings;
};

RedshiftDaemon::RedshiftDaemon(QObject* parent) : QObject(parent) {
    d = new RedshiftDaemonPrivate();

    d->sw = new QuickSwitch("redshift");
    d->sw->setTitle("Redshift");
    connect(d->sw, &QuickSwitch::toggled, this, [ = ](bool on) {
        if (d->updatingState) return;

        //We'll only get here if the user manually toggled the switch
        d->state = on ? RedshiftDaemonPrivate::ManualOn : RedshiftDaemonPrivate::ManualOff;
        updateRedshiftState();
    });
    StateManager::statusCenterManager()->addSwitch(d->sw);

    d->redshiftStateTimer = new QTimer();
    d->redshiftStateTimer->setInterval(60000);
    connect(d->redshiftStateTimer, &QTimer::timeout, this, &RedshiftDaemon::updateRedshiftState);
    if (d->settings.value("Redshift/scheduleRedshift").toBool()) d->redshiftStateTimer->start();

    connect(&d->settings, &tSettings::settingChanged, this, [ = ](QString key, QVariant value) {
        if (key.startsWith("Redshift/")) {
            //Turn on/off the Redshift timer
            if (key == "Redshift/scheduleRedshift") {
                if (value.toBool()) {
                    d->redshiftStateTimer->start();
                } else {
                    d->redshiftStateTimer->stop();
                }
            }

            //Recalculate the Redshift state
            this->updateRedshiftState();
        }
    });

    this->updateRedshiftState();
}

RedshiftDaemon::~RedshiftDaemon() {
    StateManager::statusCenterManager()->removeSwitch(d->sw);
    d->sw->deleteLater();
    delete d;
}

void RedshiftDaemon::updateRedshiftState() {
    d->updatingState = true;
    int time = QTime::currentTime().msecsSinceStartOfDay();
    const int transitionTime = 1800000; //half an hour
    bool scheduled = d->settings.value("Redshift/scheduleRedshift").toBool();
    int intensity = d->settings.value("Redshift/intensity").toInt();

    //Time right in the middle of the transition time
    int startTime = d->settings.value("Redshift/startTime").toInt();
    int endTime = d->settings.value("Redshift/endTime").toInt();
    bool scheduledShouldBeOnMidTransitionTime = false;

    //First/last time when Redshift should be fully off
    int startTimeTransitionTime = startTime - transitionTime;
    int endTimeTransitionTime = endTime + transitionTime;
    bool scheduledShouldBeOn = false;

    //First/last time when Redshift should be fully on
    int startTimeFullTime = startTime + transitionTime;
    int endTimeFullTime = endTime - transitionTime;
    bool scheduledShouldBeOnFully = false;

    if (startTime > endTime) {
        //We're going overnight
        if (time > startTimeTransitionTime || time < endTimeTransitionTime) scheduledShouldBeOn = true;
        if (time > startTime || time < endTime) scheduledShouldBeOnMidTransitionTime = true;
        if (time > startTimeFullTime || time < endTimeFullTime) scheduledShouldBeOnFully = true;
    } else {
        if (time > startTimeTransitionTime && time < endTimeTransitionTime) scheduledShouldBeOn = true;
        if (time > startTime && time < endTime) scheduledShouldBeOnMidTransitionTime = true;
        if (time > startTimeFullTime && time < endTimeFullTime) scheduledShouldBeOnFully = true;
    }

    switch (d->state) {
        case RedshiftDaemonPrivate::Idle:
        case RedshiftDaemonPrivate::Initialising:
            if (scheduled) {
                //Set the appropriate state and recalculate the Redshift state
                d->state = RedshiftDaemonPrivate::Scheduled;
                updateRedshiftState();
            } else {
                d->state = RedshiftDaemonPrivate::Idle;
                setRedshiftTemperature(6500);
                d->sw->setChecked(false);
            }
            break;
        case RedshiftDaemonPrivate::ManualOff:
            if (scheduled && !scheduledShouldBeOn) {
                //Revert to the schedule if we've ticked into the correct time
                d->state = RedshiftDaemonPrivate::Scheduled;
            } else if (!scheduled) {
                //Revert to the idle state if we're not scheduled
                d->state = RedshiftDaemonPrivate::Idle;
                setRedshiftTemperature(6500);
            } else {
                setRedshiftTemperature(6500);
            }
            break;
        case RedshiftDaemonPrivate::ManualOn:
            if (scheduled && !scheduledShouldBeOnFully) {
                //Revert to the schedule if we've ticked into the correct time
                d->state = RedshiftDaemonPrivate::Scheduled;
            } else {
                setRedshiftTemperature(intensity);
            }
            break;
        case RedshiftDaemonPrivate::Scheduled:
            if (!scheduled) {
                //Set the appropriate state and recalculate the Redshift state
                d->state = RedshiftDaemonPrivate::Idle;
                updateRedshiftState();
            } else {
                //Calculate the appropriate Redshift temperature and set it
                if (!scheduledShouldBeOn) {
                    //Redshift shouldn't be on
                    setRedshiftTemperature(6500);
                } else if (scheduledShouldBeOnFully) {
                    //Redshift should be on fully
                    setRedshiftTemperature(intensity);
                } else {
                    //Figure out how intense the Redshift should be
                    int midIntensity;
                    if (time < startTimeFullTime && time > startTimeTransitionTime) {
                        //Use start time values
                        midIntensity = (6500 - intensity) - static_cast<int>((6500 - intensity) * static_cast<double>(time - startTimeTransitionTime) / (transitionTime * 2)) + intensity;
                    } else {
                        //Use end time values
                        midIntensity = static_cast<int>((6500 - intensity) * static_cast<double>(time - endTimeFullTime) / (transitionTime * 2)) + intensity;
                    }

                    setRedshiftTemperature(midIntensity);
                }

                //Set the Redshift switch accordingly
                d->sw->setChecked(scheduledShouldBeOn);
            }
            break;
    }

    d->updatingState = false;
}

void RedshiftDaemon::setRedshiftTemperature(int temp) {
    for (SystemScreen* screen : ScreenDaemon::instance()->screens()) {
        SystemScreen::GammaRamps ramps;
        gammaRampsForTemp(&ramps.red, &ramps.green, &ramps.blue, temp);
        screen->adjustGammaRamps("redshift", ramps);
    }
}