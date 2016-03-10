/***************************************************************************
 *   Copyright (C) 2015 Marco Martin <mart@kde.org>                        *
 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#ifndef SYSTEMTRAY_H
#define SYSTEMTRAY_H

#include <QAbstractItemModel>
#include <Plasma/Containment>

class QDBusPendingCallWatcher;
class QDBusConnection;
class QQuickItem;
class PlasmoidModel;

class SystemTray : public Plasma::Containment
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* availablePlasmoids READ availablePlasmoids CONSTANT)
    Q_PROPERTY(QStringList allowedPlasmoids READ allowedPlasmoids WRITE setAllowedPlasmoids NOTIFY allowedPlasmoidsChanged)
    Q_PROPERTY(QStringList defaultPlasmoids READ defaultPlasmoids CONSTANT)

public:
    SystemTray( QObject *parent, const QVariantList &args );
    ~SystemTray() override;

    void init() override;

    void restoreContents(KConfigGroup &group) Q_DECL_OVERRIDE;
    void restorePlasmoids();

    QStringList defaultPlasmoids() const;

    QAbstractItemModel* availablePlasmoids();

    QStringList allowedPlasmoids() const;
    void setAllowedPlasmoids(const QStringList &allowed);

    //Creates an applet *if not already existing*
    void newTask(const QString &task);

    //cleans all instances of a given applet
    void cleanupTask(const QString &task);

    //Invokable utilities
    /**
     * returns either a simple icon name or a custom path if the app is
     * using a custom theme
     */
    Q_INVOKABLE QVariant resolveIcon(const QVariant &variant, const QString &iconThemePath);

    /**
     * Given an AppletInterface pointer, shows a proper context menu fot it
     */
    Q_INVOKABLE void showPlasmoidMenu(QQuickItem *appletInterface);

    /**
     * Returns the "X-Plasma-NotificationAreaCategory"
     * of the plasmoid metadata
     */
    Q_INVOKABLE QString plasmoidCategory(QQuickItem *appletInterface) const;

    /**
     * Shows the context menu for a statusnotifieritem
     */
    Q_INVOKABLE void showStatusNotifierContextMenu(KJob *job, QQuickItem *statusNotifierIcon);

    /**
     * Find out global coordinates for a popup given local MouseArea
     * coordinates
     */
    Q_INVOKABLE QPointF popupPosition(QQuickItem* visualParent, int x, int y);

private Q_SLOTS:
    void serviceNameFetchFinished(QDBusPendingCallWatcher* watcher, const QDBusConnection &connection);
    void serviceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner);

private:
    void serviceRegistered(const QString &service);
    void serviceUnregistered(const QString &service);

Q_SIGNALS:
    void allowedPlasmoidsChanged();

private:
    void initDBusActivatables();
    QStringList m_allowedPlasmoids;
    PlasmoidModel *m_availablePlasmoidsModel;
    QHash<QString, int> m_knownPlugins;
    QHash<QString, QString> m_dbusActivatableTasks;
    QHash<QString, int> m_dbusServiceCounts;
};

#endif
