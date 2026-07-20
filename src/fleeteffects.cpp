#include "fleeteffects.h"

#include <QPainter>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QRandomGenerator>
#include <QFileInfo>
#include <QMovie>
#include <cmath>
#include <algorithm>

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

FleetEffects::FleetEffects(QWidget *parentWidget)
    : QObject(parentWidget)
    , m_parent(parentWidget)
{
    initParticleImages();
    initOverlay();
    initPet();
    startTimers();
}

FleetEffects::~FleetEffects()
{
    stop();
}

// ════════════════════════════════════════════════════════════
//  启动 / 停止
// ════════════════════════════════════════════════════════════

void FleetEffects::start()
{
    m_enabled = true;
    if (m_overlay)   m_overlay->show();
    if (m_petLabel) { m_petLabel->raise(); m_petLabel->show(); }
    if (m_fallTimer) m_fallTimer->start();
    if (m_animTimer) m_animTimer->start();
}

void FleetEffects::stop()
{
    m_enabled = false;
    if (m_fallTimer) m_fallTimer->stop();
    if (m_animTimer) m_animTimer->stop();
    if (m_overlay)   m_overlay->hide();
    if (m_petMovie)  m_petMovie->stop();
    if (m_petLabel)  m_petLabel->hide();
    m_fallParticles.clear();
    m_burstParticles.clear();
}

void FleetEffects::setEnabled(bool on)
{
    on ? start() : stop();
}

// ════════════════════════════════════════════════════════════
//  初始化
// ════════════════════════════════════════════════════════════

void FleetEffects::initParticleImages()
{
    static const char *kFileNames[] = {
        "assets/icon0.png", "assets/icon1.png",
        "assets/icon2.png", "assets/icon3.png",
    };
    static const QColor kFallbackColors[] = {
        QColor(255,183,197), QColor(160,196,255),
        QColor(200,150,255), QColor(255,220,150),
    };
    for (int i = 0; i < 4; ++i) {
        QPixmap pm(QString::fromLatin1(kFileNames[i]));
        if (pm.isNull()) {
            // 文件不存在时回退到内建像素方块
            pm = QPixmap(16, 16);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(kFallbackColors[i]);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(2, 2, 12, 12, 3, 3);
            p.end();
        }
        m_particleImages.append(pm);
    }
}



void FleetEffects::initPet()
{
    m_petLabel = new QLabel(m_parent);
    m_petLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_petLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    m_petLabel->resize(80, 80);
    m_petLabel->setAlignment(Qt::AlignCenter);
    m_petLabel->hide();

    // 查找可用 GIF 素材
    auto findGif = [](const QString &name) -> QString {
        const QStringList dirs = {
            QStringLiteral("assets/Aemeath/"),
            QStringLiteral("../assets/Aemeath/"),
            QStringLiteral("../../assets/Aemeath/"),
        };
        for (const auto &d : dirs) {
            QString path = d + name;
            if (QFileInfo::exists(path))
                return path;
        }
        return {};
    };
    m_petGifFly   = findGif(QStringLiteral("Aemeath_FLY.gif"));
    m_petGifGlass = findGif(QStringLiteral("Aemeath_GLASS.gif"));
    m_petGifJump  = findGif(QStringLiteral("Aemeath_JUMP.gif"));
    m_petGifUp    = findGif(QStringLiteral("Aemeath_UP.gif"));

    // 无素材用 emoji 占位
    if (m_petGifFly.isEmpty()) {
        m_petLabel->setText(QStringLiteral("🌸"));
        m_petLabel->setStyleSheet(QStringLiteral("background:transparent;font-size:48px;"));
        return;
    }
    // 初始显示 idle
    setPetGif(m_petGifGlass);
}

void FleetEffects::startTimers()
{
    // 背景飘落生成定时器（每 1.2s 生成一个）
    m_fallTimer = new QTimer(this);
    m_fallTimer->setInterval(1200);
    connect(m_fallTimer, &QTimer::timeout, this, [this]() {
        spawnFallParticle();
    });

    // 动画帧定时器（60fps）— 同时驱动粒子 + 桌宠
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(16);
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        updateFallParticles();
        updateBurstParticles();
        updatePet();
        m_overlay->update();
    });
}

// ════════════════════════════════════════════════════════════
//  事件过滤 — 捕获父窗口点击 → 粒子爆散
// ════════════════════════════════════════════════════════════

bool FleetEffects::eventFilter(QObject *obj, QEvent *event)
{
    if (m_enabled && obj == m_parent && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);
        // 忽略对子控件（按钮等）的点击 → 只在空白区域触发
        QWidget *child = m_parent->childAt(me->pos());
        if (!child || child == m_overlay || child == m_petLabel)
            emitBurst(me->pos());
    }
    return QObject::eventFilter(obj, event);
}

// ════════════════════════════════════════════════════════════
//  鼠标点击 — 粒子爆散
// ════════════════════════════════════════════════════════════

void FleetEffects::onMousePress(const QPoint &localPos)
{
    if (!m_enabled) return;
    emitBurst(localPos);
}

void FleetEffects::emitBurst(const QPoint &pos)
{
    if (!m_enabled) return;

    int count = 6;
    for (int i = 0; i < count && m_burstParticles.size() < m_maxBurstNodes; ++i) {
        BurstParticle bp;
        bp.pos = QPointF(pos);
        double angle = QRandomGenerator::global()->bounded(2.0 * M_PI);
        double speed = 3.0 + QRandomGenerator::global()->bounded(6.0);
        bp.vel = QPointF(std::cos(angle) * speed, std::sin(angle) * speed);
        bp.life = 1.0f;
        bp.rot = QRandomGenerator::global()->bounded(360.0f);
        bp.imageIndex = QRandomGenerator::global()->bounded(m_particleImages.size());
        m_burstParticles.append(bp);
    }
}

void FleetEffects::updateBurstParticles()
{
    for (int i = m_burstParticles.size() - 1; i >= 0; --i) {
        auto &p = m_burstParticles[i];
        p.pos += p.vel;
        p.vel *= 0.92;
        p.vel += QPointF(0, 0.2);    // 重力
        p.rot += 5;
        p.life -= 0.03f;
        if (p.life <= 0)
            m_burstParticles.remove(i);
    }
}

// ════════════════════════════════════════════════════════════
//  背景飘落粒子
// ════════════════════════════════════════════════════════════

void FleetEffects::spawnFallParticle()
{
    if (!m_enabled || m_fallParticles.size() >= m_maxFallParticles)
        return;

    int w = m_parent->width();
    FallParticle fp;
    fp.pos = QPointF(QRandomGenerator::global()->bounded(w), -30);
    fp.speedY = 0.5 + QRandomGenerator::global()->bounded(1.5);
    fp.driftX = (QRandomGenerator::global()->bounded(200) - 100) / 100.0f * 0.3f;
    fp.rotation = QRandomGenerator::global()->bounded(360.0f);
    fp.opacity = 0.2f + QRandomGenerator::global()->bounded(50) / 100.0f;
    fp.imageIndex = QRandomGenerator::global()->bounded(m_particleImages.size());
    m_fallParticles.append(fp);
}

void FleetEffects::updateFallParticles()
{
    int h = m_parent->height();
    for (int i = m_fallParticles.size() - 1; i >= 0; --i) {
        auto &p = m_fallParticles[i];
        p.pos += QPointF(p.driftX, p.speedY);
        p.rotation += 0.5f;
        if (p.pos.y() > h + 30)
            m_fallParticles.remove(i);
    }
}

// ════════════════════════════════════════════════════════════
//  桌宠
// ════════════════════════════════════════════════════════════

void FleetEffects::setPetGif(const QString &path)
{
    if (path.isEmpty() || path == m_petCurGif) return;
    m_petCurGif = path;

    // 停止旧动画
    if (m_petMovie) {
        m_petMovie->stop();
        m_petLabel->clear();
    }

    auto *movie = new QMovie(path);
    movie->setScaledSize(QSize(80, 80));
    movie->setCacheMode(QMovie::CacheAll);
    if (!movie->isValid()) {
        delete movie;
        // 回退到静态图片
        QPixmap pm(path);
        if (!pm.isNull())
            m_petLabel->setPixmap(pm.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return;
    }

    m_petMovie = movie;
    m_petLabel->setMovie(m_petMovie);
    m_petMovie->start();
}

void FleetEffects::pickPetTarget()
{
    int pw = m_parent->width();
    int ph = m_parent->height();
    int pad = 60;
    int x = pad + QRandomGenerator::global()->bounded(pw - pad * 2);
    int y = pad + QRandomGenerator::global()->bounded(ph - pad * 2);
    // 避开底部控制栏
    int ctrlY = ph - 90;
    if (y > ctrlY) y = ctrlY - 30;

    m_petTarget = QPointF(x, y);
    // 移动到目标的速度 (像素/秒) 随机
    m_petSpeed = 20.0f + QRandomGenerator::global()->bounded(40);
}

void FleetEffects::applyPetDirection()
{
    // 根据水平方向翻转图片（CSS transform: scaleX）
    if (m_petVel.x() > 0.5f)
        m_petLabel->setStyleSheet(QStringLiteral(
            "background: transparent; transform: scaleX(1);"));
    else if (m_petVel.x() < -0.5f)
        m_petLabel->setStyleSheet(QStringLiteral(
            "background: transparent; transform: scaleX(-1);"));
}

void FleetEffects::updatePet()
{
    if (!m_enabled || !m_petLabel) return;

    if (m_petGifFly.isEmpty()) {
        // 无素材时 emoji 原地呼吸
        return;
    }

    // 首次需要显示
    if (!m_petLabel->isVisible()) {
        m_petLabel->raise();
        m_petLabel->show();
        int pw = m_parent->width();
        int ph = m_parent->height();
        m_petPos = QPointF(QRandomGenerator::global()->bounded(pw),
                           QRandomGenerator::global()->bounded(ph));
        m_petLabel->move(m_petPos.toPoint());
        pickPetTarget();
    }

    // ── 停留 / 移动 ──
    QPointF diff = m_petTarget - m_petPos;
    float dist = std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());

    if (m_petWait > 0) {
        // 停留倒计时
        m_petWait--;
        setPetGif(m_petGifGlass);       // idle 动画
        m_petVel = QPointF(0, 0);
        if (m_petWait == 0)
            pickPetTarget();
        return;
    }

    if (dist < 5.0f) {
        // 到达目标 → 停留 1~3 秒
        m_petWait = 60 + QRandomGenerator::global()->bounded(120);  // 60fps 帧数
        setPetGif(m_petGifGlass);
        m_petVel = QPointF(0, 0);
        return;
    }

    // ── 朝目标移动 ──
    float step = m_petSpeed / 60.0f;      // 每帧移动距离
    QPointF dir = diff / dist;
    m_petPos += dir * step;
    m_petVel = dir * m_petSpeed;
    m_petLabel->move(qRound(m_petPos.x()), qRound(m_petPos.y()));

    // ── 根据运动方向切换 GIF ──
    bool movingUp = dir.y() < -0.3f;
    bool movingHoriz = std::abs(dir.x()) > 0.4f;

    if (movingUp && m_petGifUp.isEmpty())
        setPetGif(m_petGifJump);    // 无 UP 时用 JUMP 代替上升
    else if (movingUp)
        setPetGif(m_petGifUp);
    else if (movingHoriz)
        setPetGif(m_petGifFly);
    else
        setPetGif(m_petGifJump);    // 斜下/垂直下用跳跃

    applyPetDirection();
}

// ════════════════════════════════════════════════════════════
//  绘制（由 overlay 的 paintEvent 触发）
// ════════════════════════════════════════════════════════════

// 注：m_overlay 的绘制在构造函数中通过 eventFilter + update() 触发
// 但 overlay 是普通 QWidget，我们需要子类化或重写 paintEvent。
// 这里采用安装事件过滤器到 m_overlay 的 paintEvent 方案。
// 实际使用中，我们会让 mainwindow 刷新时调用 overlay->update()。
// 替代方案：在 timer 中直接画到 overlay。

// 为了让粒子可绘制，我们在 animTimer 中让 overlay 刷新。
// overlay 需要自定义 paintEvent。更简单：将 overlay 替换为一个
// 自定义 QWidget 子类，但为了封装干净，我们用事件过滤器处理 paint。

// 由于 QWidget 的 paint 不能通过 eventFilter 拦截，
// 我们改为在 m_overlay 上安装事件过滤器，并处理 Paint 事件。

// 但在 Qt 中，QEvent::Paint 不能通过 eventFilter 拦截并直接绘制。
// 正确做法：子类化 OverlayWidget。

// 简化方案：使用 lambda 连接 animTimer 直接绘制到 overlay 上。
// 但 QWidget 绘制必须在 paintEvent 中。
// 最终方案：在构造函数中重写 m_overlay 的 paintEvent 通过
// 事件过滤器处理 QEvent::Paint。

// 实际上 Qt 不允许 eventFilter 拦截 Paint 事件。
// 我们改为：在 animTimer 中调用 m_overlay->update()，
// 并让 m_overlay 使用事件过滤器… 但 paint 不行。

// 最干净的方案：创建一个覆盖窗口类，内嵌在 fleeteffects.cpp 中。

// ════════════════════════════════════════════════════════════
//  OverlayWidget — 透明覆盖层，仅绘制粒子，鼠标穿透
// ════════════════════════════════════════════════════════════

class OverlayWidget : public QWidget
{
public:
    FleetEffects *m_effects = nullptr;
    explicit OverlayWidget(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);  // 鼠标穿透
        setAttribute(Qt::WA_AlwaysStackOnTop);
        setStyleSheet(QStringLiteral("background: transparent;"));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        if (!m_effects) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        for (const auto &fp : m_effects->m_fallParticles) {
            if (fp.imageIndex < 0 || fp.imageIndex >= m_effects->m_particleImages.size())
                continue;
            QPixmap pm = m_effects->m_particleImages[fp.imageIndex];
            p.save();
            p.translate(fp.pos);
            p.rotate(fp.rotation);
            p.setOpacity(fp.opacity);
            p.drawPixmap(-pm.width() / 2, -pm.height() / 2, pm);
            p.restore();
        }
        for (const auto &bp : m_effects->m_burstParticles) {
            if (bp.imageIndex < 0 || bp.imageIndex >= m_effects->m_particleImages.size())
                continue;
            QPixmap pm = m_effects->m_particleImages[bp.imageIndex];
            p.save();
            p.translate(bp.pos);
            p.rotate(bp.rot);
            p.setOpacity(std::max(0.0, (double)bp.life));
            float s = 0.5f + (1.0f - bp.life) * 0.5f;
            p.scale(s, s);
            p.drawPixmap(-pm.width() / 2, -pm.height() / 2, pm);
            p.restore();
        }
    }
};

void FleetEffects::initOverlay()
{
    auto *overlay = new OverlayWidget(m_parent);
    overlay->m_effects = this;
    overlay->setGeometry(m_parent->rect());
    overlay->raise();
    overlay->hide();
    m_overlay = overlay;

    // 在父窗口上安装事件过滤器，捕获全局点击用于爆散效果
    m_parent->removeEventFilter(this);
    m_parent->installEventFilter(this);
}
