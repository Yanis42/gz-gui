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

    // set patch_mode depending on the current tab index
    connect(ui->tabwidget, &QTabWidget::currentChanged,
        [this](int index)
        {
            settings.patch_mode = static_cast<PatcherSettings::
                                              patch_mode_t>(index);
            update_go_state();
        });

    init_rom_tab();
    init_wad_tab();
    init_iso_tab();

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

void MainWindow::init_rom_tab()
{
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
}

void MainWindow::init_wad_tab()
{
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
            ui->button_extrom->setEnabled(state);
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
        [](int index)
        {
            settings.wad_remap = static_cast<PatcherSettings::
                                             controller_remap_t>(index);
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
        [](int index)
        {
            settings.wad_region = static_cast<PatcherSettings::
                                              console_region_t>(index);
        });
}

void MainWindow::init_iso_tab()
{
    default_extrom_mq_iso_text = ui->label_extrom_mq_iso->text();
    update_iso_mq_state(nullptr);

    connect(ui->button_iso, &QPushButton::clicked,
        [this]()
        {
            QString path = QFileDialog::
                getOpenFileName(this, "Select ISO", "",
                                "Nintendo GameCube ISO (*.iso)");
            if (!path.isEmpty()) {
                update_iso_mq_state(&path);
                settings.iso_path = path.toStdString();
                path.remove(0, path.lastIndexOf('\\') + 1);
                path.remove(0, path.lastIndexOf('/') + 1);
                ui->label_iso->setText(path);
            }
            update_go_state();
        });
    
    connect(ui->checkbox_extrom_iso, &QCheckBox::stateChanged,
        [this](int state)
        {
            settings.iso_opt_extrom = state;
            ui->button_extrom_iso->setEnabled(state);
            update_go_state();
        });
    
    connect(ui->button_extrom_iso, &QPushButton::clicked,
        [this]()
        {
            QString path = QFileDialog::
                getOpenFileName(this, "Select ROM", "",
                                "Nintendo 64 ROM (*.z64 *.v64 *.n64)");
            if (!path.isEmpty()) {
                settings.iso_extrom_path = path.toStdString();
                path.remove(0, path.lastIndexOf('\\') + 1);
                path.remove(0, path.lastIndexOf('/') + 1);
                ui->label_extrom_iso->setText(path);
                settings.iso_opt_extrom = true;
                ui->checkbox_extrom_iso->setCheckState(Qt::Checked);
            }
            update_go_state();
        });
    
    connect(ui->checkbox_extrom_mq_iso, &QCheckBox::stateChanged,
        [this](int state)
        {
            settings.iso_opt_extrom_mq = state;
            ui->button_extrom_mq_iso->setEnabled(state);
            update_go_state();
        });

    connect(ui->button_extrom_mq_iso, &QPushButton::clicked,
        [this]()
        {
            QString path = QFileDialog::
                getOpenFileName(this, "Select Master Quest ROM", "",
                                "Nintendo 64 ROM (*.z64 *.v64 *.n64)");
            if (!path.isEmpty()) {
                settings.iso_extrom_mq_path = path.toStdString();
                path.remove(0, path.lastIndexOf('\\') + 1);
                path.remove(0, path.lastIndexOf('/') + 1);
                ui->label_extrom_mq_iso->setText(path);
                settings.iso_opt_extrom_mq = true;
                ui->checkbox_extrom_mq_iso->setCheckState(Qt::Checked);
            }
            update_go_state();
        });

    connect(ui->combobox_remap_iso, QOverload<int>::of(&QComboBox::activated), this,
        [](int index)
        {
            settings.iso_remap = static_cast<PatcherSettings::
                                             controller_remap_t>(index);
        });
    
    connect(ui->combobox_id_iso, QOverload<int>::of(&QComboBox::activated), this,
        [this](int index)
        {
            switch (index) {
                case 0: {
                    ui->combobox_id_iso->removeItem(2);
                    settings.game_id.clear();
                    break;
                }
                case 1: {
                    ui->combobox_id_iso->removeItem(2);
                    bool ok;
                    QString text = QInputDialog::
                        getText(this, "", "Game ID:",
                                QLineEdit::Normal, "", &ok);
                    if (ok && !text.isEmpty()) {
                        ui->combobox_id_iso->addItem(text);
                        ui->combobox_id_iso->setCurrentIndex(2);
                        settings.game_id = text.toStdString();
                    }
                    else
                        ui->combobox_id_iso->setCurrentIndex(0);
                    break;
                }
            }
        });
    
    connect(ui->combobox_title_iso, QOverload<int>::of(&QComboBox::activated), this,
        [this](int index)
        {
            switch (index) {
                case 0: {
                    ui->combobox_title_iso->removeItem(2);
                    settings.game_name.clear();
                    break;
                }
                case 1: {
                    ui->combobox_title_iso->removeItem(2);
                    bool ok;
                    QString text = QInputDialog::
                         getText(this, "", "Game Name:",
                                 QLineEdit::Normal, "", &ok);
                    if (ok && !text.isEmpty()) {
                        ui->combobox_title_iso->addItem(text);
                        ui->combobox_title_iso->setCurrentIndex(2);
                        settings.game_name = text.toStdString();
                    }
                    else
                        ui->combobox_title_iso->setCurrentIndex(0);
                    break;
                }
            }
        });
    
    connect(ui->checkbox_iso_no_trim, &QCheckBox::stateChanged,
        [this](int state)
        {
            settings.iso_do_trim = state;
            update_go_state();
        });
}

void MainWindow::update_iso_mq_state(QString *path)
{
    //! TODO: improve how the label text update is handled
    QString new_text = "This game doesn't have Master Quest.";

    settings.iso_is_mq = false;

    if (path != nullptr) {
        QFile iso(*path);

        // technically this isn't a text file but
        // we only need to read the first 6 characters of the file
        if (iso.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream stream(&iso);
            QString game_id = stream.read(6);

            // supported MQ disc IDs (JP and US)
            if (game_id == "D43J01" || game_id == "D43E01") {
                new_text = QString::fromStdString(settings.iso_extrom_mq_path);

                settings.iso_is_mq = true;

                if (new_text != "") {
                    new_text.remove(0, new_text.lastIndexOf('\\') + 1);
                    new_text.remove(0, new_text.lastIndexOf('/') + 1);
                } else {
                    new_text = default_extrom_mq_iso_text;
                }
            }
        } else {
            QMessageBox::warning(nullptr, "", "ERROR: The ISO can't be opened.");
        }
    }

    // update the ui accordingly
    ui->checkbox_extrom_mq_iso->setEnabled(settings.iso_is_mq);
    ui->checkbox_extrom_mq_iso->setVisible(settings.iso_is_mq);
    ui->button_extrom_mq_iso->setDisabled(ui->checkbox_extrom_mq_iso->isChecked());
    ui->button_extrom_mq_iso->setVisible(settings.iso_is_mq);
    ui->label_extrom_mq_iso->setText(new_text);
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
        case PatcherSettings::patch_mode_t::ISO: {
            if (settings.iso_is_mq) {
                enable_go = !settings.iso_path.empty()
                            && (!settings.iso_opt_extrom || !settings.iso_extrom_path.empty())
                            && (!settings.iso_opt_extrom_mq || !settings.iso_extrom_mq_path.empty());
            } else {
                enable_go = !settings.iso_path.empty()
                            && (!settings.iso_opt_extrom || !settings.iso_extrom_path.empty());
            }
            break;
        }
    }
    ui->button_go->setEnabled(enable_go);
}
