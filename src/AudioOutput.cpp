#include "AudioOutput.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QThread>
#include <cstring>

class QueueDevice final : public QIODevice {
public:
    explicit QueueDevice(AudioOutput *owner) : owner_(owner) {}
    bool isSequential() const override { return true; }
protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }
private:
    AudioOutput *owner_;
    friend class AudioOutput;
};

AudioOutput::AudioOutput(QObject *parent) : QObject(parent) {}
AudioOutput::~AudioOutput() { stop(); }

bool AudioOutput::start(int rate, int channels) {
    stop();
    QAudioFormat fmt;
    fmt.setSampleRate(rate);
    fmt.setChannelCount(channels);
    fmt.setSampleFormat(QAudioFormat::Int16);
    const auto dev = QMediaDevices::defaultAudioOutput();
    if (!dev.isFormatSupported(fmt)) return false;
    sink_ = new QAudioSink(dev, fmt, this);
    sink_->setVolume(1.0);
    stopping_ = false;
    device_ = new QueueDevice(this);
    device_->open(QIODevice::ReadOnly);
    sink_->start(device_);
    return true;
}
void AudioOutput::writePcm(const uint8_t *data, int bytes) {
    QMutexLocker lock(&mutex_);
    if (!sink_ || stopping_ || bytes <= 0) return;
    // Bound queued audio to roughly 1.5 seconds to avoid unbounded memory use.
    const int cap = 48000 * 2 * 2 * 3;
    if (queue_.size() > cap) queue_.remove(0, queue_.size() - cap);
    queue_.append(reinterpret_cast<const char*>(data), bytes);
    ready_.wakeAll();
}
void AudioOutput::stop() {
    {
        QMutexLocker lock(&mutex_);
        stopping_ = true;
        queue_.clear();
        ready_.wakeAll();
    }
    if (sink_) { sink_->stop(); delete sink_; sink_ = nullptr; }
    if (device_) { device_->close(); delete device_; device_ = nullptr; }
}
void AudioOutput::setVolume(float v) { if (sink_) sink_->setVolume(qBound(0.0f, v, 1.0f)); }

qint64 QueueDevice::readData(char *data, qint64 maxSize) {
    QMutexLocker lock(&owner_->mutex_);
    if (owner_->queue_.isEmpty() && !owner_->stopping_)
        owner_->ready_.wait(&owner_->mutex_, 20);
    const qint64 n = qMin(maxSize, qint64(owner_->queue_.size()));
    if (n > 0) {
        std::memcpy(data, owner_->queue_.constData(), size_t(n));
        owner_->queue_.remove(0, int(n));
    } else if (owner_->stopping_) {
        return 0;
    } else {
        std::memset(data, 0, size_t(maxSize));
        return maxSize;
    }
    return n;
}
