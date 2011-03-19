﻿/*******************************************************************************
 * Copyright (C) 2008-2011 Konstantinos Smanis <konstantinos.smanis@gmail.com> *
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

//Own
#include "kcm_grub2.h"

//Qt
#include <QFile>

//KDE
#include <KAboutData>
#include <KDebug>
#include <KFileDialog>
#include <KMessageBox>
#include <kmountpoint.h>
#include <KPluginFactory>
#include <KProgressDialog>
#include <KShell>
#include <KSplashScreen>
#include <KAuth/Action>
#include <KAuth/ActionReply>
#include <KAuth/ActionWatcher>
using namespace KAuth;

//Project
#include "settings.h"

K_PLUGIN_FACTORY(GRUB2Factory, registerPlugin<KCMGRUB2>();)
K_EXPORT_PLUGIN(GRUB2Factory("kcmgrub2"))

KCMGRUB2::KCMGRUB2(QWidget *parent, const QVariantList &list) : KCModule(GRUB2Factory::componentData(), parent, list)
{
    KAboutData *about = new KAboutData("kcmgrub2", 0, ki18n("KDE GRUB2 Bootloader Control Module"), "0.2.5", KLocalizedString(), KAboutData::License_GPL_V3, ki18n("Copyright (C) 2008-2011 Konstantinos Smanis"), KLocalizedString(), QByteArray(), "konstantinos.smanis@gmail.com");
    about->addAuthor(ki18n( "Κonstantinos Smanis" ), ki18n( "Main Developer" ), "konstantinos.smanis@gmail.com");
    setAboutData(about);

    setupUi(this);
    setupObjects();
    setupConnections();
}
KCMGRUB2::~KCMGRUB2()
{
    if (splash) {
        delete splash;
        splash = 0;
    }
}

void KCMGRUB2::load()
{
    readDevices();
    readEntries();
    readSettings();

    comboBox_default->blockSignals(true);
    comboBox_default->addItems(m_entries);
    if (m_settings["GRUB_DEFAULT"].compare("saved", Qt::CaseInsensitive) != 0) {
        radioButton_default->setChecked(true);
        if (m_entries.contains(m_settings["GRUB_DEFAULT"])) {
            comboBox_default->setCurrentIndex(comboBox_default->findText(m_settings["GRUB_DEFAULT"]));
        } else {
            comboBox_default->setCurrentIndex(m_settings["GRUB_DEFAULT"].toInt());
        }
    } else {
        radioButton_saved->setChecked(true);
    }
    checkBox_savedefault->setChecked(m_settings.value("GRUB_SAVEDEFAULT").compare("true", Qt::CaseInsensitive) == 0);
    comboBox_default->blockSignals(false);

    kintspinbox_hiddenTimeout->blockSignals(true);
    kintspinbox_timeout->blockSignals(true);
    checkBox_hiddenTimeout->setChecked(m_settings.value("GRUB_HIDDEN_TIMEOUT").toInt() > 0);
    kintspinbox_hiddenTimeout->setValue(m_settings.value("GRUB_HIDDEN_TIMEOUT").toInt());
    checkBox_hiddenTimeoutShowTimer->setChecked(m_settings.value("GRUB_HIDDEN_TIMEOUT_QUIET").compare("true", Qt::CaseInsensitive) != 0);
    checkBox_timeout->setChecked(m_settings.value("GRUB_TIMEOUT").toInt() > -1);
    radioButton_timeout0->setChecked(m_settings.value("GRUB_TIMEOUT").toInt() == 0);
    radioButton_timeout->setChecked(m_settings.value("GRUB_TIMEOUT").toInt() > 0);
    kintspinbox_timeout->setValue(m_settings.value("GRUB_TIMEOUT").toInt());
    kintspinbox_hiddenTimeout->blockSignals(false);
    kintspinbox_timeout->blockSignals(false);

    comboBox_normalForeground->blockSignals(true);
    comboBox_normalBackground->blockSignals(true);
    comboBox_highlightForeground->blockSignals(true);
    comboBox_highlightBackground->blockSignals(true);
    kurlrequester_background->blockSignals(true);
    kurlrequester_theme->blockSignals(true);
    klineedit_gfxmode->setText(m_settings.value("GRUB_GFXMODE"));
    if (m_settings.value("GRUB_GFXPAYLOAD_LINUX").compare("text", Qt::CaseInsensitive) == 0) {
        radioButton_gfxpayloadText->setChecked(true);
    } else if (m_settings.value("GRUB_GFXPAYLOAD_LINUX").compare("keep", Qt::CaseInsensitive) == 0) {
        radioButton_gfxpayloadKeep->setChecked(true);
    } else {
        radioButton_gfxpayloadOther->setChecked(true);
        klineedit_gfxpayload->setText(m_settings.value("GRUB_GFXPAYLOAD_LINUX"));
    }
    radioButton_gfxpayloadText->setChecked(m_settings.value("GRUB_GFXPAYLOAD_LINUX").compare("text", Qt::CaseInsensitive) == 0);
    radioButton_gfxpayloadKeep->setChecked(m_settings.value("GRUB_GFXPAYLOAD_LINUX").compare("keep", Qt::CaseInsensitive) == 0);
    klineedit_gfxpayload->setText(m_settings.value("GRUB_GFXPAYLOAD_LINUX"));
    int normalForegroundIndex = comboBox_normalForeground->findData(m_settings.value("GRUB_COLOR_NORMAL").section('/', 0, 0));
    comboBox_normalForeground->setCurrentIndex(normalForegroundIndex != -1 ? normalForegroundIndex : comboBox_normalForeground->findData("light-gray"));
    int normalBackgroundIndex = comboBox_normalBackground->findData(m_settings.value("GRUB_COLOR_NORMAL").section('/', 1));
    comboBox_normalBackground->setCurrentIndex(normalBackgroundIndex != -1 ? normalBackgroundIndex : comboBox_normalBackground->findData("black"));
    int highlightForegroundIndex = comboBox_highlightForeground->findData(m_settings.value("GRUB_COLOR_HIGHLIGHT").section('/', 0, 0));
    comboBox_highlightForeground->setCurrentIndex(highlightForegroundIndex != -1 ? highlightForegroundIndex : comboBox_highlightForeground->findData("light-gray"));
    int highlightBackgroundIndex = comboBox_highlightBackground->findData(m_settings.value("GRUB_COLOR_HIGHLIGHT").section('/', 1));
    comboBox_highlightBackground->setCurrentIndex(highlightBackgroundIndex != -1 ? highlightBackgroundIndex : comboBox_highlightBackground->findData("black"));
    kurlrequester_background->setText(m_settings.value("GRUB_BACKGROUND"));
    kpushbutton_preview->setEnabled(!m_settings.value("GRUB_BACKGROUND").isEmpty());
    kurlrequester_theme->setText(m_settings.value("GRUB_THEME"));
    comboBox_normalForeground->blockSignals(false);
    comboBox_normalBackground->blockSignals(false);
    comboBox_highlightForeground->blockSignals(false);
    comboBox_highlightBackground->blockSignals(false);
    kurlrequester_background->blockSignals(false);
    kurlrequester_theme->blockSignals(false);

    klineedit_cmdlineDefault->setText(m_settings.value("GRUB_CMDLINE_LINUX_DEFAULT"));
    klineedit_cmdline->setText(m_settings.value("GRUB_CMDLINE_LINUX"));
    klineedit_terminal->setText(m_settings.value("GRUB_TERMINAL"));
    klineedit_terminal->setText(m_settings.value("GRUB_TERMINAL_INPUT"));
    klineedit_terminal->setText(m_settings.value("GRUB_TERMINAL_OUTPUT"));
    klineedit_distributor->setText(m_settings.value("GRUB_DISTRIBUTOR"));
    klineedit_serial->setText(m_settings.value("GRUB_SERIAL_COMMAND"));
    checkBox_uuid->setChecked(m_settings.value("GRUB_DISABLE_LINUX_UUID").compare("true", Qt::CaseInsensitive) != 0);
    checkBox_recovery->setChecked(m_settings.value("GRUB_DISABLE_RECOVERY").compare("true", Qt::CaseInsensitive) != 0);
    checkBox_osProber->setChecked(m_settings.value("GRUB_DISABLE_OS_PROBER").compare("true", Qt::CaseInsensitive) != 0);
}
void KCMGRUB2::save()
{
    QString config;
    QTextStream stream(&config, QIODevice::WriteOnly | QIODevice::Text);
    for (QHash<QString, QString>::const_iterator it = m_settings.constBegin(); it != m_settings.constEnd(); it++) {
        QString key = it.key(), value = it.value();
        if (!value.startsWith('`') || !value.endsWith('`')) {
            value = KShell::quoteArg(value);
        }
        stream << key << '=' << value << endl;
    }
    if (!saveFile(Settings::configPaths().at(0), config)) {
        return;
    }
    if (KMessageBox::questionYesNo(this, i18nc("@info", "<para>Your configuration was successfully saved.<nl/>For the changes to take effect your GRUB menu has to be updated.</para><para>Update your GRUB menu?</para>")) == KMessageBox::Yes) {
        updateGRUB(Settings::menuPaths().at(0));
    }
}

void KCMGRUB2::on_radioButton_default_clicked(bool checked)
{
    Q_UNUSED(checked)
    on_comboBox_default_currentIndexChanged(comboBox_default->currentIndex());
}
void KCMGRUB2::on_comboBox_default_currentIndexChanged(int index)
{
    m_settings["GRUB_DEFAULT"] = m_entries.at(index);
    emit changed(true);
}
void KCMGRUB2::on_radioButton_saved_clicked(bool checked)
{
    Q_UNUSED(checked)
    m_settings["GRUB_DEFAULT"] = "saved";
    emit changed(true);
}
void KCMGRUB2::on_checkBox_savedefault_clicked(bool checked)
{
    if (checked) {
        m_settings["GRUB_SAVEDEFAULT"] = "true";
    } else {
        m_settings.remove("GRUB_SAVEDEFAULT");
    }
    emit changed(true);
}
void KCMGRUB2::on_checkBox_hiddenTimeout_clicked(bool checked)
{
    if (checked) {
        m_settings["GRUB_HIDDEN_TIMEOUT"] = QString::number(kintspinbox_hiddenTimeout->value());
    } else {
        m_settings.remove("GRUB_HIDDEN_TIMEOUT");
    }
    emit changed(true);
}
void KCMGRUB2::on_kintspinbox_hiddenTimeout_valueChanged(int i)
{
    m_settings["GRUB_HIDDEN_TIMEOUT"] = QString::number(i);
    emit changed(true);
}
void KCMGRUB2::on_checkBox_hiddenTimeoutShowTimer_clicked(bool checked)
{
    if (checked) {
        m_settings.remove("GRUB_HIDDEN_TIMEOUT_QUIET");
    } else {
        m_settings["GRUB_HIDDEN_TIMEOUT_QUIET"] = "true";
    }
    emit changed(true);
}
void KCMGRUB2::on_checkBox_timeout_clicked(bool checked)
{
    if (checked) {
        if (radioButton_timeout0->isChecked()) {
            m_settings["GRUB_TIMEOUT"] = "0";
        } else {
            m_settings["GRUB_TIMEOUT"] = QString::number(kintspinbox_timeout->value());
        }
    } else {
        m_settings["GRUB_TIMEOUT"] = "-1";
    }
    emit changed(true);
}
void KCMGRUB2::on_radioButton_timeout0_clicked(bool checked)
{
    Q_UNUSED(checked)
    m_settings["GRUB_TIMEOUT"] = "0";
    emit changed(true);
}
void KCMGRUB2::on_radioButton_timeout_clicked(bool checked)
{
    Q_UNUSED(checked)
    m_settings["GRUB_TIMEOUT"] = QString::number(kintspinbox_timeout->value());
    emit changed(true);
}
void KCMGRUB2::on_kintspinbox_timeout_valueChanged(int i)
{
    m_settings["GRUB_TIMEOUT"] = QString::number(i);
    emit changed(true);
}

void KCMGRUB2::on_klineedit_gfxmode_textEdited(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_GFXMODE"] = text;
    } else {
        m_settings.remove("GRUB_GFXMODE");
    }
    emit changed(true);
}
void KCMGRUB2::on_radioButton_gfxpayloadText_clicked(bool checked)
{
    Q_UNUSED(checked)
    m_settings["GRUB_GFXPAYLOAD_LINUX"] = "text";
    emit changed(true);
}
void KCMGRUB2::on_radioButton_gfxpayloadKeep_clicked(bool checked)
{
    Q_UNUSED(checked)
    m_settings["GRUB_GFXPAYLOAD_LINUX"] = "keep";
    emit changed(true);
}
void KCMGRUB2::on_radioButton_gfxpayloadOther_clicked(bool checked)
{
    Q_UNUSED(checked)
    on_klineedit_gfxpayload_textEdited(klineedit_gfxpayload->text());
}
void KCMGRUB2::on_klineedit_gfxpayload_textEdited(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_GFXPAYLOAD_LINUX"] = text;
    } else {
        m_settings.remove("GRUB_GFXPAYLOAD_LINUX");
    }
    emit changed(true);
}
void KCMGRUB2::on_comboBox_normalForeground_currentIndexChanged(int index)
{
    QString normalForeground = comboBox_normalForeground->itemData(index).toString();
    QString normalBackground = comboBox_normalBackground->itemData(comboBox_normalBackground->currentIndex()).toString();
    if (normalForeground.compare("light-gray") != 0 || normalBackground.compare("black") != 0) {
        m_settings["GRUB_COLOR_NORMAL"] = normalForeground + '/' + normalBackground;
    } else {
        m_settings.remove("GRUB_COLOR_NORMAL");
    }
    emit changed(true);
}
void KCMGRUB2::on_comboBox_normalBackground_currentIndexChanged(int index)
{
    QString normalForeground = comboBox_normalForeground->itemData(comboBox_normalForeground->currentIndex()).toString();
    QString normalBackground = comboBox_normalBackground->itemData(index).toString();
    if (normalForeground.compare("light-gray") != 0 || normalBackground.compare("black") != 0) {
        m_settings["GRUB_COLOR_NORMAL"] = normalForeground + '/' + normalBackground;
    } else {
        m_settings.remove("GRUB_COLOR_NORMAL");
    }
    emit changed(true);
}
void KCMGRUB2::on_comboBox_highlightForeground_currentIndexChanged(int index)
{
    QString highlightForeground = comboBox_highlightForeground->itemData(index).toString();
    QString highlightBackground = comboBox_highlightBackground->itemData(comboBox_highlightBackground->currentIndex()).toString();
    if (highlightForeground.compare("black") != 0 || highlightBackground.compare("light-gray") != 0) {
        m_settings["GRUB_COLOR_HIGHLIGHT"] = highlightForeground + '/' + highlightBackground;
    } else {
        m_settings.remove("GRUB_COLOR_HIGHLIGHT");
    }
    emit changed(true);
}
void KCMGRUB2::on_comboBox_highlightBackground_currentIndexChanged(int index)
{
    QString highlightForeground = comboBox_highlightForeground->itemData(comboBox_highlightForeground->currentIndex()).toString();
    QString highlightBackground = comboBox_highlightBackground->itemData(index).toString();
    if (highlightForeground.compare("black") != 0 || highlightBackground.compare("light-gray") != 0) {
        m_settings["GRUB_COLOR_HIGHLIGHT"] = highlightForeground + '/' + highlightBackground;
    } else {
        m_settings.remove("GRUB_COLOR_HIGHLIGHT");
    }
    emit changed(true);
}
void KCMGRUB2::on_kurlrequester_background_textChanged(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_BACKGROUND"] = text;
    } else {
        m_settings.remove("GRUB_BACKGROUND");
    }
    kpushbutton_preview->setEnabled(!text.isEmpty());
    emit changed(true);
}
void KCMGRUB2::on_kpushbutton_preview_clicked(bool checked)
{
    Q_UNUSED(checked)
    QFile file(kurlrequester_background->url().toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    if (splash) {
        delete splash;
    }

    splash = new KSplashScreen(QPixmap::fromImage(QImage::fromData(file.readAll())));
    splash->show();
}
void KCMGRUB2::on_kurlrequester_theme_textChanged(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_THEME"] = text;
    } else {
        m_settings.remove("GRUB_THEME");
    }
    emit changed(true);
}

void KCMGRUB2::on_klineedit_cmdlineDefault_textEdited(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_CMDLINE_LINUX_DEFAULT"] = text;
    } else {
        m_settings.remove("GRUB_CMDLINE_LINUX_DEFAULT");
    }
    emit changed(true);
}
void KCMGRUB2::on_klineedit_cmdline_textEdited(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_CMDLINE_LINUX"] = text;
    } else {
        m_settings.remove("GRUB_CMDLINE_LINUX");
    }
    emit changed(true);
}
void KCMGRUB2::on_klineedit_terminal_textEdited(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_TERMINAL"] = text;
    } else {
        m_settings.remove("GRUB_TERMINAL");
    }
    emit changed(true);
}
void KCMGRUB2::on_klineedit_terminalInput_textEdited(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_TERMINAL_INPUT"] = text;
    } else {
        m_settings.remove("GRUB_TERMINAL_INPUT");
    }
    emit changed(true);
}
void KCMGRUB2::on_klineedit_terminalOutput_textEdited(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_TERMINAL_OUTPUT"] = text;
    } else {
        m_settings.remove("GRUB_TERMINAL_OUTPUT");
    }
    emit changed(true);
}
void KCMGRUB2::on_klineedit_distributor_textEdited(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_DISTRIBUTOR"] = text;
    } else {
        m_settings.remove("GRUB_DISTRIBUTOR");
    }
    emit changed(true);
}
void KCMGRUB2::on_klineedit_serial_textEdited(const QString &text)
{
    if (!text.isEmpty()) {
        m_settings["GRUB_SERIAL_COMMAND"] = text;
    } else {
        m_settings.remove("GRUB_SERIAL_COMMAND");
    }
    emit changed(true);
}
void KCMGRUB2::on_checkBox_uuid_clicked(bool checked)
{
    if (checked) {
        m_settings.remove("GRUB_DISABLE_LINUX_UUID");
    } else {
        m_settings["GRUB_DISABLE_LINUX_UUID"] = "true";
    }
    emit changed(true);
}
void KCMGRUB2::on_checkBox_recovery_clicked(bool checked)
{
    if (checked) {
        m_settings.remove("GRUB_DISABLE_RECOVERY");
    } else {
        m_settings["GRUB_DISABLE_RECOVERY"] = "true";
    }
    emit changed(true);
}
void KCMGRUB2::on_checkBox_osProber_clicked(bool checked)
{
    if (checked) {
        m_settings.remove("GRUB_DISABLE_OS_PROBER");
    } else {
        m_settings["GRUB_DISABLE_OS_PROBER"] = "true";
    }
    emit changed(true);
}

void KCMGRUB2::setupObjects()
{
    setButtons(Apply);
    setNeedsAuthorization(true);

    comboBox_normalForeground->blockSignals(true);
    comboBox_normalBackground->blockSignals(true);
    comboBox_highlightForeground->blockSignals(true);
    comboBox_highlightBackground->blockSignals(true);
    QPixmap black(16, 16), transparent(16, 16);
    black.fill(Qt::black);
    transparent.fill(Qt::transparent);
    comboBox_normalForeground->addItem(QIcon(black), i18nc("@item:inlistbox", "Black"), "black");
    comboBox_highlightForeground->addItem(QIcon(black), i18nc("@item:inlistbox", "Black"), "black");
    comboBox_normalBackground->addItem(QIcon(transparent), i18nc("@item:inlistbox", "Transparent"), "black");
    comboBox_highlightBackground->addItem(QIcon(transparent), i18nc("@item:inlistbox", "Transparent"), "black");
    QHash<QString, QString> colors;
    colors.insertMulti("blue", i18nc("@item:inlistbox", "Blue"));
    colors.insertMulti("blue", "blue");
    colors.insertMulti("cyan", i18nc("@item:inlistbox", "Cyan"));
    colors.insertMulti("cyan", "cyan");
    colors.insertMulti("dark-gray", i18nc("@item:inlistbox", "Dark Gray"));
    colors.insertMulti("dark-gray", "darkgray");
    colors.insertMulti("green", i18nc("@item:inlistbox", "Green"));
    colors.insertMulti("green", "green");
    colors.insertMulti("light-cyan", i18nc("@item:inlistbox", "Light Cyan"));
    colors.insertMulti("light-cyan", "lightcyan");
    colors.insertMulti("light-blue", i18nc("@item:inlistbox", "Light Blue"));
    colors.insertMulti("light-blue", "lightblue");
    colors.insertMulti("light-green", i18nc("@item:inlistbox", "Light Green"));
    colors.insertMulti("light-green", "lightgreen");
    colors.insertMulti("light-gray", i18nc("@item:inlistbox", "Light Gray"));
    colors.insertMulti("light-gray", "lightgray");
    colors.insertMulti("light-magenta", i18nc("@item:inlistbox", "Light Magenta"));
    colors.insertMulti("light-magenta", "magenta");
    colors.insertMulti("light-red", i18nc("@item:inlistbox", "Light Red"));
    colors.insertMulti("light-red", "orangered");
    colors.insertMulti("magenta", i18nc("@item:inlistbox", "Magenta"));
    colors.insertMulti("magenta", "darkmagenta");
    colors.insertMulti("red", i18nc("@item:inlistbox", "Red"));
    colors.insertMulti("red", "red");
    colors.insertMulti("white", i18nc("@item:inlistbox", "White"));
    colors.insertMulti("white", "white");
    colors.insertMulti("yellow", i18nc("@item:inlistbox", "Yellow"));
    colors.insertMulti("yellow", "yellow");
    for (QHash<QString, QString>::const_iterator it = colors.constBegin(); it != colors.constEnd(); it += 2) {
        QPixmap color(16, 16);
        color.fill(QColor(colors.values(it.key()).at(0)));
        comboBox_normalForeground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        comboBox_highlightForeground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        comboBox_normalBackground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        comboBox_highlightBackground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
    }
    comboBox_normalForeground->blockSignals(false);
    comboBox_normalBackground->blockSignals(false);
    comboBox_highlightForeground->blockSignals(false);
    comboBox_highlightBackground->blockSignals(false);

    splash = 0;
    kpushbutton_preview->setIcon(KIcon("image-png"));
}
void KCMGRUB2::setupConnections()
{
}

QString KCMGRUB2::convertToGRUBFileName(const QString &fileName)
{
    QString grubFileName = fileName;
    QString mountpoint = KMountPoint::currentMountPoints().findByPath(grubFileName)->mountPoint();
    if (m_devices.contains(mountpoint)) {
        if (mountpoint.compare("/") != 0) {
            grubFileName.remove(0, mountpoint.length());
        }
        grubFileName.prepend(m_devices.value(mountpoint));
    }
    return grubFileName;
}
QString KCMGRUB2::convertToLocalFileName(const QString &grubFileName)
{
    QString fileName = grubFileName;
    for (QHash<QString, QString>::const_iterator it = m_devices.constBegin(); it != m_devices.constEnd(); it++) {
        if (fileName.startsWith(it.value())) {
            fileName.remove(0, it.value().length());
            if (it.key().compare("/") != 0) {
                fileName.prepend(it.key());
            }
            break;
        }
    }
    return fileName;
}

QString KCMGRUB2::readFile(const QString &fileName)
{
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        return stream.readAll();
    }

    Action loadAction("org.kde.kcontrol.kcmgrub2.load");
    loadAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    loadAction.addArgument("fileName", fileName);

    ActionReply reply = loadAction.execute();
    if (reply.failed()) {
        kDebug() << "Error reading:" << fileName;
        kDebug() << "Error code:" << reply.errorCode();
        kDebug() << "Error description:" << reply.errorDescription();
        return QString();
    }
    return reply.data().value("fileContents").toString();
}
bool KCMGRUB2::saveFile(const QString &fileName, const QString &fileContents)
{
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << fileContents;
        return true;
    }

    Action saveAction("org.kde.kcontrol.kcmgrub2.save");
    saveAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    saveAction.addArgument("fileName", fileName);
    saveAction.addArgument("fileContents", fileContents);

    ActionReply reply = saveAction.execute();
    if (reply.failed()) {
        KMessageBox::detailedError(this, i18nc("@info", "Error writing <filename>%1</filename>", fileName), i18nc("@info", "Error code: %1<nl/>Error description: %2", reply.errorCode(), reply.errorDescription()));
        return false;
    }
    return true;
}
void KCMGRUB2::updateGRUB(const QString &fileName)
{
    Action updateAction("org.kde.kcontrol.kcmgrub2.update");
    updateAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    updateAction.addArgument("fileName", fileName);

    if (updateAction.authorize() == Action::Authorized) {
        KProgressDialog progressDlg(this, i18nc("@title:window", "Updating GRUB"), i18nc("@info:progress", "Updating the GRUB menu..."));
        progressDlg.setAllowCancel(false);
        progressDlg.progressBar()->setMinimum(0);
        progressDlg.progressBar()->setMaximum(0);
        progressDlg.show();
        connect(updateAction.watcher(), SIGNAL(actionPerformed(ActionReply)), &progressDlg, SLOT(hide()));
        ActionReply reply = updateAction.execute();
        if (reply.succeeded()) {
            KDialog *dialog = new KDialog(this, Qt::Dialog);
            dialog->setCaption(i18nc("@title:window", "Information"));
            dialog->setButtons(KDialog::Ok | KDialog::Details);
            dialog->setModal(true);
            dialog->setDefaultButton(KDialog::Ok);
            dialog->setEscapeButton(KDialog::Ok);
            KMessageBox::createKMessageBox(dialog, QMessageBox::Information, i18nc("@info", "Successfully updated the GRUB menu."), QStringList(), QString(), 0, KMessageBox::Notify, reply.data().value("output").toString());
        } else {
            KMessageBox::detailedError(this, i18nc("@info", "Failed to update the GRUB menu."), reply.data().value("output").toString());
        }
    }
}

bool KCMGRUB2::readDevices()
{
    QStringList mountPoints;
    foreach(const KMountPoint::Ptr mp, KMountPoint::currentMountPoints()) {
        if (mp->mountedFrom().startsWith("/dev")) {
            mountPoints.append(mp->mountPoint());
        }
    }

    Action probeAction("org.kde.kcontrol.kcmgrub2.probe");
    probeAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    probeAction.addArgument("mountPoints", mountPoints);

    if (probeAction.authorize() == Action::Authorized) {
        KProgressDialog progressDlg(this, i18nc("@title:window", "Probing devices"), i18nc("@info:progress", "Probing devices for their GRUB names..."));
        progressDlg.setAllowCancel(false);
        progressDlg.show();
        connect(probeAction.watcher(), SIGNAL(progressStep(int)), progressDlg.progressBar(), SLOT(setValue(int)));
        ActionReply reply = probeAction.execute();
        if (reply.succeeded()) {
            QStringList grubPartitions = reply.data().value("grubPartitions").toStringList();
            if (mountPoints.size() != grubPartitions.size()) {
                KMessageBox::error(this, i18nc("@info", "Helper returned malformed device list."));
                return false;
            }
            for (int i = 0; i < mountPoints.size(); i++) {
                m_devices[mountPoints.at(i)] = grubPartitions.at(i);
            }
        } else {
            KMessageBox::error(this, i18nc("@info", "Failed to get GRUB device names."));
            return false;
        }
    }
    return true;
}
bool KCMGRUB2::readEntries()
{
    QStringList fileNames = Settings::menuPaths();
    for (int i = 0; i < fileNames.size(); i++) {
        QString fileContents = readFile(fileNames.at(i));
        if (!fileContents.isEmpty()) {
            if (i != 0) {
                fileNames.prepend(fileNames.at(i));
                fileNames.removeDuplicates();
                Settings::setMenuPaths(fileNames);
                Settings::self()->writeConfig();
            }
            parseEntries(fileContents);
            return true;
        }
    }
    while (KMessageBox::questionYesNoList(this, i18nc("@info", "<para>None of the following files were readable.</para><para>Select another file?</para>"), fileNames) == KMessageBox::Yes) {
        QString newPath = KFileDialog::getOpenFileName();
        if (newPath.isEmpty()) {
            break;
        }
        fileNames.prepend(newPath);
        fileNames.removeDuplicates();
        Settings::setMenuPaths(fileNames);
        Settings::self()->writeConfig();
        QString fileContents = readFile(newPath);
        if (!fileContents.isEmpty()) {
            parseEntries(fileContents);
            return true;
        }
    }
    return false;
}
bool KCMGRUB2::readSettings()
{
    QStringList fileNames = Settings::configPaths();
    for (int i = 0; i < fileNames.size(); i++) {
        QString fileContents = readFile(fileNames.at(i));
        if (!fileContents.isEmpty()) {
            if (i != 0) {
                fileNames.prepend(fileNames.at(i));
                fileNames.removeDuplicates();
                Settings::setConfigPaths(fileNames);
                Settings::self()->writeConfig();
            }
            parseSettings(fileContents);
            return true;
        }
    }
    while (KMessageBox::questionYesNoList(this, i18nc("@info", "<para>None of the following files were readable.</para><para>Select another file?</para>"), fileNames) == KMessageBox::Yes) {
        QString newPath = KFileDialog::getOpenFileName();
        if (newPath.isEmpty()) {
            break;
        }
        fileNames.prepend(newPath);
        fileNames.removeDuplicates();
        Settings::setConfigPaths(fileNames);
        Settings::self()->writeConfig();
        QString fileContents = readFile(newPath);
        if (!fileContents.isEmpty()) {
            parseSettings(fileContents);
            return true;
        }
    }
    return false;
}

void KCMGRUB2::parseSettings(const QString &config)
{
    QString line, settingsConfig = config;
    QTextStream stream(&settingsConfig, QIODevice::ReadOnly);
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        if (line.startsWith("GRUB_", Qt::CaseInsensitive)) {
            m_settings[line.section('=', 0, 0)] = unquoteWord(line.section('=', 1, QString::SectionIncludeTrailingSep));
        }
    }
}
void KCMGRUB2::parseEntries(const QString &config)
{
    QChar ch;
    QString entry, entriesConfig = config;
    QTextStream stream(&entriesConfig, QIODevice::ReadOnly | QIODevice::Text);
    while (!stream.atEnd()) {
        stream >> ch;
        if (ch != '\n') {
            continue;
        }
        stream.skipWhiteSpace();
        if (stream.atEnd()) {
            return;
        }
        QString word;
        stream >> word;
        if (word.compare("menuentry", Qt::CaseInsensitive) != 0) {
            continue;
        }
        stream.skipWhiteSpace();
        if (stream.atEnd()) {
            return;
        }
        stream >> ch;
        if (ch == '\'') {
            while (true) {
                if (stream.atEnd()) {
                    return;
                }
                stream >> ch;
                if (ch == '\'') {
                    break;
                } else {
                    entry.append(ch);
                }
            }
        } else if (ch == '"') {
            while (true) {
                if (stream.atEnd()) {
                    return;
                }
                stream >> ch;
                if (ch == '\\') {
                    if (stream.atEnd()) {
                        return;
                    }
                    stream >> ch;
                    switch (ch.toAscii()) {
                        case '$':
                        case '"':
                        case '\\':
                            entry.append(ch);
                            break;
                        case '\n':
                            entry.append(' ');
                            break;
                        default:
                            entry.append('\\').append(ch);
                            break;
                    }
                } else if (ch == '"') {
                    break;
                } else {
                    entry.append(ch);
                }
            }
        } else {
            while (true) {
                if (ch == '\\') {
                    if (stream.atEnd()) {
                        return;
                    }
                    stream >> ch;
                    switch (ch.toAscii()) {
                        case '\n':
                            break;
                        default:
                            entry.append(ch);
                            break;
                    }
                } else if (ch.isSpace()) {
                    break;
                } else {
                    entry.append(ch);
                }
                if (stream.atEnd()) {
                    return;
                }
                stream >> ch;
            }
        }
        m_entries.append(entry);
        entry.clear();
    }
}
QString KCMGRUB2::unquoteWord(const QString &word)
{
    QChar ch;
    QString quotedWord = word, unquotedWord;
    QTextStream stream(&quotedWord, QIODevice::ReadOnly | QIODevice::Text);
    while (!stream.atEnd()) {
        stream >> ch;
        if (ch == '\'') {
            while (true) {
                if (stream.atEnd()) {
                    return QString();
                }
                stream >> ch;
                if (ch == '\'') {
                    return unquotedWord;
                } else {
                    unquotedWord.append(ch);
                }
            }
        } else if (ch == '"') {
            while (true) {
                if (stream.atEnd()) {
                    return QString();
                }
                stream >> ch;
                if (ch == '\\') {
                    if (stream.atEnd()) {
                        return QString();
                    }
                    stream >> ch;
                    switch (ch.toAscii()) {
                    case '$':
                    case '"':
                    case '\\':
                        unquotedWord.append(ch);
                        break;
                    case '\n':
                        unquotedWord.append(' ');
                        break;
                    default:
                        unquotedWord.append('\\').append(ch);
                        break;
                    }
                } else if (ch == '"') {
                    return unquotedWord;
                } else {
                    unquotedWord.append(ch);
                }
            }
        } else {
            while (true) {
                if (ch == '\\') {
                    if (stream.atEnd()) {
                        return unquotedWord;
                    }
                    stream >> ch;
                    switch (ch.toAscii()) {
                    case '\n':
                        break;
                    default:
                        unquotedWord.append(ch);
                        break;
                    }
                } else {
                    unquotedWord.append(ch);
                }
                if (stream.atEnd()) {
                    return unquotedWord;
                }
                stream >> ch;
            }
        }
    }
    return QString();
}
