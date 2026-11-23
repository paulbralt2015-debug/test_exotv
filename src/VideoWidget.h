#pragma once
#include <QWidget>
#include <QImage>

class VideoWidget final : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    void setFrame(const QImage &frame);
    void setAspectMode(int mode);
    void setFreeze(bool enabled);
    void setHideTopLines(int lines);
    void setRowOffset(int pixels);
    void setBrightness(int value);
    void setContrast(int value);
    void setSaturation(int value);
    void setInverse(bool enabled);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    void rebuildProcessedFrame();
    QImage frame_;
    QImage processedFrame_;
    int aspectMode_ = 0; // 0=source, 1=4:3, 2=16:9, 3=stretch
    int hideTopLines_ = 0;
    int rowOffset_ = 0;
    int brightness_ = 0; // -100..100
    int contrast_ = 100; // 0..200, 100=neutral
    int saturation_ = 100; // 0..200, 100=neutral
    bool freeze_ = false;
    bool inverse_ = false;
};
