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
        ISO,
    };

    enum controller_remap_t
    {
        DEFAULT,
        RAPHNET,
        NONE,
    };

    enum console_region_t
    {
        JAP,
        USA,
        EUR,
        FREE,
    };

    patch_mode_t patch_mode = patch_mode_t::ROM;

    // Patch a ROM
    std::string rom_path;
    std::string ucode_path;
    bool opt_ucode = false;

    // Patch a WAD
    std::string wad_path;
    std::string extrom_path;
    bool opt_extrom = false;
    enum controller_remap_t wad_remap = controller_remap_t::DEFAULT;
    std::string channel_id;
    std::string channel_title;
    enum console_region_t wad_region = console_region_t::FREE;

    // Patch an ISO
    std::string iso_path;
    std::string iso_extrom_path;
    bool iso_is_mq = false; // "true" if the base ISO ID is "D43J01" or "D43E01"
    bool iso_do_trim = true; // "true" to remove the useless files to save space
    bool iso_opt_extrom = false;
    std::string iso_extrom_mq_path;
    bool iso_opt_extrom_mq = false;
    enum controller_remap_t iso_remap = controller_remap_t::DEFAULT;
    std::string game_id;
    std::string game_name;
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
