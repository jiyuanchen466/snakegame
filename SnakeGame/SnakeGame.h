#ifndef SNAKEGAME_H
#define SNAKEGAME_H

#include <QWidget>
#include <QTimer>
#include <QPoint>
#include <QPointF>
#include <QList>
#include <QSet>
#include <QPainter>
#include <QPixmap>
#include <QKeyEvent>
#include <QMediaPlayer>
#include <QUrl>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioOutput>
#endif

enum Dir { STOP = 0, UP, DOWN, LEFT, RIGHT };

struct LevelConfig
{
    int targetFood = 0;
    int targetStar = 0;
    int targetDiamond = 0;
    int barrierCount = 0;
    int  bombMax = 0;
    bool bombMoving = false;
    int  bombLifeMs = 5000;
    int  bombSpawnMinMs = 10000;
    int  bombSpawnMaxMs = 15000;
    bool foodEnabled = true;
    bool starEnabled = false;
    bool diamondEnabled = false;
    int starsOnScreen = 1;
    int diamondsOnScreen = 1;
};

struct Bomb
{
    QPointF pos;
    QPointF velocity;
    int     remainingMs;
    bool    alive = true;
};

class SnakeGame : public QWidget
{
    Q_OBJECT
public:
    enum GameMode { LevelMode, EndlessMode, BattleMode, SurpriseMode };
    Q_ENUM(GameMode)
        enum LevelState { Playing, LevelWon, LevelLost, AllCleared };

    explicit SnakeGame(QWidget* parent = nullptr);
    ~SnakeGame() override;

    void setMode(GameMode mode);
    GameMode mode() const { return m_mode; }
    void setLevel(int lv);
    int  currentLevel() const { return m_currentLevel; }
    static const int MAX_LEVEL = 60;
    static QString levelName(int lv);
    void togglePause();
    void restartLevel();
    void nextLevel();
    void setSpeed(int interval); // ★ 外部设置速度
    void setMusicEnabled(bool enabled); // ★ 新增：控制背景音乐
    void setSfxEnabled(bool enabled);   // ★ 新增：控制游戏音效
    int maxClearedLevel() const { return m_maxClearedLevel; }

signals:
    void backToMenu();
    void levelProgressChanged(int maxCleared);

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void gameMove();
    void updateAnimations();
    void resolveSurpriseEffect();
    void battleTick();

private:
    void resetGame();
    void loadLevel();
    void loadSurpriseLevel();
    void loadBattleMode();
    void spawnSnakeAtEdge();
    void spawnSnakeRandom();
    void generateBarriers();
    void spawnFood();
    void spawnFood2();
    void ensureStarsOnScreen();
    void ensureDiamondsOnScreen();
    void spawnBomb();
    void trySpawnBombByTimer();
    void endBattle();
    void playSfx(const QString& filename); // ★ 音效播放

    bool canMove(Dir d, const QList<QPoint>& snake) const;
    bool isBarrier(int x, int y) const;
    bool isSnakeCell(int x, int y) const;
    bool pointHitsBarrierOrBomb(QPoint cell) const;
    QPoint mapOffset() const;

    void onEatFood();
    void onEatStar();
    void onEatDiamond();
    void checkLevelState();
    void teleportSnake();
    void spawnSurpriseObstacle();

    void drawLevelUI(QPainter& p);
    void drawOverlay(QPainter& p);

private:
    const int BLOCK_SIZE = 40;
    const int MAP_W = 30;
    const int MAP_H = 15;

    QTimer* m_timer = nullptr;
    QTimer* m_animTimer = nullptr;
    QTimer* m_bombTimer = nullptr;
    QTimer* m_blinkTimer = nullptr;
    QTimer* m_effectTimer = nullptr;
    QTimer* m_battleTimer = nullptr;
    qint64  m_lastAnimMs = 0;

    bool m_gameOver = false;
    bool m_paused = false;
    Dir  m_dir = STOP;

    GameMode   m_mode = LevelMode;
    LevelState m_state = Playing;

    int m_currentLevel = 1;
    int m_maxClearedLevel = 0;
    int m_surpriseLevel = 0;

    LevelConfig m_config;

    int m_foodEaten = 0;
    int m_starEaten = 0;
    int m_diamondEaten = 0;
    int m_score = 0;
    int m_lives = 3;

    int    m_effectType = 0;
    QPoint m_effectPos;

    QList<QPoint> m_snake;
    QPoint        m_foodPos;
    QSet<int>     m_barriers;
    QList<QPoint> m_stars;
    QList<QPoint> m_diamonds;
    QList<Bomb>   m_bombs;
    QSet<int>     m_surpriseObstacles;

    // 对战模式专属
    QList<QPoint> m_snake2;
    QPoint        m_foodPos2;
    Dir           m_dir2 = STOP;
    int           m_battleScore1 = 0;
    int           m_battleScore2 = 0;
    int           m_battleTimeLeft = 120;
    int           m_battleReadyTime = 3;
    bool          m_battleRunning = false;
    bool          m_battleEnded = false;
    QString       m_battleResult;

    int  m_nextBombMs = 10000;
    bool m_blinkOn = true;
    int  m_customInterval = 150;
    bool m_musicEnabled = true;
    bool m_sfxEnabled = true;

    QMediaPlayer* m_bgm = nullptr; // 背景音乐

    QPixmap m_head, m_body, m_tail, m_food, m_bg;
    QPixmap m_barrier, m_star, m_diamond, m_bomb;
    QPixmap m_unknown;
    QPixmap m_head2, m_body2, m_tail2, m_food2;
};

#endif