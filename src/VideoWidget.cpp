#include "VideoWidget.h"
#include <QPainter>
#include <QPaintEvent>

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(480, 270);
    setAutoFillBackground(false);
}
void VideoWidget::setFrame(const QImage &frame) { frame_ = frame; update(); }
void VideoWidget::setAspectMode(int mode) { aspectMode_ = mode; update(); }
void VideoWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (frame_.isNull()) {
        p.setPen(QColor(180,180,180));
        p.drawText(rect(), Qt::AlignCenter, "ExoTV Modern\nOuvrir un fichier vidéo");
        return;
    }
    QRect target = rect();
    if (aspectMode_ != 3) {
        double ratio = frame_.width() / double(frame_.height());
        if (aspectMode_ == 1) ratio = 4.0/3.0;
        if (aspectMode_ == 2) ratio = 16.0/9.0;
        const double boxRatio = width() / double(qMax(1, height()));
        if (boxRatio > ratio) {
            int w = int(height() * ratio);
            target = QRect((width()-w)/2, 0, w, height());
        } else {
            int h = int(width() / ratio);
            target = QRect(0, (height()-h)/2, width(), h);
        }
    }
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(target, frame_);
}
