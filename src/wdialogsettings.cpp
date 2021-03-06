/*
Copyright (C) 2014  Gilles Degottex <gilles.degottex@gmail.com>

This file is part of DFasma.

DFasma is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DFasma is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License is available in the LICENSE.txt
file provided in the source code of DFasma. Another copy can be found at
<http://www.gnu.org/licenses/>.
*/

#include "wdialogsettings.h"
#include "ui_wdialogsettings.h"

#include <iostream>
#include <QSettings>
#include <QMessageBox>
#include <QTextCodec>
#include <QFontDialog>
#include <QPixmapCache>
#include "../external/libqxt/qxtspanslider.h"

#include "ftsound.h"
#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "gvwaveform.h"
#include "gvspectrogram.h"
#include "gvspectrumamplitude.h"
#include "gvspectrumphase.h"
#include "gvspectrumgroupdelay.h"
#include "gvspectrogramwdialogsettings.h"
#include "ftlabels.h"
#include "ftfzero.h"

WDialogSettings::WDialogSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WDialogSettings)
{
    ui->setupUi(this);
    ui->btnSettingsSave->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui->btnSettingsClear->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
    ui->lblViewsCacheLimit->hide();
    ui->sbViewsCacheLimit->hide();

    // Fill the comboboxes
    QList<QByteArray> avcodecs = QTextCodec::availableCodecs();
    qSort(avcodecs);
    for(int ai=0; ai<avcodecs.size(); ++ai)
        ui->cbLabelsDefaultTextEncoding->addItem(avcodecs[ai]);
    ui->cbLabelsDefaultTextEncoding->setCurrentText("UTF-8");
    ui->cbLabelsDefaultFormat->addItem(FTLabels::s_formatstrings[FTLabels::FFTEXTTimeText]);
    ui->cbLabelsDefaultFormat->addItem(FTLabels::s_formatstrings[FTLabels::FFTEXTSegmentsFloat]);
    ui->cbLabelsDefaultFormat->addItem(FTLabels::s_formatstrings[FTLabels::FFTEXTSegmentsSample]);
    ui->cbLabelsDefaultFormat->addItem(FTLabels::s_formatstrings[FTLabels::FFTEXTSegmentsHTK]);
    #ifdef SUPPORT_SDIF
        ui->cbLabelsDefaultFormat->addItem(FTLabels::s_formatstrings[FTLabels::FFSDIF]);
    #endif
    ui->cbLabelsDefaultFormat->setCurrentText(FTLabels::s_formatstrings[FTLabels::FFTEXTTimeText]);

    ui->cbF0DefaultFormat->addItem(FTFZero::s_formatstrings[FTFZero::FFAsciiTimeValue]);
    #ifdef SUPPORT_SDIF
        ui->cbF0DefaultFormat->addItem(FTFZero::s_formatstrings[FTFZero::FFSDIF]);
    #endif
    ui->cbF0DefaultFormat->setCurrentText(FTFZero::s_formatstrings[FTFZero::FFAsciiTimeValue]);


    setWindowIcon(QIcon(":/icons/settings.svg"));
    setWindowIconText("Settings");
    setWindowTitle("Settings");

    connect(ui->ckPlaybackAvoidClicksAddWindows, SIGNAL(toggled(bool)), this, SLOT(setCKAvoidClicksAddWindows(bool)));
    connect(ui->sbPlaybackButterworthOrder, SIGNAL(valueChanged(int)), this, SLOT(setSBButterworthOrderChangeValue(int)));
    connect(ui->sbPlaybackAvoidClicksWindowDuration, SIGNAL(valueChanged(double)), this, SLOT(setSBAvoidClicksWindowDuration(double)));
    connect(ui->btnSettingsSave, SIGNAL(clicked()), this, SLOT(settingsSave()));
    connect(ui->btnSettingsClear, SIGNAL(clicked()), this, SLOT(settingsClear()));  
    connect(ui->sbViewsCacheLimit, SIGNAL(valueChanged(int)), this, SLOT(setCacheLimit(int)));

    gMW->m_settings.add(ui->sbPlaybackButterworthOrder);
    gMW->m_settings.add(ui->cbPlaybackFilteringCompensateEnergy);
    gMW->m_settings.add(ui->ckPlaybackAvoidClicksAddWindows);
    gMW->m_settings.add(ui->sbPlaybackAvoidClicksWindowDuration);
    gMW->m_settings.add(ui->cbLabelsDefaultTextEncoding, true);
    gMW->m_settings.add(ui->cbLabelsDefaultFormat);
    gMW->m_settings.add(ui->cbF0DefaultFormat);
    gMW->m_settings.add(ui->sbViewsToolBarSizes);
    gMW->m_settings.add(ui->sbFileListItemSize);
    gMW->m_settings.add(ui->sbViewsTimeDecimals);
    gMW->m_settings.add(ui->cbViewsShowMusicNoteNames);
    gMW->m_settings.add(ui->cbViewsAddMarginsOnSelection);
    gMW->m_settings.add(ui->cbViewsScrollBarsShow);
    gMW->m_settings.add(ui->sbViewsCacheLimit);
    gMW->m_settings.addFont(ui->lblGridFontSample);
    gMW->m_settings.add(ui->dsbEstimationF0Min);
    gMW->m_settings.add(ui->dsbEstimationF0Max);
    ui->pbGridFontChange->setText(ui->lblGridFontSample->font().family());

    // Load the documentation
    QFile docfile(":/doc_content.html");
    docfile.open(QFile::ReadOnly | QFile::Text);
    QString style = "<style>li { margin-bottom: 0.5em; }\n h4 { margin-top: 1.5em; margin-bottom: 0em; }</style>";
    ui->textBrowser->clear();
    ui->textBrowser->insertHtml(style+QTextStream(&docfile).readAll());
    QTextCursor txtcursor;
    txtcursor.movePosition(QTextCursor::Start);
    ui->textBrowser->setTextCursor(txtcursor);
    
    adjustSize();
}

void WDialogSettings::settingsSave() {

    // Save all the automatic settings
    gMW->m_settings.saveAll();

    // Save the particular global settings
    gMW->m_settings.setValue("cbPlaybackAudioOutputDevices", ui->cbPlaybackAudioOutputDevices->currentText());

    // If some files are loaded, save the geometry of the views
    if(gFL->count()>0){
        // Save the particular settings of different widgets
        gMW->m_settings.setValue("m_qxtSpectrogramSpanSlider_lower", gMW->m_qxtSpectrogramSpanSlider->lowerValue());
        gMW->m_settings.setValue("m_qxtSpectrogramSpanSlider_upper", gMW->m_qxtSpectrogramSpanSlider->upperValue());

        QString strMain;
        QList<int> sizeslist = gMW->ui->splitterMain->sizes();
        for(QList<int>::iterator it=sizeslist.begin(); it!=sizeslist.end(); it++){
            if(*it==0)
                strMain += "100 ";
            else
                strMain += QString::number(*it) + " ";
        }
        gMW->m_settings.setValue("splitterMain", strMain);

        QString strViews;
        sizeslist = gMW->ui->splitterViews->sizes();
        for(QList<int>::iterator it=sizeslist.begin(); it!=sizeslist.end(); it++) {
            if(*it==0)
                strViews += "100 ";
            else
                strViews += QString::number(*it) + " ";
        }
        gMW->m_settings.setValue("splitterViews", strViews);

        QString strSpectra;
        sizeslist = gMW->ui->splitterSpectra->sizes();
        for(QList<int>::iterator it=sizeslist.begin(); it!=sizeslist.end(); it++) {
            if(*it==0)
                strSpectra += "100 ";
            else
                strSpectra += QString::number(*it) + " ";
        }
        gMW->m_settings.setValue("splitterSpectra", strSpectra);
    }
}
void WDialogSettings::settingsClear() {
    gMW->m_settings.clearAll();
    QMessageBox::warning(this, "Factory settings restored", "<p>The settings have been restored to their original values.</p><p>Please restart DFasma</p>");
}


void WDialogSettings::setSBButterworthOrderChangeValue(int order) {
    if(order%2==1)
        ui->sbPlaybackButterworthOrder->setValue(order+1);
}

void WDialogSettings::setCKAvoidClicksAddWindows(bool add) {
    FTSound::s_playwin_use = add;
    FTSound::setAvoidClicksWindowDuration(ui->sbPlaybackAvoidClicksWindowDuration->value());
    ui->sbPlaybackAvoidClicksWindowDuration->setEnabled(add);
    ui->lblAvoidClicksWindowDurationLabel->setEnabled(add);
}

void WDialogSettings::setSBAvoidClicksWindowDuration(double halfduration) {
    FTSound::setAvoidClicksWindowDuration(halfduration);
}

void WDialogSettings::changeFont() {
    QFontDialog dlg(ui->lblGridFontSample->font(), this);
    if(dlg.exec()==QDialog::Accepted){
        ui->lblGridFontSample->setFont(dlg.selectedFont());
        gMW->m_gvWaveform->m_giGrid->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        gMW->m_gvSpectrogram->m_giGrid->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        gMW->m_gvSpectrumAmplitude->m_giGrid->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        gMW->m_gvSpectrumPhase->m_giGrid->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        gMW->m_gvSpectrumGroupDelay->m_giGrid->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        ui->pbGridFontChange->setText(dlg.selectedFont().family());
    }
}

void WDialogSettings::setCacheLimit(int limitmega) {
    QPixmapCache::setCacheLimit(limitmega*1024);
}

WDialogSettings::~WDialogSettings() {
    delete ui;
}
