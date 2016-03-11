//
// Created by Benjamin Baron on 01/03/16.
//

#include "spatial_stats_layer.h"
#include "constants.h"

bool SpatialStatsLayer::load(Loader* loader) {
    return _spatialStats->computeStats(loader);
}

QGraphicsItemGroup *SpatialStatsLayer::draw() {
    qDebug() << "Draw the spatial stats";
    _groupItem = new QGraphicsItemGroup();
    _groupItem->setHandlesChildEvents(false);

    QHash<Geometry*, QHash<Geometry*, GeometryMatrixValue*>* > matrix;
    _spatialStats->getGeometryMatrix(&matrix);

    for(auto it = matrix.begin(); it != matrix.end(); ++it) {
        Geometry* geom = it.key();
        GeometryGraphics* item;
        if(geom->getGeometryType() == CircleType)
            item = new CircleGraphics(static_cast<Circle*>(geom));
        else if(geom->getGeometryType() == CellType)
            item = new CellGraphics(static_cast<Cell*>(geom));

        GeometryValue* val;
        _spatialStats->getValue(&val, it.key());
        item->setBrush(QBrush(val->color));
        item->setPen(Qt::NoPen);
        item->setOpacity(CELL_OPACITY);
        addGraphicsItem(item);
        _geometryGraphics.insert(it.key(), item);

        // add behavior on mouse press
        connect(item, &GeometryGraphics::mousePressedEvent, this, &SpatialStatsLayer::onMousePress);
    }

    return _groupItem;
}


void SpatialStatsLayer::addMenuBar() {
    _menu = new QMenu();
    _menu->setTitle("Stat analysis");
    _parent->addMenu(_menu);

    // add action to compute the location allocation
    QAction* actionComputeAllocation = _menu->addAction("Compute allocation");
    connect(actionComputeAllocation, &QAction::triggered, this, &SpatialStatsLayer::computeAllocation);

    // add action to the menu to export a contour file
    QAction* actionExportContour = _menu->addAction("Export contour file");
    connect(actionExportContour, &QAction::triggered, this, &SpatialStatsLayer::exportContourFile);

    // add action to the menu to launch the REST server
    QAction* actionRestServer = _menu->addAction("Start REST server");
    connect(actionRestServer, &QAction::triggered, this, &SpatialStatsLayer::startRESTServer);

    // TODO use method addBarMenuItems (see trace_layer.h)
    hideMenu();
}


void SpatialStatsLayer::exportContourFile() {
    if(!_selectedGeometry) {
        QMessageBox q(QMessageBox::Warning, "Error", "No geometry is selected", QMessageBox::Ok);
        q.exec(); // synchronous
        return;
    }

    // get output filename
    QString filename = QFileDialog::getSaveFileName(0,
                                                    tr("Export contour file"),
                                                    QString(),
                                                    tr("CSV file (*.csv)"));
    if(filename.isEmpty())
        return;

    // compute the contour plot
    qDebug() << "Export the contour plot file";
    QFile file(filename);
    if(!file.open(QFile::WriteOnly))
    {
        qDebug() << "Unable to write in file "<< filename;
        return;
    }

    QHash<Geometry*, GeometryValue*> geometries;
    _spatialStats->getGeometries(&geometries);
    Geometry* randomGeom = geometries.keys().first();
    double cellSize = randomGeom->getBounds().getQRectF().width();
    QPointF topLeft = randomGeom->getBounds().getTopLeft();
    QPointF bottomRight = randomGeom->getBounds().getBottomRight();
    for(Geometry* geom : geometries.keys()) {
        QPointF tl = geom->getBounds().getTopLeft();
        QPointF br = geom->getBounds().getBottomRight();
        if(tl.x() < topLeft.x())
            topLeft.setX(tl.x());
        if(tl.y() < topLeft.y())
            topLeft.setY(tl.y());
        if(br.x() > bottomRight.x())
            bottomRight.setX(br.x());
        if(br.y() > bottomRight.y())
            bottomRight.setY(br.y());
    }

    QTextStream out(&file);
    // first line (header)
    out << "x;y;z\n";
//    out << QString::number(_selectedGeometry->getCenter().x(), 'f', 4) << ";"
//        << QString::number(_selectedGeometry->getCenter().y(), 'f', 4) << ";"
//        << QString::number(1e10, 'f', 4) << "\n";

    qDebug() << topLeft << bottomRight << cellSize << (bottomRight.x() - topLeft.x()) / cellSize << (bottomRight.y() - topLeft.y()) / cellSize;
    QHash<Geometry*, GeometryMatrixValue*>* cells;
    _spatialStats->getValues(&cells, _selectedGeometry);
    for(double i = topLeft.x(); i < bottomRight.x(); i += 10) {
        for(double j = topLeft.y(); j < bottomRight.y(); j += 10) {
            QSet<Geometry*> geoms;
            _spatialStats->getGeometriesAt(&geoms, i, j);
            bool foundRightGeom = false;
            if(!geoms.isEmpty()) {
                for(Geometry* geom : geoms) {
                    if(geom->getGeometryType() == CellType) {
                        // add the geometry to the output
                        if(cells->contains(geom)) {
                            GeometryMatrixValue* val = cells->value(geom);
//                            double x = geom->getCenter().x();
//                            double y = geom->getCenter().y();
                            double z = 10 * val->avgScore;
                            out << QString::number(i, 'f', 0) << ";"
                            << QString::number(j, 'f', 0) << ";"
                            << QString::number(z, 'f', 2) << "\n";
                            foundRightGeom = true;
                            break;
                        }
                    }
                }
            }
            if(!foundRightGeom) {
                // add the generic output
                out << QString::number(i, 'f', 0) << ";"
                << QString::number(j, 'f', 0) << ";"
                << QString::number(0, 'f', 2) << "\n";
            }
        }
    }
//    for(Geometry* geom : cells->keys()) {
//        GeometryMatrixValue* val = cells->value(geom);
//        double x = geom->getCenter().x();
//        double y = geom->getCenter().y();
//        double z = 100.0 * val->avgScore;
//
//        out << QString::number(x, 'f', 0) << ";"
//            << QString::number(y, 'f', 0) << ";"
//            << QString::number(z, 'f', 2) << "\n";
//    }

    file.close();

    qDebug() << "[DONE] export contour file";
}

void SpatialStatsLayer::startRESTServer() {
    qDebug() << "Launch REST server";
    if(!_restServer) {

        // create an instance of the compute allocation
        ComputeAllocation* computeAllocation = new ComputeAllocation(_spatialStats);

        // start the server
        _restServer = new RESTServer(10, 0, computeAllocation);
        _restServer->setTimeOut(500);
        _restServer->listen(8080);
        qDebug() << "REST Server listening on port 8080";
    }
}


void SpatialStatsLayer::onMousePress(Geometry* geom, bool mod) {

    if(!_spatialStats->hasValue(geom) || !_spatialStats->hasMatrixValue(geom))
        return;

    qDebug() << "Clicked on geometry" << geom->toString() << mod;
    // select all the reachable geometries
    // restore the parameters for the previously selected geometry
    if(_selectedGeometry && mod) {
        if(!_spatialStats->hasMatrixValue(_selectedGeometry, geom))
            return;

        if(!_plots) {
            // docks the widget on the mainwindow
            _plots = new DockWidgetPlots(_parent, _spatialStats);
        }
        if(_parent)
            _parent->addDockWidget(Qt::RightDockWidgetArea, _plots);
        _plots->show();
        _plots->showLinkData(_selectedGeometry, geom);

    } else {
        if(_selectedGeometry) {
            // restore the "normal" opacity
            _geometryGraphics[_selectedGeometry]->setOpacity(CELL_OPACITY);
            // restore the "normal" colors for the neighbor geometries

            QHash<Geometry*, GeometryMatrixValue*>* geoms;
            _spatialStats->getValues(&geoms, _selectedGeometry);
            for(auto jt = geoms->begin(); jt != geoms->end(); ++jt) {
                Geometry* g = jt.key();
                if(_geometryGraphics.contains(g)) {
                    GeometryValue* val;
                    _spatialStats->getValue(&val,g);
                    _geometryGraphics[g]->setBrush(QBrush(val->color));
                    _geometryGraphics[g]->update();
                }
            }

            if(_selectedGeometry == geom) {
                GeometryValue* val;
                _spatialStats->getValue(&val, _selectedGeometry);
                _geometryGraphics[_selectedGeometry]->setBrush(val->color);
                _selectedGeometry = NULL;
                qDebug() << "clicked on same geometry";
                return;
            }
        }

        QHash<Geometry*, GeometryMatrixValue*>* geoms;
        _spatialStats->getValues(&geoms, geom);
        double maxWeight = 0.0;
        for(auto jt = geoms->begin(); jt != geoms->end(); ++jt) {
            if(jt.key() == geom) continue;
            double score = jt.value()->avgScore;
            if(score > maxWeight) maxWeight = score;
        }
        _geometryGraphics[geom]->setOpacity(1.0);
        _geometryGraphics[geom]->setBrush(CELL_COLOR);
        for(auto jt = geoms->begin(); jt != geoms->end(); ++jt) {
            Geometry* g = jt.key();
            if(g == geom) continue;
            auto val = jt.value();
            double score = val->avgScore;
            int factor = (int) (50.0 + 150.0 * score / maxWeight);
//                    qDebug() << score << maxWeight << factor;

            if(_geometryGraphics.contains(g)) {
                _geometryGraphics[g]->setBrush(CELL_COLOR.darker(factor));
                _geometryGraphics[g]->update();
            }
        }

        if(!_plots) {
            _plots = new DockWidgetPlots(_parent, _spatialStats); // see to dock the widget on the mainwindow
        }
        if(_parent)
            _parent->addDockWidget(Qt::RightDockWidgetArea, _plots);
        _plots->show();
        _plots->showNodeData(geom);

        _selectedGeometry = geom;
    }

}

void SpatialStatsLayer::computeAllocation() {
    if(!_computeAllocationLayer) {
        // Initialize a new compute allocation layer
        ComputeAllocation* computeAllocation = new ComputeAllocation(_spatialStats);
        _computeAllocationLayer = new ComputeAllocationLayer(_parent, "Compute allocation", computeAllocation);
    }
    _computeAllocationLayer->computeAllocation();
}