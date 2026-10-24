/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#include "../NavigationButton.h"
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QMouseEvent>
#include <cstdlib>

static void check(bool condition, char const* message)
{
    if (!condition) {
        qCritical() << "FAIL:" << message;
        std::exit(1);
    }
}
static void wait(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
static void mouse(Thoth::NavigationButton& button, QEvent::Type type, Qt::MouseButton which)
{
    auto center = button.rect().center();
    QMouseEvent event(type, center, button.mapToGlobal(center), which,
        type == QEvent::MouseButtonPress ? which : Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&button, &event);
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Thoth::NavigationButton button;
    button.setFixedSize(40, 40);
    button.setFocusPolicy(Qt::NoFocus);
    button.setIconSize({ 18, 18 });
    QDir assets(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../Icons/lucide"));
    QAction action("Reload");
    action.setIcon(QIcon(assets.filePath("rotate-cw-regular.png")));
    button.setDefaultAction(&action);
    button.set_loading_icon(QIcon(assets.filePath("loader-circle-regular.png")));
    int triggered = 0;
    QObject::connect(&action, &QAction::triggered, [&] { ++triggered; });
    button.show();
    app.processEvents();
    auto idle = button.grab().toImage();
    auto size = button.size();
    check(!button.menu() && !button.autoRepeat() && !button.isCheckable(), "navigation must have one non-toggle action without a left-click popup");
    mouse(button, QEvent::MouseButtonPress, Qt::LeftButton);
    wait(750);
    check(triggered == 0, "holding a button must neither repeat nor trigger a second interaction mode");
    mouse(button, QEvent::MouseButtonRelease, Qt::LeftButton);
    check(triggered == 1, "one press and release must trigger exactly one reload");
    auto acknowledged = button.grab().toImage();
    check(acknowledged != idle, "a completed click must leave visible acknowledgement");
    wait(300);
    check(button.grab().toImage() == idle, "click feedback must settle back to the same idle appearance");
    button.set_loading(true);
    auto busy = button.grab().toImage();
    wait(120);
    check(button.grab().toImage() != busy, "loading must show an animated library icon");
    check(button.size() == size && triggered == 1, "loading feedback must not move the target or trigger another reload");
    button.set_loading(false);
    check(button.grab().toImage() == idle, "completion must restore the reload icon");
    mouse(button, QEvent::MouseButtonPress, Qt::RightButton);
    mouse(button, QEvent::MouseButtonRelease, Qt::RightButton);
    check(triggered == 1, "right click must not execute the primary navigation action");
    action.setEnabled(false);
    mouse(button, QEvent::MouseButtonPress, Qt::LeftButton);
    mouse(button, QEvent::MouseButtonRelease, Qt::LeftButton);
    check(triggered == 1, "disabled history controls must not execute navigation");
    qInfo() << "PASS: one click, one action, one acknowledgement and stable loading feedback";
}
