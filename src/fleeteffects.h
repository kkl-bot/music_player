#ifndef FLEETEFFECTS_H
#define FLEETEFFECTS_H

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPoint>
#include <QColor>
#include <QVector>
#include <QPixmap>

// ── 背景飘落粒子 ──
struct FallParticle {
    QPointF pos;
    float speedY;
    float driftX;
    float rotation;
    float opacity;
    int imageIndex;
};

// ── 点击爆散粒子 ──
struct BurstParticle {
    QPointF pos;
    QPointF vel;
    float life;
    float rot;
    int imageIndex;
};

/// FleetEffects —— 主题 "Fleet-Snowfluff" 特效管理器
///
/// 功能：
///   1. 鼠标点击 → 彩色粒子爆散（Burst）
///   2. 背景飘落粒子（Falling）
///   3. 运动桌宠（Chibi）
///
/// 全部封装在此类中，mainwindow 仅调用接口。
class FleetEffects : public QObject
{
    Q_OBJECT
    friend class OverlayWidget;

public:
    explicit FleetEffects(QWidget *parentWidget);
    ~FleetEffects() override;

    /// 启动特效（背景飘落 + 桌宠）
    void start();
    /// 停止特效并清理
    void stop();

    /// 由 mainwindow 在鼠标按下时调用
    void onMousePress(const QPoint &globalPos);

    /// 切换启用/停用
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // ── 初始化 ──
    void initOverlay();
    void initParticleImages();
    void initPet();
    void startTimers();

    // ── 背景飘落 ──
    void spawnFallParticle();
    void updateFallParticles();

    // ── 点击爆散 ──
    void emitBurst(const QPoint &pos);
    void updateBurstParticles();

    // ── 桌宠 ──
    void updatePet();
    void setPetGif(const QString &path);
    void pickPetTarget();
    void applyPetDirection();

    QWidget *m_parent    = nullptr;
    bool     m_enabled   = true;

    // ── 透明覆盖层（用于绘制粒子） ──
    QWidget *m_overlay   = nullptr;

    // ── 粒子图片 ──
    QVector<QPixmap> m_particleImages;

    // ── 背景飘落 ──
    QVector<FallParticle> m_fallParticles;
    QTimer *m_fallTimer   = nullptr;
    int     m_maxFallParticles = 30;

    // ── 点击爆散 ──
    QVector<BurstParticle> m_burstParticles;
    int m_maxBurstNodes = 20;

    // ── 动画帧定时器（60fps） ──
    QTimer *m_animTimer   = nullptr;

    // ── 桌宠 ──
    QLabel  *m_petLabel   = nullptr;
    QMovie  *m_petMovie   = nullptr;
    QPointF  m_petPos;
    QPointF  m_petTarget;
    QPointF  m_petVel;
    float    m_petSpeed    = 0.0f;
    int      m_petWait     = 0;       // 停留计数
    QString  m_petCurGif;             // 当前 GIF 路径
    // GIF 映射
    QString  m_petGifFly;
    QString  m_petGifGlass;
    QString  m_petGifJump;
    QString  m_petGifUp;
};

#endif // FLEETEFFECTS_H
