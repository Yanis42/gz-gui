#ifndef PATCHER_H
#define PATCHER_H
#include <string>
#include <exception>
#include <QFileDialog>
#include <QThread>

class PatcherSettings
{
public:
    enum patch_mode_t
    {
        ROM,
        WAD,
    };

    enum wad_remap_t
    {
        DEFAULT,
        RAPHNET,
        NONE,
    };

    enum wad_region_t
    {
        JAP,
        USA,
        EUR,
        FREE,
    };

    patch_mode_t patch_mode = patch_mode_t::ROM;

    std::string rom_path;
    std::string ucode_path;
    bool opt_ucode = false;

    std::string wad_path;
    std::string extrom_path;
    bool opt_extrom = false;
    enum wad_remap_t wad_remap = wad_remap_t::DEFAULT;
    std::string channel_id;
    std::string channel_title;
    enum wad_region_t wad_region = wad_region_t::FREE;
};

class Patcher : public QThread
{
    Q_OBJECT

public:
    Patcher(const PatcherSettings &settings, QWidget *parent = nullptr);

    int getResult();
    void run() override;

signals:
    void output(const QString &);
    void needSaveFileName(QString *result,
                          const QString &caption = QString(),
                          const QString &dir = QString(),
                          const QString &filter = QString(),
                          QString *selectedFilter = nullptr,
                          QFileDialog::Option options =
                              static_cast<QFileDialog::Option>(0));

private:
    PatcherSettings settings;
    std::exception_ptr eptr;
    int result;

    int patch();
};

#endif
