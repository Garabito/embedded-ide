#ifndef DOCUMENTVIEW_H
#define DOCUMENTVIEW_H

#include <QWidget>
#include <QModelIndex>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>

#include "makefileinfo.h"
#include "etags.h"

namespace Ui {
    class ProjectView;
}

class QLabel;
class QToolButton;
class QMenu;
class QProcess;
class DebugInterface;
class TagList;

class MyFileSystemModel: public QFileSystemModel
{
    Q_OBJECT
public:
    MyFileSystemModel(QObject *parent = 0l);
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

private:
    QStringList sectionName;
};

class ProjectView : public QWidget
{
    Q_OBJECT

public:
    typedef QPair<QString, QVariant> Entry_t;
    typedef QList<Entry_t> EntryList_t;

    explicit ProjectView(QWidget *parent = 0);
    ~ProjectView();

    QString project() const;
    QString projectName() const;
    QDir projectPath() const;
    const MakefileInfo &makeInfo() const { return mk_info; }
    const ETags &tags() const { return mk_info.tags; }
    DebugInterface *getDebugInterface() const;
    void setMainMenu(QMenu *m);

public slots:
    void closeProject();
    void openProject(const QString& projectFile);
    void setDebugOn(bool on);

private slots:
    void on_treeView_activated(const QModelIndex &index);

    void updateMakefileInfo(const MakefileInfo &info);

    void on_targetList_doubleClicked(const QModelIndex &index);

    void on_toolButton_documentNew_clicked();

    void on_toolButton_folderNew_clicked();

    void on_toolButton_elementDel_clicked();

    void on_toolButton_export_clicked();

    void toolAction();

    void fileProperties(const QFileInfo& info);

    void on_treeView_pressed(const QModelIndex &index);

    void on_toolButton_find_clicked();

signals:
    void projectOpened();
    void fileOpen(const QString& file);
    void startBuild(const QString& target);
    void execTool(const QString& command);
    void openFindDialog();

private:
    Ui::ProjectView *ui;
    MakefileInfo mk_info;
    TagList *tagList;
    QList<QToolButton*> projectButtons;
    QLabel *labelStatus;

    QMenu *createExternalToolsMenu();
};

#endif // DOCUMENTVIEW_H
