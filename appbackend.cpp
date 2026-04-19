#include "appbackend.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>

namespace {
QString sqlErrorMessage(const QSqlQuery &query)
{
    return query.lastError().text();
}

QString humanizeDatabaseError(const QString &message)
{
    if (message.contains(QStringLiteral("responsible_people_department_id_fkey"), Qt::CaseInsensitive)) {
        return QStringLiteral("Нельзя удалить департамент, пока к нему привязаны ответственные лица.");
    }
    if (message.contains(QStringLiteral("equipment_items_department_id_fkey"), Qt::CaseInsensitive) ||
        message.contains(QStringLiteral("issuance_records_issued_to_department_id_fkey"), Qt::CaseInsensitive)) {
        return QStringLiteral("Нельзя удалить департамент, пока он используется в оборудовании или журнале выдачи.");
    }
    if (message.contains(QStringLiteral("equipment_items_equipment_type_id_fkey"), Qt::CaseInsensitive)) {
        return QStringLiteral("Нельзя удалить тип оборудования, пока существуют связанные карточки оборудования.");
    }
    if (message.contains(QStringLiteral("equipment_items_responsible_person_id_fkey"), Qt::CaseInsensitive) ||
        message.contains(QStringLiteral("issuance_records_issued_by_person_id_fkey"), Qt::CaseInsensitive)) {
        return QStringLiteral("Нельзя удалить ответственное лицо, пока оно используется в оборудовании или журнале выдачи.");
    }
    if (message.contains(QStringLiteral("issuance_records_equipment_item_id_fkey"), Qt::CaseInsensitive)) {
        return QStringLiteral("Нельзя удалить единицу оборудования, пока по ней есть записи в журнале выдачи.");
    }
    if (message.contains(QStringLiteral("duplicate key"), Qt::CaseInsensitive) ||
        message.contains(QStringLiteral("23505"), Qt::CaseInsensitive)) {
        return QStringLiteral("Такая запись уже существует. Проверьте уникальные поля.");
    }
    return message;
}
}

AppBackend::AppBackend(QObject *parent)
    : QObject(parent)
    , connectionName(QStringLiteral("equipment_connection"))
{
}

AppBackend::~AppBackend()
{
    disconnectDatabase();
}

bool AppBackend::connected() const
{
    return databaseConnected;
}

QString AppBackend::lastError() const
{
    return errorMessage;
}

QVariantList AppBackend::departments() const
{
    return departmentItems;
}

QVariantList AppBackend::equipmentTypes() const
{
    return equipmentTypeItems;
}

QVariantList AppBackend::responsiblePeople() const
{
    return responsiblePersonItems;
}

QVariantList AppBackend::equipmentItems() const
{
    return equipmentItemItems;
}

QVariantList AppBackend::issuanceRecords() const
{
    return issuanceRecordItems;
}

bool AppBackend::connectToDatabase(const QString &host,int port,const QString &databaseName,
                    const QString &adminDatabaseName,const QString &user,const QString &password)
{
    disconnectDatabase();
    clearLastError();
    if (!ensureDatabaseExists(host, port, databaseName, adminDatabaseName, user, password)) {
        return false;
    }
    database = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), connectionName);
    database.setHostName(host);
    database.setPort(port);
    database.setDatabaseName(databaseName);
    database.setUserName(user);
    database.setPassword(password);
    if (!database.open()) {
        setLastError(QStringLiteral("Не удалось подключиться к базе данных.\n") + database.lastError().text());
        return false;
    }
    if (!createTables()) {
        disconnectDatabase();
        return false;
    }
    databaseConnected = true;
    emit connectedChanged();
    refreshAll();
    return true;
}

void AppBackend::disconnectDatabase()
{
    if (database.isValid()) {
        if (database.isOpen()) {
            database.close();
        }
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    }

    if (databaseConnected) {
        databaseConnected = false;
        emit connectedChanged();
    }
}

void AppBackend::refreshAll()
{
    if (!databaseConnected) {
        return;
    }

    loadDepartments();
    loadEquipmentTypes();
    loadResponsiblePeople();
    loadEquipmentItems();
    loadIssuanceRecords();
}

bool AppBackend::addDepartment(const QString &name)
{
    if (name.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Введите название департамента."));
        return false;
    }
    if (!executeQuery(QStringLiteral("INSERT INTO departments(name) VALUES (?)"), {name.trimmed()})) {
        return false;
    }
    loadDepartments();
    loadResponsiblePeople();
    loadEquipmentItems();
    loadIssuanceRecords();
    return true;
}

bool AppBackend::deleteDepartment(int id)
{
    if (!executeQuery(QStringLiteral("DELETE FROM departments WHERE id = ?"), {id})) {
        return false;
    }
    refreshAll();
    return true;
}

bool AppBackend::addEquipmentType(const QString &name, const QString &description)
{
    if (name.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Введите название типа оборудования."));
        return false;
    }
    if (!executeQuery(QStringLiteral("INSERT INTO equipment_types(name, description) VALUES (?, ?)"),
                      {name.trimmed(), description.trimmed()})) {
        return false;
    }
    loadEquipmentTypes();
    loadEquipmentItems();
    loadIssuanceRecords();
    return true;
}

bool AppBackend::deleteEquipmentType(int id)
{
    if (!executeQuery(QStringLiteral("DELETE FROM equipment_types WHERE id = ?"), {id})) {
        return false;
    }
    refreshAll();
    return true;
}

bool AppBackend::addResponsiblePerson(const QString &fullName, const QString &position, int departmentId)
{
    if (fullName.trimmed().isEmpty() || position.trimmed().isEmpty() || departmentId <= 0) {
        setLastError(QStringLiteral("Заполните ФИО, должность и департамент."));
        return false;
    }
    if (!executeQuery(
            QStringLiteral("INSERT INTO responsible_people(full_name, position, department_id) VALUES (?, ?, ?)"),
            {fullName.trimmed(), position.trimmed(), departmentId})) {
        return false;
    }
    loadResponsiblePeople();
    loadEquipmentItems();
    loadIssuanceRecords();
    return true;
}

bool AppBackend::deleteResponsiblePerson(int id)
{
    if (!executeQuery(QStringLiteral("DELETE FROM responsible_people WHERE id = ?"), {id})) {
        return false;
    }
    refreshAll();
    return true;
}

bool AppBackend::addEquipmentItem(const QString &inventoryNumber,
                                  const QString &serialNumber,
                                  int equipmentTypeId,
                                  int departmentId,
                                  int responsiblePersonId,
                                  const QString &status)
{
    if (inventoryNumber.trimmed().isEmpty() || serialNumber.trimmed().isEmpty() || status.trimmed().isEmpty() ||
        equipmentTypeId <= 0 || departmentId <= 0 || responsiblePersonId <= 0) {
        setLastError(QStringLiteral("Заполните все поля оборудования."));
        return false;
    }

    if (!executeQuery(
            QStringLiteral("INSERT INTO equipment_items("
                           "inventory_number, serial_number, equipment_type_id, department_id, responsible_person_id, status)"
                           " VALUES (?, ?, ?, ?, ?, ?)"),
            {inventoryNumber.trimmed(),
             serialNumber.trimmed(),
             equipmentTypeId,
             departmentId,
             responsiblePersonId,
             status.trimmed()})) {
        return false;
    }
    loadEquipmentItems();
    loadIssuanceRecords();
    return true;
}

bool AppBackend::deleteEquipmentItem(int id)
{
    if (!executeQuery(QStringLiteral("DELETE FROM equipment_items WHERE id = ?"), {id})) {
        return false;
    }
    loadEquipmentItems();
    loadIssuanceRecords();
    return true;
}

bool AppBackend::addIssuanceRecord(int equipmentItemId,
                                   int issuedToDepartmentId,
                                   int issuedByPersonId,
                                   const QString &issuedAt,
                                   const QString &returnDueDate,
                                   const QString &note)
{
    if (equipmentItemId <= 0 || issuedToDepartmentId <= 0 || issuedByPersonId <= 0) {
        setLastError(QStringLiteral("Выберите оборудование, департамент и ответственного."));
        return false;
    }

    const QDate issueDate = QDate::fromString(issuedAt, Qt::ISODate);
    const QDate dueDate = QDate::fromString(returnDueDate, Qt::ISODate);
    if (!issueDate.isValid() || !dueDate.isValid() || dueDate < issueDate) {
        setLastError(QStringLiteral("Проверьте дату выдачи и срок возврата."));
        return false;
    }

    if (!executeQuery(
            QStringLiteral("INSERT INTO issuance_records("
                           "equipment_item_id, issued_to_department_id, issued_by_person_id, issued_at, return_due_date, note)"
                           " VALUES (?, ?, ?, ?, ?, ?)"),
            {equipmentItemId, issuedToDepartmentId, issuedByPersonId, issueDate, dueDate, note.trimmed()})) {
        return false;
    }
    loadIssuanceRecords();
    return true;
}

bool AppBackend::deleteIssuanceRecord(int id)
{
    if (!executeQuery(QStringLiteral("DELETE FROM issuance_records WHERE id = ?"), {id})) {
        return false;
    }
    loadIssuanceRecords();
    return true;
}

bool AppBackend::ensureDatabaseExists(const QString &host,
                                      int port,
                                      const QString &databaseName,
                                      const QString &adminDatabaseName,
                                      const QString &user,
                                      const QString &password)
{
    const QString bootstrapConnectionName = QStringLiteral("bootstrap_connection");
    {
        QSqlDatabase bootstrap = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), bootstrapConnectionName);
        bootstrap.setHostName(host);
        bootstrap.setPort(port);
        bootstrap.setDatabaseName(adminDatabaseName);
        bootstrap.setUserName(user);
        bootstrap.setPassword(password);

        if (!bootstrap.open()) {
            setLastError(QStringLiteral("Не удалось подключиться к серверу PostgreSQL.\n") + bootstrap.lastError().text());
            return false;
        }

        QSqlQuery checkQuery(bootstrap);
        checkQuery.prepare(QStringLiteral("SELECT 1 FROM pg_database WHERE datname = ?"));
        checkQuery.addBindValue(databaseName);
        if (!checkQuery.exec()) {
            setLastError(QStringLiteral("Не удалось проверить существование базы данных.\n") + sqlErrorMessage(checkQuery));
            bootstrap.close();
            QSqlDatabase::removeDatabase(bootstrapConnectionName);
            return false;
        }

        if (!checkQuery.next()) {
            QSqlQuery createQuery(bootstrap);
            const QString sql = QStringLiteral("CREATE DATABASE \"%1\"").arg(databaseName);
            if (!createQuery.exec(sql)) {
                setLastError(QStringLiteral("Не удалось создать базу данных.\n") + sqlErrorMessage(createQuery));
                bootstrap.close();
                QSqlDatabase::removeDatabase(bootstrapConnectionName);
                return false;
            }
        }

        bootstrap.close();
    }

    QSqlDatabase::removeDatabase(bootstrapConnectionName);
    return true;
}

bool AppBackend::createTables()
{
    const QStringList queries = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS departments ("
                       "id SERIAL PRIMARY KEY,"
                       "name VARCHAR(150) NOT NULL UNIQUE)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS equipment_types ("
                       "id SERIAL PRIMARY KEY,"
                       "name VARCHAR(150) NOT NULL UNIQUE,"
                       "description TEXT NOT NULL DEFAULT '')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS responsible_people ("
                       "id SERIAL PRIMARY KEY,"
                       "full_name VARCHAR(200) NOT NULL,"
                       "position VARCHAR(150) NOT NULL,"
                       "department_id INTEGER NOT NULL REFERENCES departments(id) ON DELETE RESTRICT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS equipment_items ("
                       "id SERIAL PRIMARY KEY,"
                       "inventory_number VARCHAR(100) NOT NULL UNIQUE,"
                       "serial_number VARCHAR(100) NOT NULL,"
                       "equipment_type_id INTEGER NOT NULL REFERENCES equipment_types(id) ON DELETE RESTRICT,"
                       "department_id INTEGER NOT NULL REFERENCES departments(id) ON DELETE RESTRICT,"
                       "responsible_person_id INTEGER NOT NULL REFERENCES responsible_people(id) ON DELETE RESTRICT,"
                       "status VARCHAR(100) NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS issuance_records ("
                       "id SERIAL PRIMARY KEY,"
                       "equipment_item_id INTEGER NOT NULL REFERENCES equipment_items(id) ON DELETE RESTRICT,"
                       "issued_to_department_id INTEGER NOT NULL REFERENCES departments(id) ON DELETE RESTRICT,"
                       "issued_by_person_id INTEGER NOT NULL REFERENCES responsible_people(id) ON DELETE RESTRICT,"
                       "issued_at DATE NOT NULL,"
                       "return_due_date DATE NOT NULL,"
                       "note TEXT NOT NULL DEFAULT '')")
    };

    for (const QString &sql : queries) {
        if (!executeQuery(sql)) {
            return false;
        }
    }

    return true;
}

bool AppBackend::executeQuery(const QString &sql, const QVariantList &values)
{
    clearLastError();
    QSqlQuery query(database);
    query.prepare(sql);
    for (const QVariant &value : values) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        setLastError(humanizeDatabaseError(sqlErrorMessage(query)));
        return false;
    }

    return true;
}

QVariantList AppBackend::queryToVariantList(const QString &sql, const QStringList &keys) const
{
    QVariantList result;
    QSqlQuery query(database);
    query.exec(sql);
    while (query.next()) {
        QVariantMap item;
        for (int i = 0; i < keys.size(); ++i) {
            item.insert(keys.at(i), query.value(i));
        }
        result.append(item);
    }
    return result;
}

void AppBackend::setLastError(const QString &message)
{
    if (errorMessage == message) {
        return;
    }
    errorMessage = message;
    emit lastErrorChanged();
}

void AppBackend::clearLastError()
{
    if (errorMessage.isEmpty()) {
        return;
    }
    errorMessage.clear();
    emit lastErrorChanged();
}

void AppBackend::loadDepartments()
{
    departmentItems = queryToVariantList(QStringLiteral("SELECT id, name FROM departments ORDER BY name"),
                                         {QStringLiteral("id"), QStringLiteral("name")});
    emit departmentsChanged();
}

void AppBackend::loadEquipmentTypes()
{
    equipmentTypeItems = queryToVariantList(
        QStringLiteral("SELECT id, name, description FROM equipment_types ORDER BY name"),
        {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("description")});
    emit equipmentTypesChanged();
}

void AppBackend::loadResponsiblePeople()
{
    responsiblePersonItems = queryToVariantList(
        QStringLiteral("SELECT rp.id, rp.full_name, rp.position, d.name, rp.department_id "
                       "FROM responsible_people rp "
                       "JOIN departments d ON d.id = rp.department_id "
                       "ORDER BY rp.full_name"),
        {QStringLiteral("id"),
         QStringLiteral("full_name"),
         QStringLiteral("position"),
         QStringLiteral("department_name"),
         QStringLiteral("department_id")});
    emit responsiblePeopleChanged();
}

void AppBackend::loadEquipmentItems()
{
    equipmentItemItems = queryToVariantList(
        QStringLiteral("SELECT ei.id, ei.inventory_number, ei.serial_number, et.name, d.name, rp.full_name, ei.status "
                       "FROM equipment_items ei "
                       "JOIN equipment_types et ON et.id = ei.equipment_type_id "
                       "JOIN departments d ON d.id = ei.department_id "
                       "JOIN responsible_people rp ON rp.id = ei.responsible_person_id "
                       "ORDER BY ei.inventory_number"),
        {QStringLiteral("id"),
         QStringLiteral("inventory_number"),
         QStringLiteral("serial_number"),
         QStringLiteral("type_name"),
         QStringLiteral("department_name"),
         QStringLiteral("responsible_name"),
         QStringLiteral("status")});
    emit equipmentItemsChanged();
}

void AppBackend::loadIssuanceRecords()
{
    issuanceRecordItems = queryToVariantList(
        QStringLiteral("SELECT ir.id, ir.issued_at, ir.return_due_date, ei.inventory_number, et.name, d.name, rp.full_name, ir.note "
                       "FROM issuance_records ir "
                       "JOIN equipment_items ei ON ei.id = ir.equipment_item_id "
                       "JOIN equipment_types et ON et.id = ei.equipment_type_id "
                       "JOIN departments d ON d.id = ir.issued_to_department_id "
                       "JOIN responsible_people rp ON rp.id = ir.issued_by_person_id "
                       "ORDER BY ir.issued_at DESC, ir.id DESC"),
        {QStringLiteral("id"),
         QStringLiteral("issued_at"),
         QStringLiteral("return_due_date"),
         QStringLiteral("inventory_number"),
         QStringLiteral("type_name"),
         QStringLiteral("department_name"),
         QStringLiteral("issued_by"),
         QStringLiteral("note")});
    emit issuanceRecordsChanged();
}
