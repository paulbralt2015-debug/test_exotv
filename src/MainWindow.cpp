#include "MainWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QAudioOutput>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("ExoTV Modern 1.0");
    resize(1100, 700);
    video_ = new VideoWidget(this);
    position_ = new QSlider(Qt::Horizontal, this);
    position_->setRange(0, 1000);
    status_ = new QLabel("Prêt — lecteur FFmpeg intégré", this);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6,6,6,6);
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
    viewMenu->addAction("Plein écran", this, [this]{ isFullScreen()?showNormal():showFullScreen(); }, QKeySequence(Qt::Key_F11));
    auto *ratio = viewMenu->addMenu("Format d'image");
    for (auto item : {QString("Source"), QString("4:3"), QString("16:9"), QString("Étirer")}) {
        QAction *a = ratio->addAction(item);
        connect(a, &QAction::triggered, this, [this,item]{
            video_->setAspectMode(item=="4:3"?1:item=="16:9"?2:item=="Étirer"?3:0);
        });
    }
    auto *tools = menuBar()->addMenu("&Traitement");
    auto *historic = tools->addMenu("Modules historiques");
    for (const QString &name : {"Nagravision / Syster", "Videocrypt", "PAL Plus", "SECAM", "Color Correlation", "ADL / ReCC", "Block Split", "Hide Black Lines", "Row Offset", "Freeze"}) {
        QAction *a = historic->addAction(name);
        a->setEnabled(false);
        a->setToolTip("Module historique à porter après validation du code ou de la documentation d'origine.");
    }
    auto *capture = tools->addMenu("Capture / Export");
    QAction *bmp = capture->addAction("Capture d'image (BMP)");
    connect(bmp, &QAction::triggered, this, [this]{
        if (video_->grab().save(QFileDialog::getSaveFileName(this,"Capture BMP",{},"Bitmap (*.bmp)"))) status_->setText("Capture enregistrée.");
    });
    auto *audioMenu = menuBar()->addMenu("&Audio");
    auto *volume = new QSlider(Qt::Horizontal);
    volume->setRange(0,100); volume->setValue(100); volume->setFixedWidth(130);
    auto *volAction = new QWidgetAction(this);
    volAction->setDefaultWidget(volume); audioMenu->addAction(volAction);
    connect(volume, &QSlider::valueChanged, this, [this](int v){ Q_UNUSED(v); });

    auto *bar = addToolBar("Lecture");
    bar->addAction(openAct); bar->addAction(playAction_);
    auto *stop = bar->addAction(style()->standardIcon(QStyle::SP_MediaStop), "Arrêter");
    connect(stop, &QAction::triggered, this, &MainWindow::stopPlayback);
    connect(position_, &QSlider::sliderPressed, this, [this]{ sliderMoving_=true; });
    connect(position_, &QSlider::sliderReleased, this, [this]{ sliderMoving_=false; seekTo(position_->value()); });
    connect(&engine_, &VideoEngine::frameReady, video_, &VideoWidget::setFrame);
    connect(&engine_, &VideoEngine::positionChanged, this, &MainWindow::updatePosition);
    connect(&engine_, &VideoEngine::errorOccurred, this, [this](const QString &e){ QMessageBox::critical(this,"Erreur média",e); });
    connect(&engine_, &VideoEngine::playbackFinished, this, [this]{ status_->setText("Lecture terminée."); });
}
void MainWindow::openFile() {
    const QString path = QFileDialog::getOpenFileName(this,"Ouvrir une vidéo",{},
      "Vidéos (*.mp4 *.mkv *.avi *.mov *.mpeg *.mpg *.ts *.m2ts *.wmv *.flv *.webm);;Tous les fichiers (*.*)");
    if (path.isEmpty()) return;
    if (!engine_.open(path)) { QMessageBox::warning(this,"ExoTV","Impossible d'ouvrir ce fichier."); return; }
    status_->setText(path);
    engine_.play();
}
void MainWindow::togglePlayback() { engine_.play(); }
void MainWindow::stopPlayback() { engine_.stop(); status_->setText("Arrêté."); }
void MainWindow::updatePosition(double s) {
    if (!sliderMoving_) position_->setValue(int(s*10));
}
void MainWindow::seekTo(int value) { engine_.seek(value/10.0); }
