#include "SnakeGame.h"
#include <QRandomGenerator>
#include <QFont>
#include <QFontMetrics>
#include <QShortcut>
#include <QDebug>
#include <QtMath>
#include <QDateTime>
#include <QLineF>
#include <QStringList>
#include <QSettings>

// ================================================================
//  关卡配置获取
// ================================================================
static LevelConfig getLevelConfig(int level)
{
    LevelConfig c;
    int round = (level - 1) / 10;
    int idx = (level - 1) % 10;

    if (round == 0) {
        static const int t[10] = { 100, 150, 250, 400, 600, 850, 1150, 1500, 1900, 2350 };
        c.targetFood = t[idx] / 10; c.foodEnabled = true;
    }
    else if (round == 1) {
        c.targetFood = (120 + idx * 100) / 10;
        c.foodEnabled = true;
        c.barrierCount = 100 + idx * 150 / 9;
    }
    else if (round == 2) {
        if (idx < 5) {
            static const int st[5] = { 55, 85, 110, 145, 180 };
            c.targetStar = st[idx]; c.starEnabled = true; c.starsOnScreen = (idx >= 2) ? 2 : 1; c.foodEnabled = false;
        }
        else {
            static const int st[5] = { 120, 165, 210, 260, 290 };
            static const int di[5] = { 22, 35, 48, 58, 68 };
            c.targetStar = st[idx - 5]; c.targetDiamond = di[idx - 5];
            c.starEnabled = true; c.diamondEnabled = true; c.starsOnScreen = 2; c.diamondsOnScreen = 1; c.foodEnabled = false;
        }
    }
    else if (round == 3) {
        static const int t[10] = { 250, 350, 500, 700, 950, 1250, 1600, 2000, 2450, 2950 };
        c.targetFood = t[idx] / 10; c.foodEnabled = true;
        c.bombMax = (idx < 4) ? 1 : 2; c.bombMoving = false; c.bombLifeMs = 5000;
        c.bombSpawnMinMs = 10000; c.bombSpawnMaxMs = 15000;
    }
    else if (round == 4) {
        c.targetStar = 200 + idx * 80; c.targetDiamond = 30 + idx * 15;
        c.starEnabled = true; c.diamondEnabled = true; c.starsOnScreen = 2; c.diamondsOnScreen = 1; c.foodEnabled = false;
        if (idx < 4) { c.bombMax = 1; c.bombSpawnMinMs = 15000; c.bombSpawnMaxMs = 15000; }
        else { c.bombMax = 2 + (idx % 2); c.bombSpawnMinMs = 25000; c.bombSpawnMaxMs = 25000; }
        c.bombMoving = true; c.bombLifeMs = -1;
    }
    else {
        c.barrierCount = 100 + idx * 80 / 9;
        if (idx < 3) {
            static const int st[3] = { 210, 300, 390 };
            c.targetStar = st[idx]; c.starEnabled = true; c.starsOnScreen = 2;
        }
        else if (idx < 7) {
            static const int st[4] = { 400, 500, 600, 700 }; static const int di[4] = { 45, 65, 85, 105 };
            c.targetStar = st[idx - 3]; c.targetDiamond = di[idx - 3]; c.starEnabled = true; c.diamondEnabled = true; c.starsOnScreen = 2; c.diamondsOnScreen = 1;
        }
        else {
            static const int st[3] = { 750, 900, 1050 }; static const int di[3] = { 120, 140, 160 };
            c.targetStar = st[idx - 7]; c.targetDiamond = di[idx - 7]; c.starEnabled = true; c.diamondEnabled = true; c.starsOnScreen = 2; c.diamondsOnScreen = 1;
            c.bombMax = 2; c.bombMoving = false; c.bombLifeMs = 5000; c.bombSpawnMinMs = 10000; c.bombSpawnMaxMs = 15000;
        }
        c.foodEnabled = false;
    }
    return c;
}

QString SnakeGame::levelName(int lv)
{
    int round = (lv - 1) / 10 + 1;
    int idx = (lv - 1) % 10 + 1;
    return QString("%1-%2").arg(round).arg(idx);
}

// ================================================================
//  构造 / 析构
// ================================================================
SnakeGame::SnakeGame(QWidget* parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    // 加载资源图片
    m_head.load(":/img/head.png"); m_body.load(":/img/body.png"); m_tail.load(":/img/tail.png");
    m_food.load(":/img/food.png"); m_bg.load(":/img/background.png"); m_barrier.load(":/img/barrier.png");
    m_star.load(":/img/star.png"); m_diamond.load(":/img/diamond.png"); m_bomb.load(":/img/boom.png");
    m_unknown.load(":/img/unknown.png");
    m_head2.load(":/img/opposite_head.png"); m_body2.load(":/img/opposite_body.png");
    m_tail2.load(":/img/opposite_tail.png"); m_food2.load(":/img/opposite_food.png");

    // 初始化背景音乐
    m_bgm = new QMediaPlayer(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioOutput* audioOutput = new QAudioOutput(this);
    m_bgm->setAudioOutput(audioOutput);
    m_bgm->setSource(QUrl("qrc:/audio/background_music.mp3"));
    audioOutput->setVolume(0.4); // ★ Qt6 在 QAudioOutput 设置音量 (0.0 ~ 1.0)
#else
    m_bgm->setMedia(QUrl("qrc:/audio/background_music.mp3"));
    m_bgm->setVolume(40);        // ★ Qt5 在 QMediaPlayer 设置音量 (0 ~ 100)
#endif
    m_bgm->setLoops(QMediaPlayer::Infinite);
    m_bgm->play();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SnakeGame::gameMove);

    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, this, &SnakeGame::updateAnimations);
    m_animTimer->start(30);
    m_lastAnimMs = QDateTime::currentMSecsSinceEpoch();

    m_bombTimer = new QTimer(this);
    m_bombTimer->setSingleShot(true);
    connect(m_bombTimer, &QTimer::timeout, this, &SnakeGame::trySpawnBombByTimer);

    m_blinkTimer = new QTimer(this);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() { m_blinkOn = !m_blinkOn; update(); });
    m_blinkTimer->start(500);

    m_effectTimer = new QTimer(this);
    m_effectTimer->setSingleShot(true);
    connect(m_effectTimer, &QTimer::timeout, this, &SnakeGame::resolveSurpriseEffect);

    m_battleTimer = new QTimer(this);
    connect(m_battleTimer, &QTimer::timeout, this, &SnakeGame::battleTick);

    QShortcut* pauseSC = new QShortcut(QKeySequence(Qt::Key_Space), this);
    pauseSC->setContext(Qt::ApplicationShortcut);
    pauseSC->setAutoRepeat(false);
    connect(pauseSC, &QShortcut::activated, this, [this]() {
        if (m_state == LevelLost) { restartLevel(); return; }
        togglePause();
        });

    resetGame();
    m_timer->start(m_customInterval);
}

SnakeGame::~SnakeGame() {}

void SnakeGame::playSfx(const QString& filename)
{
    if (!m_sfxEnabled) return;
    QMediaPlayer* player = new QMediaPlayer(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioOutput* audio = new QAudioOutput(this);
    player->setAudioOutput(audio);
    player->setSource(QUrl("qrc:/audio/" + filename));
#else
    player->setMedia(QUrl("qrc:/audio/" + filename));
#endif
    player->play();

    connect(player, &QMediaPlayer::mediaStatusChanged, player, [player](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia) {
            player->deleteLater();
        }
        });
}

// ================================================================
//  模式与初始化
// ================================================================
void SnakeGame::setMode(GameMode mode)
{
    m_mode = mode;
    resetGame();
    if (m_mode != BattleMode) m_timer->start(m_customInterval);
}

void SnakeGame::setLevel(int lv)
{
    if (lv < 1) lv = 1;
    if (lv > MAX_LEVEL) lv = 1;
    m_currentLevel = lv;
    loadLevel();
}

void SnakeGame::setSpeed(int interval)
{
    m_customInterval = interval;
    if (m_mode != BattleMode && m_state == Playing) {
        m_timer->start(m_customInterval);
    }
}
void SnakeGame::setMusicEnabled(bool enabled)
{
    m_musicEnabled = enabled;
    if (m_bgm) {
        if (enabled) {
            m_bgm->play();
        }
        else {
            m_bgm->pause();
        }
    }
}

void SnakeGame::setSfxEnabled(bool enabled)
{
    m_sfxEnabled = enabled;
}

void SnakeGame::resetGame()
{
    m_paused = false; m_gameOver = false; m_state = Playing; m_effectType = 0;
    m_bombs.clear(); m_nextBombMs = 0;
    if (m_bombTimer->isActive()) m_bombTimer->stop();
    if (m_effectTimer->isActive()) m_effectTimer->stop();
    if (m_battleTimer->isActive()) m_battleTimer->stop();

    if (m_mode == LevelMode) {
        loadLevel();
    }
    else if (m_mode == SurpriseMode) {
        loadSurpriseLevel();
    }
    else if (m_mode == BattleMode) {
        loadBattleMode();
    }
    else {
        m_config = LevelConfig(); m_config.foodEnabled = true; m_config.targetFood = 999999;
        m_barriers.clear(); m_surpriseObstacles.clear(); m_stars.clear(); m_diamonds.clear();
        m_foodEaten = m_starEaten = m_diamondEaten = 0; m_score = 0;
        m_snake.clear(); m_dir = RIGHT;
        m_snake.append(QPoint(5, 5)); m_snake.append(QPoint(4, 5)); m_snake.append(QPoint(3, 5));
        spawnFood(); update();
    }
}

void SnakeGame::loadBattleMode()
{
    m_barriers.clear(); m_surpriseObstacles.clear(); m_stars.clear(); m_diamonds.clear(); m_bombs.clear();

    m_snake.clear(); m_dir = RIGHT;
    m_snake.append(QPoint(5, 7)); m_snake.append(QPoint(4, 7)); m_snake.append(QPoint(3, 7));

    m_snake2.clear(); m_dir2 = LEFT;
    m_snake2.append(QPoint(22, 7)); m_snake2.append(QPoint(23, 7)); m_snake2.append(QPoint(24, 7));

    m_battleScore1 = 0; m_battleScore2 = 0;
    m_battleTimeLeft = 120; m_battleReadyTime = 3;
    m_battleRunning = false; m_battleEnded = false;
    m_battleResult = "";

    // ★ 关键修复
    m_state = Playing;
    m_paused = false;
    m_effectType = 0;
    m_gameOver = false;  // ★ 补上这句

    spawnFood();
    spawnFood2();

    m_timer->stop();
    m_battleTimer->start(1000);
    update();
}

void SnakeGame::battleTick()
{
    if (m_state != Playing) return;

    if (m_battleReadyTime > 0) {
        m_battleReadyTime--;
        if (m_battleReadyTime == 0) {
            m_battleRunning = true;
            m_timer->start(m_customInterval);
        }
        update();
        return;
    }

    if (m_battleRunning && m_battleTimeLeft > 0) {
        m_battleTimeLeft--;
        if (m_battleTimeLeft <= 0) endBattle();
        update();
    }
}

void SnakeGame::endBattle()
{
    m_battleRunning = false;
    m_battleEnded = true;
    m_timer->stop();
    m_battleTimer->stop();
    m_state = LevelWon;

    if (m_battleScore1 > m_battleScore2) { m_battleResult = "Player 1 Wins!"; playSfx("win.mp3"); }
    else if (m_battleScore2 > m_battleScore1) { m_battleResult = "Player 2 Wins!"; playSfx("win.mp3"); }
    else m_battleResult = "Draw!";

    update();
}

void SnakeGame::loadLevel()
{
    m_config = getLevelConfig(m_currentLevel);
    m_foodEaten = m_starEaten = m_diamondEaten = 0;
    m_barriers.clear(); m_surpriseObstacles.clear(); m_stars.clear(); m_diamonds.clear(); m_bombs.clear();

    // ★ 关键修复：重置游戏状态
    m_state = Playing;
    m_paused = false;
    m_effectType = 0;
    m_gameOver = false;  // ★ 补上这句
    if (m_effectTimer->isActive()) m_effectTimer->stop();

    if (m_config.barrierCount > 0) generateBarriers();
    spawnSnakeAtEdge();
    if (m_config.foodEnabled) spawnFood();
    ensureStarsOnScreen();
    ensureDiamondsOnScreen();

    if (m_config.bombMax > 0) {
        int first = QRandomGenerator::global()->bounded(m_config.bombSpawnMinMs, m_config.bombSpawnMaxMs + 1);
        m_bombTimer->start(first);
    }

    // ★ 确保定时器开启（如果是关卡模式）
    if (m_mode != BattleMode) {
        m_timer->start(m_customInterval);
    }
    update();
}

void SnakeGame::loadSurpriseLevel()
{
    m_config = LevelConfig();
    m_foodEaten = 0; m_starEaten = 0; m_diamondEaten = 0; m_score = 0; m_lives = 3;
    m_barriers.clear(); m_stars.clear(); m_diamonds.clear(); m_bombs.clear(); m_surpriseObstacles.clear();

    // ★ 关键修复
    m_state = Playing;
    m_paused = false;
    m_effectType = 0;
    m_gameOver = false;  // ★ 补上这句
    if (m_effectTimer->isActive()) m_effectTimer->stop();

    spawnSnakeRandom();
    m_config.foodEnabled = true;
    spawnFood();

    if (m_bombTimer->isActive()) m_bombTimer->stop();
    m_timer->start(m_customInterval);
    update();
}

void SnakeGame::spawnSnakeAtEdge()
{
    m_snake.clear();
    auto isEmpty = [&](int x, int y) { return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H && !isBarrier(x, y); };

    for (int attempt = 0; attempt < 200; ++attempt) {
        int side = QRandomGenerator::global()->bounded(4);
        int x = 0, y = 0; Dir d = RIGHT; QPoint bodyOffsets[2];
        if (side == 0) { x = QRandomGenerator::global()->bounded(3, MAP_W - 3); y = 2;             d = DOWN;  bodyOffsets[0] = QPoint(0, -1); bodyOffsets[1] = QPoint(0, -2); }
        else if (side == 1) { x = MAP_W - 3;  y = QRandomGenerator::global()->bounded(3, MAP_H - 3); d = LEFT;  bodyOffsets[0] = QPoint(1, 0);  bodyOffsets[1] = QPoint(2, 0); }
        else if (side == 2) { x = QRandomGenerator::global()->bounded(3, MAP_W - 3); y = MAP_H - 3;    d = UP;    bodyOffsets[0] = QPoint(0, 1);  bodyOffsets[1] = QPoint(0, 2); }
        else { x = 2;          y = QRandomGenerator::global()->bounded(3, MAP_H - 3); d = RIGHT; bodyOffsets[0] = QPoint(-1, 0); bodyOffsets[1] = QPoint(-2, 0); }

        if (!isEmpty(x, y) || !isEmpty(x + bodyOffsets[0].x(), y + bodyOffsets[0].y()) || !isEmpty(x + bodyOffsets[1].x(), y + bodyOffsets[1].y())) continue;

        m_dir = d;
        m_snake.append(QPoint(x, y));
        m_snake.append(QPoint(x + bodyOffsets[0].x(), y + bodyOffsets[0].y()));
        m_snake.append(QPoint(x + bodyOffsets[1].x(), y + bodyOffsets[1].y()));
        return;
    }
    m_dir = RIGHT; m_snake.append(QPoint(5, 5)); m_snake.append(QPoint(4, 5)); m_snake.append(QPoint(3, 5));
}

void SnakeGame::spawnSnakeRandom()
{
    m_snake.clear();
    int x = 0, y = 0; int guard = 0;
    do {
        x = QRandomGenerator::global()->bounded(3, MAP_W - 3);
        y = QRandomGenerator::global()->bounded(3, MAP_H - 3);
        guard++;
    } while (guard < 100 && (m_surpriseObstacles.contains(y * MAP_W + x)));

    m_dir = static_cast<Dir>(QRandomGenerator::global()->bounded(1, 5));
    m_snake.append(QPoint(x, y));
    if (m_dir == UP) { m_snake.append(QPoint(x, y + 1)); m_snake.append(QPoint(x, y + 2)); }
    else if (m_dir == DOWN) { m_snake.append(QPoint(x, y - 1)); m_snake.append(QPoint(x, y - 2)); }
    else if (m_dir == LEFT) { m_snake.append(QPoint(x + 1, y)); m_snake.append(QPoint(x + 2, y)); }
    else { m_snake.append(QPoint(x - 1, y)); m_snake.append(QPoint(x - 2, y)); }
}

void SnakeGame::generateBarriers()
{
    m_barriers.clear();
    int total = m_config.barrierCount; int placed = 0; int guard = 0;
    auto key = [&](int x, int y) { return y * MAP_W + x; };
    while (placed < total && guard < 5000) {
        ++guard;
        bool horizontal = QRandomGenerator::global()->bounded(2) == 0;
        int len = QRandomGenerator::global()->bounded(4, 12); int x, y;
        if (horizontal) {
            if (MAP_W - len - 2 <= 2) continue;
            x = QRandomGenerator::global()->bounded(2, MAP_W - len - 2); y = QRandomGenerator::global()->bounded(2, MAP_H - 2);
        }
        else {
            if (MAP_H - len - 2 <= 2) continue;
            x = QRandomGenerator::global()->bounded(2, MAP_W - 2); y = QRandomGenerator::global()->bounded(2, MAP_H - len - 2);
        }
        for (int i = 0; i < len && placed < total; ++i) {
            int cx = horizontal ? x + i : x; int cy = horizontal ? y : y + i;
            if (cx >= 8 && cx <= 11 && cy >= 5 && cy <= 9) continue;
            if (i % 4 == 3) continue;
            if (!m_barriers.contains(key(cx, cy))) { m_barriers.insert(key(cx, cy)); ++placed; }
        }
    }
}

bool SnakeGame::isBarrier(int x, int y) const { return (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) ? false : m_barriers.contains(y * MAP_W + x); }
bool SnakeGame::isSnakeCell(int x, int y) const { for (const auto& p : m_snake) if (p.x() == x && p.y() == y) return true; return false; }
bool SnakeGame::pointHitsBarrierOrBomb(QPoint cell) const {
    if (isBarrier(cell.x(), cell.y()) || m_surpriseObstacles.contains(cell.y() * MAP_W + cell.x())) return true;
    QPointF center(cell.x() * BLOCK_SIZE + BLOCK_SIZE / 2.0, cell.y() * BLOCK_SIZE + BLOCK_SIZE / 2.0);
    for (const auto& b : m_bombs) {
        if (!b.alive) continue;
        QPointF bc = b.pos + QPointF(BLOCK_SIZE / 2.0, BLOCK_SIZE / 2.0);
        if (QLineF(center, bc).length() < BLOCK_SIZE * 0.8) return true;
    }
    return false;
}

void SnakeGame::spawnFood()
{
    QPoint p; int guard = 0;
    do {
        p = QPoint(QRandomGenerator::global()->bounded(MAP_W), QRandomGenerator::global()->bounded(MAP_H));
        ++guard;
    } while (guard < 500 && (isBarrier(p.x(), p.y()) || m_surpriseObstacles.contains(p.y() * MAP_W + p.x()) || isSnakeCell(p.x(), p.y()) || pointHitsBarrierOrBomb(p) || (m_mode == BattleMode && (m_snake2.contains(p) || p == m_foodPos2))));
    m_foodPos = p;
}

void SnakeGame::spawnFood2()
{
    QPoint p; int guard = 0;
    do {
        p = QPoint(QRandomGenerator::global()->bounded(MAP_W), QRandomGenerator::global()->bounded(MAP_H));
        ++guard;
    } while (guard < 500 && (isBarrier(p.x(), p.y()) || m_surpriseObstacles.contains(p.y() * MAP_W + p.x()) || m_snake2.contains(p) || p == m_foodPos || m_snake.contains(p)));
    m_foodPos2 = p;
}

void SnakeGame::ensureStarsOnScreen()
{
    if (!m_config.starEnabled) return;
    while (m_stars.size() < m_config.starsOnScreen) {
        QPoint p; int guard = 0;
        do {
            p = QPoint(QRandomGenerator::global()->bounded(MAP_W), QRandomGenerator::global()->bounded(MAP_H)); ++guard;
        } while (guard < 500 && (isBarrier(p.x(), p.y()) || m_surpriseObstacles.contains(p.y() * MAP_W + p.x()) || isSnakeCell(p.x(), p.y()) || p == m_foodPos || m_stars.contains(p) || m_diamonds.contains(p) || pointHitsBarrierOrBomb(p)));
        m_stars.append(p);
    }
}

void SnakeGame::ensureDiamondsOnScreen()
{
    if (!m_config.diamondEnabled) return;
    while (m_diamonds.size() < m_config.diamondsOnScreen) {
        QPoint p; int guard = 0;
        do {
            p = QPoint(QRandomGenerator::global()->bounded(MAP_W), QRandomGenerator::global()->bounded(MAP_H)); ++guard;
        } while (guard < 500 && (isBarrier(p.x(), p.y()) || m_surpriseObstacles.contains(p.y() * MAP_W + p.x()) || isSnakeCell(p.x(), p.y()) || p == m_foodPos || m_stars.contains(p) || m_diamonds.contains(p) || pointHitsBarrierOrBomb(p)));
        m_diamonds.append(p);
    }
}

void SnakeGame::spawnSurpriseObstacle()
{
    QPoint p; int guard = 0;
    do {
        p = QPoint(QRandomGenerator::global()->bounded(MAP_W), QRandomGenerator::global()->bounded(MAP_H)); guard++;
    } while (guard < 500 && (isSnakeCell(p.x(), p.y()) || m_surpriseObstacles.contains(p.y() * MAP_W + p.x()) || p == m_foodPos));
    m_surpriseObstacles.insert(p.y() * MAP_W + p.x());
}

void SnakeGame::teleportSnake()
{
    int len = m_snake.size(); int dx = 0, dy = 0;
    if (m_dir == UP) dy = 1; else if (m_dir == DOWN) dy = -1; else if (m_dir == LEFT) dx = 1; else if (m_dir == RIGHT) dx = -1;
    int nx = 0, ny = 0; int guard = 0; bool ok = false;
    while (!ok && guard < 200) {
        nx = QRandomGenerator::global()->bounded(2, MAP_W - 2); ny = QRandomGenerator::global()->bounded(2, MAP_H - 2); ok = true;
        for (int i = 0; i < len; ++i) {
            int tx = nx + dx * i, ty = ny + dy * i;
            if (isBarrier(tx, ty) || m_surpriseObstacles.contains(ty * MAP_W + tx)) { ok = false; break; }
        }
        guard++;
    }
    if (ok) {
        m_snake.clear();
        for (int i = 0; i < len; ++i) m_snake.append(QPoint(nx + dx * i, ny + dy * i));
    }
}

void SnakeGame::spawnBomb()
{
    if (m_bombs.size() >= m_config.bombMax) return;
    Bomb b; b.remainingMs = m_config.bombLifeMs;
    if (m_config.bombMoving) {
        int side = QRandomGenerator::global()->bounded(4); qreal speed = BLOCK_SIZE * 4.0;
        qreal rx = QRandomGenerator::global()->bounded(MAP_W) * BLOCK_SIZE; qreal ry = QRandomGenerator::global()->bounded(MAP_H) * BLOCK_SIZE;
        if (side == 0) { b.pos = QPointF(-BLOCK_SIZE, ry);   b.velocity = QPointF(speed, 0); }
        else if (side == 1) { b.pos = QPointF(MAP_W * BLOCK_SIZE, ry); b.velocity = QPointF(-speed, 0); }
        else if (side == 2) { b.pos = QPointF(rx, -BLOCK_SIZE);   b.velocity = QPointF(0, speed); }
        else { b.pos = QPointF(rx, MAP_H * BLOCK_SIZE); b.velocity = QPointF(0, -speed); }
        b.remainingMs = -1;
    }
    else {
        QPoint cell; int guard = 0;
        do { cell = QPoint(QRandomGenerator::global()->bounded(MAP_W), QRandomGenerator::global()->bounded(MAP_H)); ++guard; } while (guard < 500 && (isBarrier(cell.x(), cell.y()) || m_surpriseObstacles.contains(cell.y() * MAP_W + cell.x()) || isSnakeCell(cell.x(), cell.y()) || cell == m_foodPos || m_stars.contains(cell) || m_diamonds.contains(cell) || pointHitsBarrierOrBomb(cell)));
        b.pos = QPointF(cell.x() * BLOCK_SIZE, cell.y() * BLOCK_SIZE); b.velocity = QPointF(0, 0);
    }
    b.alive = true; m_bombs.append(b);
}

void SnakeGame::trySpawnBombByTimer()
{
    if (m_state != Playing || m_config.bombMax <= 0) return;
    while (m_bombs.size() < m_config.bombMax) spawnBomb();
    int next = QRandomGenerator::global()->bounded(m_config.bombSpawnMinMs, m_config.bombSpawnMaxMs + 1);
    m_bombTimer->start(next);
}

void SnakeGame::updateAnimations()
{
    if (m_effectType != 0) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int dt = int(now - m_lastAnimMs); if (dt <= 0) dt = 1; m_lastAnimMs = now;
    bool needUpdate = false;
    for (int i = m_bombs.size() - 1; i >= 0; --i) {
        Bomb& b = m_bombs[i];
        if (!b.alive) { m_bombs.removeAt(i); needUpdate = true; continue; }
        if (m_config.bombMoving) {
            b.pos += b.velocity * (dt / 1000.0);
            if (b.pos.x() < -BLOCK_SIZE * 2 || b.pos.x() > MAP_W * BLOCK_SIZE + BLOCK_SIZE * 2 || b.pos.y() < -BLOCK_SIZE * 2 || b.pos.y() > MAP_H * BLOCK_SIZE + BLOCK_SIZE * 2) {
                b.alive = false; m_bombs.removeAt(i); needUpdate = true;
            }
            else needUpdate = true;
        }
        else {
            if (b.remainingMs > 0) {
                b.remainingMs -= dt;
                if (b.remainingMs <= 0) { b.alive = false; m_bombs.removeAt(i); }
                needUpdate = true;
            }
        }
    }
    if (needUpdate) update();
}

bool SnakeGame::canMove(Dir d, const QList<QPoint>& snake) const
{
    if (snake.size() < 2) return true;
    QPoint h = snake[0], n = snake[1];
    if (d == UP && h.y() > n.y()) return false; if (d == DOWN && h.y() < n.y()) return false;
    if (d == LEFT && h.x() > n.x()) return false; if (d == RIGHT && h.x() < n.x()) return false;
    return true;
}

QPoint SnakeGame::mapOffset() const
{
    int ox = (width() - MAP_W * BLOCK_SIZE) / 2; int oy = (height() - MAP_H * BLOCK_SIZE) / 2;
    return QPoint(ox, oy);
}

void SnakeGame::gameMove()
{
    if (m_state != Playing || m_paused || m_gameOver || m_effectType != 0) return;

    if (m_mode == BattleMode)
    {
        if (!m_battleRunning) return;
        if (m_dir == STOP && m_dir2 == STOP) return;

        QPoint head1 = m_snake[0]; QPoint newHead1 = head1;
        switch (m_dir) {
        case UP:    newHead1.ry() -= 1; break; case DOWN:  newHead1.ry() += 1; break;
        case LEFT:  newHead1.rx() -= 1; break; case RIGHT: newHead1.rx() += 1; break;
        default: break;
        }
        if (newHead1.x() < 0) newHead1.rx() = MAP_W - 1; else if (newHead1.x() >= MAP_W) newHead1.rx() = 0;
        if (newHead1.y() < 0) newHead1.ry() = MAP_H - 1; else if (newHead1.y() >= MAP_H) newHead1.ry() = 0;

        QPoint head2 = m_snake2[0]; QPoint newHead2 = head2;
        switch (m_dir2) {
        case UP:    newHead2.ry() -= 1; break; case DOWN:  newHead2.ry() += 1; break;
        case LEFT:  newHead2.rx() -= 1; break; case RIGHT: newHead2.rx() += 1; break;
        default: break;
        }
        if (newHead2.x() < 0) newHead2.rx() = MAP_W - 1; else if (newHead2.x() >= MAP_W) newHead2.rx() = 0;
        if (newHead2.y() < 0) newHead2.ry() = MAP_H - 1; else if (newHead2.y() >= MAP_H) newHead2.ry() = 0;

        bool p1Dead = false, p2Dead = false;

        if (newHead1 == newHead2) { p1Dead = true; p2Dead = true; }
        else {
            for (QPoint p : m_snake2) if (newHead1 == p) p1Dead = true;
            for (QPoint p : m_snake)  if (newHead2 == p) p2Dead = true;
        }

        bool ate1 = false, ate2 = false;
        if (newHead1 == m_foodPos) { m_battleScore1 += 10; ate1 = true; playSfx("eat.mp3"); }
        if (newHead2 == m_foodPos2) { m_battleScore2 += 10; ate2 = true; playSfx("eat.mp3"); }

        if (!p1Dead) m_snake.insert(0, newHead1);
        if (!p2Dead) m_snake2.insert(0, newHead2);

        if (!p1Dead && !ate1) m_snake.pop_back();
        if (!p2Dead && !ate2) m_snake2.pop_back();

        if (ate1) spawnFood();
        if (ate2) spawnFood2();

        if (p1Dead || p2Dead) {
            playSfx("hit.mp3");
            endBattle();
            if (p1Dead && !p2Dead) m_battleResult = "Player 2 Wins!";
            else if (!p1Dead && p2Dead) m_battleResult = "Player 1 Wins!";
            else m_battleResult = "Draw!";
        }

        update();
        return;
    }

    if (m_dir == STOP) return;

    QPoint head = m_snake[0]; QPoint newHead = head;
    switch (m_dir) {
    case UP:    newHead.ry() -= 1; break; case DOWN:  newHead.ry() += 1; break;
    case LEFT:  newHead.rx() -= 1; break; case RIGHT: newHead.rx() += 1; break;
    default: break;
    }

    // 穿墙
    if (newHead.x() < 0) newHead.rx() = MAP_W - 1; else if (newHead.x() >= MAP_W) newHead.rx() = 0;
    if (newHead.y() < 0) newHead.ry() = MAP_H - 1; else if (newHead.y() >= MAP_H) newHead.ry() = 0;

    bool hitObstacle = pointHitsBarrierOrBomb(newHead) || m_surpriseObstacles.contains(newHead.y() * MAP_W + newHead.x());
    if (hitObstacle) {
        if (m_mode == SurpriseMode) {
            m_lives--;
            playSfx("hit.mp3");
            if (m_lives <= 0) { m_state = LevelLost; m_gameOver = true; playSfx("game_over.mp3"); }
            else teleportSnake();
            update(); return;
        }
        else { m_state = LevelLost; m_gameOver = true; playSfx("game_over.mp3"); update(); return; }
    }

    bool hitSelf = false;
    for (int i = 0; i < m_snake.size(); ++i) if (newHead == m_snake[i]) { hitSelf = true; break; }
    if (hitSelf) {
        if (m_mode == SurpriseMode) {
            m_lives--;
            playSfx("hit.mp3");
            if (m_lives <= 0) { m_state = LevelLost; m_gameOver = true; playSfx("game_over.mp3"); }
            else teleportSnake();
            update(); return;
        }
        else { m_state = LevelLost; m_gameOver = true; playSfx("game_over.mp3"); update(); return; }
    }

    m_snake.insert(0, newHead);
    bool ate = false;

    if (m_mode == SurpriseMode) {
        if (newHead == m_foodPos) {
            m_snake.pop_front();
            int r = QRandomGenerator::global()->bounded(100);
            if (r < 70) m_effectType = 1;
            else if (r < 85) m_effectType = 2;
            else if (r < 94) m_effectType = 3;
            else if (r < 99) m_effectType = 4;
            else m_effectType = 5;

            m_effectPos = newHead;
            m_foodPos = QPoint(-1, -1);
            m_timer->stop();
            m_effectTimer->start(500);
            update();
            return;
        }
        else {
            m_snake.pop_back();
        }
    }
    else {
        if (m_config.foodEnabled && newHead == m_foodPos) { onEatFood(); ate = true; spawnFood(); }
        else if (m_stars.contains(newHead)) { m_stars.removeAll(newHead); onEatStar(); ate = false; }
        else if (m_diamonds.contains(newHead)) { m_diamonds.removeAll(newHead); onEatDiamond(); ate = false; }
        if (!ate) m_snake.pop_back();
        checkLevelState();
        if (m_config.starEnabled)    ensureStarsOnScreen();
        if (m_config.diamondEnabled) ensureDiamondsOnScreen();
    }
    update();
}

void SnakeGame::resolveSurpriseEffect()
{
    if (m_effectType == 1) {
        m_score += 10; m_snake.insert(0, m_effectPos); playSfx("eat.mp3");
    }
    else if (m_effectType == 2) {
        teleportSnake(); playSfx("eat.mp3");
    }
    else if (m_effectType == 3) {
        spawnSurpriseObstacle(); playSfx("eat.mp3");
    }
    else if (m_effectType == 4) {
        m_lives--; playSfx("boom.mp3");
        if (m_snake.size() > 1) m_snake.removeLast();
        if (m_lives <= 0) { m_state = LevelLost; m_gameOver = true; playSfx("game_over.mp3"); }
    }
    else if (m_effectType == 5) {
        m_lives++; playSfx("eat.mp3");
    }

    m_effectType = 0;
    if (m_lives > 0 && m_state == Playing) {
        spawnFood();
        m_timer->start(m_customInterval);
    }
    update();
}

void SnakeGame::onEatFood() { ++m_foodEaten; playSfx("eat.mp3"); }
void SnakeGame::onEatStar() { ++m_starEaten; playSfx("eat.mp3"); }
void SnakeGame::onEatDiamond() { ++m_diamondEaten; playSfx("eat.mp3"); }

void SnakeGame::checkLevelState()
{
    if (m_state != Playing) return;
    bool ok = true;
    if (m_config.targetFood > 0 && m_foodEaten < m_config.targetFood)    ok = false;
    if (m_config.targetStar > 0 && m_starEaten < m_config.targetStar)    ok = false;
    if (m_config.targetDiamond > 0 && m_diamondEaten < m_config.targetDiamond) ok = false;
    if (ok)
    {
        m_state = LevelWon;
        m_gameOver = true;
        playSfx("win.mp3");

        if (m_currentLevel > m_maxClearedLevel) {
            m_maxClearedLevel = m_currentLevel;

            // ★ 写入 QSettings 持久化保存
            QSettings settings("MyCompany", "SnakeGameSave");
            settings.setValue("MaxClearedLevel", m_maxClearedLevel);

            emit levelProgressChanged(m_maxClearedLevel);
        }
        if (m_currentLevel >= MAX_LEVEL)
            m_state = AllCleared;

        update();
    }
}

void SnakeGame::togglePause() {
    if (m_mode != EndlessMode || m_gameOver) return;
    m_paused = !m_paused; update();
}
void SnakeGame::restartLevel() {
    if (m_mode == LevelMode) loadLevel(); else resetGame();
}
void SnakeGame::nextLevel() {
    if (m_currentLevel >= MAX_LEVEL) m_currentLevel = 1; else ++m_currentLevel;
    loadLevel();
}

// ================================================================
//  绘制
// ================================================================
void SnakeGame::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    if (!m_bg.isNull()) p.drawPixmap(rect(), m_bg); else p.fillRect(rect(), Qt::black);

    QPoint off = mapOffset();
    auto drawAt = [&](const QPixmap& pix, int gx, int gy) {
        if (!pix.isNull()) p.drawPixmap(off.x() + gx * BLOCK_SIZE, off.y() + gy * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, pix);
        };

    // ★ 绘制倒三角指示器（悬浮在格子上方）
    auto drawIndicator = [&](int gx, int gy, QColor color) {
        int x = off.x() + gx * BLOCK_SIZE;
        int y = off.y() + gy * BLOCK_SIZE;
        QPolygon tri;
        tri << QPoint(x + BLOCK_SIZE / 2 - 10, y - 14)
            << QPoint(x + BLOCK_SIZE / 2 + 10, y - 14)
            << QPoint(x + BLOCK_SIZE / 2, y - 2);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawPolygon(tri);
        };

    for (int key : m_barriers) { int gx = key % MAP_W, gy = key / MAP_W; if (!m_barrier.isNull()) drawAt(m_barrier, gx, gy); else p.fillRect(off.x() + gx * BLOCK_SIZE, off.y() + gy * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, QColor(120, 70, 30)); }
    for (int key : m_surpriseObstacles) { int gx = key % MAP_W, gy = key / MAP_W; if (!m_barrier.isNull()) drawAt(m_barrier, gx, gy); else p.fillRect(off.x() + gx * BLOCK_SIZE, off.y() + gy * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, QColor(120, 70, 30)); }

    // 对战模式双食物
    if (m_mode == BattleMode) {
        if (!m_food.isNull()) drawAt(m_food, m_foodPos.x(), m_foodPos.y());
        else p.fillRect(off.x() + m_foodPos.x() * BLOCK_SIZE, off.y() + m_foodPos.y() * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, Qt::green);
        drawIndicator(m_foodPos.x(), m_foodPos.y(), QColor(100, 255, 100));

        if (!m_food2.isNull()) drawAt(m_food2, m_foodPos2.x(), m_foodPos2.y());
        else p.fillRect(off.x() + m_foodPos2.x() * BLOCK_SIZE, off.y() + m_foodPos2.y() * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, Qt::yellow);
        drawIndicator(m_foodPos2.x(), m_foodPos2.y(), QColor(255, 150, 50));
    }
    else if (m_mode == SurpriseMode) {
        if (m_effectType != 0) {
            int gx = m_effectPos.x(), gy = m_effectPos.y();
            if (m_effectType == 1 && !m_food.isNull()) drawAt(m_food, gx, gy);
            else if (m_effectType == 2 && !m_star.isNull()) drawAt(m_star, gx, gy);
            else if (m_effectType == 3 && !m_diamond.isNull()) drawAt(m_diamond, gx, gy);
            else if (m_effectType == 4 && !m_bomb.isNull()) drawAt(m_bomb, gx, gy);
            else if (m_effectType == 5) {
                p.setPen(QColor(255, 50, 50));
                QFont f = p.font(); f.setPointSize(38); f.setBold(true); p.setFont(f);
                p.drawText(off.x() + gx * BLOCK_SIZE, off.y() + gy * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, Qt::AlignCenter, "♥");
            }
        }
        else if (m_foodPos.x() != -1) {
            if (!m_unknown.isNull()) drawAt(m_unknown, m_foodPos.x(), m_foodPos.y());
        }
    }
    else if (m_config.foodEnabled) {
        if (!m_food.isNull()) drawAt(m_food, m_foodPos.x(), m_foodPos.y());
        else p.fillRect(off.x() + m_foodPos.x() * BLOCK_SIZE, off.y() + m_foodPos.y() * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, Qt::green);
    }

    for (const QPoint& s : m_stars) { if (!m_star.isNull()) drawAt(m_star, s.x(), s.y()); }
    for (const QPoint& d : m_diamonds) { if (!m_diamond.isNull()) drawAt(m_diamond, d.x(), d.y()); }

    auto drawSnake = [&](const QList<QPoint>& snake, Dir dir, const QPixmap& head, const QPixmap& body, const QPixmap& tail) {
        int len = snake.size();
        for (int i = 0; i < len; ++i) {
            QPoint c = snake[i]; int drawX = off.x() + c.x() * BLOCK_SIZE; int drawY = off.y() + c.y() * BLOCK_SIZE;
            if (i == 0) {
                if (!head.isNull()) {
    QTransform t; switch (dir) { case UP: t.rotate(270); break; case DOWN: t.rotate(90); break; case LEFT: t.rotate(180); break; default: t.rotate(0); break; }
                                        p.drawPixmap(drawX, drawY, BLOCK_SIZE, BLOCK_SIZE, head.transformed(t, Qt::SmoothTransformation));
                }
                else p.fillRect(drawX, drawY, BLOCK_SIZE, BLOCK_SIZE, Qt::red);
            }
            else if (i == len - 1) {
                if (!tail.isNull()) {
                    QPoint prev = snake[i - 1]; int angle = 0;
                    if (prev.x() > c.x()) angle = 0; else if (prev.x() < c.x()) angle = 180;
                    else if (prev.y() > c.y()) angle = 90; else if (prev.y() < c.y()) angle = 270;
                    QTransform t; t.rotate(angle); p.drawPixmap(drawX, drawY, BLOCK_SIZE, BLOCK_SIZE, tail.transformed(t, Qt::SmoothTransformation));
                }
                else p.fillRect(drawX, drawY, BLOCK_SIZE, BLOCK_SIZE, Qt::blue);
            }
            else {
                if (!body.isNull()) {
                    QPoint prev = snake[i - 1]; QPoint next = snake[i + 1]; int angle = 0;
                    bool hori = (prev.y() == c.y()) && (next.y() == c.y()); bool vert = (prev.x() == c.x()) && (next.x() == c.x());
                    if (hori) angle = 0; else if (vert) angle = 90;
                    QTransform t; t.rotate(angle); p.drawPixmap(drawX, drawY, BLOCK_SIZE, BLOCK_SIZE, body.transformed(t, Qt::SmoothTransformation));
                }
                else p.fillRect(drawX, drawY, BLOCK_SIZE, BLOCK_SIZE, Qt::black);
            }
        }
        };

    drawSnake(m_snake, m_dir, m_head, m_body, m_tail);
    if (m_mode == BattleMode) {
        drawSnake(m_snake2, m_dir2, m_head2, m_body2, m_tail2);
        if (!m_snake.isEmpty()) drawIndicator(m_snake[0].x(), m_snake[0].y(), QColor(100, 255, 100));
        if (!m_snake2.isEmpty()) drawIndicator(m_snake2[0].x(), m_snake2[0].y(), QColor(255, 150, 50));
    }

    for (const Bomb& b : m_bombs) {
        int x = off.x() + int(b.pos.x()), y = off.y() + int(b.pos.y());
        if (!m_bomb.isNull()) p.drawPixmap(x, y, BLOCK_SIZE, BLOCK_SIZE, m_bomb);
        else p.fillRect(x, y, BLOCK_SIZE, BLOCK_SIZE, QColor(20, 20, 20));
        if (!m_config.bombMoving && b.remainingMs > 0) {
            p.setPen(Qt::white); QFont f = p.font(); f.setPointSize(12); f.setBold(true); p.setFont(f);
            p.drawText(QRect(x, y, BLOCK_SIZE, BLOCK_SIZE), Qt::AlignCenter, QString::number((b.remainingMs + 999) / 1000));
        }
    }

    // === UI ===
    p.setPen(Qt::white); QFont uiFont = p.font(); uiFont.setPointSize(16); uiFont.setBold(true); p.setFont(uiFont);
    if (m_mode == SurpriseMode) {
        p.drawText(20, 35, QString("Surprise Mode    Score: %1").arg(m_score));
        p.setPen(QColor(255, 50, 50));
        QFont heartFont = p.font(); heartFont.setPointSize(26); heartFont.setBold(true); p.setFont(heartFont);
        QString hearts; for (int i = 0; i < m_lives; ++i) hearts += "♥ ";
        p.drawText(20, 80, hearts);
    }
    else if (m_mode == BattleMode) {
        p.setPen(QColor(100, 255, 100)); p.drawText(20, 35, QString("P1: %1").arg(m_battleScore1));
        p.setPen(QColor(255, 150, 50));  p.drawText(width() - 120, 35, QString("P2: %1").arg(m_battleScore2));
        p.setPen(Qt::white);             p.drawText(rect().adjusted(0, 20, 0, 0), Qt::AlignHCenter | Qt::AlignTop, QString("Time: %1s").arg(m_battleTimeLeft));
    }
    else {
        drawLevelUI(p);
    }
    drawOverlay(p);
}

void SnakeGame::drawLevelUI(QPainter& p)
{
    p.setPen(Qt::white); QFont f = p.font(); f.setPointSize(16); f.setBold(true); p.setFont(f);
    QString left;
    if (m_mode == LevelMode) {
        left = QString("Level %1").arg(levelName(m_currentLevel));
        QStringList targets;
        if (m_config.targetFood > 0) targets << QString("Food %1/%2").arg(m_foodEaten).arg(m_config.targetFood);
        if (m_config.targetStar > 0) targets << QString("Star %1/%2").arg(m_starEaten).arg(m_config.targetStar);
        if (m_config.targetDiamond > 0) targets << QString("Diamond %1/%2").arg(m_diamondEaten).arg(m_config.targetDiamond);
        if (!targets.isEmpty()) left += "   " + targets.join("   ");
    }
    else if (m_mode == EndlessMode) {
        left = QString("Endless Mode    Score: %1").arg(m_foodEaten * 10);
    }
    else {
        left = "Battle Mode";
    }
    p.drawText(20, 35, left);
}

void SnakeGame::drawOverlay(QPainter& p)
{
    if (m_state == Playing && !(m_paused && m_mode == EndlessMode) && !(m_mode == BattleMode && m_battleReadyTime > 0)) return;
    p.fillRect(rect(), QColor(0, 0, 0, 130));
    QFont big = p.font(); big.setPointSize(52); big.setBold(true);

    if (m_mode == BattleMode && m_state == Playing && m_battleReadyTime > 0) {
        p.setFont(big); p.setPen(Qt::yellow); p.drawText(rect(), Qt::AlignCenter, QString::number(m_battleReadyTime));
        return;
    }

    if (m_state == LevelWon || m_state == AllCleared) {
        p.setFont(big); p.setPen(QColor(255, 220, 60));
        if (m_mode == BattleMode) {
            p.drawText(QRect(0, 140, width(), 90), Qt::AlignCenter, m_battleResult);
            QFont mid = p.font(); mid.setPointSize(30); mid.setBold(true); p.setFont(mid); p.setPen(Qt::white);
            p.drawText(QRect(0, 250, width(), 40), Qt::AlignCenter, QString("P1: %1   vs   P2: %2").arg(m_battleScore1).arg(m_battleScore2));
        }
        else {
            p.drawText(QRect(0, 140, width(), 90), Qt::AlignCenter, "Level Complete!");
            QFont mid = p.font(); mid.setPointSize(24); mid.setBold(true); p.setFont(mid); p.setPen(Qt::white);
            if (m_state == AllCleared) {
                p.drawText(QRect(0, 250, width(), 40), Qt::AlignCenter, "All Levels Cleared!");
                if (m_blinkOn) p.drawText(QRect(0, height() - 90, width(), 40), Qt::AlignCenter, "Press Enter to Restart from 1-1");
            }
            else {
                if (m_blinkOn) p.drawText(QRect(0, 250, width(), 40), Qt::AlignCenter, "Press Enter for Next Level");
            }
        }
        p.setPen(QColor(200, 200, 200)); QFont sm = p.font(); sm.setPointSize(18); p.setFont(sm);
        p.drawText(QRect(0, height() - 90, width(), 40), Qt::AlignCenter, "Press Esc to Return to Menu");
    }
    else if (m_state == LevelLost) {
        p.setFont(big); p.setPen(Qt::red);
        p.drawText(QRect(0, 140, width(), 90), Qt::AlignCenter, "Game Over!");
        if (m_blinkOn) {
            QFont mid = p.font(); mid.setPointSize(24); mid.setBold(true); p.setFont(mid); p.setPen(Qt::yellow);
            p.drawText(QRect(0, 250, width(), 40), Qt::AlignCenter, "Press Space to Restart");
        }
        p.setPen(QColor(200, 200, 200)); QFont sm = p.font(); sm.setPointSize(18); p.setFont(sm);
        p.drawText(QRect(0, height() - 90, width(), 40), Qt::AlignCenter, "Press Esc to Exit to Menu");
    }
    else if (m_paused && m_mode == EndlessMode) {
        p.setFont(big); p.setPen(Qt::yellow); p.drawText(rect(), Qt::AlignCenter, "Paused\nPress Space to Resume");
    }
}

// ================================================================
//  键盘
// ================================================================
void SnakeGame::keyPressEvent(QKeyEvent* e)
{
    if (m_state == LevelWon || m_state == LevelLost || m_state == AllCleared) {
        if (e->key() == Qt::Key_Escape) { emit backToMenu(); return; }
        if (m_mode == SurpriseMode) {
            if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) { loadSurpriseLevel(); return; }
            if (e->key() == Qt::Key_Space && m_state == LevelLost) { loadSurpriseLevel(); return; }
            return;
        }
        if (m_mode == BattleMode && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) {
            loadBattleMode(); return;
        }
        if (m_state == LevelWon && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) { nextLevel(); return; }
        if (m_state == AllCleared && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) {
            m_currentLevel = 1; m_maxClearedLevel = 0; emit levelProgressChanged(0); loadLevel(); return;
        }
        if (m_state == LevelLost && e->key() == Qt::Key_Space) { restartLevel(); return; }
        return;
    }

    if (m_paused) return;
    if (m_effectType != 0) return;

    if (m_mode == BattleMode) {
        if (!m_battleRunning) return;
        if (e->key() == Qt::Key_W && canMove(UP, m_snake))    m_dir = UP;
        else if (e->key() == Qt::Key_S && canMove(DOWN, m_snake))  m_dir = DOWN;
        else if (e->key() == Qt::Key_A && canMove(LEFT, m_snake))  m_dir = LEFT;
        else if (e->key() == Qt::Key_D && canMove(RIGHT, m_snake)) m_dir = RIGHT;
        else if (e->key() == Qt::Key_Up && canMove(UP, m_snake2))    m_dir2 = UP;
        else if (e->key() == Qt::Key_Down && canMove(DOWN, m_snake2))  m_dir2 = DOWN;
        else if (e->key() == Qt::Key_Left && canMove(LEFT, m_snake2))  m_dir2 = LEFT;
        else if (e->key() == Qt::Key_Right && canMove(RIGHT, m_snake2)) m_dir2 = RIGHT;
        return;
    }

    if (m_dir == STOP) return;
    switch (e->key()) {
    case Qt::Key_Up:    if (canMove(UP, m_snake))    m_dir = UP;    break;
    case Qt::Key_Down:  if (canMove(DOWN, m_snake))  m_dir = DOWN;  break;
    case Qt::Key_Left:  if (canMove(LEFT, m_snake))  m_dir = LEFT;  break;
    case Qt::Key_Right: if (canMove(RIGHT, m_snake)) m_dir = RIGHT; break;
    default: QWidget::keyPressEvent(e); break;
    }
}