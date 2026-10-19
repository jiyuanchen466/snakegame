#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QSlider>
#include <QPainter>
#include <QMouseEvent>
#include <QFrame>
#include "SnakeGame.h"

// 自定义滑动条：粗长条 + 倒三角指示
class SpeedSlider : public QSlider
{
    Q_OBJECT
public:
    explicit SpeedSlider(QWidget* parent = nullptr) : QSlider(Qt::Horizontal, parent) {}
protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
};

// 遮罩设置层
class SettingsOverlay : public QWidget
{
    Q_OBJECT
public:
    // ★ 修改构造函数，增加音乐和音效初始状态
    explicit SettingsOverlay(QWidget* parent, int initialSpeed, bool musicOn, bool sfxOn);
signals:
    void speedChanged(int val);
    void musicToggled(bool enabled); // ★ 新增
    void sfxToggled(bool enabled);   // ★ 新增
protected:
    void mousePressEvent(QMouseEvent* e) override;
private:
    QFrame* m_panel;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void enterLevelMode();
    void enterEndlessMode();
    void enterBattleMode();
    void enterSurpriseMode();
    void openSettings();
    void updateProgressLabel(int maxCleared);

private:
    void createStartMenu();
    QPushButton* createMenuButton(const QString& text);

private:
    QStackedWidget* m_stackedWidget = nullptr;
    QWidget* m_startMenu = nullptr;
    QLabel* m_bgLabel = nullptr;
    SnakeGame* m_gameWidget = nullptr;

    QPushButton* m_btnLevel = nullptr;
    QPushButton* m_btnGear = nullptr;
    SettingsOverlay* m_settingsOverlay = nullptr;

    int              m_progressLevel = 1;
    int              m_currentSpeedVal = 5;
    bool             m_musicOn = true;  // ★ 新增
    bool             m_sfxOn = true;
};

#endif // MAINWINDOW_H