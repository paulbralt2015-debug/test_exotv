#pragma once
#include <QWidget>
#include <QImage>

class VideoWidget final : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    void setFrame(const QImage &frame);
    void setAspectMode(int mode);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QImage frame_;
    int aspectMode_ = 0; // 0=source, 1=4:3, 2=16:9, 3=stretch
};
