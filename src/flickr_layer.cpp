#include "flickr_layer.h"

#include "proj_factory.h"
#include "intermediate_pos_layer.h"
#include "spatial_stats_dialog.h"
#include "spatial_stats_layer.h"

QGraphicsItemGroup* FlickrLayer::draw() {
    // show the bus lines
    qDebug() << _trace->getNbNodes() << "nodes";
    int radius = 1000; // 1 km radius
    _groupItem = new QGraphicsItemGroup();

    QHash<QString, QMap<long long, QPointF>*> nodes;
    _trace->getNodes(&nodes);

    for(auto it = nodes.begin(); it != nodes.end(); ++it) {
        for(auto jt = it.value()->begin(); jt != it.value()->end(); ++jt) {
            double x = jt.value().x();
            double y = jt.value().y();

            QGraphicsEllipseItem* item = new QGraphicsEllipseItem(x-radius, -1*(y-radius), radius*2, -1*(radius*2));
//            item->setCacheMode(QGraphicsItem::DeviceCoordinateCache);

            item->setBrush(QBrush(QColor(255,0,0)));
            item->setPen(Qt::NoPen);

            addGraphicsItem(item);
        }
    }

    return _groupItem;
}

bool FlickrTrace::openTrace(Loader* loader) {
    QFile* file = new QFile(_filename);
    if(!file->open(QFile::ReadOnly | QFile::Text))
        return false;

    while(!file->atEnd()) {
        QString line = QString(file->readLine()).split(QRegExp("[\r\n]"), QString::SkipEmptyParts).at(0);
        QStringList fields = line.split(" ");
        QString node = fields.at(0);
        long long ts = (long long) fields.at(1).toDouble();
        double lon   = fields.at(2).toDouble();
        double lat   = fields.at(3).toDouble();
//        qDebug() << "(" << node << "," << ts << "," << lat << "," << lon << ")";
        if(ts <= 0)
            continue;
        // convert the points to the local projection
        double x, y;
        ProjFactory::getInstance().transformCoordinates(lat, lon, &x, &y);
//        qDebug() << "adding node" << node << "(" << x << "," << y << "," << ts << ")";
        addPoint(node, ts, x, y);
        loader->loadProgressChanged(1.0 - file->bytesAvailable() / (qreal) file->size(), "");
    }

    qDebug() << "[DONE] loading file";
    loader->loadProgressChanged((qreal) 1.0, "Done");

    return true;
}


void FlickrLayer::addBarMenuItems() {
    _menu = new QMenu("trace");
    QAction* actionShowIntermediatePoints = _menu->addAction("Show intermediate points");
    QAction* actionSpatialStats = _menu->addAction("Spatial statistics");

    _parent->addMenu(_menu);

    connect(actionShowIntermediatePoints, &QAction::triggered, [=](bool checked){
        qDebug() << "Show intermediate positions of" << getName();
        QString name = "Intermediate positions";
        IntermediatePosLayer* layer   = new IntermediatePosLayer(_parent, name, _trace);
        Loader loader;
        _parent->createLayer(name, layer, &loader);
    });


    connect(actionSpatialStats, &QAction::triggered, [=](bool checked){
        qDebug() << "Compute spatial Stats";
        if(!_spatialStatsLayer) {
            SpatialStatsDialog spatialStatsDialog(_parent, _trace);
            int ret = spatialStatsDialog.exec(); // synchronous
            if (ret == QDialog::Rejected) {
                return;
            }
            double sampling  = spatialStatsDialog.getSampling();
            double startTime = spatialStatsDialog.getStartTime();
            double endTime   = spatialStatsDialog.getEndTime();
            GeometryIndex* geometryIndex = GeometryIndex::make_geometryIndex(_trace, sampling, startTime, endTime,
                                                                             spatialStatsDialog.getCellSize(),
                                                                             spatialStatsDialog.getGeometryType(),
                                                                             spatialStatsDialog.getCircleFile());

            SpatialStats* spatialStats = new SpatialStats(_trace, (long long) sampling, (long long) startTime, (long long) endTime, geometryIndex);
            _spatialStatsLayer = new SpatialStatsLayer(_parent, "Spatial Stats layer", spatialStats);

        }

        // load the spatial stats layer and run it
        QString layerName = "Spatial Stats";
        Loader* loader = new Loader;
        _parent->createLayer(layerName, _spatialStatsLayer, loader);
    });
}