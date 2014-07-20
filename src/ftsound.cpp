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

#include "ftsound.h"

#include <iostream>
#include <deque>
using namespace std;

#include <qmath.h>
#include <qendian.h>
#include <QMenu>
#include <QMessageBox>
#include "wmainwindow.h"
#include "ui_wmainwindow.h"

#include "../external/mkfilter/mkfilter.h"


double FTSound::fs_common = 0; // Initially, fs is undefined
double FTSound::s_play_power = 0;
std::deque<double> FTSound::s_play_power_values;

FTSound::FTSound(const QString& _fileName, QObject *parent)
    : QIODevice(parent)
    , FileType(FTSOUND, _fileName, this)
    , wavtoplay(&wav)
    , m_pos(0)
    , m_end(0)
    , m_ampscale(1.0)
    , m_delay(0)
{
    m_actionInvPolarity = new QAction("Inverse polarity", this);
    m_actionInvPolarity->setStatusTip(tr("Inverse the polarity of the sound"));
    m_actionInvPolarity->setCheckable(true);
    m_actionInvPolarity->setChecked(false);

    m_actionResetAmpScale = new QAction("Reset amplitude", this);
    m_actionResetAmpScale->setStatusTip(tr("Reset the amplitude scaling to 1"));

    m_actionResetDelay = new QAction("Reset the delay", this);
    m_actionResetDelay->setStatusTip(tr("Reset the delay to 0s"));

    load(_fileName);

    m_wavmaxamp = 0.0;
    for(unsigned int n=0; n<wav.size(); ++n)
        m_wavmaxamp = std::max(m_wavmaxamp, abs(wav[n]));

    std::cout << wav.size() << " samples loaded (" << wav.size()/fs << "s max amplitude=" << m_wavmaxamp << ")" << endl;

//    QIODevice::open(QIODevice::ReadOnly);
}

void FTSound::fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow) {

    FileType::fillContextMenu(contextmenu, mainwindow);

    contextmenu.setTitle("Sound");

    contextmenu.addAction(mainwindow->ui->actionPlay);
    contextmenu.addAction(m_actionInvPolarity);
    connect(m_actionInvPolarity, SIGNAL(toggled(bool)), mainwindow, SLOT(soundsChanged()));
    m_actionResetAmpScale->setText(QString("Reset amplitude scaling (%1dB) to 0dB").arg(20*log10(m_ampscale), 0, 'g', 3));
    m_actionResetAmpScale->setDisabled(m_ampscale==1.0);
    contextmenu.addAction(m_actionResetAmpScale);
    connect(m_actionResetAmpScale, SIGNAL(triggered()), mainwindow, SLOT(resetAmpScale()));
    m_actionResetDelay->setText(QString("Reset delay (%1s) to 0s").arg(m_delay/mainwindow->getFs(), 0, 'g', 3));
    m_actionResetDelay->setDisabled(m_delay==0);
    contextmenu.addAction(m_actionResetDelay);
    connect(m_actionResetDelay, SIGNAL(triggered()), mainwindow, SLOT(resetDelay()));
}

double FTSound::getLastSampleTime() const {
    return (wav.size()-1)/fs;
}

void FTSound::setSamplingRate(double _fs){

    fs = _fs;

    // Check if fs is the same for all files
    if(fs_common==0){
        // The system has no defined sampling rate
        fs_common = fs;
    }
    else {
        // Check if fs is the same as that of the other files
        if(fs_common!=fs)
            throw QString("The sampling rate is not the same as that of the files already loaded.");
    }
}

double FTSound::setPlay(const QAudioFormat& format, double tstart, double tstop, double fstart, double fstop)
{
    if(tstart>tstop){
        double tmp = tstop;
        tstop = tstart;
        tstart = tmp;
    }
    if(fstart>fstop){
        double tmp = fstop;
        fstop = fstart;
        fstart = tmp;
    }

    // Fix frequency cutoffs
    // Cannot ensure the numerical stability for very small cutoff
    // Thus, force clip the desired values
    if(fstart<10) fstart=0;
    if(fstart>fs/2-10) fstart=fs/2;
    if(fstop<10) fstop=0;
    if(fstop>fs/2-10) fstop=fs/2;

    bool doLowPass = fstop>0.0 && fstop<fs/2;
    bool doHighPass = fstart>0.0 && fstart<fs/2;

    if ((fstart<fstop) && (doLowPass || doHighPass)) {
        try{
            wavfiltered = wav;

            std::vector<double> num, den;
            if (doLowPass) {
                mkfilter::make_butterworth_filter(4, fstop/fs, true, num, den);
                // mkfilter::make_chebyshev_filter(8, fstop/fs, -1, true, num, den);

//                cout << "LP-filtering (cutoff=" << fstop << " num0=" << num[0] << " size=" << wavfiltered.size() << ")" << endl;
                mkfilter::filtfilt(wavfiltered, num, den, wavfiltered);
            }

            if (doHighPass) {
                std::vector<double> num, den;
                mkfilter::make_butterworth_filter(4, fstart/fs, false, num, den);
                // mkfilter::make_chebyshev_filter(8, fstart/fs, -1, true, num, den);

//                cout << "HP-filtering (cutoff=" << fstart << " num0=" << num[0] << " size=" << wavfiltered.size() << ")" << endl;
                mkfilter::filtfilt(wavfiltered, num, den, wavfiltered);
            }

            // It seems the filtering went well, we can use the filtered sound
            wavtoplay = &wavfiltered;
        }
        catch(QString err){
            QMessageBox::warning(NULL, "Problem when filtering the sound to play", QString("The sound cannot be filtered as given by the selection in the spectrum view.\n\nReason:\n")+err+QString("\n\nPlaying the non-filtered sound ..."));
            // cout << err.toLocal8Bit().constData() << endl; // TODO Message
            wavtoplay = &wav;
        }
    }
    else
        wavtoplay = &wav;


    m_outputaudioformat = format;

    s_play_power = 0;
    s_play_power_values.clear();

    if(tstart==0.0 && tstop==0.0){
        m_start = 0;
        m_pos = m_start;
        m_end = wavtoplay->size()-1;
    }
    else{
        m_start = int(0.5+tstart*fs);
        m_pos = m_start;
        m_end = int(0.5+tstop*fs);
    }

    if(m_start<0) m_start=0;
    if(m_start>wavtoplay->size()-1) m_start=wavtoplay->size()-1;

    if(m_end<0) m_end=0;
    if(m_end>wavtoplay->size()-1) m_end=wavtoplay->size()-1;

    QIODevice::open(QIODevice::ReadOnly);

    double tobeplayed = double(m_end-m_pos+1)/fs;

//    std::cout << "DSSound::start [" << tstart << "s(" << m_pos << "), " << tstop << "s(" << m_end << ")] " << tobeplayed << "s" << endl;

    return tobeplayed;
}

void FTSound::stop()
{
    m_start = 0;
    m_pos = 0;
    m_end = 0;
    QIODevice::close();
}

qint64 FTSound::readData(char *data, qint64 len)
{
//    std::cout << "DSSound::readData requested=" << len << endl;

    qint64 writtenbytes = 0; // [bytes]

/*    while (len - writtenbytes > 0) {
        const qint64 chunk = qMin((m_buffer.size() - m_pos), len - writtenbytes);
        memcpy(data + writtenbytes, m_buffer.constData() + m_pos, chunk);
        m_pos = (m_pos + chunk) % m_buffer.size();
        writtenbytes += chunk;
    }*/

    const int channelBytes = m_outputaudioformat.sampleSize() / 8;

    unsigned char *ptr = reinterpret_cast<unsigned char *>(data);

    // Polarity apparently matters in very particular cases
    // so take it into account when playing.
    qreal a = m_ampscale;
    if(m_actionInvPolarity->isChecked())
        a *= -1;

    // Write as many bits has requested by the call
    while(writtenbytes<len) {

        int depos = m_pos - m_delay;
        if(depos>=0 && depos<int(wavtoplay->size())){
    //        double e = (*wavtoplay)[m_pos]*(*wavtoplay)[m_pos];
    //        s_play_power += e;
            double e = abs(a*(*wavtoplay)[depos]);
            s_play_power_values.push_front(e);
            while(s_play_power_values.size()/fs>0.1){
                s_play_power -= s_play_power_values.back();
                s_play_power_values.pop_back();
            }
        }

//        cout << 20*log10(sqrt(s_play_power/s_play_power_values.size())) << endl;

        qint16 value;
        // Assuming the output audio device has been open in 16bits ...
        // TODO Manage more output formats
        if(depos>=0 && depos<int(wavtoplay->size()) && m_pos<=m_end)
            value=qint16((a*(*wavtoplay)[depos])*32767);
        else
            value=0;

        qToLittleEndian<qint16>(value, ptr);
        ptr += channelBytes;
        writtenbytes += channelBytes;

        if(m_pos<m_end)
            m_pos++;
    }

    s_play_power = 0;
    for(unsigned int i=0; i<s_play_power_values.size(); i++)
        s_play_power = max(s_play_power, s_play_power_values[i]);

//    std::cout << "~DSSound::readData writtenbytes=" << writtenbytes << " m_pos=" << m_pos << " m_end=" << m_end << endl;

    if(m_pos>=m_end){
//        std::cout << "STOP!!!" << endl;
//        QIODevice::close(); // TODO do this instead ??
//        emit readChannelFinished();
        return writtenbytes;
    }
    else{
        return writtenbytes;
    }
}

qint64 FTSound::writeData(const char *data, qint64 len){

    Q_UNUSED(data)
    Q_UNUSED(len)

    std::cerr << "DSSound::writeData: There is no reason to call this function.";

    return 0;
}

FTSound::~FTSound(){
    QIODevice::close();

    delete m_actionShow;
}


//qint64 DSSound::bytesAvailable() const
//{
//    std::cout << "DSSound::bytesAvailable " << QIODevice::bytesAvailable() << endl;

//    return QIODevice::bytesAvailable();
////    return m_buffer.size() + QIODevice::bytesAvailable();
//}

/*
void Generator::generateData(const QAudioFormat &format, qint64 durationUs, int sampleRate)
{
    const int channelBytes = format.sampleSize() / 8;
//    std::cout << channelBytes << endl;
    const int sampleBytes = format.channelCount() * channelBytes;

    qint64 length = (format.sampleRate() * format.channelCount() * (format.sampleSize() / 8))
                        * durationUs / 1000000;

    Q_ASSERT(length % sampleBytes == 0);
    Q_UNUSED(sampleBytes) // suppress warning in release builds

    m_buffer.resize(length);
    unsigned char *ptr = reinterpret_cast<unsigned char *>(m_buffer.data());
    int sampleIndex = 0;

    while (length) {
        const qreal x = qSin(2 * M_PI * sampleRate * qreal(sampleIndex % format.sampleRate()) / format.sampleRate());
        for (int i=0; i<format.channelCount(); ++i) {
            if (format.sampleSize() == 8 && format.sampleType() == QAudioFormat::UnSignedInt) {
                const quint8 value = static_cast<quint8>((1.0 + x) / 2 * 255);
                *reinterpret_cast<quint8*>(ptr) = value;
            } else if (format.sampleSize() == 8 && format.sampleType() == QAudioFormat::SignedInt) {
                const qint8 value = static_cast<qint8>(x * 127);
                *reinterpret_cast<quint8*>(ptr) = value;
            } else if (format.sampleSize() == 16 && format.sampleType() == QAudioFormat::UnSignedInt) {
                quint16 value = static_cast<quint16>((1.0 + x) / 2 * 65535);
                if (format.byteOrder() == QAudioFormat::LittleEndian)
                    qToLittleEndian<quint16>(value, ptr);
                else
                    qToBigEndian<quint16>(value, ptr);
            } else if (format.sampleSize() == 16 && format.sampleType() == QAudioFormat::SignedInt) {
                qint16 value = static_cast<qint16>(x * 32767);
                if (format.byteOrder() == QAudioFormat::LittleEndian)
                    qToLittleEndian<qint16>(value, ptr);
                else
                    qToBigEndian<qint16>(value, ptr);
            }

            ptr += channelBytes;
            length -= channelBytes;
        }
        ++sampleIndex;
    }
}
*/