#include "projectnewdialog.h"
#include "ui_projectnewdialog.h"
#include "appconfig.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QPushButton>
#include <QFileDialog>
#include <QItemDelegate>

#include <QtDebug>

struct Parameter_t {
    QString name;
    QString type;
    QString value;
};

class EditableItemDelegate : public QItemDelegate {
public:
    EditableItemDelegate(QObject *parent = nullptr) : QItemDelegate(parent) {
    }

    virtual QVariant getDefault() = 0;
};

class ListEditorDelegate : public EditableItemDelegate {
public:
    const QStringList items;

    ListEditorDelegate(const QString& data, QObject *parent = nullptr) :
        EditableItemDelegate(parent), items(data.split('|'))
    {
    }

    QVariant getDefault() override {
        return items.first();
    }

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        auto cb = new QComboBox(parent);
        cb->addItems(items);
        return cb;
    }
};


class StringEditorDelegate : public EditableItemDelegate {
public:
    QString data;
    StringEditorDelegate(QString  d, QObject *parent = nullptr) :
        EditableItemDelegate(parent), data(std::move(d))
    {
    }

    QVariant getDefault() override {
        return data;
    }

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        auto ed = new QLineEdit(parent);
        ed->setText(data);
        return ed;
    }
};

class DelegateFactory {
public:
    static EditableItemDelegate *create(const Parameter_t& p, QWidget *parent = nullptr)
    {
        Constructor constructor = constructors().value( p.type );
        if ( constructor == nullptr )
            return nullptr;
        return (*constructor)( p.value, parent );
    }

    template<typename T>
    static void registerClass(const QString& key)
    {
        constructors().insert( key, &constructorHelper<T> );
    }
private:
    typedef EditableItemDelegate* (*Constructor)( const QString& data, QWidget* parent );

    template<typename T>
    static EditableItemDelegate* constructorHelper( const QString& data, QWidget* parent )
    {
        return new T( data, parent );
    }

    static QHash<QString, Constructor>& constructors()
    {
        static QHash<QString, Constructor> instance;
        return instance;
    }
};

QString projectPath(const QString& path)
{
    QDir projecPath(
          path.isEmpty()
          ? AppConfig::mutableInstance().defaultProjectPath()
          : path);
    if (!projecPath.exists())
        projecPath.mkpath(".");
    return projecPath.absolutePath();
}

ProjectNewDialog::ProjectNewDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProjectNewDialog)
{
    DelegateFactory::registerClass<ListEditorDelegate>("items");
    DelegateFactory::registerClass<StringEditorDelegate>("string");
    QDir defTemplates(":/build/templates/");
    QDir localTemplates(
          AppConfig::mutableInstance().builTemplatePath());
    ui->setupUi(this);
    ui->parameterTable->horizontalHeader()->setStretchLastSection(true);
    QStringList prjList;
    foreach(QFileInfo info, localTemplates.entryInfoList(QStringList("*.template"))) {
        prjList.append(info.baseName());
        ui->templateFile->addItem(info.baseName(), info.absoluteFilePath());
    }
    // Prefer downloaded template name over bundled template
    foreach(QFileInfo info, defTemplates.entryInfoList(QStringList("*.template"))) {
        if (!prjList.contains(info.baseName()))
            ui->templateFile->addItem(info.baseName(), info.absoluteFilePath());
    }
    ui->projectPath->setText(
          ::projectPath(
            AppConfig::mutableInstance().builDefaultProjectPath()));
    refreshProjectName();
}

ProjectNewDialog::~ProjectNewDialog()
{
    delete ui;
}

QString ProjectNewDialog::projectPath() const
{
    return ui->projectFileText->text();
}

static QString readAllText(QFile &f) {
    QTextStream s(&f);
    return s.readAll();
}

QString ProjectNewDialog::templateText() const
{
    QFile f(ui->templateFile->currentData(Qt::UserRole).toString());
    QTextStream fs(&f);
    return f.open(QFile::ReadOnly)? replaceTemplates(readAllText(f)) : QString();
}

void ProjectNewDialog::refreshProjectName()
{
    ui->projectFileText->setText(QString("%1%2%3")
                                 .arg(ui->projectPath->text())
                                 .arg(ui->projectPath->text().isEmpty()? "" : QString(QDir::separator()))
                                 .arg(ui->projectName->text()));
    QString tamplate_path = ui->templateFile->currentData(Qt::UserRole).toString();
    bool en = QFileInfo(tamplate_path).exists() &&
            QFileInfo(ui->projectPath->text()).exists() &&
            !ui->projectName->text().isEmpty();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(en);
}

void ProjectNewDialog::on_toolFindProjectPath_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Project location"));
    if (!path.isEmpty())
        ui->projectPath->setText(path);
}

void ProjectNewDialog::on_toolLoadTemplate_clicked()
{
    QString templateName = QFileDialog::getOpenFileName(this,
                                                        tr("Open template"),
                                                        QString(),
                                                        tr("Template files (*.template);;"
                                                           "Diff files (*.diff);;"
                                                           "All files (*)"));
    if (!templateName.isEmpty()) {
        QFileInfo info(templateName);
        int idx = ui->templateFile->findText(info.baseName());
        if (idx == -1)
            ui->templateFile->insertItem(idx = 0, info.baseName(), info.absoluteFilePath());
        ui->templateFile->setCurrentIndex(idx);
    }
}

static QString readAll(const QString& fileName) {
    QFile f(fileName);
    return f.open(QFile::ReadOnly)? f.readAll() : QString();
}

void ProjectNewDialog::on_templateFile_editTextChanged(const QString &fileName)
{
    Q_UNUSED(fileName);
    QString name = ui->templateFile->currentData(Qt::UserRole).toString();
    QString text = readAll(name);
    ui->parameterTable->setRowCount(0);
    if (!text.isEmpty()) {
        QRegExp re(R"(\$\{\{([a-zA-Z0-9_]+)\s*([a-zA-Z0-9_]+)*\s*\:*(.*)\}\})");
        re.setMinimal(true);
        re.setPatternSyntax(QRegExp::RegExp2);
        int idx = 0;
        while ((idx = re.indexIn(text, idx)) != -1) {
            Parameter_t p = { re.cap(1).replace('_', ' '), re.cap(2), re.cap(3) };
            auto name = new QTableWidgetItem(p.name);
            name->setFlags(name->flags()&~Qt::ItemIsEditable);
            QTableWidgetItem *value = new QTableWidgetItem(QString());
            value->setFlags(value->flags()|Qt::ItemIsEditable);
            int rows = ui->parameterTable->rowCount();
            ui->parameterTable->setRowCount(rows+1);
            ui->parameterTable->setItem(rows, 0, name);
            ui->parameterTable->setItem(rows, 1, value);
            EditableItemDelegate *ed = DelegateFactory::create(p, this);
            value->setData(Qt::EditRole, ed->getDefault());
            ui->parameterTable->setItemDelegateForRow(rows, ed);
            idx += re.matchedLength();
        }
        ui->parameterTable->resizeColumnToContents(0);
    }
}

QString ProjectNewDialog::replaceTemplates(QString text) const
{
    for(int row=0; row<ui->parameterTable->rowCount(); row++) {
        QString key = ui->parameterTable->item(row, 0)->text().replace(' ', '_');
        QString val = ui->parameterTable->item(row, 1)->text();

        QRegExp re(QString(R"(\$\{\{%1\s*([a-zA-Z0-9_]+)*\s*\:*(.*)\}\})").arg(key));
        re.setMinimal(true);
        re.setPatternSyntax(QRegExp::RegExp2);
        text.replace(re, val);
    }
    return text;
}
