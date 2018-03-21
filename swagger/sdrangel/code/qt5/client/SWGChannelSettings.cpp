/**
 * SDRangel
 * This is the web REST/JSON API of SDRangel SDR software. SDRangel is an Open Source Qt5/OpenGL 3.0+ (4.3+ in Windows) GUI and server Software Defined Radio and signal analyzer in software. It supports Airspy, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube     ---   Limitations and specifcities:       * In SDRangel GUI the first Rx device set cannot be deleted. Conversely the server starts with no device sets and its number of device sets can be reduced to zero by as many calls as necessary to /sdrangel/deviceset with DELETE method.   * Stopping instance i.e. /sdrangel with DELETE method is a server only feature. It allows stopping the instance nicely.   * Preset import and export from/to file is a server only feature.   * Device set focus is a GUI only feature.   * The following channels are not implemented (status 501 is returned): ATV demodulator, Channel Analyzer, Channel Analyzer NG, LoRa demodulator, TCP source   * The content type returned is always application/json except in the following cases:     * An incorrect URL was specified: this document is returned as text/html with a status 400    --- 
 *
 * OpenAPI spec version: 4.0.0
 * Contact: f4exb06@gmail.com
 *
 * NOTE: This class is auto generated by the swagger code generator program.
 * https://github.com/swagger-api/swagger-codegen.git
 * Do not edit the class manually.
 */


#include "SWGChannelSettings.h"

#include "SWGHelpers.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>

namespace SWGSDRangel {

SWGChannelSettings::SWGChannelSettings(QString* json) {
    init();
    this->fromJson(*json);
}

SWGChannelSettings::SWGChannelSettings() {
    channel_type = nullptr;
    m_channel_type_isSet = false;
    tx = 0;
    m_tx_isSet = false;
    am_demod_settings = nullptr;
    m_am_demod_settings_isSet = false;
    nfm_demod_settings = nullptr;
    m_nfm_demod_settings_isSet = false;
    nfm_mod_settings = nullptr;
    m_nfm_mod_settings_isSet = false;
}

SWGChannelSettings::~SWGChannelSettings() {
    this->cleanup();
}

void
SWGChannelSettings::init() {
    channel_type = new QString("");
    m_channel_type_isSet = false;
    tx = 0;
    m_tx_isSet = false;
    am_demod_settings = new SWGAMDemodSettings();
    m_am_demod_settings_isSet = false;
    nfm_demod_settings = new SWGNFMDemodSettings();
    m_nfm_demod_settings_isSet = false;
    nfm_mod_settings = new SWGNFMModSettings();
    m_nfm_mod_settings_isSet = false;
}

void
SWGChannelSettings::cleanup() {
    if(channel_type != nullptr) { 
        delete channel_type;
    }

    if(am_demod_settings != nullptr) { 
        delete am_demod_settings;
    }
    if(nfm_demod_settings != nullptr) { 
        delete nfm_demod_settings;
    }
    if(nfm_mod_settings != nullptr) { 
        delete nfm_mod_settings;
    }
}

SWGChannelSettings*
SWGChannelSettings::fromJson(QString &json) {
    QByteArray array (json.toStdString().c_str());
    QJsonDocument doc = QJsonDocument::fromJson(array);
    QJsonObject jsonObject = doc.object();
    this->fromJsonObject(jsonObject);
    return this;
}

void
SWGChannelSettings::fromJsonObject(QJsonObject &pJson) {
    ::SWGSDRangel::setValue(&channel_type, pJson["channelType"], "QString", "QString");
    
    ::SWGSDRangel::setValue(&tx, pJson["tx"], "qint32", "");
    
    ::SWGSDRangel::setValue(&am_demod_settings, pJson["AMDemodSettings"], "SWGAMDemodSettings", "SWGAMDemodSettings");
    
    ::SWGSDRangel::setValue(&nfm_demod_settings, pJson["NFMDemodSettings"], "SWGNFMDemodSettings", "SWGNFMDemodSettings");
    
    ::SWGSDRangel::setValue(&nfm_mod_settings, pJson["NFMModSettings"], "SWGNFMModSettings", "SWGNFMModSettings");
    
}

QString
SWGChannelSettings::asJson ()
{
    QJsonObject* obj = this->asJsonObject();

    QJsonDocument doc(*obj);
    QByteArray bytes = doc.toJson();
    delete obj;
    return QString(bytes);
}

QJsonObject*
SWGChannelSettings::asJsonObject() {
    QJsonObject* obj = new QJsonObject();
    if(channel_type != nullptr && *channel_type != QString("")){
        toJsonValue(QString("channelType"), channel_type, obj, QString("QString"));
    }
    if(m_tx_isSet){
        obj->insert("tx", QJsonValue(tx));
    }
    if((am_demod_settings != nullptr) && (am_demod_settings->isSet())){
        toJsonValue(QString("AMDemodSettings"), am_demod_settings, obj, QString("SWGAMDemodSettings"));
    }
    if((nfm_demod_settings != nullptr) && (nfm_demod_settings->isSet())){
        toJsonValue(QString("NFMDemodSettings"), nfm_demod_settings, obj, QString("SWGNFMDemodSettings"));
    }
    if((nfm_mod_settings != nullptr) && (nfm_mod_settings->isSet())){
        toJsonValue(QString("NFMModSettings"), nfm_mod_settings, obj, QString("SWGNFMModSettings"));
    }

    return obj;
}

QString*
SWGChannelSettings::getChannelType() {
    return channel_type;
}
void
SWGChannelSettings::setChannelType(QString* channel_type) {
    this->channel_type = channel_type;
    this->m_channel_type_isSet = true;
}

qint32
SWGChannelSettings::getTx() {
    return tx;
}
void
SWGChannelSettings::setTx(qint32 tx) {
    this->tx = tx;
    this->m_tx_isSet = true;
}

SWGAMDemodSettings*
SWGChannelSettings::getAmDemodSettings() {
    return am_demod_settings;
}
void
SWGChannelSettings::setAmDemodSettings(SWGAMDemodSettings* am_demod_settings) {
    this->am_demod_settings = am_demod_settings;
    this->m_am_demod_settings_isSet = true;
}

SWGNFMDemodSettings*
SWGChannelSettings::getNfmDemodSettings() {
    return nfm_demod_settings;
}
void
SWGChannelSettings::setNfmDemodSettings(SWGNFMDemodSettings* nfm_demod_settings) {
    this->nfm_demod_settings = nfm_demod_settings;
    this->m_nfm_demod_settings_isSet = true;
}

SWGNFMModSettings*
SWGChannelSettings::getNfmModSettings() {
    return nfm_mod_settings;
}
void
SWGChannelSettings::setNfmModSettings(SWGNFMModSettings* nfm_mod_settings) {
    this->nfm_mod_settings = nfm_mod_settings;
    this->m_nfm_mod_settings_isSet = true;
}


bool
SWGChannelSettings::isSet(){
    bool isObjectUpdated = false;
    do{
        if(channel_type != nullptr && *channel_type != QString("")){ isObjectUpdated = true; break;}
        if(m_tx_isSet){ isObjectUpdated = true; break;}
        if(am_demod_settings != nullptr && am_demod_settings->isSet()){ isObjectUpdated = true; break;}
        if(nfm_demod_settings != nullptr && nfm_demod_settings->isSet()){ isObjectUpdated = true; break;}
        if(nfm_mod_settings != nullptr && nfm_mod_settings->isSet()){ isObjectUpdated = true; break;}
    }while(false);
    return isObjectUpdated;
}
}

