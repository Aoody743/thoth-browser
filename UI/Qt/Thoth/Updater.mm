/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#include "Updater.h"
#include "Localization.h"
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QPushButton>
#import <Sparkle/Sparkle.h>

@interface ThothUpdaterDelegate : NSObject <SPUUpdaterDelegate>
@property(copy) void (^installUpdate)(void);
@end

namespace {
SPUStandardUpdaterController* controller;
ThothUpdaterDelegate* delegate;
bool updater_started = false;
std::function<bool()> prepare_restart;
std::function<void()> restart_cancelled;
bool prepare_for_restart()
{
    return !prepare_restart || prepare_restart();
}
}

@implementation ThothUpdaterDelegate
- (BOOL)updater:(SPUUpdater*)updater willInstallUpdateOnQuit:(SUAppcastItem*)item immediateInstallationBlock:(void (^)(void))handler
{
    (void)updater;
    (void)item;
    self.installUpdate = handler;
    auto* notice = new QMessageBox(QMessageBox::Information, Thoth::localized_text("Update Ready"),
        Thoth::localized_text("An update is ready. Restart to install it, or continue browsing and update later."), QMessageBox::NoButton, QApplication::activeWindow());
    notice->setAttribute(Qt::WA_DeleteOnClose);
    auto* restart = notice->addButton(Thoth::localized_text("Restart and Install Update"), QMessageBox::AcceptRole);
    notice->addButton(Thoth::localized_text("Later"), QMessageBox::RejectRole);
    QObject::connect(notice, &QMessageBox::buttonClicked, notice, [self, restart](QAbstractButton* button) {
        if (button == restart && self.installUpdate && prepare_for_restart())
            self.installUpdate();
    });
    notice->open();
    return YES;
}
- (BOOL)updater:(SPUUpdater*)updater shouldPostponeRelaunchForUpdate:(SUAppcastItem*)item untilInvokingBlock:(void (^)(void))handler
{
    (void)updater;
    (void)item;
    if (prepare_for_restart())
        return NO;
    // A manual Sparkle install can reach here while ordinary file downloads run.
    // Keep the browser open and retry once they finish; do not interrupt them.
    auto* timer = new QTimer(qApp);
    QObject::connect(timer, &QTimer::timeout, timer, [timer, handler] {
        if (prepare_for_restart()) {
            timer->stop();
            timer->deleteLater();
            handler();
        }
    });
    timer->start(1000);
    return YES;
}
- (void)updater:(SPUUpdater*)updater didAbortWithError:(NSError*)error
{
    (void)updater;
    (void)error;
    self.installUpdate = nil;
    if (restart_cancelled)
        restart_cancelled();
}
@end

namespace Thoth {
void add_update_menu(QMenu& menu, std::function<bool()> callback, std::function<void()> cancelled)
{
    prepare_restart = std::move(callback);
    restart_cancelled = std::move(cancelled);
    if (!controller) {
        delegate = [ThothUpdaterDelegate new];
        controller = [[SPUStandardUpdaterController alloc] initWithStartingUpdater:NO updaterDelegate:delegate userDriverDelegate:nil];
        NSString* feed = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"SUFeedURL"];
        NSString* key = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"SUPublicEDKey"];
        if (feed.length && key.length)
            {
            NSError* error = nil;
            updater_started = [controller.updater startUpdater:&error];
        }
    }
    auto* updates = menu.addMenu(localized_text("Updates"));
    updates->setObjectName("ThothUpdatesMenu");
    auto* check = updates->addAction(localized_text("Check for Updates…"));
    check->setMenuRole(QAction::ApplicationSpecificRole);
    QObject::connect(check, &QAction::triggered, updates, [] {
        if (!updater_started) {
            QMessageBox::information(QApplication::activeWindow(), localized_text("Updates"), localized_text("This build has no update feed. Install a release with updates configured."));
            return;
        }
        [controller checkForUpdates:nil];
    });
    auto* automatic = updates->addAction(localized_text("Automatically Check for Updates"));
    automatic->setCheckable(true);
    QObject::connect(automatic, &QAction::triggered, updates, [](bool enabled) { controller.updater.automaticallyChecksForUpdates = enabled; });
    auto* download = updates->addAction(localized_text("Automatically Download Updates"));
    download->setCheckable(true);
    QObject::connect(download, &QAction::triggered, updates, [](bool enabled) { controller.updater.automaticallyDownloadsUpdates = enabled; });
    auto* restart = updates->addAction(localized_text("Restart and Install Update"));
    QObject::connect(restart, &QAction::triggered, updates, [] {
        if (!delegate.installUpdate)
            return;
        if (!prepare_for_restart()) {
            QMessageBox::information(QApplication::activeWindow(), localized_text("Updates"), localized_text("Wait for active downloads to finish before restarting to update."));
            return;
        }
        delegate.installUpdate();
    });
    auto* version = updates->addAction(QString::fromUtf8([[[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"] UTF8String])
        + " · " + QString::fromUtf8([[[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"] UTF8String]));
    version->setEnabled(false);
    QObject::connect(updates, &QMenu::aboutToShow, updates, [check, automatic, download, restart] {
        bool started = updater_started;
        check->setEnabled(!started || controller.updater.canCheckForUpdates);
        automatic->setEnabled(started);
        download->setEnabled(started);
        automatic->setChecked(controller.updater.automaticallyChecksForUpdates);
        download->setChecked(controller.updater.automaticallyDownloadsUpdates);
        restart->setEnabled(delegate.installUpdate != nil);
    });
}
}
