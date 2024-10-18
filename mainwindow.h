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
    QString default_extrom_mq_iso_text;

    void init_rom_tab();
    void init_wad_tab();
    void init_iso_tab();
    void update_iso_mq_state(QString *path);
    void update_go_state();
};

#endif
