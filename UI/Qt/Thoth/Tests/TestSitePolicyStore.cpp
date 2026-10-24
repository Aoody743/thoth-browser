/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../SitePolicyStore.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>

static void check(bool condition, char const* message)
{
    if (!condition) {
        qCritical() << "FAIL:" << message;
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using Store = Thoth::SitePolicyStore;
    using Decision = Store::Decision;
    QTemporaryDir directory;
    auto path = directory.filePath("permissions.json");
    Store store(path);
    check(store.microphone("https://one.example") == Decision::Ask, "default asks");
    check(store.set_microphone("https://ONE.example:443/path", Decision::Allow), "save canonical origin");
    check(store.microphone("https://one.example/elsewhere") == Decision::Allow, "same origin shares decision");
    check(store.microphone("http://one.example") == Decision::Ask, "scheme boundary");
    check(store.microphone("https://one.example:444") == Decision::Ask, "port boundary");
    check(store.microphone("https://two.example") == Decision::Ask, "host boundary");
    check(store.camera("https://one.example") == Decision::Ask, "microphone permission never grants camera");
    check(store.set_camera("https://ONE.example:443/path", Decision::Block), "camera canonical origin saved");
    check(store.camera("https://one.example/elsewhere") == Decision::Block, "camera same-origin decision");
    check(store.camera("http://one.example") == Decision::Ask && store.camera("https://one.example:444") == Decision::Ask, "camera scheme and port boundaries");
    check(Store(path).camera("https://one.example") == Decision::Block, "camera decision survives restart");
    check(Store(path).microphone("https://one.example") == Decision::Allow, "camera writes preserve microphone decision");
    Store reloaded(path);
    check(reloaded.microphone("https://one.example") == Decision::Allow, "decision survives restart");
    check(reloaded.set_microphone("https://one.example", Decision::Block), "revoke grant");
    check(Store(path).microphone("https://one.example") == Decision::Block, "revocation persists");
    check(reloaded.set_microphone("https://one.example", Decision::Ask), "reset");
    check(Store(path).microphone("https://one.example") == Decision::Ask, "reset persists");
    check(Store(path).camera("https://one.example") == Decision::Block, "microphone reset preserves camera decision");
    check(reloaded.reset_permissions("https://one.example"), "reset both permissions atomically");
    check(Store(path).camera("https://one.example") == Decision::Ask && Store(path).microphone("https://one.example") == Decision::Ask, "both resets persist");
    check(!store.set_camera("file:///private", Decision::Allow), "camera rejects local origins");
    check(!store.set_microphone("null", Decision::Allow) && !store.set_microphone("file:///private", Decision::Allow), "opaque and local origins cannot persist grants");
    check(!store.set_microphone("https://user:password@one.example", Decision::Allow), "credential URL rejected");
    auto private_a = Store::shared("private-fixture", {});
    auto private_b = Store::shared("private-fixture", {});
    check(private_a->set_microphone("https://one.example", Decision::Allow), "private decision stored in memory");
    check(private_b->microphone("https://one.example") == Decision::Allow, "private tabs share their session");
    check(Store::shared("other-private-fixture", {})->microphone("https://one.example") == Decision::Ask, "private sessions isolated");
    check(private_a->set_camera("https://one.example", Decision::Allow), "private camera permission stored");
    check(private_b->camera("https://one.example") == Decision::Allow, "private camera permission shared");
    private_a.clear();
    private_b.clear();
    check(Store::shared("private-fixture", {})->microphone("https://one.example") == Decision::Ask && Store::shared("private-fixture", {})->camera("https://one.example") == Decision::Ask, "private decisions vanish after last owner");
    Store unwritable(directory.path());
    check(!unwritable.set_microphone("https://one.example", Decision::Allow), "failed disk write reported");
    check(unwritable.microphone("https://one.example") == Decision::Ask, "failed write does not change policy");
    check(!unwritable.set_camera("https://one.example", Decision::Allow), "camera failed disk write reported");
    check(unwritable.camera("https://one.example") == Decision::Ask, "failed camera write grants nothing");
    QFile legacy(directory.filePath("legacy.json"));
    check(legacy.open(QIODevice::WriteOnly), "open legacy fixture");
    legacy.write("{\"schema\":1,\"microphone\":{\"https://one.example\":true}}");
    legacy.close();
    Store legacy_store(legacy.fileName());
    check(legacy_store.microphone("https://one.example") == Decision::Allow && legacy_store.camera("https://one.example") == Decision::Ask, "legacy microphone grant never grants camera");
    check(legacy_store.set_camera("https://one.example", Decision::Block), "upgrade legacy policy");
    check(Store(legacy.fileName()).microphone("https://one.example") == Decision::Allow, "legacy microphone survives camera policy upgrade");
    QFile broken(path);
    check(broken.open(QIODevice::WriteOnly | QIODevice::Truncate), "open corrupt fixture");
    broken.write("{truncated");
    broken.close();
    check(Store(path).microphone("https://one.example") == Decision::Ask && Store(path).camera("https://one.example") == Decision::Ask, "corrupt file cannot grant access");
    qInfo() << "PASS: persistent permission decisions, origin boundaries, revocation, private lifetime and failure handling";
}
