/**
 ******************************************************************************
 *
 * @file       notifypluginoptionspage.cpp
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      Notify Plugin options page
 * @see        The GNU Public License (GPL) Version 3
 * @defgroup   notifyplugin
 * @{
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "notifypluginoptionspage.h"
#include <coreplugin/icore.h>
#include "notificationitem.h"
#include "ui_notifypluginoptionspage.h"
#include "extensionsystem/pluginmanager.h"
#include "utils/pathutils.h"

#include <QFileDialog>
#include <QtAlgorithms>
#include <QStringList>
#include <QtCore/QSettings>
#include <QTableWidget>
#include <QPalette>
#include <QBuffer>

#include "notifyplugin.h"
#include "notifyitemdelegate.h"
#include "notifytablemodel.h"
#include "notifylogging.h"

NotifyPluginOptionsPage::NotifyPluginOptionsPage(/*NotificationItem *config,*/ QObject *parent)
    : IOptionsPage(parent)
    , objManager(*ExtensionSystem::PluginManager::instance()->getObject<UAVObjectManager>())
    , owner(qobject_cast<SoundNotifyPlugin*>(parent))
    , currentCollectionPath("")
{
}

NotifyPluginOptionsPage::~NotifyPluginOptionsPage()
{
}

//creates options page widget (uses the UI file)
QWidget *NotifyPluginOptionsPage::createPage(QWidget *parent)
{
    options_page.reset(new Ui::NotifyPluginOptionsPage());
    //main widget
    QWidget *optionsPageWidget = new QWidget;
    //main layout
    options_page->setupUi(optionsPageWidget);

    listSoundFiles.clear();

    options_page->SoundDirectoryPathChooser->setExpectedKind(Utils::PathChooser::Directory);
    options_page->SoundDirectoryPathChooser->setPromptDialogTitle(tr("Choose sound collection directory"));

    // Fills the combo boxes for the UAVObjects
    QList< QList<UAVDataObject*> > objList = objManager.getDataObjects();
    foreach (QList<UAVDataObject*> list, objList) {
        foreach (UAVDataObject* obj, list) {
            options_page->UAVObject->addItem(obj->getName());
        }
    }

    connect(options_page->SoundDirectoryPathChooser, SIGNAL(changed(const QString&)), this, SLOT(on_buttonSoundFolder_clicked(const QString&)));
    connect(options_page->SoundCollectionList, SIGNAL(currentIndexChanged (int)), this, SLOT(on_soundLanguage_indexChanged(int)));
    connect(options_page->UAVObject, SIGNAL(currentIndexChanged(QString)), this, SLOT(on_UAVObject_indexChanged(QString)));

    connect(this, SIGNAL(updateNotifications(QList<NotificationItem*>)),
        owner, SLOT(updateNotificationList(QList<NotificationItem*>)));
    //connect(this, SIGNAL(resetNotification()),owner, SLOT(resetNotification()));


//    privListNotifications = ((qobject_cast<SoundNotifyPlugin*>(parent))->getListNotifications());
    privListNotifications = owner->getListNotifications();

    updateConfigView(owner->getCurrentNotification());

    initRulesTable();
    initButtons();
    initPhononPlayer();

    return optionsPageWidget;
}

void NotifyPluginOptionsPage::initButtons()
{
    options_page->chkEnableSound->setChecked(owner->getEnableSound());
    connect(options_page->chkEnableSound, SIGNAL(toggled(bool)), this, SLOT(on_chkEnableSound_toggled(bool)));

    options_page->buttonModify->setEnabled(false);
    options_page->buttonDelete->setEnabled(false);
    options_page->buttonPlayNotification->setEnabled(false);
    connect(options_page->buttonAdd, SIGNAL(pressed()), this, SLOT(on_buttonAddNotification_clicked()));
    connect(options_page->buttonDelete, SIGNAL(pressed()), this, SLOT(on_buttonDeleteNotification_clicked()));
    connect(options_page->buttonModify, SIGNAL(pressed()), this, SLOT(on_buttonModifyNotification_clicked()));
    connect(options_page->buttonPlayNotification, SIGNAL(clicked()), this, SLOT(on_buttonTestSoundNotification_clicked()));
}

void NotifyPluginOptionsPage::initPhononPlayer()
{
    notifySound.reset(Phonon::createPlayer(Phonon::NotificationCategory));
    connect(notifySound.data(),SIGNAL(stateChanged(Phonon::State,Phonon::State)),
        this,SLOT(changeButtonText(Phonon::State,Phonon::State)));
    connect(notifySound.data(), SIGNAL(finished(void)), this, SLOT(onFinishedPlaying(void)));
}

void NotifyPluginOptionsPage::initRulesTable()
{
    qNotifyDebug_if(_notifyRulesModel.isNull()) << "_notifyRulesModel.isNull())";
    qNotifyDebug_if(!_notifyRulesSelection) << "_notifyRulesSelection.isNull())";
    //QItemSelectionModel* selection = _notifyRulesSelection.take();
    _notifyRulesModel.reset(new NotifyTableModel(privListNotifications));
    _notifyRulesSelection = new QItemSelectionModel(_notifyRulesModel.data());

    connect(_notifyRulesSelection, SIGNAL(selectionChanged ( const QItemSelection &, const QItemSelection & )),
        this, SLOT(on_tableNotification_changeSelection( const QItemSelection & , const QItemSelection & )));
    connect(this, SIGNAL(entryUpdated(int)),
        _notifyRulesModel.data(), SLOT(entryUpdated(int)));
    connect(this, SIGNAL(entryAdded(int)),
        _notifyRulesModel.data(), SLOT(entryAdded(int)));

    options_page->notifyRulesView->setModel(_notifyRulesModel.data());
    options_page->notifyRulesView->setSelectionModel(_notifyRulesSelection);
    options_page->notifyRulesView->setItemDelegate(new NotifyItemDelegate(this));

    options_page->notifyRulesView->resizeRowsToContents();
    options_page->notifyRulesView->setColumnWidth(eMESSAGE_NAME,200);
    options_page->notifyRulesView->setColumnWidth(eREPEAT_VALUE,120);
    options_page->notifyRulesView->setColumnWidth(eEXPIRE_TIME,100);
    options_page->notifyRulesView->setColumnWidth(eENABLE_NOTIFICATION,60);
    options_page->notifyRulesView->setDragEnabled(true);
    options_page->notifyRulesView->setAcceptDrops(true);
    options_page->notifyRulesView->setDropIndicatorShown(true);
    options_page->notifyRulesView->setDragDropMode(QAbstractItemView::InternalMove);


}

void NotifyPluginOptionsPage::getOptionsPageValues(NotificationItem* notification)
{
    notification->setSoundCollectionPath(options_page->SoundDirectoryPathChooser->path());
    notification->setCurrentLanguage(options_page->SoundCollectionList->currentText());
    notification->setDataObject(options_page->UAVObject->currentText());
    notification->setObjectField(options_page->UAVObjectField->currentText());
    notification->setSound1(options_page->Sound1->currentText());
    notification->setSound2(options_page->Sound2->currentText());
    notification->setSound3(options_page->Sound3->currentText());
    notification->setSayOrder(options_page->SayOrder->currentText());
    notification->setValue(options_page->Value->currentText());
    notification->setSpinBoxValue(options_page->ValueSpinBox->value());
}

/*!
* Called when the user presses apply or OK.
* Saves the current values
*/
void NotifyPluginOptionsPage::apply()
{
    getOptionsPageValues(owner->getCurrentNotification());
    owner->setEnableSound(options_page->chkEnableSound->isChecked());
    emit updateNotifications(privListNotifications);
}

void NotifyPluginOptionsPage::finish()
{
    disconnect(notifySound.data(),SIGNAL(stateChanged(Phonon::State,Phonon::State)),
        this,SLOT(changeButtonText(Phonon::State,Phonon::State)));
    if (notifySound) {
        notifySound->stop();
        notifySound->clear();
    }
}

//////////////////////////////////////////////////////////////////////////////
//  Fills in the <Field> combo box when value is changed in the
//  <Object> combo box
//////////////////////////////////////////////////////////////////////////////
void NotifyPluginOptionsPage::on_UAVObject_indexChanged(QString val) {
    options_page->UAVObjectField->clear();
    ExtensionSystem::PluginManager* pm = ExtensionSystem::PluginManager::instance();
    UAVObjectManager* objManager = pm->getObject<UAVObjectManager>();
    UAVDataObject* obj = dynamic_cast<UAVDataObject*>( objManager->getObject(val) );
    QList<UAVObjectField*> fieldList = obj->getFields();
    foreach (UAVObjectField* field, fieldList) {
        options_page->UAVObjectField->addItem(field->getName());
    }
}

// locate collection folder on disk
void NotifyPluginOptionsPage::on_buttonSoundFolder_clicked(const QString& path)
{
    QDir dirPath(path);
    listDirCollections = dirPath.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    options_page->SoundCollectionList->clear();
    options_page->SoundCollectionList->addItems(listDirCollections);
}


void NotifyPluginOptionsPage::on_soundLanguage_indexChanged(int index)
{
    options_page->SoundCollectionList->setCurrentIndex(index);

    currentCollectionPath = options_page->SoundDirectoryPathChooser->path()
        + QDir::toNativeSeparators("/" + options_page->SoundCollectionList->currentText());

    QDir dirPath(currentCollectionPath);
    QStringList filters;
    filters << "*.mp3" << "*.wav";
    dirPath.setNameFilters(filters);
    listSoundFiles = dirPath.entryList(filters);
    listSoundFiles.replaceInStrings(QRegExp(".mp3|.wav"), "");
    options_page->Sound1->clear();
    options_page->Sound2->clear();
    options_page->Sound3->clear();
    options_page->Sound1->addItems(listSoundFiles);
    options_page->Sound2->addItem("");
    options_page->Sound2->addItems(listSoundFiles);
    options_page->Sound3->addItem("");
    options_page->Sound3->addItems(listSoundFiles);

}

void  NotifyPluginOptionsPage::changeButtonText(Phonon::State newstate, Phonon::State oldstate)
{
    //Q_ASSERT(Phonon::ErrorState != newstate);

    if (newstate  == Phonon::PausedState || newstate  == Phonon::StoppedState) {
        options_page->buttonPlayNotification->setText("Play");
        options_page->buttonPlayNotification->setIcon(QPixmap(":/notify/images/play.png"));
    } else {
        if (newstate  == Phonon::PlayingState) {
            options_page->buttonPlayNotification->setText("Stop");
            options_page->buttonPlayNotification->setIcon(QPixmap(":/notify/images/stop.png"));
        }
    }
}

void  NotifyPluginOptionsPage::onFinishedPlaying()
{
    notifySound->clear();
}

void NotifyPluginOptionsPage::on_buttonTestSoundNotification_clicked()
{
    NotificationItem* notification = NULL;

    if (-1 == _notifyRulesSelection->currentIndex().row())
            return;
    notifySound->clearQueue();
    notification = privListNotifications.at(_notifyRulesSelection->currentIndex().row());
    notification->parseNotifyMessage();
    QStringList sequence = notification->getMessageSequence();
    Q_ASSERT(!!sequence.size());
    foreach(QString item, sequence)
            notifySound->enqueue(Phonon::MediaSource(item));

    notifySound->play();
}

void NotifyPluginOptionsPage::on_chkEnableSound_toggled(bool state)
{
    bool state1 = 1^state;

    QList<Phonon::Path> listOutputs = notifySound->outputPaths();
    Phonon::AudioOutput * audioOutput = (Phonon::AudioOutput*)listOutputs.last().sink();
    audioOutput->setMuted(state1);
}

void NotifyPluginOptionsPage::updateConfigView(NotificationItem* notification)
{
    QString path = notification->getSoundCollectionPath();
    if (path == "") {
            //QDir dir = QDir::currentPath();
            //path = QDir::currentPath().left(QDir::currentPath().indexOf("OpenPilot",0,Qt::CaseSensitive))+"../share/sounds";
            path = Utils::PathUtils().InsertDataPath("%%DATAPATH%%sounds");
    }

    options_page->SoundDirectoryPathChooser->setPath(path);

    if (-1 != options_page->SoundCollectionList->findText(notification->getCurrentLanguage())){
            options_page->SoundCollectionList->setCurrentIndex(options_page->SoundCollectionList->findText(notification->getCurrentLanguage()));
    } else {
            options_page->SoundCollectionList->setCurrentIndex(options_page->SoundCollectionList->findText("default"));
    }

    if (options_page->UAVObject->findText(notification->getDataObject())!=-1){
            options_page->UAVObject->setCurrentIndex(options_page->UAVObject->findText(notification->getDataObject()));
    }

    // Now load the object field values:
    options_page->UAVObjectField->clear();
    QString uavDataObject = notification->getDataObject();
    UAVDataObject* obj = dynamic_cast<UAVDataObject*>(objManager.getObject(uavDataObject));
    if (obj != NULL ) {
            QList<UAVObjectField*> fieldList = obj->getFields();
            foreach (UAVObjectField* field, fieldList) {
                    options_page->UAVObjectField->addItem(field->getName());
            }
    }

    if (-1 != options_page->UAVObjectField->findText(notification->getObjectField())) {
            options_page->UAVObjectField->setCurrentIndex(options_page->UAVObjectField->findText(notification->getObjectField()));
    }

    if (-1 != options_page->Sound1->findText(notification->getSound1())) {
            options_page->Sound1->setCurrentIndex(options_page->Sound1->findText(notification->getSound1()));
    } else {
            // show item from default location
            options_page->SoundCollectionList->setCurrentIndex(options_page->SoundCollectionList->findText("default"));
            options_page->Sound1->setCurrentIndex(options_page->Sound1->findText(notification->getSound1()));

            // don't show item if it wasn't find in stored location
            //options_page->Sound1->setCurrentIndex(-1);
    }

    if (-1 != options_page->Sound2->findText(notification->getSound2())) {
            options_page->Sound2->setCurrentIndex(options_page->Sound2->findText(notification->getSound2()));
    } else {
            // show item from default location
            options_page->SoundCollectionList->setCurrentIndex(options_page->SoundCollectionList->findText("default"));
            options_page->Sound2->setCurrentIndex(options_page->Sound2->findText(notification->getSound2()));

            // don't show item if it wasn't find in stored location
            //options_page->Sound2->setCurrentIndex(-1);
    }

    if (-1 != options_page->Sound3->findText(notification->getSound3())) {
            options_page->Sound3->setCurrentIndex(options_page->Sound3->findText(notification->getSound3()));
    } else {
            // show item from default location
            options_page->SoundCollectionList->setCurrentIndex(options_page->SoundCollectionList->findText("default"));
            options_page->Sound3->setCurrentIndex(options_page->Sound3->findText(notification->getSound3()));
    }

    if (-1 != options_page->Value->findText(notification->getValue())) {
            options_page->Value->setCurrentIndex(options_page->Value->findText(notification->getValue()));
    }

    if (-1 != options_page->SayOrder->findText(notification->getSayOrder())) {
            options_page->SayOrder->setCurrentIndex(options_page->SayOrder->findText(notification->getSayOrder()));
    }

    options_page->ValueSpinBox->setValue(notification->getSpinBoxValue());
}

void NotifyPluginOptionsPage::on_tableNotification_changeSelection( const QItemSelection & selected, const QItemSelection & deselected )
{
    bool select = false;
    notifySound->stop();
    if (selected.indexes().size()) {
            select = true;
            updateConfigView(privListNotifications.at(selected.indexes().at(0).row()));
    }

    options_page->buttonModify->setEnabled(select);
    options_page->buttonDelete->setEnabled(select);
    options_page->buttonPlayNotification->setEnabled(select);
}


void NotifyPluginOptionsPage::on_buttonAddNotification_clicked()
{
    NotificationItem* notification = new NotificationItem;

    if (options_page->SoundDirectoryPathChooser->path()=="") {
            QPalette textPalette=options_page->SoundDirectoryPathChooser->palette();
            textPalette.setColor(QPalette::Normal,QPalette::Text, Qt::red);
            options_page->SoundDirectoryPathChooser->setPalette(textPalette);
            options_page->SoundDirectoryPathChooser->setPath("please select sound collection folder");
            return;
    }

    notification->setSoundCollectionPath(options_page->SoundDirectoryPathChooser->path());
    notification->setCurrentLanguage(options_page->SoundCollectionList->currentText());
    notification->setDataObject(options_page->UAVObject->currentText());
    notification->setObjectField(options_page->UAVObjectField->currentText());
    notification->setValue(options_page->Value->currentText());
    notification->setSpinBoxValue(options_page->ValueSpinBox->value());

    if (options_page->Sound1->currentText().size() > 0)
            notification->setSound1(options_page->Sound1->currentText());

    notification->setSound2(options_page->Sound2->currentText());
    notification->setSound3(options_page->Sound3->currentText());

if ( ((!options_page->Sound2->currentText().size()) && (options_page->SayOrder->currentText()=="After second"))
     || ((!options_page->Sound3->currentText().size()) && (options_page->SayOrder->currentText()=="After third")) ) {
            return;
    } else {
            notification->setSayOrder(options_page->SayOrder->currentText());
    }
    privListNotifications.append(notification);
    emit entryAdded(privListNotifications.size() - 1);
    _notifyRulesSelection->setCurrentIndex(_notifyRulesModel->index(privListNotifications.size()-1,0,QModelIndex()),
                                                                              QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}


void NotifyPluginOptionsPage::on_buttonDeleteNotification_clicked()
{
    _notifyRulesModel->removeRow(_notifyRulesSelection->currentIndex().row());
    if (!_notifyRulesModel->rowCount()
        && (_notifyRulesSelection->currentIndex().row() > 0
        && _notifyRulesSelection->currentIndex().row() < _notifyRulesModel->rowCount()) )
    {
        options_page->buttonDelete->setEnabled(false);
        options_page->buttonModify->setEnabled(false);
        options_page->buttonPlayNotification->setEnabled(false);
    }

}

void NotifyPluginOptionsPage::on_buttonModifyNotification_clicked()
{
    NotificationItem* notification = new NotificationItem;
    getOptionsPageValues(notification);
    notification->setRetryString(privListNotifications.at(_notifyRulesSelection->currentIndex().row())->retryString());
    notification->setLifetime(privListNotifications.at(_notifyRulesSelection->currentIndex().row())->lifetime());
    notification->setMute(privListNotifications.at(_notifyRulesSelection->currentIndex().row())->mute());

    privListNotifications.replace(_notifyRulesSelection->currentIndex().row(),notification);
    entryUpdated(_notifyRulesSelection->currentIndex().row());

}

