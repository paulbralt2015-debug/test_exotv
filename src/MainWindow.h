#pragma once
#include <QMainWindow>
#include <QSlider>
#include <QLabel>
#include <QAction>
#include "VideoEngine.h"
#include "VideoWidget.h"
class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
private slots:
    void openFile();
    void togglePlayback();
    void stopPlayback();
    void updatePosition(double seconds);
    void seekTo(int value);
private:
    VideoEngine engine_;
    VideoWidget *video_ = nullptr;
    QSlider *position_ = nullptr;
    QLabel *status_ = nullptr;
    QAction *playAction_ = nullptr;
    bool sliderMoving_ = false;
    double totalDuration_ = 0.0;
};
