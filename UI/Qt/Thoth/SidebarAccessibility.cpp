/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#include "SidebarAccessibility.h"
#include <QAccessibleWidget>
#include <QHash>
#include <QPointer>
#include <QTreeWidget>

namespace Thoth {
namespace {
constexpr int id_role = Qt::UserRole;
constexpr int action_role = Qt::UserRole + 9;

QString row_id(QTreeWidgetItem* item)
{
    auto id = item->data(0, id_role).toString();
    return id.isEmpty() && item->data(0, action_role).toString() == "newtab" ? QStringLiteral("/newtab") : id;
}

QList<QTreeWidgetItem*> visible_rows(QTreeWidget* tree)
{
    QList<QTreeWidgetItem*> rows;
    if (!tree)
        return rows;
    auto collect = [&](auto&& self, QTreeWidgetItem* parent) -> void {
        for (int i = 0; i < parent->childCount(); ++i) {
            auto* item = parent->child(i);
            if (item->isHidden())
                continue;
            if (!row_id(item).isEmpty())
                rows.append(item);
            if (item->isExpanded())
                self(self, item);
        }
    };
    collect(collect, tree->invisibleRootItem());
    return rows;
}

// Resolve by stable model identity on every request. QTreeWidgetItems are
// replaced during a sidebar refresh; macOS may still hold an older AX child.
class AccessibleRow final : public QAccessibleInterface, public QAccessibleActionInterface {
public:
    AccessibleRow(QTreeWidget* tree, QString id) : m_tree(tree), m_id(std::move(id)) { }
    QTreeWidgetItem* item() const
    {
        for (auto* row : visible_rows(m_tree)) {
            if (row_id(row) == m_id)
                return row;
        }
        return nullptr;
    }
    bool isValid() const override { return item() != nullptr; }
    QObject* object() const override { return nullptr; }
    QWindow* window() const override { return m_tree ? m_tree->window()->windowHandle() : nullptr; }
    QAccessibleInterface* parent() const override { return m_tree ? QAccessible::queryAccessibleInterface(m_tree) : nullptr; }
    QAccessibleInterface* child(int) const override { return nullptr; }
    QAccessibleInterface* childAt(int, int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(QAccessibleInterface const*) const override { return -1; }
    QString text(QAccessible::Text type) const override
    {
        auto* row = item();
        if (!row)
            return {};
        if (type == QAccessible::Name)
            return row->data(0, Qt::AccessibleTextRole).toString().isEmpty() ? row->text(0) : row->data(0, Qt::AccessibleTextRole).toString();
        if (type == QAccessible::Description)
            return row->toolTip(0);
        return {};
    }
    void setText(QAccessible::Text, QString const&) override { }
    QRect rect() const override
    {
        if (auto* row = item()) {
            auto rect = m_tree->visualItemRect(row);
            return { m_tree->viewport()->mapToGlobal(rect.topLeft()), rect.size() };
        }
        return {};
    }
    QAccessible::Role role() const override { return QAccessible::ListItem; }
    QAccessible::State state() const override
    {
        QAccessible::State result;
        auto* row = item();
        if (!row) {
            result.invalid = true;
            return result;
        }
        result.focusable = true;
        result.selectable = true;
        result.selected = row->isSelected();
        result.focused = m_tree->hasFocus() && m_tree->currentItem() == row;
        result.invisible = !m_tree->isVisible();
        result.offscreen = !m_tree->viewport()->rect().intersects(m_tree->visualItemRect(row));
        result.expandable = row->childCount() > 0;
        result.expanded = result.expandable && row->isExpanded();
        result.collapsed = result.expandable && !row->isExpanded();
        return result;
    }
    void* interface_cast(QAccessible::InterfaceType type) override
    {
        return type == QAccessible::ActionInterface ? static_cast<QAccessibleActionInterface*>(this) : nullptr;
    }
    QStringList actionNames() const override { return { pressAction(), setFocusAction() }; }
    QStringList keyBindingsForAction(QString const&) const override { return {}; }
    void doAction(QString const& action) override
    {
        auto* row = item();
        if (!row)
            return;
        m_tree->setCurrentItem(row);
        m_tree->scrollToItem(row);
        if (action == setFocusAction()) {
            m_tree->setFocus();
        } else if (action == pressAction()) {
            if (row->childCount() > 0)
                row->setExpanded(!row->isExpanded());
            else
                Q_EMIT m_tree->itemActivated(row, 0);
        }
    }
    QString const& id() const { return m_id; }
private:
    QPointer<QTreeWidget> m_tree;
    QString m_id;
};

class AccessiblePages final : public QAccessibleWidget {
public:
    explicit AccessiblePages(QTreeWidget* tree) : QAccessibleWidget(tree, QAccessible::List), m_tree(tree) { }
    ~AccessiblePages() override
    {
        for (auto id : m_children)
            QAccessible::deleteAccessibleInterface(id);
    }
    int childCount() const override
    {
        int count = 0;
        for (auto* row : visible_rows(m_tree))
            count += m_tree->itemWidget(row, 1) ? 2 : 1;
        return count;
    }
    QAccessibleInterface* child(int index) const override
    {
        for (auto* row : visible_rows(m_tree)) {
            if (index-- == 0) {
                auto id = row_id(row);
                if (!m_children.contains(id))
                    m_children[id] = QAccessible::registerAccessibleInterface(new AccessibleRow(m_tree, id));
                return QAccessible::accessibleInterface(m_children.value(id));
            }
            if (auto* button = m_tree->itemWidget(row, 1); button && index-- == 0)
                return QAccessible::queryAccessibleInterface(button);
        }
        return nullptr;
    }
    int indexOfChild(QAccessibleInterface const* target) const override
    {
        for (int i = 0; i < childCount(); ++i) {
            if (child(i) == target)
                return i;
        }
        return -1;
    }
    QAccessibleInterface* childAt(int x, int y) const override
    {
        for (int i = childCount() - 1; i >= 0; --i) {
            auto* candidate = child(i);
            if (candidate && candidate->rect().contains(x, y))
                return candidate;
        }
        return nullptr;
    }
    QAccessibleInterface* focusChild() const override
    {
        for (int i = 0; i < childCount(); ++i) {
            if (auto* candidate = child(i); candidate && candidate->state().focused)
                return candidate;
        }
        return nullptr;
    }
private:
    QPointer<QTreeWidget> m_tree;
    mutable QHash<QString, QAccessible::Id> m_children;
};
}

void install_sidebar_accessibility()
{
    static bool installed = false;
    if (installed)
        return;
    installed = true;
    QAccessible::installFactory([](QString const&, QObject* object) -> QAccessibleInterface* {
        auto* tree = qobject_cast<QTreeWidget*>(object);
        return tree && tree->objectName() == "ThothPageTree" ? new AccessiblePages(tree) : nullptr;
    });
}
}
