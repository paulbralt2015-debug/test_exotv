#include "AudioOutput.h"
#include <QMediaDevices>
#include <QAudioDevice>
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
};

AudioOutput::AudioOutput(QObject *parent) : QObject(parent) {}
AudioOutput::~AudioOutput() { stop(); }

bool AudioOutput::start() {
    stop();
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (dev.isNull()) return false;

    // Use the Windows device's native format. A fixed 48 kHz/16-bit format
    // is not supported by every Windows endpoint and previously caused silence.
    format_ = dev.preferredFormat();
    if (format_.sampleRate() <= 0 || format_.channelCount() <= 0 ||
        format_.sampleFormat() == QAudioFormat::Unknown)
        return false;

    sink_ = new QAudioSink(dev, format_, this);
    sink_->setVolume(volume_);
    sink_->setBufferSize(format_.bytesForDuration(250000));
    stopping_ = false;
    device_ = new QueueDevice(this);
    device_->open(QIODevice::ReadOnly);
    sink_->start(device_);
    if (sink_->error() != QAudio::NoError) {
        delete sink_;
        sink_ = nullptr;
        device_->close();
        delete device_;
        device_ = nullptr;
        stopping_ = true;
        return false;
    }
    return true;
}

QAudioFormat AudioOutput::format() const { return format_; }

void AudioOutput::writePcm(const uint8_t *data, int bytes) {
    QMutexLocker lock(&mutex_);
    if (!sink_ || stopping_ || bytes <= 0) return;
    // Keep a bounded queue: approximately 2 seconds at the active device format.
    const int bytesPerSecond = format_.bytesForDuration(1000000);
    const int cap = qMax(bytes, bytesPerSecond * 2);
    if (queue_.size() + bytes > cap)
        queue_.remove(0, qMin(queue_.size(), queue_.size() + bytes - cap));
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

void AudioOutput::setVolume(float v) {
    volume_ = qBound(0.0f, v, 1.0f);
    if (sink_) sink_->setVolume(volume_);
}

qint64 QueueDevice::readData(char *data, qint64 maxSize) {
    return owner_->readQueuedData(data, maxSize);
}

qint64 AudioOutput::readQueuedData(char *data, qint64 maxSize) {
    QMutexLocker lock(&mutex_);
    if (queue_.isEmpty() && !stopping_)
        ready_.wait(&mutex_, 20);
    const qint64 n = qMin(maxSize, qint64(queue_.size()));
    if (n > 0) {
        std::memcpy(data, queue_.constData(), size_t(n));
        queue_.remove(0, int(n));
        return n;
    }
    if (stopping_) return 0;
    // An underrun is silence, not a stalled audio device.
    std::memset(data, 0, size_t(maxSize));
    return maxSize;
}
