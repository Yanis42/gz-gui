#include <sys/stat.h>
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QtGlobal>
#include "mainwindow.h"

bool check_files()
{
    struct checkfile
    {
        const char *path;
        mode_t      mode;
    };
    struct checkfile files[] =
    {
#ifdef Q_OS_WIN
        {"bin\\gru.exe", S_IFREG | S_IXUSR},
        {"bin\\gzinject.exe", S_IFREG | S_IXUSR},
#else
        {"bin/gru", S_IFREG | S_IXUSR},
        {"bin/gzinject", S_IFREG | S_IXUSR},
#endif
        {"lua/patch-rom.lua", S_IFREG},
        {"lua/patch-wad.lua", S_IFREG},
        {"lua/patch-iso.lua", S_IFREG},
        {"lua/rom_table.lua", S_IFREG},
        {"lua/inject_ucode.lua", S_IFREG},
        {"ups", S_IFDIR},
        {"gzi", S_IFDIR},
    };
    for (auto &f : files) {
        struct stat statbuf;
        if (stat(f.path, &statbuf) != 0
            || (statbuf.st_mode & f.mode) != f.mode)
        {
            return false;
        }
    }
    return true;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

#ifdef Q_OS_DARWIN
    QDir::setCurrent(a.applicationDirPath() + "/../Resources");
#endif

    if (!check_files()) {
        QMessageBox::warning(nullptr, "",
                             "Files are missing! If you've downloaded this"
                             " program as part of a package, be sure to extract"
                             " all of the files inside the package.");
    }

    MainWindow w;
    w.show();
    return a.exec();
}
