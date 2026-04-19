#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVariantList>
#include <QVBoxLayout>

namespace {
QWidget *buildCrudTab(const QString &title,
                      QFormLayout *formLayout,
                      QTableWidget *table,
                      QPushButton *addButton,
                      QPushButton *deleteButton)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);

    auto *titleLabel = new QLabel(title);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *buttonsLayout = new QHBoxLayout;
    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(deleteButton);
    buttonsLayout->addStretch();

    layout->addWidget(titleLabel);
    layout->addLayout(formLayout);
    layout->addLayout(buttonsLayout);
    layout->addWidget(table);
    return widget;
}

void configureTable(QTableWidget *table, const QStringList &headers)
{
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , dbHost(QStringLiteral("127.0.0.1"))
    , dbPort(5433)
    , dbName(QStringLiteral("equipment_accounting"))
    , adminDbName(QStringLiteral("postgres"))
    , dbUser(QStringLiteral("postgres"))
{
    ui->setupUi(this);
    setupWindow();
    setupTabs();

    if (!promptForConnectionSettings()) {
        close();
        return;
    }

    if (!initializeDatabase() || !createTables()) {
        QMessageBox::critical(
            this,
            tr("Ошибка базы данных"),
            tr("Не удалось инициализировать PostgreSQL.\nПроверьте доступность сервера и параметры подключения."));
        close();
        return;
    }

    refreshAllData();
    clearDepartmentInputs();
    clearEquipmentTypeInputs();
    clearResponsiblePersonInputs();
    clearEquipmentItemInputs();
    clearIssuanceInputs();
}

MainWindow::~MainWindow()
{
    if (database.isOpen()) {
        database.close();
    }
    delete ui;
}

void MainWindow::setupWindow()
{
    setWindowTitle(tr("Учет компьютерного оборудования"));
    resize(1360, 820);
}

void MainWindow::setupTabs()
{
    tabWidget = new QTabWidget(this);
    tabWidget->addTab(createDepartmentsTab(), tr("Департаменты"));
    tabWidget->addTab(createEquipmentTypesTab(), tr("Типы оборудования"));
    tabWidget->addTab(createResponsiblePeopleTab(), tr("Ответственные лица"));
    tabWidget->addTab(createEquipmentItemsTab(), tr("Единицы оборудования"));
    tabWidget->addTab(createIssuanceTab(), tr("Журнал выдачи"));
    setCentralWidget(tabWidget);
    statusBar()->showMessage(tr("Готово"));
}

bool MainWindow::promptForConnectionSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Подключение к PostgreSQL"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *formLayout = new QFormLayout;

    auto *hostEdit = new QLineEdit(dbHost, &dialog);
    auto *portSpin = new QSpinBox(&dialog);
    portSpin->setRange(1, 65535);
    portSpin->setValue(dbPort);
    auto *databaseEdit = new QLineEdit(dbName, &dialog);
    auto *adminDatabaseEdit = new QLineEdit(adminDbName, &dialog);
    auto *userEdit = new QLineEdit(dbUser, &dialog);
    auto *passwordEdit = new QLineEdit(dbPassword, &dialog);
    passwordEdit->setEchoMode(QLineEdit::Password);

    formLayout->addRow(tr("Хост:"), hostEdit);
    formLayout->addRow(tr("Порт:"), portSpin);
    formLayout->addRow(tr("База приложения:"), databaseEdit);
    formLayout->addRow(tr("Служебная база:"), adminDatabaseEdit);
    formLayout->addRow(tr("Пользователь:"), userEdit);
    formLayout->addRow(tr("Пароль:"), passwordEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Подключиться"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    layout->addLayout(formLayout);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    dbHost = hostEdit->text().trimmed();
    dbPort = portSpin->value();
    dbName = databaseEdit->text().trimmed();
    adminDbName = adminDatabaseEdit->text().trimmed();
    dbUser = userEdit->text().trimmed();
    dbPassword = passwordEdit->text();

    return !dbHost.isEmpty() && !dbName.isEmpty() && !adminDbName.isEmpty() && !dbUser.isEmpty();
}

QWidget *MainWindow::createDepartmentsTab()
{
    departmentNameEdit = new QLineEdit;

    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("Название департамента:"), departmentNameEdit);

    departmentsTable = new QTableWidget;
    configureTable(departmentsTable, {tr("ID"), tr("Департамент")});
    departmentsTable->setColumnHidden(0, true);

    auto *addButton = new QPushButton(tr("Добавить"));
    auto *deleteButton = new QPushButton(tr("Удалить выбранный"));
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addDepartment);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteDepartment);

    return buildCrudTab(tr("Справочник департаментов"), formLayout, departmentsTable, addButton, deleteButton);
}

QWidget *MainWindow::createEquipmentTypesTab()
{
    equipmentTypeNameEdit = new QLineEdit;
    equipmentTypeDescriptionEdit = new QLineEdit;

    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("Название типа:"), equipmentTypeNameEdit);
    formLayout->addRow(tr("Описание:"), equipmentTypeDescriptionEdit);

    equipmentTypesTable = new QTableWidget;
    configureTable(equipmentTypesTable, {tr("ID"), tr("Тип"), tr("Описание")});
    equipmentTypesTable->setColumnHidden(0, true);

    auto *addButton = new QPushButton(tr("Добавить"));
    auto *deleteButton = new QPushButton(tr("Удалить выбранный"));
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addEquipmentType);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteEquipmentType);

    return buildCrudTab(tr("Справочник типов оборудования"), formLayout, equipmentTypesTable, addButton, deleteButton);
}

QWidget *MainWindow::createResponsiblePeopleTab()
{
    responsiblePersonNameEdit = new QLineEdit;
    responsiblePersonPositionEdit = new QLineEdit;
    responsiblePersonDepartmentCombo = new QComboBox;

    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("ФИО:"), responsiblePersonNameEdit);
    formLayout->addRow(tr("Должность:"), responsiblePersonPositionEdit);
    formLayout->addRow(tr("Департамент:"), responsiblePersonDepartmentCombo);

    responsiblePeopleTable = new QTableWidget;
    configureTable(responsiblePeopleTable,
                   {tr("ID"), tr("ФИО"), tr("Должность"), tr("Департамент")});
    responsiblePeopleTable->setColumnHidden(0, true);

    auto *addButton = new QPushButton(tr("Добавить"));
    auto *deleteButton = new QPushButton(tr("Удалить выбранного"));
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addResponsiblePerson);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteResponsiblePerson);

    return buildCrudTab(tr("Справочник материально ответственных лиц"),
                        formLayout,
                        responsiblePeopleTable,
                        addButton,
                        deleteButton);
}

QWidget *MainWindow::createEquipmentItemsTab()
{
    equipmentItemInventoryEdit = new QLineEdit;
    equipmentItemSerialEdit = new QLineEdit;
    equipmentItemStatusEdit = new QLineEdit;
    equipmentItemTypeCombo = new QComboBox;
    equipmentItemDepartmentCombo = new QComboBox;
    equipmentItemResponsibleCombo = new QComboBox;

    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("Инвентарный номер:"), equipmentItemInventoryEdit);
    formLayout->addRow(tr("Серийный номер:"), equipmentItemSerialEdit);
    formLayout->addRow(tr("Тип оборудования:"), equipmentItemTypeCombo);
    formLayout->addRow(tr("Департамент:"), equipmentItemDepartmentCombo);
    formLayout->addRow(tr("Ответственное лицо:"), equipmentItemResponsibleCombo);
    formLayout->addRow(tr("Статус:"), equipmentItemStatusEdit);

    equipmentItemsTable = new QTableWidget;
    configureTable(equipmentItemsTable,
                   {tr("ID"),
                    tr("Инв. номер"),
                    tr("Серийный номер"),
                    tr("Тип"),
                    tr("Департамент"),
                    tr("Ответственный"),
                    tr("Статус")});
    equipmentItemsTable->setColumnHidden(0, true);

    auto *addButton = new QPushButton(tr("Добавить"));
    auto *deleteButton = new QPushButton(tr("Удалить выбранную"));
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addEquipmentItem);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteEquipmentItem);

    return buildCrudTab(tr("Реестр единиц оборудования"),
                        formLayout,
                        equipmentItemsTable,
                        addButton,
                        deleteButton);
}

QWidget *MainWindow::createIssuanceTab()
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);

    auto *titleLabel = new QLabel(tr("Журнал регистрации выдачи оборудования"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    issuanceEquipmentItemCombo = new QComboBox;
    issuanceDepartmentCombo = new QComboBox;
    issuanceResponsibleCombo = new QComboBox;
    issuanceDateEdit = new QDateEdit(QDate::currentDate());
    issuanceReturnDateEdit = new QDateEdit(QDate::currentDate().addDays(14));
    issuanceNoteEdit = new QLineEdit;

    issuanceDateEdit->setCalendarPopup(true);
    issuanceDateEdit->setDisplayFormat("dd.MM.yyyy");
    issuanceReturnDateEdit->setCalendarPopup(true);
    issuanceReturnDateEdit->setDisplayFormat("dd.MM.yyyy");

    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("Единица оборудования:"), issuanceEquipmentItemCombo);
    formLayout->addRow(tr("Выдано в департамент:"), issuanceDepartmentCombo);
    formLayout->addRow(tr("Выдал:"), issuanceResponsibleCombo);
    formLayout->addRow(tr("Дата выдачи:"), issuanceDateEdit);
    formLayout->addRow(tr("Срок возврата:"), issuanceReturnDateEdit);
    formLayout->addRow(tr("Примечание:"), issuanceNoteEdit);

    auto *buttonsLayout = new QHBoxLayout;
    auto *addButton = new QPushButton(tr("Зарегистрировать выдачу"));
    auto *deleteButton = new QPushButton(tr("Удалить выбранную"));
    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(deleteButton);
    buttonsLayout->addStretch();
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addIssuanceRecord);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteIssuanceRecord);

    issuanceTable = new QTableWidget;
    configureTable(issuanceTable,
                   {tr("ID"),
                    tr("Дата выдачи"),
                    tr("Срок возврата"),
                    tr("Инв. номер"),
                    tr("Тип"),
                    tr("Кому выдано"),
                    tr("Кто выдал"),
                    tr("Примечание")});
    issuanceTable->setColumnHidden(0, true);

    layout->addWidget(titleLabel);
    layout->addLayout(formLayout);
    layout->addLayout(buttonsLayout);
    layout->addWidget(issuanceTable);
    return widget;
}

bool MainWindow::initializeDatabase()
{
    if (!ensureDatabaseExists()) {
        return false;
    }

    database = QSqlDatabase::addDatabase("QPSQL");
    database.setHostName(dbHost);
    database.setPort(dbPort);
    database.setDatabaseName(dbName);
    database.setUserName(dbUser);
    database.setPassword(dbPassword);

    if (!database.open()) {
        showDatabaseError(tr("подключиться к базе PostgreSQL"));
        return false;
    }

    return true;
}

bool MainWindow::ensureDatabaseExists()
{
    const QString connectionName = QStringLiteral("bootstrap_connection");
    {
        QSqlDatabase bootstrap = QSqlDatabase::addDatabase("QPSQL", connectionName);
        bootstrap.setHostName(dbHost);
        bootstrap.setPort(dbPort);
        bootstrap.setDatabaseName(adminDbName);
        bootstrap.setUserName(dbUser);
        bootstrap.setPassword(dbPassword);

        if (!bootstrap.open()) {
            QMessageBox::critical(this,
                                  tr("Ошибка PostgreSQL"),
                                  tr("Не удалось подключиться к серверу базы данных %1:%2.\n%3")
                                      .arg(dbHost)
                                      .arg(dbPort)
                                      .arg(bootstrap.lastError().text()));
            return false;
        }

        QSqlQuery checkQuery(bootstrap);
        checkQuery.prepare("SELECT 1 FROM pg_database WHERE datname = ?");
        checkQuery.addBindValue(dbName);
        if (!checkQuery.exec()) {
            QMessageBox::critical(this,
                                  tr("Ошибка PostgreSQL"),
                                  tr("Не удалось проверить существование базы данных.\n%1")
                                      .arg(checkQuery.lastError().text()));
            bootstrap.close();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }

        if (!checkQuery.next()) {
            QSqlQuery createQuery(bootstrap);
            const QString sql = QString("CREATE DATABASE \"%1\"").arg(dbName);
            if (!createQuery.exec(sql)) {
                QMessageBox::critical(this,
                                      tr("Ошибка PostgreSQL"),
                                      tr("Не удалось создать базу данных %1.\n%2")
                                          .arg(dbName, createQuery.lastError().text()));
                bootstrap.close();
                QSqlDatabase::removeDatabase(connectionName);
                return false;
            }
        }

        bootstrap.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
    return true;
}

bool MainWindow::createTables()
{
    const QStringList queries = {
        "CREATE TABLE IF NOT EXISTS departments ("
        "id SERIAL PRIMARY KEY,"
        "name VARCHAR(150) NOT NULL UNIQUE)",

        "CREATE TABLE IF NOT EXISTS equipment_types ("
        "id SERIAL PRIMARY KEY,"
        "name VARCHAR(150) NOT NULL UNIQUE,"
        "description TEXT NOT NULL DEFAULT '')",

        "CREATE TABLE IF NOT EXISTS responsible_people ("
        "id SERIAL PRIMARY KEY,"
        "full_name VARCHAR(200) NOT NULL,"
        "position VARCHAR(150) NOT NULL,"
        "department_id INTEGER NOT NULL REFERENCES departments(id) ON DELETE RESTRICT)",

        "CREATE TABLE IF NOT EXISTS equipment_items ("
        "id SERIAL PRIMARY KEY,"
        "inventory_number VARCHAR(100) NOT NULL UNIQUE,"
        "serial_number VARCHAR(100) NOT NULL,"
        "equipment_type_id INTEGER NOT NULL REFERENCES equipment_types(id) ON DELETE RESTRICT,"
        "department_id INTEGER NOT NULL REFERENCES departments(id) ON DELETE RESTRICT,"
        "responsible_person_id INTEGER NOT NULL REFERENCES responsible_people(id) ON DELETE RESTRICT,"
        "status VARCHAR(100) NOT NULL)",

        "CREATE TABLE IF NOT EXISTS issuance_records ("
        "id SERIAL PRIMARY KEY,"
        "equipment_item_id INTEGER NOT NULL REFERENCES equipment_items(id) ON DELETE RESTRICT,"
        "issued_to_department_id INTEGER NOT NULL REFERENCES departments(id) ON DELETE RESTRICT,"
        "issued_by_person_id INTEGER NOT NULL REFERENCES responsible_people(id) ON DELETE RESTRICT,"
        "issued_at DATE NOT NULL,"
        "return_due_date DATE NOT NULL,"
        "note TEXT NOT NULL DEFAULT '')"
    };

    for (const QString &sql : queries) {
        if (!executeQuery(sql)) {
            return false;
        }
    }
    return true;
}

bool MainWindow::executeQuery(const QString &sql, const QVariantList &values)
{
    QSqlQuery query(database);
    query.prepare(sql);
    for (const QVariant &value : values) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        showDatabaseError(sql);
        return false;
    }
    return true;
}

void MainWindow::loadDepartments()
{
    departmentsTable->setRowCount(0);
    QSqlQuery query("SELECT id, name FROM departments ORDER BY name", database);
    while (query.next()) {
        const int row = departmentsTable->rowCount();
        departmentsTable->insertRow(row);
        departmentsTable->setItem(row, 0, new QTableWidgetItem(query.value(0).toString()));
        departmentsTable->setItem(row, 1, new QTableWidgetItem(query.value(1).toString()));
    }
}

void MainWindow::loadEquipmentTypes()
{
    equipmentTypesTable->setRowCount(0);
    QSqlQuery query("SELECT id, name, description FROM equipment_types ORDER BY name", database);
    while (query.next()) {
        const int row = equipmentTypesTable->rowCount();
        equipmentTypesTable->insertRow(row);
        equipmentTypesTable->setItem(row, 0, new QTableWidgetItem(query.value(0).toString()));
        equipmentTypesTable->setItem(row, 1, new QTableWidgetItem(query.value(1).toString()));
        equipmentTypesTable->setItem(row, 2, new QTableWidgetItem(query.value(2).toString()));
    }
}

void MainWindow::loadResponsiblePeople()
{
    responsiblePeopleTable->setRowCount(0);
    const QString sql =
        "SELECT rp.id, rp.full_name, rp.position, d.name "
        "FROM responsible_people rp "
        "JOIN departments d ON d.id = rp.department_id "
        "ORDER BY rp.full_name";

    QSqlQuery query(sql, database);
    while (query.next()) {
        const int row = responsiblePeopleTable->rowCount();
        responsiblePeopleTable->insertRow(row);
        for (int column = 0; column < 4; ++column) {
            responsiblePeopleTable->setItem(row, column, new QTableWidgetItem(query.value(column).toString()));
        }
    }
}

void MainWindow::loadEquipmentItems()
{
    equipmentItemsTable->setRowCount(0);
    const QString sql =
        "SELECT ei.id, ei.inventory_number, ei.serial_number, et.name, d.name, rp.full_name, ei.status "
        "FROM equipment_items ei "
        "JOIN equipment_types et ON et.id = ei.equipment_type_id "
        "JOIN departments d ON d.id = ei.department_id "
        "JOIN responsible_people rp ON rp.id = ei.responsible_person_id "
        "ORDER BY ei.inventory_number";

    QSqlQuery query(sql, database);
    while (query.next()) {
        const int row = equipmentItemsTable->rowCount();
        equipmentItemsTable->insertRow(row);
        for (int column = 0; column < 7; ++column) {
            equipmentItemsTable->setItem(row, column, new QTableWidgetItem(query.value(column).toString()));
        }
    }
}

void MainWindow::loadIssuanceJournal()
{
    issuanceTable->setRowCount(0);
    const QString sql =
        "SELECT ir.id, ir.issued_at, ir.return_due_date, ei.inventory_number, et.name, d.name, rp.full_name, ir.note "
        "FROM issuance_records ir "
        "JOIN equipment_items ei ON ei.id = ir.equipment_item_id "
        "JOIN equipment_types et ON et.id = ei.equipment_type_id "
        "JOIN departments d ON d.id = ir.issued_to_department_id "
        "JOIN responsible_people rp ON rp.id = ir.issued_by_person_id "
        "ORDER BY ir.issued_at DESC, ir.id DESC";

    QSqlQuery query(sql, database);
    while (query.next()) {
        const int row = issuanceTable->rowCount();
        issuanceTable->insertRow(row);
        for (int column = 0; column < 8; ++column) {
            issuanceTable->setItem(row, column, new QTableWidgetItem(query.value(column).toString()));
        }
    }
}

void MainWindow::refreshCombos()
{
    responsiblePersonDepartmentCombo->clear();
    equipmentItemTypeCombo->clear();
    equipmentItemDepartmentCombo->clear();
    equipmentItemResponsibleCombo->clear();
    issuanceEquipmentItemCombo->clear();
    issuanceDepartmentCombo->clear();
    issuanceResponsibleCombo->clear();

    QSqlQuery departmentsQuery("SELECT id, name FROM departments ORDER BY name", database);
    while (departmentsQuery.next()) {
        responsiblePersonDepartmentCombo->addItem(departmentsQuery.value(1).toString(),
                                                  departmentsQuery.value(0));
        equipmentItemDepartmentCombo->addItem(departmentsQuery.value(1).toString(),
                                              departmentsQuery.value(0));
        issuanceDepartmentCombo->addItem(departmentsQuery.value(1).toString(),
                                         departmentsQuery.value(0));
    }

    QSqlQuery equipmentTypesQuery("SELECT id, name FROM equipment_types ORDER BY name", database);
    while (equipmentTypesQuery.next()) {
        equipmentItemTypeCombo->addItem(equipmentTypesQuery.value(1).toString(),
                                        equipmentTypesQuery.value(0));
    }

    QSqlQuery peopleQuery("SELECT id, full_name FROM responsible_people ORDER BY full_name", database);
    while (peopleQuery.next()) {
        equipmentItemResponsibleCombo->addItem(peopleQuery.value(1).toString(),
                                               peopleQuery.value(0));
        issuanceResponsibleCombo->addItem(peopleQuery.value(1).toString(),
                                          peopleQuery.value(0));
    }

    QSqlQuery itemsQuery("SELECT id, inventory_number FROM equipment_items ORDER BY inventory_number", database);
    while (itemsQuery.next()) {
        issuanceEquipmentItemCombo->addItem(itemsQuery.value(1).toString(), itemsQuery.value(0));
    }
}

void MainWindow::refreshAllData()
{
    loadDepartments();
    loadEquipmentTypes();
    loadResponsiblePeople();
    loadEquipmentItems();
    refreshCombos();
    loadIssuanceJournal();
}

void MainWindow::clearDepartmentInputs()
{
    departmentNameEdit->clear();
}

void MainWindow::clearEquipmentTypeInputs()
{
    equipmentTypeNameEdit->clear();
    equipmentTypeDescriptionEdit->clear();
}

void MainWindow::clearResponsiblePersonInputs()
{
    responsiblePersonNameEdit->clear();
    responsiblePersonPositionEdit->clear();
}

void MainWindow::clearEquipmentItemInputs()
{
    equipmentItemInventoryEdit->clear();
    equipmentItemSerialEdit->clear();
    equipmentItemStatusEdit->setText(QStringLiteral("В наличии"));
}

void MainWindow::clearIssuanceInputs()
{
    issuanceDateEdit->setDate(QDate::currentDate());
    issuanceReturnDateEdit->setDate(QDate::currentDate().addDays(14));
    issuanceNoteEdit->clear();
}

void MainWindow::addDepartment()
{
    const QString name = departmentNameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Проверка данных"), tr("Введите название департамента."));
        return;
    }

    if (executeQuery("INSERT INTO departments(name) VALUES (?)", {name})) {
        refreshAllData();
        clearDepartmentInputs();
    }
}

void MainWindow::deleteDepartment()
{
    const int id = selectedId(departmentsTable);
    if (id < 0) {
        QMessageBox::information(this, tr("Выбор записи"), tr("Сначала выберите департамент."));
        return;
    }

    if (executeQuery("DELETE FROM departments WHERE id = ?", {id})) {
        refreshAllData();
    }
}

void MainWindow::addEquipmentType()
{
    const QString name = equipmentTypeNameEdit->text().trimmed();
    const QString description = equipmentTypeDescriptionEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Проверка данных"), tr("Введите тип оборудования."));
        return;
    }

    if (executeQuery("INSERT INTO equipment_types(name, description) VALUES (?, ?)",
                     {name, description})) {
        refreshAllData();
        clearEquipmentTypeInputs();
    }
}

void MainWindow::deleteEquipmentType()
{
    const int id = selectedId(equipmentTypesTable);
    if (id < 0) {
        QMessageBox::information(this, tr("Выбор записи"), tr("Сначала выберите тип оборудования."));
        return;
    }

    if (executeQuery("DELETE FROM equipment_types WHERE id = ?", {id})) {
        refreshAllData();
    }
}

void MainWindow::addResponsiblePerson()
{
    const QString fullName = responsiblePersonNameEdit->text().trimmed();
    const QString position = responsiblePersonPositionEdit->text().trimmed();
    if (fullName.isEmpty() || position.isEmpty() || responsiblePersonDepartmentCombo->count() == 0) {
        QMessageBox::warning(this,
                             tr("Проверка данных"),
                             tr("Заполните ФИО, должность и департамент."));
        return;
    }

    if (executeQuery("INSERT INTO responsible_people(full_name, position, department_id) VALUES (?, ?, ?)",
                     {fullName, position, responsiblePersonDepartmentCombo->currentData()})) {
        refreshAllData();
        clearResponsiblePersonInputs();
    }
}

void MainWindow::deleteResponsiblePerson()
{
    const int id = selectedId(responsiblePeopleTable);
    if (id < 0) {
        QMessageBox::information(this, tr("Выбор записи"), tr("Сначала выберите ответственное лицо."));
        return;
    }

    if (executeQuery("DELETE FROM responsible_people WHERE id = ?", {id})) {
        refreshAllData();
    }
}

void MainWindow::addEquipmentItem()
{
    const QString inventory = equipmentItemInventoryEdit->text().trimmed();
    const QString serial = equipmentItemSerialEdit->text().trimmed();
    const QString status = equipmentItemStatusEdit->text().trimmed();

    if (inventory.isEmpty() || serial.isEmpty() || status.isEmpty() ||
        equipmentItemTypeCombo->count() == 0 || equipmentItemDepartmentCombo->count() == 0 ||
        equipmentItemResponsibleCombo->count() == 0) {
        QMessageBox::warning(this,
                             tr("Проверка данных"),
                             tr("Заполните все поля оборудования и справочники."));
        return;
    }

    if (executeQuery(
            "INSERT INTO equipment_items("
            "inventory_number, serial_number, equipment_type_id, department_id, responsible_person_id, status) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            {inventory,
             serial,
             equipmentItemTypeCombo->currentData(),
             equipmentItemDepartmentCombo->currentData(),
             equipmentItemResponsibleCombo->currentData(),
             status})) {
        refreshAllData();
        clearEquipmentItemInputs();
    }
}

void MainWindow::deleteEquipmentItem()
{
    const int id = selectedId(equipmentItemsTable);
    if (id < 0) {
        QMessageBox::information(this, tr("Выбор записи"), tr("Сначала выберите единицу оборудования."));
        return;
    }

    if (executeQuery("DELETE FROM equipment_items WHERE id = ?", {id})) {
        refreshAllData();
    }
}

void MainWindow::addIssuanceRecord()
{
    if (issuanceEquipmentItemCombo->count() == 0 || issuanceDepartmentCombo->count() == 0 ||
        issuanceResponsibleCombo->count() == 0) {
        QMessageBox::warning(this,
                             tr("Проверка данных"),
                             tr("Сначала заполните справочники и добавьте оборудование."));
        return;
    }

    if (issuanceReturnDateEdit->date() < issuanceDateEdit->date()) {
        QMessageBox::warning(this,
                             tr("Проверка данных"),
                             tr("Срок возврата не может быть раньше даты выдачи."));
        return;
    }

    if (executeQuery(
            "INSERT INTO issuance_records("
            "equipment_item_id, issued_to_department_id, issued_by_person_id, issued_at, return_due_date, note) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            {issuanceEquipmentItemCombo->currentData(),
             issuanceDepartmentCombo->currentData(),
             issuanceResponsibleCombo->currentData(),
             issuanceDateEdit->date(),
             issuanceReturnDateEdit->date(),
             issuanceNoteEdit->text().trimmed()})) {
        loadIssuanceJournal();
        clearIssuanceInputs();
    }
}

void MainWindow::deleteIssuanceRecord()
{
    const int id = selectedId(issuanceTable);
    if (id < 0) {
        QMessageBox::information(this, tr("Выбор записи"), tr("Сначала выберите запись журнала."));
        return;
    }

    if (executeQuery("DELETE FROM issuance_records WHERE id = ?", {id})) {
        loadIssuanceJournal();
    }
}

int MainWindow::selectedId(QTableWidget *table) const
{
    const int row = table->currentRow();
    if (row < 0 || table->item(row, 0) == nullptr) {
        return -1;
    }
    return table->item(row, 0)->text().toInt();
}

void MainWindow::showDatabaseError(const QString &action) const
{
    QMessageBox::critical(nullptr,
                          tr("Ошибка базы данных"),
                          tr("Не удалось %1.\n%2").arg(action, database.lastError().text()));
}
