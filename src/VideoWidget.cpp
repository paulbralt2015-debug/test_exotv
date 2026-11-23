#include "VideoWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QtGlobal>
#include <cmath>

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(480, 270);
    setAutoFillBackground(false);
}

void VideoWidget::setFrame(const QImage &frame) {
    if (freeze_ || frame.isNull()) return;
    frame_ = frame;
    rebuildProcessedFrame();
    update();
}

void VideoWidget::setAspectMode(int mode) {
    aspectMode_ = qBound(0, mode, 3);
    update();
}

void VideoWidget::setFreeze(bool enabled) {
    freeze_ = enabled;
}

void VideoWidget::setHideTopLines(int lines) {
    hideTopLines_ = qBound(0, lines, 256);
    update();
}

void VideoWidget::setRowOffset(int pixels) {
    rowOffset_ = qBound(-200, pixels, 200);
    update();
}

void VideoWidget::setBrightness(int value) {
    brightness_ = qBound(-100, value, 100);
    rebuildProcessedFrame();
    update();
}

void VideoWidget::setContrast(int value) {
    contrast_ = qBound(0, value, 200);
    rebuildProcessedFrame();
    update();
}

void VideoWidget::setSaturation(int value) {
    saturation_ = qBound(0, value, 200);
    rebuildProcessedFrame();
    update();
}

void VideoWidget::setInverse(bool enabled) {
    inverse_ = enabled;
    rebuildProcessedFrame();
    update();
}

void VideoWidget::rebuildProcessedFrame() {
    if (frame_.isNull()) {
        processedFrame_ = QImage();
        return;
    }

    processedFrame_ = frame_.convertToFormat(QImage::Format_ARGB32);
    const double contrast = contrast_ / 100.0;
    const double saturation = saturation_ / 100.0;
    for (int y = 0; y < processedFrame_.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(processedFrame_.scanLine(y));
        for (int x = 0; x < processedFrame_.width(); ++x) {
            const QRgb pixel = line[x];
            double r = qRed(pixel);
            double g = qGreen(pixel);
            double b = qBlue(pixel);

            const double luma = 0.299 * r + 0.587 * g + 0.114 * b;
            r = luma + (r - luma) * saturation;
            g = luma + (g - luma) * saturation;
            b = luma + (b - luma) * saturation;

            r = (r - 128.0) * contrast + 128.0 + brightness_;
            g = (g - 128.0) * contrast + 128.0 + brightness_;
            b = (b - 128.0) * contrast + 128.0 + brightness_;

            if (inverse_) {
                r = 255.0 - r;
                g = 255.0 - g;
                b = 255.0 - b;
            }

            line[x] = qRgba(qBound(0, int(std::lround(r)), 255),
                            qBound(0, int(std::lround(g)), 255),
                            qBound(0, int(std::lround(b)), 255),
                            qAlpha(pixel));
        }
    }
}

void VideoWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (processedFrame_.isNull()) {
        painter.setPen(QColor(180, 180, 180));
        painter.drawText(rect(), Qt::AlignCenter,
                         "ExoTV Modern\nOuvrir un fichier vidéo");
        return;
    }

    const int sourceY = qMin(hideTopLines_, processedFrame_.height() - 1);
    const QRect source(0, sourceY, processedFrame_.width(),
                       processedFrame_.height() - sourceY);
    double ratio = source.width() / double(qMax(1, source.height()));
    if (aspectMode_ == 1) ratio = 4.0 / 3.0;
    if (aspectMode_ == 2) ratio = 16.0 / 9.0;

    QRect target = rect();
    if (aspectMode_ != 3) {
        const double boxRatio = width() / double(qMax(1, height()));
        if (boxRatio > ratio) {
            const int w = int(height() * ratio);
            target = QRect((width() - w) / 2, 0, w, height());
        } else {
            const int h = int(width() / ratio);
            target = QRect(0, (height() - h) / 2, width(), h);
        }
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.save();
    painter.setClipRect(rect());
    target.translate(0, rowOffset_);
    painter.drawImage(target, processedFrame_, source);
    painter.restore();
}
