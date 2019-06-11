#ifndef ASTORAGEPROVIDER_H
#define ASTORAGEPROVIDER_H

#include "private/astorageprovider_lib.h"
#include "private/astorageprovider_p.h"
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QExplicitlySharedDataPointer>
#include <QSharedPointer>
class QSqlQueryModel;
/************************************************************************************************************************************************
 *   AStorageProvider provides an all in one dynamic interface to database creation.
 * This object is very similar to Local Storage Qml object interface, however a strict schema is applied.
 * This schema is defined by the database itself. Each column has a JSON based definition (limited to JSON value types)
 *  when the storage provider is asked to connect to the database, it will query the table layout, and schema of each table
 *
 * top level table schema object  (Used to create a table)
 * is an array with 1 to many column schemas
 *  // createTable("Temperatures",[{name:"date", type:DateType},{name:"temperature",type:QRealType, default:-24.0}]
 *
 * Gives you a table with an automatically assigned index on DateType (stored internally as quint64) :
 *          [   date  ]           |     [  temperature ]
 *        162144521       |            45.2114
 *        162154538       |            46.1991
 *        162164551       |            46.8311
 *
 *
 *  Column schema definition looks like
 * {
 *      type: Qt::JsonValueType,  // the object type (can be deduced from value sent (still must match or use a known conversion)
 *      default: 0, // Uses the provided default value, or will use the built in default (empy string, zero, current date, etc.) if not provided
 *      name:"AGE", // needed to access columns by name, this is important for the mapping of variables. should not contain spaces
 *      [check]:"AGE BETWEEN {18, 64} AND ID >= 10001" // allows a constraint to be optionally set on the column using SQL syntax
 *      [checkName]:"CK_AGE_ID
 * }
 * the check becomes "CONSTRAINT CK_AGE_ID CHECK (AGE BETWEEN {18,64} AND ID >= 10001)
 *
 *
 *
 *
 ****************************************** *******************************************************************************************************/


class ASTRGE_LIBRARY_EXPORT AStorageProvider : public QObject
{
    Q_OBJECT
public:
    explicit AStorageProvider(QString root_path = "", QObject *parent = nullptr);
    bool errored();
    const QJsonObject& schema() const;
    QString rootPath() const;
    QString name() const;
    ASqlSelect selectFrom(QString table_name) const;
    ASqlSelect selectFrom(ASqlSelect inner_sql) const;
    QStringList getNonNullValueNames() const;
    QVariantMap getLastRecord(QString table_name);
    QString     getIndexColumnName();
    QString     storagePath() const;
    QSharedPointer<QSqlQueryModel>executeQuery(const ASqlStatement& statement);
    ~AStorageProvider();
signals:
    void failedToInitializeDB(QString database_path, quint8 reason);
    void failedToExecuteQuery(QString name, QJsonObject query, quint8 reason);
    void queryFailed(QJsonObject query, quint8 reference_id, quint8 reason);
    void failedToStoreRecord(QJsonObject record);
    void databaseNeedsInitialized();
    void databaseOpen(QString root_path, QString name);
    void querySuceeded(QString name, QJsonObject schema);
    void queryReply(QJsonObject query_reply, quint8 query_id);

public slots:
    bool openDB(QString name, QString db_type = "QSQLITE");
    ASqlCreateTable createTable(QString name, bool temporary = false);
    ASqlInsert      insertInto(QString table_name);
    bool            setPath(QString path);
    void            closeDB();

private:

    QExplicitlySharedDataPointer<APrivateStorageProvider>  mData;

};

#endif // ASTORAGEPROVIDER_H
