/*******************************************************************************
 * Copyright (C) 2008-2013 Konstantinos Smanis <konstantinos.smanis@gmail.com> *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify it     *
 * under the terms of the GNU General Public License as published by the Free  *
 * Software Foundation, either version 3 of the License, or (at your option)   *
 * any later version.                                                          *
 *                                                                             *
 * This program is distributed in the hope that it will be useful, but WITHOUT *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for    *
 * more details.                                                               *
 *                                                                             *
 * You should have received a copy of the GNU General Public License along     *
 * with this program. If not, see <http://www.gnu.org/licenses/>.              *
 *******************************************************************************/

//Krazy
//krazy:excludeall=cpp

//Own
#include "helper.h"

//Qt
#include <QDir>

//KDE
#include <KDebug>
#include <KGlobal>
#include <KLocale>
#include <KProcess>
#include <KAuth/HelperSupport>

//Project
#include "../common.h"
#include "../config.h"
#if HAVE_HD
#undef slots
#include <hd.h>
#endif

//The $PATH environment variable is emptied by D-Bus activation,
//so let's provide a sane default. Needed for os-prober to work.
static const QString path = QLatin1String("/usr/sbin:/usr/bin:/sbin:/bin");

Helper::Helper()
{
    KGlobal::locale()->insertCatalog("kcm-grub2");
    qputenv("PATH", path.toLatin1());
}

ActionReply Helper::executeCommand(const QStringList &command)
{
    KProcess process;
    process.setProgram(command);
    process.setOutputChannelMode(KProcess::MergedChannels);

    kDebug() << "Executing" << command.join(" ");
    int exitCode = process.execute();

    ActionReply reply;
    if (exitCode != 0) {
        reply = ActionReply::HelperErrorReply;
        reply.setErrorCode(exitCode);
    }
    reply.addData("command", command);
    reply.addData("output", process.readAll());
    return reply;
}

ActionReply Helper::defaults(QVariantMap args)
{
    Q_UNUSED(args)
    ActionReply reply;
    QString configFileName = GRUB_CONFIG;
    QString originalConfigFileName = configFileName + ".original";

    if (!QFile::exists(originalConfigFileName)) {
        reply = ActionReply::HelperErrorReply;
        reply.addData("errorDescription", i18nc("@info", "Original configuration file <filename>%1</filename> does not exist.", originalConfigFileName));
        return reply;
    }
    if (!QFile::remove(configFileName)) {
        reply = ActionReply::HelperErrorReply;
        reply.addData("errorDescription", i18nc("@info", "Cannot remove current configuration file <filename>%1</filename>.", configFileName));
        return reply;
    }
    if (!QFile::copy(originalConfigFileName, configFileName)) {
        reply = ActionReply::HelperErrorReply;
        reply.addData("errorDescription", i18nc("@info", "Cannot copy original configuration file <filename>%1</filename> to <filename>%2</filename>.", originalConfigFileName, configFileName));
        return reply;
    }
    return reply;
}
ActionReply Helper::install(QVariantMap args)
{
    ActionReply reply;
    QString partition = args.value("partition").toString();
    QString mountPoint = args.value("mountPoint").toString();
    bool mbrInstall = args.value("mbrInstall").toBool();

    if (mountPoint.isEmpty()) {
        for (int i = 0; QDir(mountPoint = QString("%1/kcm-grub2-%2").arg(QDir::tempPath(), QString::number(i))).exists(); i++);
        if (!QDir().mkpath(mountPoint)) {
            reply = ActionReply::HelperErrorReply;
            reply.addData("errorDescription", i18nc("@info", "Failed to create temporary mount point."));
            return reply;
        }
        ActionReply mountReply = executeCommand(QStringList() << "mount" << partition << mountPoint);
        if (mountReply.failed()) {
            return mountReply;
        }
    }

    QStringList grub_installCommand;
    grub_installCommand << GRUB_INSTALL_EXE << "--root-directory" << mountPoint;
    if (mbrInstall) {
        grub_installCommand << partition.remove(QRegExp("\\d+"));
    } else {
        grub_installCommand << "--force" << partition;
    }
    return executeCommand(grub_installCommand);
}
ActionReply Helper::load(QVariantMap args)
{
    ActionReply reply;
    LoadOperations operations = (LoadOperations)(args.value("operations").toInt());

    if (operations.testFlag(MenuFile)) {
        QFile file(GRUB_MENU);
        bool ok = file.open(QIODevice::ReadOnly | QIODevice::Text);
        reply.addData(QLatin1String("menuSuccess"), ok);
        if (ok) {
            reply.addData(QLatin1String("menuContents"), file.readAll());
        } else {
            reply.addData(QLatin1String("menuError"), file.error());
            reply.addData(QLatin1String("menuErrorString"), file.errorString());
        }
    }
    if (operations.testFlag(ConfigurationFile)) {
        QFile file(GRUB_CONFIG);
        bool ok = file.open(QIODevice::ReadOnly | QIODevice::Text);
        reply.addData(QLatin1String("configSuccess"), ok);
        if (ok) {
            reply.addData(QLatin1String("configContents"), file.readAll());
        } else {
            reply.addData(QLatin1String("configError"), file.error());
            reply.addData(QLatin1String("configErrorString"), file.errorString());
        }
    }
    if (operations.testFlag(EnvironmentFile)) {
        QFile file(GRUB_ENV);
        bool ok = file.open(QIODevice::ReadOnly | QIODevice::Text);
        reply.addData(QLatin1String("envSuccess"), ok);
        if (ok) {
            reply.addData(QLatin1String("envContents"), file.readAll());
        } else {
            reply.addData(QLatin1String("envError"), file.error());
            reply.addData(QLatin1String("envErrorString"), file.errorString());
        }
    }
    if (operations.testFlag(MemtestFile)) {
        bool memtest = QFile::exists(GRUB_MEMTEST);
        reply.addData(QLatin1String("memtest"), memtest);
        if (memtest) {
            reply.addData(QLatin1String("memtestOn"), (bool)(QFile::permissions(GRUB_MEMTEST) & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther)));
        }
    }
#if HAVE_HD
    if (operations.testFlag(Vbe)) {
        QStringList gfxmodes;
        hd_data_t hd_data;
        memset(&hd_data, 0, sizeof(hd_data));
        hd_t *hd = hd_list(&hd_data, hw_framebuffer, 1, NULL);
        for (hd_res_t *res = hd->res; res; res = res->next) {
            if (res->any.type == res_framebuffer) {
                gfxmodes += QString("%1x%2x%3").arg(QString::number(res->framebuffer.width), QString::number(res->framebuffer.height), QString::number(res->framebuffer.colorbits));
            }
        }
        hd_free_hd_list(hd);
        hd_free_hd_data(&hd_data);
        reply.addData("gfxmodes", gfxmodes);
    }
#endif
    return reply;
}
ActionReply Helper::save(QVariantMap args)
{
    ActionReply reply;
    QString configFileName = GRUB_CONFIG;
    QByteArray rawConfigFileContents = args.value("rawConfigFileContents").toByteArray();
    QByteArray rawDefaultEntry = args.value("rawDefaultEntry").toByteArray();
    bool memtest = args.value("memtest").toBool();

    QFile::copy(configFileName, configFileName + ".original");

    QFile file(configFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        reply = ActionReply::HelperErrorReply;
        reply.addData("errorDescription", file.errorString());
        return reply;
    }
    file.write(rawConfigFileContents);
    file.close();

    if (args.contains("memtest")) {
        QFile::Permissions permissions = QFile::permissions(GRUB_MEMTEST);
        if (memtest) {
            permissions |= (QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
        } else {
            permissions &= ~(QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
        }
        QFile::setPermissions(GRUB_MEMTEST, permissions);
    }

    ActionReply grub_mkconfigReply = executeCommand(QStringList() << GRUB_MKCONFIG_EXE << "-o" << GRUB_MENU);
    if (grub_mkconfigReply.failed()) {
        return grub_mkconfigReply;
    }

    ActionReply grub_set_defaultReply = executeCommand(QStringList() << GRUB_SET_DEFAULT_EXE << rawDefaultEntry);
    if (grub_set_defaultReply.failed()) {
        return grub_set_defaultReply;
    }

    return grub_mkconfigReply;
}

KDE4_AUTH_HELPER_MAIN("org.kde.kcontrol.kcmgrub2", Helper)
