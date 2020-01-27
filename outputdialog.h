#ifndef OUTPUTDIALOG_H
#define OUTPUTDIALOG_H
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class OutputDialog; }
QT_END_NAMESPACE

class OutputDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(bool closable READ closable WRITE setClosable MEMBER m_closable)

public:
    explicit OutputDialog(QWidget *parent = nullptr);
    ~OutputDialog() override;

    bool closable();
    void setClosable(bool closable);

public slots:
    void write(const QString &output);
    void done(int r) override;

private:
    Ui::OutputDialog *ui;
    bool m_closable;
};

#endif
