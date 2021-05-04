/* This file is part of the KDE project
   Copyright (C) 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <config-startplasma.h>

#include <QDir>
#include <QEventLoop>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include <unistd.h>

#include <autostartscriptdesktopfile.h>
#include <updatelaunchenvjob.h>

#include "startplasma.h"

#include "../config-workspace.h"
#include "debug.h"

QTextStream out(stderr);

void messageBox(const QString &text)
{
    out << text;
    runSync(QStringLiteral("xmessage"), {QStringLiteral("-geometry"), QStringLiteral("500x100"), text});
}

QStringList allServices(const QLatin1String &prefix)
{
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    const QStringList services = bus->registeredServiceNames();
    QMap<QString, QStringList> servicesWithAliases;

    for (const QString &serviceName : services) {
        QDBusReply<QString> reply = bus->serviceOwner(serviceName);
        QString owner = reply;
        if (owner.isEmpty())
            owner = serviceName;
        servicesWithAliases[owner].append(serviceName);
    }

    QStringList names;
    for (auto it = servicesWithAliases.constBegin(); it != servicesWithAliases.constEnd(); ++it) {
        if (it.value().startsWith(prefix))
            names << it.value();
    }
    names.removeDuplicates();
    names.sort();
    return names;
}

int runSync(const QString &program, const QStringList &args, const QStringList &env)
{
    QProcess p;
    if (!env.isEmpty())
        p.setEnvironment(QProcess::systemEnvironment() << env);
    p.setProcessChannelMode(QProcess::ForwardedChannels);
    p.start(program, args);
    //     qCDebug(PLASMA_STARTUP) << "started..." << program << args;
    p.waitForFinished(-1);
    if (p.exitCode()) {
        qWarning() << program << args << "exited with code" << p.exitCode();
    }
    return p.exitCode();
}

bool isShellVariable(const QByteArray &name)
{
    return name == "_" || name.startsWith("SHLVL");
}

bool isSessionVariable(const QByteArray &name)
{
    // Check is variable is specific to session.
    return name == "DISPLAY" || name == "XAUTHORITY" || //
        name == "WAYLAND_DISPLAY" || name == "WAYLAND_SOCKET" || //
        name.startsWith("XDG_");
}

void setEnvironmentVariable(const QByteArray &name, const QByteArray &value)
{
    if (qgetenv(name) != value) {
        //        qCDebug(PLASMA_STARTUP) << "setting..." << env.left(idx) << env.mid(idx+1) << "was" << qgetenv(env.left(idx));
        qputenv(name, value);
    }
}

void sourceFiles(const QStringList &files)
{
    QStringList filteredFiles;
    std::copy_if(files.begin(), files.end(), std::back_inserter(filteredFiles), [](const QString &i) {
        return QFileInfo(i).isReadable();
    });

    if (filteredFiles.isEmpty())
        return;

    filteredFiles.prepend(QStringLiteral(CMAKE_INSTALL_FULL_LIBEXECDIR "/plasma-sourceenv.sh"));

    QProcess p;
    p.start(QStringLiteral("/bin/sh"), filteredFiles);
    p.waitForFinished(-1);

    const auto fullEnv = p.readAllStandardOutput();
    auto envs = fullEnv.split('\0');

    for (auto &env : envs) {
        const int idx = env.indexOf('=');
        if (Q_UNLIKELY(idx <= 0)) {
            continue;
        }

        const auto name = env.left(idx);
        if (isShellVariable(name)) {
            continue;
        }
        setEnvironmentVariable(name, env.mid(idx + 1));
    }
}

void createConfigDirectory()
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (!QDir().mkpath(configDir))
        out << "Could not create config directory XDG_CONFIG_HOME: " << configDir << '\n';
}

void runStartupConfig()
{
    // export LC_* variables set by kcmshell5 formats into environment
    // so it can be picked up by QLocale and friends.
    KConfig config(QStringLiteral("plasma-localerc"));
    KConfigGroup formatsConfig = KConfigGroup(&config, "Formats");

    const auto lcValues = {"LANG", "LC_NUMERIC", "LC_TIME", "LC_MONETARY", "LC_MEASUREMENT", "LC_COLLATE", "LC_CTYPE"};
    for (auto lc : lcValues) {
        const QString value = formatsConfig.readEntry(lc, QString());
        if (!value.isEmpty()) {
            qputenv(lc, value.toUtf8());
        }
    }

    KConfigGroup languageConfig = KConfigGroup(&config, "Translations");
    const QString value = languageConfig.readEntry("LANGUAGE", QString());
    if (!value.isEmpty()) {
        qputenv("LANGUAGE", value.toUtf8());
    }

    if (!formatsConfig.hasKey("LANG") && !qEnvironmentVariableIsEmpty("LANG")) {
        formatsConfig.writeEntry("LANG", qgetenv("LANG"));
        formatsConfig.sync();
    }
}

void setupCursor(bool wayland)
{
    const KConfig cfg(QStringLiteral("kcminputrc"));
    const KConfigGroup inputCfg = cfg.group("Mouse");

    const auto kcminputrc_mouse_cursorsize = inputCfg.readEntry("cursorSize", 24);
    const auto kcminputrc_mouse_cursortheme = inputCfg.readEntry("cursorTheme", QStringLiteral("breeze_cursors"));
    if (!kcminputrc_mouse_cursortheme.isEmpty()) {
#ifdef XCURSOR_PATH
        QByteArray path(XCURSOR_PATH);
        path.replace("$XCURSOR_PATH", qgetenv("XCURSOR_PATH"));
        qputenv("XCURSOR_PATH", path);
#endif
    }

    // TODO: consider linking directly
    const int applyMouseStatus =
        wayland ? 0 : runSync(QStringLiteral("kapplymousetheme"), {kcminputrc_mouse_cursortheme, QString::number(kcminputrc_mouse_cursorsize)});
    if (applyMouseStatus == 10) {
        qputenv("XCURSOR_THEME", "breeze_cursors");
    } else if (!kcminputrc_mouse_cursortheme.isEmpty()) {
        qputenv("XCURSOR_THEME", kcminputrc_mouse_cursortheme.toUtf8());
    }
    qputenv("XCURSOR_SIZE", QByteArray::number(kcminputrc_mouse_cursorsize));
}

std::optional<QStringList> getSystemdEnvironment()
{
    QStringList list;
    auto msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                              QStringLiteral("/org/freedesktop/systemd1"),
                                              QStringLiteral("org.freedesktop.DBus.Properties"),
                                              QStringLiteral("Get"));
    msg << QStringLiteral("org.freedesktop.systemd1.Manager") << QStringLiteral("Environment");
    auto reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return std::nullopt;
    }

    // Make sure the returned type is correct.
    auto arguments = reply.arguments();
    if (arguments.isEmpty() || arguments[0].userType() != qMetaTypeId<QDBusVariant>()) {
        return std::nullopt;
    }
    auto variant = qdbus_cast<QVariant>(arguments[0]);
    if (variant.type() != QVariant::StringList) {
        return std::nullopt;
    }

    return variant.toStringList();
}

// Import systemd user environment.
//
// Systemd read ~/.config/environment.d which applies to all systemd user unit.
// But it won't work if plasma is not started by systemd.
void importSystemdEnvrionment()
{
    auto environment = getSystemdEnvironment();
    if (!environment) {
        return;
    }

    for (auto &envString : environment.value()) {
        const auto env = envString.toLocal8Bit();
        const int idx = env.indexOf('=');
        if (Q_UNLIKELY(idx <= 0)) {
            continue;
        }

        const auto name = env.left(idx);
        if (isShellVariable(name) || isSessionVariable(name)) {
            continue;
        }
        setEnvironmentVariable(name, env.mid(idx + 1));
    }
}

// Source scripts found in <config locations>/plasma-workspace/env/*.sh
// (where <config locations> correspond to the system and user's configuration
// directory.
//
// Scripts are sourced in reverse order of priority of their directory, as defined
// by `QStandardPaths::standardLocations`. This ensures that high-priority scripts
// (such as those in the user's home directory) are sourced last and take precedence
// over lower-priority scripts (such as system defaults). Scripts in the same
// directory are sourced in lexical order of their filename.
//
// This is where you can define environment variables that will be available to
// all KDE programs, so this is where you can run agents using e.g. eval `ssh-agent`
// or eval `gpg-agent --daemon`.
// Note: if you do that, you should also put "ssh-agent -k" as a shutdown script
//
// (see end of this file).
// For anything else (that doesn't set env vars, or that needs a window manager),
// better use the Autostart folder.

void runEnvironmentScripts()
{
    QStringList scripts;
    auto locations = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);

    //`standardLocations()` returns locations sorted by "order of priority". We iterate in reverse
    // order so that high-priority scripts are sourced last and their modifications take precedence.
    for (auto loc = locations.crbegin(); loc != locations.crend(); loc++) {
        QDir dir(*loc);
        if (!dir.cd(QStringLiteral("./plasma-workspace/env"))) {
            // Skip location if plasma-workspace/env subdirectory does not exist
            continue;
        }
        const auto dirScripts = dir.entryInfoList({QStringLiteral("*.sh")}, QDir::Files, QDir::Name);
        for (const auto &script : dirScripts) {
            scripts << script.absoluteFilePath();
        }
    }
    sourceFiles(scripts);
}

// Mark that full KDE session is running (e.g. Konqueror preloading works only
// with full KDE running). The KDE_FULL_SESSION property can be detected by
// any X client connected to the same X session, even if not launched
// directly from the KDE session but e.g. using "ssh -X", kdesu. $KDE_FULL_SESSION
// however guarantees that the application is launched in the same environment
// like the KDE session and that e.g. KDE utilities/libraries are available.
// KDE_FULL_SESSION property is also only available since KDE 3.5.5.
// The matching tests are:
//   For $KDE_FULL_SESSION:
//     if test -n "$KDE_FULL_SESSION"; then ... whatever
//   For KDE_FULL_SESSION property (on X11):
//     xprop -root | grep "^KDE_FULL_SESSION" >/dev/null 2>/dev/null
//     if test $? -eq 0; then ... whatever
//
// Additionally there is $KDE_SESSION_UID with the uid
// of the user running the KDE session. It should be rarely needed (e.g.
// after sudo to prevent desktop-wide functionality in the new user's kded).
//
// Since KDE4 there is also KDE_SESSION_VERSION, containing the major version number.
//

void setupPlasmaEnvironment()
{
    // Manually disable auto scaling because we are scaling above
    // otherwise apps that manually opt in for high DPI get auto scaled by the developer AND manually scaled by us
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");

    qputenv("KDE_FULL_SESSION", "true");
    qputenv("KDE_SESSION_VERSION", "5");
    qputenv("KDE_SESSION_UID", QByteArray::number(getuid()));
    qputenv("XDG_CURRENT_DESKTOP", "KDE");

    qputenv("KDE_APPLICATIONS_AS_SCOPE", "1");

    // Add kdedefaults dir to allow config defaults overriding from a writable location
    QByteArray currentConfigDirs = qgetenv("XDG_CONFIG_DIRS");
    if (currentConfigDirs.isEmpty()) {
        currentConfigDirs = "/etc/xdg";
    }
    const auto extraConfigDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation).toUtf8() + "/kdedefaults";
    qputenv("XDG_CONFIG_DIRS", extraConfigDir + ":" + currentConfigDirs);
}

void setupX11()
{
    //     Set a left cursor instead of the standard X11 "X" cursor, since I've heard
    //     from some users that they're confused and don't know what to do. This is
    //     especially necessary on slow machines, where starting KDE takes one or two
    //     minutes until anything appears on the screen.
    //
    //     If the user has overwritten fonts, the cursor font may be different now
    //     so don't move this up.

    runSync(QStringLiteral("xsetroot"), {QStringLiteral("-cursor_name"), QStringLiteral("left_ptr")});
    runSync(QStringLiteral("xprop"),
            {QStringLiteral("-root"),
             QStringLiteral("-f"),
             QStringLiteral("KDE_FULL_SESSION"),
             QStringLiteral("8t"),
             QStringLiteral("-set"),
             QStringLiteral("KDE_FULL_SESSION"),
             QStringLiteral("true")});
    runSync(QStringLiteral("xprop"),
            {QStringLiteral("-root"),
             QStringLiteral("-f"),
             QStringLiteral("KDE_SESSION_VERSION"),
             QStringLiteral("32c"),
             QStringLiteral("-set"),
             QStringLiteral("KDE_SESSION_VERSION"),
             QStringLiteral("5")});
}

void cleanupX11()
{
    runSync(QStringLiteral("xprop"), {QStringLiteral("-root"), QStringLiteral("-remove"), QStringLiteral("KDE_FULL_SESSION")});
    runSync(QStringLiteral("xprop"), {QStringLiteral("-root"), QStringLiteral("-remove"), QStringLiteral("KDE_SESSION_VERSION")});
}

void cleanupPlasmaEnvironment(const std::optional<QStringList> &oldSystemdEnvironment)
{
    qunsetenv("KDE_FULL_SESSION");
    qunsetenv("KDE_SESSION_VERSION");
    qunsetenv("KDE_SESSION_UID");

    if (!oldSystemdEnvironment) {
        return;
    }

    auto currentEnv = getSystemdEnvironment();
    if (!currentEnv) {
        return;
    }

    QStringList vars;
    for (auto &env : currentEnv.value()) {
        const int idx = env.indexOf(QLatin1Char('='));
        if (Q_UNLIKELY(idx <= 0)) {
            continue;
        }

        vars.append(env.left(idx));
    }

    // According to systemd documentation:
    // If a variable is listed in both, the variable is set after this method returns, i.e. the set list overrides the unset list.
    // So this will effectively restore the state to the values in oldSystemdEnvironment.
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                                          QStringLiteral("/org/freedesktop/systemd1"),
                                                          QStringLiteral("org.freedesktop.systemd1.Manager"),
                                                          QStringLiteral("UnsetAndSetEnvironment"));
    message.setArguments({vars, oldSystemdEnvironment.value()});

    // The session program gonna quit soon, ensure the message is flushed.
    auto reply = QDBusConnection::sessionBus().asyncCall(message);
    reply.waitForFinished();
}

// kwin_wayland can possibly also start dbus-activated services which need env variables.
// In that case, the update in startplasma might be too late.
bool syncDBusEnvironment()
{
    // At this point all environment variables are set, let's send it to the DBus session server to update the activation environment
    auto job = new UpdateLaunchEnvJob(QProcessEnvironment::systemEnvironment());
    return job->exec();
}

void setupFontDpi()
{
    KConfig cfg(QStringLiteral("kcmfonts"));
    KConfigGroup fontsCfg(&cfg, "General");

    if (!fontsCfg.hasKey("forceFontDPI")) {
        return;
    }

    // TODO port to c++?
    const QByteArray input = "Xft.dpi: " + QByteArray::number(fontsCfg.readEntry("forceFontDPI", 0));
    QProcess p;
    p.start(QStringLiteral("xrdb"), {QStringLiteral("-quiet"), QStringLiteral("-merge"), QStringLiteral("-nocpp")});
    p.setProcessChannelMode(QProcess::ForwardedChannels);
    p.write(input);
    p.closeWriteChannel();
    p.waitForFinished(-1);
}

static bool desktopLockedAtStart = false;

QProcess *setupKSplash()
{
    const auto dlstr = qgetenv("DESKTOP_LOCKED");
    desktopLockedAtStart = dlstr == "true" || dlstr == "1";
    qunsetenv("DESKTOP_LOCKED"); // Don't want it in the environment

    QProcess *p = nullptr;
    if (!desktopLockedAtStart) {
        const KConfig cfg(QStringLiteral("ksplashrc"));
        // the splashscreen and progress indicator
        KConfigGroup ksplashCfg = cfg.group("KSplash");
        if (ksplashCfg.readEntry("Engine", QStringLiteral("KSplashQML")) == QLatin1String("KSplashQML")) {
            p = new QProcess;
            p->start(QStringLiteral("ksplashqml"), {ksplashCfg.readEntry("Theme", QStringLiteral("Breeze"))});
        }
    }
    return p;
}

// If something went on an endless restart crash loop it will get blacklisted, as this is a clean login we will want to reset those counters
// This is independent of whether we use the Plasma systemd boot
void resetSystemdFailedUnits()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                                          QStringLiteral("/org/freedesktop/systemd1"),
                                                          QStringLiteral("org.freedesktop.systemd1.Manager"),
                                                          QStringLiteral("ResetFailed"));
    QDBusConnection::sessionBus().call(message);
}

bool hasSystemdService(const QString &serviceName)
{
    auto msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                              QStringLiteral("/org/freedesktop/systemd1"),
                                              QStringLiteral("org.freedesktop.systemd1.Manager"),
                                              QStringLiteral("ListUnitFilesByPatterns"));
    msg << QStringList({QStringLiteral("enabled"), QStringLiteral("static")}) << QStringList({serviceName});
    auto reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return false;
    }
    // if we have a service returned then it must have found it
    return !reply.arguments().isEmpty();
}

bool useSystemdBoot()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("startkderc"), KConfig::NoGlobals);
    const QString configValue = config->group(QStringLiteral("General")).readEntry("systemdBoot", QStringLiteral("false")).toLower();

    if (configValue == QLatin1String("false")) {
        return false;
    }

    if (!hasSystemdService(QStringLiteral("plasma-workspace@.target"))) {
        qWarning() << "Systemd boot requested, but plasma services were not found";
        return false;
    }

    if (configValue == QLatin1String("force")) {
        qInfo() << "Systemd boot forced";
        return true;
    }

    // xdg-desktop-autostart.target is shipped with an systemd 246 and provides a generator
    // for creating units out of existing autostart files
    // only enable our systemd boot if that exists, unless the user has forced the systemd boot above
    return hasSystemdService(QStringLiteral("xdg-desktop-autostart.target"));
}

bool startPlasmaSession(bool wayland)
{
    resetSystemdFailedUnits();
    OrgKdeKSplashInterface iface(QStringLiteral("org.kde.KSplash"), QStringLiteral("/KSplash"), QDBusConnection::sessionBus());
    iface.setStage(QStringLiteral("kinit"));
    // finally, give the session control to the session manager
    // see kdebase/ksmserver for the description of the rest of the startup sequence
    // if the KDEWM environment variable has been set, then it will be used as KDE's
    // window manager instead of kwin.
    // if KDEWM is not set, ksmserver will ensure kwin is started.
    // kwrapper5 is used to reduce startup time and memory usage
    // kwrapper5 does not return useful error codes such as the exit code of ksmserver.
    // We only check for 255 which means that the ksmserver process could not be
    // started, any problems thereafter, e.g. ksmserver failing to initialize,
    // will remain undetected.
    // If the session should be locked from the start (locked autologin),
    // lock now and do the rest of the KDE startup underneath the locker.

    bool rc = true;
    QEventLoop e;

    QDBusServiceWatcher serviceWatcher;
    serviceWatcher.setConnection(QDBusConnection::sessionBus());

    // We want to exit when both ksmserver and plasma-session-shutdown have finished
    // This also closes if ksmserver crashes unexpectedly, as in those cases plasma-shutdown is not running
    serviceWatcher.addWatchedService(QStringLiteral("org.kde.ksmserver"));
    serviceWatcher.addWatchedService(QStringLiteral("org.kde.Shutdown"));
    serviceWatcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    QObject::connect(&serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, [&]() {
        const QStringList watchedServices = serviceWatcher.watchedServices();
        bool plasmaSessionRunning = std::any_of(watchedServices.constBegin(), watchedServices.constEnd(), [](const QString &service) {
            return QDBusConnection::sessionBus().interface()->isServiceRegistered(service);
        });
        if (!plasmaSessionRunning) {
            e.quit();
        }
    });

    // Create .desktop files for the scripts in .config/autostart-scripts
    migrateUserScriptsAutostart();

    if (!useSystemdBoot()) {
        qCDebug(PLASMA_STARTUP) << "Using classic boot";
        QProcess startPlasmaSession;

        QStringList plasmaSessionOptions;
        if (wayland) {
            plasmaSessionOptions << QStringLiteral("--no-lockscreen");
        } else {
            if (desktopLockedAtStart) {
                plasmaSessionOptions << QStringLiteral("--lockscreen");
            }
        }

        startPlasmaSession.setProcessChannelMode(QProcess::ForwardedChannels);
        QObject::connect(&startPlasmaSession, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [&rc, &e](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 255) {
                // Startup error
                messageBox(QStringLiteral("startkde: Could not start ksmserver. Check your installation.\n"));
                rc = false;
                e.quit();
            }
        });

        startPlasmaSession.start(QStringLiteral(CMAKE_INSTALL_FULL_BINDIR "/plasma_session"), plasmaSessionOptions);
        // plasma-session starts everything else up then quits
        rc = startPlasmaSession.waitForFinished(120 * 1000);
    } else {
        qCDebug(PLASMA_STARTUP) << "Using systemd boot";
        const QString platform = wayland ? QStringLiteral("wayland") : QStringLiteral("x11");

        auto msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                                  QStringLiteral("/org/freedesktop/systemd1"),
                                                  QStringLiteral("org.freedesktop.systemd1.Manager"),
                                                  QStringLiteral("StartUnit"));
        msg << QStringLiteral("plasma-workspace@%1.target").arg(platform) << QStringLiteral("fail");
        QDBusReply<QDBusObjectPath> reply = QDBusConnection::sessionBus().call(msg);
        if (!reply.isValid()) {
            qWarning() << "Could not start systemd managed Plasma session:" << reply.error().name() << reply.error().message();
            messageBox(QStringLiteral("startkde: Could not start Plasma session.\n"));
            rc = false;
        }
    }
    if (rc) {
        e.exec();
    }
    return rc;
}

void waitForKonqi()
{
    const KConfig cfg(QStringLiteral("startkderc"));
    const KConfigGroup grp = cfg.group("WaitForDrKonqi");
    bool wait_drkonqi = grp.readEntry("Enabled", true);
    if (wait_drkonqi) {
        // wait for remaining drkonqi instances with timeout (in seconds)
        const int wait_drkonqi_timeout = grp.readEntry("Timeout", 900) * 1000;
        QElapsedTimer wait_drkonqi_counter;
        wait_drkonqi_counter.start();
        QStringList services = allServices(QLatin1String("org.kde.drkonqi-"));
        while (!services.isEmpty()) {
            sleep(5);
            services = allServices(QLatin1String("org.kde.drkonqi-"));
            if (wait_drkonqi_counter.elapsed() >= wait_drkonqi_timeout) {
                // ask remaining drkonqis to die in a graceful way
                for (const auto &service : qAsConst(services)) {
                    QDBusInterface iface(service, QStringLiteral("/MainApplication"));
                    iface.call(QStringLiteral("quit"));
                }
                break;
            }
        }
    }
}

static void migrateUserScriptsAutostart()
{
    QDir configLocation(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
    QDir autostartScriptsLocation(configLocation.filePath(QStringLiteral("autostart-scripts")));
    if (!autostartScriptsLocation.exists()) {
        return;
    }
    const QDir autostartScriptsMovedLocation(configLocation.filePath(QStringLiteral("old-autostart-scripts")));
    const auto entries = autostartScriptsLocation.entryInfoList(QDir::Files);
    for (const auto &info : entries) {
        const auto scriptName = info.fileName();
        const auto scriptPath = info.absoluteFilePath();
        const auto scriptMovedPath = autostartScriptsMovedLocation.filePath(scriptName);

        // Don't migrate backup files
        if (scriptName.endsWith(QLatin1Char('~')) || scriptName.endsWith(QLatin1String(".bak"))
            || (scriptName[0] == QLatin1Char('%') && scriptName.endsWith(QLatin1Char('%')))
            || (scriptName[0] == QLatin1Char('#') && scriptName.endsWith(QLatin1Char('#')))) {
            qCDebug(PLASMA_STARTUP) << "Not migrating backup autostart script" << scriptName;
            continue;
        }

        // Migrate autostart script to a standard .desktop autostart file
        AutostartScriptDesktopFile desktopFile(scriptName, info.isSymLink() ? info.symLinkTarget() : scriptMovedPath);
        qCInfo(PLASMA_STARTUP) << "Migrated legacy autostart script" << scriptPath << "to" << desktopFile.fileName();

        if (info.isSymLink() && QFile::remove(scriptPath)) {
            qCInfo(PLASMA_STARTUP) << "Removed legacy autostart script" << scriptPath << "that pointed to" << info.symLinkTarget();
        }
    }
    // Delete or rename autostart-scripts to old-autostart-scripts to avoid running the migration again
    if (autostartScriptsLocation.entryInfoList(QDir::Files).empty()) {
        autostartScriptsLocation.removeRecursively();
    } else {
        configLocation.rename(autostartScriptsLocation.dirName(), autostartScriptsMovedLocation.dirName());
    }
    // Reload systemd so that the XDG autostart generator is run again to pick up the new .desktop files
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                                          QStringLiteral("/org/freedesktop/systemd1"),
                                                          QStringLiteral("org.freedesktop.systemd1.Manager"),
                                                          QStringLiteral("Reload"));
    QDBusConnection::sessionBus().call(message);
}
