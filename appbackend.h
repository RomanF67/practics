#ifndef APPBACKEND_H
#define APPBACKEND_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>

class AppBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantList departments READ departments NOTIFY departmentsChanged)
    Q_PROPERTY(QVariantList equipmentTypes READ equipmentTypes NOTIFY equipmentTypesChanged)
    Q_PROPERTY(QVariantList responsiblePeople READ responsiblePeople NOTIFY responsiblePeopleChanged)
    Q_PROPERTY(QVariantList equipmentItems READ equipmentItems NOTIFY equipmentItemsChanged)
    Q_PROPERTY(QVariantList issuanceRecords READ issuanceRecords NOTIFY issuanceRecordsChanged)

public:
    explicit AppBackend(QObject *parent = nullptr);
    ~AppBackend() override;

    bool connected() const;
    QString lastError() const;
    QVariantList departments() const;
    QVariantList equipmentTypes() const;
    QVariantList responsiblePeople() const;
    QVariantList equipmentItems() const;
    QVariantList issuanceRecords() const;

    Q_INVOKABLE bool connectToDatabase(const QString &host,
                                       int port,
                                       const QString &databaseName,
                                       const QString &adminDatabaseName,
                                       const QString &user,
                                       const QString &password);
    Q_INVOKABLE void disconnectDatabase();
    Q_INVOKABLE void refreshAll();

    Q_INVOKABLE bool addDepartment(const QString &name);
    Q_INVOKABLE bool deleteDepartment(int id);

    Q_INVOKABLE bool addEquipmentType(const QString &name, const QString &description);
    Q_INVOKABLE bool deleteEquipmentType(int id);

    Q_INVOKABLE bool addResponsiblePerson(const QString &fullName, const QString &position, int departmentId);
    Q_INVOKABLE bool deleteResponsiblePerson(int id);

    Q_INVOKABLE bool addEquipmentItem(const QString &inventoryNumber,
                                      const QString &serialNumber,
                                      int equipmentTypeId,
                                      int departmentId,
                                      int responsiblePersonId,
                                      const QString &status);
    Q_INVOKABLE bool deleteEquipmentItem(int id);

    Q_INVOKABLE bool addIssuanceRecord(int equipmentItemId,
                                       int issuedToDepartmentId,
                                       int issuedByPersonId,
                                       const QString &issuedAt,
                                       const QString &returnDueDate,
                                       const QString &note);
    Q_INVOKABLE bool deleteIssuanceRecord(int id);

signals:
    void connectedChanged();
    void lastErrorChanged();
    void departmentsChanged();
    void equipmentTypesChanged();
    void responsiblePeopleChanged();
    void equipmentItemsChanged();
    void issuanceRecordsChanged();

private:
    bool ensureDatabaseExists(const QString &host,
                              int port,
                              const QString &databaseName,
                              const QString &adminDatabaseName,
                              const QString &user,
                              const QString &password);
    bool createTables();
    bool executeQuery(const QString &sql, const QVariantList &values = {});
    QVariantList queryToVariantList(const QString &sql, const QStringList &keys) const;
    void setLastError(const QString &message);
    void clearLastError();
    void loadDepartments();
    void loadEquipmentTypes();
    void loadResponsiblePeople();
    void loadEquipmentItems();
    void loadIssuanceRecords();

    QString connectionName;
    QSqlDatabase database;
    bool databaseConnected = false;
    QString errorMessage;

    QVariantList departmentItems;
    QVariantList equipmentTypeItems;
    QVariantList responsiblePersonItems;
    QVariantList equipmentItemItems;
    QVariantList issuanceRecordItems;
};

#endif // APPBACKEND_H
