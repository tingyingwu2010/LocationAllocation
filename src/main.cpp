#include "mainwindow.h"
#include "proj_factory.h"
#include "gtfs_layer.h"
#include "geometry_index.h"
#include "spatial_stats.h"
#include <QApplication>
#include <qcommandlineparser.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    QCommandLineParser parser;
    parser.setApplicationDescription("LocAll");
    parser.addHelpOption();
    parser.addVersionOption();

    // A boolean option for running it via GUI (--gui)
    QCommandLineOption noguiOption(QStringList() << "nogui", "Running it via GUI.");
    parser.addOption(noguiOption);
    QCommandLineOption gtfsOption(QStringList() << "gtfs", "Load a GTFS directory.", "dir", QString());
    parser.addOption(gtfsOption);
    QCommandLineOption traceOption(QStringList() << "trace", "Load a trace file.", "file", QString());
    parser.addOption(traceOption);
    QCommandLineOption traceDirOption(QStringList() << "trace-dir", "Load a trace directory.", "dir", QString());
    parser.addOption(traceDirOption);
    QCommandLineOption pointOption(QStringList() << "points", "Use a points file.", "file", QString());
    parser.addOption(pointOption);
    QCommandLineOption serverOption(QStringList() << "server", "Run the location allocation API server.");
    parser.addOption(serverOption);
    QCommandLineOption projInOption(QStringList() << "proj-in", "Choose the input projection.", "projection", QString());
    parser.addOption(projInOption);
    QCommandLineOption projOutOption(QStringList() << "proj-out", "Choose the output projection.", "projection", QString());
    parser.addOption(projOutOption);
    QCommandLineOption samplingOption(QStringList() << "sampling", "Sampling for the spatial stats.", "value", "-1");
    parser.addOption(samplingOption);
    QCommandLineOption startTimeOption(QStringList() << "starttime", "startTime for the spatial stats.", "value", "-1");
    parser.addOption(startTimeOption);
    QCommandLineOption endTimeOption(QStringList() << "endtime", "endTime for the spatial stats.", "value", "-1");
    parser.addOption(endTimeOption);
    QCommandLineOption cellSizeOption(QStringList() << "cell-size", "cell size for the spatial stats.", "value", "-1");
    parser.addOption(cellSizeOption);

    qDebug() << "parser options" << parser.optionNames();

    // Process the actual command line arguments given by the user
    parser.process(a);
    if (parser.isSet(noguiOption)) {
        if(parser.isSet(projInOption) && parser.isSet(projOutOption)) {
            QString projIn = parser.value(projInOption);
            QString projOut = parser.value(projOutOption);
            ProjFactory::getInstance().setProj(projIn, projOut);
        }
        double sampling = -1;
        double startTime = -1;
        double endTime = -1;
        double cellSize = -1;
        if(parser.isSet(samplingOption))
            sampling = parser.value(samplingOption).toDouble();
        if(parser.isSet(startTimeOption))
            startTime = parser.value(startTimeOption).toDouble();
        if(parser.isSet(endTimeOption))
            endTime = parser.value(endTimeOption).toDouble();
        if(parser.isSet(cellSizeOption))
            cellSize = parser.value(cellSizeOption).toDouble();

        GeometryType  pointsType = NoneType;
        QString pointsFile = QString();
        if(parser.isSet(pointOption)) {
            pointsType = CircleType;
            pointsFile = parser.value(pointOption);
        }

        TraceLayer* layer;

        if(parser.isSet(gtfsOption)) {
            QString gtfsPath = parser.value(gtfsOption);
            qDebug() << "load GTFS directory" << gtfsPath << "...";

            layer = new GTFSLayer(0, "gtfs layer", gtfsPath, true);
        } else if(parser.isSet(traceOption)) {
            QString tracePath = parser.value(traceOption);
        } else if(parser.isSet(traceDirOption)) {
            QString traceDir = parser.value(traceDirOption);
        }
        layer->load(nullptr);

        qDebug() << "Compute geometry index";
        GeometryIndex* geometryIndex = GeometryIndex::make_geometryIndex(*layer, sampling, startTime, endTime,
                                                                         cellSize, pointsType, pointsFile);

        qDebug() << "compute spatial stats";
        SpatialStats spatialStats(0, "spatial stats layer", layer,
                                  (int) sampling, (long long) startTime, (long long) endTime,
                                  geometryIndex);
        spatialStats.load(nullptr);

        ComputeAllocation computeAllocation(&spatialStats);
        RESTServer server(8, nullptr, &computeAllocation);
        server.setTimeOut(500);
        server.listen(8080);
        qDebug() << "Launching REST server";

    } else {
        w.show();
    }

    return a.exec();
}
