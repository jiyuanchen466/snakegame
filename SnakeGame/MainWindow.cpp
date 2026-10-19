#include "MainWindow.h"
#include <QFont>
#include <QPixmap>
#include <QLinearGradient>
#include <QTimer>
#include <QApplication>
#include <QDebug>
#include <QCheckBox>
#include <QSettings>

// ================================================================
//  自定义滑动条绘制
// ================================================================
void SpeedSlider::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int h = height();
    int w = width();

    // 1. 绘制粗长条背景
    QRect grooveRect(12, h / 2 - 8, w - 24, 16);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(80, 80, 80));
    p.drawRoundedRect(grooveRect, 8, 8);

    // 2. 计算倒三角位置
    double ratio = 0.0;
    if (maximum() != minimum())
        ratio = (double)(value() - minimum()) / (maximum() - minimum());
    int handleX = grooveRect.left() + ratio * grooveRect.width();

    // 3. 绘制进度填充
    p.setBrush(QColor(255, 150, 50));
    QRect fillRect(grooveRect.left(), grooveRect.top(), handleX - grooveRect.left(), grooveRect.height());
    p.drawRoundedRect(fillRect, 8, 8);

    // 4. 绘制倒三角指示器
    QPolygon tri;
    tri << QPoint(handleX - 10, grooveRect.top() - 12)
        << QPoint(handleX + 10, grooveRect.top() - 12)
        << QPoint(handleX, grooveRect.top() - 2);

    p.setBrush(QColor(255, 80, 50));
    p.setPen(QPen(Qt::white, 2));
    p.drawPolygon(tri);
}

void SpeedSlider::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        double ratio = (double)(e->pos().x() - 12) / (width() - 24);
        ratio = qBound(0.0, ratio, 1.0);
        int val = minimum() + ratio * (maximum() - minimum());
        setValue(val);
    }
    QSlider::mousePressEvent(e);
}

// ================================================================
//  SettingsOverlay 设置遮罩
// ================================================================
SettingsOverlay::SettingsOverlay(QWidget* parent, int initialSpeed, bool musicOn, bool sfxOn)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background-color: rgba(0, 0, 0, 180);");

    m_panel = new QFrame(this);
    m_panel->setFixedSize(600, 320); // ★ 高度从 250 增加到 320 以容纳新组件
    m_panel->setStyleSheet(
        "QFrame { background-color: #2b2b2b; border-radius: 15px; border: 2px solid #ff5733; }"
    );

    QVBoxLayout* mainPanelLayout = new QVBoxLayout(m_panel);
    mainPanelLayout->setContentsMargins(40, 30, 40, 30);
    mainPanelLayout->setSpacing(20);

    // 1. 标题
    QLabel* titleLabel = new QLabel("Settings", m_panel);
    titleLabel->setStyleSheet("font-size: 32px; font-weight: bold; color: #ffb566; background: transparent; border: none;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainPanelLayout->addWidget(titleLabel);

    // 2. 滑动条
    QHBoxLayout* hLayout = new QHBoxLayout();
    hLayout->setSpacing(20);
    QLabel* speedTitle = new QLabel("Snake Speed", m_panel);
    speedTitle->setStyleSheet("font-size: 20px; color: white; background: transparent; border: none;");
    hLayout->addWidget(speedTitle);

    SpeedSlider* slider = new SpeedSlider(m_panel);
    slider->setRange(1, 10);
    slider->setValue(initialSpeed);
    slider->setFixedHeight(60);
    slider->setStyleSheet("background: transparent; border: none;");
    hLayout->addWidget(slider, 1);

    QLabel* valueLabel = new QLabel(QString::number(initialSpeed), m_panel);
    valueLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #ff5733; background: transparent; border: none;");
    valueLabel->setFixedWidth(40);
    valueLabel->setAlignment(Qt::AlignCenter);
    hLayout->addWidget(valueLabel);
    mainPanelLayout->addLayout(hLayout);

    // ★ 3. 新增：音效与音乐开关
    QHBoxLayout* toggleLayout = new QHBoxLayout();
    toggleLayout->setSpacing(40);

    QString checkStyle =
        "QCheckBox { font-size: 18px; color: white; background: transparent; border: none; }"
        "QCheckBox::indicator { width: 24px; height: 24px; border-radius: 4px; border: 2px solid #ffb566; background: transparent; }"
        "QCheckBox::indicator:checked { background: #ff5733; border: 2px solid #ff5733; image: url(:/img/check.png); }"; // 如果没有check.png，去掉image那行，或者用默认勾选

    QCheckBox* musicCheck = new QCheckBox("Background Music", m_panel);
    musicCheck->setChecked(musicOn);
    musicCheck->setStyleSheet(checkStyle);
    connect(musicCheck, &QCheckBox::toggled, this, &SettingsOverlay::musicToggled);
    toggleLayout->addWidget(musicCheck);

    QCheckBox* sfxCheck = new QCheckBox("Sound Effects", m_panel);
    sfxCheck->setChecked(sfxOn);
    sfxCheck->setStyleSheet(checkStyle);
    connect(sfxCheck, &QCheckBox::toggled, this, &SettingsOverlay::sfxToggled);
    toggleLayout->addWidget(sfxCheck);

    mainPanelLayout->addLayout(toggleLayout);



    // 信号连接
    connect(slider, &QSlider::valueChanged, this, [this, valueLabel](int val) {
        valueLabel->setText(QString::number(val));
        emit speedChanged(val);
        });

    QVBoxLayout* wrapperLayout = new QVBoxLayout(this);
    wrapperLayout->addWidget(m_panel, 0, Qt::AlignCenter);
}


void SettingsOverlay::mousePressEvent(QMouseEvent* e)
{
    if (!m_panel->geometry().contains(e->pos())) {
        hide();
    }
}

// ================================================================
//  MainWindow
// ================================================================
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    this->setWindowTitle("Snake Game");
    this->setFixedSize(1200, 600);

    m_bgLabel = new QLabel(this);
    m_bgLabel->setGeometry(0, 0, 1200, 600);
    QPixmap bg(":/img/background.png"); // 背景修改为资源路径
    if (!bg.isNull()) {
        bg = bg.scaled(1200, 600, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        m_bgLabel->setPixmap(bg);
    }
    m_bgLabel->lower();

    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setGeometry(0, 0, 1200, 600);
    setCentralWidget(m_stackedWidget);

    createStartMenu();

    m_gameWidget = new SnakeGame();
    m_gameWidget->setFixedSize(1200, 600);
    m_gameWidget->setSpeed(150);

    m_stackedWidget->addWidget(m_startMenu);
    m_stackedWidget->addWidget(m_gameWidget);
    m_stackedWidget->setCurrentIndex(0);

    connect(m_gameWidget, &SnakeGame::backToMenu, this, [this]() {
        m_stackedWidget->setCurrentIndex(0);
        });

    connect(m_gameWidget, &SnakeGame::levelProgressChanged,
        this, &MainWindow::updateProgressLabel);

    QSettings settings("MyCompany", "SnakeGameSave");
    int savedLevel = settings.value("MaxClearedLevel", 0).toInt(); // 默认读取为 0
    updateProgressLabel(savedLevel);
}

void MainWindow::createStartMenu()
{
    m_startMenu = new QWidget();
    m_startMenu->setStyleSheet("background:transparent;");

    QHBoxLayout* mainLayout = new QHBoxLayout(m_startMenu);
    mainLayout->setContentsMargins(60, 40, 60, 40);
    mainLayout->setSpacing(40);

    QLabel* titleLabel = new QLabel("Snake Game");
    QFont titleFont;
    titleFont.setFamily("Segoe Script");
    titleFont.setPointSize(72);
    titleFont.setBold(true);
    titleFont.setItalic(true);
    if (!titleFont.exactMatch()) {
        titleFont.setFamily("Comic Sans MS");
        titleFont.setPointSize(72);
        titleFont.setBold(true);
    }
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet(
        "color: #ffff00; background: transparent; text-shadow: 4px 4px 12px rgba(0, 0, 0, 0.9);"
    );
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel, 1, Qt::AlignCenter);

    QWidget* btnContainer = new QWidget();
    btnContainer->setStyleSheet("background:transparent;");
    QVBoxLayout* btnLayout = new QVBoxLayout(btnContainer);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(16);
    btnLayout->setAlignment(Qt::AlignCenter);

    m_btnLevel = createMenuButton("Level Mode");
    QPushButton* btnSurprise = createMenuButton("Surprise Mode");
    QPushButton* btnEndless = createMenuButton("Endless Mode");
    QPushButton* btnBattle = createMenuButton("Battle Mode");

    btnSurprise->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #c471ed, stop:1 #7b2ff7); color: white; border-radius: 12px; border: 2px solid #ffffff; font-weight: bold; letter-spacing: 2px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #d99aff, stop:1 #9452ff); border: 2px solid #fff59d; }"
        "QPushButton:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #a24ce0, stop:1 #5a1fd0); }"
    );

    m_btnLevel->setFocusPolicy(Qt::NoFocus);
    btnSurprise->setFocusPolicy(Qt::NoFocus);
    btnEndless->setFocusPolicy(Qt::NoFocus);
    btnBattle->setFocusPolicy(Qt::NoFocus);

    btnLayout->addWidget(m_btnLevel);
    btnLayout->addWidget(btnSurprise);
    btnLayout->addWidget(btnEndless);
    btnLayout->addWidget(btnBattle);

    connect(m_btnLevel, &QPushButton::clicked, this, &MainWindow::enterLevelMode);
    connect(btnSurprise, &QPushButton::clicked, this, &MainWindow::enterSurpriseMode);
    connect(btnEndless, &QPushButton::clicked, this, &MainWindow::enterEndlessMode);
    connect(btnBattle, &QPushButton::clicked, this, &MainWindow::enterBattleMode);

    mainLayout->addWidget(btnContainer, 0, Qt::AlignCenter);

    m_btnGear = new QPushButton("⚙️", m_startMenu);
    m_btnGear->setGeometry(1120, 520, 60, 60);
    m_btnGear->setStyleSheet(
        "QPushButton { background: transparent; border: none; font-size: 40px; color: white; }"
        "QPushButton:hover { color: #ffb566; }"
    );
    m_btnGear->setFocusPolicy(Qt::NoFocus);
    connect(m_btnGear, &QPushButton::clicked, this, &MainWindow::openSettings);
}

QPushButton* MainWindow::createMenuButton(const QString& text)
{
    QPushButton* btn = new QPushButton(text);
    btn->setFixedSize(260, 65);
    QFont btnFont; btnFont.setFamily("Microsoft YaHei"); btnFont.setPointSize(14); btnFont.setBold(true);
    btn->setFont(btnFont);
    btn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ff9a3c, stop:1 #ff5733); color: white; border-radius: 12px; border: 2px solid #ffffff; font-weight: bold; letter-spacing: 2px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffb566, stop:1 #ff7043); border: 2px solid #fff59d; }"
        "QPushButton:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e67600, stop:1 #d84315); }"
    );
    return btn;
}

void MainWindow::enterLevelMode()
{
    m_gameWidget->setMode(SnakeGame::LevelMode);
    m_gameWidget->setLevel(m_progressLevel);
    m_stackedWidget->setCurrentIndex(1);
    QTimer::singleShot(0, m_gameWidget, [this]() { m_gameWidget->setFocus(); });
}

void MainWindow::enterEndlessMode()
{
    m_gameWidget->setMode(SnakeGame::EndlessMode);
    m_stackedWidget->setCurrentIndex(1);
    QTimer::singleShot(0, m_gameWidget, [this]() { m_gameWidget->setFocus(); });
}

void MainWindow::enterBattleMode()
{
    m_gameWidget->setMode(SnakeGame::BattleMode);
    m_stackedWidget->setCurrentIndex(1);
    QTimer::singleShot(0, m_gameWidget, [this]() { m_gameWidget->setFocus(); });
}

void MainWindow::enterSurpriseMode()
{
    m_gameWidget->setMode(SnakeGame::SurpriseMode);
    m_stackedWidget->setCurrentIndex(1);
    QTimer::singleShot(0, m_gameWidget, [this]() { m_gameWidget->setFocus(); });
}

void MainWindow::openSettings()
{
    if (!m_settingsOverlay) {
        // ★ 传入当前的音乐/音效状态
        m_settingsOverlay = new SettingsOverlay(this, m_currentSpeedVal, m_musicOn, m_sfxOn);

        connect(m_settingsOverlay, &SettingsOverlay::speedChanged, this, [this](int val) {
            m_currentSpeedVal = val;
            int interval = 250 - (val - 1) * (200 / 9.0);
            m_gameWidget->setSpeed(interval);
            });

        // ★ 连接音乐开关
        connect(m_settingsOverlay, &SettingsOverlay::musicToggled, this, [this](bool enabled) {
            m_musicOn = enabled;
            m_gameWidget->setMusicEnabled(enabled);
            });

        // ★ 连接音效开关
        connect(m_settingsOverlay, &SettingsOverlay::sfxToggled, this, [this](bool enabled) {
            m_sfxOn = enabled;
            m_gameWidget->setSfxEnabled(enabled);
            });
    }
    m_settingsOverlay->setGeometry(rect());
    m_settingsOverlay->show();
    m_settingsOverlay->raise();
}

void MainWindow::updateProgressLabel(int maxCleared)
{
    int next = maxCleared + 1;
    if (next > SnakeGame::MAX_LEVEL) next = 1;
    m_progressLevel = next;

    if (!m_btnLevel) return;
    if (maxCleared <= 0) {
        m_btnLevel->setText(QString("Level Mode  %1").arg(SnakeGame::levelName(m_progressLevel)));
    }
    else if (maxCleared >= SnakeGame::MAX_LEVEL) {
        m_btnLevel->setText("Level Mode  ALL CLEARED → 1-1");
    }
    else {
        m_btnLevel->setText(QString("Level Mode  %1").arg(SnakeGame::levelName(m_progressLevel)));
    }
}