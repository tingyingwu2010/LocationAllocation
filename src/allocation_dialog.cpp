#include "allocation_dialog.h"

#include <QVBoxLayout>
#include <QDebug>
#include <QSettings>

// example http://doc.qt.io/qt-5/qtwidgets-dialogs-extension-example.html

AllocationDialog::AllocationDialog(QWidget *parent)
    : QDialog(parent) {
    methodChoiceLabel = new QLabel("Choose method");
    methodChoiceComboBox = new QComboBox();
    QStringList methodChoiceItems = QStringList() << "" << LOCATION_ALLOCATION_MEHTOD_NAME << PAGE_RANK_MEHTOD_NAME << K_MEANS_MEHTOD_NAME << RANDOM_METHOD_NAME;
    methodChoiceComboBox->addItems(methodChoiceItems);
    // two overloads for QComboBox::currentIndexChanged, need to cast the right method
    // @see https://bugreports.qt.io/browse/QTBUG-30926
    connect(methodChoiceComboBox, (void (QComboBox::*)(const QString&))&QComboBox::currentIndexChanged, [=](const QString& text) {
        method = text;
        if(method.isEmpty()) {
            nbStorageExtension->setVisible(false);
            deadlineExtension->setVisible(false);
            delExtension->setVisible(false);
            showNbStorage = false;
            showDeadline = false;
            showDel = false;
        }
        if(method == LOCATION_ALLOCATION_MEHTOD_NAME || method == PAGE_RANK_MEHTOD_NAME) {
            nbStorageExtension->setVisible(true);
            deadlineExtension->setVisible(true);
            delExtension->setVisible(true);
            showNbStorage = true;
            showDeadline = true;
            showDel = true;
        } else if(method == K_MEANS_MEHTOD_NAME || method == RANDOM_METHOD_NAME) {
            nbStorageExtension->setVisible(true);
            deadlineExtension->setVisible(false);
            delExtension->setVisible(false);
            showNbStorage = true;
            showDeadline = true;
            showDel = false;
        }
        checkConsistency();
    });

    nbStorageRadioButton = new QRadioButton("Set number of storage nodes");
    connect(nbStorageRadioButton, &QRadioButton::toggled, [=](bool checked) {
        nbStorageExtension->setVisible(checked);
        enableSetNbStorageNode = checked;
        if(checked) {
            nbStorageNodes = nbStorageNodesLineEdit->text().toInt();
        } else {
            nbStorageNodes = -1;
        }
        checkConsistency();
    });

    /* Storage extension */
    nbStorageExtension = new QWidget;
    QHBoxLayout *nbStorageLayout = new QHBoxLayout;
    nbStorageNodesLabel = new QLabel("Number of storage nodes:");
    nbStorageNodesLineEdit = new QLineEdit;
    nbStorageNodesLabel->setBuddy(nbStorageNodesLineEdit);

    connect(nbStorageNodesLineEdit, &QLineEdit::textChanged, this, [=](QString text){
        bool ok;
        nbStorageNodes = text.toInt(&ok);
        if(!ok) nbStorageNodes = -1;
        toggleBoldFont(nbStorageNodesLineEdit, ok);
        checkConsistency();
    });
    nbStorageLayout->addWidget(nbStorageNodesLabel);
    nbStorageLayout->addWidget(nbStorageNodesLineEdit);
    nbStorageExtension->setLayout(nbStorageLayout);

    allStorageRadioButton = new QRadioButton("All storage nodes");
    connect(allStorageRadioButton, &QRadioButton::toggled, [=](bool checked) {
        allStorageExtension->setVisible(checked);
        enableSetAllStorageNode = checked;
        if(checked) {
            allStorageNodesPath = allStorageNodesLineEdit->text();
        } else {
            allStorageNodesPath = "";
        }
        checkConsistency();
    });

    allStorageExtension = new QWidget;
    QHBoxLayout *allStorageLayout = new QHBoxLayout;
    allStorageNodesLabel = new QLabel("Compute every storage nodes:");
    allStorageNodesLineEdit = new QLineEdit;
    allStorageNodesLabel->setBuddy(allStorageNodesLineEdit);
    allStorageNodesFileButton = new QPushButton("Browse");

    connect(allStorageNodesFileButton, &QPushButton::clicked, [=] () {
        QFileDialog d(this, "Open an output directory for the allocations");
        d.setFileMode(QFileDialog::Directory);
        QString path = d.getExistingDirectory(this,
                                              "Open an output directory",
                                              allStorageNodesPath.isEmpty()?QFileInfo(allStorageNodesPath).absolutePath():QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));

        if(path.isEmpty())
            return;

        allStorageNodesLineEdit->setText(path);
    });
    connect(allStorageNodesLineEdit, &QLineEdit::textChanged, [=](QString text) {
        // record the file path
        allStorageNodesPath = text;
        checkConsistency();
    });

    allStorageLayout->addWidget(allStorageNodesLabel);
    allStorageLayout->addWidget(allStorageNodesLineEdit);
    allStorageLayout->addWidget(allStorageNodesFileButton);
    allStorageExtension->setLayout(allStorageLayout);

    storageNodeExtension = new QWidget;
    QVBoxLayout* storageNodeLayout = new QVBoxLayout;
    storageNodeLayout->addWidget(nbStorageRadioButton);
    storageNodeLayout->addWidget(nbStorageExtension);
    storageNodeLayout->addWidget(allStorageRadioButton);
    storageNodeLayout->addWidget(allStorageExtension);
    storageNodeExtension->setLayout(storageNodeLayout);

    /* Deadline extension */
    deadlineExtension = new QWidget;
    QHBoxLayout *deadlineLayout = new QHBoxLayout;
    deadlineLabel = new QLabel("Deadline:");
    deadlineLineEdit = new QLineEdit;
    deadlineLabel->setBuddy(deadlineLineEdit);
    connect(deadlineLineEdit, &QLineEdit::textChanged, this, [=](QString text){
        bool ok;
        deadline = text.toInt(&ok);
        if(!ok) deadline = -1;
        toggleBoldFont(deadlineLineEdit, ok);
        checkConsistency();
    });
    deadlineLayout->addWidget(deadlineLabel);
    deadlineLayout->addWidget(deadlineLineEdit);
    deadlineExtension->setLayout(deadlineLayout);

    /* Deletion extension */
    delExtension = new QWidget;
    QVBoxLayout *delLayout = new QVBoxLayout;
    delFactorLabel = new QLabel("Deletion factor:");
    delFactorLineEdit = new QLineEdit;
    delFactorLabel->setBuddy(delFactorLineEdit);
    connect(delFactorLineEdit, &QLineEdit::textChanged, this, [=](QString text){
        bool ok;
        deletionFactor = text.toDouble(&ok);
        if(!ok) deletionFactor = -1.0;
        else ok = deletionFactor >= 0.0;
        toggleBoldFont(delFactorLineEdit, ok);
        bool ret = checkConsistency();
        qDebug() << text << deletionFactor << "ok" << ok << "ret" << ret;
    });

    /* Travel time extension */
    travelTimeExtension = new QWidget;

    travelTimeLineEdit = new QLineEdit();
    connect(travelTimeLineEdit, &QLineEdit::textChanged, [=](QString text) {
        bool ok;
        travelTime = text.toDouble(&ok);
        if(!ok) travelTime = -1.0;
        else ok = travelTime > 0.0;
        toggleBoldFont(travelTimeLineEdit, ok);
        checkConsistency();
    });

    travelTimeMedianRadioButton = new QRadioButton("Median travel time");
    connect(travelTimeMedianRadioButton, &QRadioButton::toggled, [=](bool checked) {
        if(checked) ttStat = MedTTStat;

        checkConsistency();
    });

    travelTimeAverageRadioButton = new QRadioButton("Average travel time");
    connect(travelTimeAverageRadioButton, &QRadioButton::toggled, [=](bool checked) {
        if(checked) ttStat = AvgTTStat;
        checkConsistency();
    });

    QVBoxLayout *travelTimeExtensionLayout = new QVBoxLayout;
    travelTimeExtensionLayout->setMargin(0);
    travelTimeExtensionLayout->addWidget(travelTimeLineEdit);
    travelTimeExtensionLayout->addWidget(travelTimeMedianRadioButton);
    travelTimeExtensionLayout->addWidget(travelTimeAverageRadioButton);
    travelTimeExtension->setLayout(travelTimeExtensionLayout);

    travelTimeCheckBox = new QCheckBox("Travel time (seconds)");
    connect(travelTimeCheckBox, &QCheckBox::toggled, [=] (bool checked) {
        enableTravelTime = checked;
        travelTimeExtension->setVisible(checked);
        checkConsistency();
        if(!checked) ttStat = NoneTTStat;
    });

    /* Distance extension */
    distanceExtension = new QWidget;

    distanceFixedLineEdit = new QLineEdit();
    connect(distanceFixedLineEdit, &QLineEdit::textChanged, [=](QString text) {
        bool ok;
        distance = text.toDouble(&ok);
        if(!ok) distance = -1.0;
        else ok = distance > 0.0;
        toggleBoldFont(distanceFixedLineEdit, ok);
        checkConsistency();
    });
    distanceFixedRadioButton = new QRadioButton("Fixed");
    connect(distanceFixedRadioButton, &QRadioButton::toggled, [=](bool checked) {
        distanceFixedLineEdit->setEnabled(checked);
        fixedDistance = checked;
        if(checked) {
            distanceFixedLineEdit->setFocus();
            double prevDistance = distance < 0 ? distanceFixedLineEdit->text().toDouble() : distance;
            distanceFixedLineEdit->textChanged(QString::number(prevDistance, 'f', 2));
            dStat = FixedDStat;
        }
        checkConsistency();
    });

    QHBoxLayout *distanceFixedLayout = new QHBoxLayout;
    distanceFixedLayout->setMargin(0);
    distanceFixedLayout->addWidget(distanceFixedRadioButton);
    distanceFixedLayout->addWidget(distanceFixedLineEdit);

    distanceAutoRadioButton = new QRadioButton("AutoDStat");
    connect(distanceAutoRadioButton, &QRadioButton::toggled, [=](bool checked) {
        if(checked) distance = -1.0;
        if(checked) dStat = AutoDStat;
        checkConsistency();
    });

    QVBoxLayout *distanceExtensionLayout = new QVBoxLayout;
    distanceExtensionLayout->setMargin(0);
    distanceExtensionLayout->setSpacing(0);
    distanceExtensionLayout->addLayout(distanceFixedLayout);
    distanceExtensionLayout->addWidget(distanceAutoRadioButton);
    distanceExtension->setLayout(distanceExtensionLayout);

    distanceCheckBox = new QCheckBox("Distance (meters)");
    connect(distanceCheckBox, &QCheckBox::toggled, [=](bool checked) {
        enableDistance = checked;
        distanceExtension->setVisible(checked);
        if(!checked) dStat = NoneDStat;
        checkConsistency();
    });
    delLayout->addWidget(delFactorLabel);
    delLayout->addWidget(delFactorLineEdit);
    delLayout->addWidget(delFactorLineEdit);
    delLayout->addWidget(travelTimeCheckBox);
    delLayout->addWidget(travelTimeExtension);
    delLayout->addWidget(distanceCheckBox);
    delLayout->addWidget(distanceExtension);
    delExtension->setLayout(delLayout);

    buttonBox = new QDialogButtonBox(Qt::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &AllocationDialog::done);
    connect(buttonBox, &QDialogButtonBox::rejected, [=]() {
        QDialog::reject();
    });

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);

    mainLayout->addWidget(methodChoiceLabel);
    mainLayout->addWidget(methodChoiceComboBox);
    mainLayout->addWidget(storageNodeExtension);
    mainLayout->addWidget(deadlineExtension);
    mainLayout->addWidget(delExtension);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    setWindowTitle(tr("Choose parameters"));

    /* Restore previous saved values */
    QSettings settings;
    // restore the chosen method
    if(settings.contains("savedAllocationMethod"))
        methodChoiceComboBox->setCurrentText(settings.value("savedAllocationMethod").toString());
    else {
        qDebug() << "setting the current method" << LOCATION_ALLOCATION_MEHTOD_NAME;
        methodChoiceComboBox->setCurrentText(LOCATION_ALLOCATION_MEHTOD_NAME);
    }

    if(showNbStorage && settings.contains("savedNbStorageNodes") && (settings.value("savedNbStorageNodes").toInt() != -1)) {
        nbStorageNodesLineEdit->setText(QString::number(settings.value("savedNbStorageNodes").toInt()));
        enableSetNbStorageNode = true;
        enableSetNbStorageNode = false;
        nbStorageRadioButton->setChecked(true);
    } else if(showNbStorage && settings.contains("savedAllStorageNodesPath") && !settings.value("savedAllStorageNodesPath").toString().isEmpty()) {
        allStorageNodesLineEdit->setText(settings.value("savedAllStorageNodesPath").toString());
        enableSetAllStorageNode = true;
        enableSetNbStorageNode = false;
        allStorageRadioButton->setChecked(true);
    }

    if(showDeadline && settings.contains("savedDeadline"))
        deadlineLineEdit->setText(QString::number(settings.value("savedDeadline").toInt()));

    qDebug() << "ShowDel" << showDel << settings.value("savedDeletionFactor") << settings.value("savedEnableDistance") << settings.contains("savedAllocationMethod");

    if(showDel) {
        if(settings.contains("savedDeletionFactor"))
            delFactorLineEdit->setText(QString::number(settings.value("savedDeletionFactor").toDouble(), 'f', 2));

        if(settings.contains("savedEnableTravelTime")) {
            bool enabled = settings.value("savedEnableTravelTime").toBool();
            travelTimeCheckBox->setChecked(enabled);
            travelTimeCheckBox->toggled(enabled);
            travelTimeExtension->setVisible(enabled);
            enableTravelTime = enabled;
            if(!enabled) ttStat = NoneTTStat;
        } else {
            travelTimeCheckBox->setChecked(false);
            travelTimeCheckBox->toggled(false);
            travelTimeExtension->setVisible(false);
            enableTravelTime = false;
            ttStat = NoneTTStat;
        }

        if(settings.contains("savedEnableDistance")) {
            bool enabled = settings.value("savedEnableDistance").toBool();
            distanceCheckBox->setChecked(enabled);
            distanceCheckBox->toggled(enabled);
            distanceExtension->setVisible(enabled);
            enableDistance = enabled;
            if(!enabled) dStat = NoneDStat;
        } else {
            distanceCheckBox->setChecked(false);
            distanceCheckBox->toggled(false);
            distanceExtension->setVisible(false);
            enableDistance = false;
            dStat = NoneDStat;
        }

        if(settings.contains("savedTravelTime")) {
            travelTime = settings.value("savedTravelTime").toDouble();
            if(travelTime > 0.0) {
                travelTimeLineEdit->setText(QString::number(travelTime));
                qDebug() << travelTime << settings.value("savedTravelTimeStat");
                if(settings.contains("savedTravelTimeStat")) {
                    ttStat = static_cast<TravelTimeStat>(settings.value("savedTravelTimeStat").toInt());
                    if(ttStat == AvgTTStat) travelTimeAverageRadioButton->setChecked(true);
                    if(ttStat == MedTTStat) travelTimeMedianRadioButton->setChecked(true);
                    else {
                        travelTimeAverageRadioButton->setChecked(true);
                        ttStat = AvgTTStat;
                    }
                } else {
                    travelTimeAverageRadioButton->setChecked(true);
                    ttStat = AvgTTStat;
                }
            } else {
                travelTimeAverageRadioButton->setChecked(true);
                ttStat = AvgTTStat;
            }
        } else {
            travelTimeAverageRadioButton->setChecked(true);
            ttStat = AvgTTStat;
        }

        if(settings.contains("savedDistance")) {
            distance = settings.value("savedDistance").toDouble();
            qDebug() <<"savedDistance" << distance << settings.value("savedDistance") << settings.value("savedDistanceStat");
            if(settings.contains("savedDistanceStat")) {
                dStat = static_cast<DistanceStat>(settings.value("savedDistanceStat").toInt());
                if(dStat == FixedDStat) {
                    if(distance > 0.0) {
                        distanceFixedRadioButton->setChecked(true);
                        distanceFixedLineEdit->setText(QString::number(distance));
                    }
                } else if(dStat == AutoDStat) {
                    distanceAutoRadioButton->setChecked(true);
                } else {
                    distanceAutoRadioButton->setChecked(true);
                    dStat = AutoDStat;
                }
            } else {
                distanceAutoRadioButton->setChecked(true);
                dStat = AutoDStat;
            }
        } else {
            distanceAutoRadioButton->setChecked(true);
            dStat = AutoDStat;
        }
    }
    checkConsistency();
}

void AllocationDialog::done() {
    /* Save all of the attributes */
    QSettings settings;
    settings.setValue("savedNbStorageNodes", nbStorageNodes);
    settings.setValue("savedAllStorageNodesPath", allStorageNodesPath);
    settings.setValue("savedDeadline", deadline);
    settings.setValue("savedDeletionFactor", deletionFactor);
    settings.setValue("savedEnableTravelTime", enableTravelTime);
    settings.setValue("savedEnableDistance", enableDistance);
    settings.setValue("savedTravelTime", travelTime);
    settings.setValue("savedDistance", distance);
    settings.setValue("savedTravelTimeStat", ttStat);
    settings.setValue("savedDistanceStat", dStat);

    QDialog::accept();
}

bool AllocationDialog::checkConsistency() {
    bool flag = false;
    if(method.isEmpty())
        flag = true;
    if((showNbStorage && enableSetNbStorageNode && nbStorageNodes <= 0)
       || (showNbStorage && enableSetAllStorageNode && allStorageNodesPath.isEmpty())
       || (showDeadline && deadline <= 0)
       || (showDel && deletionFactor < 0.0)) {
        flag = true;
        qDebug() << "deadline" << deadline << "nbStorageNodes" << nbStorageNodes << enableSetNbStorageNode << "deletionFactor" << deletionFactor << "allStorageNodesPath" << allStorageNodesPath << enableSetAllStorageNode;
    }
    if(showDel && enableTravelTime && travelTime <= 0.0) {
        flag = true;
        qDebug() << "enableTravelTime" << enableTravelTime << "fixedTravelTime" << fixedTravelTime << "travelTime" << travelTime;
    }
    if(showDel && enableTravelTime && (ttStat != MedTTStat && ttStat != AvgTTStat)) {
        flag = true;
    }
    if(showDel && enableDistance && dStat == FixedDStat && distance <= 0.0) {
        flag = true;
        qDebug() << "enableDistance" << enableDistance << "fixedDistance" << fixedDistance << "distance" << distance;

    }

    // Disable the "OK" button depending on the final value of flag
    buttonBox->button(QDialogButtonBox::Ok)->setDisabled(flag);
    return flag;
}
