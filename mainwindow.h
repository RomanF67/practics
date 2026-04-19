#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QComboBox;
class QDateEdit;
class QLineEdit;
class QSpinBox;
class QTabWidget;
class QTableWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupWindow();
    void setupTabs();

    QWidget *createDepartmentsTab();
    QWidget *createEquipmentTypesTab();
    QWidget *createResponsiblePeopleTab();
    QWidget *createEquipmentItemsTab();
    QWidget *createIssuanceTab();
    bool promptForConnectionSettings();

    bool initializeDatabase();
    bool ensureDatabaseExists();
    bool createTables();
    bool executeQuery(const QString &sql, const QVariantList &values = {});

    void loadDepartments();
    void loadEquipmentTypes();
    void loadResponsiblePeople();
    void loadEquipmentItems();
    void loadIssuanceJournal();
    void refreshCombos();
    void refreshAllData();

    void clearDepartmentInputs();
    void clearEquipmentTypeInputs();
    void clearResponsiblePersonInputs();
    void clearEquipmentItemInputs();
    void clearIssuanceInputs();

    void addDepartment();
    void deleteDepartment();
    void addEquipmentType();
    void deleteEquipmentType();
    void addResponsiblePerson();
    void deleteResponsiblePerson();
    void addEquipmentItem();
    void deleteEquipmentItem();
    void addIssuanceRecord();
    void deleteIssuanceRecord();

    int selectedId(QTableWidget *table) const;
    void showDatabaseError(const QString &action) const;

    Ui::MainWindow *ui;
    QSqlDatabase database;
    QString dbHost;
    int dbPort = 5432;
    QString dbName;
    QString adminDbName;
    QString dbUser;
    QString dbPassword;

    QTabWidget *tabWidget = nullptr;

    QLineEdit *departmentNameEdit = nullptr;
    QTableWidget *departmentsTable = nullptr;

    QLineEdit *equipmentTypeNameEdit = nullptr;
    QLineEdit *equipmentTypeDescriptionEdit = nullptr;
    QTableWidget *equipmentTypesTable = nullptr;

    QLineEdit *responsiblePersonNameEdit = nullptr;
    QLineEdit *responsiblePersonPositionEdit = nullptr;
    QComboBox *responsiblePersonDepartmentCombo = nullptr;
    QTableWidget *responsiblePeopleTable = nullptr;

    QLineEdit *equipmentItemInventoryEdit = nullptr;
    QLineEdit *equipmentItemSerialEdit = nullptr;
    QLineEdit *equipmentItemStatusEdit = nullptr;
    QComboBox *equipmentItemTypeCombo = nullptr;
    QComboBox *equipmentItemDepartmentCombo = nullptr;
    QComboBox *equipmentItemResponsibleCombo = nullptr;
    QTableWidget *equipmentItemsTable = nullptr;

    QComboBox *issuanceEquipmentItemCombo = nullptr;
    QComboBox *issuanceDepartmentCombo = nullptr;
    QComboBox *issuanceResponsibleCombo = nullptr;
    QDateEdit *issuanceDateEdit = nullptr;
    QDateEdit *issuanceReturnDateEdit = nullptr;
    QLineEdit *issuanceNoteEdit = nullptr;
    QTableWidget *issuanceTable = nullptr;
};

#endif // MAINWINDOW_H
