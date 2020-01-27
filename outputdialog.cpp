#include <QCloseEvent>
#include <QScrollBar>
#include "outputdialog.h"
#include "ui_outputdialog.h"

OutputDialog::OutputDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::OutputDialog)
    , m_closable(false)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint
                   | Qt::WindowMinMaxButtonsHint);

    connect(ui->pushbutton_close, &QPushButton::clicked, this,
        [this]()
        {
            close();
        });
}

OutputDialog::~OutputDialog()
{
    delete ui;
}

bool OutputDialog::closable()
{
    return m_closable;
}

void OutputDialog::setClosable(bool closable)
{
    m_closable = closable;
    ui->pushbutton_close->setEnabled(m_closable);
}

void OutputDialog::write(const QString &output)
{
    auto scroll_value = ui->plaintextedit_output->verticalScrollBar()->value();
    auto scroll_max = ui->plaintextedit_output->verticalScrollBar()->maximum();
    bool max_scrolled = scroll_value == scroll_max;

    QString plaintext = ui->plaintextedit_output->toPlainText();
    plaintext.append(output);
    ui->plaintextedit_output->setPlainText(plaintext);

    if (max_scrolled) {
        scroll_max = ui->plaintextedit_output->verticalScrollBar()->maximum();
        ui->plaintextedit_output->verticalScrollBar()->setValue(scroll_max);
    }
}

void OutputDialog::done(int r)
{
    if (m_closable)
        QDialog::done(r);
}
