#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    QString last_extrom_mq_iso_text;

    void connect_rom();
    void connect_wad();
    void connect_iso();
    void update_iso_mq_state(QString *path);
    void update_go_state();
};

#endif
