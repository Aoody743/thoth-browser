/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#include "../SidebarAccessibility.h"
#include <QAccessible>
#include <QApplication>
#include <QTreeWidget>
#include <QToolButton>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Thoth::install_sidebar_accessibility();
    QTreeWidget tree;
    tree.setObjectName("ThothPageTree");
    tree.setColumnCount(2);
    auto* first = new QTreeWidgetItem(&tree, { "First page" });
    first->setData(0, Qt::UserRole, "page-1");
    auto* second = new QTreeWidgetItem(&tree, { "Second page" });
    second->setData(0, Qt::UserRole, "page-2");
    auto* mute = new QToolButton(&tree);
    mute->setAccessibleName("Mute Second page");
    tree.setItemWidget(second, 1, mute);
    auto* accessible = QAccessible::queryAccessibleInterface(&tree);
    if (!accessible || accessible->role() != QAccessible::List || accessible->childCount() != 3 || accessible->tableInterface())
        return 1;
    auto* retained = accessible->child(0);
    if (!retained || retained->text(QAccessible::Name) != "First page" || !retained->actionInterface())
        return 2;
    int activations = 0;
    QObject::connect(&tree, &QTreeWidget::itemActivated, &tree, [&](auto*, int) { ++activations; });
    retained->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    if (activations != 1)
        return 3;
    tree.clear();
    // macOS can still query a previously returned child after the model clears.
    if (retained->isValid() || !retained->state().invalid || !retained->text(QAccessible::Name).isEmpty() || !retained->rect().isEmpty())
        return 4;
    auto* replacement = new QTreeWidgetItem(&tree, { "First page renamed" });
    replacement->setData(0, Qt::UserRole, "page-1");
    if (accessible->childCount() != 1 || accessible->child(0) != retained || retained->text(QAccessible::Name) != "First page renamed")
        return 5;
    return 0;
}
