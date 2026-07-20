#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QPushButton>
#include <QHBoxLayout>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QIODevice>
#include <QBuffer>

// ── 粒子结构（置于类外，避免 MSVC 嵌套类型模板解析问题） ──
struct VisualizerParticle {
    QPointF pos;
    QPointF vel;
    float life = 0.0f;
    float maxLife = 1.0f;
    QColor color;
    float size = 4.0f;
};

/// AudioVisualizer - 实时音频可视化组件（Qt6）
///
/// 通过 QAudioDecoder 解码音频文件获取 PCM 数据，
/// 结合 QMediaPlayer 播放位置同步显示。
/// 在同一画布上分区域展示三种效果：
///   - 频谱柱状图（冷→暖渐变，顶部光晕）
///   - 圆形波形图（呼吸式缩放）
///   - 节拍粒子系统（拖尾 + 重力）
class AudioVisualizer : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int displayMode READ displayMode WRITE setDisplayMode)

public:
    /// 显示模式（位标志，可组合）
    enum DisplayFlag {
        Spectrum    = 0x01,
        Waveform    = 0x02,
        ParticleMode = 0x04,
    };
    Q_DECLARE_FLAGS(DisplayFlags, DisplayFlag)

    explicit AudioVisualizer(QMediaPlayer *player, QWidget *parent = nullptr);
    ~AudioVisualizer() override;

    void setDisplayMode(int flags);
    int displayMode() const { return m_displayFlags; }

    /// 设置音频源文件路径（Qt6 需要此路径启动解码器）
    void setSourceFile(const QString &filePath);

signals:
    void displayModeChanged(int flags);

protected:
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onModeBtnClicked(int flag);
    void onDecoderBufferReady();
    void onDecoderFinished();

private:
    // ── 音频探测初始化 ──
    void initAudioProbe();

    // ── 音频处理 ──
    void processPcmData(const QByteArray &data, int sampleRate, int channels);
    void computeFft(QVector<double> &samples);
    void detectBeat(const QVector<double> &fftMag);

    // ── 绘制方法 ──
    void drawSpectrum(QPainter &p, const QRect &rect);
    void drawWaveform(QPainter &p, const QRect &rect);
    void drawParticles(QPainter &p, const QRect &rect);

    // ── 粒子系统 ──
    void emitParticles(int count, const QColor &baseColor);
    void updateParticles(float dt);

    // ── 控件 ──
    void setupControlBar();

    QMediaPlayer *m_mediaPlayer = nullptr;

    // ── 音频数据源 ──
    QAudioDecoder *m_decoder        = nullptr;
    QByteArray     m_decodedBuffer;           // 完整解码 PCM 数据
    int            m_decodedBytes   = 0;      // 已解码字节数
    int            m_decChannels    = 2;      // 声道数
    int            m_decSampleRate  = 44100;  // 采样率
    bool           m_decoderFinished = false; // 解码完成
    QString        m_sourcePath;

    // ── 音频数据 ──
    void feedFrameAtPosition();

    QVector<double> m_fftData;          // FFT 幅值（频谱用）
    QVector<double> m_waveformData;     // 波形采样（波形图用）
    int    m_sampleRate   = 44100;
    double m_volumeLevel  = 0.0;        // 当前音量（0~1）
    double m_smoothVolume = 0.0;        // 平滑音量（呼吸用）

    // ── 节拍检测 ──
    double m_beatStrength = 0.0;
    int    m_beatDecay    = 0;
    // ── 粒子系统 ──
    QVector<VisualizerParticle> m_particles;
    static constexpr int kMaxParticles = 300;

    // ── 显示模式 ──
    int m_displayFlags = Spectrum | Waveform | ParticleMode;

    // ── 动画 ──
    int  m_animTimerId = 0;

    // ── 控制按钮 ──
    QWidget      *m_btnBar     = nullptr;
    QPushButton  *m_btnSpec    = nullptr;
    QPushButton  *m_btnWave    = nullptr;
    QPushButton  *m_btnPart    = nullptr;
    QPushButton  *m_btnClose   = nullptr;

    // ── 频谱 MilkDrop 风格控制 ──
    static constexpr int kBarCount = 48;
    double m_barLevel[kBarCount]   = {};   // 当前平滑值
    double m_barAttack[kBarCount]  = {};   // 快速上升跟踪
    double m_barPeak[kBarCount]    = {};   // 峰值保持
    int    m_barPeakAge[kBarCount] = {};   // 峰值保持时长

    // ── FFT 辅助 ──
    static QVector<double> s_hannWindow;
    static QVector<double> buildHannWindow(int size);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AudioVisualizer::DisplayFlags)

#endif // VISUALIZER_H
