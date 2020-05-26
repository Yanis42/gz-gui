#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include "mainwindow.h"
#include "outputdialog.h"
#include "patcher.h"
#include "ui_mainwindow.h"

static PatcherSettings settings;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(minimumWidth(), minimumHeight());

    connect(ui->tabwidget, &QTabWidget::currentChanged,
        [this](int index)
        {
            settings.patch_mode = static_cast<PatcherSettings::
                                              patch_mode_t>(index);
            update_go_state();
        });
    connect(ui->button_rom, &QPushButton::clicked,
        [this]()
        {
            QString path = QFileDialog::
                getOpenFileName(this, "Select ROM", "",
                                "Nintendo 64 ROM (*.z64 *.v64 *.n64)");
            if (!path.isEmpty()) {
                settings.rom_path = path.toStdString();
                path.remove(0, path.lastIndexOf('\\') + 1);
                path.remove(0, path.lastIndexOf('/') + 1);
                ui->label_rom->setText(path);
            }
            update_go_state();
        });
    connect(ui->checkbox_ucode, &QCheckBox::stateChanged,
        [this](int state)
        {
            settings.opt_ucode = state;
            update_go_state();
        });
    connect(ui->button_ucode, &QPushButton::clicked,
        [this]()
        {
            QString path = QFileDialog::
                getOpenFileName(this, "Select ROM", "",
                                "Nintendo 64 ROM (*.z64 *.v64 *.n64)");
            if (!path.isEmpty()) {
                settings.ucode_path = path.toStdString();
                path.remove(0, path.lastIndexOf('\\') + 1);
                path.remove(0, path.lastIndexOf('/') + 1);
                ui->label_ucode->setText(path);
                settings.opt_ucode = true;
                ui->checkbox_ucode->setCheckState(Qt::Checked);
            }
            update_go_state();
        });
    connect(ui->button_wad, &QPushButton::clicked,
        [this]()
        {
            QString path = QFileDialog::
                getOpenFileName(this, "Select WAD", "",
                                "Nintendo Wii WAD (*.wad)");
            if (!path.isEmpty()) {
                settings.wad_path = path.toStdString();
                path.remove(0, path.lastIndexOf('\\') + 1);
                path.remove(0, path.lastIndexOf('/') + 1);
                ui->label_wad->setText(path);
            }
            update_go_state();
        });
    connect(ui->checkbox_extrom, &QCheckBox::stateChanged,
        [this](int state)
        {
            settings.opt_extrom = state;
            update_go_state();
        });
    connect(ui->button_extrom, &QPushButton::clicked,
        [this]()
        {
            QString path = QFileDialog::
                getOpenFileName(this, "Select ROM", "",
                                "Nintendo 64 ROM (*.z64 *.v64 *.n64)");
            if (!path.isEmpty()) {
                settings.extrom_path = path.toStdString();
                path.remove(0, path.lastIndexOf('\\') + 1);
                path.remove(0, path.lastIndexOf('/') + 1);
                ui->label_extrom->setText(path);
                settings.opt_extrom = true;
                ui->checkbox_extrom->setCheckState(Qt::Checked);
            }
            update_go_state();
        });
    connect(ui->combobox_remap, QOverload<int>::of(&QComboBox::activated), this,
        [this](int index)
        {
            settings.wad_remap = static_cast<PatcherSettings::
                                             wad_remap_t>(index);
        });
    connect(ui->combobox_id, QOverload<int>::of(&QComboBox::activated), this,
        [this](int index)
        {
            switch (index) {
                case 0: {
                    ui->combobox_id->removeItem(2);
                    settings.channel_id.clear();
                    break;
                }
                case 1: {
                    ui->combobox_id->removeItem(2);
                    bool ok;
                    QString text = QInputDialog::
                        getText(this, "", "Channel ID:",
                                QLineEdit::Normal, "", &ok);
                    if (ok && !text.isEmpty()) {
                        ui->combobox_id->addItem(text);
                        ui->combobox_id->setCurrentIndex(2);
                        settings.channel_id = text.toStdString();
                    }
                    else
                        ui->combobox_id->setCurrentIndex(0);
                    break;
                }
            }
        });
    connect(ui->combobox_title, QOverload<int>::of(&QComboBox::activated), this,
        [this](int index)
        {
            switch (index) {
                case 0: {
                    ui->combobox_title->removeItem(2);
                    settings.channel_title.clear();
                    break;
                }
                case 1: {
                    ui->combobox_title->removeItem(2);
                    bool ok;
                    QString text = QInputDialog::
                         getText(this, "", "Channel title:",
                                 QLineEdit::Normal, "", &ok);
                    if (ok && !text.isEmpty()) {
                        ui->combobox_title->addItem(text);
                        ui->combobox_title->setCurrentIndex(2);
                        settings.channel_title = text.toStdString();
                    }
                    else
                        ui->combobox_title->setCurrentIndex(0);
                    break;
                }
            }
        });
    connect(ui->combobox_region, QOverload<int>::of(&QComboBox::activated),
            this,
        [this](int index)
        {
            settings.wad_region = static_cast<PatcherSettings::
                                              wad_region_t>(index);
        });
    connect(ui->button_go, &QPushButton::clicked,
        [this]()
        {
            OutputDialog pd(this);
            Patcher patcher(settings, this);

            connect(&patcher, &Patcher::output,
                    &pd, &OutputDialog::write);
            connect(&patcher, &Patcher::needSaveFileName, this,
                [&pd](QString *result, const QString &caption,
                    const QString &dir, const QString &filter,
                    QString *selectedFilter, QFileDialog::Option options)
                {
                    *result = QFileDialog::
                        getSaveFileName(&pd, caption, dir, filter,
                                        selectedFilter, options);
                },
                    Qt::BlockingQueuedConnection);
            connect(&patcher, &Patcher::finished, this,
                [&pd, &patcher]()
                {
                    pd.setClosable(true);
                    try {
                        int result = patcher.getResult();
                        if (result == 0)
                            pd.close();
                        else if (result == 2) {
                            QMessageBox::
                                warning(&pd, "Error",
                                        "Your ROM wasn't recognized. Try a"
                                        " different input rom and/or microcode"
                                        " rom.");
                        }
                        else {
                            QMessageBox::
                                warning(&pd, "Error",
                                        "Something went wrong! Refer to the"
                                        " output log for details.");
                        }
                    }
                    catch (const std::exception &e) {
                        pd.write(e.what());
                        QMessageBox::
                            warning(&pd, "Error",
                                    "Something went wrong! Refer to the"
                                    " output log for details.");
                    }
                });

            patcher.start();
            pd.exec();
        });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::update_go_state()
{
    bool enable_go = false;
    switch (settings.patch_mode) {
        case PatcherSettings::patch_mode_t::ROM: {
            enable_go = !settings.rom_path.empty()
                        && (!settings.opt_ucode
                            || !settings.ucode_path.empty());
            break;
        }
        case PatcherSettings::patch_mode_t::WAD: {
            enable_go = !settings.wad_path.empty()
                        && (!settings.opt_extrom
                            || !settings.extrom_path.empty());
            break;
        }
    }
    ui->button_go->setEnabled(enable_go);
}
