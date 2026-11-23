#pragma once
#include <QObject>
#include <QImage>
#include <QString>
#include <atomic>
#include <thread>
class AudioOutput;
class VideoEngine final : public QObject {
    Q_OBJECT
public:
    explicit VideoEngine(QObject *parent = nullptr);
    ~VideoEngine() override;
    bool open(const QString &path);
    void play();
    void pause();
    void stop();
    void seek(double seconds);
    double duration() const;
    bool isPaused() const { return paused_.load(); }
    bool isRunning() const { return running_.load(); }
    void setVolume(float volume);
signals:
    void frameReady(const QImage &frame);
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void playbackFinished();
    void errorOccurred(const QString &message);
private:
    void decodeLoop();
    QString path_;
    std::thread worker_;
    std::atomic_bool running_{false};
    std::atomic_bool paused_{false};
    std::atomic<double> seekTarget_{-1.0};
    double duration_ = 0;
    AudioOutput *audio_ = nullptr;
};
