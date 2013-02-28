/**
 ******************************************************************************
 *
 * @file       spectrogramscope.cpp
 * @author     Tau Labs, http://www.taulabs.org Copyright (C) 2013.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup ScopePlugin Scope Gadget Plugin
 * @{
 * @brief The scope Gadget, graphically plots the states of UAVObjects
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

#include "spectrogramplotdata.h"
#include "scopes3d/spectrogramscopeconfig.h"
#include "coreplugin/icore.h"
#include "coreplugin/connectionmanager.h"


/**
 * @brief SpectrogramScope::SpectrogramScope Default constructor
 */
SpectrogramScope::SpectrogramScope()
{
    m_refreshInterval = 50; //TODO: This should not be set here. Probably should come from a define somewhere.
    yAxisUnits = "";
    timeHorizon = 60;
    samplingFrequency = 100;
    windowWidth = 64;
    zMaximum = 120;
    colorMapType = ColorMap::STANDARD;
}


/**
 * @brief SpectrogramScope::SpectrogramScope Constructor using the XML settings
 * @param qSettings settings XML object
 */
SpectrogramScope::SpectrogramScope(QSettings *qSettings)
{
    timeHorizon = qSettings->value("timeHorizon").toDouble();
    samplingFrequency = qSettings->value("samplingFrequency").toDouble();
    windowWidth       = qSettings->value("windowWidth").toInt();
    zMaximum = qSettings->value("zMaximum").toDouble();
    colorMapType = (ColorMap::ColorMapType) qSettings->value("colorMap").toInt();

    int plot3dCurveCount = qSettings->value("dataSourceCount").toInt();

    for(int i = 0; i < plot3dCurveCount; i++){
        Plot3dCurveConfiguration *plotCurveConf = new Plot3dCurveConfiguration();

        qSettings->beginGroup(QString("spectrogramDataSource") + QString().number(i));

        plotCurveConf->uavObjectName = qSettings->value("uavObject").toString();
        plotCurveConf->uavFieldName  = qSettings->value("uavField").toString();
        plotCurveConf->color         = qSettings->value("color").value<QRgb>();
        plotCurveConf->yScalePower   = qSettings->value("yScalePower").toInt();
        plotCurveConf->mathFunction  = qSettings->value("mathFunction").toString();
        plotCurveConf->yMeanSamples  = qSettings->value("yMeanSamples").toInt();

        plotCurveConf->yMinimum = qSettings->value("yMinimum").toDouble();
        plotCurveConf->yMaximum = qSettings->value("yMaximum").toDouble();

        //Stop reading XML block
        qSettings->endGroup();

        m_spectrogramSourceConfigs.append(plotCurveConf);
    }
}


/**
 * @brief SpectrogramScope::SpectrogramScope Constructor using the GUI settings
 * @param options_page GUI settings preference pane
 */
SpectrogramScope::SpectrogramScope(Ui::ScopeGadgetOptionsPage *options_page)
{
    bool parseOK = false;

    windowWidth = options_page->sbSpectrogramWidth->value();
    samplingFrequency = options_page->sbSpectrogramFrequency->value();
    timeHorizon = options_page->sbSpectrogramTimeHorizon->value();
    zMaximum = options_page->spnMaxSpectrogramZ->value();
    colorMapType = (ColorMap::ColorMapType) options_page->cmbColorMapSpectrogram->itemData(options_page->cmbColorMapSpectrogram->currentIndex()).toInt();

    Plot3dCurveConfiguration* newPlotCurveConfigs = new Plot3dCurveConfiguration();
    newPlotCurveConfigs->uavObjectName = options_page->cmbUAVObjectsSpectrogram->currentText();
    newPlotCurveConfigs->uavFieldName  = options_page->cmbUavoFieldSpectrogram->currentText();
    newPlotCurveConfigs->yScalePower   = options_page->sbSpectrogramDataMultiplier->value();
    newPlotCurveConfigs->yMeanSamples  = options_page->spnMeanSamplesSpectrogram->value();
    newPlotCurveConfigs->mathFunction  = options_page->cmbMathFunctionSpectrogram->currentText();

    QVariant varColor = (int)QColor(options_page->btnColorSpectrogram->text()).rgb();
    int rgb = varColor.toInt(&parseOK);
    if(!parseOK)
        newPlotCurveConfigs->color = QColor(Qt::red).rgb();
    else
        newPlotCurveConfigs->color = (QRgb) rgb;

    m_spectrogramSourceConfigs.append(newPlotCurveConfigs);

}

SpectrogramScope::~SpectrogramScope()
{

}


/**
 * @brief SpectrogramScope::cloneScope Clones scope from existing GUI configuration
 * @param originalScope
 * @return
 */
ScopesGeneric* SpectrogramScope::cloneScope(ScopesGeneric *originalScope)
{
    SpectrogramScope *originalSpectrogramScope = (SpectrogramScope*) originalScope;
    SpectrogramScope *cloneObj = new SpectrogramScope();

    cloneObj->timeHorizon = originalSpectrogramScope->timeHorizon;
    cloneObj->colorMapType = originalSpectrogramScope->colorMapType;

    int plotCurveCount = originalSpectrogramScope->m_spectrogramSourceConfigs.size();

    for (int i = 0; i < plotCurveCount; i++){
        Plot3dCurveConfiguration *currentPlotCurveConf = originalSpectrogramScope->m_spectrogramSourceConfigs.at(i);
        Plot3dCurveConfiguration *newSpectrogramConf     = new Plot3dCurveConfiguration();

        newSpectrogramConf->uavObjectName = currentPlotCurveConf->uavObjectName;
        newSpectrogramConf->uavFieldName  = currentPlotCurveConf->uavFieldName;
        newSpectrogramConf->color         = currentPlotCurveConf->color;
        newSpectrogramConf->yScalePower   = currentPlotCurveConf->yScalePower;
        newSpectrogramConf->yMeanSamples  = currentPlotCurveConf->yMeanSamples;
        newSpectrogramConf->mathFunction  = currentPlotCurveConf->mathFunction;

        newSpectrogramConf->yMinimum = currentPlotCurveConf->yMinimum;
        newSpectrogramConf->yMaximum = currentPlotCurveConf->yMaximum;

        cloneObj->m_spectrogramSourceConfigs.append(newSpectrogramConf);
    }

    return cloneObj;
}


/**
 * @brief SpectrogramScope::saveConfiguration Saves configuration to XML file
 * @param qSettings
 */
void SpectrogramScope::saveConfiguration(QSettings* qSettings)
{
    //Start writing new XML block
    qSettings->beginGroup(QString("plot3d"));

    int plot3dCurveCount = m_spectrogramSourceConfigs.size();

    qSettings->setValue("plot3dType", SPECTROGRAM);
    qSettings->setValue("dataSourceCount", plot3dCurveCount);

    qSettings->setValue("colorMap", colorMapType);
    qSettings->setValue("samplingFrequency", samplingFrequency);
    qSettings->setValue("timeHorizon", timeHorizon);
    qSettings->setValue("windowWidth", windowWidth);
    qSettings->setValue("zMaximum",  zMaximum);

    for(int i = 0; i < plot3dCurveCount; i++){
        Plot3dCurveConfiguration *plotCurveConf = m_spectrogramSourceConfigs.at(i);

        //Start new XML block
        qSettings->beginGroup(QString("spectrogramDataSource") + QString().number(i));

        qSettings->setValue("uavObject",  plotCurveConf->uavObjectName);
        qSettings->setValue("uavField",  plotCurveConf->uavFieldName);
        qSettings->setValue("colormap",  plotCurveConf->color);

        //Stop writing XML block
        qSettings->endGroup();
    }

    //Stop writing XML block
    qSettings->endGroup();
}


/**
 * @brief SpectrogramScope::replaceSpectrogramDataSource Replaces the list of spectrogram data sources
 * @param spectrogramSourceConfigs
 */
void SpectrogramScope::replaceSpectrogramDataSource(QList<Plot3dCurveConfiguration*> spectrogramSourceConfigs)
{
    m_spectrogramSourceConfigs.clear();
    m_spectrogramSourceConfigs.append(spectrogramSourceConfigs);
}


/**
 * @brief SpectrogramScope::loadConfiguration loads the plot configuration into the scope gadget widget
 * @param scopeGadgetWidget
 */
void SpectrogramScope::loadConfiguration(ScopeGadgetWidget *scopeGadgetWidget)
{
    scopeGadgetWidget->setupSpectrogramPlot(this);
    scopeGadgetWidget->setRefreshInterval(m_refreshInterval);

    //There should be only one spectrogram per plot //TODO: Upgrade this to handle multiple spectrograms on a single axis
    if ( m_spectrogramSourceConfigs.length() != 1)
        return;

    Plot3dCurveConfiguration* spectrogramSourceConfigs = m_spectrogramSourceConfigs.front();
    QString uavObjectName = spectrogramSourceConfigs->uavObjectName;
    QString uavFieldName = spectrogramSourceConfigs->uavFieldName;

    // Get and store the units
    units = getUavObjectFieldUnits(uavObjectName, uavFieldName);

    SpectrogramData* spectrogramData = new SpectrogramData(uavObjectName, uavFieldName, samplingFrequency, windowWidth, timeHorizon);
    spectrogramData->setXMinimum(0);
    spectrogramData->setXMaximum(samplingFrequency/2);
    spectrogramData->setYMinimum(0);
    spectrogramData->setYMaximum(timeHorizon);
    spectrogramData->setZMaximum(zMaximum);
    spectrogramData->setScalePower(spectrogramSourceConfigs->yScalePower);
    spectrogramData->setMeanSamples(spectrogramSourceConfigs->yMeanSamples);
    spectrogramData->setMathFunction(spectrogramSourceConfigs->mathFunction);

    //Generate the waterfall name
    QString waterfallName = (spectrogramData->getUavoName()) + "." + (spectrogramData->getUavoFieldName());
    if(spectrogramData->getHaveSubFieldFlag())
        waterfallName = waterfallName.append("." + spectrogramData->getUavoSubFieldName());

    //Get the uav object
    ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
    UAVObjectManager *objManager = pm->getObject<UAVObjectManager>();
    UAVDataObject* obj = dynamic_cast<UAVDataObject*>(objManager->getObject((spectrogramData->getUavoName())));
    if(!obj) {
        qDebug() << "Object " << spectrogramData->getUavoName() << " is missing";
        return;
    }

    //Get the units
    QString units = getUavObjectFieldUnits(spectrogramData->getUavoName(), spectrogramData->getUavoFieldName());

    //Generate name with scaling factor appeneded
    QString waterfallNameScaled;
    if(spectrogramSourceConfigs->yScalePower == 0)
        waterfallNameScaled = waterfallName + "(" + units + ")";
    else
        waterfallNameScaled = waterfallName + "(x10^" + QString::number(spectrogramSourceConfigs->yScalePower) + " " + units + ")";

    //Create the waterfall plot
    QwtPlotSpectrogram* plotSpectrogram = new QwtPlotSpectrogram(waterfallNameScaled);
    plotSpectrogram->setRenderThreadCount( 0 ); // use system specific thread count
    plotSpectrogram->setRenderHint(QwtPlotItem::RenderAntialiased);
    plotSpectrogram->setColorMap(new ColorMap(colorMapType) );

    // Initial raster data
    spectrogramData->rasterData = new QwtMatrixRasterData();

    QDateTime NOW = QDateTime::currentDateTime(); //TODO: Upgrade this to show UAVO time and not system time
    for ( uint i = 0; i < timeHorizon; i++ ){
        spectrogramData->timeDataHistory->append(NOW.toTime_t() + NOW.time().msec() / 1000.0 + i);
    }

    if (((double) windowWidth) * timeHorizon < (double) 10000000.0 * sizeof(spectrogramData->zDataHistory->front())){ //Don't exceed 10MB for memory
        for ( uint i = 0; i < windowWidth*timeHorizon; i++ ){
            spectrogramData->zDataHistory->append(0);
        }
    }
    else{
        qDebug() << "For some reason, we're trying to allocate a gigantic spectrogram. This probably represents a problem in the configuration file. TimeHorizion: "<< timeHorizon << ", windowWidth: "<< windowWidth;
        Q_ASSERT(0);
        return;
    }

    int numColumns = windowWidth;
    spectrogramData->rasterData->setValueMatrix( *(spectrogramData->zDataHistory), numColumns );

    //Set the ranges for the plot
    spectrogramData->rasterData->setInterval( Qt::XAxis, QwtInterval(spectrogramData->getXMinimum(), spectrogramData->getXMaximum()));
    spectrogramData->rasterData->setInterval( Qt::YAxis, QwtInterval(spectrogramData->getYMinimum(), spectrogramData->getYMaximum()));
    spectrogramData->rasterData->setInterval( Qt::ZAxis, QwtInterval(0, zMaximum));

    //Set up colorbar on right axis
    spectrogramData->rightAxis = scopeGadgetWidget->axisWidget( QwtPlot::yRight );
    spectrogramData->rightAxis->setTitle( "Intensity" );
    spectrogramData->rightAxis->setColorBarEnabled( true );
    spectrogramData->rightAxis->setColorMap( QwtInterval(0, zMaximum), new ColorMap(colorMapType));
    scopeGadgetWidget->setAxisScale( QwtPlot::yRight, 0, zMaximum);
    scopeGadgetWidget->enableAxis( QwtPlot::yRight );

    plotSpectrogram->setData(spectrogramData->rasterData);

    plotSpectrogram->attach(scopeGadgetWidget);
    spectrogramData->spectrogram = plotSpectrogram;

    //Keep the curve details for later
    scopeGadgetWidget->insertDataSources(waterfallNameScaled, spectrogramData);

    //Link to the new signal data only if this UAVObject has not been connected yet
    if (!scopeGadgetWidget->m_connectedUAVObjects.contains(obj->getName())) {
        scopeGadgetWidget->m_connectedUAVObjects.append(obj->getName());
        connect(obj, SIGNAL(objectUpdated(UAVObject*)), scopeGadgetWidget, SLOT(uavObjectReceived(UAVObject*)));
    }

    mutex.lock();
    scopeGadgetWidget->replot();
    mutex.unlock();
}



/**
 * @brief SpectrogramScope::setGuiConfiguration Set the GUI elements based on values from the XML settings file
 * @param options_page
 */
void SpectrogramScope::setGuiConfiguration(Ui::ScopeGadgetOptionsPage *options_page)
{
    //Set the tab widget to 3D
    options_page->tabWidget2d3d->setCurrentWidget(options_page->tabPlot3d);

    //Set the plot type
    options_page->cmb3dPlotType->setCurrentIndex(options_page->cmb3dPlotType->findData(SPECTROGRAM));

    options_page->sbSpectrogramTimeHorizon->setValue(timeHorizon);
    options_page->sbSpectrogramFrequency->setValue(samplingFrequency);
    options_page->spnMaxSpectrogramZ->setValue(zMaximum);
    options_page->cmbColorMapSpectrogram->setCurrentIndex(options_page->cmbColorMapSpectrogram->findData(colorMapType));

    foreach (Plot3dCurveConfiguration* plot3dData,  m_spectrogramSourceConfigs) {
        int uavoIdx= options_page->cmbUAVObjectsSpectrogram->findText(plot3dData->uavObjectName);
        options_page->cmbUAVObjectsSpectrogram->setCurrentIndex(uavoIdx);
        options_page->sbSpectrogramWidth->setValue(windowWidth);

        int uavoFieldIdx= options_page->cmbUavoFieldSpectrogram->findText(plot3dData->uavFieldName);
        options_page->cmbUavoFieldSpectrogram->setCurrentIndex(uavoFieldIdx);
    }

}


/**
 * @brief SpectrogramScope::preparePlot Prepares the Qwt plot colors and axes
 * @param scopeGadgetWidget
 */
void SpectrogramScope::preparePlot(ScopeGadgetWidget *scopeGadgetWidget)
{
    scopeGadgetWidget->setMinimumSize(64, 64);
    scopeGadgetWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    scopeGadgetWidget->setCanvasBackground(QColor(64, 64, 64));

    //Remove grid lines
    scopeGadgetWidget->m_grid->enableX( false );
    scopeGadgetWidget->m_grid->enableY( false );
    scopeGadgetWidget->m_grid->enableXMin( false );
    scopeGadgetWidget->m_grid->enableYMin( false );
    scopeGadgetWidget->m_grid->setMajPen(QPen(Qt::gray, 0, Qt::DashLine));
    scopeGadgetWidget->m_grid->setMinPen(QPen(Qt::lightGray, 0, Qt::DotLine));
    scopeGadgetWidget->m_grid->setPen(QPen(Qt::darkGray, 1, Qt::DotLine));
    scopeGadgetWidget->m_grid->attach(scopeGadgetWidget);

    // Only start the timer if we are already connected
    Core::ConnectionManager *cm = Core::ICore::instance()->connectionManager();
    if (cm->getCurrentConnection() && scopeGadgetWidget->replotTimer)
    {
        if (!scopeGadgetWidget->replotTimer->isActive())
            scopeGadgetWidget->replotTimer->start(m_refreshInterval);
        else
            scopeGadgetWidget->replotTimer->setInterval(m_refreshInterval);
    }
}
