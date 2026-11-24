#include "MainWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QVBoxLayout>
#include <QWidget>
#include <QSlider>
#include <QStyle>
#include <QWidgetAction>
#include <QKeySequence>
#include <functional>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("ExoTV Modern 1.1");
    resize(1100, 700);
    video_ = new VideoWidget(this);
    position_ = new QSlider(Qt::Horizontal, this);
    position_->setRange(0, 10000);
    position_->setEnabled(false);
    status_ = new QLabel("Prêt — lecteur FFmpeg intégré", this);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(video_, 1);
    layout->addWidget(position_);
    layout->addWidget(status_);
    setCentralWidget(central);

    auto *fileMenu = menuBar()->addMenu("&Fichier");
    auto *openAct = fileMenu->addAction("Ouvrir un fichier…");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);
    fileMenu->addAction("Quitter", this, &QWidget::close, QKeySequence::Quit);

    auto *playMenu = menuBar()->addMenu("&Lecture");
    playAction_ = playMenu->addAction("Lire / Pause");
    playAction_->setShortcut(Qt::Key_Space);
    connect(playAction_, &QAction::triggered, this, &MainWindow::togglePlayback);
    playMenu->addAction("Arrêter", this, &MainWindow::stopPlayback);

    auto *viewMenu = menuBar()->addMenu("&Affichage");
    viewMenu->addAction("Plein écran", this, [this] {
        isFullScreen() ? showNormal() : showFullScreen();
    }, QKeySequence(Qt::Key_F11));
    auto *ratio = viewMenu->addMenu("Format d'image");
    for (const QString &item : {QString("Source"), QString("4:3"),
                                QString("16:9"), QString("Étirer")}) {
        QAction *action = ratio->addAction(item);
        connect(action, &QAction::triggered, this, [this, item] {
            video_->setAspectMode(item == "4:3" ? 1 : item == "16:9" ? 2 :
                                  item == "Étirer" ? 3 : 0);
        });
    }

    auto *tools = menuBar()->addMenu("&Traitement");
    auto *imageMenu = tools->addMenu("Réglages vidéo");

    auto *freeze = imageMenu->addAction("Geler l'image");
    freeze->setCheckable(true);
    freeze->setToolTip("Fige la dernière image reçue.");
    connect(freeze, &QAction::toggled, video_, &VideoWidget::setFreeze);

    auto *inverse = imageMenu->addAction("Inverser les couleurs");
    inverse->setCheckable(true);
    connect(inverse, &QAction::toggled, video_, &VideoWidget::setInverse);

    auto addSlider = [this](QMenu *parent, const QString &title,
                            int minimum, int maximum, int initial,
                            std::function<void(int)> apply) {
        QMenu *submenu = parent->addMenu(title);
        auto *slider = new QSlider(Qt::Horizontal);
        slider->setRange(minimum, maximum);
        slider->setValue(initial);
        slider->setFixedWidth(190);
        auto *widgetAction = new QWidgetAction(submenu);
        widgetAction->setDefaultWidget(slider);
        submenu->addAction(widgetAction);
        connect(slider, &QSlider::valueChanged, this, apply);
    };

    addSlider(imageMenu, "Lignes supérieures masquées", 0, 32, 0,
              [this](int value) { video_->setHideTopLines(value); });
    addSlider(imageMenu, "Décalage vertical", -50, 50, 0,
              [this](int value) { video_->setRowOffset(value); });
    imageMenu->addSeparator();
    addSlider(imageMenu, "Luminosité", -100, 100, 0,
              [this](int value) { video_->setBrightness(value); });
    addSlider(imageMenu, "Contraste", 0, 200, 100,
              [this](int value) { video_->setContrast(value); });
    addSlider(imageMenu, "Saturation", 0, 200, 100,
              [this](int value) { video_->setSaturation(value); });

    auto *historic = tools->addMenu("Modules de décodage historiques");
    for (const QString &name : {QString("Nagravision / Syster"),
                                QString("Videocrypt"), QString("PAL Plus"),
                                QString("Color Correlation"), QString("ADL / ReCC"),
                                QString("Block Split")}) {
        QAction *action = historic->addAction(name);
        action->setEnabled(false);
        action->setToolTip("Non porté : l'archive ne contient pas le code source d'ExoTV.");
    }

    auto *capture = tools->addMenu("Capture / Export");
    QAction *bmp = capture->addAction("Capture d'image (BMP)");
    connect(bmp, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getSaveFileName(
            this, "Capture BMP", {}, "Bitmap (*.bmp)");
        if (!path.isEmpty() && video_->grab().save(path))
            status_->setText("Capture enregistrée.");
    });

    auto *audioMenu = menuBar()->addMenu("&Audio");
    auto *volume = new QSlider(Qt::Horizontal);
    volume->setRange(0, 100);
    volume->setValue(100);
    volume->setFixedWidth(130);
    auto *volAction = new QWidgetAction(this);
    volAction->setDefaultWidget(volume);
    audioMenu->addAction(volAction);
    connect(volume, &QSlider::valueChanged, this,
            [this](int value) { engine_.setVolume(value / 100.0f); });

    auto *bar = addToolBar("Lecture");
    bar->addAction(openAct);
    bar->addAction(playAction_);
    auto *stop = bar->addAction(style()->standardIcon(QStyle::SP_MediaStop), "Arrêter");
    connect(stop, &QAction::triggered, this, &MainWindow::stopPlayback);

    connect(position_, &QSlider::sliderPressed, this, [this] { sliderMoving_ = true; });
    connect(position_, &QSlider::sliderReleased, this, [this] {
        sliderMoving_ = false;
        seekTo(position_->value());
    });
    connect(&engine_, &VideoEngine::frameReady, video_, &VideoWidget::setFrame);
    connect(&engine_, &VideoEngine::positionChanged, this, &MainWindow::updatePosition);
    connect(&engine_, &VideoEngine::durationChanged, this, [this](double seconds) {
        totalDuration_ = seconds;
        position_->setEnabled(seconds > 0.0);
    });
    connect(&engine_, &VideoEngine::errorOccurred, this, [this](const QString &error) {
        engine_.stop();
        QMessageBox::critical(this, "Erreur média", error);
    });
    connect(&engine_, &VideoEngine::playbackFinished, this, [this] {
        engine_.stop();
        status_->setText("Lecture terminée.");
    });
}

void MainWindow::openFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Ouvrir une vidéo", {},
        "Vidéos (*.mp4 *.mkv *.avi *.mov *.mpeg *.mpg *.ts *.m2ts *.wmv *.flv *.webm);;Tous les fichiers (*.*)");
    if (path.isEmpty()) return;
    if (!engine_.open(path)) {
        QMessageBox::warning(this, "ExoTV", "Impossible d'ouvrir ce fichier.");
        return;
    }
    status_->setText(path);
    engine_.play();
}

void MainWindow::togglePlayback() {
    if (engine_.isRunning() && !engine_.isPaused()) engine_.pause();
    else engine_.play();
}

void MainWindow::stopPlayback() {
    engine_.stop();
    position_->setValue(0);
    status_->setText("Arrêté.");
}

void MainWindow::updatePosition(double seconds) {
    if (!sliderMoving_ && totalDuration_ > 0.0)
        position_->setValue(qBound(0, int((seconds / totalDuration_) * 10000.0), 10000));
}

void MainWindow::seekTo(int value) {
    if (totalDuration_ > 0.0)
        engine_.seek((value / 10000.0) * totalDuration_);
}
